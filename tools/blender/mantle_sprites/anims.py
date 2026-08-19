"""Procedural pose libraries per AnimID (mirrors src/render/sprites.h).

Conventions: character faces -Y at rest, +Z up. Rotations are given about
WORLD axes at the bone pivot (see rig.pose_apply):
  * limbs pointing down: forward swing = ('X', -deg)
  * spine/head pointing up: lean forward = ('X', +deg)
  * left arm outward = ('Y', -deg), right arm outward = ('Y', +deg)
"""
from . import rig

# (key, row, frames, loop) — must match get_anim_def() in sprites.h
ANIM_DEFS = {
    'idle':   dict(frames=4, loop=True),
    'walk':   dict(frames=6, loop=True),
    'attack': dict(frames=5, loop=False),
    'dash':   dict(frames=3, loop=False),
    'cast':   dict(frames=7, loop=False),
    'death':  dict(frames=6, loop=False),
}
ANIM_ORDER = ['idle', 'walk', 'attack', 'dash', 'cast', 'death']

# sheet rows: (row, anim key, direction)
ROWS = (
    [(i, 'idle', d) for i, d in enumerate('DULR')] +
    [(4 + i, 'walk', d) for i, d in enumerate('DULR')] +
    [(8 + i, 'attack', d) for i, d in enumerate('DULR')] +
    [(12, 'dash', 'D'), (13, 'cast', 'D'), (14, 'death', 'D')]
)

# object Z rotation (degrees) per direction; character faces -Y at 0
DIR_ROT = {'D': 0.0, 'U': 180.0, 'L': -90.0, 'R': 90.0}

FRAME_BASE = 10
FRAME_STRIDE = 100


def anim_base(key):
    return FRAME_BASE + ANIM_ORDER.index(key) * FRAME_STRIDE


def anim_span(key):
    d = ANIM_DEFS[key]
    return d['frames'] if d['loop'] else d['frames'] - 1


def render_frames(key):
    b = anim_base(key)
    return [b + k for k in range(ANIM_DEFS[key]['frames'])]


# =============================================================================
# Humanoid
# =============================================================================
def _arms(lx=0, rx=0, lf=0, rf=0, ly=0, ry=0):
    return {
        'upper_arm.L': {'rot': [('X', lx), ('Y', -ly)]},
        'upper_arm.R': {'rot': [('X', rx), ('Y', ry)]},
        'forearm.L': {'rot': [('X', lf)]},
        'forearm.R': {'rot': [('X', rf)]},
    }


def _legs(lt=0, rt=0, ls=0, rs=0):
    return {
        'thigh.L': {'rot': [('X', lt)]},
        'thigh.R': {'rot': [('X', rt)]},
        'shin.L': {'rot': [('X', ls)]},
        'shin.R': {'rot': [('X', rs)]},
    }


def humanoid_base(style):
    """Rest pose tweaks per weapon style."""
    base = rig.merge_pose(_arms(ly=6, ry=6, lf=-10, rf=-10))
    if style in ('swing', 'raise'):        # staff held upright in right hand (see characters.STAFF_TILT)
        base = rig.merge_pose(base, _arms(rf=-15, ry=4))
    elif style == 'thrust':                # spear (pre-tilted +20 in characters.py)
        base = rig.merge_pose(base, _arms(rf=-10, ry=6))
    elif style == 'bow':                   # bow in left hand (pre-tilted +40)
        base = rig.merge_pose(base, _arms(lf=-30, ly=8))
    elif style == 'claw':                  # hunched leper
        base = rig.merge_pose(base, {'spine': {'rot': [('X', 18)]},
                                     'head': {'rot': [('X', 12)]}},
                              _arms(lx=-25, rx=-25, lf=-40, rf=-40))
    return base


