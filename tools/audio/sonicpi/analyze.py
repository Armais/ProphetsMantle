"""Gates numericos de las pistas de MANTLE.

Yo no puedo escuchar lo que genero, asi que cada captura pasa por aqui. Los
gates no dicen si una melodia es bonita; cazan los defectos CONCRETOS que
tenia la tanda de 2026-08-26 y los que el pipeline puede producir en silencio:

  span     rango dinamico entre el bloque mas fuerte y el mas flojo. Un bucle
           con todas las capas sonando de principio a fin da ~2 dB; una pieza
           con secciones e intro da 6+.
  seam     salto de nivel en la costura del bucle: el final tiene que empalmar
           con el principio o se oye un escalon a cada vuelta.
  quiet    ninguna ventana de 2 s puede quedarse en nada.
  corr     correlacion L/R. ~1.0 es mono disfrazado de estereo.
  centroid brillo. Por debajo de 400 Hz la pista suena embarrada; por encima
           de 3500 Hz, fina y sin cuerpo.
  bands    reparto grave/medio/agudo: que ninguna banda se lo coma todo.
  onsets   densidad de ataques por segundo. Es lo que caza LA CAPA MUERTA por
           sample mal nombrado, que no da error al evaluar y se pierde en
           silencio (la trampa que costo la sesion del pipeline).

Uso:  python analyze.py <fichero.ogg|wav> [...]   (tambien vale un directorio)
"""

import os
import sys

import numpy as np

GATES = {
    # Rango dinamico entre el bloque mas fuerte y el mas flojo. El umbral sale
    # de medir, no de la intuicion: las diez pistas viejas daban 0,3-2,9 dB (un
    # bucle sin arco ninguno) y una pieza con intro, secciones y salida se pone
    # en 4-6 dB en cuanto las capas entran y salen. Mas de 14 dB seria musica de
    # fondo que se esconde bajo los SFX, que tampoco vale para un juego.
    "span_db_min":     4.0,
    "span_db_max":    14.0,
    "seam_db_max":     6.0,
    "quiet_dbfs_min": -45.0,
    "corr_max":        0.985,
    "centroid_min":    300.0,
    "centroid_max":   4000.0,
    # Energia minima por encima de 2 kHz. Es el gate que de verdad separa una
    # pista con aire de una tapada: las diez viejas estaban en el 0-2%.
    "high_share_min":  0.04,
    # Y su pareja por abajo: una pista sin graves suena delgada, que es el
    # defecto contrario al de la tanda vieja y sale igual de mal.
    "low_share_min":   0.08,
    # Tope flojo: lo que importa de verdad son low_share_min y high_share_min.
    # Una pieza etérea sin percusion (cap04_intro) llega al 86% de medios y no
    # es un defecto; lo seria no tener nada arriba ni abajo, y eso ya se mide.
    "band_max_share":  0.90,
    "band_min_share":  0.01,
    "onsets_min":      0.4,
    "onsets_max":      9.0,
}


def _db(x):
    return 20.0 * np.log10(max(float(x), 1e-9))


def _rms(x):
    return float(np.sqrt(np.mean(np.square(x, dtype=np.float64)))) if len(x) else 0.0


