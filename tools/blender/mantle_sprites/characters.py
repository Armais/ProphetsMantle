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
# Face (head sphere: centre (0,0,1.62), r 0.135, z-scale 1.1, front = -Y)
# =============================================================================
# The head is ~20 px on screen, so the eyes have to be figurine-sized to read at all:
# bone-white sclera + a very dark iris + a thick brow. Internal features get NO outline
# from the post pass (it only dilates the silhouette), so all the contrast is colour.
EYE_X, EYE_Y, EYE_Z = 0.055, -0.100, 1.648
EYE_R, EYE_FLAT = 0.039, 0.50      # sclera radius and its squash along Y
IRIS_Y, IRIS_R = -0.126, 0.027
BROW_Z, BROW_Y = 1.702, -0.112
# degrees the OUTER end of the brow rises (mirrored per side): >0 = ceno duro, <0 = afligido
BROW_TILT = {'calm': 4.0, 'noble': 10.0, 'angry': 20.0, 'wild': 13.0, 'sad': -14.0}


def _face(name, spec, A, skin):
    """Eyes, brows, nose and mouth, all riding the head bone (so HEAD_SCALE grows them)."""
    if not spec.get('face', True):
        return
    white = get_mat('eye_white', vivid(hexc('#f2ece0'), 1.0))
    iris = _mat(name, 'iris', spec.get('eyes', '#241a14'))
    brow_col = spec.get('brow_color') or spec.get('hair') or spec.get('beard') or '#2a1a12'
    brow_mat = _mat(name, 'brow', brow_col)
    tilt = BROW_TILT.get(spec.get('brow', 'calm'), 0.0)
    for s, sx in (('L', 1.0), ('R', -1.0)):
        x = EYE_X * sx
        A(sphere(f'{name}_eye_{s}', (x, EYE_Y, EYE_Z), EYE_R, white,
                 scale=(1.0, EYE_FLAT, 1.05)), 'head')
        A(sphere(f'{name}_iris_{s}', (x, IRIS_Y, EYE_Z - 0.004), IRIS_R, iris,
                 scale=(1.0, 0.55, 1.1)), 'head')
        A(box(f'{name}_brow_{s}', (x, BROW_Y, BROW_Z), (0.068, 0.036, 0.020), brow_mat,
              rot=Matrix.Rotation(math.radians(-tilt * sx), 4, 'Y'), round=0.9), 'head')
    A(sphere(f'{name}_nose', (0, -0.122, 1.600), 0.030, skin, scale=(1.0, 1.0, 0.85)), 'head')
    if spec.get('mouth', True) and not spec.get('beard'):
        A(box(f'{name}_mouth', (0, -0.124, 1.552), (0.058, 0.024, 0.016),
              _mat(name, 'mouth', spec.get('mouth_color', '#6b3226')), round=0.9), 'head')


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
    A(sphere(f'{name}_head', (0, 0, 1.62), 0.135, skin, scale=(1, 0.98, 1.1)), 'head')
    A(tube(f'{name}_neck', (0, 0, 1.43), (0, 0, 1.53), 0.05, 0.05, skin), 'neck')
    _face(name, spec, A, skin)
    if spec.get('hair'):
        hair = M('hair', spec['hair'])
        # Casquete: arranca por encima de las cejas (z 1.70) y cae por detras. Todo lo que
        # baje del entrecejo tapa la cara y devuelve la "bola de pelo" de antes.
        A(sphere(f'{name}_hair', (0, 0.036, 1.702), 0.136, hair, scale=(1.02, 1.0, 0.70)), 'head')
        A(box(f'{name}_hair_fringe', (0, -0.074, 1.718), (0.198, 0.095, 0.040), hair,
              round=0.8, taper=(0.82, 0.9)), 'head')
        for s, x in (('L', 0.122), ('R', -0.122)):
            A(sphere(f'{name}_hair_side_{s}', (x, 0.048, 1.638), 0.064, hair,
                     scale=(0.65, 1.0, 1.05)), 'head')
        if spec.get('hair_long'):
            A(box(f'{name}_hair_back', (0, 0.108, 1.498), (0.205, 0.098, 0.27), hair,
                  round=0.6, taper=(0.8, 0.85)), 'head')
    if spec.get('beard'):
        beard = M('beard', spec['beard'])
        A(sphere(f'{name}_beard', (0, -0.046, 1.524), 0.104, beard, scale=(0.94, 1.06, 0.74)), 'head')
        A(sphere(f'{name}_mustache', (0, -0.108, 1.576), 0.062, beard, scale=(1.0, 0.62, 0.40)), 'head')
        if spec.get('beard_long'):
            A(tube(f'{name}_beard2', (0, -0.052, 1.545), (0, -0.030, 1.395), 0.088, 0.042,
                   beard, rim_round=0.02), 'head')
            A(sphere(f'{name}_beard_tip', (0, -0.030, 1.395), 0.042, beard, scale=(1, 1, 0.8)), 'head')
    if spec.get('helmet'):
        helm = M('helmet', spec['helmet'])
        # Cupula alta + aro abierto + nasal: por delante la esfera sube hasta z~1.66, justo
        # sobre las cejas, asi que el rostro queda a la vista.
        A(sphere(f'{name}_helm', (0, 0.008, 1.752), 0.143, helm, scale=(1, 1, 0.74)), 'head')
        A(tube(f'{name}_helm_rim', (0, 0, 1.698), (0, 0, 1.742), 0.157, 0.157, helm,
               caps=False), 'head')
        A(box(f'{name}_helm_nasal', (0, -0.138, 1.642), (0.028, 0.032, 0.110), helm, round=0.8), 'head')
        for s, x in (('L', 0.130), ('R', -0.130)):
            A(box(f'{name}_helm_cheek_{s}', (x, -0.030, 1.578), (0.024, 0.098, 0.105), helm,
                  round=0.6, taper=(1.0, 1.1)), 'head')
        if spec.get('plume'):
            plume = M('plume', spec['plume'])
            A(box(f'{name}_plume', (0, 0.02, 1.90), (0.055, 0.26, 0.15), plume,
                  round=0.9, taper=(0.6, 1.0)), 'head')
    if spec.get('headdress'):
        hd = M('headdress', spec['headdress'])
        A(tube(f'{name}_headdress', (0, 0, 1.70), (0, 0, 1.92), 0.13, 0.09, hd, rim_round=0.02), 'head')
        horn = M('horn', '#e8dcc0')
        A(tube(f'{name}_horn_l', (0.10, 0, 1.80), (0.19, 0, 1.98), 0.03, 0.005, horn), 'head')
        A(tube(f'{name}_horn_r', (-0.10, 0, 1.80), (-0.19, 0, 1.98), 0.03, 0.005, horn), 'head')
    if spec.get('crown') or spec.get('diadem'):
        # Corona de rey (Acab) / diadema de reina (Jezabel). Se apoya SOBRE el casquete de
        # pelo: el aro arranca en z 1.726, por encima de la ceja (1.712), asi que el rostro
        # queda libre igual que con el casco.
        thin = bool(spec.get('diadem'))
        cr = M('crown', spec.get('crown') or spec['diadem'])
        # La diadema va mas baja y mas ceñida que la corona: un aro fino y ancho deja ver el
        # pelo por el hueco y lee como agujero en la coronilla, no como joya.
        z0, h_band, r = (1.718, 0.052, 0.142) if thin else (1.726, 0.082, 0.150)
        z1 = z0 + h_band
        A(tube(f'{name}_crown', (0, 0, z0), (0, 0, z1), r, r, cr, caps=False, rim_round=0.015), 'head')
        # Puntas al frente y a los lados: de perfil tambien tiene que leerse como corona
        for i, deg in enumerate((0.0, 42.0, -42.0) if thin else (0.0, 34.0, -34.0, 70.0, -70.0)):
            a = math.radians(deg)
            h = (0.075 if thin else 0.090) if i == 0 else (0.044 if thin else 0.064)
            A(box(f'{name}_crown_pt{i}', (math.sin(a) * r, -math.cos(a) * r, z1 + h * 0.5),
                  (0.050, 0.038, h), cr, round=0.5, taper=(0.25, 0.35)), 'head')
        if spec.get('gem'):
            A(sphere(f'{name}_crown_gem', (0, -r - 0.010, z0 + h_band * 0.62), 0.032,
                     M('gem', spec['gem']), scale=(1.0, 0.6, 1.1)), 'head')
    if spec.get('headcloth'):
        # Pano de cabeza (keffiyeh / velo). NO es la capucha: la capucha es un cuello
        # redondo cerrado y esta cae a los LADOS de la cara. Hacen falta las dos siluetas
        # porque cinco de los siete locutores de los encuentros llevan algo en la cabeza, y
        # con una sola forma se parecerian todos en un retrato de 32 unidades.
        # Arranca donde el casquete de pelo (z 1.700), asi la frente y las cejas quedan
        # libres: la regla de que nada baje de z 1.60 por delante sigue en pie.
        cloth = M('headcloth', spec['headcloth'])
        drop = 0.34 if spec.get('headcloth_len') == 'long' else 0.20
        A(sphere(f'{name}_cloth', (0, 0.030, 1.700), 0.152, cloth,
                 scale=(1.03, 1.04, 0.66)), 'head')
        A(box(f'{name}_cloth_brow', (0, -0.072, 1.724), (0.212, 0.096, 0.042), cloth,
              round=0.8, taper=(0.86, 0.94)), 'head')
        for s, x in (('L', 0.140), ('R', -0.140)):
            A(box(f'{name}_cloth_side_{s}', (x, 0.012, 1.678 - drop * 0.5),
                  (0.062, 0.150, drop), cloth, round=0.6, taper=(0.90, 1.08)), 'head')
        A(box(f'{name}_cloth_back', (0, 0.120, 1.678 - drop * 0.55),
              (0.245, 0.100, drop * 1.05), cloth, round=0.6, taper=(0.88, 1.0)), 'head')
        if spec.get('headcloth_cord'):
            A(tube(f'{name}_cloth_cord', (0, 0.006, 1.744), (0, 0.006, 1.780), 0.152, 0.152,
                   M('cord', spec['headcloth_cord']), caps=False, rim_round=0.012), 'head')
    if spec.get('band'):
        # Cinta lisa: un aro y nada mas. La corona y la diadema llevan PUNTAS, y unas puntas
        # sobre la mujer de Sunem la convertirian en reina, que es justo lo que no es
        # (2 Reyes 4:8 dice "mujer importante", no reina).
        bm = M('band', spec['band'])
        A(tube(f'{name}_band', (0, 0, 1.718), (0, 0, 1.768), 0.150, 0.150, bm,
               caps=False, rim_round=0.014), 'head')
        if spec.get('gem'):
            A(sphere(f'{name}_band_gem', (0, -0.154, 1.742), 0.030, M('gem', spec['gem']),
                     scale=(1.0, 0.6, 1.1)), 'head')
    if spec.get('hood'):
        hood = M('hood', spec['hood'])
        # Cupula echada atras + capucho abierto alrededor de la cara. `hood_open` solo decide
        # cuanto adelanta el capucho: el rostro se ve siempre (antes la esfera se comia la
        # cabeza entera y quedaba un boton sin cara).
        open_ = spec.get('hood_open')
        A(sphere(f'{name}_hood', (0, 0.048, 1.690), 0.158, hood, scale=(1.0, 1.06, 0.95)), 'head')
        cy = -0.060 if open_ else -0.098
        A(tube(f'{name}_hood_cowl', (0, cy + 0.075, 1.645), (0, cy, 1.645), 0.152, 0.166,
               hood, caps=False), 'head')
        A(box(f'{name}_hood_nape', (0, 0.135, 1.545), (0.24, 0.12, 0.22), hood,
              round=0.6, taper=(0.85, 0.9)), 'head')

    # --- torso ----------------------------------------------------------
    # `slim` solo estrecha el tronco; hombros y brazos se quedan sobre sus huesos para no
    # descolocar las animaciones. Con el pelo largo y la falda acampanada ya lee femenino.
    slim = 0.85 if spec.get('slim') else 1.0
    A(box(f'{name}_torso_lo', (0, 0, 1.15), (0.34 * slim, 0.20, 0.22), tunic, round=0.55), 'spine')
    chest_mat = M('armor', spec['armor']) if spec.get('armor') else tunic
    A(box(f'{name}_torso_up', (0, 0, 1.36), (0.40 * slim, 0.22, 0.22), chest_mat,
          round=0.55, taper=(0.92, 0.95)), 'chest')
    for s, x in (('L', 0.22), ('R', -0.22)):
        A(sphere(f'{name}_shoulder_{s}', (x, 0, 1.42), 0.08, chest_mat), 'chest')
    if spec.get('belt'):
        A(tube(f'{name}_belt', (0, 0, 1.01), (0, 0, 1.08), 0.205, 0.205,
               M('belt', spec['belt']), rim_round=0.02), 'hips')
    if spec.get('sash'):
        A(box(f'{name}_sash', (0, -0.115, 1.25), (0.09, 0.02, 0.44), M('sash', spec['sash']),
              round=0.9), 'spine')
    # tunic skirt / robe
    if spec.get('tunic_len', 'short') == 'long':
        robe = M('robe', spec.get('robe', spec['tunic']))
        r_waist, r_hem = (0.165, 0.335) if spec.get('slim') else (0.19, 0.30)
        A(tube(f'{name}_robe', (0, 0, 1.00), (0, 0, 0.10), r_waist, r_hem, robe, segs=32,
               rim_round=0.03), 'hips')
    else:
        A(tube(f'{name}_skirt', (0, 0, 1.00), (0, 0, 0.62), 0.19, 0.24, tunic, segs=32,
               rim_round=0.03), 'hips')
    if spec.get('mantle'):
        mantle = M('mantle', spec['mantle'])
        A(box(f'{name}_mantle', (0, 0.135, 1.18), (0.42, 0.07, 0.56), mantle,
              round=0.9, taper=(1.12, 1.0)), 'chest')
        for s, x in (('L', 0.21), ('R', -0.21)):
            A(box(f'{name}_mantle_sh_{s}', (x, 0.02, 1.45), (0.16, 0.22, 0.09), mantle,
                  round=0.8), 'chest')
    if spec.get('cape'):
        cape = M('cape', spec['cape'])
        A(box(f'{name}_cape', (0, 0.15, 1.02), (0.40, 0.05, 0.88), cape,
              round=0.9, taper=(0.78, 1.0)), 'chest')

    # --- arms -----------------------------------------------------------
    for s, x in (('L', 0.22), ('R', -0.22)):
        A(tube(f'{name}_uarm_{s}', (x, 0, 1.42), (x, 0, 1.16), 0.058, 0.052, sleeves), 'upper_arm.' + s)
        fmat = M('bracer', spec['bracer']) if spec.get('bracer') else skin
        A(tube(f'{name}_farm_{s}', (x, 0, 1.16), (x, 0, 0.92), 0.05, 0.045, fmat), 'forearm.' + s)
        A(sphere(f'{name}_hand_{s}', (x, 0, 0.88), 0.055, skin), 'hand.' + s)
    if spec.get('bandages'):
        band = M('bandage', '#e8e4d8')
        A(tube(f'{name}_band_head', (0, 0, 1.58), (0, 0, 1.63), 0.14, 0.14, band, rim_round=0.012), 'head')
        A(tube(f'{name}_band_arm', (0.22, 0, 1.10), (0.22, 0, 1.00), 0.056, 0.056, band, rim_round=0.012), 'forearm.L')
        A(tube(f'{name}_band_arm2', (-0.22, 0, 1.36), (-0.22, 0, 1.28), 0.062, 0.062, band, rim_round=0.012), 'upper_arm.R')
        A(tube(f'{name}_band_leg', (-0.09, 0, 0.40), (-0.09, 0, 0.30), 0.065, 0.065, band, rim_round=0.012), 'shin.R')

    # --- legs -----------------------------------------------------------
    for s, x in (('L', 0.09), ('R', -0.09)):
        A(tube(f'{name}_thigh_{s}', (x, 0, 0.92), (x, 0, 0.50), 0.075, 0.065, legs, rim_round=0.02), 'thigh.' + s)
        A(tube(f'{name}_shin_{s}', (x, 0, 0.50), (x, 0, 0.07), 0.06, 0.05, legs, rim_round=0.018), 'shin.' + s)
        A(box(f'{name}_foot_{s}', (x, -0.05, 0.04), (0.11, 0.22, 0.08), shoes,
              round=0.75, taper=(0.85, 1.0)), 'foot.' + s)

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
            tip_child = tube(f'{name}_tip_halo', tip_center + Vector((0, 0, -0.05)), tip_center + Vector((0, 0, 0.09)), 0.11, 0.11, halo, segs=6, caps=False)
        else:
            idol = M('idol', wc.get('tip', '#7a1f1f'))
            horn = M('horn', '#e8dcc0')
            tip_obj = box(f'{name}_idol', tip_center + Vector((0, 0, 0.02)), (0.12, 0.10, 0.14), idol, round=0.45)
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
        A(box(f'{name}_blade', (hx, -0.07, 1.30), (0.035, 0.075, 0.72), steel,
              round=0.55, taper=(0.45, 0.35)), 'hand.R')
        A(box(f'{name}_guard', (hx, -0.07, 0.95), (0.16, 0.04, 0.035), guard, round=0.6), 'hand.R')
    elif w == 'spear':
        wood = M('wood', wc.get('wood', '#5a3a1a'))
        steel = M('steel', wc.get('blade', '#c8ccd0'))
        for ob in (tube(f'{name}_spear', (hx, -0.06, 0.10), (hx, -0.06, 1.50), 0.022, 0.02, wood),
                   tube(f'{name}_spear_tip', (hx, -0.06, 1.50), (hx, -0.06, 1.72), 0.05, 0.004, steel)):
            rotate_about(ob, (hx, 0.0, 0.88), 'X', 20.0)   # compensates base forearm bend
            A(ob, 'hand.R')
    elif w == 'bow':
        wood = M('wood', wc.get('wood', '#5a3a1a'))
        string = M('string', '#e8e0c8')
        lx = 0.22
        for ob in (tube(f'{name}_bow', (lx, -0.08, 0.35), (lx, -0.08, 1.45), 0.024, 0.024, wood),
                   box(f'{name}_bow_string', (lx, 0.0, 0.90), (0.012, 0.012, 1.08), string, round=0.9)):
            rotate_about(ob, (lx, 0.0, 0.88), 'X', 40.0)   # compensates base forearm bend
            A(ob, 'hand.L')
    sh = spec.get('shield')
    if sh:
        shield = M('shield', spec.get('shield_color', '#8b4a2b'))
        boss = M('shield_boss', spec.get('shield_boss', '#c9a24a'))
        if sh == 'round':
            A(tube(f'{name}_shield', (0.28, -0.03, 1.04), (0.28, -0.075, 1.04), 0.18, 0.175,
                   shield, segs=32, rim_round=0.022), 'forearm.L')
            A(sphere(f'{name}_shield_boss', (0.28, -0.08, 1.04), 0.05, boss), 'forearm.L')
        else:
            A(box(f'{name}_shield', (0.30, -0.05, 1.02), (0.34, 0.05, 0.56), shield,
                  round=0.85, taper=(0.9, 1.0)), 'forearm.L')
            A(box(f'{name}_shield_rim', (0.30, -0.08, 1.02), (0.08, 0.02, 0.50), boss, round=0.9), 'forearm.L')
    if spec.get('quiver'):
        A(tube(f'{name}_quiver', (0.13, 0.15, 1.08), (0.13, 0.15, 1.52), 0.055, 0.052,
               M('quiver', '#6b4423'), rim_round=0.014), 'chest')
        A(box(f'{name}_arrows', (0.13, 0.15, 1.56), (0.09, 0.09, 0.10), M('fletch', '#e8e0c8'),
              round=0.5), 'chest')
    if spec.get('censer'):
        # Brasero colgado de la mano izquierda: las brasas que revientan cuando cae.
        # Cuelga por debajo de la mano (z 0.88) para que se lea sin tapar el torso.
        cc = spec.get('censer_colors', {})
        chain = M('chain', cc.get('chain', '#5a5a62'))
        bowl = M('censer', cc.get('bowl', '#b08d3a'))
        coals = M('coals', cc.get('coals', '#ff8c1a'))
        lx = 0.22
        A(tube(f'{name}_censer_chain', (lx, -0.03, 0.86), (lx, -0.03, 0.70), 0.012, 0.012, chain), 'hand.L')
        A(tube(f'{name}_censer', (lx, -0.03, 0.70), (lx, -0.03, 0.56), 0.11, 0.05, bowl, segs=10), 'hand.L')
        A(sphere(f'{name}_censer_coals', (lx, -0.03, 0.72), 0.09, coals, scale=(1, 1, 0.5)), 'hand.L')

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

    A(box(f'{name}_body_r', (0, 0.22, 0.64), (0.40, 0.55, 0.36), fur, round=0.7), 'spine_rear')
    A(box(f'{name}_body_f', (0, -0.22, 0.66), (0.44, 0.50, 0.40), fur, round=0.7), 'spine_front')
    A(box(f'{name}_belly', (0, 0.0, 0.50), (0.34, 0.90, 0.10), belly, round=0.9), 'spine_front')
    A(sphere(f'{name}_mane', (0, -0.50, 0.74), 0.34, mane, scale=(1, 0.8, 1)), 'neck')
    A(sphere(f'{name}_head', (0, -0.78, 0.82), 0.19, fur, scale=(1, 1.15, 0.95)), 'head')
    A(sphere(f'{name}_muzzle', (0, -0.985, 0.765), 0.095, belly, scale=(1.0, 0.95, 0.78)), 'head')
    A(sphere(f'{name}_nose', (0, -1.055, 0.805), 0.038, dark, scale=(1.0, 0.7, 0.75)), 'head')
    white = get_mat('eye_white', vivid(hexc('#f2ece0'), 1.0))
    for s, x in (('L', 0.10), ('R', -0.10)):
        A(sphere(f'{name}_ear_{s}', (x, -0.72, 0.98), 0.05, mane), 'head')
        A(sphere(f'{name}_eye_{s}', (x * 0.82, -0.930, 0.878), 0.048, white, scale=(1, 0.6, 0.95)), 'head')
        A(sphere(f'{name}_iris_{s}', (x * 0.82, -0.962, 0.874), 0.028, dark, scale=(1, 0.6, 1.05)), 'head')
        A(box(f'{name}_brow_{s}', (x * 0.85, -0.930, 0.930), (0.075, 0.045, 0.026), mane,
              rot=Matrix.Rotation(math.radians(-16.0 * (1 if x > 0 else -1)), 4, 'Y'), round=0.9), 'head')
    A(tube(f'{name}_tail', (0, 0.42, 0.62), (0, 0.92, 0.52), 0.04, 0.03, fur), 'tail')
    A(sphere(f'{name}_tail_tuft', (0, 0.96, 0.50), 0.06, mane), 'tail')
    for tag, x, y in (('FL', 0.16, -0.36), ('FR', -0.16, -0.36), ('RL', 0.16, 0.36), ('RR', -0.16, 0.36)):
        A(tube(f'{name}_uleg_{tag}', (x, y, 0.60), (x, y, 0.32), 0.09, 0.07, fur, rim_round=0.02), 'upper_' + tag)
        A(tube(f'{name}_lleg_{tag}', (x, y, 0.32), (x, y, 0.03), 0.065, 0.06, fur, rim_round=0.018), 'lower_' + tag)
        A(box(f'{name}_paw_{tag}', (x, y - 0.03, 0.04), (0.14, 0.18, 0.08), belly, round=0.8), 'lower_' + tag)
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
    white = get_mat('eye_white', vivid(hexc('#f2ece0'), 1.0))
    for i, x in enumerate((-0.15, 0.15)):
        # big cartoon eyes: sclera + iris + a highlight, so the blob still reads as a face
        A(sphere(f'{name}_eye_{i}', (x, -0.400, 0.520), 0.092, white, scale=(1, 0.75, 1.0)))
        A(sphere(f'{name}_iris_{i}', (x, -0.452, 0.514), 0.052, eye, scale=(1, 0.7, 1.05)))
        A(sphere(f'{name}_glint_{i}', (x - 0.025, -0.470, 0.552), 0.022, white))
    A(tube(f'{name}_puddle', (0, 0, 0.0), (0, 0, 0.05), 0.55, 0.50, vein, segs=32, rim_round=0.02))
    return {'name': name, 'arm': arm, 'kind': 'blob', 'tip': None}


