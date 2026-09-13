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
import re
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
# Crossfade de la costura. 64 muestras (1,5 ms) sellaban el click pero no un
# escalon de NIVEL: 30 ms lo funden sin difuminar nada audible, y en una pieza
# ambiental es la diferencia entre oir el salto del bucle y no oirlo.
XFADE = 1323                  # 30 ms a 44,1 kHz
ONSET_THRESHOLD = 0.005

# Sonoridad: TODAS las pistas se normalizan al mismo objetivo integrado (LUFS),
# no al RMS de un backup. El backup solo existia para las 10 pistas viejas, asi
# que las nuevas salian con gain 1.0 y volumenes dispares entre si.
TARGET_LUFS = -18.0
TARGET_PEAK = 0.84            # -1.5 dBFS de techo, margen de sobra para el true-peak
# La musica baja a 32 kHz: por encima de 16 kHz no hay nada audible en pads,
# ney y percusion de marco, y son ~25 MB menos en assets/ con 27 pistas.
MUSIC_SR = 32000
OGG_QUALITY = "3"

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
    """lib.rb primero (define los helpers, que persisten en la sesion) y luego
    el runner. Asi la partitura que manda el MCP solo trae musica y el mensaje
    OSC se queda corto."""
    for fname in ("lib.rb", "runner.rb"):
        with open(os.path.join(HERE, fname), "r", encoding="utf-8") as f:
            spider_send(ports, "/run-code", f.read())
        time.sleep(0.4)


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


def _biquad(x: np.ndarray, b, a) -> np.ndarray:
    """Aplica un biquad normalizado (a0 = 1) canal por canal.

    Con scipy son milisegundos; el bucle de abajo es el plan B, que tarda ~5 s
    por pista y por filtro (aqui hay dos), pero evita la dependencia dura.
    """
    b0, b1, b2 = b
    a1, a2 = a
    try:
        from scipy.signal import lfilter
        return lfilter([b0, b1, b2], [1.0, a1, a2], x.astype(np.float64),
                       axis=0).astype(np.float32)
    except ImportError:
        pass
    out = np.empty_like(x)
    for c in range(x.shape[1]):
        v = x[:, c].astype(np.float64)
        y = np.zeros_like(v)
        x1 = x2 = y1 = y2 = 0.0
        for i in range(len(v)):
            xi = v[i]
            yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
            x2, x1 = x1, xi
            y2, y1 = y1, yi
            y[i] = yi
        out[:, c] = y
    return out.astype(np.float32)


def highpass(x: np.ndarray, sr: int, fc: float = 55.0) -> np.ndarray:
    """High-pass de 2o orden (RBJ).

    Por debajo de 55 Hz no hay musica: hay el retumbe de los pads y del bajo,
    que no se OYE en un portatil ni en un movil pero si se lleva energia.
    """
    w0 = 2.0 * np.pi * fc / sr
    alpha = np.sin(w0) / (2.0 * 0.707)
    cw = np.cos(w0)
    a0 = 1 + alpha
    return _biquad(x,
                   ((1 + cw) / 2 / a0, -(1 + cw) / a0, (1 + cw) / 2 / a0),
                   (-2 * cw / a0, (1 - alpha) / a0))


