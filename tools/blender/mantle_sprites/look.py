"""Luz, materiales, atmosfera y decorado de los paneles cinematicos (EEVEE).

Los paneles se montaban con el mismo Workbench que las hojas de sprites: luz de estudio
plana, un color liso por material, sin sombras que se proyecten, sin distancia. A 128 px
eso es una figurita; a 1600x720 es una maqueta de plastico. Lo que separa una maqueta de
una ilustracion son cuatro cosas, y las cuatro viven aqui:

  1. LUZ: un sol calido con sombras suaves y un contraluz frio, ambiente del color del
     cielo, oclusion ambiental. Las llamas EMITEN y ademas ILUMINAN (una luz puntual
     por antorcha, se detectan por material): una antorcha que no alumbra la pared es
     una piruleta naranja.
  2. MATERIA: cada material pasa a nodos con un ruido de valor a dos escalas (grano fino
     + manchas): tierra y roca con fuerza, tela poco, piel casi nada; metal con brillo,
     agua espejo. Un color liso no tiene escala; el grano la da.
  3. AIRE: niebla exponencial por distancia, del color del cielo que el motor pinta detras
     (tabla SKY, copiada de ui/cinematic.cpp): lo lejano se funde con el fondo en vez de
     recortarse con el mismo contraste que lo cercano.
  4. SUELO: piedras, guijarros y matas repartidos por hash sobre el suelo, evitando actores,
     props y cauces (los registra cine.py al montar). Un llano de un solo color es una mesa.

Se aplica DESPUES de montar la escena (`apply`), asi que los constructores de cine.py no
saben nada de esto: siguen pidiendo colores lisos con `M()`.
"""
import math

import bpy
import numpy as np
from mathutils import Vector

from . import parts

# Cielo del motor por panel (ui/cinematic.cpp): (arriba, abajo) en sRGB 0..255.
SKY = {
    1: ((28, 24, 52), (122, 92, 104)), 2: ((20, 16, 22), (52, 42, 44)),
    3: ((58, 34, 74), (150, 118, 132)), 4: ((12, 10, 26), (40, 32, 52)),
    5: ((26, 24, 46), (96, 92, 124)), 6: ((22, 17, 22), (58, 44, 42)),
    7: ((84, 74, 96), (186, 150, 108)), 8: ((96, 32, 26), (224, 118, 44)),
    11: ((84, 74, 96), (186, 150, 108)), 12: ((64, 38, 72), (178, 122, 106)),
    13: ((148, 138, 122), (214, 178, 128)), 14: ((10, 12, 30), (46, 52, 86)),
    15: ((96, 90, 120), (212, 168, 100)), 16: ((40, 52, 44), (124, 142, 104)),
    17: ((70, 72, 84), (160, 158, 150)), 18: ((24, 20, 18), (56, 46, 38)),
    19: ((90, 120, 160), (192, 205, 198)), 20: ((16, 14, 36), (90, 54, 42)),
    21: ((84, 80, 118), (210, 170, 116)), 22: ((70, 18, 20), (235, 140, 52)),
    23: ((30, 20, 48), (190, 96, 44)), 24: ((70, 70, 96), (188, 168, 140)),
}

