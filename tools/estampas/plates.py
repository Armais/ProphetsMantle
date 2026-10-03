"""Las 46 estampas, una por relato, en el ORDEN de `Ev::` (src/game/events.h).

Cada lamina es una funcion `(cv) -> grado` que pinta sobre el lienzo y devuelve los
parametros del post (`core.finish`). Regla de composicion, la misma en todas:

1. cielo y luz primero (de donde viene la luz decide la luz de borde de todo lo demas);
2. de lejos a cerca, con niebla (`cv.haze`) que baja con la distancia;
3. UN foco: la figura o el objeto del pasaje, en el tercio, a contraluz;
4. el primer plano oscuro enmarca (matas, rocas) y el polvo/pavesas da aire.

Nada contradice 1-2 Reyes (regla del proyecto) y ninguna lamina lleva texto.
"""
import math

import numpy as np

from .core import *  # noqa: F401,F403 (las laminas usan todo el vocabulario)
from .props import *  # noqa: F401,F403

ORDER = [
    'CUERVOS_QUERIT', 'RASTREADOR_DE_ACAB', 'ARROYO_SECO',
    'VIUDA_SAREPTA', 'TINAJA_HARINA', 'HIJO_DE_LA_VIUDA',
    'ALTAR_CARMELO', 'ABDIAS', 'DOS_PENSAMIENTOS', 'NUBECILLA',
    'BOCA_CUEVA', 'ENEBRO_ANGEL', 'TRES_UNCIONES', 'SIETE_MIL',
    'LLAMADO_ELISEO', 'VINA_NABOT', 'TERCER_CAPITAN',
    'AGUAS_JERICO', 'CINCUENTA_BUSCAN', 'MUCHACHOS_BETEL', 'HIEL_DE_JERICO',
    'ACEITE_VIUDA', 'ZANJAS_MOAB', 'MUERTE_EN_LA_OLLA', 'PANES_CEBADA', 'REY_DE_MOAB',
    'APOSENTO_SUNAMITA', 'NINO_SUNAMITA', 'HACHA_QUE_FLOTA', 'TIERRAS_SUNAMITA',
    'NAAMAN_PUERTA', 'MUCHACHA_CAUTIVA', 'TIERRA_DE_ISRAEL',
    'CRIADO_ELISEO', 'SIRIOS_DOTAN', 'CUATRO_LEPROSOS', 'PRINCIPE_SAMARIA',
    'HAZAEL_BENADAD', 'UNCION_JEHU',
    'ASC_GILGAL', 'ASC_BETEL', 'ASC_JERICO', 'ASC_JORDAN',
    'EPI_JEHU', 'EPI_ARCO_JOAS', 'EPI_HUESOS',
]

PLATES = {}


def plate(fn):
    PLATES[fn.__name__.upper()] = fn
    return fn


# =============================================================================
# Cielos de uso comun
# =============================================================================
def dawn(cv, sx=150, sy=52, horizon=60):
    sky(cv, [(0, '#2a2a4a'), (25, '#6a5a7a'), (45, '#d8907a'), (horizon, '#ffd8a0')])
    sun(cv, sx, sy, 4.5, halo='#ffc27a', halo_r=70)
    cv.haze = '#e8b890'


def day(cv, sx=40, sy=18, horizon=62):
    sky(cv, [(0, '#3a6aa8'), (35, '#7aa8d0'), (horizon, '#e8e0c8')])
    sun(cv, sx, sy, 4.0, halo='#fff0c0', halo_r=55, amount=0.7)
    cv.haze = '#d8dcd0'


def dusk(cv, sx=60, sy=50, horizon=60):
    sky(cv, [(0, '#1e1a3a'), (22, '#5a3a5a'), (42, '#c0604a'), (horizon, '#ffb070')])
    sun(cv, sx, sy, 5.0, color='#ffe0a0', halo='#ff8a4a', halo_r=80)
    cv.haze = '#d07a5a'


def night(cv, mx=160, my=18, horizon=62):
    sky(cv, [(0, '#0a0c1e'), (40, '#1a2244'), (horizon, '#3a3a5a')])
    stars(cv, 120, horizon - 8, seed=cv.seed)
    moon(cv, mx, my, 4.0)
    cv.haze = '#3a4466'


def storm(cv, horizon=60):
    sky(cv, [(0, '#2a2a34'), (30, '#4a4a58'), (horizon, '#8a8478')])
    cv.sun = (100, -40)
    cv.haze = '#7a7a7a'


# =============================================================================
# Cap. 1 - Querit
# =============================================================================
@plate
def cuervos_querit(cv):
    dawn(cv, 148, 50)
    mountains(cv, 60, 14, '#8a6a7a', seed=2, haze=0.6, rim=0.3, freq=0.03)
    profile(cv, canyon_fn(118, 70, 70, 34, seed=4, rough=4), '#8a5e48', fog=0.35, haze=0.25, rim=0.9)
    profile(cv, wall_fn(4, 66, 82, 74, seed=9, rough=6), '#94603e', grad_to='#4a2c1e', rim=1.1, tex=0.32)
    profile(cv, wall_fn(198, 150, 82, 62, seed=3, rough=5), '#4e3026', grad_to='#2a1a14', rim=0.5, tex=0.3)
    ground(cv, 80, '#9a744e', '#3e2c20', seed=1)
    glow_spot(cv, 120, 84, 40, '#ffb070', 0.18)
    river(cv, [(116, 79), (108, 83), (118, 88), (98, 94), (70, 100.5)],
          [(124, 79), (120, 83), (130, 88), (116, 94), (102, 100.5)])
    rocks(cv, 142, 90, 5, '#6a5444', seed=1)
    palm(cv, 30, 98, 50, seed=2, lean=0.12)
    cutout(cv, 'elias_kneel_up_l', 84, 97, 44, backlit=0.45, rim=1.0, flip=True)
    raven(cv, 118, 34, 7.5, facing=-1, flying=True, bread_col='#d8a060', wing=0.1)
    raven(cv, 146, 22, 5.5, facing=-1, flying=True, bread_col='#b84a3a', wing=-0.2)
    light_rays(cv, 148, 50, '#ffd8a0', n=8, spread=1.2, length=150, strength=0.10, angle=math.pi * 0.85)
    bush(cv, 188, 100, 36, 16, '#2e2c1a', seed=4)
    rocks(cv, 8, 101, 9, '#2a2018', seed=21, n=2, rim=0.2)
    tuft(cv, 56, 100.5, 7, '#4a5a2a', seed=4)
    particles(cv, 40, (0, 20, W, 90), '#ffe6b8', size=(0.15, 0.35), seed=8, glow=0.6, alpha=(0.2, 0.6))
    return dict(warm=0.6)


