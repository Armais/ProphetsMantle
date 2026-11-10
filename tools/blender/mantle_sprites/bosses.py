"""Los nueve jefes con hoja propia (2026-10-03).

Peticion del usuario: «los jefes deben de ser distintos a los enemigos principales». Hasta
hoy cada jefe era la hoja de un enemigo escalada x1,6 (el Capitan era un soldado grande, La
Muerte un idolatra grande...). Cada uno sale ahora de su propia receta, sobre el MISMO
esqueleto humanoide de `characters.py` (las animaciones de `anims.py` valen tal cual) y con
piezas propias que cambian la SILUETA, que es lo que se lee a 32 unidades:

    boss_capitan          escudo torre con sol, cresta de crin, hombreras y capa carmesi
    boss_sacerdote        tiara conica de oro con cuernos de toro y disco solar, pectoral
    boss_sumo_sacerdote   corona de cuernos enormes, dos cuchillos, cortes en los brazos (1 R 18:28)
    boss_desesperacion    espectro encapuchado sin piernas, ojos de hielo, jirones
    boss_espiritu         columna de agua corrupta con corona de espuma y garras
    boss_acreedor         prestamista grueso con turbante, bolsas de dinero y cadena
    boss_muerte           sudario negro, calavera, alas rotas y guadana
    boss_guiezi           Guiezi leproso «blanco como la nieve» (2 R 5:27) con dos talentos de plata
    boss_benadad          rey sirio con tiara alta, barba rizada y espada ancha

Acab y Jezabel (cap. 5) ya tenian hoja y no se tocan.

Las piezas de la cabeza se escalan con HEAD_SCALE alrededor de HEAD_PIVOT igual que hace
`build_humanoid`, y la regla de la cara sigue en pie: nada baja de z 1.60 por delante.
"""
import math

import bpy
from mathutils import Matrix, Vector

from . import characters as C
from .parts import box, sphere, tube, attach, scale_about, get_mat, vivid, hexc


# =============================================================================
# Utilidades
# =============================================================================
def _M(name, key, col):
    return C._mat(name, key, col)


def _glow(name, key, col):
    """Color 'emisivo' para Workbench (no hay emision): saturado y casi blanco en el centro."""
    return get_mat(f'{name}_{key}', vivid(hexc(col), 1.15, 1.10))


def _drop(name, *keys):
    """Quita piezas de `build_humanoid` por sufijo (piernas de un espectro, ojos humanos)."""
    for k in keys:
        ob = bpy.data.objects.get(f'{name}_{k}')
        if ob is not None:
            bpy.data.objects.remove(ob, do_unlink=True)


def _H(arm, ob):
    """Pieza de cabeza: misma estilizacion que build_humanoid (cabezon)."""
    scale_about(ob, C.HEAD_PIVOT, C.HEAD_SCALE)
    return attach(ob, arm, 'head')


def _widen(name, keys, sx, sy=None):
    """Ensancha piezas del tronco sobre su propio centro (el Acreedor)."""
    sy = sx if sy is None else sy
    for k in keys:
        ob = bpy.data.objects.get(f'{name}_{k}')
        if ob is None:
            continue
        vs = ob.data.vertices
        cx = sum(v.co.x for v in vs) / len(vs)
        cy = sum(v.co.y for v in vs) / len(vs)
        for v in vs:
            v.co.x = cx + (v.co.x - cx) * sx
            v.co.y = cy + (v.co.y - cy) * sy


def _arc(name, arm, bone, center, radius, a0, a1, z, thick, mat, n=7, axis='XZ', head=False,
         taper=1.0):
    """Arco de cajas (cuernos, hoja de guadana). Plano XZ por defecto: de frente se ve la curva."""
    obs = []
    for i in range(n):
        t0 = i / n
        t1 = (i + 1) / n
        a = math.radians(a0 + (a1 - a0) * (t0 + t1) * 0.5)
        r = thick * (1.0 - (1.0 - taper) * (t0 + t1) * 0.5)
        if axis == 'XZ':
            p = Vector((center[0] + math.cos(a) * radius, center[1], center[2] + math.sin(a) * radius))
            rot = Matrix.Rotation(-a + math.pi / 2, 4, 'Y')
        else:   # XY: arco en el plano del suelo (vista cenital)
            p = Vector((center[0] + math.cos(a) * radius, center[1] + math.sin(a) * radius, z))
            rot = Matrix.Rotation(a + math.pi / 2, 4, 'Z')
        seg = (math.radians(abs(a1 - a0)) * radius / n) * 1.25
        ob = box(f'{name}_{bone}_arc{i}_{len(obs)}', p, (r, r, seg), mat, rot=rot.to_3x3(), round=0.9)
        obs.append(_H(arm, ob) if head else attach(ob, arm, bone))
    return obs


