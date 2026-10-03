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

CELL = 192          # px por retrato (2026-10-03: el doble; el motor lo pinta a 32 unidades = 128 px a 720p
                    # y a 1080p eran 96 px estirados al 200%)
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
    # Locutores del POZO de encuentros (GDD2.0 fase 2). Se anaden AL FINAL: los ocho
    # primeros siguen siendo EnemyType y cualquier reordenacion rompe el Bestiario en
    # silencio. Los encuentros que son ESCENA y no persona no salen aqui — llevan
    # `Portrait::NONE`, que es un estado soportado y no un hueco por rellenar.
    'abdias', 'hijo_profetas', 'hombre_baalsalisa',
    'muchacha_cautiva', 'leproso_samaria', 'principe_samaria',
    # Vinetas del pozo: los encuentros que son ESCENA y no persona. No se les pone cara
    # porque seria mentir sobre lo que el jugador esta mirando.
    'arroyo_seco', 'dos_pensamientos', 'nubecilla', 'enebro_angel',
    'tres_unciones', 'siete_mil', 'manto_jordan',
    # Los nueve jefes con hoja propia (bosses.py, 2026-10-03): hablan antes de pelear y hasta
    # hoy lo hacian con la cara de un enemigo raso (el Capitan era el soldado).
    'boss_capitan', 'boss_sacerdote', 'boss_sumo_sacerdote', 'boss_desesperacion', 'boss_espiritu',
    'boss_acreedor', 'boss_muerte', 'boss_guiezi', 'boss_benadad',
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


def scene_arroyo_seco():
    """El arroyo de Querit, seco (1 Reyes 17:7): los cantos rodados al aire y un ultimo charco.

    Dos intentos con el CAUCE fallaron, y por una razon que vale para cualquier vineta que
    venga: la camara va casi de frente (13 grados de picado), asi que **un hundido del
    terreno no se ve** — lo tapa lo que tenga delante. Las tres vinetas que si funcionan
    (el cuervo, el altar, la boca de la cueva) tienen por sujeto un OBJETO, no un relieve.
    Aqui el objeto son los cantos: piedras de rio pulidas y al descubierto, que es lo que se
    ve cuando el agua se va, con un ultimo charco entre ellas.
    """
    stone = _m('as_stone', '#c6bca8')
    stone_d = _m('as_stone_d', '#9b9080')
    stone_w = _m('as_stone_w', '#7d7264')   # las de abajo, todavia humedas
    water = _m('as_water', '#3d9ad6')
    mud = _m('as_mud', '#8a6b45')
    parts.box('lecho', (0, 0.35, -0.66), (2.4, 1.3, 0.28), mud, round=0.16)
    # El charco que queda, ENTRE las piedras y en primer plano
    parts.sphere('charco', (0.05, -0.34, -0.50), 0.42, water, scale=(1.5, 1.0, 0.14))
    # Monton de cantos: pulidos, aplastados y de tamanos muy distintos, que es lo que los
    # separa de un monton de escombros
    for i, (x, y, z, r, m, sc) in enumerate((
            (-0.72, 0.15, -0.34, 0.42, stone, (1.35, 1.0, 0.72)),
            (-0.10, 0.42, -0.26, 0.50, stone_d, (1.30, 1.0, 0.78)),
            (0.62, 0.18, -0.36, 0.38, stone, (1.30, 1.0, 0.70)),
            (0.98, 0.48, -0.30, 0.30, stone_d, (1.25, 1.0, 0.74)),
            (-1.02, 0.50, -0.38, 0.26, stone, (1.30, 1.0, 0.70)),
            (0.24, 0.05, -0.48, 0.24, stone_w, (1.40, 1.0, 0.55)),
            (-0.42, -0.10, -0.50, 0.20, stone_w, (1.40, 1.0, 0.52)))):
        parts.sphere('canto%d' % i, (x, y, z), r, m, scale=sc, segs=12, rings=8)


