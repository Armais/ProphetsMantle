"""Acto II - Eliseo (capitulos 6-10). Mas fresco y mas lirico: agua,
sanacion. Violin solo, kanun, chelo, ney, y al final las trompas y el coro.

  cap   tema (jugando)                 apertura (dialogo)      estampa A              estampa B
  06    violin / kanun 3+3+2+darbuka    ney / arpa+bordon       chelo / cuerda grave+agua  campanas / marco 3+3+2
  07    kanun / pizzicato+riq           viola / lira            ney / cuerda grave+pizz    oud / palmas+darbuka
  08    chelo / cuerdas+arpa+latido     lira / coro             voz / cuerda grave         kanun / latido+crotalos
  09    ney / galope+ostinato           duduk / trompas         arpa / coro                kaval / agua+shaker
  10    trompas / taikos+ostinato+coro  oud / tremolo           ney / bordon+timbales      shofar / marcha de taikos
"""

from arrange import (Score, Scale, nm, IQA, MOTIF, ANSWER, STR5, STR4, CHOIR4,
                     sections, chords_to, drone, transpose_mel)

TRACKS = {}


def track(fn):
    TRACKS[fn.__name__] = fn
    return fn


c1, c2, c3, c4, c5, c6, c7 = ([0, 2, 4], [1, 3, 5], [2, 4, 6], [3, 5, 0], [4, 6, 1],
                              [5, 0, 2], [6, 1, 3])


def std_form():
    return 0, 4, 12, 20, 28


# ============================================================================
# CAPITULO 6 - LAS AGUAS DE JERICO (frigio en SI -> dorico en SI)
# ============================================================================
@track
def cap06():
    """"Las aguas son malas y la tierra esteril"; Eliseo echa sal en el
    manantial y el agua sana (2 Reyes 2:19-22). Empieza en FRIGIO, turbio, con
    el kanun cojeando en 3+3+2, y al sanar el agua (A') pasa a DORICO sobre la
    misma tonica: el violin repite su melodia y suena a otro mundo.
    100 BPM, 32 compases = 76,8 s."""
    S = Score("cap06", 100, 32)
    sp = Scale(nm("B", 2), "phrygian")
    sd = Scale(nm("B", 2), "dorian")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c2, c7, c1, c6, c2, c7, c1]
    pB = [c4, c2, c7, c2, c4, c6, c2, c2]
    pA2 = [c1, c4, c1, c4, c3, c7, c4, c1]
    st = sections(S, STR5, rev={"hall": -6})
    chords_to(st, STR5[:2], sp, INTRO, [c1, c2, c1, c2], vel=0.44)
    chords_to(st, STR5[:3], sp, A, pA, vel=0.46)
    chords_to(st, STR4, sp, B, pB, vel=0.52)
    chords_to(st, STR5, sd, A2, pA2, vel=0.58)
    chords_to(st, STR5[:2], sd, OUT, [c1, c4, c1, c1], vel=0.44)

    # kanun: 3+3+2 corcheas, el compas cojo del capitulo
    ka = S.part("kanun", "kanun", rev={"hall": -9})
    fig = [0, 1, 2, 0, 1, 2, 3, 2]
    acc = {0: 1.2, 3: 1.15, 6: 1.1}
    for k, (ch, sc_) in enumerate([(c, sp) for c in [c1, c2, c1, c2] + pA + pB] +
                                  [(c, sd) for c in pA2 + [c1, c4, c1, c1]]):
        for j in range(8):
            o, i = divmod(fig[j], 3)
            ka.n(S.beat(k) + j * 0.5, 0.5, sc_.p(ch[i], 1 + o), 0.5 * acc.get(j, 0.85))
    ka.dyn([(0, 0.75), (4, 0.9), (12, 0.85), (20, 1.0), (28, 0.8), (32, 0.75)])

    melA = ("0:1.5^ 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 2:2~ | 4:1 5:.5 4:.5 3:1 1:1 | "
            "2:1 1:.5 0:.5 0:2v | 4:1.5 5:.5 7:2~ | 8:1 7:.5 8:.5 6:2 | 6:1 5:1 4:1 3:1 | "
            "2:1 1:1 0:2")
    vn = S.part("violin", "solo_vln", rev={"hall": -6, "temple": -12}, gain=1)
    vn.mel(A, melA, sp, oct=2, vel=0.7)
    vn.mel(B + 4, "7:4~ | 8:2 9:2 | 8:4~ | 8:4", sp, oct=1, vel=0.62)
    vn.mel(A2, melA.replace("2:2~", "2:2t"), sd, oct=2, vel=0.78)
    vn.mel(OUT, "0:1.5^ 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 0:2v | r:4 | r:4", sd, oct=2, vel=0.6)
    vc = S.part("vc_mel", "vc", rev={"hall": -6}, gain=1)
    vc.mel(B, "3:2 4:1 5:1 | 5:2 4:1 2:1 | 3:3 1:1 | 1:4 | 3:2 4:1 5:1 | 7:2 5:2 | "
              "6:2 4:1 3:1 | 3:4", sp, oct=1, vel=0.62)

    db = S.part("darbuka", "darbuka", rev={"room": -9, "hall": -15})
    db.pat(INTRO, 4, "D.....T.....T...", step=0.25, vel=0.45)
    db.pat(A, 8, "D.k..TkkT.k..T.k", step=0.25, vel=0.6)
    db.pat(B, 8, "D.....T.....T...", step=0.25, vel=0.55)
    db.pat(A2, 7, "D.kk.TkkT.kk.TkT", step=0.25, vel=0.66)
    db.pat(A2 + 7, 1, "D.kkTkkTkkTkTkTk", step=0.25, vel=0.7)
    db.pat(OUT, 4, "D.....T.....T...", step=0.25, vel=0.45)
    fr = S.part("frame", "frame", rev={"hall": -11}, gain=-2)
    fr.pat(A2, 8, IQA["malfuf"], vel=0.66)
    # el agua sana: celesta, crotalos y violines agudos en tremolo
    cel = S.part("celesta", "celesta", rev={"hall": -6})
    cel.arp(A2, 8, pA2, sd, [6, None, 5, None, 4, 5, None, 3], step=0.5, oct=1, vel=0.45)
    tr = S.part("shimmer", "vln1", rev={"hall": -5}, art="trem", gain=-6)
    chords_to({"vln1": tr}, [("vln1", (71, 86))], sd, A2, pA2, vel=0.45)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=-3)
    cr.n(S.beat(B + 7) + 3, 2, sd.p(4, 3), 0.5).n(S.beat(A2), 2, sd.p(0, 4), 0.5)
    wa = S.part("water", "water", rev={"hall": -10}, gain=-6)
    wa.n(S.beat(INTRO), 20, 0, 0.55, fade=2.0)
    wa.n(S.beat(OUT - 1), 22, 0, 0.55, fade=2.0)
    return S


