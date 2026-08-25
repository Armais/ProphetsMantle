"""Cinematic panels for the prologue: wide multi-character dioramas.

Same Workbench look as the sprite sheets (it reuses `render.setup_scene` for shading,
film transparency and PNG settings) but with a WIDE frame and a PERSPECTIVE camera aimed
at an arbitrary target. The sheets use an orthographic camera because a sprite has to tile
with the game grid; a landscape does not, and in ortho there is no horizon — distant hills
just climb the screen forever instead of sitting on the skyline.

Each panel is split into three TRANSPARENT layers that the engine pans at different speeds:

    L0 far   distant silhouettes (mountains, city, cave mouth)   parallax ~0.25
    L1 mid   the actual scene: ground, set, actors               parallax  1.00
    L2 near  foreground framing (rocks, grass, columns)          parallax ~1.70

The SKY is not rendered here. The engine paints it as a vertical gradient: at 320x180
logical units banding is invisible, it costs no texture memory, and it can be tinted and
animated per scene (the sky closing over Israel in escena 6).

Panels are 1600x720 = 400x180 game units at ASSET_SCALE 4, i.e. 80 units of horizontal
slack over the 320-unit screen for the pan.

Run (Blender + blender-mcp):
    import mantle_sprites.cine as c; importlib.reload(c)
    c.main(r.SPRITES_DIR, r.TMP_DIR, only=[1])
"""
import math
import os

import bpy
import numpy as np
from mathutils import Euler, Vector

from . import anims, characters, parts, render, rig, tiles
from .parts import box, get_mat, hexc, sphere, tube, vivid

PANEL_W, PANEL_H = 1600, 720
SS = 2                      # supersampling (render at PANEL*SS)
OUTLINE_PX = 3              # 2 px is calibrated for 128 px cells; a full panel needs a hair more

LAYERS = ('L0', 'L1', 'L2')
FAR, MID, NEAR = 0, 1, 2

_TMP = None


# =============================================================================
# Materials
# =============================================================================
def M(key, col, sat=1.18, val=1.0):
    """Backdrop palette: less punchy than the characters (SAT 1.35) so actors read on top."""
    return get_mat(f'cine_{key}', vivid(hexc(col), sat, val))


# =============================================================================
# Layers
# =============================================================================
def _collection(name):
    col = bpy.data.collections.get(name)
    if col is None:
        col = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(col)
    if name not in {c.name for c in bpy.context.scene.collection.children}:
        bpy.context.scene.collection.children.link(col)
    col.hide_render = False
    return col


def _put(ob, layer):
    for c in list(ob.users_collection):
        c.objects.unlink(ob)
    _collection(LAYERS[layer]).objects.link(ob)
    return ob


def build(layer, fn, *a, **kw):
    """Run a builder and file everything it created into `layer`. Returns the new objects."""
    before = set(bpy.data.objects)
    fn(*a, **kw)
    made = [ob for ob in bpy.data.objects if ob not in before]
    for ob in made:
        _put(ob, layer)
    return made


def place(layer, fn, at=(0.0, 0.0, 0.0), scale=1.0, rot_z=0.0, *a, **kw):
    """Same, for the tiles.py prop builders: they always build at the origin unparented,
    so scaling/rotating means touching the vertices and then moving the object."""
    made = build(layer, fn, *a, **kw)
    for ob in made:
        if ob.type != 'MESH':
            continue
        if scale != 1.0:
            for v in ob.data.vertices:
                v.co *= scale
        if rot_z:
            parts.rotate_about(ob, (0.0, 0.0, 0.0), 'Z', rot_z)
        ob.location = at
    return made


# =============================================================================
# Camera / scene
# =============================================================================
def setup_cine(target=(0.0, 0.0, 1.5), dist=18.0, elev=8.0, yaw=0.0, lens=40.0,
               w=PANEL_W, h=PANEL_H, ss=SS):
    """Wide perspective shot. `elev` low = landscape with a high horizon; higher = looking
    down on the actors, closer to the in-game angle."""
    render.setup_scene(_TMP)                 # shading, film_transparent, PNG settings
    sc = bpy.context.scene
    sc.render.resolution_x = w * ss
    sc.render.resolution_y = h * ss
    cam = sc.camera
    cd = cam.data
    cd.type = 'PERSP'
    cd.lens = lens
    cd.sensor_fit = 'HORIZONTAL'             # fov from the lens only, independent of aspect
    cd.shift_x = cd.shift_y = 0.0
    cd.clip_start = 0.05
    cd.clip_end = 20000.0        # las cordilleras del fondo estan a kilometros: 1 unidad = 1 metro
    e, y = math.radians(elev), math.radians(yaw)
    off = Vector((0.0, -math.cos(e) * dist, math.sin(e) * dist))
    off.rotate(Euler((0.0, 0.0, y)))
    cam.location = Vector(target) + off
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, y)
    return cam


def frame_at(y, safe=1.0, cam=None):
    """Banda visible `(x0, x1, z0, z1)` en el plano horizontal y=`y`. COLOCA CON ESTO.

    Cinco de las ocho capas NEAR salieron en blanco por ponerlas a ojo: a 4 m de la camara
    el cuadro mide 3 m de ancho, no 12. El sensor es de 36 mm con ajuste HORIZONTAL, asi que
    media anchura angular es `18/lens` y media altura `18/lens * PANEL_H/PANEL_W`; lo demas
    es proyectar los cuatro rayos del borde sobre el plano y=`y`.

    `safe` estrecha la banda SOLO EN X para lo que tenga que verse siempre: el motor ensena
    320 de las 400 unidades del panel y el parallax de la capa cerca la mueve otras +-21, o
    sea que el margen fiable es el 65% central (`safe=0.65`, `SAFE_NEAR`). En vertical el
    panel se ve entero — 180 unidades de 180 — asi que ahi no se recorta nada.

    Requiere `setup_cine` YA llamado: en estas escenas la camara se monta primero y el
    decorado se cuelga de ella, no al reves.
    """
    cam = cam or bpy.context.scene.camera
    if abs(cam.rotation_euler.z) > 1e-4:
        raise ValueError('frame_at asume yaw 0 (ninguna escena lo usa)')
    d = y - cam.location.y                       # la camara mira hacia +Y
    if d <= 0.05:
        raise ValueError(f'frame_at: y={y} cae detras de la camara (y={cam.location.y:.2f})')
    pitch = math.radians(90.0) - cam.rotation_euler.x      # >0 = picada
    tan_h = 18.0 / cam.data.lens
    th = math.atan(tan_h * (PANEL_H / float(PANEL_W)))
    half_x = d * tan_h * safe / math.cos(pitch)
    return (cam.location.x - half_x, cam.location.x + half_x,
            cam.location.z - d * math.tan(pitch + th),
            cam.location.z - d * math.tan(pitch - th))


def near_y(z=0.0, bite=0.75):
    """`y` donde plantar el primer plano para que ENTRE POR EL BORDE INFERIOR del cuadro.

    El borde inferior corta el suelo a una distancia concreta de la camara. Todo lo que se
    apoye mas lejos de esa raya se ve ENTERO, con la base flotando en mitad del panel, y deja
    de leerse como encuadre en sombra: parece un menhir plantado en el llano. `bite` acerca
    la silueta a la camara, o sea cuanto mordisco se lleva del borde.

    Devuelve None si la camara no llega a mirar al suelo (encuadres muy horizontales).
    """
    cam = bpy.context.scene.camera
    pitch = math.radians(90.0) - cam.rotation_euler.x
    tan_v = (18.0 / cam.data.lens) * (PANEL_H / float(PANEL_W))
    drop = math.tan(pitch + math.atan(tan_v))
    if drop <= 1e-4:
        return None
    return cam.location.y + (cam.location.z - z) / drop * bite


