"""DSP de la orquesta: filtros, ruido lento, osciladores de tabla, reverb de
convolucion con respuestas al impulso SINTETICAS y la cadena de mastering.

Todo trabaja a SR = 32 kHz, que es la frecuencia a la que se encodea la musica
del juego: sintetizar a 48 kHz para tirar la mitad del espectro al bajar seria
CPU tirada. Lo unico no lineal (saturacion de cinta) va sobremuestreado x2.

Convencion de los bucles: una pista es PERIODICA. Lo lineal (cuerpos de
instrumento, reverb) se calcula sobre el "one-shot" y se PLIEGA modulo el
periodo, que es exactamente el regimen estacionario de tocarla en bucle; lo no
lineal (compresor, limitador) se calcula con un precalentamiento circular
(`circular`). Resultado: la costura empalma a la muestra.
"""

import subprocess
import re

import numpy as np
from scipy import signal
from scipy.ndimage import minimum_filter1d, uniform_filter1d

SR = 32000
NYQ = SR / 2.0


# ============================================================================
# Utilidades
# ============================================================================

def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def undb(d):
    return 10.0 ** (np.asarray(d, dtype=np.float64) / 20.0)


def rng_for(*keys):
    """RNG determinista a partir de cadenas/enteros (crc32, no hash():
    el hash de Python cambia en cada proceso)."""
    import zlib
    s = "|".join(str(k) for k in keys).encode("utf-8")
    return np.random.default_rng(zlib.crc32(s) & 0xFFFFFFFF)


def pan_gains(pan):
    """Panorama de potencia constante. pan en [-1, 1]."""
    a = (np.clip(pan, -1.0, 1.0) + 1.0) * 0.25 * np.pi
    return np.cos(a), np.sin(a)


def smoothstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3.0 - 2.0 * u)


# ============================================================================
# Biquads (RBJ) -> sos de scipy
# ============================================================================

def _sos(b0, b1, b2, a0, a1, a2):
    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