@track
def cap06_intro():
    """Apertura en Jerico: "el agua es mala y la tierra esteril". Ney sobre
    arpa y un bordon de cuerdas graves, sin percusion. Frigio en SI.
    80 BPM, 16 compases = 48 s."""
    S = Score("cap06_intro", 80, 16)
    sc = Scale(nm("B", 2), "phrygian")
    prog = [c1, c2, c1, c7, c1, c2, c6, c2, c4, c2, c7, c1, c6, c2, c1, c1]
    hp = S.part("harp", "harp", rev={"hall": -7})
    hp.arp(0, 16, prog, sc, [0, 2, 4, 2, 3, 2, 4, 2], step=0.5, oct=0, vel=0.48)
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"hall": -6})
    drone(lo["cb"], sc.p(0, 0), 0, 16, vel=0.44, every=4)
    drone(lo["vc"], sc.p(4, 0), 0, 16, vel=0.3, every=4)
    ney = S.part("ney", "ney", rev={"temple": -6, "hall": -12})
    ney.mel(2, "0:2^ 1:2~ | 2:1 1:1 0:2 | 4:3 3:.5 2:.5 | 1:4~ | 3:1.5 4:.5 5:2 | "
               "5:1 4:.5 3:.5 1:2 | 2:2 1:1 0:1 | 1:4~v | 4:2^ 5:1 4:1 | 3:1 2:1 1:2 | "
               "2:2 1:2 | 0:4~", sc, oct=2, vel=0.66)
    for p in lo.values():
        p.dyn([(0, 0.8), (6, 1.1), (10, 1.15), (16, 0.8)])
    return S


@track
def cap06_cine1():
    """Estampa A: Jerico en verde enfermo, el agua amarga. Un chelo solo que
    murmura en el grave sobre cuerdas graves y un manantial turbio. Frigio en SI.
    60 BPM, 4 compases = 16 s."""
    S = Score("cap06_cine1", 60, 4)
    sc = Scale(nm("B", 2), "phrygian")
    lo = sections(S, [("cb", (31, 50)), ("vla", (50, 64))], rev={"hall": -6})
    lo["cb"].n(0, 16, sc.p(0, 0), 0.5, voice=0)
    lo["vla"].n(1, 7, sc.p(1, 1), 0.36, cresc=0.4, voice=0).n(8, 8, sc.p(0, 1), 0.36, voice=0)
    wa = S.part("water", "water", rev={"hall": -8}, gain=-1)
    wa.n(0, 16, 0, 0.6, fade=2.0)
    vc = S.part("cello", "solo_vc", rev={"hall": -6, "temple": -12})
    vc.mel(0, "r:.5 0:1.5^ 1:2~ | 2:1 1:1 0:2 | 4:1.5 3:.5 1:2v | 0:4~", sc, oct=1, vel=0.66)
    pz = S.part("drip", "kanun", rev={"hall": -6}, gain=-7)
    for t, d in ((2.5, 7), (6.0, 9), (9.5, 8), (13.0, 11)):
        pz.n(t, 0.5, sc.p(d, 1), 0.4, lv=True)
    return S


@track
def cap06_cine2():
    """Estampa B: la sal en la fuente, "yo sane estas aguas". Pulso cojo de
    tambor de marco (3+3+2) y la voz de campanas y celesta, ya en DORICO.
    80 BPM, 4 compases = 12 s."""
    S = Score("cap06_cine2", 80, 4)
    sc = Scale(nm("B", 2), "dorian")
    fr = S.part("frame", "frame", rev={"hall": -9})
    fr.pat(0, 4, IQA["malfuf"], vel=0.65)
    st = sections(S, STR4, rev={"hall": -5}, gain=-2)
    chords_to(st, STR4, sc, 0, [c1, c4, c3, c4], vel=0.5)
    for p in st.values():
        p.dyn([(0, 0.8), (2, 1.1), (4, 0.85)])
    bl = S.part("bells", "bells", rev={"temple": -6})
    bl.mel(0, "0:1 2:1 4:2 | 3:1 4:1 6:2 | 7:2 6:1 4:1 | 4:4", sc, oct=2, vel=0.58)
    cel = S.part("celesta", "celesta", rev={"hall": -6})
    cel.arp(0, 4, [c1, c4, c3, c4], sc, [3, 4, 5, 6, 5, 4, 3, 4], step=0.5, oct=1, vel=0.5)
    sh = S.part("shaker", "shaker", rev={"room": -10})
    sh.pat(0, 4, "SsSsSsSs", vel=0.5)
    return S


