"""Character builders: shared humanoid (spec-driven), lion (quadruped) and blob."""
import math
from mathutils import Vector, Matrix
from . import parts
from .parts import get_mat, hexc, vivid, box, sphere, tube, attach, attach_obj, set_origin, rotate_about, scale_about
from .rig import build_armature

STAFF_TILT = 25.0   # degrees; must match the forearm.R bend of humanoid_base('swing'/'raise')

# Estilizacion ADULTA (2026-11-10, el usuario: "que no se vean tan infantiles, sino mas
# maduros"). Hasta entonces era la figurita de Clash Royale: cabeza x1,45, ojos de boton
# blancos, nariz de bola, manos de bola y paleta saturada x1,35 — un juguete. Lo que lo
# hace adulto, por orden de peso a 128 px: (1) la CABEZA a escala casi real (~6 cabezas de
# alto en vez de 4,5), (2) la cara esculpida — arco de la ceja que hace sombra, ojos
# pequenos y entornados bajo el parpado, nariz en cuna, mandibula y pomulos — y (3) color
# de tierra en vez de caramelo. El esqueleto, las animaciones y la posicion de cada pieza
# de la cabeza (casco, corona, pano) NO cambian: el craneo nuevo ocupa el mismo sitio.
HEAD_SCALE = 1.06        # humanoid head parts scaled about HEAD_PIVOT (era 1.45)
HEAD_PIVOT = (0, 0, 1.56)
LION_HEAD_SCALE = 1.12
SAT = 1.0                # material saturation boost (skin uses SKIN_SAT); era 1.35
SKIN_SAT = 0.96
VAL = 0.97               # un punto mas oscuro: la gama de tierra pesa, la de caramelo no


def _mat(name, key, col, skin=False):
    return get_mat(f'{name}_{key}', vivid(hexc(col), SKIN_SAT if skin else SAT, VAL))


def _squash_y(ob, k, cy=0.0):
    """Aplasta una pieza en profundidad (Y) sobre `cy`. Las tunicas son conos REDONDOS y el
    tronco es mas ancho que hondo: de perfil la falda redonda era una barriga."""
    for v in ob.data.vertices:
        v.co.y = cy + (v.co.y - cy) * k
    return ob


def _soft(ob, levels=2):
    """Subdivision: la caja biselada deja una cara PLANA en medio que la luz de estudio pinta
    como un panel con su marco (el pecho de robot). Subdividida es carne o tela."""
    m = ob.modifiers.new('soft', 'SUBSURF')
    m.levels = m.render_levels = levels
    # Todas las caras SUAVES: la cara plana del medio, subdividida y en flat, salia a rayas
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def _cloak(name, mat, z0, z1, r0, r1, arc=210.0, cy=0.0, flutes=6, amp=0.06, depth=0.75):
    """Capa que cuelga por la espalda: un trozo de cono ABIERTO (solo la mitad de atras y
    los hombros, `arc` grados centrados en +Y), con pliegues que crecen hacia el bajo.
    La caja plana de antes se leia como una tabla (o una mochila) colgada del cuello."""
    import bmesh
    bm = bmesh.new()
    nu, nv = 28, 8
    rows = []
    for j in range(nv + 1):
        t = j / nv                              # 0 arriba, 1 en el bajo
        z = z0 + (z1 - z0) * t
        r = r0 + (r1 - r0) * t
        row = []
        for i in range(nu + 1):
            u = i / nu
            a = math.radians(90.0 - arc * 0.5 + arc * u)      # 90 = +Y (la espalda)
            k = 1.0 + amp * t * math.cos(flutes * (a - math.pi / 2) * 2.0)
            row.append(bm.verts.new((math.cos(a) * r * k, cy + math.sin(a) * r * k * depth, z)))
        rows.append(row)
    for j in range(nv):
        for i in range(nu):
            f = bm.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
            f.smooth = True
    return parts._finish(name, bm, mat)


