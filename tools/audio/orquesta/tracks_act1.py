"""Acto I - Elias (capitulos 1-5). Mas calido y mas aspero: duduk, oud,
trompas, ney, kaval; tambores de marco, taikos, fuego.

Cada capitulo tiene cuatro pistas y NINGUNA repite solista ni fondo de otra:

  cap   tema (jugando)              apertura (dialogo)       estampa A            estampa B
  01    duduk / cuerdas+tanpura+lira  chelo / arpa           kanun / coro+viento  ney / oud+darbuka+pizz
  02    oud / darbuka+riq+cuerdas     violin / lira          campanas / coro+mar  kaval / arpa+darbuka
  03    trompas / ostinato+taikos     duduk / tremolo+timbal coro grave / zils    shofar / taikos+escala
  04    ney / cueva+terremoto         oud (taqsim) / bordon  celesta / cb+viento  arpa / latido+armonicos
  05    kaval / cuerdas+arpa+marco    violin / lira+pizz     celesta / coro       trompas / marco+arpa

El modo y el centro tonal SI se comparten dentro del capitulo: es lo que hace
que las cuatro suenen del mismo sitio sin sonar a lo mismo.
"""

from arrange import (Score, Scale, nm, IQA, MOTIF, ANSWER, STR5, STR4, CHOIR4,
                     sections, chords_to, drone, tanpura, transpose_mel)

TRACKS = {}


def track(fn):
    TRACKS[fn.__name__] = fn
    return fn


# Acordes por grados (validos en cualquier modo: la calidad la pone el modo)
c1, c2, c3, c4, c5, c6, c7 = ([0, 2, 4], [1, 3, 5], [2, 4, 6], [3, 5, 0], [4, 6, 1],
                              [5, 0, 2], [6, 1, 3])


def std_form():
    return 0, 4, 12, 20, 28


# ============================================================================
# CAPITULO 1 - EL ARROYO DE QUERIT (dorico en RE)
# ============================================================================
@track
def cap01():
    """Soledad en el canon, el agua que se va secando y un solo duduk.
    80 BPM, 32 compases = 96 s: intro | A duduk | B chelos y lira | A' | salida."""
    S = Score("cap01", 80, 32)
    sc = Scale(nm("D", 3), "dorian")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c4, c7, c1, c3, c4, c5, c1]
    pB = [c3, c7, c4, c4, c3, c7, c5, c5]
    st = sections(S, STR5, rev={"hall": -6})
    drone(st["cb"], sc.p(0, -1), INTRO, 4, vel=0.42)
    drone(st["vc"], sc.p(4, -1), INTRO, 4, vel=0.32)
    chords_to(st, STR5[:3], sc, A, pA, vel=0.46)
    chords_to(st, STR5[:4], sc, B, pB, vel=0.5)
    chords_to(st, STR4, sc, A2, pA, vel=0.6)
    drone(st["cb"], sc.p(0, -1), OUT, 4, vel=0.42)
    drone(st["vc"], sc.p(4, -1), OUT, 4, vel=0.3)

    tp = S.part("tanpura", "tanpura", rev={"hall": -10}, gain=-3)
    tanpura(tp, sc, INTRO, 12, root_oct=-1, step=1.0, vel=0.45)
    tanpura(tp, sc, OUT, 4, root_oct=-1, step=1.0, vel=0.45)
    tanpura(tp, sc, A2, 8, root_oct=-1, step=1.0, vel=0.38)

    melA = ("0:1.5^ 1:.5 2:1 4:1g | 3:1 2:.5 1:.5 2:2~ | 4:1 3:.5 2:.5 1:1 2:1 | "
            "1:1 0:.5 -1:.5 0:2v | 4:1.5^ 5:.5 6:1 7:1 | 6:.75 5:.25 4:1 5:2~ | "
            "4:1 6:.5 5:.5 4:1m 2:1 | 1:1 2:.5 1:.5 0:2v")
    dud = S.part("duduk", "duduk", rev={"temple": -8, "hall": -14})
    dud.mel(A, melA, sc, oct=1, vel=0.72)
    dud.mel(A2, melA.replace("2:2~", "2:2t"), sc, oct=1, vel=0.8)
    dud.mel(OUT, "0:1.5^ 1:.5 2:1 4:1g | 3:1 2:.5 1:.5 0:2v | r:4 | r:4", sc, oct=1, vel=0.6)

    vcm = S.part("vc_mel", "vc", rev={"hall": -6}, gain=1.0)
    vcm.mel(B, "2:2 4:1 5:1 | 6:2 5:1 4:1 | 5:3 3:1 | 3:4 | 2:2 4:1 5:1 | 6:1 7:1 8:1 7:1 | "
               "6:2 5:1 4:1 | 4:4", sc, oct=0, vel=0.62)
    vct = S.part("vln_ctr", "vln1", rev={"hall": -5})
    vct.mel(A2, "7:4 | 7:2 8:2 | 9:4 | 8:2 7:2 | 9:4 | 10:4 | 9:2 8:2 | 7:4", sc, oct=1,
            vel=0.5)

    ly = S.part("lyre", "lyre", rev={"hall": -9})
    ly.arp(A, 8, pA, sc, [0, 2, 4, 2, 3, 2, 4, 2], step=0.5, oct=0, vel=0.5)
    ly.arp(B, 8, pB, sc, [0, 1, 2, 4, 2, 1, 0, None], step=0.5, oct=0, vel=0.45)
    ly.arp(A2, 8, pA, sc, [0, 2, 4, 2, 3, 2, 4, 5], step=0.5, oct=0, vel=0.55)
    ly.arp(INTRO + 2, 2, [c1], sc, [0, None, 2, None, 4, None, None, None], step=0.5,
           oct=0, vel=0.4)

    fr = S.part("frame", "frame", rev={"hall": -12})
    fr.pat(A, 8, "D..gT.g.", vel=0.58)
    fr.pat(A2, 7, IQA["maqsum"], vel=0.72)
    fr.pat(A2 + 7, 1, "DTkTDTTT", vel=0.78)
    db = S.part("darbuka", "darbuka", rev={"room": -10, "hall": -16}, gain=-2)
    db.pat(A2, 8, "D.tkT.tk..tkT.tk", step=0.25, vel=0.55)
    sh = S.part("shaker", "shaker", rev={"room": -12})
    sh.pat(A, 8, "S.s.S.s.", vel=0.4)
    sh.pat(A2, 8, "SsSsSsSs", vel=0.5)
    sh.pat(INTRO, 4, "S.......", vel=0.3)
    sh.pat(OUT, 4, "S.......", vel=0.3)

    wa = S.part("water", "water", rev={"hall": -10}, gain=-6)
    wa.n(S.beat(INTRO), 20, 0, 0.5, fade=2.0)
    wa.n(S.beat(OUT - 1), 20, 0, 0.5, fade=2.0)
    cr = S.part("crotales", "crotales", rev={"hall": -6}, gain=-2)
    for b, d in ((1, 4), (3, 2), (13, 7), (17, 4), (29, 4)):
        cr.n(S.beat(b) + 2, 2, sc.p(d, 2), 0.4)
    return S