# Rigs de luz. sun/fill = (acimut, elevacion, potencia, color): acimut 0 = detras de la
# camara (desde -Y), positivo hacia +X. amb = fuerza del ambiente (color del cielo).
# fog = (distancia D de la niebla exponencial, opacidad maxima) o None en interiores.
KINDS = {
    'day':      dict(sun=(-38, 52, 4.2, (1.00, 0.95, 0.86)), fill=(150, 28, 0.9, (0.72, 0.82, 1.00)),
                     amb=1.10, fog=(110.0, 0.70)),
    'dusk':     dict(sun=(-58, 24, 3.0, (1.00, 0.74, 0.52)), fill=(140, 32, 0.8, (0.55, 0.60, 0.95)),
                     amb=0.95, fog=(70.0, 0.75)),
    'sunset':   dict(sun=(18, 14, 3.4, (1.00, 0.62, 0.36)), fill=(-150, 36, 0.6, (0.62, 0.45, 0.72)),
                     amb=0.85, fog=(60.0, 0.85)),
    'overcast': dict(sun=(-30, 58, 2.2, (0.92, 0.94, 1.00)), fill=(150, 30, 1.1, (0.80, 0.85, 0.95)),
                     amb=1.35, fog=(50.0, 0.80)),
    # Noche: luna, no cueva. Con sol 1.0 y ambiente 0.6 la estampa 14 era una mancha
    # azul marino con una bolita naranja; una cinematica tiene que LEERSE.
    'night':    dict(sun=(-40, 42, 1.9, (0.58, 0.68, 1.00)), fill=(140, 26, 0.5, (0.42, 0.48, 0.85)),
                     amb=0.90, fog=(60.0, 0.70)),
    'fire':     dict(sun=(-30, 36, 2.6, (1.00, 0.70, 0.44)), fill=(150, 30, 0.9, (0.50, 0.35, 0.60)),
                     amb=0.85, fog=(75.0, 0.60)),
    'interior': dict(sun=(-25, 46, 1.8, (1.00, 0.86, 0.70)), fill=(150, 30, 1.0, (0.60, 0.60, 0.80)),
                     amb=0.95, fog=None),
    'interior_night': dict(sun=(-25, 46, 0.8, (0.62, 0.62, 0.92)), fill=(150, 30, 0.6, (0.50, 0.50, 0.72)),
                     amb=0.55, fog=None),
}

# Por panel: tipo de luz y decorado del suelo ('field' = piedras + matas, 'rock' = solo
# piedras, None = interiores y ciudad, donde un guijarro en el enlosado es basura).
LOOK = {
    1: dict(kind='dusk', dress='field', tuft='#8a7a48'),
    2: dict(kind='interior', dress=None),
    3: dict(kind='dusk', dress=None),
    4: dict(kind='night', dress='rock'),
    5: dict(kind='dusk', dress='rock'),
    6: dict(kind='interior', dress=None),
    7: dict(kind='day', dress='field', tuft='#7f9a4e'),
    # niebla corta: la sierra del titulo tiene que ser SILUETA contra el ocaso, no una
    # banda del color del cielo
    8: dict(kind='sunset', dress=None, fog=(160.0, 0.40)),
    11: dict(kind='day', dress='field', tuft='#7f9a4e'),
    12: dict(kind='dusk', dress='rock'),
    13: dict(kind='day', dress='rock', fog=(150.0, 0.50)),   # el mar tiene que seguir azul
    14: dict(kind='night', dress='rock'),
    15: dict(kind='day', dress='field', tuft='#b8a04a'),
    16: dict(kind='dusk', dress=None, fog=(170.0, 0.40)),   # el cielo verdoso lo tenia todo
    17: dict(kind='overcast', dress='rock'),
    18: dict(kind='interior_night', dress=None),
    19: dict(kind='day', dress='field', tuft='#6f9a5a'),
    20: dict(kind='night', dress=None),
    21: dict(kind='day', dress='field', tuft='#7f9a4e'),
    22: dict(kind='fire', dress='rock'),
    23: dict(kind='fire', dress='rock'),
    24: dict(kind='dusk', dress='field', tuft='#7f9a4e'),
}

# Que material es que cosa, por subcadena del nombre (cine_<key>, <personaje>_<key>,
# claves de tiles.mats()). Lo que no casa con nada es 'prop' (madera, ceramica...).
EMISSIVE = ('fire', 'flame', 'light', 'coals', 'windowlight', 'fmane', 'fhorse', 'dfire')
METAL = ('armor', 'crown', 'helmet', 'steel', 'chain', 'gold', 'shield_boss', 'guard', 'gem',
         'ruby', 'diadem', 'thg_', 'manto_gold', 'altw_')
