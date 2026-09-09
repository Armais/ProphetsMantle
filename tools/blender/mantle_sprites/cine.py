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

from . import anims, characters, look, parts, render, rig, tiles
from .parts import box, get_mat, hexc, sphere, tube, vivid

PANEL_W, PANEL_H = 1600, 720
SS = 2                      # supersampling (render at PANEL*SS)
OUTLINE_PX = 3              # 2 px is calibrated for 128 px cells; a full panel needs a hair more

LAYERS = ('L0', 'L1', 'L2')
FAR, MID, NEAR = 0, 1, 2

_TMP = None
# Registro de lo que se monta (suelo, actores, props, cauces) para que look.dress reparta
# piedras y matas sin pisar nada. Lo rellenan ground/actor/place/transform/road_strip.
_REG = {'actors': [], 'strips': []}


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
    if layer == MID:
        _REG['actors'].append((at[0], at[1], 0.9 * scale))
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
    _REG.clear()
    _REG.update({'actors': [], 'strips': []})
    for name in LAYERS:                      # clear_scene drops objects, not collections
        _collection(name)


# =============================================================================
# Terrain / set pieces
# =============================================================================
def ground(layer, col, far=55.0, near=-300.0, half_w=400.0, z=0.0):
    """Flat plane that ENDS at `far`. In perspective it recedes towards the horizon, and
    everything the far layer holds has to live beyond that edge or the mid ground — drawn
    on top of it — would cover it."""
    if layer == MID:
        _REG['ground'] = (half_w, near, far, z, col)
    return build(layer, box, 'cine_ground', (0.0, (far + near) * 0.5, z - 0.5),
                 (half_w * 2.0, far - near, 1.0), M('ground', col))


def hill(layer, tag, x, y, r, col, sy=1.4, sz=0.55, z=0.0):
    return build(layer, sphere, f'cine_hill_{tag}', (x, y, z), r, M(f'hill_{col}', col),
                 scale=(sy, 1.0, sz))


def _blur(Z, sigma):
    """Desenfoque gaussiano separable (numpy puro), bordes replicados."""
    if sigma < 0.3:
        return Z
    n = int(sigma * 3.0) + 1
    k = np.exp(-0.5 * (np.arange(-n, n + 1) / sigma) ** 2)
    k /= k.sum()
    P = np.pad(Z, n, mode='edge')
    P = np.apply_along_axis(lambda row: np.convolve(row, k, mode='valid'), 1, P)
    P = np.apply_along_axis(lambda col: np.convolve(col, k, mode='valid'), 0, P)
    return P


def mound_field(layer, tag, mounds, col, cells=720, skirt=0.12):
    """Varias lomas como UNA superficie continua (mapa de alturas), no esferas solapadas.

    Con esferas, el cavity de Workbench subraya cada interseccion y una cordillera de
    cerca sale como un collar de perlas (05: el filo de la cumbre). Aqui z(x,y) es el
    maximo de las cupulas elipsoidales, desenfocado un poco para que el valle entre dos
    lomas sea un valle y no una arista.
    Fuera de su elipse cada cupula SIGUE bajando (una falda con la misma tangente
    vertical del borde): cortarla en seco a ras de suelo dejaba un escalon de un metro
    en dientes de sierra (04, el risco), y aplanarla en cero levantaba un delantal. Se
    conserva hasta `skirt` x altura por debajo de la base: lo justo para que el suelo
    la tape y, en la capa lejana, para que la cordillera se apoye en el horizonte.
    mounds: [(x, y, r, sx, sy, sz, zc)] — el mismo elipsoide que dibujaria `hill`.
    """
    x0 = min(x - r * sx for x, y, r, sx, sy, sz, zc in mounds)
    x1 = max(x + r * sx for x, y, r, sx, sy, sz, zc in mounds)
    y0 = min(y - r * sy for x, y, r, sx, sy, sz, zc in mounds)
    y1 = max(y + r * sy for x, y, r, sx, sy, sz, zc in mounds)
    r_min = min(r * min(sx, sy) for x, y, r, sx, sy, sz, zc in mounds)
    step = max(r_min * 0.05, (x1 - x0) / cells, (y1 - y0) / cells)
    xs = np.arange(x0 - step, x1 + 2 * step, step)
    ys = np.arange(y0 - step, y1 + 2 * step, step)
    X, Y = np.meshgrid(xs, ys)
    zc_min = min(zc for x, y, r, sx, sy, sz, zc in mounds)
    h_min = min(r * sz for x, y, r, sx, sy, sz, zc in mounds)
    Z = None
    for x, y, r, sx, sy, sz, zc in mounds:
        d2 = ((X - x) / (r * sx)) ** 2 + ((Y - y) / (r * sy)) ** 2
        zi = np.where(d2 < 1.0, zc + r * sz * np.sqrt(np.clip(1.0 - d2, 0.0, None)),
                      zc - r * sz * 0.6 * np.sqrt(np.clip(d2 - 1.0, 0.0, None)))
        Z = zi if Z is None else np.maximum(Z, zi)
    Z = _blur(Z, (r_min * 0.12) / step)
    return build(layer, parts.heightfield, f'cine_field_{tag}', xs, ys, Z, M(f'hill_{col}', col),
                 floor=zc_min - h_min * skirt)


def boulder(layer, tag, x, y, r, col, seed=0, sy=1.25, sz=0.62, rough=0.14):
    """Un penasco semienterrado (sustituye a `hill` cuando lo que se quiere es una ROCA:
    una esfera lisa a media distancia es un globo)."""
    return build(layer, parts.rock, f'cine_boulder_{tag}', (x, y, 0.0), r, M(f'rock_{col}', col),
                 scale=(sy, 1.0, sz), seed=seed, rough=rough)


def ridge(layer, tag, xs, y, r, col, sz=0.55, jitter=0.0, z=0.0, stagger=1.5, skirt=None):
    """A row of overlapping mounds: a mountain range in one call.

    En MID/NEAR es UNA superficie (`mound_field`): el cavity dibujaba cada solape de las
    esferas. En FAR siguen siendo esferas SALVO que se pida `skirt`, y no por pereza: en
    varias estampas la camara mira tan abajo que el horizonte queda fuera del cuadro
    (05: elev 12 con lente 55 deja el borde superior 2,7 grados BAJO la horizontal) y lo
    que se ve de la cordillera lejana es la MITAD INFERIOR de las esferas — una banda
    con el bajo ondulado que se lee como sierra en la calima. Un mapa de alturas cortado
    en la base ahi desaparece entero (L0 al 0%). Con la camara casi horizontal (08) pasa
    lo contrario: las mitades de abajo son lentejas flotando, y entonces `skirt` (fraccion
    de la altura conservada bajo la base) pide el mapa de alturas apoyado en el horizonte.
    stagger: las lomas se reparten en tres filas separadas esa distancia en Y. Para un
    filo de cumbre de cerca conviene bajarlo: con 1.5 m y radios de 1 m quedan VENTANAS
    de cielo entre una fila y la siguiente.
    """
    mounds = []
    for i, x in enumerate(xs):
        rr = r * (1.0 + (jitter * ((i * 37 % 11) / 11.0 - 0.5)))
        mounds.append((x, y + (i % 3) * stagger, rr, 1.4, 1.0,
                       sz * (0.85 + 0.3 * ((i * 53 % 7) / 7.0)), z))
    if layer == FAR and skirt is None:
        out = []
        for i, (x, yy, rr, sx, sy, szi, zc) in enumerate(mounds):
            out += hill(layer, f'{tag}{i}', x, yy, rr, col, sz=szi, z=zc)
        return out
    return mound_field(layer, tag, mounds, col, skirt=0.12 if skirt is None else skirt)


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