@track
def cap01_intro():
    """Dialogo de apertura en Querit: un chelo solo sobre el arpa, sin pulso.
    60 BPM, 12 compases = 48 s. Melodia por saltos, la mitad de rapida que el
    tema, y nada de duduk ni de lira."""
    S = Score("cap01_intro", 60, 12)
    sc = Scale(nm("D", 3), "dorian")
    prog = [c1, c7, c3, c4, c1, c5, c4, c1, c3, c7, c1, c1]
    hp = S.part("harp", "harp", rev={"hall": -7})
    hp.arp(0, 12, prog, sc, [0, 2, 4, 7, 4, 2, 4, 2], step=0.5, oct=-1, vel=0.5)
    st = sections(S, [("vla", (50, 67)), ("vln2", (57, 74))], rev={"hall": -5}, gain=-4)
    chords_to(st, [("vla", (50, 67)), ("vln2", (57, 74))], sc, 0, prog, vel=0.35)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=0)
    for k, ch in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(ch[0], -1), 0.46, voice=0)
    vc = S.part("cello", "solo_vc", rev={"hall": -7, "temple": -12})
    vc.mel(2, "4:3 2:1 | 3:1.5 4:.5 5:2 | 4:2~ 2:1 1:1 | 1:3 -1:1 | 0:2 1:1 3:1 | 4:4~ | "
              "5:1.5 4:.5 2:2 | 1:2 -1:2 | 0:4~", sc, oct=1, vel=0.66)
    cel = S.part("celesta", "celesta", rev={"hall": -6}, gain=-4)
    for b, d in ((1, 4), (5, 2), (9, 4), (11, 7)):
        cel.n(S.beat(b) + 2, 2, sc.p(d, 2), 0.38)
    return S


@track
def cap01_cine1():
    """Estampa A: "Vive Jehova... no habra lluvia". Textura sin pulso: viento,
    coro lejano en 'u' y el kanun que gotea el motivo (RE MI FA LA) en agudo.
    60 BPM, 4 compases = 16 s; todo entra en los primeros 8 s."""
    S = Score("cap01_cine1", 60, 4)
    sc = Scale(nm("D", 3), "dorian")
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=0)
    wd.n(0, 16, 0, 0.7, fade=3.0)
    ch = sections(S, CHOIR4[1:3], rev={"hall": -2, "temple": -8}, far=0.4, vowel=("u", "o"))
    # crece hasta la mitad y vuelve: asi el final empalma con el principio
    ch["choir_t"].n(0.5, 7.5, sc.p(0, 0), 0.5, cresc=0.5, voice=0)
    ch["choir_t"].n(8.0, 7.5, sc.p(0, 0), 0.5, cresc=-0.5, voice=0)
    ch["choir_t"].n(0.5, 15, sc.p(4, 0), 0.4, voice=1)
    ch["choir_a"].n(1.0, 7.0, sc.p(2, 1), 0.48, cresc=0.6, voice=0)
    ch["choir_a"].n(8.0, 7.5, sc.p(1, 1), 0.46, cresc=-0.6, voice=0)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-2)
    cb.n(0, 16, sc.p(0, -1), 0.4, voice=0)
    ka = S.part("kanun", "kanun", rev={"hall": -6, "temple": -10})
    ka.mel(0, "r:1 0:1.5 1:.5 2:1 | 4:4 | r:1 3:.75 2:.25 1:1 2:1 | 0:4", sc, oct=2, vel=0.6,
           lv=True)
    ka.mel(0, "r:1.5 4:2.5 | r:2 7:2 | r:4 | r:2 4:2", sc, oct=1, vel=0.4, lv=True)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=-4)
    cr.n(0.25, 2, sc.p(4, 2), 0.4).n(9, 2, sc.p(0, 3), 0.35)
    return S


@track
def cap01_cine2():
    """Estampa B: los cuervos bajan por la franja de cielo y los soldados de
    Acab pasan de largo. Entra el PULSO: oud que sube grado a grado, darbuka y
    pizzicato; el ney son los graznidos (1 Reyes 17:6). 80 BPM, 4 compases = 12 s."""
    S = Score("cap01_cine2", 80, 4)
    sc = Scale(nm("D", 3), "dorian")
    oud = S.part("oud", "oud", rev={"hall": -10})
    oud.mel(0, "0:.5 0:.25 0:.25 1:.5 2:.5 0:.5 2:.5 3:.5 4:.5 | 2:.5 2:.25 2:.25 3:.5 4:.5 "
               "2:.5 4:.5 5:.5 6:.5 | 4:.5 4:.25 4:.25 5:.5 6:.5 7:.5 6:.5 5:.5 4:.5 | "
               "7:.5 6:.25 5:.25 4:.5 2:.5 0:1 0:1", sc, oct=0, vel=0.62)
    pz = S.part("pizz", "pizz", rev={"hall": -8}, pan=0.3)
    for k, ch in enumerate([c1, c3, c4, c1]):
        for j in range(4):
            pz.n(S.beat(k) + j, 0.5, sc.p(ch[0] if j % 2 == 0 else ch[2], -1), 0.55)
    db = S.part("darbuka", "darbuka", rev={"room": -10, "hall": -14})
    db.pat(0, 3, "D..kT.k.D.k.T.kk", step=0.25, vel=0.6)
    db.pat(3, 1, "D.kkT.kkTkTkTkTk", step=0.25, vel=0.7)
    ney = S.part("ney", "ney", rev={"temple": -6, "hall": -12})
    ney.mel(0, "r:2.5 9:.5^ 11:1v | r:1 9:.5^ 10:.5 8:2v | r:2 11:.5^ 12:1.5v | r:1 9:1^ 7:2~",
            sc, oct=1, vel=0.66)
    st = sections(S, [("vla", (50, 67)), ("vc", (40, 60))], rev={"hall": -6}, gain=-3)
    st["vc"].n(0, 16, sc.p(0, 0), 0.4, cresc=0.6, voice=0)
    st["vla"].n(8, 8, sc.p(4, 0), 0.4, cresc=0.7, voice=0)
    return S


