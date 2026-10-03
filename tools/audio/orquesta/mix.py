"""Mezcla y mastering de una partitura: instrumentos -> cuerpos -> buses de
reverb (convolucion circular con IR sinteticas) -> bus maestro.

Bus maestro (en este orden, todo CIRCULAR para que el bucle cierre):
  paso alto 35 Hz -> compresor de pegamento 2:1 -> saturacion de cinta (x2)
  -> equilibrio de aire/cuerpo (shelves adaptativos) -> -18 LUFS
  -> limitador de pico verdadero a -1,5 dBTP.
"""

from collections import defaultdict

import numpy as np

import dsp
from dsp import SR
import instruments as I
from compose import part_events

TAIL_S = 14.0      # cola maxima de una nota mas alla del periodo (se pliega)

# Calibracion (dB): con ella, una nota tenida de dinamica 0,6 en la tesitura
# media de cada instrumento sale a ~-20 LUFS (pulsadas ~-22, campanas ~-24).
# Asi las ganancias de las partituras hablan de EQUILIBRIO musical y no de
# compensar lo fuerte o flojo que salga cada modelo de sintesis.
CAL = {
    "cb": -1.0, "vc": 2.9, "vla": 1.3, "vln2": 0.5, "vln1": -2.4, "solo_vln": 2.4,
    "solo_vla": -2.6, "solo_vc": 1.2, "choir_b": 1.0, "choir_t": 0.7, "choir_a": 2.1,
    "choir_s": 0.2, "voice": 2.1, "voice_m": 0.6, "duduk": -6.5, "ney": -7.1, "kaval": -4.6,
    "shofar": 3.0, "horns": 8.0, "lowbrass": 8.8, "oud": 9.5, "kanun": 8.0, "harp": 9.6,
    "lyre": 11.9, "pizz": 14.4, "celesta": 4.2, "bells": 4.5, "crotales": 5.1, "frame": 7.7,
    "daf": 6.4, "darbuka": 8.7, "riq": 2.0, "taiko": 5.5, "timp": 2.9, "zills": -5.8,
    "shaker": 1.4, "clap": 1.3, "gong": 4.3}

# ============================================================================
# Salas
# ============================================================================

ROOMS = {
    # sala de concierto: la orquesta
    "hall": dict(t60=2.5, predelay=0.024, er_n=16, er_span=0.08, er_gain=0.5, damp=0.85,
                 low_mult=1.1, onset=0.03, seed=11),
    # templo de piedra: reflexiones tempranas separadas, cola larga y clara
    "temple": dict(t60=3.6, predelay=0.040, er_n=10, er_span=0.16, er_gain=0.75, damp=1.0,
                   bright=1.25, low_mult=1.0, onset=0.05, seed=23),
    # cueva: oscura, muy larga
    "cave": dict(t60=5.0, predelay=0.055, er_n=8, er_span=0.22, er_gain=0.6, damp=0.55,
                 low_mult=1.25, onset=0.08, seed=37),
    # estancia corta: percusion de cerca, palmas
    "room": dict(t60=0.9, predelay=0.008, er_n=12, er_span=0.03, er_gain=0.6, damp=0.8,
                 low_mult=1.0, onset=0.01, seed=5),
}

_IR = {}


def get_ir(name):
    if name not in _IR:
        _IR[name] = dsp.make_ir(**ROOMS[name])
    return _IR[name]


# ============================================================================
# Lineas: reparto de voces y frases
# ============================================================================