WATER = ('jwater', 'csea', '_sea', 'water', 'wallo_', 'wallskirt', 'wet', 'foam')
SKIN = ('skin', 'mouth', 'belly')
HAIR = ('hair', 'beard', 'brow', 'fur', 'mane', 'plume')
CLOTH = ('tunic', 'robe', 'sleeves', 'mantle', 'cape', 'hood', 'sash', 'headcloth', 'headdress',
         'bandage', 'cord', 'quiver', 'banner', 'boycloth', 'bedc', 'bedpillow', 'manto_', 'tent',
         'thc_', 'cloth', 'legs')
TERRAIN = ('ground', 'hill', 'spire', 'rock', 'road', 'cliff', 'cave', 'crag', 'mesa', 'stone',
           'jbank', 'sand', 'earth', 'mortar', 'slab', 'adobe', 'wbase', 'wdeb', 'tower', 'merlon',
           'gate_', 'lintel', 'col_', 'altar_', 'town_sil', 'aldea_sil', 'field', 'dplain',
           'dparapet', 'dmerlon', 'jtower', 'jmerlon', 'jlintel', 'slintel', 'wall', 'clay',
           'grass', 'meadow', 'moss', 'idol', 'smoke', 'cloud')


def _kind_of(name):
    n = name.lower()
    for group, keys in (('emissive', EMISSIVE), ('metal', METAL), ('water', WATER),
                        ('skin', SKIN), ('hair', HAIR), ('cloth', CLOTH), ('terrain', TERRAIN)):
        if any(k in n for k in keys):
            return group
    return 'prop'


# (ruido fino: escala, amplitud), (ruido grueso: escala, amplitud), rugosidad, metalico, spec
SURFACE = {
    'terrain': ((6.0, 0.10), (0.45, 0.14), 0.90, 0.0, 0.25),
    'prop':    ((9.0, 0.07), (1.20, 0.06), 0.75, 0.0, 0.35),
    'cloth':   ((14.0, 0.06), (2.50, 0.05), 0.90, 0.0, 0.20),
    'skin':    ((12.0, 0.025), (3.0, 0.02), 0.55, 0.0, 0.45),
    'hair':    ((18.0, 0.09), (4.0, 0.05), 0.70, 0.0, 0.30),
    'metal':   ((10.0, 0.05), (2.0, 0.04), 0.32, 0.75, 0.80),
    'water':   ((5.0, 0.04), (0.8, 0.05), 0.12, 0.0, 0.90),
    'emissive': ((8.0, 0.05), (2.0, 0.04), 0.60, 0.0, 0.30),
}

_STEP_LOG = []


def srgb(rgb255):
    return tuple(parts.srgb_to_linear(c / 255.0) for c in rgb255)


# =============================================================================
# Escena y luces
# =============================================================================
def setup_eevee(sc):
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.film_transparent = True
    sc.render.filter_size = 1.2
    ee = sc.eevee
    ee.taa_render_samples = 32
    ee.use_gtao = True
    ee.gtao_distance = 0.9
    ee.gtao_factor = 1.0
    ee.use_soft_shadows = True
    ee.shadow_cube_size = '1024'
    ee.shadow_cascade_size = '2048'
    ee.use_shadow_high_bitdepth = True
    # Bloom: sobre pelicula transparente no escribe alfa (leccion de vfx.py), pero las
    # antorchas van casi siempre DELANTE de algo opaco (muro, suelo) y ahi si luce.
    ee.use_bloom = True
    ee.bloom_threshold = 1.1
    ee.bloom_intensity = 0.08
    ee.bloom_radius = 5.5
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'


def _sun_rotation(az_deg, el_deg):
    az, el = math.radians(az_deg), math.radians(el_deg)
    p = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    return (-p).to_track_quat('-Z', 'Y').to_euler()


def _light(name, kind, energy, color, rotation=None, location=None, **kw):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    ld.color = color
    for k, v in kw.items():
        setattr(ld, k, v)
    ob = bpy.data.objects.new(name, ld)
    if rotation is not None:
        ob.rotation_euler = rotation
    if location is not None:
        ob.location = location
    bpy.context.scene.collection.objects.link(ob)
    return ob