def _horn(name, arm, side, base, mat, length=0.34, thick=0.055, rise=1.0, out=1.0, n=8):
    """Cuerno de toro: sale hacia fuera, sube y curva hacia dentro. Tubos encadenados."""
    pts = []
    for i in range(n + 1):
        t = i / n
        x = base[0] + side * out * (math.sin(t * 1.9) * length * 0.95)
        z = base[2] + rise * (t * t * length * 0.9 + t * length * 0.25)
        y = base[1] + t * 0.04
        pts.append(Vector((x, y, z)))
    for i in range(n):
        r0 = thick * (1.0 - i / n) + 0.006
        r1 = thick * (1.0 - (i + 1) / n) + 0.004
        _H(arm, tube(f'{name}_horn{side}_{i}', pts[i], pts[i + 1], r0, r1, mat, segs=12))


def _float_robe(name, arm, mat, hem=0.36, flutes=11, amp=0.12):
    """Faldon que flota (espectros): sin pies, el bajo en jirones (pliegues fuertes)."""
    _drop(name, 'robe', 'skirt')
    attach(tube(f'{name}_robe_float', (0, 0, 1.00), (0, 0, 0.16), 0.20, hem, mat, segs=48,
                rim_round=0.02, flutes=flutes, flute_amp=amp), arm, 'hips')
    for s in ('L', 'R'):
        _drop(name, f'thigh_{s}', f'shin_{s}', f'foot_{s}')


def _glow_eyes(name, arm, col, r=0.034, y=-0.118):
    """Ojos que brillan en una cara en sombra (espectro, Muerte, espiritu)."""
    _drop(name, *C.FACE_KEYS)
    core = _glow(name, 'eyeglow', col)
    halo = get_mat(f'{name}_eyehalo', vivid(hexc(col), 1.0, 0.75))
    for s in (1.0, -1.0):
        _H(arm, sphere(f'{name}_geye_halo_{s}', (C.EYE_X * s, y + 0.010, C.EYE_Z), r * 1.55, halo,
                       scale=(1.25, 0.5, 0.8)))
        _H(arm, sphere(f'{name}_geye_{s}', (C.EYE_X * s, y, C.EYE_Z), r, core, scale=(1.2, 0.6, 0.85)))


