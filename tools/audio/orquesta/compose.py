"""Composicion: modos y maqamat, notacion compacta de melodias con adornos,
conduccion de voces, ritmos (iqa'at), arpegios, ostinatos y el modelo de
partitura (Score / Part) que luego renderiza mix.py.

Todo en PULSOS (negras); la conversion a segundos, la humanizacion y el
pliegue al periodo del bucle se hacen en `Part.events`.
"""

import re

import numpy as np

from instruments import Ev

# ============================================================================
# Modos y maqamat (semitonos desde la tonica; .5 = cuarto de tono)
# ============================================================================

MODES = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "aeolian": [0, 2, 3, 5, 7, 8, 10],
    "hijaz": [0, 1, 4, 5, 7, 8, 10],        # maqam Hijaz (= frigio dominante)
    "hijazkar": [0, 1, 4, 5, 7, 8, 11],     # doble armonica: la 7a mayor
    "harmminor": [0, 2, 3, 5, 7, 8, 11],
    "melminor": [0, 2, 3, 5, 7, 9, 11],
    "bayati": [0, 1.5, 3, 5, 7, 8, 10],     # segunda de tres cuartos de tono
    "rast": [0, 2, 3.5, 5, 7, 9, 10.5],
    "saba": [0, 1.5, 3, 4, 7, 8, 10],
}

NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6,
        "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}


def nm(name, octave):
    """'E', 3 -> 52 (MI3, convencion de Sonic Pi: DO4 = 60)."""
    return 12 * (octave + 1) + NOTE[name]


class Scale:
    def __init__(self, root, mode):
        self.root = root
        self.mode = mode
        self.steps = MODES[mode]

    def p(self, deg, oct=0, acc=0.0):
        d = int(np.floor(deg))
        o, i = divmod(d, 7)
        return self.root + 12 * (oct + o) + self.steps[i] + acc

    def pc(self, deg):
        return (self.steps[int(deg) % 7]) % 12

    def chord_pcs(self, degs):
        return [(self.root + self.steps[int(d) % 7]) % 12 for d in degs]

    def with_mode(self, mode):
        return Scale(self.root, mode)


# ============================================================================
# Notacion de melodias
# ============================================================================
#
#   "0:1.5 1:.5 2:1 4 | 3:.75 2:.25 1:1 2:2^ r:1 -:1"
#
#   grado[alt][octava][:duracion][adornos]
#     grado    entero (negativo vale); r = silencio; - = liga con la anterior
#     alt      # b (semitono)  q d (cuarto de tono arriba / abajo)
#     octava   ' sube, , baja (se pueden repetir)
#     dur      en pulsos; si falta, la de la nota anterior
#     adornos  ^ entra desde abajo (portamento)    s portamento largo desde la anterior
#              g apoyatura desde arriba  h desde abajo   m mordente  w mordente inferior
#              t trino   v caida al final   > acento   . picado   ~ vibrato ancho
#              ! re-articula (no ligar)   _ sin vibrato
#   Las barras | son solo para leer.

def parse_mel(text):
    out = []
    prev_dur = 1.0
    for tok in text.replace("|", " ").split():
        head, _, rest = tok.partition(":")
        orn = ""
        if rest:
            mm = re.match(r"^(\d*\.?\d+)(.*)$", rest)
            if mm:
                dur = float(mm.group(1))
                orn = mm.group(2)
            else:
                dur = prev_dur
                orn = rest
        else:
            j = len(head)
            while j > 0 and head[j - 1] in "^sghmwtv>.~!_":
                j -= 1
            orn = head[j:]
            head = head[:j]
            dur = prev_dur
        prev_dur = dur
        if head == "r":
            out.append(("r", dur, 0, 0, orn))
            continue
        if head == "-":
            out.append(("-", dur, 0, 0, orn))
            continue
        k = 0
        if head and head[0] == "-":
            k = 1
        while k < len(head) and head[k].isdigit():
            k += 1
        deg = int(head[:k])
        acc = 0.0
        octv = 0
        for ch in head[k:]:
            if ch == "#":
                acc += 1
            elif ch == "b":
                acc -= 1
            elif ch == "q":
                acc += 0.5
            elif ch == "d":
                acc -= 0.5
            elif ch == "'":
                octv += 1
            elif ch == ",":
                octv -= 1
        out.append((deg, dur, acc, octv, orn))
    return out


# ============================================================================
# Partitura
# ============================================================================

WARN = []


