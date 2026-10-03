"""Las siete pistas globales: titulo, Santuario, jefe, victoria, derrota,
Ascension y prologo."""

from arrange import (Score, Scale, nm, IQA, MOTIF, MOTIF_HEAD, ANSWER, STR5, STR4,
                     CHOIR4, sections, chords_to, drone, tanpura, augment)

TRACKS = {}


def track(fn):
    TRACKS[fn.__name__] = fn
    return fn


# Grados de acorde en Hijaz (MI): I = MI SOL# SI, bII = FA LA DO, iv = LA DO MI,
# bVII = RE FA LA, bVI+ = DO MI SOL# (aumentado), v = SI RE FA
I_, bII, iv, bVII, bVIa, v_ = [0, 2, 4], [1, 3, 5], [3, 5, 0], [6, 1, 3], [5, 0, 2], [4, 6, 1]


# ============================================================================
# TEMA PRINCIPAL (titulo) - Hijaz en MI, 80 BPM, 32 compases = 96 s
# ============================================================================
@track
def main_theme():
    """El tema de MANTLE. El duduk lo presenta (A), los violines cantan el tema
    B sobre el coro, y en A' lo dicen las trompas, los tenores y los violines a
    la vez sobre taikos y darbuka. La salida devuelve el motivo al duduk solo
    sobre el bordon: es lo que empalma con la entrada."""
    S = Score("main_theme", 80, 32)
    sc = Scale(nm("E", 3), "hijaz")
    INTRO, A, B, A2, OUT = 0, 4, 12, 20, 28

    # --- cuerdas: lecho -------------------------------------------------------
    st = sections(S, STR5, rev={"hall": -6})
    st["cb"].dyn([(0, 0.7), (4, 0.8), (12, 0.9), (20, 1.1), (28, 0.8), (32, 0.7)])
    # intro y salida: bordon de MI en contrabajos y chelos, sul tasto
    drone(st["cb"], sc.p(0, -1), INTRO, 4, vel=0.42)
    drone(st["vc"], sc.p(0, 0), INTRO, 4, vel=0.34)
    drone(st["cb"], sc.p(0, -1), OUT, 4, vel=0.42)
    drone(st["vc"], sc.p(4, -1), OUT, 4, vel=0.32)
    # A: tres voces graves
    chords_to(st, STR5[:3], sc, A, [I_, bII, iv, I_, I_, bVII, iv, I_], vel=0.48)
    # B: cuatro voces (los violines primeros cantan)
    chords_to(st, STR4, sc, B, [iv, bII, bVIa, I_, iv, bVII, bII, I_], vel=0.56)
    # A': todo
    chords_to(st, STR4, sc, A2, [I_, bII, iv, I_, I_, bVII, iv, I_], vel=0.66)

    # --- melodias -------------------------------------------------------------
    dud = S.part("duduk", "duduk", rev={"temple": -8, "hall": -14}, gain=0.5)
    dud.mel(A, "0:1.5^ 1:.5 2:1 4:1g | 3:.75 2:.25 1:1 2:2~ | 4:1 5:.5m 4:.5 3:1 2:1 | "
               "1:1 2:.5 1:.5 0:2v", sc, oct=1, vel=0.74)
    dud.mel(A + 4, "4:1.5^ 5:.5 4:1 2:1 | 3:.75 2:.25 1:1 -1:2~ | 0:1 1:.5 2:.5 3:1 4:1g | "
                   "2:1 1:.5 2:.5 0:2", sc, oct=1, vel=0.72)
    dud.mel(OUT, "0:1.5^ 1:.5 2:1 4:1g | 3:.75 2:.25 1:1 0:2v | 0:4~ | r:4", sc, oct=1, vel=0.62)

    btheme = ("3:1 4:.5 5:.5 7:2 | 6:1.5 5:.5 3:2 | 5:1 4:.5 5:.5 7:2 | 6:1 4:1 2:2 | "
              "3:1 4:.5 5:.5 7:1.5 8:.5 | 8:1 7:.5 6:.5 5:2 | 5:1 6:.5 5:.5 3:1 1:1 | 2:3~ 1:.5 2:.5")
    vm = S.part("vln_mel", "vln1", rev={"hall": -5}, gain=1.0, nv=10)
    vm.mel(B, btheme, sc, oct=1, vel=0.66)
    vm.mel(A2, MOTIF + " | " + ANSWER, sc, oct=1, vel=0.8)
    cm = S.part("vc_mel", "vc", rev={"hall": -6}, gain=-1.0)
    cm.mel(B, btheme, sc, oct=0, vel=0.6)

    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.3)
    chords_to({"horns": hn}, [("horns", (52, 64))], sc, B + 4, [iv, bVII, bII, I_], vel=0.45)
    hn.mel(A2, MOTIF + " | " + ANSWER, sc, oct=0, vel=0.78)
    lb = S.part("lowbrass", "lowbrass", rev={"hall": -5}, far=0.3, gain=-3)
    for k, ch in enumerate([I_, bII, iv, I_, I_, bVII, iv, I_]):
        lb.n(S.beat(A2 + k), 4, sc.p(ch[0], -1), 0.6)

    # --- coro ----------------------------------------------------------------
    ch = sections(S, CHOIR4, rev={"hall": -3, "temple": -9}, far=0.35, vowel=("o", "a"))
    for k in ("choir_a", "choir_t"):
        ch[k].prm["vowel"] = ("u", "o")
    chords_to(ch, CHOIR4[1:3], sc, INTRO, [I_, I_, bII, I_], vel=0.42)
    chords_to(ch, CHOIR4, sc, B, [iv, bII, bVIa, I_, iv, bVII, bII, I_], vel=0.5)
    chords_to(ch, CHOIR4[:2] + CHOIR4[3:], sc, A2, [I_, bII, iv, I_, I_, bVII, iv, I_],
              vel=0.6)
    chords_to(ch, CHOIR4[1:3], sc, OUT, [I_, bII, I_, I_], vel=0.42)
    ten = S.part("tenor_mel", "choir_t", rev={"hall": -3}, far=0.35, vowel="a", gain=-1)
    ten.mel(A2, MOTIF + " | " + ANSWER, sc, oct=0, vel=0.62)

    # --- arpa y oud ----------------------------------------------------------
    hp = S.part("harp", "harp", rev={"hall": -8})
    hp.arp(A, 8, [I_, bII, iv, I_, I_, bVII, iv, I_], sc, [0, 1, 2, 3, 4, 3, 2, 1], step=0.5,
           oct=-1, vel=0.5)
    hp.arp(B, 8, [iv, bII, bVIa, I_, iv, bVII, bII, I_], sc, [3, 4, 5, 4, 2, 4, 5, 6],
           step=0.5, oct=0, vel=0.42)
    hp.arp(A2, 8, [I_, bII, iv, I_, I_, bVII, iv, I_], sc, [0, 2, 3, 4, 5, 4, 3, 2],
           step=0.5, oct=0, vel=0.55)
    oud = S.part("oud", "oud", rev={"hall": -12}, gain=-2)
    oud.mel(A2, "0:.5 0:.25 0:.25 4,:.5 0:.5 1:.5 0:.5 4,:1 " * 8, sc, oct=0, vel=0.55)

    # --- percusion -------------------------------------------------------------
    fr = S.part("frame", "frame", rev={"hall": -12})
    fr.pat(INTRO + 2, 2, "D.......", vel=0.45)
    fr.pat(A, 8, "D..gT.g.", vel=0.6)
    fr.pat(A2, 7, IQA["maqsum"], vel=0.75)
    fr.pat(A2 + 7, 1, "DTTTDTTT", vel=0.8)
    fr.pat(OUT, 2, "D.......", vel=0.4)
    db = S.part("darbuka", "darbuka", rev={"room": -10, "hall": -16})
    db.pat(A2, 8, "D.tkT.tkD.tkT.tk", step=0.25, vel=0.6, skip=(7,))
    db.pat(A2 + 7, 1, "D.tkT.tkTkTkTkTk", step=0.25, vel=0.7)
    tk = S.part("taiko", "taiko", rev={"hall": -7}, far=0.2)
    for b in range(A2, A2 + 8):
        tk.hit(S.beat(b), "D", 0.85 if b % 4 == 0 else 0.65)
        tk.hit(S.beat(b) + 2.5, "d", 0.5)
    tk.pat(A2 + 7, 1, "D.d.D.dd", vel=0.8)
    tm = S.part("timp", "timp", rev={"hall": -7})
    tm.roll(S.beat(B + 3), 4, 0.55, stroke="D", cresc=0.9, pitch=82.4)
    tm.roll(S.beat(B + 7), 4, 0.65, stroke="D", cresc=0.95, pitch=82.4)
    for b in (A2, A2 + 4):
        tm.n(S.beat(b), 1, sc.p(0, -1), 0.8)
    gg = S.part("gong", "gong", rev={"hall": -6}, gain=-4)
    gg.hit(S.beat(A2), "D", 0.6)
    zl = S.part("zills", "zills", rev={"hall": -10})
    zl.pat(A2, 8, "x...X...", vel=0.5)
    sh = S.part("shaker", "shaker", rev={"room": -12})
    sh.pat(A, 8, "S.s.S.s.", vel=0.4)
    sh.pat(A2, 8, "SsSsSsSs", vel=0.55)

    # --- aire --------------------------------------------------------------------
    wd = S.part("wind", "wind", rev={"hall": -10}, gain=-2)
    wd.n(S.beat(INTRO), 16, 0, 0.55, fade=2.5)
    wd.n(S.beat(OUT), 16, 0, 0.55, fade=2.5)
    cr = S.part("crotales", "crotales", rev={"hall": -6})
    for b, d in ((1, 4), (3, 2), (12, 7), (16, 4), (29, 4)):
        cr.n(S.beat(b), 2, sc.p(d, 2), 0.42)
    return S


