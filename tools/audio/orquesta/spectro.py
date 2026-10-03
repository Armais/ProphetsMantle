"""Espectrogramas en PNG (PIL) para revisar a ojo lo que no se puede escuchar:
secciones, ataques, clips, continua, costuras.

  python spectro.py <fichero.ogg|wav> [salida.png] [--seam]

Eje vertical logaritmico (40 Hz - 16 kHz), color por dB. Debajo, la
envolvente RMS (L en verde, R en magenta) y una marca roja si una muestra pasa
de -1 dBFS. Con --seam se pinta tambien el final+principio pegados (la costura).
"""

import os
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

SR_DEF = 32000


def load(path):
    if path.endswith(".wav"):
        from scipy.io import wavfile
        sr, x = wavfile.read(path)
        x = x.astype(np.float32)
        if x.dtype != np.float32 or np.max(np.abs(x)) > 2:
            x = x / 32768.0
        return (x if x.ndim == 2 else x[:, None]), sr
    out = subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "a:0",
                                   "-show_entries", "stream=sample_rate,channels",
                                   "-of", "csv=p=0", path], text=True).strip().split(",")
    sr, ch = int(out[0]), int(out[1])
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-"])
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, ch), sr


def colormap(v):
    # v en [0,1] -> negro, azul, magenta, naranja, amarillo, blanco
    stops = np.array([[0, 0, 0], [20, 20, 90], [120, 30, 140], [230, 90, 40],
                      [250, 210, 60], [255, 255, 255]], dtype=np.float64)
    pos = np.clip(v, 0, 1) * (len(stops) - 1)
    i = np.minimum(len(stops) - 2, pos.astype(int))
    f = (pos - i)[..., None]
    return (stops[i] * (1 - f) + stops[i + 1] * f).astype(np.uint8)


def spectrogram(x, sr, width=1400, height=420, fmin=40.0, fmax=16000.0, floor_db=-90):
    mono = x.mean(axis=1)
    n = len(mono)
    nfft = 4096
    hop = max(1, (n - nfft) // width)
    win = np.hanning(nfft)
    cols = []
    for i in range(width):
        s = i * hop
        seg = mono[s:s + nfft]
        if len(seg) < nfft:
            seg = np.pad(seg, (0, nfft - len(seg)))
        cols.append(np.abs(np.fft.rfft(seg * win)))
    S = np.array(cols).T
    S = 20 * np.log10(S / (nfft / 4) + 1e-9)
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
    fy = np.exp(np.linspace(np.log(fmax), np.log(fmin), height))
    idx = np.clip(np.searchsorted(freqs, fy), 0, len(freqs) - 1)
    img = S[idx, :]
    v = (img - floor_db) / (-floor_db)
    return colormap(v)


def render_png(x, sr, out, title=""):
    W, H = 1400, 420
    spec = spectrogram(x, sr, W, H)
    env_h = 120
    canvas = Image.new("RGB", (W, H + env_h + 24), (16, 16, 16))
    canvas.paste(Image.fromarray(spec), (0, 24))
    d = ImageDraw.Draw(canvas)
    d.text((6, 4), f"{title}  {len(x) / sr:.1f}s  pico {np.max(np.abs(x)):.3f}", fill=(230, 230, 230))
    for f in (100, 1000, 2000, 5000, 10000):
        y = 24 + int((np.log(16000) - np.log(f)) / (np.log(16000) - np.log(40)) * H)
        d.line([(0, y), (12, y)], fill=(200, 200, 200))
        d.text((14, y - 6), f"{f}", fill=(200, 200, 200))
    n = len(x)
    hop = max(1, n // W)
    for ch, col in ((0, (90, 220, 90)), (1, (220, 90, 220))):
        pts = []
        for i in range(W):
            seg = x[i * hop:(i + 1) * hop, min(ch, x.shape[1] - 1)]
            r = np.sqrt(np.mean(seg ** 2)) if len(seg) else 0
            dbv = 20 * np.log10(max(r, 1e-6))
            yy = 24 + H + env_h - int(np.clip((dbv + 60) / 60, 0, 1) * env_h)
            pts.append((i, yy))
            if len(seg) and np.max(np.abs(seg)) > 0.891:
                d.line([(i, 24 + H), (i, 24 + H + 6)], fill=(255, 0, 0))
        d.line(pts, fill=col)
    canvas.save(out)


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    path = argv[0]
    out = argv[1] if len(argv) > 1 and not argv[1].startswith("--") else \
        os.path.splitext(path)[0] + ".png"
    x, sr = load(path)
    if "--seam" in argv:
        k = int(3 * sr)
        x = np.concatenate([x[-k:], x[:k]])
    render_png(x, sr, out, os.path.basename(path))
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
