"""Instrumentos de la orquesta de MANTLE: sintesis propia, sin una muestra.

Cada familia tiene su modelo:

  arco      cuerdas (secciones y solistas): tabla con el espectro de Helmholtz
            del arco (con las muescas del punto de contacto), MUCHAS voces por
            seccion con vibrato y deriva independientes, legato con portamento,
            ruido de arco, brillo que sigue a la dinamica y un CUERPO MODAL
            (decenas de resonancias aleatorias alrededor de los picos del
            violin) por convolucion: el vibrato barre esas resonancias y de ahi
            sale el brillo cambiante que separa una cuerda de una sierra.
  coro      pulso glotal (tabla) con jitter, shimmer y vibrato por cantante,
            8-12 voces por cuerda, banco de FORMANTES por vocal y tesitura
            (tablas de soprano/contralto/tenor/bajo), morph de vocal y aliento.
  viento    duduk, ney, kaval, shofar: fuente + ruido de soplo (parte
            enganchada al periodo), cuerpo formantico, ataques con portamento
            desde abajo, vibrato que ENTRA tarde en las notas largas.
  metal     trompas/metales graves: el filtro se abre con la dinamica.
  pulsada   oud, kanun, arpa, lira, pizzicato: Karplus-Strong de verdad con
            filtro de lazo de 3 coeficientes (fase lineal, afinacion exacta
            remuestreando), excitacion con posicion de pulsacion, cuerpo.
  parche    tar/daf, darbuka, riq, taikos de guerra, timbales, zils, shaker,
            palmas, gong: sintesis MODAL (modos de membrana circular, Bessel)
            con caida por modo, glissando de tension y ruido de contacto.
  metalofono campanas, celesta, crotalos: modos inarmonicos.
  bordon    tanpura (jawari: el pico espectral que barre los armonicos).
  ambiente  viento, mar, brasas: ruido filtrado con movimiento.

Lo LINEAL de cada instrumento (cuerpo, formantes, EQ) se aplica una vez por
PARTE, sobre la suma de sus notas: es la misma cuenta y cuesta una fraccion.
"""

import zlib

import numpy as np
from scipy import signal

import dsp
from dsp import SR

CENT = np.log(2.0) / 1200.0


# ============================================================================
# Eventos
# ============================================================================

class Ev:
    """Una nota en SEGUNDOS (ya humanizada y plegada al periodo).
    p: altura MIDI (float, admite cuartos de tono) o golpe de percusion (str).
    fl: dict de banderas (articulacion, adornos, voz...)."""
    __slots__ = ("t", "d", "p", "v", "fl")

    def __init__(self, t, d, p, v, fl=None):
        self.t, self.d, self.p, self.v = t, d, p, v
        self.fl = fl or {}

    def __repr__(self):
        return f"Ev({self.t:.3f},{self.d:.3f},{self.p},{self.v:.2f})"


# ============================================================================
# Espectros (tablas de onda)
# ============================================================================

def _bow_amp(n, beta=0.113):
    # Movimiento de Helmholtz: 1/n, con las muescas del punto de contacto del
    # arco (los armonicos multiplos de 1/beta casi desaparecen).
    return (abs(np.sin(np.pi * n * beta)) + 0.12) / n


def _glottal_amp(n):
    return n ** -1.2


def _duduk_amp(n):
    # Tubo cilindrico de lengueta doble: impares por delante en el registro
    # grave, y una caida rapida que lo deja oscuro y nasal.
    base = n ** -1.2
    return base * (1.7 if n % 2 else 0.75) * (1.0 if n > 1 else 1.25)


def _ney_amp(n):
    return [1.0, 0.30, 0.13, 0.07, 0.035, 0.02][n - 1] if n <= 6 else 0.012 / (n - 5)


def _kaval_amp(n):
    return [1.0, 0.48, 0.30, 0.17, 0.11, 0.07, 0.045][n - 1] if n <= 7 else 0.03 / (n - 6)


def _brass_amp(n):
    return n ** -0.72


def _shofar_amp(n):
    return n ** -0.8 * (1.3 if n % 2 else 1.0)


def _voice_amp(n):
    return n ** -1.05


_WT = {}


def wt(name):
    if name not in _WT:
        fn = {"bow": _bow_amp, "glottal": _glottal_amp, "duduk": _duduk_amp,
              "ney": _ney_amp, "kaval": _kaval_amp, "brass": _brass_amp,
              "shofar": _shofar_amp, "voice": _voice_amp}[name]
        _WT[name] = dsp.Wavetable(fn)
    return _WT[name]


# ============================================================================
# Controles de una frase (altura, amplitud, vibrato) - comun a arco/voz/viento
# ============================================================================

DEF_LINE = dict(
    attack=0.12, release=0.35, glide=0.07, slide=0.22, dip=0.10, dip_t=0.07,
    legato_gap=0.06, scoop=0.0, scoop_t=0.09, vib_delay=0.18, vib_rise=0.45,
    vib_cont=True, swell=0.0, swell_min=1.2, gamma=1.6, sag=0.0, stacc=False,
    stacc_hold=0.10, stacc_rel=0.09, overshoot=0.0,
)


def build_line(notes, P):
    """Curvas de control de una frase monofonica. notes: lista de Ev.
    Devuelve (off, pitch, amp, vib, onsets) con off = muestra de inicio."""
    pre = max(P["scoop_t"], P["glide"]) + 0.02
    t0 = notes[0].t - pre
    t_end = max(e.t + e.d for e in notes) + P["release"] * 3.0 + 0.05
    off = int(np.floor(t0 * SR))
    n = int(np.ceil(t_end * SR)) - off
    pitch = np.empty(n)
    amp = np.zeros(n)
    vib = np.zeros(n)
    onsets = []

    idx = lambda t: int(round(t * SR)) - off
    nn = len(notes)
    conn = [False] * nn      # conn[i]: la nota i se liga con la i+1
    for i in range(nn - 1):
        a, b = notes[i], notes[i + 1]
        gap = b.t - (a.t + a.d)
        conn[i] = (not P["stacc"]) and gap < P["legato_gap"] and \
            not a.fl.get("sep") and not b.fl.get("sep")

    pitch[:] = notes[0].p
    prev_level = 0.0
    for i, e in enumerate(notes):
        s = max(0, idx(e.t))
        e_end = max(s + 1, idx(e.t + e.d))
        nxt = notes[i + 1] if i + 1 < nn else None
        lvl = max(e.v, 0.0) ** P["gamma"] * e.fl.get("amp", 1.0)
        ln_prev = i > 0 and conn[i - 1]
        stop = idx(nxt.t) if (nxt is not None and conn[i]) else e_end
        stop = max(stop, s + 1)
        seg_n = stop - s
        # ---- altura ----
        pitch[s:] = e.p
        if ln_prev:
            g = e.fl.get("glide", P["slide"] if e.fl.get("slide") else P["glide"])
            gn = max(2, int(g * SR))
            a0 = max(0, s - int(gn * 0.35))
            b0 = min(n, a0 + gn)
            u = np.linspace(0, 1, b0 - a0)
            pitch[a0:b0] = notes[i - 1].p + (e.p - notes[i - 1].p) * dsp.smoothstep(u)
        else:
            sc = e.fl.get("scoop", P["scoop"])
            if sc:
                st = int(e.fl.get("scoop_t", P["scoop_t"]) * SR)
                st = max(2, min(st, seg_n))
                u = np.linspace(0, 1, st)
                pitch[s:s + st] = e.p - sc / 100.0 * (1.0 - dsp.smoothstep(u)) ** 1.5
        if e.fl.get("fall"):
            ft = int(0.16 * SR)
            a0 = max(s, stop - ft)
            u = np.linspace(0, 1, stop - a0)
            pitch[a0:stop] = e.p - e.fl["fall"] * u ** 2
        # ---- amplitud ----
        seg = np.full(seg_n, lvl)
        dur = e.d
        if P["swell"] and dur >= P["swell_min"] and not P["stacc"]:
            u = np.linspace(0, 1, seg_n)
            sw = P["swell"] * e.fl.get("swell", 1.0)
            seg *= (1 - sw) + sw * np.sin(np.pi * (0.12 + 0.76 * u)) ** 0.8
        if e.fl.get("cresc"):
            u = np.linspace(0, 1, seg_n)
            c = e.fl["cresc"]          # >0 crescendo, <0 diminuendo
            seg *= (1 - abs(c)) + abs(c) * (u if c > 0 else 1 - u)
        if P["stacc"]:
            hold = int(min(dur, P["stacc_hold"] + 0.4 * dur) * SR)
            rel = int(P["stacc_rel"] * SR)
            env = np.zeros(seg_n)
            h = min(hold, seg_n)
            env[:h] = 1.0
            if h < seg_n:
                r = np.arange(seg_n - h)
                env[h:] = np.exp(-r / max(rel, 1) * 3.0)
            seg *= env
        if ln_prev:
            tr = max(2, int(0.06 * SR))
            tr = min(tr, seg_n)
            u = dsp.smoothstep(np.linspace(0, 1, tr))
            seg[:tr] = prev_level + (seg[:tr] - prev_level) * u
            dp = P["dip"] * e.fl.get("dip", 1.0)
            if dp > 0 and not e.fl.get("slur"):
                dn = min(seg_n, max(2, int(P["dip_t"] * SR)))
                seg[:dn] *= 1.0 - dp * np.sin(np.pi * np.linspace(0, 1, dn))
        else:
            att = e.fl.get("attack", P["attack"])
            if e.fl.get("acc"):
                att *= 0.35
            an = min(seg_n, max(2, int(att * SR)))
            u = np.linspace(0, 1, an)
            shape = 1.0 - np.exp(-4.0 * u)
            shape /= shape[-1]
            ov = P["overshoot"] + (0.35 if e.fl.get("acc") else 0.0)
            if ov:
                shape = shape * (1 + ov * np.exp(-3.0 * u) * u * 4.0)
            seg[:an] *= shape
            onsets.append((s, lvl))
        amp[s:stop] = seg
        prev_level = seg[-1]
        if not conn[i]:
            rel = e.fl.get("release", P["release"])
            if P["stacc"]:
                rel = 0.02
            rn = int(rel * 3.0 * SR)
            lim = idx(nxt.t) if nxt is not None else n
            r_end = min(n, stop + rn, max(stop, lim))
            if r_end > stop:
                r = np.arange(r_end - stop) / SR
                amp[stop:r_end] = prev_level * np.exp(-r / max(rel, 1e-3) * 1.6)
                if P["sag"]:
                    pitch[stop:r_end] = e.p - P["sag"] / 100.0 * (1 - np.exp(-r / 0.25))
            prev_level = 0.0
        # ---- vibrato ----
        vd = e.fl.get("vib", 1.0)
        if vd > 0:
            if P["vib_cont"] and ln_prev:
                vib[s:stop] = vd
            else:
                dl = int(min(P["vib_delay"], 0.45 * dur) * SR)
                rs = max(2, int(P["vib_rise"] * SR))
                a0 = min(stop, s + dl)
                k = np.arange(stop - a0)
                vib[a0:stop] = vd * np.minimum(1.0, k / rs)
            if not conn[i]:
                vib[stop:min(n, stop + int(0.5 * SR))] = vib[stop - 1] if stop > 0 else 0
    amp = dsp.onepole_lp(amp, 60.0)
    vib = dsp.onepole_lp(vib, 4.0)
    return off, pitch, amp, vib, onsets


