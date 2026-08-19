"""Character builders: shared humanoid (spec-driven), lion (quadruped) and blob."""
import math
from mathutils import Vector, Matrix
from .parts import get_mat, hexc, vivid, box, sphere, tube, attach, attach_obj, set_origin, rotate_about, scale_about
from .rig import build_armature

STAFF_TILT = 25.0   # degrees; must match the forearm.R bend of humanoid_base('swing'/'raise')

# Stylisation ("Clash Royale" figurines): oversized heads + saturated palette
HEAD_SCALE = 1.45        # humanoid head parts scaled about HEAD_PIVOT
HEAD_PIVOT = (0, 0, 1.56)
LION_HEAD_SCALE = 1.30
SAT = 1.35               # material saturation boost (skin uses SKIN_SAT)
SKIN_SAT = 1.15


def _mat(name, key, col, skin=False):
    return get_mat(f'{name}_{key}', vivid(hexc(col), SKIN_SAT if skin else SAT))

# =============================================================================
# Humanoid skeleton (height ~1.75, faces -Y, +X = character's left)
# =============================================================================
def _humanoid_bones():
    B = [
        ('root', (0, 0, 0), (0, 0, 0.25), None),
        ('hips', (0, 0, 0.92), (0, 0, 1.05), 'root'),
        ('spine', (0, 0, 1.05), (0, 0, 1.25), 'hips'),
        ('chest', (0, 0, 1.25), (0, 0, 1.45), 'spine'),
        ('neck', (0, 0, 1.45), (0, 0, 1.50), 'chest'),
        ('head', (0, 0, 1.50), (0, 0, 1.75), 'neck'),
    ]
    for s, x in (('L', 0.22), ('R', -0.22)):
        B += [
            ('upper_arm.' + s, (x, 0, 1.42), (x, 0, 1.16), 'chest'),
            ('forearm.' + s, (x, 0, 1.16), (x, 0, 0.92), 'upper_arm.' + s),
            ('hand.' + s, (x, 0, 0.92), (x, 0, 0.82), 'forearm.' + s),
        ]
    for s, x in (('L', 0.09), ('R', -0.09)):
        B += [
            ('thigh.' + s, (x, 0, 0.92), (x, 0, 0.50), 'hips'),
            ('shin.' + s, (x, 0, 0.50), (x, 0, 0.07), 'thigh.' + s),
            ('foot.' + s, (x, 0, 0.07), (x, -0.13, 0.03), 'shin.' + s),
        ]
    return B


