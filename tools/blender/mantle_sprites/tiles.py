"""Terrain tileset: procedural 3D props / ground tiles -> assets/sprites/tiles.png.

Sheet: 64 px cells (16 game units x4), TILE_COLS columns. Ground tiles are rendered top-down (seamless-ish,
features kept away from the edges); props use the same 30 deg camera as the characters
so they sit in the world with matching perspective and lighting. Tall props (tree,
altars, torch) are 16x32 (2 cells high, origin = bottom cell).

Cell layout (mirrored by TileSprite in src/game/rooms.h — keep in sync):
    row 0: floor_a floor_b floor_c floor_d path_a path_b water_a water_b
    row 1: cliff   rock    bush    grass   ford   bridge  chest   pit
    row 2: tree(top)  altar_god(top)  altar_baal(top)  torch(top)  exit(top)  ...
    row 3: tree(bot)  altar_god(bot)  altar_baal(bot)  torch(bot)  exit(bot)  ...
    row 4: pickups + projectiles
    row 5: grass_a..d (meadow floor)  stone_a..d (paved floor)
    row 6: detail decals: tuft_a tuft_b pebbles flowers crack twigs bones moss
    row 7: shore autotiling pieces (edge / outer corner / inner corner), authored for
           N = +Y; the other 9 orientations are the same cells rotated 90/180/270 in C++,
           y despues cliff_rock / cliff_rock_tall (la cara de roca, que ocupa hasta la row 8)
"""
import math
import os
import random

import bpy
import numpy as np
from mathutils import Vector, Matrix

from . import parts, render
from .parts import get_mat, hexc, box, sphere, tube, plane_quad

CELL = 64            # px per cell = 16 game units x ASSET_SCALE 4 (rooms.h TILESET_CELL)
TILE_COLS = 8
TILE_ROWS = 9
SS = 2
U = render.ORTHO_SCALE / render.FRAME * CELL     # world units per cell (1.275)
OUTLINE = render.OUTLINE

# name -> (col, row, cells_w, cells_h)
LAYOUT = {
    'floor_a': (0, 0, 1, 1), 'floor_b': (1, 0, 1, 1), 'floor_c': (2, 0, 1, 1), 'floor_d': (3, 0, 1, 1),
    'path_a': (4, 0, 1, 1), 'path_b': (5, 0, 1, 1), 'water_a': (6, 0, 1, 1), 'water_b': (7, 0, 1, 1),
    'cliff': (0, 1, 1, 1), 'rock': (1, 1, 1, 1), 'bush': (2, 1, 1, 1), 'grass': (3, 1, 1, 1),
    'ford': (4, 1, 1, 1), 'bridge': (5, 1, 1, 1), 'chest': (6, 1, 1, 1), 'pit': (7, 1, 1, 1),
    'tree': (0, 2, 1, 2), 'altar_god': (1, 2, 1, 2), 'altar_baal': (2, 2, 1, 2),
    'torch': (3, 2, 1, 2), 'exit': (4, 2, 1, 2), 'tree_dead': (5, 2, 1, 2), 'palm': (6, 2, 1, 2),
    'cliff_tall': (7, 2, 1, 2),
    # row 4: pickups + projectiles (small sprites centred in the cell)
    'heart': (0, 4, 1, 1), 'orb': (1, 4, 1, 1), 'fireball': (2, 4, 1, 1), 'lightbolt': (3, 4, 1, 1),
    'arrow': (4, 4, 1, 1), 'darkbolt': (5, 4, 1, 1), 'skull': (6, 4, 1, 1), 'star': (7, 4, 1, 1),
    # row 5: extra floor families (meadow grass, paved stone) — 4 variants each
    'grass_a': (0, 5, 1, 1), 'grass_b': (1, 5, 1, 1), 'grass_c': (2, 5, 1, 1), 'grass_d': (3, 5, 1, 1),
    'stone_a': (4, 5, 1, 1), 'stone_b': (5, 5, 1, 1), 'stone_c': (6, 5, 1, 1), 'stone_d': (7, 5, 1, 1),
    # row 6: ground detail decals (transparent, drawn over floor tiles by biome)
    'det_tuft_a': (0, 6, 1, 1), 'det_tuft_b': (1, 6, 1, 1), 'det_pebbles': (2, 6, 1, 1), 'det_flowers': (3, 6, 1, 1),
    'det_crack': (4, 6, 1, 1), 'det_twigs': (5, 6, 1, 1), 'det_bones': (6, 6, 1, 1), 'det_moss': (7, 6, 1, 1),
    # row 7: shore pieces drawn over water tiles (rotated in C++ for the other sides)
    'shore_edge': (0, 7, 1, 1), 'shore_out': (1, 7, 1, 1), 'shore_in': (2, 7, 1, 1),
    # rows 7-8: cara de ROCA para los bordes naturales/canon. `cliff` y `cliff_tall` son
    # silleria y ahi se quedan: en ciudad y palacio el ladrillo es lo correcto.
    'cliff_rock': (3, 7, 1, 1), 'cliff_rock_tall': (4, 7, 1, 2),
    # Secretos de bioma (GDD2.0 fase 4): lo que se rompe y lo que se abre.
    'jar': (5, 7, 1, 1), 'crack': (6, 7, 1, 1),
    # rows 7-8: puerta a un interior (GDD2.0 fase 3)
    'door': (7, 7, 1, 2),
}


# =============================================================================
# Colors
# =============================================================================
def M(name, h):
    return get_mat('t_' + name, hexc(h))


def mats():
    return {
        'sand': M('sand', '#b39262'), 'sand_dk': M('sand_dk', '#9a7a4e'), 'sand_lt': M('sand_lt', '#c9aa78'),
        'earth': M('earth', '#8f6d44'), 'earth_dk': M('earth_dk', '#74563a'), 'earth_lt': M('earth_lt', '#a98a5e'),
        'water': M('water', '#3f7fbb'), 'water_lt': M('water_lt', '#7ab1dd'), 'water_dk': M('water_dk', '#2c5f92'),
        'rock': M('rock', '#7d6b57'), 'rock_lt': M('rock_lt', '#a08c6c'), 'rock_dk': M('rock_dk', '#4d3f31'),
        'stone': M('stone', '#8e877a'), 'stone_lt': M('stone_lt', '#b3ab9b'),
        'trunk': M('trunk', '#6a4a2e'), 'leaf': M('leaf', '#4f8c3a'), 'leaf_lt': M('leaf_lt', '#74ad50'),
        'leaf_dk': M('leaf_dk', '#37672a'), 'grass': M('grass', '#7fb45c'), 'grass_dk': M('grass_dk', '#5a8f42'),
        'berry': M('berry', '#c8412f'), 'wood': M('wood', '#9b6b3c'), 'wood_dk': M('wood_dk', '#6f4a28'),
        'gold': M('gold', '#e4b84a'), 'fire': M('fire', '#f0862a'), 'fire_lt': M('fire_lt', '#ffd25a'),
        'purple': M('purple', '#5b2c6f'), 'purple_dk': M('purple_dk', '#31183d'), 'ruby': M('ruby', '#c0392b'),
        'black': M('black', '#15100c'), 'white': M('white', '#f1ecdc'), 'dead': M('dead', '#8a7660'),
        'sky': M('sky', '#f5e6b3'),
        # Barro cocido de la tinaja: naranja quemado, para que no se confunda con la madera
        # del cofre ni con la tierra del suelo (los tres son pardos).
        'clay': M('clay', '#b5673c'), 'clay_dk': M('clay_dk', '#87462a'),
        'clay_lt': M('clay_lt', '#d18a5e'),
        # meadow / paving / decals
        'meadow': M('meadow', '#6fae4e'), 'meadow_dk': M('meadow_dk', '#558f3c'), 'meadow_lt': M('meadow_lt', '#8fc765'),
        'slab': M('slab', '#9a9283'), 'slab_lt': M('slab_lt', '#b2aa9a'), 'slab_dk': M('slab_dk', '#7d7567'),
        'mortar': M('mortar', '#5e574c'), 'moss': M('moss', '#4f7d3b'),
        'flower_w': M('flower_w', '#f6f1e4'), 'flower_y': M('flower_y', '#f2cf4f'), 'flower_r': M('flower_r', '#e2604c'),
        'bone': M('bone', '#ece4d0'), 'twig': M('twig', '#6b4a2e'),
        # shore autotiling
        'wet': M('wet', '#7d5f3c'), 'foam': M('foam', '#aec9dd'),
    }