def lights(idx, rig):
    for ob in list(bpy.data.objects):
        if ob.type == 'LIGHT':
            bpy.data.objects.remove(ob, do_unlink=True)
    az, el, en, col = rig['sun']
    sun = _light('look_sun', 'SUN', en, col, rotation=_sun_rotation(az, el),
                 angle=math.radians(3.5), use_contact_shadow=True)
    sun.data.shadow_cascade_max_distance = 70.0
    sun.data.shadow_cascade_count = 4
    sun.data.shadow_cascade_fade = 0.25
    az, el, en, col = rig['fill']
    _light('look_fill', 'SUN', en, col, rotation=_sun_rotation(az, el), angle=math.radians(12.0),
           use_shadow=False)
    sc = bpy.context.scene
    world = sc.world or bpy.data.worlds.new('World')
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    top, bot = SKY.get(idx, ((60, 60, 80), (150, 140, 130)))
    amb = tuple(0.55 * a + 0.45 * b for a, b in zip(srgb(top), srgb(bot)))
    bg.inputs['Color'].default_value = (*amb, 1.0)
    bg.inputs['Strength'].default_value = rig['amb']


def torch_lights(max_lights=14):
    """Una luz puntual por objeto de material emisivo: las llamas alumbran lo que tienen
    al lado. Se agrupan por proximidad (la llama de un cayado son tres piezas)."""
    # Sin esto, matrix_world de las piezas colgadas de un hueso es la de REPOSO (o la
    # identidad): la luz de la llama del cayado caia en el origen de la escena, que en
    # casi todas las estampas es un punto del suelo al pie de la camara — un charco de
    # luz naranja en mitad de la nada (12, 17, 21, 23, 24).
    bpy.context.view_layer.update()
    spots = []
    for ob in bpy.data.objects:
        if ob.type != 'MESH' or not ob.data.materials or not len(ob.data.vertices):
            continue
        if not any(m and _kind_of(m.name) == 'emissive' for m in ob.data.materials):
            continue
        if not ob.visible_get():
            continue
        bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
        c = sum(bb, Vector()) / 8.0
        size = max((max(v[i] for v in bb) - min(v[i] for v in bb)) for i in range(3))
        col = ob.data.materials[0].diffuse_color[:3]
        if any((c - s[0]).length < max(0.6, size) for s in spots):
            continue
        spots.append((c, size, col))
    spots.sort(key=lambda s: -s[1])
    for i, (c, size, col) in enumerate(spots[:max_lights]):
        warm = (min(1.0, col[0] * 1.2 + 0.2), col[1] * 0.9 + 0.1, col[2] * 0.6)
        energy = max(45.0, 110.0 * size ** 1.5)
        _light(f'look_torch{i}', 'POINT', energy, warm, location=c + Vector((0, 0, size * 0.1)),
               shadow_soft_size=max(0.25, size * 0.4), use_shadow=True)


# =============================================================================
# Materiales
# =============================================================================
def _noise_value(nt, coord, fine, coarse):
    """Nodo que devuelve 1 +- (grano fino + manchas gruesas), para el Value de un HSV."""
    nodes, links = nt.nodes, nt.links
    total = None
    for scale, amp in (fine, coarse):
        if amp <= 0.0:
            continue
        nz = nodes.new('ShaderNodeTexNoise')
        nz.inputs['Scale'].default_value = scale
        nz.inputs['Detail'].default_value = 5.0
        nz.inputs['Roughness'].default_value = 0.62
        links.new(coord, nz.inputs['Vector'])
        mr = nodes.new('ShaderNodeMapRange')
        mr.inputs['From Min'].default_value = 0.35
        mr.inputs['From Max'].default_value = 0.65
        mr.inputs['To Min'].default_value = -amp
        mr.inputs['To Max'].default_value = amp
        links.new(nz.outputs['Fac'], mr.inputs['Value'])
        if total is None:
            total = mr.outputs['Result']
        else:
            add = nodes.new('ShaderNodeMath')
            add.operation = 'ADD'
            links.new(total, add.inputs[0])
            links.new(mr.outputs['Result'], add.inputs[1])
            total = add.outputs['Value']
    one = nodes.new('ShaderNodeMath')
    one.operation = 'ADD'
    one.inputs[1].default_value = 1.0
    if total is not None:
        links.new(total, one.inputs[0])
    else:
        one.inputs[0].default_value = 0.0
    return one.outputs['Value']