def split_lines(evs, phrase_gap=0.45):
    """Reparte notas (posible polifonia) en lineas monofonicas y cada linea en
    frases (un hueco de mas de `phrase_gap` s corta la frase)."""
    groups = defaultdict(list)
    auto = []
    for e in evs:
        if "voice" in e.fl:
            groups[("v", e.fl["voice"])].append(e)
        else:
            auto.append(e)
    # asignacion automatica: la voz libre mas cercana en altura
    voices = []     # [fin, ultima_altura, lista]
    for e in sorted(auto, key=lambda x: (x.t, x.p if not isinstance(x.p, str) else 0)):
        best = None
        for v in voices:
            if v[0] <= e.t + 0.02:
                d = abs(v[1] - e.p) + (e.t - v[0]) * 0.5
                if best is None or d < best[0]:
                    best = (d, v)
        if best is None:
            v = [0.0, e.p, []]
            voices.append(v)
        else:
            v = best[1]
        v[2].append(e)
        v[0] = e.t + e.d
        v[1] = e.p
    lines = list(groups.values()) + [v[2] for v in voices]
    phrases = []
    for ln in lines:
        ln = sorted(ln, key=lambda x: x.t)
        cur = []
        end = -1e9
        for e in ln:
            if cur and e.t - end > phrase_gap:
                phrases.append(cur)
                cur = []
            # monofonia estricta: si se solapa, recorta la anterior
            if cur and e.t < cur[-1].t + cur[-1].d:
                cur[-1].d = max(0.03, e.t - cur[-1].t)
            cur.append(e)
            end = e.t + e.d
        if cur:
            phrases.append(cur)
    return phrases


# ============================================================================
# Una parte -> estereo
# ============================================================================

def render_part(part, total_n, rng, evs=None):
    if evs is None:
        evs = part_events(part, rng)
    if not evs:
        return None
    inst = part.inst
    prm = dict(part.prm)
    y = None
    eqspec = None
    if inst in I.STRINGS:
        cfg = I.STRINGS[inst]
        y = I.render_bowed(split_lines(evs), cfg, prm, rng, total_n)
        y = I.apply_body(y, cfg["body"], seed=prm.get("body_seed", hash_inst(inst)),
                         depth=0.95 if cfg["nv"] == 1 else 0.55)
        eqspec = cfg["eq"]
    elif inst in I.CHOIR:
        cfg = I.CHOIR[inst]
        y = I.render_choir(split_lines(evs), cfg, prm, rng, total_n)
        eqspec = [("hp", 90 if cfg["reg"] in "sa" else 60), ("lp", 10000)]
    elif inst in I.WINDS:
        cfg = I.WINDS[inst]
        y = I.render_wind(split_lines(evs, 0.3), cfg, prm, rng, total_n)
        eqspec = cfg["eq"]
    elif inst in I.BRASS:
        cfg = I.BRASS[inst]
        y = I.render_brass(split_lines(evs), cfg, prm, rng, total_n)
        eqspec = cfg["eq"]
    elif inst in I.PLUCK:
        cfg = I.PLUCK[inst]
        y = I.render_pluck(evs, cfg, prm, rng, total_n, inst)
        y = I.apply_body(y, cfg["body"], seed=hash_inst(inst) + 3, wet=0.8, depth=0.8)
        eqspec = cfg["eq"]
    elif inst in I.PERC:
        cfg = I.PERC[inst]
        y = I.render_perc(evs, cfg, prm, rng, total_n, inst)
        eqspec = cfg["eq"]
    elif inst in I.BELLS:
        cfg = I.BELLS[inst]
        y = I.render_bells(evs, cfg, prm, rng, total_n, inst)
        eqspec = cfg["eq"]
    elif inst == "tanpura":
        y = I.render_tanpura(evs, {}, prm, rng, total_n)
        eqspec = [("hp", 60), ("lp", 7500)]
    elif inst in ("wind", "sea", "fire", "water"):
        y = I.render_ambient(evs, inst, prm, rng, total_n)
        eqspec = [("hp", 80)]
    else:
        raise ValueError(f"instrumento desconocido: {inst}")
    if eqspec:
        y = dsp.filt(y, dsp.eq(eqspec))
    if part.eq:
        y = dsp.filt(y, dsp.eq(part.eq))
    if part.far > 0:
        y = dsp.filt(y, dsp.eq([("lp", 13000 - 8000 * part.far, 0.6)]))
    return y * dsp.undb(part.gain + CAL.get(inst, 0.0))


def hash_inst(name):
    import zlib
    return zlib.crc32(name.encode()) % 1000