# ============================================================================
# EL SANTUARIO (menu) - eolico en SOL, 64 BPM, 24 compases = 90 s
# ============================================================================
c1, c2, c3, c4, c5, c6, c7 = ([0, 2, 4], [1, 3, 5], [2, 4, 6], [3, 5, 0], [4, 6, 1],
                              [5, 0, 2], [6, 1, 3])


@track
def menu():
    """Entre partidas, mientras el jugador gasta ofrendas: el unico sitio sin
    prisa ni peligro. Sin percusion. El arpa dice el motivo en eolico (SOL LA
    SIb RE), el chelo canta en B y la celesta contesta. Cuerdas en sul tasto."""
    S = Score("menu", 64, 24)
    sc = Scale(nm("G", 2), "aeolian")
    pI = [c1, c6, c3, c7]
    pA = [c1, c6, c3, c1, c1, c7, c4, c1]
    pB = [c6, c3, c7, c1, c4, c6, c7, c7]
    allp = pI + pA + pB + pI
    st = sections(S, STR5[:4], rev={"hall": -5}, gain=-1)
    chords_to(st, STR5[:4], sc, 0, allp, vel=0.42)
    for p in st.values():
        p.dyn([(0, 0.85), (4, 0.95), (12, 1.1), (20, 0.85), (24, 0.85)])
    hl = S.part("harp_lh", "harp", rev={"hall": -7}, gain=-2)
    hl.arp(0, 24, allp, sc, [0, 2, 4, 2, 3, 2, 4, 2], step=0.5, oct=0, vel=0.46)
    hr = S.part("harp_rh", "harp", rev={"hall": -7}, gain=2, pan=-0.3)
    hr.mel(4, MOTIF + " | " + ANSWER, sc, oct=2, vel=0.62, lv=True)
    hr.mel(20, MOTIF_HEAD + " | 3:.75 2:.25 1:1 0:2 | r:4 | r:4", sc, oct=2, vel=0.56, lv=True)
    vc = S.part("cello", "solo_vc", rev={"hall": -6, "temple": -12})
    vc.mel(12, "2:2 4:1 5:1 | 6:2 4:2 | 3:1.5 4:.5 5:2 | 4:4~ | 3:1.5 2:.5 0:2 | 2:1 4:1 7:2 | "
               "6:2 5:1 3:1 | 1:4~", sc, oct=1, vel=0.64)
    cel = S.part("celesta", "celesta", rev={"hall": -6}, gain=-2)
    for b, d in ((3, 4), (7, 2), (11, 4), (15, 7), (19, 8), (23, 4)):
        cel.n(S.beat(b) + 2, 1, sc.p(d, 3), 0.42)
        cel.n(S.beat(b) + 3, 1, sc.p(d - 1, 3), 0.36)
    return S