def _catmull(pts, sub=5):
    """Curva de Catmull-Rom por los puntos de control (PASA por ellos, asi que lo que se
    coloque en un punto de control sigue cayendo sobre el trazado)."""
    if len(pts) < 3:
        return list(pts)
    P = [pts[0]] + list(pts) + [pts[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(sub):
            t = k / float(sub)
            out.append(tuple(0.5 * (2.0 * p1[a] + (-p0[a] + p2[a]) * t
                                    + (2.0 * p0[a] - 5.0 * p1[a] + 4.0 * p2[a] - p3[a]) * t * t
                                    + (-p0[a] + 3.0 * p1[a] - 3.0 * p2[a] + p3[a]) * t * t * t)
                             for a in range(2)))
    out.append(tuple(pts[-1]))
    return out


def road_strip(layer, tag, pts, w0, w1, col, z=0.05, smooth=True):
    """Continuous ribbon through (x, y) points, tapering from w0 to w1. One quad per span
    sharing its edge with the next: separate boxes read as a staircase, not as a road.
    smooth: el trazado se remuestrea con una curva (un rio de cuatro tramos rectos tiene
    cuatro codos de poligono, y a escala de panel se ven)."""
    mat = M(f'road_{tag}', col)
    if smooth:
        pts = _catmull(pts)
    n = len(pts)
    if layer == MID:
        _REG['strips'].append(([tuple(p) for p in pts], max(w0, w1) * 0.5))
    left, right = [], []
    for i, (x, y) in enumerate(pts):
        # El ancho va PERPENDICULAR al trazado (desplazarlo siempre en X solo funciona si
        # el camino corre en Y), y la perpendicular de cada punto es la MEDIA de sus dos
        # tramos: asi los dos lados son polilineas continuas y la tira, una sola malla.
        (xa, ya), (xb, yb) = pts[max(i - 1, 0)], pts[min(i + 1, n - 1)]
        dx, dy = xb - xa, yb - ya
        L = math.hypot(dx, dy) or 1.0
        px, py = -dy / L, dx / L
        h = (w0 + (w1 - w0) * i / (n - 1.0)) * 0.5
        left.append((x - px * h, y - py * h, z))
        right.append((x + px * h, y + py * h, z))
    build(layer, parts.ribbon, f'cine_road_{tag}', left, right, mat)


def tuft(layer, tag, x, y, z, n, col, h=0.5, spread=0.6, w=0.07, lean=0.0, curl=0.35):
    """Foreground grass/reed silhouette.

    curl: cada hoja se ARQUEA (desplazamiento parabolico hacia la punta) en vez de salir
    recta: una cuna recta afilada es un diente; una hoja que se vence es hierba."""
    out = []
    mat = M(f'tuft_{col}', col)
    for i in range(n):
        a = (i * 2.399)
        dx, dy = math.cos(a) * spread * ((i % 4) + 1) / 4.0, math.sin(a) * spread * 0.4
        hh = h * (0.6 + 0.7 * ((i * 29 % 9) / 9.0))
        ob = build(layer, box, f'cine_tuft_{tag}{i}', (x + dx, y + dy, z + hh * 0.5),
                   (w, w * 0.6, hh), mat, round=0.7, taper=(0.12, 0.3))
        if curl:
            c = curl * hh * (0.5 + ((i * 13 % 7) / 7.0)) * (1 if i % 2 else -1)
            for o in ob:
                for v in o.data.vertices:
                    t = max(0.0, (v.co.z - z) / hh)
                    v.co.x += c * t * t
        if lean:
            for o in ob:
                parts.rotate_about(o, (x + dx, y + dy, z), 'Y',
                                   lean * (0.4 + ((i * 17 % 5) / 5.0)) * (1 if i % 2 else -1))
        out += ob
    return out


def spire(layer, tag, x, y, h, r, col, lean=0.0, round=0.92, seed=None):
    """Upright rock in the immediate foreground: breaks the bottom edge of the frame.

    Era una caja biselada y afilada, y con tres tramos de bisel a tamano de panel salia un
    cristal de cuarzo: bandas planas. Ahora es `parts.rock` — un elipsoide con lomos —
    estrechado hacia arriba; la semilla sale del nombre para que dos riscos no sean gemelos.
    """
    if seed is None:
        seed = sum(ord(c) for c in str(tag)) % 23
    ob = build(layer, parts.rock, f'cine_spire_{tag}', (x, y, h * 0.5), 1.0,
               M(f'spire_{col}', col), scale=(r, r * 0.8, h * 0.5), seed=seed, rough=0.2,
               taper=0.35)
    if lean:
        for o in ob:
            parts.rotate_about(o, (x, y, 0.0), 'Y', lean)
    return ob


def rock_cluster(m, seed=0):
    """El monton de piedras de tiles.build_rock, pero con `parts.rock`: tres esferas lisas
    solapadas son tres globos con el solape subrayado por el cavity."""
    parts.rock('r0', (-0.08, 0.02, 0.20), 0.36, m['stone'], scale=(1.15, 0.9, 0.7), seed=seed, rough=0.14)
    parts.rock('r1', (0.22, -0.1, 0.14), 0.22, m['stone_lt'], scale=(1.0, 0.9, 0.75), seed=seed + 3, rough=0.16)
    parts.rock('r2', (-0.3, -0.18, 0.09), 0.14, m['stone'], scale=(1, 1, 0.7), seed=seed + 7, rough=0.16)


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
    _REG['actors'].append((at[0], at[1], 1.6 * scale))
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
    if char['kind'] == 'humanoid' and pose:
        _drape_legs(name, arm, pose)
    base = anims.humanoid_base(char.get('attack', 'swing')) if char['kind'] == 'humanoid' else {}
    rig.pose_apply(arm, rig.merge_pose(base, pose) if pose else base)
    arm.location = at
    arm.rotation_euler = (0.0, 0.0, math.radians(yaw))
    arm.scale = (scale, scale, scale)
    for ob in bpy.data.objects:
        if ob not in before:
            _put(ob, layer)
    if layer == MID:
        _REG['actors'].append((at[0], at[1], 1.0 * scale))
    return char


def _leg_bend(pose, side):
    return sum(deg for axis, deg in pose.get('thigh.' + side, {}).get('rot', []) if axis == 'X')


def _drape_legs(name, arm, pose):
    """La tunica SIGUE a las piernas dobladas (arrodillarse, sentarse).

    La falda de characters.py es un cono rigido colgado de `hips`: vale de pie y andando,
    pero al doblar un muslo 80 grados la pierna sale por la pared del cono y el profeta
    parece llevar los pies fuera de la tunica (la queja del usuario, 2026-09-09, en la
    estampa 5 del prologo). Aqui se cambia esa pieza por cuatro que van cada una en su
    hueso: cadera (hips), regazo sobre el muslo adelantado (thigh), rodilla y caida de la
    rodilla al suelo (shin) y la cola sobre el muslo de atras (el otro thigh). Es lo que
    hace una tela de verdad: se apoya en la rodilla y cae desde ella. Solo actua cuando
    algun muslo pasa de 45 grados; una pose de pie deja al personaje exactamente igual.
    Va ANTES de posar y con el armature en el origen: `attach` fija la pieza en reposo.
    """
    bl, br = abs(_leg_bend(pose, 'L')), abs(_leg_bend(pose, 'R'))
    if max(bl, br) < 45.0:
        return
    F, B = ('L', 'R') if bl >= br else ('R', 'L')   # F = el muslo mas doblado lleva el regazo
    spec = characters.SPECS[name][1]
    long = spec.get('tunic_len', 'short') == 'long'
    prefix = name + ('_robe' if long else '_skirt')
    old = next((o for o in bpy.data.objects if o.parent == arm and o.name.startswith(prefix)), None)
    if old is None:
        return
    mat = old.data.materials[0]
    bpy.data.objects.remove(old, do_unlink=True)
    saved = (arm.location.copy(), arm.rotation_euler.copy(), arm.scale.copy())
    arm.location, arm.rotation_euler, arm.scale = (0, 0, 0), (0, 0, 0), (1, 1, 1)
    bpy.context.view_layer.update()

    def squash(o, k, kx=1.0):   # seccion eliptica: ancho en X, fina en Y (= vertical al girar el muslo)
        for v in o.data.vertices:
            v.co.y *= k
            v.co.x *= kx
        return o

    def taper_x(o, z0, z1, k0, k1):   # ancho en X que va de k0 (en z0) a k1 (en z1)
        for v in o.data.vertices:
            t = min(1.0, max(0.0, (z0 - v.co.z) / (z0 - z1)))
            v.co.x *= k0 + (k1 - k0) * t
        return o

    def A(o, bone):
        return parts.attach(o, arm, bone)

    folds = dict(flutes=9, flute_amp=0.06) if parts.DETAIL else {}
    n = f'{name}_drape'
    if long:
        rw, rh = (0.165, 0.335) if spec.get('slim') else (0.19, 0.30)
        r = lambda z: rw + (rh - rw) * (1.0 - z) / 0.90 + 0.012   # el radio del cono original
        A(tube(f'{n}_hip', (0, 0, 1.00), (0, 0, 0.76), rw, r(0.76), mat, segs=32), 'hips')
        # El regazo va ANCHO y PLANO (x1.15, y0.55): con la seccion de la cadera salia un
        # tubo, y un tubo horizontal a la altura de la cintura es una pierna gorda, no tela.
        # ...y se ESTRECHA hacia la rodilla: de ancho constante, visto de lado o de espaldas
        # era un cojin rectangular sobre el que el profeta parecia sentado.
        A(squash(taper_x(tube(f'{n}_lap', (0, 0, 0.96), (0, 0, 0.50), r(0.76) + 0.02, r(0.50) * 0.9,
                              mat, segs=32), 0.96, 0.50, 1.15, 0.92), 0.58), 'thigh.' + F)
        A(sphere(f'{n}_knee', (0, 0, 0.50), r(0.50) * 0.9, mat, scale=(0.92, 0.66, 0.66)), 'shin.' + F)
        A(squash(tube(f'{n}_fall', (0, 0, 0.50), (0, 0, 0.06), r(0.50), rh + 0.012, mat, segs=32,
                      rim_round=0.03, **folds), 0.66, 1.1), 'shin.' + F)
        A(squash(tube(f'{n}_back', (0, 0, 0.94), (0, 0, 0.44), r(0.76), r(0.30), mat, segs=32,
                      rim_round=0.03, **folds), 0.82), 'thigh.' + B)
    else:
        A(tube(f'{n}_hip', (0, 0, 1.00), (0, 0, 0.78), 0.19, 0.21, mat, segs=32), 'hips')
        A(squash(taper_x(tube(f'{n}_lap', (0, 0, 0.96), (0, 0, 0.52), 0.23, 0.215, mat, segs=32),
                         0.96, 0.52, 1.12, 0.92), 0.62), 'thigh.' + F)
        A(sphere(f'{n}_knee', (0, 0, 0.52), 0.20, mat, scale=(0.92, 0.66, 0.66)), 'shin.' + F)
        A(squash(tube(f'{n}_back', (0, 0, 0.94), (0, 0, 0.60), 0.215, 0.245, mat, segs=32,
                      rim_round=0.03, **folds), 0.8), 'thigh.' + B)
    arm.location, arm.rotation_euler, arm.scale = saved


# Poses used by more than one panel (world axes, see rig.pose_apply)
#
# SIGNO DE LAS PIERNAS (2026-09-09, tras la queja «Elias parece con el cuerpo al reves»):
# una rotacion X positiva es la MISMA rotacion para todos los huesos, y eso significa que
# la punta de un hueso que apunta ARRIBA (spine, chest, head) va hacia delante y la de uno
# que apunta ABAJO (muslos, brazos) va hacia ATRAS. POSE_POINT ya lo sabia para los brazos
# («X negativo lleva el brazo AL FRENTE»); las piernas de las poses de arrodillado se
# escribieron con el signo de la columna, y el muslo «alzado» salia horizontal hacia atras
# a la altura de la cadera, con la espinilla cayendo por detras del cuerpo. Con la falda
# rigida de antes solo se veia una pierna suelta; con el drapeado, la rodilla entera
# detras de la espalda. Regla: muslo adelantado = X NEGATIVO; espinilla que baja desde
# una rodilla alzada = X positivo (deshace el del muslo); espinilla que va por el suelo
# hacia atras = X positivo grande.
POSE_KNEEL = {
    'root': {'loc': (0.0, 0.0, -0.42)},
    'thigh.L': {'rot': [('X', -82.0)]}, 'shin.L': {'rot': [('X', 92.0)]},
    'thigh.R': {'rot': [('X', 20.0)]}, 'shin.R': {'rot': [('X', 72.0)]},
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
    'root': {'loc': (0.0, 0.0, -0.36)},
    'thigh.L': {'rot': [('X', -84.0)]}, 'shin.L': {'rot': [('X', 86.0)]},
    'thigh.R': {'rot': [('X', -84.0)]}, 'shin.R': {'rot': [('X', 86.0)]},
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
    'thigh.L': {'rot': [('X', -86.0)]}, 'shin.L': {'rot': [('X', 96.0)]},
    'thigh.R': {'rot': [('X', 34.0)]}, 'shin.R': {'rot': [('X', 60.0)]},
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
        place(MID, rock_cluster, at=(x, y, 0.0), scale=s, rot_z=i * 41.0, m=m)

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
        boulder(MID, f'rk{i}', x, y, r, '#39323f', seed=i)
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
    ridge(MID, 'lip', [-9.5, -7, -4.8, -2.6, -0.5, 1.6, 3.8, 6.2, 8.6], 6.4, 1.15,
          '#5f5546', sz=0.34, jitter=0.35, stagger=0.45)
    ridge(MID, 'lip2', [-6, -3, 0.5, 4], 5.6, 0.85, '#6d6152', sz=0.30, jitter=0.3, stagger=0.3)
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
        place(MID, rock_cluster, at=(x, y, 0.0), scale=s, rot_z=i * 47.0, m=m)
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
    # (2026-09-09) Y aun asi, con ocho lomas de 530 m de ancho cada 250 m, la hilera
    # seguia siendo una BANDA marron de lado a lado. Menos lomas, mas separadas y mas
    # altas: entre una y otra tiene que verse cielo.
    ridge(FAR, 'a', [-980, -540, -120, 300, 760, 1200], 1100.0, 170.0, '#4a2f28',
          sz=0.50, jitter=0.5, skirt=0.02)
    ridge(FAR, 'b', [-620, -300, 40, 380, 700], 620.0, 90.0, '#5c3a2c', sz=0.34,
          jitter=0.4, skirt=0.02)
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


# =============================================================================
# Piezas de las intros de capitulo
# =============================================================================
def altar_piedras(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0, col='#8d8478',
                  wood='#5a4028'):
    """Altar de piedras sin labrar con los lenos cruzados encima (1 Reyes 18:31-33).

    Dos hiladas de cantos redondeados, no una caja: un altar-bloque leia como un pilar
    de hormigon. Los lenos SIN fuego a proposito: en el Carmelo el drama es justo que
    ningun fuego humano esta encendido.
    """
    mat = M(f'altar_{col}', col)
    w_mat = M(f'altw_{wood}', wood)
    objs = []
    for i, (dx, dy, dz, w) in enumerate(((-0.55, 0.10, 0.28, 0.75),
                                         (0.15, -0.15, 0.30, 0.85),
                                         (0.70, 0.12, 0.26, 0.65),
                                         (-0.25, 0.05, 0.82, 0.70),
                                         (0.45, -0.05, 0.80, 0.75),
                                         (0.10, 0.10, 1.28, 0.90))):
        objs += build(layer, box, f'cine_alt{tag}{i}', (dx, dy, dz), (w, w * 0.8, 0.55),
                      mat, round=0.45, taper=(0.90, 0.92))
    objs += build(layer, box, f'cine_alt{tag}_w1', (0.0, -0.10, 1.62), (1.5, 0.22, 0.16),
                  w_mat, round=0.5)
    objs += build(layer, box, f'cine_alt{tag}_w2', (0.1, 0.15, 1.74), (0.2, 1.3, 0.15),
                  w_mat, round=0.5)
    return transform(objs, at=at, yaw=yaw, scale=s)


def buey(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0, col='#8a5a34'):
    """Buey de labor (1 Reyes 19:19). Cuerpo macizo, testuz baja y cuernos claros: lo
    que lo lee como buey a media distancia es la masa y los cuernos, no el detalle."""
    body = M(f'ox_{col}', col)
    horn = M('oxhorn', '#d8cbb0')
    objs = []
    objs += build(layer, box, f'cine_ox{tag}_b', (0, 0, 0.78), (0.85, 1.7, 0.85), body,
                  round=0.65, taper=(0.94, 0.90))
    objs += build(layer, box, f'cine_ox{tag}_h', (0, -1.05, 0.95), (0.42, 0.55, 0.45),
                  body, round=0.6, taper=(0.85, 0.90))
    objs += build(layer, box, f'cine_ox{tag}_m', (0, -1.32, 0.82), (0.30, 0.25, 0.28),
                  M('oxmuzzle', '#c9a88a'), round=0.7)
    for sg in (-1, 1):
        objs += build(layer, tube, f'cine_ox{tag}_hn{sg}', (sg * 0.20, -1.12, 1.18),
                      (sg * 0.62, -1.30, 1.54), 0.075, 0.02, horn, segs=12)
        for j, dy in enumerate((-0.62, 0.62)):
            objs += build(layer, box, f'cine_ox{tag}_l{sg}{j}', (sg * 0.30, dy, 0.24),
                          (0.16, 0.20, 0.50), body, round=0.4)
    objs += build(layer, box, f'cine_ox{tag}_t', (0, 0.90, 0.90), (0.07, 0.50, 0.07),
                  body, round=0.5, taper=(0.60, 0.80))
    return transform(objs, at=at, yaw=yaw, scale=s)


def cama(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, wood='#5a4028', cloth='#b8a988'):
    """El aposento sunamita (2 Reyes 4:10): cama de marco de madera con jergon claro.
    La almohada va al extremo -Y (la cabecera mira a la camara sin girarla)."""
    w = M(f'bedw_{wood}', wood)
    objs = []
    objs += build(layer, box, f'cine_bed{tag}_f', (0, 0, 0.42), (1.05, 2.1, 0.16), w,
                  round=0.3)
    for sx in (-1, 1):
        for sy in (-1, 1):
            objs += build(layer, box, f'cine_bed{tag}_l{sx}{sy}',
                          (sx * 0.45, sy * 0.95, 0.17), (0.12, 0.12, 0.34), w, round=0.3)
    objs += build(layer, box, f'cine_bed{tag}_m', (0, 0, 0.56), (0.95, 2.0, 0.14),
                  M(f'bedc_{cloth}', cloth), round=0.5)
    objs += build(layer, box, f'cine_bed{tag}_p', (0, -0.75, 0.66), (0.60, 0.40, 0.12),
                  M('bedpillow', '#d8cbb0'), round=0.6)
    return transform(objs, at=at, yaw=yaw)


def tienda(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0, col='#7a6a52'):
    """Tienda de campana siria: un prisma (taper llevado casi a cero en X deja la caja
    en cuna de dos aguas), no una piramide."""
    objs = build(layer, box, f'cine_tent{tag}', (0, 0, 0.78), (2.2, 1.8, 1.56),
                 M(f'tent_{col}', col), round=0.12, taper=(0.06, 1.0))
    objs += build(layer, box, f'cine_tentb{tag}', (0, 0, 0.10), (2.3, 1.9, 0.20),
                  M('tentbase', '#5a4a38'), round=0.2)
    return transform(objs, at=at, yaw=yaw, scale=s)


# =============================================================================
# Intros de capitulo (paneles 11..20 = capitulo + 10)
# =============================================================================
def scene_cap01_querit():
    """11. INTRO CAP. 1 — el canon de Querit ABIERTO: las paredes a los lados, el arroyo
    bajando del fondo, Elias bebiendo y TRES cuervos con el pan.

    Mismo lugar que la estampa 7 del prologo, pero otro encuadre a proposito: el jugador
    la ve justo despues de aquella, asi que aqui manda el CANON (el bioma del capitulo),
    con las paredes en la capa lejana y bien atras — la leccion de scene_querit: un tajo
    cerca de esta camara no sale imponente, sale tapandolo todo.
    """
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 4.0, 1.30), dist=14.0, elev=6.5, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#9a8256', far=26.0, near=-40.0, half_w=70.0)

    # --- far (FLAT, siluetas): las laderas de la hondonada a los lados — el enfoque de
    # scene_querit; construir las paredes con cliff_face salia o tapia lisa (FLAT) o
    # bloque de chocolate flotante (MID) —, las dunas del fondo del vano y una
    # cordillera con la cresta ROTA: picos separados; solapados eran una salchicha.
    ridge(FAR, 'l', [-27, -21, -15, -10.5], 30.0, 9.0, '#7d6a4e', sz=0.72, jitter=0.3)
    ridge(FAR, 'r', [10.5, 15.5, 21, 27.5], 31.0, 9.5, '#6b5940', sz=0.75, jitter=0.3)
    ridge(FAR, 'm', [-8, -3, 2, 7], 34.5, 6.0, '#8a7454', sz=0.45, jitter=0.35)
    ridge(FAR, 'c', [-520, -260, 0, 250, 510], 800.0, 95.0, '#4e4560',
          sz=0.36, jitter=0.5)
    ridge(FAR, 'c2', [-380, -120, 130, 390], 860.0, 70.0, '#5a5170', sz=0.30, jitter=0.4)
    # dos agujas de roca a media distancia: la pared del canon que SI lleva cavity
    spire(MID, 'cl', -11.0, 15.0, 4.6, 1.5, '#7a643f', lean=-4.0)
    spire(MID, 'cr', 10.5, 17.0, 5.6, 1.7, '#6f5a38', lean=5.0)

    # --- mid: el arroyo baja EN DIAGONAL desde el vano del canon (la perspectiva lo
    # estrecha sola), con su ribete de guijarros para que el agua no sea una calcomania
    brook = [(2.2, 24.0), (0.8, 19.0), (-0.6, 14.5), (0.2, 10.5), (1.4, 7.0),
             (0.6, 3.6), (-1.2, 0.4), (-3.0, -3.0)]
    road_strip(MID, 'bank', brook, 2.6, 4.2, '#b6a074', z=0.03)
    road_strip(MID, 'w', brook, 1.5, 2.5, '#3f7f96', z=0.06)
    # En la ORILLA izquierda, vuelto hacia el agua: puesto en x=-0.6 caia DENTRO del cauce
    # y el profeta bebia arrodillado sobre el agua.
    actor('elias', at=(-2.4, 5.4, 0.0), yaw=78.0, pose=POSE_DRINK)
    # Tres cuervos: uno posado en la roca junto al profeta, dos llegando con el pan
    crow(MID, 'a', (3.1, 6.4, 1.35), yaw=-44.0, pitch=-6.0, s=1.15, bread=True)
    crow(MID, 'b', (-3.6, 9.8, 2.45), yaw=30.0, pitch=-16.0, s=1.05, bread=True)
    crow(MID, 'c', (4.8, 12.5, 3.10), yaw=-18.0, pitch=-12.0, s=0.95)
    for i, (x, y, s) in enumerate(((4.4, 5.8, 2.6), (-4.8, 7.2, 2.1), (6.4, 10.6, 2.9),
                                   (-6.8, 12.4, 2.4))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=s, rot_z=i * 47.0, m=m)
    place(MID, tiles.build_tree_dead, at=(-7.6, 16.5, 0.0), scale=2.3, m=m)
    place(MID, tiles.build_palm, at=(7.2, 18.0, 0.0), scale=1.6, m=m)

    # --- near: el labio de la hondonada y los juncos del arroyo
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'r{i}', x, y0, lift(zb, zt, 0.58 + 0.16 * i), (x1 - x0) * 0.09,
              '#6b5a3c', lean=6.0 * (1 if i else -1))
    for i, x in enumerate((x0 * 0.55, x1 * 0.55)):
        tuft(NEAR, f'g{i}', x, y0 + 0.7, zb * 0.88, 7, '#4e6b34', h=(zt - zb) * 0.16,
             spread=0.35, w=0.06, lean=10.0)


def scene_cap02_sarepta():
    """12. INTRO CAP. 2 — la puerta de Sarepta al atardecer: la viuda recoge sus dos
    leños junto a la puerta (1 Reyes 17:10) y Elias llega andando. El mar y la silueta
    de la ciudad fenicia al fondo."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 3.0, 1.50), dist=13.0, elev=5.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#8f7c60', far=40.0, near=-40.0, half_w=70.0)

    # --- far (FLAT): el mar del crepusculo como banda plana — asoma por los EXTREMOS,
    # mas alla de donde acaba el muro — y la ciudad en silueta sobre la muralla
    build(FAR, box, 'cine_sea', (0.0, 220.0, -0.3), (900.0, 360.0, 0.5),
          M('sea', '#3d4a66'))
    for i, (x, y, w, h) in enumerate(((-9.0, 42.0, 6.0, 5.5), (-3.0, 46.0, 5.0, 7.5),
                                      (3.5, 41.0, 5.5, 4.8), (9.5, 45.0, 5.0, 6.5),
                                      (-14.5, 44.0, 5.0, 4.2), (15.5, 43.0, 5.5, 5.0))):
        build(FAR, box, f'cine_sarepta{i}', (x, y, h * 0.5), (w, 6.0, h),
              M('town_sil', '#5c4560'), round=0.10, taper=(0.94, 0.94))
    ridge(FAR, 'sc', [-420, -180, 60, 300, 540], 900.0, 90.0, '#503f5e', sz=0.30,
          jitter=0.45)

    # --- mid: el muro bajo del arrabal, CORTO — el mar se ve por sus dos cabos — con
    # su puerta (el vano es un tunel, como siempre), la viuda y Elias llegando
    stone_wall(MID, 'sl', -13.0, -2.2, 24.0, 3.0, col='#8d7c66', col2='#7c6c58')
    stone_wall(MID, 'sr', 2.2, 13.0, 24.0, 3.0, col='#8d7c66', col2='#7c6c58')
    stone_wall(MID, 'st', -2.2, 2.2, 24.0, 3.0, col='#8d7c66', col2='#7c6c58', z0=2.1)
    for sg in (-1, 1):
        build(MID, box, f'cine_sgate_jamb{sg}', (sg * 1.85, 25.2, 1.05), (0.5, 2.6, 2.10),
              M('sgate_jamb', '#5c4f42'), round=0.05)
    build(MID, box, 'cine_sgate_soffit', (0.0, 25.2, 2.12), (4.2, 2.6, 0.40),
          M('sgate_soffit', '#493e34'), round=0.05)
    build(MID, box, 'cine_sgate_back', (0.0, 26.5, 1.00), (3.8, 0.5, 2.00),
          M('sgate_back', '#241a20', sat=1.0, val=0.7))
    build(MID, box, 'cine_sgate_lintel', (0.0, 23.8, 2.25), (5.2, 0.9, 0.55),
          M('slintel', '#9b8a74'), round=0.14)
    # La viuda INCLINADA sobre los leños (la pose de beber es exactamente agacharse a
    # recoger del suelo), los dos leños delante y Elias entrando por la izquierda
    actor('viuda_sarepta', at=(2.6, 11.0, 0.0), yaw=-24.0, pose=POSE_DRINK)
    for i, (dx, dy) in enumerate(((0.0, -1.1), (0.55, -1.45))):
        build(MID, box, f'cine_stick{i}', (2.6 + dx, 11.0 + dy, 0.07),
              (0.14, 1.15, 0.12), M('stick', '#6a4a2c'), round=0.5, taper=(0.7, 0.8))
    actor('elias', at=(-3.8, 6.8, 0.0), yaw=38.0, pose=POSE_WALK)
    # Hacia el centro: pegados al borde los eclipsaban las columnas del encuadre
    place(MID, tiles.build_palm, at=(-6.0, 13.5, 0.0), scale=1.8, m=m)
    place(MID, rock_cluster, at=(4.8, 7.2, 0.0), scale=1.7, rot_z=31.0, m=m)

    # --- near: las jambas del arrabal por las que miramos
    y0 = near_y(bite=0.80)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0 * 1.03, x1 * 1.03)):
        column(NEAR, f'sf{i}', x, y0, h=8.0, r=0.42, col='#7a6a56')


def scene_cap03_carmelo():
    """13. INTRO CAP. 3 — la cima del Carmelo: DOS altares y ningun fuego encendido por
    mano humana. Elias erguido frente a los profetas de Baal; el pueblo mira de
    espaldas desde el primer plano, y el mar de fondo (el Carmelo es un promontorio)."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 2.6, 1.30), dist=13.0, elev=7.0, lens=44.0)
    flat_layer(FAR)
    # El suelo acaba a 13 m: la cima. Detras, el mar — es lo que la lee como cumbre.
    ground(MID, '#7a6b4e', far=13.0, near=-40.0, half_w=70.0)
    # El mar LLEGA al horizonte y no hay nada detras: con una cordillera al fondo la
    # franja de agua leia como un lago entre montes, y el Carmelo da al Mediterraneo.
    build(FAR, box, 'cine_csea', (0.0, 700.0, -0.5), (2000.0, 1300.0, 0.5),
          M('csea', '#46607a', sat=1.0, val=0.92))
    ridge(FAR, 'cl', [-30, -22, -14], 26.0, 6.5, '#6b5f48', sz=0.55, jitter=0.3)
    ridge(FAR, 'cr', [15, 23, 31], 27.0, 7.0, '#5f5440', sz=0.58, jitter=0.3)

    # --- mid: los dos altares — el de Jehova reparado (12 piedras) y el de Baal con
    # los profetas alrededor — y Elias solo frente a todos
    altar_piedras(MID, 'god', at=(-3.6, 7.0, 0.0), yaw=12.0, s=1.15)
    altar_piedras(MID, 'baal', at=(4.0, 7.6, 0.0), yaw=-8.0, s=1.05,
                  col='#6e6260', wood='#4a3424')
    actor('elias', at=(-1.2, 3.9, 0.0), yaw=24.0, pose=POSE_RAISE)
    actor('profeta_baal', at=(3.0, 5.4, 0.0), yaw=-150.0, pose=POSE_RAISE)
    actor('profeta_baal', at=(5.6, 5.8, 0.0), yaw=160.0, pose=POSE_WALK)
    actor('profeta_baal', at=(4.6, 9.8, 0.0), yaw=-30.0, pose=POSE_KNEEL)
    ridge(MID, 'lip', [-9, -6.2, -3.5, -0.8, 2, 4.6, 7.4, 10], 11.8, 1.05,
          '#6b5e46', sz=0.32, jitter=0.35)
    for i, (x, y, s) in enumerate(((-6.8, 9.0, 1.9), (7.8, 11.0, 2.2))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=s, rot_z=i * 53.0, m=m)

    # --- near: el pueblo, de espaldas, cerrando el plano (como el gentio de Jezabel)
    y0 = near_y(z=0.9, bite=1.0)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.04)):
        hooded(NEAR, f'pu{i}', x, y0 - 0.4 + (i % 3) * 0.4,
               h=1.64 + (i % 4) * 0.06, col='#4a4034', yaw=180.0)


