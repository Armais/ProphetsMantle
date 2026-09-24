"""El Carro de Fuego del milagro -> assets/sprites/chariot.png (2026-09-24).

El usuario: "tiene que aparecer un carro de fuego de verdad, como hace el capitulo del
ascenso de Elias". Hasta hoy el milagro era una embestida de 0,4 s con bolas de fuego: ni
carro ni caballos. Aqui se monta el MISMO carro y los mismos caballos de fuego que la
estampa del Ascenso (cine.carro_fuego / cine.caballo_fuego), rehechos por piezas para dos
cosas que la estampa no necesita:

  - GALOPE: cuatro fotogramas con las patas en cuatro fases, las ruedas girando (radios) y
    las llamas latiendo.
  - DOS CAPAS por fotograma: `back` (todo) se dibuja DETRAS del profeta y `front` (solo lo
    que queda entre el profeta y la camara: la pared cercana de la caja, el peto, la rueda
    de este lado) DELANTE. Asi el profeta que maneja el jugador va DENTRO del carro con su
    propio sprite — el de Elias, el de Eliseo o el de la skin que lleve.

Hoja: 8 columnas x 3 filas de 256 px (64 unidades a x4). Fila = direccion (0 este, 1 sur
—hacia la camara—, 2 norte; el oeste es el este en espejo), columnas 0-3 = `back` de los
fotogramas 0-3, columnas 4-7 = `front`. El ANCLA (el suelo de la caja, donde apoya los
pies el profeta) cae en un pixel fijo por direccion (ANCHOR, copiado en src/game/player.h
como CHARIOT_ANCHOR): cambiar uno sin el otro deja al profeta flotando fuera de la caja.

Estilo: el de las hojas de personaje (Workbench, 30 grados, contorno oscuro de post_soft)
mas un halo calido en el post (el `_glow` de vfx.py, que desenfoca brillo Y alfa): es fuego.
"""
import math
import os

import bpy
import numpy as np
from mathutils import Vector

from . import parts, render
from .parts import box, sphere, tube

CELL = 256
COLS, ROWS = 8, 3
SS = 2
FRAMES = 4
SCALE = 0.90                              # el grupo entero (5,2 m de carro y tiro) a 4,7 m
PX_PER_M = CELL / (render.ORTHO_SCALE * 2.0)   # misma escala que las hojas: 128 px = 2,55 m
DIRS = (('east', 90.0), ('south', 0.0), ('north', 180.0))
# pixel del ancla (x, y hacia abajo) en cada fila: los caballos van por DELANTE, asi que el
# ancla se aparta hacia atras para que quepan (este: a la izquierda; sur: arriba; norte: abajo)
ANCHOR = {'east': (52, 158), 'south': (128, 92), 'north': (128, 200)}
FLOOR_Z = 0.95                            # suelo de la caja, en el carro sin escalar


def M(name, h, sat=1.3):
    return parts.get_mat('ch_' + name, parts.vivid(parts.hexc(h), sat=sat))


def _mats():
    return {
        'hull': M('hull', '#b83a18'), 'hull_d': M('hull_d', '#8a2a10'),
        'trim': M('trim', '#f8c050', 1.25), 'flame': M('flame', '#ffb030', 1.2),
        'flame_l': M('flame_l', '#fff0a0', 1.0), 'body': M('hbody', '#e87828'),
        'dark': M('hdark', '#b64a1e', 1.25), 'mane': M('mane', '#ffd860', 1.2),
    }


# Cada pieza lleva una etiqueta: 'front' si, de cara a la camara, tapa al profeta.
_FRONT = []


def _front(ob, cond=True):
    if cond:
        _FRONT.append(ob.name)
    return ob