def check_bars(text, meter, where=""):
    """Si la melodia lleva barras |, cada compas tiene que sumar `meter`
    pulsos. Un error de duracion no da excepcion: desplaza todo lo que viene
    detras y la melodia acaba cayendo a contratiempo de la armonia."""
    if "|" not in text:
        return
    segs = text.split("|")
    for i, sgm in enumerate(segs):
        toks = parse_mel(sgm)
        tot = sum(t[1] for t in toks)
        last = i == len(segs) - 1
        if abs(tot - meter) > 1e-6 and not (last and tot < meter):
            WARN.append(f"{where}: compas {i} de la melodia suma {tot:g} pulsos "
                        f"(deberian ser {meter}): '{sgm.strip()}'")


class Part:
    def __init__(self, score, name, inst, gain=0.0, rev=None, eq=None, human=1.0,
                 far=0.0, **prm):
        self.score = score
        self.name = name
        self.inst = inst
        self.gain = gain
        self.rev = rev if rev is not None else {}
        self.eq = eq
        self.human = human
        self.far = far
        self.prm = prm
        self.notes = []       # (pulso, dur, altura, vel, flags)
        self.dyn_pts = None

    # ---- notas sueltas ------------------------------------------------------
    def n(self, beat, dur, pitch, vel=0.7, **fl):
        self.notes.append((float(beat), float(dur), pitch, float(vel), fl))
        return self

    def dyn(self, pts):
        """Curva de dinamica por compases: [(compas, factor), ...]."""
        self.dyn_pts = pts
        return self

    # ---- melodias -------------------------------------------------------------
    def mel(self, bar, text, sc, oct=0, vel=0.72, **fl):
        """Coloca una melodia en notacion compacta a partir del compas `bar`
        (puede ser fraccionario). Devuelve el pulso en el que acaba."""
        b = self.score.beat(bar)
        b_start = b
        first_idx = len(self.notes)
        spb = self.score.spb
        check_bars(text, self.score.meter, f"{self.score.name}/{self.name}@{bar}")
        toks = parse_mel(text)
        last = None
        for (deg, dur, acc, octv, orn) in toks:
            if deg == "r":
                b += dur
                last = None
                continue
            if deg == "-":
                if last is not None:
                    bb, dd, pp, vv, ff = self.notes[last]
                    self.notes[last] = (bb, dd + dur, pp, vv, ff)
                b += dur
                continue
            p = sc.p(deg, oct + octv, acc)
            v = vel
            f = dict(fl)
            if ">" in orn:
                f["acc"] = True
                v = min(1.0, v * 1.15)
            if "~" in orn:
                f["vib"] = 1.5
            if "_" in orn:
                f["vib"] = 0.0
            if "!" in orn:
                f["sep"] = True
            if "^" in orn:
                f["scoop"] = 140.0
                f["scoop_t"] = 0.16
            if "s" in orn:
                f["slide"] = True
            if "v" in orn:
                f["fall"] = 1.2
            d = dur
            if "." in orn:
                d = dur * 0.5
                f["sep"] = True
            g = min(0.075 / spb, dur * 0.3)        # apoyatura ~75 ms
            fast = dict(glide=0.025, dip=0.25, vib=0.0)
            if "g" in orn or "h" in orn:
                gp = sc.p(deg + (1 if "g" in orn else -1), oct + octv)
                self.notes.append((b, g, gp, v * 0.85, dict(f, **fast)))
                self.notes.append((b + g, d - g, p, v, dict(f, glide=0.03)))
            elif "m" in orn or "w" in orn:
                np_ = sc.p(deg + (1 if "m" in orn else -1), oct + octv)
                self.notes.append((b, g, p, v, dict(f, **fast)))
                self.notes.append((b + g, g, np_, v * 0.85, dict(f, **fast)))
                self.notes.append((b + 2 * g, d - 2 * g, p, v, dict(f, glide=0.03)))
            elif "t" in orn:
                up = sc.p(deg + 1, oct + octv)
                step = max(g * 0.9, 0.085 / spb)
                tt = b
                k = 0
                while tt + step < b + d * 0.75:
                    self.notes.append((tt, step, p if k % 2 == 0 else up, v * (1 if k % 2 == 0 else 0.85),
                                       dict(f, glide=0.02, dip=0.15, vib=0.0)))
                    tt += step
                    k += 1
                self.notes.append((tt, b + d - tt, p, v, dict(f, glide=0.025)))
            else:
                self.notes.append((b, d, p, v, f))
            last = len(self.notes) - 1
            b += dur
        if fl.get("shape", True):
            self._shape_phrases(first_idx, b_start, b)
        return b

    def _shape_phrases(self, i0, b0, b1):
        """Fraseo: lo que haria un interprete con las notas que acaba de poner
        `mel`. Cada frase (separada por silencios de un pulso o mas) hace un
        arco suave de dinamica, las notas agudas pesan un poco mas y la nota
        final de frase se suelta. Sin esto, todas las notas de una melodia
        salen a la misma dinamica y suena a secuenciador."""
        idx = list(range(i0, len(self.notes)))
        if len(idx) < 2:
            return
        phrases, cur = [], [idx[0]]
        for a, c in zip(idx, idx[1:]):
            ba, da = self.notes[a][0], self.notes[a][1]
            if self.notes[c][0] - (ba + da) >= 0.99:
                phrases.append(cur)
                cur = []
            cur.append(c)
        phrases.append(cur)
        for ph in phrases:
            ps = [self.notes[k][2] for k in ph if not isinstance(self.notes[k][2], str)]
            if not ps:
                continue
            pm = float(np.mean(ps))
            s0 = self.notes[ph[0]][0]
            s1 = self.notes[ph[-1]][0] + self.notes[ph[-1]][1]
            span = max(1e-6, s1 - s0)
            for j, k in enumerate(ph):
                bb, dd, pp, vv, ff = self.notes[k]
                u = (bb - s0) / span
                g = 0.92 + 0.14 * np.sin(np.pi * min(1.0, u * 1.15))
                if not isinstance(pp, str):
                    g *= 1.0 + 0.02 * (pp - pm)
                if j == len(ph) - 1 and len(ph) > 2:
                    g *= 0.92
                self.notes[k] = (bb, dd, pp, float(np.clip(vv * g, 0.0, 1.0)), ff)

    # ---- acordes y voces -------------------------------------------------------
    def hold(self, bar, bars, pitches, vel=0.6, **fl):
        b = self.score.beat(bar)
        d = bars * self.score.meter
        for i, p in enumerate(pitches):
            self.notes.append((b, d, p, vel, dict(fl, voice=fl.get("voice", i))))
        return self

    # ---- patrones de percusion ---------------------------------------------------
    def pat(self, bar, bars, pattern, step=0.5, vel=0.75, swing=0.0, every=None,
            skip=(), accent=None, **fl):
        """Patron de golpes por compas: cada caracter es un paso de `step`
        pulsos. Mayuscula = golpe, minuscula = golpe flojo, '.' = nada.
        `skip` = compases (relativos) que se saltan. `accent`: dict paso->factor."""
        pattern = pattern.replace(" ", "")
        steps_per_bar = int(round(self.score.meter / step))
        for k in range(bars):
            if k in skip:
                continue
            b0 = self.score.beat(bar + k)
            for i in range(steps_per_bar):
                ch = pattern[(k * steps_per_bar + i) % len(pattern)]
                if ch == ".":
                    continue
                v = vel * (0.55 if ch.islower() else 1.0)
                if accent and i in accent:
                    v *= accent[i]
                sw = swing * step if (i % 2 == 1) else 0.0
                self.notes.append((b0 + i * step + sw, step, ch.upper() if ch.isalpha() else ch,
                                   min(1.0, v), dict(fl)))
        return self

    def hit(self, beat, stroke, vel=0.8, **fl):
        self.notes.append((float(beat), 0.25, stroke, vel, fl))
        return self

    def roll(self, beat, dur, vel=0.6, stroke="T", cresc=0.8, **fl):
        self.notes.append((float(beat), float(dur), stroke, vel, dict(fl, roll=True, cresc=cresc)))
        return self

    # ---- arpegios y ostinatos -----------------------------------------------------
    def arp(self, bar, bars, chords, sc, pattern, step=0.5, oct=0, vel=0.6, per_bar=1,
            accent_first=1.15, dur_mult=1.0, **fl):
        """Arpegio sobre una progresion: `chords` = lista de grados de acorde
        (uno por cada 1/per_bar compas, ciclica). `pattern` = indices en el
        acorde extendido por octavas (0,1,2 = tonica, 3a, 5a; 3 = tonica 8va...)."""
        spb_bar = self.score.meter
        steps = int(round(spb_bar / step))
        for k in range(bars):
            for i in range(steps):
                beat_in_bar = i * step
                ci = (k * per_bar + int(beat_in_bar / (spb_bar / per_bar))) % len(chords)
                ch = chords[ci]
                idx = pattern[(k * steps + i) % len(pattern)]
                if idx is None:
                    continue
                o, j = divmod(idx, len(ch))
                p = sc.p(ch[j], oct + o)
                v = vel * (accent_first if i == 0 else 1.0)
                self.notes.append((self.score.beat(bar + k) + beat_in_bar, step * dur_mult,
                                   p, min(1.0, v), dict(fl)))
        return self