def build_humanoid(name, spec):
    arm = build_armature(name, _humanoid_bones())
    M = lambda key, col: _mat(name, key, col)
    skin = _mat(name, 'skin', spec['skin'], skin=True)
    tunic = M('tunic', spec['tunic'])
    sleeves = M('sleeves', spec['sleeves']) if spec.get('sleeves') else tunic
    legs = M('legs', spec['legs']) if spec.get('legs') else skin
    shoes = M('shoes', spec.get('shoes', '#4a3018'))
    tip_obj = None
    head_parts = []

    def A(ob, bone):
        if bone == 'head':
            head_parts.append(ob)   # scaled up below (big-head stylisation)
        return attach(ob, arm, bone)

    # --- head -----------------------------------------------------------
    A(sphere(f'{name}_head', (0, 0, 1.62), 0.135, skin, scale=(1, 1, 1.1)), 'head')
    A(tube(f'{name}_neck', (0, 0, 1.43), (0, 0, 1.53), 0.05, 0.05, skin), 'neck')
    if spec.get('hair'):
        hair = M('hair', spec['hair'])
        A(sphere(f'{name}_hair', (0, 0.025, 1.68), 0.145, hair, scale=(1.0, 1.05, 0.85)), 'head')
        if spec.get('hair_long'):
            A(box(f'{name}_hair_back', (0, 0.10, 1.50), (0.24, 0.10, 0.28), hair), 'head')
    if spec.get('beard'):
        beard = M('beard', spec['beard'])
        A(box(f'{name}_beard', (0, -0.10, 1.51), (0.17, 0.09, 0.16), beard), 'head')
        if spec.get('beard_long'):
            A(box(f'{name}_beard2', (0, -0.08, 1.40), (0.13, 0.08, 0.14), beard), 'head')
    if spec.get('helmet'):
        helm = M('helmet', spec['helmet'])
        A(sphere(f'{name}_helm', (0, 0, 1.66), 0.155, helm, scale=(1, 1, 0.9)), 'head')
        A(tube(f'{name}_helm_rim', (0, 0, 1.55), (0, 0, 1.60), 0.16, 0.16, helm), 'head')
        if spec.get('plume'):
            plume = M('plume', spec['plume'])
            A(box(f'{name}_plume', (0, 0.02, 1.87), (0.06, 0.26, 0.14), plume), 'head')
    if spec.get('headdress'):
        hd = M('headdress', spec['headdress'])
        A(tube(f'{name}_headdress', (0, 0, 1.70), (0, 0, 1.92), 0.13, 0.09, hd, smooth=False), 'head')
        horn = M('horn', '#e8dcc0')
        A(tube(f'{name}_horn_l', (0.10, 0, 1.80), (0.19, 0, 1.98), 0.03, 0.005, horn), 'head')
        A(tube(f'{name}_horn_r', (-0.10, 0, 1.80), (-0.19, 0, 1.98), 0.03, 0.005, horn), 'head')
    if spec.get('hood'):
        hood = M('hood', spec['hood'])
        A(sphere(f'{name}_hood', (0, 0.02, 1.65), 0.16, hood, scale=(1, 1.05, 1.0)), 'head')

    # --- torso ----------------------------------------------------------
    A(box(f'{name}_torso_lo', (0, 0, 1.15), (0.34, 0.20, 0.22), tunic), 'spine')
    chest_mat = M('armor', spec['armor']) if spec.get('armor') else tunic
    A(box(f'{name}_torso_up', (0, 0, 1.36), (0.40, 0.22, 0.22), chest_mat), 'chest')
    for s, x in (('L', 0.22), ('R', -0.22)):
        A(sphere(f'{name}_shoulder_{s}', (x, 0, 1.42), 0.08, chest_mat), 'chest')
    if spec.get('belt'):
        A(tube(f'{name}_belt', (0, 0, 1.01), (0, 0, 1.08), 0.205, 0.205, M('belt', spec['belt']), smooth=False), 'hips')
    if spec.get('sash'):
        A(box(f'{name}_sash', (0, -0.115, 1.25), (0.09, 0.02, 0.44), M('sash', spec['sash'])), 'spine')
    # tunic skirt / robe
    if spec.get('tunic_len', 'short') == 'long':
        robe = M('robe', spec.get('robe', spec['tunic']))
        A(tube(f'{name}_robe', (0, 0, 1.00), (0, 0, 0.10), 0.19, 0.30, robe, segs=10), 'hips')
    else:
        A(tube(f'{name}_skirt', (0, 0, 1.00), (0, 0, 0.62), 0.19, 0.24, tunic, segs=10), 'hips')
    if spec.get('mantle'):
        mantle = M('mantle', spec['mantle'])
        A(box(f'{name}_mantle', (0, 0.135, 1.18), (0.42, 0.07, 0.56), mantle), 'chest')
        for s, x in (('L', 0.21), ('R', -0.21)):
            A(box(f'{name}_mantle_sh_{s}', (x, 0.02, 1.45), (0.16, 0.22, 0.09), mantle), 'chest')
    if spec.get('cape'):
        cape = M('cape', spec['cape'])
        A(box(f'{name}_cape', (0, 0.15, 1.02), (0.40, 0.05, 0.88), cape), 'chest')

    # --- arms -----------------------------------------------------------
    for s, x in (('L', 0.22), ('R', -0.22)):
        A(tube(f'{name}_uarm_{s}', (x, 0, 1.42), (x, 0, 1.16), 0.058, 0.052, sleeves), 'upper_arm.' + s)
        fmat = M('bracer', spec['bracer']) if spec.get('bracer') else skin
        A(tube(f'{name}_farm_{s}', (x, 0, 1.16), (x, 0, 0.92), 0.05, 0.045, fmat), 'forearm.' + s)
        A(sphere(f'{name}_hand_{s}', (x, 0, 0.88), 0.055, skin), 'hand.' + s)
    if spec.get('bandages'):
        band = M('bandage', '#e8e4d8')
        A(tube(f'{name}_band_head', (0, 0, 1.58), (0, 0, 1.63), 0.14, 0.14, band, smooth=False), 'head')
        A(tube(f'{name}_band_arm', (0.22, 0, 1.10), (0.22, 0, 1.00), 0.056, 0.056, band, smooth=False), 'forearm.L')
        A(tube(f'{name}_band_arm2', (-0.22, 0, 1.36), (-0.22, 0, 1.28), 0.062, 0.062, band, smooth=False), 'upper_arm.R')
        A(tube(f'{name}_band_leg', (-0.09, 0, 0.40), (-0.09, 0, 0.30), 0.065, 0.065, band, smooth=False), 'shin.R')

    # --- legs -----------------------------------------------------------
    for s, x in (('L', 0.09), ('R', -0.09)):
        A(tube(f'{name}_thigh_{s}', (x, 0, 0.92), (x, 0, 0.50), 0.075, 0.065, legs), 'thigh.' + s)
        A(tube(f'{name}_shin_{s}', (x, 0, 0.50), (x, 0, 0.07), 0.06, 0.05, legs), 'shin.' + s)
        A(box(f'{name}_foot_{s}', (x, -0.05, 0.04), (0.11, 0.22, 0.08), shoes), 'foot.' + s)

    # --- weapons & gear ----------------------------------------------------
    w = spec.get('weapon')
    wc = spec.get('weapon_colors', {})
    hx = -0.22  # right hand x
    if w in ('staff_fire', 'staff_light', 'staff_idol'):
        # The staff is authored vertical, then pre-tilted back by STAFF_TILT about the
        # hand so that the base pose (forearm.R bent forward by STAFF_TILT) shows it upright.
        wood = M('wood', wc.get('wood', '#5a3a1a'))
        pivot = (hx, 0.0, 0.88)
        sy = -0.06
        top = spec.get('staff_top', 1.72)
        parts_ = [tube(f'{name}_staff', (hx, sy, 0.22), (hx, sy, top), 0.028, 0.024, wood)]
        tip_center = Vector((hx, sy, top + 0.07))
        if w == 'staff_fire':
            fire = M('fire', wc.get('tip', '#ff8c1a'))
            core = M('fire_core', wc.get('core', '#ffd23f'))
            tip_obj = sphere(f'{name}_tip', tip_center + Vector((0, 0, 0.02)), 0.10, fire, scale=(1, 1, 1.25))
            tip_child = sphere(f'{name}_tip_core', tip_center + Vector((0, 0, 0.05)), 0.05, core, scale=(1, 1, 1.4))
        elif w == 'staff_light':
            light = M('light', wc.get('tip', '#eaf6ff'))
            halo = M('light_halo', wc.get('core', '#8ad0ff'))
            tip_obj = sphere(f'{name}_tip', tip_center + Vector((0, 0, 0.02)), 0.075, light)
            tip_child = tube(f'{name}_tip_halo', tip_center + Vector((0, 0, -0.05)), tip_center + Vector((0, 0, 0.09)), 0.11, 0.11, halo, segs=6, smooth=False)
        else:
            idol = M('idol', wc.get('tip', '#7a1f1f'))
            horn = M('horn', '#e8dcc0')
            tip_obj = box(f'{name}_idol', tip_center + Vector((0, 0, 0.02)), (0.12, 0.10, 0.14), idol)
            tip_child = tube(f'{name}_idol_h1', tip_center + Vector((0.05, 0, 0.08)), tip_center + Vector((0.10, 0, 0.18)), 0.02, 0.004, horn)
            parts_.append(tube(f'{name}_idol_h2', tip_center + Vector((-0.05, 0, 0.08)), tip_center + Vector((-0.10, 0, 0.18)), 0.02, 0.004, horn))
        for ob in parts_ + [tip_obj, tip_child]:
            rotate_about(ob, pivot, 'X', STAFF_TILT)
        rot_center = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(STAFF_TILT), 4, 'X') @ Matrix.Translation(-Vector(pivot)) @ tip_center
        set_origin(tip_obj, rot_center)
        attach_obj(tip_child, tip_obj)
        for ob in parts_ + [tip_obj]:
            A(ob, 'hand.R')
        if w == 'staff_idol':
            tip_obj = None  # idol head does not glow
    elif w == 'sword':
        steel = M('steel', wc.get('blade', '#c8ccd0'))
        guard = M('guard', wc.get('guard', '#8a6a2a'))
        A(box(f'{name}_blade', (hx, -0.07, 1.30), (0.035, 0.075, 0.72), steel), 'hand.R')
        A(box(f'{name}_guard', (hx, -0.07, 0.95), (0.16, 0.04, 0.035), guard), 'hand.R')
    elif w == 'spear':
        wood = M('wood', wc.get('wood', '#5a3a1a'))
        steel = M('steel', wc.get('blade', '#c8ccd0'))
        for ob in (tube(f'{name}_spear', (hx, -0.06, 0.10), (hx, -0.06, 1.50), 0.022, 0.02, wood),
                   tube(f'{name}_spear_tip', (hx, -0.06, 1.50), (hx, -0.06, 1.72), 0.05, 0.004, steel, smooth=False)):
            rotate_about(ob, (hx, 0.0, 0.88), 'X', 20.0)   # compensates base forearm bend
            A(ob, 'hand.R')
    elif w == 'bow':
        wood = M('wood', wc.get('wood', '#5a3a1a'))
        string = M('string', '#e8e0c8')
        lx = 0.22
        for ob in (tube(f'{name}_bow', (lx, -0.08, 0.35), (lx, -0.08, 1.45), 0.024, 0.024, wood),
                   box(f'{name}_bow_string', (lx, 0.0, 0.90), (0.012, 0.012, 1.08), string)):
            rotate_about(ob, (lx, 0.0, 0.88), 'X', 40.0)   # compensates base forearm bend
            A(ob, 'hand.L')
    sh = spec.get('shield')
    if sh:
        shield = M('shield', spec.get('shield_color', '#8b4a2b'))
        boss = M('shield_boss', spec.get('shield_boss', '#c9a24a'))
        if sh == 'round':
            A(tube(f'{name}_shield', (0.28, -0.03, 1.04), (0.28, -0.07, 1.04), 0.18, 0.18, shield, segs=12), 'forearm.L')
            A(sphere(f'{name}_shield_boss', (0.28, -0.08, 1.04), 0.05, boss), 'forearm.L')
        else:
            A(box(f'{name}_shield', (0.30, -0.05, 1.02), (0.34, 0.05, 0.56), shield), 'forearm.L')
            A(box(f'{name}_shield_rim', (0.30, -0.08, 1.02), (0.08, 0.02, 0.50), boss), 'forearm.L')
    if spec.get('quiver'):
        A(box(f'{name}_quiver', (0.13, 0.15, 1.30), (0.10, 0.10, 0.44), M('quiver', '#6b4423')), 'chest')
        A(box(f'{name}_arrows', (0.13, 0.15, 1.56), (0.09, 0.09, 0.10), M('fletch', '#e8e0c8')), 'chest')

    # Big-head stylisation: everything riding the head bone grows about the neck top
    for ob in head_parts:
        scale_about(ob, HEAD_PIVOT, HEAD_SCALE)

    return {'name': name, 'arm': arm, 'kind': 'humanoid',
            'attack': spec.get('attack', 'swing'), 'tip': tip_obj}