# ============================================================================
# CAPITULO 2 - SAREPTA (eolico en LA)
# ============================================================================
@track
def cap02():
    """La casa de la viuda: hambre, un punado de harina, hospitalidad. Oud
    solista con darbuka y riq, calido y de puertas adentro. En B los violines
    cantan la cabeza del motivo (LA SI DO MI). 88 BPM, 32 compases = 87,3 s."""
    S = Score("cap02", 88, 32)
    sc = Scale(nm("A", 2), "aeolian")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c6, c7, c1, c4, c6, c5, c1]
    pB = [c3, c7, c4, c1, c6, c4, c5, c5]
    st = sections(S, STR5, rev={"hall": -6})
    chords_to(st, STR5[:3], sc, INTRO, [c1, c6, c7, c1], vel=0.4)
    chords_to(st, STR5[:3], sc, A, pA, vel=0.45)
    chords_to(st, STR4, sc, B, pB, vel=0.52)
    chords_to(st, STR4, sc, A2, pA, vel=0.58)
    chords_to(st, STR5[:3], sc, OUT, [c1, c6, c7, c1], vel=0.4)

    melA = ("4:1 2:.5 3:.5 4:1g 2:1 | 0:.5 1:.5 2:1 0:1.5 -1:.5 | 1:1 3:.5 1:.5 -1:1 1:1 | "
            "0:1 2:.5 1:.5 0:2 | 3:1 5:.5 4:.5 3:1m 0:1 | 2:1 0:.5 2:.5 5:1 4:1 | "
            "4:1.5 6:.5 4:1 1:1 | 2:1 1:.5 0:.5 0:.25 0:.25 0:.25 0:.25 0:1")
    oud = S.part("oud", "oud", rev={"hall": -9}, gain=1.0)
    oud.mel(A, melA, sc, oct=1, vel=0.7)
    oud.mel(A2, melA, sc, oct=1, vel=0.78)
    oud.mel(INTRO, "r:2 0:.25 0:.25 0:.5 4,:1 | 0:.5 1:.5 2:1 1:.5 0:.5 -1:1 | r:2 0:.25 0:.25 "
                   "0:.5 2:1 | 1:.5 0:.5 -1:.5 0:.5 0:2", sc, oct=1, vel=0.55)
    oud.mel(OUT, "0:.5 1:.5 2:1 1:.5 0:.5 -1:1 | 0:4 | r:2 0:.25 0:.25 0:.5 4,:1 | "
                 "0:.5 1:.5 2:.5 1:.5 0:.25 0:.25 0:.25 0:.25 -1:1", sc, oct=1, vel=0.6)
    # en B el oud acompana: rasgueo en corcheas sobre la fundamental
    for k, ch in enumerate(pB):
        for j in range(8):
            if j in (0, 3, 4, 6):
                oud.n(S.beat(B + k) + j * 0.5, 0.5, sc.p(ch[0], 1), 0.5 if j else 0.6)

    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(B, "0:1.5 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 3:2 | 5:1 4:.5 3:.5 0:2 | 2:1 1:1 0:2 | "
              "2:1.5 3:.5 5:1 7:1 | 5:1 3:1 5:1 7:1 | 6:1.5 5:.5 4:2 | 4:2 3:1 1:1", sc,
           oct=2, vel=0.62)
    vct = S.part("vla_ctr", "vla", rev={"hall": -6}, gain=-1)
    vct.mel(A2, "4:4 | 5:4 | 3:4 | 4:4 | 5:4 | 5:2 4:2 | 4:4 | 4:4", sc, oct=1, vel=0.48)

    hp = S.part("harp", "harp", rev={"hall": -9}, gain=-2)
    hp.arp(B, 8, pB, sc, [0, 2, 4, 7, 4, 2, 4, 2], step=0.5, oct=0, vel=0.45)
    hp.arp(A2, 8, pA, sc, [0, 2, 4, 2, 7, 4, 2, 4], step=0.5, oct=0, vel=0.42)

    db = S.part("darbuka", "darbuka", rev={"room": -9, "hall": -16})
    db.pat(INTRO, 4, "D.....k.T.......", step=0.25, vel=0.5)
    db.pat(A, 8, "D..kT.k.D.k.T.k.", step=0.25, vel=0.62)
    db.pat(B, 8, "D.......T.......", step=0.25, vel=0.45)
    db.pat(A2, 7, "D.tkT.tkD.tkT.tk", step=0.25, vel=0.68)
    db.pat(A2 + 7, 1, "D.tkTkTkTkTkTkTk", step=0.25, vel=0.72)
    db.pat(OUT, 3, "D.....k.T.......", step=0.25, vel=0.5)
    db.pat(OUT + 3, 1, "D..kT.k.D.k.T.kk", step=0.25, vel=0.55)
    rq = S.part("riq", "riq", rev={"room": -10, "hall": -16}, gain=-3)
    rq.pat(A, 8, "D.T.S.T.", vel=0.5)
    rq.pat(A2, 8, "DtTtStTt", vel=0.55)
    fr = S.part("frame", "frame", rev={"hall": -12}, gain=-3)
    fr.pat(A2, 8, "D...D...", vel=0.6)
    return S


@track
def cap02_intro():
    """Apertura en Sarepta: un violin que reza sobre la lira, sin percusion.
    72 BPM, 16 compases = 53,3 s."""
    S = Score("cap02_intro", 72, 16)
    sc = Scale(nm("A", 2), "aeolian")
    prog = [c1, c6, c3, c7, c1, c4, c6, c5, c1, c6, c3, c7, c4, c5, c1, c1]
    ly = S.part("lyre", "lyre", rev={"hall": -8})
    ly.arp(0, 16, prog, sc, [0, 2, 4, 2, 7, 4, 2, 4], step=0.5, oct=0, vel=0.5)
    st = sections(S, [("vc", (40, 57)), ("vla", (50, 64))], rev={"hall": -5}, gain=-4)
    chords_to(st, [("vc", (40, 57)), ("vla", (50, 64))], sc, 0, prog, vel=0.38)
    vn = S.part("violin", "solo_vln", rev={"hall": -7, "temple": -12})
    vn.mel(2, "2:3 4:1 | 3:2 1:2 | 2:1.5 1:.5 0:2~ | 3:2 5:1 7:1 | 7:2 6:1 5:1 | 4:4~ | "
              "4:1.5 5:.5 6:1 7:1 | 5:1.5 4:.5 2:2 | 4:2 6:1 4:1 | 3:1 1:1 -1:2 | "
              "0:2 1:1 3:1 | 1:4~ | 0:4~", sc, oct=2, vel=0.64)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-1)
    for k, ch in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(ch[0], 0), 0.44, voice=0)
    return S


@track
def cap02_cine1():
    """Estampa A: atardecer violeta sobre el mar fenicio; la viuda recoge dos
    lenos. Mar, coro en 'a' y campanas que dan el motivo. 60 BPM, 4 compases."""
    S = Score("cap02_cine1", 60, 4)
    sc = Scale(nm("A", 2), "aeolian")
    sea = S.part("sea", "sea", rev={"hall": -10}, gain=2)
    sea.n(0, 16, 0, 0.75, fade=2.0, period=7.0)
    ch = sections(S, CHOIR4[1:], rev={"hall": -2, "temple": -9}, far=0.4, vowel=("a", "o"))
    chords_to(ch, CHOIR4[1:], sc, 0, [c1, c6], bpc=2, vel=0.45)
    bl = S.part("bells", "bells", rev={"temple": -5}, gain=1)
    bl.mel(0, "r:.5 0:1.5 1:.5 2:1.5 | 4:4 | r:1 3:.75 2:.25 1:2 | 0:4", sc, oct=2, vel=0.6)
    hp = S.part("harp", "harp", rev={"hall": -7}, gain=-5)
    hp.arp(0, 4, [c1, c1, c6, c6], sc, [0, 4, 7, 9, 7, 4, None, None], step=0.5, oct=0, vel=0.4)
    cb = S.part("cb", "cb", rev={"hall": -6})
    cb.n(0, 8, sc.p(0, 0), 0.5, voice=0).n(8, 8, sc.p(5, -1), 0.5, voice=0)
    return S


