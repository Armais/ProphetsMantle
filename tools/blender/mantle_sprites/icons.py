"""Iconos de objetos y milagros -> assets/sprites/icons.png (2026-09-24).

El usuario: "cada objeto y milagro debe tener su propio icono". Hasta hoy los 22 objetos
compartian CUATRO iconos del tileset (uno por categoria: flecha, estrella, orbe, corazon) y
los 10 milagros seis (el Carro de Fuego y el Fuego del Cielo eran la misma bola de fuego).

Hoja: 8 columnas x 7 filas de 128 px (= 32 unidades a x4). Celda i = objeto con id i
(0..21, el orden de ItemRegistry::init); celda 22 + m = milagro con MiracleID m (0..9);
celdas 32..50 = las mejoras de El Santuario (`SANCTUARY`, 2026-10-03).
La misma tabla vive en src/render/sprites.h (namespace Icon): mantenlas iguales.

Estilo: el de los props del tileset (Workbench + cavity + contorno oscuro, post_soft), con
la camara a 35 grados y el ENCUADRE MEDIDO por la caja de cada objeto (como portraits.py):
una tinaja y un arco no caben en el mismo marco fijo. Se dibujan de 9 a 22 unidades de
alto, asi que lo que cuenta es la SILUETA y el color: una pieza gorda y un color propio
por icono, sin detalles de menos de un octavo del marco.

Uso (Blender 4.x, o el modulo bpy):
    import mantle_sprites.icons as ic; ic.main(SPRITES_DIR, TMP_DIR)          # los 32
    ic.main(SPRITES_DIR, TMP_DIR, only=['honda', 'carro'])                    # parchea
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Vector, Matrix

from . import parts, render
from .parts import box, sphere, tube

CELL = 128
COLS, ROWS = 8, 7       # 32 objetos y milagros + 19 de El Santuario (celdas 32..50)
SS = 2
ELEV = 35.0
OUTLINE_PX = 3          # mas grueso que en las hojas: a 9 unidades un contorno de 2 px se pierde
MARGIN = 1.12           # aire alrededor de la caja del objeto


def M(name, h, sat=1.25):
    return parts.get_mat('ic_' + name, parts.vivid(parts.hexc(h), sat=sat))


def rot(ob, pivot, axis, deg):
    parts.rotate_about(ob, pivot, axis, deg)
    return ob


# =============================================================================
# OBJETOS (id 0..21)
# =============================================================================
def honda():
    """0. Honda de David: la bolsa de cuero con su piedra, colgada de las dos cuerdas que se
    juntan en el lazo del dedo. Con dos tramos por cuerda salia un hexagono de alambre."""
    leather = M('leather', '#8a5a34')
    cord = M('cord', '#e0c890')
    stone = M('stone', '#a0a6ae')
    sphere('pouch', (0, 0, -0.45), 0.36, leather, scale=(1.3, 0.8, 0.5))
    sphere('rock', (0, -0.08, -0.30), 0.25, stone, scale=(1.0, 0.95, 0.9))
    top = (0.10, 0, 0.75)
    for s in (-1, 1):
        tube(f'c{s}', (s * 0.44, 0, -0.42), top, 0.04, 0.035, cord, segs=8)
    sphere('knot', top, 0.08, cord)
    tube('loop', (top[0], 0, top[2]), (top[0] + 0.12, 0, top[2] + 0.22), 0.04, 0.04, cord, segs=8)
    sphere('peb', (0.62, -0.1, -0.70), 0.14, M('stone_d', '#7c828a'), scale=(1, 0.9, 0.8))
    sphere('peb2', (-0.60, -0.05, -0.74), 0.11, M('stone_d', '#7c828a'), scale=(1, 0.9, 0.8))

def vara():
    """1. Vara de Aaron: la vara que florecio (Numeros 17:8) — brotes, flores y almendras."""
    wood = M('rod', '#7a5230')
    leaf = M('leaf', '#5fae3e')
    petal = M('petal', '#f6d8e4', sat=1.0)
    almond = M('almond', '#c9a064')
    ob = tube('staff', (0, 0, -0.9), (0, 0, 0.95), 0.07, 0.055, wood, segs=10)
    buds = ((0.0, 0.62, 1), (0.0, 0.25, -1), (0.0, -0.12, 1), (0.0, 0.88, -1))
    for i, (_, z, s) in enumerate(buds):
        sphere(f'lf{i}', (s * 0.17, -0.02, z), 0.13, leaf, scale=(1.3, 0.5, 0.55))
    for i, (x, z) in enumerate(((0.12, 0.98), (-0.14, 0.45), (0.15, 0.08))):
        for k in range(5):
            a = k * 2 * math.pi / 5
            sphere(f'p{i}_{k}', (x + math.cos(a) * 0.09, -0.08, z + math.sin(a) * 0.09), 0.07, petal,
                   scale=(1, 0.5, 1))
        sphere(f'pc{i}', (x, -0.12, z), 0.05, M('pistil', '#f2c040'))
    sphere('alm', (-0.14, -0.05, -0.35), 0.10, almond, scale=(0.8, 0.7, 1.3))
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'Y', 28.0)


def pan():
    """2. Pan de cada dia: una hogaza redonda con sus cortes."""
    crust = M('crust', '#c98a3a')
    top = M('crust_t', '#e0a852')
    sphere('loaf', (0, 0, 0.0), 0.62, crust, scale=(1.0, 0.95, 0.55))
    sphere('dome', (0, -0.02, 0.10), 0.52, top, scale=(1.0, 0.95, 0.52))
    for i, x in enumerate((-0.24, 0.0, 0.24)):
        box(f'cut{i}', (x, -0.05, 0.33), (0.07, 0.62, 0.05), M('cut', '#f2d9a2', sat=1.0), round=0.6)


def aceite():
    """3. Aceite de la Uncion: el CUERNO de aceite (1 Samuel 16:13) con su tapa de oro."""
    horn = M('horn', '#e4d2a8', sat=1.0)
    horn_d = M('horn_d', '#b89a6a', sat=1.0)
    gold = M('gold', '#e8b840')
    oil = M('oil', '#f0c030')
    pts = [(-0.75, 0, -0.15), (-0.42, 0, 0.10), (-0.05, 0, 0.22), (0.32, 0, 0.18), (0.62, 0, 0.02)]
    radii = [0.07, 0.16, 0.24, 0.30, 0.33]
    for i in range(len(pts) - 1):
        tube(f'h{i}', pts[i], pts[i + 1], radii[i], radii[i + 1], horn if i % 2 else horn_d, segs=16)
    tube('rim', (0.60, 0, 0.03), (0.72, 0, -0.03), 0.34, 0.35, gold, segs=16, rim_round=0.03)
    sphere('oilsurf', (0.70, 0, -0.02), 0.30, oil, scale=(0.35, 1.0, 1.0))
    sphere('drop', (0.86, -0.02, -0.42), 0.11, oil, scale=(1, 1, 1.4))
    tube('band', (-0.42, 0, 0.10), (-0.36, 0, 0.13), 0.17, 0.18, gold, segs=14)


def sandalias():
    """4. Sandalias del profeta: el par con la suela GIRADA hacia la camara (tumbada, de
    canto, era una raya), suela redondeada y dos tiras cruzadas, no una escalera."""
    sole = M('sole', '#8c5a30')
    sole_t = M('sole_t', '#b88048')
    strap = M('strap', '#5a3418')
    for i, (x, yaw) in enumerate(((-0.36, 8.0), (0.36, -8.0))):
        objs = [
            box(f'sole{i}', (x, 0, 0), (0.46, 1.20, 0.10), sole, round=0.95),
            box(f'top{i}', (x, 0, 0.06), (0.40, 1.12, 0.04), sole_t, round=0.95),
            box(f'x1{i}', (x, -0.22, 0.12), (0.52, 0.10, 0.06), strap, round=0.6,
                rot=Matrix.Rotation(math.radians(30), 3, 'Z')),
            box(f'x2{i}', (x, -0.22, 0.12), (0.52, 0.10, 0.06), strap, round=0.6,
                rot=Matrix.Rotation(math.radians(-30), 3, 'Z')),
            box(f'toe{i}', (x, -0.46, 0.11), (0.08, 0.14, 0.06), strap, round=0.6),
            tube(f'heel{i}', (x - 0.2, 0.40, 0.14), (x + 0.2, 0.40, 0.14), 0.05, 0.05, strap, segs=8),
        ]
        for o in objs:
            rot(o, (x, 0, 0), 'Z', yaw)
    # +55 en X lleva la normal de la suela a la direccion de la camara (35 grados)
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'X', 50.0)

def mana():
    """5. Mana del cielo: el gomer (un cuenco) colmado de copos blancos (Exodo 16:31)."""
    bowl = M('bowl', '#a8683c')
    flake = M('flake', '#fbf6e8', sat=1.0)
    tube('bowl', (0, 0, -0.35), (0, 0, 0.12), 0.40, 0.62, bowl, segs=24, rim_round=0.05)
    k = 0
    for ring, (r, z, n) in enumerate(((0.45, 0.16, 11), (0.28, 0.26, 7), (0.08, 0.33, 3))):
        for i in range(n):
            a = i * 2 * math.pi / n + ring * 0.4
            sphere(f'f{k}', (math.cos(a) * r, math.sin(a) * r * 0.9, z), 0.12, flake)
            k += 1
    for i, (x, z) in enumerate(((-0.55, 0.55), (0.1, 0.72), (0.55, 0.5))):
        sphere(f'fall{i}', (x, -0.05, z), 0.07, flake)


def tablas():
    """6. Tablas de la Ley: dos losas de piedra de cabeza redonda, con sus renglones."""
    slab = M('slab', '#b8b2a4', sat=1.0)
    line = M('slab_line', '#6e6a62', sat=1.0)
    for i, x in enumerate((-0.33, 0.33)):
        box(f's{i}', (x, 0, -0.10), (0.58, 0.16, 1.05), slab, round=0.25)
        tube(f'top{i}', (x, -0.08, 0.42), (x, 0.08, 0.42), 0.29, 0.29, slab, segs=24)
        for k in range(4):
            box(f'l{i}_{k}', (x, -0.09, 0.30 - k * 0.20), (0.36, 0.02, 0.05), line)


def arca():
    """7. Arca del Pacto: el cofre de oro, las varas y los dos querubines."""
    gold = M('agold', '#e8b438')
    gold_d = M('agold_d', '#b88420')
    wood = M('awood', '#8a5a30')
    box('chest', (0, 0, 0.0), (1.30, 0.72, 0.62), gold, round=0.12)
    box('lid', (0, 0, 0.36), (1.38, 0.78, 0.10), gold_d, round=0.3)
    box('band', (0, -0.37, 0.0), (1.30, 0.02, 0.10), gold_d)
    for s in (-1, 1):
        tube(f'pole{s}', (-0.95, s * 0.25, -0.12), (0.95, s * 0.25, -0.12), 0.05, 0.05, wood, segs=10)
        # querubin: cuerpo pequeno arrodillado y un ala que se alza hacia el otro
        sphere(f'cb{s}', (s * 0.45, 0, 0.55), 0.14, gold, scale=(0.9, 0.9, 1.2))
        sphere(f'ch{s}', (s * 0.45, -0.02, 0.76), 0.09, gold)
        w = box(f'w{s}', (s * 0.22, 0.02, 0.86), (0.52, 0.06, 0.20), gold_d, round=0.8, taper=(0.4, 1.0))
        rot(w, (s * 0.45, 0.02, 0.70), 'Y', s * 35.0)


def vino():
    """8. Vino nuevo: el odre de cuero con su racimo de uvas."""
    skin = M('wineskin', '#7a4a2c')
    tie = M('tie', '#d0b078')
    grape = M('grape', '#6a2a8a')
    sphere('body', (-0.12, 0.0, -0.05), 0.55, skin, scale=(1.0, 0.85, 0.85))
    tube('neck', (0.30, 0, 0.20), (0.58, 0, 0.48), 0.16, 0.09, skin, segs=12)
    tube('tie', (0.50, 0, 0.40), (0.56, 0, 0.46), 0.12, 0.12, tie, segs=12)
    for i, (x, y, z) in enumerate(((0.38, -0.35, -0.30), (0.56, -0.33, -0.30), (0.47, -0.36, -0.46),
                                   (0.29, -0.34, -0.47), (0.65, -0.32, -0.47), (0.38, -0.36, -0.62),
                                   (0.56, -0.34, -0.62), (0.47, -0.37, -0.77))):
        sphere(f'g{i}', (x, y, z), 0.10, grape)
    sphere('leaf', (0.47, -0.30, -0.16), 0.14, M('vleaf', '#4f9a3a'), scale=(1.4, 0.4, 0.7))


def agua_jordan():
    """9. Agua del Jordan: el cantaro con el agua azul asomando por la boca."""
    clay = M('wclay', '#c07a48')
    clay_d = M('wclay_d', '#8e5230')
    water = M('water', '#3c9ae0')
    tube('lo', (0, 0, -0.55), (0, 0, -0.10), 0.26, 0.45, clay, segs=20, rim_round=0.03)
    tube('hi', (0, 0, -0.10), (0, 0, 0.25), 0.45, 0.22, clay, segs=20)
    tube('neck', (0, 0, 0.25), (0, 0, 0.42), 0.18, 0.20, clay_d, segs=16)
    tube('rim', (0, 0, 0.40), (0, 0, 0.47), 0.25, 0.25, clay, segs=16, rim_round=0.02)
    sphere('surf', (0, 0, 0.45), 0.20, water, scale=(1, 1, 0.3))
    for i, (x, z, r) in enumerate(((0.30, 0.62, 0.09), (0.45, 0.45, 0.07), (-0.28, 0.66, 0.07))):
        sphere(f'd{i}', (x, -0.1, z), r, water, scale=(1, 1, 1.3))
    tube('wave', (-0.42, -0.05, -0.05), (0.42, -0.05, -0.05), 0.05, 0.05, M('wband', '#3c9ae0'), segs=8)


def antorcha():
    """10. Antorcha del altar: mango de madera, trapo enrollado y llama alta."""
    wood = M('twood', '#6e4626')
    rag = M('rag', '#c8b48a', sat=1.0)
    fire = M('fire', '#f07a22')
    fire_l = M('fire_l', '#ffd24a')
    tube('handle', (0.0, 0, -1.0), (0.0, 0, 0.25), 0.07, 0.10, wood, segs=10)
    tube('rag', (0.0, 0, 0.12), (0.0, 0, 0.42), 0.15, 0.16, rag, segs=14, rim_round=0.03)
    sphere('f0', (0.0, 0, 0.72), 0.30, fire, scale=(1.0, 0.9, 1.35))
    sphere('f1', (0.03, -0.1, 0.80), 0.18, fire_l, scale=(1.0, 0.9, 1.4))
    sphere('f2', (-0.12, 0, 1.08), 0.10, fire, scale=(1, 1, 1.5))
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'Y', -18.0)


def memorial():
    """11. Piedra del memorial: el monton de piedras levantado junto al Jordan (Josue 4)."""
    cols = ('#8e8a82', '#a6a096', '#77736c', '#9c968a', '#b2ac9e')
    k = 0
    for row, (z, n, r) in enumerate(((-0.45, 4, 0.26), (-0.08, 3, 0.24), (0.26, 2, 0.22))):
        for i in range(n):
            x = (i - (n - 1) * 0.5) * r * 1.75
            parts.rock(f'r{k}', (x, 0, z), r, M(f'mem{k % 5}', cols[k % 5], sat=1.0),
                       scale=(1.0, 0.85, 0.72), seed=k + 3, rough=0.12)
            k += 1
    parts.rock('cap', (0, 0, 0.58), 0.22, M('mem_cap', '#c8c0ae', sat=1.0), scale=(0.8, 0.75, 1.05),
               seed=11, rough=0.1)


def ceniza():
    """12. Ceniza del Carmelo: un cuenco de ceniza gris con ascuas encendidas."""
    bowl = M('abowl', '#5a4a3e')
    ash = M('ash', '#9c9894', sat=1.0)
    ember = M('ember', '#f06a20')
    ember_l = M('ember_l', '#ffc040')
    tube('bowl', (0, 0, -0.35), (0, 0, 0.08), 0.42, 0.64, bowl, segs=24, rim_round=0.05)
    sphere('ash', (0, 0, 0.10), 0.58, ash, scale=(1.0, 1.0, 0.35))
    for i, (x, y) in enumerate(((-0.2, -0.1), (0.16, -0.18), (0.05, 0.12), (0.30, 0.05), (-0.30, 0.15))):
        sphere(f'e{i}', (x, y, 0.26), 0.09, ember if i % 2 else ember_l)
    for i, (x, z) in enumerate(((-0.1, 0.55), (0.18, 0.75), (-0.05, 0.95))):
        sphere(f'smk{i}', (x, 0.05, z), 0.13 - i * 0.02, M('smoke', '#cfcac2', sat=1.0), scale=(1.2, 1, 0.8))


def tinaja_harina():
    """13. Tinaja de harina (1 Reyes 17:14): la tinaja con la harina blanca colmada."""
    clay = M('hclay', '#b5673c')
    clay_d = M('hclay_d', '#87462a')
    flour = M('flour', '#fbf4e4', sat=1.0)
    tube('lo', (0, 0, -0.6), (0, 0, -0.12), 0.30, 0.52, clay, segs=20, rim_round=0.03)
    tube('hi', (0, 0, -0.12), (0, 0, 0.30), 0.52, 0.34, clay, segs=20)
    tube('neck', (0, 0, 0.30), (0, 0, 0.42), 0.30, 0.33, clay_d, segs=16)
    sphere('flour', (0, 0, 0.44), 0.33, flour, scale=(1, 1, 0.55))
    tube('band', (0, 0, -0.02), (0, 0, 0.06), 0.52, 0.51, M('hband', '#d18a5e'), segs=20)
    box('scoop', (0.30, -0.10, 0.62), (0.34, 0.14, 0.06), M('scoop', '#8a6a44'), round=0.6)


def vasija_aceite():
    """14. Vasija de aceite (2 Reyes 4:2): la vasija panzuda con pico y el aceite de oro."""
    clay = M('oclay', '#9c5a36')
    clay_l = M('oclay_l', '#c07a4c')
    oil = M('ooil', '#f2c42c')
    sphere('body', (0, 0, -0.15), 0.52, clay, scale=(1.0, 1.0, 0.82))
    tube('neck', (0, 0, 0.22), (0, 0, 0.44), 0.22, 0.17, clay_l, segs=16)
    tube('spout', (0.18, 0, 0.36), (0.50, 0, 0.52), 0.10, 0.05, clay_l, segs=10)
    tube('handle', (-0.24, 0, 0.36), (-0.55, 0, 0.0), 0.06, 0.06, clay, segs=10)
    sphere('oil', (0, 0, 0.44), 0.16, oil, scale=(1, 1, 0.35))
    sphere('drop', (0.60, -0.02, 0.36), 0.08, oil, scale=(1, 1, 1.4))


def manto():
    """15. El Manto de Elias: el manto que CAE (2 Reyes 2:13) — una tela ondulando en el
    aire, con el orillo de oro por los bordes y sus mechones de pelo (2 Reyes 1:8). Como
    capa vista a 35 grados salia una pantalla de lampara y como pliegues apilados, una
    tarta: lo que se lee como TELA es la superficie ondulada con su borde."""
    cloth = M('mcloth', '#d0702a')
    cloth_d = M('mcloth_d', '#8a3e16')
    gold = M('mgold', '#ecc454')
    nx, ny = 44, 30
    xs = np.linspace(-0.85, 0.85, nx)
    ys = np.linspace(-0.55, 0.55, ny)
    X, Y = np.meshgrid(xs, ys)
    Z = 0.16 * np.sin(4.2 * X + 0.6) + 0.07 * np.sin(6.0 * Y + 1.3 * X) + 0.10 * X
    parts.heightfield('cloth', xs, ys, Z, cloth)
    # el reves, un pelo por debajo: sin grosor la tela se ve de papel al girarla
    parts.heightfield('back', xs, ys, Z - 0.035, cloth_d)
    def edge(pts, tag):
        for i in range(len(pts) - 1):
            tube(f'{tag}{i}', pts[i], pts[i + 1], 0.045, 0.045, gold, segs=8)
    zf = lambda x, y: 0.16 * math.sin(4.2 * x + 0.6) + 0.07 * math.sin(6.0 * y + 1.3 * x) + 0.10 * x
    for y0, tag in ((-0.55, 'hf'), (0.55, 'hb')):
        edge([(x, y0, zf(x, y0)) for x in np.linspace(-0.85, 0.85, 18)], tag)
    for x0, tag in ((-0.85, 'hl'), (0.85, 'hr')):
        edge([(x0, y, zf(x0, y)) for y in np.linspace(-0.55, 0.55, 10)], tag)
    for i, x in enumerate(np.linspace(-0.7, 0.7, 6)):
        sphere(f'tuft{i}', (x, -0.60, zf(x, -0.55) - 0.10), 0.06, cloth_d, scale=(0.8, 0.8, 1.6))
    # inclinada hacia la camara, cayendo en diagonal
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'X', 38.0)
            rot(o, (0, 0, 0), 'Y', -14.0)

def arco():
    """16. Arco de Joas: el arco tensado y tres saetas (2 Reyes 13:17)."""
    wood = M('bow', '#8a5230')
    string = M('string', '#efe6cc', sat=1.0)
    shaft = M('shaft', '#c49a60')
    head = M('ahead', '#9aa2ac', sat=1.0)
    feather = M('fletch', '#e8e0d0', sat=1.0)
    n = 9
    prev = None
    for i in range(n + 1):
        t = i / n
        a = math.radians(-70 + 140 * t)
        p = (-0.55 + math.cos(a) * 0.5, 0, math.sin(a) * 0.95)
        if prev is not None:
            r = 0.07 - abs(t - 0.5) * 0.06
            tube(f'b{i}', prev, p, r, r, wood, segs=10)
        prev = p
    top = (-0.55 + math.cos(math.radians(70)) * 0.5, 0, math.sin(math.radians(70)) * 0.95)
    bot = (top[0], 0, -top[2])
    tube('str', top, bot, 0.018, 0.018, string, segs=6)
    for i, dz in enumerate((-0.16, 0.0, 0.16)):
        a = math.radians(dz * 60)
        x0, x1 = -0.35, 0.85
        z0, z1 = dz * 0.6, dz * 0.6 + (x1 - x0) * math.sin(a)
        tube(f's{i}', (x0, -0.05, z0), (x1, -0.05, z1), 0.028, 0.028, shaft, segs=8)
        tube(f'h{i}', (x1, -0.05, z1), (x1 + 0.18, -0.05, z1 + 0.18 * math.sin(a)), 0.07, 0.005, head, segs=8)
        box(f'f{i}', (x0 + 0.05, -0.05, z0), (0.16, 0.03, 0.10), feather, round=0.4)


def higos():
    """17. Higos de Ezequias (2 Reyes 20:7): tres higos, uno abierto con su pulpa roja."""
    fig = M('fig', '#6a3a6e')
    fig_d = M('fig_d', '#4a2650')
    pulp = M('pulp', '#e0506a')
    seeds = M('seed', '#f2d890', sat=1.0)
    sphere('f0', (-0.35, 0.10, -0.05), 0.33, fig, scale=(1, 1, 1.15))
    tube('st0', (-0.35, 0.10, 0.30), (-0.30, 0.10, 0.45), 0.05, 0.03, M('stem', '#5a7a30'), segs=8)
    sphere('f1', (0.30, 0.18, -0.02), 0.30, fig_d, scale=(1, 1, 1.15))
    tube('st1', (0.30, 0.18, 0.30), (0.36, 0.18, 0.44), 0.05, 0.03, M('stem', '#5a7a30'), segs=8)
    # el abierto, delante: media esfera con la cara roja mirando a camara
    sphere('open', (0.0, -0.28, -0.20), 0.30, fig, scale=(1.0, 0.55, 1.0))
    sphere('pulp', (0.0, -0.40, -0.20), 0.25, pulp, scale=(1.0, 0.25, 0.95))
    for i, (x, z) in enumerate(((-0.08, -0.12), (0.08, -0.22), (0.0, -0.32), (-0.1, -0.28), (0.1, -0.08))):
        sphere(f'sd{i}', (x, -0.47, z), 0.03, seeds)


def sal():
    """18. Sal de Eliseo (2 Reyes 2:20): la vasija NUEVA, blanca y limpia, llena de sal."""
    bowl = M('sbowl', '#e2ddd0', sat=1.0)
    rim = M('srim', '#5aa0c8')
    salt = M('salt', '#ffffff', sat=1.0)
    tube('bowl', (0, 0, -0.35), (0, 0, 0.10), 0.36, 0.60, bowl, segs=24, rim_round=0.05)
    tube('rim', (0, 0, 0.06), (0, 0, 0.13), 0.61, 0.61, rim, segs=24)
    k = 0
    for ring, (r, z, n) in enumerate(((0.40, 0.16, 10), (0.22, 0.26, 6), (0.0, 0.33, 1))):
        for i in range(n):
            a = i * 2 * math.pi / max(n, 1) + ring
            ob = box(f'x{k}', (math.cos(a) * r, math.sin(a) * r, z), (0.14, 0.14, 0.14), salt)
            rot(ob, (math.cos(a) * r, math.sin(a) * r, z), 'Z', 30 + k * 17)
            rot(ob, (math.cos(a) * r, math.sin(a) * r, z), 'X', 20 + k * 11)
            k += 1
    for i, (x, z) in enumerate(((-0.22, 0.62), (0.26, 0.52))):
        sphere(f'glint{i}', (x, -0.1, z), 0.05, M('glint', '#fff4c0', sat=1.0))


def cinto():
    """19. Cinto de cuero (2 Reyes 1:8): el cinturon enrollado con su hebilla."""
    leather = M('belt', '#6a4024')
    leather_l = M('belt_l', '#8c5a34')
    metal = M('buckle', '#c8b070')
    n = 16
    for loop, (r, z) in enumerate(((0.55, -0.12), (0.42, 0.02))):
        prev = None
        for i in range(n + 1):
            a = i * 2 * math.pi / n
            p = (math.cos(a) * r, math.sin(a) * r * 0.8, z + i * 0.004)
            if prev is not None:
                tube(f'l{loop}_{i}', prev, p, 0.10, 0.10, leather if loop else leather_l, segs=6)
            prev = p
    tube('tail', (0.55, 0.0, -0.12), (0.95, -0.35, -0.18), 0.10, 0.10, leather_l, segs=6)
    box('buckle', (0.42, -0.30, 0.04), (0.30, 0.08, 0.26), metal, round=0.3)
    box('buckle_in', (0.42, -0.34, 0.04), (0.16, 0.06, 0.12), leather)


def reloj():
    """20. El Reloj de Acaz (2 Reyes 20:11): la escalinata del cuadrante con su gnomon y la
    sombra que RETROCEDE diez grados — el sol va encima para que no sea una escalera."""
    stone = M('dstone', '#c8bca4', sat=1.0)
    stone_d = M('dstone_d', '#a09480', sat=1.0)
    shadow = M('dshadow', '#4a4050', sat=1.0)
    sun = M('dsun', '#ffc83a')
    for i in range(4):
        box(f'step{i}', (0, -0.30 + i * 0.18, -0.48 + i * 0.16), (1.30 - i * 0.12, 0.40, 0.16),
            stone if i % 2 else stone_d, round=0.2)
    tube('gnomon', (0.0, 0.30, 0.08), (0.0, 0.30, 0.78), 0.06, 0.05, M('gnom', '#8a6a44'), segs=10)
    box('shadow', (-0.34, -0.30, -0.39), (0.48, 0.34, 0.02), shadow)
    sphere('sun', (0.52, 0.40, 0.80), 0.22, sun)
    for k in range(8):
        a = k * math.pi / 4
        box(f'ray{k}', (0.52 + math.cos(a) * 0.34, 0.40, 0.80 + math.sin(a) * 0.34), (0.14, 0.04, 0.05), sun,
            rot=Matrix.Rotation(-a, 3, 'Y'))


def panes_cebada():
    """21. Panes de cebada (2 Reyes 4:42): tortas planas apiladas y dos espigas."""
    loaf = M('bloaf', '#d09a52')
    loaf_d = M('bloaf_d', '#a8743a')
    stalk = M('stalk', '#d8c060')
    for i, (x, z) in enumerate(((-0.05, -0.40), (0.05, -0.20), (-0.02, 0.0))):
        sphere(f'l{i}', (x, 0, z), 0.52, loaf if i % 2 else loaf_d, scale=(1.0, 0.9, 0.22))
    for s in (-1, 1):
        base = (s * 0.52, 0.1, -0.35)
        tip = (s * 0.72, 0.0, 0.55)
        tube(f'stk{s}', base, tip, 0.025, 0.02, stalk, segs=6)
        for k in range(5):
            t = 0.55 + k * 0.1
            p = tuple(base[j] + (tip[j] - base[j]) * t for j in range(3))
            sphere(f'gr{s}{k}', p, 0.06, stalk, scale=(0.8, 0.8, 1.4))


# =============================================================================
# MILAGROS (celda 22 + MiracleID)
# =============================================================================
def cuervos():
    """22+0. Cuervos de Querit: el cuervo DE PERFIL con el pan en el pico (portraits.py)."""
    feather = M('rv', '#2b3244')
    feather_d = M('rv_d', '#1b2130')
    beak = M('rv_beak', '#8a92a0')
    bread = M('rv_bread', '#d6a559')
    sphere('body', (0.18, 0.00, -0.22), 0.50, feather, scale=(1.20, 0.88, 0.92))
    ob = box('tail', (0.98, 0.02, -0.12), (0.86, 0.20, 0.28), feather_d, round=0.7, taper=(0.40, 1.0))
    rot(ob, (0.62, 0.02, -0.16), 'Y', 22.0)
    ob = box('wing', (0.20, -0.30, -0.02), (0.95, 0.16, 0.44), feather_d, round=0.85, taper=(0.55, 0.95))
    rot(ob, (0.20, -0.30, -0.10), 'Y', -18.0)
    tube('neck', (-0.10, 0.00, 0.06), (-0.34, 0.00, 0.30), 0.20, 0.17, feather, segs=16)
    sphere('head', (-0.46, 0.00, 0.40), 0.27, feather, scale=(1.05, 0.96, 0.94))
    ob = box('beak', (-0.86, 0.00, 0.32), (0.48, 0.13, 0.13), beak, round=0.45, taper=(0.16, 0.5))
    rot(ob, (-0.62, 0.00, 0.36), 'Y', -14.0)
    sphere('eye', (-0.50, -0.22, 0.48), 0.085, M('rv_eye', '#efe7d4', sat=1.0), scale=(1.0, 0.6, 1.0))
    sphere('pan', (-1.13, 0.00, 0.18), 0.24, bread, scale=(1.05, 0.86, 0.72))


def sequia():
    """22+1. Palabra de sequia (1 Reyes 17:1): la tierra agrietada, la mata seca y el sol."""
    earth = M('dry', '#c49a5a')
    crack = M('dcrack', '#5a3a22')
    sun = M('ssun', '#ffb82a')
    dead = M('dead', '#8a6a4a')
    tube('ground', (0, 0, -0.45), (0, 0, -0.30), 0.85, 0.85, earth, segs=28, rim_round=0.04)
    for i in range(6):
        a = i * math.pi / 3 + 0.3
        ob = box(f'cr{i}', (math.cos(a) * 0.38, math.sin(a) * 0.38, -0.29), (0.55, 0.05, 0.03), crack)
        rot(ob, (math.cos(a) * 0.38, math.sin(a) * 0.38, -0.29), 'Z', math.degrees(a))
    tube('stem', (0.0, 0.0, -0.30), (0.05, 0, 0.20), 0.05, 0.03, dead, segs=8)
    tube('br0', (0.03, 0, 0.0), (-0.25, 0, 0.22), 0.03, 0.015, dead, segs=6)
    tube('br1', (0.04, 0, 0.08), (0.30, 0, 0.26), 0.03, 0.015, dead, segs=6)
    sphere('sun', (0.0, 0.3, 0.70), 0.30, sun)
    for k in range(10):
        a = k * math.pi / 5
        box(f'ray{k}', (math.cos(a) * 0.48, 0.3, 0.70 + math.sin(a) * 0.48), (0.18, 0.05, 0.06), sun,
            rot=Matrix.Rotation(-a, 3, 'Y'))


def fuego_cielo():
    """22+2. Fuego del cielo (2 Reyes 1:10, "descendio fuego del cielo"): la NUBE arriba, la
    columna de fuego que baja de ella y el estallido en el suelo. Como cometa salio primero
    una oruga de bolas y luego una zanahoria."""
    cloud = M('ff_cloud', '#d8d4dc', sat=1.0)
    cloud_d = M('ff_cloud_d', '#9a94a4', sat=1.0)
    fire = M('ff', '#f06a1c')
    fire_l = M('ff_l', '#ffc83a')
    hot = M('ff_hot', '#fff4c8', sat=1.0)
    for i, (x, z, r, m) in enumerate(((-0.42, 0.62, 0.30, cloud_d), (0.40, 0.60, 0.28, cloud_d),
                                      (0.0, 0.70, 0.40, cloud), (-0.22, 0.86, 0.28, cloud),
                                      (0.24, 0.84, 0.26, cloud))):
        sphere(f'c{i}', (x, 0.1, z), r, m, scale=(1.2, 0.8, 0.8))
    # la columna: ancha abajo, zigzag leve (tres tramos) para que se lea como llama
    col = [(0.0, 0.0, 0.50), (0.10, 0.0, 0.12), (-0.06, 0.0, -0.25), (0.02, 0.0, -0.62)]
    rad = [0.10, 0.17, 0.24, 0.32]
    for i in range(3):
        tube(f'col{i}', col[i], col[i + 1], rad[i], rad[i + 1], fire, segs=24)
        tube(f'core{i}', (col[i][0], -0.12, col[i][2]), (col[i + 1][0], -0.12, col[i + 1][2]),
             rad[i] * 0.5, rad[i + 1] * 0.55, fire_l, segs=16)
    sphere('burst', (0.02, 0.0, -0.70), 0.42, fire, scale=(1.5, 1.0, 0.55))
    sphere('burst_l', (0.02, -0.18, -0.66), 0.26, fire_l, scale=(1.5, 1.0, 0.6))
    sphere('burst_h', (0.02, -0.30, -0.64), 0.12, hot, scale=(1.5, 1.0, 0.7))
    for i, (x, z) in enumerate(((-0.70, -0.55), (0.72, -0.50), (-0.52, -0.30), (0.55, -0.26))):
        box(f'lick{i}', (x, -0.05, z), (0.12, 0.12, 0.30), fire_l if i % 2 else fire, round=0.8,
            taper=(0.15, 0.2), rot=Matrix.Rotation(math.radians(-25 if x > 0 else 25), 3, 'Y'))

def carro():
    """22+3. Carro de Fuego (2 Reyes 2:11): el carro de oro y rojo con dos llamas por
    rueda, de perfil a tres cuartos (los caballos no caben a 16 unidades; van en el sprite
    del milagro, vfx `chariot.png`)."""
    hull = M('ch_hull', '#b83a18')
    trim = M('ch_trim', '#f8c050')
    flame = M('ch_flame', '#ffb030')
    flame_l = M('ch_flame_l', '#fff0a0', sat=1.0)
    for s in (-1, 1):
        tube(f'w{s}', (s * 0.50, 0.25, -0.18), (s * 0.62, 0.25, -0.18), 0.50, 0.50, trim, segs=24)
        sphere(f'hub{s}', (s * 0.66, 0.25, -0.18), 0.14, hull)
    box('hull', (0, 0.25, 0.22), (1.05, 1.10, 0.55), hull, round=0.5, taper=(0.7, 0.9))
    ob = box('peto', (0, -0.35, 0.42), (1.05, 0.18, 0.62), hull, round=0.5, taper=(0.85, 1.0))
    rot(ob, (0, -0.35, 0.15), 'X', -12.0)
    box('rib', (0, -0.42, 0.74), (1.08, 0.14, 0.12), trim, round=0.6)
    tube('pole', (0, -0.45, -0.05), (0, -1.10, 0.05), 0.06, 0.05, trim, segs=10)
    for i, (dx, dy, dz, hh) in enumerate(((-0.35, 0.72, 0.60, 0.55), (0.05, 0.78, 0.66, 0.70),
                                          (0.40, 0.72, 0.58, 0.50))):
        ob = box(f'fl{i}', (dx, dy, dz), (0.20, 0.20, hh), flame if i % 2 else flame_l, round=0.7, taper=(0.2, 0.3))
        rot(ob, (dx, dy, dz - hh * 0.5), 'X', 30.0)
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'Z', -55.0)


def altar_consumido():
    """22+4. Altar consumido (1 Reyes 18:38): las doce piedras ardiendo y la zanja de agua."""
    stone = M('ac_stone', '#c2b69f', sat=1.0)
    stone_d = M('ac_stone_d', '#948a76', sat=1.0)
    water = M('ac_water', '#3d9ad6')
    fire = M('ac_fire', '#f0701c')
    fire_l = M('ac_fire_l', '#ffd23a')
    tube('trench', (0, 0, -0.62), (0, 0, -0.52), 0.95, 0.95, water, segs=28)
    for row in range(3):
        z = -0.40 + row * 0.22
        half = 0.62 - row * 0.1
        for i in range(4):
            x = -half + (2 * half) * (i / 3.0)
            box(f'p{row}_{i}', (x, 0.0, z), (0.34 - row * 0.02, 0.40, 0.20),
                stone if (row + i) % 2 == 0 else stone_d, round=0.45)
    sphere('f0', (0, -0.02, 0.40), 0.42, fire, scale=(1.0, 0.8, 1.3))
    sphere('f1', (0.05, -0.18, 0.42), 0.26, fire_l, scale=(1.0, 0.8, 1.35))
    sphere('f2', (-0.2, 0.0, 0.82), 0.16, fire, scale=(1, 1, 1.5))
    sphere('f3', (0.22, 0.0, 0.76), 0.14, fire_l, scale=(1, 1, 1.5))


def sanar_aguas():
    """22+5. Sanar aguas (2 Reyes 2:21): la gota de agua clara con el destello de la sal."""
    water = M('sa_water', '#3aa6e8')
    water_l = M('sa_water_l', '#9ad8ff', sat=1.0)
    sparkle = M('sa_spark', '#ffffff', sat=1.0)
    sphere('drop', (0, 0, -0.20), 0.52, water)
    tube('tip', (0, 0, 0.10), (0, 0, 0.78), 0.45, 0.02, water, segs=24)
    sphere('shine', (-0.18, -0.34, -0.02), 0.14, water_l, scale=(0.8, 0.5, 1.4))
    for k in range(4):
        a = k * math.pi / 2
        box(f'sp{k}', (0.46 + math.cos(a) * 0.14, -0.3, 0.52 + math.sin(a) * 0.14), (0.26, 0.04, 0.06), sparkle,
            rot=Matrix.Rotation(-a, 3, 'Y'))
    tube('ripple', (0, 0, -0.74), (0, 0, -0.70), 0.72, 0.72, water_l, segs=28, caps=True)


def aceite_inagotable():
    """22+6. Aceite inagotable (2 Reyes 4:5): la vasija inclinada vertiendo un chorro de oro
    en otra — la que se llena y la que no se acaba. La de arriba lleva PICO: sin el era una
    bola parda flotando."""
    clay = M('ai_clay', '#9c5a36')
    clay_l = M('ai_clay_l', '#c07a4c')
    oil = M('ai_oil', '#f4c428')
    jar = [sphere('jar', (-0.35, 0, 0.32), 0.36, clay, scale=(1, 1, 0.9)),
           tube('neck', (-0.35, 0, 0.56), (-0.35, 0, 0.72), 0.15, 0.13, clay_l, segs=14),
           tube('lip', (-0.35, 0, 0.70), (-0.35, 0, 0.76), 0.19, 0.19, clay_l, segs=14),
           tube('handle', (-0.62, 0, 0.55), (-0.66, 0, 0.18), 0.05, 0.05, clay, segs=8)]
    for o in jar:
        rot(o, (-0.35, 0, 0.32), 'Y', 62.0)
    tube('stream', (0.16, -0.02, 0.36), (0.30, -0.02, -0.28), 0.07, 0.06, oil, segs=10)
    tube('pot', (0.30, 0, -0.78), (0.30, 0, -0.30), 0.22, 0.36, clay_l, segs=18, rim_round=0.03)
    tube('potband', (0.30, 0, -0.50), (0.30, 0, -0.44), 0.30, 0.31, clay, segs=18)
    sphere('potoil', (0.30, 0, -0.30), 0.32, oil, scale=(1, 1, 0.25))
    for i, (x, z) in enumerate(((0.56, -0.20), (0.06, -0.22))):
        sphere(f'splash{i}', (x, -0.05, z), 0.06, oil)

def ojos_abiertos():
    """22+7. Ojos abiertos (2 Reyes 6:17): un ojo abierto de iris dorado que irradia luz."""
    white = M('oa_white', '#f4efe2', sat=1.0)
    iris = M('oa_iris', '#e0a020')
    pupil = M('oa_pupil', '#20182a', sat=1.0)
    lid = M('oa_lid', '#c89878')
    ray = M('oa_ray', '#ffe070')
    sphere('eye', (0, 0, 0), 0.62, white, scale=(1.35, 0.45, 0.72))
    sphere('iris', (0, -0.20, 0), 0.30, iris, scale=(1.0, 0.35, 1.0))
    sphere('pupil', (0, -0.29, 0), 0.14, pupil, scale=(1.0, 0.3, 1.0))
    sphere('glint', (-0.09, -0.34, 0.09), 0.05, M('oa_glint', '#ffffff', sat=1.0))
    for s in (-1, 1):
        box(f'lid{s}', (0, -0.05, s * 0.42), (1.55, 0.30, 0.10), lid, round=0.9, taper=(0.55, 1.0) if s > 0 else (1.0, 1.0))
    for k in range(7):
        a = math.radians(20 + k * 23.3)
        box(f'r{k}', (math.cos(a) * 0.95, 0.05, math.sin(a) * 0.78), (0.24, 0.05, 0.06), ray,
            rot=Matrix.Rotation(-a, 3, 'Y'))


def ejercito():
    """22+8. Ejercito celestial (2 Reyes 6:17): escudo de luz con dos lanzas cruzadas y alas
    — "mas son los que estan con nosotros"."""
    shield = M('ej_shield', '#e8c050')
    boss = M('ej_boss', '#fff4c8', sat=1.0)
    spear = M('ej_spear', '#c0c8d8', sat=1.0)
    shaft = M('ej_shaft', '#8a6a44')
    wing = M('ej_wing', '#f4f0e6', sat=1.0)
    for s in (-1, 1):
        ob = tube(f'sp{s}', (s * -0.75, 0.2, -0.85), (s * 0.75, 0.2, 0.85), 0.04, 0.04, shaft, segs=8)
        tube(f'hd{s}', (s * 0.75, 0.2, 0.85), (s * 0.92, 0.2, 1.05), 0.10, 0.01, spear, segs=8)
        for k in range(3):
            w = box(f'w{s}{k}', (s * (0.72 + k * 0.05), 0.25, 0.10 - k * 0.18), (0.62 - k * 0.1, 0.06, 0.16), wing,
                    round=0.8, taper=(0.4, 1.0))
            rot(w, (s * 0.45, 0.25, 0.1 - k * 0.18), 'Y', s * (-20.0 + k * 18.0))
    tube('shield', (0, -0.05, 0), (0, -0.20, 0), 0.62, 0.58, shield, segs=32, rim_round=0.04)
    sphere('boss', (0, -0.24, 0), 0.22, boss, scale=(1, 0.5, 1))
    for k in range(4):
        a = k * math.pi / 4
        box(f'st{k}', (0, -0.22, 0), (0.95, 0.04, 0.08), M('ej_star', '#fff0a0', sat=1.0),
            rot=Matrix.Rotation(a, 3, 'Y'))


def doble_porcion():
    """22+9. Doble porcion (2 Reyes 2:9): una lampara de aceite de DOS mecheros con dos
    llamas ALTAS e iguales — la porcion del primogenito. Con llamas redondas y bajas era un
    cangrejo, y con conos pequenos, una barca."""
    clay = M('dp_clay', '#b0784a')
    clay_d = M('dp_clay_d', '#7e4c2c')
    fire = M('dp_fire', '#f07a22')
    fire_l = M('dp_fire_l', '#ffe070')
    sphere('lamp', (0, 0, -0.55), 0.50, clay, scale=(1.30, 0.85, 0.40))
    tube('foot', (0, 0, -0.80), (0, 0, -0.68), 0.22, 0.28, clay_d, segs=18)
    sphere('hole', (0, -0.05, -0.40), 0.14, clay_d, scale=(1, 1, 0.3))
    for s in (-1, 1):
        tube(f'nozzle{s}', (s * 0.40, 0, -0.52), (s * 0.66, -0.05, -0.42), 0.16, 0.11, clay, segs=14)
        sphere(f'fb{s}', (s * 0.66, -0.05, -0.20), 0.22, fire, scale=(1, 0.9, 1.1))
        box(f'f{s}', (s * 0.66, -0.05, 0.25), (0.42, 0.34, 0.95), fire, round=0.9, taper=(0.12, 0.15))
        box(f'fl{s}', (s * 0.66, -0.16, 0.12), (0.24, 0.20, 0.62), fire_l, round=0.9, taper=(0.12, 0.15))

# =============================================================================
# EL SANTUARIO (celda 32 + n): un icono PROPIO por mejora (2026-10-03)
# =============================================================================
# Hasta hoy El Santuario tomaba prestadas celdas de objetos y milagros, y el usuario lo vio
# a la primera: "Fuego Interior" era la antorcha de un objeto, "Fe Renovada" un cuenco de
# ceniza, "La Mesa de Eliseo" unas tortas sueltas y "La Sombra que Vuelve" una escalera.
# Regla: el icono dice el EFECTO con un simbolo de juego (corazon = vida, dados = otra
# tirada, monedas = ofrendas) y el NOMBRE lo viste con su pasaje; nunca la celda de un
# objeto o un milagro que el jugador ya conoce por otra cosa.

def _torus(name, center, major, minor, mat, tilt=0.0):
    """Aro en el plano XY girado `tilt` grados en X. Como el resto de piezas: vertices en
    coordenadas de mundo con el origen en 0, asi rot() funciona igual."""
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor,
                                     major_segments=72, minor_segments=18, location=(0, 0, 0))
    ob = bpy.context.object
    ob.name = name
    me = ob.data
    xf = Matrix.Translation(center) @ Matrix.Rotation(math.radians(tilt), 4, 'X')
    for v in me.vertices:
        v.co = xf @ v.co
    for p in me.polygons:
        p.use_smooth = True
    me.materials.append(mat)
    return ob


def _heart(name, center, size, mat, depth=0.42):
    """Corazon mullido: una esfera deformada por la curva clasica del corazon en el plano
    XZ (16 sin^3 t, 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t). Dos esferas y un cono dejaban
    un escalon donde el cono asomaba entre los lobulos."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=72, v_segments=48, radius=1.0)
    for v in bm.verts:
        x, y, z = v.co
        r = math.sqrt(x * x + z * z)
        t = math.atan2(x, z)
        hx = 16.0 * math.sin(t) ** 3 / 17.0
        hz = (13.0 * math.cos(t) - 5.0 * math.cos(2 * t) - 2.0 * math.cos(3 * t) - math.cos(4 * t)) / 17.0
        v.co = Vector((center[0] + hx * r * size, center[1] + y * depth * size,
                       center[2] + (hz * r + 0.16) * size))
    return parts._finish(name, bm, mat, smooth=True)