def humanoid_keys(key, style):
    """Return list of (phase, pose) for anim `key`."""
    if key == 'idle':
        return [
            (0.0, {}),
            (0.5, rig.merge_pose({'chest': {'rot': [('X', 3)]},
                                  'head': {'rot': [('X', 2)]},
                                  'hips': {'loc': (0, 0, -0.012)}},
                                 _arms(lx=3, rx=3))),
            (1.0, {}),
        ]
    if key == 'walk':
        contact_l = rig.merge_pose(_legs(lt=-28, rt=22, ls=6, rs=32),
                                   _arms(lx=18, rx=-18, lf=-5, rf=-15),
                                   {'spine': {'rot': [('X', 3)]},
                                    'hips': {'rot': [('Z', 5)]}})
        passing_l = rig.merge_pose(_legs(lt=-3, rt=-6, ls=2, rs=48),
                                   {'hips': {'loc': (0, 0, 0.03)}, 'spine': {'rot': [('X', 3)]}})
        contact_r = rig.merge_pose(_legs(lt=22, rt=-28, ls=32, rs=6),
                                   _arms(lx=-18, rx=18, lf=-15, rf=-5),
                                   {'spine': {'rot': [('X', 3)]},
                                    'hips': {'rot': [('Z', -5)]}})
        passing_r = rig.merge_pose(_legs(lt=-6, rt=-3, ls=48, rs=2),
                                   {'hips': {'loc': (0, 0, 0.03)}, 'spine': {'rot': [('X', 3)]}})
        return [(0.0, contact_l), (0.25, passing_l), (0.5, contact_r),
                (0.75, passing_r), (1.0, contact_l)]
    if key == 'attack':
        return _attack_keys(style)
    if key == 'dash':
        return [
            (0.0, rig.merge_pose({'spine': {'rot': [('X', 15)]}},
                                 _legs(lt=-20, rt=15, rs=20), _arms(lx=20, rx=20))),
            (1.0, rig.merge_pose({'spine': {'rot': [('X', 32)]}, 'head': {'rot': [('X', -12)]},
                                  'hips': {'loc': (0, 0, 0.04)}},
                                 _legs(lt=-48, rt=30, ls=10, rs=45),
                                 _arms(lx=38, rx=38, lf=-20, rf=-20))),
        ]
    if key == 'cast':
        # right arm forward/up, wrist counter-rotated so the staff stays upright
        up = rig.merge_pose(_arms(lx=-100, rx=-60, lf=-15, rf=0, ly=20, ry=12), _hand(60),   # staff upright
                            {'head': {'rot': [('X', -15)]}, 'spine': {'rot': [('X', -6)]},
                             'hips': {'loc': (0, 0, 0.02)}})
        return [
            (0.0, {}),
            (0.25, rig.merge_pose(_arms(lx=-45, rx=-35, lf=-30, rf=-10), _hand(45),
                                  {'head': {'rot': [('X', -5)]}})),
            (0.5, up),
            (0.8, rig.merge_pose(up, {'hips': {'loc': (0, 0, 0.035)}})),
            (1.0, rig.merge_pose(_arms(lx=-55, rx=-40, lf=-20, rf=-10), _hand(50),
                                 {'head': {'rot': [('X', -5)]}})),
        ]
    if key == 'death':
        # collapse sideways (about Y) so the body reads as lying down; root shifted to stay in frame
        return [
            (0.0, rig.merge_pose({'spine': {'rot': [('X', -10)]}, 'head': {'rot': [('X', -10)]}},
                                 _arms(lx=-30, rx=-30))),
            (0.35, rig.merge_pose({'root': {'rot': [('Y', 30)], 'loc': (-0.4, 0, 0.04)},
                                   'spine': {'rot': [('X', -15)]}, 'head': {'rot': [('X', 10)]}},
                                  _arms(lx=-60, rx=-60, lf=-30, rf=-30, ly=10, ry=20))),
            (0.7, rig.merge_pose({'root': {'rot': [('Y', 72)], 'loc': (-1.0, 0, 0.30)},
                                  'spine': {'rot': [('X', -5)]}, 'head': {'rot': [('X', 10)]}},
                                 _arms(lx=-60, rx=-60, ly=5, ry=20), _legs(lt=-15, rt=-10, ls=10))),
            # Rest on the ground (big heads: further left). Kept ~4 px clear of the cell bottom:
            # at 86 deg / z 0.26 the head touched the frame edge and got clipped on 7 sheets.
            (1.0, rig.merge_pose({'root': {'rot': [('Y', 82)], 'loc': (-1.06, 0, 0.52)},
                                  'head': {'rot': [('X', 10)]}},
                                 _arms(lx=-60, rx=-60, ly=5, ry=20), _legs(lt=-15, rt=-15, ls=15))),
        ]
    return [(0.0, {}), (1.0, {})]