# ============================================================================
# Partitura completa -> bucle masterizado
# ============================================================================

def render_score(score, verbose=False, stems=False):
    P = int(round(score.period * SR))
    # Los eventos primero (con su propio flujo aleatorio): la cola del buffer
    # tiene que cubrir la nota que MAS se pase del periodo. Una nota larga del
    # compas 0 cuyo ataque humanizado cae unos ms antes del cero se pliega al
    # FINAL del bucle y entera va a la cola; con una cola fija mas corta que
    # la nota, el final del bucle se quedaba sin ella.
    all_evs = []
    max_end = score.period
    for part in score.parts:
        evs = part_events(part, dsp.rng_for(score.name, part.name, "ev"))
        all_evs.append(evs)
        for e in evs:
            max_end = max(max_end, e.t + e.d)
    total = P + int((max_end - score.period + TAIL_S) * SR)
    dry = np.zeros((total, 2))
    buses = {}
    info = {"parts": {}}
    stem_out = {}
    for part, evs in zip(score.parts, all_evs):
        rng = dsp.rng_for(score.name, part.name)
        y = render_part(part, total, rng, evs)
        if y is None:
            continue
        lvl = float(np.sqrt(np.mean(y ** 2)))
        info["parts"][part.name] = 20 * np.log10(max(lvl, 1e-9))
        dry_g = 1.0 - 0.45 * part.far
        dry += y * dry_g
        for bus, gdb in part.rev.items():
            if bus not in buses:
                buses[bus] = np.zeros((total, 2))
            buses[bus] += y * dsp.undb(gdb) * (1.0 + 0.6 * part.far)
        if stems:
            stem_out[part.name] = dsp.fold(y, P)
        if verbose:
            print(f"    {part.name:14s} {part.inst:10s} rms {info['parts'][part.name]:6.1f} dB",
                  flush=True)
    mix = dsp.fold(dry, P)
    for bus, x in buses.items():
        xf = dsp.fold(x, P)
        # retorno de sala: medido, deja la relacion directo/reverb en ~+3 dB en
        # una pista tipica (+6 a +7 era una toma de microfono cercano, seca)
        mix += dsp.reverb_stereo(xf, get_ir(bus), P) * 0.72
    out, minfo = master(mix, score.master)
    info.update(minfo)
    if stems:
        info["stems"] = stem_out
    return out, info