# ============================================================================
# Cuerpos: IR modal (decenas de resonancias) + EQ
# ============================================================================

_BODY = {}


def body_ir(name, peaks, n_modes=70, f_lo=180.0, f_hi=6000.0, q=(15, 40),
            depth=0.6, length=0.30, seed=7):
    """Respuesta al impulso de una caja de resonancia.

    Dos capas: la ENVOLVENTE (picos de `peaks` [(f, dB, ancho_oct)], caida por
    debajo de f_lo y por encima de f_hi) es la que da el color medio del
    instrumento y se controla al decibelio; la ESTRUCTURA FINA sale de
    `n_modes` resonancias aleatorias (Q 15-40) y se aplica con profundidad
    `depth` (0,5 en secciones, que promedian muchos instrumentos; ~1 en un
    solista). Al hacer vibrato, los armonicos barren esa estructura fina: es el
    brillo vivo que separa una cuerda de una sierra filtrada."""
    key = (name, seed, depth)
    if key in _BODY:
        return _BODY[key]
    rng = np.random.default_rng(seed)
    nfft = 1 << 14
    t = np.arange(nfft) / SR
    fr = 2 ** rng.uniform(np.log2(f_lo * 0.7), np.log2(min(f_hi * 1.4, 12000)), n_modes)
    acc = np.zeros(nfft)
    for f in fr:
        Q = rng.uniform(*q)
        tau = Q / (np.pi * f)
        acc += 10 ** (rng.normal(0, 3.0) / 20) * np.exp(-t / tau) * \
            np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)) * (2 * np.pi * f / SR)
    acc[0] += 0.15
    H = np.fft.rfft(acc)
    mag = np.abs(H) + 1e-9
    k = np.arange(len(mag))
    cs = np.concatenate([[0.0], np.cumsum(mag)])
    lo = np.clip((k / 2 ** (1 / 6)).astype(int), 0, len(mag) - 1)
    hi = np.clip((k * 2 ** (1 / 6)).astype(int) + 1, 1, len(mag))
    smooth = (cs[hi] - cs[lo]) / np.maximum(1, hi - lo)
    fine = (mag / smooth) ** depth
    f = np.fft.rfftfreq(nfft, 1.0 / SR)
    fl = np.maximum(f, 1.0)
    T = np.zeros_like(f)
    for (pf, pdb, pw) in peaks:
        T += pdb * np.exp(-0.5 * (np.log2(fl / pf) / (pw * 0.6)) ** 2)
    T -= 12.0 * np.maximum(0, np.log2(f_lo / fl))
    T -= 12.0 * np.maximum(0, np.log2(fl / f_hi))
    Hn = fine * 10 ** (T / 20.0) * np.exp(1j * np.angle(H))
    ir = np.fft.irfft(Hn, nfft)[:int(length * SR)]
    w = int(0.05 * SR)
    ir[-w:] *= np.linspace(1, 0, w) ** 2
    band = (f > 200) & (f < 4000)
    g = np.sqrt(np.mean(np.abs(np.fft.rfft(ir, nfft))[band] ** 2))
    ir /= max(g, 1e-9)
    _BODY[key] = ir
    return ir


BODIES = {
    # violin: A0 ~280, B1- ~450, B1+ ~550, joroba del puente ~2,5 kHz
    "violin": dict(peaks=[(285, 6, 0.25), (460, 5, 0.3), (560, 4, 0.3), (1100, 0, 0.5),
                          (2600, 2.5, 0.5)], f_lo=200, f_hi=4800),
    "viola": dict(peaks=[(230, 6, 0.3), (380, 5, 0.3), (480, 3, 0.3), (1000, 0, 0.5),
                         (2000, 2.5, 0.5)], f_lo=150, f_hi=4200),
    "cello": dict(peaks=[(105, 5, 0.3), (190, 6, 0.3), (400, 4, 0.4), (750, 1, 0.5),
                         (1500, 3, 0.5)], f_lo=80, f_hi=4500),
    "bass": dict(peaks=[(60, 5, 0.3), (110, 6, 0.3), (220, 3, 0.4), (500, 1, 0.5),
                        (1000, 2, 0.5)], f_lo=45, f_hi=3000),
    "oud": dict(peaks=[(110, 4, 0.4), (220, 6, 0.4), (450, 3, 0.4), (1300, 2, 0.6),
                       (2600, 1, 0.5)], f_lo=80, f_hi=5000),
    "kanun": dict(peaks=[(300, 3, 0.4), (900, 3, 0.5), (2200, 5, 0.6), (4000, 3, 0.5)],
                  f_lo=200, f_hi=8000),
    "harp": dict(peaks=[(180, 4, 0.5), (400, 3, 0.5), (1200, 2, 0.6), (2600, 1, 0.5)],
                 f_lo=100, f_hi=6000),
    "lyre": dict(peaks=[(240, 5, 0.4), (520, 4, 0.4), (1500, 3, 0.5), (3000, 2, 0.5)],
                 f_lo=150, f_hi=6000),
}


def apply_body(x, name, seed=7, wet=1.0, depth=0.6):
    spec = BODIES[name]
    ir = body_ir(name, spec["peaks"], f_lo=spec["f_lo"], f_hi=spec["f_hi"], seed=seed,
                 depth=depth)
    y = signal.oaconvolve(x, ir[:, None] if x.ndim == 2 else ir, mode="full", axes=0)[:len(x)]
    if wet < 1.0:
        y = x * (1 - wet) + y * wet
    return y


# ============================================================================
# ARCO: secciones y solistas
# ============================================================================

STRINGS = {
    #          voces  panorama          cuerpo    vib(Hz, cents) deriva  dispersion(ms) eq
    "vln1": dict(nv=8, pan=(-0.78, -0.28), body="violin", vib=(5.7, 13), drift=4.5,
                 spread=14, detune=5, eq=[("hp", 170), ("lp", 7500)], noise=0.018),
    "vln2": dict(nv=7, pan=(-0.42, 0.0), body="violin", vib=(5.5, 12), drift=4.5,
                 spread=14, detune=5, eq=[("hp", 160), ("lp", 7000)], noise=0.016),
    "vla": dict(nv=6, pan=(0.0, 0.42), body="viola", vib=(5.4, 12), drift=4.5,
                spread=15, detune=5, eq=[("hp", 110), ("lp", 7000)], noise=0.015),
    "vc": dict(nv=6, pan=(0.1, 0.5), body="cello", vib=(5.2, 11), drift=4.0,
               spread=15, detune=4.5, eq=[("hp", 55), ("lp", 6000)], noise=0.010),
    "cb": dict(nv=4, pan=(-0.05, 0.38), body="bass", vib=(5.0, 8), drift=3.5,
               spread=16, detune=4, eq=[("hp", 32), ("peak", 70, 2.0, 0.9),
                                        ("lp", 3500)], noise=0.006),
    "solo_vln": dict(nv=1, pan=(-0.12, -0.12), body="violin", vib=(5.9, 24), drift=2.5,
                     spread=0, detune=0, eq=[("hp", 180), ("peak", 2800, 1.0, 1.0),
                                             ("lp", 8500)], noise=0.02),
    "solo_vla": dict(nv=1, pan=(0.08, 0.08), body="viola", vib=(5.6, 22), drift=2.2,
                     spread=0, detune=0, eq=[("hp", 120), ("peak", 2200, 1.5, 1.0),
                                             ("lp", 8500)], noise=0.018),
    "solo_vc": dict(nv=1, pan=(0.1, 0.1), body="cello", vib=(5.4, 20), drift=2.0,
                    spread=0, detune=0, eq=[("hp", 60), ("peak", 1600, 1.5, 1.0),
                                            ("lp", 7500)], noise=0.018),
}