@plate
def rastreador_de_acab(cv):
    day(cv, 168, 16)
    mountains(cv, 58, 16, '#a08a80', seed=3, haze=0.55, rim=0.3)
    profile(cv, wall_fn(200, 120, 74, 46, seed=5, rough=5), '#8a6448', grad_to='#4a3424', rim=0.9, tex=0.3)
    profile(cv, wall_fn(0, 64, 76, 56, seed=11, rough=6), '#6a4a36', grad_to='#3a281c', rim=0.5, tex=0.3)
    ground(cv, 76, '#a88a62', '#5a4430', seed=2)
    river(cv, [(96, 75), (104, 80), (92, 86), (110, 93), (128, 100.5)],
          [(104, 75), (114, 80), (104, 86), (126, 93), (156, 100.5)])
    cutout(cv, 'soldado_drink_l', 138, 94, 40, backlit=0.0, rim=0.6, tint='#fff4e0')
    draw(cv, capsule(162, 95, 157, 52, 0.5), '#5a3a1a')
    draw(cv, poly([(155.8, 52), (157.6, 44), (159.4, 52)]), '#c8ccd0', rim=0.8)
    rocks(cv, 58, 93, 10, '#6a5848', seed=5, n=4)
    cutout(cv, 'elias_stand_r', 40, 96, 38, backlit=0.6, rim=0.5)
    rocks(cv, 26, 101, 13, '#3a2e24', seed=9, n=3, rim=0.2)
    rocks(cv, 70, 101, 8, '#3a2e24', seed=19, n=2, rim=0.2)
    bush(cv, 192, 101, 26, 13, '#3a3a1c', seed=1)
    particles(cv, 25, (0, 30, W, 95), '#fff0d0', size=(0.15, 0.3), seed=4, alpha=(0.2, 0.5))
    return dict()


@plate
def arroyo_seco(cv):
    sky(cv, [(0, '#6a7088'), (30, '#c0aa8a'), (62, '#f4dca0')])
    sun(cv, 104, 20, 6.5, color='#fffae8', halo='#ffe0a0', halo_r=95, amount=1.0)
    cv.haze = '#ead2a0'
    mountains(cv, 60, 18, '#b88e6a', seed=4, haze=0.5, rim=0.3)
    profile(cv, wall_fn(0, 64, 72, 44, seed=2, rough=4), '#a87a52', grad_to='#6a4a30', rim=0.8)
    profile(cv, wall_fn(200, 150, 72, 36, seed=6, rough=4), '#9a7048', grad_to='#5a3e28', rim=0.8)
    ground(cv, 70, '#caa070', '#8a6440', seed=5, tex=0.2)
    bed_ = poly([(84, 70), (118, 70), (152, 84), (180, 100.5), (32, 100.5), (60, 84)])
    draw(cv, bed_, '#b89060', grad=('v', col('#c8a070'), col('#946a40')), tex=0.3, tex_scale=0.5)
    rng = np.random.default_rng(3)
    for i in range(70):
        x = rng.uniform(42, 172)
        y = rng.uniform(74, 100)
        pts = [(x, y)]
        for k in range(4):
            pts.append((pts[-1][0] + rng.uniform(-3.5, 3.5), pts[-1][1] + rng.uniform(-1.2, 1.2)))
        draw(cv, polyline(pts, 0.22 * (y / 90), 0.05), '#5a3a22', alpha=0.55)
    river(cv, [(100, 70), (102, 80), (99, 88), (104, 100.5)], [(101, 70), (103.4, 80), (101.2, 88), (107.5, 100.5)],
          deep='#4a7a8a', shallow='#8ab8c8', bank='#7a5a3a')
    rocks(cv, 154, 94, 7, '#9a7a5a', seed=2)
    rocks(cv, 70, 98, 7, '#8a6a4c', seed=12, n=2)
    cutout(cv, 'elias_bow_r', 70, 98, 42, backlit=0.15, rim=0.5, tint='#fff0d0')
    palm(cv, 182, 100, 40, leaf='#7a7a3a', seed=5, lean=-0.18)
    particles(cv, 30, (0, 20, W, 90), '#fff4d8', size=(0.15, 0.3), seed=2, alpha=(0.2, 0.5))
    return dict(warm=1.0, sat=0.95)


# =============================================================================
# Cap. 2 - Sarepta
# =============================================================================
@plate
def viuda_sarepta(cv):
    day(cv, 36, 22)
    sea(cv, 58, '#a8c8d0', '#4a7a90')
    cv.haze = '#d8dcd0'
    wall_city(cv, 96, 210, 70, 18, '#d6ba90', towers=3, haze=0.15, gate=150)
    for hx, hw, hh in ((112, 9, 8), (134, 8, 9), (172, 10, 7), (194, 8, 8)):
        house(cv, hx, 52, hw, hh, wall='#e0c8a0', roof='#9a7a58', haze=0.3, door=False)
    ground(cv, 76, '#c8a87a', '#8a6a48', seed=3)
    road = poly([(140, 76), (160, 76), (200, 100.5), (100, 100.5)])
    draw(cv, road, '#dcc49a', tex=0.2, alpha=0.55)
    tree(cv, 18, 98, 40, kind='olive', leaf='#6a7a4a', seed=3)
    cutout(cv, 'viuda_gather_l', 70, 98, 42, rim=0.5, tint='#fff4e0')
    for k in range(5):
        draw(cv, capsule(80 + k * 2.6, 99, 88 + k * 2.4, 97, 0.45), '#6a4a2a', shade=0.6, rim=0.4)
    cutout(cv, 'elias_walk_l', 138, 96, 40, rim=0.6, tint='#fff4e0')
    tuft(cv, 110, 100, 7, '#7a7a3a', seed=5)
    tuft(cv, 186, 101, 9, '#6a6a32', seed=6)
    particles(cv, 20, (0, 20, W, 90), '#ffffff', size=(0.12, 0.25), seed=2, alpha=(0.15, 0.4))
    return dict(sat=1.04)


@plate
def tinaja_harina(cv):
    interior(cv, wall='#8a6a50', floor='#5a4434', floor_y=68, light_at=(104, 56), seed=2)
    window(cv, 162, 34, 18, 22, sky_top='#7aa8d0', sky_bot='#e8e0c8', star=False, light='#fff0c8')
    table(cv, 160, 90, 26, h=9)
    bread(cv, 154, 81, 7)
    cruse(cv, 166, 81, 8, color='#d8b878')
    jar(cv, 106, 96, 36, color='#b8743a')
    glow_spot(cv, 106, 61, 20, '#fff4d8', 0.6)
    particles(cv, 30, (94, 46, 118, 62), '#fff6e0', size=(0.12, 0.3), seed=4, glow=0.8)
    cutout(cv, 'viuda_offer_r', 74, 98, 44, rim=0.7, tint='#ffe0b0')
    sack(cv, 26, 99, 16)
    jar(cv, 42, 100, 18, color='#a8643a')
    return dict(warm=0.8)


@plate
def hijo_de_la_viuda(cv):
    interior(cv, wall='#5a4a5a', floor='#3a3040', floor_y=66, light_at=(110, 60), light='#ffb860', seed=4, dark=0.7)
    window(cv, 36, 28, 16, 20)
    ty = bed(cv, 120, 96, 56, h=7)
    cutout(cv, 'nino_lie', 120, ty + 3, 30, rim=0.6, tint='#ffd8a8')
    oil_lamp(cv, 160, 82, 2.8)
    table(cv, 166, 90, 16)
    cutout(cv, 'elias_kneel_up_l', 80, 99, 44, rim=0.8, tint='#ffcc90', flip=True)
    light_rays(cv, 116, 0, '#fff4d8', n=6, spread=0.5, length=95, strength=0.12, width=0.05)
    particles(cv, 25, (60, 20, 160, 90), '#fff0d0', size=(0.12, 0.28), seed=7, glow=0.8, alpha=(0.2, 0.6))
    return dict(warm=0.4, vignette=0.5)