def _shade(col, k):
    """`col` ('#rrggbb') oscurecido o aclarado por `k` (0.7 = 30% mas oscuro)."""
    r, g, b = hexc(col)
    return '#%02x%02x%02x' % tuple(int(max(0, min(255, c * 255 * k))) for c in (r, g, b))


# =============================================================================
# Face (craneo: centro (0,0,1.625), r 0.130, escala (0.90, 0.98, 1.10); front = -Y)
# =============================================================================
# Una cara ADULTA se lee por sus sombras, no por el blanco de los ojos: el arco de la ceja
# avanza y deja los ojos hundidos, el parpado de arriba los entorna, la nariz es una cuna
# que sale de la frente y la mandibula se estrecha hacia el menton. Internal features get
# NO outline from the post pass (it only dilates the silhouette): el contraste lo ponen el
# cavity de Workbench (que oscurece cada hueco) y el color.
EYE_X, EYE_Y, EYE_Z = 0.045, -0.112, 1.645
EYE_R, EYE_FLAT = 0.021, 0.45      # sclera radius and its squash along Y (era 0.039: boton)
IRIS_Y, IRIS_R = -0.119, 0.0125
BROW_Z, BROW_Y = 1.680, -0.131
# degrees the OUTER end of the brow rises (mirrored per side): >0 = ceno duro, <0 = afligido
BROW_TILT = {'calm': 4.0, 'noble': 10.0, 'angry': 20.0, 'wild': 13.0, 'sad': -14.0}
# Piezas de la cara que un jefe con otra cara (espectro, calavera) tiene que quitar
FACE_KEYS = ('eye_L', 'eye_R', 'iris_L', 'iris_R', 'lid_L', 'lid_R', 'brow_L', 'brow_R',
             'brow_ridge', 'nose', 'nose_tip', 'cheek_L', 'cheek_R', 'mouth')