def measure(x: np.ndarray, sr: int) -> dict:
    """x: float32 (n, canales) ya recortado al bucle."""
    n = len(x)
    mono = x.mean(axis=1) if x.ndim > 1 and x.shape[1] > 1 else x.reshape(-1)
    dur = n / float(sr)
    m = {"dur": dur, "peak": float(np.max(np.abs(x))) if n else 0.0}

    # --- rango dinamico: 32 bloques -------------------------------------
    nb = 32
    blocks = [_rms(mono[i * n // nb:(i + 1) * n // nb]) for i in range(nb)]
    bdb = [_db(b) for b in blocks]
    m["span_db"] = max(bdb) - min(bdb)
    m["blocks_db"] = bdb

    # --- costura del bucle ----------------------------------------------
    # Ventana de 2 s, no de 0,5: con medio segundo, que un golpe de percusion
    # esporadico caiga dentro o fuera mueve el RMS 10 dB sin que se oiga ningun
    # escalon. Lo que delata una costura mala es un salto de nivel SOSTENIDO.
    w = int(2.0 * sr)
    m["seam_db"] = abs(_db(_rms(mono[-w:])) - _db(_rms(mono[:w]))) if n > 2 * w else 0.0

    # --- ventana silenciosa ---------------------------------------------
    w2 = int(2.0 * sr)
    if n > w2:
        hops = [_db(_rms(mono[i:i + w2])) for i in range(0, n - w2, max(1, w2 // 2))]
        m["quiet_dbfs"] = min(hops)
    else:
        m["quiet_dbfs"] = _db(_rms(mono))

    # --- correlacion L/R -------------------------------------------------
    if x.ndim > 1 and x.shape[1] == 2:
        l, r = x[:, 0].astype(np.float64), x[:, 1].astype(np.float64)
        den = np.sqrt(np.sum(l * l) * np.sum(r * r))
        m["corr"] = float(np.sum(l * r) / den) if den > 1e-12 else 1.0
    else:
        m["corr"] = 1.0

    # --- espectro: centroide y reparto por bandas ------------------------
    # La ventana se encoge para senales cortas: con 32768 fijas, cualquier
    # cosa de menos de 0,75 s no llenaba ni un segmento y salia con centroide
    # 0, que parece "sin agudo" cuando en realidad es "sin medir".
    seg = 1 << 15
    while seg > 256 and seg > n:
        seg >>= 1
    win = np.hanning(seg)
    acc = np.zeros(seg // 2 + 1)
    steps = 0
    for i in range(0, max(1, n - seg + 1), max(1, seg // 2)):
        chunk = mono[i:i + seg]
        if len(chunk) < seg:
            break
        acc += np.abs(np.fft.rfft(chunk * win)) ** 2
        steps += 1
    if steps:
        acc /= steps
    freqs = np.fft.rfftfreq(seg, 1.0 / sr)
    tot = float(np.sum(acc)) or 1e-12
    m["centroid"] = float(np.sum(freqs * acc) / tot)
    m["bands"] = {
        "low":  float(np.sum(acc[freqs < 200]) / tot),
        "mid":  float(np.sum(acc[(freqs >= 200) & (freqs < 2000)]) / tot),
        "high": float(np.sum(acc[freqs >= 2000]) / tot),
    }

    # --- onsets: picos de la envolvente en bloques de 10 ms --------------
    hop = max(1, sr // 100)
    env = np.array([_rms(mono[i:i + hop]) for i in range(0, n - hop, hop)])
    if len(env) > 3:
        d = np.diff(env)
        d[d < 0] = 0.0
        thr = float(np.mean(d) + 1.5 * np.std(d))
        peaks = 0
        last = -10
        for i in range(1, len(d) - 1):
            if d[i] > thr and d[i] >= d[i - 1] and d[i] > d[i + 1] and i - last > 4:
                peaks += 1
                last = i
        m["onsets_per_s"] = peaks / dur if dur > 0 else 0.0
    else:
        m["onsets_per_s"] = 0.0
    return m


def verdict(m: dict) -> list:
    """Lista de fallos. Vacia = en verde."""
    g = GATES
    bad = []
    if m["span_db"] < g["span_db_min"]:
        bad.append(f"PLANA: rango dinamico {m['span_db']:.1f} dB "
                   f"(minimo {g['span_db_min']}) - faltan secciones o capas que descansen")
    if m["span_db"] > g["span_db_max"]:
        bad.append(f"IRREGULAR: rango dinamico {m['span_db']:.1f} dB "
                   f"(maximo {g['span_db_max']}) - hay tramos que se van a nada")
    if m["seam_db"] > g["seam_db_max"]:
        bad.append(f"COSTURA: {m['seam_db']:.1f} dB de salto entre el final y el principio")
    if m["quiet_dbfs"] < g["quiet_dbfs_min"]:
        bad.append(f"HUECO: hay 2 s a {m['quiet_dbfs']:.1f} dBFS")
    if m["corr"] > g["corr_max"]:
        bad.append(f"MONO: correlacion L/R {m['corr']:.3f}")
    if not (g["centroid_min"] <= m["centroid"] <= g["centroid_max"]):
        bad.append(f"TIMBRE: centroide {m['centroid']:.0f} Hz fuera de "
                   f"{g['centroid_min']:.0f}-{g['centroid_max']:.0f}")
    if m["bands"]["low"] < g["low_share_min"]:
        bad.append(f"SIN CUERPO: solo el {m['bands']['low'] * 100:.1f}% bajo 200 Hz "
                   f"(minimo {g['low_share_min'] * 100:.0f}%)")
    if m["bands"]["high"] < g["high_share_min"]:
        bad.append(f"SIN AIRE: solo el {m['bands']['high'] * 100:.1f}% sobre 2 kHz "
                   f"(minimo {g['high_share_min'] * 100:.0f}%)")
    for k, v in m["bands"].items():
        if v > g["band_max_share"]:
            bad.append(f"BANDA {k}: se lleva el {v * 100:.0f}% de la energia")
        if v < g["band_min_share"]:
            bad.append(f"BANDA {k}: solo el {v * 100:.1f}% de la energia")
    if m["onsets_per_s"] < g["onsets_min"]:
        bad.append(f"CAPA MUERTA?: {m['onsets_per_s']:.2f} ataques/s "
                   f"(minimo {g['onsets_min']}) - revisa los nombres de sample")
    if m["onsets_per_s"] > g["onsets_max"]:
        bad.append(f"SATURADA: {m['onsets_per_s']:.2f} ataques/s")
    if m["peak"] > 0.99:
        bad.append(f"CLIP: pico {m['peak']:.3f}")
    return bad


def report(x: np.ndarray, sr: int, name: str) -> bool:
    m = measure(x, sr)
    b = m["bands"]
    print(f"  [{name}] {m['dur']:.1f}s span={m['span_db']:.1f}dB "
          f"seam={m['seam_db']:.1f}dB corr={m['corr']:.3f} "
          f"centroide={m['centroid']:.0f}Hz "
          f"bandas={b['low'] * 100:.0f}/{b['mid'] * 100:.0f}/{b['high'] * 100:.0f} "
          f"onsets={m['onsets_per_s']:.2f}/s pico={m['peak']:.2f}", flush=True)
    bad = verdict(m)
    for line in bad:
        print(f"  GATE FALLA -> {line}", flush=True)
    if not bad:
        print("  gates: OK", flush=True)
    return not bad


def main(argv):
    import record  # aqui dentro: record importa analyze de forma perezosa
    paths = []
    for a in argv:
        if os.path.isdir(a):
            paths += [os.path.join(a, f) for f in sorted(os.listdir(a))
                      if f.endswith((".ogg", ".wav"))]
        else:
            paths.append(a)
    if not paths:
        print(__doc__)
        return 2
    fails = 0
    for p in paths:
        x, sr = record.decode_f32(p)
        if not report(x, sr, os.path.basename(p)):
            fails += 1
    print(f"\n{len(paths) - fails}/{len(paths)} pistas en verde")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