BOW_LINE = dict(DEF_LINE, attack=0.18, release=0.40, glide=0.06, slide=0.25, dip=0.12,
                dip_t=0.08, vib_delay=0.12, vib_rise=0.5, vib_cont=True, swell=0.18,
                swell_min=1.4, gamma=1.6)


def render_bowed(parts_lines, cfg, prm, rng, total_n):
    """parts_lines: lista de frases (cada una lista de Ev). Devuelve (total_n, 2)
    sin cuerpo (se aplica fuera, sobre la suma)."""
    out = np.zeros((total_n, 2))
    P = dict(BOW_LINE, **prm.get("line", {}))
    art = prm.get("art", "leg")
    if art in ("stac", "spic"):
        P.update(stacc=True, stacc_hold=0.05 if art == "spic" else 0.09,
                 stacc_rel=0.07 if art == "spic" else 0.10, attack=0.012)
    nv = prm.get("nv", cfg["nv"])
    vr, vc = cfg["vib"]
    vc = vc * prm.get("vib_mult", 1.0)
    w = wt("bow")
    pan_lo, pan_hi = prm.get("pan_range", cfg["pan"])
    bright_lo = prm.get("dark", 2800.0)
    trem = art == "trem"
    for phrase in parts_lines:
        off, pitch, amp, vib, onsets = build_line(phrase, P)
        n = len(pitch)
        fmax = dsp.midi_hz(np.max(pitch) + 0.6)
        fbase = dsp.midi_hz(pitch)
        Lb = np.zeros(n)
        Rb = np.zeros(n)
        for v in range(nv):
            sh = int(rng.normal(0, cfg["spread"]) * SR / 1000.0) if nv > 1 else 0
            sh = int(np.clip(sh, -0.04 * SR, 0.04 * SR))
            det = rng.normal(0, cfg["detune"])
            drift = dsp.slow_noise(n, 0.7, rng) * cfg["drift"]
            rate = vr * (1 + 0.07 * rng.standard_normal()) + 0.35 * dsp.slow_noise(n, 0.6, rng)
            vph = np.cumsum(rate) * (2 * np.pi / SR) + rng.uniform(0, 2 * np.pi)
            depth = vc * (0.7 + 0.6 * rng.random())
            cents = det + drift + depth * np.sin(vph) * dsp.shift(vib, sh)
            f = dsp.shift(fbase, sh) * (1.0 + cents * CENT)
            ph = np.cumsum(f) / SR + rng.random()
            y = w.render(ph, fmax)
            a = dsp.shift(amp, sh) * (1.0 + 0.07 * dsp.slow_noise(n, 2.5, rng))
            if trem:
                tr = 13.0 * (1 + 0.1 * rng.standard_normal())
                tph = np.cumsum(tr * (1 + 0.08 * dsp.slow_noise(n, 1.0, rng))) / SR
                a = a * (0.35 + 0.65 * np.abs(np.sin(np.pi * tph + rng.random() * 3)) ** 0.6)
            y *= a
            pn = pan_lo + (pan_hi - pan_lo) * ((v + 0.5) / nv) + rng.normal(0, 0.04)
            gl, gr = dsp.pan_gains(pn)
            Lb += y * gl
            Rb += y * gr
        # ruido de arco: siempre un poco, mas en los ataques y en forte
        nz = rng.standard_normal((n, 2))
        nz = dsp.filt(nz, dsp.eq([("hp", 1800), ("lp", 7000)]))
        tr_env = np.zeros(n)
        for (s, lv) in onsets:
            k = min(n - s, int(0.09 * SR))
            if k > 0:
                tr_env[s:s + k] += lv * np.exp(-np.arange(k) / (0.025 * SR))
        nlev = cfg["noise"] * prm.get("noise_mult", 1.0) * (1.8 if trem else 1.0)
        nenv = amp * nlev * np.sqrt(nv) * (1 + 1.2 * amp) + tr_env * nlev * 2.5 * np.sqrt(nv)
        Lb += nz[:, 0] * nenv
        Rb += nz[:, 1] * nenv
        # brillo que sigue a la dinamica (sul tasto en piano, abierto en forte)
        ref = max(1e-6, prm.get("bright_ref", 0.55))
        b = np.clip(amp / ref, 0, 1) ** 0.7
        if trem:
            b = np.minimum(1, b + 0.2)
        dark = dsp.eq([("lp", bright_lo, 0.6)])
        Ld = dsp.filt(Lb, dark)
        Rd = dsp.filt(Rb, dark)
        Lb = Ld + b * (Lb - Ld)
        Rb = Rd + b * (Rb - Rd)
        a0 = max(0, off)
        s0 = a0 - off
        a1 = min(total_n, off + n)
        if a1 > a0:
            out[a0:a1, 0] += Lb[s0:s0 + a1 - a0]
            out[a0:a1, 1] += Rb[s0:s0 + a1 - a0]
    scale = 1.0 / np.sqrt(nv)
    return out * scale * 0.5


# ============================================================================
# CORO y VOZ SOLISTA: pulso glotal + formantes
# ============================================================================

# (F1..F5 Hz, dB, ancho de banda Hz) - tablas clasicas de formantes cantados
FORMANTS = {
    "s": {"a": ([800, 1150, 2900, 3900, 4950], [0, -6, -32, -20, -50], [80, 90, 120, 130, 140]),
          "e": ([350, 2000, 2800, 3600, 4950], [0, -20, -15, -40, -56], [60, 100, 120, 150, 200]),
          "i": ([270, 2140, 2950, 3900, 4950], [0, -12, -26, -26, -44], [60, 90, 100, 120, 120]),
          "o": ([450, 800, 2830, 3800, 4950], [0, -11, -22, -22, -50], [70, 80, 100, 130, 135]),
          "u": ([325, 700, 2700, 3800, 4950], [0, -16, -35, -40, -60], [50, 60, 170, 180, 200])},
    "a": {"a": ([800, 1150, 2800, 3500, 4950], [0, -4, -20, -36, -60], [80, 90, 120, 130, 140]),
          "e": ([400, 1600, 2700, 3300, 4950], [0, -24, -30, -35, -60], [60, 80, 120, 150, 200]),
          "i": ([350, 1700, 2700, 3700, 4950], [0, -20, -30, -36, -60], [50, 100, 120, 150, 200]),
          "o": ([450, 800, 2830, 3500, 4950], [0, -9, -16, -28, -55], [70, 80, 100, 130, 135]),
          "u": ([325, 700, 2530, 3500, 4950], [0, -12, -30, -40, -64], [50, 60, 170, 180, 200])},
    "t": {"a": ([650, 1080, 2650, 2900, 3250], [0, -6, -7, -8, -22], [80, 90, 120, 130, 140]),
          "e": ([400, 1700, 2600, 3200, 3580], [0, -14, -12, -14, -20], [70, 80, 100, 120, 120]),
          "i": ([290, 1870, 2800, 3250, 3540], [0, -15, -18, -20, -30], [40, 90, 100, 120, 120]),
          "o": ([400, 800, 2600, 2800, 3000], [0, -10, -12, -12, -26], [40, 80, 100, 120, 120]),
          "u": ([350, 600, 2700, 2900, 3300], [0, -20, -17, -14, -26], [40, 60, 100, 120, 120])},
    "b": {"a": ([600, 1040, 2250, 2450, 2750], [0, -7, -9, -9, -20], [60, 70, 110, 120, 130]),
          "e": ([400, 1620, 2400, 2800, 3100], [0, -12, -9, -12, -18], [40, 80, 100, 120, 120]),
          "i": ([250, 1750, 2600, 3050, 3340], [0, -30, -16, -22, -28], [60, 90, 100, 120, 120]),
          "o": ([400, 750, 2400, 2600, 2900], [0, -11, -21, -20, -40], [40, 80, 100, 120, 120]),
          "u": ([350, 600, 2400, 2675, 2950], [0, -20, -32, -28, -36], [40, 80, 100, 120, 120])},
}

CHOIR = {
    "choir_s": dict(reg="s", nv=10, pan=(-0.62, -0.18), vib=(5.4, 30), jit=6.0),
    "choir_a": dict(reg="a", nv=10, pan=(-0.25, 0.12), vib=(5.2, 28), jit=6.0),
    "choir_t": dict(reg="t", nv=9, pan=(0.05, 0.40), vib=(5.1, 26), jit=6.5),
    "choir_b": dict(reg="b", nv=9, pan=(0.28, 0.66), vib=(4.9, 24), jit=6.5),
    "voice": dict(reg="s", nv=2, pan=(-0.04, 0.04), vib=(5.3, 38), jit=4.0),
    "voice_m": dict(reg="t", nv=2, pan=(0.02, 0.08), vib=(5.0, 30), jit=4.0),
}