# ============================================================================
# TEMA DEL JEFE - Hijaz en RE, 132 BPM, 32 compases = 58,2 s
# ============================================================================
@track
def boss_theme():
    """Suena en las diez arenas, asi que tiene que aguantar diez peleas sin
    cansar: dos oleadas con una calma tensa en medio. Taikos de guerra,
    ostinato de cuerdas graves picado, riff de metales graves sacado del
    motivo, golpes de coro, y en la segunda oleada las trompas dicen el motivo
    AUMENTADO por encima de todo. En la calma, el shofar."""
    S = Score("boss_theme", 132, 32)
    sc = Scale(nm("D", 2), "hijaz")
    ENT, W1, CALM, W2, TR = 0, 4, 14, 20, 30
    prog = ([I_, I_, bII, I_] + [I_, bII, I_, bII, iv, bII, bVII, I_, I_, bII] +
            [iv, iv, bII, bII, I_, I_] + [I_, bII, iv, I_, I_, bVII, iv, I_, I_, bII] +
            [bII, bII])
    osv = S.part("ostinato", "vc", rev={"hall": -9}, art="spic", gain=2)
    osa = S.part("ostinato_vla", "vla", rev={"hall": -9}, art="spic", gain=0)
    cbo = S.part("ostinato_cb", "cb", rev={"hall": -9}, art="stac", gain=-1)
    fig = [0, 0, 1, 0, 0, 2, 1, 0]
    for k, ch in enumerate(prog):
        r = ch[0]
        for j in range(8):
            osv.n(S.beat(k) + j * 0.5, 0.5, sc.p(r + fig[j], 1), 0.62 if j in (0, 3, 6) else 0.5)
            if not (CALM <= k < W2):
                osa.n(S.beat(k) + j * 0.5, 0.5, sc.p(r + [4, 4, 5, 4, 4, 2, 4, 5][j], 1), 0.48)
        if not (CALM <= k < W2):
            cbo.n(S.beat(k), 1.0, sc.p(r, 0), 0.65)
            cbo.n(S.beat(k) + 1.5, 1.0, sc.p(r, 0), 0.55)
            cbo.n(S.beat(k) + 3.0, 0.5, sc.p(r, 0), 0.5)
    osv.dyn([(0, 0.8), (4, 0.95), (14, 0.65), (20, 1.05), (30, 0.95), (32, 0.8)])
    lb = S.part("lowbrass", "lowbrass", rev={"hall": -6}, far=0.15)
    riff = ("0:.5> 0:.5 0:.5 1:.5 2:1> 1:.5 0:.5 | 0:.5> 0:.5 0:.5 -1:.5 0:2 | ")
    for k in range(0, 10, 2):
        lb.mel(W1 + k, riff, sc, oct=1, vel=0.72, sep=True)
    for k in range(0, 10, 2):
        lb.mel(W2 + k, riff, sc, oct=1, vel=0.8, sep=True)
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.15, gain=1)
    hn.mel(W1 + 6, "4:2> 5:2 | 4:4", sc, oct=2, vel=0.7)
    # el motivo ensanchado (cada compas del motivo dura dos)
    hn.mel(W2, "0:3> 1:1 | 2:2 4:2 | 3:1.5 2:.5 1:2 | 2:4 | "
               "4:2 5:1 4:1 | 3:2 2:2 | 1:2 2:1 1:1 | 0:4", sc, oct=2, vel=0.86)
    ch = sections(S, CHOIR4, rev={"hall": -3, "temple": -10}, far=0.25, vowel="a")
    for k in range(W1, CALM):
        c = prog[k]
        for vi, (inst, _) in enumerate(CHOIR4):
            p = sc.p(c[vi % 3], 1 + (1 if vi >= 2 else 0))
            ch[inst].n(S.beat(k) + 1.5, 0.45, p, 0.62, acc=True, sep=True)
            ch[inst].n(S.beat(k) + 3.5, 0.45, p, 0.55, acc=True, sep=True)
    chords_to(ch, CHOIR4, sc, W2, prog[W2:W2 + 10], vel=0.6)
    cu = sections(S, CHOIR4[:2], prefix="calm_", rev={"hall": -3}, far=0.3, vowel=("u", "o"))
    chords_to(cu, CHOIR4[:2], sc, CALM, prog[CALM:W2], vel=0.48)
    sh = S.part("shofar", "shofar", rev={"temple": -6, "hall": -10})
    sh.mel(CALM + 1, "0:3.5 r:.5 | r:4 | 4:.5 4:.5 4:.5 4:.5 0:2 | r:4", sc, oct=2, vel=0.72)
    tk = S.part("taiko", "taiko", rev={"hall": -7}, gain=-2.5)
    tk.pat(ENT, 3, "D.......D...d...", step=0.25, vel=0.7)
    tk.pat(ENT + 3, 1, "D...d.d.D.d.dddd", step=0.25, vel=0.78)
    tk.pat(W1, 10, "D..dD...D..dD.d.", step=0.25, vel=0.8)
    tk.pat(CALM, 6, "D...............", step=0.25, vel=0.55)
    tk.pat(W2, 10, "D..dD.d.D..dD.dd", step=0.25, vel=0.88)
    tk.pat(TR, 2, "D.d.D.d.DdDdDDDD", step=0.25, vel=0.85)
    tm = S.part("timp", "timp", rev={"hall": -7})
    tm.roll(S.beat(W2 - 2), 8, 0.6, stroke="D", cresc=0.95, pitch=73.4)
    for b in (W1, W1 + 4, W1 + 8, W2, W2 + 4, W2 + 8):
        tm.n(S.beat(b), 1, sc.p(0, 0), 0.8)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-3)
    gg.hit(S.beat(W1), "D", 0.6).hit(S.beat(W2), "D", 0.75)
    df = S.part("daf", "daf", rev={"hall": -12}, gain=0)
    df.pat(W1, 10, IQA["saidi"], vel=0.6)
    df.pat(W2, 10, IQA["saidi"], vel=0.7)
    zl = S.part("zills", "zills", rev={"hall": -10}, gain=0)
    zl.pat(W1, 10, "x.......X.......", step=0.25, vel=0.5)
    zl.pat(W2, 10, "x...x...X...x...", step=0.25, vel=0.55)
    rq = S.part("riq", "riq", rev={"room": -10, "hall": -16}, gain=-2)
    rq.pat(CALM, 6, "S...s...", vel=0.45)
    return S