def band_shares(x):
    mono = x.mean(axis=1)
    seg = 1 << 15
    n = len(mono)
    while seg > 256 and seg > n:
        seg >>= 1
    win = np.hanning(seg)
    acc = np.zeros(seg // 2 + 1)
    for i in range(0, max(1, n - seg + 1), seg // 2):
        c = mono[i:i + seg]
        if len(c) < seg:
            break
        acc += np.abs(np.fft.rfft(c * win)) ** 2
    fr = np.fft.rfftfreq(seg, 1.0 / SR)
    tot = float(np.sum(acc)) or 1e-12
    return (float(np.sum(acc[fr < 200]) / tot), float(np.sum(acc[fr >= 2000]) / tot))


def block_span(x, nb=32):
    mono = x.mean(axis=1)
    n = len(mono)
    b = [np.sqrt(np.mean(mono[i * n // nb:(i + 1) * n // nb] ** 2)) for i in range(nb)]
    d = 20 * np.log10(np.maximum(b, 1e-9))
    return float(np.max(d) - np.min(d))


def leveler(x, max_span=12.5):
    from scipy.ndimage import uniform_filter1d
    if block_span(x) <= max_span:
        return x, 1.0
    p = np.mean(x ** 2, axis=1)
    p = uniform_filter1d(p, size=int(1.0 * SR), mode="wrap")
    e = 10 * np.log10(p + 1e-12)
    e = uniform_filter1d(e, size=int(2.5 * SR), mode="wrap")
    med = float(np.median(e))
    y = x
    R = 1.0
    for R in (1.2, 1.4, 1.6, 1.8, 2.1, 2.5, 3.0, 4.0):
        g = -(e - med) * (1.0 - 1.0 / R)
        g = np.clip(g, -9.0, 7.0)
        y = x * dsp.undb(g)[:, None]
        if block_span(y) <= max_span:
            break
    return y, R


def master(x, m):
    warm = int(4.0 * SR)
    info = {}
    # 1) paso alto 35 Hz (4o orden) - nada de retumbe por debajo
    hp = dsp.eq([("hp4", 35.0)])
    x = dsp.circular(lambda z: dsp.filt(z, hp), x, warm)
    # 2) equilibrio de aire y cuerpo ANTES de la dinamica (shelves adaptativos):
    #    lo mismo que hace un ingeniero de mastering pista a pista, para que
    #    todas queden en el mismo sitio aunque la orquestacion cambie.
    low, high = band_shares(x)
    air_db = 0.0
    if high < m["air_target"]:
        y = x
        for g in (1.5, 3.0, 4.5, 6.0, 7.5):
            y = dsp.circular(lambda z: dsp.filt(z, dsp.eq([("hs", 2600, g)])), x, warm)
            air_db = g
            if band_shares(y)[1] >= m["air_target"]:
                break
        x = y
    body_db = 0.0
    low, high = band_shares(x)
    if low < m["low_target"]:
        y = x
        for g in (1.5, 3.0, 4.5, 6.0, 7.5):
            y = dsp.circular(lambda z: dsp.filt(z, dsp.eq([("ls", 160, g)])), x, warm)
            body_db = g
            if band_shares(y)[0] >= m["low_target"]:
                break
        x = y
    info["air_db"], info["body_db"] = air_db, body_db
    # balance estatico L/R (maximo +-2 dB): la cuerda grave y la percusion de
    # mano tiran a la derecha en la colocacion de orquesta
    rl = float(np.sqrt(np.mean(x[:, 0] ** 2) / max(1e-12, np.mean(x[:, 1] ** 2))))
    g = float(np.clip(1.0 / np.sqrt(rl), 10 ** (-2 / 20), 10 ** (2 / 20)))
    x = x * np.array([g, 1.0 / g])[None, :]
    info["balance_db"] = 20 * np.log10(g)
    # 2b) nivelador: solo si el arco dinamico se pasa de 12,5 dB entre el tramo
    #     mas fuerte y el mas flojo. Es "llevar el fader": sube lo flojo y baja
    #     lo fuerte alrededor de la mediana, con una envolvente lentisima
    #     (circular, el bucle sigue cerrando). Musica de fondo de un juego: un
    #     tramo 15 dB por debajo se pierde bajo los efectos.
    x, info["level_ratio"] = leveler(x, m.get("max_span", 12.5))
    # 3) compresor de pegamento: umbral relativo a la sonoridad de la pista
    l0 = dsp.lufs(x)
    thr = l0 - 1.0
    x, gr = dsp.compressor(np.concatenate([x[-warm:], x]), thr, ratio=m["comp_ratio"],
                           attack=0.03, release=0.35)
    x = x[warm:]
    info["comp_gr"] = gr
    # 4) cinta: se lleva la senal a un nivel de referencia, se satura y se devuelve
    pk = float(np.max(np.abs(x))) or 1.0
    ref = 0.95 / pk
    if m["tape"] > 0:
        x = dsp.circular(lambda z: dsp.tape(z * ref, m["tape"]) / ref, x, int(0.1 * SR))
    # 5) sonoridad + limitador (dos pasadas: el limitador baja un poco la sonoridad)
    ceiling = 10 ** (m["ceiling_db"] / 20.0)
    for _ in range(3):
        l1 = dsp.lufs(x)
        x = x * 10 ** ((m["lufs"] - l1) / 20.0)
        x, lim_db = dsp.limiter(x, ceiling)
        if abs(dsp.lufs(x) - m["lufs"]) < 0.15:
            break
    info["limit_db"] = lim_db
    info["lufs"] = dsp.lufs(x)
    info["tp"] = 20 * np.log10(max(dsp.true_peak(x), 1e-9))
    return x.astype(np.float32), info