SAFE_NEAR = 0.78        # el borde de PANTALLA (80% central del panel) menos el paneo
SAFE_MID = 0.80         # el medio se pierde menos: parallax 1.0


def lift(zb, zt, k):
    """Altura (desde el suelo) para que algo del primer plano entre `k` en el cuadro.

    Una roca a 4 m de una camara puesta a 5 m de alto tiene TODA la base fuera de campo: el
    cuadro ahi empieza en z=4,2. Lo que la hace visible no es medir 4 m sino pasar de `zb`,
    que es exactamente lo que hace una silueta que entra por el borde inferior.
    """
    return zb + (zt - zb) * k


def spread(n, x0, x1, inset=0.10):
    """n posiciones repartidas dentro de [x0, x1], sin pegarse a los bordes."""
    a, b = x0 + (x1 - x0) * inset, x1 - (x1 - x0) * inset
    return [a] if n == 1 else [a + (b - a) * i / (n - 1.0) for i in range(n)]


_FLAT = set()


def flat_layer(*layers):
    """Renderiza esas capas con luz FLAT y sin cavity/specular: SILUETA, no volumen.

    Una cordillera son doce esferas solapadas y el cavity subraya cada interseccion, asi que
    a lo lejos se ve un collar de perlas en vez de un perfil de montanas (el mismo problema
    que tuvieron las piezas de orilla de tiles.py, y la misma cura). No vale para las capas
    lejanas de silleria: ahi el cavity es justo lo que dibuja las juntas.
    """
    _FLAT.update(layers)


def _shading(flat):
    sh = bpy.context.scene.display.shading
    sh.light = 'FLAT' if flat else render.LIGHT_MODE
    sh.show_cavity = not flat
    sh.show_specular_highlight = not flat


def new_scene():
    parts.clear_scene()
    _FLAT.clear()
    for name in LAYERS:                      # clear_scene drops objects, not collections
        _collection(name)


# =============================================================================
# Terrain / set pieces
# =============================================================================
def ground(layer, col, far=55.0, near=-300.0, half_w=400.0, z=0.0):
    """Flat plane that ENDS at `far`. In perspective it recedes towards the horizon, and
    everything the far layer holds has to live beyond that edge or the mid ground — drawn
    on top of it — would cover it."""
    return build(layer, box, 'cine_ground', (0.0, (far + near) * 0.5, z - 0.5),
                 (half_w * 2.0, far - near, 1.0), M('ground', col))


def hill(layer, tag, x, y, r, col, sy=1.4, sz=0.55, z=0.0):
    return build(layer, sphere, f'cine_hill_{tag}', (x, y, z), r, M(f'hill_{col}', col),
                 scale=(sy, 1.0, sz))


def ridge(layer, tag, xs, y, r, col, sz=0.55, jitter=0.0, z=0.0):
    """A row of overlapping mounds: a mountain range in one call."""
    out = []
    for i, x in enumerate(xs):
        rr = r * (1.0 + (jitter * ((i * 37 % 11) / 11.0 - 0.5)))
        out += hill(layer, f'{tag}{i}', x, y + (i % 3) * 1.5, rr, col,
                    sz=sz * (0.85 + 0.3 * ((i * 53 % 7) / 7.0)), z=z)
    return out


def smoke(layer, tag, x, y, z0, h=45.0, col='#6e6a72', r0=1.6, r1=7.0, drift=8.0, segs=2):
    """Column of altar smoke: ONE tapered tube per segment, not a stack of spheres.

    Overlapping spheres are the obvious way to build a plume and it does not work here:
    Workbench's cavity pass darkens every intersection, so the column comes out threaded
    like a screw (the same "string of pearls" that bit the shore tiles). A single cone has
    no internal creases.
    """
    out = []
    mat = M(f'smoke_{col}', col)
    for i in range(segs):
        t0, t1 = i / segs, (i + 1) / segs
        out += build(layer, tube, f'cine_smoke_{tag}{i}',
                     (x + drift * t0 * t0, y, z0 + h * t0),
                     (x + drift * t1 * t1, y, z0 + h * t1),
                     r0 + (r1 - r0) * t0, r0 + (r1 - r0) * t1, mat, segs=20, caps=False)
    return out


def road_strip(layer, tag, pts, w0, w1, col, z=0.05):
    """Continuous ribbon through (x, y) points, tapering from w0 to w1. One quad per span
    sharing its edge with the next: separate boxes read as a staircase, not as a road."""
    mat = M(f'road_{tag}', col)
    n = len(pts)
    for i in range(n - 1):
        t0, t1 = i / (n - 1.0), (i + 1) / (n - 1.0)
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        # El ancho va PERPENDICULAR al trazado. Desplazarlo siempre en X solo funciona si el
        # camino corre en Y; un arroyo que cruza el plano sale como una raya de un pixel.
        dx, dy = x1 - x0, y1 - y0
        L = math.hypot(dx, dy) or 1.0
        px, py = -dy / L, dx / L
        h0 = (w0 + (w1 - w0) * t0) * 0.5
        h1 = (w0 + (w1 - w0) * t1) * 0.5
        build(layer, parts.plane_quad, f'cine_road_{tag}{i}',
              [(x0 - px * h0, y0 - py * h0, z), (x0 + px * h0, y0 + py * h0, z),
               (x1 + px * h1, y1 + py * h1, z), (x1 - px * h1, y1 - py * h1, z)], mat)


def tuft(layer, tag, x, y, z, n, col, h=0.5, spread=0.6, w=0.07, lean=0.0):
    """Foreground grass/reed silhouette."""
    out = []
    mat = M(f'tuft_{col}', col)
    for i in range(n):
        a = (i * 2.399)
        dx, dy = math.cos(a) * spread * ((i % 4) + 1) / 4.0, math.sin(a) * spread * 0.4
        hh = h * (0.6 + 0.7 * ((i * 29 % 9) / 9.0))
        ob = build(layer, box, f'cine_tuft_{tag}{i}', (x + dx, y + dy, z + hh * 0.5),
                   (w, w, hh), mat, round=0.7, taper=(0.15, 0.15))
        if lean:
            for o in ob:
                parts.rotate_about(o, (x + dx, y + dy, z), 'Y',
                                   lean * (0.4 + ((i * 17 % 5) / 5.0)) * (1 if i % 2 else -1))
        out += ob
    return out


def spire(layer, tag, x, y, h, r, col, lean=0.0, round=0.92):
    """Upright rock in the immediate foreground: breaks the bottom edge of the frame.

    Heavily bevelled on purpose: a lightly rounded taper reads as a cast concrete block,
    not as stone.
    """
    ob = build(layer, box, f'cine_spire_{tag}', (x, y, h * 0.5), (r * 2.0, r * 1.6, h),
               M(f'spire_{col}', col), round=round, taper=(0.62, 0.66))
    if lean:
        for o in ob:
            parts.rotate_about(o, (x, y, 0.0), 'Y', lean)
    return ob


def transform(objs, at=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, scale=1.0):
    """Scale/rotate/move a group built at the origin (parts authors vertices in world space
    with the object sitting at 0, so the geometry is what moves)."""
    for ob in objs:
        if ob.type != 'MESH':
            continue
        if scale != 1.0:
            for v in ob.data.vertices:
                v.co *= scale
        if pitch:
            parts.rotate_about(ob, (0.0, 0.0, 0.0), 'X', pitch)
        if yaw:
            parts.rotate_about(ob, (0.0, 0.0, 0.0), 'Z', yaw)
        ob.location = at
    return objs