# =============================================================================
# Camera helpers
# =============================================================================
def _cam(px, elev_deg, ortho, feet_row):
    """Square frame of px*px pixels (rendered at px*SS), camera at elev, origin on feet_row."""
    sc = bpy.context.scene
    render.setup_scene(render.__dict__.get('_TMP', os.path.join(os.environ.get('TEMP', '/tmp'), 'mantle_sprites')))
    sc.render.resolution_x = px * SS
    sc.render.resolution_y = px * SS
    cam = sc.camera
    cd = cam.data
    cd.ortho_scale = ortho
    e = math.radians(elev_deg)
    dist = 12.0
    cam.location = Vector((0.0, -math.cos(e) * dist, math.sin(e) * dist))
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    cd.shift_y = (feet_row - px * 0.5) / px
    cd.shift_x = 0.0
    return cam


def _cam_ground():
    return _cam(CELL, 89.9, U, CELL * 0.5)


def _cam_prop(px, feet_row):
    return _cam(px, render.CAM_ELEV_DEG, render.ORTHO_SCALE / render.FRAME * px, feet_row)


# =============================================================================
# Post: downscale + palette + optional outline (size-generic version of render.post_process)
# =============================================================================
def _post(hi, px, palette, outline, alpha_cut=0.5, gain=None):
    """palette=None -> smooth (render.post_soft); palette -> legacy pixel-art path."""
    if palette is None:
        return render.post_soft(hi, px, SS, outline=outline, gain=gain)
    a = hi[..., 3:4]
    prem = hi[..., :3] * a
    prem = prem.reshape(px, SS, px, SS, 3).mean(axis=(1, 3))
    a = a.reshape(px, SS, px, SS, 1).mean(axis=(1, 3))
    rgb = np.where(a > 1e-4, prem / np.maximum(a, 1e-4), 0.0)
    rgb = np.clip(rgb * (render.GAIN if gain is None else gain), 0.0, 1.0)
    solid = a[..., 0] >= alpha_cut
    out = np.zeros((px, px, 4), dtype=np.float32)
    if palette is not None and solid.any():
        p = rgb[solid]
        d = ((p[:, None, :] - palette[None, :, :]) ** 2).sum(axis=2)
        rgb_q = rgb.copy()
        rgb_q[solid] = palette[d.argmin(axis=1)]
        rgb = rgb_q
    out[..., :3] = rgb
    out[..., 3] = solid.astype(np.float32)
    if outline:
        s = solid
        nb = np.zeros_like(s)
        nb[1:, :] |= s[:-1, :]
        nb[:-1, :] |= s[1:, :]
        nb[:, 1:] |= s[:, :-1]
        nb[:, :-1] |= s[:, 1:]
        edge = nb & ~s
        out[edge, :3] = OUTLINE
        out[edge, 3] = 1.0
    return out


def _palette():
    cols = []
    for rgb in parts._MATS.values():
        for s in render.SHADES:
            c = np.array(rgb, dtype=np.float32)
            if s > 1.0:
                c = np.clip(c + (1.0 - c) * (s - 1.0) * 0.8, 0, 1)
            else:
                c = np.clip(c * s, 0, 1)
            cols.append(c)
    cols.append(np.array(OUTLINE, dtype=np.float32))
    return np.array(cols, dtype=np.float32)


# =============================================================================
# Builders — ground (top-down). Ground plane covers the whole cell.
# =============================================================================
def _ground(mat, z=0.0, size=None):
    s = (size or U) * 0.5 + 0.02
    return plane_quad('ground', [(-s, -s, z), (s, -s, z), (s, s, z), (-s, s, z)], mat)


def _pebbles(rng, m, n, r=(0.05, 0.11), keep=0.42, z=0.0, cands=None):
    """Sprinkle flat pebbles away from the cell edges (|x|,|y| < keep*U)."""
    cands = cands or [m['sand_dk'], m['sand_lt']]
    for i in range(n):
        x = rng.uniform(-keep, keep) * U
        y = rng.uniform(-keep, keep) * U
        rr = rng.uniform(*r)
        sphere(f'peb{i}', (x, y, z + rr * 0.15), rr, rng.choice(cands), scale=(1.0, rng.uniform(0.6, 1.0), 0.35), segs=8, rings=5)


def build_floor(m, variant):
    rng = random.Random(100 + variant)
    _ground(m['sand'])
    n = [2, 4, 5, 3][variant % 4]
    _pebbles(rng, m, n)
    if variant == 2:  # crack line
        for i in range(3):
            box(f'crack{i}', (rng.uniform(-0.3, 0.3) * U, rng.uniform(-0.3, 0.3) * U, 0.01), (0.22, 0.05, 0.02), m['sand_dk'])
    if variant == 3:  # tiny tuft
        for i in range(3):
            tube(f'tuft{i}', (0.15 * U + i * 0.05, -0.1 * U, 0.0), (0.15 * U + i * 0.06 - 0.06, -0.1 * U + 0.02, 0.18), 0.03, 0.005, m['grass_dk'], segs=5)


def build_path(m, variant):
    rng = random.Random(200 + variant)
    _ground(m['earth_lt'])
    # Worn dirt as irregular darker blotches that run off the cell edges. A centred "worn"
    # box left a dark frame on every side, so a trail read as a row of paving slabs.
    for i in range(3):
        sphere(f'wear{i}', (rng.uniform(-0.5, 0.5) * U, rng.uniform(-0.5, 0.5) * U, 0.004),
               rng.uniform(0.22, 0.36), m['earth'],
               scale=(rng.uniform(0.8, 1.3), rng.uniform(0.7, 1.1), 0.03), segs=12, rings=6)
    _pebbles(rng, m, 3 + variant, r=(0.05, 0.09), keep=0.36, cands=[m['stone'], m['earth_dk']])


def build_water(m, variant):
    rng = random.Random(300 + variant)
    _ground(m['water'])
    off = 0.0 if variant == 0 else 0.33 * U
    for i in range(3):
        y = -0.35 * U + i * 0.35 * U + off
        y = ((y + 0.5 * U) % U) - 0.5 * U
        w = rng.uniform(0.28, 0.42) * U
        x = rng.uniform(-0.2, 0.2) * U
        box(f'rip{i}', (x, y, 0.02), (w, 0.06, 0.02), m['water_lt'])
        box(f'rips{i}', (x + w * 0.5, y - 0.09, 0.02), (w * 0.4, 0.05, 0.02), m['water_dk'])


def build_ford(m):
    build_water(m, 0)
    rng = random.Random(42)
    for i, (x, y) in enumerate([(-0.28, -0.26), (0.22, -0.2), (-0.15, 0.2), (0.28, 0.27), (0.02, 0.0)]):
        sphere(f'st{i}', (x * U, y * U, 0.06), rng.uniform(0.15, 0.2), rng.choice([m['stone'], m['stone_lt']]),
               scale=(1.0, 0.85, 0.5), segs=8, rings=5)