def build_chariot(m, frame):
    """El carro, mirando a -Y (hacia delante). Caja por PAREDES sueltas (no una caja
    maciza) para poder mandar la pared cercana a la capa `front`."""
    objs = []
    spin = frame * (360.0 / 6.0) / FRAMES          # seis radios: un sexto de vuelta por ciclo
    for s in (-1, 1):
        wx = s * 0.72
        objs.append(tube(f'rim{s}', (wx - 0.06, 0.25, 0.70), (wx + 0.06, 0.25, 0.70), 0.70, 0.70,
                         m['trim'], segs=32, caps=False))
        objs.append(tube(f'tyre{s}', (wx - 0.05, 0.25, 0.70), (wx + 0.05, 0.25, 0.70), 0.62, 0.62,
                         m['hull_d'], segs=32, caps=False))
        for k in range(6):
            a = math.radians(spin + k * 60.0)
            p1 = (wx, 0.25 + math.cos(a) * 0.62, 0.70 + math.sin(a) * 0.62)
            objs.append(tube(f'spoke{s}{k}', (wx, 0.25, 0.70), p1, 0.045, 0.045, m['trim'], segs=8))
        objs.append(sphere(f'hub{s}', (wx + s * 0.06, 0.25, 0.70), 0.16, m['hull']))
    objs.append(box('floor', (0, 0.25, FLOOR_Z - 0.04), (1.20, 1.20, 0.10), m['hull_d'], round=0.3))
    for s in (-1, 1):
        objs.append(box(f'side{s}', (s * 0.58, 0.30, FLOOR_Z + 0.28), (0.10, 1.10, 0.56), m['hull'],
                        round=0.5, taper=(1.0, 0.9)))
        objs.append(box(f'siderim{s}', (s * 0.58, 0.30, FLOOR_Z + 0.58), (0.14, 1.12, 0.09), m['trim'], round=0.6))
    peto = box('peto', (0, -0.36, FLOOR_Z + 0.40), (1.24, 0.14, 0.78), m['hull'], round=0.5, taper=(0.85, 1.0))
    parts.rotate_about(peto, (0, -0.36, FLOOR_Z), 'X', -12.0)
    objs.append(peto)
    objs.append(box('rib', (0, -0.44, FLOOR_Z + 0.80), (1.26, 0.16, 0.12), m['trim'], round=0.6))
    objs.append(box('rear', (0, 0.84, FLOOR_Z + 0.10), (1.18, 0.10, 0.16), m['trim'], round=0.6))
    objs.append(tube('pole', (0, -0.45, 0.95), (0, -1.70, 1.10), 0.07, 0.05, m['trim'], segs=10))
    objs.append(tube('yoke', (-0.75, -1.70, 1.15), (0.75, -1.70, 1.15), 0.06, 0.06, m['trim'], segs=10))
    # llamas: la caja arde hacia ATRAS (+Y) y laten con el fotograma
    beat = (1.0, 1.18, 0.92, 1.10)[frame % 4]
    for i, (dx, dy, dz, hh) in enumerate(((-0.45, 0.70, 1.55, 0.55), (0.0, 0.78, 1.62, 0.72),
                                          (0.45, 0.70, 1.52, 0.50), (-0.72, 0.80, 1.00, 0.42),
                                          (0.72, 0.80, 1.00, 0.42))):
        h = hh * (beat if i % 2 == 0 else 2.1 - beat)
        fl = box(f'fl{i}', (dx, dy, dz), (0.16, 0.18, h), m['flame'] if i % 2 else m['flame_l'],
                 round=0.7, taper=(0.2, 0.3))
        parts.rotate_about(fl, (dx, dy, dz - h * 0.5), 'X', 38.0)
        objs.append(fl)
    return objs