CHOIR_LINE = dict(DEF_LINE, attack=0.35, release=0.6, glide=0.09, slide=0.25, dip=0.05,
                  vib_delay=0.25, vib_rise=0.6, vib_cont=True, swell=0.22, swell_min=1.2,
                  gamma=1.5)
VOICE_LINE = dict(DEF_LINE, attack=0.12, release=0.45, glide=0.08, slide=0.22, dip=0.06,
                  scoop=40, scoop_t=0.12, vib_delay=0.35, vib_rise=0.55, vib_cont=False,
                  swell=0.3, swell_min=0.9, gamma=1.5, sag=12)


def formant_bank(x, reg, vowel, bw_mult=1.4, shift_ratio=1.0, f0=None):
    fs, gains, bws = FORMANTS[reg][vowel]
    y = np.zeros_like(x)
    for k, (f, g, bw) in enumerate(zip(fs, gains, bws)):
        f = f * shift_ratio
        if k == 0 and f0:
            # sintonia de formante: los cantantes suben F1 con la nota para que
            # el fundamental no se quede fuera (sopranos sobre todo)
            f = max(f, 1.12 * f0)
        if f > SR * 0.45:
            continue
        Q = f / (bw * bw_mult)
        y += dsp.filt(x, dsp.bq("bp", f, Q)) * 10 ** (g / 20.0)
    return y


def render_choir(phrases, cfg, prm, rng, total_n):
    out = np.zeros((total_n, 2))
    breath = np.zeros((total_n, 2))
    solo = cfg["nv"] <= 2
    P = dict(VOICE_LINE if solo else CHOIR_LINE, **prm.get("line", {}))
    nv = prm.get("nv", cfg["nv"])
    vr, vc = cfg["vib"]
    vc *= prm.get("vib_mult", 1.0)
    w = wt("glottal")
    pan_lo, pan_hi = prm.get("pan_range", cfg["pan"])
    for phrase in phrases:
        off, pitch, amp, vib, onsets = build_line(phrase, P)
        n = len(pitch)
        fmax = dsp.midi_hz(np.max(pitch) + 0.8)
        fbase = dsp.midi_hz(pitch)
        Lb = np.zeros(n)
        Rb = np.zeros(n)
        for v in range(nv):
            sh = int(np.clip(rng.normal(0, 22 if nv > 2 else 4), -50, 50) * SR / 1000)
            det = rng.normal(0, 7 if nv > 2 else 2)
            drift = dsp.slow_noise(n, 0.8, rng) * (5 if nv > 2 else 3)
            jit = dsp.slow_noise(n, 30.0, rng) * cfg["jit"]
            rate = vr * (1 + 0.08 * rng.standard_normal()) + 0.4 * dsp.slow_noise(n, 0.7, rng)
            vph = np.cumsum(rate) * (2 * np.pi / SR) + rng.uniform(0, 2 * np.pi)
            depth = vc * (0.65 + 0.7 * rng.random())
            cents = det + drift + jit + depth * np.sin(vph) * dsp.shift(vib, sh)
            f = dsp.shift(fbase, sh) * (1.0 + cents * CENT)
            ph = np.cumsum(f) / SR + rng.random()
            y = w.render(ph, fmax)
            shim = 1.0 + 0.10 * dsp.slow_noise(n, 12.0, rng) + 0.08 * dsp.slow_noise(n, 1.5, rng)
            y *= dsp.shift(amp, sh) * shim
            pn = pan_lo + (pan_hi - pan_lo) * ((v + 0.5) / nv) + rng.normal(0, 0.03)
            gl, gr = dsp.pan_gains(pn)
            Lb += y * gl
            Rb += y * gr
        a0 = max(0, off)
        s0 = a0 - off
        a1 = min(total_n, off + n)
        if a1 > a0:
            out[a0:a1, 0] += Lb[s0:s0 + a1 - a0]
            out[a0:a1, 1] += Rb[s0:s0 + a1 - a0]
            br = rng.standard_normal((a1 - a0, 2)) * (amp[s0:s0 + a1 - a0, None] ** 0.8)
            breath[a0:a1] += br
    out *= 1.0 / np.sqrt(nv)
    # Formantes: una (o dos, con morph) vocales por parte
    reg = cfg["reg"]
    vowels = prm.get("vowel", "a")
    if isinstance(vowels, str):
        vowels = (vowels,)
    bwm = prm.get("bw", 1.6 if not solo else 1.15)
    bl = prm.get("breath", 0.05 if not solo else 0.07)
    src = out + breath * bl
    allp = [e.p for ph in phrases for e in ph if not isinstance(e.p, str)]
    f0 = float(dsp.midi_hz(np.median(allp))) if allp else None
    if len(vowels) == 1:
        y = formant_bank(src, reg, vowels[0], bwm, f0=f0)
    else:
        y1 = formant_bank(src, reg, vowels[0], bwm, f0=f0)
        y2 = formant_bank(src, reg, vowels[1], bwm, f0=f0)
        per = prm.get("morph_period", 9.0)
        tt = np.arange(total_n) / SR
        m = 0.5 - 0.5 * np.cos(2 * np.pi * tt / per + rng.uniform(0, 6.28))
        m = m[:, None]
        y = y1 * (1 - m) + y2 * m
    # cuerpo grave (el formante paso-banda se come el fundamental) y aire
    body = dsp.filt(out, dsp.eq([("lp", 520, 0.6)])) * prm.get("chest", 0.75)
    air = dsp.filt(breath, dsp.eq([("hp", 5500)])) * bl * 0.6
    y = y * 2.4 + body + air
    return y * 0.55


# ============================================================================
# VIENTOS: duduk, ney, kaval, shofar
# ============================================================================

WINDS = {
    "duduk": dict(wt="duduk", line=dict(attack=0.07, release=0.22, glide=0.07, slide=0.20,
                                        dip=0.10, scoop=45, scoop_t=0.11, vib_delay=0.45,
                                        vib_rise=0.6, vib_cont=False, swell=0.32,
                                        swell_min=0.8, sag=14, gamma=1.4),
                  vib=(5.0, 17), breath=0.045, pbreath=0.06,
                  eq=[("hp", 120), ("peak", 620, 4.0, 1.1), ("peak", 1500, 2.5, 1.4),
                      ("peak", 2900, -3.0, 1.5), ("lp", 4600, 0.7)],
                  dark=1100.0, bright_add=3800.0),
    "ney": dict(wt="ney", line=dict(attack=0.09, release=0.25, glide=0.08, slide=0.24,
                                    dip=0.14, scoop=30, scoop_t=0.10, vib_delay=0.40,
                                    vib_rise=0.5, vib_cont=False, swell=0.30, swell_min=0.8,
                                    sag=10, gamma=1.4, overshoot=0.15),
                vib=(4.9, 16), breath=0.20, pbreath=0.10,
                eq=[("hp", 200), ("peak", 1200, 2.0, 0.8), ("lp", 7000, 0.7)],
                dark=2600.0, bright_add=2000.0),
    "kaval": dict(wt="kaval", line=dict(attack=0.06, release=0.2, glide=0.06, slide=0.18,
                                        dip=0.12, scoop=25, scoop_t=0.08, vib_delay=0.35,
                                        vib_rise=0.5, vib_cont=False, swell=0.25,
                                        swell_min=0.8, sag=8, gamma=1.4, overshoot=0.12),
                  vib=(5.3, 14), breath=0.12, pbreath=0.08,
                  eq=[("hp", 220), ("peak", 2400, 2.5, 1.0), ("lp", 8500, 0.7)],
                  dark=2800.0, bright_add=3000.0),
    "shofar": dict(wt="shofar", line=dict(attack=0.05, release=0.18, glide=0.10, slide=0.25,
                                          dip=0.2, scoop=180, scoop_t=0.16, vib_delay=0.5,
                                          vib_rise=0.4, vib_cont=False, swell=0.2,
                                          swell_min=0.8, sag=-35, gamma=1.3,
                                          overshoot=0.25),
                   vib=(6.5, 12), breath=0.10, pbreath=0.05,
                   eq=[("hp", 250), ("peak", 1250, 6.0, 1.2), ("peak", 2700, 4.0, 1.5),
                       ("lp", 6000, 0.7)],
                   dark=1500.0, bright_add=5000.0),
}