def _hand(r=0, l=0):
    return {'hand.R': {'rot': [('X', r)]}, 'hand.L': {'rot': [('X', l)]}}


def _attack_keys(style):
    # NOTE: weapons are rigidly held, so a forward arm swing of A deg tilts the weapon top
    # BACK by A. hand.R/L rotations below counter that so the weapon points where intended.
    if style == 'swing':
        return [
            (0.0, {}),
            (0.35, rig.merge_pose(_arms(rx=25, rf=-60, ry=25), _hand(45),        # wind-up: weapon up/back
                                  {'chest': {'rot': [('Z', 18)]}, 'head': {'rot': [('Z', 5)]}})),
            (0.6, rig.merge_pose(_arms(rx=-50, rf=-20, ry=10), _hand(205),        # chop: weapon tip forward/DOWN
                                 {'chest': {'rot': [('Z', -22)]}, 'spine': {'rot': [('X', 10)]}},
                                 _legs(lt=-10))),
            (0.8, rig.merge_pose(_arms(rx=-40, rf=-25), _hand(180),
                                 {'chest': {'rot': [('Z', -15)]}, 'spine': {'rot': [('X', 6)]}})),
            (1.0, rig.merge_pose(_arms(rx=-30), _hand(40), {'chest': {'rot': [('Z', -5)]}})),
        ]
    if style == 'thrust':
        return [
            (0.0, {}),
            (0.3, rig.merge_pose(_arms(rx=30, rf=-40), _hand(30), {'chest': {'rot': [('Z', 20)]}})),
            (0.6, rig.merge_pose(_arms(rx=-60, rf=0), _hand(150),                 # spear horizontal forward
                                 {'chest': {'rot': [('Z', -20)]}, 'spine': {'rot': [('X', 12)]}},
                                 _legs(lt=-20, rt=15))),
            (1.0, rig.merge_pose(_arms(rx=-35), _hand(60), {'chest': {'rot': [('Z', -8)]}})),
        ]
    if style == 'bow':
        return [
            (0.0, {}),
            (0.3, rig.merge_pose(_arms(lx=-85, lf=0, rx=-60, rf=-120, ry=40), _hand(l=85),   # bow upright, held forward
                                 {'chest': {'rot': [('Z', -15)]}})),
            (0.6, rig.merge_pose(_arms(lx=-85, lf=0, rx=-30, rf=-30, ry=60), _hand(l=85),
                                 {'chest': {'rot': [('Z', -15)]}})),
            (1.0, rig.merge_pose(_arms(lx=-40), _hand(l=40))),
        ]
    if style == 'claw':
        return [
            (0.0, {}),
            (0.35, rig.merge_pose(_arms(lx=40, rx=40, lf=-60, rf=-60),
                                  {'spine': {'rot': [('X', -12)]}})),
            (0.6, rig.merge_pose(_arms(lx=-100, rx=-100, lf=-20, rf=-20, ly=20, ry=20),
                                 {'spine': {'rot': [('X', 25)]}, 'head': {'rot': [('X', 10)]}})),
            (1.0, rig.merge_pose(_arms(lx=-50, rx=-50), {'spine': {'rot': [('X', 10)]}})),
        ]
    if style == 'raise':
        up = rig.merge_pose(_arms(rx=-45, rf=0, lx=-40, ry=15), _hand(45),      # staff upright, held forward
                            {'spine': {'rot': [('X', -8)]}})
        return [
            (0.0, {}),
            (0.4, up),
            (0.7, rig.merge_pose(up, {'hips': {'loc': (0, 0, 0.02)}})),
            (1.0, rig.merge_pose(_arms(rx=-30), _hand(30))),
        ]
    return [(0.0, {}), (1.0, {})]