# =============================================================================
# Lion (quadruped)
# =============================================================================
def build_lion(name, spec):
    bones = [
        ('root', (0, 0, 0), (0, 0, 0.25), None),
        ('spine_rear', (0, 0.42, 0.62), (0, 0.0, 0.66), 'root'),
        ('spine_front', (0, 0.0, 0.66), (0, -0.45, 0.66), 'spine_rear'),
        ('neck', (0, -0.45, 0.66), (0, -0.62, 0.80), 'spine_front'),
        ('head', (0, -0.62, 0.80), (0, -0.90, 0.80), 'neck'),
        ('tail', (0, 0.42, 0.62), (0, 0.95, 0.50), 'spine_rear'),
    ]
    for tag, x, y in (('FL', 0.16, -0.36), ('FR', -0.16, -0.36), ('RL', 0.16, 0.36), ('RR', -0.16, 0.36)):
        bones += [
            ('upper_' + tag, (x, y, 0.60), (x, y, 0.32), 'spine_front' if tag[0] == 'F' else 'spine_rear'),
            ('lower_' + tag, (x, y, 0.32), (x, y, 0.02), 'upper_' + tag),
        ]
    arm = build_armature(name, bones)
    M = lambda key, col: _mat(name, key, col)
    fur = M('fur', spec['fur'])
    mane = M('mane', spec['mane'])
    belly = M('belly', spec['belly'])
    dark = M('dark', '#3a2414')
    head_parts = []

    def A(ob, b):
        if b == 'head':
            head_parts.append(ob)
        return attach(ob, arm, b)

    A(box(f'{name}_body_r', (0, 0.22, 0.64), (0.40, 0.55, 0.36), fur), 'spine_rear')
    A(box(f'{name}_body_f', (0, -0.22, 0.66), (0.44, 0.50, 0.40), fur), 'spine_front')
    A(box(f'{name}_belly', (0, 0.0, 0.50), (0.34, 0.90, 0.10), belly), 'spine_front')
    A(sphere(f'{name}_mane', (0, -0.50, 0.74), 0.34, mane, scale=(1, 0.8, 1)), 'neck')
    A(sphere(f'{name}_head', (0, -0.78, 0.82), 0.19, fur, scale=(1, 1.15, 0.95)), 'head')
    A(box(f'{name}_muzzle', (0, -0.98, 0.76), (0.16, 0.14, 0.13), belly), 'head')
    A(box(f'{name}_nose', (0, -1.05, 0.80), (0.07, 0.03, 0.04), dark), 'head')
    for s, x in (('L', 0.10), ('R', -0.10)):
        A(sphere(f'{name}_ear_{s}', (x, -0.72, 0.98), 0.05, mane), 'head')
        A(box(f'{name}_eye_{s}', (x * 0.8, -0.95, 0.87), (0.04, 0.02, 0.03), dark), 'head')
    A(tube(f'{name}_tail', (0, 0.42, 0.62), (0, 0.92, 0.52), 0.04, 0.03, fur), 'tail')
    A(sphere(f'{name}_tail_tuft', (0, 0.96, 0.50), 0.06, mane), 'tail')
    for tag, x, y in (('FL', 0.16, -0.36), ('FR', -0.16, -0.36), ('RL', 0.16, 0.36), ('RR', -0.16, 0.36)):
        A(tube(f'{name}_uleg_{tag}', (x, y, 0.60), (x, y, 0.32), 0.09, 0.07, fur), 'upper_' + tag)
        A(tube(f'{name}_lleg_{tag}', (x, y, 0.32), (x, y, 0.03), 0.065, 0.06, fur), 'lower_' + tag)
        A(box(f'{name}_paw_{tag}', (x, y - 0.03, 0.04), (0.14, 0.18, 0.08), belly), 'lower_' + tag)
    for ob in head_parts:
        scale_about(ob, (0, -0.72, 0.80), LION_HEAD_SCALE)
    return {'name': name, 'arm': arm, 'kind': 'quadruped', 'tip': None}