# =============================================================================
# Architecture
# =============================================================================
def stone_wall(layer, tag, x0, x1, y, h, col='#8d8478', col2='#7c7367', bh=0.55, depth=0.5,
               z0=0.0):
    """Ashlar wall: staggered courses. The cavity pass draws the joints for free.

    `z0` starts the courses partway up, which is how you get a lintel over a gate: the wall
    is left+right+lintel, not one slab with a dark box stuck in front of it.
    """
    n = 0
    for r in range(int((h - z0) / bh) + 1):
        z = z0 + r * bh
        x = x0 - (bh * 0.9 if r % 2 else 0.0)
        while x < x1:
            w = min(bh * (1.6 + 0.5 * (((n * 37 + r * 13) % 7) / 7.0)), x1 - x)
            if w < bh * 0.3:
                break
            build(layer, box, f'cine_w{tag}{n}', (x + w * 0.5, y, z + bh * 0.5),
                  (w * 0.94, depth, bh * 0.90),
                  M(f'stone_{col if (n + r) % 2 == 0 else col2}',
                    col if (n + r) % 2 == 0 else col2), round=0.16)
            x += w
            n += 1


def column(layer, tag, x, y, h=4.5, r=0.42, col='#a19889'):
    mat = M(f'col_{col}', col)
    build(layer, box, f'cine_cb{tag}', (x, y, 0.16), (r * 2.9, r * 2.9, 0.32), mat, round=0.3)
    build(layer, tube, f'cine_cs{tag}', (x, y, 0.32), (x, y, h - 0.42), r, r * 0.86, mat, segs=24)
    build(layer, box, f'cine_cc{tag}', (x, y, h - 0.21), (r * 3.0, r * 3.0, 0.42), mat,
          round=0.35, taper=(1.25, 1.25))


def banner(layer, tag, x, y, z_top=4.2, w=0.9, h=2.8, col='#7a1f2c', bar='#c9a24a'):
    build(layer, box, f'cine_bn{tag}', (x, y, z_top - h * 0.5), (w, 0.06, h),
          M(f'banner_{col}', col), round=0.15, taper=(0.82, 1.0))
    build(layer, tube, f'cine_bb{tag}', (x - w * 0.62, y, z_top), (x + w * 0.62, y, z_top),
          0.06, 0.06, M(f'bar_{bar}', bar), segs=14)


def throne(layer, tag, x, y, wood='#4a2c1a', gold='#c9a24a', cloth='#6d1f2e'):
    w, g, c = M(f'thw_{wood}', wood), M(f'thg_{gold}', gold), M(f'thc_{cloth}', cloth)
    build(layer, box, f'cine_th{tag}_dais', (x, y + 0.15, 0.16), (3.6, 2.4, 0.32), w, round=0.12)
    build(layer, box, f'cine_th{tag}_dais2', (x, y + 0.15, 0.44), (2.9, 1.9, 0.28), w, round=0.12)
    build(layer, box, f'cine_th{tag}_seat', (x, y, 0.82), (1.5, 1.1, 0.22), w, round=0.35)
    build(layer, box, f'cine_th{tag}_back', (x, y + 0.46, 1.62), (1.5, 0.20, 1.60), w,
          round=0.30, taper=(0.86, 1.0))
    build(layer, box, f'cine_th{tag}_cush', (x, y, 0.96), (1.28, 0.92, 0.14), c, round=0.6)
    for s in (-1, 1):
        build(layer, box, f'cine_th{tag}_arm{s}', (x + s * 0.80, y - 0.02, 1.06),
              (0.20, 0.95, 0.18), w, round=0.5)
        build(layer, tube, f'cine_th{tag}_post{s}', (x + s * 0.80, y - 0.44, 0.71),
              (x + s * 0.80, y - 0.44, 0.98), 0.10, 0.10, g, segs=16)
        build(layer, sphere, f'cine_th{tag}_ball{s}', (x + s * 0.75, y + 0.46, 2.46), 0.16, g)
    build(layer, box, f'cine_th{tag}_crest', (x, y + 0.46, 2.52), (1.1, 0.16, 0.36), g,
          round=0.5, taper=(0.45, 1.0))


def idol_baal(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, scale=1.0, stone='#2f2436',
              horn='#c9a24a', poles='#3a2a1c'):
    """The image of Baal carried in on a litter: horned, arms raised, on a plinth."""
    s, h = M(f'idol_{stone}', stone), M(f'ihorn_{horn}', horn)
    objs = []
    objs += build(layer, box, f'cine_id{tag}_lit', (0, 0, 0.10), (2.5, 1.3, 0.20),
                  M(f'poles_{poles}', poles), round=0.2)
    for sg in (-1, 1):
        objs += build(layer, tube, f'cine_id{tag}_pole{sg}', (-2.4, sg * 0.52, 0.06),
                      (2.4, sg * 0.52, 0.06), 0.055, 0.055,
                      M(f'poles_{poles}', poles), segs=14)
    objs += build(layer, box, f'cine_id{tag}_plinth', (0, 0, 0.36), (1.5, 0.9, 0.32), s, round=0.2)
    objs += build(layer, box, f'cine_id{tag}_body', (0, 0, 1.18), (0.86, 0.56, 1.32), s,
                  round=0.35, taper=(1.25, 1.15))
    objs += build(layer, sphere, f'cine_id{tag}_head', (0, 0, 2.02), 0.34, s, scale=(1, 0.95, 1.1))
    for sg in (-1, 1):
        objs += build(layer, tube, f'cine_id{tag}_horn{sg}', (sg * 0.24, 0, 2.14),
                      (sg * 0.62, -0.10, 2.52), 0.09, 0.02, h, segs=16)
        objs += build(layer, tube, f'cine_id{tag}_arm{sg}', (sg * 0.42, -0.06, 1.62),
                      (sg * 0.86, -0.30, 2.32), 0.13, 0.10, s, segs=16)
    return transform(objs, at=at, yaw=yaw, scale=scale)


def cave_mouth(layer, tag, x, y, w=6.0, h=4.2, col='#3a3038', dark='#070509'):
    """Dark arch in a rock face: the hundred prophets hidden by fifties.

    Built from rounded lumps rather than tapered boxes — slabs read as a concrete bunker.
    """
    rock, void = M(f'cave_{col}', col), M(f'void_{dark}', dark, sat=1.0, val=0.5)
    # El vano va DELANTE de los bultos de roca (y menor) y los laterales son poco profundos:
    # con las esferas cruzando el centro se comian el hueco y quedaba un peñasco liso.
    build(layer, box, f'cine_cv{tag}_void', (x, y - 1.1, h * 0.42), (w * 0.92, 1.0, h * 0.88),
          void, round=0.75)
    for sg in (-1, 1):
        build(layer, sphere, f'cine_cv{tag}_side{sg}', (x + sg * (w * 0.82), y, h * 0.34),
              w * 0.52, rock, scale=(1.0, 0.95, h / (w * 0.52) * 0.95))
    build(layer, sphere, f'cine_cv{tag}_top', (x, y + 0.2, h * 0.96), w * 0.72, rock,
          scale=(1.45, 1.0, 0.62))
    for i, sg in enumerate((-1.75, 1.75)):
        build(layer, sphere, f'cine_cv{tag}_bl{i}', (x + sg * w * 0.80, y - 1.8, h * 0.12),
              w * 0.32, rock, scale=(1.2, 1.0, 0.75))


def cliff_face(layer, tag, x, y, w, h, col='#7d6a4e', col2='#6b5940', n=5, seed=0):
    """Canyon wall: a stack of offset slabs, so the silhouette is not a single box."""
    for i in range(n):
        t = i / max(1, n - 1.0)
        k = (seed + i * 29) % 7 / 7.0
        build(layer, box, f'cine_cf{tag}{i}',
              (x + (k - 0.5) * w * 0.30, y + (k - 0.5) * 2.0, h * (0.10 + 0.86 * t) * 0.55),
              (w * (1.05 - 0.28 * t), 3.4 + 2.0 * k, h * (0.30 + 0.55 * t)),
              M(f'cliff{i % 2}_{col if i % 2 else col2}', col if i % 2 else col2),
              round=0.22, taper=(0.82, 0.88))