@track
def cap02_cine2():
    """Estampa B: "un punado de harina en la tinaja". Entra el pulso: arpa en
    corcheas y darbuka suave; el kaval juega con la cabeza del motivo.
    72 BPM, 4 compases = 13,3 s."""
    S = Score("cap02_cine2", 72, 4)
    sc = Scale(nm("A", 2), "aeolian")
    prog = [c1, c4, c6, c5]
    hp = S.part("harp", "harp", rev={"hall": -8})
    hp.arp(0, 4, prog, sc, [0, 2, 4, 7, 2, 4, 7, 9], step=0.5, oct=0, vel=0.55)
    db = S.part("darbuka", "darbuka", rev={"room": -9, "hall": -15})
    db.pat(0, 4, "D..kT.k.D.kkT.k.", step=0.25, vel=0.55)
    kv = S.part("kaval", "kaval", rev={"temple": -7, "hall": -13})
    kv.mel(0, "0:1.5^ 1:.5 2:1 4:1g | 5:1 4:.5 3:.5 0:2 | 2:1 4:.5 5:.5 7:1 6:.5 5:.5 | "
              "4:2~ 4:.5 2:.5 1:.5 0:.5", sc, oct=2, vel=0.66)
    pz = S.part("pizz", "pizz", rev={"hall": -8}, pan=0.35)
    for k, ch in enumerate(prog):
        pz.n(S.beat(k), 1, sc.p(ch[0], 0), 0.55).n(S.beat(k) + 2, 1, sc.p(ch[2], 0), 0.45)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-2)
    for k, ch in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(ch[0], 0), 0.45, voice=0)
    return S


# ============================================================================
# CAPITULO 3 - EL MONTE CARMELO (Hijaz en MI)
# ============================================================================
H_I, H_bII, H_iv, H_bVII, H_bVIa = [0, 2, 4], [1, 3, 5], [3, 5, 0], [6, 1, 3], [5, 0, 2]


@track
def cap03():
    """El desafio a los 450 profetas de Baal y el fuego que cae sobre el altar.
    El tema mas duro del Acto I: ostinato de cuerdas en semicorcheas, taikos y
    daf; las trompas dicen el motivo como un desafio; en B los profetas de Baal
    claman (coro grave en ostinato), y en A' cae el fuego: todo, con el coro
    arriba. 96 BPM, 32 compases = 80 s."""
    S = Score("cap03", 96, 32)
    sc = Scale(nm("E", 2), "hijaz")
    INTRO, A, B, A2, OUT = std_form()
    pA = [H_I, H_bII, H_iv, H_I, H_I, H_bVII, H_iv, H_I]
    pB = [H_iv, H_iv, H_bII, H_bII, H_I, H_bVII, H_bII, H_I]
    # ostinato de cuerdas graves (picado), sobre la fundamental de cada acorde
    os_ = S.part("ostinato", "vc", rev={"hall": -8}, art="spic", gain=2)
    os2 = S.part("ostinato_vla", "vla", rev={"hall": -8}, art="spic", gain=-1)
    cbo = S.part("ostinato_cb", "cb", rev={"hall": -8}, art="stac", gain=1)
    allp = [H_I] * 4 + pA + pB + pA + [H_I] * 4
    for k, ch in enumerate(allp):
        r = ch[0]
        figure = [0, 0, 1, 0, 0, 2, 1, 0] if k % 2 == 0 else [0, 0, 1, 0, 2, 1, 0, -1]
        dense = A <= k < OUT
        for j in range(8 if dense else 4):
            step = 0.5 if dense else 1.0
            d = r + figure[j % 8] if dense else r
            os_.n(S.beat(k) + j * step, step, sc.p(d, 1), 0.62 if j % 4 == 0 else 0.5)
        if A2 <= k < OUT or B <= k < A2:
            for j in range(8):
                os2.n(S.beat(k) + j * 0.5, 0.5, sc.p(r + [4, 4, 5, 4, 4, 2, 4, 5][j], 1), 0.48)
        cbo.n(S.beat(k), 1.0, sc.p(r, 0), 0.62)
        cbo.n(S.beat(k) + 2.5, 0.5, sc.p(r, 0), 0.5)
    os_.dyn([(0, 0.6), (4, 0.85), (12, 0.9), (20, 1.05), (28, 0.8), (32, 0.6)])
    cbo.dyn([(0, 0.5), (4, 0.8), (20, 1.0), (28, 0.7), (32, 0.5)])

    st = sections(S, STR4, rev={"hall": -6}, gain=-2)
    chords_to(st, STR4[2:], sc, A, pA, vel=0.45)
    chords_to(st, STR4[1:], sc, A2, pA, vel=0.55)
    trem = S.part("trem", "vln2", rev={"hall": -6}, art="trem", gain=-3)
    chords_to({"vln2": trem}, [("vln2", (59, 72))], sc, B, pB, vel=0.5)

    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.2, gain=1)
    hn.mel(A, MOTIF, sc, oct=2, vel=0.72)
    hn.mel(A + 4, "4:1> 4:.5 4:.5 5:1 4:1 | 3:1 2:1 1:2 | 0:.5 1:.5 2:1 3:1 4:1 | "
                  "5:1.5 4:.5 1:1 0:1", sc, oct=2, vel=0.74)
    hn.mel(B + 6, "4:4 | 4:2 5:2", sc, oct=1, vel=0.62, cresc=0.6)
    hn.mel(A2, MOTIF + " | 4:1> 4:.5 4:.5 5:1 4:1 | 3:1 2:1 1:2 | 0:.5 1:.5 2:1 3:1 4:1 | "
                       "5:1.5 4:.5 1:1 0:1", sc, oct=2, vel=0.85)
    hn.mel(OUT, "0:1.5> 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 0:2 | r:4 | r:4", sc, oct=2, vel=0.66)
    lb = S.part("lowbrass", "lowbrass", rev={"hall": -5}, far=0.2, gain=-2)
    for k, ch in enumerate(pA):
        lb.n(S.beat(A2 + k), 2, sc.p(ch[0], 0), 0.68)
        lb.n(S.beat(A2 + k) + 2.5, 1.5, sc.p(ch[0], 0), 0.55)

    # los profetas de Baal: coro grave en ostinato (B), luego el fuego (A')
    bc = S.part("baal_b", "choir_b", rev={"hall": -4}, far=0.3, vowel=("a", "o"), gain=1)
    tc = S.part("baal_t", "choir_t", rev={"hall": -4}, far=0.3, vowel=("o", "a"))
    chant = "0:.5> 0:.5 0:.5 1:.5 0:1 0:1 | 0:.5> 0:.5 1:.5 0:.5 -1:1 0:1 | "
    for k in range(4):
        bc.mel(B + 2 * k, chant, sc, oct=1, vel=0.55 + 0.05 * k, sep=True)
        tc.mel(B + 2 * k, transpose_mel(chant, 4), sc, oct=1, vel=0.5 + 0.05 * k, sep=True)
    ch = sections(S, CHOIR4[2:], rev={"hall": -3, "temple": -9}, far=0.3, vowel="a")
    chords_to(ch, CHOIR4[2:], sc, A2, pA, vel=0.58)

    # percusion
    tk = S.part("taiko", "taiko", rev={"hall": -7}, far=0.2, gain=-2)
    tk.pat(INTRO, 4, "D.......d.......", step=0.25, vel=0.5)
    tk.pat(A, 8, "D.....d.D...d...", step=0.25, vel=0.7)
    tk.pat(B, 6, "D.......D.......", step=0.25, vel=0.6)
    tk.pat(B + 6, 2, "D...d.d.D.d.dddd", step=0.25, vel=0.75)
    tk.pat(A2, 7, "D..dD.d.D..dD.dd", step=0.25, vel=0.85)
    tk.pat(A2 + 7, 1, "DdDdDdDdD.D.DDDD", step=0.25, vel=0.9)
    tk.pat(OUT, 4, "D.......d.......", step=0.25, vel=0.55)
    df = S.part("daf", "daf", rev={"hall": -11})
    df.pat(A, 8, IQA["saidi"], vel=0.6)
    df.pat(B, 8, "DTTTDTTT", vel=0.5 + 0.0)
    df.pat(A2, 8, IQA["saidi"], vel=0.72)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-2)
    gg.hit(S.beat(A2), "D", 0.75)
    tm = S.part("timp", "timp", rev={"hall": -7})
    tm.roll(S.beat(A2 - 1), 4, 0.7, stroke="D", cresc=0.95, pitch=82.4)
    tm.roll(S.beat(A - 1), 4, 0.55, stroke="D", cresc=0.9, pitch=82.4)
    zl = S.part("zills", "zills", rev={"hall": -10})
    zl.pat(B, 8, "X...x...X...x.x.", step=0.25, vel=0.5)
    return S


