"""Iconos de objetos y milagros -> assets/sprites/icons.png (2026-09-24).

El usuario: "cada objeto y milagro debe tener su propio icono". Hasta hoy los 22 objetos
compartian CUATRO iconos del tileset (uno por categoria: flecha, estrella, orbe, corazon) y
los 10 milagros seis (el Carro de Fuego y el Fuego del Cielo eran la misma bola de fuego).

Hoja: 8 columnas x 4 filas de 128 px (= 32 unidades a x4). Celda i = objeto con id i
(0..21, el orden de ItemRegistry::init); celda 22 + m = milagro con MiracleID m (0..9).
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

import bpy
import numpy as np
from mathutils import Vector, Matrix

from . import parts, render
from .parts import box, sphere, tube

CELL = 128
COLS, ROWS = 8, 4
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


def _frame(tmp_dir):
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
    size = max(max(xs) - min(xs), max(ys) - min(ys)) * MARGIN
    center = right * cx + up * cy
    cam.location = center - fwd * 12.0
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    cam.data.ortho_scale = size
    cam.data.shift_x = 0.0
    cam.data.shift_y = 0.0
    sc.display.shading.light = 'STUDIO'
    return cam


def render_icon(builder, tmp_dir):
    parts.clear_scene()
    builder()
    _frame(tmp_dir)
    hi = render._render_to_array()
    return render.post_soft(hi, CELL, SS, outline=True, outline_px=OUTLINE_PX)


def _load(path, shape):
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
    return arr if arr.shape == shape else None


def main(sprites_dir, tmp_dir, only=None, log=print):
    render._TMP = tmp_dir
    out = os.path.join(sprites_dir, 'icons.png')
    shape = (ROWS * CELL, COLS * CELL, 4)
    sheet = np.zeros(shape, dtype=np.float32)
    if only:
        prev = _load(out, shape)
        if prev is not None:
            sheet = prev
    for i, (name, fn) in enumerate(ICONS):
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