# ============================================================================
# CAPITULO 7 - EL ACEITE DE LA VIUDA (SOL mayor)
# ============================================================================
@track
def cap07():
    """La viuda, el acreedor y la vasija que no se vacia hasta que no quedan
    vasijas (2 Reyes 4:1-7). Musica de camara: el kanun canta, pizzicato y
    riq; en B las cuerdas dicen el motivo en mayor. El aceite se oye en el
    ARPA, que va sumando notas seccion a seccion (dos, cuatro, ocho) como las
    vasijas que se llenan, y en la salida se queda en dos.
    96 BPM, 32 compases = 80 s."""
    S = Score("cap07", 96, 32)
    sc = Scale(nm("G", 2), "major")
    INTRO, A, B, A2, OUT = std_form()
    pI = [c1, c4, c1, c5]
    pA = [c1, c4, c1, c5, c6, c4, c5, c1]
    pB = [c6, c4, c1, c5, c6, c4, c5, c1]
    allp = pI + pA + pB + pA + pI
    pz = S.part("pizz", "pizz", rev={"hall": -8}, pan=0.3)
    for k, ch in enumerate(allp):
        for j in range(4):
            pz.n(S.beat(k) + j, 1, sc.p(ch[0] if j % 2 == 0 else ch[2], 0), 0.6 if j == 0 else 0.5)
    pz2 = S.part("pizz_hi", "pizz", rev={"hall": -8}, pan=-0.3, gain=-3)
    for k, ch in enumerate(pA):
        for j in (1, 3):
            for i in (1, 2):
                pz2.n(S.beat(A2 + k) + j + 0.5, 0.5, sc.p(ch[i], 1), 0.48)
    hp = S.part("harp", "harp", rev={"hall": -8})
    hp.arp(INTRO, 4, pI, sc, [0, None, None, None, 2, None, None, None], step=0.5, oct=1, vel=0.5)
    hp.arp(A, 8, pA, sc, [0, None, None, None, 2, None, None, None], step=0.5, oct=1, vel=0.5)
    hp.arp(B, 8, pB, sc, [0, None, 2, None, 3, None, 4, None], step=0.5, oct=1, vel=0.5)
    hp.arp(A2, 8, pA, sc, [0, 1, 2, 3, 4, 3, 2, 1], step=0.5, oct=1, vel=0.52)
    hp.arp(OUT, 4, pI, sc, [0, None, None, None, 2, None, None, None], step=0.5, oct=1, vel=0.48)

    melA = ("4:.5 4:.5 2:.5 4:.5 7:1 4:1 | 5:.5 5:.5 3:.5 5:.5 7:2 | 4:.5 2:.5 0:.5 2:.5 4:1 2:1 | "
            "1:1 3:.5 1:.5 -1:2 | 2:.5 4:.5 5:1 4:.5 2:.5 1:1 | 0:1 2:.5 3:.5 5:2 | "
            "4:1 1:.5 3:.5 4:1 6:1 | 7:2 4:1 0:1")
    ka = S.part("kanun", "kanun", rev={"hall": -8}, gain=1.5)
    ka.mel(A, melA, sc, oct=2, vel=0.66)
    ka.mel(A2, melA, sc, oct=2, vel=0.74)
    ka.mel(INTRO + 2, "4:.5 4:.5 2:.5 4:.5 7:2 | 5:1 3:1 4:2", sc, oct=2, vel=0.55)
    ka.mel(OUT, "4:.5 4:.5 2:.5 4:.5 7:1 4:1 | 5:1 3:1 4:2 | r:4 | r:4", sc, oct=2, vel=0.55)
    st = sections(S, [("vla", (50, 67)), ("vln2", (57, 74))], rev={"hall": -5})
    chords_to(st, [("vla", (50, 67)), ("vln2", (57, 74))], sc, B, pB, vel=0.44)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(B, MOTIF + " | " + ANSWER, sc, oct=2, vel=0.64)
    va = S.part("vla_ctr", "vla", rev={"hall": -6}, gain=-2)
    va.mel(A2, "2:4 | 3:2 2:2 | 2:4 | 1:4 | 2:4 | 3:4 | 3:2 1:2 | 2:4", sc, oct=1, vel=0.46)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-2)
    for k, ch in enumerate(pB):
        cb.n(S.beat(B + k), 4, sc.p(ch[0], 0), 0.45, voice=0)
    rq = S.part("riq", "riq", rev={"room": -10, "hall": -16})
    rq.pat(A, 8, "D.T.S.T.", vel=0.5)
    rq.pat(B, 8, "D...S...", vel=0.42)
    rq.pat(A2, 8, "DtTtStTt", vel=0.55)
    rq.pat(INTRO, 4, "D...S...", vel=0.4)
    rq.pat(OUT, 4, "D...S...", vel=0.4)
    fr = S.part("frame", "frame", rev={"hall": -12}, gain=-3)
    fr.pat(A2, 8, "D...D.T.", vel=0.6)
    return S


@track
def cap07_intro():
    """"Tu sierva ninguna cosa tiene en casa, sino una vasija de aceite". Una
    viola sola, mas grave y mas cansada que el kanun del tema, sobre la lira.
    Sin percusion. 80 BPM, 16 compases = 48 s."""
    S = Score("cap07_intro", 80, 16)
    sc = Scale(nm("G", 2), "major")
    prog = [c6, c4, c1, c5, c6, c4, c2, c5, c6, c4, c1, c5, c4, c5, c6, c6]
    ly = S.part("lyre", "lyre", rev={"hall": -8})
    ly.arp(0, 16, prog, sc, [0, 2, 4, 2, 3, 2, 4, 2], step=0.5, oct=0, vel=0.48)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-1)
    vc = S.part("vc", "vc", rev={"hall": -6}, gain=-5)
    for k, ch in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(ch[0], 0), 0.44, voice=0)
    chords_to({"vc": vc}, [("vc", (45, 57))], sc, 0, prog, vel=0.38)
    va = S.part("viola", "solo_vla", rev={"hall": -7, "temple": -12})
    va.mel(2, "5:2 4:1 2:1 | 1:3 -1:1 | 0:2 2:1 4:1 | 4:4~ | 5:1.5 6:.5 7:2 | 6:2 4:2 | "
              "5:1.5 4:.5 2:2 | 1:4~ | 3:2 2:1 0:1 | 1:2 2:2 | 2:4~ | r:4 | 5:4~", sc, oct=1,
           vel=0.64)
    for p in (cb, vc):
        p.dyn([(0, 0.8), (6, 1.1), (11, 1.15), (16, 0.8)])
    return S


@track
def cap07_cine1():
    """Estampa A: alba gris sobre la aldea pobre; los acreedores a la puerta
    (el pizzicato que llama). Un ney grave y solo. 60 BPM, 16 s."""
    S = Score("cap07_cine1", 60, 4)
    sc = Scale(nm("G", 2), "major")
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"hall": -6})
    lo["cb"].n(0, 16, sc.p(5, -1), 0.48, voice=0)
    lo["vc"].n(0.5, 7.5, sc.p(2, 0), 0.38, voice=0).n(8, 8, sc.p(1, 0), 0.38, voice=0)
    kn = S.part("knock", "pizz", rev={"hall": -7}, pan=0.25)
    for t in (2.0, 2.5, 3.0, 8.0, 8.5, 9.0, 14.0, 14.5):
        kn.n(t, 0.5, sc.p(5, 0), 0.6)
    ney = S.part("ney", "ney", rev={"temple": -6, "hall": -12})
    ney.mel(0, "r:.5 5:1.5^ 4:2~ | 2:1 4:1 1:2v | 2:1.5 1:.5 -2:2~ | r:4", sc, oct=2, vel=0.62)
    return S