@track
def cap03_intro():
    """"Hasta cuando claudicareis entre dos pensamientos?". El duduk pregunta
    subiendo a la 2a menor y se queda colgado; debajo, cuerdas en tremolo y
    redobles de timbal que crecen. Sin patron de percusion. 80 BPM, 16 c = 48 s."""
    S = Score("cap03_intro", 80, 16)
    sc = Scale(nm("E", 2), "hijaz")
    prog = [H_I, H_bII, H_I, H_bII, H_I, H_bII, H_iv, H_bII, H_I, H_bII, H_iv, H_bVII,
            H_bII, H_I, H_I, H_bII]
    st = {k: S.part(k, k, rev={"hall": -5}, art="trem", gain=-3) for k in ("vla", "vln2")}
    chords_to(st, [("vla", (52, 64)), ("vln2", (59, 72))], sc, 0, prog, vel=0.42)
    cb = S.part("cb", "cb", rev={"hall": -6})
    drone(cb, sc.p(0, 0), 0, 16, vel=0.42, every=4)
    vc = S.part("vc", "vc", rev={"hall": -6}, gain=-3)
    drone(vc, sc.p(4, 0), 0, 16, vel=0.32, every=4)
    tm = S.part("timp", "timp", rev={"hall": -6})
    for b in (3, 7, 11, 15):
        tm.roll(S.beat(b), 4, 0.5, stroke="D", cresc=0.9, pitch=82.4)
    dud = S.part("duduk", "duduk", rev={"temple": -7, "hall": -13})
    dud.mel(2, "0:2^ 1:2~ | 1:1 2:1 1:1 0:1 | 4:3 3:.5 2:.5 | 1:4~ | 3:1.5 4:.5 5:2 | "
               "5:1 4:.5 3:.5 1:2 | 2:2 1:1 0:1 | 1:4~v | 5:2^ 4:1 3:1 | 3:1 1:1 -1:2 | "
               "1:2 2:2 | 0:4~", sc, oct=2, vel=0.7)
    zl = S.part("zills", "zills", rev={"hall": -9}, gain=1)
    for b in range(16):
        zl.n(S.beat(b) + 3, 0.5, "X" if b % 2 else "x", 0.45)
    rq = S.part("rattle", "riq", rev={"hall": -10}, far=0.5, gain=-1)
    rq.pat(0, 16, "S...s.s.", vel=0.45)
    # el viento caliente del monte, con su siseo
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=-2)
    wd.n(0, 66, 0, 0.6, fade=4.0)
    return S


@track
def cap03_cine1():
    """Estampa A: mediodia en el Carmelo, el altar y todo Israel mirando. No es
    la calma de Querit sino el calor antes del fuego: coro de hombres en 'u'
    que murmura, viento de calor y zils lejanos (los profetas de Baal danzan).
    60 BPM, 4 compases = 16 s."""
    S = Score("cap03_cine1", 60, 4)
    sc = Scale(nm("E", 2), "hijaz")
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=-2)
    wd.n(0, 16, 0, 0.6, fade=2.0)
    cb = S.part("cb", "cb", rev={"hall": -6}, art="trem", gain=-2)
    cb.n(0, 16, sc.p(0, 0), 0.45, voice=0)
    bb = S.part("chant_b", "choir_b", rev={"hall": -3, "temple": -8}, far=0.3, vowel=("u", "o"))
    tt = S.part("chant_t", "choir_t", rev={"hall": -3, "temple": -8}, far=0.3, vowel=("o", "a"))
    bb.mel(0, "0:2^ 0:1 1:1 | 0:3 -1:1 | 0:2 1:1 2:1 | 1:2 0:2~", sc, oct=1, vel=0.55)
    tt.mel(0.5, "r:2 4:1 5:1 | 4:3 3:1 | 4:2 5:1 4:1 | 3:2 4:2", sc, oct=1, vel=0.48)
    zl = S.part("zills", "zills", rev={"hall": -8}, gain=-2)
    zl.pat(0, 4, "X.x.....x.X.....", step=0.25, vel=0.45)
    fr = S.part("frame", "frame", rev={"hall": -10}, gain=-6, far=0.5)
    fr.pat(0, 4, "D.......g.......", step=0.25, vel=0.45)
    return S


@track
def cap03_cine2():
    """Estampa B: "y cayo fuego de Jehova". Pulso marcial de taikos en negras,
    escalas de violines que suben como pavesas, y el SHOFAR (tekiah). 80 BPM,
    4 compases = 12 s."""
    S = Score("cap03_cine2", 80, 4)
    sc = Scale(nm("E", 2), "hijaz")
    tk = S.part("taiko", "taiko", rev={"hall": -6})
    tk.pat(0, 3, "D...D...D...D.d.", step=0.25, vel=0.72)
    tk.pat(3, 1, "D.d.D.d.DdDdDDDD", step=0.25, vel=0.85)
    vr = S.part("runs", "vln1", rev={"hall": -5}, gain=1)
    run = " ".join(f"{d}:.25" for d in range(0, 15))
    vr.mel(1, run + " 15:.25", sc, oct=2, vel=0.6, sep=False)
    vr.mel(3, " ".join(f"{d}:.25" for d in range(4, 19)) + " 19:.25", sc, oct=1, vel=0.68)
    lo = sections(S, [("vc", (40, 60)), ("cb", (31, 50))], rev={"hall": -6})
    lo["cb"].n(0, 16, sc.p(0, 0), 0.6, voice=0)
    lo["vc"].n(0, 8, sc.p(0, 1), 0.55, voice=0).n(8, 8, sc.p(1, 1), 0.6, voice=0)
    sh = S.part("shofar", "shofar", rev={"temple": -6, "hall": -10})
    sh.mel(0, "r:.5 0:3.5 | r:1 4:.5 4:.5 4:.5 4:.5 0:1 | r:.5 0:3.5~ | 4:2 0:2", sc, oct=2,
           vel=0.75)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-4)
    gg.hit(0, "D", 0.6)
    return S