def build_bridge(m):
    build_water(m, 1)
    for i in range(4):
        y = -0.36 * U + i * 0.24 * U
        box(f'plank{i}', (0, y, 0.05), (U * 1.02, 0.2 * U, 0.06), m['wood'] if i % 2 == 0 else m['wood_dk'])
    box('rail0', (-0.44 * U, 0, 0.09), (0.08 * U, U * 1.02, 0.06), m['wood_dk'])
    box('rail1', (0.44 * U, 0, 0.09), (0.08 * U, U * 1.02, 0.06), m['wood_dk'])


def build_pit(m):
    _ground(m['sand'])
    sphere('hole', (0, 0, -0.05), 0.55, m['black'], scale=(1.0, 0.85, 0.2), segs=12, rings=6)
    sphere('rim', (0, 0, -0.06), 0.62, m['sand_dk'], scale=(1.0, 0.85, 0.15), segs=12, rings=6)


def _blades(rng, m, n, cx, cy, spread, ln=(0.12, 0.24), cands=None, tag='bl', z=0.02):
    """Grass blades seen from above: thin slivers radiating from (cx, cy)."""
    cands = cands or [m['meadow_dk'], m['grass_dk'], m['meadow_lt']]
    for i in range(n):
        ang = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(0, spread)
        L = rng.uniform(*ln)
        x = cx + math.cos(ang) * (d + L * 0.5)
        y = cy + math.sin(ang) * (d + L * 0.5)
        box(f'{tag}{i}_{cx:.2f}_{cy:.2f}', (x, y, z), (L, 0.035, 0.03), rng.choice(cands),
            rot=Matrix.Rotation(ang, 3, 'Z'))


def build_meadow(m, variant):
    """Meadow floor: green ground, subtle tone patches and blade clusters (edges kept clean)."""
    rng = random.Random(400 + variant)
    _ground(m['meadow'])
    soft_dk = get_mat('t_meadow_sdk', hexc('#6aa94b'))
    soft_lt = get_mat('t_meadow_slt', hexc('#74b353'))
    soft_bl = get_mat('t_meadow_sbl', hexc('#609f43'))
    # Tone patches: 3 clusters of 3 overlapping ellipses each. A single ellipse read as a
    # circle stamped on the grass; overlapping ones give an irregular, cloud-like edge.
    for i in range(3):
        cx, cy = rng.uniform(-0.34, 0.34) * U, rng.uniform(-0.34, 0.34) * U
        mat = rng.choice([soft_dk, soft_lt])
        for j in range(3):
            sphere(f'patch{i}_{j}', (cx + rng.uniform(-0.12, 0.12) * U, cy + rng.uniform(-0.1, 0.1) * U, 0.0),
                   rng.uniform(0.13, 0.22), mat,
                   scale=(rng.uniform(0.9, 1.4), rng.uniform(0.6, 1.0), 0.04), segs=10, rings=5)
    # a couple of quiet blade pairs per cell; the busy tufts come from the decal layer
    for i in range(2 + (variant % 2)):
        _blades(rng, m, 2, rng.uniform(-0.34, 0.34) * U, rng.uniform(-0.34, 0.34) * U, 0.03,
                ln=(0.10, 0.18), cands=[soft_bl, soft_dk], tag=f'g{i}')
    if variant == 3:  # clover dots
        for i in range(3):
            sphere(f'clov{i}', (rng.uniform(-0.3, 0.3) * U, rng.uniform(-0.3, 0.3) * U, 0.01), 0.04, soft_bl, scale=(1, 1, 0.3), segs=6, rings=4)


def build_stone(m, variant):
    """Paved floor: 2x2 slabs per cell, gaps aligned with the cell edges so cells tile."""
    rng = random.Random(500 + variant)
    _ground(m['mortar'])
    gap = 0.05
    half = U * 0.5
    shades = [m['slab'], m['slab_lt'], m['slab'], m['slab_dk']]
    for i in range(2):
        for j in range(2):
            cx = -half * 0.5 + i * half
            cy = -half * 0.5 + j * half
            box(f'slab{i}{j}', (cx + rng.uniform(-0.01, 0.01), cy + rng.uniform(-0.01, 0.01), 0.02),
                (half - gap, half - gap, 0.04), rng.choice(shades))
    if variant == 1:  # cracked slab
        for i in range(3):
            box(f'crk{i}', (rng.uniform(-0.3, 0.3) * U, rng.uniform(-0.3, 0.3) * U, 0.045), (0.18, 0.03, 0.01), m['mortar'])
    if variant == 2:  # moss in the joints
        for i in range(3):
            sphere(f'moss{i}', (rng.uniform(-0.4, 0.4) * U, rng.choice([-half * 0.02, 0.0]) + rng.uniform(-0.02, 0.02), 0.035),
                   rng.uniform(0.06, 0.1), m['moss'], scale=(1.6, 0.7, 0.2), segs=6, rings=4)
    if variant == 3:  # chipped corner + pebble
        sphere('chip', (rng.uniform(-0.3, 0.3) * U, rng.uniform(-0.3, 0.3) * U, 0.045), 0.07, m['slab_dk'], scale=(1, 0.8, 0.15), segs=6, rings=4)


# --- ground detail decals (transparent, top-down, STUDIO light) -------------
def det_tuft(m, variant):
    rng = random.Random(600 + variant)
    _blades(rng, m, 9 + 3 * variant, 0.0, 0.0, 0.06, ln=(0.16, 0.30))
    if variant == 1:
        _blades(rng, m, 5, 0.28, -0.18, 0.04, ln=(0.12, 0.22), tag='b2')


def det_pebbles(m):
    rng = random.Random(610)
    for i, (x, y) in enumerate([(-0.18, -0.08), (0.12, -0.16), (0.2, 0.14), (-0.06, 0.2)]):
        rr = rng.uniform(0.07, 0.12)
        sphere(f'pb{i}', (x * U * 0.5, y * U * 0.5, 0.0), rr, rng.choice([m['stone'], m['stone_lt'], m['rock']]),
               scale=(1.0, rng.uniform(0.7, 1.0), 0.5), segs=8, rings=5)


def det_flowers(m):
    rng = random.Random(620)
    _blades(rng, m, 6, 0.0, 0.0, 0.10, ln=(0.12, 0.2))
    for i, (x, y, c) in enumerate([(-0.14, 0.04, 'flower_w'), (0.1, -0.12, 'flower_y'), (0.16, 0.14, 'flower_r'), (-0.04, -0.2, 'flower_w')]):
        tube(f'stem{i}', (x, y, 0.0), (x, y, 0.16), 0.012, 0.01, m['grass_dk'], segs=4)
        sphere(f'fl{i}', (x, y, 0.18), 0.055, m[c], scale=(1, 1, 0.6), segs=8, rings=5)
        sphere(f'fc{i}', (x, y, 0.21), 0.02, m['flower_y' if c != 'flower_y' else 'flower_r'], segs=6, rings=4)


def det_crack(m):
    rng = random.Random(630)
    x, y, ang = -0.3, -0.22, 0.8
    for i in range(6):
        ln = rng.uniform(0.12, 0.2)
        nx, ny = x + math.cos(ang) * ln, y + math.sin(ang) * ln
        box(f'ck{i}', ((x + nx) * 0.5, (y + ny) * 0.5, 0.0), (ln, 0.035, 0.02), m['black'],
            rot=Matrix.Rotation(ang, 3, 'Z'))
        x, y = nx, ny
        ang += rng.uniform(-0.7, 0.7)


