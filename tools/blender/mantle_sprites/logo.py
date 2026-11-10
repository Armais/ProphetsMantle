"""Logo e icono del juego -> assets/branding/logo.png y mark_512.png.

    import mantle_sprites.logo as lg; importlib.reload(lg)
    lg.main(r.ROOT, r.TMP_DIR)                  # los dos encuadres
    lg.main(r.ROOT, r.TMP_DIR, only=['mark'])   # solo el recorte cerrado

Igual que portraits.py, esto USA el pipeline de personajes tal cual (Workbench, luz de
estudio, cavity, `post_soft` con contorno): el icono tiene que parecer el mismo muneco que
se ve jugando. Lo unico propio es la camara y el manto que cruza entre los dos, que no existe como pieza.

UNA composicion (el busto a dos) y DOS encuadres de ella:

- `logo`  : busto ancho, con hombros. 256/128/64 px.
- `mark`  : el mismo busto recortado a las dos cabezas. 48/32/24/16 px.

Antes eran dos motivos DISTINTOS —los profetas de cuerpo entero y un pano con fuego— y el
usuario lo rechazo el 2026-08-26: a 32 px no se reconocia ninguno. El gate ahora es la tira
de contacto de tools/branding/contact.py, que se mira ANTES de dar nada por bueno.

El contorno de `post_soft` son 2 px pensados para una celda de 128: a 1024 no se veria, asi
que se escala con el tamano (`outline_px`).
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Vector

from . import render as R
from . import characters as C
from . import anims
from . import parts
from . import rig

SS = 2
LOGO_PX = 1024
MARK_PX = 512
ELEV = 9.0          # grados de picado; los 30 de las hojas dejan al profeta mirandose los pies

# Paleta del manto. Lana parda con orillo de oro: el manto de Elias es de pelo
# (2 Reyes 1:8), no una capa de rey, asi que el oro es un ribete y no la tela.
WOOL = '#8a4a2c'
WOOL_D = '#5e2f1b'
HEM = '#d9a341'


def _m(key, col):
    return parts.get_mat('logo_' + key, parts.vivid(parts.hexc(col)))


# =============================================================================
# El manto y la llama: mallas CONTINUAS, no piezas de parts.py
# =============================================================================
#
# Dos intentos fallidos antes de esto, los dos por la misma razon de fondo: una TELA no se
# puede montar con las piezas de `parts`. Con una caja vertical salio una puerta de armario;
# con trece cajas en fila, un xilofono — cada caja trae su bisel y su cavity, asi que se leen
# como trece objetos y no como un pano. Una tela es UNA superficie continua, y lo que la hace
# parecer tela son los pliegues: ondular la malla en PROFUNDIDAD (eje Y) y dejar que el
# sombreado de Workbench dibuje las luces y las sombras a lo largo de la curva.
# =============================================================================
def _sheet(name, mat, x0, x1, y0, z_top, sag, drop, v0=0.0, v1=1.0,
           thick=0.05, nu=26, nv=10, fold=0.09, waves=2.6, hem_wave=0.045,
           tilt=0.0, flare=0.0):
    """Pano colgado entre x0 y x1: borde de arriba en catenaria, bajo ondulado y pliegues en
    profundidad. Malla cerrada (cara delantera, trasera y canto).

    `tilt` sube un extremo y baja el otro (el pano cruza en DIAGONAL en vez de colgar recto)
    y `flare` lo abre hacia el bajo — estrecho arriba y ancho abajo es lo que separa una
    PRENDA de un trapo tendido."""
    span = x1 - x0
    xc = (x0 + x1) * 0.5

    def surf(u, v):
        bow = math.sin(math.pi * u)                     # 0 en los extremos, 1 en el centro
        top = z_top - sag * bow + tilt * (u - 0.5)
        bot = top - drop * (0.60 + 0.40 * bow) + hem_wave * math.sin(3.3 * math.pi * u + 0.6)
        z = top + (bot - top) * v
        x = xc + (x0 + span * u - xc) * (1.0 + flare * v)
        # Los pliegues se abren hacia el bajo: arriba la tela esta sujeta, abajo vuela
        y = y0 + fold * math.sin(waves * math.pi * u + 0.4) * (0.35 + 0.65 * v)
        return x, y, z

    bm = bmesh.new()
    F = [[None] * (nv + 1) for _ in range(nu + 1)]
    B = [[None] * (nv + 1) for _ in range(nu + 1)]
    for i in range(nu + 1):
        u = i / float(nu)
        for j in range(nv + 1):
            v = v0 + (v1 - v0) * (j / float(nv))
            x, y, z = surf(u, v)
            F[i][j] = bm.verts.new((x, y - thick * 0.5, z))
            B[i][j] = bm.verts.new((x, y + thick * 0.5, z))
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((F[i][j], F[i + 1][j], F[i + 1][j + 1], F[i][j + 1]))
            bm.faces.new((B[i][j], B[i][j + 1], B[i + 1][j + 1], B[i + 1][j]))
    for i in range(nu):                                 # cantos superior e inferior
        bm.faces.new((F[i][0], B[i][0], B[i + 1][0], F[i + 1][0]))
        bm.faces.new((F[i][nv], F[i + 1][nv], B[i + 1][nv], B[i][nv]))
    for j in range(nv):                                 # cantos laterales
        bm.faces.new((F[0][j], F[0][j + 1], B[0][j + 1], B[0][j]))
        bm.faces.new((F[nu][j], B[nu][j], B[nu][j + 1], F[nu][j + 1]))
    return parts._finish(name, bm, mat, smooth=True)


def mantle_span(x0=-0.62, x1=0.62, y=-0.30, z_top=1.10, sag=0.20, drop=0.66,
                fold=0.15, waves=3.4, name='manto', **kw):
    """El manto: lana parda con orillo de oro en el bajo.

    Los pliegues son lo unico que separa un pano de una PANCARTA, y hay que pasarse: con
    fold 0.09 el cavity apenas los marcaba y la tela salia lisa."""
    _sheet(name, _m('wool', WOOL), x0, x1, y, z_top, sag, drop, fold=fold, waves=waves, **kw)
    # El orillo es la MISMA superficie en su ultimo tramo, un pelo mas gruesa para que
    # envuelva el canto en vez de pelearse con el en el z-buffer.
    _sheet(name + '_orillo', _m('hem', HEM), x0, x1, y, z_top, sag, drop,
           v0=0.90, v1=1.0, thick=0.064, nv=3, fold=fold, waves=waves, **kw)


# =============================================================================
# La pieza: el busto a dos
# =============================================================================
#
# UNA sola composicion para todos los tamanos, y dos encuadres de ella (eleccion del
# usuario, 2026-08-26, sobre la tira de contacto de tools/branding/contact.py):
#
#   logo (256/128/64)  : busto ancho, con hombros.
#   mark (48/32/24/16) : el mismo busto recortado a las dos cabezas y un trozo de manto.
#
# Es lo contrario del icono anterior, que tenia DOS motivos distintos (los profetas de
# cuerpo entero arriba, un pano con fuego abajo) y por eso a 32 px no se reconocia ninguno
# de los dos. Lo que sobrevive a 32 px es una CABEZA, no un cuerpo: a ese tamano una figura
# de pie mide cuatro pixeles de ancho.
#
# Tres bocetos mas se compararon y perdieron, y conviene no repetirlos: el traspaso (Elias
# echando el manto sobre Eliseo) deja el pano colgando por delante y se lee como un escudo;
# el manto solo, con capucha y llama, sale campana o brasero — el mismo fallo que se estaba
# arreglando; y una M tipografica aguanta 16 px pero renuncia a los dos profetas.
# =============================================================================

# El cayado son CUATRO objetos y no uno: `_staff` y, colgando de el, `_tip` / `_tip_core` /
# `_tip_halo`. Borrar solo el padre deja la punta encendida flotando — y en un encuadre de
# busto eso es justo lo que cae encima de la cabeza del profeta.
STAFF = ('_staff', '_tip')


def _drop(*subs):
    """Borra las mallas cuyo nombre contenga alguno de `subs`."""
    for o in list(bpy.data.objects):
        if o.type == 'MESH' and any(t in o.name for t in subs):
            bpy.data.objects.remove(o, do_unlink=True)


def _stand(name, x, turn):
    """Un profeta de pie en x, girado `turn` grados (positivo = mira hacia +X)."""
    char = C.build(name)
    anims.animate(char)
    arm = char['arm']
    arm.location = (arm.location.x + x, arm.location.y, arm.location.z)
    arm.rotation_mode = 'XYZ'
    arm.rotation_euler = (0.0, 0.0, math.radians(turn))
    return char


def scene_busto():
    """Elias y Eliseo hombro con hombro, con el manto cruzando en diagonal entre los dos.

    Se giran hacia dentro: de frente puro, el hueco central se lee como un pasillo. Los dos
    se distinguen por lo que sobrevive al reducir — pelo y barba oscuros contra calva,
    naranja contra azul —, no por el detalle de la cara.
    """
    for name, x, turn in (('elias', -0.33, 14.0), ('eliseo', 0.33, -14.0)):
        _stand(name, x, turn)
    _drop(*STAFF)
    bpy.context.scene.frame_set(anims.anim_base('idle'))
    # Cruza por DEBAJO de la barbilla: a la altura de la cara tapaba a Elias, y un icono en
    # el que el manto borra a uno de los dos no arregla nada.
    # Los DOS extremos tienen que caber en el cuadro (x <= 0.82 con el vuelo aplicado): un
    # pano que se sale por los dos lados deja de tener largo y se lee como un tablon.
    mantle_span(name='manto_diag', x0=-0.64, x1=0.64, y=-0.30, z_top=1.40, sag=0.04,
                drop=0.32, fold=0.11, waves=2.2, tilt=-0.28, flare=0.12)


# pieza -> (encuadre (x0, x1, z0, z1), lado en px). El encuadre sale CUADRADO por el lado
# mayor (`setup` usa max(ancho, alto)), asi que estrechar la caja del `mark` es justo lo que
# agranda las cabezas: menos ancho = menos ortho = todo mas grande.
PIECES = {'logo': ((-0.82, 0.82, 1.00, 1.98), LOGO_PX),
          'mark': ((-0.62, 0.62, 0.98, 2.22), MARK_PX)}


def setup(tmp_dir, px, lo, hi, pad=1.10):
    """Camara de la pieza: la de siempre (Workbench orto) encuadrando la caja medida."""
    cam = R.setup_scene(tmp_dir)
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = px * SS
    cd = cam.data
    cd.type = 'ORTHO'
    cd.shift_x = cd.shift_y = 0.0
    cx, cz = (lo.x + hi.x) * 0.5, (lo.z + hi.z) * 0.5
    d = 14.0
    a = math.radians(ELEV)
    cam.location = Vector((cx, -d * math.cos(a), cz + d * math.sin(a)))
    cam.rotation_euler = (math.radians(90.0 - ELEV), 0.0, 0.0)
    cd.ortho_scale = max(hi.x - lo.x, hi.z - lo.z) * pad
    return cam


def main(root, tmp_dir, only=None, log=print):
    # A tools/, NO a assets/: estos dos PNG son la FUENTE del .ico y no los carga el juego.
    # Lo que se distribuye son mantle.ico y mark_256.png, y los escribe make_icon.py.
    out_dir = os.path.join(root, 'tools', 'branding', 'src')
    os.makedirs(out_dir, exist_ok=True)
    paths = {}
    for name in (only or ('logo', 'mark')):
        box, px = PIECES[name]
        parts.clear_scene()
        scene_busto()
        lo, hi = Vector((box[0], 0, box[2])), Vector((box[1], 0, box[3]))
        setup(tmp_dir, px, lo, hi, pad=1.0)
        raw = R._render_to_array()
        # El contorno de 2 px es para celdas de 128: a 1024 hay que escalarlo o desaparece
        img = R.post_soft(raw, px, SS, outline=True, outline_px=max(2, px // 96))
        fname = 'logo.png' if name == 'logo' else 'mark_%d.png' % px
        path = os.path.join(out_dir, fname)
        R.save_png(img, path, 'mantle_' + name)
        cov = 100.0 * (img[..., 3] > 0.05).mean()
        paths[name] = path
        log('  %-5s %dx%d  caja %.2f x %.2f  cobertura %.1f%%  -> %s'
            % (name, px, px, hi.x - lo.x, hi.z - lo.z, cov, path))
    parts.clear_scene()
    return paths


# =============================================================================
# El icono DINAMICO (2026-11-10): Elias en contrapicado, la llama en alto y el manto al viento
# =============================================================================
#
# Peticion del usuario: "otro icono para el apk y el exe que sea mas dinamico, mas visual,
# usa las mejores practicas en el diseno de iconos". El busto a dos de arriba se leia (las dos
# cabezas aguantaban 32 px) pero era una foto de carnet: dos munecos de frente, quietos.
# Lo que piden las guias de iconos (Material, Apple HIG, las tiendas) y lo que hacen los iconos
# de juego que funcionan:
#
#   - UN foco, no dos: un personaje de cerca, con gesto. Dos cabezas a 32 px compiten.
#   - DIAGONAL: la llama arriba a la derecha, la cara en el centro, el manto barriendo hacia
#     abajo a la izquierda. Una composicion en diagonal se lee como movimiento; una simetrica,
#     como un retrato.
#   - Contrapicado (camara baja mirando arriba): el heroe crece. A la altura de los ojos es
#     un vecino.
#   - Luz de drama: la llama ilumina de lado y un contraluz frio recorta el borde contra el
#     fondo calido (lo que separa la figura del fondo a 32 px es ese filo).
#   - Fondo A SANGRE (el estallido del fuego del cielo lo pinta tools/branding/make_icon.py),
#     nada de recorte transparente: en Android el adaptativo pone el fondo igual.
#
# Esto solo renderiza la FIGURA (fondo transparente) con EEVEE y apunta en un json donde caen
# la llama y la cara, para que el empaquetado ponga ahi el resplandor y reencuadre a 16-32 px.

HERO_PX = 1024
HERO_POSE = {
    # El brazo del cayado se ABRE (Y) y no se alza: mas alla de la horizontal vuelca el
    # baculo (regla de cine.py), y adelantandolo (X negativo) la vara CRUZA el pecho — fue la
    # primera prueba. Abierto y casi vertical, la vara sube junto al hombro y la llama queda
    # fuera de la cabeza, en la esquina de arriba.
    'upper_arm.R': {'rot': [('X', 2.0), ('Y', -20.0)]},
    'forearm.R': {'rot': [('X', -10.0)]},
    # El brazo libre, ABAJO: adelantado cruzaba el pecho con un antebrazo y una mano enormes
    # en primer plano (a 1024 px una salchicha; a 32, una mancha color carne).
    'upper_arm.L': {'rot': [('X', 10.0), ('Y', 16.0)]},
    'forearm.L': {'rot': [('X', -6.0)]},
    'chest': {'rot': [('X', -6.0)]},
    'head': {'rot': [('X', -9.0), ('Z', -6.0)]},
}
HERO_TURN = -24.0      # grados: tres cuartos (la imagen se voltea al empaquetar)
HERO_CAM = dict(lens=70.0, dist=2.5, elev=-9.0, aim=(-0.10, 0.0, 1.58))


def scene_heroe():
    char = C.build('elias')
    arm = char['arm']
    arm.rotation_mode = 'XYZ'
    arm.rotation_euler = (0.0, 0.0, math.radians(HERO_TURN))
    rig.pose_apply(arm, HERO_POSE)
    # Llama grande en la punta: la del sprite es una gota pensada para 128 px. Va colgada
    # de la punta que ya existe, asi que sigue al cayado a donde lo lleve la pose.
    # La LLAMA no es una malla: en 3D salia un huevo amarillo plano (la emision satura y
    # Workbench/EEVEE no dan el degradado de una llama). La pinta make_icon.py en 2D, con su
    # nucleo blanco, su borde rojo y su resplandor, donde cae la punta del cayado (el json).
    # Aqui la gota del sprite se quita: asoma como una canica bajo la llama pintada.
    tip = bpy.data.objects.get('elias_tip')
    flame = []
    if tip is not None:
        bpy.context.view_layer.update()
        flame.append(tip.matrix_world.translation.copy())
        for o in list(bpy.data.objects):
            if o.name.startswith('elias_tip'):
                bpy.data.objects.remove(o, do_unlink=True)
    # El MANTO al viento: sale de los hombros y barre hacia el lado CONTRARIO a la llama y
    # hacia abajo, con su orillo de oro. Es la pieza que da el movimiento (y el titulo).
    mantle_span(name='hero_manto', x0=-0.18, x1=1.05, y=0.24, z_top=1.47, sag=0.05,
                drop=0.66, fold=0.17, waves=3.2, tilt=-0.40, flare=0.32)
    for o in bpy.data.objects:
        if o.name.startswith('hero_manto'):
            o.rotation_euler = (math.radians(-14.0), 0.0, math.radians(HERO_TURN * 0.6))
    return char, flame


def _hero_lights():
    """EEVEE: la llama como luz principal (calida, de arriba a la derecha), contraluz frio
    fuerte desde atras a la izquierda y un relleno bajo y tenue."""
    from . import icons as I
    I._eevee_look()
    for o in list(bpy.data.objects):
        if o.type == 'LIGHT':
            bpy.data.objects.remove(o, do_unlink=True)
    I._light('hero_key', 'SUN', (40, 0, 52), 4.2, (1.0, 0.78, 0.50), angle=10)
    I._light('hero_rim', 'SUN', (-62, 0, -38), 7.5, (0.70, 0.82, 1.0), angle=3)
    I._light('hero_fill', 'SUN', (95, 0, -150), 0.6, (1.0, 0.55, 0.35), angle=25)
    w = bpy.context.scene.world
    bg = w.node_tree.nodes.get('Background') if w and w.node_tree else None
    if bg is not None:
        bg.inputs[0].default_value = (0.55, 0.35, 0.30, 1.0)
        bg.inputs[1].default_value = 0.45


def main_heroe(root, tmp_dir, log=print):
    """-> tools/branding/src/heroe_1024.png + heroe.json (donde caen la llama y la cara)."""
    import json
    from bpy_extras.object_utils import world_to_camera_view
    out_dir = os.path.join(root, 'tools', 'branding', 'src')
    os.makedirs(out_dir, exist_ok=True)
    parts.clear_scene()
    parts.set_quality('cine')            # a 1024 px las facetas de la hoja se verian
    try:
        char, flame = scene_heroe()
        cam = R.setup_scene(tmp_dir)
        sc = bpy.context.scene
        sc.render.resolution_x = sc.render.resolution_y = HERO_PX * SS
        cd = cam.data
        cd.type = 'PERSP'
        cd.lens = HERO_CAM['lens']
        cd.shift_x = cd.shift_y = 0.0
        ax, ay, az = HERO_CAM['aim']
        e = math.radians(HERO_CAM['elev'])
        d = HERO_CAM['dist']
        cam.location = Vector((ax, ay - d * math.cos(e), az + d * math.sin(e)))
        cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
        _hero_lights()
        bpy.context.view_layer.update()
        raw = R._render_to_array()
        img = R.post_soft(raw, HERO_PX, SS, outline=True, outline_px=9, gain=1.0)
        path = os.path.join(out_dir, 'heroe_%d.png' % HERO_PX)
        R.save_png(img, path, 'mantle_heroe')

        def proj(p):
            v = world_to_camera_view(sc, cam, p)
            return [round(v.x, 4), round(1.0 - v.y, 4)]      # 0..1, y hacia abajo
        bpy.context.view_layer.update()
        head = bpy.data.objects.get('elias_head')
        meta = {'size': HERO_PX,
                'flame': proj(flame[0] if flame else Vector((0, 0, 2))),
                'head': proj(head.matrix_world.translation if head else Vector((0, 0, 1.62)))}
        if head:
            vs = head.data.vertices
            c = sum((head.matrix_world @ v.co for v in vs), Vector()) / len(vs)
            meta['head'] = proj(c)
        # Los iris, para el destello de fuego en los ojos que pinta make_icon (la mirada es
        # lo primero que se busca en una cara, y a 1024 px el iris son cuatro pixeles oscuros)
        eyes = []
        for s in ('L', 'R'):
            ob = bpy.data.objects.get('elias_iris_' + s)
            if ob is not None:
                vs = ob.data.vertices
                c = sum((ob.matrix_world @ v.co for v in vs), Vector()) / len(vs)
                eyes.append(proj(c))
        meta['eyes'] = eyes
        with open(os.path.join(out_dir, 'heroe.json'), 'w') as f:
            json.dump(meta, f, indent=1)
        cov = 100.0 * (img[..., 3] > 0.05).mean()
        log('  heroe %dx%d  cobertura %.1f%%  llama %s  cara %s -> %s'
            % (HERO_PX, HERO_PX, cov, meta['flame'], meta['head'], path))
    finally:
        parts.set_quality('sheet')
        parts.clear_scene()
    return path