class Score:
    def __init__(self, name, bpm, bars, meter=4, title=""):
        self.name = name
        self.bpm = float(bpm)
        self.bars = bars
        self.meter = meter
        self.spb = 60.0 / self.bpm
        self.period = bars * meter * self.spb
        self.parts = []
        self.title = title
        self.master = dict(lufs=-18.0, ceiling_db=-1.5, comp_ratio=2.0, tape=1.0,
                           air_target=0.05, low_target=0.11)
        self.irs = {}

    def beat(self, bar):
        return float(bar) * self.meter

    def part(self, name, inst, **kw):
        p = Part(self, name, inst, **kw)
        self.parts.append(p)
        return p


# ============================================================================
# Pulsos -> segundos: humanizacion determinista y pliegue al periodo
# ============================================================================

def interp_dyn(pts, bar):
    if not pts:
        return 1.0
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return float(np.interp(bar, xs, ys))


def part_events(part, rng):
    sc = part.score
    spb = sc.spb
    per = sc.period
    evs = []
    sig_t = 0.009 * part.human
    for (b, d, p, v, fl) in sorted(part.notes, key=lambda x: x[0]):
        t = b * spb
        if not fl.get("tight") and part.human > 0:
            t += float(np.clip(rng.normal(0, sig_t), -2.5 * sig_t, 2.5 * sig_t))
        vv = v * interp_dyn(part.dyn_pts, b / sc.meter)
        if part.human > 0:
            vv *= float(np.clip(1.0 + rng.normal(0, 0.07 * part.human), 0.75, 1.25))
        vv = float(np.clip(vv, 0.0, 1.0))
        t = t % per
        evs.append(Ev(t, d * spb, p, vv, dict(fl)))
    evs.sort(key=lambda e: e.t)
    return evs