def hooded(layer, tag, x, y, h=1.75, col='#3a3040', yaw=0.0, lean=0.0):
    """Anonymous robed figure: the hidden prophets, the crowd at the gate. Should read as a
    silhouette — a full character would put a face where the story wants none."""
    k = h / 1.75
    mat = M(f'hood_{col}', col)
    objs = []
    # Casi cilindrico: un cono con la base ancha lee como pieza de ajedrez, no como persona.
    objs += build(layer, tube, f'cine_hf{tag}_b', (0, 0, 0.0), (0, 0, 1.22 * k),
                  0.30 * k, 0.26 * k, mat, segs=20)
    objs += build(layer, box, f'cine_hf{tag}_s', (0, 0.01 * k, 1.31 * k),
                  (0.70 * k, 0.34 * k, 0.28 * k), mat, round=0.8, taper=(0.70, 0.85))
    objs += build(layer, sphere, f'cine_hf{tag}_h', (0, -0.03 * k, 1.54 * k), 0.19 * k, mat,
                  scale=(1.0, 1.06, 1.15))
    if lean:
        for ob in objs:
            parts.rotate_about(ob, (0.0, 0.0, 0.0), 'X', lean)
    return transform(objs, at=(x, y, 0.0), yaw=yaw)


def crow(layer, tag, at, yaw=0.0, pitch=0.0, s=1.0, col='#1b1720', beak='#d8a63a',
         bread=False):
    """Los cuervos de Querit. No hay nada aviar en el pipeline, asi que va a mano.

    `bread`: la hogaza en el pico. Sin ella son pajaros negros cualesquiera; con ella son
    los cuervos que le traian pan y carne por la manana y por la tarde.
    """
    body, bk = M(f'crow_{col}', col), M(f'beak_{beak}', beak)
    objs = []
    if bread:
        objs += build(layer, box, f'cine_cr{tag}_p', (0, -0.57, 0.01), (0.17, 0.15, 0.11),
                      M('bread', '#c79a58'), round=0.55, taper=(0.80, 0.85))
    objs += build(layer, sphere, f'cine_cr{tag}_b', (0, 0, 0), 0.20, body, scale=(0.82, 1.65, 0.78))
    objs += build(layer, sphere, f'cine_cr{tag}_h', (0, -0.30, 0.07), 0.115, body)
    objs += build(layer, box, f'cine_cr{tag}_k', (0, -0.45, 0.05), (0.05, 0.17, 0.05), bk,
                  round=0.6, taper=(0.18, 0.25))
    objs += build(layer, box, f'cine_cr{tag}_t', (0, 0.36, 0.0), (0.11, 0.30, 0.032), body,
                  round=0.5, taper=(0.55, 0.45))
    for sg in (-1, 1):
        w = build(layer, box, f'cine_cr{tag}_w{sg}', (sg * 0.40, 0.02, 0.09),
                  (0.66, 0.26, 0.04), body, round=0.6, taper=(0.30, 0.62))
        for o in w:
            parts.rotate_about(o, (0.0, 0.0, 0.0), 'Y', -42.0 * sg)
        objs += w
    return transform(objs, at=at, yaw=yaw, pitch=pitch, scale=s)


# =============================================================================
# Actors
# =============================================================================
def actor(name, at=(0.0, 0.0, 0.0), yaw=0.0, scale=1.0, pose=None, layer=MID):
    """Build a character from characters.SPECS, pose it and drop it in the scene.

    `pose` is a rig.pose_apply dict, merged OVER the character's attack base pose (which is
    what holds the staff/sword), and applied WITHOUT a frame. Deliberately no keyframed
    animation here: `anims.animate` + `frame_set` would look tempting for a ready-made pose,
    but frame_set is global — posing the second actor would re-pose the first one from its
    own action. Explicit poses are frame-independent, so a panel can hold ten actors.
    """
    before = set(bpy.data.objects)
    char = characters.build(name)
    arm = char['arm']
    base = anims.humanoid_base(char.get('attack', 'swing')) if char['kind'] == 'humanoid' else {}
    rig.pose_apply(arm, rig.merge_pose(base, pose) if pose else base)
    arm.location = at
    arm.rotation_euler = (0.0, 0.0, math.radians(yaw))
    arm.scale = (scale, scale, scale)
    for ob in bpy.data.objects:
        if ob not in before:
            _put(ob, layer)
    return char


# Poses used by more than one panel (world axes, see rig.pose_apply)
POSE_KNEEL = {
    'root': {'loc': (0.0, 0.0, -0.42)},
    'thigh.L': {'rot': [('X', 82.0)]}, 'shin.L': {'rot': [('X', -92.0)]},
    'thigh.R': {'rot': [('X', 20.0)]}, 'shin.R': {'rot': [('X', -95.0)]},
    'spine': {'rot': [('X', 10.0)]}, 'chest': {'rot': [('X', 8.0)]},
    'head': {'rot': [('X', 16.0)]},
    'upper_arm.L': {'rot': [('X', -28.0), ('Y', 14.0)]},
    'upper_arm.R': {'rot': [('X', -6.0), ('Y', -10.0)]},
    'forearm.L': {'rot': [('X', -46.0)]}, 'forearm.R': {'rot': [('X', -26.0)]},
}

POSE_POINT = {   # "delante de quien estoy": mano IZQUIERDA al rey, cayado en alto
    # El cayado cuelga de `hand.R` (characters.py), asi que extender ESE brazo lo ponia
    # horizontal: una pertiga de dos metros cruzando la sala y clavandosele a Acab en el
    # pecho. Senala la mano libre y la derecha alza el cayado en diagonal.
    'upper_arm.L': {'rot': [('X', -99.0), ('Y', 12.0)]},
    'forearm.L': {'rot': [('X', -5.0)]},
    'upper_arm.R': {'rot': [('X', 8.0), ('Y', -17.0)]},
    'forearm.R': {'rot': [('X', -8.0)]},
    'chest': {'rot': [('Z', -8.0), ('X', -6.0)]}, 'head': {'rot': [('X', -8.0)]},
    'thigh.L': {'rot': [('X', 14.0)]}, 'thigh.R': {'rot': [('X', -12.0)]},
}

POSE_ENTHRONED = {
    'root': {'loc': (0.0, 0.0, -0.30)},
    'thigh.L': {'rot': [('X', 84.0)]}, 'shin.L': {'rot': [('X', -86.0)]},
    'thigh.R': {'rot': [('X', 84.0)]}, 'shin.R': {'rot': [('X', -86.0)]},
    'upper_arm.L': {'rot': [('X', -14.0), ('Y', 26.0)]},
    'upper_arm.R': {'rot': [('X', -14.0), ('Y', -26.0)]},
    'forearm.L': {'rot': [('X', -58.0)]}, 'forearm.R': {'rot': [('X', -58.0)]},
    'head': {'rot': [('X', -4.0)]},
}

POSE_STAND_PROUD = {
    'upper_arm.L': {'rot': [('Y', 20.0)]}, 'upper_arm.R': {'rot': [('Y', -20.0)]},
    'forearm.L': {'rot': [('X', -22.0)]}, 'forearm.R': {'rot': [('X', -22.0)]},
    'chest': {'rot': [('X', -5.0)]}, 'head': {'rot': [('X', -6.0)]},
}

POSE_GUARD = {
    'upper_arm.L': {'rot': [('Y', 13.0)]}, 'upper_arm.R': {'rot': [('Y', -13.0)]},
    'forearm.L': {'rot': [('X', -12.0)]},
    'thigh.L': {'rot': [('X', 5.0)]}, 'thigh.R': {'rot': [('X', -5.0)]},
}

