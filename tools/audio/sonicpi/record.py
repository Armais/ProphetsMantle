"""Grabador de las pistas de Sonic Pi para MANTLE.

El MCP de sonic-pi solo REPRODUCE (manda codigo por OSC al puerto de cues);
este harness es lo que convierte lo que suena en los .ogg del juego:

  python record.py boot                 arranca el daemon de Sonic Pi sin GUI,
                                        mantiene el keep-alive (sin el, el
                                        daemon se suicida en segundos) y deja
                                        puertos+token en ports.json. Vive
                                        hasta que se le mate o 'shutdown'.
  python record.py runner               inyecta runner.rb via spider (para un
                                        arranque manual o tras un stop).
  python record.py capture <pista> <periodo_s>
                                        stop-all + runner + arma la grabacion
                                        y espera el /mantle/started que el
                                        runner emite al recibir el segmento
                                        del MCP; graba 2 periodos + cola,
                                        corta el segundo periodo (regimen
                                        estacionario), sella la costura con un
                                        micro-crossfade y encodea el .ogg en
                                        assets/audio/music/ igualando la
                                        sonoridad RMS del backup.
  python record.py run <fichero.rb>     manda un .rb directo al spider
                                        (smoke test sin pasar por el MCP).
  python record.py stop                 /stop-all-jobs.
  python record.py shutdown             /daemon/exit ordenado.

Sin dependencias: OSC se codifica a mano y el audio va por ffmpeg + numpy.
"""

import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
MUSIC_DIR = os.path.join(ROOT, "assets", "audio", "music")
BACKUP_DIR = os.path.join(ROOT, "tools", "audio", "music_backup")
PORTS_JSON = os.path.join(HERE, "ports.json")
TMP_DIR = os.path.join(HERE, "tmp")
HARNESS_PORT = 5052          # donde runner.rb avisa /mantle/started
XFADE = 64                    # muestras de crossfade en la costura del bucle
ONSET_THRESHOLD = 0.005

SONIC_PI_ROOTS = [
    # Arbol extraido del MSI a mano (msiexec estaba bloqueado por un UAC
    # pendiente); si algun dia se instala de verdad, las otras rutas valen.
    os.path.expandvars(r"%LOCALAPPDATA%\Programs\SonicPiPortable\PFiles\Sonic Pi"),
    r"C:\Program Files\Sonic Pi",
    r"C:\Program Files (x86)\Sonic Pi",
]


# ============================================================================
# OSC minimo (codificacion a mano: un mensaje UDP, sin bundles)
# ============================================================================

def _pad(b: bytes) -> bytes:
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


def osc_message(addr: str, *args) -> bytes:
    msg = _pad(addr.encode("utf-8") + b"\x00")
    tags = ","
    data = b""
    for a in args:
        if isinstance(a, bool):
            raise ValueError("bool no soportado")
        if isinstance(a, int):
            tags += "i"
            data += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"
            data += struct.pack(">f", a)
        else:
            tags += "s"
            data += _pad(str(a).encode("utf-8") + b"\x00")
    return msg + _pad(tags.encode("ascii") + b"\x00") + data


def osc_send(port: int, addr: str, *args) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(osc_message(addr, *args), ("127.0.0.1", port))
    finally:
        sock.close()


def osc_addr_of(packet: bytes) -> str:
    end = packet.find(b"\x00")
    return packet[:end].decode("utf-8", "replace") if end > 0 else ""


def osc_first_string_arg(packet: bytes) -> str:
    # Tras la direccion y las etiquetas, el primer argumento string (si lo hay).
    try:
        i = packet.find(b",")
        if i < 0:
            return ""
        tags_end = packet.find(b"\x00", i)
        tags = packet[i + 1:tags_end].decode("ascii", "replace")
        pos = (tags_end + 4) & ~3
        for t in tags:
            if t == "i" or t == "f":
                pos += 4
            elif t == "s":
                end = packet.find(b"\x00", pos)
                return packet[pos:end].decode("utf-8", "replace")
        return ""
    except Exception:
        return ""


# ============================================================================
# Daemon de Sonic Pi
# ============================================================================

def find_sonic_pi() -> str:
    for root in SONIC_PI_ROOTS:
        if os.path.isfile(os.path.join(root, "app", "server", "ruby", "bin", "daemon.rb")):
            return root
    raise SystemExit("No encuentro Sonic Pi instalado (daemon.rb)")


def load_ports() -> dict:
    with open(PORTS_JSON, "r", encoding="utf-8") as f:
        return json.load(f)


