"""Retratos de los 12 personajes -> assets/sprites/portraits.png (6 col x 2 fil, celda 96 px).

    import mantle_sprites.portraits as pt; importlib.reload(pt)
    pt.main(r.SPRITES_DIR, r.TMP_DIR)             # los doce
    pt.main(r.SPRITES_DIR, r.TMP_DIR, only=['elias'])   # PARCHEA solo esos

Al reves que vfx.py, esto SI usa el pipeline de personajes tal cual: Workbench, luz de
estudio, cavity y el `post_soft` con su contorno de 2 px. Un retrato tiene que parecer el
mismo muneco que se ve en la sala; si se renderizara con otro motor cantaria al lado del
sprite.

Lo unico propio es la camara: primer plano de cabeza y hombros, de frente (los personajes
se construyen mirando a -Y) y sin la elevacion de 30 grados de las hojas, que en un
primer plano deja al personaje mirandose los pies.

El orden de `LAYOUT` tiene que coincidir con `namespace Portrait` en src/render/sprites.h.
"""
import math
import os

import bpy
import numpy as np
from mathutils import Vector

from . import render as R
from . import characters as C
from . import parts

CELL = 96           # px por retrato = 24 unidades de juego a ASSET_SCALE 4
COLS = 6
SS = 2

# Encuadre AUTOMATICO a partir de la caja del personaje ya construido. A ojo no vale:
# el leon es un cuadrupedo y el Agua Corrupta una masa, y con una altura fija de humano
# el leon salia diminuto en la esquina de arriba. Se mide y se encuadra.
HEAD_FRAC  = 0.26   # a que fraccion de la altura, contando desde arriba, esta la cara
FRAME_FRAC = 0.55   # cuanto de la altura del personaje entra en el retrato
AIM_Z = 1.44        # solo el valor por defecto antes de medir
ORTHO = 0.98

# Mismo orden que `PortraitID` en src/ui/codex.h. Los ocho enemigos van PRIMERO y en el
# orden de EnemyType, para que el Bestiario pueda indexar directamente por tipo.
# Los OCHO PRIMEROS van en el orden de EnemyType: el Bestiario indexa por tipo sin tabla
# de traduccion, asi que reordenar esta lista lo rompe en silencio.
# Del 12 en adelante, los locutores de los encuentros (game/events.h), en orden de capitulo.
LAYOUT = [
    'soldado_acab', 'arquero_real', 'profeta_baal', 'leon_desierto',
    'soldado_sirio', 'general_sirio', 'idolatra_rimon', 'agua_corrupta',
    'elias', 'eliseo', 'acab', 'jezabel',
    'viuda_sarepta', 'eliseo_labrador', 'hombre_jerico', 'mujer_profeta',
    'sunamita', 'naaman', 'criado_eliseo',
    'cuervos_querit', 'altar_carmelo', 'boca_cueva',
]

def _bbox():
    """Caja de todo lo que hay en la escena, en coordenadas de mundo."""
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in bpy.data.objects:
        if o.type != 'MESH':
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            for i in range(3):
                lo[i] = min(lo[i], w[i])
                hi[i] = max(hi[i], w[i])
    if lo.z > hi.z:
        return Vector((0, 0, 0)), Vector((0.6, 0.6, 1.9))
    return lo, hi


def setup(tmp_dir):
    """Escena de personaje de siempre, con la camara cambiada a primer plano."""
    cam = R.setup_scene(tmp_dir)
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = CELL * SS
    cd = cam.data
    cd.type = 'ORTHO'
    # De frente y a la altura de la cara: los 30 grados de elevacion de las hojas, en un
    # primer plano, dejan al personaje mirandose los pies.
    cam.location = Vector((0.0, -8.0, AIM_Z))
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    cd.shift_x = 0.0
    cd.shift_y = 0.0
    return cam