# =============================================================================
# Recetas
# =============================================================================
SPECS = {
    # Cap 1. 1 R 18:10: el que lleva la caceria. Azul de la guardia de Acab en oscuro, bronce
    # y carmesi; el ESCUDO TORRE es su verbo (bloquea de frente) y tiene que verse de lejos.
    'boss_capitan': dict(
        skin='#c98c5a', beard='#2a1a12', helmet='#b8862e', plume='#c0392b',
        eyes='#241a14', brow='angry',
        tunic='#1f4e7a', tunic_len='short', sleeves='#1f4e7a', armor='#c9a24a',
        legs='#7a5a2a', belt='#5a3a1a', cape='#9b2335', shoes='#3a2a1a', bracer='#c9a24a',
        weapon='sword', weapon_colors=dict(blade='#e4e8ec', guard='#d4a537'),
        attack='swing'),
    # Cap 2. Sacerdote del Baal de Sidon: carmesi y negro, no el morado del profeta raso.
    'boss_sacerdote': dict(
        skin='#c98c5a', beard='#151015', beard_long=True,
        eyes='#b0281e', brow='angry',
        tunic='#7a1e2e', tunic_len='long', robe='#4a0f1c', sleeves='#7a1e2e',
        belt='#d4a537', sash='#d4a537', mantle='#120c14', shoes='#1a1420', staff_top=1.78,
        weapon='staff_idol', weapon_colors=dict(wood='#2a1a2a', tip='#d4a537'),
        attack='raise'),
    # Cap 3. El que encabeza el corro del Carmelo. Negro y sangre, pelo suelto, cuernos.
    'boss_sumo_sacerdote': dict(
        skin='#b07a4a', hair='#151015', hair_long=True, beard='#151015', beard_long=True,
        eyes='#c0281e', brow='wild',
        tunic='#1a1420', tunic_len='long', robe='#0d0a10', sleeves='#8e1f1f',
        belt='#d4a537', sash='#b0281e', cape='#5b0f14', shoes='#1a1420', bracer='#d4a537',
        weapon='sword', weapon_colors=dict(blade='#d8dce0', guard='#7a1f1f'),
        attack='swing'),
    # Cap 4. No es un hombre: es lo que Elias sintio bajo el enebro (1 R 19:4). Sin piernas,
    # sin cara, solo dos ojos de hielo bajo la capucha.
    'boss_desesperacion': dict(
        skin='#161a24', hood='#1b2030', beard=None, face=True,
        eyes='#9fe8ff', brow='sad',
        tunic='#232a3a', tunic_len='long', robe='#141824', sleeves='#1e2433',
        shoes='#141824', weapon=None, attack='claw'),
    # Cap 6. Las aguas malas de Jerico (2 R 2:19) hechas cuerpo: verde de cienaga y espuma.
    'boss_espiritu': dict(
        skin='#1f5a52', beard=None, eyes='#ff4b3a', brow='angry',
        tunic='#1c4a45', tunic_len='long', robe='#163b38', sleeves='#1f5a52',
        shoes='#163b38', weapon=None, attack='claw'),
    # Cap 7. El acreedor de la viuda (2 R 4:1): mostaza, pardo y oro, y GRUESO.
    'boss_acreedor': dict(
        skin='#d29a68', hair='#3a2418', beard='#3a2418',
        eyes='#2a1c14', brow='noble', mouth_color='#7a3a2a',
        tunic='#8c5a2b', tunic_len='long', robe='#5e3a1a', sleeves='#a8723a',
        belt='#d4a537', sash='#d4a537', cape='#2e4a3a', bracer='#d4a537', shoes='#3a2418',
        weapon=None, attack='swing'),
    # Cap 8. La Muerte que entra en el aposento de Sunem: sudario, calavera, alas rotas.
    'boss_muerte': dict(
        skin='#e8e2d0', hood='#121016', beard=None,
        eyes='#ff5a3a', brow='angry',
        tunic='#1a1820', tunic_len='long', robe='#0e0d12', sleeves='#1a1820',
        shoes='#0e0d12', weapon=None, attack='swing'),
    # Cap 9. 2 R 5:23-27: dos talentos de plata y dos mudas de ropa... y la lepra de Naaman.
    'boss_guiezi': dict(
        skin='#f1ebe2', hair='#2a1a12', beard='#2a1a12',
        eyes='#3a2a1e', brow='angry', mouth_color='#9a5a50',
        headcloth='#7a3a5e', headcloth_cord='#c8ccd4',
        tunic='#2f6f7f', tunic_len='long', robe='#24555f', sleeves='#3a8a96',
        belt='#b8bcc4', sash='#b8bcc4', mantle='#6a2a4a', shoes='#3a3028',
        weapon=None, attack='raise'),
    # Cap 10. Ben-adad: tiara alta, barba rizada a la asiria, rojo real, oro y purpura.
    'boss_benadad': dict(
        skin='#c98c5a', hair='#151015', hair_long=True, beard='#151015', beard_long=True,
        eyes='#241a14', brow='angry',
        tunic='#8f1d2c', tunic_len='long', robe='#5e1220', sleeves='#a8283a',
        armor='#d4a537', belt='#d4a537', sash='#d4a537', cape='#3b1a5a', bracer='#d4a537',
        shoes='#2a1a12',
        weapon='sword', weapon_colors=dict(blade='#eef0f2', guard='#d4a537'),
        attack='swing'),
}

NAMES = list(SPECS.keys())