def det_twigs(m):
    rng = random.Random(640)
    for i, (x, y, a, ln) in enumerate([(-0.1, 0.05, 0.4, 0.5), (0.12, -0.12, -1.1, 0.36)]):
        p0 = (x - math.cos(a) * ln * 0.5, y - math.sin(a) * ln * 0.5, 0.02)
        p1 = (x + math.cos(a) * ln * 0.5, y + math.sin(a) * ln * 0.5, 0.02)
        tube(f'tw{i}', p0, p1, 0.02, 0.012, m['twig'], segs=6)
        tube(f'twb{i}', ((p0[0] + p1[0]) * 0.5, (p0[1] + p1[1]) * 0.5, 0.02),
             ((p0[0] + p1[0]) * 0.5 + math.cos(a + 1.2) * 0.12, (p0[1] + p1[1]) * 0.5 + math.sin(a + 1.2) * 0.12, 0.02),
             0.012, 0.006, m['twig'], segs=5)
    for i in range(3):
        sphere(f'leaf{i}', (rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), 0.0), 0.04, m['dead'], scale=(1.4, 0.8, 0.3), segs=6, rings=4)


def det_bones(m):
    sphere('skull', (-0.14, 0.02, 0.06), 0.1, m['bone'], scale=(1.0, 1.1, 0.8), segs=10, rings=6)
    box('jaw', (-0.14, -0.07, 0.02), (0.12, 0.06, 0.05), m['bone'])
    for i, (x, y) in enumerate([(-0.11, 0.0), (-0.17, 0.0)]):
        sphere(f'eye{i}', (x, -0.075, 0.07), 0.025, m['black'], segs=6, rings=4)
    tube('bone', (0.05, -0.16, 0.02), (0.3, 0.12, 0.02), 0.022, 0.022, m['bone'], segs=6)
    for (x, y) in [(0.05, -0.16), (0.3, 0.12)]:
        sphere(f'knob{x}', (x, y, 0.02), 0.04, m['bone'], segs=6, rings=4)


def det_moss(m):
    rng = random.Random(650)
    sphere('moss0', (-0.04, 0.0, 0.0), 0.2, m['moss'], scale=(1.3, 0.8, 0.04), segs=12, rings=6)
    sphere('moss1', (0.2, 0.1, 0.0), 0.11, m['leaf_dk'], scale=(1.0, 0.9, 0.04), segs=8, rings=5)
    for i in range(4):
        sphere(f'dot{i}', (rng.uniform(-0.25, 0.25), rng.uniform(-0.18, 0.18), 0.005), 0.025, m['meadow_dk'], scale=(1, 1, 0.3), segs=6, rings=4)


# =============================================================================
# Builders — props (30 deg camera). Origin = ground contact.
# =============================================================================
def _stone_wall(m, h, rows, seed):
    """Stacked stone blocks (brick pattern) over a dark mortar body, light slab on top."""
    rng = random.Random(seed)
    box('body', (0, 0, h * 0.5), (U + 0.04, U + 0.04, h), m['rock_dk'])
    rh = h / rows
    shades = [m['rock'], m['rock_lt'], m['rock'], m['stone']]
    for r in range(rows):
        z = rh * (r + 0.5)
        n = 2 if r % 2 == 0 else 3
        bw = (U + 0.04) / n
        for i in range(n):
            x = -(U + 0.04) * 0.5 + bw * (i + 0.5)
            box(f'blk{r}_{i}', (x, -U * 0.5 - 0.02 + 0.02, z), (bw - 0.06, 0.08, rh - 0.05), rng.choice(shades))
    box('top', (0, 0, h + 0.03), (U + 0.04, U + 0.04, 0.06), m['rock_lt'])
    for i in range(3):
        sphere(f'tp{i}', (rng.uniform(-0.35, 0.35) * U, rng.uniform(-0.3, 0.3) * U, h + 0.06), 0.07,
               m['rock'], scale=(1, 1, 0.4), segs=6, rings=4)
    box('shadow', (0, -U * 0.5 - 0.06, 0.02), (U + 0.04, 0.06, 0.04), m['black'])


def build_cliff(m):
    _stone_wall(m, 0.62, 2, 7)


def build_cliff_tall(m):
    _stone_wall(m, 2.1, 6, 9)


def _rock_wall(m, h, cols, seed):
    """Escarpa de ROCA: masas desiguales con las juntas VERTICALES y la coronacion mordida.

    Lo que hacia que el canon del Querit se leyera como una tapia no era el color, era la
    GEOMETRIA de `_stone_wall`: hiladas horizontales, todas del mismo alto, alineadas. Una
    escarpa dice lo contrario — bloques de tamanos distintos, cantos girados y grietas que
    corren de arriba abajo.

    Se hace con cajas y no con esferas a proposito (misma leccion que la pared de la cueva
    en portraits.py): una corona de esferas iguales con cavity sale "collar de perlas", y
    unas masas angulosas se leen como piedra a la primera. Dos cosas que costaron una
    pasada: las columnas tienen que SOLAPAR (si solo se tocan, entre ellas se ve el cuerpo
    oscuro y parece un agujero), y el cuerpo va en `rock`, no en `rock_dk` — lo que asome
    tiene que leerse como sombra de la roca y no como un hueco negro.
    """
    rng = random.Random(seed)
    W = U + 0.04
    # El cuerpo oscuro asoma por las juntas y ES la grieta, igual que el mortero de la
    # silleria. Sin junta no hay bloques: sale una pared lisa con manchas.
    box('body', (0, 0, h * 0.5), (W, W, h), m['rock_dk'])
    face_y = -U * 0.5
    shades = [m['rock'], m['rock_lt'], m['stone'], m['rock'], m['rock_lt']]
    # Cortes repartidos con temblor, NO al azar puro: dos cortes aleatorios caian juntos y
    # dejaban una columna del 80% del ancho, o sea una losa de lado a lado.
    cuts = [(c + 1) / float(cols) + rng.uniform(-0.09, 0.09) for c in range(cols - 1)]
    xs = [0.0] + cuts + [1.0]
    for c in range(cols):
        x0, x1 = xs[c], xs[c + 1]
        cw = (x1 - x0) * W
        cx = -W * 0.5 + (x0 + x1) * 0.5 * W
        # Cada columna corta por su cuenta: las juntas NO se alinean en hilada, y eso es
        # todo lo que separa una escarpa de un muro de sillares.
        z, k = 0.0, 0
        while z < h - 0.01:
            step = min(h - z, rng.uniform(0.23, 0.42))
            if h - (z + step) < 0.10:
                step = h - z
            box('m%d_%d' % (c, k), (cx + rng.uniform(-0.02, 0.02), face_y, z + step * 0.5),
                (cw - 0.05, 0.09, step - 0.035), rng.choice(shades),
                rot=Matrix.Rotation(math.radians(rng.uniform(-11.0, 11.0)), 4, 'Y'),
                round=0.15, taper=(rng.uniform(0.76, 1.08), 1.0))
            z += step
            k += 1
    # Lascas: cantos sueltos, en la MISMA familia de color. En gris claro (`stone`) se
    # leian como escombro pegado con cola, no como parte de la roca.
    for k in range(1 if h < 1.0 else 3):
        box('ch%d' % k, (rng.uniform(-0.38, 0.38) * W, face_y - 0.045, rng.uniform(0.15, 0.88) * h),
            (rng.uniform(0.16, 0.30), 0.04, rng.uniform(0.06, 0.11)),
            rng.choice([m['rock_lt'], m['rock']]),
            rot=Matrix.Rotation(math.radians(rng.uniform(-28.0, 28.0)), 4, 'Y'), round=0.3)
    # Coronacion mordida, no losa recta
    box('top', (0, 0, h + 0.02), (W, W, 0.05), m['rock'])
    for k in range(3):
        x = -W * 0.5 + W * (k + 0.5) / 3.0
        sphere('cap%d' % k, (x + rng.uniform(-0.10, 0.10), rng.uniform(-0.34, 0.08) * U,
                             h + 0.03),
               rng.uniform(0.09, 0.17), rng.choice([m['rock_lt'], m['rock']]),
               scale=(1.4, 1.0, 0.45), segs=8, rings=5)
    box('shadow', (0, -U * 0.5 - 0.06, 0.02), (W, 0.06, 0.04), m['black'])