def _face(name, spec, A, skin):
    """Eyes, brows, nose and mouth, all riding the head bone (so HEAD_SCALE grows them)."""
    if not spec.get('face', True):
        return
    white = get_mat('eye_white', vivid(hexc('#e6ddcc'), 1.0))
    iris = _mat(name, 'iris', spec.get('eyes', '#241a14'))
    brow_col = spec.get('brow_color') or spec.get('hair') or spec.get('beard') or '#2a1a12'
    brow_mat = _mat(name, 'brow', brow_col)
    tilt = BROW_TILT.get(spec.get('brow', 'calm'), 0.0)
    fem = bool(spec.get('slim'))      # las mujeres: arco y nariz mas finos, sin menton marcado
    # Arco superciliar: la repisa de hueso que da sombra a los ojos. Es lo que mas envejece
    # una cara de muneco — sin el, los ojos flotan en una bola.
    A(box(f'{name}_brow_ridge', (0, -0.108 if fem else -0.114, 1.672),
          (0.160, 0.026, 0.022) if fem else (0.175, 0.036, 0.030), skin, round=0.95), 'head')
    for s, sx in (('L', 1.0), ('R', -1.0)):
        x = EYE_X * sx
        A(sphere(f'{name}_eye_{s}', (x, EYE_Y, EYE_Z), EYE_R, white,
                 scale=(1.30, EYE_FLAT, 0.72)), 'head')
        A(sphere(f'{name}_iris_{s}', (x, IRIS_Y, EYE_Z - 0.002), IRIS_R, iris,
                 scale=(1.0, 0.5, 1.05)), 'head')
        # Parpado de arriba: tapa el tercio alto del ojo -> mirada entornada, no de susto
        A(box(f'{name}_lid_{s}', (x, -0.117, EYE_Z + 0.011), (0.056, 0.016, 0.013), skin,
              rot=Matrix.Rotation(math.radians(-tilt * 0.5 * sx), 4, 'Y'), round=0.9), 'head')
        A(box(f'{name}_brow_{s}', (x, BROW_Y, BROW_Z), (0.060, 0.016, 0.015), brow_mat,
              rot=Matrix.Rotation(math.radians(-tilt * sx), 4, 'Y'), round=0.9), 'head')
        # Pomulo: bajo el ojo, un poco hacia fuera
        A(sphere(f'{name}_cheek_{s}', (0.060 * sx, -0.084, 1.606), 0.028, skin,
                 scale=(1.0, 0.7, 0.62)), 'head')
    # Nariz en CUNA (estrecha arriba, ancha abajo) que nace del arco de la ceja; la bola de
    # antes era la nariz de un payaso.
    nk = 0.8 if fem else 1.0
    A(box(f'{name}_nose', (0, -0.125, 1.613), (0.030 * nk, 0.032 * nk, 0.068), skin,
          round=0.5, taper=(0.5, 0.45)), 'head')
    A(sphere(f'{name}_nose_tip', (0, -0.138 if fem else -0.141, 1.586), 0.019 * nk, skin,
             scale=(1.3, 0.9, 0.85)), 'head')
    if spec.get('mouth', True) and not spec.get('beard'):
        A(box(f'{name}_mouth', (0, -0.122, 1.540), (0.046, 0.012, 0.010),
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
    # Craneo ovalado (mas alto que ancho) + mandibula que se estrecha + menton: la bola
    # perfecta era la mitad del muneco. Ocupa el mismo sitio que la esfera de antes (r 0.135
    # en z 1.62), asi que casco, corona, pano y capucha siguen encajando sin tocarlos.
    A(sphere(f'{name}_head', (0, 0, 1.625), 0.130, skin, scale=(0.90, 0.98, 1.10)), 'head')
    fem = bool(spec.get('slim'))
    A(sphere(f'{name}_jaw', (0, -0.035, 1.555), 0.090, skin,
             scale=(0.92, 0.96, 0.88) if fem else (1.0, 1.0, 0.90)), 'head')
    if not fem:
        A(sphere(f'{name}_chin', (0, -0.090, 1.497), 0.034, skin, scale=(1.3, 0.9, 0.9)), 'head')
    for s, x in (('L', 0.115), ('R', -0.115)):
        A(sphere(f'{name}_ear_{s}', (x, 0.008, 1.615), 0.030, skin, scale=(0.5, 0.85, 1.25)), 'head')
    A(tube(f'{name}_neck', (0, 0, 1.43), (0, 0, 1.53), 0.062, 0.058, skin), 'neck')
    _face(name, spec, A, skin)
    if spec.get('hair'):
        hair = M('hair', spec['hair'])
        # Casquete: arranca por encima del arco de la ceja (z 1.70) y cae por detras. Todo lo
        # que baje del entrecejo tapa la cara y devuelve la "bola de pelo" de antes.
        A(sphere(f'{name}_hair', (0, 0.030, 1.700), 0.132, hair, scale=(0.94, 1.0, 0.72)), 'head')
        # Raya en medio: dos flequillos que bajan hacia las sienes. El bloque recto de antes
        # era una cinta (o el borde de un casco).
        for k, sx in (('', 1.0), ('2', -1.0)):
            A(box(f'{name}_hair_fringe{k}', (0.050 * sx, -0.078, 1.712), (0.105, 0.080, 0.036),
                  hair, rot=Matrix.Rotation(math.radians(-13.0 * sx), 4, 'Y'),
                  round=0.85, taper=(0.82, 0.9)), 'head')
        for s, x in (('L', 0.110), ('R', -0.110)):
            A(sphere(f'{name}_hair_side_{s}', (x, 0.045, 1.640), 0.058, hair,
                     scale=(0.6, 1.0, 1.05)), 'head')
        if spec.get('hair_long'):
            A(box(f'{name}_hair_back', (0, 0.100, 1.500), (0.205, 0.095, 0.29), hair,
                  round=0.6, taper=(0.8, 0.85)), 'head')
            if spec.get('brow') == 'wild':
                # Melena de profeta del desierto: tres mechones que se abren por la espalda.
                # Elias es "varon velludo" (2 Reyes 1:8); con el bloque liso parecia peluca.
                for i, (dx, dy, dz) in enumerate(((0.07, -0.02, 1.775), (-0.06, 0.04, 1.785),
                                                  (0.0, 0.09, 1.76))):
                    A(sphere(f'{name}_hair_tuft{i}', (dx, dy, dz), 0.055, hair,
                             scale=(1.1, 1.0, 0.55)), 'head')
                for i, (dx, rz) in enumerate(((-0.075, -14.0), (0.0, 0.0), (0.075, 14.0))):
                    A(box(f'{name}_hair_lock{i}', (dx, 0.105, 1.335), (0.075, 0.07, 0.15),
                          hair, rot=Matrix.Rotation(math.radians(rz), 4, 'Y'),
                          round=0.8, taper=(1.3, 1.15)), 'head')
    elif spec.get('fringe'):
        # Calvo con corona de pelo por detras y sobre las orejas (Eliseo, 2 Reyes 2:23): un
        # craneo pelado del todo es un bebe; con el cerco de pelo es un hombre maduro.
        A(sphere(f'{name}_hair_fringe', (0, 0.050, 1.598), 0.124, M('fringe', spec['fringe']),
                 scale=(1.0, 0.95, 0.40)), 'head')
    if spec.get('beard'):
        beard = M('beard', spec['beard'])
        A(sphere(f'{name}_beard', (0, -0.058, 1.520), 0.095, beard, scale=(0.98, 0.92, 0.88)), 'head')
        A(sphere(f'{name}_mustache', (0, -0.126, 1.560), 0.042, beard, scale=(1.35, 0.5, 0.42)), 'head')
        # Patillas: atan la barba al pelo (o a la sien del calvo). Sin ellas la barba es
        # una bola pegada a la cara, como la de un disfraz.
        for s, x in (('L', 0.097), ('R', -0.097)):
            A(box(f'{name}_sideburn_{s}', (x, -0.030, 1.585), (0.030, 0.070, 0.100), beard,
                  round=0.8), 'head')
        if spec.get('beard_long'):
            A(tube(f'{name}_beard2', (0, -0.075, 1.490), (0, -0.058, 1.360), 0.078, 0.030,
                   beard, rim_round=0.02), 'head')
            A(sphere(f'{name}_beard_tip', (0, -0.058, 1.362), 0.030, beard, scale=(1, 1, 0.9)), 'head')
    if spec.get('helmet'):
        helm = M('helmet', spec['helmet'])
        # Cupula alta + aro abierto + nasal: por delante la esfera sube hasta z~1.66, justo
        # sobre las cejas, asi que el rostro queda a la vista.
        A(sphere(f'{name}_helm', (0, 0.008, 1.752), 0.136, helm, scale=(0.94, 1, 0.74)), 'head')
        A(_squash_y(tube(f'{name}_helm_rim', (0, 0, 1.698), (0, 0, 1.742), 0.146, 0.146, helm,
                         caps=False, rim_round=0.01), 1.0), 'head')
        A(box(f'{name}_helm_nasal', (0, -0.138, 1.642), (0.028, 0.032, 0.110), helm, round=0.8), 'head')
        for s, x in (('L', 0.118), ('R', -0.118)):
            A(box(f'{name}_helm_cheek_{s}', (x, -0.030, 1.578), (0.022, 0.092, 0.100), helm,
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
    # Tronco en V: pecho ancho arriba, cintura estrecha. Biselado entero (round 0.9) porque
    # con 0.55 cada caja dejaba una cara plana con su marco y el cavity lo dibujaba como el
    # panel de un robot de juguete.
    slim = 0.85 if spec.get('slim') else 1.0
    A(_soft(box(f'{name}_torso_lo', (0, 0, 1.15), (0.31 * slim, 0.20, 0.24), tunic, round=0.9,
                taper=(1.14, 1.04))), 'spine')
    chest_mat = M('armor', spec['armor']) if spec.get('armor') else tunic
    A(_soft(box(f'{name}_torso_up', (0, 0, 1.355), (0.39 * slim, 0.225, 0.24), chest_mat,
                round=0.9, taper=(1.10, 0.92))), 'chest')
    for s, x in (('L', 0.215), ('R', -0.215)):
        A(sphere(f'{name}_shoulder_{s}', (x, 0, 1.415), 0.082, chest_mat,
                 scale=(1.12, 1.0, 0.86)), 'chest')
    if spec.get('belt'):
        # Ceñido a la cintura (y aplastado como la tunica): el aro redondo de antes sobresalia
        # cinco centimetros por delante y se leia como un flotador.
        A(_squash_y(tube(f'{name}_belt', (0, 0, 1.012), (0, 0, 1.068), 0.198, 0.198,
                         M('belt', spec['belt']), rim_round=0.016), 0.80), 'hips')
    if spec.get('sash'):
        A(box(f'{name}_sash', (0, -0.115, 1.25), (0.09, 0.02, 0.44), M('sash', spec['sash']),
              round=0.9), 'spine')
    # tunic skirt / robe. Con parts.DETAIL (paneles) la tela cae con pliegues: nueve
    # lobulos que crecen hacia el bajo. En una hoja de 128 px no se ven y no se piden.
    # Desde 2026-11-10 las hojas tambien llevan pliegues (suaves): el cono liso era lo que
    # mas leia "plastico" en una tunica de 60 px. En los paneles (DETAIL) van mas marcados.
    folds = dict(flutes=9, flute_amp=0.07) if parts.DETAIL else dict(flutes=7, flute_amp=0.05)
    if spec.get('tunic_len', 'short') == 'long':
        robe_col = spec.get('robe', spec['tunic'])
        robe = M('robe', robe_col)
        r_waist, r_hem = (0.165, 0.335) if spec.get('slim') else (0.19, 0.30)
        A(_squash_y(tube(f'{name}_robe', (0, 0, 1.00), (0, 0, 0.10), r_waist, r_hem, robe,
                         segs=32, rim_round=0.03, **folds), 0.80), 'hips')
        # Orla del bajo, un tono mas oscura: el borde gastado de una prenda que se lleva
        A(_squash_y(tube(f'{name}_robe_hem', (0, 0, 0.175), (0, 0, 0.095), r_hem * 0.965,
                         r_hem * 1.03, M('robe_hem', _shade(robe_col, 0.72)), segs=32,
                         rim_round=0.02, **folds), 0.80), 'hips')
    else:
        A(_squash_y(tube(f'{name}_skirt', (0, 0, 1.00), (0, 0, 0.62), 0.19, 0.24, tunic,
                         segs=32, rim_round=0.03, **folds), 0.80), 'hips')
    if spec.get('mantle'):
        mantle = M('mantle', spec['mantle'])
        # Manto que CAE por la espalda hasta las corvas y se abre abajo, con cuello vuelto:
        # la losa corta de antes era una mochila.
        # Capa de verdad: abraza los hombros y cae por la espalda abriendose hasta las
        # corvas, con pliegues. Las cajas de antes eran una tabla y dos hombreras de rugby.
        A(_cloak(f'{name}_mantle', mantle, 1.47, 0.62, 0.215, 0.33, arc=230.0, cy=0.025,
                 depth=0.70), 'chest')
        A(tube(f'{name}_mantle_collar', (0, 0.012, 1.425), (0, 0.012, 1.50), 0.135, 0.100,
               mantle, rim_round=0.02, caps=False), 'chest')
    if spec.get('cape'):
        cape = M('cape', spec['cape'])
        A(box(f'{name}_cape', (0, 0.15, 1.02), (0.40, 0.05, 0.88), cape,
              round=0.9, taper=(0.78, 1.0)), 'chest')

    # --- arms -----------------------------------------------------------
    # Brazo con musculo (biceps mas grueso que la muneca) y MANO cerrada mas pequena: la
    # bola de 0.055 era la mano de un playmobil.
    for s, x in (('L', 0.22), ('R', -0.22)):
        A(tube(f'{name}_uarm_{s}', (x, 0, 1.42), (x, 0, 1.16), 0.062, 0.050, sleeves,
               rim_round=0.015), 'upper_arm.' + s)
        fmat = M('bracer', spec['bracer']) if spec.get('bracer') else skin
        A(tube(f'{name}_farm_{s}', (x, 0, 1.16), (x, 0, 0.92), 0.052, 0.038, fmat,
               rim_round=0.012), 'forearm.' + s)
        A(sphere(f'{name}_hand_{s}', (x, -0.004, 0.885), 0.044, skin, scale=(0.82, 0.95, 1.18)),
          'hand.' + s)
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
            # Gota de llama, no piruleta: la esfera de 0.10 era mas grande que la cabeza nueva
            tip_obj = sphere(f'{name}_tip', tip_center + Vector((0, 0, 0.03)), 0.078, fire, scale=(1, 1, 1.45))
            tip_child = sphere(f'{name}_tip_core', tip_center + Vector((0, 0, 0.05)), 0.042, core, scale=(1, 1, 1.6))
        elif w == 'staff_light':
            light = M('light', wc.get('tip', '#eaf6ff'))
            halo = M('light_halo', wc.get('core', '#8ad0ff'))
            tip_obj = sphere(f'{name}_tip', tip_center + Vector((0, 0, 0.02)), 0.066, light)
            tip_child = tube(f'{name}_tip_halo', tip_center + Vector((0, 0, -0.03)), tip_center + Vector((0, 0, 0.07)), 0.088, 0.088, halo, segs=6, caps=False)
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
        skin='#d6a476', hair='#2a1a12', hair_long=True, beard='#2a1a12', beard_long=True,
        eyes='#20303c', brow='wild',
        tunic='#b8682a', tunic_len='long', robe='#94501f', sleeves='#b8682a',
        belt='#4a2e16', mantle='#4a2c1a', shoes='#3e2814',
        weapon='staff_fire', weapon_colors=dict(wood='#5a3a1a', tip='#ff7a12', core='#ffd23f'),
        attack='swing')),
    'eliseo': ('humanoid', dict(
        skin='#d09a66', hair=None, fringe='#4a3628', beard='#4a3628',
        eyes='#2d3f5e', brow='noble',
        tunic='#3a66a6', tunic_len='long', robe='#2c5288', sleeves='#3a66a6',
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
        skin='#d09a66', hair=None, fringe='#4a3628', beard='#4a3628',
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
    # Cap 3. Mayordomo del palacio de Acab (1 Reyes 18:3): hombre de corte, no de tropa.
    # Lo que lo separa de los otros locutores a 24 unidades es el PELO Y LA BARBA CANOSOS —
    # es el unico cano del juego — mas el pano blanco con cordon de oro y el indigo de casa
    # real. Nada de casco ni penacho: no es un soldado, es el que lleva la casa.
    'abdias': ('humanoid', dict(
        skin='#d9a066', hair='#8d8377', beard='#8d8377',
        eyes='#3a3a34', brow='sad', mouth_color='#7a4438',
        headcloth='#efe9d8', headcloth_cord='#c9a24a',
        tunic='#3a4270', tunic_len='long', robe='#2b3055', sleeves='#454e80',
        belt='#c9a24a', sash='#c9a24a', shoes='#3a3028',
        weapon=None, attack='raise')),
    # Caps 7 y 8. Uno de los hijos de los profetas (la olla, el hacha prestada). Es el UNICO
    # locutor con la cabeza descubierta y el pelo largo suelto: viven en comunidad, pobres, y
    # el pasaje del hacha deja claro que ni un hierro es suyo. Lana cruda sin tenir.
    'hijo_profetas': ('humanoid', dict(
        skin='#c98c5a', hair='#3f2c1c', hair_long=True, beard='#3f2c1c',
        eyes='#2a1e14', brow='calm', mouth_color='#7a4438',
        tunic='#9d9382', tunic_len='long', robe='#83795f', sleeves='#9d9382',
        belt='#5f4a2e', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 7. El de Baal-salisa trae las primicias (2 Reyes 4:42): labrador, no profeta.
    # Va en la gama de la MIES — pano y tunica de paja y mostaza — que no la usa nadie mas.
    'hombre_baalsalisa': ('humanoid', dict(
        skin='#c07f47', hair='#6b4a26', beard='#6b4a26',
        eyes='#2a1e14', brow='calm', mouth_color='#7a4438',
        headcloth='#d9b45e', headcloth_cord='#8a6a30',
        tunic='#b58a3c', tunic_len='short', sleeves='#9c742f',
        belt='#6b5230', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 9. La muchacha israelita cautiva (2 Reyes 5:2): toda la historia arranca en ella.
    # Es la UNICA mujer del juego con la cabeza DESCUBIERTA, y es lo que dice lo que es: una
    # esclava no lleva velo de casada. Sin oro, sin cinta, lana pobre.
    'muchacha_cautiva': ('humanoid', dict(
        skin='#e0b080', hair='#2a1a12', hair_long=True, beard=None, slim=True,
        eyes='#3a2a1e', brow='sad', mouth_color='#8a4a44',
        tunic='#8f9a86', tunic_len='long', robe='#77826f', sleeves='#8f9a86',
        belt='#5a5248', shoes='#4a3018',
        weapon=None, attack='raise')),
    # Cap 10. Uno de los cuatro leprosos a la puerta de Samaria (2 Reyes 7:3). Son los HEROES
    # del pasaje, nunca enemigos (regla del proyecto, ver el caso del "Leproso Enloquecido").
    # Capucha echada y ABIERTA — calada taparia la cara entera — piel apagada y ceniza en todo.
    'leproso_samaria': ('humanoid', dict(
        skin='#c3a487', hair='#5a5248', beard='#7d766a', slim=True,
        eyes='#4a4238', brow='sad', mouth_color='#6f4038',
        hood='#8a8276', hood_open=True,
        tunic='#6e6a62', tunic_len='long', robe='#585449', sleeves='#6e6a62',
        belt='#4a443a', shoes='#3a3630',
        weapon=None, attack='raise')),
    # Cap 10. El principe sobre cuyo brazo se apoya el rey (2 Reyes 7:2). Corte de Samaria:
    # peto y capa AZUL con plata, sin nada en la cabeza mas que un aro liso. Asi no se
    # confunde ni con Naaman (teal y oro) ni con el General Sirio (casco con penacho).
    'principe_samaria': ('humanoid', dict(
        skin='#d9a066', hair='#1a1410', beard='#1a1410',
        eyes='#242a34', brow='noble', mouth_color='#7a3a34',
        band='#c8ccd4',
        tunic='#2f4a86', tunic_len='long', robe='#233a66', sleeves='#3a5a9e',
        armor='#c8ccd4', belt='#c8ccd4', cape='#1d2f5e', bracer='#c8ccd4', shoes='#2a2a2a',
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
    # Los jefes (bosses.py, 2026-10-03) van por su propio registro: humanoide + piezas propias.
    from . import bosses
    if name in bosses.SPECS:
        return bosses.build_boss(name, bosses.SPECS[name])
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