# =============================================================================
# Registry (palette guidance: Elias oranges/reds/golds, Eliseo blues/greens/whites,
# Baal purples/blacks/dark reds)
# =============================================================================
SPECS = {
    'elias': ('humanoid', dict(
        skin='#e0b080', hair='#2a1a12', hair_long=True, beard='#2a1a12', beard_long=True,
        eyes='#20303c', brow='wild',
        tunic='#c8782a', tunic_len='long', robe='#a85a24', sleeves='#c8782a',
        belt='#5a3a1a', mantle='#4a2c1a', shoes='#4a3018',
        weapon='staff_fire', weapon_colors=dict(wood='#5a3a1a', tip='#ff7a12', core='#ffd23f'),
        attack='swing')),
    'eliseo': ('humanoid', dict(
        skin='#d9a066', hair=None, beard='#4a3628',
        eyes='#2d3f5e', brow='calm',
        tunic='#3d6fb4', tunic_len='long', robe='#2f5a94', sleeves='#3d6fb4',
        belt='#3e9b5f', sash='#3e9b5f', mantle='#e4ecef', shoes='#4a3018',
        weapon='staff_light', weapon_colors=dict(wood='#7a5a3a', tip='#f4fbff', core='#8ad0ff'),
        attack='swing')),
    'soldado_acab': ('humanoid', dict(
        skin='#d9a066', hair=None, helmet='#c9a24a',
        eyes='#241a14', brow='angry',
        tunic='#2980b9', tunic_len='short', sleeves='#2980b9', armor='#b08d3a',
        belt='#4a2e14', shoes='#4a3018',
        weapon='sword', shield='round', shield_color='#8b4a2b', shield_boss='#c9a24a',
        attack='swing')),
    'arquero_real': ('humanoid', dict(
        skin='#d9a066', hood='#6b4423', beard=None,
        eyes='#241a14', brow='angry', brow_color='#3a2416',
        tunic='#e67e22', tunic_len='short', sleeves='#e67e22', bracer='#6b4423',
        belt='#4a2e14', shoes='#4a3018', quiver=True,
        weapon='bow', weapon_colors=dict(wood='#5a3a1a'),
        attack='bow')),
    'profeta_baal': ('humanoid', dict(
        skin='#c98c5a', hair=None, beard='#151015', beard_long=True, headdress='#c9a24a',
        eyes='#8e1f1f', brow='angry',
        tunic='#8e44ad', tunic_len='long', robe='#5b2c83', sleeves='#5b2c83',
        belt='#c9a24a', mantle='#1a1420', shoes='#1a1420', staff_top=1.55,
        weapon='staff_idol', weapon_colors=dict(wood='#2a1a2a', tip='#7a1f1f'),
        attack='raise')),
    'leon_desierto': ('lion', dict(fur='#e6b422', mane='#8a4b1a', belly='#f0d090',
                                   place=dict(offset=(0, 0.42, 0), scale=0.8))),
    'soldado_sirio': ('humanoid', dict(
        skin='#c98c5a', hair=None, helmet='#5a666b',
        eyes='#241a14', brow='angry',
        tunic='#16a085', tunic_len='short', sleeves='#5a666b', armor='#1a8f7a', legs='#5f6a6e',
        belt='#3a3a3a', shoes='#2a2a2a', bracer='#5a666b',
        weapon='spear', weapon_colors=dict(wood='#5a3a1a', blade='#c8ccd0'),
        shield='tall', shield_color='#7a5a3a', shield_boss='#16a085',
        attack='thrust')),
    'general_sirio': ('humanoid', dict(
        skin='#c98c5a', hair=None, beard='#2a1a12', helmet='#d4a537', plume='#e74c3c',
        eyes='#241a14', brow='angry',
        tunic='#8f3a2b', tunic_len='short', sleeves='#8f3a2b', armor='#c0392b', legs='#5f6a6e',
        belt='#d4a537', cape='#7a1f1f', shoes='#2a2a2a', bracer='#d4a537',
        weapon='sword', weapon_colors=dict(blade='#e0e4e8', guard='#d4a537'),
        attack='swing')),
    'idolatra_rimon': ('humanoid', dict(
        skin='#c98c5a', hair=None, beard='#2a1a2a', hood='#42285e', hood_open=True,
        eyes='#8f3d12', brow='wild',
        tunic='#8e5bb8', tunic_len='long', robe='#5c3a80', sleeves='#5c3a80', legs='#42285e',
        belt='#c9a24a', shoes='#2a1a2a',
        censer=True, censer_colors=dict(bowl='#c9a24a', coals='#ff8c1a'),
        weapon=None, attack='raise')),
    'agua_corrupta': ('blob', dict(body='#2c3e50', vein='#3a2f5c', slime='#3fae7c', eye='#c0392b',
                                   place=dict(offset=(0, 0.2, 0), scale=0.9))),
    # Rey de Israel: rojos oscuros y oro. NO lleva morados de Baal — el culto lo trae
    # Jezabel; Acab lo consiente, que es justo lo que cuenta el prologo.
    'acab': ('humanoid', dict(
        skin='#d9a066', hair='#3a2418', beard='#3a2418',
        eyes='#2a1c14', brow='angry',
        crown='#d4a537', gem='#8e1f1f',
        tunic='#7d2b3a', tunic_len='long', robe='#5a1e2a', sleeves='#7d2b3a',
        armor='#8f3346', belt='#d4a537', cape='#6d1f2e', bracer='#d4a537', shoes='#3a2418',
        weapon='sword', weapon_colors=dict(blade='#e0e4e8', guard='#d4a537'),
        attack='swing')),
    # Reina sidonia y patrona de Baal: morados, negro y oro, ojos rojos como el profeta.
    'jezabel': ('humanoid', dict(
        skin='#e8c49a', hair='#151015', hair_long=True, beard=None, slim=True,
        eyes='#8e1f1f', brow='angry', brow_color='#151015', mouth_color='#8e2436',
        diadem='#c9a24a', gem='#8e1f1f',
        tunic='#5b2c83', tunic_len='long', robe='#3b1a45', sleeves='#8e44ad',
        belt='#c9a24a', sash='#c9a24a', mantle='#1a1420', shoes='#1a1420',
        # staff_top bajo: con el idolo a la altura de la cara le tapaba la mejilla
        weapon='staff_idol', weapon_colors=dict(wood='#2a1a2a', tip='#7a1f1f'), staff_top=1.40,
        attack='raise')),
    # -------------------------------------------------------------------------
    # Locutores de los encuentros (game/events.h). SOLO existen para su RETRATO:
    # no andan por el mundo, asi que no llevan hoja de animacion (ver SHEET_NAMES).
    # El encuadre del retrato corta por la cintura, asi que lo que los distingue tiene
    # que estar de los hombros para arriba: pano de cabeza, pelo, barba y color de tunica.
    # -------------------------------------------------------------------------
    # Cap 2. Fenicia, y la mas pobre de las tres mujeres: lino crudo sin tenir y nada de oro.
    'viuda_sarepta': ('humanoid', dict(
        skin='#e0b080', hair='#3a2418', hair_long=True, beard=None, slim=True,
        eyes='#3a2a1e', brow='sad', mouth_color='#8a4a44',
        headcloth='#b07a62', headcloth_len='long',
        tunic='#7a6a5e', tunic_len='long', robe='#5f5248', sleeves='#7a6a5e',
        belt='#4e4038', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 6. Hombre de ciudad: el unico locutor con barba y pano corto con cordon.
    'hombre_jerico': ('humanoid', dict(
        skin='#c98c5a', hair='#3a2418', beard='#3a2418',
        eyes='#241a14', brow='calm',
        headcloth='#d8c9a4', headcloth_cord='#5a4634',
        tunic='#7d8a4a', tunic_len='short', sleeves='#6b7740',
        belt='#5a4a30', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 7. Viuda israelita de luto: mismo velo largo que la de Sarepta pero AZUL OSCURO,
    # que es lo que separa a las dos de un vistazo.
    'mujer_profeta': ('humanoid', dict(
        skin='#d9a066', hair='#241a14', hair_long=True, beard=None, slim=True,
        eyes='#2a2a34', brow='sad', mouth_color='#7a3a3a',
        headcloth='#3c4a63', headcloth_len='long',
        tunic='#5a6473', tunic_len='long', robe='#414b58', sleeves='#5a6473',
        belt='#3a3a42', shoes='#2a2a2a',
        weapon=None, attack='raise')),
    # Cap 8. "Mujer importante" (2 Reyes 4:8): NO es pobre. Lino fino, verde jade y oro.
    # Lleva `band` (aro liso) y no `diadem`: las puntas la harian reina.
    'sunamita': ('humanoid', dict(
        skin='#e4bc8e', hair='#2a1a12', hair_long=True, beard=None, slim=True,
        eyes='#3a2a1e', brow='noble', mouth_color='#8e4a4a',
        headcloth='#efe4cc', headcloth_len='long', band='#c9a24a', gem='#2e7d5b',
        tunic='#2f8f6f', tunic_len='long', robe='#256b55', sleeves='#3aa885',
        belt='#c9a24a', sash='#c9a24a', shoes='#5a4030',
        weapon=None, attack='raise')),
    # Cap 10. Guiezi ANTES de su castigo: joven y sin barba, que es todo lo que hay que
    # leer. La lepra llega despues (2 Reyes 5:27), aqui todavia no.
    'criado_eliseo': ('humanoid', dict(
        skin='#d9a066', hair='#2a1a12', beard=None,
        eyes='#3a2a1e', brow='calm', mouth_color='#7a4030',
        headcloth='#7d6748',
        tunic='#ddd2b4', tunic_len='short', sleeves='#c9bc9a',
        belt='#6b5436', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 5. Eliseo cuando todavia ara con doce yuntas: misma cara que `eliseo` (esa es la
    # gracia), pero ropa de faena. Sin el manto blanco y sin el cayado de luz, que son las
    # dos cosas que aun no le han dado.
    'eliseo_labrador': ('humanoid', dict(
        skin='#d9a066', hair=None, beard='#4a3628',
        eyes='#2d3f5e', brow='calm',
        tunic='#a8814e', tunic_len='short', sleeves='#8d6a3f',
        belt='#5a4028', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 9. Noble sirio, no oficial de tropa: sin casco ni penacho (asi se distingue del
    # General Sirio), pelo y barba negros a la vista, cinta de oro, teal en vez de rojo y
    # tunica larga. Sin arma: viene a que lo sanen.
    'naaman': ('humanoid', dict(
        skin='#c98c5a', hair='#151015', beard='#151015',
        eyes='#241a14', brow='noble',
        band='#d4a537',
        tunic='#1f7a68', tunic_len='long', robe='#155a4d', sleeves='#2a9c85',
        armor='#d4a537', belt='#d4a537', cape='#0f4f45', bracer='#d4a537', shoes='#2a2a2a',
        weapon=None, attack='raise')),
}

# Los que ANDAN por el mundo y por tanto tienen hoja de 896x1920 en assets/sprites/.
# Los locutores de los encuentros salen solo en un retrato, asi que quedan fuera: sin esto
# `run.main()` sin argumentos les fabricaria siete hojas de 15 filas que nadie carga, y una
# regla del proyecto es que assets/ no crece.
SHEET_NAMES = [
    'elias', 'eliseo', 'soldado_acab', 'arquero_real', 'profeta_baal', 'leon_desierto',
    'soldado_sirio', 'general_sirio', 'idolatra_rimon', 'agua_corrupta', 'acab', 'jezabel',
]
PORTRAIT_ONLY = [n for n in SPECS if n not in SHEET_NAMES]
NAMES = SHEET_NAMES


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