POSE_WALK = {
    'thigh.L': {'rot': [('X', 25.0)]}, 'shin.L': {'rot': [('X', -15.0)]},
    'thigh.R': {'rot': [('X', -21.0)]}, 'shin.R': {'rot': [('X', -27.0)]},
    'upper_arm.L': {'rot': [('X', -19.0), ('Y', 10.0)]},
    'upper_arm.R': {'rot': [('X', 15.0), ('Y', -10.0)]},
    'chest': {'rot': [('X', -3.0)]},
}

POSE_RAISE = {   # firme en el promontorio, el cayado erguido y el manto al viento
    # NO intentes levantar el brazo aqui: de frente a la camara el brazo alzado pasa por
    # DELANTE de la cara y le tapa el rostro, y subir el brazo del cayado (hand.R) mas alla
    # de la horizontal vuelca el baculo y deja la llama a la altura de la cintura. Lo que
    # lee en un plano de titulo es la silueta: cayado vertical y hombros abiertos.
    'upper_arm.R': {'rot': [('X', 4.0), ('Y', -15.0)]},
    'forearm.R': {'rot': [('X', -8.0)]},
    'upper_arm.L': {'rot': [('X', 8.0), ('Y', 30.0)]},
    'forearm.L': {'rot': [('X', -14.0)]},
    'chest': {'rot': [('X', -11.0)]}, 'head': {'rot': [('X', -15.0)]},
    'thigh.L': {'rot': [('X', 12.0)]}, 'thigh.R': {'rot': [('X', -14.0)]},
}

POSE_DRINK = {   # de rodillas junto al agua, la mano hacia el arroyo
    'root': {'loc': (0.0, 0.0, -0.44)},
    'thigh.L': {'rot': [('X', 86.0)]}, 'shin.L': {'rot': [('X', -96.0)]},
    'thigh.R': {'rot': [('X', 34.0)]}, 'shin.R': {'rot': [('X', -102.0)]},
    'spine': {'rot': [('X', 26.0)]}, 'chest': {'rot': [('X', 20.0)]},
    'head': {'rot': [('X', 26.0)]},
    'upper_arm.L': {'rot': [('X', -62.0), ('Y', 12.0)]},
    'forearm.L': {'rot': [('X', -30.0)]},
    'upper_arm.R': {'rot': [('X', 2.0), ('Y', -8.0)]},
    'forearm.R': {'rot': [('X', -6.0)]},
}