# =============================================================================
# Quadruped (lion)
# =============================================================================
def _qlegs(fl=0, fr=0, rl=0, rr=0, lfl=0, lfr=0, lrl=0, lrr=0):
    return {
        'upper_FL': {'rot': [('X', fl)]}, 'upper_FR': {'rot': [('X', fr)]},
        'upper_RL': {'rot': [('X', rl)]}, 'upper_RR': {'rot': [('X', rr)]},
        'lower_FL': {'rot': [('X', lfl)]}, 'lower_FR': {'rot': [('X', lfr)]},
        'lower_RL': {'rot': [('X', lrl)]}, 'lower_RR': {'rot': [('X', lrr)]},
    }


def quadruped_keys(key):
    if key == 'idle':
        return [
            (0.0, {'tail': {'rot': [('Z', 15)]}}),
            (0.5, {'tail': {'rot': [('Z', -15)]}, 'head': {'rot': [('X', 4)]},
                   'spine_front': {'loc': (0, 0, -0.015)}}),
            (1.0, {'tail': {'rot': [('Z', 15)]}}),
        ]
    if key == 'walk':
        a = rig.merge_pose(_qlegs(fl=-28, rr=-28, fr=25, rl=25, lfr=25, lrl=25),
                           {'head': {'rot': [('X', 4)]}, 'tail': {'rot': [('Z', 10)]}})
        pa = rig.merge_pose(_qlegs(fl=-5, rr=-5, fr=-8, rl=-8, lfr=45, lrl=45),
                            {'root': {'loc': (0, 0, 0.03)}})
        b = rig.merge_pose(_qlegs(fr=-28, rl=-28, fl=25, rr=25, lfl=25, lrr=25),
                           {'head': {'rot': [('X', 4)]}, 'tail': {'rot': [('Z', -10)]}})
        pb = rig.merge_pose(_qlegs(fr=-5, rl=-5, fl=-8, rr=-8, lfl=45, lrr=45),
                            {'root': {'loc': (0, 0, 0.03)}})
        return [(0.0, a), (0.25, pa), (0.5, b), (0.75, pb), (1.0, a)]
    if key == 'attack':
        return [
            (0.0, {}),
            (0.3, rig.merge_pose({'root': {'loc': (0, 0.05, -0.12)}, 'head': {'rot': [('X', -10)]}},
                                 _qlegs(rl=-15, rr=-15, lrl=40, lrr=40, fl=10, fr=10))),
            (0.6, rig.merge_pose({'root': {'loc': (0, -0.3, 0.18)}, 'head': {'rot': [('X', 12)]},
                                  'spine_front': {'rot': [('X', 8)]}},
                                 _qlegs(fl=-75, fr=-75, lfl=-15, lfr=-15, rl=30, rr=30))),
            (1.0, rig.merge_pose({'root': {'loc': (0, -0.15, 0.0)}}, _qlegs(fl=-30, fr=-30))),
        ]
    if key == 'dash':
        return [
            (0.0, rig.merge_pose({'root': {'loc': (0, -0.05, 0.02)}}, _qlegs(fl=-30, fr=-30, rl=25, rr=25))),
            (1.0, rig.merge_pose({'root': {'loc': (0, -0.15, 0.06)}, 'head': {'rot': [('X', -6)]}},
                                 _qlegs(fl=-55, fr=-55, rl=48, rr=48, lrl=30, lrr=30))),
        ]
    if key == 'cast':   # roar
        return [
            (0.0, {}),
            (0.4, rig.merge_pose({'head': {'rot': [('X', -25)]}, 'neck': {'rot': [('X', -10)]},
                                  'root': {'loc': (0, 0, 0.03)}}, _qlegs(fl=-20, fr=-20))),
            (0.75, rig.merge_pose({'head': {'rot': [('X', -25)]}, 'neck': {'rot': [('X', -10)]},
                                   'root': {'loc': (0, 0, 0.03)}}, _qlegs(fl=-20, fr=-20))),
            (1.0, {}),
        ]
    if key == 'death':
        return [
            (0.0, {'head': {'rot': [('X', 10)]}}),
            # roll about Y around the origin (ground): the body centre (z~0.65) swings out to +X,
            # so shift back in X and lift so the flank rests on the ground instead of sinking
            # ('Z', 90) is applied last so the rolled body ends up lying along X (side view for the camera)
            (0.4, rig.merge_pose({'root': {'rot': [('Z', 40), ('Y', 40)], 'loc': (0, -0.2, 0.0)}},
                                 _qlegs(fl=-15, fr=-15, rl=-10, rr=-10))),
            (0.7, rig.merge_pose({'root': {'rot': [('Z', 80), ('Y', 80)], 'loc': (0, -0.45, 0.12)},
                                  'head': {'rot': [('X', 15)]}},
                                 _qlegs(fl=-35, fr=-35, rl=-30, rr=-30))),
            (1.0, rig.merge_pose({'root': {'rot': [('Z', 90), ('Y', 88)], 'loc': (0, -0.55, 0.16)},
                                  'head': {'rot': [('X', 20)]}},
                                 _qlegs(fl=-40, fr=-40, rl=-35, rr=-35))),
        ]
    return [(0.0, {}), (1.0, {})]