def bq(kind, f, q=0.7071, gain_db=0.0, sr=SR):
    f = float(np.clip(f, 5.0, sr * 0.49))
    w0 = 2.0 * np.pi * f / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / (2.0 * q)
    A = 10.0 ** (gain_db / 40.0)
    if kind == "lp":
        return _sos((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha)
    if kind == "hp":
        return _sos((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha)
    if kind == "bp":      # pico de ganancia 1
        return _sos(alpha, 0.0, -alpha, 1 + alpha, -2 * cw, 1 - alpha)
    if kind == "peak":
        return _sos(1 + alpha * A, -2 * cw, 1 - alpha * A,
                    1 + alpha / A, -2 * cw, 1 - alpha / A)
    if kind in ("ls", "hs"):
        sa = 2.0 * np.sqrt(A) * alpha
        if kind == "ls":
            return _sos(A * ((A + 1) - (A - 1) * cw + sa),
                        2 * A * ((A - 1) - (A + 1) * cw),
                        A * ((A + 1) - (A - 1) * cw - sa),
                        (A + 1) + (A - 1) * cw + sa,
                        -2 * ((A - 1) + (A + 1) * cw),
                        (A + 1) + (A - 1) * cw - sa)
        return _sos(A * ((A + 1) + (A - 1) * cw + sa),
                    -2 * A * ((A - 1) + (A + 1) * cw),
                    A * ((A + 1) + (A - 1) * cw - sa),
                    (A + 1) - (A - 1) * cw + sa,
                    2 * ((A - 1) - (A + 1) * cw),
                    (A + 1) - (A - 1) * cw - sa)
    raise ValueError(kind)


def chain(*parts):
    parts = [p for p in parts if p is not None and len(p)]
    return np.vstack(parts) if parts else None


def eq(spec, sr=SR):
    """Cadena de EQ desde una lista compacta:
    [("hp", f, q), ("peak", f, gain_db, q), ("lp", f, q), ("hs", f, gain_db), ...]"""
    out = []
    for s in spec:
        k = s[0]
        if k in ("lp", "hp", "bp"):
            out.append(bq(k, s[1], s[2] if len(s) > 2 else 0.7071, sr=sr))
        elif k == "peak":
            out.append(bq("peak", s[1], s[3] if len(s) > 3 else 1.0, s[2], sr=sr))
        elif k in ("ls", "hs"):
            out.append(bq(k, s[1], s[3] if len(s) > 3 else 0.7071, s[2], sr=sr))
        elif k == "lp4":
            out.append(signal.butter(4, s[1], "low", fs=sr, output="sos"))
        elif k == "hp4":
            out.append(signal.butter(4, s[1], "high", fs=sr, output="sos"))
        else:
            raise ValueError(k)
    return chain(*out)


def filt(x, sos, axis=0):
    if sos is None:
        return x
    return signal.sosfilt(sos, x, axis=axis)


def onepole_lp(x, fc, sr=SR, axis=0):
    a = np.exp(-2.0 * np.pi * fc / sr)
    return signal.lfilter([1.0 - a], [1.0, -a], x, axis=axis)


def tv_filter(x, kind, fc, q=0.7071, block=128, sr=SR):
    """Filtro de 2o orden con frecuencia de corte VARIABLE en el tiempo
    (por bloques, arrastrando el estado). x: (n,) o (n, c); fc: array (n,).
    Es lo que hace que un metal se abra al subir la dinamica."""
    n = x.shape[0]
    y = np.empty_like(x)
    zi = None
    for i in range(0, n, block):
        j = min(n, i + block)
        sos = bq(kind, float(fc[min(n - 1, (i + j) // 2)]), q, sr=sr)
        if zi is None:
            zi = np.zeros((1, 2) + x.shape[1:])
        y[i:j], zi = signal.sosfilt(sos, x[i:j], axis=0, zi=zi)
    return y


# ============================================================================
# Senales de control
# ============================================================================

def slow_noise(n, rate_hz, rng, sr=SR):
    """Ruido suave de media 0 y desviacion ~1, con puntos de control cada
    1/rate segundos e interpolacion coseno. Deriva de afinacion, rafagas de
    viento, el temblor del arco..."""
    if n <= 0:
        return np.zeros(0)
    step = max(1, int(sr / max(rate_hz, 1e-3)))
    k = n // step + 3
    pts = rng.standard_normal(k)
    pos = np.arange(n) / step
    i0 = pos.astype(np.int64)
    fr = pos - i0
    w = 0.5 - 0.5 * np.cos(np.pi * fr)
    return pts[i0] * (1.0 - w) + pts[i0 + 1] * w


def shift(x, s):
    """Desplaza un array s muestras (s>0 retrasa), rellenando con el borde."""
    if s == 0:
        return x
    y = np.empty_like(x)
    if s > 0:
        y[s:] = x[:-s]
        y[:s] = x[0]
    else:
        y[:s] = x[-s:]
        y[s:] = x[-1]
    return y


# ============================================================================
# Osciladores de tabla limitados en banda (mip-maps por medio octava)
# ============================================================================

class Wavetable:
    """Una forma de onda definida por su ESPECTRO armonico (amp_fn(n) -> a_n),
    guardada en tablas por medio octava con solo los armonicos que caben por
    debajo de 14,5 kHz. Sin aliasing, y cualquier espectro: arco, pulso
    glotal, lengueta doble, metal."""

    SIZE = 4096
    FMAX = 14500.0

    def __init__(self, amp_fn, phase_fn=None, seed=0):
        self.levels = []          # (f_hi, tabla)
        k = np.arange(self.SIZE) / self.SIZE
        rng = np.random.default_rng(seed)
        nmax = int(self.FMAX / 16.0)
        amps = np.array([amp_fn(n) for n in range(1, nmax + 1)], dtype=np.float64)
        phases = (np.array([phase_fn(n, rng) for n in range(1, nmax + 1)])
                  if phase_fn else np.zeros(nmax))
        f_hi = 16.0 * 2 ** 0.5
        while f_hi < 9000.0:
            nh = max(1, min(nmax, int(self.FMAX / f_hi)))
            # Suma por FFT inversa: rapido aunque haya cientos de armonicos.
            spec = np.zeros(self.SIZE // 2 + 1, dtype=np.complex128)
            spec[1:nh + 1] = amps[:nh] * np.exp(1j * (phases[:nh] - np.pi / 2))
            tab = np.fft.irfft(spec, self.SIZE) * (self.SIZE / 2)
            self.levels.append((f_hi, np.append(tab, tab[0])))
            f_hi *= 2 ** 0.5
        peak = max(np.max(np.abs(t)) for _, t in self.levels)
        self.levels = [(f, t / peak) for f, t in self.levels]
        del k

    def table_for(self, fmax):
        for f_hi, t in self.levels:
            if fmax <= f_hi:
                return t
        return self.levels[-1][1]

    def render(self, phase, fmax):
        """phase en ciclos (float64, puede crecer sin limite)."""
        t = self.table_for(fmax)
        p = (phase - np.floor(phase)) * self.SIZE
        i = p.astype(np.int64)
        fr = p - i
        return t[i] + fr * (t[i + 1] - t[i])


def phase_from_freq(f, phase0=0.0, sr=SR):
    ph = np.cumsum(f, dtype=np.float64) / sr
    return ph + phase0


def midi_hz(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=np.float64) - 69.0) / 12.0)


# ============================================================================
# Reverb de convolucion: respuestas al impulso sinteticas
# ============================================================================

# Bandas de decaimiento: (frecuencia de corte superior de la banda, factor del T60)
_BANDS = [(150, 1.25), (400, 1.12), (1000, 1.0), (2500, 0.86), (5000, 0.66),
          (9000, 0.46), (16000, 0.32)]


def make_ir(t60=2.6, length=None, predelay=0.022, er_n=14, er_span=0.09,
            er_gain=0.55, damp=1.0, low_mult=1.0, onset=0.035, seed=1,
            bright=1.0, sr=SR):
    """IR estereo (4 canales: LL, LR, RL, RR) de una sala sintetica.

    - Cola difusa: ruido gaussiano DECORRELADO por canal, partido en bandas,
      cada una con su propio T60 (los agudos mueren antes: `damp` < 1 los
      acorta mas, `bright` > 1 los alarga -> piedra).
    - Reflexiones tempranas: `er_n` ecos dispersos en `er_span` segundos, cada
      uno algo filtrado y con su propio sitio en el estereo.
    - Rampa de entrada (`onset`) para que la cola no arranque de golpe.
    """
    rng = np.random.default_rng(seed)
    if length is None:
        length = min(6.5, t60 * low_mult * 1.25 * 1.15 + predelay)
    n = int(length * sr)
    t = np.arange(n) / sr
    out = np.zeros((n, 4))
    lo = 20.0
    edges = []
    for hi, mult in _BANDS:
        if lo >= sr * 0.49:
            break
        edges.append((lo, min(hi, sr * 0.49), mult))
        lo = hi
    for ch in range(4):
        noise = rng.standard_normal(n)
        tail = np.zeros(n)
        for i, (fl, fh, mult) in enumerate(edges):
            if fl <= 25:
                sos = signal.butter(4, fh, "low", fs=sr, output="sos")
            elif fh >= sr * 0.48:
                sos = signal.butter(4, fl, "high", fs=sr, output="sos")
            else:
                sos = signal.butter(2, [fl, fh], "band", fs=sr, output="sos")
            band = signal.sosfilt(sos, noise)
            m = mult
            if i == 0:
                m *= low_mult
            if fl >= 2500:
                m = m * (damp if damp < 1 else 1.0) * (bright if bright > 1 else 1.0)
            T = max(0.08, t60 * m)
            tail += band * np.exp(-6.9078 * t / T)
        tail *= 1.0 - np.exp(-np.maximum(t - predelay, 0) / max(onset, 1e-3))
        tail[t < predelay] = 0.0
        out[:, ch] = tail
    # Normaliza la energia de la cola (cada canal a energia 1)
    for ch in range(4):
        e = np.sqrt(np.sum(out[:, ch] ** 2))
        if e > 0:
            out[:, ch] /= e
    # Reflexiones tempranas
    er = np.zeros((n, 4))
    lp = bq("lp", 6000)
    for k in range(er_n):
        dt = predelay * 0.4 + (k + rng.random()) / er_n * er_span
        idx = int(dt * sr)
        if idx >= n:
            continue
        g = er_gain * (0.85 ** k) * (0.6 + 0.4 * rng.random())
        side = rng.random()
        pol = 1.0 if rng.random() > 0.3 else -1.0
        er[idx, 0] += pol * g * (0.5 + side)
        er[idx, 3] += pol * g * (1.5 - side)
        er[idx, 1] += pol * g * 0.35 * rng.random()
        er[idx, 2] += pol * g * 0.35 * rng.random()
    er = signal.sosfilt(lp, er, axis=0)
    ir = out * 0.9 + er * 0.25
    # Los canales cruzados (L->R, R->L) un poco por debajo: conserva la imagen
    ir[:, 1] *= 0.62
    ir[:, 2] *= 0.62
    return ir.astype(np.float64)


def conv_circular(x, h, period):
    """Convolucion CIRCULAR de periodo `period` (x ya plegado, longitud period).
    Es el regimen estacionario exacto de la reverb de una pista en bucle."""
    y = signal.oaconvolve(x, h, mode="full")
    out = y[:period].copy()
    k = period
    while k < len(y):
        seg = y[k:k + period]
        out[:len(seg)] += seg
        k += period
    return out


def reverb_stereo(x, ir, period):
    """x: (period, 2). ir: (n, 4) [LL, LR, RL, RR]."""
    L, R = x[:, 0], x[:, 1]
    oL = conv_circular(L, ir[:, 0], period) + conv_circular(R, ir[:, 2], period)
    oR = conv_circular(L, ir[:, 1], period) + conv_circular(R, ir[:, 3], period)
    return np.stack([oL, oR], axis=1)


def fold(x, period):
    """Pliega un one-shot de longitud >= period modulo el periodo."""
    out = x[:period].copy()
    k = period
    while k < len(x):
        seg = x[k:k + period]
        out[:len(seg)] += seg
        k += period
    return out


def circular(fn, x, warm):
    """Aplica un proceso con memoria (no lineal o IIR) a un bucle como si
    sonara en bucle: se precalienta con el final del propio bucle."""
    warm = min(warm, len(x))
    ext = np.concatenate([x[-warm:], x], axis=0)
    y = fn(ext)
    return y[warm:]


# ============================================================================
# Sonoridad (BS.1770-4) y pico verdadero
# ============================================================================

def _kweight_sos(sr=SR):
    # Coeficientes del filtro K a cualquier frecuencia de muestreo (los mismos
    # prototipos que usa pyloudnorm).
    G, Q, fc = 3.99984385397, 0.7071752369554193, 1681.974450955533
    A = 10 ** (G / 40.0)
    w0 = 2 * np.pi * fc / sr
    alpha = np.sin(w0) / (2 * Q)
    cw = np.cos(w0)
    sa = 2 * np.sqrt(A) * alpha
    s1 = _sos(A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw),
              A * ((A + 1) + (A - 1) * cw - sa), (A + 1) - (A - 1) * cw + sa,
              2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa)
    Q2, fc2 = 0.5003270373253953, 38.13547087613982
    w0 = 2 * np.pi * fc2 / sr
    alpha = np.sin(w0) / (2 * Q2)
    cw = np.cos(w0)
    s2 = _sos((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha)
    return np.vstack([s1, s2])


def lufs(x, sr=SR):
    """Sonoridad integrada (LUFS) con las dos compuertas de la norma."""
    y = signal.sosfilt(_kweight_sos(sr), x, axis=0)
    blk = int(0.4 * sr)
    hop = int(0.1 * sr)
    if len(y) < blk:
        z = np.array([np.sum(np.mean(y ** 2, axis=0))])
    else:
        sq = np.sum(y ** 2, axis=1)
        cs = np.concatenate([[0.0], np.cumsum(sq)])
        starts = np.arange(0, len(y) - blk + 1, hop)
        z = (cs[starts + blk] - cs[starts]) / blk
    lk = -0.691 + 10 * np.log10(np.maximum(z, 1e-20))
    g1 = z[lk > -70.0]
    if len(g1) == 0:
        return -99.0
    rel = -0.691 + 10 * np.log10(np.mean(g1)) - 10.0
    g2 = z[(lk > -70.0) & (lk > rel)]
    return float(-0.691 + 10 * np.log10(np.mean(g2)))


def true_peak(x):
    up = signal.resample_poly(x, 4, 1, axis=0)
    return float(np.max(np.abs(up)))


def lufs_ffmpeg(x, sr=SR):
    p = subprocess.run(
        ["ffmpeg", "-v", "info", "-f", "f32le", "-ar", str(sr), "-ac", str(x.shape[1]),
         "-i", "-", "-filter:a", "ebur128=framelog=quiet:peak=true", "-f", "null", "-"],
        input=x.astype(np.float32).tobytes(), stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE)
    txt = p.stderr.decode("utf-8", "replace")
    m = re.findall(r"I:\s+(-?\d+\.\d+)\s+LUFS", txt)
    pk = re.findall(r"Peak:\s+(-?\d+\.\d+)\s+dBFS", txt)
    return (float(m[-1]) if m else None, float(pk[-1]) if pk else None)


# ============================================================================
# Mastering
# ============================================================================

def compressor(x, thr_db, ratio=2.0, attack=0.025, release=0.30, knee=6.0,
               block=64, sr=SR):
    """Compresor de bus (detector RMS estereo enlazado, diezmado por bloques).
    Devuelve la senal y la reduccion media (dB)."""
    n = len(x)
    nb = (n + block - 1) // block
    pad = nb * block - n
    m = np.mean(x ** 2, axis=1)
    if pad:
        m = np.concatenate([m, np.zeros(pad)])
    pw = m.reshape(nb, block).mean(axis=1)
    # RMS suavizado ~10 ms
    pw = onepole_lp(pw, 1.0 / 0.010 / (2 * np.pi), sr=sr / block)
    lvl = 10 * np.log10(np.maximum(pw, 1e-12))
    over = lvl - thr_db
    gr = np.where(over <= -knee / 2, 0.0,
                  np.where(over >= knee / 2, over * (1 - 1 / ratio),
                           (1 - 1 / ratio) * (over + knee / 2) ** 2 / (2 * knee)))
    # Ataque/relajacion sobre la reduccion (en dB), recursivo por bloques
    fs_b = sr / block
    ka = np.exp(-1.0 / (attack * fs_b))
    kr = np.exp(-1.0 / (release * fs_b))
    g = np.empty(nb)
    s = 0.0
    for i in range(nb):
        v = gr[i]
        k = ka if v > s else kr
        s = k * s + (1 - k) * v
        g[i] = s
    gl = undb(-g)
    gs = np.interp(np.arange(n), np.arange(nb) * block + block / 2, gl)
    return x * gs[:, None], float(np.mean(g))


def tape(x, drive=1.0):
    """Saturacion de cinta suave (tanh) sobremuestreada x2."""
    if drive <= 0:
        return x
    up = signal.resample_poly(x, 2, 1, axis=0)
    y = np.tanh(up * drive) / drive
    return signal.resample_poly(y, 1, 2, axis=0)


def limiter(x, ceiling, look=0.0025, release=0.06, sr=SR):
    """Limitador de pico VERDADERO con anticipacion: detecta sobre la senal
    sobremuestreada x4 y nunca deja pasar un pico por encima de `ceiling`."""
    up = np.abs(signal.resample_poly(x, 4, 1, axis=0))
    n = len(x)
    pk = up[:n * 4].reshape(n, 4, -1).max(axis=(1, 2))
    need = np.minimum(1.0, ceiling / np.maximum(pk, 1e-9))
    L = max(2, int(look * sr))
    g = minimum_filter1d(need, size=2 * L + 1 + int(release * sr * 0.5), mode="wrap")
    g = uniform_filter1d(g, size=L, mode="wrap")
    g = np.minimum(g, need)       # garantia dura
    return x * g[:, None], float(20 * np.log10(np.min(g)))