# ============================================================================
# CAPITULO 4 - HOREB (eolico en DO#)
# ============================================================================
@track
def cap04():
    """"No estaba Jehova en el viento... ni en el terremoto... ni en el fuego. Y
    tras el fuego un silbo apacible y delicado" (1 Reyes 19). La forma ES el
    pasaje: viento | terremoto | fuego | el silbo (un ney solo, agudo y limpio)
    | vuelve el viento. Sala de CUEVA. 60 BPM, 28 compases = 112 s."""
    S = Score("cap04", 60, 28)
    sc = Scale(nm("C#", 2), "aeolian")
    WIND, QUAKE, FIRE, VOICE, BACK = 0, 6, 12, 18, 26
    cave = {"cave": -6}
    wd = S.part("wind", "wind", rev={"cave": -8}, gain=3)
    wd.n(0, 26, 0, 0.85, fade=3.0)
    wd.n(S.beat(FIRE), 26, 0, 0.45, fade=4.0)
    wd.n(S.beat(BACK - 2), 18, 0, 0.85, fade=3.0)
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60)), ("vla", (50, 67))], rev=cave)
    drone(lo["cb"], sc.p(0, 0), WIND, 18, vel=0.5, every=6)
    drone(lo["vc"], sc.p(4, 1), WIND, 6, vel=0.38, every=6)
    lo["vc"].dyn([(0, 0.8), (6, 1.0), (12, 1.0), (18, 0.6), (28, 0.8)])
    # terremoto: redobles de timbal y taikos que retumban, metales graves
    tm = S.part("timp", "timp", rev={"cave": -6})
    for b in range(QUAKE, FIRE, 2):
        tm.roll(S.beat(b), 7.5, 0.62, stroke="D", cresc=0.7, pitch=69.3)
    tk = S.part("taiko", "taiko", rev={"cave": -5}, far=0.3, gain=-1)
    for b in range(QUAKE, FIRE):
        tk.hit(S.beat(b), "D", 0.75, pitch=46.0)
        tk.hit(S.beat(b) + 1.5, "d", 0.5, pitch=46.0)
        tk.hit(S.beat(b) + 2.75, "D", 0.62, pitch=46.0)
    lb = S.part("lowbrass", "lowbrass", rev={"cave": -6}, far=0.3, gain=-2)
    lb.n(S.beat(QUAKE), 12, sc.p(0, 1), 0.55, cresc=0.5)
    lb.n(S.beat(QUAKE + 3), 12, sc.p(1, 1), 0.6, cresc=0.5)
    # fuego: tremolo que sube, crepitar
    tr = sections(S, [("vla", (50, 67)), ("vln2", (57, 74)), ("vln1", (62, 81))],
                  prefix="trem_", rev=cave, art="trem", gain=-2)
    fprog = [c1, c6, c7, c1, c6, c7]
    chords_to(tr, [("vla", (52, 64)), ("vln2", (59, 71)), ("vln1", (64, 76))], sc, FIRE,
              fprog, vel=0.48)
    for k in tr.values():
        k.dyn([(FIRE, 0.7), (VOICE - 1, 1.2), (VOICE, 0.6)])
    fi = S.part("fire", "fire", rev={"cave": -8}, gain=0)
    fi.n(S.beat(FIRE), 24, 0, 0.7, fade=2.5)
    hn = S.part("horns", "horns", rev={"cave": -5}, far=0.3, gain=-2)
    hn.mel(FIRE + 2, "0:2> 1:2 | 2:4 | r:4 | 0:2> 1:2 | 4:4", sc, oct=2, vel=0.6)
    # el silbo apacible: ney agudo y limpio, celesta y armonicos de violin
    voice_prog = [c1, c6, c3, c7, c1, c6, c4, c1]
    pads = sections(S, [("vla", (52, 64)), ("vln2", (59, 71))], prefix="v_", rev=cave, gain=-4)
    chords_to(pads, [("vla", (52, 64)), ("vln2", (59, 71))], sc, VOICE, voice_prog, vel=0.44)
    hv = S.part("harmonics", "vln1", rev=cave, gain=-7, dark=4000.0)
    hv.n(S.beat(VOICE), 32, sc.p(4, 3), 0.42, voice=0)
    ney = S.part("ney", "ney", rev={"cave": -5})
    ney.mel(VOICE, "r:1 4:3~ | 5:1.5 4:.5 2:2 | 4:1 6:1 7:2~ | 6:2 5:1 3:1 | 4:4~ | "
                   "2:1.5 3:.5 4:1 7:1 | 7:2 5:1 3:1 | 2:2 0:2~", sc, oct=2, vel=0.72)
    cel = S.part("celesta", "celesta", rev={"cave": -5}, gain=-2)
    for k, ch in enumerate(voice_prog):
        cel.n(S.beat(VOICE + k) + 2, 1, sc.p(ch[2], 3), 0.42)
        cel.n(S.beat(VOICE + k) + 3, 1, sc.p(ch[1], 3), 0.35)
    # gotas en la cueva durante el viento
    dr = S.part("drops", "kanun", rev={"cave": -4}, gain=-6)
    for b, d in ((1, 7), (2, 4), (3, 9), (4, 5), (27, 7)):
        dr.n(S.beat(b) + 1.5, 0.5, sc.p(d, 2), 0.42, lv=True)
    return S


@track
def cap04_intro():
    """"Que haces aqui, Elias?". Un oud solo en taqsim (libre, con silencios y
    notas repetidas) sobre un bordon de cuerdas graves y el coro de hombres en
    'u', muy quieto. Sin percusion. 60 BPM, 12 compases = 48 s."""
    S = Score("cap04_intro", 60, 12)
    sc = Scale(nm("C#", 2), "aeolian")
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"cave": -6})
    drone(lo["cb"], sc.p(0, 0), 0, 12, vel=0.45, every=6)
    drone(lo["vc"], sc.p(4, 1), 0, 12, vel=0.34, every=6)
    ch = sections(S, CHOIR4[:2], rev={"cave": -4}, far=0.4, vowel=("u", "o"))
    chords_to(ch, CHOIR4[:2], sc, 0, [c1, c6, c1, c4, c1, c7], bpc=2, vel=0.42)
    for p in list(ch.values()) + list(lo.values()):
        p.dyn([(0, 0.65), (3, 0.9), (6, 1.25), (9, 0.95), (12, 0.65)])
    oud = S.part("oud", "oud", rev={"cave": -7})
    oud.mel(0, "r:2 0:.25 0:.25 0:.25 0:.25 4:1 | 3:.5 2:.5 1:.5 0:.5 0:2 | "
               "r:1 2:.5 3:.5 4:1 5:.5 4:.5 | 4:.25 4:.25 4:.25 4:.25 3:.5 2:.5 2:2 | "
               "r:2 7:.5 6:.5 5:.5 4:.5 | 5:1 4:.5 3:.5 2:1 0:1 | r:1 0:.25 0:.25 0:.25 0:.25 "
               "1:.5 2:.5 4:.5 3:.5 | 2:2 0:2 | r:2 4:.25 4:.25 4:.25 4:.25 5:.5 6:.5 | "
               "7:1 6:.5 5:.5 4:1 2:1 | 3:.5 2:.5 1:.5 2:.5 0:2 | r:4", sc, oct=1, vel=0.62)
    return S


