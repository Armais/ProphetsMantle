"""Hoja de efectos propia -> assets/sprites/vfx.png (8 fotogramas x 7 efectos, celda 128 px).

    import mantle_sprites.vfx as v; importlib.reload(v)
    v.main(r.SPRITES_DIR, r.TMP_DIR)            # todo
    v.main(r.SPRITES_DIR, r.TMP_DIR, only=[0])  # PARCHEA solo esas filas

Por que este modulo NO usa nada del pipeline de personajes:

  * **EEVEE, no Workbench.** Workbench es un sombreador de solidos: no tiene emision, y un
    fogonazo sin emision es una bola de plastico gris. Aqui cada efecto es geometria
    EMISIVA, que es lo unico que se parece a luz.
  * **Sin contorno oscuro.** Los 2 px de `post_soft` son lo que hace que un personaje se
    lea como figurita; en una chispa lo unico que hacen es convertirla en una pegatina.
  * **El resplandor se hace en el post, no con el bloom de EEVEE.** Comprobado: con
    `film_transparent`, el bloom pinta color fuera de la geometria pero NO escribe alfa,
    asi que el halo se recorta y desaparece. `_glow()` desenfoca brillo Y alfa a la vez.

Celda de 128 px = 32 unidades de juego a ASSET_SCALE 4, igual que un personaje. Ojo al
tamano: `spawn_fx` escala contra 32, no contra los 16 del tileset (ver game/combat.h).

`LAYOUT` tiene que ir en el mismo orden que `FxID` en src/game/combat.h.
"""
import math
import os

import bpy
import numpy as np

from . import render as R

CELL = 128          # px por fotograma = 32 unidades de juego x ASSET_SCALE 4
FRAMES = 8          # fotogramas por efecto (columnas)
SS = 2              # supermuestreo
ORTHO = 3.7         # unidades de mundo que caben en la celda (con 3.2 las puas
                    # del impacto se salian del marco en los ultimos fotogramas)

# Orden = FxID en src/game/combat.h. Cambiar aqui obliga a cambiar alli.
LAYOUT = ['impact', 'explosion', 'heal', 'sparks', 'smoke', 'holy', 'ring']


# =============================================================================
# Escena
# =============================================================================
def setup(tmp_dir):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x = sc.render.resolution_y = CELL * SS
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.filter_size = 1.2
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    sc.render.image_settings.color_depth = '8'
    sc.display_settings.display_device = 'sRGB'
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    sc.view_settings.exposure = 0.0
    sc.view_settings.gamma = 1.0
    ee = sc.eevee
    ee.use_bloom = False          # el halo se hace en el post: el bloom no escribe alfa
    ee.taa_render_samples = 24
    ee.use_gtao = False
    sc.world = sc.world or bpy.data.worlds.new('World')
    os.makedirs(tmp_dir, exist_ok=True)
    sc.render.filepath = os.path.join(tmp_dir, 'vfx_frame.png')

    cam = bpy.data.objects.get('vfx_cam')
    if cam is None:
        cd = bpy.data.cameras.new('vfx_cam')
        cam = bpy.data.objects.new('vfx_cam', cd)
        sc.collection.objects.link(cam)
    cd = cam.data
    cd.type = 'ORTHO'
    cd.ortho_scale = ORTHO
    cd.clip_start = 0.01
    cd.clip_end = 100.0
    # De frente: un efecto es plano y se mira de cara, no en 3/4 como los personajes
    cam.location = (0.0, -10.0, 0.0)
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    cd.shift_x = cd.shift_y = 0.0
    sc.camera = cam
    return cam


def _clear():
    for o in list(bpy.data.objects):
        if o.name == 'vfx_cam':
            continue
        bpy.data.objects.remove(o, do_unlink=True)


def _emit_mat(name, rgb, strength):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    em.inputs['Strength'].default_value = strength
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    nt.links.new(em.outputs['Emission'], out.inputs['Surface'])
    m.blend_method = 'BLEND'
    return m


_MAT_N = [0]


def blob(loc, radius, rgb, strength=4.0, scale=(1, 1, 1), rot=0.0):
    """Esfera emisiva. `scale` la estira (una chispa es una esfera aplastada en su
    direccion de vuelo, que es lo que le da la estela sin tener que simular nada)."""
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, segments=20, ring_count=12,
                                         location=(loc[0], 0.0, loc[1]))
    ob = bpy.context.object
    ob.scale = (scale[0], scale[1], scale[2])
    ob.rotation_euler = (0.0, rot, 0.0)
    _MAT_N[0] += 1
    ob.data.materials.append(_emit_mat('vfx_m%d' % _MAT_N[0], rgb, strength))
    bpy.ops.object.shade_smooth()
    return ob