def scene_dos_pensamientos():
    """"Si Jehova es Dios, seguidle; y si Baal, id en pos de el" (1 Reyes 18:21).

    Una multitud no se dibuja en una celda de 96 px. Lo que SI se dibuja es la eleccion:
    DOS altares enfrentados, uno de piedra sin labrar y otro con el idolo encima, y el
    hueco entre los dos. Ese hueco es el pasaje entero.
    """
    stone = _m('dp_stone', '#c2b69f')
    stone_d = _m('dp_stone_d', '#8e8472')
    idol = _m('dp_idol', '#6b2f7a')
    idol_d = _m('dp_idol_d', '#3d1a46')
    earth = _m('dp_earth', '#7a6749')
    parts.box('suelo', (0, 0.45, -0.62), (2.6, 1.5, 0.26), earth, round=0.14)
    # Izquierda: doce piedras sin labrar, apiladas a lo bruto
    for row in range(2):
        for i in range(3):
            x = -1.42 + i * 0.36
            parts.box('jp%d_%d' % (row, i), (x, 0.25, -0.36 + row * 0.32),
                      (0.34, 0.50, 0.28), stone if (row + i) % 2 else stone_d,
                      round=0.45, taper=(0.9, 1.0))
    # Derecha: sillar labrado y el idolo de Baal, con cuernos
    parts.box('bp', (1.05, 0.25, -0.34), (1.05, 0.55, 0.34), stone_d, round=0.12)
    parts.box('bp2', (1.05, 0.25, -0.02), (0.86, 0.48, 0.30), stone_d, round=0.12)
    parts.box('idolo', (1.05, 0.18, 0.34), (0.34, 0.30, 0.44), idol, round=0.35, taper=(0.7, 0.9))
    parts.sphere('idolo_c', (1.05, 0.12, 0.60), 0.15, idol_d, scale=(1.0, 0.9, 1.0))
    for sx in (-1.0, 1.0):
        parts.tube('cuerno%d' % int(sx), (1.05 + sx * 0.10, 0.10, 0.66),
                   (1.05 + sx * 0.26, 0.10, 0.80), 0.045, 0.012, idol_d, segs=8)


def scene_nubecilla():
    """"Una pequena nube como la palma de la mano" (1 Reyes 18:44), sobre la cumbre.

    El pasaje son TRES anos sin lluvia y una nube del tamano de una mano. Asi que la nube
    tiene que salir PEQUENA de verdad contra la roca: agrandarla arruina el chiste.
    """
    rock = _m('nb_rock', '#8d8272')
    rock_d = _m('nb_rock_d', '#5f584c')
    cloud = _m('nb_cloud', '#e8eef4')
    cloud_d = _m('nb_cloud_d', '#a9b6c4')
    sea = _m('nb_sea', '#397fae')
    # El mar, al fondo y bajo: una banda, no un plano infinito
    parts.box('mar', (0, 1.20, -0.62), (2.8, 0.9, 0.16), sea, round=0.10)
    # La cumbre: tres masas, de mayor a menor hacia la izquierda
    for i, (x, y, w, h) in enumerate(((-0.75, 0.30, 1.5, 1.0), (0.55, 0.45, 1.2, 0.72),
                                      (1.35, 0.20, 0.8, 0.44))):
        parts.box('c%d' % i, (x, y, -0.70 + h * 0.5), (w, 1.0, h),
                  rock if i % 2 == 0 else rock_d, round=0.35, taper=(0.62, 0.86))
    # La nube: pequena, arriba a la derecha, y con su sombra propia
    for i, (dx, dy, r) in enumerate(((0.0, 0.0, 0.20), (0.17, -0.04, 0.15), (-0.16, 0.03, 0.13))):
        parts.sphere('nb%d' % i, (1.06 + dx, -0.10 + dy, 0.86), r, cloud, scale=(1.3, 1.0, 0.8))
    parts.sphere('nb_s', (1.02, 0.10, 0.76), 0.17, cloud_d, scale=(1.4, 1.0, 0.5))