@track
def cap04_cine1():
    """Estampa A: noche azul cerrada sobre la boca de la cueva. Una nota tenida
    de contrabajo, viento y gotas de celesta que caen en la roca. 60 BPM, 16 s."""
    S = Score("cap04_cine1", 60, 4)
    sc = Scale(nm("C#", 2), "aeolian")
    cb = S.part("cb", "cb", rev={"cave": -6})
    cb.n(0, 16, sc.p(0, 0), 0.5, voice=0)
    vc = S.part("vc", "vc", rev={"cave": -6}, gain=-4)
    vc.n(2, 14, sc.p(4, 1), 0.38, cresc=0.5, voice=0)
    wd = S.part("wind", "wind", rev={"cave": -8}, gain=1)
    wd.n(0, 16, 0, 0.65, fade=2.0)
    cel = S.part("celesta", "celesta", rev={"cave": -4}, gain=1)
    for t, d in ((0.5, 11), (2.25, 7), (3.5, 9), (5.0, 4), (6.75, 7), (8.5, 2), (10.0, 9),
                 (12.25, 4)):
        cel.n(t, 1, sc.p(d, 2), 0.5)
    ch = S.part("choir_t", "choir_t", rev={"cave": -3}, far=0.5, vowel="u", gain=-3)
    ch.n(4, 12, sc.p(0, 1), 0.4, cresc=0.5, voice=0)
    return S


@track
def cap04_cine2():
    """Estampa B: el haz de luz, "un silbo apacible y delicado". Entra el pulso
    como un LATIDO lento de tambor de marco; el arpa sube; armonicos agudos de
    violin. 60 BPM, 4 compases = 16 s."""
    S = Score("cap04_cine2", 60, 4)
    sc = Scale(nm("C#", 2), "aeolian")
    fr = S.part("heart", "frame", rev={"cave": -8})
    fr.pat(0, 4, "D.d.....D.d.....", step=0.25, vel=0.58)
    fr.dyn([(0, 1.0), (2, 1.1), (4, 0.9)])
    hp = S.part("harp", "harp", rev={"cave": -5})
    # cada gesto del arpa dura medio compas (como el latido): el principio y el
    # final del bucle tienen el mismo peso
    hp.arp(0, 4, [c1, c6, c3, c7], sc, [0, 2, 4, 7, 3, 5, 7, 10], step=0.5, oct=0,
           vel=0.55)
    hv = sections(S, [("vln1", (64, 88)), ("vln2", (60, 80))], rev={"cave": -5}, gain=-5)
    hv["vln1"].n(0.5, 7.5, sc.p(4, 3), 0.44, cresc=0.5, voice=0)
    hv["vln1"].n(8.0, 7.5, sc.p(4, 3), 0.44, cresc=-0.5, voice=0)
    hv["vln2"].n(1.0, 7.0, sc.p(0, 3), 0.4, cresc=0.5, voice=0)
    hv["vln2"].n(8.0, 7.5, sc.p(0, 3), 0.4, cresc=-0.5, voice=0)
    cb = S.part("cb", "cb", rev={"cave": -6}, gain=-3)
    cb.n(0, 16, sc.p(0, 0), 0.42, voice=0)
    return S


# ============================================================================
# CAPITULO 5 - EL LLAMADO (lidio en FA)
# ============================================================================
@track
def cap05():
    """Eliseo arando con doce yuntas cuando Elias le echa el manto encima
    (1 Reyes 19:19). Lidio: la 4a aumentada lo abre y lo hace esperanzado. El
    kaval canta los campos; en B las trompas dicen el motivo en lidio (el
    manto), y A' es todo luz. 92 BPM, 32 compases = 83,5 s."""
    S = Score("cap05", 92, 32)
    sc = Scale(nm("F", 2), "lydian")
    INTRO, A, B, A2, OUT = std_form()
    pA = [c1, c2, c1, c2, c6, c3, c2, c1]
    pB = [c1, c2, c5, c1, c6, c3, c2, c1]
    st = sections(S, STR5, rev={"hall": -6})
    chords_to(st, STR5[:3], sc, INTRO, [c1, c2, c1, c2], vel=0.42)
    chords_to(st, STR5[:3], sc, A, pA, vel=0.46)
    chords_to(st, STR4, sc, B, pB, vel=0.54)
    chords_to(st, STR5, sc, A2, pA, vel=0.58)
    chords_to(st, STR5[:3], sc, OUT, [c1, c2, c1, c1], vel=0.42)
    st["vln1"].dyn([(20, 0.8), (28, 0.9)])

    melA = ("4:1 2:.5 4:.5 7:1 6:.5 4:.5 | 5:1 3:.5 5:.5 8:2~ | 7:1 6:.5 7:.5 4:1 2:1 | "
            "3:1.5 4:.5 5:2~ | 5:1 7:.5 5:.5 2:1 0:1 | 2:1 4:.5 6:.5 4:2 | "
            "5:1 6:.5 5:.5 3:1g 1:1 | 2:1 1:.5 2:.5 0:2")
    kv = S.part("kaval", "kaval", rev={"temple": -9, "hall": -12})
    kv.mel(A, melA, sc, oct=2, vel=0.7)
    kv.mel(A2, melA.replace("8:2~", "8:2t"), sc, oct=2, vel=0.76)
    kv.mel(OUT, "4:1 2:.5 4:.5 7:2~ | 5:1 3:.5 5:.5 4:2 | r:4 | r:4", sc, oct=2, vel=0.6)
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.25, gain=1)
    hn.mel(B, MOTIF, sc, oct=1, vel=0.7)
    hn.mel(B + 4, "2:4 | 3:4 | 1:4 | 0:4", sc, oct=1, vel=0.55)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(B + 4, ANSWER, sc, oct=2, vel=0.62)
    ch = sections(S, CHOIR4[1:], rev={"hall": -3, "temple": -9}, far=0.35, vowel=("a", "o"))
    chords_to(ch, CHOIR4[1:], sc, A2, pA, vel=0.5)

    hp = S.part("harp", "harp", rev={"hall": -8})
    hp.arp(INTRO, 4, [c1, c2, c1, c2], sc, [0, 2, 4, 7, 4, 2, None, None], step=0.5, oct=0,
           vel=0.45)
    hp.arp(A, 8, pA, sc, [0, 2, 4, 7, 9, 7, 4, 2], step=0.5, oct=0, vel=0.5)
    hp.arp(B, 8, pB, sc, [0, 4, 7, 4, 2, 4, 7, 9], step=0.5, oct=0, vel=0.45)
    hp.arp(A2, 8, pA, sc, [0, 2, 4, 7, 9, 7, 4, 2], step=0.5, oct=0, vel=0.55)
    hp.arp(OUT, 4, [c1, c2, c1, c1], sc, [0, 2, 4, 7, 4, 2, None, None], step=0.5, oct=0,
           vel=0.45)
    fr = S.part("frame", "frame", rev={"hall": -11})
    fr.pat(A, 8, "D..TD.T.", vel=0.6)
    fr.pat(B, 8, "D.......", vel=0.45)
    fr.pat(A2, 8, IQA["fallahi"], vel=0.66)
    sh = S.part("shaker", "shaker", rev={"room": -12})
    sh.pat(INTRO, 4, "S...s...", vel=0.35)
    sh.pat(A, 8, "S.s.S.s.", vel=0.45)
    sh.pat(A2, 8, "SsSsSsSs", vel=0.5)
    sh.pat(OUT, 4, "S...s...", vel=0.35)
    bl = S.part("oxbells", "bells", rev={"hall": -8}, gain=-8, far=0.5)
    for b in (1, 3, 9, 25, 29, 31):
        bl.n(S.beat(b) + 1, 1, sc.p(4, 2), 0.35)
    return S