# =============================================================================
# Cap. 3 - El Carmelo
# =============================================================================
@plate
def altar_carmelo(cv):
    dusk(cv, 36, 46)
    sea(cv, 58, '#c08a7a', '#4a3a5a', sun_x=36)
    mountains(cv, 62, 10, '#6a4a5a', seed=7, haze=0.4, rim=0.4)
    hills(cv, 72, 14, '#7a5a48', seed=3, rim=0.8, grad_to='#3a2a24')
    crowd_cut(cv, CROWD_BAAL, 140, 196, 70, 18, n=9, seed=3, haze=0.35, backlit=0.4, rim=0.9, depth=3)
    altar_stones(cv, 96, 92, 34, 14, seed=2)
    for k in range(4):
        jar(cv, 54 + k * 8, 99, 11, color='#a8643a', handles=False)
    cutout(cv, 'elias_arms_up_l', 136, 99, 42, backlit=0.35, rim=1.0)
    light_rays(cv, 96, -10, '#fff0d0', n=7, spread=0.35, length=110, strength=0.12, width=0.04)
    particles(cv, 30, (0, 30, W, 95), '#ffd8a0', size=(0.15, 0.3), seed=5, alpha=(0.2, 0.5))
    return dict(warm=0.4)


@plate
def abdias(cv):
    night(cv, 36, 16)
    mountains(cv, 60, 18, '#2a3050', seed=2, haze=0.3, rim=0.5)
    rock_cave(cv, 134, 86, 36, 48, rock='#3e3846', side=1, rim=0.5, seed=3)
    glow_spot(cv, 134, 74, 26, '#ffb060', 0.4)
    for k in range(6):
        figure(cv, 122 + k * 4.8, 85, 10, '#3a3038', hood='#2a2228', pose='stand', rim=0.7, rim_col='#ffb060',
               alpha=0.9)
    ground(cv, 86, '#34303e', '#16121c', seed=6)
    cutout(cv, 'abdias_lamp_r', 74, 98, 42, rim=0.7, tint='#c8c8ff', light=0.9)
    bread(cv, 96, 99, 5)
    jar(cv, 104, 100, 9, color='#8a6a4a', handles=False)
    particles(cv, 20, (0, 0, W, 60), '#ffffff', size=(0.1, 0.25), seed=9, glow=0.8)
    return dict(sat=0.95)


@plate
def dos_pensamientos(cv):
    day(cv, 160, 16)
    mountains(cv, 58, 14, '#8a9aa8', seed=4, haze=0.6)
    hills(cv, 70, 18, '#8a8a5a', seed=8, haze=0.2, rim=0.5, grad_to='#5a5a38')
    crowd_cut(cv, CROWD_PUEBLO, 4, 196, 72, 16, n=26, seed=2, haze=0.35, depth=4)
    altar_stones(cv, 34, 78, 14, 6, trench_water=False, seed=4)
    altar_stones(cv, 168, 78, 14, 6, trench_water=False, seed=7, stone='#7a6a7a')
    rocks(cv, 100, 90, 14, '#8a7a68', seed=4, n=3)
    cutout(cv, 'elias_point_r', 100, 84, 34, rim=0.8, tint='#fff4e0')
    crowd_cut(cv, CROWD_PUEBLO, -4, 204, 94, 26, n=12, seed=5, haze=0.0, depth=6)
    return dict()


@plate
def nubecilla(cv):
    sky(cv, [(0, '#3a3a48'), (35, '#7a7a88'), (56, '#c8b8a0')])
    cv.sun = (150, 50)
    cv.haze = '#8a8a8a'
    sea(cv, 56, '#a8b0b0', '#3a5060')
    cloud(cv, 162, 52, 9, 3.2, '#686878', shade_col='#3a3a48', seed=2, rim=0.5)
    glow_spot(cv, 162, 50, 28, '#fff0d0', 0.25)
    cloud_band(cv, 16, 10, '#2a2a34', alpha=0.75, seed=3)
    hills(cv, 74, 14, '#5a5048', seed=9, rim=0.6, grad_to='#2a2420')
    cutout(cv, 'elias_bow_l', 64, 92, 42, rim=0.7, tint='#e8e0d0', flip=True)
    cutout(cv, 'criado_point_r', 118, 90, 38, rim=0.7, tint='#e8e0d0')
    rocks(cv, 186, 101, 10, '#2a2420', seed=8, n=2, rim=0.2)
    particles(cv, 18, (0, 20, W, 90), '#ffffff', size=(0.1, 0.22), seed=7, alpha=(0.15, 0.35))
    return dict(sat=0.9)


# =============================================================================
# Cap. 4 - Horeb
# =============================================================================
@plate
def boca_cueva(cv):
    sky(cv, [(0, '#0a0a18'), (40, '#2a2240'), (62, '#6a3a3a')])
    stars(cv, 60, 40, seed=4)
    cv.sun = (40, 50)
    cv.haze = '#3a2a3a'
    mountains(cv, 64, 26, '#1a1622', seed=5, haze=0.2, rim=0.5)
    for k in range(7):
        fire(cv, 6 + k * 11, 64 + (k % 3) * 1.5, 5 + (k % 2) * 3, seed=3 + k, glow=0.7)
    glow_spot(cv, 40, 58, 60, '#ff6a2a', 0.4)
    for k in range(3):
        draw(cv, polyline(bezier((20 + k * 20, 60), (14 + k * 24, 40), (30 + k * 30, 10), n=12), 2 + k, 6 + k * 2),
             '#2a1a24', alpha=0.4, soft=3)
    for k in range(10):
        draw(cv, polyline(bezier((-10 + k * 10, 18 + k * 4), (40 + k * 9, 8 + k * 5), (110 + k * 8, 22 + k * 3), n=12),
                          0.25, 0.05), '#a8a0c0', alpha=0.22)
    rng = np.random.default_rng(4)
    for k in range(6):
        x0 = rng.uniform(10, 90)
        pts = [(x0, 74 + k * 3)]
        for j in range(5):
            pts.append((pts[-1][0] + rng.uniform(2, 5), pts[-1][1] + rng.uniform(-1, 1)))
        draw(cv, polyline(pts, 0.4, 0.15), '#0a0608', alpha=0.8)
    rock_cave(cv, 136, 98, 44, 64, rock='#3a3240', side=1, rim=0.6, seed=5)
    ground(cv, 92, '#2a2430', '#120e14', seed=6)
    cutout(cv, 'elias_veiled_r', 132, 98, 42, backlit=0.3, rim=1.0, rim_col='#ffb080', tint='#c0b0d0')
    particles(cv, 30, (0, 30, 110, 90), '#ffb070', size=(0.15, 0.3), seed=5, glow=0.9, alpha=(0.3, 0.8))
    return dict(sat=1.0, vignette=0.5)


@plate
def enebro_angel(cv):
    sky(cv, [(0, '#3a3a5a'), (30, '#8a7a90'), (60, '#f0c8a0')])
    sun(cv, 36, 58, 4.0, halo='#ffd0a0', halo_r=60, amount=0.6)
    cv.haze = '#e8c8a8'
    mountains(cv, 62, 12, '#a08a90', seed=3, haze=0.5)
    ground(cv, 70, '#d0a87a', '#8a6844', seed=4, tex=0.2)
    bush(cv, 92, 86, 56, 30, '#6a7a3a', seed=6, berries='#e8e0c8')
    cutout(cv, 'elias_lie', 92, 96, 38, rim=0.5, tint='#fff0d8')
    angel_light(cv, 158, 98, 40)
    cutout(cv, 'angel_reach_l', 158, 98, 42, rim=1.2, rim_col='#ffffff', tint='#fffaf0', glow=0.25,
           glow_col='#fff0d0')
    draw(cv, ellipse(126, 99, 6, 1.8), '#3a2a1a')
    bread(cv, 126, 98, 6, color='#e0a860')
    cruse(cv, 136, 100, 8, color='#c8b88a')
    particles(cv, 40, (110, 30, 196, 98), '#fff8e0', size=(0.12, 0.3), seed=6, glow=1.0, alpha=(0.3, 0.8))
    return dict(warm=0.3)