def _fog_factor(nt, D, fmax):
    """1 - exp(-z/D), acotado a fmax: la niebla exponencial de toda la vida."""
    nodes, links = nt.nodes, nt.links
    cam = nodes.new('ShaderNodeCameraData')
    div = nodes.new('ShaderNodeMath')
    div.operation = 'DIVIDE'
    div.inputs[1].default_value = -D
    links.new(cam.outputs['View Z Depth'], div.inputs[0])
    ex = nodes.new('ShaderNodeMath')
    ex.operation = 'EXPONENT'
    links.new(div.outputs['Value'], ex.inputs[0])
    sub = nodes.new('ShaderNodeMath')
    sub.operation = 'SUBTRACT'
    sub.inputs[0].default_value = 1.0
    links.new(ex.outputs['Value'], sub.inputs[1])
    mul = nodes.new('ShaderNodeMath')
    mul.operation = 'MULTIPLY'
    mul.inputs[1].default_value = fmax
    links.new(sub.outputs['Value'], mul.inputs[0])
    return mul.outputs['Value']


def materialize(mat, fog, unlit=False):
    """Convierte un material liso (diffuse_color) en su version con nodos."""
    base = tuple(mat.diffuse_color[:3])
    kind = _kind_of(mat.name)
    fine, coarse, rough, metal, spec = SURFACE[kind]
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    nodes, links = nt.nodes, nt.links
    out = nodes.new('ShaderNodeOutputMaterial')
    tc = nodes.new('ShaderNodeTexCoord')
    coord = tc.outputs['Object']
    val = _noise_value(nt, coord, fine, coarse)
    hsv = nodes.new('ShaderNodeHueSaturation')
    hsv.inputs['Color'].default_value = (*base, 1.0)
    links.new(val, hsv.inputs['Value'])
    if unlit:
        # Las siluetas lejanas se pintaron para la luz plana de Workbench + gain 1.7:
        # sin luz y a 1.0 salian apagadas. Y menos niebla que lo iluminado: a esa
        # distancia la formula las fundia del todo con el cielo.
        shader = nodes.new('ShaderNodeEmission')
        links.new(hsv.outputs['Color'], shader.inputs['Color'])
        shader.inputs['Strength'].default_value = 1.35
        if fog is not None:
            fog = (fog[0], fog[1] * 0.7, fog[2])
    else:
        shader = nodes.new('ShaderNodeBsdfPrincipled')
        links.new(hsv.outputs['Color'], shader.inputs['Base Color'])
        shader.inputs['Roughness'].default_value = rough
        shader.inputs['Metallic'].default_value = metal
        if 'Specular IOR Level' in shader.inputs:
            shader.inputs['Specular IOR Level'].default_value = spec
        if kind == 'emissive':
            # Con 5.0 la llama del cayado era un disco amarillo recortado: la emision
            # satura y pierde el color. Justo por encima del umbral del bloom.
            links.new(hsv.outputs['Color'], shader.inputs['Emission Color'])
            core = any(k in mat.name.lower() for k in ('core', 'halo', 'fire_lt'))
            strength = 2.6 if core else 1.8
            if 'windowlight' in mat.name.lower():
                strength = 1.25     # a 1.8 la ventana era un rectangulo blanco recortado
            shader.inputs['Emission Strength'].default_value = strength
    if fog is not None:
        D, fmax, fcol = fog
        fogc = nodes.new('ShaderNodeEmission')
        fogc.inputs['Color'].default_value = (*fcol, 1.0)
        fogc.inputs['Strength'].default_value = 1.0
        mix = nodes.new('ShaderNodeMixShader')
        links.new(_fog_factor(nt, D, fmax), mix.inputs['Fac'])
        links.new(shader.outputs[0], mix.inputs[1])
        links.new(fogc.outputs['Emission'], mix.inputs[2])
        links.new(mix.outputs['Shader'], out.inputs['Surface'])
    else:
        links.new(shader.outputs[0], out.inputs['Surface'])