def ring(radius, thickness, rgb, strength=4.0, squash=0.45, z=0.0):
    """Onda expansiva: el aro elipsado de un juego cenital.

    OJO al orden de los ejes. `primitive_torus_add` deja el toro en el plano XY, o sea
    de CANTO para esta camara (que mira a lo largo de +Y): sin rotarlo sale una raya
    horizontal, no un aro. Se gira 90 grados en X para ponerlo de cara, y como la escala
    de Blender es LOCAL y se aplica antes de girar, el aplastado en pantalla se pide en
    el eje local Y (que tras el giro apunta al Z del mundo)."""
    bpy.ops.mesh.primitive_torus_add(major_radius=radius, minor_radius=thickness,
                                     major_segments=48, minor_segments=10,
                                     location=(0, 0, z))
    ob = bpy.context.object
    ob.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    ob.scale = (1.0, squash, 1.0)
    _MAT_N[0] += 1
    ob.data.materials.append(_emit_mat('vfx_m%d' % _MAT_N[0], rgb, strength))
    bpy.ops.object.shade_smooth()
    return ob


def beam(top, bottom, radius, rgb, strength=8.0, taper=0.55):
    """Haz vertical: un CONO, no una esfera estirada. Una esfera con la Z multiplicada
    por diez es una pastilla con los bordes redondos, y se leia como una capsula de
    farmacia; un cono tiene la base ancha arriba y la punta abajo, que es como cae la luz."""
    h = max(0.01, top - bottom)
    bpy.ops.mesh.primitive_cone_add(vertices=28, radius1=radius, radius2=radius * taper,
                                    depth=h, location=(0.0, 0.0, (top + bottom) * 0.5))
    ob = bpy.context.object
    _MAT_N[0] += 1
    ob.data.materials.append(_emit_mat('vfx_m%d' % _MAT_N[0], rgb, strength))
    bpy.ops.object.shade_smooth()
    return ob


def shard(angle, dist, length, width, rgb, strength=5.0):
    """Punta alargada apuntando hacia fuera: los rayos de un fogonazo."""
    x, z = math.cos(angle) * dist, math.sin(angle) * dist
    ob = blob((x, z), 1.0, rgb, strength,
              scale=(length, width, width), rot=-angle)
    return ob


# =============================================================================
# Los siete efectos. Cada funcion monta el fotograma `t` (0..1) y no devuelve nada.
# =============================================================================
def fx_impact(t):
    """Golpe: destello blanco que se abre en puas y se apaga. Corto y seco."""
    k = 1.0 - t
    core = 0.42 * (1.0 - t * 0.65)
    blob((0, 0), core, (1.0, 0.95, 0.82), 9.0 * k + 1.0)
    n = 7
    for i in range(n):
        a = (i / n) * math.tau + 0.35
        d = 0.20 + t * 1.05
        ln = (0.30 + t * 0.42) * (1.0 - t * 0.5)
        w = 0.075 * (1.0 - t * 0.7)
        shard(a, d, ln, w, (1.0, 0.80, 0.42), 7.0 * k + 0.6)


def fx_explosion(t):
    """Estallido: nucleo que se abre en jirones y sale despedido.

    La primera version dejaba el nucleo entero creciendo y le pegaba encima las bolas de
    pavesa: salia un huevo naranja con lunares grises. Es el "collar de perlas" del que
    avisa CLAUDE.md. Aqui el nucleo MENGUA segun se abre y la metralla vuela POR FUERA."""
    hot = max(0.0, 1.0 - t * 1.35)
    col_hot = (1.0, 0.80 - 0.30 * t, 0.32 - 0.24 * t)
    col_ash = (0.34 + 0.12 * t, 0.29 + 0.08 * t, 0.28 + 0.07 * t)
    # Nucleo: crece de golpe al principio y se deshace despues
    core = (0.55 * min(1.0, t * 3.5)) * (1.0 - 0.75 * max(0.0, (t - 0.3) / 0.7))
    if core > 0.02:
        blob((0, 0.05 * t), core, col_hot, 9.0 * hot + 0.5)
    n = 10
    for i in range(n):
        a = (i / n) * math.tau + 0.7
        # Fuera del nucleo desde el primer fotograma: nada de bolas pegadas encima
        d = 0.30 + t * (0.85 + 0.30 * ((i * 7) % 5) / 5.0)
        r = (0.19 - 0.07 * t) * (0.6 + 0.6 * ((i * 3) % 4) / 4.0)
        warm = hot > 0.12 and (i % 3 != 0)
        blob((math.cos(a) * d, math.sin(a) * d * 0.75 + t * 0.22), r,
             col_hot if warm else col_ash, (7.0 * hot + 0.5) if warm else 0.7)
    if t > 0.45:
        ring(0.35 + t * 1.05, 0.045 * (1.0 - t * 0.6), (1.0, 0.85, 0.55),
             5.0 * (1.0 - t) + 0.3, squash=0.34)


