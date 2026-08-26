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