@plate
def tres_unciones(cv):
    sky(cv, [(0, '#5a7aa8'), (35, '#a8b8c8'), (62, '#f0d8b0')])
    sun(cv, 40, 22, 4.0, halo='#fff0c0', halo_r=55, amount=0.6)
    cv.haze = '#e8d8c0'
    mountains(cv, 58, 18, '#a89aa0', seed=7, haze=0.6)
    hills(cv, 68, 10, '#c8a87a', seed=2, haze=0.3)
    ground(cv, 72, '#d8b080', '#9a7048', seed=8, tex=0.2)
    road = poly([(100, 72), (108, 72), (160, 100.5), (60, 100.5)])
    draw(cv, road, '#e8c898', tex=0.2, alpha=0.7)
    wall_city(cv, 94, 116, 64, 5, '#c8b8a0', towers=2, haze=0.6)
    cutout(cv, 'elias_walk_horn_r', 104, 98, 44, rim=0.8, tint='#fff4e0')
    for k, c in enumerate(('#ffd070', '#ffe8a0', '#fff4d0')):
        glow_spot(cv, 130 + k * 22, 26 - k * 3, 10, c, 0.4)
        draw(cv, circle(130 + k * 22, 26 - k * 3, 1.1), '#fffaf0', glow=1.2)
    palm(cv, 26, 100, 40, seed=4)
    tuft(cv, 170, 100, 8, '#8a8a4a', seed=5)
    return dict(warm=0.4)


@plate
def siete_mil(cv):
    night(cv, 150, 20)
    mountains(cv, 62, 16, '#1c2240', seed=2, haze=0.2, rim=0.4)
    hills(cv, 74, 12, '#141a30', seed=6, rim=0.3)
    rng = np.random.default_rng(7)
    for i in range(160):
        x = rng.uniform(0, W)
        y = rng.uniform(62, 98)
        draw(cv, circle(x, y, 0.25 + (y - 62) * 0.008), '#ffd890', glow=0.9, alpha=rng.uniform(0.5, 1.0))
    profile(cv, wall_fn(0, 80, 86, 50, seed=4, rough=4), '#241e2c', grad_to='#100c14', rim=0.6, tex=0.2)
    cutout(cv, 'elias_lookup_r', 40, 58, 40, backlit=0.5, rim=0.8, rim_col='#c0d0ff')
    return dict(sat=1.0)


# =============================================================================
# Cap. 5 - El llamado
# =============================================================================
@plate
def llamado_eliseo(cv):
    dusk(cv, 164, 46)
    hills(cv, 62, 8, '#a87a5a', seed=2, haze=0.4)
    ground(cv, 64, '#8a6a3a', '#4a3820', seed=4, tex=0.2)
    for k in range(7):
        y = 68 + k * 4.6
        draw(cv, capsule(-5, y, 205, y + 1.5, 0.3 + k * 0.05), '#5a4228', alpha=0.55)
    quadruped(cv, 30, 88, 20, body='#7a4a2a', facing=1, kind='ox', rim=1.0)
    quadruped(cv, 52, 97, 25, body='#8a5a34', facing=1, kind='ox', rim=1.0)
    draw(cv, capsule(52, 74, 84, 76, 0.6), '#5a3a1a', rim=0.8)
    fire(cv, 120, 99, 8, seed=3)
    pot(cv, 120, 97, 11, fire_under=False)
    cutout(cv, 'eliseo_lab_stand_r', 98, 100, 44, backlit=0.35, rim=1.0)
    cutout(cv, 'elias_walk_r', 168, 80, 24, backlit=0.5, rim=1.0, haze=0.2)
    particles(cv, 30, (96, 50, 150, 98), '#ffb060', size=(0.15, 0.3), seed=3, glow=1.0)
    return dict(warm=0.6)


@plate
def vina_nabot(cv):
    day(cv, 160, 20)
    hills(cv, 58, 8, '#a8b0a0', seed=3, haze=0.5)
    tower(cv, 176, 60, 10, 22, color='#c8b090', window_y=16, haze=0.35)
    ground(cv, 60, '#9a9a5a', '#5a5a32', seed=2)
    vines(cv, 62, 92, rows=4, seed=1)
    cutout(cv, 'acab_stand_l', 128, 96, 42, rim=0.8, tint='#fff4e0')
    cutout(cv, 'elias_point_r', 66, 99, 44, rim=0.8, tint='#fff4e0')
    light_rays(cv, 66, -10, '#fff6d8', n=6, spread=0.3, length=110, strength=0.14, width=0.04)
    vines(cv, 100, 104, rows=2, seed=9)
    return dict()


@plate
def tercer_capitan(cv):
    sky(cv, [(0, '#3a2a2a'), (30, '#8a5a4a'), (62, '#e8a070')])
    sun(cv, 36, 46, 4, halo='#ff9050', halo_r=60, amount=0.6)
    cv.haze = '#c88a6a'
    mountains(cv, 60, 14, '#7a5a50', seed=4, haze=0.5)
    hills(cv, 70, 32, '#5a4234', seed=6, rim=0.8, freq=0.012, grad_to='#2a1e18')
    rng = np.random.default_rng(3)
    for i in range(18):
        x = rng.uniform(10, 190)
        draw(cv, ellipse(x, rng.uniform(84, 98), rng.uniform(3, 6), 0.8), '#1a1210', alpha=0.6)
    cutout(cv, 'elias_stand_l', 156, 52, 30, rim=1.0, backlit=0.2)
    for k in range(5):
        cutout(cv, 'soldado_kneel_l', 22 + k * 15, 92 - (k % 2) * 3, 30, rim=0.8, tint='#ffd8b8')
    cutout(cv, 'capitan_kneel_l', 112, 99, 42, rim=0.9, tint='#ffd8b8')
    particles(cv, 20, (0, 30, W, 90), '#ffc090', size=(0.12, 0.28), seed=6, alpha=(0.2, 0.5))
    return dict(warm=0.6)


# =============================================================================
# Cap. 6 - Jerico (Acto II: el color se enfria)
# =============================================================================
@plate
def aguas_jerico(cv):
    day(cv, 36, 18)
    hills(cv, 60, 10, '#a8a890', seed=2, haze=0.5)
    wall_city(cv, 96, 206, 64, 16, '#c8b48a', towers=3, haze=0.25, gate=150)
    palm(cv, 116, 68, 22, seed=3, lean=-0.1)
    palm(cv, 188, 66, 24, seed=4)
    ground(cv, 70, '#a8a070', '#5a6040', seed=3)
    pool(cv, 96, 90, 34, 7, color='#4a7a5a', rim_col='#9ac8a8')
    reeds(cv, 110, 132, 92, 9, seed=5)
    cutout(cv, 'eliseo_bowl_r', 58, 99, 44, rim=0.8, tint='#fff8f0')
    for k in range(7):
        draw(cv, circle(78 + k * 2.6, 78 + k * 1.4, 0.45), '#ffffff', glow=0.8)
    cutout(cv, 'hombre_jerico_point_l', 150, 99, 42, rim=0.7, tint='#fff4e0')
    return dict(sat=1.02)


