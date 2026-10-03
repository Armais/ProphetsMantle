"""Renderiza la musica de MANTLE con la orquesta sintetica propia.

  python tools/audio/orquesta/render.py                 las 47 pistas
  python tools/audio/orquesta/render.py cap03 boss      solo las que EMPIEZAN
                                                         por esos prefijos
  opciones:  -j N        procesos en paralelo (por defecto, los nucleos)
             --wav-only  no encodea: deja solo tmp/<pista>.wav
             --spectro   ademas pinta tmp/<pista>.png

Cada pista sale como un BUCLE EXACTO: lo lineal (notas, cuerpos, reverb) se
pliega modulo el periodo y lo no lineal se precalienta con su propio final, que
es lo mismo que grabar dos vueltas y cortar la segunda pero sin pagar dos
vueltas. Despues: .wav en tmp/ (float, 32 kHz), .ogg Vorbis en
assets/audio/music/, y los gates de tools/audio/sonicpi/analyze.py.

Determinista: las semillas salen del nombre de la pista y de cada parte, asi
que dos pasadas dan ficheros identicos.
"""

import os
import subprocess
import sys
import time
from multiprocessing import Pool

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
MUSIC_DIR = os.path.join(ROOT, "assets", "audio", "music")
TMP = os.path.join(HERE, "tmp")
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "sonicpi"))

import dsp  # noqa: E402
import mix  # noqa: E402

OGG_Q = "3"


def all_tracks():
    import tracks_global
    import tracks_act1
    import tracks_act2
    t = {}
    for m in (tracks_global, tracks_act1, tracks_act2):
        t.update(m.TRACKS)
    return t


def encode_ogg(x, path):
    p = subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(dsp.SR),
                        "-ac", "2", "-i", "-", "-c:a", "libvorbis", "-q:a", OGG_Q, path],
                       input=x.astype(np.float32).tobytes())
    if p.returncode != 0:
        raise RuntimeError("ffmpeg fallo al encodear " + path)


def decode(path):
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le",
                                   "-ac", "2", "-ar", str(dsp.SR), "-"])
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2)


def click_score(x):
    """Peor salto muestra a muestra relativo al RMS local (5 ms). Un clic de
    verdad da > 6; la musica normal, < 2,5."""
    d = np.abs(np.diff(np.concatenate([x, x[:1]]), axis=0)).max(axis=1)
    m = np.mean(x ** 2, axis=1)
    k = int(0.005 * dsp.SR)
    loc = np.sqrt(np.convolve(m, np.ones(k) / k, mode="same")) + 1e-4
    r = d / loc
    i = int(np.argmax(r))
    return float(r[i]), i


def job(args):
    name, opts = args
    t0 = time.time()
    tracks = all_tracks()
    S = tracks[name]()
    x, info = mix.render_score(S)
    os.makedirs(TMP, exist_ok=True)
    from scipy.io import wavfile
    wavfile.write(os.path.join(TMP, name + ".wav"), dsp.SR, x)
    res = dict(name=name, dur=len(x) / dsp.SR, lufs=info["lufs"], tp=info["tp"],
               air=info["air_db"], body=info["body_db"], comp=info["comp_gr"],
               lim=info["limit_db"], secs=0.0, nan=bool(np.isnan(x).any()))
    res["click"], res["click_at"] = click_score(x)
    if not opts.get("wav_only"):
        out = os.path.join(MUSIC_DIR, name + ".ogg")
        encode_ogg(x, out)
        y = decode(out)
        res["peak_ogg"] = float(np.max(np.abs(y)))
        res["kb"] = os.path.getsize(out) / 1024.0
        lu, tpk = dsp.lufs_ffmpeg(y)
        res["lufs_ogg"] = lu
        res["tp_ogg"] = tpk
    if opts.get("spectro"):
        import spectro
        spectro.render_png(x, dsp.SR, os.path.join(TMP, name + ".png"), name)
    res["secs"] = time.time() - t0
    return res


def gates(names):
    import analyze
    ok = 0
    for n in names:
        p = os.path.join(MUSIC_DIR, n + ".ogg")
        x = decode(p)
        if analyze.report(x, dsp.SR, n + ".ogg"):
            ok += 1
    print(f"\n{ok}/{len(names)} pistas en verde")
    return ok == len(names)


def main(argv):
    opts = {}
    jobs = os.cpu_count() or 2
    pref = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-j":
            jobs = int(argv[i + 1])
            i += 2
            continue
        if a == "--wav-only":
            opts["wav_only"] = True
        elif a == "--spectro":
            opts["spectro"] = True
        else:
            pref.append(a)
        i += 1
    tracks = all_tracks()
    names = sorted(n for n in tracks if not pref or any(n.startswith(p) for p in pref))
    if not names:
        print("Ninguna pista coincide:", pref)
        return 2
    # las caras primero (duracion x numero de partes): reparte mejor la carga
    cost = {}
    for n in names:
        S = tracks[n]()
        cost[n] = S.period * len(S.parts)
    names.sort(key=lambda n: -cost[n])
    t0 = time.time()
    print(f"Renderizando {len(names)} pistas con {jobs} procesos...", flush=True)
    results = []
    with Pool(min(jobs, len(names))) as pool:
        for r in pool.imap_unordered(job, [(n, opts) for n in names]):
            results.append(r)
            print(f"  {r['name']:14s} {r['dur']:6.1f}s  {r['lufs']:6.1f} LUFS  tp {r['tp']:5.1f}"
                  f"  aire {r['air']:+.1f}  cuerpo {r['body']:+.1f}  comp {r['comp']:.1f}"
                  f"  lim {r['lim']:.1f}  clic {r['click']:.1f}"
                  + (f"  ogg pico {r['peak_ogg']:.3f} {r['kb']:.0f} KB" if 'kb' in r else "")
                  + f"  [{r['secs']:.0f}s]", flush=True)
    print(f"Hecho en {time.time() - t0:.0f} s")
    if not opts.get("wav_only"):
        gates(sorted(r["name"] for r in results))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