@track
def cap07_cine2():
    """Estampa B: las vasijas prestadas; el aceite no para hasta que se acaban.
    Palmas y darbuka; el oud corre en semicorcheas que suben, vasija tras
    vasija. 80 BPM, 4 compases = 12 s."""
    S = Score("cap07_cine2", 80, 4)
    sc = Scale(nm("G", 2), "major")
    cl = S.part("claps", "clap", rev={"room": -6, "hall": -14})
    cl.pat(1, 3, "..X...X...X...X.", step=0.25, vel=0.62)
    cl.dyn([(1, 0.7), (3, 1.15), (4, 0.9)])
    db = S.part("darbuka", "darbuka", rev={"room": -9, "hall": -15})
    db.pat(0, 4, "D.tkT.tkD.tkT.tk", step=0.25, vel=0.55)
    oud = S.part("oud", "oud", rev={"hall": -9})
    for k in range(4):
        run = " ".join(f"{d}:.25" for d in [0, 2, 4, 2, 1, 3, 5, 3, 2, 4, 6, 4, 3, 5, 7, 5])
        oud.mel(k, transpose_mel(run, k), sc, oct=1, vel=0.6)
    st = sections(S, STR4, rev={"hall": -5}, gain=-3)
    chords_to(st, STR4, sc, 0, [c1, c4, c5, c1], vel=0.48)
    for p in list(st.values()) + [oud, db]:
        p.dyn([(0, 0.7), (2.5, 1.15), (4, 0.8)])
    return S


# ============================================================================
# CAPITULO 8 - LA SUNAMITA (menor melodico en RE -> RE mayor)
# ============================================================================
@track
def cap08():
    """La mujer que hospeda al profeta, el hijo que le nace, el hijo que se le
    muere en el regazo y Eliseo tendido sobre el nino hasta que estornuda
    siete veces (2 Reyes 4:8-37). El LATIDO es el tema: un pulso grave que en B
    se ralentiza y se para; vuelve en A', ya en MAYOR, con siete notas de
    celesta (los siete estornudos). Chelo solista. 76 BPM, 32 c = 101 s."""
    S = Score("cap08", 76, 32)
    sm = Scale(nm("D", 2), "melminor")
    sM = Scale(nm("D", 2), "major")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c4, c1, c5, c1, c4, c5, c1]
    pB = [c1, c2, c5, c1, c4, c2, c5, c5]
    st = sections(S, STR5, rev={"hall": -6})
    chords_to(st, STR5[:3], sm, INTRO, [c1, c4, c1, c5], vel=0.42)
    chords_to(st, STR5[:3], sm, A, pA, vel=0.46)
    chords_to(st, STR4, sm, B, pB, vel=0.5)
    chords_to(st, STR5, sM, A2, pA, vel=0.58)
    chords_to(st, STR5[:3], sm, OUT, [c1, c4, c1, c5], vel=0.42)
    melA = ("4:1.5 2:.5 0:1 2:1 | 3:1.5 5:.5 4:2 | 2:1 4:1 7:1 6:.5 5:.5 | 4:4~ | "
            "7:1.5 6:.5 4:1 2:1 | 3:1 5:1 4:1 3:1 | 1:1.5 2:.5 1:1 -1:1 | 0:4~")
    vc = S.part("cello", "solo_vc", rev={"hall": -6, "temple": -12}, gain=1)
    vc.mel(A, melA, sm, oct=2, vel=0.7)
    vc.mel(B, "7:3 6:1 | 5:2 4:2 | 4:1 3:1 1:2 | 0:4~ | 3:2 4:1 5:1 | 4:2 1:2 | 1:3 -1:1 | "
              "-1:4~", sm, oct=2, vel=0.64)
    vc.mel(A2, melA, sM, oct=2, vel=0.76)
    vc.mel(OUT, "4:1.5 2:.5 0:1 2:1 | 1:2 0:2~ | r:4 | r:4", sm, oct=2, vel=0.58)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5}, gain=-1)
    vm.mel(A2, melA, sM, oct=3, vel=0.6)
    hp = S.part("harp", "harp", rev={"hall": -8}, gain=-1)
    hp.arp(A, 8, pA, sm, [0, 1, 2, 3, 2, 1, 2, 1], step=0.5, oct=1, vel=0.45)
    hp.arp(A2, 8, pA, sM, [0, 1, 2, 3, 4, 3, 2, 1], step=0.5, oct=1, vel=0.5)
    # el latido: cada pulso en A; en B se espacia y se para; vuelve en A'
    hb = S.part("heart", "frame", rev={"hall": -10}, gain=1)
    hb.pat(INTRO, 4, "D.d.....D.d.....", step=0.25, vel=0.5)
    hb.pat(A, 8, "D.d.D.d.D.d.D.d.", step=0.25, vel=0.55)
    hb.pat(B, 2, "D.d.....D.d.....", step=0.25, vel=0.5)
    hb.pat(B + 2, 2, "D.d.............", step=0.25, vel=0.45)
    hb.pat(A2, 8, "D.d.D.d.D.d.D.d.", step=0.25, vel=0.6)
    hb.pat(OUT, 4, "D.d.....D.d.....", step=0.25, vel=0.5)
    ch = sections(S, CHOIR4[1:3], rev={"hall": -3, "temple": -9}, far=0.4, vowel=("u", "o"))
    chords_to(ch, CHOIR4[1:3], sm, B + 4, pB[4:], vel=0.44)
    cel = S.part("celesta", "celesta", rev={"hall": -6})
    for i in range(7):
        cel.n(S.beat(A2) + i * 0.5, 0.5, sM.p(i, 2), 0.5)
    cel.arp(B, 4, pB[:4], sm, [4, None, None, 3, None, None, 2, None], step=0.5, oct=1, vel=0.4)
    return S


@track
def cap08_intro():
    """"Hagamos un pequeno aposento de paredes". La lira canta sola sobre un
    coro en 'u' muy suave, sin percusion. 80 BPM, 16 compases = 48 s."""
    S = Score("cap08_intro", 80, 16)
    sc = Scale(nm("D", 2), "melminor")
    prog = [c1, c4, c1, c5, c1, c2, c5, c1, c4, c1, c2, c5, c1, c4, c5, c1]
    ch = sections(S, CHOIR4[:3], rev={"hall": -3, "temple": -9}, far=0.35, vowel=("u", "o"))
    chords_to(ch, CHOIR4[:3], sc, 0, prog, vel=0.42)
    for p in ch.values():
        p.dyn([(0, 0.85), (8, 1.15), (16, 0.85)])
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-2)
    for k, c in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(c[0], 0), 0.42, voice=0)
    ly = S.part("lyre", "lyre", rev={"hall": -7}, gain=2)
    ly.mel(1, "4:1 2:.5 4:.5 7:2 | 6:1 5:.5 4:.5 5:2 | 4:1 2:1 0:2 | 1:1 2:.5 1:.5 -1:2 | "
              "0:1 2:1 4:1 7:1 | 6:1.5 5:.5 4:2 | 3:1 5:1 4:1 2:1 | 0:4 | 3:1 4:.5 5:.5 6:2 | "
              "7:1 6:.5 5:.5 4:2 | 4:1 3:.5 2:.5 1:2 | 1:2 2:1 4:1 | 4:1.5 2:.5 0:2 | "
              "1:1 2:1 3:1 4:1 | 0:4", sc, oct=2, vel=0.62)
    ly.arp(0, 16, prog, sc, [0, None, 2, None, 0, None, 2, None], step=0.5, oct=1, vel=0.38)
    return S