# =============================================================================
# Blob (Agua Corrupta)
# =============================================================================
def build_blob(name, spec):
    arm = build_armature(name, [('root', (0, 0, 0), (0, 0, 0.5), None)])
    M = lambda key, col: _mat(name, key, col)
    body = M('body', spec['body'])
    vein = M('vein', spec['vein'])
    slime = M('slime', spec['slime'])
    eye = M('eye', spec.get('eye', '#c0392b'))
    A = lambda ob: attach(ob, arm, 'root')
    A(sphere(f'{name}_body', (0, 0, 0.42), 0.46, body, scale=(1.05, 1.0, 0.85), segs=12, rings=8))
    A(sphere(f'{name}_top', (0.05, 0.05, 0.72), 0.22, slime, scale=(1, 1, 0.9)))
    A(sphere(f'{name}_blob2', (-0.30, -0.15, 0.30), 0.20, vein))
    A(sphere(f'{name}_blob3', (0.32, 0.20, 0.28), 0.17, vein))
    A(sphere(f'{name}_blob4', (0.10, -0.38, 0.22), 0.16, slime))
    A(sphere(f'{name}_drop1', (-0.20, 0.35, 0.60), 0.08, slime))
    for x in (-0.15, 0.15):
        A(sphere(f'{name}_eye_{x}', (x, -0.40, 0.52), 0.085, eye))   # big cartoon eyes
    A(tube(f'{name}_puddle', (0, 0, 0.0), (0, 0, 0.05), 0.55, 0.50, vein, segs=14, smooth=False))
    return {'name': name, 'arm': arm, 'kind': 'blob', 'tip': None}