@plate
def cincuenta_buscan(cv):
    dusk(cv, 150, 44)
    mountains(cv, 60, 26, '#6a4a5a', seed=6, haze=0.35, rim=0.6, freq=0.02)
    hills(cv, 70, 18, '#4a3a3a', seed=8, rim=0.5)
    rng = np.random.default_rng(2)
    for i in range(16):
        x = rng.uniform(20, 180)
        y = rng.uniform(46, 68)
        figure(cv, x, y, 5, '#3a2e30', cloth='#5a4a40', rim=0.6, haze=0.3)
        draw(cv, circle(x + 1, y - 5, 0.45), '#ffc070', glow=1.0)
    wall_city(cv, -6, 54, 92, 14, '#8a7058', towers=2)
    cutout(cv, 'eliseo_stand_r', 104, 99, 44, backlit=0.4, rim=1.0)
    cutout(cv, 'hijo_prof_stand_l', 148, 99, 42, backlit=0.4, rim=1.0)
    return dict(warm=0.4)


@plate
def muchachos_betel(cv):
    day(cv, 160, 18)
    hills(cv, 60, 10, '#9aa090', seed=4, haze=0.5)
    tree(cv, 18, 78, 34, leaf='#3a5a2a', seed=1)
    tree(cv, 48, 74, 28, leaf='#4a6a30', seed=2)
    tree(cv, 4, 86, 40, leaf='#2e4a24', seed=3)
    ground(cv, 74, '#b8a070', '#6a5a3a', seed=5)
    road = poly([(116, 74), (136, 74), (196, 100.5), (96, 100.5)])
    draw(cv, road, '#d8c090', tex=0.2, alpha=0.6)
    quadruped(cv, 42, 96, 26, body='#4a3020', facing=1, kind='bear', gait=0.6, rim=0.8)
    quadruped(cv, 72, 88, 20, body='#5a3a24', facing=1, kind='bear', gait=0.3, rim=0.8)
    crowd_cut(cv, ['pueblo_5', 'pueblo_1', 'pueblo_7'], 96, 128, 84, 22, n=5, seed=4, haze=0.0, depth=4)
    cutout(cv, 'eliseo_walk_r', 158, 99, 42, rim=0.7, tint='#fff8f0')
    return dict()


@plate
def hiel_de_jerico(cv):
    sky(cv, [(0, '#3a4a6a'), (35, '#a8a0a0'), (62, '#e8c8a0')])
    sun(cv, 176, 40, 4, halo='#ffd0a0', halo_r=60, amount=0.6)
    cv.haze = '#d8c0a8'
    ground(cv, 74, '#b8a07a', '#6a5a40', seed=2)
    wall_city(cv, 30, 206, 78, 32, '#b8a07c', towers=2, gate=100, seed=2)
    rocks(cv, 18, 96, 8, '#8a7a68', seed=1)
    cutout(cv, 'hombre_jerico_stand_l', 154, 99, 44, rim=0.8, tint='#fff0e0')
    cutout(cv, 'eliseo_stand_r', 56, 99, 44, rim=0.8, tint='#fff0e0')
    return dict(warm=0.3)


# =============================================================================
# Cap. 7 - El aceite de la viuda
# =============================================================================
@plate
def aceite_viuda(cv):
    interior(cv, wall='#7a5a40', floor='#4a3626', floor_y=62, light_at=(100, 56), seed=6)
    rng = np.random.default_rng(4)
    for r in range(3):
        for k in range(9):
            x = 8 + k * 22 + rng.uniform(-3, 3) + r * 7
            if 84 < x < 126:
                continue
            y = 72 + r * 10
            hh = 12 + r * 3
            jar(cv, x, y, hh, color=['#b8743a', '#a8643a', '#c8844a'][(k + r) % 3], handles=(k % 2 == 0))
            if (k + r) % 3 != 0:
                draw(cv, ellipse(x, y - hh * 0.99, hh * 0.17, 0.55), '#e8c040', glow=0.5)
    cutout(cv, 'mujer_prof_pour_r', 96, 100, 44, rim=0.7, tint='#ffe0b0')
    draw(cv, capsule(116, 76, 118, 90, 0.6, 0.35), '#f0c040', glow=1.0)
    jar(cv, 120, 100, 14, color='#b8743a')
    glow_spot(cv, 118, 88, 16, '#ffd060', 0.45)
    return dict(warm=0.7)


@plate
def zanjas_moab(cv):
    sky(cv, [(0, '#3a2a3a'), (28, '#8a4a4a'), (52, '#f09060')])
    sun(cv, 108, 48, 5, halo='#ff7040', halo_r=80)
    cv.haze = '#c8704a'
    mountains(cv, 54, 14, '#6a3a3a', seed=3, haze=0.4)
    for k in range(6):
        tent(cv, 18 + k * 36, 58, 9, 5, haze=0.45)
    ground(cv, 58, '#8a5a3a', '#3a2418', seed=2)
    vx, vy = 112, 56
    for k in range(-5, 7):
        x_near = vx + k * 34
        pts = bezier((vx + k * 3.0, 60), (vx + k * 12, 74), (x_near, 101), n=18)
        trench(cv, pts, 0.9 + abs(k) * 0.05)
    cutout(cv, 'eliseo_stand_r', 40, 99, 44, backlit=0.5, rim=1.0)
    figure(cv, 66, 98, 30, '#7a5a3a', cloth='#c8b890', pose='carry', facing=1, backlit=0.5, rim=1.0)
    return dict(warm=0.5)


@plate
def muerte_en_la_olla(cv):
    sky(cv, [(0, '#2a2a3a'), (30, '#6a5a5a'), (60, '#c09070')])
    sun(cv, 170, 52, 3.5, halo='#ff9a60', halo_r=50, amount=0.5)
    cv.haze = '#9a7a6a'
    hills(cv, 62, 8, '#6a5a4a', seed=3, haze=0.4)
    ground(cv, 68, '#7a6a4a', '#3a3020', seed=4)
    house(cv, 168, 72, 26, 15, wall='#a08a6a', roof='#6a5a3a')
    pot(cv, 100, 99, 28, seed=2)
    rng = np.random.default_rng(5)
    for k in range(6):
        draw(cv, circle(90 + k * 4, 76 + rng.uniform(-1, 1), 1.5), '#8aa04a', shade=0.8, bevel=0.8)
    cutout(cv, 'hijo_prof_cry_l', 144, 99, 42, rim=0.7, tint='#ffd8b0')
    cutout(cv, 'eliseo_point_r', 58, 99, 44, rim=0.8, tint='#ffe0c0')
    sack(cv, 76, 100, 10, color='#d8c8a0')
    return dict(warm=0.4)


@plate
def panes_cebada(cv):
    day(cv, 36, 18)
    hills(cv, 60, 10, '#b8b08a', seed=6, haze=0.4)
    ground(cv, 64, '#c8b070', '#8a7040', seed=2)
    rng = np.random.default_rng(1)
    for i in range(80):
        x = rng.uniform(0, W)
        y = rng.uniform(64, 74)
        draw(cv, capsule(x, y, x + rng.uniform(-0.5, 0.5), y - 3, 0.2), '#d8b860', alpha=0.7)
    crowd_cut(cv, CROWD_HIJOS + ['pueblo_3', 'pueblo_6'], 118, 200, 80, 26, n=12, seed=7, haze=0.15, depth=8)
    cutout(cv, 'baalsalisa_basket_l', 94, 99, 44, rim=0.8, tint='#fff4e0')
    cutout(cv, 'eliseo_stand_r', 40, 99, 44, rim=0.8, tint='#fff4e0')
    for k in range(7):
        bread(cv, 58 + k * 5, 101 - (k % 2), 5, color='#c8904a')
    return dict(warm=0.3)