@track
def cap08_cine1():
    """Estampa A: el aposento a la luz del candil, el nino en la cama de Eliseo.
    Una VOZ sola (vocalise en 'u'/'o') sobre cuerdas graves. 60 BPM, 16 s."""
    S = Score("cap08_cine1", 60, 4)
    sc = Scale(nm("D", 2), "melminor")
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60)), ("vla", (50, 64))],
                  rev={"hall": -5})
    chords_to(lo, [("cb", (31, 50)), ("vc", (40, 60)), ("vla", (50, 64))], sc, 0,
              [c1, c4, c2, c5], vel=0.42)
    vo = S.part("voice", "voice", rev={"hall": -5, "temple": -9}, vowel=("u", "o"), breath=0.1)
    vo.mel(0, "r:.5 4:1.5^ 5:.5 4:1.5 | 3:1 2:1 0:2~ | 2:1 3:.5 4:.5 5:1 4:1 | 1:4~", sc,
           oct=2, vel=0.62)
    cel = S.part("celesta", "celesta", rev={"hall": -5}, gain=-1)
    for t, d in ((0.25, 7), (4.25, 9), (8.25, 6), (12.25, 11)):
        cel.n(t, 1, sc.p(d, 2), 0.42)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=2)
    for t, d in ((2.0, 4), (4.0, 9), (6.0, 7), (8.0, 11), (10.0, 4), (12.0, 7), (14.0, 9)):
        cr.n(t, 2, sc.p(d, 3), 0.42)
    hh = S.part("harmonics", "vln1", rev={"hall": -5}, gain=-4, art="trem")
    hh.n(0.5, 15, sc.p(4, 4), 0.4, voice=0)
    nd = S.part("night", "wind", rev={"hall": -10}, gain=-5)   # el aire de la noche fuera
    nd.n(0, 16, 0, 0.5, fade=2.0)
    cel2 = S.part("celesta_hi", "celesta", rev={"hall": -5}, gain=-3)
    for t, d in ((1.5, 7), (5.5, 9), (9.5, 11), (13.5, 7)):
        cel2.n(t, 1, sc.p(d, 3), 0.4)
    return S


@track
def cap08_cine2():
    """Estampa B: "y el nino estornudo siete veces". Vuelve el LATIDO, el
    kanun sube y siete crotalos cuentan; las cuerdas crecen. 80 BPM, 12 s."""
    S = Score("cap08_cine2", 80, 4)
    sc = Scale(nm("D", 2), "major")
    hb = S.part("heart", "frame", rev={"hall": -9}, gain=1)
    hb.pat(0, 4, "D.d.....D.d.....", step=0.25, vel=0.6)
    ka = S.part("kanun", "kanun", rev={"hall": -7})
    ka.mel(0, "0:.5 2:.5 4:.5 7:.5 4:1 2:1 | 1:.5 3:.5 5:.5 8:.5 5:2 | "
              "4:.5 5:.5 7:.5 9:.5 7:1 4:1 | 7:4", sc, oct=1, vel=0.62)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=-2)
    for i in range(7):
        cr.n(4.0 + i * 1.0, 1, sc.p(i, 3), 0.45)
    st = sections(S, STR4, rev={"hall": -5}, gain=-3)
    chords_to(st, STR4, sc, 0, [c1, c5, c4, c1], vel=0.5)
    for p in st.values():
        p.dyn([(0, 0.75), (3, 1.15), (4, 0.8)])
    return S