# ============================================================================
# Conduccion de voces
# ============================================================================

def voice_chords(sc, chords, ranges, bass_root=True, start=None):
    """Para cada acorde (lista de grados, el primero es la fundamental) elige
    una altura por voz dentro de su tesitura moviendo lo minimo. ranges = lista
    (grave->agudo) de (lo, hi) MIDI. Devuelve lista de listas de alturas."""
    import itertools
    res = []
    prev = start
    for ch in chords:
        pcs = [sc.p(d) % 12 for d in ch]
        root_pc = pcs[0]
        cands = []
        for vi, (lo, hi) in enumerate(ranges):
            opts = [m for m in range(int(lo), int(hi) + 1) if (m % 12) in [round(x) % 12 for x in pcs]]
            if vi == 0 and bass_root:
                opts = [m for m in opts if m % 12 == round(root_pc) % 12] or opts
            cands.append(opts)
        best = None
        for combo in itertools.product(*cands):
            if any(combo[i] >= combo[i + 1] for i in range(len(combo) - 1)):
                continue
            cost = 0.0
            if prev is not None:
                cost += sum(abs(a - b) for a, b in zip(combo, prev))
            got = set(m % 12 for m in combo)
            cost += 6.0 * sum(1 for x in pcs[:3] if round(x) % 12 not in got)
            # espacio: que las voces de arriba no se amontonen
            for i in range(1, len(combo) - 1):
                if combo[i + 1] - combo[i] < 3:
                    cost += 2.0
            if prev is None:
                cost += sum(abs(m - (lo + hi) / 2) for m, (lo, hi) in zip(combo, ranges)) * 0.3
            if best is None or cost < best[0]:
                best = (cost, combo)
        if best is None:
            combo = tuple(int((lo + hi) / 2) for lo, hi in ranges)
        else:
            combo = best[1]
        # alturas exactas (los cuartos de tono del modo se respetan)
        exact = []
        for m in combo:
            for d in ch:
                for o in range(-4, 5):
                    q = sc.p(d, o)
                    if abs(q - m) < 0.51:
                        m2 = q
                        break
                else:
                    continue
                break
            else:
                m2 = m
            exact.append(m2)
        res.append(exact)
        prev = combo
    return res


def pad(part, sc, bar, chords, voicing_idx, bars_per_chord=1, vel=0.6, hold_common=True,
        **fl):
    """Pone UNA voz de una conduccion (voicing_idx) como linea ligada: las
    notas comunes entre acordes se mantienen (no se re-atacan)."""
    seq = [v[voicing_idx] for v in chords]
    b = part.score.beat(bar)
    d = bars_per_chord * part.score.meter
    i = 0
    while i < len(seq):
        j = i
        while hold_common and j + 1 < len(seq) and abs(seq[j + 1] - seq[i]) < 0.01:
            j += 1
        part.n(b + i * d, (j - i + 1) * d, seq[i], vel, voice=fl.get("voice", voicing_idx),
               **{k: v for k, v in fl.items() if k != "voice"})
        i = j + 1


# ============================================================================
# Ritmos del Levante (iqa'at) en corcheas, compas de 4/4
# ============================================================================

IQA = {
    "maqsum": "DT.TD.T.",
    "baladi": "DD.TD.T.",
    "saidi": "DT.DD.T.",
    "malfuf": "D..T..T.",
    "ayoub": "D.kTD.kT",
    "wahda": "D...T...",
    "fallahi": "DTkTDTkT",
    "masmudi_half": "DD..T.k.",
}