def _prism(name, poly, y0, y1, mat):
    """Poligono del plano XZ extruido entre y0 e y1 (un gnomon, una cuna de sombra)."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in poly]
    b = [bm.verts.new((x, y1, z)) for x, z in poly]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(poly)
    for i in range(n):
        bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    return parts._finish(name, bm, mat)


def _flat_poly(name, poly, z0, z1, mat, bevel=0.0):
    """Poligono del plano XY extruido entre z0 y z1 (una sombra tumbada, una suela).
    `bevel` redondea el canto de las dos tapas."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y, z0)) for x, y in poly]
    b = [bm.verts.new((x, y, z1)) for x, y in poly]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(poly)
    for i in range(n):
        f = bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
        f.smooth = True
    if bevel > 0.0:
        bm.edges.ensure_lookup_table()
        rim = [e for e in bm.edges if (e.verts[0].co.z == e.verts[1].co.z)]
        parts._bevel(bm, rim, bevel, segments=3)
    return parts._finish(name, bm, mat)


# Ejes de PANTALLA: la camara mira desde -Y bajando ELEV grados. Lo que tiene que verse de
# cara (destellos, una flecha de "atras") se construye en este plano.
_SR = Vector((1.0, 0.0, 0.0))
_SU = Vector((0.0, math.sin(math.radians(ELEV)), math.cos(math.radians(ELEV))))
_SF = Vector((0.0, math.cos(math.radians(ELEV)), -math.sin(math.radians(ELEV))))