def highshelf(x: np.ndarray, sr: int, fc: float = 3000.0,
              gain_db: float = 6.0) -> np.ndarray:
    """Realce de agudos de bus (RBJ high-shelf).

    Es la pieza que faltaba en la mezcla. Los samples de percusion aguda SI
    tienen aire (hat_cab: el 98% de su energia por encima de 2 kHz), pero a
    amp 0,06 y 0,16 s de duracion no pesan nada al lado de un bajo a 0,42
    sostenido: las diez pistas viejas se quedaban en el 0-2% de energia sobre
    2 kHz y por eso sonaban tapadas. El shelf levanta TODO el agudo de la
    mezcla —armonicos de pads, ataque de la kalimba y del ney incluidos—, que
    es justo lo que hace un ingeniero y no una capa mas de shaker.
    """
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * fc / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2.0 * np.sqrt((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0)
    sa = 2.0 * np.sqrt(A) * alpha
    a0 = (A + 1) - (A - 1) * cw + sa
    return _biquad(x,
                   (A * ((A + 1) + (A - 1) * cw + sa) / a0,
                    -2 * A * ((A - 1) + (A + 1) * cw) / a0,
                    A * ((A + 1) + (A - 1) * cw - sa) / a0),
                   (2 * ((A - 1) - (A + 1) * cw) / a0,
                    ((A + 1) - (A - 1) * cw - sa) / a0))


def _high_share(x: np.ndarray, sr: int) -> float:
    """Fraccion de energia por encima de 2 kHz (el "aire" de la pista)."""
    mono = x.mean(axis=1) if x.ndim > 1 and x.shape[1] > 1 else x.reshape(-1)
    seg = 1 << 15
    if len(mono) < seg:
        return 0.0
    acc = np.zeros(seg // 2 + 1)
    win = np.hanning(seg)
    n = 0
    for i in range(0, len(mono) - seg, seg):
        acc += np.abs(np.fft.rfft(mono[i:i + seg] * win)) ** 2
        n += 1
    if not n:
        return 0.0
    freqs = np.fft.rfftfreq(seg, 1.0 / sr)
    tot = float(np.sum(acc)) or 1e-12
    return float(np.sum(acc[freqs >= 2000]) / tot)


def air_balance(x: np.ndarray, sr: int, target: float = 0.12) -> tuple:
    """Ajusta el brillo de CADA pista al mismo objetivo, como el LUFS hace con
    el volumen.

    Un shelf fijo no vale: con +8 dB, `cap01` (pads, ney y un tambor de marco)
    se quedaba en el 6% de energia sobre 2 kHz y `main_theme` (campanas, coro y
    cabasa) se iba al 44%. Sonando seguidas, una parece tapada y la otra
    delgada. Aqui se busca la ganancia que deja a las dos en el mismo sitio.
    """
    best, best_gain = None, 0.0
    for gain_db in (-6, -4, -2, 0, 2, 4, 6, 8, 10, 12):
        y = highshelf(x, sr, 2800.0, gain_db) if gain_db else x
        share = _high_share(y, sr)
        d = abs(share - target)
        if best is None or d < best:
            best, best_gain = d, gain_db
        if share > target:
            break
    out = highshelf(x, sr, 2800.0, best_gain) if best_gain else x
    return out, best_gain


def measure_lufs(x: np.ndarray, sr: int) -> float:
    """Sonoridad integrada (EBU R128) via ffmpeg. None si no se puede medir."""
    p = subprocess.run(
        ["ffmpeg", "-v", "info", "-f", "f32le", "-ar", str(sr),
         "-ac", str(x.shape[1]), "-i", "-", "-filter:a", "ebur128=framelog=quiet",
         "-f", "null", "-"],
        input=x.tobytes(), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    m = re.findall(r"I:\s+(-?\d+\.\d+)\s+LUFS", p.stderr.decode("utf-8", "replace"))
    return float(m[-1]) if m else None


def rms_of(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.square(x, dtype=np.float64))))


def _seam_db(loop: np.ndarray, sr: int) -> float:
    """Salto de nivel entre los ultimos 2 s del bucle y los dos primeros."""
    mono = loop.mean(axis=1) if loop.ndim > 1 else loop
    w = int(2.0 * sr)
    if len(mono) < 2 * w:
        return 0.0
    a = float(np.sqrt(np.mean(np.square(mono[-w:], dtype=np.float64))))
    b = float(np.sqrt(np.mean(np.square(mono[:w], dtype=np.float64))))
    return abs(20 * np.log10(max(a, 1e-9)) - 20 * np.log10(max(b, 1e-9)))


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

    # Crossfade adaptativo. 30 ms sellan el click, pero no un ESCALON de nivel,
    # que es lo que dejan las capas de cola larga al cruzar la costura: el
    # principio del bucle lleva la cola del ciclo anterior y el final no. Si el
    # salto pasa de 6 dB se funde con 1,5 s, que en material sostenido no se
    # nota y quita el escalon de verdad.
    xf = XFADE
    if _seam_db(loop, sr) > 6.0 and b + int(1.5 * sr) <= len(x):
        xf = int(1.5 * sr)
        print(f"costura de {_seam_db(loop, sr):.1f} dB: crossfade largo "
              f"({xf / sr:.1f} s)", flush=True)
    if b + xf > len(x):
        xf = len(x) - b
    if xf > 0:
        tail = x[b:b + xf]
        ramp = np.linspace(0.0, 1.0, xf, dtype=np.float32)[:, None]
        loop[:xf] = loop[:xf] * ramp + tail * (1.0 - ramp)
    return loop, onset


def cmd_reprocess(name: str, period_s: float) -> None:
    """Vuelve a cortar, mezclar y encodear desde el WAV que dejo `capture`.

    Afinar la mezcla no deberia costar 3,5 minutos de grabacion por intento:
    el WAV de tmp/ ya tiene la interpretacion entera.
    """
    wav_path = os.path.join(TMP_DIR, f"{name}.wav")
    if not os.path.exists(wav_path):
        raise SystemExit(f"No hay grabacion previa en {wav_path}: hay que capturar.")
    _finish(name, wav_path, period_s)


def send_score(ports: dict, rb_path: str) -> None:
    """Manda una partitura al puerto de CUES con /run-code.

    Es exactamente lo que hace el MCP `sonic-pi` (UDP al 4560), y lo recoge el
    mismo `sync "/osc*/run-code"` de runner.rb. Tenerlo aqui es lo que permite
    encadenar capturas sin una ida y vuelta por pista.
    """
    with open(rb_path, "r", encoding="utf-8") as f:
        code = f.read()
    osc_send(ports["osc_cues"], "/run-code", code)


def cmd_batch(items) -> None:
    """Captura una lista de (pista, periodo) en cadena, leyendo su .rb."""
    ok, bad = [], []
    for name, period in items:
        rb = os.path.join(HERE, f"{name}.rb")
        if not os.path.exists(rb):
            print(f"SALTO {name}: no hay {rb}", flush=True)
            bad.append(name)
            continue
        print(f"===== {name} ({period:.2f} s) =====", flush=True)
        try:
            cmd_capture(name, period, 5.0, rb_path=rb)
            ok.append(name)
        except SystemExit as e:
            print(f"FALLO {name}: {e}", flush=True)
            bad.append(name)
    print(f"BATCH: {len(ok)} capturadas, {len(bad)} con problema "
          f"{'(' + ', '.join(bad) + ')' if bad else ''}", flush=True)


def cmd_capture(name: str, period_s: float, extra_tail: float = 4.0,
                wait_start: float = 180.0, rb_path: str = None) -> None:
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
    if rb_path:
        time.sleep(0.4)
        send_score(ports, rb_path)
        print(f"ARMED {name}: partitura mandada ({os.path.basename(rb_path)})",
              flush=True)
    else:
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

    _finish(name, wav_path, period_s)


def _finish(name: str, wav_path: str, period_s: float) -> None:
    x, sr = decode_f32(wav_path)
    loop, onset = cut_loop(x, sr, period_s)

    # Mezcla de bus antes de medir la sonoridad: si se filtra DESPUES de
    # normalizar, la pista se queda por debajo del objetivo.
    loop = highpass(loop, sr, 55.0)
    loop, air_db = air_balance(loop, sr, 0.12)

    # Sonoridad: objetivo integrado FIJO para las 27 pistas. Asi el jugador no
    # tiene que tocar el volumen al cambiar de capitulo.
    gain = 1.0
    lufs = measure_lufs(loop, sr)
    if lufs is not None:
        gain = float(np.clip(10.0 ** ((TARGET_LUFS - lufs) / 20.0), 0.1, 10.0))
    else:
        print("AVISO: ebur128 no midio nada; me quedo con el RMS del backup",
              flush=True)
        backup = os.path.join(BACKUP_DIR, f"{name}.ogg")
        if os.path.exists(backup):
            bx, _ = decode_f32(backup)
            r_new, r_old = rms_of(loop), rms_of(bx)
            if r_new > 1e-6:
                gain = float(np.clip(r_old / r_new, 0.25, 4.0))
    peak = float(np.max(np.abs(loop))) * gain
    if peak > TARGET_PEAK:
        gain *= TARGET_PEAK / peak
    loop = (loop * gain).astype(np.float32)

    out_path = os.path.join(MUSIC_DIR, f"{name}.ogg")
    enc = subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(sr),
         "-ac", str(loop.shape[1]), "-i", "-", "-c:a", "libvorbis",
         "-q:a", OGG_QUALITY, "-ar", str(MUSIC_SR), out_path],
        input=loop.tobytes())
    if enc.returncode != 0:
        raise SystemExit("ffmpeg fallo al encodear")
    dur = len(loop) / sr
    print(f"OK {name}: {dur:.2f} s, sr={sr}->{MUSIC_SR}, onset={onset / sr:.2f} s, "
          f"aire {air_db:+d} dB, "
          f"LUFS {lufs if lufs is None else round(lufs, 1)} -> {TARGET_LUFS}, "
          f"gain=x{gain:.2f}, peak={float(np.max(np.abs(loop))):.3f}, "
          f"{os.path.getsize(out_path) / 1024:.0f} KB -> {out_path}", flush=True)

    # Gate automatico: no puedo escuchar la pista, asi que se mide.
    try:
        import analyze
        analyze.report(loop, sr, name)
    except Exception as e:
        print(f"AVISO: el analisis no corrio ({e})", flush=True)


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
    elif cmd == "reprocess":
        cmd_reprocess(sys.argv[2], float(sys.argv[3]))
    elif cmd == "batch":
        # batch pista:periodo pista:periodo ...
        items = []
        for a in sys.argv[2:]:
            n, _, p = a.partition(":")
            items.append((n, float(p)))
        cmd_batch(items)
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