def build_cliff_rock(m):
    _rock_wall(m, 0.62, 3, 21)


def build_cliff_rock_tall(m):
    _rock_wall(m, 2.1, 3, 23)


def build_rock(m):
    sphere('r0', (-0.08, 0.02, 0.22), 0.36, m['stone'], scale=(1.15, 0.9, 0.7), segs=10, rings=6)
    sphere('r1', (0.22, -0.1, 0.16), 0.22, m['stone_lt'], scale=(1.0, 0.9, 0.75), segs=8, rings=5)
    sphere('r2', (-0.3, -0.18, 0.1), 0.14, m['stone'], scale=(1, 1, 0.7), segs=8, rings=5)


def build_bush(m):
    sphere('b0', (0, 0, 0.32), 0.42, m['leaf'], scale=(1.15, 1.0, 0.75), segs=10, rings=6)
    sphere('b1', (-0.28, -0.12, 0.28), 0.28, m['leaf_lt'], scale=(1, 1, 0.8), segs=8, rings=5)
    sphere('b2', (0.3, -0.05, 0.3), 0.26, m['leaf_dk'], scale=(1, 1, 0.8), segs=8, rings=5)
    for i, (x, y, z) in enumerate([(-0.1, -0.35, 0.36), (0.25, -0.3, 0.22), (0.05, -0.28, 0.5)]):
        sphere(f'berry{i}', (x, y, z), 0.045, m['berry'], segs=6, rings=4)


def build_grass(m):
    rng = random.Random(5)
    for i in range(7):
        x = rng.uniform(-0.45, 0.45)
        y = rng.uniform(-0.3, 0.3)
        h = rng.uniform(0.35, 0.6)
        lean = rng.uniform(-0.18, 0.18)
        tube(f'g{i}', (x, y, 0.0), (x + lean, y + rng.uniform(-0.05, 0.05), h), 0.045, 0.006,
             m['grass'] if i % 2 else m['grass_dk'], segs=5)


def build_jar(m):
    """Tinaja de barro: se rompe de un tiro y a veces esconde algo.

    Perfil de PANZA — dos troncos de cono opuestos —, no un cilindro: un cilindro con asas es
    un cubo de fregar. El cuello estrecho y el borde volteado son lo que la leen como vasija
    a 16 unidades, mucho antes que las asas.
    """
    tube('body_lo', (0, 0, 0.02), (0, 0, 0.34), 0.22, 0.36, m['clay'], segs=20, rim_round=0.03)
    tube('body_hi', (0, 0, 0.34), (0, 0, 0.60), 0.36, 0.20, m['clay'], segs=20)
    tube('neck', (0, 0, 0.60), (0, 0, 0.72), 0.16, 0.15, m['clay_dk'], segs=16)
    tube('rim', (0, 0, 0.70), (0, 0, 0.76), 0.21, 0.20, m['clay_lt'], segs=16, rim_round=0.02)
    for i, sx in ((0, 1.0), (1, -1.0)):
        tube('handle%d' % i, (sx * 0.33, 0, 0.34), (sx * 0.40, 0, 0.54), 0.05, 0.05,
             m['clay_dk'], segs=8)
    # Franja clara a media panza: sin ella el barro sale como una mancha lisa
    tube('band', (0, 0, 0.40), (0, 0, 0.46), 0.345, 0.335, m['clay_lt'], segs=20)


def build_crack(m):
    """Grieta en la roca: SOLO la abre el Fuego del Cielo, y tiene que verse mucho antes de
    que el jugador tenga el milagro — ese es el gate de diseno de la fase 4.

    Se monta sobre la silueta de `build_rock` (un canto rodado partido en DOS), y no con
    cajas: con cajas salen dos maletas grises, probado. Lo que dice "esto se abre" es que la
    silueta este PARTIDA y que entre las dos mitades haya una hendidura que se ensancha hacia
    arriba. Y la hendidura tiene que ser geometria HUNDIDA: con luz de estudio, Workbench
    aclara cualquier plano oscuro hasta gris medio, asi que pintarla no vale (misma leccion
    que el vano de la cueva en portraits.py).
    """
    # Las dos mitades: mismo canto rodado de `build_rock`, separado y girado hacia fuera.
    sphere('half_l', (-0.25, 0.02, 0.21), 0.34, m['rock'], scale=(0.85, 0.95, 0.74), segs=10, rings=6)
    sphere('half_r', (0.26, -0.02, 0.19), 0.31, m['rock_lt'], scale=(0.82, 0.92, 0.72), segs=10, rings=6)
    # Cuna oscura entre las dos, mas ancha ARRIBA: eso es lo que se lee como grieta y no
    # como junta. Va hundida en Y para que no sea una raya pegada al frente.
    # DELANTE (y negativa), no detras: puesta atras la tapaban las dos mitades y la grieta se
    # quedaba en una muesca en el borde de arriba. Y llega hasta el suelo, o el canto se lee
    # partido solo por la coronilla.
    box('gap', (0.0, -0.07, 0.19), (0.11, 0.48, 0.42), m['black'],
        taper=(1.9, 1.0), round=0.08)
    box('gap_deep', (0.0, 0.10, 0.15), (0.06, 0.26, 0.32), m['black'], round=0.08)
    # Un canto encima de cada mitad rompe la simetria de las dos medias esferas
    sphere('top_l', (-0.30, -0.12, 0.34), 0.13, m['stone'], scale=(1.0, 0.9, 0.7), segs=8, rings=5)
    sphere('top_r', (0.30, -0.10, 0.30), 0.11, m['rock_dk'], scale=(1.0, 0.9, 0.7), segs=8, rings=5)
    # Lascas al pie: dicen "esto se ha movido alguna vez"
    for i, (x, y, sz) in enumerate(((-0.40, -0.26, 0.11), (0.38, -0.30, 0.09), (0.04, -0.38, 0.07))):
        sphere('chip%d' % i, (x, y, sz * 0.5), sz, m['stone'], scale=(1.2, 1.0, 0.6), segs=6, rings=4)


def build_door(m):
    """Puerta a un interior: dos jambas, dintel y el vano HUNDIDO.

    El vano NO se hace poniendo un plano oscuro: con luz de estudio, Workbench aclara
    cualquier plano oscuro hasta gris medio (misma leccion que la boca de la cueva en
    portraits.py). Se hace con GEOMETRIA — el fondo va metido hacia dentro y las jambas y el
    dintel le echan su propia sombra.

    Y tiene que caber en UNA celda de ancho: los props altos se rendizan en dos celdas y se
    recorta la columna central, asi que lo que se salga de +-U/2 desaparece.
    """
    h = 2.05
    box('back', (0, 0.40, h * 0.46), (U * 0.64, 0.12, h * 0.86), m['black'])
    for i, sx in ((0, -1.0), (1, 1.0)):
        box('jamb%d' % i, (sx * U * 0.36, 0.0, h * 0.5), (0.30, 0.52, h), m['stone'], round=0.14)
    box('lintel', (0, 0.0, h - 0.15), (U * 0.96, 0.56, 0.32), m['stone_lt'], round=0.12)
    box('sill', (0, -0.22, 0.06), (U * 0.86, 0.34, 0.12), m['rock_lt'], round=0.22)
    # Dos sillares sueltos en las jambas: sin ellos las dos piezas salen lisas y la puerta
    # parece un marco de foto.
    for i, (sx, z) in enumerate(((-1.0, 0.62), (1.0, 1.24), (-1.0, 1.50))):
        box('quoin%d' % i, (sx * U * 0.36, -0.02, z), (0.33, 0.50, 0.26), m['rock'], round=0.12)