# =============================================================================
# Panels
# =============================================================================
def scene_tierra():
    """1. LA TIERRA PARTIDA — colinas al anochecer, un camino, humo de altares lejanos."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(6.0, 0.0, 6.0), dist=120.0, elev=4.0, lens=38.0)
    flat_layer(FAR)            # cordillera y humo: siluetas, si no son un collar de perlas

    # --- far: cordillera y humo de los altares, MAS ALLA del borde del suelo (far=1000)
    ridge(FAR, 'a', [-2100, -1450, -900, -350, 190, 730, 1300, 1900], 2400.0, 300.0, '#453f5e',
          sz=0.50, jitter=0.35)
    ridge(FAR, 'b', [-1500, -980, -480, 40, 560, 1100, 1620], 1550.0, 210.0, '#5a4f6a',
          sz=0.44, jitter=0.30)
    # A 1200 m el cuadro mide 1130 m de ancho por 510 de alto: una columna de 60 m era una
    # rayita. Con 170-210 m y en un gris mas claro que la cordillera si se leen los altares.
    # Con la capa lejana en FLAT el color sale tal cual, sin sombrear: a #8a8296 las columnas
    # eran tres colmillos blancos. Un punto por encima de la cordillera basta, y estrechas.
    for i, (x, y, hh) in enumerate(((-330.0, 1290.0, 175.0), (110.0, 1160.0, 140.0),
                                    (430.0, 1350.0, 200.0))):
        smoke(FAR, f's{i}', x, y, 6.0, h=hh, col='#6b6379', r0=3.0, r1=13.0, drift=34.0, segs=3)

    # --- mid: suelo, lomas cercanas, el camino y arboles secos
    ground(MID, '#96774a', far=1000.0, near=-500.0, half_w=1600.0)
    ridge(MID, 'c', [-320, -180, -50, 80, 220, 360], 330.0, 46.0, '#856a3c', sz=0.38, jitter=0.3)
    ridge(MID, 'd', [-260, -120, 30, 175, 320], 190.0, 30.0, '#9d7f4b', sz=0.34, jitter=0.35)
    # camino: una cinta continua que se estrecha y se pierde hacia el fondo
    road_strip(MID, 'a', [(-14.0 + math.sin(i / 8.0 * 2.6) * 11.0 + (i / 8.0) * 40.0,
                           -55.0 + (i / 8.0) * 330.0) for i in range(9)],
               11.0, 2.2, '#b3a074')
    for i, (x, y, s) in enumerate(((-46.0, 90.0, 1.5), (58.0, 130.0, 1.2), (-92.0, 170.0, 1.0))):
        place(MID, tiles.build_tree_dead, at=(x, y, 0.0), scale=s * 5.0, m=m)
    # peñascos sueltos: el llano quedaba vacio de aqui al horizonte. La roca del tileset mide
    # medio metro escasa, asi que hace falta multiplicarla por diez para que sea un peñasco.
    for i, (x, y, s) in enumerate(((-140.0, 40.0, 16.0), (96.0, 62.0, 11.0), (-64.0, 128.0, 9.0),
                                   (168.0, 150.0, 13.0), (-190.0, 210.0, 10.0), (24.0, 235.0, 8.0))):
        place(MID, tiles.build_rock, at=(x, y, 0.0), scale=s, rot_z=i * 41.0, m=m)

    # --- near: siluetas VERTICALES que rompen el borde inferior. Un monticulo aqui se veria
    # desde arriba y quedaria en disco: lo que lee es lo que se levanta.
    y0 = near_y(bite=0.75)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    # A los BORDES: en el centro el peñasco se plantaba encima del camino. Y anchos: a
    # r=0,83 salian tres postes de plastilina de seis veces mas altos que gruesos.
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0 - 3.0 + i * 5.0, lift(zb, zt, 0.44 + 0.14 * i),
              (x1 - x0) * 0.085, '#4a3b24', lean=5.0 * (1 if i else -1))
    for i, x in enumerate(spread(4, x0, x1, inset=0.02)):
        tuft(NEAR, f'g{i}', x + 2.4, y0 + 1.0, zb * 0.72, 9, '#5a4726',
             h=(zt - zb) * 0.22, spread=2.2, w=0.30, lean=8.0)


def _throne_hall(m):
    """Set compartido por las escenas 2 y 6: la sala del trono de Samaria.

    Se llama DESPUES de setup_cine: las columnas de encuadre se colocan con frame_at, y las
    dos escenas miran el salon desde distancias distintas.
    """
    ground(MID, '#6f665a', far=7.0, near=-40.0, half_w=40.0)   # acaba ANTES del muro (y=7.2)
    stone_wall(FAR, 'back', -11.0, 11.0, 7.2, 6.4)
    for x in (-2.6, 2.6):
        banner(FAR, f'b{x:.0f}', x, 6.82, z_top=5.0, w=1.05, h=3.1)
    for x in (-3.6, 3.6):
        column(MID, f'c{x:.0f}', x, 5.0, h=5.4, r=0.46)
    throne(MID, 'a', 0.0, 4.3)
    # antorchas de PARED. A escala 2,4 median 4,4 m: salian por el techo y cortadas.
    for x in (-3.0, 3.0):
        place(MID, tiles.build_torch, at=(x, 6.4, 0.0), scale=1.15, m=m)
    # columnas de encuadre: a ras del borde, no fuera. En la 2 caian justo en el filo del
    # cuadro (x=+-3,1 con un semiancho de 3,05) y en la 6 se perdian del todo.
    y0 = near_y(bite=0.80)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0 * 1.03, x1 * 1.03)):
        column(NEAR, f'f{i}', x, y0, h=8.0, r=0.42)


def scene_acab():
    """2. ACAB, REY DE ISRAEL — coronado en Samaria, guardias y estandartes.

    De PIE ante el trono, no sentado: la tunica larga es un cono rigido pegado a la cadera
    y al sentarse se come las piernas, dejando un busto sobre un pedestal.
    """
    new_scene()
    setup_cine(target=(0.0, 2.6, 1.65), dist=13.0, elev=10.0, lens=46.0)
    _throne_hall(tiles.mats())
    actor('acab', at=(0.0, 2.9, 0.0), yaw=0.0, pose=POSE_STAND_PROUD)
    actor('soldado_acab', at=(-2.7, 2.1, 0.0), yaw=-16.0, pose=POSE_GUARD)
    actor('soldado_acab', at=(2.7, 2.1, 0.0), yaw=16.0, pose=POSE_GUARD)


def scene_jezabel():
    """3. JEZABEL DE SIDON — el cortejo entra por la puerta con la imagen de Baal."""
    new_scene()
    m = tiles.mats()
    # Objetivo BAJO (antes 3,10): con la camara a 4,7 m de alto el borde inferior del cuadro
    # a 8 m ya estaba a 2 m del suelo, o sea que el gentio del primer plano no cabia —
    # ninguna altura de persona cabia — y la capa cerca salio en blanco.
    setup_cine(target=(0.0, 3.0, 1.70), dist=15.0, elev=6.0, lens=40.0)
    ground(MID, '#8b7f6a', far=16.6, near=-40.0, half_w=60.0)   # acaba ANTES de la muralla
    # --- far: muralla con vano real (izquierda + derecha + dintel) y sus torres
    stone_wall(FAR, 'l', -26.0, -3.4, 17.0, 9.0)
    stone_wall(FAR, 'r', 3.4, 26.0, 17.0, 9.0)
    stone_wall(FAR, 'top', -3.4, 3.4, 17.0, 9.0, z0=5.0)
    # El vano DELANTE del muro y bien negro: a #191324 el shading lo levantaba a gris azulado
    # y la puerta parecia una pizarra colgada.
    # La puerta es un TUNEL, no una losa negra. Workbench con luz de estudio aclara
    # cualquier plano oscuro hasta gris medio — el vano pintado de negro puro seguia
    # leyendose como una pizarra colgada del muro. La profundidad hay que construirla:
    # jambas que se meten hacia dentro, sofito y fondo, y son las caras en angulo las que
    # se apagan solas.
    for sg in (-1, 1):
        build(FAR, box, f'cine_gate_jamb{sg}', (sg * 2.95, 18.7, 2.35), (0.60, 3.6, 4.70),
              M('gate_jamb', '#584d43'), round=0.05)
    build(FAR, box, 'cine_gate_soffit', (0.0, 18.7, 4.92), (6.5, 3.6, 0.55),
          M('gate_soffit', '#463d35'), round=0.05)
    build(FAR, box, 'cine_gate_back', (0.0, 20.5, 2.30), (5.9, 0.5, 4.60),
          M('gate_back', '#191527', sat=1.0, val=0.7))
    build(FAR, box, 'cine_gate_lintel', (0.0, 16.8, 5.15), (7.8, 1.1, 0.8),
          M('lintel', '#9b9184'), round=0.14)
    for x in (-5.4, 5.4):
        build(FAR, box, f'cine_tower{x:.0f}', (x, 17.4, 6.2), (3.6, 3.8, 12.4),
              M('tower', '#847a6d'), round=0.08, taper=(0.92, 0.92))
        build(FAR, box, f'cine_merlon{x:.0f}', (x, 17.4, 12.7), (4.2, 4.4, 0.7),
              M('merlon', '#6f665a'), round=0.15)

    # --- mid: Jezabel al frente, sacerdotes de Baal y la imagen sobre andas
    actor('jezabel', at=(-1.4, 2.6, 0.0), yaw=-8.0, pose=POSE_WALK)
    actor('profeta_baal', at=(2.6, 4.4, 0.0), yaw=14.0, pose=POSE_WALK)
    actor('profeta_baal', at=(-4.2, 5.6, 0.0), yaw=-18.0, pose=POSE_WALK)
    # Las andas AL HOMBRO (z=1.38) y cuatro porteadores, dos por vara. A la altura de la
    # cadera el idolo flotaba entre ellos, sin nadie que lo llevara.
    idol_baal(MID, 'a', at=(3.0, 8.8, 1.38), yaw=-14.0, scale=1.0)
    for i, (x, y) in enumerate(((0.5, 7.4), (0.5, 10.2), (5.5, 7.4), (5.5, 10.2))):
        hooded(MID, f'bear{i}', x, y, h=1.74, col='#2f2740', yaw=6.0 if i < 2 else -6.0)

    # --- near: gentio de espaldas, cerrando el plano por abajo
    y0 = near_y(z=0.9, bite=1.0)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(5, x0, x1, inset=0.02)):
        hooded(NEAR, f'cr{i}', x, y0 - 0.5 + (i % 3) * 0.4,
               h=1.66 + (i % 4) * 0.05, col='#2a2330', yaw=180.0)


def scene_caceria():
    """4. LA CACERIA — antorchas barriendo la ladera, profetas escondidos en la cueva."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 9.0, 2.30), dist=17.0, elev=11.0, lens=40.0)
    flat_layer(FAR)
    # El suelo acababa a 120 m: llenaba el 95% del panel, o sea que ni el cielo nocturno ni
    # las lomas de la capa lejana llegaban a verse. Ahora muere justo detras de la loma por
    # la que sube la batida, y esa loma hace de horizonte.
    ground(MID, '#4a4436', far=30.0, near=-40.0, half_w=150.0)
    # --- far: la ladera y la partida que bate el monte con antorchas
    ridge(FAR, 'h', [-260, -150, -40, 70, 185, 300], 620.0, 130.0, '#2e2b3a', sz=0.44, jitter=0.3)
    ridge(FAR, 'i', [-180, -80, 30, 140], 320.0, 80.0, '#38344a', sz=0.38, jitter=0.3)
    # La partida sube por una loma. A 30 m eran cuatro puntos: a 20 m y sobre el lomo se
    # leen como una batida, y la antorcha va EN LA MANO (a escala 2,6 median 4,8 m).
    ridge(MID, 'j', [-26, -6, 14, 34, 56], 26.0, 11.0, '#413a30', sz=0.30, jitter=0.3)
    for i, (x, y) in enumerate(((-7.0, 19.0), (-2.5, 21.5), (2.0, 19.5), (6.5, 22.0))):
        actor('soldado_acab', at=(x, y, 1.1), yaw=182.0 + i * 5.0, pose=POSE_WALK, layer=MID)
        place(MID, tiles.build_torch, at=(x + 0.9, y - 0.7, 1.4), scale=0.9, m=m)

    # --- mid: la boca de la cueva a un lado y los que quedaron dentro
    # w=4,2 daba un peñasco de 11 m de ancho (el bulto de arriba mide w*1,05*1,30): se comia
    # el tercio izquierdo del panel y salia cortado por el borde.
    cave_mouth(MID, 'a', -6.6, 14.0, w=3.4, h=3.4)
    for i, (x, y, h, ln) in enumerate(((-7.4, 11.8, 1.70, 14.0), (-6.0, 12.3, 1.78, 6.0),
                                       (-4.6, 11.6, 1.64, 18.0))):
        hooded(MID, f'p{i}', x, y, h=h, col='#3b3242', lean=ln)
    for i, (x, y, r) in enumerate(((-2.4, 10.5, 0.85), (5.2, 13.5, 1.05))):
        hill(MID, f'rk{i}', x, y, r, '#39323f', sy=1.25, sz=0.62)
    place(MID, tiles.build_tree_dead, at=(8.6, 17.5, 0.0), scale=2.2, m=m)

    # --- near: la roca por la que asomamos
    y0 = near_y(bite=0.72)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0, lift(zb, zt, 0.62 + 0.16 * i), (x1 - x0) * 0.09, '#241f2c',
              lean=7.0 * (1 if i else -1))
    for i, x in enumerate((x0 * 0.62, x1 * 0.62)):
        tuft(NEAR, f'g{i}', x, y0 + 0.8, zb * 0.90, 6, '#2b2634', h=(zt - zb) * 0.15,
             spread=0.5, w=0.13, lean=12.0)