def _extras_capitan(n, arm):
    bronze = _M(n, 'bronze', '#c9a24a')
    dark = _M(n, 'bronze_dk', '#8a6420')
    crimson = _M(n, 'crimson', '#b02a36')
    # Cresta de crin de proa a popa, alta: la silueta del capitan
    for i in range(6):
        t = i / 5.0
        y = -0.10 + t * 0.26
        z = 1.86 + math.sin(t * math.pi) * 0.07
        _H(arm, box(f'{n}_crest{i}', (0, y, z), (0.050, 0.070, 0.14 - abs(t - 0.5) * 0.06),
                    crimson, round=0.9))
    # Hombreras de laminas
    for s, x in (('L', 0.25), ('R', -0.25)):
        for k in range(3):
            attach(box(f'{n}_paul_{s}{k}', (x * (1.0 + k * 0.06), 0.0, 1.47 - k * 0.055),
                       (0.17 - k * 0.012, 0.22, 0.05), bronze if k % 2 == 0 else dark,
                       round=0.8, taper=(1.08, 1.0)), arm, 'chest')
    # Faldellin de tiras de cuero (pteruges)
    leather = _M(n, 'leather', '#6b4423')
    attach(tube(f'{n}_pteruges', (0, 0, 1.02), (0, 0, 0.66), 0.215, 0.26, leather, segs=48,
                rim_round=0.01, flutes=12, flute_amp=0.10), arm, 'hips')
    # Escudo torre (sustituye al redondo): rectangulo alto curvo con sol de oro
    wood = _M(n, 'shield', '#7a2a24')
    attach(box(f'{n}_tower', (0.31, -0.06, 0.98), (0.10, 0.06, 0.78), wood, round=0.6), arm, 'forearm.L')
    attach(box(f'{n}_tower_face', (0.31, -0.10, 0.98), (0.42, 0.04, 0.74), wood,
               round=0.7, taper=(0.92, 1.0)), arm, 'forearm.L')
    attach(box(f'{n}_tower_rim', (0.31, -0.125, 0.98), (0.46, 0.02, 0.78), dark,
               round=0.5, taper=(0.92, 1.0)), arm, 'forearm.L')
    attach(box(f'{n}_tower_inner', (0.31, -0.135, 0.98), (0.38, 0.02, 0.70), wood,
               round=0.6, taper=(0.92, 1.0)), arm, 'forearm.L')
    sun = _M(n, 'sun', '#ffd35a')
    attach(sphere(f'{n}_tower_sun', (0.31, -0.15, 1.02), 0.075, sun, scale=(1, 0.35, 1)), arm, 'forearm.L')
    for k in range(8):
        a = k * math.pi / 4
        attach(box(f'{n}_tower_ray{k}', (0.31 + math.cos(a) * 0.12, -0.148, 1.02 + math.sin(a) * 0.12),
                   (0.03, 0.015, 0.07), sun, rot=Matrix.Rotation(-a + math.pi / 2, 3, 'Y'), round=0.6),
               arm, 'forearm.L')


def _extras_sacerdote(n, arm):
    gold = _M(n, 'gold', '#d9ab3a')
    red = _M(n, 'disk', '#c0281e')
    ivory = _M(n, 'ivory', '#efe3c4')
    # Tiara conica alta con tres aros
    _H(arm, tube(f'{n}_tiara', (0, 0.01, 1.70), (0, 0.02, 2.06), 0.150, 0.060, gold, rim_round=0.02))
    for k, z in enumerate((1.76, 1.87, 1.97)):
        r = 0.150 - (z - 1.70) * 0.25
        _H(arm, tube(f'{n}_tiara_ring{k}', (0, 0.01, z), (0, 0.01, z + 0.022), r + 0.008, r + 0.006,
                     red if k == 1 else gold, caps=False, rim_round=0.008))
    _H(arm, sphere(f'{n}_tiara_knob', (0, 0.02, 2.08), 0.04, gold))
    # Cuernos de toro a los lados de la tiara y el disco del sol entre ellos
    for side in (1, -1):
        _horn(n, arm, side, (side * 0.12, 0.0, 1.78), ivory, length=0.23, thick=0.042)
    _H(arm, tube(f'{n}_sun_disk', (0, 0.05, 1.98), (0, 0.10, 1.98), 0.085, 0.085, red, segs=32))
    # Pectoral de oro con gema
    attach(box(f'{n}_pectoral', (0, -0.125, 1.33), (0.26, 0.03, 0.17), gold, round=0.7,
               taper=(1.15, 1.0)), arm, 'chest')
    attach(sphere(f'{n}_pect_gem', (0, -0.145, 1.34), 0.04, red, scale=(1, 0.5, 1)), arm, 'chest')
    for s, x in (('L', 0.09), ('R', -0.09)):
        attach(sphere(f'{n}_pect_gem{s}', (x, -0.142, 1.31), 0.026, _M(n, 'lapis', '#2a4aa8'),
                      scale=(1, 0.5, 1)), arm, 'chest')