# ============================================================================
# VICTORIA - Hijaz en MI -> MI mayor, 80 BPM, 16 compases = 48 s
# ============================================================================
@track
def victory():
    """Al cerrar un capitulo: fanfarria triunfal (el motivo en MAYOR en trompas,
    violines y coro) y despues serena (arpa, cuerdas en piano y el kaval).
    Los dos ultimos compases crecen hacia la fanfarria: el bucle empalma."""
    S = Score("victory", 80, 16)
    sh = Scale(nm("E", 2), "hijaz")
    sM = Scale(nm("E", 2), "major")
    st = sections(S, STR5, rev={"hall": -5})
    chords_to(st, STR5, sh, 0, [I_, bII], vel=0.56)
    chords_to(st, STR5, sM, 2, [c1, c4, c5, c1, c4, c5], vel=0.62)
    chords_to(st, STR5[:4], sM, 8, [c1, c6, c4, c1, c4, c1], vel=0.44)
    chords_to(st, STR5, sM, 14, [c5, c5], vel=0.5)
    for p in st.values():
        p.dyn([(0, 0.95), (2, 1.05), (7, 1.0), (8, 0.75), (13, 0.7), (16, 0.95)])
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.15, gain=1)
    hn.mel(0, "0:1.5> 1:.5 2:1 4:1 | 4:2 7:2", sh, oct=2, vel=0.72)
    hn.mel(2, MOTIF + " | 3:2 4:2 | 7:4", sM, oct=2, vel=0.82)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(2, MOTIF + " | 3:2 4:2 | 7:4", sM, oct=3, vel=0.66)
    ch = sections(S, CHOIR4, rev={"hall": -3, "temple": -9}, far=0.3, vowel="a")
    chords_to(ch, CHOIR4, sM, 2, [c1, c4, c5, c1, c4, c1], vel=0.58)
    lb = S.part("lowbrass", "lowbrass", rev={"hall": -5}, far=0.2, gain=-3)
    for k, c in enumerate([c1, c4, c5, c1, c4, c1]):
        lb.n(S.beat(2 + k), 3.5, sM.p(c[0], 1), 0.62)
    tm = S.part("timp", "timp", rev={"hall": -6})
    for b in (0, 2, 4, 6):
        tm.n(S.beat(b), 1, sM.p(0, 0), 0.8).n(S.beat(b) + 3, 1, sM.p(4, -1), 0.65)
    tm.roll(S.beat(14), 8, 0.5, stroke="D", cresc=0.9, pitch=82.4)
    tk = S.part("taiko", "taiko", rev={"hall": -7}, gain=-3)
    tk.pat(0, 8, "D.......D...d...", step=0.25, vel=0.7)
    cr = S.part("crotales", "crotales", rev={"hall": -5}, gain=-2)
    for b in (2, 4, 6, 7):
        cr.n(S.beat(b), 2, sM.p(4, 3), 0.45)
    hp = S.part("harp", "harp", rev={"hall": -7})
    hp.arp(8, 6, [c1, c6, c4, c1, c4, c1], sM, [0, 1, 2, 3, 4, 3, 2, 1], step=0.5, oct=1,
           vel=0.5)
    hp.arp(14, 2, [c5, c5], sM, [0, 1, 2, 3, 4, 5, 6, 7], step=0.5, oct=1, vel=0.55)
    kv = S.part("kaval", "kaval", rev={"temple": -7, "hall": -12})
    kv.mel(9, "0:1.5 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 2:2~ | 4:2 2:1 1:1 | 0:4~", sM, oct=2,
           vel=0.6)
    return S