def build_chest(m):
    box('base', (0, 0, 0.2), (0.9, 0.62, 0.4), m['wood'])
    box('lid', (0, 0, 0.47), (0.92, 0.66, 0.16), m['wood_dk'])
    box('band', (0, 0, 0.3), (0.94, 0.14, 0.62), m['gold'])
    box('lock', (0, -0.33, 0.36), (0.16, 0.06, 0.16), m['gold'])


def build_tree(m):
    tube('trunk', (0, 0, -0.05), (0.02, 0.0, 1.35), 0.16, 0.10, m['trunk'], segs=8)
    tube('branch', (0.02, 0, 1.0), (0.4, 0.05, 1.45), 0.06, 0.03, m['trunk'], segs=6)
    sphere('c0', (0.0, 0.0, 1.85), 0.60, m['leaf'], scale=(1.0, 0.95, 0.72), segs=12, rings=7)
    sphere('c1', (-0.36, -0.1, 1.68), 0.38, m['leaf_dk'], scale=(1, 1, 0.8), segs=10, rings=6)
    sphere('c2', (0.34, -0.15, 1.72), 0.36, m['leaf_lt'], scale=(1, 1, 0.8), segs=10, rings=6)
    sphere('c3', (0.05, -0.3, 2.12), 0.32, m['leaf_lt'], scale=(1, 1, 0.8), segs=10, rings=6)


def build_tree_dead(m):
    tube('trunk', (0, 0, -0.05), (0.03, 0.0, 1.5), 0.15, 0.07, m['dead'], segs=7)
    tube('b0', (0.02, 0, 1.0), (0.45, 0.05, 1.5), 0.06, 0.02, m['dead'], segs=5)
    tube('b1', (0.0, 0, 1.2), (-0.4, -0.05, 1.7), 0.06, 0.02, m['dead'], segs=5)
    tube('b2', (0.03, 0, 1.5), (0.2, 0.0, 2.0), 0.05, 0.015, m['dead'], segs=5)


def build_palm(m):
    tube('trunk', (0, 0, -0.05), (0.15, 0.0, 1.7), 0.14, 0.09, m['trunk'], segs=8)
    for i in range(6):
        a = i * math.pi / 3.0
        dx, dy = math.cos(a) * 0.55, math.sin(a) * 0.4
        tube(f'leaf{i}', (0.15, 0, 1.75), (0.15 + dx, dy, 1.75 - 0.35), 0.10, 0.02, m['leaf'] if i % 2 else m['leaf_lt'], segs=5)
    sphere('coco', (0.15, -0.1, 1.68), 0.1, m['trunk'], segs=6, rings=4)


def build_altar_god(m):
    box('base', (0, 0, 0.18), (1.05, 0.8, 0.36), m['stone'])
    box('mid', (0, 0, 0.5), (0.85, 0.62, 0.3), m['stone_lt'])
    box('top', (0, 0, 0.72), (0.95, 0.7, 0.14), m['stone'])
    sphere('f0', (0, 0, 1.05), 0.3, m['fire'], scale=(1, 0.9, 1.15), segs=8, rings=6)
    sphere('f1', (0.03, -0.05, 1.22), 0.17, m['fire_lt'], scale=(1, 0.9, 1.3), segs=8, rings=6)


def build_altar_baal(m):
    box('base', (0, 0, 0.2), (1.05, 0.8, 0.4), m['purple_dk'])
    box('mid', (0, 0, 0.55), (0.8, 0.6, 0.3), m['purple'])
    tube('h0', (-0.32, 0, 0.7), (-0.5, 0.0, 1.35), 0.07, 0.02, m['purple_dk'], segs=6)
    tube('h1', (0.32, 0, 0.7), (0.5, 0.0, 1.35), 0.07, 0.02, m['purple_dk'], segs=6)
    sphere('gem', (0, -0.2, 0.9), 0.14, m['ruby'], segs=8, rings=5)
    box('slab', (0, 0, 0.75), (0.7, 0.5, 0.1), m['black'])


def build_torch(m):
    tube('pole', (0, 0, -0.05), (0, 0, 1.4), 0.07, 0.05, m['wood_dk'], segs=6)
    box('cup', (0, 0, 1.45), (0.3, 0.3, 0.16), m['rock_dk'])
    sphere('f0', (0, 0, 1.72), 0.2, m['fire'], scale=(1, 0.9, 1.3), segs=8, rings=6)
    sphere('f1', (0.02, -0.04, 1.85), 0.11, m['fire_lt'], scale=(1, 0.9, 1.4), segs=8, rings=6)


def build_exit(m):
    """Chapter exit: stone arch with a glowing doorway."""
    box('l', (-0.45, 0, 0.9), (0.3, 0.5, 1.8), m['rock'])
    box('r', (0.45, 0, 0.9), (0.3, 0.5, 1.8), m['rock'])
    box('top', (0, 0, 1.92), (1.2, 0.5, 0.3), m['rock_lt'])
    box('key', (0, -0.05, 2.1), (0.3, 0.45, 0.2), m['gold'])
    box('glow', (0, 0.05, 0.85), (0.62, 0.1, 1.7), m['fire_lt'])
    box('glow2', (0, 0.0, 0.85), (0.34, 0.12, 1.5), m['white'])
    box('step', (0, -0.3, 0.06), (1.2, 0.35, 0.12), m['stone_lt'])


# --- small centred sprites (pickups / projectiles): camera looks at origin, object floats at z=0
def build_heart(m):
    sphere('l', (-0.17, 0, 0.12), 0.2, m['ruby'], segs=8, rings=6)
    sphere('r', (0.17, 0, 0.12), 0.2, m['ruby'], segs=8, rings=6)
    box('tip', (0, 0, -0.1), (0.42, 0.3, 0.42), m['ruby'], rot=Matrix.Rotation(math.radians(45), 3, 'Y'))
    sphere('shine', (-0.2, -0.15, 0.2), 0.06, m['white'], segs=6, rings=4)


def build_orb(m):
    sphere('o', (0, 0, 0), 0.32, m['sky'], segs=10, rings=7)
    sphere('c', (0, -0.1, 0.05), 0.18, m['white'], segs=8, rings=5)
    for i in range(4):
        a = i * math.pi / 2 + 0.4
        sphere(f'sp{i}', (math.cos(a) * 0.42, 0, math.sin(a) * 0.42), 0.06, m['fire_lt'], segs=5, rings=4)


def build_fireball(m):
    """Comet flying toward +X: white-hot core, orange body, cone tail and two flame licks.
    The tail must stay on -X — the projectile is drawn rotated into its flight direction."""
    sphere('body', (0.06, 0, 0), 0.30, m['fire'], segs=12, rings=8)
    # Trail: shrinking blobs, not a straight cone — reads as flame instead of a rocket
    sphere('t0', (-0.26, 0, 0.03), 0.21, m['fire'], scale=(1.2, 1, 1), segs=10, rings=6)
    sphere('t1', (-0.53, 0, 0.06), 0.13, m['fire'], scale=(1.2, 1, 1), segs=8, rings=5)
    sphere('t2', (-0.74, 0, 0.10), 0.07, m['fire_lt'], segs=6, rings=4)
    # Hot core: pushed toward the camera (-Y) so it is not swallowed by the body
    sphere('core', (0.10, -0.20, 0.01), 0.19, m['fire_lt'], segs=10, rings=7)
    sphere('hot', (0.13, -0.30, 0.02), 0.10, m['white'], segs=8, rings=5)