def cmd_boot() -> None:
    root = find_sonic_pi()
    ruby = os.path.join(root, "app", "server", "native", "ruby", "bin", "ruby.exe")
    daemon = os.path.join(root, "app", "server", "ruby", "bin", "daemon.rb")
    if not os.path.isfile(ruby):
        raise SystemExit(f"No encuentro ruby.exe en {ruby}")
    print(f"Arrancando daemon: {daemon}", flush=True)
    proc = subprocess.Popen([ruby, daemon], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, cwd=root)
    # El daemon suelta lineas en blanco y avisos "Critical" inofensivos por
    # stdout ANTES y DESPUES de la linea de puertos: buscarla, no asumirla.
    nums = []
    deadline = time.time() + 60.0
    while time.time() < deadline:
        line = proc.stdout.readline()
        if line == "" and proc.poll() is not None:
            raise SystemExit("El daemon murio antes de dar los puertos")
        line = line.strip()
        if line:
            print(f"daemon: {line}", flush=True)
        toks = line.split()
        if len(toks) == 8 and all(t.lstrip("-").isdigit() for t in toks):
            nums = [int(t) for t in toks]
            break
    if len(nums) < 8:
        raise SystemExit("El daemon no dio la linea de puertos en 60 s")
    # Orden segun daemon.rb 4.x: daemon, gui-listen-to-spider, gui-send-to-spider,
    # scsynth, osc-cues, tau, phx, token
    ports = {
        "daemon": nums[0],
        "gui_listen_to_spider": nums[1],
        "gui_send_to_spider": nums[2],
        "scsynth": nums[3],
        "osc_cues": nums[4],
        "tau": nums[5],
        "phx": nums[6],
        "token": nums[7],
        "pid": proc.pid,
    }
    with open(PORTS_JSON, "w", encoding="utf-8") as f:
        json.dump(ports, f, indent=2)
    print(f"ports.json escrito: {ports}", flush=True)

    def drain():
        for extra in proc.stdout:
            pass  # el daemon apenas habla por stdout; los logs van a ~/.sonic-pi/log

    threading.Thread(target=drain, daemon=True).start()

    # Keep-alive: sin esto el daemon mata spider+scsynth en segundos.
    print("Keep-alive en marcha (Ctrl+C o 'shutdown' para parar)", flush=True)
    try:
        while proc.poll() is None:
            osc_send(ports["daemon"], "/daemon/keep-alive", ports["token"])
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    print(f"daemon terminado (rc={proc.poll()})", flush=True)


def spider_send(ports: dict, addr: str, *args) -> None:
    osc_send(ports["gui_send_to_spider"], addr, ports["token"], *args)


def inject_runner(ports: dict) -> None:
    with open(os.path.join(HERE, "runner.rb"), "r", encoding="utf-8") as f:
        code = f.read()
    spider_send(ports, "/run-code", code)


# ============================================================================
# Captura y corte del bucle
# ============================================================================

def ffprobe_props(path: str):
    out = subprocess.check_output([
        "ffprobe", "-v", "error", "-select_streams", "a:0",
        "-show_entries", "stream=sample_rate,channels",
        "-of", "csv=p=0", path], text=True).strip()
    sr, ch = out.split(",")[:2]
    return int(sr), int(ch)


def decode_f32(path: str):
    sr, ch = ffprobe_props(path)
    raw = subprocess.check_output(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-"],
    )
    x = np.frombuffer(raw, dtype=np.float32).reshape(-1, ch)
    return x, sr


def rms_of(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.square(x, dtype=np.float64))))


def cut_loop(x: np.ndarray, sr: int, period_s: float):
    mono = np.max(np.abs(x), axis=1)
    above = np.nonzero(mono > ONSET_THRESHOLD)[0]
    if len(above) == 0:
        raise SystemExit("La grabacion esta EN SILENCIO: no ha sonado nada.")
    onset = int(above[0])
    period = int(round(period_s * sr))
    a = onset + period
    b = a + period
    if b + XFADE > len(x):
        raise SystemExit(
            f"Grabacion corta: necesito {b + XFADE} muestras y hay {len(x)} "
            f"(onset={onset}, periodo={period}). Sube la cola de captura.")
    loop = x[a:b].copy()
    tail = x[b:b + XFADE]
    ramp = np.linspace(0.0, 1.0, XFADE, dtype=np.float32)[:, None]
    loop[:XFADE] = loop[:XFADE] * ramp + tail * (1.0 - ramp)
    return loop, onset