# ============================================================================
# DERROTA - eolico en RE, 60 BPM, 12 compases = 48 s
# ============================================================================
@track
def death():
    """Lo que suena cuando el profeta cae. Corta, sin pulso: un chelo canta el
    motivo INVERTIDO (todo lo que subia, baja) sobre un bajo de lamento que
    desciende un grado por compas; coro en 'u' y golpes sueltos de timbal."""
    S = Score("death", 60, 12)
    sc = Scale(nm("D", 2), "aeolian")
    prog = [c1, c7, c6, c5, c1, c4, c6, c5, c1, c7, c6, c1]
    st = sections(S, STR5[:4], rev={"hall": -5})
    chords_to(st, STR5[:4], sc, 0, prog, vel=0.44)
    for p in st.values():
        p.dyn([(0, 0.9), (4, 1.05), (8, 1.0), (12, 0.9)])
    vc = S.part("cello", "solo_vc", rev={"hall": -6, "temple": -12}, gain=1)
    vc.mel(1, "0:1.5 -1:.5 -2:1 -4:1 | -3:.75 -2:.25 -1:1 -2:2 | -1:1 -2:.5 -3:.5 -4:1 -5:1 | "
              "-6:1 -5:.5 -6:.5 -7:2~ | -4:2 -3:1 -2:1 | -2:1.5 -1:.5 0:2 | -1:1 -3:1 -4:2 | "
              "-7:4~", sc, oct=3, vel=0.66)
    ch = sections(S, CHOIR4[:3], rev={"hall": -3, "temple": -9}, far=0.4, vowel=("u", "o"))
    chords_to(ch, CHOIR4[:3], sc, 4, prog[4:], vel=0.42)
    hp = S.part("harp", "harp", rev={"hall": -7})
    hp.arp(0, 12, prog, sc, [0, None, 2, None, 3, None, 2, None], step=0.5, oct=0, vel=0.48)
    tm = S.part("timp", "timp", rev={"hall": -6}, far=0.3)
    for b in (0, 4, 8):
        tm.n(S.beat(b), 1, sc.p(0, 0), 0.55)
    return S