def render_wind(phrases, cfg, prm, rng, total_n):
    out = np.zeros((total_n, 2))
    P = dict(DEF_LINE, **cfg["line"])
    P.update(prm.get("line", {}))
    vr, vc = cfg["vib"]
    vc *= prm.get("vib_mult", 1.0)
    w = wt(cfg["wt"])
    pan = prm.get("pan", 0.0)
    gl, gr = dsp.pan_gains(pan)
    for phrase in phrases:
        off, pitch, amp, vib, onsets = build_line(phrase, P)
        n = len(pitch)
        fmax = dsp.midi_hz(np.max(pitch) + 0.5)
        drift = dsp.slow_noise(n, 0.9, rng) * 3.0
        wob = dsp.slow_noise(n, 7.0, rng) * (8.0 if cfg["wt"] == "shofar" else 1.5)
        rate = vr * (1 + 0.05 * rng.standard_normal()) + 0.45 * dsp.slow_noise(n, 0.8, rng)
        vph = np.cumsum(rate) * (2 * np.pi / SR)
        vdepth = vc * (1 + 0.25 * dsp.slow_noise(n, 0.5, rng))
        cents = drift + wob + vdepth * np.sin(vph) * vib
        f = dsp.midi_hz(pitch) * (1.0 + cents * CENT)
        ph = np.cumsum(f) / SR + rng.random()
        y = w.render(ph, fmax)
        # vibrato de aire: el soplo tambien late un poco con el vibrato
        a = amp * (1.0 + 0.10 * np.sin(vph) * vib + 0.05 * dsp.slow_noise(n, 3.0, rng))
        tone = y * a
        # ruido de soplo: una parte ancha y otra pegada al periodo
        nz = rng.standard_normal(n)
        broad = dsp.filt(nz, dsp.eq([("hp", 900), ("lp", 6500)]))
        pitched = nz * (0.55 + 0.45 * np.sin(2 * np.pi * ph))
        pitched = dsp.filt(pitched, dsp.eq([("hp", 400), ("lp", 4500)]))
        chiff = np.zeros(n)
        for (s, lv) in onsets:
            k = min(n - s, int(0.06 * SR))
            if k > 0:
                chiff[s:s + k] += lv * np.exp(-np.arange(k) / (0.015 * SR))
        br = cfg["breath"] * prm.get("breath_mult", 1.0)
        noise = (broad * (amp ** 0.9) * br + pitched * amp * cfg["pbreath"] +
                 broad * chiff * br * 2.5)
        sig = tone + noise
        # brillo con la dinamica
        ref = max(1e-6, prm.get("bright_ref", 0.6))
        b = np.clip(amp / ref, 0, 1) ** 0.8
        d = dsp.filt(sig, dsp.eq([("lp", cfg["dark"], 0.6)]))
        sig = d + b * (sig - d)
        a0 = max(0, off)
        s0 = a0 - off
        a1 = min(total_n, off + n)
        if a1 > a0:
            out[a0:a1, 0] += sig[s0:s0 + a1 - a0] * gl
            out[a0:a1, 1] += sig[s0:s0 + a1 - a0] * gr
    return out * 0.6


# ============================================================================
# METALES: trompas y metales graves (seccion)
# ============================================================================

BRASS = {
    "horns": dict(nv=4, pan=(-0.42, -0.08), vib=(5.0, 5), detune=5, spread=16,
                  fc=(2.0, 9.0), fmin=280, fmax=4200,
                  eq=[("hp", 70), ("peak", 420, 3.0, 0.9), ("peak", 2600, -3.0, 1.0),
                      ("lp", 5200, 0.7)]),
    "lowbrass": dict(nv=4, pan=(0.12, 0.45), vib=(4.6, 3), detune=4, spread=14,
                     fc=(2.5, 14.0), fmin=200, fmax=5200,
                     eq=[("hp", 40), ("peak", 300, 3.0, 0.8), ("peak", 1300, 2.0, 1.2),
                         ("lp", 6500, 0.7)]),
}
BRASS_LINE = dict(DEF_LINE, attack=0.06, release=0.25, glide=0.05, dip=0.15, dip_t=0.06,
                  scoop=30, scoop_t=0.05, vib_delay=0.6, vib_rise=0.6, vib_cont=False,
                  swell=0.15, swell_min=1.2, gamma=1.5, overshoot=0.1)


def render_brass(phrases, cfg, prm, rng, total_n):
    out = np.zeros((total_n, 2))
    P = dict(BRASS_LINE, **prm.get("line", {}))
    nv = prm.get("nv", cfg["nv"])
    vr, vc = cfg["vib"]
    w = wt("brass")
    pan_lo, pan_hi = prm.get("pan_range", cfg["pan"])
    for phrase in phrases:
        off, pitch, amp, vib, onsets = build_line(phrase, P)
        n = len(pitch)
        fmax = dsp.midi_hz(np.max(pitch) + 0.6)
        fbase = dsp.midi_hz(pitch)
        Lb = np.zeros(n)
        Rb = np.zeros(n)
        for v in range(nv):
            sh = int(np.clip(rng.normal(0, cfg["spread"]), -40, 40) * SR / 1000) if nv > 1 else 0
            det = rng.normal(0, cfg["detune"])
            drift = dsp.slow_noise(n, 0.8, rng) * 3.0
            vph = np.cumsum(vr * (1 + 0.06 * rng.standard_normal()) * np.ones(n)) * 2 * np.pi / SR
            cents = det + drift + vc * np.sin(vph + rng.random() * 6) * dsp.shift(vib, sh)
            f = dsp.shift(fbase, sh) * (1.0 + cents * CENT)
            ph = np.cumsum(f) / SR + rng.random()
            y = w.render(ph, fmax) * dsp.shift(amp, sh)
            pn = pan_lo + (pan_hi - pan_lo) * ((v + 0.5) / nv)
            gl, gr = dsp.pan_gains(pn)
            Lb += y * gl
            Rb += y * gr
        # el filtro se abre con la dinamica: es lo que suena a metal
        ref = max(1e-6, prm.get("bright_ref", 0.55))
        lvl = np.clip(amp / ref, 0, 1.4)
        f0 = float(np.median(fbase))
        k0, k1 = cfg["fc"]
        fc = np.clip(f0 * (k0 + k1 * lvl ** 1.5), cfg["fmin"], cfg["fmax"] * prm.get("open", 1.0))
        X = np.stack([Lb, Rb], axis=1)
        X = dsp.tv_filter(X, "lp", fc, q=0.9, block=256)
        X = dsp.tv_filter(X, "lp", fc * 1.3, q=0.6, block=256)
        a0 = max(0, off)
        s0 = a0 - off
        a1 = min(total_n, off + n)
        if a1 > a0:
            out[a0:a1] += X[s0:s0 + a1 - a0]
    return out * (1.0 / np.sqrt(nv)) * 0.7


# ============================================================================
# PULSADAS: Karplus-Strong
# ============================================================================

_KS_CACHE = {}


def ks_pluck(f, dur, b=0.12, t60=2.0, pick=0.13, exc_lp=0.5, seed=0):
    """Cuerda pulsada Karplus-Strong con filtro de lazo simetrico [b, 1-2b, b]
    (retardo exacto de N muestras) calculado POR FILAS de un periodo: son
    dur*f iteraciones de numpy, no dur*SR de Python. La afinacion exacta se
    consigue leyendo la cuerda a velocidad f*N/SR (interpolacion lineal)."""
    rng = np.random.default_rng(seed)
    N = max(4, int(round(SR / f)))
    r = f * N / SR
    n_out = int(dur * SR)
    n_ks = int(n_out * r) + N + 4
    rows = n_ks // N + 2
    f_ks = SR / N
    g = 10.0 ** (-3.0 / (t60 * f_ks))
    exc = rng.uniform(-1, 1, N)
    # excitacion: paso bajo (blandura del dedo/pua) y peine de la posicion
    a = exc_lp
    exc = signal.lfilter([1 - a], [1, -a], np.concatenate([exc, exc]))[N:]
    p = max(1, int(pick * N))
    exc = exc - np.roll(exc, p)
    exc -= exc.mean()
    exc /= max(1e-9, np.max(np.abs(exc)))
    Y = np.zeros((rows, N))
    Y[0] = exc
    c0, c1 = 1.0 - 2.0 * b, b
    left = np.empty(N)
    right = np.empty(N)
    for k in range(1, rows):
        prev = Y[k - 1]
        left[0] = Y[k - 2, -1] if k >= 2 else 0.0
        left[1:] = prev[:-1]
        right[:-1] = prev[1:]
        right[-1] = 0.0
        cur = g * (c0 * prev + c1 * (left + right))
        cur[-1] += g * c1 * cur[0]
        Y[k] = cur
    flat = Y.ravel()
    pos = np.arange(n_out) * r
    out = np.interp(pos, np.arange(len(flat)), flat)
    # quita el escalon de continua y suaviza el primer instante
    out = signal.lfilter([1, -1], [1, -0.995], out)
    return out


PLUCK = {
    #        brillo b   T60(s) a 220 Hz  pua   paso bajo exc  cuerpo   tope de pulsacion
    "oud": dict(b=0.2, t60=1.6, pick=0.22, exc=0.68, body="oud", ring=1.6, rel=0.25,
                pick_noise=0.03, pan=0.22,
                eq=[("hp", 70), ("peak", 220, 3.0, 0.8), ("lp", 6500)]),
    "kanun": dict(b=0.10, t60=3.2, pick=0.12, exc=0.6, body="kanun", ring=3.0, rel=0.6,
                  pick_noise=0.02, pan=0.3,
                  eq=[("hp", 120), ("lp", 9500)]),
    "harp": dict(b=0.13, t60=3.0, pick=0.22, exc=0.76, body="harp", ring=4.0, rel=1.0,
                 pick_noise=0.008, pan=-0.45,
                 eq=[("hp", 50), ("lp", 9000)]),
    "lyre": dict(b=0.18, t60=1.2, pick=0.22, exc=0.72, body="lyre", ring=1.5, rel=0.3,
                 pick_noise=0.02, pan=-0.3,
                 eq=[("hp", 110), ("lp", 8000)]),
    "pizz": dict(b=0.22, t60=0.55, pick=0.2, exc=0.55, body="violin", ring=1.0, rel=0.15,
                 pick_noise=0.01, pan=0.0, voices=3,
                 eq=[("hp", 60), ("lp", 6000)]),
}