@plate
def rey_de_moab(cv):
    sky(cv, [(0, '#1a1418'), (30, '#5a2a2a'), (60, '#c0603a')])
    sun(cv, 156, 52, 4, color='#ffc080', halo='#ff5030', halo_r=70)
    cv.haze = '#8a4a3a'
    for k in range(3):
        draw(cv, polyline(bezier((96 + k * 5, 46), (86 + k * 10, 26), (106 + k * 14, 0), n=14), 3 + k, 8 + k * 2),
             '#2a1e22', alpha=0.45, soft=3)
    wall_city(cv, 20, 180, 78, 30, '#6a4a3a', towers=3, rim=0.8, gate=60)
    cutout(cv, 'mesa_arms_up', 100, 48, 26, backlit=0.8, rim=1.0)
    fire(cv, 100, 48, 3.5, seed=4)
    ground(cv, 86, '#4a3226', '#1a120e', seed=3)
    crowd_cut(cv, ['soldado_sirio_stand_r', 'soldado_acab_stand_l'], 0, 200, 92, 24, n=9, seed=4, backlit=0.7,
              rim=0.9, haze=0.0, depth=7)
    return dict(sat=0.95, vignette=0.5)


# =============================================================================
# Cap. 8 - Sunem
# =============================================================================
@plate
def aposento_sunamita(cv):
    interior(cv, wall='#8a7058', floor='#5a4434', floor_y=64, light_at=(124, 60), seed=8)
    window(cv, 36, 30, 16, 20, sky_top='#3a4a7a', sky_bot='#c8a8a0', star=True)
    bed(cv, 64, 96, 58, h=8, cloth='#d8c8a8')
    table(cv, 124, 92, 18, h=9)
    oil_lamp(cv, 122, 82, 2.6)
    draw(cv, rect(140, 84, 148, 92, r=0.8), '#6a4a2c', shade=0.6, rim=0.4)
    cutout(cv, 'sunamita_lamp_r', 168, 100, 44, rim=0.8, tint='#ffd8a8')
    return dict(warm=0.6)


@plate
def nino_sunamita(cv):
    day(cv, 160, 18)
    mountains(cv, 56, 28, '#7a8a6a', seed=4, haze=0.35, rim=0.6, peaks=[(156, 30, 26)])
    hills(cv, 68, 10, '#9a9a6a', seed=6, haze=0.2)
    ground(cv, 70, '#b8a070', '#6a5a3a', seed=3)
    road = poly([(144, 70), (154, 70), (90, 100.5), (36, 100.5)])
    draw(cv, road, '#d8c090', alpha=0.6, tex=0.2)
    quadruped(cv, 74, 99, 30, body='#8a7a6a', facing=1, kind='donkey', gait=0.8, rim=0.8)
    cutout(cv, 'sunamita_stand_r', 72, 84, 30, rim=0.7, tint='#fff4e0', shadow_k=0.0)
    cutout(cv, 'eliseo_stand_l', 156, 54, 20, rim=0.8, haze=0.2)
    cutout(cv, 'guiezi_stand_r', 136, 74, 24, rim=0.7, haze=0.15)
    particles(cv, 40, (30, 84, 110, 101), '#e8d8b0', size=(0.3, 0.8), seed=4, alpha=(0.15, 0.4), glow=0.0)
    return dict()


@plate
def hacha_que_flota(cv):
    day(cv, 60, 20)
    hills(cv, 58, 10, '#9aa890', seed=3, haze=0.5)
    tree(cv, 156, 64, 28, kind='olive', leaf='#5a7a3a', seed=2)
    tree(cv, 184, 62, 24, kind='olive', leaf='#4a6a30', seed=3)
    ground(cv, 64, '#a89a6a', '#5a5a3a', seed=2)
    river(cv, [(-5, 72), (60, 74), (130, 76), (205, 78)], [(-5, 100.5), (60, 100.5), (130, 100.5), (205, 100.5)],
          deep='#2a5a6a', shallow='#6a9aa8', smooth=False)
    for k in range(4):
        draw(cv, capsule(146 + k * 4, 69 - k * 0.5, 178 + k * 3, 67 - k * 0.5, 0.9), '#8a6a3a', shade=0.6, rim=0.5)
    axe_head(cv, 104, 88, 8)
    glow_spot(cv, 104, 88, 16, '#bfe6ff', 0.55)
    for k in range(3):
        draw(cv, ellipse(104, 91, 7 + k * 3.5, 1.2 + k * 0.7).cut(ellipse(104, 91, 6.3 + k * 3.5, 0.8 + k * 0.7)),
             '#e8f8ff', alpha=0.5 - k * 0.12)
    cutout(cv, 'eliseo_point_r', 52, 76, 34, rim=0.8, tint='#fff8f0')
    draw(cv, capsule(60, 74, 84, 86, 0.5), '#6a4a2a')
    cutout(cv, 'hijo_prof_reach_l', 140, 76, 32, rim=0.7, tint='#fff4e0')
    return dict()


@plate
def tierras_sunamita(cv):
    interior(cv, wall='#6a5a6a', floor='#3a3240', floor_y=64, light_at=(140, 50), light='#ffd090', seed=9,
             beams=False)
    for k in range(4):
        draw(cv, rect(16 + k * 54, 0, 26 + k * 54, 64), '#8a7a7a', shade=0.6, rim=0.4, tex=0.2)
    draw(cv, poly([(136, 64), (176, 64), (200, 100.5), (120, 100.5)]), '#7a1f2c', alpha=0.85)
    cutout(cv, 'joram_throne', 160, 80, 38, rim=0.6, tint='#ffe8c8')
    cutout(cv, 'sunamita_kneel_r', 92, 99, 42, rim=0.8, tint='#ffe0c0')
    cutout(cv, 'nino_stand', 66, 100, 30, rim=0.6, tint='#ffe0c0')
    cutout(cv, 'guiezi_stand_r', 122, 96, 36, rim=0.6, tint='#ffe0c0')
    return dict(warm=0.4)


# =============================================================================
# Cap. 9 - Naaman
# =============================================================================
@plate
def naaman_puerta(cv):
    day(cv, 176, 18)
    hills(cv, 58, 10, '#a8a090', seed=5, haze=0.5)
    ground(cv, 68, '#b8a07a', '#6a5a40', seed=6)
    house(cv, 30, 84, 36, 24, wall='#cdb48c', roof='#8a6a48')
    quadruped(cv, 168, 90, 26, body='#e8e0d0', facing=-1, kind='horse', rim=0.8)
    quadruped(cv, 186, 84, 22, body='#6a4a3a', facing=-1, kind='horse', rim=0.8)
    chariot(cv, 138, 92, 18, body='#1f7a68', wheel='#d4a537', facing=-1)
    cutout(cv, 'naaman_stand_l', 110, 99, 44, rim=0.8, tint='#fff4e0')
    cutout(cv, 'mozo_point_r', 66, 99, 40, rim=0.7, tint='#fff4e0')
    return dict()