def _extras_sumo(n, arm):
    ivory = _M(n, 'ivory', '#efe3c4')
    gold = _M(n, 'gold', '#d4a537')
    blood = _M(n, 'blood', '#b0181e')
    # Aro de oro y cuernos ENORMES, abiertos hacia fuera
    _H(arm, tube(f'{n}_band', (0, 0, 1.712), (0, 0, 1.760), 0.152, 0.152, gold, caps=False,
                 rim_round=0.012))
    for side in (1, -1):
        _horn(n, arm, side, (side * 0.13, 0.0, 1.74), ivory, length=0.33, thick=0.058, rise=0.9,
              out=1.1, n=10)
    # Los cortes de 1 R 18:28 en los brazos (rojo sobre las mangas)
    for s, x in (('L', 0.22), ('R', -0.22)):
        for k in range(3):
            attach(box(f'{n}_cut{s}{k}', (x, -0.052, 1.36 - k * 0.07), (0.07, 0.012, 0.012), blood,
                       rot=Matrix.Rotation(math.radians(20 if s == 'L' else -20), 3, 'Y'), round=0.5),
                   arm, 'upper_arm.' + s)
    # Segundo cuchillo en la izquierda (el de la derecha es el `sword` con hoja corta)
    steel = _M(n, 'steel2', '#d8dce0')
    attach(box(f'{n}_knife_l', (0.22, -0.07, 0.70), (0.03, 0.06, 0.32), steel, round=0.55,
               taper=(0.4, 0.3)), arm, 'hand.L')
    attach(box(f'{n}_knife_l_guard', (0.22, -0.07, 0.86), (0.10, 0.035, 0.03), blood, round=0.6),
           arm, 'hand.L')
    # Brazaletes
    for s, x in (('L', 0.22), ('R', -0.22)):
        attach(tube(f'{n}_armlet_{s}', (x, 0, 1.33), (x, 0, 1.29), 0.064, 0.064, gold,
                    rim_round=0.01), arm, 'upper_arm.' + s)
    # La hoja del cuchillo derecho, mas corta y ancha que una espada
    ob = bpy.data.objects.get(f'{n}_blade')
    if ob is not None:
        for v in ob.data.vertices:
            v.co.z = 0.95 + (v.co.z - 0.95) * 0.55
            v.co.y = -0.07 + (v.co.y + 0.07) * 1.4


def _extras_desesperacion(n, arm):
    robe = _M(n, 'robe2', '#141824')
    rag = _M(n, 'rag', '#0c0f16')
    _float_robe(n, arm, robe, hem=0.40, flutes=9, amp=0.16)
    _glow_eyes(n, arm, '#9fe8ff', r=0.036)
    # Jirones que cuelgan de las mangas y de la espalda
    for s, x in (('L', 0.24), ('R', -0.24)):
        for k in range(3):
            attach(box(f'{n}_tatter_{s}{k}', (x + (0.02 if s == 'L' else -0.02) * k, 0.02 + k * 0.03,
                                             0.86 - k * 0.06), (0.05, 0.03, 0.22 - k * 0.04), rag,
                       round=0.8, taper=(0.3, 0.5)), arm, 'forearm.' + s)
    for k in range(5):
        x = -0.16 + k * 0.08
        attach(box(f'{n}_back_tatter{k}', (x, 0.17, 1.00 - (k % 2) * 0.08), (0.07, 0.03, 0.42), rag,
                   round=0.8, taper=(0.25, 0.6)), arm, 'chest')
    # Manos de garra palidas
    claw = _M(n, 'claw', '#5a6a80')
    for s, x in (('L', 0.22), ('R', -0.22)):
        for k in range(3):
            attach(box(f'{n}_claw_{s}{k}', (x + (k - 1) * 0.022, -0.02, 0.80), (0.014, 0.014, 0.09),
                       claw, round=0.6, taper=(0.3, 0.3)), arm, 'hand.' + s)


def _extras_espiritu(n, arm):
    water = _M(n, 'water', '#1c4a45')
    foam = _M(n, 'foam', '#7fe0b0')
    slime = _M(n, 'slime', '#3fae7c')
    _float_robe(n, arm, water, hem=0.46, flutes=13, amp=0.18)
    _drop(n, 'hair', 'hair_fringe', 'hair_fringe2', 'hair_side_L', 'hair_side_R')
    _glow_eyes(n, arm, '#ff4b3a', r=0.040)
    # Corona de espuma: puntas que suben de la cabeza
    for k in range(7):
        a = math.radians(-70 + k * (140 / 6))
        x = math.sin(a) * 0.12
        y = math.cos(a) * 0.05 + 0.03
        h = 0.16 + (0.07 if k == 3 else 0.0) - abs(k - 3) * 0.015
        _H(arm, tube(f'{n}_spike{k}', (x, y, 1.70), (x * 1.5, y + 0.02, 1.70 + h), 0.045, 0.004,
                     foam if k % 2 == 0 else slime, segs=12))
    # Burbujas que suben por la columna
    for k, (x, y, z, r) in enumerate(((0.16, -0.18, 0.55, 0.05), (-0.20, -0.12, 0.40, 0.04),
                                      (0.10, -0.24, 0.30, 0.035), (-0.08, -0.20, 0.75, 0.03),
                                      (0.24, 0.0, 0.70, 0.04))):
        attach(sphere(f'{n}_bubble{k}', (x, y, z), r, foam), arm, 'hips')
    # Charco a los pies
    attach(tube(f'{n}_pool', (0, 0, 0.0), (0, 0, 0.04), 0.52, 0.48, slime, segs=48, rim_round=0.02),
           arm, 'root')
    # Garras de agua
    for s, x in (('L', 0.22), ('R', -0.22)):
        for k in range(3):
            attach(tube(f'{n}_wclaw_{s}{k}', (x + (k - 1) * 0.03, -0.02, 0.86),
                        (x + (k - 1) * 0.05, -0.06, 0.72), 0.022, 0.004, foam, segs=10),
                   arm, 'hand.' + s)