def render_pluck(notes, cfg, prm, rng, total_n, inst):
    out = np.zeros((total_n, 2))
    base_pan = prm.get("pan", cfg["pan"])
    width = prm.get("width", 0.12)
    nvox = prm.get("voices", cfg.get("voices", 1))
    t60m = prm.get("t60_mult", 1.0)
    brm = prm.get("bright", 0.0)
    for e in notes:
        if e.v <= 0:
            continue
        f = float(dsp.midi_hz(e.p))
        # las cuerdas agudas suenan menos tiempo
        t60 = cfg["t60"] * t60m * (220.0 / f) ** 0.45
        layer = 0 if e.v < 0.45 else (1 if e.v < 0.75 else 2)
        b = max(0.02, cfg["b"] - 0.03 * layer - brm)
        exc = max(0.0, cfg["exc"] - 0.12 * layer)
        dur = min(t60 * 1.1, e.d * cfg["ring"] + cfg["rel"] + 0.05)
        if e.fl.get("lv"):
            dur = t60 * 1.1
        dur = max(0.08, min(dur, 8.0))
        for vx in range(nvox):
            var = int(rng.integers(0, 3))
            key = (inst, round(f * 4), layer, var, round(t60, 2), round(b, 3))
            if key not in _KS_CACHE:
                _KS_CACHE[key] = ks_pluck(f, min(t60 * 1.15, 8.0), b=b, t60=t60,
                                          pick=cfg["pick"] * (0.85 + 0.1 * var),
                                          exc_lp=exc, seed=zlib.crc32(repr(key).encode()) & 0xFFFF)
            src = _KS_CACHE[key]
            k = min(len(src), int(dur * SR))
            y = src[:k].copy()
            # apagado: la mano corta la cuerda al acabar la nota
            fade = int(min(cfg["rel"], dur) * SR)
            if fade > 1 and fade < k:
                y[k - fade:] *= np.linspace(1, 0, fade) ** 2
            amp = e.v ** 1.4 * e.fl.get("amp", 1.0)
            if nvox > 1:
                amp /= np.sqrt(nvox)
            # ruido de pua: el "tic" del ataque
            pn = cfg["pick_noise"]
            if pn:
                m = min(k, int(0.012 * SR))
                tick = rng.standard_normal(m) * np.exp(-np.arange(m) / (0.0025 * SR))
                y[:m] += dsp.filt(tick, dsp.eq([("hp", 2500)])) * pn * (0.6 + 0.6 * layer)
            dt = int(rng.normal(0, 0.008) * SR) if nvox > 1 else 0
            s = int(round(e.t * SR)) + dt
            pn_ = base_pan + rng.uniform(-width, width) + e.fl.get("pan", 0.0)
            gl, gr = dsp.pan_gains(pn_)
            a0 = max(0, s)
            a1 = min(total_n, s + k)
            if a1 > a0:
                seg = y[a0 - s:a1 - s] * amp
                out[a0:a1, 0] += seg * gl
                out[a0:a1, 1] += seg * gr
    return out * 0.5


# ============================================================================
# PERCUSION MODAL
# ============================================================================

# Ceros de Bessel / j01: modos de una membrana circular ideal
MEMB = np.array([1.0, 1.593, 2.136, 2.295, 2.653, 2.917, 3.155, 3.500, 3.598, 3.647,
                 4.059, 4.131, 4.230, 4.601, 4.610, 4.832, 4.903, 5.083, 5.131, 5.412])
SYM = [0, 3, 8, 16]      # modos (0,n): los que excita un golpe en el centro

_HIT_CACHE = {}


def modal(freqs, amps, t60s, dur, glide=0.0, glide_t=0.03, attack=0.0008, seed=0):
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    gl = 1.0 + glide * np.exp(-t / glide_t) if glide else None
    for f, a, T in zip(freqs, amps, t60s):
        if f >= SR * 0.47 or a <= 0:
            continue
        env = np.exp(-6.9078 * t / max(T, 1e-3))
        if gl is not None:
            ph = 2 * np.pi * np.cumsum(f * gl) / SR
        else:
            ph = 2 * np.pi * f * t
        y += a * env * np.sin(ph + rng.uniform(0, 2 * np.pi))
    if attack:
        y *= 1.0 - np.exp(-t / attack)
    return y


def noise_burst(dur, decay, hp=None, lp=None, seed=0, attack=0.0005):
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = rng.standard_normal(n) * np.exp(-t / decay) * (1 - np.exp(-t / attack))
    spec = []
    if hp:
        spec.append(("hp", hp))
    if lp:
        spec.append(("lp", lp))
    return dsp.filt(x, dsp.eq(spec)) if spec else x


def _membrane(f1, center, n_modes, dur, t1, hi_decay, bright, glide, seed):
    rng = np.random.default_rng(seed)
    fr = f1 * MEMB[:n_modes] * (1 + rng.normal(0, 0.006, n_modes))
    amps = np.zeros(n_modes)
    for i in range(n_modes):
        sym = i in SYM
        base = (1.0 if sym else 0.25) if center else (0.35 if sym else 0.8)
        amps[i] = base * (1.0 / (1 + (fr[i] / bright) ** 2)) * rng.uniform(0.7, 1.2)
    t60 = t1 * (fr[0] / fr) ** hi_decay
    return modal(fr, amps, t60, dur, glide=glide, seed=seed)


def _mixin(y, z):
    """Suma dos senales de distinta longitud (alineadas al principio)."""
    if len(z) > len(y):
        y, z = z, y
    y = y.copy()
    y[:len(z)] += z
    return y