def _sparkle(tag, center, size, mat):
    """Destello de cuatro puntas de cara a la camara: dos husos cruzados."""
    c = Vector(center)
    sphere(f'{tag}_h', c, size * 0.5, mat, scale=(1.0, 0.10, 0.14))
    ob = sphere(f'{tag}_v', c, size * 0.5, mat, scale=(0.14, 0.10, 1.0))
    rot(ob, c, 'X', -ELEV)


def _chain(tag, pts, r0, r1, mat, segs=12):
    """Polilinea de tubos con esferas en las juntas (sin costuras en los codos)."""
    n = len(pts) - 1
    for i in range(n):
        ra = r0 + (r1 - r0) * (i / n)
        rb = r0 + (r1 - r0) * ((i + 1) / n)
        tube(f'{tag}{i}', pts[i], pts[i + 1], ra, rb, mat, segs=segs)
        if i > 0:
            sphere(f'{tag}j{i}', pts[i], ra, mat)


def _flame(name, base, radius, height, mat, sway=0.0, phase=0.0, depth=0.85, lean=0.0):
    """Lengua de fuego: una esfera estirada en GOTA (redonda abajo, aguda arriba) cuya punta
    se cimbrea (`sway`) o se inclina (`lean`). Con cajas afiladas las llamas salian
    piramides y la hoguera parecia una corona."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=40, v_segments=28, radius=1.0)
    bx, by, bz = base
    for v in bm.verts:
        x, y, z = v.co
        s = (z + 1.0) * 0.5
        r = math.sqrt(x * x + y * y)
        ax, ay = (x / r, y / r) if r > 1e-6 else (0.0, 0.0)
        rho = radius * math.sin(math.pi * s ** 0.55)
        off = sway * math.sin(math.pi * 1.5 * s + phase) * s * s + lean * s
        v.co = Vector((bx + ax * rho + off, by + ay * rho * depth, bz + s * height))
    return parts._finish(name, bm, mat, smooth=True)


def corazon():
    """S0 Provision del Cuervo (+vida maxima): el CORAZON, que es lo que se lee como vida en
    cualquier juego, con el cuervo de 1 Reyes 17:6 posado encima y el pan en el pico. Solo
    el cuervo de perfil era el icono del MILAGRO Cuervos de Querit; una pluma suelta se
    leia como un cuchillo."""
    _heart('heart', (-0.10, 0.0, -0.16), 0.86, M('heart', '#d8263a'))
    before = set(bpy.data.objects)
    cuervos()
    for o in [o for o in bpy.data.objects if o not in before]:
        parts.scale_about(o, (0, 0, 0), 0.42)
        for v in o.data.vertices:
            v.co += Vector((0.52, -0.08, 0.80))


def hoguera():
    """S1 Fuego Interior (+dano): una HOGUERA — lenos en tienda, corona de piedras y una
    llama alta de tres lenguas con chispas. La antorcha del objeto 10 contaba otra cosa."""
    stone = M('hg_stone', '#8a8478', sat=1.0)
    stone_l = M('hg_stone_l', '#ada596', sat=1.0)
    wood = M('hg_log', '#6a4224')
    wood_end = M('hg_log_end', '#d6aa6c', sat=1.0)
    fire = M('hg_fire', '#f2641a')
    fire_m = M('hg_fire_m', '#ff9a2a')
    fire_l = M('hg_fire_l', '#ffd84a')
    hot = M('hg_fire_hot', '#fff6d0', sat=1.0)
    gz = -0.70
    for k in range(11):
        a = k * 2 * math.pi / 11 + 0.2
        parts.rock(f'st{k}', (math.cos(a) * 0.86, math.sin(a) * 0.56, gz + 0.04), 0.17,
                   stone if k % 2 else stone_l, scale=(1.0, 0.9, 0.7), seed=k + 5, rough=0.12)
    top = Vector((0.0, 0.04, -0.10))
    for k, deg in enumerate((205.0, -25.0, 95.0)):
        a = math.radians(deg)
        b = Vector((math.cos(a) * 0.66, math.sin(a) * 0.44, gz + 0.06))
        d = (top - b).normalized()
        tube(f'log{k}', b, top, 0.11, 0.08, wood, segs=14)
        tube(f'end{k}', b - d * 0.015, b + d * 0.02, 0.095, 0.095, wood_end, segs=14)
    l0, l1 = Vector((-0.62, -0.46, gz + 0.13)), Vector((0.56, -0.30, gz + 0.13))
    tube('logf', l0, l1, 0.11, 0.11, wood, segs=14)
    d = (l1 - l0).normalized()
    for tag, p, s in (('ef0', l0, -1), ('ef1', l1, 1)):
        tube(tag, p - d * 0.015 * s, p + d * 0.02 * s, 0.095, 0.095, wood_end, segs=14)
    _flame('fl_main', (0.0, 0.02, -0.66), 0.50, 1.80, fire, sway=0.14, depth=0.60)
    _flame('fl_left', (-0.34, 0.06, -0.60), 0.30, 1.10, fire_m, sway=0.06, lean=-0.26, depth=0.70)
    _flame('fl_right', (0.32, 0.06, -0.60), 0.30, 1.22, fire_m, sway=-0.07, phase=0.8, lean=0.22,
           depth=0.70)
    _flame('fl_in', (0.02, -0.22, -0.62), 0.31, 1.18, fire_l, sway=0.10, phase=0.5, depth=0.80)
    _flame('fl_hot', (0.02, -0.40, -0.58), 0.16, 0.56, hot, depth=0.8)
    for i, (x, z, r) in enumerate(((0.44, 0.80, 0.055), (-0.40, 0.98, 0.05), (0.14, 1.22, 0.045),
                                   (-0.14, 0.72, 0.04))):
        sphere(f'spark{i}', (x, -0.12, z), r, fire_l)


def _sole_poly(a=0.66, n=56):
    """Silueta de una suela: elipse cuya media anchura crece del talon (0,17) a la planta
    (0,24), con el arco un poco mas estrecho. Una caja redondeada se leia como una venda."""
    pts = []
    for k in range(n):
        th = 2 * math.pi * k / n
        x = a * math.cos(th)
        u = (x + a) / (2 * a)
        b = 0.17 + 0.075 * max(0.0, min(1.0, (u - 0.35) / 0.4)) - 0.02 * math.exp(-((u - 0.42) / 0.12) ** 2)
        pts.append((x, b * math.sin(th)))
    return pts


def sandalia_veloz():
    """S2 Sandalias Rapidas (+velocidad): UNA sandalia en zancada, la punta arriba, con tres
    estelas de velocidad y el polvo del talon. El par quieto es el objeto 4."""
    sole = M('sv_sole', '#7e4e28')
    sole_t = M('sv_sole_t', '#c89058')
    strap = M('sv_strap', '#5a3418')
    streak = M('sv_streak', '#f6eed8', sat=1.0)
    dust = M('sv_dust', '#d8c4a2', sat=1.0)
    poly = _sole_poly()
    sandal = [_flat_poly('sole', poly, 0.0, 0.10, sole, bevel=0.03),
              _flat_poly('insole', [(x * 0.93, y * 0.86) for x, y in poly], 0.10, 0.13, sole_t, bevel=0.012)]
    for k, ang in enumerate((34.0, -34.0)):
        sandal.append(box(f'x{k}', (0.16, 0, 0.15), (0.09, 0.50, 0.05), strap, round=0.6,
                          rot=Matrix.Rotation(math.radians(ang), 3, 'Z')))
    sandal.append(box('toe', (0.44, 0.04, 0.15), (0.13, 0.20, 0.05), strap, round=0.6))
    heel = [(-0.40, -0.17, 0.12), (-0.43, -0.15, 0.27), (-0.45, 0.0, 0.36), (-0.43, 0.15, 0.27),
            (-0.40, 0.17, 0.12)]
    for i in range(len(heel) - 1):
        sandal.append(tube(f'heel{i}', heel[i], heel[i + 1], 0.04, 0.04, strap, segs=10))
        if i:
            sandal.append(sphere(f'heelj{i}', heel[i], 0.04, strap))
    for sy in (-1, 1):
        sandal.append(tube(f'tie{sy}', (-0.43, sy * 0.14, 0.26), (0.10, sy * 0.10, 0.17), 0.03, 0.03,
                           strap, segs=8))
    for o in sandal:
        rot(o, (0, 0, 0), 'X', 62.0)
        rot(o, (0, 0, 0), 'Y', -30.0)
        parts.scale_about(o, (0, 0, 0), 1.45)
        for v in o.data.vertices:
            v.co.x += 0.30
            v.co.z += 0.06
    for i, (z, x0, x1, r) in enumerate(((0.12, -0.62, -1.18, 0.060), (-0.12, -0.70, -1.38, 0.065),
                                        (-0.36, -0.60, -1.10, 0.055))):
        tube(f'streak{i}', (x0, 0.30, z), (x1, 0.30, z), r, 0.012, streak, segs=12)
    for i, (x, z, r) in enumerate(((-0.52, -0.62, 0.14), (-0.74, -0.70, 0.10), (-0.32, -0.74, 0.09))):
        sphere(f'dust{i}', (x, -0.10, z), r, dust, scale=(1.3, 1.0, 0.85))


def halo():
    """S3 Fe Renovada (+fe al empezar): un HALO de luz — el aro dorado inclinado, tres
    destellos y el resplandor que le pone el post (`POST_GLOW`). Con rayos alrededor salia
    un sol; con un toro translucido de aura, un neumatico gris (EEVEE no le daba alfa). El
    cuenco de ceniza del objeto 12 no decia fe."""
    ring = M('halo', '#ffd04a')
    ring_l = M('halo_core', '#fff6cc', sat=1.0)
    spark = M('halo_spark', '#ffffff', sat=1.0)
    _torus('ring', (0, 0, 0.0), 0.74, 0.13, ring, tilt=14.0)
    _torus('ring_l', (0, -0.02, 0.085), 0.74, 0.05, ring_l, tilt=14.0)
    _sparkle('sp0', _SR * 0.92 + _SU * 0.66 - _SF * 0.4, 0.46, spark)
    _sparkle('sp1', _SR * -0.95 + _SU * -0.52 - _SF * 0.4, 0.34, spark)
    _sparkle('sp2', _SR * -0.30 + _SU * 0.78 - _SF * 0.4, 0.24, spark)


def aliento():
    """S4 Aliento de Vida (levantarse al caer): el corazon que vuelve a latir envuelto en
    el SOPLO, una espiral de aire que sube (1 Reyes 17:22). El de la Provision es rojo y
    quieto; este, rosado y con el aire alrededor."""
    heart = M('al_heart', '#f25a7a')
    wind = M('al_wind', '#f2f8ff', sat=1.0)
    wind_b = M('al_wind_b', '#a8d4ff', sat=1.0)
    _heart('heart', (0.0, 0.0, -0.02), 0.56, heart)
    for tag, mat, ph, r0, z0, z1, turns in (('w', wind, -2.4, 0.64, -0.46, 0.92, 1.25),
                                             ('v', wind_b, 0.9, 0.62, -0.50, 0.60, 0.95)):
        pts = []
        n = 34
        for i in range(n + 1):
            t = i / n
            a = ph + t * 2 * math.pi * turns
            R = r0 * (1.0 - 0.38 * t)
            pts.append((R * math.cos(a), R * math.sin(a) * 0.80, z0 + (z1 - z0) * t))
        _chain(tag, pts, 0.062, 0.014, mat)
    _sparkle('sp0', (0.62, -0.30, 0.86), 0.30, M('al_spark', '#fff6c8', sat=1.0))


def primicias():
    """S5 Primicias (carta de objeto al empezar): la GAVILLA de las primicias atada con su
    cordon, y una granada y un racimo al pie (Deuteronomio 26:2). La hogaza del objeto 2
    era el Pan de Cada Dia."""
    stalk = M('pr_stalk', '#d8b850')
    stalk_d = M('pr_stalk_d', '#b08a34')
    ear = M('pr_ear', '#f0c454')
    cord = M('pr_cord', '#c03a2a')
    pom = M('pr_pom', '#c42838')
    pom_c = M('pr_pom_c', '#8a1a24')
    grape = M('pr_grape', '#6a3a9a')
    leaf = M('pr_leaf', '#5fae3e')
    tz = -0.22
    N = 9
    for i in range(N):
        u = (i - (N - 1) / 2) / ((N - 1) / 2)
        yy = (i % 3 - 1) * 0.06
        base = Vector((u * 0.30, yy, -0.88))
        tie = Vector((u * 0.07, yy, tz))
        top = Vector((u * 0.66, yy + 0.04, 0.50 - abs(u) * 0.18))
        m = stalk if i % 2 else stalk_d
        tube(f'sb{i}', base, tie, 0.035, 0.03, m, segs=8)
        tube(f'st{i}', tie, top, 0.03, 0.022, m, segs=8)
        d = (top - tie).normalized()
        tube(f'ear{i}', top - d * 0.04, top + d * 0.40, 0.08, 0.03, ear, segs=12)
    tube('cord', (0, 0, tz - 0.07), (0, 0, tz + 0.07), 0.15, 0.15, cord, segs=20, rim_round=0.02)
    for s in (-1, 1):
        sphere(f'bow{s}', (s * 0.10, -0.17, tz), 0.07, cord, scale=(1.3, 0.6, 0.8))
    sphere('pom', (0.44, -0.36, -0.70), 0.26, pom)
    tube('pom_c', (0.44, -0.36, -0.47), (0.44, -0.36, -0.38), 0.06, 0.09, pom_c, segs=10)
    sphere('leaf', (0.20, -0.30, -0.56), 0.12, leaf, scale=(1.5, 0.5, 0.6))
    for i, (x, z) in enumerate(((-0.46, -0.58), (-0.58, -0.62), (-0.34, -0.63), (-0.52, -0.74),
                                (-0.40, -0.75), (-0.46, -0.86))):
        sphere(f'gr{i}', (x, -0.36, z), 0.085, grape)


def suertes():
    """S6 Echar Suertes (otras tres cartas): dos DADOS de hueso rodando. El dado es el
    simbolo universal de "otra tirada"; las suertes de Israel eran piedras marcadas y a este
    tamano no se distinguen de un guijarro (que es lo que salia con el memorial)."""
    bone = M('ds_bone', '#f2e6c8', sat=1.0)
    pip = M('ds_pip', '#3a2216', sat=1.0)
    pip_r = M('ds_pip_r', '#c8283a')
    PIPS = {1: ((0, 0),), 2: ((-1, -1), (1, 1)), 3: ((-1, -1), (0, 0), (1, 1)),
            4: ((-1, -1), (-1, 1), (1, -1), (1, 1)), 5: ((-1, -1), (-1, 1), (0, 0), (1, -1), (1, 1)),
            6: ((-1, -1), (-1, 0), (-1, 1), (1, -1), (1, 0), (1, 1))}
    FACES = ((2, 1, 1), (2, -1, 6), (1, -1, 2), (1, 1, 5), (0, -1, 3), (0, 1, 4))

    def die(tag, c, s, rots):
        objs = [box(tag, c, (s, s, s), bone, round=0.42)]
        h = s * 0.5
        for axis, sign, n in FACES:
            other = [i for i in range(3) if i != axis]
            for k, (a, b) in enumerate(PIPS[n]):
                p = list(c)
                p[axis] += sign * h
                p[other[0]] += a * s * 0.26
                p[other[1]] += b * s * 0.26
                sc = [1.0, 1.0, 1.0]
                sc[axis] = 0.30
                objs.append(sphere(f'{tag}p{axis}{sign}{k}', tuple(p), s * 0.09,
                                   pip_r if n == 1 else pip, scale=tuple(sc)))
        for ax, deg in rots:
            for o in objs:
                rot(o, c, ax, deg)

    die('da', (-0.40, 0.08, -0.36), 0.74, (('Z', 28.0),))
    die('db', (0.44, -0.16, 0.22), 0.62, (('X', 32.0), ('Y', -24.0), ('Z', -38.0)))


def alforja():
    """S7 Alforja Ancha (+huecos): la ALFORJA de cuero con su solapa, la hebilla, la correa
    en arco y una redoma asomando — se lee "para llevar cosas". El cinto del objeto 19 era
    un cinturon."""
    leather = M('af_leather', '#a06a3a')
    leather_d = M('af_leather_d', '#724626')
    strap = M('af_strap', '#5a3418')
    buckle = M('af_buckle', '#ecc454')
    clay = M('af_clay', '#c07a4c')
    cork = M('af_cork', '#d8b886', sat=1.0)
    box('body', (0, 0, -0.30), (1.36, 0.64, 0.98), leather, round=0.55)
    box('flap_top', (0, 0.0, 0.18), (1.40, 0.68, 0.12), leather_d, round=0.6)
    box('flap', (0, -0.33, -0.04), (1.34, 0.10, 0.50), leather_d, round=0.75)
    box('tongue', (0, -0.37, -0.40), (0.18, 0.05, 0.40), strap, round=0.5)
    box('buckle', (0, -0.41, -0.26), (0.26, 0.06, 0.20), buckle, round=0.35)
    box('buckle_in', (0, -0.44, -0.26), (0.13, 0.04, 0.09), strap, round=0.3)
    pts = [(-0.60 + 1.20 * t, 0.06, 0.20 + 0.80 * math.sin(math.pi * t)) for t in np.linspace(0, 1, 17)]
    _chain('strap', pts, 0.055, 0.055, strap, segs=10)
    tube('flask', (-0.36, 0.14, 0.16), (-0.38, 0.14, 0.50), 0.16, 0.10, clay, segs=16)
    tube('flask_lip', (-0.38, 0.14, 0.48), (-0.38, 0.14, 0.54), 0.12, 0.12, clay, segs=16)
    tube('flask_cork', (-0.38, 0.14, 0.52), (-0.38, 0.14, 0.64), 0.08, 0.09, cork, segs=12)


def _jar_r(z):
    """Perfil de la tinaja de Sarepta: radio a la altura z (lo usa el agua que resbala)."""
    prof = ((-0.72, 0.28), (-0.15, 0.52), (0.30, 0.30), (0.40, 0.26), (0.50, 0.34))
    if z <= prof[0][0]:
        return prof[0][1]
    for (z0, r0), (z1, r1) in zip(prof, prof[1:]):
        if z <= z1:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return prof[-1][1]


def tinaja_sarepta():
    """S8 La Tinaja de Sarepta (Agua del Jordan al empezar): la tinaja que NO se agota,
    rebosando agua por el borde y con un destello (1 Reyes 17:16). La de harina del objeto
    13 y el cantaro del objeto 9 estan quietos; esta se desborda."""
    clay = M('ts_clay', '#c47a48')
    clay_d = M('ts_clay_d', '#8e5230')
    water = M('ts_water', '#3c9ae0')
    water_l = M('ts_water_l', '#9ad8ff', sat=1.0)
    tube('base', (0, 0, -0.86), (0, 0, -0.70), 0.22, 0.28, clay_d, segs=24)
    tube('lo', (0, 0, -0.72), (0, 0, -0.15), 0.28, 0.52, clay, segs=32, rim_round=0.02)
    tube('hi', (0, 0, -0.15), (0, 0, 0.30), 0.52, 0.30, clay, segs=32)
    tube('band', (0, 0, -0.06), (0, 0, 0.02), 0.515, 0.50, clay_d, segs=32)
    tube('neck', (0, 0, 0.28), (0, 0, 0.42), 0.27, 0.26, clay_d, segs=24)
    tube('rim', (0, 0, 0.42), (0, 0, 0.52), 0.35, 0.35, clay, segs=28, rim_round=0.03)
    sphere('surf', (0, 0, 0.53), 0.34, water, scale=(1.0, 1.0, 0.12))
    for s in (-1, 1):
        _chain(f'h{s}', [(s * 0.46, 0, 0.12), (s * 0.64, 0, 0.22), (s * 0.60, 0, 0.40),
                         (s * 0.34, 0, 0.46)], 0.05, 0.05, clay_d, segs=10)
    # Llena hasta el borde y UN chorro que rebosa por delante. Tres chorros eran tentaculos y
    # las gotas saltando, burbujas.
    sphere('brim', (0, 0, 0.535), 0.35, water, scale=(1.0, 1.0, 0.10))
    sphere('shine', (-0.10, -0.12, 0.565), 0.12, water_l, scale=(1.6, 0.8, 0.12))
    phi = -0.18
    pts = []
    for z in np.linspace(0.55, 0.06, 9):
        r = _jar_r(min(z, 0.50)) + 0.045
        pts.append((r * math.sin(phi), -r * math.cos(phi), z))
    _chain('run', pts, 0.085, 0.06, water, segs=12)
    x, y, z = pts[-1]
    sphere('drip', (x, y - 0.01, z - 0.10), 0.075, water, scale=(1, 1, 1.45))
    sphere('drip2', (x + 0.02, y - 0.03, z - 0.34), 0.055, water_l, scale=(1, 1, 1.4))
    _sparkle('sp0', (0.56, -0.30, 1.00), 0.34, M('ts_spark', '#ffffff', sat=1.0))


def diezmo():
    """S9 Diezmo (+ofrendas): la BOLSA de las ofrendas abierta y las monedas de oro —las del
    contador del HUD— derramandose delante. El Arca del objeto 7 no decia nada de esto."""
    bag = M('dz_bag', '#9a6a3c')
    bag_d = M('dz_bag_d', '#6e4628')
    cord = M('dz_cord', '#d8b860')
    coin = M('dz_coin', '#f4c43a')
    coin_e = M('dz_coin_e', '#d0901c')
    sphere('bag', (-0.18, 0.12, -0.26), 0.56, bag, scale=(1.0, 0.9, 0.95))
    tube('neck', (-0.18, 0.12, 0.16), (-0.18, 0.12, 0.36), 0.25, 0.21, bag_d, segs=24)
    tube('mouth', (-0.18, 0.12, 0.34), (-0.18, 0.12, 0.56), 0.21, 0.34, bag, segs=24, rim_round=0.02)
    tube('tie', (-0.18, 0.12, 0.22), (-0.18, 0.12, 0.31), 0.27, 0.27, cord, segs=24)
    for i, (dx, dz, tx) in enumerate(((-0.08, 0.58, 10.0), (0.10, 0.62, -14.0), (0.0, 0.66, 4.0))):
        c = Vector((-0.18 + dx, 0.12, dz))
        ob = tube(f'peek{i}', c - Vector((0, 0, 0.03)), c + Vector((0, 0, 0.03)), 0.15, 0.15, coin,
                  segs=24, rim_round=0.01)
        rot(ob, c, 'Y', tx)
        rot(ob, c, 'X', -20.0)
    for i in range(5):
        x = 0.46 + (0.03 if i % 2 else -0.02)
        z0 = -0.86 + i * 0.105
        tube(f'stk{i}', (x, -0.22, z0), (x, -0.22, z0 + 0.085), 0.25, 0.25, coin if i % 2 == 0 else coin_e,
             segs=28, rim_round=0.012)
    tube('stk_face', (0.44, -0.22, -0.36), (0.44, -0.22, -0.34), 0.15, 0.15, coin_e, segs=24)
    for i, (x, y, z, ax) in enumerate(((-0.62, -0.46, -0.74, (0.35, -0.85, 0.40)),
                                       (0.02, -0.62, -0.84, (0.0, -0.30, 1.0)),
                                       (-0.20, -0.66, -0.66, (-0.50, -0.70, 0.50)))):
        c = Vector((x, y, z))
        d = Vector(ax).normalized()
        tube(f'loose{i}', c - d * 0.04, c + d * 0.04, 0.22, 0.22, coin, segs=28, rim_round=0.012)


def altar_portatil():
    """S10 Altar Portatil (orar sin altar): el altar de cuernos con sus DOS VARAS para
    llevarlo a hombros (Exodo 27:7) y la llama encima. Las doce piedras del milagro Altar
    Consumido no se mueven de sitio."""
    wood = M('ap_wood', '#6e4a2e')
    wood_d = M('ap_wood_d', '#4e321e')
    brass = M('ap_bronze', '#c8823a')
    pole = M('ap_pole', '#9a6a3e')
    fire_m = M('ap_fire_m', '#ff9a2a')
    hot = M('ap_fire_hot', '#fff6d0', sat=1.0)
    fire = M('ap_fire', '#f2701c')
    fire_l = M('ap_fire_l', '#ffd84a')
    box('body', (0, 0, -0.30), (0.96, 0.80, 0.86), wood, round=0.22)
    box('band', (0, 0, -0.30), (1.01, 0.85, 0.14), brass, round=0.4)
    box('top', (0, 0, 0.16), (1.06, 0.90, 0.10), brass, round=0.4)
    box('foot', (0, 0, -0.76), (1.06, 0.90, 0.10), wood_d, round=0.4)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(f'horn{sx}{sy}', (sx * 0.45, sy * 0.37, 0.36), (0.20, 0.20, 0.36), brass, round=0.8,
                taper=(0.30, 0.30))
    for sy in (-1, 1):
        tube(f'pole{sy}', (-1.05, sy * 0.48, -0.30), (1.05, sy * 0.48, -0.30), 0.055, 0.055, pole, segs=12)
        for sx in (-1, 1):
            box(f'ring{sx}{sy}', (sx * 0.30, sy * 0.45, -0.30), (0.10, 0.08, 0.20), brass, round=0.5)
    _flame('fl', (0.0, 0.02, 0.14), 0.30, 0.92, fire, sway=0.08, depth=0.6)
    _flame('fl_s', (-0.18, 0.05, 0.15), 0.17, 0.56, fire_m, lean=-0.14, depth=0.7)
    _flame('fl_s2', (0.18, 0.05, 0.15), 0.17, 0.62, fire_m, lean=0.12, sway=-0.04, depth=0.7)
    _flame('fl_in', (0.0, -0.13, 0.14), 0.18, 0.62, fire_l, sway=0.06, phase=0.6, depth=0.8)
    _flame('fl_hot', (0.0, -0.24, 0.15), 0.09, 0.28, hot, depth=0.8)


def _grid_surface(name, fn, nu, nv, mat):
    """Superficie (u, v) -> punto, en UNA malla de cuadrilateros suavizados."""
    bm = bmesh.new()
    vs = [[bm.verts.new(fn(i / (nu - 1), j / (nv - 1))) for i in range(nu)] for j in range(nv)]
    for j in range(nv - 1):
        for i in range(nu - 1):
            f = bm.faces.new((vs[j][i], vs[j][i + 1], vs[j + 1][i + 1], vs[j + 1][i]))
            f.smooth = True
    return parts._finish(name, bm, mat)


def _cape(u, v, inset=0.0):
    """La capa vista DE ESPALDAS: media vuelta alrededor del cuello (u) y caida (v). Cuello
    estrecho, hombros que bajan en seguida, costados casi rectos y el bajo ondulado. Un cono
    entero era una pantalla de lampara; esto es la silueta de capa de cualquier juego."""
    th = math.pi * u
    R = 0.13 + 0.36 * min(1.0, v / 0.09) ** 0.6 + 0.24 * v
    R *= 1.0 + 0.10 * v * math.sin(7.0 * th)
    R -= inset
    z = 0.80 - 1.75 * v + 0.07 * v * math.sin(5.0 * th + 0.7)
    return (R * math.cos(th), -R * 0.55 * math.sin(th), z)


def herencia():
    """S11 Herencia Ampliada (Eliseo hereda mas fe): el MANTO de pelo de Elias (2 Reyes
    1:8) como capa —cuello, capucha caida, pliegues y orillo de oro— envuelto en la luz de
    la fe que pasa con el (2 Reyes 2:13-15). Como tela suelta y plana se leia como una
    alfombra (magica, con destellos) o un felpudo; pardo, para no ser el objeto 15."""
    cloth = M('hr_cloth', '#8a5230')
    cloth_d = M('hr_cloth_d', '#4e2a14')
    gold = M('hr_gold', '#f0c850')
    _grid_surface('cape', lambda u, v: _cape(u, v), 49, 30, cloth)
    _grid_surface('lining', lambda u, v: _cape(u, v, inset=0.035), 49, 30, cloth_d)
    hem = [_cape(u, 1.0) for u in np.linspace(0.0, 1.0, 26)]
    _chain('hem', hem, 0.045, 0.045, gold, segs=8)
    for s_ in (0.0, 1.0):
        edge = [_cape(s_, v) for v in np.linspace(0.0, 1.0, 14)]
        _chain(f'edge{int(s_)}', edge, 0.04, 0.04, gold, segs=8)
    _chain('collar', [_cape(u, 0.0) for u in np.linspace(0.0, 1.0, 14)], 0.06, 0.06, gold, segs=10)
    for s_ in (-1, 1):
        sphere(f'clasp{s_}', (s_ * 0.16, 0.0, 0.80), 0.085, gold)
    # la capucha caida sobre la espalda: una gota con la punta hacia abajo
    hood = _flame('hood', (0.0, -0.30, 0.30), 0.30, 0.60, cloth_d, depth=0.35)
    rot(hood, (0.0, -0.30, 0.60), 'Y', 180.0)
    rim = _flame('hood_in', (0.0, -0.36, 0.40), 0.20, 0.42, cloth, depth=0.25)
    rot(rim, (0.0, -0.36, 0.61), 'Y', 180.0)
    for i, u in enumerate(np.linspace(0.06, 0.94, 11)):
        x, y, z = _cape(u, 1.0)
        sphere(f'tuft{i}', (x, y - 0.02, z - 0.07), 0.055, cloth_d, scale=(0.8, 0.8, 1.6))
    # de ESPALDAS a la camara: -35 en X le pone la espalda de cara (con la camara a 35
    # grados se veia el hueco del cuello y salia un cubo de papel)
    for o in list(bpy.data.objects):
        if o.type == 'MESH':
            rot(o, (0, 0, 0), 'X', -30.0)
            rot(o, (0, 0, 0), 'Y', 10.0)


def lamparas():
    """S12 Doble Porcion Mejorada (tres huecos de milagro): la lampara del milagro Doble
    Porcion, pero de TRES mecheros y con el borde de oro — tres llamas, tres milagros."""
    clay = M('dl_clay', '#c08a58')
    clay_d = M('dl_clay_d', '#865230')
    gold = M('dl_gold', '#f0c450')
    fire = M('dl_fire', '#f07a22')
    fire_l = M('dl_fire_l', '#ffe070')
    sphere('lamp', (0, 0.05, -0.58), 0.52, clay, scale=(1.45, 0.95, 0.40))
    tube('foot', (0, 0.05, -0.84), (0, 0.05, -0.72), 0.24, 0.30, clay_d, segs=20)
    tube('ring', (0, 0.05, -0.42), (0, 0.05, -0.38), 0.20, 0.20, gold, segs=24, rim_round=0.01)
    sphere('hole', (0, 0.03, -0.40), 0.14, clay_d, scale=(1, 1, 0.3))
    nozzles = ((-1, 0.0), (1, 0.0), (0, 1))
    for i, (sx, back) in enumerate(nozzles):
        if back:
            n0, n1 = (0.0, 0.40, -0.52), (0.0, 0.66, -0.42)
            fx, fy, h = 0.0, 0.66, 1.10
        else:
            n0, n1 = (sx * 0.52, 0.05, -0.54), (sx * 0.84, 0.0, -0.44)
            fx, fy, h = sx * 0.84, 0.0, 0.90
        tube(f'nozzle{i}', n0, n1, 0.16, 0.11, clay, segs=14)
        _flame(f'f{i}', (fx, fy - 0.02, -0.46), 0.22, h, fire, sway=0.06 * (1 if i % 2 else -1),
               depth=0.6)
        _flame(f'fl{i}', (fx, fy - 0.16, -0.44), 0.13, h * 0.62, fire_l, depth=0.8)


def escuela():
    """S13 La Escuela de Profetas (un discipulo junto a Eliseo): el ROLLO abierto de la
    escuela con dos cayados cruzados detras — el del maestro y el del discipulo (2 Reyes
    6:1). El escudo con lanzas era el milagro Ejercito Celestial."""
    parch = M('es_parch', '#f2e4be', sat=1.0)
    roll = M('es_roll', '#dcc896', sat=1.0)
    rod = M('es_rod', '#7a4a28')
    gold = M('es_gold', '#e8b840')
    ink = M('es_ink', '#4a3424', sat=1.0)
    staff = M('es_staff', '#8a5a32')
    staff_l = M('es_staff_l', '#a8784a')
    scroll = []
    for s in (-1, 1):
        x = s * 0.64
        scroll.append(tube(f'roll{s}', (x, 0, -0.44), (x, 0, 0.44), 0.14, 0.14, roll, segs=20, rim_round=0.02))
        scroll.append(tube(f'rod{s}', (x, 0, -0.66), (x, 0, 0.66), 0.045, 0.045, rod, segs=10))
        scroll.append(sphere(f'k{s}a', (x, 0, 0.70), 0.075, gold))
        scroll.append(sphere(f'k{s}b', (x, 0, -0.70), 0.075, gold))
    nx, ny = 30, 20
    xs = np.linspace(-0.62, 0.62, nx)
    ys = np.linspace(-0.40, 0.40, ny)
    X, Y = np.meshgrid(xs, ys)
    Z = 0.035 * np.cos(np.pi * X / 1.24)
    sheet = parts.heightfield('sheet', xs, ys, Z, parch)
    rot(sheet, (0, 0, 0), 'X', 90.0)
    scroll.append(sheet)
    for col, cx in enumerate((-0.27, 0.27)):
        for k in range(6):
            w = (0.40, 0.36, 0.40, 0.30, 0.40, 0.22)[(k + col * 2) % 6]
            scroll.append(box(f'ln{col}{k}', (cx - (0.40 - w) * 0.5, -0.045, 0.29 - k * 0.115),
                              (w, 0.02, 0.035), ink))
    for o in scroll:
        rot(o, (0, 0, 0), 'X', -24.0)
    _chain('stA', [(-0.95, 0.42, -0.95), (0.70, 0.42, 0.88)], 0.055, 0.05, staff)
    crook = [(0.70, 0.42, 0.88)]
    for k in range(1, 8):
        a = math.radians(225.0 - k * 30.0)
        crook.append((0.70 + 0.16 + 0.16 * math.cos(a) + 0.11, 0.42, 0.88 + 0.16 * math.sin(a) + 0.11))
    _chain('crook', crook, 0.05, 0.045, staff)
    _chain('stB', [(0.92, 0.50, -0.88), (-0.66, 0.50, 0.70)], 0.045, 0.04, staff_l)


def mesa():
    """S14 La Mesa de Eliseo (los Panes de Cebada en las cartas): una MESA de madera de
    verdad —tablero, faldon y cuatro patas— con los panes de cebada y el cantaro encima
    (2 Reyes 4:10 y 4:42). Las tortas sueltas del objeto 21 no se leian como mesa."""
    wood = M('ms_wood', '#b07840')
    wood_d = M('ms_wood_d', '#7a4e28')
    groove = M('ms_groove', '#5e3a1e')
    loaf = M('ms_loaf', '#dca45a')
    loaf_d = M('ms_loaf_d', '#b07a3a')
    cut = M('ms_cut', '#f4dcaa', sat=1.0)
    clay = M('ms_clay', '#bc6c3c')
    clay_d = M('ms_clay_d', '#8a4a28')
    box('top', (0, 0, 0.0), (1.80, 1.00, 0.13), wood, round=0.3)
    for k, y in enumerate((-0.17, 0.17)):
        box(f'gr{k}', (0, y, 0.066), (1.74, 0.022, 0.012), groove)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(f'leg{sx}{sy}', (sx * 0.76, sy * 0.38, -0.50), (0.13, 0.13, 0.88), wood_d, round=0.3)
    for sy in (-1, 1):
        box(f'apron{sy}', (0, sy * 0.40, -0.13), (1.46, 0.06, 0.15), wood_d, round=0.3)
    for sx in (-1, 1):
        box(f'side{sx}', (sx * 0.78, 0, -0.13), (0.06, 0.74, 0.15), wood_d, round=0.3)
    for i, (x, y, z, r) in enumerate(((-0.46, -0.12, 0.13, 0.27), (-0.12, 0.16, 0.13, 0.25),
                                      (-0.44, -0.10, 0.25, 0.23))):
        sphere(f'loaf{i}', (x, y, z), r, loaf if i != 1 else loaf_d, scale=(1.0, 0.92, 0.40))
        box(f'cut{i}', (x, y - 0.02, z + r * 0.38), (r * 1.1, 0.035, 0.02), cut, round=0.6)
    tube('jug_lo', (0.48, 0.10, 0.06), (0.48, 0.10, 0.32), 0.16, 0.22, clay, segs=20, rim_round=0.02)
    tube('jug_hi', (0.48, 0.10, 0.32), (0.48, 0.10, 0.52), 0.22, 0.11, clay, segs=20)
    tube('jug_neck', (0.48, 0.10, 0.50), (0.48, 0.10, 0.66), 0.09, 0.12, clay_d, segs=16)
    _chain('jug_h', [(0.66, 0.10, 0.48), (0.78, 0.10, 0.40), (0.74, 0.10, 0.22), (0.64, 0.10, 0.18)],
           0.035, 0.035, clay_d, segs=8)


def sombra():
    """S15 La Sombra que Vuelve (Reloj de Acaz en las cartas): un cuadrante solar de piedra
    con su gnomon, la SOMBRA larga y oscura sobre las horas y la flecha de oro que la hace
    VOLVER atras (2 Reyes 20:11). La escalinata del objeto 20 se leia como escalera."""
    stone = M('sb_stone', '#d2c4a2', sat=1.0)
    stone_d = M('sb_stone_d', '#9a8c72', sat=1.0)
    mark = M('sb_mark', '#5a4a3a', sat=1.0)
    shade = M('sb_shade', '#2a2036', sat=1.0)
    gnomon = M('sb_gnomon', '#8a6a44')
    arrow = M('sb_arrow', '#ffc83a')
    zt = -0.30
    tube('plinth', (0, 0, -0.62), (0, 0, zt - 0.10), 0.78, 0.90, stone_d, segs=48)
    tube('plate', (0, 0, zt - 0.12), (0, 0, zt), 0.98, 0.98, stone, segs=64, rim_round=0.04)
    for k in range(12):
        a = k * 2 * math.pi / 12
        box(f'h{k}', (math.cos(a) * 0.80, math.sin(a) * 0.80, zt + 0.006), (0.17, 0.04, 0.012), mark,
            rot=Matrix.Rotation(a, 3, 'Z'))
    # la sombra: una cuna oscura que sale del pie del gnomon hacia la izquierda
    a0 = math.radians(196.0)
    poly = [(0.02, 0.06), (0.02, -0.06),
            (math.cos(a0 - 0.10) * 0.86, math.sin(a0 - 0.10) * 0.86),
            (math.cos(a0 + 0.10) * 0.86, math.sin(a0 + 0.10) * 0.86)]
    _flat_poly('shadow', poly, zt + 0.004, zt + 0.014, shade)
    # el gnomon de cara a la camara: de canto (en el plano YZ) era una raya
    _prism('gnomon', [(-0.30, zt), (0.24, zt), (0.24, zt + 0.62)], -0.045, 0.045, gnomon)
    # la flecha de "atras" (la de deshacer), de cara a la camara, encima del cuadrante
    C = Vector((0.0, 0.10, zt)) + _SU * 0.92
    R = 0.50
    pts = [C + _SR * (R * math.cos(math.radians(a))) + _SU * (R * math.sin(math.radians(a)) * 0.82)
           for a in np.linspace(10.0, 165.0, 18)]
    _chain('arc', pts, 0.06, 0.075, arrow)
    a = math.radians(165.0)
    tang = (_SR * (-math.sin(a)) + _SU * (math.cos(a) * 0.82)).normalized()
    tip0 = pts[-1] - tang * 0.04
    tube('head', tip0, tip0 + tang * 0.34, 0.20, 0.01, arrow, segs=20)


def _cayado(tag, tip, core, glow):
    """Las tres skins del cayado: el MISMO cayado con la luz de la punta en su color. Antes
    era la antorcha del objeto 10 tenida, y una antorcha no es un cayado."""
    wood = M(f'{tag}_wood', '#7e5030')
    wood_d = M(f'{tag}_wood_d', '#4e2e16')
    gold = M(f'{tag}_gold', '#e8c050')
    orb = M(f'{tag}_light', tip)
    orb_c = M(f'{tag}_light_c', core, sat=1.0)
    spark = M(f'{tag}_spark', glow, sat=1.0)
    st = [tube('shaft', (0, 0, -1.10), (0, 0, 0.52), 0.065, 0.052, wood, segs=12),
          tube('grip', (0, 0, -0.32), (0, 0, -0.02), 0.078, 0.076, wood_d, segs=12),
          tube('collar', (0, 0, 0.48), (0, 0, 0.60), 0.085, 0.11, gold, segs=16, rim_round=0.02),
          _flame('tip', (0, 0.0, 0.50), 0.26, 0.88, orb, sway=0.06, depth=0.8),
          _flame('tip_c', (0, -0.14, 0.52), 0.14, 0.52, orb_c, depth=0.8)]
    for o in st:
        rot(o, (0, 0, 0), 'Y', 26.0)
    _sparkle('sp0', (-0.05, -0.35, 1.08), 0.34, spark)
    _sparkle('sp1', (0.82, -0.35, 0.40), 0.24, spark)


def cayado_brasa():
    """S16 Brasa del Carmelo: el cayado con la punta en rojo brasa."""
    _cayado('cb', '#ff6a30', '#ffd8a8', '#ffb070')


def cayado_jordan():
    """S17 Luz del Jordan: el cayado con la punta en azul claro."""
    _cayado('cj', '#8ad8ff', '#ffffff', '#d8f4ff')


def cayado_horeb():
    """S18 Ceniza del Horeb: el cayado con la punta en gris violaceo."""
    _cayado('ch', '#8e78c4', '#d6c8f2', '#e4dcf6')


# Orden = celdas 32.. de la hoja, espejo de `Icon::SANCT_*` en src/render/sprites.h.
SANCTUARY = [
    ('corazon', corazon), ('hoguera', hoguera), ('sandalia_veloz', sandalia_veloz), ('halo', halo),
    ('aliento', aliento), ('primicias', primicias), ('suertes', suertes), ('alforja', alforja),
    ('tinaja_sarepta', tinaja_sarepta), ('diezmo', diezmo), ('altar_portatil', altar_portatil),
    ('herencia', herencia), ('lamparas', lamparas), ('escuela', escuela), ('mesa', mesa),
    ('sombra', sombra), ('cayado_brasa', cayado_brasa), ('cayado_jordan', cayado_jordan),
    ('cayado_horeb', cayado_horeb),
]


# Orden = celdas de la hoja. NO reordenar sin tocar src/render/sprites.h (namespace Icon).
ICONS = [
    ('honda', honda), ('vara', vara), ('pan', pan), ('aceite', aceite),
    ('sandalias', sandalias), ('mana', mana), ('tablas', tablas), ('arca', arca),
    ('vino', vino), ('agua_jordan', agua_jordan), ('antorcha', antorcha), ('memorial', memorial),
    ('ceniza', ceniza), ('tinaja_harina', tinaja_harina), ('vasija_aceite', vasija_aceite), ('manto', manto),
    ('arco', arco), ('higos', higos), ('sal', sal), ('cinto', cinto),
    ('reloj', reloj), ('panes_cebada', panes_cebada),
    ('cuervos', cuervos), ('sequia', sequia), ('fuego_cielo', fuego_cielo), ('carro', carro),
    ('altar_consumido', altar_consumido), ('sanar_aguas', sanar_aguas),
    ('aceite_inagotable', aceite_inagotable), ('ojos_abiertos', ojos_abiertos),
    ('ejercito', ejercito), ('doble_porcion', doble_porcion),
]


# =============================================================================
# Encuadre y render
# =============================================================================
def _bbox_points():
    pts = []
    for o in bpy.data.objects:
        if o.type != 'MESH':
            continue
        for c in o.bound_box:
            pts.append(o.matrix_world @ Vector(c))
    return pts


def _frame(tmp_dir, margin=MARGIN):
    """Camara orto a ELEV grados mirando desde -Y, encuadrando la caja de la escena con
    MARGIN de aire. Se mide en el ESPACIO DE CAMARA (proyectando las 8 esquinas de cada
    objeto), que es lo que de verdad ocupa la imagen."""
    sc = bpy.context.scene
    cam = render.setup_scene(tmp_dir)
    sc.render.resolution_x = CELL * SS
    sc.render.resolution_y = CELL * SS
    e = math.radians(ELEV)
    fwd = Vector((0.0, math.cos(e), -math.sin(e)))          # hacia donde mira
    right = Vector((1.0, 0.0, 0.0))
    up = right.cross(fwd).normalized() * -1.0
    pts = _bbox_points()
    xs = [p.dot(right) for p in pts]
    ys = [p.dot(up) for p in pts]
    cx, cy = (min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5
    size = max(max(xs) - min(xs), max(ys) - min(ys)) * margin
    center = right * cx + up * cy
    cam.location = center - fwd * 12.0
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    cam.data.ortho_scale = size
    cam.data.shift_x = 0.0
    cam.data.shift_y = 0.0
    sc.display.shading.light = 'STUDIO'
    return cam


# -----------------------------------------------------------------------------
# Luz de "figurita brillante" (2026-10-03): EEVEE en vez de la luz de estudio
# -----------------------------------------------------------------------------
# Con Workbench cada icono era plastico mate con la misma luz plana que los props del
# tileset. Un icono de objeto se mira de cerca y se lee por su BRILLO: luz principal calida
# desde arriba a la izquierda, un contraluz frio y fuerte desde atras que recorta el borde
# (lo que separa la pieza del fondo oscuro de la celda), relleno suave del cielo, sombras
# propias blandas y materiales con laca (coat). El oro y el bronce, metalicos; el fuego, las
# estrellas y la luz, emisivos. Los modelos, el encuadre y el contorno no cambian.
EEVEE = True
METAL_KEYS = ('gold', 'oro', 'metal', 'spear', 'blade', 'coin', 'bronze', 'iron', 'hd', 'brass',
              'shield', 'boss', 'ark', 'arca', 'crown', 'buckle', 'reloj_dial')
GLOW_KEYS = ('fire', 'flame', 'glow', 'sun', 'ray', 'star', 'light', 'spark', 'ember', 'halo',
             'llama', 'fuego', 'wick', 'drop_l', 'eye_glow')


def _light(name, kind, rot_deg, energy, color, angle=None, size=None):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    ld.color = color
    if angle is not None and kind == 'SUN':
        ld.angle = math.radians(angle)
    if size is not None and kind == 'AREA':
        ld.size = size
    ob = bpy.data.objects.new(name, ld)
    bpy.context.scene.collection.objects.link(ob)
    ob.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    return ob


def _eevee_look():
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE_NEXT'
    ee = sc.eevee
    ee.taa_render_samples = 48
    for attr, val in (('use_shadows', True), ('use_gtao', True), ('gtao_distance', 0.4),
                      ('use_raytracing', False), ('shadow_ray_count', 2), ('shadow_step_count', 8)):
        if hasattr(ee, attr):
            try:
                setattr(ee, attr, val)
            except (AttributeError, TypeError):
                pass
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    # Ambiente: el "cielo" ilumina por debajo de la luz principal (fondo transparente igual)
    w = sc.world
    w.use_nodes = True
    bg = w.node_tree.nodes.get('Background')
    if bg is not None:
        bg.inputs[0].default_value = (0.62, 0.64, 0.74, 1.0)
        bg.inputs[1].default_value = 0.70
    # Tres luces. Rotaciones de un SOL: el rayo baja por -Z local y la camara mira hacia +Y,
    # asi que una X POSITIVA manda la luz hacia +Y (de cara, desde la camara) y una NEGATIVA
    # hacia la camara (contraluz). La primera version giraba el contraluz 205 en Z, que lo
    # devolvia de cara: dos luces frontales y ningun borde.
    _light('ic_key', 'SUN', (50, 0, -38), 3.5, (1.0, 0.95, 0.86), angle=12)
    _light('ic_rim', 'SUN', (-58, 0, 30), 6.5, (0.88, 0.93, 1.0), angle=3)
    _light('ic_fill', 'SUN', (70, 0, 60), 0.8, (0.70, 0.78, 1.0), angle=20)
    # Materiales: los planos de parts.get_mat pasan a nodos con laca
    for m in list(bpy.data.materials):
        name = m.name.lower()
        col = tuple(m.diffuse_color)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new('ShaderNodeOutputMaterial')
        bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
        nt.links.new(bsdf.outputs[0], out.inputs[0])
        bsdf.inputs['Base Color'].default_value = col
        metal = any(k in name for k in METAL_KEYS)
        glow = any(k in name for k in GLOW_KEYS)
        # Metal a medias: sin entorno que reflejar, un metal pleno sale OSCURO (el oro del
        # Arca se apagaba). Medio metalico conserva el color y gana el brillo.
        bsdf.inputs['Metallic'].default_value = 0.5 if metal else 0.0
        bsdf.inputs['Roughness'].default_value = 0.30 if metal else 0.42
        if 'Coat Weight' in bsdf.inputs and not metal:
            bsdf.inputs['Coat Weight'].default_value = 0.35
            bsdf.inputs['Coat Roughness'].default_value = 0.12
        if glow:
            bsdf.inputs['Emission Color'].default_value = col
            bsdf.inputs['Emission Strength'].default_value = 1.6


# Resplandor en el post (color, radio en px, fuerza) y margen propio, por constructor. Un
# halo de luz sin halo no es luz; como geometria translucida EEVEE lo pintaba opaco y gris.
POST_GLOW = {'halo': ((1.0, 0.85, 0.45), 9, 0.80), 'herencia': ((1.0, 0.86, 0.50), 8, 0.70)}
FRAME_MARGIN = {'halo': 1.34, 'herencia': 1.30}


def _blur1(x, r, axis):
    k = 2 * r + 1
    pad = [(r, r) if i == axis else (0, 0) for i in range(x.ndim)]
    c = np.cumsum(np.pad(x, pad, mode='constant'), axis=axis)
    c = np.concatenate([np.zeros_like(np.take(c, [0], axis=axis)), c], axis=axis)
    n = c.shape[axis]
    return (np.take(c, range(k, n), axis=axis) - np.take(c, range(0, n - k), axis=axis)) / k


def _post_glow(img, color, radius, strength):
    """Resplandor DETRAS del icono: el alfa desenfocado (tres pasadas de caja, casi una
    gaussiana) tenido de `color`. Va despues del contorno, asi que lo rodea por fuera."""
    a = img[..., 3]
    g = a
    for _ in range(3):
        g = _blur1(_blur1(g, radius, 0), radius, 1)
    ga = np.clip(g * 1.7, 0.0, 1.0) * strength
    out_a = a + ga * (1.0 - a)
    col = np.array(color, dtype=np.float32)
    rgb = (img[..., :3] * a[..., None] + col * (ga * (1.0 - a))[..., None]) / np.maximum(out_a[..., None], 1e-4)
    out = img.copy()
    out[..., :3] = np.clip(rgb, 0.0, 1.0)
    out[..., 3] = np.clip(out_a, 0.0, 1.0)
    return out


def render_icon(builder, tmp_dir):
    parts.clear_scene()
    builder()
    _frame(tmp_dir, FRAME_MARGIN.get(builder.__name__, MARGIN))
    if EEVEE:
        _eevee_look()
    hi = render._render_to_array()
    # gain 1.0 con EEVEE: el 1.7 por defecto compensa la luz de estudio de Workbench
    out = render.post_soft(hi, CELL, SS, outline=True, outline_px=OUTLINE_PX,
                           gain=1.08 if EEVEE else None)
    glow = POST_GLOW.get(builder.__name__)
    return _post_glow(out, *glow) if glow else out


def _load(path, shape):
    """La hoja anterior, si tiene el mismo ANCHO (una hoja de menos filas se pega arriba:
    asi se amplio de 4 a 7 filas sin volver a renderizar las 32 celdas que ya estaban)."""
    if not os.path.exists(path):
        return None
    img = bpy.data.images.load(path)
    try:
        img.colorspace_settings.name = 'Non-Color'
        buf = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
        img.pixels.foreach_get(buf)
        arr = buf.reshape(img.size[1], img.size[0], 4)[::-1].copy()
    finally:
        bpy.data.images.remove(img)
    return arr if arr.shape[1:] == shape[1:] and arr.shape[0] <= shape[0] else None


def main(sprites_dir, tmp_dir, only=None, log=print):
    render._TMP = tmp_dir
    out = os.path.join(sprites_dir, 'icons.png')
    shape = (ROWS * CELL, COLS * CELL, 4)
    sheet = np.zeros(shape, dtype=np.float32)
    if only:
        prev = _load(out, shape)
        if prev is not None:
            sheet[:prev.shape[0]] = prev
    for i, (name, fn) in enumerate(ICONS + SANCTUARY):
        if only and name not in only:
            continue
        fr = render_icon(fn, tmp_dir)
        r, c = divmod(i, COLS)
        sheet[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL] = fr
        cov = float((fr[..., 3] > 0.5).mean())
        log(f'  icon {i:2d} {name:18s} cobertura {cov:.2f}')
    render.save_png(sheet, out, 'mantle_icons')
    log(f'icons -> {out}')
    return out