@plate
def muchacha_cautiva(cv):
    interior(cv, wall='#7a6a5a', floor='#4a3a30', floor_y=64, light_at=(60, 40), seed=10, beams=False)
    window(cv, 60, 30, 24, 30, sky_top='#7aa8d0', sky_bot='#e8e0c8', star=False, light='#fff0c8')
    for k in range(6):
        cx = 128 + k * 13
        draw(cv, poly([(cx - 7, 4), (cx + 7, 4), (cx + 5 + (k % 2) * 2, 64), (cx - 6, 64)]), '#1f6a5a',
             shade=0.7, bevel=4, rim=0.3, alpha=0.9, grad=('v', col('#2a8a72'), col('#123a32')))
    cutout(cv, 'muchacha_stand_r', 80, 99, 40, rim=0.9, tint='#fff4e0')
    cutout(cv, 'esposa_naaman_sit_l', 142, 99, 44, rim=0.7, tint='#fff0e0')
    return dict(warm=0.2)


@plate
def tierra_de_israel(cv):
    day(cv, 100, 16)
    hills(cv, 56, 12, '#9ab0a0', seed=2, haze=0.5)
    reeds(cv, 0, 60, 68, 10, seed=1)
    reeds(cv, 140, 200, 68, 10, seed=2)
    ground(cv, 64, '#8a9a6a', '#4a5a3a', seed=3)
    river(cv, [(-5, 68), (60, 69), (140, 70), (205, 71)], [(-5, 100.5), (60, 100.5), (140, 100.5), (205, 100.5)],
          deep='#2a6a7a', shallow='#7ab8c8', smooth=False)
    cutout(cv, 'naaman_limpio_up', 100, 96, 44, rim=1.0, tint='#fffaf0', glow=0.05)
    for k in range(3):
        draw(cv, ellipse(100, 96, 10 + k * 5, 2.0 + k * 0.9).cut(ellipse(100, 96, 9 + k * 5, 1.3 + k * 0.9)),
             '#ffffff', alpha=0.5 - k * 0.14)
    light_rays(cv, 100, -10, '#fff8e0', n=7, spread=0.3, length=110, strength=0.14, width=0.04)
    quadruped(cv, 172, 67, 16, body='#7a6a5a', facing=-1, kind='donkey')
    sack(cv, 170, 58, 7, color='#8a6a4a')
    return dict(sat=1.05)


# =============================================================================
# Cap. 10 - Dotan y Samaria
# =============================================================================
@plate
def criado_eliseo(cv):
    dawn(cv, 36, 50)
    mountains(cv, 60, 28, '#7a5a6a', seed=4, haze=0.35, rim=0.6, freq=0.02)
    rng = np.random.default_rng(3)
    for k in range(6):
        fire_chariot(cv, 14 + k * 36 + rng.uniform(-4, 4), 56 - (k % 2) * 6, 15, flip=bool(k % 2), seed=k)
    hills(cv, 74, 12, '#5a4a4a', seed=6, rim=0.6, grad_to='#2a2224')
    cutout(cv, 'eliseo_raise', 88, 99, 44, backlit=0.3, rim=1.0)
    cutout(cv, 'criado_recoil_r', 128, 99, 40, backlit=0.3, rim=1.0)
    particles(cv, 40, (0, 30, W, 80), '#ffb050', size=(0.15, 0.35), seed=8, glow=1.0)
    return dict(warm=0.5)


@plate
def sirios_dotan(cv):
    day(cv, 176, 18)
    hills(cv, 58, 8, '#a8a8a0', seed=3, haze=0.5)
    wall_city(cv, 124, 206, 68, 20, '#b8a888', towers=2, gate=166, haze=0.2)
    ground(cv, 70, '#b0a07a', '#6a5a40', seed=4)
    cutout(cv, 'eliseo_walk_r', 108, 97, 42, rim=0.8, tint='#fff8f0')
    for k in range(5):
        cutout(cv, 'soldado_sirio_blind_r', 70 - k * 15, 94 + (k % 2) * 3, 36 - k * 1.2, rim=0.6, tint='#f0f0f0',
               haze=0.06 * k)
    cutout(cv, 'general_blind_r', 86, 99, 40, rim=0.6, tint='#f0f0f0')
    return dict()


@plate
def cuatro_leprosos(cv):
    sky(cv, [(0, '#1a1a30'), (30, '#4a3a5a'), (58, '#c07a5a')])
    sun(cv, 164, 54, 4, color='#ffd0a0', halo='#ff8a50', halo_r=60, amount=0.6)
    cv.haze = '#7a5a6a'
    tent(cv, 150, 64, 14, 8, haze=0.5)
    tent(cv, 172, 62, 12, 7, haze=0.55)
    tent(cv, 128, 62, 10, 6, haze=0.6)
    hills(cv, 68, 6, '#4a3a44', seed=4, haze=0.3)
    wall_city(cv, -10, 58, 88, 36, '#5a4a48', towers=2, gate=26, rim=0.4)
    ground(cv, 86, '#4a3a34', '#1e1814', seed=4)
    for k, key in enumerate(('leproso_walk_r', 'leproso_point_r', 'leproso_stand_r', 'leproso_sit_r')):
        cutout(cv, key, 76 + k * 22, 99 - (k % 2) * 2, 40, backlit=0.45, rim=1.0)
    return dict(sat=0.95, vignette=0.45)


@plate
def principe_samaria(cv):
    day(cv, 36, 16)
    wall_city(cv, -5, 205, 68, 32, '#b8a888', towers=3, gate=100)
    ground(cv, 68, '#a89a7a', '#5a5040', seed=5)
    crowd_cut(cv, CROWD_PUEBLO, 62, 138, 74, 24, n=12, seed=3, haze=0.1, depth=6)
    for k in range(5):
        sack(cv, 18 + k * 9, 100, 12, color='#e8dcc0')
        sack(cv, 150 + k * 9, 100, 12, color='#e8dcc0')
    cutout(cv, 'principe_stand_r', 100, 99, 44, rim=0.8, tint='#fff4e0')
    particles(cv, 60, (20, 80, 200, 100), '#f8f0e0', size=(0.15, 0.4), seed=6, glow=0.1)
    return dict()


@plate
def hazael_benadad(cv):
    sky(cv, [(0, '#5a6a90'), (35, '#c0b0a0'), (62, '#f0d0a0')])
    sun(cv, 160, 18, 4.0, halo='#fff0c0', halo_r=55, amount=0.6)
    cv.haze = '#e8d0b0'
    mountains(cv, 58, 12, '#b0a0a0', seed=5, haze=0.5)
    ground(cv, 68, '#c8a878', '#8a6a48', seed=3)
    for k in range(5):
        quadruped(cv, 150 + k * 13, 80 + k * 3.5, 22 - k * 1.5, body='#c09060', facing=-1, kind='camel',
                  haze=0.08 * (4 - k))
    cutout(cv, 'hazael_offer_l', 114, 99, 44, rim=0.8, tint='#fff0e0')
    cutout(cv, 'eliseo_veiled_l', 56, 99, 44, rim=0.8, tint='#fff0e0')
    coins(cv, 92, 100, n=9, color='#ffd35a', spread=6)
    return dict(warm=0.4)


@plate
def uncion_jehu(cv):
    interior(cv, wall='#5a5050', floor='#3a3030', floor_y=64, light_at=(100, 50), seed=12, dark=0.7)
    window(cv, 36, 28, 16, 20, sky_top='#7aa0c8', sky_bot='#e8d8c0', star=False, light='#fff0d0')
    cutout(cv, 'jehu_kneel_r', 92, 99, 44, rim=0.8, tint='#ffe8d0')
    cutout(cv, 'hijo_prof_flask_r', 132, 99, 44, rim=0.8, tint='#ffe8d0', flip=True)
    draw(cv, capsule(112, 62, 110, 72, 0.6, 0.3), '#e8c060', glow=1.2)
    glow_spot(cv, 110, 70, 16, '#ffd870', 0.5)
    return dict(warm=0.3)