def perc_hit(inst, stroke, layer, var, pitch=None):
    """Una muestra de percusion sintetizada (cacheada). layer 0-2 = dinamica."""
    key = (inst, stroke, layer, var, pitch)
    if key in _HIT_CACHE:
        return _HIT_CACHE[key]
    sd = zlib.crc32(repr(key).encode()) & 0x7FFFFFFF
    vb = 0.6 + 0.3 * layer     # mas fuerte = mas brillante
    if inst == "frame":            # tar / bendir grande
        f1 = 74.0 * (1 + 0.01 * var)
        if stroke == "D":
            y = _membrane(f1, True, 12, 1.1, 0.75, 0.6, 500 * vb, 0.07, sd)
            y = _mixin(y, noise_burst(0.05, 0.008, lp=900, seed=sd) * 0.25)
        elif stroke in ("T", "K"):
            y = _membrane(f1 * 2.1, False, 18, 0.5, 0.22, 0.9, 2600 * vb, 0.02, sd)
            y = _mixin(y, noise_burst(0.08, 0.012 if stroke == "T" else 0.008, hp=1500,
                                      lp=7000, seed=sd) * (0.55 if stroke == "T" else 0.35))
            if stroke == "K":
                y *= 0.6
        else:   # 'g' golpe fantasma de los dedos
            y = noise_burst(0.06, 0.01, hp=1200, lp=6000, seed=sd) * 0.4
            y = _mixin(y, _membrane(f1 * 2.4, False, 10, 0.25, 0.1, 1.0, 2000, 0.0, sd) * 0.3)
    elif inst == "daf":            # daf: tar con anillas
        f1 = 82.0
        if stroke == "D":
            y = _membrane(f1, True, 12, 1.0, 0.6, 0.6, 600 * vb, 0.06, sd)
            y = _mixin(y, _jingles(0.35, 0.12, 0.35 * vb, sd))
        else:
            y = _membrane(f1 * 2.0, False, 16, 0.4, 0.18, 0.9, 2500 * vb, 0.02, sd) * 0.6
            y = _mixin(y, _jingles(0.45, 0.14, 0.9 * vb, sd))
            y = _mixin(y, noise_burst(0.05, 0.008, hp=2000, seed=sd) * 0.3)
    elif inst == "darbuka":
        if stroke == "D":
            y = _membrane(96.0, True, 10, 0.9, 0.45, 0.7, 700 * vb, 0.12, sd)
            y = _mixin(y, modal([84.0], [0.6], [0.5], 0.9, seed=sd))   # Helmholtz del caliz
            y = _mixin(y, noise_burst(0.04, 0.006, lp=1200, seed=sd) * 0.3)
        elif stroke in ("T", "K"):
            sc = 1.0 if stroke == "T" else 0.75
            y = _membrane(410.0, False, 14, 0.35, 0.12, 1.0, 5000 * vb, 0.01, sd) * 0.7
            y = _mixin(y, modal([2150, 3380, 4630], [0.25, 0.18, 0.1], [0.22, 0.16, 0.1], 0.35,
                                seed=sd) * sc)                         # el cuerpo de metal
            y = _mixin(y, noise_burst(0.03, 0.004, hp=2500, seed=sd) * 0.6 * sc)
            y *= sc
        else:  # 'S' slap
            y = noise_burst(0.08, 0.012, hp=900, lp=6000, seed=sd) * 0.8
            y = _mixin(y, _membrane(180.0, False, 12, 0.3, 0.12, 0.8, 3000, 0.0, sd) * 0.5)
    elif inst == "riq":
        if stroke == "D":
            y = _membrane(160.0, True, 8, 0.5, 0.2, 0.7, 900, 0.03, sd) * 0.5
            y = _mixin(y, _jingles(0.3, 0.09, 0.35 * vb, sd))
        elif stroke == "S":   # sacudida: las anillas solas, mas largas
            y = _jingles(0.5, 0.2, 0.9 * vb, sd, rattle=0.05)
        else:
            y = _jingles(0.35, 0.1, 0.8 * vb, sd)
            y = _mixin(y, noise_burst(0.03, 0.004, hp=3000, seed=sd) * 0.25)
    elif inst == "taiko":
        f1 = (pitch or 50.0) * (1 + 0.012 * var)
        if stroke in ("D", "d"):
            fr = f1 * np.array([1.0, 1.52, 1.97, 2.3, 2.66, 3.1])
            am = np.array([1.0, 0.45, 0.25, 0.3, 0.12, 0.08])
            t60 = np.array([1.5, 0.8, 0.55, 0.45, 0.35, 0.25])
            y = modal(fr, am, t60, 2.2, glide=0.38, glide_t=0.045, attack=0.0015, seed=sd)
            y = _mixin(y, noise_burst(0.06, 0.012, lp=1800 * vb, seed=sd) * 0.45)
            y = _mixin(y, noise_burst(0.03, 0.003, hp=2000, lp=7000, seed=sd) * 0.12 * vb)
            if stroke == "d":
                y *= 0.55
        else:  # 'K' borde (ka)
            y = modal([880, 2280, 3650, 5200], [1, 0.6, 0.35, 0.2], [0.08, 0.05, 0.04, 0.03],
                      0.2, seed=sd) * 0.5
            y = _mixin(y, noise_burst(0.03, 0.004, hp=1500, seed=sd) * 0.4)
    elif inst == "timp":
        f = pitch or 73.4
        fr = f * np.array([0.62, 1.0, 1.504, 1.98, 2.44, 2.9, 3.36])
        am = np.array([0.5, 1.0, 0.5, 0.32, 0.18, 0.1, 0.05])
        t60 = np.array([0.22, 2.6, 1.8, 1.3, 1.0, 0.8, 0.6])
        y = modal(fr, am * (np.ones(7) if layer else np.array([1, 1, .7, .5, .35, .2, .1])),
                  t60, 3.0, glide=0.015, glide_t=0.05, attack=0.002, seed=sd)
        y = _mixin(y, noise_burst(0.05, 0.01, lp=900, seed=sd) * 0.25)
    elif inst == "zills":
        fr = np.array([2610, 3810, 5240, 6650, 8130, 9620, 11300, 12900]) * (1 + 0.01 * var)
        am = np.array([1.0, 0.8, 0.7, 0.5, 0.4, 0.3, 0.2, 0.15])
        t60 = np.array([3.0, 2.4, 2.0, 1.6, 1.3, 1.0, 0.8, 0.6])
        if stroke == "x":       # cerrado
            t60 = t60 / 7.0
        fr = np.concatenate([fr, fr + 3.7])
        y = modal(fr, np.concatenate([am, am * 0.7]), np.concatenate([t60, t60]),
                  3.2 if stroke != "x" else 0.6, seed=sd) * 0.3
        y = _mixin(y, noise_burst(0.02, 0.002, hp=4000, seed=sd) * 0.2)
    elif inst == "shaker":
        n = int(0.16 * SR)
        t = np.arange(n) / SR
        rng = np.random.default_rng(sd)
        grains = (rng.random(n) < 0.25) * rng.standard_normal(n)
        env = (t / 0.012) * np.exp(1 - t / 0.012) if stroke == "S" else \
            (t / 0.018) * np.exp(1 - t / 0.018)
        y = dsp.filt(grains * env, dsp.eq([("hp", 3500), ("lp", 11000)])) * \
            (1.0 if stroke == "S" else 0.55)
    elif inst == "clap":
        rng = np.random.default_rng(sd)
        n = int(0.15 * SR)
        y = np.zeros(n)
        for c in range(4):
            o = int(rng.uniform(0, 0.012) * SR)
            for k in range(3):
                oo = o + int(k * 0.006 * SR)
                m = n - oo
                y[oo:] += rng.standard_normal(m) * np.exp(-np.arange(m) / (0.004 * SR if k < 2
                                                                          else 0.02 * SR))
        y = dsp.filt(y, dsp.eq([("bp", 1300, 0.8), ("peak", 3000, 4, 1.0)])) * 0.7
    elif inst == "gong":
        rng = np.random.default_rng(sd)
        fr = 2 ** rng.uniform(np.log2(90), np.log2(5000), 90)
        am = (fr / 300.0) ** -0.4 * rng.uniform(0.5, 1, 90)
        t60 = 7.0 * (300.0 / fr) ** 0.35
        y = modal(fr, am, t60, 7.0, attack=0.004, seed=sd) * 0.25
        # florecimiento: los modos altos crecen despues del golpe
        t = np.arange(len(y)) / SR
        hi = dsp.filt(y, dsp.eq([("hp", 1500)]))
        y = y + hi * (1 - np.exp(-t / 0.6)) * 1.5
        y = _mixin(y, noise_burst(0.2, 0.05, lp=600, seed=sd) * 0.3)
    else:
        raise ValueError(inst)
    y = y / max(1e-9, np.max(np.abs(y)))
    _HIT_CACHE[key] = y
    return y


def _jingles(dur, decay, level, seed, rattle=0.025):
    """Anillas de metal (daf, riq): parciales inarmonicos agudos que golpean
    varias veces en unos milisegundos."""
    rng = np.random.default_rng(seed + 77)
    n = int(dur * SR)
    y = np.zeros(n)
    for k in range(5):
        o = int(rng.uniform(0, rattle) * SR) if k else 0
        fr = 2 ** rng.uniform(np.log2(3500), np.log2(13500), 14)
        am = rng.uniform(0.3, 1.0, 14)
        t60 = decay * rng.uniform(0.6, 1.4, 14)
        seg = modal(fr, am, t60, dur, seed=seed + k, attack=0.0002) * (0.8 ** k)
        y[o:] += seg[:n - o]
    y += noise_burst(dur, decay * 0.3, hp=5000, seed=seed) * 0.5
    return y * level / max(1e-9, np.max(np.abs(y)))


PERC = {
    "frame": dict(pan=-0.08, width=0.06, eq=[("hp", 40), ("lp", 12000)]),
    "daf": dict(pan=-0.22, width=0.05, eq=[("hp", 50)]),
    "darbuka": dict(pan=0.12, width=0.05, eq=[("hp", 50)]),
    "riq": dict(pan=-0.35, width=0.05, eq=[("hp", 120)]),
    "taiko": dict(pan=0.0, width=0.25, eq=[("hp", 28)], ensemble=3),
    "timp": dict(pan=-0.3, width=0.04, eq=[("hp", 30)]),
    "zills": dict(pan=0.42, width=0.05, eq=[("hp", 1500)]),
    "shaker": dict(pan=0.35, width=0.08, eq=[]),
    "clap": dict(pan=0.0, width=0.4, eq=[("hp", 300)], ensemble=3),
    "gong": dict(pan=-0.1, width=0.0, eq=[("hp", 40)]),
}


def render_perc(notes, cfg, prm, rng, total_n, inst):
    out = np.zeros((total_n, 2))
    base_pan = prm.get("pan", cfg["pan"])
    width = prm.get("width", cfg["width"])
    ens = prm.get("ensemble", cfg.get("ensemble", 1))
    for e in notes:
        if e.v <= 0:
            continue
        stroke = e.p if isinstance(e.p, str) else "D"
        pitch = e.fl.get("pitch")
        if inst == "timp" and not isinstance(e.p, str):
            pitch = round(float(dsp.midi_hz(e.p)), 2)
            stroke = "D"
        if e.fl.get("roll"):
            _roll(out, e, inst, stroke, pitch, base_pan, width, rng, total_n)
            continue
        layer = 0 if e.v < 0.45 else (1 if e.v < 0.78 else 2)
        for k in range(ens):
            var = int(rng.integers(0, 4))
            pk = pitch
            if inst == "taiko":
                pk = round((pitch or 50.0) * [1.0, 0.82, 1.21, 0.9][k % 4], 1)
            y = perc_hit(inst, stroke, layer, var, pk)
            dt = int(rng.normal(0, 0.009) * SR) if k else 0
            s = int(round(e.t * SR)) + dt
            amp = e.v ** 1.5 * e.fl.get("amp", 1.0) * (1.0 if k == 0 else 0.6)
            choke = e.fl.get("choke")
            yy = y
            if choke:
                m = int(choke * SR)
                if m < len(y):
                    yy = y[:m] * np.linspace(1, 0, m) ** 2
            pn = base_pan + (rng.uniform(-width, width) if k == 0 else
                             [-1, 1, -0.5, 0.5][k % 4] * width * 1.6)
            gl, gr = dsp.pan_gains(pn)
            a0 = max(0, s)
            a1 = min(total_n, s + len(yy))
            if a1 > a0:
                seg = yy[a0 - s:a1 - s] * amp
                out[a0:a1, 0] += seg * gl
                out[a0:a1, 1] += seg * gr
    return out * 0.5