def build_lightbolt(m):
    """Same comet silhouette in holy white/blue (Eliseo)."""
    sphere('body', (0.06, 0, 0), 0.28, m['sky'], segs=12, rings=8)
    sphere('t0', (-0.25, 0, 0.03), 0.19, m['sky'], scale=(1.2, 1, 1), segs=10, rings=6)
    sphere('t1', (-0.50, 0, 0.06), 0.12, m['water_lt'], scale=(1.2, 1, 1), segs=8, rings=5)
    sphere('t2', (-0.70, 0, 0.09), 0.06, m['white'], segs=6, rings=4)
    sphere('core', (0.09, -0.19, 0.01), 0.18, m['water_lt'], segs=10, rings=7)
    sphere('hot', (0.12, -0.29, 0.02), 0.10, m['white'], segs=8, rings=5)


def build_arrow(m):
    tube('shaft', (-0.5, 0, 0), (0.35, 0, 0), 0.05, 0.05, m['wood'], segs=5)
    tube('head', (0.3, 0, 0), (0.58, 0, 0), 0.13, 0.01, m['stone_lt'], segs=5)
    box('fl', (-0.42, 0, 0.1), (0.2, 0.04, 0.16), m['white'])
    box('fl2', (-0.42, 0, -0.1), (0.2, 0.04, 0.16), m['white'])


def build_darkbolt(m):
    """Enemy version of the comet: violet body, pale pink core."""
    mag = M('magenta', '#b04ae0')
    sphere('body', (0.06, 0, 0), 0.29, mag, segs=12, rings=8)
    sphere('t0', (-0.25, 0, 0.03), 0.20, mag, scale=(1.2, 1, 1), segs=10, rings=6)
    sphere('t1', (-0.51, 0, 0.06), 0.12, m['purple'], scale=(1.2, 1, 1), segs=8, rings=5)
    sphere('t2', (-0.71, 0, 0.09), 0.06, m['purple'], segs=6, rings=4)
    sphere('core', (0.09, -0.19, 0.01), 0.17, M('pink', '#ffb0f0'), segs=10, rings=7)


def build_skull(m):
    sphere('head', (0, 0, 0.1), 0.3, m['white'], scale=(1, 0.9, 1), segs=10, rings=7)
    box('jaw', (0, -0.05, -0.22), (0.36, 0.3, 0.2), m['white'])
    sphere('e0', (-0.12, -0.28, 0.12), 0.08, m['black'], segs=6, rings=4)
    sphere('e1', (0.12, -0.28, 0.12), 0.08, m['black'], segs=6, rings=4)


def build_star(m):
    for i in range(4):
        a = i * math.pi / 4
        box(f'ray{i}', (0, 0, 0), (0.9, 0.12, 0.14), m['gold'], rot=Matrix.Rotation(a, 3, 'Y'))
    sphere('c', (0, -0.05, 0), 0.16, m['fire_lt'], segs=8, rings=6)


SMALL = {'heart': build_heart, 'orb': build_orb, 'fireball': build_fireball, 'lightbolt': build_lightbolt,
         'arrow': build_arrow, 'darkbolt': build_darkbolt, 'skull': build_skull, 'star': build_star}


# =============================================================================
# Shore pieces (autotiling banks). Authored in GAME units on a -8..8 cell with
# N = +Y, drawn over the water tile: wet sand band + foam line on the water side.
# rooms.cpp rotates the same cell 90/180/270 for the other sides/corners.
# =============================================================================
def _g(v):
    """Game units (16 per cell) -> Blender units."""
    return v * U / 16.0


def _densify(pts, step):
    """Insert points so consecutive discs overlap instead of reading as a string of pearls."""
    out = []
    for i, p in enumerate(pts):
        out.append(p)
        if i + 1 >= len(pts):
            break
        q = pts[i + 1]
        d = math.hypot(q[0] - p[0], q[1] - p[1])
        for k in range(1, int(d / step)):
            f = k * step / d
            out.append((p[0] + (q[0] - p[0]) * f, p[1] + (q[1] - p[1]) * f))
    return out


def _blobs(pts, mat, radius, prefix, z=0.02, jitter=0.10, seed=0):
    """Chain of flat discs along a polyline: an organic band with rounded ends."""
    rng = random.Random(seed)
    for i, (gx, gy) in enumerate(_densify(pts, radius * 0.6)):
        r = _g(radius) * (1.0 + rng.uniform(-jitter, jitter))
        tube(f'{prefix}{i}', (_g(gx), _g(gy), z), (_g(gx), _g(gy), z + 0.02), r, r, mat, segs=16)


def _arc(cx, cy, radius, a0, a1, step=9.0):
    n = max(2, int(abs(a1 - a0) / step) + 1)
    return [(cx + radius * math.cos(math.radians(a0 + (a1 - a0) * i / (n - 1))),
             cy + radius * math.sin(math.radians(a0 + (a1 - a0) * i / (n - 1))))
            for i in range(n)]


SHORE_LINE = 6.5    # reference line of the bank, in units from the cell centre (tile edge = 8)
SHORE_ARC = 5.5     # corner radius of that line
# Three layers from land to water, each an offset of the same line: dry sand blends into the
# floor tile next door, wet sand is the dark rim that makes the curve readable, foam is a thin
# bright lip on the water. (offset toward the water, disc radius)
SHORE_LAYERS = (('sand', -1.0, 2.6), ('wet', 0.8, 1.9), ('foam', 2.5, 0.65))


def _shore_layers(line, m, seed):
    """line(offset) -> polyline pushed `offset` units toward the water."""
    for i, (mat, off, r) in enumerate(SHORE_LAYERS):
        _blobs(line(off), m[mat], r, mat, z=0.02 + 0.02 * i,
               jitter=0.05 if mat == 'foam' else 0.10, seed=seed + i)


def build_shore_edge(m):
    """Land on the N side only: straight bank along the top of the cell."""
    xs = [-11 + 1.2 * i for i in range(20)]
    _shore_layers(lambda off: [(x, SHORE_LINE - off) for x in xs], m, seed=1)


def build_shore_out(m):
    """Land on N and E: the band turns the corner on a radius instead of a right angle.
    The straight parts match build_shore_edge so a corner tile blends with its neighbours."""
    cx = cy = SHORE_LINE - SHORE_ARC

    def line(off):
        pts = [(-11 + 1.2 * i, SHORE_LINE - off) for i in range(int((cx + 11) / 1.2) + 1)]
        pts += _arc(cx, cy, SHORE_ARC - off, 90.0, 0.0)
        pts += [(SHORE_LINE - off, cy - 1.2 * i) for i in range(1, int((cy + 11) / 1.2) + 1)]
        return pts

    _shore_layers(line, m, seed=4)


def build_shore_in(m):
    """Water on N and E but land on the NE diagonal: rounded tongue of sand in the corner.
    Here the layers are arcs around the corner itself instead of offsets of a straight line."""
    _blobs([(8, 8), (8, 5.6), (5.6, 8), (6.9, 6.9)], m['sand'], 2.9, 'dry', seed=7)
    _blobs(_arc(8, 8, 4.0, 180.0, 270.0, step=10.0), m['wet'], 1.6, 'wet', z=0.04, seed=8)
    _blobs(_arc(8, 8, 5.4, 180.0, 270.0, step=7.0), m['foam'], 0.65, 'foam', z=0.06,
           jitter=0.05, seed=9)