def fx_heal(t):
    """Curacion: motas que SUBEN y un aro que se cierra a los pies. Nada golpea."""
    ring(1.05 - t * 0.50, 0.055 + 0.02 * (1 - t), (0.55, 1.0, 0.68),
         5.5 * (1.0 - t) + 0.5, squash=0.40, z=-0.75 + t * 0.15)
    n = 7
    for i in range(n):
        a = (i / n) * math.tau + t * 1.1
        d = (0.70 - t * 0.34) * math.cos(a)
        z = -0.70 + t * 1.45 + 0.14 * math.sin(i * 2.1)
        r = 0.185 * (1.0 - t * 0.45)          # antes 0.115: se veian como granos de arroz
        blob((d, z), r, (0.74, 1.0, 0.82), 7.0 * (1.0 - t * 0.6) + 0.5)


def fx_sparks(t):
    """Chispas: puntos que salen disparados, estirados en su direccion de vuelo."""
    n = 11
    for i in range(n):
        a = (i / n) * math.tau + 1.3 + 0.35 * ((i * 5) % 3)
        sp = 0.50 + 0.45 * (((i * 7) % 5) / 5.0)
        d = t * sp * 1.35                       # antes 1.6: se salian de la celda al final
        z = math.sin(a) * d - 0.55 * t * t          # les tira la gravedad
        r = 0.085 * (1.0 - t * 0.65)
        blob((math.cos(a) * d, z), r, (1.0, 0.86, 0.50),
             8.0 * (1.0 - t) + 0.5, scale=(1.0 + t * 2.2, 1.0, 1.0), rot=-a)


def fx_smoke(t):
    """Humo: bocanadas que suben, se separan y se deshacen.

    Las bocanadas van SEPARADAS y de tamanos distintos: juntas y del mismo tamano se
    fundian en un pedrusco gris, que es lo que parecia la primera version."""
    n = 9
    for i in range(n):
        a = (i / n) * math.tau + 0.4
        d = (0.10 + t * 0.78) * (0.5 + 0.9 * ((i * 3) % 4) / 4.0)
        z = -0.55 + t * 1.15 + 0.22 * math.sin(i * 1.7) + 0.30 * t * ((i * 5) % 3) / 3.0
        r = (0.15 + t * 0.20) * (0.55 + 0.75 * ((i * 5) % 3) / 3.0)
        g = 0.40 + 0.20 * ((i * 2) % 3) / 3.0 - 0.14 * t
        blob((math.cos(a) * d, z), r, (g, g * 0.97, g * 0.99), 0.50 + 0.30 * (1 - t))


def fx_holy(t):
    """Luz de lo alto: el haz BAJA, toca el suelo y se abre en un aro.

    El haz entra por arriba del cuadro (top fuera de marco) y su punta desciende: si el
    cono estuviera entero dentro se leeria como un objeto flotando, no como luz que viene
    de fuera. Al tocar el suelo se abre el aro y se enciende el fogonazo."""
    fall = min(1.0, t / 0.45)
    hit = max(0.0, (t - 0.42) / 0.58)
    tip = 0.95 - fall * 1.65                    # punta del haz: baja hasta el suelo
    if t < 0.92:
        beam(2.2, tip, 0.20 + 0.06 * t, (1.0, 0.98, 0.88),
             9.0 * (1.0 - 0.5 * hit) + 0.5, taper=0.42)
    if hit > 0.0:
        ring(0.18 + hit * 1.05, 0.055 * (1.0 - hit * 0.55), (1.0, 0.94, 0.72),
             7.5 * (1.0 - hit) + 0.5, squash=0.36, z=-0.70)
        blob((0, -0.62), 0.34 * (1.0 - hit * 0.55), (1.0, 0.97, 0.85),
             8.0 * (1.0 - hit) + 0.5, scale=(1.35, 1.0, 0.65))


def fx_ring(t):
    """Onda expansiva sola: un aro fino que se abre. Sirve para todo lo que empuja."""
    ring(0.14 + t * 1.30, 0.085 * (1.0 - t * 0.70), (0.86, 0.93, 1.0),
         8.0 * (1.0 - t) + 0.4, squash=0.38)
    if t < 0.45:
        k = 1.0 - t / 0.45
        blob((0, 0), 0.34 * k, (1.0, 1.0, 1.0), 7.0 * k + 0.3, scale=(1.2, 1.0, 0.7))


