"""Fondo del menu principal -> assets/sprites/menu_bg.png (1280x720, con alfa).

    import mantle_sprites.menu as mn; importlib.reload(mn)
    mn.main(r.SPRITES_DIR, r.TMP_DIR)

La MISMA composicion del icono (logo.py: Elias y Eliseo con el manto cruzando entre los
dos) abierta a 16:9: cada profeta flanquea un lado del cuadro y el manto tiende su
diagonal por el tercio bajo, dejando el centro y el cielo libres para el titulo y las
filas del menu. A diferencia del logo, este PNG SI lo carga el juego (MenuSystem), y por
eso va a assets/sprites/ y no a tools/branding/src.

El cielo NO se hornea: el motor pinta su degradado de ascua y la vineta debajo, asi que
el render sale con film_transparent como los paneles del prologo. Marco rectangular =
post_soft_rect, nunca post_soft.
"""
import math
import os

from mathutils import Vector
import bpy

from . import render as R
from . import parts
from . import anims
from . import logo as L

SS = 2
OUT_W, OUT_H = 1280, 720
ELEV = 9.0                      # el picado del logo: mas y los profetas se miran los pies

# Encuadre en unidades de mundo. El ancho manda (ortho_scale va al lado mayor del sensor)
# y el alto sale de la proporcion 16:9. Z0 corta a los profetas por el pecho: esto es un
# BUSTO a dos como el icono, no una figura de cuerpo entero de cuatro pixeles.
# Con 3.40 de ancho sobraba medio cielo y los profetas se hundian en el borde: a 3.20 las
# cabezas suben hasta y~65 de 180 (justo bajo el subtitulo) y el cielo queda para el titulo.
FRAME_W = 3.20
Z0 = 0.80
FRAME_H = FRAME_W * OUT_H / float(OUT_W)


def scene_menu():
    """Elias flanqueando la izquierda, Eliseo la derecha, girados hacia dentro; el manto
    cruza en diagonal entre los dos por debajo de los hombros, con los DOS extremos dentro
    del cuadro (la leccion del logo: un pano que se sale por los lados se lee como tablon).
    """
    for name, x, turn in (('elias', -1.15, 16.0), ('eliseo', 1.15, -16.0)):
        L._stand(name, x, turn)
    L._drop(*L.STAFF)
    bpy.context.scene.frame_set(anims.anim_base('idle'))
    # Tres lecciones de los dos primeros intentos: el bajo FUERA del cuadro deja una
    # pancarta parda (el orillo de oro es lo que lo lee como manto); un z_top de 1.48 le
    # tapa la barba a Elias (la regla del logo: por DEBAJO de la barbilla); y sin sag el
    # borde de arriba cruza recto y sale un estandarte de desfile, no una tela colgada.
    L.mantle_span(name='manto_menu', x0=-1.00, x1=1.00, y=-0.30, z_top=1.32, sag=0.20,
                  drop=0.26, fold=0.18, waves=4.2, tilt=-0.10, flare=0.10)


def setup(tmp_dir):
    cam = R.setup_scene(tmp_dir)
    sc = bpy.context.scene
    sc.render.resolution_x = OUT_W * SS
    sc.render.resolution_y = OUT_H * SS
    cd = cam.data
    cd.type = 'ORTHO'
    cd.shift_x = cd.shift_y = 0.0
    cz = Z0 + FRAME_H * 0.5
    d = 14.0
    a = math.radians(ELEV)
    cam.location = Vector((0.0, -d * math.cos(a), cz + d * math.sin(a)))
    cam.rotation_euler = (math.radians(90.0 - ELEV), 0.0, 0.0)
    cd.ortho_scale = FRAME_W
    return cam


def main(sprites_dir, tmp_dir, log=print):
    parts.clear_scene()
    scene_menu()
    setup(tmp_dir)
    raw = R._render_to_array()
    # Contorno a juego con el del logo (2 px por celda de 128): los bustos aqui son ~5x.
    img = R.post_soft_rect(raw, OUT_W, OUT_H, SS, outline=True, outline_px=6)
    path = os.path.join(sprites_dir, 'menu_bg.png')
    R.save_png(img, path, 'menu_bg')
    cov = 100.0 * (img[..., 3] > 0.05).mean()
    log('  menu_bg %dx%d  cobertura %.1f%%  -> %s' % (OUT_W, OUT_H, cov, path))
    parts.clear_scene()
    return path