# ============================================================================
# CAPITULO 9 - EL JORDAN (mixolidio en LA)
# ============================================================================
@track
def cap09():
    """Naaman, general de Siria y leproso, viene con caballos y carros y se va
    limpio tras zambullirse SIETE veces en un rio que le parecia poca cosa
    (2 Reyes 5). Mixolidio: mayor pero con la 7a baja, noble y sin cerrar. El
    galope de daf y marco y el ostinato grave son el orgullo; en B las siete
    zambullidas son siete crecidas de cuerda sobre el agua; A' es el agua
    limpia. Ney solista. 104 BPM, 32 compases = 73,8 s."""
    S = Score("cap09", 104, 32)
    sc = Scale(nm("A", 2), "mixolydian")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c7, c1, c4, c1, c7, c5, c1]
    pB = [c1, c1, c6, c4, c1, c7, c5, c1]
    os_ = S.part("ostinato", "vc", rev={"hall": -8}, art="spic", gain=1)
    cbo = S.part("ostinato_cb", "cb", rev={"hall": -8}, art="stac")
    allp = [c1] * 4 + pA + pB + pA + [c1] * 4
    for k, ch in enumerate(allp):
        if B <= k < A2:
            continue
        for j in range(8):
            os_.n(S.beat(k) + j * 0.5, 0.5, sc.p(ch[0] + [0, 0, 4, 0, 0, 4, 2, 4][j], 1),
                  0.58 if j in (0, 3, 6) else 0.46)
        cbo.n(S.beat(k), 1.5, sc.p(ch[0], 0), 0.6)
        cbo.n(S.beat(k) + 1.5, 1.0, sc.p(ch[0], 0), 0.5)
    os_.dyn([(0, 0.7), (4, 0.9), (20, 1.05), (28, 0.75), (32, 0.7)])
    st = sections(S, STR4, rev={"hall": -6})
    chords_to(st, STR4[1:], sc, A, pA, vel=0.44)
    chords_to(st, STR4, sc, A2, pA, vel=0.56)
    # las siete zambullidas: siete crecidas de cuerda (B)
    sw = sections(S, STR5, prefix="dip_", rev={"hall": -4})
    for i in range(7):
        b0 = B + i * 8.0 / 7.0
        ch = pB[min(7, int(i * 8 / 7))]
        chords_to(sw, STR5, sc, b0, [ch], bpc=8.0 / 7.0, vel=0.55)
    for p in sw.values():
        for idx in range(len(p.notes)):
            bb, dd, pp, vv, ff = p.notes[idx]
            p.notes[idx] = (bb, dd * 0.95, pp, vv, dict(ff, swell=1.0))
        p.prm["line"] = {"swell": 0.85, "swell_min": 0.5, "attack": 0.4, "release": 0.5}
    melA = ("0:1 2:.5 4:.5 7:2 | 6:1.5 5:.5 3:2 | 4:1 2:1 4:1 6:1 | 5:2 3:1 5:1 | "
            "7:1.5 6:.5 4:1 2:1 | 1:1 3:1 6:2 | 4:1 6:.5 5:.5 4:1 1:1 | 2:1 1:.5 2:.5 0:2")
    ney = S.part("ney", "ney", rev={"temple": -7, "hall": -12}, gain=1)
    ney.mel(A, melA, sc, oct=2, vel=0.7)
    ney.mel(B, "0:2^ 1:1 2:1 | 4:4~ | r:4 | r:4 | 0:2^ 1:1 2:1 | 4:2 3:2 | 4:4~ | r:4", sc,
            oct=2, vel=0.64)
    ney.mel(A2, melA, sc, oct=2, vel=0.76)
    ney.mel(OUT, "0:1 2:.5 4:.5 7:2~ | 6:1.5 5:.5 4:2 | r:4 | r:4", sc, oct=2, vel=0.6)
    cel = S.part("celesta", "celesta", rev={"hall": -6})
    for i in range(7):
        cel.n(S.beat(B) + i * 32.0 / 7.0 + 0.25, 1, sc.p(7 - i, 2), 0.5)
    cel.arp(A2, 8, pA, sc, [4, None, 3, None, 5, None, 4, 3], step=0.5, oct=1, vel=0.4)
    wa = S.part("water", "water", rev={"hall": -10}, gain=-5)
    wa.n(S.beat(B), 32, 0, 0.6, fade=2.0)
    # galope: daf y marco
    df = S.part("daf", "daf", rev={"hall": -11})
    df.pat(INTRO, 4, "D.TD.T..", vel=0.5)
    df.pat(A, 8, "D.TD.TD.", vel=0.62)
    df.pat(A2, 7, "D.TD.TDT", vel=0.7)
    df.pat(A2 + 7, 1, "DTTDTTDT", vel=0.75)
    df.pat(OUT, 4, "D.TD.T..", vel=0.5)
    fr = S.part("frame", "frame", rev={"hall": -12}, gain=-2)
    fr.pat(A, 8, "D...D...", vel=0.55)
    fr.pat(A2, 8, "D..TD.T.", vel=0.62)
    fr.pat(B, 8, "D.......", vel=0.4)
    sh = S.part("shaker", "shaker", rev={"room": -12})
    sh.pat(B, 8, "S.s.S.s.", vel=0.4)
    return S


@track
def cap09_intro():
    """"No son Abana y Farfar, rios de Damasco, mejores que todas las aguas de
    Israel?". El orgullo antes de doblarse: un duduk sobre trompas muy suaves y
    cuerdas graves, sin percusion. 80 BPM, 16 compases = 48 s."""
    S = Score("cap09_intro", 80, 16)
    sc = Scale(nm("A", 2), "mixolydian")
    prog = [c1, c7, c4, c1, c1, c7, c5, c1, c6, c4, c7, c1, c4, c7, c1, c1]
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.3, gain=-2)
    chords_to({"horns": hn}, [("horns", (52, 64))], sc, 0, prog, vel=0.42)
    hn2 = S.part("horns2", "horns", rev={"hall": -4}, far=0.3, gain=-4)
    chords_to({"horns": hn2}, [("horns", (57, 69))], sc, 0, prog, vel=0.38)
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"hall": -6})
    chords_to(lo, [("cb", (31, 50)), ("vc", (40, 60))], sc, 0, prog, vel=0.42)
    for p in (hn, hn2, lo["vc"]):
        p.dyn([(0, 0.8), (7, 1.15), (12, 1.0), (16, 0.8)])
    dud = S.part("duduk", "duduk", rev={"temple": -7, "hall": -13})
    dud.mel(2, "4:2^ 2:1 4:1 | 7:3 6:1 | 5:2 3:2 | 4:4~ | 2:1.5 3:.5 4:2 | 6:2 5:1 4:1 | "
               "3:1 1:1 6,:2 | 0:4~ | 2:1 4:1 5:2 | 5:1 4:1 3:2 | 1:2 3:2 | 4:4~", sc,
            oct=1, vel=0.68)
    pz = S.part("pizz", "pizz", rev={"hall": -8}, pan=0.3, gain=-2)
    for k, c in enumerate(prog):
        pz.n(S.beat(k), 1, sc.p(c[0], 1), 0.5).n(S.beat(k) + 2, 1, sc.p(c[2], 1), 0.42)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=1)
    for k, c in enumerate(prog):
        cr.n(S.beat(k) + 3, 1, sc.p(c[2], 3), 0.42 if k % 2 else 0.36)
    zl = S.part("zills", "zills", rev={"hall": -9}, far=0.4, gain=0)
    for k in range(0, 16, 2):
        zl.n(S.beat(k), 0.5, "X" if k % 4 == 0 else "x", 0.42)
    hp = S.part("harp", "harp", rev={"hall": -6}, gain=-3)
    hp.arp(0, 16, prog, sc, [6, None, None, 7, None, None, 8, None], step=0.5, oct=1, vel=0.4)
    wa = S.part("jordan", "water", rev={"hall": -10}, gain=-4)   # el Jordan, al lado
    wa.n(0, 64, 0, 0.5, fade=3.0)
    return S


@track
def cap09_cine1():
    """Estampa A: dia claro sobre el Jordan, el carro del general parado en la
    orilla. Luz abierta: el arpa canta sobre coro y cuerdas. 60 BPM, 16 s."""
    S = Score("cap09_cine1", 60, 4)
    sc = Scale(nm("A", 2), "mixolydian")
    ch = sections(S, CHOIR4, rev={"hall": -2, "temple": -8}, far=0.35, vowel=("a", "o"))
    chords_to(ch, CHOIR4, sc, 0, [c1, c7, c4, c1], vel=0.44)
    hp = S.part("harp", "harp", rev={"hall": -6}, gain=2)
    hp.mel(0, "4:.5 7:.5 9:1 7:.5 6:.5 4:1 | 6:.5 8:.5 10:1 8:2 | 7:.5 9:.5 11:1 10:.5 9:.5 7:1 | "
              "7:4", sc, oct=1, vel=0.62, lv=True)
    hp.arp(0, 4, [c1, c7, c4, c1], sc, [0, None, 2, None, 3, None, None, None], step=0.5, oct=0,
           vel=0.45)
    wa = S.part("water", "water", rev={"hall": -9}, gain=-4)
    wa.n(0, 16, 0, 0.55, fade=2.0)
    return S