BUILDERS = {
    'impact': fx_impact, 'explosion': fx_explosion, 'heal': fx_heal,
    'sparks': fx_sparks, 'smoke': fx_smoke, 'holy': fx_holy, 'ring': fx_ring,
}


# =============================================================================
# Post: resplandor propio (el bloom de EEVEE no sirve, ver cabecera)
# =============================================================================
def _box_blur(x, r):
    """Desenfoque de caja separable por sumas acumuladas: O(n) y sin dependencias."""
    if r < 1:
        return x
    pad = np.pad(x, ((r + 1, r), (r + 1, r)), mode='constant')
    c = np.cumsum(np.cumsum(pad, axis=0), axis=1)
    c = np.pad(c, ((1, 0), (1, 0)), mode='constant')
    k = 2 * r + 1
    h, w = x.shape
    out = (c[k:k + h, k:k + w] - c[0:h, k:k + w] - c[k:k + h, 0:w] + c[0:h, 0:w])
    return out / float(k * k)


def _glow(rgb, a, radius=7, strength=0.85):
    """Halo alrededor de lo brillante. Desenfoca brillo Y alfa: si solo se desenfocara
    el color, el halo quedaria recortado por el alfa de la geometria y no se veria nada
    (que es exactamente lo que pasa con el bloom de EEVEE sobre pelicula transparente)."""
    lum = (rgb[..., 0] * 0.30 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11) * a
    halo = _box_blur(_box_blur(lum, radius), max(1, radius // 2)) * strength
    halo = np.clip(halo * 2.2, 0.0, 1.0)
    tint = np.zeros_like(rgb)
    for c in range(3):
        tint[..., c] = _box_blur(rgb[..., c] * a, radius)
    m = np.maximum(tint.max(axis=2, keepdims=True), 1e-4)
    tint = tint / m                                  # color del halo, normalizado
    a_out = np.clip(a + halo * (1.0 - a), 0.0, 1.0)
    # Debajo del alfa original manda la geometria; fuera, el halo
    w = np.where(a[..., None] > 0.02, a[..., None], 0.0)
    rgb_out = rgb * w + tint * (1.0 - w) * np.clip(halo, 0, 1)[..., None] ** 0.6
    return np.clip(rgb_out, 0, 1), a_out


def _post(hi):
    """(CELL*SS, CELL*SS, 4) -> (CELL, CELL, 4). Sin contorno: un efecto no es figurita."""
    a_hi = hi[..., 3]
    rgb_hi = hi[..., :3]
    prem = (rgb_hi * a_hi[..., None]).reshape(CELL, SS, CELL, SS, 3).mean(axis=(1, 3))
    a = a_hi.reshape(CELL, SS, CELL, SS).mean(axis=(1, 3))
    rgb = np.where(a[..., None] > 1e-4, prem / np.maximum(a[..., None], 1e-4), 0.0)
    rgb, a = _glow(np.clip(rgb, 0, 1), np.clip(a, 0, 1))
    out = np.zeros((CELL, CELL, 4), dtype=np.float32)
    out[..., :3] = rgb
    out[..., 3] = a
    return out


# =============================================================================
# Hoja
# =============================================================================
def _load_sheet(path):
    """Lee la hoja existente para PARCHEAR solo las filas pedidas (mismo truco que
    tiles.py: iterar sobre un efecto no puede borrar los otros seis)."""
    if not os.path.exists(path):
        return None
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = 'sRGB'
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)[::-1].copy()


def main(sprites_dir, tmp_dir, only=None, log=print):
    out_path = os.path.join(sprites_dir, 'vfx.png')
    W, H = CELL * FRAMES, CELL * len(LAYOUT)
    sheet = _load_sheet(out_path) if only else None
    if sheet is None or sheet.shape[0] != H or sheet.shape[1] != W:
        sheet = np.zeros((H, W, 4), dtype=np.float32)

    setup(tmp_dir)
    rows = range(len(LAYOUT)) if not only else [i for i in only if 0 <= i < len(LAYOUT)]
    for r in rows:
        name = LAYOUT[r]
        build = BUILDERS[name]
        for f in range(FRAMES):
            t = f / float(FRAMES - 1)
            _clear()
            build(t)
            hi = R._render_to_array()
            cell = _post(hi)
            sheet[r * CELL:(r + 1) * CELL, f * CELL:(f + 1) * CELL] = cell
        cov = 100.0 * (sheet[r * CELL:(r + 1) * CELL, :, 3] > 0.05).mean()
        log('  vfx %-10s fila %d  cobertura %.1f%%' % (name, r, cov))

    _clear()
    R.save_png(sheet, out_path, 'mantle_vfx')
    log('vfx.png -> %s (%dx%d)' % (out_path, W, H))
    return out_path
