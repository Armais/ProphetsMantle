"""Armature creation and world-axis pose keyframing helpers."""
import math
import bpy
from mathutils import Vector, Matrix, Quaternion

WORLD_AXES = {
    'X': Vector((1, 0, 0)),
    'Y': Vector((0, 1, 0)),
    'Z': Vector((0, 0, 1)),
}


def _view3d_override():
    win = bpy.context.window
    if win is None and bpy.context.window_manager.windows:
        win = bpy.context.window_manager.windows[0]
    if win is None:
        return None
    for area in win.screen.areas:
        if area.type == 'VIEW_3D':
            for region in area.regions:
                if region.type == 'WINDOW':
                    return dict(window=win, screen=win.screen, area=area, region=region)
    return dict(window=win, screen=win.screen)


def build_armature(name, bones):
    """bones: list of (name, head, tail, parent_or_None). Returns armature object."""
    arm_data = bpy.data.armatures.new(name + '_rig')
    arm = bpy.data.objects.new(name + '_rig', arm_data)
    bpy.context.scene.collection.objects.link(arm)
    arm_data.display_type = 'STICK'
    arm.show_in_front = True

    ov = _view3d_override() or {}
    with bpy.context.temp_override(**ov, active_object=arm, selected_objects=[arm],
                                   object=arm):
        bpy.context.view_layer.objects.active = arm
        arm.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')
        eb = arm_data.edit_bones
        for bname, head, tail, parent in bones:
            b = eb.new(bname)
            b.head = Vector(head)
            b.tail = Vector(tail)
            b.roll = 0.0
            if parent:
                b.parent = eb[parent]
                b.use_connect = False
        bpy.ops.object.mode_set(mode='OBJECT')

    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    return arm


# =============================================================================
# Poses in WORLD axes -> bone local
# =============================================================================

def _rest_rot_inv(arm, bone_name):
    return arm.data.bones[bone_name].matrix_local.to_3x3().inverted()


def pose_apply(arm, pose, frame=None):
    """Apply a pose dict and optionally keyframe ALL bones at `frame`.

    pose = { bone: {'rot': [('X', deg), ...], 'loc': (x,y,z), 'scale': (sx,sy,sz)} }
    Axes/offsets/scales are expressed in world (rest) axes; unspecified bones reset.
    """
    for pb in arm.pose.bones:
        spec = pose.get(pb.name, {})
        inv = _rest_rot_inv(arm, pb.name)
        q = Quaternion((1, 0, 0, 0))
        for axis, deg in spec.get('rot', []):
            local_axis = inv @ WORLD_AXES[axis]
            q = q @ Quaternion(local_axis, math.radians(deg))
        pb.rotation_quaternion = q
        loc = spec.get('loc')
        pb.location = (inv @ Vector(loc)) if loc else Vector((0, 0, 0))
        sc = spec.get('scale')
        if sc:
            # map world scale onto local axes by dominant alignment
            R = arm.data.bones[pb.name].matrix_local.to_3x3()
            local = [1.0, 1.0, 1.0]
            for i in range(3):
                col = Vector((R[0][i], R[1][i], R[2][i]))
                j = max(range(3), key=lambda k: abs(col[k]))
                local[i] = sc[j]
            pb.scale = local
        else:
            pb.scale = (1, 1, 1)
        if frame is not None:
            pb.keyframe_insert('rotation_quaternion', frame=frame)
            pb.keyframe_insert('location', frame=frame)
            pb.keyframe_insert('scale', frame=frame)


def merge_pose(*poses):
    """Merge pose dicts (later wins per bone; rot lists concatenate)."""
    out = {}
    for p in poses:
        for b, spec in p.items():
            cur = out.setdefault(b, {})
            if 'rot' in spec:
                cur['rot'] = cur.get('rot', []) + list(spec['rot'])
            for k in ('loc', 'scale'):
                if k in spec:
                    cur[k] = spec[k]
    return out


def linearize(arm):
    ad = arm.animation_data
    if ad and ad.action:
        for fc in ad.action.fcurves:
            for kp in fc.keyframe_points:
                kp.interpolation = 'LINEAR'