def _load_sheet(path, w, h):
    if not os.path.exists(path):
        return None
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = 'sRGB'
    iw, ih = img.size
    px = np.empty(iw * ih * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    arr = px.reshape(ih, iw, 4)[::-1].copy()
    return arr if (ih == h and iw == w) else None


# =============================================================================
# Vinetas: los tres locutores que NO son personas
# =============================================================================
#
# "Los cuervos de Querit", "El altar del Carmelo" y "La boca de la cueva" no son
# personajes: son la ESCENA. Ponerles una cara seria mentir sobre lo que el jugador esta
# mirando, y reusar el prop del tileset tampoco vale — es una silueta de 16 unidades
# pensada para verse desde arriba en una sala, y en un marco de retrato se lee como un
# mueble. Asi que se construyen aparte, aqui, porque solo existen como retrato.
#
# La camara es la misma de los retratos (de frente, orto), pero el encuadre NO: un
# personaje se encuadra por la cara (HEAD_FRAC) y una escena tiene que entrar ENTERA.
# =============================================================================

def _m(key, col):
    return parts.get_mat('scn_' + key, parts.vivid(parts.hexc(col)))


def scene_altar_carmelo():
    """Doce piedras (1 Reyes 18:31), la lena encima y la zanja llena de agua."""
    stone = _m('carm_stone', '#c2b69f')
    stone_d = _m('carm_stone_d', '#948a76')
    wood = _m('carm_wood', '#8a5f38')
    water = _m('carm_water', '#3d9ad6')
    earth = _m('carm_earth', '#7a6749')
    earth_d = _m('carm_earth_d', '#4e4132')
    # La zanja va DELANTE del altar (y negativa = hacia camara) y el agua ARRIBA de ella:
    # debajo de las piedras, o tras un labio, no se veia en ninguna de las dos pasadas.
    parts.box('suelo', (0, 0.75, -0.66), (3.30, 1.70, 0.30), earth, round=0.15)
    parts.box('zanja', (0, -0.55, -0.66), (3.10, 0.92, 0.30), earth_d, round=0.12)
    parts.box('agua', (0, -0.55, -0.50), (2.86, 0.70, 0.10), water, round=0.30)
    # DOCE piedras, una por cada tribu: 4 + 4 + 4, alternando tono para que se cuenten
    for row in range(3):
        z = -0.30 + row * 0.34
        half = 1.02 - row * 0.12
        for i in range(4):
            x = -half + (2 * half) * (i / 3.0)
            m = stone if (row + i) % 2 == 0 else stone_d
            parts.box('p%d_%d' % (row, i), (x, 0.34, z), (0.48 - row * 0.03, 0.56, 0.30),
                      m, round=0.45, taper=(0.92, 1.0))
    # La lena, cruzada y bien proud sobre la ultima hilada
    for i, (x, rot) in enumerate(((-0.36, 16.0), (0.00, -12.0), (0.36, 9.0))):
        ob = parts.tube('lena%d' % i, (x - 0.54, 0.26, 0.66), (x + 0.54, 0.26, 0.66),
                        0.086, 0.070, wood, segs=10, rim_round=0.02)
        parts.rotate_about(ob, (x, 0.26, 0.66), 'Y', rot)
    ob = parts.tube('lena_t', (-0.42, 0.06, 0.80), (0.42, 0.06, 0.80), 0.072, 0.058, wood,
                    segs=10, rim_round=0.02)
    parts.rotate_about(ob, (0, 0.06, 0.80), 'Y', -24.0)
    # Los cuatro cantaros del texto, a los lados y con panza
    jar = _m('carm_jar', '#b0714a')
    jar_d = _m('carm_jar_d', '#8a5436')
    for i, (x, z) in enumerate(((-1.52, -0.32), (-1.20, -0.36), (1.20, -0.36), (1.52, -0.32))):
        parts.sphere('cant%d' % i, (x, 0.10, z), 0.21, jar, scale=(1.0, 0.9, 0.95))
        parts.tube('cuello%d' % i, (x, 0.10, z + 0.17), (x, 0.10, z + 0.31), 0.10, 0.12,
                   jar_d, segs=14, rim_round=0.03)


def scene_cuervos_querit():
    """Un cuervo DE PERFIL con el pan en el pico (1 Reyes 17:6).

    De frente no hay manera: un pajaro simetrico con algo en el pico sale con la comida
    cruzada sobre la cara y se lee como un babero, o como un pico amarillo de tucan. De
    perfil el pico es una cuna que sobresale y el pan cuelga de su punta. Por eso este es
    el unico modelo del proyecto construido mirando a -X en vez de a -Y.
    """
    # Negro plano NO: Workbench con luz de estudio lo sube a gris medio y se pierde el
    # volumen. Un azul-negro saturado si lee como cuervo.
    feather = _m('cue_feather', '#2b3244')
    feather_d = _m('cue_feather_d', '#1b2130')
    beak = _m('cue_beak', '#767d8b')
    bread = _m('cue_bread', '#d6a559')
    branch = _m('cue_branch', '#5a4630')
    leg = _m('cue_leg', '#4a4f5c')
    parts.tube('rama', (-1.60, 0.10, -1.02), (1.60, 0.10, -1.08), 0.12, 0.11, branch,
               segs=12, rim_round=0.03)
    # Cuerpo tumbado sobre X, cola atras (a la derecha) y cabeza adelante (a la izquierda)
    parts.sphere('cuerpo', (0.18, 0.00, -0.22), 0.50, feather, scale=(1.20, 0.88, 0.92))
    ob = parts.box('cola', (0.98, 0.02, -0.12), (0.86, 0.20, 0.28), feather_d,
                   round=0.7, taper=(0.40, 1.0))
    parts.rotate_about(ob, (0.62, 0.02, -0.16), 'Y', 22.0)
    # Ala plegada sobre el costado que mira a camara
    ob = parts.box('ala', (0.26, -0.30, -0.16), (0.92, 0.16, 0.40), feather_d,
                   round=0.85, taper=(0.55, 0.95))
    parts.rotate_about(ob, (0.26, -0.30, -0.16), 'Y', 12.0)
    parts.tube('cuello', (-0.10, 0.00, 0.06), (-0.34, 0.00, 0.30), 0.20, 0.17, feather, segs=16)
    parts.sphere('cabeza', (-0.46, 0.00, 0.40), 0.27, feather, scale=(1.05, 0.96, 0.94))
    # El pico: cuna gris que sobresale por delante, que es lo que faltaba de frente
    ob = parts.box('pico', (-0.86, 0.00, 0.32), (0.48, 0.13, 0.13), beak,
                   round=0.45, taper=(0.16, 0.5))
    # Giro PEQUENO y hacia abajo: la caja ya esta authored a lo largo de X (hacia
    # delante). Girarla 78 grados la ponia de pie y el cuervo salia con el pico al cielo.
    parts.rotate_about(ob, (-0.62, 0.00, 0.36), 'Y', -14.0)
    parts.sphere('ojo', (-0.50, -0.22, 0.48), 0.085, _m('cue_eye', '#efe7d4'),
                 scale=(1.0, 0.6, 1.0))
    parts.sphere('iris', (-0.51, -0.26, 0.48), 0.048, _m('cue_iris', '#c9a24a'),
                 scale=(1.0, 0.6, 1.0))
    # El pan colgando de la PUNTA del pico
    parts.sphere('pan', (-1.13, 0.00, 0.18), 0.23, bread, scale=(1.05, 0.86, 0.72))
    parts.box('corteza', (-1.13, -0.02, 0.31), (0.34, 0.13, 0.07),
              _m('cue_bread_d', '#a87c3c'), round=0.8)
    for i, x in enumerate((0.02, 0.30)):
        parts.tube('pata%d' % i, (x, 0.02, -0.62), (x, 0.02, -0.96), 0.045, 0.040, leg, segs=10)
        parts.box('garra%d' % i, (x - 0.04, 0.02, -0.98), (0.22, 0.10, 0.06), leg, round=0.7)


def scene_boca_cueva():
    """El vano de Horeb (1 Reyes 19:9). Tres reglas del proyecto mandan aqui:

    - El negro se hace HUNDIENDO geometria. Workbench con luz de estudio aclara cualquier
      plano oscuro hasta gris medio, asi que bajar el color no da un vano, da una mancha.
    - El fondo del vano es PLANO, no una esfera: una esfera oscura se lee como una bola
      metida en el hueco, no como un hueco.
    - Nada de corona de esferas iguales (el cavity subraya cada una: "collar de perlas") ni
      lascas sueltas DENTRO del vano, que salen flotando. La pared son POCAS masas grandes,
      y lo que estrecha la boca va pegado a ellas y de su mismo color.
    """
    rock = _m('cav_rock', '#8f8578')
    rock_d = _m('cav_rock_d', '#6b6259')
    deep = _m('cav_deep', '#241f1a')
    parts.box('fondo', (0, 2.60, 0.10), (3.30, 0.40, 3.40), deep)
    for nm, c, sz, rot in (('masa_i', (-1.75, 0.55, 0.00), (1.90, 1.30, 3.60), 8.0),
                           ('masa_d', (1.78, 0.55, 0.05), (1.85, 1.30, 3.60), -6.0),
                           ('techo', (0.05, 0.55, 1.86), (3.80, 1.30, 1.50), 4.0)):
        ob = parts.box(nm, c, sz, rock, round=0.55, taper=(0.86, 1.0))
        parts.rotate_about(ob, c, 'Y', rot)
    # Los hombros van PEGADOS a las masas y de su mismo color: asi estrechan la boca por
    # arriba y la dejan irregular en vez de leerse como dos bolas colgando dentro.
    parts.sphere('hombro_i', (-0.98, 0.45, 1.20), 0.66, rock, scale=(1.35, 0.85, 0.78))
    parts.sphere('hombro_d', (1.02, 0.45, 1.02), 0.58, rock, scale=(1.30, 0.85, 0.82))
    parts.box('suelo', (0, 0.10, -1.60), (3.40, 1.70, 0.52), rock_d, round=0.25)
    for i, (x, rr) in enumerate(((-0.86, 0.26), (-0.30, 0.18), (0.38, 0.22), (0.90, 0.28))):
        parts.sphere('cascote%d' % i, (x, -0.50, -1.36 + rr * 0.3), rr, rock,
                     scale=(1.25, 1.0, 0.70))


SCENES = {
    'altar_carmelo': scene_altar_carmelo,
    'cuervos_querit': scene_cuervos_querit,
    'boca_cueva': scene_boca_cueva,
}


SCENE_ELEV = 13.0   # grados de picado para las vinetas (los retratos van de frente)


def setup_scene_cam(tmp_dir, lo, hi):
    """Camara de vineta: como la de retrato pero con un punto de PICADO y encuadrando la
    escena entera. De frente puro, la zanja del altar cae detras del altar y no se ve."""
    cam = setup(tmp_dir)
    cx, cz = (lo.x + hi.x) * 0.5, (lo.z + hi.z) * 0.5
    d = 8.0
    a = math.radians(SCENE_ELEV)
    cam.location = Vector((cx, -d * math.cos(a), cz + d * math.sin(a)))
    cam.rotation_euler = (math.radians(90.0 - SCENE_ELEV), 0.0, 0.0)
    cam.data.ortho_scale = max(hi.z - lo.z, hi.x - lo.x) * 1.10
    return cam


def main(sprites_dir, tmp_dir, only=None, log=print):
    rows = (len(LAYOUT) + COLS - 1) // COLS
    W, H = CELL * COLS, CELL * rows
    out_path = os.path.join(sprites_dir, 'portraits.png')
    sheet = _load_sheet(out_path, W, H) if only else None
    if sheet is None:
        sheet = np.zeros((H, W, 4), dtype=np.float32)

    wanted = only or LAYOUT
    cam = setup(tmp_dir)
    for i, name in enumerate(LAYOUT):
        if name not in wanted:
            continue
        parts.clear_scene()
        scene = SCENES.get(name)
        if scene:
            scene()
        else:
            C.build(name)
        lo, hi = _bbox()                      # medir ANTES de meter la camara en la escena
        # `clear_scene` se lleva la camara por delante: se vuelve a montar por retrato
        cam = setup(tmp_dir)
        h = max(0.4, hi.z - lo.z)
        if scene:
            # Una vineta entra ENTERA, centrada y con picado: encuadrarla por la cara, como
            # a una persona, le cortaria justo la mitad que cuenta el pasaje.
            cam = setup_scene_cam(tmp_dir, lo, hi)
            z, ortho = cam.location.z, cam.data.ortho_scale
        else:
            z = hi.z - HEAD_FRAC * h
            # El ancho tambien manda: el Agua Corrupta es mas ancha que alta y encuadrada
            # solo por altura se le cortaba la cupula.
            ortho = max(0.62, FRAME_FRAC * h, (hi.x - lo.x) * 0.80)
        if not scene:
            cam.location = Vector(((lo.x + hi.x) * 0.5, -8.0, z))
            cam.data.ortho_scale = ortho
        raw = R._render_to_array()            # `hi` es la caja: no pisarla
        cell = R.post_soft(raw, CELL, SS, outline=True)
        r, c = divmod(i, COLS)
        sheet[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL] = cell
        cov = 100.0 * (cell[..., 3] > 0.05).mean()
        log('  retrato %-16s [%d,%d]  alto %.2f  mira z=%.2f ortho=%.2f  cobertura %.1f%%'
            % (name, r, c, h, z, ortho, cov))

    parts.clear_scene()
    R.save_png(sheet, out_path, 'mantle_portraits')
    log('portraits.png -> %s (%dx%d)' % (out_path, W, H))
    return out_path