# =============================================================================
# Registry (palette guidance: Elias oranges/reds/golds, Eliseo blues/greens/whites,
# Baal purples/blacks/dark reds)
# =============================================================================
SPECS = {
    'elias': ('humanoid', dict(
        skin='#e0b080', hair='#2a1a12', hair_long=True, beard='#2a1a12', beard_long=True,
        tunic='#c8782a', tunic_len='long', robe='#a85a24', sleeves='#c8782a',
        belt='#5a3a1a', mantle='#4a2c1a', shoes='#4a3018',
        weapon='staff_fire', weapon_colors=dict(wood='#5a3a1a', tip='#ff7a12', core='#ffd23f'),
        attack='swing')),
    'eliseo': ('humanoid', dict(
        skin='#d9a066', hair=None, beard='#4a3628',
        tunic='#3d6fb4', tunic_len='long', robe='#2f5a94', sleeves='#3d6fb4',
        belt='#3e9b5f', sash='#3e9b5f', mantle='#e4ecef', shoes='#4a3018',
        weapon='staff_light', weapon_colors=dict(wood='#7a5a3a', tip='#f4fbff', core='#8ad0ff'),
        attack='swing')),
    'soldado_acab': ('humanoid', dict(
        skin='#d9a066', hair=None, helmet='#c9a24a',
        tunic='#2980b9', tunic_len='short', sleeves='#2980b9', armor='#b08d3a',
        belt='#4a2e14', shoes='#4a3018',
        weapon='sword', shield='round', shield_color='#8b4a2b', shield_boss='#c9a24a',
        attack='swing')),
    'arquero_real': ('humanoid', dict(
        skin='#d9a066', hood='#6b4423', beard=None,
        tunic='#e67e22', tunic_len='short', sleeves='#e67e22', bracer='#6b4423',
        belt='#4a2e14', shoes='#4a3018', quiver=True,
        weapon='bow', weapon_colors=dict(wood='#5a3a1a'),
        attack='bow')),
    'profeta_baal': ('humanoid', dict(
        skin='#c98c5a', hair=None, beard='#151015', beard_long=True, headdress='#c9a24a',
        tunic='#8e44ad', tunic_len='long', robe='#5b2c83', sleeves='#5b2c83',
        belt='#c9a24a', mantle='#1a1420', shoes='#1a1420', staff_top=1.55,
        weapon='staff_idol', weapon_colors=dict(wood='#2a1a2a', tip='#7a1f1f'),
        attack='raise')),
    'leon_desierto': ('lion', dict(fur='#e6b422', mane='#8a4b1a', belly='#f0d090',
                                   place=dict(offset=(0, 0.42, 0), scale=0.8))),
    'soldado_sirio': ('humanoid', dict(
        skin='#c98c5a', hair=None, helmet='#5a666b',
        tunic='#16a085', tunic_len='short', sleeves='#5a666b', armor='#1a8f7a', legs='#5f6a6e',
        belt='#3a3a3a', shoes='#2a2a2a', bracer='#5a666b',
        weapon='spear', weapon_colors=dict(wood='#5a3a1a', blade='#c8ccd0'),
        shield='tall', shield_color='#7a5a3a', shield_boss='#16a085',
        attack='thrust')),
    'general_sirio': ('humanoid', dict(
        skin='#c98c5a', hair=None, beard='#2a1a12', helmet='#d4a537', plume='#e74c3c',
        tunic='#8f3a2b', tunic_len='short', sleeves='#8f3a2b', armor='#c0392b', legs='#5f6a6e',
        belt='#d4a537', cape='#7a1f1f', shoes='#2a2a2a', bracer='#d4a537',
        weapon='sword', weapon_colors=dict(blade='#e0e4e8', guard='#d4a537'),
        attack='swing')),
    'leproso': ('humanoid', dict(
        skin='#a8b090', hair='#6a6a5a', beard=None,
        tunic='#4f7f5f', tunic_len='short', sleeves='#4f7f5f', legs='#a8b090',
        belt=None, shoes='#7a7a6a', bandages=True,
        weapon=None, attack='claw')),
    'agua_corrupta': ('blob', dict(body='#2c3e50', vein='#3a2f5c', slime='#3fae7c', eye='#c0392b',
                                   place=dict(offset=(0, 0.2, 0), scale=0.9))),
}

NAMES = list(SPECS.keys())


def build(name):
    kind, spec = SPECS[name]
    if kind == 'humanoid':
        char = build_humanoid(name, spec)
    elif kind == 'lion':
        char = build_lion(name, spec)
    else:
        char = build_blob(name, spec)
    # Optional placement: the armature rotates about its own origin for the 4 directions,
    # so `offset` (world) stays fixed on screen and `scale` shrinks wide-footprint characters.
    place = spec.get('place')
    if place:
        arm = char['arm']
        arm.location = place.get('offset', (0, 0, 0))
        s = place.get('scale', 1.0)
        arm.scale = (s, s, s)
    return char