def scene_cap04_horeb():
    """14. INTRO CAP. 4 — la noche del desierto: la boca de la cueva de Horeb, Elias
    arrodillado ante ella y el enebro del camino. El silbo apacible lo pone el motor
    (CineFx_Shaft en la escena B): aqui solo hay noche, roca y un hombre agotado."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 5.0, 1.60), dist=14.0, elev=7.5, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#3f3a4a', far=24.0, near=-40.0, half_w=70.0)
    # --- far (FLAT): dunas y crestas del Sinai en dos azules de noche
    ridge(FAR, 'ha', [-460, -230, 10, 250, 490], 700.0, 120.0, '#262238', sz=0.40,
          jitter=0.5)
    ridge(FAR, 'hb', [-300, -90, 130, 350], 760.0, 85.0, '#332e48', sz=0.32, jitter=0.4)
    ridge(FAR, 'hm', [-16, -9, -2, 5, 12], 28.0, 5.5, '#4a4258', sz=0.50, jitter=0.35)

    # --- mid: la boca de la cueva a la izquierda, el profeta arrodillado ante ella y
    # el enebro (el arbol bajo el que pidio morir) a la derecha
    cave_mouth(MID, 'h', -4.8, 15.0, w=3.8, h=3.8, col='#3a3448', dark='#08060c')
    # El vano del helper lleva round=0.75 y esa cara ABOMBADA es la que coge el
    # degradado de la luz de estudio (por eso salia una pizarra gris con brillo; el
    # fondo PLANO de la puerta de Jezabel si sale oscuro). Se sustituye por un panel
    # plano, mate y sin curvatura.
    for ob in list(bpy.data.objects):
        if ob.name.startswith('cine_cvh_void'):
            bpy.data.objects.remove(ob, do_unlink=True)
    # La boca por dentro, en DOS pasos como la puerta de Jezabel. La leccion de esta
    # estampa: el color casi da igual (el fondo de Jezabel es igual de oscuro) — lo que
    # lee como HUECO es que el rectangulo oscuro sea PEQUENO y este enmarcado; a toda
    # anchura, el mismo gris lee como una pared. Retranqueo de roca + jambas interiores
    # que estrechan + nucleo oscuro pequeno.
    build(MID, box, 'cine_cvh_flat', (-4.8, 13.9, 1.60), (3.5, 1.0, 3.34),
          M('cave_flat', '#332e44', sat=1.0, val=0.8), round=0.05)
    for sg in (-1, 1):
        build(MID, box, f'cine_cvh_jamb{sg}', (-4.8 + sg * 1.35, 13.7, 1.35),
              (0.85, 0.8, 2.70), M('cave_jamb', '#262134', sat=1.0, val=0.75),
              round=0.35, taper=(0.85, 0.9))
    build(MID, box, 'cine_cvh_deep', (-4.8, 13.5, 1.05), (1.55, 0.5, 2.10),
          M('cave_deep', '#0a0810', sat=1.0, val=0.5), round=0.05)
    for mm in bpy.data.materials:
        if mm.name.startswith(('cine_cave_flat', 'cine_cave_deep', 'cine_cave_jamb')):
            mm.roughness = 1.0
            mm.metallic = 0.0
    actor('elias', at=(-3.6, 9.8, 0.0), yaw=156.0, pose=POSE_KNEEL)
    place(MID, tiles.build_tree_dead, at=(5.2, 10.0, 0.0), scale=2.4, m=m)
    for i, (x, y, r) in enumerate(((3.2, 13.5, 1.0), (-8.6, 10.5, 0.85), (7.8, 15.5, 1.2))):
        boulder(MID, f'hrk{i}', x, y, r, '#443e54', seed=i + 4)

    # --- near: rocas oscuras del desierto
    y0 = near_y(bite=0.72)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'hr{i}', x, y0, lift(zb, zt, 0.60 + 0.16 * i), (x1 - x0) * 0.09,
              '#2b2638', lean=7.0 * (1 if i else -1))
    for i, x in enumerate((x0 * 0.60, x1 * 0.60)):
        tuft(NEAR, f'hg{i}', x, y0 + 0.8, zb * 0.90, 6, '#3a3448', h=(zt - zb) * 0.14,
             spread=0.5, w=0.10, lean=11.0)


def scene_cap05_llamado():
    """15. INTRO CAP. 5 — los campos de labor: Eliseo arando tras la yunta de bueyes y
    Elias que pasa por delante de el, con el manto puesto, a echarselo encima. Surcos
    en diagonal, lomas doradas y el humo de la aldea al fondo."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 3.4, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#a08748', far=34.0, near=-40.0, half_w=70.0)
    # --- far (FLAT): lomas de labor y el humo de las chimeneas de Abel-mehola
    ridge(FAR, 'fa', [-500, -240, 20, 280, 540], 800.0, 110.0, '#6e5e36', sz=0.34,
          jitter=0.45)
    ridge(FAR, 'fb', [-320, -80, 160, 400], 860.0, 80.0, '#7d6c40', sz=0.28, jitter=0.4)
    ridge(FAR, 'fm', [-18, -10, -2, 6, 14], 38.0, 6.0, '#8a7440', sz=0.42, jitter=0.35)
    # --- mid: los surcos en diagonal (la perspectiva los estrecha) y la yunta DE
    # PERFIL cruzando el campo — de espaldas se veian las ancas y nada mas. El buey
    # lejano va ADELANTADO media cabeza: en paralelo perfecto el cercano lo tapaba
    # entero y la yunta quedaba en una sola bestia.
    for i in range(5):
        x0s = -7.5 + i * 3.4
        road_strip(MID, f'fur{i}', [(x0s, 2.0), (x0s + 2.2, 12.0), (x0s + 3.6, 24.0)],
                   1.0, 0.5, '#7d6434', z=0.03)
    buey(MID, 'a', at=(1.2, 11.4, 0.0), yaw=90.0)
    buey(MID, 'b', at=(2.4, 13.3, 0.0), yaw=90.0)
    # el yugo cruza los cuellos (con yaw 90 la testuz cae hacia +X) y el timon baja
    # del yugo a la reja clavada en el surco, detras de las bestias
    build(MID, tube, 'cine_yoke', (2.0, 11.0, 1.22), (3.2, 13.4, 1.22), 0.08, 0.08,
          M('yoke', '#6a4a2c'), segs=12)
    beam = build(MID, box, 'cine_plow_beam', (0.7, 12.2, 0.68), (2.5, 0.16, 0.13),
                 M('yoke', '#6a4a2c'), round=0.4)
    for o in beam:
        parts.rotate_about(o, (-0.5, 12.2, 0.18), 'Y', -20.0)
    build(MID, box, 'cine_plow_blade', (-0.5, 12.2, 0.16), (0.34, 0.22, 0.34),
          M('plow', '#8d8478'), round=0.35, taper=(0.5, 0.7))
    actor('eliseo_labrador', at=(-1.7, 12.6, 0.0), yaw=96.0, pose=POSE_WALK)
    actor('elias', at=(-3.6, 5.4, 0.0), yaw=58.0, pose=POSE_WALK)
    place(MID, tiles.build_tree, at=(-9.0, 17.5, 0.0), scale=2.2, m=m)

    # --- near: espigas FINAS (anchas eran una fila de dientes) y dos postes de cerca
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.02)):
        tuft(NEAR, f'fg{i}', x + 1.6, y0 + 0.9, zb * 0.75, 12, '#b89a4e',
             h=(zt - zb) * 0.13, spread=1.9, w=0.08, lean=9.0)
    for i, x in enumerate((x0, x1)):
        build(NEAR, box, f'cine_post{i}', (x, y0, lift(zb, zt, 0.26 + 0.08 * i)),
              (0.36, 0.36, (zt - zb) * 0.62),
              M('post', '#5a4426'), round=0.35, taper=(0.85, 0.9))