SHORES = {'shore_edge': build_shore_edge, 'shore_out': build_shore_out, 'shore_in': build_shore_in}

PROPS_TALL = {'tree': build_tree, 'altar_god': build_altar_god, 'altar_baal': build_altar_baal,
              'torch': build_torch, 'exit': build_exit, 'tree_dead': build_tree_dead, 'palm': build_palm,
              'cliff_tall': build_cliff_tall, 'cliff_rock_tall': build_cliff_rock_tall,
              'door': build_door}
PROPS = {'cliff': build_cliff, 'cliff_rock': build_cliff_rock,
         'jar': build_jar, 'crack': build_crack, 'rock': build_rock, 'bush': build_bush, 'grass': build_grass, 'chest': build_chest}
GROUND = {'floor_a': lambda m: build_floor(m, 0), 'floor_b': lambda m: build_floor(m, 1),
          'floor_c': lambda m: build_floor(m, 2), 'floor_d': lambda m: build_floor(m, 3),
          'path_a': lambda m: build_path(m, 0), 'path_b': lambda m: build_path(m, 1),
          'water_a': lambda m: build_water(m, 0), 'water_b': lambda m: build_water(m, 1),
          'ford': build_ford, 'bridge': build_bridge, 'pit': build_pit,
          'grass_a': lambda m: build_meadow(m, 0), 'grass_b': lambda m: build_meadow(m, 1),
          'grass_c': lambda m: build_meadow(m, 2), 'grass_d': lambda m: build_meadow(m, 3),
          'stone_a': lambda m: build_stone(m, 0), 'stone_b': lambda m: build_stone(m, 1),
          'stone_c': lambda m: build_stone(m, 2), 'stone_d': lambda m: build_stone(m, 3)}
DETAILS = {'det_tuft_a': lambda m: det_tuft(m, 0), 'det_tuft_b': lambda m: det_tuft(m, 1),
           'det_pebbles': det_pebbles, 'det_flowers': det_flowers, 'det_crack': det_crack,
           'det_twigs': det_twigs, 'det_bones': det_bones, 'det_moss': det_moss}

_K = CELL // 16        # px per game unit (4)
CLIFF_FEET_ROW = 12 * _K   # origin row for cliff: its front-bottom edge (y=-U/2) lands on the cell bottom
PROP_FEET_ROW = 13 * _K    # 1-cell props: contact point 3 units above the cell bottom (rooms.cpp sort_y = py+13)
TALL_FEET_ROW = 29 * _K    # 2-cell props (bottom cell): contact 3 units above the cell bottom
SMOOTH = True              # soft post (no palette) for everything; False = legacy pixel art


# =============================================================================
# Render
# =============================================================================
def _render_one(px, palette, outline, alpha_cut=0.5, gain=None):
    hi = render._render_to_array()
    return _post(hi, px, palette, outline, alpha_cut, gain)


def _load_sheet(path, shape):
    """Read back a previously saved tiles.png so `only=[...]` can patch single cells
    instead of blanking the other 40+. 'Non-Color' round-trips save_png() (no transform)."""
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
    if arr.shape == shape:
        return arr
    # Sheet grew (new rows added): keep what fits, leave the new rows empty
    out = np.zeros(shape, dtype=np.float32)
    h, w = min(arr.shape[0], shape[0]), min(arr.shape[1], shape[1])
    out[:h, :w] = arr[:h, :w]
    return out


def render_tileset(out_path, preview_path=None, log=print, only=None):
    shape = (TILE_ROWS * CELL, TILE_COLS * CELL, 4)
    sheet = np.zeros(shape, dtype=np.float32)
    if only is not None:
        prev = _load_sheet(out_path, shape)
        if prev is not None:
            sheet = prev
            log('  (patching existing tiles.png)')
    names = list(LAYOUT.keys()) if only is None else only

    def place(name, fr):
        col, row, cw, ch = LAYOUT[name]
        y0, x0 = row * CELL, col * CELL
        sheet[y0:y0 + ch * CELL, x0:x0 + cw * CELL] = fr

    for name in names:
        parts.clear_scene()
        m = mats()
        pal = None
        palette = (lambda: None) if SMOOTH else _palette
        if name in GROUND:
            GROUND[name](m)
            _cam_ground()
            bpy.context.scene.display.shading.light = 'FLAT'
            pal = palette()
            fr = _render_one(CELL, pal, outline=False, alpha_cut=0.3, gain=1.0)
            fr[..., 3] = 1.0  # ground is always opaque
        elif name in DETAILS:
            DETAILS[name](m)
            _cam_ground()                 # top-down like the floor it lies on
            bpy.context.scene.display.shading.light = 'STUDIO'
            pal = palette()
            fr = _render_one(CELL, pal, outline=False)   # soft alpha decal, no outline
        elif name in SHORES:
            SHORES[name](m)
            _cam_ground()                 # top-down, flat light: the sand must match the floor tiles
            shading = bpy.context.scene.display.shading
            shading.light = 'FLAT'
            cav, spec = shading.show_cavity, shading.show_specular_highlight
            # Cavity/specular would outline every overlapping disc ("string of pearls")
            shading.show_cavity = False
            shading.show_specular_highlight = False
            pal = palette()
            fr = _render_one(CELL, pal, outline=False, alpha_cut=0.3, gain=1.0)
            shading.show_cavity, shading.show_specular_highlight = cav, spec
        elif name in SMALL:
            SMALL[name](m)
            _cam_prop(CELL, CELL * 0.5)   # origin at the cell centre
            bpy.context.scene.display.shading.light = 'STUDIO'
            pal = palette()
            fr = _render_one(CELL, pal, outline=True)
        elif name in PROPS:
            cliff = name.startswith('cliff')
            PROPS[name](m)
            _cam_prop(CELL, CLIFF_FEET_ROW if cliff else PROP_FEET_ROW)
            bpy.context.scene.display.shading.light = 'FLAT' if cliff else 'STUDIO'
            pal = palette()
            fr = _render_one(CELL, pal, outline=not cliff, gain=1.0 if cliff else None)
            if cliff:
                fr[..., 3] = (fr[..., 3] >= 0.5).astype(np.float32)  # cliffs tile edge-to-edge: hard alpha, no AA seams
        else:
            cliff = name.startswith('cliff')
            PROPS_TALL[name](m)
            _cam_prop(2 * CELL, CLIFF_FEET_ROW + CELL if cliff else TALL_FEET_ROW)
            bpy.context.scene.display.shading.light = 'FLAT' if cliff else 'STUDIO'
            pal = palette()
            full = _render_one(2 * CELL, pal, outline=not cliff, gain=1.0 if cliff else None)
            fr = full[:, CELL // 2: CELL // 2 + CELL]     # crop centre columns (1 cell wide)
            if cliff:
                fr[..., 3] = (fr[..., 3] >= 0.5).astype(np.float32)
        place(name, fr)
        log(f'  tile {name} done')

    render.save_png(sheet, out_path, 'mantle_tiles')
    if preview_path:
        render.upscale_preview(sheet, preview_path, scale=2, cell=CELL)
    log(f'tileset -> {out_path}')
    return sheet


def main(out_dir, tmp_dir, only=None):
    render._TMP = tmp_dir
    out = os.path.join(out_dir, 'tiles.png')
    pv = os.path.join(tmp_dir, 'tiles_preview.png')
    render_tileset(out, pv, only=only)
    return out, pv