# ============================================================================
# LA ASCENSION - Hijaz en MI -> MI mayor, 76 BPM, 24 compases = 75,8 s
# ============================================================================
@track
def ascension():
    """"He aqui un carro de fuego con caballos de fuego... y Elias subio al
    cielo en un torbellino" (2 Reyes 2:11). Torbellino que se forma (viento,
    tremolo que sube, coro en 'u') | el carro (galope de taikos, el motivo en
    las trompas, en Hijaz) | la gloria (MI MAYOR: el motivo AUMENTADO en coro,
    trompas y violines, campanas) | el manto cae (arpa y una voz sola)."""
    S = Score("ascension", 76, 24)
    sh = Scale(nm("E", 2), "hijaz")
    sM = Scale(nm("E", 2), "major")
    WHIRL, CHAR, GLORY, MANTLE = 0, 4, 12, 20
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=2)
    wd.n(0, 18, 0, 0.75, fade=3.0)
    wd.n(S.beat(MANTLE + 2), 16, 0, 0.75, fade=3.0)
    tr = sections(S, STR4, prefix="trem_", rev={"hall": -5}, art="trem", gain=-2)
    chords_to(tr, STR4, sh, WHIRL, [I_, bII, I_, bII], vel=0.5)
    for p in tr.values():
        p.dyn([(0, 0.7), (4, 1.0)])
    st = sections(S, STR5, rev={"hall": -5})
    chords_to(st, STR5, sh, CHAR, [I_, bII, iv, I_, I_, bVII, iv, I_], vel=0.56)
    chords_to(st, STR5, sM, GLORY, [c1, c4, c5, c1, c6, c4, c5, c1], vel=0.64)
    chords_to(st, STR5[:3], sM, MANTLE, [c1, c4, c1, c1], vel=0.44)
    # escalas de torbellino (violines primeros)
    runs = S.part("runs", "vln1", rev={"hall": -5}, gain=-1)
    for b in range(CHAR, GLORY, 2):
        runs.mel(b + 1.5, " ".join(f"{d}:.25" for d in range(0, 8)), sh, oct=3, vel=0.55)
    hn = S.part("horns", "horns", rev={"hall": -4}, far=0.15, gain=1)
    hn.mel(CHAR, MOTIF + " | " + ANSWER, sh, oct=2, vel=0.76)
    glory = augment(MOTIF, 2.0)
    hn.mel(GLORY, glory, sM, oct=2, vel=0.86)
    vm = S.part("vln_mel", "vln1", rev={"hall": -5})
    vm.mel(GLORY, glory, sM, oct=3, vel=0.7)
    ch = sections(S, CHOIR4, rev={"hall": -3, "temple": -8}, far=0.3, vowel=("u", "o"))
    chords_to(ch, CHOIR4[1:3], sh, WHIRL, [I_, bII, I_, bII], vel=0.46)
    for p in ch.values():
        p.prm["vowel"] = ("a", "o")
    chords_to(ch, CHOIR4, sM, GLORY, [c1, c4, c5, c1, c6, c4, c5, c1], vel=0.6)
    sop = S.part("sop_mel", "choir_s", rev={"hall": -3, "temple": -8}, far=0.3, vowel="a")
    sop.mel(GLORY, glory, sM, oct=3, vel=0.6)
    tk = S.part("taiko", "taiko", rev={"hall": -7}, far=0.15)
    tk.pat(CHAR, 7, "D.dD.dD.D.dD.dDd", step=0.25, vel=0.78)
    tk.pat(CHAR + 7, 1, "DdDdDdDdDDDDDDDD", step=0.25, vel=0.85)
    tk.pat(GLORY, 8, "D.......D.......", step=0.25, vel=0.8)
    tm = S.part("timp", "timp", rev={"hall": -6})
    tm.roll(S.beat(CHAR - 1), 4, 0.6, stroke="D", cresc=0.9, pitch=82.4)
    tm.roll(S.beat(GLORY - 1), 4, 0.7, stroke="D", cresc=0.95, pitch=82.4)
    for b in range(GLORY, MANTLE, 2):
        tm.n(S.beat(b), 1, sM.p(0, 0), 0.75)
    fi = S.part("fire", "fire", rev={"hall": -8}, gain=-3)
    fi.n(S.beat(CHAR), 32, 0, 0.6, fade=2.0)
    bl = S.part("bells", "bells", rev={"temple": -5})
    for k, d in enumerate([0, 4, 2, 7, 4, 0, 4, 7]):
        bl.n(S.beat(GLORY + k), 4, sM.p(d, 2), 0.55)
    gg = S.part("gong", "gong", rev={"hall": -5}, gain=-2)
    gg.hit(S.beat(GLORY), "D", 0.8)
    hp = S.part("harp", "harp", rev={"hall": -6})
    hp.arp(GLORY, 8, [c1, c4, c5, c1, c6, c4, c5, c1], sM,
           [0, 1, 2, 3, 4, 5, 6, 7], step=0.5, oct=1, vel=0.48)
    hp.arp(MANTLE, 4, [c1, c4, c1, c1], sM, [0, 2, 4, 2, 3, 2, 4, 2], step=0.5, oct=1,
           vel=0.5)
    vo = S.part("voice", "voice", rev={"hall": -5, "temple": -8}, vowel=("a", "o"))
    vo.mel(MANTLE, "0:1.5^ 1:.5 2:1 4:1 | 3:.75 2:.25 1:1 2:2~ | 4:2 2:1 1:1 | 0:4~", sM, oct=2,
           vel=0.6)
    return S


