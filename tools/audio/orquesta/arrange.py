"""Ayudas de arreglo comunes a las 47 partituras: el MOTIVO del juego, las
tesituras, secciones de cuerda/coro con conduccion de voces, bordones,
percusion por secciones y pequenas utilidades de forma.

El motivo (maqam Hijaz, MI): "0 1 2 4" = MI FA SOL# SI, la llamada del profeta,
y la caida por la segunda aumentada. Se transforma por modo (dorico, lidio,
mayor...), por aumentacion (Ascension), por inversion (Derrota) y por
fragmentos (cabeza = el primer compas) en los temas de capitulo.
"""

from compose import Score, Scale, nm, voice_chords, pad, IQA, parse_mel  # noqa: F401

# El tema. 4 compases de 4/4.
MOTIF = "0:1.5 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 2:2 | 4:1 5:.5 4:.5 3:1 2:1 | 1:1 2:.5 1:.5 0:2"
MOTIF_HEAD = "0:1.5 1:.5 2:1 4:1"
# Respuesta (los 4 compases que contestan al motivo)
ANSWER = "4:1.5 5:.5 4:1 2:1 | 3:.75 2:.25 1:1 -1:2 | 0:1 1:.5 2:.5 3:1 4:1 | 2:1 1:.5 2:.5 0:2"


def transpose_mel(text, k):
    """Sube k grados una melodia en notacion compacta (respeta silencios,
    ligaduras, alteraciones, octavas y adornos)."""
    out = []
    for tok in text.split():
        if tok == "|":
            out.append(tok)
            continue
        head, sep, rest = tok.partition(":")
        if head in ("r", "-") or not head:
            out.append(tok)
            continue
        i = 1 if head[0] == "-" else 0
        while i < len(head) and head[i].isdigit():
            i += 1
        num = head[:i]
        if num in ("", "-"):
            out.append(tok)
            continue
        out.append(str(int(num) + k) + head[i:] + sep + rest)
    return " ".join(out)


def augment(text, f):
    """Multiplica las duraciones por f."""
    out = []
    for tok in text.split():
        if tok == "|":
            continue          # las barras ya no cuadran: cada compas vale f compases
        if ":" not in tok:
            out.append(tok)
            continue
        head, _, rest = tok.partition(":")
        j = 0
        while j < len(rest) and (rest[j].isdigit() or rest[j] == "."):
            j += 1
        num = rest[:j]
        if num.endswith(".") and num.count(".") == 1 and not num.startswith("."):
            num, j = num[:-1], j - 1
        try:
            d = float(num) * f
        except ValueError:
            out.append(tok)
            continue
        out.append(f"{head}:{d:g}{rest[j:]}")
    return " ".join(out)


# Tesituras (MIDI) para las conducciones de voces
STR5 = [("cb", (31, 50)), ("vc", (40, 60)), ("vla", (50, 67)), ("vln2", (57, 74)),
        ("vln1", (62, 81))]
STR4 = STR5[:4]
CHOIR4 = [("choir_b", (41, 57)), ("choir_t", (48, 64)), ("choir_a", (55, 69)),
          ("choir_s", (60, 77))]

# Ganancia de base de cada seccion (equilibrio medido a igual dinamica)
GAIN = {"cb": 0.0, "vc": 0.0, "vla": -0.5, "vln2": -0.5, "vln1": 0.0,
        "choir_b": 0.0, "choir_t": 0.0, "choir_a": -0.5, "choir_s": 0.0}


def sections(S, spec, prefix="", rev=None, gain=0.0, **kw):
    """Crea una parte por seccion. spec = lista de (inst, rango)."""
    rev = rev if rev is not None else {"hall": -6}
    parts = {}
    for inst, _ in spec:
        parts[inst] = S.part(prefix + inst, inst, rev=dict(rev),
                             gain=gain + GAIN.get(inst, 0.0), **kw)
    return parts


def chords_to(parts, spec, sc, bar, chords, bpc=1, vel=0.55, octave=0, start=None, **fl):
    """Escribe una progresion conducida en las secciones `spec`."""
    ranges = [(lo + 12 * octave, hi + 12 * octave) for _, (lo, hi) in spec]
    V = voice_chords(sc, chords, ranges, start=start)
    for i, (inst, _) in enumerate(spec):
        if inst in parts:
            pad(parts[inst], sc, bar, V, i, bars_per_chord=bpc, vel=vel, **fl)
    return V


def drone(part, pitches, bar, bars, vel=0.5, every=4, **fl):
    """Bordon re-arcado cada `every` compases (las notas se solapan un poco)."""
    if isinstance(pitches, (int, float)):
        pitches = [pitches]
    b = bar
    while b < bar + bars - 1e-6:
        L = min(every, bar + bars - b)
        for i, p in enumerate(pitches):
            part.n(part.score.beat(b), L * part.score.meter, p, vel, voice=100 + i, **fl)
        b += L
    return part


def tanpura(part, sc, bar, bars, root_oct=-1, step=1.0, vel=0.45, fifth=4):
    """Ciclo de tanpura: quinta, tonica, tonica, tonica grave (uno por pulso)."""
    seq = [sc.p(fifth, root_oct), sc.p(0, root_oct + 1), sc.p(0, root_oct + 1),
           sc.p(0, root_oct)]
    b = part.score.beat(bar)
    n = int(bars * part.score.meter / step)
    for i in range(n):
        part.n(b + i * step, step * 3.0, seq[i % 4], vel * (1.0 if i % 4 == 0 else 0.85))
    return part


def section_dyn(points):
    """[(compas, factor)...] tal cual; atajo legible."""
    return points