def scene_galaad():
    """5. UN HOMBRE DE GALAAD — Elias solo en el monte, de espaldas. La luz baja a el."""
    new_scene()
    # Objetivo bajo: sube al profeta en el encuadre y lo saca de la franja de texto. Y algo
    # mas picada que antes (9 -> 12): de perfil casi horizontal, POSE_KNEEL no se leia como
    # arrodillado sino como un cono de pie.
    setup_cine(target=(0.0, 1.0, 0.70), dist=11.0, elev=12.0, lens=55.0)
    flat_layer(FAR)
    # El suelo acaba a 7 m: detras del profeta hay cielo, no mas llano. Es lo unico que
    # hace que una explanada se lea como CUMBRE.
    ground(MID, '#6d6152', far=7.0, near=-40.0, half_w=60.0)
    # --- far: picos y banco de nubes de tormenta. Los tonos de antes (#3c3850) eran casi el
    # cielo de esta escena ({26,24,46} arriba): la cordillera no se despegaba del fondo.
    ridge(FAR, 'p', [-900, -520, -160, 210, 580, 950], 1400.0, 220.0, '#565270', sz=0.62,
          jitter=0.4)
    ridge(FAR, 'q', [-500, -180, 160, 520], 700.0, 130.0, '#6a6590', sz=0.50, jitter=0.3)
    for i, (x, y, r) in enumerate(((-520.0, 900.0, 190.0), (-90.0, 820.0, 150.0),
                                   (330.0, 880.0, 210.0), (760.0, 940.0, 170.0))):
        build(FAR, sphere, f'cine_cloud{i}', (x, y, 205.0 + (i % 2) * 45.0), r,
              M('cloud', '#7d76a2'), scale=(2.1, 1.0, 0.34))

    # --- mid: la cumbre y el profeta arrodillado, en tres cuartos de espaldas
    actor('elias', at=(0.0, 1.0, 0.0), yaw=104.0, pose=POSE_KNEEL)
    # El filo de la cumbre: sin esto el borde del suelo es una RECTA que cruza el panel de
    # lado a lado y lo que se lee es una mesa, no un monte.
    ridge(MID, 'lip', [-9.5, -7, -4.8, -2.6, -0.5, 1.6, 3.8, 6.2, 8.6], 6.5, 1.15,
          '#5f5546', sz=0.34, jitter=0.35)
    ridge(MID, 'lip2', [-6, -3, 0.5, 4], 5.6, 0.85, '#6d6152', sz=0.30, jitter=0.3)
    for i, (x, y, h, r) in enumerate(((-4.6, 3.4, 1.5, 0.75), (4.4, 3.0, 1.1, 0.60),
                                      (-2.6, 5.4, 0.9, 0.50), (2.9, 5.0, 1.2, 0.62))):
        spire(MID, f'k{i}', x, y, h, r, '#5a5142', lean=5.0 * (1 if i % 2 else -1))

    # --- near: el borde de roca por el que miramos
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0, lift(zb, zt, 0.66 + 0.18 * i), (x1 - x0) * 0.09, '#3a3244',
              lean=8.0 * (1 if i else -1))


def scene_trono():
    """6. DELANTE DE QUIEN ESTOY — Elias irrumpe y senala al rey. Acab de pie, Jezabel al lado."""
    new_scene()
    setup_cine(target=(0.0, 2.4, 1.60), dist=11.0, elev=8.0, lens=45.0)
    _throne_hall(tiles.mats())
    actor('acab', at=(0.6, 3.2, 0.0), yaw=-8.0, pose=POSE_STAND_PROUD)
    # Jezabel a 2,9 caia justo sobre la columna del medio (x=3,6) y salia partida por ella;
    # el soldado de la derecha estaba en 4,8, fuera del cuadro (el semiancho ahi es 4,3).
    actor('jezabel', at=(1.9, 4.4, 0.0), yaw=-14.0, pose=POSE_STAND_PROUD)
    actor('soldado_acab', at=(-2.8, 2.9, 0.0), yaw=-20.0, pose=POSE_GUARD)
    actor('soldado_acab', at=(2.8, 2.7, 0.0), yaw=26.0, pose=POSE_GUARD)
    # Elias de perfil por la izquierda, vuelto hacia el trono: de frente y tan cerca de la
    # camara tapaba al rey y el brazo extendido no se leia.
    actor('elias', at=(-1.3, 1.5, 0.0), yaw=58.0, pose=POSE_POINT)


def scene_querit():
    """7. EL ARROYO DE QUERIT — Elias bebe del arroyo y los cuervos le traen pan."""
    new_scene()
    m = tiles.mats()
    # elev 22 era una picada de vista de juego: TODO el cuadro caia por debajo del horizonte,
    # asi que la capa media tapaba el panel entero (100% opaco) y ni el cielo ni el fondo
    # llegaban a verse. Con lens 45 el limite es elev < 11,4; a 6 queda un cuarto de cielo.
    setup_cine(target=(0.0, 3.4, 1.15), dist=11.0, elev=6.0, lens=45.0)
    flat_layer(FAR)
    ground(MID, '#9a8256', far=18.2, near=-40.0, half_w=60.0)
    # --- far: las laderas de la hondonada. NO paredes de canon: a 30 m esta camara solo
    # abarca 5 m de altura, asi que un tajo de 26 m no sale "imponente", sale tapandolo todo.
    ridge(FAR, 'l', [-24, -18, -12.5, -8], 18.5, 6.0, '#7d6a4e', sz=0.44, jitter=0.3)
    ridge(FAR, 'r', [8.5, 14, 19.5, 26], 19.0, 6.4, '#6f5e44', sz=0.42, jitter=0.3)
    ridge(FAR, 'm', [-16, -12, -8.5, -5, -1.5, 2, 5.5], 18.8, 3.2, '#8a7454', sz=0.40,
          jitter=0.35)
    ridge(FAR, 'b', [-30, -10, 12, 34], 32.0, 9.0, '#5e5039', sz=0.34, jitter=0.3)
    ridge(FAR, 'c', [-260, -120, 30, 190, 340], 620.0, 150.0, '#4e4560', sz=0.34, jitter=0.35)

    # --- mid: el arroyo bajando DESDE EL FONDO hacia la camara. De lado a lado y recto era
    # una cinta azul pegada al suelo; en diagonal la perspectiva lo estrecha sola y se lee
    # que corre. Debajo, un ribete de guijarros: si no, el agua es una calcomania.
    brook = [(4.9, 20.0), (3.6, 16.4), (1.5, 12.8), (0.4, 9.6), (1.0, 6.8),
             (2.1, 4.2), (1.1, 1.6), (-0.9, -0.8), (-2.6, -3.2)]
    road_strip(MID, 'bank', brook, 3.6, 5.4, '#b6a074', z=0.03)
    road_strip(MID, 'w', brook, 2.2, 3.4, '#3f7f96', z=0.06)
    # De perfil hacia el agua (a su derecha) y en la orilla, no detras del arroyo.
    actor('elias', at=(-0.2, 5.2, 0.0), yaw=72.0, pose=POSE_DRINK)
    crow(MID, 'a', (3.2, 6.6, 1.85), yaw=-40.0, pitch=-14.0, s=1.2, bread=True)
    crow(MID, 'b', (-3.4, 10.5, 2.30), yaw=32.0, pitch=-18.0, s=1.05, bread=True)
    for i, (x, y, s) in enumerate(((4.6, 6.0, 2.4), (-4.4, 6.6, 2.0), (6.6, 11.0, 2.8))):
        place(MID, tiles.build_rock, at=(x, y, 0.0), scale=s, rot_z=i * 47.0, m=m)
    place(MID, tiles.build_palm, at=(-5.8, 14.5, 0.0), scale=1.7, m=m)

    # --- near: el labio de la hondonada y unos juncos
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0, lift(zb, zt, 0.60 + 0.16 * i), (x1 - x0) * 0.09, '#6b5a3c',
              lean=6.0 * (1 if i else -1))
    for i, x in enumerate((x0 * 0.55, x1 * 0.55)):
        tuft(NEAR, f'g{i}', x, y0 + 0.7, zb * 0.88, 7, '#4e6b34', h=(zt - zb) * 0.16,
             spread=0.35, w=0.06, lean=10.0)