def _extras_acreedor(n, arm):
    _widen(n, ('torso_lo', 'torso_up'), 1.38, 1.30)
    _widen(n, ('robe', 'belt', 'sash'), 1.22, 1.22)
    turban = _M(n, 'turban', '#d9a43a')
    turban2 = _M(n, 'turban2', '#b9802a')
    gem = _M(n, 'gem', '#2e7d5b')
    _drop(n, 'hair', 'hair_fringe', 'hair_fringe2')
    # Turbante: tres vueltas de tela y una gema al frente
    for k in range(3):
        z = 1.715 + k * 0.05
        r = 0.165 - k * 0.012
        _H(arm, sphere(f'{n}_turban{k}', (0, 0.02, z), r, turban if k % 2 == 0 else turban2,
                       scale=(1.0, 1.0, 0.42)))
    _H(arm, sphere(f'{n}_turban_top', (0, 0.03, 1.85), 0.09, turban, scale=(1, 1, 0.8)))
    _H(arm, sphere(f'{n}_turban_gem', (0, -0.16, 1.75), 0.035, gem, scale=(1, 0.5, 1.2)))
    # Bolsas de dinero colgadas del cinto
    sack = _M(n, 'sack', '#c8a878')
    tie = _M(n, 'tie', '#6b4423')
    coin = _M(n, 'coin', '#ffd35a')
    for k, (x, y) in enumerate(((0.22, -0.12), (-0.24, -0.08), (0.05, -0.27))):
        attach(sphere(f'{n}_bag{k}', (x, y, 0.88), 0.085, sack, scale=(1, 1, 1.1)), arm, 'hips')
        attach(tube(f'{n}_bagtie{k}', (x, y, 0.95), (x, y, 1.0), 0.03, 0.02, tie), arm, 'hips')
        attach(sphere(f'{n}_bagcoin{k}', (x, y - 0.05, 0.98), 0.025, coin, scale=(1, 0.4, 1)), arm, 'hips')
    # Cadena enrollada en la mano derecha (la que lanza)
    chain = _M(n, 'chain', '#8a8f98')
    for k in range(9):
        a = k * 2 * math.pi / 9
        attach(tube(f'{n}_chain{k}', (-0.22 + math.cos(a) * 0.08, -0.06 + math.sin(a) * 0.03, 0.80 - k * 0.012),
                    (-0.22 + math.cos(a + 0.6) * 0.08, -0.06 + math.sin(a + 0.6) * 0.03, 0.80 - (k + 1) * 0.012),
                    0.016, 0.016, chain, segs=8), arm, 'hand.R')
    for k in range(4):
        attach(sphere(f'{n}_chainhang{k}', (-0.22, -0.07, 0.72 - k * 0.06), 0.022, chain), arm, 'hand.R')
    # Tablilla de cuentas en la izquierda
    clay = _M(n, 'clay', '#b07a52')
    attach(box(f'{n}_tablet', (0.22, -0.10, 0.84), (0.16, 0.03, 0.20), clay, round=0.6), arm, 'hand.L')
    for k in range(4):
        attach(box(f'{n}_tablet_line{k}', (0.22, -0.118, 0.90 - k * 0.04), (0.11, 0.006, 0.008),
                   _M(n, 'ink', '#5a3a22'), round=0.3), arm, 'hand.L')