def scene_enebro_angel():
    """El enebro, la torta cocida sobre las brasas y la vasija de agua (1 Reyes 19:6).

    Sin la comida esto es un arbusto en un desierto. Lo que hace el pasaje son las DOS
    cosas que aparecen a la cabecera, asi que van delante y grandes.
    """
    leaf = _m('ea_leaf', '#6f8a5a')
    leaf_d = _m('ea_leaf_d', '#4e6640')
    wood = _m('ea_wood', '#6a5236')
    sand = _m('ea_sand', '#c2a878')
    bread = _m('ea_bread', '#c8975a')
    bread_d = _m('ea_bread_d', '#9d7038')
    jar = _m('ea_jar', '#5f7f9a')
    coal = _m('ea_coal', '#b5462a')
    parts.box('suelo', (0, 0.45, -0.62), (2.6, 1.5, 0.26), sand, round=0.14)
    # El enebro: tronco corto y tres masas de hoja, cargadas a la izquierda para dejar
    # sitio delante — la comida es el sujeto de la escena, no el arbusto.
    parts.tube('tronco', (-0.55, 0.55, -0.50), (-0.48, 0.55, 0.10), 0.13, 0.10, wood, segs=10)
    for i, (dx, dy, dz, r) in enumerate(((0.0, 0.0, 0.32, 0.62), (-0.42, -0.10, 0.16, 0.42),
                                         (0.36, 0.08, 0.14, 0.40))):
        parts.sphere('hoja%d' % i, (-0.50 + dx, 0.55 + dy, 0.10 + dz), r,
                     leaf if i % 2 == 0 else leaf_d, scale=(1.15, 1.0, 0.82))
    # Las brasas y la torta encima
    parts.sphere('brasas', (0.72, -0.20, -0.46), 0.30, coal, scale=(1.3, 1.1, 0.34))
    parts.sphere('torta', (0.72, -0.22, -0.32), 0.30, bread, scale=(1.25, 1.05, 0.30))
    parts.sphere('torta_b', (0.72, -0.22, -0.27), 0.20, bread_d, scale=(1.2, 1.0, 0.16))
    # La vasija de agua
    parts.sphere('vasija', (1.42, -0.10, -0.36), 0.26, jar, scale=(1.0, 0.92, 0.95))
    parts.tube('cuello', (1.42, -0.10, -0.16), (1.42, -0.10, 0.02), 0.11, 0.13, jar,
               segs=14, rim_round=0.03)


def scene_tres_unciones():
    """El cuerno de aceite y TRES gotas (1 Reyes 19:15-16): Hazael, Jehu y Eliseo.

    No se dibujan los tres hombres — a esta escala serian tres manchas. Se dibuja lo que
    los une, que es el mismo cuerno inclinandose tres veces.
    """
    horn = _m('tu_horn', '#d9c79a')
    horn_d = _m('tu_horn_d', '#a8916a')
    oil = _m('tu_oil', '#e0a92f')
    cloth = _m('tu_cloth', '#7d5a86')
    parts.box('pano', (0, 0.45, -0.60), (2.4, 1.3, 0.16), cloth, round=0.30)
    # El cuerno, volcado hacia la derecha y de punta fina
    ob = parts.tube('cuerno', (-0.95, 0.10, 0.10), (0.62, 0.10, -0.16), 0.30, 0.09,
                    horn, segs=18, rim_round=0.05)
    parts.tube('boca', (-1.02, 0.10, 0.12), (-0.92, 0.10, 0.10), 0.31, 0.31, horn_d,
               segs=18, caps=False, rim_round=0.03)
    # Tres gotas cayendo, cada una un poco mas abajo
    for i, (x, z, r) in enumerate(((0.74, -0.24, 0.115), (0.92, -0.40, 0.10), (1.08, -0.54, 0.085))):
        parts.sphere('gota%d' % i, (x, 0.06, z), r, oil, scale=(0.85, 0.85, 1.35))
    parts.sphere('charco', (1.18, 0.10, -0.60), 0.26, oil, scale=(1.3, 1.0, 0.22))