def build_horse(m, tag, at, frame):
    """Caballo de fuego (cine.caballo_fuego) con las patas en la fase `frame` del galope."""
    ax, ay, az = at
    objs = []
    body, dark, mane = m['body'], m['dark'], m['mane']
    objs.append(box(f'{tag}_b', (ax, ay + 0.15, az + 1.18), (0.58, 1.70, 0.60), body, round=0.65, taper=(0.9, 0.85)))
    objs.append(sphere(f'{tag}_ch', (ax, ay - 0.62, az + 1.22), 0.40, body))
    bob = (0.0, 0.05, 0.0, -0.05)[frame % 4]
    objs.append(tube(f'{tag}_n', (ax, ay - 0.55, az + 1.35), (ax, ay - 1.15, az + 2.00 + bob), 0.26, 0.17, body, segs=14))
    hd = box(f'{tag}_h', (ax, ay - 1.38, az + 2.00 + bob), (0.22, 0.55, 0.26), dark, round=0.6, taper=(0.8, 0.85))
    parts.rotate_about(hd, (ax, ay - 1.15, az + 2.02 + bob), 'X', 30.0)
    objs.append(hd)
    objs.append(tube(f'{tag}_mn', (ax, ay - 0.42, az + 1.60), (ax, ay - 1.00, az + 2.16 + bob), 0.13, 0.07, mane, segs=12))
    lick = (1.0, 1.2, 0.9, 1.1)[frame % 4]
    for i, (dy, dz, hh, ang) in enumerate(((-0.15, 2.06, 0.48, 40.0), (0.22, 1.80, 0.42, 48.0), (0.58, 1.58, 0.36, 56.0))):
        h = hh * (lick if i % 2 == 0 else 2.1 - lick)
        fl = box(f'{tag}_fl{i}', (ax, ay + dy, az + dz), (0.10, 0.13, h), mane, round=0.7, taper=(0.15, 0.25))
        parts.rotate_about(fl, (ax, ay + dy, az + dz - h * 0.5), 'X', ang)
        objs.append(fl)
    # patas: fase del galope (delanteras y traseras en contrafase), angulo desde la vertical
    swing_f = (-38.0, -8.0, 26.0, -8.0)[frame % 4]
    swing_b = (24.0, 40.0, -10.0, 8.0)[frame % 4]
    for sg in (-1, 1):
        off = 10.0 * sg
        for kind, (hy, hz, ang, ln) in (('f', (-0.55, 0.98, swing_f + off, 0.78)), ('b', (0.72, 0.90, swing_b - off, 0.82))):
            a = math.radians(ang)
            hip = (ax + sg * 0.20, ay + hy, az + hz)
            hoof = (hip[0], hip[1] + math.sin(a) * ln, hip[2] - math.cos(a) * ln)
            objs.append(tube(f'{tag}_l{kind}{sg}', hip, hoof, 0.15, 0.08, dark, segs=10))
    tail = box(f'{tag}_t', (ax, ay + 1.10, az + 1.42), (0.13, 0.16, 0.62 * lick), mane, round=0.7, taper=(0.2, 0.3))
    parts.rotate_about(tail, (ax, ay + 1.05, az + 1.15), 'X', -52.0)
    objs.append(tail)
    return objs


def build_group(frame):
    """Carro + tiro de dos caballos, mirando a -Y, con el ANCLA (suelo de la caja) en el
    origen y todo escalado a SCALE. Rellena _FRONT con las piezas del carro que quedan a
    cada lado; la eleccion de cual TAPA al profeta depende de la direccion (en `render`)."""
    parts.clear_scene()
    m = _mats()
    objs = build_chariot(m, frame)
    # la yunta, lado a lado delante del yugo; el de la derecha adelantado un palmo (la
    # leccion de la yunta del cap. 5: dos iguales a la par se leen como uno)
    # (el tiro va PEGADO al yugo: con la lanza de la estampa, 2,4 m, los caballos salian
    # sueltos, lejos del carro, en el sprite)
    horses = build_horse(m, 'ha', (-0.42, -2.35, 0.10), frame) + \
        build_horse(m, 'hb', (0.42, -2.50, 0.14), (frame + 1) % 4)
    for ob in objs + horses:
        for v in ob.data.vertices:
            v.co = (v.co - Vector((0.0, 0.25, FLOOR_Z))) * SCALE
    return objs, horses