def cmd_capture(name: str, period_s: float, extra_tail: float = 4.0,
                wait_start: float = 180.0) -> None:
    ports = load_ports()
    os.makedirs(TMP_DIR, exist_ok=True)
    wav_path = os.path.join(TMP_DIR, f"{name}.wav").replace("\\", "/")
    if os.path.exists(wav_path):
        os.remove(wav_path)

    listen = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen.bind(("127.0.0.1", HARNESS_PORT))
    listen.settimeout(wait_start)

    spider_send(ports, "/stop-all-jobs")
    time.sleep(0.7)
    inject_runner(ports)
    time.sleep(0.7)
    spider_send(ports, "/start-recording")
    time.sleep(0.3)
    print(f"ARMED {name}: esperando el segmento del MCP "
          f"(hasta {wait_start:.0f} s)...", flush=True)

    try:
        while True:
            packet, _ = listen.recvfrom(65536)
            addr = osc_addr_of(packet)
            if addr == "/mantle/started":
                break
            if addr == "/mantle/error":
                spider_send(ports, "/stop-recording")
                raise SystemExit(
                    f"El runner no pudo evaluar el segmento: "
                    f"{osc_first_string_arg(packet)}")
    except socket.timeout:
        spider_send(ports, "/stop-recording")
        raise SystemExit("Nadie mando el segmento (timeout).")
    finally:
        listen.close()

    total = 2.0 * period_s + extra_tail
    print(f"Grabando {total:.1f} s (2 periodos de {period_s:.2f} s + cola)...",
          flush=True)
    time.sleep(total)
    spider_send(ports, "/stop-recording")
    time.sleep(0.5)
    spider_send(ports, "/save-recording", wav_path)
    time.sleep(2.0)
    spider_send(ports, "/stop-all-jobs")
    if not os.path.exists(wav_path):
        raise SystemExit(f"El spider no escribio {wav_path}")

    x, sr = decode_f32(wav_path)
    loop, onset = cut_loop(x, sr, period_s)

    # Sonoridad: igualar el RMS del backup para respetar la mezcla del juego.
    gain = 1.0
    backup = os.path.join(BACKUP_DIR, f"{name}.ogg")
    if os.path.exists(backup):
        bx, _ = decode_f32(backup)
        r_new, r_old = rms_of(loop), rms_of(bx)
        if r_new > 1e-6:
            gain = float(np.clip(r_old / r_new, 0.25, 4.0))
    peak = float(np.max(np.abs(loop))) * gain
    if peak > 0.98:
        gain *= 0.98 / peak
    loop = (loop * gain).astype(np.float32)

    out_path = os.path.join(MUSIC_DIR, f"{name}.ogg")
    enc = subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(sr),
         "-ac", str(loop.shape[1]), "-i", "-", "-c:a", "libvorbis",
         "-q:a", "4", out_path],
        input=loop.tobytes())
    if enc.returncode != 0:
        raise SystemExit("ffmpeg fallo al encodear")
    dur = len(loop) / sr
    print(f"OK {name}: {dur:.2f} s, sr={sr}, onset={onset / sr:.2f} s, "
          f"gain=x{gain:.2f}, peak={float(np.max(np.abs(loop))):.3f}, "
          f"{os.path.getsize(out_path) / 1024:.0f} KB -> {out_path}", flush=True)


# ============================================================================
# main
# ============================================================================

def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        raise SystemExit(2)
    cmd = sys.argv[1]
    if cmd == "boot":
        cmd_boot()
    elif cmd == "runner":
        inject_runner(load_ports())
        print("runner.rb inyectado")
    elif cmd == "capture":
        name = sys.argv[2]
        period = float(sys.argv[3])
        tail = float(sys.argv[4]) if len(sys.argv) > 4 else 4.0
        cmd_capture(name, period, tail)
    elif cmd == "run":
        with open(sys.argv[2], "r", encoding="utf-8") as f:
            spider_send(load_ports(), "/run-code", f.read())
        print(f"{sys.argv[2]} mandado al spider")
    elif cmd == "stop":
        spider_send(load_ports(), "/stop-all-jobs")
        print("stop-all-jobs mandado")
    elif cmd == "shutdown":
        ports = load_ports()
        osc_send(ports["daemon"], "/daemon/exit", ports["token"])
        print("daemon/exit mandado")
    else:
        print(__doc__)
        raise SystemExit(2)


if __name__ == "__main__":
    main()