def _extras_muerte(n, arm):
    shroud = _M(n, 'shroud', '#0e0d12')
    _float_robe(n, arm, shroud, hem=0.42, flutes=10, amp=0.18)
    bone = _M(n, 'bone', '#e8e2d0')
    socket = _M(n, 'socket', '#0a0808')
    # Calavera: cuencas negras con brasa al fondo y dientes
    _drop(n, *C.FACE_KEYS)
    ember = _glow(n, 'ember', '#ff5a3a')
    for s in (1.0, -1.0):
        _H(arm, sphere(f'{n}_socket_{s}', (C.EYE_X * s, -0.100, C.EYE_Z), 0.048, socket,
                       scale=(1.05, 0.5, 1.1)))
        _H(arm, sphere(f'{n}_ember_{s}', (C.EYE_X * s, -0.122, C.EYE_Z - 0.004), 0.018, ember,
                       scale=(1.0, 0.5, 1.0)))
    _H(arm, sphere(f'{n}_nosehole', (0, -0.124, 1.598), 0.020, socket, scale=(0.8, 0.5, 1.2)))
    for k in range(5):
        _H(arm, box(f'{n}_tooth{k}', (-0.044 + k * 0.022, -0.122, 1.553), (0.016, 0.016, 0.028), bone,
                    round=0.5))
    # Alas rotas a la espalda: plumas negras en abanico
    wing = _M(n, 'wing', '#1d1824')
    wing2 = _M(n, 'wing2', '#2c2436')
    for s in (1, -1):
        for k in range(5):
            a = math.radians(18 + k * 16)
            L = 0.62 - k * 0.07
            p = Vector((s * (0.16 + math.cos(a) * L * 0.5), 0.20, 1.34 + math.sin(a) * L * 0.5))
            rot = Matrix.Rotation(s * (math.pi / 2 - a), 3, 'Y')
            attach(box(f'{n}_feather{s}{k}', p, (0.10, 0.03, L), wing if k % 2 == 0 else wing2,
                       rot=rot, round=0.8, taper=(0.35, 0.6)), arm, 'chest')
    # Guadana: asta larga y hoja curva arriba
    wood = _M(n, 'pole', '#2a2420')
    steel = _M(n, 'scythe', '#c8ccd2')
    hx = -0.22
    pole = tube(f'{n}_pole', (hx, -0.06, 0.15), (hx, -0.06, 2.02), 0.024, 0.022, wood)
    from .parts import rotate_about
    pieces = [pole]
    for k in range(7):
        t0, t1 = k / 7, (k + 1) / 7
        a0, a1 = math.radians(180 - t0 * 120), math.radians(180 - t1 * 120)
        r = 0.36
        cx, cz = hx + 0.36, 1.98
        p0 = Vector((cx + math.cos(a0) * r, -0.06, cz + math.sin(a0) * r * 0.55))
        p1 = Vector((cx + math.cos(a1) * r, -0.06, cz + math.sin(a1) * r * 0.55))
        pieces.append(tube(f'{n}_blade{k}', p0, p1, 0.045 * (1 - t0) + 0.006, 0.045 * (1 - t1) + 0.004,
                           steel, segs=10))
    for ob in pieces:
        rotate_about(ob, (hx, 0.0, 0.88), 'X', C.STAFF_TILT)
        attach(ob, arm, 'hand.R')
    # Manos de hueso
    for s in ('L', 'R'):
        ob = bpy.data.objects.get(f'{n}_hand_{s}')
        if ob is not None:
            ob.data.materials[0] = bone