# ============================================================================
# PROLOGO: "EL CIELO CERRADO" - frigio en MI, 66 BPM, 24 compases = 87,3 s
# ============================================================================
@track
def prologue():
    """Elias ante Acab: "Vive Jehova... que no habra lluvia ni rocio en estos
    anos sino por mi palabra" (1 Reyes 17:1). Un juramento, no una batalla.
    Antes (aire seco, bordon, un shofar lejano) | el juramento (el duduk sobre
    cuerdas y un tambor de marco lento; el coro afirma) | la sequia: las capas
    se van y no vuelven; las ultimas gotas del kanun, y al final solo el viento
    y el bordon, que es lo que empalma con el principio."""
    S = Score("prologue", 66, 24)
    sc = Scale(nm("E", 2), "phrygian")
    BEFORE, OATH, DROUGHT = 0, 6, 14
    wd = S.part("wind", "wind", rev={"hall": -9}, gain=1)
    wd.n(0, 30, 0, 0.7, fade=3.0)
    wd.n(S.beat(12), 50, 0, 0.75, fade=4.0)
    lo = sections(S, [("cb", (31, 50)), ("vc", (40, 60))], rev={"hall": -6})
    drone(lo["cb"], sc.p(0, 0), 0, 24, vel=0.5, every=4)
    drone(lo["vc"], sc.p(4, 0), 0, 24, vel=0.34, every=4)
    st = sections(S, STR4[1:], rev={"hall": -5}, gain=-1)
    prog = [c1, c2, c1, c7, c1, c2, c4, c1]
    chords_to(st, STR4[1:], sc, OATH, prog, vel=0.5)
    chords_to(st, STR4[1:3], sc, DROUGHT, [c1, c2, c1, c1], vel=0.4)
    sf = S.part("shofar", "shofar", rev={"temple": -5, "hall": -8}, far=0.6, gain=-3)
    sf.mel(1, "0:3.5 r:.5 | r:4 | r:4 | 4:.5 4:.5 4:.5 r:.5 0:2", sc, oct=2, vel=0.62)
    dud = S.part("duduk", "duduk", rev={"temple": -7, "hall": -13})
    dud.mel(OATH, "0:2^ 1:1 0:1 | 2:1.5 1:.5 0:2 | 4:3 3:.5 2:.5 | 3:2 1:2 | "
                  "0:1.5^ 1:.5 2:1 4:1g | 3:.75 2:.25 1:1 2:2~ | 4:1 5:.5 4:.5 3:1 2:1 | "
                  "1:1 2:.5 1:.5 0:2v | r:4 | 0:4~", sc, oct=2, vel=0.72)
    fr = S.part("frame", "frame", rev={"hall": -11})
    fr.pat(OATH, 8, "D.....T.", vel=0.6)
    fr.pat(DROUGHT, 2, "D.......", vel=0.45)
    ch = sections(S, CHOIR4[:3], rev={"hall": -3, "temple": -9}, far=0.35, vowel=("o", "a"))
    chords_to(ch, CHOIR4[:3], sc, OATH + 4, prog[4:], vel=0.52)
    ka = S.part("drops", "kanun", rev={"hall": -6, "temple": -10}, gain=-2)
    for t, d in ((14.5, 7), (15.25, 4), (16.0, 9), (17.5, 5), (18.75, 8), (21.0, 4), (2.5, 7),
                 (4.5, 9)):
        ka.n(S.beat(t), 0.5, sc.p(d, 2), 0.45, lv=True)
    tp = S.part("tanpura", "tanpura", rev={"hall": -10}, gain=-5)
    tanpura(tp, sc, BEFORE, 6, root_oct=0, step=1.0, vel=0.42)
    tanpura(tp, sc, DROUGHT + 2, 8, root_oct=0, step=1.0, vel=0.36)
    return S