def scene_cap06_jerico():
    """16. INTRO CAP. 6 — el manantial malo de Jerico: Eliseo de pie ante la charca
    enferma, un hombre de la ciudad arrodillado, arboles secos y la muralla al fondo.

    La muralla va con cavity (silleria: las juntas la dibujan), asi que la capa lejana NO
    se aplana — y por eso no lleva cordillera detras: una ridge sin FLAT es un collar de
    perlas. La ciudad ES el fondo. El vano de la puerta es un TUNEL (jambas + sofito +
    fondo), la leccion de scene_jezabel: un plano oscuro no se lee como hueco.
    """
    new_scene()
    m = tiles.mats()
    # elev BAJA (5): la muralla es el horizonte y hace falta cielo encima; a 7 grados la
    # ciudad llenaba el panel hasta el borde y el cielo enfermo no llegaba a verse.
    setup_cine(target=(0.0, 3.0, 1.50), dist=13.0, elev=5.0, lens=42.0)
    # El suelo llega hasta el PIE de la muralla: acabado antes, la ciudad flotaba sobre
    # una franja de cielo; acabado despues, la taparia (se dibuja encima de FAR).
    ground(MID, '#7c7a5c', far=43.0, near=-40.0, half_w=70.0)

    # --- far: la muralla de Jerico EN EL HORIZONTE, con su puerta y torres almenadas
    stone_wall(FAR, 'l', -24.0, -3.2, 44.0, 4.2, col='#7f7a68', col2='#6e6a5a')
    stone_wall(FAR, 'r', 3.2, 24.0, 44.0, 4.2, col='#7f7a68', col2='#6e6a5a')
    stone_wall(FAR, 'top', -3.2, 3.2, 44.0, 4.2, col='#7f7a68', col2='#6e6a5a', z0=2.9)
    for sg in (-1, 1):
        build(FAR, box, f'cine_jgate_jamb{sg}', (sg * 2.75, 45.6, 1.45), (0.55, 3.4, 2.90),
              M('jgate_jamb', '#57534a'), round=0.05)
    build(FAR, box, 'cine_jgate_soffit', (0.0, 45.6, 2.92), (6.0, 3.4, 0.45),
          M('jgate_soffit', '#454239'), round=0.05)
    build(FAR, box, 'cine_jgate_back', (0.0, 47.3, 1.40), (5.5, 0.5, 2.80),
          M('jgate_back', '#171a14', sat=1.0, val=0.7))
    build(FAR, box, 'cine_jgate_lintel', (0.0, 43.8, 3.10), (7.2, 1.0, 0.7),
          M('jlintel', '#94907e'), round=0.14)
    for x in (-8.4, 8.4):
        build(FAR, box, f'cine_jtower{x:.0f}', (x, 44.4, 2.6), (3.2, 3.6, 5.2),
              M('jtower', '#787362'), round=0.08, taper=(0.92, 0.92))
        build(FAR, box, f'cine_jmerlon{x:.0f}', (x, 44.4, 5.45), (3.8, 4.2, 0.6),
              M('jmerlon', '#63604f'), round=0.15)

    # --- mid: la charca enferma y su reguero, Eliseo y el hombre de Jerico
    # La charca son dos lentejas aplastadas (orilla + agua): una caja seria una piscina
    build(MID, sphere, 'cine_jpool_bank', (0.9, 6.6, 0.02), 3.3,
          M('jbank', '#8a8468'), scale=(1.6, 1.1, 0.012))
    build(MID, sphere, 'cine_jpool_w', (0.9, 6.6, 0.05), 2.5,
          M('jwater', '#3c5a48'), scale=(1.5, 1.0, 0.016))
    # El reguero se EXTIENDE desde el manantial hacia la ciudad, fino y mas oscuro que
    # la charca: ancho salia una estrella de mar, no una corrupcion que viaja
    road_strip(MID, 'jrun', [(2.2, 9.0), (3.4, 14.0), (3.8, 21.0), (2.8, 30.0)],
               1.0, 0.5, '#2f4a3c', z=0.05)
    road_strip(MID, 'jrun2', [(-0.4, 9.2), (-1.6, 14.0), (-2.0, 20.0)],
               0.8, 0.4, '#37543f', z=0.05)
    actor('eliseo', at=(-2.2, 4.4, 0.0), yaw=24.0, pose=POSE_STAND_PROUD)
    # Al borde NORESTE de la charca, mirando al agua: en (2.8, 5.0) caia sobre el agua
    # y pegado a la esquina delantera lo tapaba la columna del encuadre.
    actor('hombre_jerico', at=(4.4, 9.8, 0.0), yaw=-140.0, pose=POSE_KNEEL)
    # La tierra esteril: dos arboles secos a los lados — hacia DENTRO del cuadro, que
    # pegados al borde los eclipsaban las columnas — y dos vecinos que se van, ya
    # lejos, camino de la puerta (cerca de la camara leian como bolardos)
    place(MID, tiles.build_tree_dead, at=(6.0, 12.0, 0.0), scale=1.7, m=m)
    place(MID, tiles.build_tree_dead, at=(-6.4, 13.5, 0.0), scale=2.0, rot_z=63.0, m=m)
    hooded(MID, 'j0', -3.5, 26.0, h=1.70, col='#4a4438', yaw=170.0)
    hooded(MID, 'j1', -2.3, 29.0, h=1.62, col='#3f3a30', yaw=178.0)
    for i, (x, y, s) in enumerate(((-5.6, 8.5, 1.9), (6.8, 10.0, 1.5))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=s, rot_z=i * 71.0, m=m)

    # --- near: el portico arruinado por el que miramos (dos columnas al filo del cuadro)
    y0 = near_y(bite=0.80)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0 * 1.03, x1 * 1.03)):
        column(NEAR, f'jf{i}', x, y0, h=8.0, r=0.44, col='#8f8878')