@track
def cap09_cine2():
    """Estampa B: las siete zambullidas, "y su carne se volvio como la de un
    nino". Agua, shaker y un tambor de marco que cuenta; el kaval sube, limpio.
    80 BPM, 4 compases = 12 s."""
    S = Score("cap09_cine2", 80, 4)
    sc = Scale(nm("A", 2), "mixolydian")
    wa = S.part("water", "water", rev={"hall": -8}, gain=-1)
    wa.n(0, 16, 0, 0.6, fade=1.5)
    sh = S.part("shaker", "shaker", rev={"room": -10})
    sh.pat(0, 4, "SsSsSsSs", vel=0.55)
    fr = S.part("frame", "frame", rev={"hall": -9})
    fr.pat(0, 4, "D...T...", vel=0.62)
    kv = S.part("kaval", "kaval", rev={"temple": -7, "hall": -12})
    kv.mel(0, "0:.5 1:.5 2:1 4:2 | 2:.5 3:.5 4:1 6:2 | 4:.5 5:.5 6:1 7:1 9:1 | 7:4~", sc,
           oct=2, vel=0.66)
    st = sections(S, STR4, rev={"hall": -5}, gain=-3)
    chords_to(st, STR4, sc, 0, [c1, c7, c4, c1], vel=0.48)
    for p in list(st.values()) + [sh, fr]:
        p.dyn([(0, 0.65), (2.5, 1.2), (4, 0.75)])
    return S


# ============================================================================
# CAPITULO 10 - SAMARIA SITIADA (frigio en MI -> MI mayor)
# ============================================================================
@track
def cap10():
    """El hambre del cerco, los cuatro leprosos a la puerta y el ejercito sirio
    que huye de noche porque OYE estruendo de carros y caballos (2 Reyes 6-7).
    El final del juego y el unico que va de la opresion a la liberacion:
    marcha en FRIGIO mientras dura el cerco (trompas sobre taikos y ostinato),
    los carros en B (el coro y el tambor que acelera), y en A' el motivo en MI
    MAYOR, trompas, coro y violines a la vez. 112 BPM, 32 c = 68,6 s."""
    S = Score("cap10", 112, 32)
    sp = Scale(nm("E", 2), "phrygian")
    sM = Scale(nm("E", 2), "major")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c2, c1, c2, c4, c2, c7, c1]
    pB = [c1, c7, c6, c2, c1, c7, c6, c2]
    pA2 = [c1, c4, c5, c1, c6, c4, c5, c1]
    os_ = S.part("ostinato", "vc", rev={"hall": -8}, art="spic", gain=2)
    cbo = S.part("ostinato_cb", "cb", rev={"hall": -8}, art="stac", gain=1)
    allp = [(c, sp) for c in [c1, c2, c1, c1] + pA + pB] + [(c, sM) for c in pA2] + \
        [(c, sp) for c in [c1, c2, c1, c1]]
    for k, (ch, s_) in enumerate(allp):
        dense = B <= k < OUT
        n_ = 8 if dense else 4
        for j in range(n_):
            st_ = 4.0 / n_
            os_.n(S.beat(k) + j * st_, st_, s_.p(ch[0] + ([0, 0, 1, 0, 2, 0, 1, 0][j] if dense else 0), 1),
                  0.6 if j % 4 == 0 else 0.5)
        cbo.n(S.beat(k), 1.0, s_.p(ch[0], 0), 0.62)
        cbo.n(S.beat(k) + 2, 1.0, s_.p(ch[0], 0), 0.55)
    os_.dyn([(0, 0.6), (4, 0.8), (12, 0.95), (20, 1.05), (28, 0.65), (32, 0.6)])
    st = sections(S, STR5, rev={"hall": -6}, gain=-1)
    chords_to(st, STR4[2:], sp, A, pA, vel=0.46)
    chords_to(st, STR4[1:], sp, B, pB, vel=0.52)
    chords_to(st, STR5, sM, A2, pA2, vel=0.6)

    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.2, gain=1)
    hn.mel(A, "0:1> 0:.5 0:.5 2:1 4:1 | 5:1.5 4:.5 3:2 | 4:1> 4:.5 4:.5 5:1 4:1 | 3:1 2:1 1:2 | "
              "3:1> 3:.5 3:.5 5:1 7:1 | 6:1.5 5:.5 3:2 | 3:1 1:1 -1:1 1:1 | 0:4", sp, oct=2,
           vel=0.72)
    for k in range(8):
        hn.n(S.beat(B + k) + 1.5, 0.5, sp.p(pB[k][0], 2), 0.66, acc=True)
        hn.n(S.beat(B + k) + 3.5, 0.5, sp.p(pB[k][0], 2), 0.6, acc=True)
    hn.mel(A2, MOTIF + " | " + ANSWER, sM, oct=2, vel=0.86)
    hn.mel(OUT, "0:1> 0:.5 0:.5 2:1 4:1 | 5:1.5 4:.5 3:2 | r:4 | r:4", sp, oct=2, vel=0.62)
    lb = S.part("lowbrass", "lowbrass", rev={"hall": -5}, far=0.2, gain=-2)
    for k, ch in enumerate(pA2):
        lb.n(S.beat(A2 + k), 3.5, sM.p(ch[0], 1), 0.66)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(A2, MOTIF + " | " + ANSWER, sM, oct=3, vel=0.66)

    ch = sections(S, CHOIR4, rev={"hall": -3, "temple": -9}, far=0.3, vowel=("a", "o"))
    chant = "0:1> 0:1 1:1 0:1 | 0:1> -1:1 0:2 | "
    ch["choir_b"].mel(B, chant * 4, sp, oct=1, vel=0.55, sep=True)
    ch["choir_t"].mel(B, transpose_mel(chant, 2) * 4, sp, oct=1, vel=0.5, sep=True)
    chords_to(ch, CHOIR4, sM, A2, pA2, vel=0.6)

    tk = S.part("taiko", "taiko", rev={"hall": -7}, far=0.15)
    tk.pat(INTRO, 4, "D.......D.......", step=0.25, vel=0.55)
    tk.pat(A, 8, "D...d...D...d.d.", step=0.25, vel=0.68)
    tk.pat(B, 4, "D.d.D.d.D.d.D.d.", step=0.25, vel=0.72)
    tk.pat(B + 4, 3, "DddDddDdD.dDdd.d", step=0.25, vel=0.78)
    tk.pat(B + 7, 1, "DdDdDdDdDDDDDDDD", step=0.25, vel=0.85)
    tk.pat(A2, 7, "D..dD.d.D..dD.dd", step=0.25, vel=0.86)
    tk.pat(A2 + 7, 1, "D.d.D.d.DdDdDDDD", step=0.25, vel=0.85)
    tk.pat(OUT, 4, "D.......D.......", step=0.25, vel=0.55)
    tm = S.part("timp", "timp", rev={"hall": -7})
    tm.roll(S.beat(A2 - 1), 4, 0.7, stroke="D", cresc=0.95, pitch=82.4)
    for b in (A2, A2 + 4):
        tm.n(S.beat(b), 1, sM.p(0, 0), 0.8)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-2)
    gg.hit(S.beat(A2), "D", 0.8)
    gg.hit(S.beat(B), "D", 0.5)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=-3)
    for b in (A2, A2 + 2, A2 + 4, A2 + 6):
        cr.n(S.beat(b), 2, sM.p(4, 3), 0.45)
    return S