def materialize_all(fog, flat_layer_names):
    """Todos los materiales en uso. Los objetos de las capas planas (siluetas lejanas)
    reciben una COPIA sin luz: la niebla ya hace de sombreado."""
    used = set()
    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue
        flat = any(c.name in flat_layer_names for c in ob.users_collection)
        for slot in ob.material_slots:
            m = slot.material
            if m is None:
                continue
            if flat:
                nm = m.name + '_unlit'
                um = bpy.data.materials.get(nm)
                if um is None:
                    um = m.copy()
                    um.name = nm
                    um.use_nodes = False
                    materialize(um, fog, unlit=True)
                slot.material = um
            else:
                used.add(m.name)
    for name in used:
        m = bpy.data.materials.get(name)
        if m is not None and not m.use_nodes:
            materialize(m, fog)


# =============================================================================
# Decorado de suelo
# =============================================================================
def _h(k, salt=0.0):
    """Hash determinista en [0, 1)."""
    x = math.sin(k * 12.9898 + salt * 78.233) * 43758.5453
    return x - math.floor(x)


def _dist_to_strip(x, y, pts, half):
    best = 1e9
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        dx, dy = x1 - x0, y1 - y0
        L2 = dx * dx + dy * dy or 1e-9
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / L2))
        px, py = x0 + dx * t, y0 + dy * t
        best = min(best, math.hypot(x - px, y - py) - half)
    return best