def scene_siete_mil():
    """"Siete mil, cuyas rodillas no se doblaron ante Baal" (1 Reyes 19:18).

    Siete piedras EN PIE frente a un idolo caido. Es lo contrario del altar del Carmelo:
    alli las doce piedras se apilan, aqui se sostienen. Y el idolo esta tumbado, que es lo
    unico que hace falta decir.
    """
    stone = _m('sm_stone', '#c2b69f')
    stone_d = _m('sm_stone_d', '#948a76')
    idol = _m('sm_idol', '#6b2f7a')
    earth = _m('sm_earth', '#7a6749')
    parts.box('suelo', (0, 0.45, -0.62), (2.6, 1.5, 0.26), earth, round=0.14)
    # SIETE, en dos filas para que se cuenten de un vistazo (4 delante, 3 detras)
    for i in range(4):
        x = -1.15 + i * 0.76
        parts.box('pf%d' % i, (x, -0.10, -0.20), (0.30, 0.30, 0.70), stone,
                  round=0.30, taper=(0.80, 0.92))
    for i in range(3):
        x = -0.78 + i * 0.76
        parts.box('pt%d' % i, (x, 0.55, -0.12), (0.26, 0.28, 0.60), stone_d,
                  round=0.30, taper=(0.80, 0.92))
    # El idolo, tumbado boca abajo en primer plano
    ob = parts.box('idolo', (0.95, -0.62, -0.52), (0.34, 0.30, 0.90), idol,
                   round=0.32, taper=(0.7, 0.9))
    parts.rotate_about(ob, (0.95, -0.62, -0.52), 'X', 82.0)


def scene_manto_jordan():
    """El manto golpea el Jordan y las aguas se apartan (2 Reyes 2:8, 2:14).

    Lo que se ve no es un rio: es un rio PARTIDO. El cauce seco por el medio y las dos
    paredes de agua a los lados son toda la escena; el manto solo tiene que estar cayendo.
    """
    water = _m('mj_water', '#3d9ad6')
    water_d = _m('mj_water_d', '#2a6f9e')
    bed = _m('mj_bed', '#9a8562')
    wool = _m('mj_wool', '#8a4a2c')
    hem = _m('mj_hem', '#d9a341')
    parts.box('lecho', (0, 0.4, -0.66), (2.7, 1.6, 0.28), bed, round=0.14)
    # Las dos paredes de agua, altas y con cresta clara
    for sx in (-1.0, 1.0):
        parts.box('pared%d' % int(sx), (sx * 1.20, 0.45, -0.16), (1.05, 1.75, 0.86),
                  water, round=0.22, taper=(0.86, 1.0))
        parts.box('cresta%d' % int(sx), (sx * 1.20, 0.30, 0.30), (1.02, 1.60, 0.14),
                  water_d, round=0.40)
    # El manto en el aire, doblado: un pano corto y su orillo de oro
    ob = parts.box('manto', (0, -0.42, 0.34), (0.92, 0.16, 0.34), wool, round=0.30,
                   taper=(1.15, 1.0))
    parts.rotate_about(ob, (0, -0.42, 0.34), 'Y', -14.0)
    ob = parts.box('orillo', (0, -0.44, 0.19), (0.94, 0.14, 0.09), hem, round=0.40)
    parts.rotate_about(ob, (0, -0.44, 0.19), 'Y', -14.0)


SCENES = {
    'altar_carmelo': scene_altar_carmelo,
    'cuervos_querit': scene_cuervos_querit,
    'boca_cueva': scene_boca_cueva,
    'arroyo_seco': scene_arroyo_seco,
    'dos_pensamientos': scene_dos_pensamientos,
    'nubecilla': scene_nubecilla,
    'enebro_angel': scene_enebro_angel,
    'tres_unciones': scene_tres_unciones,
    'siete_mil': scene_siete_mil,
    'manto_jordan': scene_manto_jordan,
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