def scene_titulo():
    """8. TITULO — Elias en alto, el manto al viento, contra un cielo de fuego."""
    new_scene()
    m = tiles.mats()
    # Objetivo alto y camara lejos: el profeta baja en el encuadre y deja libre la franja
    # de arriba, que es donde entra el titulo.
    setup_cine(target=(0.0, 4.0, 2.60), dist=12.5, elev=1.0, lens=58.0)
    flat_layer(FAR)
    # El suelo acaba justo detras del promontorio: si sigue hasta el horizonte, el profeta
    # queda recortado contra tierra y el plano de titulo pierde la silueta.
    ground(MID, '#5a4430', far=9.0, near=-40.0, half_w=60.0)
    # sz 0,20 aplastaba las cordilleras hasta dejarlas en una salchicha horizontal con un
    # pellizco en medio. Mas altas, mas cimas y las dos hileras a alturas distintas.
    ridge(FAR, 'a', [-820, -560, -320, -70, 180, 430, 700, 980], 1100.0, 190.0, '#4a2f28',
          sz=0.26, jitter=0.45)
    ridge(FAR, 'b', [-540, -330, -120, 90, 310, 540], 620.0, 110.0, '#5c3a2c', sz=0.17,
          jitter=0.35)
    # promontorio: el profeta tiene que quedar recortado contra el cielo, no metido en un
    # hueco. Roca baja y ancha, y el la ocupa entera; los bultos le rompen la silueta para
    # que deje de leerse como una rampa de carton.
    build(MID, box, 'cine_crag', (0.0, 4.0, 0.55), (5.2, 4.6, 1.1), M('crag', '#4e3c2a'),
          round=0.55, taper=(0.74, 0.8))
    build(MID, box, 'cine_crag2', (-0.4, 2.2, 0.24), (7.6, 3.4, 0.48), M('crag2', '#453424'),
          round=0.6, taper=(0.82, 0.86))
    for i, (x, y, h, r) in enumerate(((-2.9, 3.2, 1.5, 0.62), (3.0, 3.6, 1.2, 0.52),
                                      (-4.6, 5.4, 0.9, 0.70), (4.4, 5.0, 1.1, 0.66))):
        spire(MID, f'k{i}', x, y, h, r, '#463320', lean=6.0 * (1 if i % 2 else -1))
    # POSE_RAISE llevaba escrita en cine.py desde el principio y no la usaba nadie: el plano
    # de titulo iba con la misma pose de "de pie" que las escenas de palacio.
    actor('elias', at=(0.0, 4.0, 1.10), yaw=8.0, pose=POSE_RAISE)
    y0 = near_y(bite=0.45)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0, lift(zb, zt, 0.58 + 0.16 * i), (x1 - x0) * 0.09, '#33241c',
              lean=7.0 * (1 if i else -1))


PANELS = {
    1: ('tierra', scene_tierra),
    2: ('acab', scene_acab),
    3: ('jezabel', scene_jezabel),
    4: ('caceria', scene_caceria),
    5: ('galaad', scene_galaad),
    6: ('trono', scene_trono),
    7: ('querit', scene_querit),
    8: ('titulo', scene_titulo),
}


# =============================================================================
# Render
# =============================================================================
def render_panel(idx, out_dir, log=print):
    name, builder = PANELS[idx]
    builder()
    sc = bpy.context.scene
    written = []
    for li, lname in enumerate(LAYERS):
        col = bpy.data.collections.get(lname)
        if col is None or not col.objects:
            log(f'  {lname}: vacia, saltada')
            continue
        for lj, other in enumerate(LAYERS):
            c = bpy.data.collections.get(other)
            if c is not None:
                c.hide_render = (lj != li)
        _shading(li in _FLAT)
        hi = render._render_to_array()
        # la capa lejana lleva contorno mas fino: a esa escala un filete grueso la acerca
        fr = render.post_soft_rect(hi, PANEL_W, PANEL_H, SS, outline=True,
                                   outline_px=OUTLINE_PX - (1 if li == FAR else 0))
        path = os.path.join(out_dir, f'{idx:02d}_{name}_L{li}.png')
        render.save_png(fr, path, f'mantle_cine_{idx}_{li}')
        written.append(path)
        # Cobertura: la comprobacion que faltaba. Una capa colocada fuera del cuadro sale
        # en blanco y el PNG se escribe igual (0% opaco), y un MID que llega hasta el
        # horizonte tapa el cielo del motor Y la capa lejana entera.
        cov = float((fr[..., 3] > 0.5).mean()) * 100.0
        warn = ''
        if cov < 3.0:
            warn = '  <-- PERDIDA: casi nada cae dentro del cuadro (usa frame_at)'
        elif li == MID and cov > 90.0:
            warn = '  <-- TAPA el cielo y la capa lejana (recorta ground(far=))'
        log(f'  {lname} -> {os.path.basename(path)}  ({cov:.1f}% opaco){warn}')
    for other in LAYERS:                      # dejar la escena visible para inspeccion
        c = bpy.data.collections.get(other)
        if c is not None:
            c.hide_render = False
    _shading(False)
    return written


def flatten(idx, out_path=None, bg='#2a2436'):
    """Composite the three layers over a flat colour: a quick look at the finished panel."""
    name, _ = PANELS[idx]
    acc = np.zeros((PANEL_H, PANEL_W, 3), dtype=np.float32)
    acc[:] = np.array(vivid(hexc(bg), 1.0), dtype=np.float32)
    for li in range(len(LAYERS)):
        p = os.path.join(_OUT, f'{idx:02d}_{name}_L{li}.png')
        if not os.path.exists(p):
            continue
        img = bpy.data.images.load(p, check_existing=False)
        w, h = img.size
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        bpy.data.images.remove(img)
        lay = px.reshape(h, w, 4)[::-1]
        a = lay[..., 3:4]
        acc = lay[..., :3] * a + acc * (1.0 - a)
    out = np.concatenate([acc, np.ones((PANEL_H, PANEL_W, 1), dtype=np.float32)], axis=2)
    out_path = out_path or os.path.join(_TMP, f'panel_{idx:02d}.png')
    render.save_png(out, out_path, 'mantle_cine_flat')
    return out_path


_OUT = None


def main(sprites_dir, tmp_dir, only=None, log=print):
    global _TMP, _OUT
    _TMP = tmp_dir
    _OUT = os.path.join(sprites_dir, 'cine')
    os.makedirs(_OUT, exist_ok=True)
    render._TMP = tmp_dir
    out = {}
    for idx in (only or sorted(PANELS)):
        log(f'panel {idx} ({PANELS[idx][0]})')
        out[idx] = render_panel(idx, _OUT, log=log)
    return out