def dress(idx, reg, frame_at, layer_put):
    """Piedras y matas sobre el suelo registrado, esquivando actores, props y cauces."""
    cfg = LOOK.get(idx, {})
    mode = cfg.get('dress')
    g = reg.get('ground')
    if not mode or g is None:
        return 0
    half_w, near, far, z0, col = g
    cam_y = reg.get('cam_y', -12.0)
    y0, y1 = cam_y + 0.8, far - 0.3
    if y1 <= y0:
        return 0
    base = parts.hexc(col)
    stone_dk = parts.get_mat('look_pebble_dk', tuple(c * 0.68 for c in base))
    stone_lt = parts.get_mat('look_pebble_lt', tuple(min(1.0, c * 1.18 + 0.03) for c in base))
    stone_md = parts.get_mat('look_pebble_md', tuple(c * 0.85 for c in base))
    tuft_mat = parts.get_mat('look_tuft', parts.vivid(parts.hexc(cfg.get('tuft', '#7f9a4e')), 1.15, 1.0))
    blocked = [(x, y, r) for (x, y, r) in reg.get('actors', [])]
    strips = reg.get('strips', [])

    def free(x, y, r):
        if any(math.hypot(x - bx, y - by) < br + r for bx, by, br in blocked):
            return False
        if any(_dist_to_strip(x, y, pts, half) < r + 0.25 for pts, half in strips):
            return False
        return True

    made = 0
    n_pebbles = 70 if mode == 'field' else 55
    n_tufts = 22 if mode == 'field' else 0
    n_rocks = 8
    k = idx * 1000.0
    for i in range(n_pebbles + n_rocks + n_tufts):
        # y con mas peso cerca de la camara (perspectiva: el fondo ya tiene poco sitio)
        t = _h(k + i, 1.0) ** 1.6
        y = y0 + (y1 - y0) * t
        x0, x1, _, _ = frame_at(y, 1.05)
        x = x0 + (x1 - x0) * _h(k + i, 2.0)
        if abs(x) > half_w:
            continue
        if i < n_pebbles:
            r = 0.05 + 0.12 * _h(k + i, 3.0) ** 2
            if not free(x, y, r + 0.15):
                continue
            mat = (stone_dk, stone_lt, stone_md)[i % 3]
            ob = parts.rock(f'look_pebble{i}', (x, y, z0 + r * 0.25), r, mat,
                            scale=(1.0 + 0.4 * _h(k + i, 4.0), 1.0, 0.55 + 0.25 * _h(k + i, 5.0)),
                            seed=i % 23, rough=0.18, segs=24, rings=16)
            parts.rotate_about(ob, (x, y, z0), 'Z', 360.0 * _h(k + i, 6.0))
        elif i < n_pebbles + n_rocks:
            r = 0.22 + 0.32 * _h(k + i, 3.0)
            y = y0 + (y1 - y0) * (0.35 + 0.65 * _h(k + i, 1.0))   # los penascos, lejos del borde
            x0, x1, _, _ = frame_at(y, 1.05)
            x = x0 + (x1 - x0) * _h(k + i, 2.0)
            if abs(x) > half_w or not free(x, y, r + 0.6):
                continue
            ob = parts.rock(f'look_rock{i}', (x, y, z0 + r * 0.15), r, (stone_md, stone_dk)[i % 2],
                            scale=(1.3, 1.0, 0.62), seed=(i * 7) % 23, rough=0.16, segs=36, rings=24)
            parts.rotate_about(ob, (x, y, z0), 'Z', 360.0 * _h(k + i, 6.0))
            blocked.append((x, y, r * 1.3))
        else:
            if not free(x, y, 0.45):
                continue
            n = 4 + int(4 * _h(k + i, 7.0))
            h = 0.28 + 0.35 * _h(k + i, 8.0)
            for j in range(n):
                a = j * 2.399 + _h(k + i, 9.0) * 6.28
                dx, dy = math.cos(a) * 0.22 * (0.4 + _h(k + i + j, 10.0)), math.sin(a) * 0.12
                hh = h * (0.7 + 0.6 * _h(k + i + j, 11.0))
                ob = parts.box(f'look_tuft{i}_{j}', (x + dx, y + dy, z0 + hh * 0.5),
                               (0.05, 0.03, hh), tuft_mat, round=0.7, taper=(0.12, 0.3))
                c = 0.4 * hh * (1 if j % 2 else -1)
                for v in ob.data.vertices:
                    tt = max(0.0, (v.co.z - z0) / hh)
                    v.co.x += c * tt * tt
                parts.rotate_about(ob, (x + dx, y + dy, z0), 'Z', 360.0 * _h(k + i + j, 12.0))
                layer_put(ob)
                made += 1
            continue
        layer_put(ob)
        made += 1
    return made


# =============================================================================
# Post: gradado
# =============================================================================
def grade(fr, contrast=1.10, saturation=1.10):
    """Curva S suave y un punto mas de saturacion. Standard sin gradar es correcto y
    aburrido; esto es lo que hace una ilustracion con la foto del render."""
    rgb = fr[..., :3]
    lum = rgb[..., 0:1] * 0.30 + rgb[..., 1:2] * 0.59 + rgb[..., 2:3] * 0.11
    rgb = lum + (rgb - lum) * saturation
    rgb = 0.5 + (rgb - 0.5) * contrast
    out = fr.copy()
    out[..., :3] = np.clip(rgb, 0.0, 1.0)
    return out


# =============================================================================
# Entrada
# =============================================================================
def apply(idx, flat_layer_names, reg, frame_at, layer_put):
    """Luz + materiales + niebla + antorchas + decorado para el panel `idx` ya montado."""
    cfg = LOOK.get(idx, dict(kind='day', dress=None))
    rig = dict(KINDS[cfg['kind']])
    if 'fog' in cfg:
        rig['fog'] = cfg['fog']
    sc = bpy.context.scene
    setup_eevee(sc)
    n = dress(idx, reg, frame_at, layer_put)
    lights(idx, rig)
    fog = None
    if rig['fog'] is not None:
        top, bot = SKY.get(idx, ((60, 60, 80), (150, 140, 130)))
        fog = (rig['fog'][0], rig['fog'][1], srgb(bot))
    materialize_all(fog, set(flat_layer_names))
    torch_lights()
    return n