# =============================================================================
# La Ascension (2 R 2:1-14)
# =============================================================================
@plate
def asc_gilgal(cv):
    day(cv, 160, 20)
    hills(cv, 60, 14, '#a8a080', seed=2, haze=0.5)
    ground(cv, 68, '#b8a070', '#6a5a3a', seed=3)
    road = poly([(124, 68), (136, 68), (160, 100.5), (64, 100.5)])
    draw(cv, road, '#d8c090', alpha=0.6, tex=0.2)
    cutout(cv, 'elias_walk_r', 104, 98, 44, rim=0.8, tint='#fff4e0')
    cutout(cv, 'eliseo_walk_r', 76, 100, 44, rim=0.8, tint='#fff4e0')
    palm(cv, 176, 100, 42, seed=3)
    return dict()


@plate
def asc_betel(cv):
    day(cv, 36, 18)
    hills(cv, 60, 10, '#a0a090', seed=4, haze=0.5)
    house(cv, 156, 70, 22, 13, haze=0.2)
    house(cv, 184, 68, 18, 11, haze=0.25)
    ground(cv, 70, '#a89a70', '#5a5038', seed=4)
    for k in range(4):
        cutout(cv, 'hijo_prof_stand_l', 116 + k * 15, 92 + (k % 2) * 5, 34 + (k % 2) * 4, rim=0.7, tint='#fff4e0',
               haze=0.08 * (3 - k))
    cutout(cv, 'eliseo_veiled_l', 62, 99, 44, rim=0.8, tint='#fff4e0', flip=True)
    return dict()


@plate
def asc_jerico(cv):
    dusk(cv, 150, 50)
    hills(cv, 60, 10, '#6a5a6a', seed=3, haze=0.4)
    crowd_cut(cv, CROWD_HIJOS, 118, 196, 64, 12, n=14, seed=2, haze=0.45, backlit=0.6, rim=1.0, depth=2)
    ground(cv, 66, '#7a6a4a', '#3a3020', seed=3)
    river(cv, [(-5, 72), (80, 74), (205, 76)], [(-5, 84), (80, 86), (205, 88)], deep='#3a4a6a',
          shallow='#c08a7a', smooth=False)
    cutout(cv, 'elias_stand_r', 72, 100, 44, backlit=0.5, rim=1.0)
    cutout(cv, 'eliseo_stand_r', 46, 101, 44, backlit=0.5, rim=1.0)
    return dict(warm=0.4)


@plate
def asc_jordan(cv):
    sky(cv, [(0, '#2a2240'), (26, '#7a4a6a'), (52, '#f0a070')])
    sun(cv, 100, 46, 5, halo='#ffa060', halo_r=80)
    cv.haze = '#d07a5a'
    hills(cv, 58, 10, '#5a4a5a', seed=2, haze=0.4)
    river(cv, [(70, 58), (60, 70), (40, 100.5)], [(130, 58), (140, 70), (160, 100.5)], deep='#2a3a5a',
          shallow='#c07a7a')
    draw(cv, poly([(80, 60), (120, 60), (130, 100.5), (70, 100.5)]), '#6a5a40', tex=0.2,
         grad=('v', col('#8a6a4a'), col('#3a2a20')))
    for sx in (-1, 1):
        draw(cv, poly([(100 + sx * 20, 58), (100 + sx * 30, 58), (100 + sx * 38, 100.5), (100 + sx * 28, 100.5)]),
             '#4a7aa0', grad=('v', col('#c0d8e8'), col('#2a4a6a')), glow=0.2, glow_col='#a0c8ff', alpha=0.9)
    cutout(cv, 'elias_point_l', 114, 98, 44, backlit=0.4, rim=1.0)
    cutout(cv, 'eliseo_stand_r', 86, 99, 44, backlit=0.4, rim=1.0)
    light_rays(cv, 100, 46, '#ffd0a0', n=8, spread=1.4, length=150, strength=0.1, angle=math.pi / 2)
    return dict(warm=0.5)


# =============================================================================
# Epilogo: El Legado
# =============================================================================
@plate
def epi_jehu(cv):
    sky(cv, [(0, '#4a5a7a'), (35, '#a8a0a0'), (62, '#e0c8a0')])
    sun(cv, 36, 20, 4.0, halo='#fff0c0', halo_r=55, amount=0.6)
    cv.haze = '#d8c8b0'
    hills(cv, 58, 10, '#9a9080', seed=3, haze=0.5)
    tower(cv, 172, 78, 24, 56, color='#a89070', window_y=44)
    ground(cv, 76, '#b0986a', '#6a5a3a', seed=3)
    particles(cv, 90, (0, 70, 90, 101), '#e0c8a0', size=(0.5, 1.6), seed=3, glow=0.0, alpha=(0.1, 0.35))
    quadruped(cv, 92, 98, 28, body='#6a4a3a', facing=1, kind='horse', gait=1.0, rim=0.8)
    quadruped(cv, 106, 93, 25, body='#3a2a22', facing=1, kind='horse', gait=0.9, rim=0.8)
    chariot(cv, 58, 98, 22, body='#8a6a2a', wheel='#c9a24a', facing=1)
    cutout(cv, 'jehu_drive_r', 56, 88, 34, rim=0.8, tint='#fff4e0', shadow_k=0.0)
    return dict()


@plate
def epi_arco_joas(cv):
    interior(cv, wall='#5a4a44', floor='#3a2e2a', floor_y=62, light_at=(156, 40), seed=13, dark=0.65)
    window(cv, 156, 32, 20, 26, sky_top='#f0b080', sky_bot='#ffe0b0', star=False, light='#ffd8a0')
    ty = bed(cv, 58, 98, 62, h=8, cloth='#c8c0b0')
    cutout(cv, 'eliseo_old_lie', 58, ty + 3.5, 38, rim=0.6, tint='#ffe0c0')
    cutout(cv, 'joas_shoot_r', 126, 99, 44, rim=0.9, tint='#ffe0c0')
    draw(cv, capsule(144, 52, 156, 36, 0.35), '#6a4a2a')
    return dict(warm=0.5)


@plate
def epi_huesos(cv):
    night(cv, 36, 16)
    hills(cv, 68, 18, '#2a2a3a', seed=4, rim=0.4)
    rock_cave(cv, 112, 98, 34, 50, rock='#4a4450', side=1, rim=0.5, seed=8)
    glow_spot(cv, 112, 82, 30, '#fff0c8', 0.6)
    bones(cv, 112, 96, 5)
    light_rays(cv, 112, 78, '#fff4d8', n=7, spread=1.0, length=60, strength=0.15, angle=-math.pi / 2)
    cutout(cv, 'resucitado', 112, 97, 40, rim=1.0, rim_col='#ffffff', tint='#fff8e8', glow=0.1)
    crowd_cut(cv, ['soldado_sirio_stand_r'], 18, 60, 96, 34, n=3, seed=2, backlit=0.75, rim=0.7, haze=0.0, depth=4)
    particles(cv, 40, (80, 50, 146, 98), '#fff8e0', size=(0.12, 0.3), seed=9, glow=1.0)
    return dict(vignette=0.5)