@track
def cap05_intro():
    """"Te ruego que me dejes besar a mi padre y a mi madre". Un violin tierno
    sobre lira y pizzicato, sin percusion, a la mitad de velocidad que el tema.
    80 BPM, 16 compases = 48 s."""
    S = Score("cap05_intro", 80, 16)
    sc = Scale(nm("F", 2), "lydian")
    prog = [c1, c3, c6, c2, c1, c5, c6, c2, c1, c3, c2, c5, c6, c2, c1, c1]
    ly = S.part("lyre", "lyre", rev={"hall": -8})
    ly.arp(0, 16, prog, sc, [0, 4, 7, 4, 2, 4, 7, 9], step=0.5, oct=0, vel=0.48)
    pz = S.part("pizz", "pizz", rev={"hall": -8}, pan=0.35)
    for k, ch in enumerate(prog):
        pz.n(S.beat(k), 1, sc.p(ch[0], 0), 0.5).n(S.beat(k) + 2, 1, sc.p(ch[2], 0), 0.42)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=-1)
    for k, ch in enumerate(prog):
        cb.n(S.beat(k), 4, sc.p(ch[0], 0), 0.44, voice=0)
    va = S.part("vla", "vla", rev={"hall": -5}, gain=-5)
    chords_to({"vla": va}, [("vla", (53, 65))], sc, 0, prog, vel=0.35)
    vn = S.part("violin", "solo_vln", rev={"hall": -7, "temple": -12})
    vn.mel(2, "5:2 4:1 2:1 | 3:1.5 4:.5 5:2 | 4:3 2:1 | 4:1 6:1 8:2~ | 7:1.5 5:.5 2:2 | "
              "3:2 1:2 | 2:1.5 4:.5 7:2 | 6:2 4:2 | 5:1 6:.5 5:.5 3:2 | 4:4~ | "
              "5:1.5 4:.5 2:2 | 1:2 3:2 | 0:4~", sc, oct=2, vel=0.64)
    return S


@track
def cap05_cine1():
    """Estampa A: dorado de dia sobre los campos de labor y las doce yuntas.
    Textura luminosa sin pulso: coro en 'a', cuerdas agudas y la celesta que
    brilla con el motivo en lidio. 60 BPM, 4 compases = 16 s."""
    S = Score("cap05_cine1", 60, 4)
    sc = Scale(nm("F", 2), "lydian")
    ch = sections(S, CHOIR4[1:], rev={"hall": -2, "temple": -8}, far=0.35, vowel=("e", "a"),
                  breath=0.09)
    chords_to(ch, CHOIR4[1:], sc, 0, [c1, c2], bpc=2, vel=0.46)
    hp = S.part("harp", "harp", rev={"hall": -6}, gain=-3)
    hp.arp(0, 4, [c1, c1, c2, c2], sc, [6, None, 7, None, 8, None, 7, None], step=0.5, oct=1,
           vel=0.42)
    st = sections(S, [("vln2", (60, 76)), ("vln1", (67, 84))], rev={"hall": -4}, gain=-4)
    chords_to(st, [("vln2", (60, 76)), ("vln1", (67, 84))], sc, 0, [c1, c2], bpc=2, vel=0.4)
    cel = S.part("celesta", "celesta", rev={"hall": -5})
    cel.mel(0, "r:.5 0:1 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 2:2 | 4:1 5:.5 4:.5 3:1 2:1 | "
               "1:1 2:.5 1:.5 0:2", sc, oct=3, vel=0.55)
    cb = S.part("cb", "cb", rev={"hall": -6}, gain=0)
    cb.n(0, 8, sc.p(0, 0), 0.46, voice=0).n(8, 8, sc.p(1, 0), 0.46, voice=0)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=0)
    for t, d in ((0.25, 4), (2.0, 7), (3.5, 9), (5.5, 4), (7.25, 11), (9.0, 7), (11.0, 4),
                 (13.0, 9)):
        cr.n(t, 2, sc.p(d, 3), 0.45)
    br = S.part("breeze", "wind", rev={"hall": -10}, gain=-1)   # brisa sobre los campos
    br.n(0, 16, 0, 0.5, fade=2.5)
    for p in list(ch.values()) + list(st.values()):
        p.dyn([(0, 0.8), (2, 1.15), (4, 0.85)])
    return S


@track
def cap05_cine2():
    """Estampa B: el manto sobre los hombros de Eliseo. Las trompas dicen la
    cabeza del motivo en lidio sobre tambor de marco y arpa. 80 BPM, 4 c = 12 s."""
    S = Score("cap05_cine2", 80, 4)
    sc = Scale(nm("F", 2), "lydian")
    fr = S.part("frame", "frame", rev={"hall": -10})
    fr.pat(0, 3, "D..TD.T.", vel=0.62)
    fr.pat(3, 1, "D.TTDTTT", vel=0.7)
    hp = S.part("harp", "harp", rev={"hall": -7})
    hp.arp(0, 4, [c1, c2, c6, c1], sc, [0, 2, 4, 7, 9, 7, 4, 2], step=0.5, oct=0, vel=0.52)
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.2)
    hn.mel(0, "r:1 0:1.5 1:.5 2:1 | 4:4 | 3:.75 2:.25 1:1 2:2 | 4:2 7:2", sc, oct=2, vel=0.74)
    st = sections(S, STR4[:3], rev={"hall": -6}, gain=-3)
    chords_to(st, STR4[:3], sc, 0, [c1, c2, c6, c1], vel=0.5)
    return S