@track
def cap10_intro():
    """"Jehova, te ruego que abras sus ojos para que vea". Un oud que reza en
    frigio sobre cuerdas en tremolo y un pedal grave; sin percusion.
    80 BPM, 16 compases = 48 s."""
    S = Score("cap10_intro", 80, 16)
    sc = Scale(nm("E", 2), "phrygian")
    prog = [c1, c2, c1, c7, c1, c2, c6, c2, c4, c2, c7, c1, c6, c2, c1, c1]
    tr = {k: S.part(k, k, rev={"hall": -5}, art="trem", gain=-3) for k in ("vla", "vln2")}
    chords_to(tr, [("vla", (52, 64)), ("vln2", (59, 71))], sc, 0, prog, vel=0.42)
    cb = S.part("cb", "cb", rev={"hall": -6})
    drone(cb, sc.p(0, 0), 0, 16, vel=0.44, every=4)
    for p in list(tr.values()) + [cb]:
        p.dyn([(0, 0.8), (6, 1.1), (11, 1.2), (16, 0.8)])
    oud = S.part("oud", "oud", rev={"hall": -8}, gain=1)
    oud.mel(1, "0:.25 0:.25 0:.5 1:.5 2:.5 1:.5 0:.5 -1:1 | 0:2 r:2 | "
               "2:.5 3:.5 4:1 3:.5 2:.5 1:1 | 2:.25 2:.25 2:.25 2:.25 1:.5 0:.5 0:2 | "
               "4:.5 5:.5 6:.5 5:.5 4:1 2:1 | 3:.5 2:.5 1:.5 2:.5 0:2 | r:4 | "
               "7:.5 6:.5 5:.5 4:.5 5:1 4:1 | 3:.5 2:.5 1:.5 2:.5 3:1 2:1 | "
               "1:.25 1:.25 1:.25 1:.25 0:.5 -1:.5 0:2 | r:4 | 4:.5 5:.5 4:.5 2:.5 1:1 0:1 | "
               "-1:.5 0:.5 1:.5 2:.5 0:2 | r:4", sc, oct=1, vel=0.64)
    return S


@track
def cap10_cine1():
    """Estampa A: noche de asedio; los carros de fuego, todavia invisibles en el
    monte. La mas oscura de las diez: bordon grave, timbales lejanos como
    trueno y un ney grave y solo. Frigio en MI. 60 BPM, 16 s."""
    S = Score("cap10_cine1", 60, 4)
    sc = Scale(nm("E", 2), "phrygian")
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"hall": -6})
    lo["cb"].n(0, 16, sc.p(0, 0), 0.52, voice=0)
    lo["vc"].n(0, 8, sc.p(1, 1), 0.38, voice=0).n(8, 8, sc.p(0, 1), 0.38, voice=0)
    tm = S.part("timp", "timp", rev={"hall": -5}, far=0.5)
    tm.roll(1.0, 3.0, 0.45, stroke="D", cresc=0.8, pitch=82.4)
    tm.roll(9.0, 3.0, 0.42, stroke="D", cresc=-0.8, pitch=82.4)
    ney = S.part("ney", "ney", rev={"temple": -6, "hall": -12})
    ney.mel(0, "r:1 0:2^ 1:1~ | 0:2 -1:1 0:1 | 2:1.5 1:.5 0:2v | r:4", sc, oct=2, vel=0.6)
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=-1)
    wd.n(0, 16, 0, 0.55, fade=2.0)
    zl = S.part("zills", "zills", rev={"hall": -8}, far=0.6, gain=-2)
    for t in (3.0, 7.5, 11.0, 14.5):
        zl.n(t, 0.5, "x", 0.4)
    return S


@track
def cap10_cine2():
    """Estampa B: "mas son los que estan con nosotros que los que estan con
    ellos": el monte lleno de caballos y carros de fuego. Marcha de taikos que
    ACELERA (negras, corcheas, semicorcheas), cuerdas en tremolo que crecen y el
    shofar. 80 BPM, 4 compases = 12 s."""
    S = Score("cap10_cine2", 80, 4)
    sc = Scale(nm("E", 2), "phrygian")
    tk = S.part("taiko", "taiko", rev={"hall": -6})
    tk.pat(0, 1, "D...D...D...D...", step=0.25, vel=0.7)
    tk.pat(1, 1, "D.d.D.d.D.d.D.d.", step=0.25, vel=0.75)
    tk.pat(2, 1, "DdddDdddDdddDddd", step=0.25, vel=0.8)
    tk.pat(3, 1, "D.D.DdDdDDDDD...", step=0.25, vel=0.85)
    tr = sections(S, STR4, rev={"hall": -5}, art="trem", gain=-2)
    chords_to(tr, STR4, sc, 0, [c1, c2, c1, c2], vel=0.5)
    for p in tr.values():
        p.dyn([(0, 0.65), (3.5, 1.2), (4, 0.7)])
    sh = S.part("shofar", "shofar", rev={"temple": -6, "hall": -10})
    sh.mel(0, "0:3.5 r:.5 | 4:.5 4:.5 4:.5 4:.5 4:.5 4:.5 r:1 | 0:3.5 r:.5 | 4:2 7:2", sc,
           oct=2, vel=0.74)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-4)
    gg.hit(0, "D", 0.55)
    return S