def scene_cap07_aceite():
    """17. INTRO CAP. 7 — el patio pobre al alba: la madre inclinada sobre las vasijas
    prestadas, los dos hijos acarreando mas, y Eliseo que manda seguir. La aldea gris
    de fondo con su humo de chimeneas."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 3.0, 1.40), dist=12.0, elev=6.5, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#8a8172', far=26.0, near=-40.0, half_w=70.0)
    # --- far (FLAT): la aldea baja en gris, LEJOS — a 30 m las casas eran losas
    # enormes y el humo dos embudos que llenaban el cielo
    for i, (x, y, w, h) in enumerate(((-16.0, 48.0, 5.5, 2.8), (-8.5, 52.0, 4.5, 3.8),
                                      (-1.5, 49.0, 5.0, 2.6), (6.0, 51.5, 4.2, 3.2),
                                      (13.5, 48.5, 5.0, 2.7), (20.0, 52.0, 4.5, 3.0))):
        build(FAR, box, f'cine_aldea{i}', (x, y, h * 0.5), (w, 5.0, h),
              M('aldea_sil', '#5f5a64'), round=0.08, taper=(0.95, 0.95))
    ridge(FAR, 'am', [-30, -15, 0, 15, 30], 64.0, 8.0, '#6e6858', sz=0.35, jitter=0.4)

    # --- mid: el monton de vasijas — pocas no cuentan la historia: "no pidas pocas" —
    # la madre volcada sobre una, los hijos con las suyas y Eliseo senalando
    for i, (x, y, s) in enumerate(((0.8, 6.2, 1.6), (1.7, 5.7, 1.3), (0.1, 5.4, 1.2),
                                   (2.4, 6.6, 1.5), (1.3, 7.1, 1.35), (-0.6, 6.6, 1.45),
                                   (2.9, 5.3, 1.1), (-1.2, 5.6, 1.05))):
        place(MID, tiles.build_jar, at=(x, y, 0.0), scale=s, rot_z=i * 39.0, m=m)
    # La madre DELANTE del monton, no detras: volcada sobre la vasija mas cercana
    # (detras la tapaba el propio monton y en el panel no salia ninguna madre)
    actor('mujer_profeta', at=(-1.6, 4.9, 0.0), yaw=-34.0, pose=POSE_DRINK)
    hooded(MID, 'kid0', 3.9, 8.6, h=1.05, col='#7a6a52', yaw=-140.0)
    hooded(MID, 'kid1', -2.9, 8.9, h=0.95, col='#6a5c46', yaw=170.0)
    actor('eliseo', at=(-3.2, 7.0, 0.0), yaw=52.0, pose=POSE_POINT)
    # la casa del profeta muerto: cascarones bajos de adobe, atras
    build(MID, box, 'cine_casa_l', (-9.0, 16.5, 1.1), (3.2, 2.4, 2.2),
          M('adobe', '#8a7a64'), round=0.12, taper=(0.94, 0.9))
    build(MID, box, 'cine_casa_r', (8.6, 17.5, 1.2), (3.6, 2.6, 2.4),
          M('adobe', '#8a7a64'), round=0.12, taper=(0.94, 0.9))
    place(MID, tiles.build_tree_dead, at=(5.6, 12.0, 0.0), scale=1.6, m=m)

    # --- near: las jambas del patio y un pano tendido
    y0 = near_y(bite=0.80)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0 * 1.03, x1 * 1.03)):
        column(NEAR, f'af{i}', x, y0, h=8.0, r=0.40, col='#6e6252')
    banner(NEAR, 'pano', x0 * 0.94, y0 + 0.3, z_top=6.4, w=1.6, h=2.2,
           col='#8a7a5e', bar='#5a4a38')


def scene_cap08_sunamita():
    """18. INTRO CAP. 8 — el aposento de la sunamita: el nino tendido en la cama, la
    madre de rodillas a su lado y Eliseo de pie. La ventana es un vano con LUZ — el
    unico interior de las diez intros, y aqui el MID puede taparlo todo (>90%)."""
    new_scene()
    m = tiles.mats()
    setup_cine(target=(0.0, 2.4, 1.45), dist=11.0, elev=8.0, lens=45.0)
    ground(MID, '#6e5f4c', far=7.4, near=-40.0, half_w=40.0)
    # --- far: el muro del fondo con su ventana iluminada (el reverso del vano oscuro:
    # un panel CLARO detras del hueco si funciona a la primera)
    stone_wall(FAR, 'wl', -11.0, -1.1, 7.6, 5.6, col='#7a6e5c', col2='#6a5f4e')
    stone_wall(FAR, 'wr', 1.1, 11.0, 7.6, 5.6, col='#7a6e5c', col2='#6a5f4e')
    stone_wall(FAR, 'wt', -1.1, 1.1, 7.6, 5.6, col='#7a6e5c', col2='#6a5f4e', z0=3.4)
    stone_wall(FAR, 'wb', -1.1, 1.1, 7.6, 1.6, col='#7a6e5c', col2='#6a5f4e')
    build(FAR, box, 'cine_window_light', (0.0, 7.9, 2.5), (2.0, 0.4, 1.8),
          M('windowlight', '#e8d8a8', sat=1.0, val=1.0), round=0.05)

    # --- mid: la cama con el nino, la madre arrodillada, mesa, silla y candelero —
    # los cuatro muebles del versiculo (2 Reyes 4:10)
    cama(MID, 'a', at=(1.6, 4.6, 0.0), yaw=10.0)
    build(MID, box, 'cine_boy_blanket', (1.55, 4.75, 0.72), (0.60, 1.05, 0.20),
          M('boycloth', '#9a4f42'), round=0.7)
    build(MID, sphere, 'cine_boy_head', (1.42, 3.95, 0.76), 0.145,
          M('boyskin', '#e8b48a'))
    actor('sunamita', at=(-0.6, 3.6, 0.0), yaw=-58.0, pose=POSE_KNEEL)
    actor('eliseo', at=(-2.6, 5.4, 0.0), yaw=38.0, pose=POSE_STAND_PROUD)
    build(MID, box, 'cine_mesa', (-3.9, 6.4, 0.55), (1.3, 0.9, 0.10),
          M('mesa', '#5a4028'), round=0.3)
    for sx in (-1, 1):
        for sy in (-1, 1):
            build(MID, box, f'cine_mesa_l{sx}{sy}', (-3.9 + sx * 0.55, 6.4 + sy * 0.35,
                  0.25), (0.10, 0.10, 0.50), M('mesa', '#5a4028'), round=0.3)
    build(MID, box, 'cine_silla_a', (4.4, 5.9, 0.42), (0.85, 0.7, 0.10),
          M('mesa', '#5a4028'), round=0.3)
    build(MID, box, 'cine_silla_b', (4.4, 6.25, 0.95), (0.85, 0.12, 0.95),
          M('mesa', '#5a4028'), round=0.3, taper=(0.85, 1.0))
    place(MID, tiles.build_torch, at=(3.4, 7.0, 0.0), scale=1.15, m=m)

    # --- near: las columnas del vano de la puerta
    y0 = near_y(bite=0.80)
    x0, x1, _, _ = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0 * 1.03, x1 * 1.03)):
        column(NEAR, f'nf{i}', x, y0, h=8.0, r=0.42, col='#8a7c66')


def scene_cap09_naaman():
    """19. INTRO CAP. 9 — la orilla del Jordan: Naaman erguido con su orgullo puesto,
    el general y un soldado a su espalda, y el rio ancho por delante. Las tiendas del
    campamento sirio y la cordillera al fondo."""
    new_scene()
    setup_cine(target=(0.0, 3.2, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#8a8256', far=30.0, near=-40.0, half_w=70.0)
    # --- far (FLAT): la cordillera siria HUNDIDA (z negativa). Una esfera de r=100 a
    # 800 m queda entera por ENCIMA de la linea de vista del borde del suelo y flota
    # como un zepelin: hay que enterrarle la panza bajo el horizonte.
    ridge(FAR, 'na', [-520, -260, 0, 260, 520], 800.0, 100.0, '#5e6474', sz=0.34,
          jitter=0.45, z=-30.0)
    ridge(FAR, 'nb', [-360, -120, 120, 380], 860.0, 72.0, '#6e7484', sz=0.28,
          jitter=0.4, z=-34.0)
    for i, (x, y, s) in enumerate(((-18.0, 42.0, 1.4), (-12.5, 45.5, 1.2), (-7.0, 43.0, 1.3),
                                   (11.0, 44.0, 1.3), (16.5, 46.5, 1.2))):
        tienda(FAR, f'nt{i}', at=(x, y, -0.35), yaw=(i * 23.0) % 40 - 20.0, s=s,
               col='#5a5464')

    # --- mid: el Jordan viene DESDE EL FONDO hacia la camara por la derecha — la
    # leccion del arroyo de Querit: de lado a lado y lejos era una raya azul de un
    # dedo; en diagonal la perspectiva lo ensancha y se lee que corre
    river = [(-12.0, 27.0), (-6.0, 20.5), (0.0, 15.0), (5.5, 10.5), (9.5, 6.0),
             (12.0, 1.0), (13.0, -3.0)]
    road_strip(MID, 'nbank', river, 4.5, 8.5, '#9a9068', z=0.03)
    road_strip(MID, 'nw', river, 3.0, 6.2, '#4a7a8c', z=0.06)
    # Naaman en la orilla, vuelto a medias hacia el agua; su escolta detras
    actor('naaman', at=(-1.2, 5.0, 0.0), yaw=-30.0, pose=POSE_STAND_PROUD)
    actor('general_sirio', at=(1.8, 7.6, 0.0), yaw=-34.0, pose=POSE_GUARD)
    actor('soldado_sirio', at=(-4.8, 7.6, 0.0), yaw=8.0, pose=POSE_GUARD)
    # los carros y caballos "con su plata y su oro" quedan sugeridos por el cofre
    place(MID, tiles.build_chest, at=(-5.4, 4.6, 0.0), scale=1.5, m=tiles.mats())
    for i, (x, y) in enumerate(((7.6, 9.2), (3.4, 13.5), (-8.5, 12.0))):
        tuft(MID, f'nr{i}', x, y, 0.0, 8, '#5a7a44', h=1.4, spread=0.8, w=0.10, lean=8.0)

    # --- near: las canas de la ribera
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.02)):
        tuft(NEAR, f'ng{i}', x + 1.4, y0 + 0.8, zb * 0.78, 10, '#4e6b34',
             h=(zt - zb) * 0.16, spread=1.4, w=0.10, lean=10.0)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'nk{i}', x, y0, lift(zb, zt, 0.42 + 0.12 * i), (x1 - x0) * 0.07,
              '#6b5a3c', lean=6.0 * (1 if i else -1))


def scene_cap10_dotan():
    """20. INTRO CAP. 10 — la noche del asedio, vista desde la azotea: Eliseo en pie y
    el criado senalando el cerco — tiendas y fogatas rodeando la ciudad. Los carros de
    fuego del monte los pone el motor (Embers + Shaft en la escena B).

    Sin flat_layer: la capa lejana lleva las fogatas y las tiendas, y a cambio no hay
    cordillera (una ridge con cavity es un collar de perlas; la noche cerrada basta).
    """
    new_scene()
    setup_cine(target=(0.0, 4.0, 2.10), dist=13.0, elev=9.0, lens=42.0)
    # El "suelo" de la escena es la azotea; la llanura sitiada esta ABAJO y lejos.
    # El techo acaba a 8.6 m: mas alla se cae a la llanura (la capa lejana).
    ground(MID, '#3a3444', far=8.6, near=-40.0, half_w=40.0)
    # --- far: la llanura nocturna con el cerco — fogatas pequenas y tiendas oscuras.
    # Sin platillos de resplandor y sin pretil gigante: los puntos de brasa sobre el
    # llano oscuro son los que cuentan el asedio.
    # La llanura casi a ras (top -0.3): hundida 3 m, tiendas y fogatas quedaban
    # colgadas en el aire como una repisa con adornos.
    build(FAR, box, 'cine_dplain', (0.0, 200.0, -0.55), (900.0, 340.0, 0.5),
          M('dplain', '#1c1930', sat=1.0, val=0.8))
    ridge(FAR, 'dh', [-420, -180, 60, 300], 700.0, 90.0, '#141226', sz=0.30,
          jitter=0.4, z=-28.0)
    # Tiendas DE PERFIL (yaw ~80): de frente el prisma es una piramide. Repartidas en
    # profundidad de verdad (y 32..64), no en una fila de feria.
    for i, (x, y, s, yw) in enumerate(((-24.0, 44.0, 1.5, 78.0), (-14.0, 34.0, 1.3, 96.0),
                                       (-6.0, 52.0, 1.7, 70.0), (5.0, 33.0, 1.4, 102.0),
                                       (14.5, 42.0, 1.5, 84.0), (24.0, 56.0, 1.6, 74.0),
                                       (-33.0, 60.0, 1.8, 88.0), (33.0, 48.0, 1.6, 92.0))):
        tienda(FAR, f'dt{i}', at=(x, y, -0.35), yaw=yw, s=s, col='#332e40')
    for i, (x, y) in enumerate(((-20.0, 38.0), (-10.0, 31.5), (-1.5, 45.0), (9.5, 32.0),
                                (19.0, 38.5), (28.0, 50.0), (-28.5, 52.0), (1.5, 58.0),
                                (-38.0, 68.0), (38.0, 64.0))):
        build(FAR, sphere, f'cine_dfire{i}', (x, y, -0.22), 0.55,
              M('dfire', '#e8863a', sat=1.3, val=1.0), scale=(1.15, 1.0, 0.85))

    # --- mid: la azotea de Dotan, con su pretil BAJO y oscuro en el borde y las
    # tinajas del tejado; el profeta sereno y el criado arrodillado senalando el cerco
    build(MID, box, 'cine_dpar', (0.0, 8.3, 0.38), (17.0, 0.6, 0.76),
          M('dparapet', '#454052'), round=0.15)
    build(MID, box, 'cine_dpar2', (0.0, 8.3, 0.84), (17.4, 0.8, 0.20),
          M('dparapet2', '#3a3646'), round=0.2)
    actor('eliseo', at=(-2.2, 4.6, 0.0), yaw=8.0, pose=POSE_STAND_PROUD)
    actor('criado_eliseo', at=(0.6, 4.4, 0.0), yaw=-26.0, pose=POSE_KNEEL)
    for x in (-6.0, 6.2):
        place(MID, tiles.build_jar, at=(x, 6.4, 0.0), scale=1.5, rot_z=x * 9.0,
              m=tiles.mats())
    place(MID, tiles.build_torch, at=(-4.2, 7.6, 0.0), scale=1.0, m=tiles.mats())

    # --- near: almenas BAJAS y con huecos (una fila de losas altas era un cementerio)
    y0 = near_y(bite=0.80)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.06)):
        build(NEAR, box, f'cine_dmerlon{i}', (x, y0, lift(zb, zt, 0.10)),
              ((x1 - x0) * 0.13, 0.7, (zt - zb) * 0.22),
              M('dmerlon', '#3a3444'), round=0.15)


# =============================================================================
# Piezas del Ascenso (2 Reyes 2:8-14)
# =============================================================================
def agua_muro(layer, tag, at=(0.0, 0.0, 0.0), dir=1, W=5.4, L=3.2, h=2.6,
              col='#3d6c86', foam='#9cc8d4'):
    """Muro de agua del Jordan partido. La leccion que costo dos intentos: el muro es el
    RIO entero represado — cruza el cauce de orilla a orilla (W) y presenta hacia la
    brecha su CORTE, un tajo vertical mas claro —, no una tapia larga paralela al agua
    (asi salian sofas azules). `at` marca el borde de la brecha y `dir` hacia donde se
    extiende el agua; la cresta cae alejandose (el rio vuelve a su nivel) y tiene que
    SACARLE cabeza y media al profeta: dos lomos a su altura eran mobiliario, no milagro."""
    objs = []
    # la falda: una lenteja honda bajo las olas (una caja biselada asomaba esquinas de
    # gabarra por delante de la primera ola)
    objs += build(layer, sphere, f'cine_wall{tag}_sk', (dir * L * 0.95, 0.0, h * 0.04), 1.0,
                  M('wallskirt', '#2f5a74'), scale=(L * 1.35, (W + 0.6) * 0.5, h * 0.15))
    # tres OLAS en cascada: cupulas de esfera semienterradas, la mas alta al filo de la
    # brecha y bajando hacia atras. Ni cajas con taper (tienda de campana) ni lajas
    # verticales (silos): el agua detenida es una masa REDONDA, como todo en este juego.
    olas = ((1.55, 1.8, 1.00, 0.50, '#4d80a0'), (4.0, 1.6, 0.74, 0.46, col),
            (6.1, 1.5, 0.52, 0.42, '#33607a'))
    for i, (x, rx, hk, wk, sc) in enumerate(olas):
        # parts.rock con rugosidad minima: la superficie ONDULA (agua) en vez de ser una
        # cupula lisa (un iglu azul).
        objs += build(layer, parts.rock, f'cine_wall{tag}_o{i}',
                      (dir * x, (i % 2) * 0.3 - 0.15, h * hk * 0.30), 1.0,
                      M(f'wallo_{sc}', sc), scale=(rx, W * wk, h * hk * 0.74), seed=i * 5 + 2,
                      rough=0.05)
    # SIN espuma, a proposito: el casquete claro en la cumbre convertia cada ola en una
    # tinaja con boca, y el cordon volado, en un rail. El volumen redondo ya dice agua.
    return transform(objs, at=at)


def caballo_fuego(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0,
                  body_col='#e87828', mane_col='#ffd860'):
    """Caballo de fuego, construido mirando a -Y como todos los builders. No reusa al
    buey: lo que lee "caballo" a media distancia es el cuello ARQUEADO y las patas
    largas al galope, y lo que lee "de fuego" es la crin clara y las lenguas de llama
    hacia atras. Las patas delanteras van recogidas y las traseras extendidas."""
    body = M(f'fhorse_{body_col}', body_col, sat=1.3)
    dark = M('fhorse_dark', '#b64a1e', sat=1.25)
    mane = M(f'fmane_{mane_col}', mane_col, sat=1.2)
    objs = []
    # tronco: caja redondeada + esfera de pecho (masa continua, no piezas sueltas)
    objs += build(layer, box, f'cine_fh{tag}_b', (0, 0.15, 1.18), (0.60, 1.75, 0.62),
                  body, round=0.65, taper=(0.9, 0.85))
    objs += build(layer, sphere, f'cine_fh{tag}_ch', (0, -0.62, 1.22), 0.40, body)
    # cuello: UN tubo arqueado del pecho a la testuz — las cajas giradas del primer
    # intento dejaban la cabeza flotando separada del cuello
    objs += build(layer, tube, f'cine_fh{tag}_n', (0, -0.55, 1.35), (0, -1.15, 2.00),
                  0.26, 0.17, body, segs=14)
    hd = build(layer, box, f'cine_fh{tag}_h', (0, -1.38, 2.00), (0.22, 0.55, 0.26),
               dark, round=0.6, taper=(0.8, 0.85))
    for o in hd:
        parts.rotate_about(o, (0, -1.15, 2.02), 'X', 30.0)
    objs += hd
    # crin de fuego: un tubo fino pegado a la nuca + lenguas hacia atras
    objs += build(layer, tube, f'cine_fh{tag}_mn', (0, -0.42, 1.60), (0, -1.00, 2.16),
                  0.13, 0.07, mane, segs=12)
    for i, (dy, dz, hh, ang) in enumerate(((-0.15, 2.06, 0.48, 40.0), (0.22, 1.80, 0.42, 48.0),
                                           (0.58, 1.58, 0.36, 56.0))):
        fl = build(layer, box, f'cine_fh{tag}_fl{i}', (0, dy, dz), (0.10, 0.13, hh),
                   mane, round=0.7, taper=(0.15, 0.25))
        for o in fl:
            parts.rotate_about(o, (0, dy, dz - hh * 0.5), 'X', ang)
        objs += fl
    # patas al galope, tubos: delanteras alcanzando adelante, traseras estiradas atras
    for sg in (-1, 1):
        objs += build(layer, tube, f'cine_fh{tag}_lf{sg}', (sg * 0.20, -0.55, 0.95),
                      (sg * 0.23, -1.02, 0.58), 0.15, 0.08, dark, segs=10)
        objs += build(layer, tube, f'cine_fh{tag}_lb{sg}', (sg * 0.20, 0.72, 0.85),
                      (sg * 0.25, 1.38, 0.32), 0.16, 0.08, dark, segs=10)
    # cola: una llama que flamea hacia atras y arriba
    tail = build(layer, box, f'cine_fh{tag}_t', (0, 1.10, 1.42), (0.13, 0.16, 0.62),
                 mane, round=0.7, taper=(0.2, 0.3))
    for o in tail:
        parts.rotate_about(o, (0, 1.05, 1.15), 'X', -52.0)
    objs += tail
    return transform(objs, at=at, yaw=yaw, scale=s)


def carro_fuego(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0):
    """El carro de fuego: caja curva sobre dos ruedas macizas, lanza al yugo y llamas en
    el borde. Construido mirando a -Y (los caballos van aparte, en la escena, para poder
    adelantar medio cuerpo el de fuera — la leccion de la yunta del cap. 5)."""
    hull = M('fcar_hull', '#a03818', sat=1.3)
    trim = M('fcar_trim', '#f8c050', sat=1.25)
    flame = M('fcar_flame', '#ffe890', sat=1.15)
    objs = []
    # ruedas: discos macizos DE ORO (del color de la caja, carro y rueda eran un solo
    # borron rojo); el buje rojo marca el centro
    for sg in (-1, 1):
        objs += build(layer, tube, f'cine_fc{tag}_w{sg}', (sg * 0.62, 0.35, 0.72),
                      (sg * 0.74, 0.35, 0.72), 0.70, 0.70, trim, segs=18)
        objs += build(layer, sphere, f'cine_fc{tag}_hub{sg}', (sg * 0.70, 0.35, 0.72), 0.22,
                      hull, scale=(1.0, 1.0, 1.0))
    # caja: panza curvada + peto delantero alto con ribete de oro
    objs += build(layer, box, f'cine_fc{tag}_hull', (0, 0.30, 1.28), (1.20, 1.30, 0.72),
                  hull, round=0.5, taper=(0.66, 0.9))
    peto = build(layer, box, f'cine_fc{tag}_peto', (0, -0.42, 1.56), (1.24, 0.22, 0.78),
                 hull, round=0.5, taper=(0.85, 1.0))
    for o in peto:
        parts.rotate_about(o, (0, -0.42, 1.2), 'X', -12.0)
    objs += peto
    objs += build(layer, box, f'cine_fc{tag}_rib', (0, -0.50, 1.94), (1.26, 0.16, 0.14),
                  trim, round=0.6)
    # lanza al tiro y yugo
    objs += build(layer, tube, f'cine_fc{tag}_pole', (0, -0.5, 0.95), (0, -2.45, 1.15),
                  0.07, 0.05, trim, segs=10)
    objs += build(layer, tube, f'cine_fc{tag}_yoke', (-0.85, -2.45, 1.2), (0.85, -2.45, 1.2),
                  0.06, 0.06, trim, segs=10)
    # llamas del borde y de las ruedas, flameando hacia atras (+Y)
    for i, (dx, dy, dz, hh) in enumerate(((-0.5, 0.85, 1.75, 0.5), (0.0, 0.9, 1.8, 0.62),
                                          (0.5, 0.85, 1.75, 0.46), (-0.7, 0.95, 0.95, 0.4),
                                          (0.7, 0.95, 0.95, 0.4))):
        fl = build(layer, box, f'cine_fc{tag}_fl{i}', (dx, dy, dz), (0.14, 0.16, hh),
                   flame, round=0.7, taper=(0.2, 0.3))
        for o in fl:
            parts.rotate_about(o, (dx, dy, dz - hh * 0.5), 'X', 34.0)
        objs += fl
    return transform(objs, at=at, yaw=yaw, scale=s)


def manto_caido(layer, tag, at=(0.0, 0.0, 0.0), yaw=0.0, s=1.0):
    """El manto de Elias en el suelo: un monton de pliegues BLANDOS — esferas aplastadas
    solapadas, no lajas (las cajas planas con una punta alzada salian un trineo con
    aleron) — y un orillo de oro CORTO asomando por el borde delantero."""
    cloth = M('manto_cloth', '#d8702c', sat=1.3)
    cloth2 = M('manto_cloth2', '#e88a3c', sat=1.3)
    dark = M('manto_dark', '#b0521e', sat=1.25)
    gold = M('manto_gold', '#e8c050', sat=1.2)
    objs = []
    objs += build(layer, sphere, f'cine_mt{tag}_a', (0.0, 0.0, 0.10), 1.0,
                  cloth, scale=(0.85, 0.55, 0.17))
    objs += build(layer, sphere, f'cine_mt{tag}_b', (0.38, 0.22, 0.13), 1.0,
                  cloth2, scale=(0.55, 0.38, 0.15))
    objs += build(layer, sphere, f'cine_mt{tag}_c', (-0.38, -0.14, 0.11), 1.0,
                  dark, scale=(0.46, 0.32, 0.13))
    objs += build(layer, sphere, f'cine_mt{tag}_tip', (-0.62, -0.30, 0.20), 1.0,
                  cloth, scale=(0.22, 0.16, 0.16))
    objs += build(layer, box, f'cine_mt{tag}_hem', (0.12, -0.46, 0.08), (0.72, 0.11, 0.05),
                  gold, round=0.6)
    return transform(objs, at=at, yaw=yaw, scale=s)


# Poses propias del Ascenso. hand.R lleva el cayado: pasarlo de la horizontal vuelca el
# baculo (regla de POSE_RAISE), asi que el brazo que se alza de verdad es el IZQUIERDO.
POSE_CRY = {   # Eliseo ante el torbellino: el brazo libre al cielo, ABIERTO en Y para
    # que asome en silueta (pegado al torso desaparecia visto desde atras)
    'upper_arm.L': {'rot': [('X', -145.0), ('Y', 55.0)]},
    'forearm.L': {'rot': [('X', -16.0)]},
    # el brazo del cayado se queda BAJO: a -70 el baculo se volcaba en horizontal y
    # le cruzaba la cara (la regla de POSE_RAISE, que aqui se olvido una pasada)
    'upper_arm.R': {'rot': [('X', -25.0), ('Y', -20.0)]},
    'forearm.R': {'rot': [('X', -10.0)]},
    'chest': {'rot': [('X', -14.0)]}, 'head': {'rot': [('X', -26.0)]},
    'thigh.L': {'rot': [('X', 8.0)]}, 'thigh.R': {'rot': [('X', -10.0)]},
}

POSE_RECOIL = dict(POSE_KNEEL, **{   # de rodillas, echado atras, la vista en el carro
    # El brazo alzado va tambien ABIERTO (Y 44): solo hacia delante, con el cuerpo a
    # medio girar, la camara lo ve en escorzo y desaparece.
    'chest': {'rot': [('X', -12.0)]}, 'head': {'rot': [('X', -20.0)]},
    'upper_arm.L': {'rot': [('X', -118.0), ('Y', 44.0)]},
    'forearm.L': {'rot': [('X', -24.0)]},
})


# =============================================================================
# Los paneles del Ascenso
# =============================================================================
def scene_asc01_jordan():
    """21. EL ASCENSO 1 — el Jordan ABIERTO: los dos profetas cruzan en seco por el
    pasillo entre los muros de agua, hacia la camara. El rio corre de lado a lado y el
    corredor lo corta perpendicular: lo que cuenta la estampa es la BRECHA."""
    new_scene()
    setup_cine(target=(0.0, 3.2, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#9a8b60', far=30.0, near=-40.0, half_w=70.0)
    # --- far (FLAT): los montes de Galaad, hundidos para no flotar (leccion de naaman)
    ridge(FAR, 'ja', [-520, -260, 0, 260, 520], 800.0, 100.0, '#6a5e50', sz=0.34,
          jitter=0.45, z=-30.0)
    ridge(FAR, 'jb', [-360, -120, 120, 380], 860.0, 72.0, '#7a6e5c', sz=0.28,
          jitter=0.4, z=-34.0)
    # --- mid: el rio cruza el plano medio; la brecha seca va en x 0..4
    left = [(-30.0, 16.0), (-18.0, 14.5), (-8.0, 13.6), (-1.2, 13.2)]
    right = [(5.2, 12.9), (14.0, 12.4), (22.0, 12.8), (30.0, 13.6)]
    for tag, pts in (('jl', left), ('jr', right)):
        road_strip(MID, f'{tag}b', pts, 7.5, 7.5, '#9a9068', z=0.03)
        road_strip(MID, f'{tag}w', pts, 5.6, 5.6, '#4a7a8c', z=0.06)
    # el lecho seco del corredor, por ENCIMA del agua para que corte
    road_strip(MID, 'jdry', [(1.4, 18.5), (1.8, 13.0), (2.2, 7.5)], 3.6, 4.4,
               '#b0a070', z=0.09)
    # los muros de agua: el rio entero represado a cada lado de la brecha
    agua_muro(MID, 'l', at=(-0.8, 13.4, 0.0), dir=-1, W=5.4, L=3.2, h=2.6)
    agua_muro(MID, 'r', at=(4.8, 12.9, 0.0), dir=1, W=5.4, L=3.4, h=2.9)
    # los profetas salen de la brecha hacia la camara, Elias delante
    actor('elias', at=(1.3, 8.2, 0.0), yaw=6.0, pose=POSE_WALK)
    actor('eliseo', at=(2.2, 10.2, 0.0), yaw=-5.0, pose=POSE_WALK)
    for i, (x, y, ss) in enumerate(((-7.0, 8.0, 1.6), (8.6, 9.0, 1.3))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=ss, rot_z=i * 67.0,
              m=tiles.mats())
    # --- near: las canas de la ribera (el mismo encuadre que naaman: es el mismo rio)
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.02)):
        tuft(NEAR, f'jg{i}', x + 1.4, y0 + 0.8, zb * 0.78, 10, '#4e6b34',
             h=(zt - zb) * 0.16, spread=1.4, w=0.10, lean=10.0)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'jk{i}', x, y0, lift(zb, zt, 0.40 + 0.10 * i), (x1 - x0) * 0.07,
              '#6b5a3c', lean=6.0 * (1 if i else -1))


def scene_asc02_carro():
    """22. EL ASCENSO 2 — el carro de fuego con caballos de fuego aparta a los dos:
    cruza el plano medio hacia +X ya despegado del suelo, con Elias erguido en la caja
    y Eliseo de rodillas en primer termino. El cielo en llamas lo pone el motor."""
    new_scene()
    setup_cine(target=(0.0, 3.2, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#8a7048', far=30.0, near=-40.0, half_w=70.0)
    ridge(FAR, 'ca', [-520, -260, 0, 260, 520], 800.0, 100.0, '#4a3a34', sz=0.34,
          jitter=0.45, z=-30.0)
    ridge(FAR, 'cb', [-360, -120, 120, 380], 860.0, 72.0, '#5a4438', sz=0.28,
          jitter=0.4, z=-34.0)
    # --- mid: el tiro y el carro, DESPEGADOS (z 0.45): ya no tocan el suelo. Viajan
    # hacia -X; el caballo LEJANO va adelantado media cabeza (la leccion de la yunta).
    lift_z = 0.45
    caballo_fuego(MID, 'a', at=(-1.3, 11.9, lift_z + 0.15), yaw=-90.0)
    caballo_fuego(MID, 'b', at=(0.9, 10.1, lift_z), yaw=-90.0)
    carro_fuego(MID, 'c', at=(3.4, 10.8, lift_z), yaw=-90.0)
    # Elias erguido EN la caja (los pies dentro: el peto le tapa las piernas), la
    # silueta de POSE_RAISE contra el cielo
    actor('elias', at=(3.3, 10.6, lift_z + 0.75), yaw=8.0, pose=POSE_RAISE)
    # SIN estela de humo, a proposito: dos intentos salieron como chimeneas tapando el
    # carro (un tubo de smoke() a esta distancia no baja de un brazo de grosor). El
    # fuego en movimiento lo ponen las pavesas del motor (CineFx_Embers).
    # Eliseo apartado, de rodillas, la mano libre alzada hacia el carro
    actor('eliseo', at=(-4.6, 5.8, 0.0), yaw=30.0, pose=POSE_RECOIL)
    # el suelo acusa el paso: dos rocas y matas quemadas
    for i, (x, y, ss) in enumerate(((-8.2, 9.5, 1.5), (9.4, 8.0, 1.2))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=ss, rot_z=i * 53.0,
              m=tiles.mats())
    for i, (x, y) in enumerate(((-6.5, 12.0), (7.5, 14.5))):
        tuft(MID, f'cq{i}', x, y, 0.0, 8, '#6a4a2c', h=1.1, spread=0.8, w=0.10, lean=14.0)
    # --- near: rocas quemadas enmarcando
    y0 = near_y(bite=0.78)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'ck{i}', x, y0, lift(zb, zt, 0.44 + 0.10 * i), (x1 - x0) * 0.08,
              '#4a3a2c', lean=7.0 * (1 if i else -1))
    for i, x in enumerate(spread(3, x0, x1, inset=0.10)):
        tuft(NEAR, f'cg{i}', x + 1.2, y0 + 0.9, zb * 0.78, 9, '#4a3420',
             h=(zt - zb) * 0.13, spread=1.6, w=0.09, lean=12.0)


def scene_asc03_torbellino():
    """23. EL ASCENSO 3 — el torbellino: la columna de fuego sube y se sale por el borde
    alto del cuadro con Elias pequeno en lo alto, y Eliseo DE ESPALDAS en primer
    termino, el brazo libre al cielo. De espaldas a proposito: un brazo alzado de frente
    pasa por delante de la cara (regla de POSE_RAISE), de espaldas es puro clamor."""
    new_scene()
    setup_cine(target=(0.0, 3.2, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#6a5240', far=30.0, near=-40.0, half_w=70.0)
    ridge(FAR, 'ta', [-520, -260, 0, 260, 520], 800.0, 100.0, '#3a2c30', sz=0.34,
          jitter=0.45, z=-30.0)
    ridge(FAR, 'tb', [-360, -120, 120, 380], 860.0, 72.0, '#4a3838', sz=0.28,
          jitter=0.4, z=-34.0)
    # --- mid: el torbellino — una columna ESBELTA y lejana que se sale por el borde
    # alto (con radios de 2-5 m y a diez metros era una maceta gigante que se tragaba
    # el panel y a Elias dentro). Tres tubos anidados, del rescoldo al nucleo claro.
    smoke(MID, 'w0', 3.6, 18.5, 0.0, h=14.0, col='#c05a24', r0=1.0, r1=2.4, drift=1.8,
          segs=3)
    smoke(MID, 'w1', 3.4, 18.0, 0.3, h=13.0, col='#e8863a', r0=0.65, r1=1.6, drift=2.6,
          segs=3)
    smoke(MID, 'w2', 3.2, 17.6, 0.7, h=12.0, col='#f8c050', r0=0.35, r1=0.9, drift=3.2,
          segs=3)
    # la base del remolino muerde el suelo: un plato de brasa y polvo levantado
    build(MID, sphere, 'cine_wbase', (3.6, 18.2, 0.08), 2.6,
          M('wbase', '#8a5a30'), scale=(1.5, 1.1, 0.05))
    build(MID, sphere, 'cine_wbase2', (3.5, 18.0, 0.14), 1.7,
          M('wbase2', '#e8863a', sat=1.3), scale=(1.4, 1.0, 0.06))
    # escombro girando alrededor de la columna
    for i, (dx, dy, dz, ss) in enumerate(((-1.9, -0.8, 1.6, 0.30), (2.1, 0.6, 2.9, 0.24),
                                          (-1.6, 0.4, 4.1, 0.20), (1.8, -0.6, 5.0, 0.16))):
        build(MID, box, f'cine_wdeb{i}', (3.5 + dx, 18.0 + dy, dz), (ss, ss * 0.8, ss * 0.6),
              M('wdeb', '#5a4630'), round=0.5)
    # Elias alzado DELANTE de la columna, a media altura: en la boca del tubo quedaba
    # cortado por el borde alto del panel y medio fundido con el rescoldo
    actor('elias', at=(2.6, 14.0, 2.1), yaw=8.0, scale=0.95, pose=POSE_RAISE)
    # Eliseo a tres cuartos de FRENTE, la cara al cielo: de espaldas el manto blanco
    # leia como una mochila y el clamor no se veia
    actor('eliseo', at=(-2.6, 5.6, 0.0), yaw=30.0, pose=POSE_CRY)
    for i, (x, y, ss) in enumerate(((-8.8, 10.0, 1.4), (9.2, 12.5, 1.1))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=ss, rot_z=i * 47.0,
              m=tiles.mats())
    # --- near: dos agujas oscuras y matas, el marco en sombra
    y0 = near_y(bite=0.78)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'tk{i}', x, y0, lift(zb, zt, 0.46 + 0.08 * i), (x1 - x0) * 0.08,
              '#38281f', lean=8.0 * (1 if i else -1))
    for i, x in enumerate(spread(3, x0, x1, inset=0.12)):
        tuft(NEAR, f'tg{i}', x + 1.0, y0 + 0.8, zb * 0.78, 8, '#2f231a',
             h=(zt - zb) * 0.12, spread=1.5, w=0.09, lean=11.0)


def scene_asc04_manto():
    """24. EL ASCENSO 4 — el manto caido: Eliseo arrodillado alargando la mano hacia el
    manto de Elias, el Jordan en calma detras y, en el cielo lejano, el ultimo hilo del
    torbellino deshaciendose. La manana despues del fuego."""
    new_scene()
    setup_cine(target=(0.0, 3.2, 1.40), dist=13.0, elev=6.0, lens=42.0)
    flat_layer(FAR)
    ground(MID, '#9a8b60', far=30.0, near=-40.0, half_w=70.0)
    ridge(FAR, 'ma', [-520, -260, 0, 260, 520], 800.0, 100.0, '#6a6258', sz=0.34,
          jitter=0.45, z=-30.0)
    ridge(FAR, 'mb', [-360, -120, 120, 380], 860.0, 72.0, '#7a7264', sz=0.28,
          jitter=0.4, z=-34.0)
    # SIN rastro del torbellino en el cielo: el humo lejano de smoke() colgaba del borde
    # alto como un colmillo palido. La manana esta en calma y eso ES la escena.
    # --- mid: el Jordan en diagonal, en calma (el trazado EXACTO de naaman: mismo rio,
    # y aplanado en horizontal salia una franja tiesa de lado a lado)
    river = [(-12.0, 27.0), (-6.0, 20.5), (0.0, 15.0), (5.5, 10.5), (9.5, 6.0),
             (12.0, 1.0), (13.0, -3.0)]
    road_strip(MID, 'mbank', river, 4.5, 8.5, '#9a9068', z=0.03)
    road_strip(MID, 'mw', river, 3.0, 6.2, '#4a7a8c', z=0.06)
    # Eliseo arrodillado ante el manto, la mano tendida (POSE_DRINK es exactamente
    # eso); el manto PEQUENO y bajo su mano — a tamano real era una colchoneta
    manto_caido(MID, 'm', at=(-2.0, 5.2, 0.0), yaw=14.0, s=0.8)
    actor('eliseo', at=(-2.1, 6.3, 0.0), yaw=-6.0, pose=POSE_DRINK)
    for i, (x, y) in enumerate(((6.4, 9.6), (2.6, 13.8), (-8.0, 11.5))):
        tuft(MID, f'mr{i}', x, y, 0.0, 8, '#5a7a44', h=1.4, spread=0.8, w=0.10, lean=8.0)
    for i, (x, y, ss) in enumerate(((-7.6, 8.2, 1.5), (8.8, 13.0, 1.2))):
        place(MID, rock_cluster, at=(x, y, 0.0), scale=ss, rot_z=i * 59.0,
              m=tiles.mats())
    # --- near: las canas, cerrando el mismo encuadre con el que abrio el panel 21
    y0 = near_y(bite=0.74)
    x0, x1, zb, zt = frame_at(y0, SAFE_NEAR)
    for i, x in enumerate(spread(4, x0, x1, inset=0.02)):
        tuft(NEAR, f'mg{i}', x + 1.4, y0 + 0.8, zb * 0.78, 10, '#4e6b34',
             h=(zt - zb) * 0.16, spread=1.4, w=0.10, lean=10.0)
    for i, x in enumerate((x0, x1)):
        spire(NEAR, f'mk{i}', x, y0, lift(zb, zt, 0.40 + 0.10 * i), (x1 - x0) * 0.07,
              '#6b5a3c', lean=6.0 * (1 if i else -1))


PANELS = {
    1: ('tierra', scene_tierra),
    2: ('acab', scene_acab),
    3: ('jezabel', scene_jezabel),
    4: ('caceria', scene_caceria),
    5: ('galaad', scene_galaad),
    6: ('trono', scene_trono),
    7: ('querit', scene_querit),
    8: ('titulo', scene_titulo),
    # Intros de capitulo (indice = capitulo + 10). Se anaden por fases: el motor cae a la
    # carta de texto en los capitulos cuya estampa aun no existe (CHAPTER_CINE, art=nullptr).
    11: ('cap01_querit', scene_cap01_querit),
    12: ('cap02_sarepta', scene_cap02_sarepta),
    13: ('cap03_carmelo', scene_cap03_carmelo),
    14: ('cap04_horeb', scene_cap04_horeb),
    15: ('cap05_llamado', scene_cap05_llamado),
    16: ('cap06_jerico', scene_cap06_jerico),
    17: ('cap07_aceite', scene_cap07_aceite),
    18: ('cap08_sunamita', scene_cap08_sunamita),
    19: ('cap09_naaman', scene_cap09_naaman),
    20: ('cap10_dotan', scene_cap10_dotan),
    # La cinematica del Ascenso (2 Reyes 2:8-14): cuatro estampas, seis escenas en el
    # motor (cinematic_ascension en ui/cinematic.cpp; los paneles 21 y 24 llevan dos).
    21: ('asc01_jordan', scene_asc01_jordan),
    22: ('asc02_carro', scene_asc02_carro),
    23: ('asc03_torbellino', scene_asc03_torbellino),
    24: ('asc04_manto', scene_asc04_manto),
}


# =============================================================================
# Render
# =============================================================================
def render_panel(idx, out_dir, log=print):
    name, builder = PANELS[idx]
    # Densidad de malla y pliegues de panel SOLO mientras se monta este: una hoja de
    # personaje renderizada despues en la misma sesion tiene que salir como siempre.
    prev = parts.set_quality('cine')
    try:
        builder()
        # Luz, materiales, niebla y decorado (look.py) sobre la escena ya montada
        _REG['cam_y'] = bpy.context.scene.camera.location.y
        n = look.apply(idx, {LAYERS[l] for l in _FLAT}, _REG, frame_at, lambda ob: _put(ob, MID))
        log('  look: %s, %d piezas de decorado' % (look.LOOK.get(idx, {}).get('kind', 'day'), n))
    finally:
        parts.set_quality(prev)
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
        # gain 1.0: el 1.7 de render.py compensa la luz de estudio de Workbench; EEVEE
        # ya expone bien. Y un gradado suave encima.
        fr = render.post_soft_rect(hi, PANEL_W, PANEL_H, SS, outline=True, gain=1.0,
                                   outline_px=OUTLINE_PX - (1 if li == FAR else 0))
        fr = look.grade(fr)
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
