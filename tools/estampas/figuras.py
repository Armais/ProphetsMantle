"""Figuras de las estampas: los MISMOS munecos del juego (characters.py + bosses.py), posados
y renderizados en Blender para recortarlos y componerlos sobre los fondos 2D de plates.py.

Corre bajo `bpy` (Blender 4.2 de PyPI, ver tools/blender/CLAUDE.md):

    EGL_PLATFORM=surfaceless <venv-bpy>/bin/python tools/estampas/figuras.py [clave ...]

Escribe tools/estampas/figs/<clave>.png (RGBA recortado a la silueta) y figs/index.json con
la escala (metros por pixel) y el punto de apoyo, para que plates.py ponga cada figura a la
altura que le toca por su distancia: un humano mide 1,75 m en todas las laminas.

Por que figuras 3D y no dibujadas: una persona en SDF es un trapecio con una bola encima. La
figurita del juego ya tiene anatomia, ropa y la cara que el jugador conoce; aqui solo se la
posa (poses de cine.py + las de abajo) y la lamina le pone la luz (contraluz, borde, aire).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'blender'))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from mantle_sprites import parts, characters, rig, anims, render, cine  # noqa: E402
from mantle_sprites.parts import box, sphere, tube, attach, get_mat, vivid, hexc  # noqa: E402

OUT = os.path.join(HERE, 'figs')
MPP = 2.6 / 560.0          # metros por pixel de salida (un humano de 1,75 m = ~377 px)
SS = 2
ELEV = 7.0                 # grados de picado: casi a la altura de los ojos, como una estampa


# =============================================================================
# Poses (ejes de mundo, ver rig.pose_apply; X negativo en un brazo = al frente)
# =============================================================================
P = {
    'stand': {},
    'kneel': cine.POSE_KNEEL,
    'point': cine.POSE_POINT,
    'proud': cine.POSE_STAND_PROUD,
    'guard': cine.POSE_GUARD,
    'walk': cine.POSE_WALK,
    'raise': cine.POSE_RAISE,
    'lookup': cine.POSE_LOOKUP,
    'drink': cine.POSE_DRINK,
    'throne': cine.POSE_ENTHRONED,
    'cry': cine.POSE_CRY,
    'recoil': cine.POSE_RECOIL,
    # de rodillas con los brazos al cielo (la oracion de Querit, el Carmelo)
    # El brazo ALZADO es el izquierdo: el derecho lleva el cayado y alzarlo lo vuelca. Por eso
    # estas poses se ruedan mirando a la IZQUIERDA (giro negativo): el brazo alzado queda del
    # lado de la camara y no detras de la cabeza.
    'kneel_up': dict(cine.POSE_KNEEL, **{
        'upper_arm.L': {'rot': [('X', -150.0), ('Y', 20.0)]}, 'forearm.L': {'rot': [('X', -14.0)]},
        'upper_arm.R': {'rot': [('X', -4.0), ('Y', -14.0)]}, 'forearm.R': {'rot': [('X', -10.0)]},
        'head': {'rot': [('X', -20.0)]}, 'chest': {'rot': [('X', -8.0)]}, 'spine': {'rot': [('X', 2.0)]}}),
    # de rodillas, inclinado, la cara entre las rodillas (1 R 18:42)
    'bow': dict(cine.POSE_KNEEL, **{
        'spine': {'rot': [('X', 34.0)]}, 'chest': {'rot': [('X', 28.0)]}, 'head': {'rot': [('X', 30.0)]},
        'upper_arm.L': {'rot': [('X', -40.0), ('Y', 10.0)]}, 'forearm.L': {'rot': [('X', -70.0)]},
        'upper_arm.R': {'rot': [('X', -30.0), ('Y', -10.0)]}, 'forearm.R': {'rot': [('X', -40.0)]}}),
    # sentado en una piedra, abatido (el arroyo seco, el enebro)
    'sit_sad': dict(cine.POSE_ENTHRONED, **{
        'spine': {'rot': [('X', 22.0)]}, 'chest': {'rot': [('X', 14.0)]}, 'head': {'rot': [('X', 34.0)]},
        'upper_arm.L': {'rot': [('X', -52.0), ('Y', 8.0)]}, 'forearm.L': {'rot': [('X', -60.0)]},
        'upper_arm.R': {'rot': [('X', -20.0), ('Y', -12.0)]}, 'forearm.R': {'rot': [('X', -30.0)]}}),
    # de pie con las dos manos al frente (ofrecer, recibir, sostener una vasija)
    'offer': {
        'upper_arm.L': {'rot': [('X', -55.0), ('Y', 8.0)]}, 'forearm.L': {'rot': [('X', -40.0)]},
        'upper_arm.R': {'rot': [('X', -55.0), ('Y', -8.0)]}, 'forearm.R': {'rot': [('X', -40.0)]},
        'head': {'rot': [('X', 10.0)]}},
    # verter (el aceite de la viuda, los cantaros del Carmelo): brazos al frente y abajo
    'pour': {
        'upper_arm.L': {'rot': [('X', -70.0), ('Y', 6.0)]}, 'forearm.L': {'rot': [('X', -10.0)]},
        'upper_arm.R': {'rot': [('X', -60.0), ('Y', -6.0)]}, 'forearm.R': {'rot': [('X', -20.0)]},
        'spine': {'rot': [('X', 14.0)]}, 'head': {'rot': [('X', 18.0)]}},
    # de pie, brazos al cielo (el milagro, el cayado en alto)
    'arms_up': {
        'upper_arm.L': {'rot': [('X', -160.0), ('Y', 22.0)]}, 'forearm.L': {'rot': [('X', -6.0)]},
        'upper_arm.R': {'rot': [('X', 2.0), ('Y', -24.0)]}, 'forearm.R': {'rot': [('X', -10.0)]},
        'head': {'rot': [('X', -20.0)]}, 'chest': {'rot': [('X', -8.0)]}},
    # el rostro cubierto con el manto (1 R 19:13)
    'veiled': {
        'upper_arm.L': {'rot': [('X', -130.0), ('Y', -18.0)]}, 'forearm.L': {'rot': [('X', -70.0)]},
        'head': {'rot': [('X', 16.0)]}, 'chest': {'rot': [('X', 6.0)]}},
    # dormido de costado (bajo el enebro, el lecho de Eliseo)
    'lie': {'root': {'rot': [('Y', 84.0)], 'loc': (-0.95, 0.0, 0.25)},
            'head': {'rot': [('X', 8.0)]},
            'upper_arm.L': {'rot': [('X', -40.0)]}, 'upper_arm.R': {'rot': [('X', -30.0)]},
            'forearm.L': {'rot': [('X', -50.0)]}, 'thigh.L': {'rot': [('X', -25.0)]},
            'shin.L': {'rot': [('X', 35.0)]}},
    # agachado recogiendo (la viuda y sus dos lenos)
    'gather': {
        'spine': {'rot': [('X', 34.0)]}, 'chest': {'rot': [('X', 16.0)]}, 'head': {'rot': [('X', -10.0)]},
        'upper_arm.L': {'rot': [('X', -40.0), ('Y', 6.0)]}, 'forearm.L': {'rot': [('X', -20.0)]},
        'upper_arm.R': {'rot': [('X', -50.0), ('Y', -6.0)]}, 'forearm.R': {'rot': [('X', -30.0)]},
        'thigh.L': {'rot': [('X', -18.0)]}, 'thigh.R': {'rot': [('X', 12.0)]}},
    # tantear a ciegas (el ejercito ciego de Dotan)
    'blind': {
        'upper_arm.L': {'rot': [('X', -80.0), ('Y', 16.0)]}, 'forearm.L': {'rot': [('X', -6.0)]},
        'upper_arm.R': {'rot': [('X', -70.0), ('Y', -16.0)]}, 'forearm.R': {'rot': [('X', -6.0)]},
        'head': {'rot': [('X', -14.0)]},
        'thigh.L': {'rot': [('X', 16.0)]}, 'shin.L': {'rot': [('X', -10.0)]},
        'thigh.R': {'rot': [('X', -14.0)]}, 'shin.R': {'rot': [('X', -18.0)]}},
    # tirar una flecha (Joas por la ventana del oriente)
    'shoot': {
        'upper_arm.L': {'rot': [('X', -88.0), ('Y', 2.0)]}, 'forearm.L': {'rot': [('X', 0.0)]},
        'upper_arm.R': {'rot': [('X', -84.0), ('Y', -40.0)]}, 'forearm.R': {'rot': [('X', -120.0)]},
        'chest': {'rot': [('Z', -12.0)]}, 'head': {'rot': [('Z', 10.0)]}},
}


# =============================================================================
# Variantes de reparto: personajes que solo salen en una lamina (sin hoja ni retrato)
# =============================================================================
def _derive(base, **over):
    kind, spec = characters.SPECS[base]
    d = dict(spec)
    d.update(over)
    return (kind, d)


EXTRA = {
    'fig_angel': _derive('eliseo', skin='#f2e2c8', tunic='#f4f0e6', robe='#e8e4da', sleeves='#f4f0e6',
                         mantle='#fff8e8', belt='#e8c870', sash='#e8c870', beard=None, hair='#e8d8a8',
                         hair_long=True, weapon=None, attack='raise'),
    'fig_nino': _derive('criado_eliseo', skin='#e0b080', headcloth=None, hair='#2a1a12', tunic='#e8dcc0',
                        sleeves='#e8dcc0', weapon=None),
    'fig_joram': _derive('acab', crown='#c8ccd4', gem='#2a4aa8', tunic='#2f4a86', robe='#233a66',
                         sleeves='#3a5a9e', armor='#c8ccd4', cape='#1d2f5e', hair='#2a1a12', beard='#2a1a12',
                         weapon=None, attack='raise'),
    'fig_joas': _derive('acab', crown='#d4a537', gem='#2e7d5b', tunic='#3a5a2a', robe='#2a4420',
                        sleeves='#4a6a34', armor='#c9a24a', cape='#5a2a1a', beard=None, hair='#3a2418',
                        weapon='bow', weapon_colors=dict(wood='#5a3a1a'), attack='bow'),
    'fig_jehu': _derive('general_sirio', helmet=None, plume=None, hair='#2a1a12', beard='#2a1a12',
                        tunic='#8a6a2a', sleeves='#8a6a2a', armor='#9a7a3a', cape='#5a2a1a',
                        weapon=None, attack='raise'),
    'fig_hazael': _derive('naaman', band=None, headcloth='#c8b890', headcloth_cord='#5a4634',
                          tunic='#7a3a2a', robe='#5a2a1e', sleeves='#8a4a32', cape='#3a1a14',
                          armor='#b08d3a'),
    'fig_mesa': _derive('acab', crown='#b08d3a', gem='#7a1f1f', tunic='#4a3a2a', robe='#3a2c20',
                        sleeves='#5a4632', armor='#8a6a3a', cape='#2a1a12', weapon=None, attack='raise'),
    'fig_naaman_limpio': _derive('naaman', armor=None, cape=None, tunic='#e8e0cc', robe='#d8d0bc',
                                 sleeves='#e8e0cc', belt='#c9a24a', bracer=None),
    'fig_eliseo_viejo': _derive('eliseo', beard='#d8d4cc', hair='#d8d4cc', weapon=None, attack='raise'),
    'fig_mozo': _derive('criado_eliseo', headcloth='#8a6a4a', tunic='#9a8a6a', sleeves='#8a7a5a'),
    'fig_esposa_naaman': _derive('sunamita', tunic='#1f7a68', robe='#155a4d', sleeves='#2a9c85',
                                 headcloth='#efe4cc', band='#d4a537', gem='#8e1f1f'),
    'fig_muerto': _derive('hijo_profetas', tunic='#e8e4d8', robe='#d8d4c8', sleeves='#e8e4d8', hair='#3a2a1a',
                          beard='#3a2a1a', belt='#b8b0a0'),
    'fig_capitan50': _derive('soldado_acab', beard='#2a1a12', plume='#c0392b', helmet='#b8862e',
                             tunic='#1f4e7a', sleeves='#1f4e7a', weapon=None, shield=None, attack='raise'),
}
for k, v in EXTRA.items():
    characters.SPECS[k] = v


# =============================================================================
# Objetos en la mano (se cuelgan del hueso de la mano DESPUES de construir)
# =============================================================================
def _M(key, c):
    return get_mat(f'fig_{key}', vivid(hexc(c), 1.25))


def prop_jar(arm, name, hand='hand.L', col='#b8743a'):
    x = 0.22 if hand.endswith('L') else -0.22
    attach(sphere(f'{name}_pjar', (x, -0.12, 0.72), 0.13, _M('jar', col), scale=(1, 1, 1.15)), arm, hand)
    attach(tube(f'{name}_pjar_n', (x, -0.12, 0.84), (x, -0.12, 0.92), 0.06, 0.07, _M('jar', col)), arm, hand)


def prop_sticks(arm, name):
    for k in range(2):
        attach(tube(f'{name}_stick{k}', (0.16 + k * 0.08, -0.16, 0.70), (0.30 + k * 0.08, -0.20, 1.02),
                    0.018, 0.014, _M('stick', '#6a4a2a')), arm, 'hand.L')


def prop_lamp(arm, name, hand='hand.L'):
    x = 0.22 if hand.endswith('L') else -0.22
    attach(sphere(f'{name}_lamp', (x, -0.12, 0.84), 0.07, _M('lampclay', '#b8743a'), scale=(1.4, 1, 0.6)),
           arm, hand)
    attach(sphere(f'{name}_flame', (x + 0.08, -0.12, 0.90), 0.035, _M('flame', '#ffd23f'), scale=(1, 1, 1.6)),
           arm, hand)


def prop_horn(arm, name):
    attach(tube(f'{name}_horn', (0.22, -0.12, 0.80), (0.30, -0.18, 1.00), 0.05, 0.02, _M('horn', '#e8dcc0')),
           arm, 'hand.L')


def prop_bowl(arm, name):
    attach(sphere(f'{name}_bowl', (0.0, -0.30, 1.05), 0.12, _M('bowl', '#d8c8a8'), scale=(1.3, 1.3, 0.5)),
           arm, 'hand.L')
    attach(sphere(f'{name}_salt', (0.0, -0.30, 1.09), 0.09, _M('salt', '#f8f8f0'), scale=(1.3, 1.3, 0.35)),
           arm, 'hand.L')


def prop_basket(arm, name):
    attach(tube(f'{name}_basket', (0.0, -0.30, 0.92), (0.0, -0.30, 1.08), 0.18, 0.22, _M('basket', '#b8904a')),
           arm, 'hand.L')
    for k in range(4):
        attach(sphere(f'{name}_loaf{k}', (-0.09 + k * 0.06, -0.30 + (k % 2) * 0.05, 1.10), 0.06,
                      _M('loaf', '#d09a5a'), scale=(1.3, 1, 0.7)), arm, 'hand.L')


def prop_flask(arm, name):
    attach(sphere(f'{name}_flask', (-0.22, -0.14, 0.86), 0.07, _M('flask', '#d8c070'), scale=(1, 1, 1.2)),
           arm, 'hand.R')
    attach(tube(f'{name}_flask_n', (-0.22, -0.14, 0.92), (-0.22, -0.14, 1.0), 0.025, 0.02, _M('flask', '#d8c070')),
           arm, 'hand.R')


PROPS = {'jar': prop_jar, 'sticks': prop_sticks, 'lamp': prop_lamp, 'horn': prop_horn,
         'bowl': prop_bowl, 'basket': prop_basket, 'flask': prop_flask}


# =============================================================================
# Reparto: clave -> (personaje, pose, giro, [objetos])
# giro: 0 = de frente; +35 = tres cuartos mirando a la DERECHA de la lamina; -35 a la izquierda
# =============================================================================
FIGS = {
    'elias_kneel_up_l': ('elias', 'kneel_up', -40, []),
    'elias_walk_r': ('elias', 'walk', 50, []),
    'elias_walk_l': ('elias', 'walk', -50, []),
    'elias_stand_r': ('elias', 'stand', 30, []),
    'elias_stand_l': ('elias', 'stand', -30, []),
    'elias_point_r': ('elias', 'point', 40, []),
    'elias_point_l': ('elias', 'point', -40, []),
    'elias_raise': ('elias', 'raise', 15, []),
    'elias_arms_up_l': ('elias', 'arms_up', -25, []),
    'elias_lookup_r': ('elias', 'lookup', 35, []),
    'elias_bow_l': ('elias', 'bow', -55, []),
    'elias_bow_r': ('elias', 'bow', 55, []),
    'elias_kneel_r': ('elias', 'kneel', 50, []),
    'elias_veiled_r': ('elias', 'veiled', 30, []),
    'elias_lie': ('elias', 'lie', 0, []),
    'elias_walk_horn_r': ('elias', 'walk', 50, ['horn']),
    'soldado_drink_l': ('soldado_acab', 'drink', -60, []),
    'capitan_kneel_l': ('fig_capitan50', 'kneel', -45, []),
    'soldado_kneel_l': ('soldado_acab', 'kneel', -40, []),
    'viuda_gather_l': ('viuda_sarepta', 'gather', -40, ['sticks']),
    'viuda_offer_r': ('viuda_sarepta', 'offer', 30, []),
    'viuda_stand_l': ('viuda_sarepta', 'stand', -30, []),
    'viuda_cry_l': ('viuda_sarepta', 'veiled', -30, []),
    'abdias_lamp_r': ('abdias', 'walk', 40, ['lamp']),
    'abdias_bow_l': ('abdias', 'bow', -50, []),
    'criado_point_r': ('criado_eliseo', 'point', 35, []),
    'criado_recoil_r': ('criado_eliseo', 'recoil', 30, []),
    'angel_reach_l': ('fig_angel', 'offer', -45, []),
    'nino_stand': ('fig_nino', 'stand', 20, []),
    'nino_lie': ('fig_nino', 'lie', 0, []),
    'eliseo_lab_stand_r': ('eliseo_labrador', 'stand', 30, []),
    'eliseo_lab_walk_l': ('eliseo_labrador', 'walk', -50, []),
    'acab_stand_l': ('acab', 'proud', -30, []),
    'acab_stand_r': ('acab', 'proud', 30, []),
    'eliseo_stand_r': ('eliseo', 'stand', 30, []),
    'eliseo_stand_l': ('eliseo', 'stand', -30, []),
    'eliseo_walk_r': ('eliseo', 'walk', 50, []),
    'eliseo_walk_l': ('eliseo', 'walk', -50, []),
    'eliseo_point_r': ('eliseo', 'point', 40, []),
    'eliseo_point_l': ('eliseo', 'point', -40, []),
    'eliseo_raise': ('eliseo', 'raise', 10, []),
    'eliseo_arms_up_l': ('eliseo', 'arms_up', -25, []),
    'eliseo_bowl_r': ('eliseo', 'offer', 35, ['bowl']),
    'eliseo_cry_r': ('eliseo', 'cry', 30, []),
    'eliseo_kneel_r': ('eliseo', 'kneel', 45, []),
    'eliseo_veiled_l': ('eliseo', 'veiled', -30, []),
    'eliseo_old_lie': ('fig_eliseo_viejo', 'lie', 0, []),
    'hombre_jerico_point_l': ('hombre_jerico', 'point', -35, []),
    'hombre_jerico_stand_l': ('hombre_jerico', 'stand', -30, []),
    'hijo_prof_stand_l': ('hijo_profetas', 'stand', -30, []),
    'hijo_prof_stand_r': ('hijo_profetas', 'stand', 30, []),
    'hijo_prof_cry_l': ('hijo_profetas', 'arms_up', -30, []),
    'hijo_prof_reach_l': ('hijo_profetas', 'drink', -50, []),
    'hijo_prof_flask_r': ('hijo_profetas', 'offer', 40, ['flask']),
    'hijo_prof_walk_l': ('hijo_profetas', 'walk', -50, []),
    'mujer_prof_pour_r': ('mujer_profeta', 'pour', 40, ['jar']),
    'baalsalisa_basket_l': ('hombre_baalsalisa', 'offer', -35, ['basket']),
    'mesa_arms_up': ('fig_mesa', 'arms_up', 0, []),
    'sunamita_lamp_r': ('sunamita', 'stand', 30, ['lamp']),
    'sunamita_stand_r': ('sunamita', 'stand', 30, []),
    'sunamita_kneel_r': ('sunamita', 'kneel', 40, []),
    'joram_throne': ('fig_joram', 'throne', -20, []),
    'naaman_stand_l': ('naaman', 'proud', -30, []),
    'naaman_limpio_up': ('fig_naaman_limpio', 'arms_up', 10, []),
    'muchacha_stand_r': ('muchacha_cautiva', 'offer', 30, []),
    'soldado_sirio_blind_r': ('soldado_sirio', 'blind', 40, []),
    'general_blind_r': ('general_sirio', 'blind', 40, []),
    'leproso_stand_r': ('leproso_samaria', 'stand', 30, []),
    'leproso_walk_r': ('leproso_samaria', 'walk', 50, []),
    'leproso_point_r': ('leproso_samaria', 'point', 35, []),
    'leproso_sit_r': ('leproso_samaria', 'sit_sad', 30, []),
    'principe_stand_r': ('principe_samaria', 'proud', 30, []),
    'hazael_offer_l': ('fig_hazael', 'offer', -35, []),
    'jehu_kneel_r': ('fig_jehu', 'kneel', 40, []),
    'joas_shoot_r': ('fig_joas', 'shoot', 60, []),
    'mozo_point_r': ('fig_mozo', 'point', 35, []),
    'guiezi_stand_r': ('criado_eliseo', 'stand', 30, []),
    'esposa_naaman_sit_l': ('fig_esposa_naaman', 'throne', -35, []),
    'buey_r': ('@buey', None, 70, []),
    'jehu_drive_r': ('fig_jehu', 'point', 40, []),
    'resucitado': ('fig_muerto', 'arms_up', 10, []),
    # Pueblo: los locutores del pozo como gente de la multitud (de pie, brazos, andando)
    'pueblo_1': ('hijo_profetas', 'stand', 20, []),
    'pueblo_2': ('hombre_jerico', 'stand', -20, []),
    'pueblo_3': ('viuda_sarepta', 'stand', 15, []),
    'pueblo_4': ('hombre_baalsalisa', 'arms_up', -15, []),
    'pueblo_5': ('criado_eliseo', 'stand', 25, []),
    'pueblo_6': ('mujer_profeta', 'stand', -25, []),
    'pueblo_7': ('leproso_samaria', 'walk', 40, []),
    'pueblo_8': ('muchacha_cautiva', 'stand', -15, []),
    'baal_stand': ('profeta_baal', 'stand', -20, []),
    'baal_up': ('profeta_baal', 'arms_up', -25, []),
    'baal_up2': ('profeta_baal', 'arms_up', 25, []),
    'soldado_sirio_stand_r': ('soldado_sirio', 'guard', 30, []),
    'soldado_acab_stand_l': ('soldado_acab', 'guard', -30, []),
}


# =============================================================================
# Render
# =============================================================================
def _setup():
    render.setup_scene(os.path.join(OUT, '_tmp'))
    sc = bpy.context.scene
    cam = sc.camera
    cam.data.type = 'ORTHO'
    cam.data.shift_x = 0.0
    cam.data.shift_y = 0.0
    return cam


def _bbox_cam(cam):
    """Extremos de todo lo visible en ejes de camara (derecha, arriba)."""
    bpy.context.view_layer.update()
    R = cam.matrix_world.to_3x3()
    right, up = R.col[0], R.col[1]
    xs, ys = [], []
    for ob in bpy.data.objects:
        if ob.type != 'MESH' or ob.hide_render:
            continue
        mw = ob.matrix_world
        for v in ob.data.vertices:
            p = mw @ v.co
            xs.append(p.dot(right))
            ys.append(p.dot(up))
    return min(xs), max(xs), min(ys), max(ys)


def render_fig(key):
    name, pose_key, yaw, props = FIGS[key]
    parts.clear_scene()
    prev = parts.set_quality('cine')
    try:
        if name.startswith('@'):
            return _render_prop(key, name[1:], yaw)
        char = characters.build(name)
        arm = char['arm']
        pose = P[pose_key]
        for pr in props:
            PROPS[pr](arm, name)
        if char['kind'] == 'humanoid' and pose:
            cine._drape_legs(name, arm, pose)
        base = anims.humanoid_base(char.get('attack', 'swing')) if char['kind'] == 'humanoid' else {}
        rig.pose_apply(arm, rig.merge_pose(base, pose) if pose else base)
        arm.rotation_euler = (0.0, 0.0, math.radians(yaw))
        cam = _setup()
        e = math.radians(ELEV)
        cam.location = Vector((0.0, -math.cos(e) * 20.0, 0.9 + math.sin(e) * 20.0))
        cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
        x0, x1, y0, y1 = _bbox_cam(cam)
        m = 0.06
        x0, x1, y0, y1 = x0 - m, x1 + m, y0 - m, y1 + m
        w_px = int(math.ceil((x1 - x0) / MPP / 2) * 2)
        h_px = int(math.ceil((y1 - y0) / MPP / 2) * 2)
        sc = bpy.context.scene
        sc.render.resolution_x = w_px * SS
        sc.render.resolution_y = h_px * SS
        cam.data.ortho_scale = max(w_px, h_px) * MPP
        # centrar la caja: el desplazamiento va en fracciones del lado MAYOR
        R = cam.matrix_world.to_3x3()
        right, up = R.col[0], R.col[1]
        c = cam.location
        cx = (x0 + x1) / 2 - c.dot(right)
        cy = (y0 + y1) / 2 - c.dot(up)
        big = max(w_px, h_px) * MPP
        cam.data.shift_x = cx / big
        cam.data.shift_y = cy / big
        hi = render._render_to_array()
        img = render.post_soft_rect(hi, w_px, h_px, SS, outline=True, outline_px=2)
        # el suelo (z = 0) en pixeles desde arriba: donde apoyan los pies
        ground = (y1 - (0.0 * up.z + c.dot(up) - c.dot(up))) if False else None
        z0_cam = Vector((0, 0, 0)).dot(up)
        feet_px = (y1 - z0_cam) / MPP
        os.makedirs(OUT, exist_ok=True)
        from PIL import Image
        Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(
            os.path.join(OUT, f'{key}.png'), optimize=True)
        return {'w': w_px, 'h': h_px, 'feet_y': feet_px, 'origin_x': (0.0 - x0) / MPP, 'mpp': MPP}
    finally:
        parts.set_quality(prev)


def _render_prop(key, which, yaw):
    """Props de cine.py (el buey de la yunta) como figura: mismo encuadre y escala."""
    if which == 'buey':
        cine.buey(cine.MID, 'fig', at=(0.0, 0.0, 0.0), yaw=yaw, s=1.0)
    return _shoot(key)


def _shoot(key):
    cam = _setup()
    e = math.radians(ELEV)
    cam.location = Vector((0.0, -math.cos(e) * 20.0, 0.9 + math.sin(e) * 20.0))
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    x0, x1, y0, y1 = _bbox_cam(cam)
    m = 0.06
    x0, x1, y0, y1 = x0 - m, x1 + m, y0 - m, y1 + m
    w_px = int(math.ceil((x1 - x0) / MPP / 2) * 2)
    h_px = int(math.ceil((y1 - y0) / MPP / 2) * 2)
    sc = bpy.context.scene
    sc.render.resolution_x = w_px * SS
    sc.render.resolution_y = h_px * SS
    cam.data.ortho_scale = max(w_px, h_px) * MPP
    R = cam.matrix_world.to_3x3()
    right, up = R.col[0], R.col[1]
    c = cam.location
    big = max(w_px, h_px) * MPP
    cam.data.shift_x = ((x0 + x1) / 2 - c.dot(right)) / big
    cam.data.shift_y = ((y0 + y1) / 2 - c.dot(up)) / big
    hi = render._render_to_array()
    img = render.post_soft_rect(hi, w_px, h_px, SS, outline=True, outline_px=2)
    z0_cam = Vector((0, 0, 0)).dot(up)
    from PIL import Image
    os.makedirs(OUT, exist_ok=True)
    Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(
        os.path.join(OUT, f'{key}.png'), optimize=True)
    return {'w': w_px, 'h': h_px, 'feet_y': (y1 - z0_cam) / MPP, 'origin_x': (0.0 - x0) / MPP, 'mpp': MPP}


def main(keys):
    idx_path = os.path.join(OUT, 'index.json')
    index = {}
    if os.path.exists(idx_path):
        with open(idx_path) as f:
            index = json.load(f)
    todo = keys or list(FIGS.keys())
    for k in todo:
        if k not in FIGS:
            print('?? figura desconocida', k)
            continue
        index[k] = render_fig(k)
        print('fig', k, index[k], flush=True)
    with open(idx_path, 'w') as f:
        json.dump(index, f, indent=1, sort_keys=True)


if __name__ == '__main__':
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    main(args)