def _cam_for(direction):
    """Camara de las hojas (orto, 30 grados, luz de estudio) con el ancla en su pixel."""
    sc = bpy.context.scene
    cam = render.setup_scene(render.__dict__.get('_TMP', '/tmp/mantle_sprites'))
    sc.render.resolution_x = CELL * SS
    sc.render.resolution_y = CELL * SS
    e = math.radians(render.CAM_ELEV_DEG)
    fwd = Vector((0.0, math.cos(e), -math.sin(e)))
    right = Vector((1.0, 0.0, 0.0))
    up = Vector((0.0, math.sin(e), math.cos(e)))
    ortho = CELL / PX_PER_M
    ax, ay = ANCHOR[direction]
    s = ortho / CELL
    uc = 0.0 - (ax - CELL * 0.5) * s
    vc = 0.0 + (ay - CELL * 0.5) * s
    cam.location = right * uc + up * vc - fwd * 12.0
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    cam.data.ortho_scale = ortho
    cam.data.shift_x = cam.data.shift_y = 0.0
    sc.display.shading.light = 'STUDIO'
    return cam


def _glow(rgb, a, radius=6, strength=0.55):
    """Halo calido: desenfoca el brillo de lo que es fuego (rojo alto) y su alfa (vfx.py)."""
    hot = np.clip((rgb[..., 0] - 0.55) * 2.2, 0.0, 1.0) * a
    k = np.ones(2 * radius + 1, dtype=np.float32)
    k /= k.sum()
    blur = hot.copy()
    for axis in (0, 1):
        blur = np.apply_along_axis(lambda r: np.convolve(r, k, mode='same'), axis, blur)
    halo = np.clip(blur * strength, 0.0, 1.0)
    col = np.array((1.0, 0.62, 0.22), dtype=np.float32)
    out_a = np.clip(a + halo * (1.0 - a), 0.0, 1.0)
    w = np.where(out_a > 1e-4, a / np.maximum(out_a, 1e-4), 0.0)[..., None]
    out_rgb = rgb * w + col * (1.0 - w)
    return out_rgb, out_a


def _render_layer(front_only, keep):
    for o in bpy.data.objects:
        if o.type == 'MESH':
            o.hide_render = front_only and (o.name not in keep)
    hi = render._render_to_array()
    fr = render.post_soft(hi, CELL, SS, outline=True)
    if not front_only:
        rgb, a = _glow(fr[..., :3], fr[..., 3])
        fr = np.concatenate([rgb, a[..., None]], axis=-1)
    return fr


def main(sprites_dir, tmp_dir, log=print):
    render._TMP = tmp_dir
    sheet = np.zeros((ROWS * CELL, COLS * CELL, 4), dtype=np.float32)
    for row, (dname, yaw) in enumerate(DIRS):
        for f in range(FRAMES):
            chariot, horses = build_group(f)
            for ob in chariot + horses:
                parts.rotate_about(ob, (0, 0, 0), 'Z', yaw)
            bpy.context.view_layer.update()
            # Lo que TAPA al profeta: piezas del carro mas cerca de la camara (y menor) que
            # el ancla y que suben por encima del suelo de la caja
            keep = set()
            for ob in chariot:
                ws = [ob.matrix_world @ v.co for v in ob.data.vertices]
                cy = sum(p.y for p in ws) / len(ws)
                top = max(p.z for p in ws)
                if cy < -0.18 and top > 0.05 and not ob.name.startswith('fl'):
                    keep.add(ob.name)
            _cam_for(dname)
            back = _render_layer(False, keep)
            front = _render_layer(True, keep)
            for col, fr in ((f, back), (FRAMES + f, front)):
                sheet[row * CELL:(row + 1) * CELL, col * CELL:(col + 1) * CELL] = fr
            solid = back[..., 3] > 0.5
            edges = ''.join(t for t, hit in (('T', solid[0].any()), ('B', solid[-1].any()),
                                             ('L', solid[:, 0].any()), ('R', solid[:, -1].any())) if hit)
            log(f'  {dname} f{f}: front {sorted(keep)}{"  RECORTADO " + edges if edges else ""}')
    out = os.path.join(sprites_dir, 'chariot.png')
    render.save_png(sheet, out, 'mantle_chariot')
    log(f'chariot -> {out}')
    return out