def _roll(out, e, inst, stroke, pitch, pan, width, rng, total_n):
    """Redoble: golpes a ~17/s, manos alternas, con la dinamica de la nota
    (fl['cresc'] > 0 crece, < 0 se apaga)."""
    rate = e.fl.get("rate", 17.0)
    k = max(2, int(e.d * rate))
    c = e.fl.get("cresc", 0.0)
    for i in range(k):
        u = i / max(1, k - 1)
        v = e.v * ((1 - abs(c)) + abs(c) * (u if c > 0 else 1 - u))
        v *= 0.85 + 0.25 * rng.random()
        layer = 0 if v < 0.45 else (1 if v < 0.78 else 2)
        y = perc_hit(inst, stroke if inst != "frame" else ("T" if i % 2 else "K"), layer,
                     int(rng.integers(0, 4)), pitch)
        s = int(round((e.t + i / rate + rng.normal(0, 0.004)) * SR))
        gl, gr = dsp.pan_gains(pan + (0.05 if i % 2 else -0.05))
        a0 = max(0, s)
        a1 = min(total_n, s + len(y))
        if a1 > a0:
            seg = y[a0 - s:a1 - s] * v ** 1.5 * 0.55
            out[a0:a1, 0] += seg * gl
            out[a0:a1, 1] += seg * gr


# ============================================================================
# METALOFONOS: campanas, celesta, crotalos
# ============================================================================

BELLS = {
    "bells": dict(ratios=[0.5, 1.0, 1.19, 1.5, 2.0, 2.51, 2.66, 3.01, 4.17, 5.43],
                  amps=[0.35, 1.0, 0.5, 0.45, 0.6, 0.28, 0.22, 0.18, 0.12, 0.07],
                  t60=[7.0, 5.0, 4.0, 3.5, 3.0, 2.4, 2.1, 1.8, 1.3, 0.9], pan=0.32,
                  eq=[("hp", 150)]),
    "celesta": dict(ratios=[1.0, 2.0, 3.0, 4.15, 9.6],
                    amps=[1.0, 0.18, 0.07, 0.05, 0.08],
                    t60=[1.8, 1.0, 0.6, 0.4, 0.06], pan=0.36,
                    eq=[("hp", 200)]),
    "crotales": dict(ratios=[1.0, 2.0, 2.76, 5.40, 8.9],
                     amps=[1.0, 0.25, 0.4, 0.22, 0.1],
                     t60=[4.0, 2.8, 2.5, 1.5, 0.8], pan=-0.36,
                     eq=[("hp", 400)]),
}


def render_bells(notes, cfg, prm, rng, total_n, inst):
    out = np.zeros((total_n, 2))
    base_pan = prm.get("pan", cfg["pan"])
    t60m = prm.get("t60_mult", 1.0)
    for e in notes:
        f = float(dsp.midi_hz(e.p))
        key = (inst, round(f, 2), round(t60m, 2))
        if key not in _HIT_CACHE:
            rr = np.array(cfg["ratios"]) * (1 + np.random.default_rng(int(f)).normal(0, 0.002, len(cfg["ratios"])))
            hf = (f / 600.0) ** 0.5
            t60 = np.array(cfg["t60"]) * t60m / max(0.6, hf)
            y = modal(f * rr, cfg["amps"], t60, float(np.max(t60)) * 1.05, attack=0.0006,
                      seed=int(f))
            y = _mixin(y, noise_burst(0.02, 0.002, hp=3000, seed=int(f)) * 0.05)
            _HIT_CACHE[key] = y / max(1e-9, np.max(np.abs(y)))
        y = _HIT_CACHE[key]
        s = int(round(e.t * SR))
        amp = e.v ** 1.5 * e.fl.get("amp", 1.0)
        gl, gr = dsp.pan_gains(base_pan + rng.uniform(-0.08, 0.08))
        a0 = max(0, s)
        a1 = min(total_n, s + len(y))
        if a1 > a0:
            seg = y[a0 - s:a1 - s] * amp
            out[a0:a1, 0] += seg * gl
            out[a0:a1, 1] += seg * gr
    return out * 0.4


# ============================================================================
# BORDON: tanpura con jawari
# ============================================================================

def tanpura_note(f, dur=5.5, seed=0):
    key = ("tanpura", round(f, 2), seed)
    if key in _HIT_CACHE:
        return _HIT_CACHE[key]
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    t = np.arange(n) / SR
    H = int(min(60, 6500 / f))
    y = np.zeros(n)
    c = 3.0 + 22.0 * np.exp(-t / 1.1)          # el pico del jawari baja por los armonicos
    env = np.exp(-t / 2.4) * (1 - np.exp(-t / 0.004))
    for h in range(1, H + 1):
        fh = f * h * (1 + 0.00012 * h * h)
        if fh > 7000:
            break
        base = h ** -0.7
        jw = 0.25 + 1.6 * np.exp(-0.5 * ((h - c) / 3.2) ** 2)
        y += base * jw * np.sin(2 * np.pi * fh * t + rng.uniform(0, 6.28))
    y *= env
    y /= max(1e-9, np.max(np.abs(y)))
    _HIT_CACHE[key] = y
    return y


def render_tanpura(notes, cfg, prm, rng, total_n):
    out = np.zeros((total_n, 2))
    for e in notes:
        f = float(dsp.midi_hz(e.p))
        y = tanpura_note(f, 5.5, int(rng.integers(0, 3)))
        s = int(round(e.t * SR))
        k = min(len(y), int((e.d + 2.5) * SR))
        yy = y[:k].copy()
        fd = int(0.8 * SR)
        if fd < k:
            yy[k - fd:] *= np.linspace(1, 0, fd)
        gl, gr = dsp.pan_gains(prm.get("pan", 0.0) + rng.uniform(-0.3, 0.3))
        a0 = max(0, s)
        a1 = min(total_n, s + k)
        if a1 > a0:
            seg = yy[a0 - s:a1 - s] * e.v ** 1.5
            out[a0:a1, 0] += seg * gl
            out[a0:a1, 1] += seg * gr
    return out * 0.35


# ============================================================================
# AMBIENTE: viento, mar, brasas (ruido con movimiento)
# ============================================================================

def render_ambient(notes, kind, prm, rng, total_n):
    out = np.zeros((total_n, 2))
    for e in notes:
        s = max(0, int(e.t * SR))
        n = min(total_n - s, int(e.d * SR))
        if n <= 0:
            continue
        t = np.arange(n) / SR
        env = np.minimum(1, t / max(0.01, e.fl.get("fade", 2.0))) * \
            np.minimum(1, (e.d - t) / max(0.01, e.fl.get("fade", 2.0)))
        env = np.clip(env, 0, 1) * e.v
        nz = rng.standard_normal((n, 2))
        if kind == "wind":
            cf = np.exp(np.log(700) + 0.9 * dsp.slow_noise(n, 0.25, rng))
            gust = np.clip(0.8 + 0.3 * dsp.slow_noise(n, 0.18, rng), 0.4, 1.4)
            y = dsp.tv_filter(nz, "bp", cf, q=1.4, block=512)
            y += dsp.filt(nz, dsp.eq([("hp", 2500), ("lp", 9000)])) * 0.08
            y *= gust[:, None]
        elif kind == "sea":
            per = e.fl.get("period", 7.5)
            sw = 0.5 - 0.5 * np.cos(2 * np.pi * t / per + rng.random() * 6)
            sw = sw ** 2.0 * (0.8 + 0.3 * dsp.slow_noise(n, 0.1, rng))
            lo = dsp.filt(nz, dsp.eq([("lp", 600)])) * 0.8
            hi = dsp.filt(nz, dsp.eq([("hp", 1800), ("lp", 9000)])) * 0.5
            y = lo * (0.3 + sw[:, None]) + hi * sw[:, None] ** 1.5
        elif kind == "fire":
            y = dsp.filt(nz, dsp.eq([("lp", 400)])) * (0.6 + 0.3 * dsp.slow_noise(n, 2, rng))[:, None]
            # chasquidos: rafagas de 1-4 ms (no impulsos de una muestra, que
            # suenan a clic digital), cada una con su brillo
            cr = (rng.random((n, 2)) < 0.0007) * rng.uniform(0.1, 1.0, (n, 2)) * \
                np.sign(rng.standard_normal((n, 2)))
            k = int(0.0025 * SR)
            ker = np.exp(-np.arange(k) / (0.0008 * SR)) * rng.standard_normal(k)
            ker /= np.max(np.abs(ker))
            cr = signal.oaconvolve(cr, ker[:, None], mode="full", axes=0)[:n]
            cr = np.tanh(dsp.filt(cr, dsp.eq([("hp", 900), ("lp", 7000)])) * 1.5) * 0.25
            y += cr
        elif kind == "water":
            # arroyo: burbujas (chirridos que suben) sobre un siseo
            y = dsp.filt(nz, dsp.eq([("hp", 900), ("lp", 5000)])) * 0.3 * \
                (0.7 + 0.3 * dsp.slow_noise(n, 3, rng))[:, None]
            nb = int(e.d * 9)
            for _ in range(nb):
                bt = int(rng.uniform(0, n - 0.05 * SR))
                m = int(0.03 * SR)
                f0 = rng.uniform(600, 1800)
                tt = np.arange(m) / SR
                ch = np.sin(2 * np.pi * np.cumsum(f0 * (1 + 25 * tt)) / SR) * np.exp(-tt / 0.008)
                y[bt:bt + m, int(rng.integers(0, 2))] += ch * rng.uniform(0.1, 0.4)
        else:
            raise ValueError(kind)
        out[s:s + n] += y * env[:, None]
    return out * 0.25