def _extras_guiezi(n, arm):
    leper = _M(n, 'leper', '#fbf7f2')
    sore = _M(n, 'sore', '#e2aaa2')
    # Manchas de lepra en cara y brazos
    for k, (x, z, r) in enumerate(((0.07, 1.585, 0.022), (-0.08, 1.640, 0.018), (0.02, 1.690, 0.015),
                                   (-0.05, 1.560, 0.016))):
        _H(arm, sphere(f'{n}_spot{k}', (x, -0.122, z), r, leper if k % 2 == 0 else sore,
                       scale=(1.0, 0.35, 1.0)))
    for s, x in (('L', 0.22), ('R', -0.22)):
        for k in range(2):
            attach(sphere(f'{n}_armspot{s}{k}', (x, -0.048, 1.08 - k * 0.07), 0.02,
                          leper if k == 0 else sore, scale=(1, 0.4, 1)), arm, 'forearm.' + s)
    # Dos talentos de plata: sacos de arpillera atados, con la plata asomando por la boca
    sack = _M(n, 'sack', '#8d8070')
    silver = _M(n, 'silver', '#eef2f8')
    tie = _M(n, 'tie', '#4a3a2a')
    for s, x in (('L', 0.25), ('R', -0.25)):
        attach(sphere(f'{n}_talent_{s}', (x, -0.04, 0.66), 0.13, sack, scale=(1.0, 0.85, 1.05)),
               arm, 'hand.' + s)
        attach(tube(f'{n}_talent_neck_{s}', (x, -0.04, 0.76), (x, -0.04, 0.86), 0.06, 0.035, sack),
               arm, 'hand.' + s)
        attach(tube(f'{n}_talent_tie_{s}', (x, -0.04, 0.80), (x, -0.04, 0.83), 0.052, 0.052, tie,
                    rim_round=0.008), arm, 'hand.' + s)
        for k in range(4):
            attach(sphere(f'{n}_coin_{s}{k}', (x - 0.06 + k * 0.04, -0.15, 0.64 + (k % 2) * 0.05), 0.032,
                          silver, scale=(1.0, 0.3, 1.0)), arm, 'hand.' + s)
    # La segunda muda de ropa doblada al hombro
    cloth = _M(n, 'cloth2', '#a83a6a')
    attach(box(f'{n}_garment', (0.20, 0.06, 1.47), (0.18, 0.26, 0.08), cloth, round=0.9), arm, 'chest')
    attach(box(f'{n}_garment_fold', (0.25, 0.12, 1.28), (0.08, 0.16, 0.34), cloth, round=0.9,
               taper=(0.8, 1.0)), arm, 'chest')


def _extras_benadad(n, arm):
    gold = _M(n, 'gold', '#d9ab3a')
    red = _M(n, 'red', '#8f1d2c')
    _drop(n, 'hair_fringe', 'hair_fringe2')
    # Tiara asiria: cono truncado alto con bandas y un pincho
    _H(arm, tube(f'{n}_tiara', (0, 0.01, 1.70), (0, 0.02, 2.00), 0.152, 0.120, gold, rim_round=0.02))
    for k, z in enumerate((1.75, 1.84, 1.93)):
        _H(arm, tube(f'{n}_tiara_band{k}', (0, 0.01, z), (0, 0.01, z + 0.028), 0.158 - k * 0.012,
                     0.155 - k * 0.012, red, caps=False, rim_round=0.008))
    _H(arm, tube(f'{n}_tiara_spike', (0, 0.02, 2.00), (0, 0.02, 2.10), 0.05, 0.008, gold))
    for k in range(6):
        a = math.radians(k * 60)
        _H(arm, sphere(f'{n}_rosette{k}', (math.sin(a) * 0.15, -math.cos(a) * 0.15 + 0.01, 1.89), 0.022,
                       gold if k % 2 else red))
    # Barba rizada: filas de rizos sobre la barba larga
    curl = _M(n, 'curl', '#1e161c')
    for row in range(3):
        for k in range(4 - (row == 2)):
            x = -0.06 + k * 0.04 + (0.02 if row == 2 else 0.0)
            _H(arm, sphere(f'{n}_curl{row}{k}', (x, -0.100 + row * 0.010, 1.500 - row * 0.045), 0.026,
                           curl))
    # Hombreras de oro con cabeza de leon (esfera + melena)
    for s, x in (('L', 0.26), ('R', -0.26)):
        attach(sphere(f'{n}_paul_{s}', (x, 0.0, 1.44), 0.11, gold, scale=(1.1, 1.0, 0.75)), arm, 'chest')
        attach(sphere(f'{n}_paul_gem_{s}', (x * 1.05, -0.09, 1.45), 0.03, red), arm, 'chest')
    # Espada ancha: se ensancha la hoja que pone build_humanoid
    ob = bpy.data.objects.get(f'{n}_blade')
    if ob is not None:
        for v in ob.data.vertices:
            v.co.y = -0.07 + (v.co.y + 0.07) * 1.6
            v.co.z = 0.95 + (v.co.z - 0.95) * 1.12


EXTRAS = {
    'boss_capitan': _extras_capitan,
    'boss_sacerdote': _extras_sacerdote,
    'boss_sumo_sacerdote': _extras_sumo,
    'boss_desesperacion': _extras_desesperacion,
    'boss_espiritu': _extras_espiritu,
    'boss_acreedor': _extras_acreedor,
    'boss_muerte': _extras_muerte,
    'boss_guiezi': _extras_guiezi,
    'boss_benadad': _extras_benadad,
}


def build_boss(name, spec):
    """build_humanoid + las piezas propias del jefe. Devuelve el mismo dict que aquel."""
    char = C.build_humanoid(name, spec)
    fn = EXTRAS.get(name)
    if fn is not None:
        fn(name, char['arm'])
    return char