# =============================================================================
# Blob (Agua Corrupta)
# =============================================================================
def blob_keys(key):
    R = 'root'
    if key == 'idle':
        return [(0.0, {}), (0.5, {R: {'scale': (1.06, 1.06, 0.92)}}), (1.0, {})]
    if key == 'walk':
        return [
            (0.0, {R: {'scale': (0.95, 0.95, 1.12), 'loc': (0, 0, 0.05)}}),
            (0.5, {R: {'scale': (1.12, 1.12, 0.88), 'loc': (0, 0, 0.0)}}),
            (1.0, {R: {'scale': (0.95, 0.95, 1.12), 'loc': (0, 0, 0.05)}}),
        ]
    if key == 'attack':
        return [
            (0.0, {}),
            (0.3, {R: {'scale': (1.15, 0.8, 1.05), 'loc': (0, 0.08, 0)}}),
            (0.6, {R: {'scale': (0.85, 1.2, 0.95), 'loc': (0, -0.05, 0.05)}}),
            (1.0, {}),
        ]
    if key == 'dash':
        return [(0.0, {R: {'scale': (0.9, 1.1, 0.95), 'loc': (0, 0.0, 0)}}),
                (1.0, {R: {'scale': (0.85, 1.15, 0.9), 'loc': (0, 0.0, 0)}})]
    if key == 'cast':
        return [
            (0.0, {}),
            (0.4, {R: {'scale': (1.15, 1.15, 1.3), 'loc': (0, 0, 0.1)}}),
            (0.75, {R: {'scale': (1.15, 1.15, 1.3), 'loc': (0, 0, 0.1)}}),
            (1.0, {}),
        ]
    if key == 'death':
        return [(0.0, {}), (0.5, {R: {'scale': (1.2, 1.1, 0.5)}}),
                (1.0, {R: {'scale': (1.4, 1.15, 0.15)}})]
    return [(0.0, {}), (1.0, {})]


# =============================================================================
# Driver
# =============================================================================
def animate(char):
    """Keyframe every animation of `char` on the timeline. char: dict from characters.build()."""
    arm = char['arm']
    kind = char['kind']
    style = char.get('attack', 'swing')
    base_pose = humanoid_base(style) if kind == 'humanoid' else {}
    for key in ANIM_ORDER:
        b = anim_base(key)
        span = anim_span(key)
        if kind == 'humanoid':
            keys = humanoid_keys(key, style)
        elif kind == 'quadruped':
            keys = quadruped_keys(key)
        else:
            keys = blob_keys(key)
        for phase, pose in keys:
            rig.pose_apply(arm, rig.merge_pose(base_pose, pose), frame=b + phase * span)
    rig.linearize(arm)

    # weapon tip glow during cast
    tip = char.get('tip')
    if tip is not None:
        b = anim_base('cast')
        span = anim_span('cast')
        for f, s in ((0, 1.0), (b, 1.0), (b + 0.5 * span, 1.5), (b + 0.8 * span, 1.5), (b + span, 1.0)):
            tip.scale = (s, s, s)
            tip.keyframe_insert('scale', frame=f)
        if tip.animation_data and tip.animation_data.action:
            for fc in tip.animation_data.action.fcurves:
                for kp in fc.keyframe_points:
                    kp.interpolation = 'LINEAR'
