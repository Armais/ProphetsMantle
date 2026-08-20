"""Primitive builders (bmesh, no bpy.ops), flat materials and bone attachment."""
import bpy
import bmesh
from mathutils import Vector, Matrix

# =============================================================================
# Materials (Workbench uses diffuse_color directly, no node trees needed)
# =============================================================================
_MATS = {}


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def get_mat(name, rgb):
    """Return (creating if needed) a flat material. rgb = sRGB display values 0..1.

    diffuse_color is stored linear so that a flat render reproduces `rgb` exactly.
    """
    key = name
    m = bpy.data.materials.get(key)
    if m is None:
        m = bpy.data.materials.new(key)
    m.use_nodes = False
    m.diffuse_color = (srgb_to_linear(rgb[0]), srgb_to_linear(rgb[1]), srgb_to_linear(rgb[2]), 1.0)
    m.roughness = 0.9
    m.metallic = 0.0
    m.specular_intensity = 0.1
    _MATS[key] = tuple(rgb)
    return m


def hexc(h):
    """'#rrggbb' -> (r,g,b) floats (sRGB display values)."""
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def vivid(rgb, sat=1.35, val=1.04):
    """Boost saturation (and a touch of value) in HSV space: toon/figurine palette."""
    import colorsys
    h, s, v = colorsys.rgb_to_hsv(*rgb)
    s = min(1.0, s * sat)
    v = min(1.0, v * val)
    return colorsys.hsv_to_rgb(h, s, v)


# =============================================================================
# Mesh helpers
# =============================================================================

def _finish(name, bm, mat, smooth=False):
    """smooth=True forces every face smooth; otherwise the per-face `smooth` flags set
    by the builders (bevel strips, tube sides) are carried over untouched."""
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if smooth:
        for f in bm.faces:
            f.smooth = True
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    if mat is not None:
        me.materials.append(mat)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def _basis_from_dir(d):
    """Rotation matrix whose local Z maps onto unit vector d."""
    d = Vector(d)
    if d.length < 1e-8:
        return Matrix.Identity(3)
    z = d.normalized()
    up = Vector((0, 0, 1)) if abs(z.z) < 0.99 else Vector((0, -1, 0))
    x = up.cross(z).normalized()
    y = z.cross(x).normalized()
    m = Matrix((x, y, z)).transposed()  # columns = x,y,z
    return m


def _bevel(bm, geom, width, segments=3, profile=0.5):
    """Bevel `geom` (edges) by `width` world units and smooth-shade the strips it creates.

    Flat faces stay flat: a rounded *edge* reads as a soft figurine, a fully smoothed box
    reads as a pillow. Returns the new faces."""
    if width <= 0.0 or not geom:
        return []
    res = bmesh.ops.bevel(bm, geom=geom, offset=width, offset_type='OFFSET',
                          segments=segments, profile=profile, affect='EDGES',
                          clamp_overlap=True)
    for f in res.get('faces', ()):
        f.smooth = True
    return res.get('faces', ())


def box(name, center, size, mat, rot=None, round=0.0, taper=None, segments=3):
    """Axis-aligned box centred at `center` with full extents `size`.

    round: 0 = hard 90 deg edges (default, so tiles.py keeps its crisp look); >0 bevels every
           edge by `round` x half the smallest extent.
    taper: scale of the +Z face, float or (x, y) - open skirts, capes, mantles.
    """
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    if taper is not None:
        tx, ty = (taper, taper) if isinstance(taper, (int, float)) else taper
        for v in bm.verts:
            if v.co.z > 0.0:
                v.co.x *= tx
                v.co.y *= ty
    m = Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    if rot is not None:
        m = rot.to_4x4() @ m
    m = Matrix.Translation(center) @ m
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    # bevel *after* the transform: the width is world-space, so a thin slab keeps an even edge
    _bevel(bm, list(bm.edges), min(size) * 0.5 * round, segments)
    return _finish(name, bm, mat)


def sphere(name, center, radius, mat, scale=(1, 1, 1), segs=10, rings=7):
    # Rendered at 128 px now: denser spheres so silhouettes stay round (callers' low
    # counts were tuned for 32 px)
    segs, rings = max(segs, 28), max(rings, 18)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=radius)
    m = Matrix.Translation(center) @ Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return _finish(name, bm, mat, smooth=True)


def tube(name, p0, p1, r0, r1, mat, segs=8, smooth=True, caps=True, rim_round=0.0):
    """Cylinder/frustum from p0 to p1 with radii r0 (at p0) and r1 (at p1).

    caps=False leaves both ends open: a ring (helmet band, hood cowl).
    rim_round bevels the hard edge where the cap meets the side.
    """
    p0 = Vector(p0)
    p1 = Vector(p1)
    d = p1 - p0
    length = d.length
    segs = max(segs, 24)   # smoother limbs/staffs/skirts at 128 px
    bm = bmesh.new()
    # create_cone builds along Z centred at origin: radius1 at -Z/2, radius2 at +Z/2
    bmesh.ops.create_cone(bm, cap_ends=caps, cap_tris=False, segments=segs,
                          radius1=r0, radius2=r1, depth=length)
    # sides smooth, caps flat: a smoothed cap bends the silhouette back on itself
    bm.normal_update()
    for f in bm.faces:
        f.smooth = smooth and abs(f.normal.z) < 0.9
    if caps:
        rim = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > 0.9]
        _bevel(bm, rim, rim_round, 2)
    rot = _basis_from_dir(d).to_4x4()
    m = Matrix.Translation((p0 + p1) * 0.5) @ rot
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return _finish(name, bm, mat)


def plane_quad(name, verts, mat):
    """Single quad (list of 4 world points) - used for capes/cloth flaps."""
    bm = bmesh.new()
    vs = [bm.verts.new(v) for v in verts]
    bm.faces.new(vs)
    return _finish(name, bm, mat)


# =============================================================================
# Bone attachment (rigid parts, no skinning)
# =============================================================================

def attach(ob, arm, bone_name):
    """Parent `ob` to bone keeping its current world transform."""
    bone = arm.data.bones[bone_name]
    ob.parent = arm
    ob.parent_type = 'BONE'
    ob.parent_bone = bone_name
    tail = arm.matrix_world @ bone.matrix_local @ Matrix.Translation((0.0, bone.length, 0.0))
    ob.matrix_parent_inverse = tail.inverted()
    return ob


def attach_obj(ob, parent):
    """Regular object parenting keeping world transform (parent must be unparented itself)."""
    ob.parent = parent
    ob.matrix_parent_inverse = parent.matrix_basis.inverted()
    return ob


def rotate_about(ob, pivot, axis, deg):
    """Rotate an (unparented, origin-at-0) object's geometry about a world pivot."""
    import math
    M = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-Vector(pivot))
    for v in ob.data.vertices:
        v.co = M @ v.co
    return ob


def scale_about(ob, pivot, s):
    """Uniformly scale an object's geometry about a world pivot (works before or after
    attach(): vertices are authored in world space with the object at the origin)."""
    p = Vector(pivot)
    for v in ob.data.vertices:
        v.co = p + (v.co - p) * s
    return ob


def set_origin(ob, point):
    """Move the object's origin to `point` (world) without moving the geometry."""
    p = Vector(point)
    me = ob.data
    for v in me.vertices:
        v.co -= p
    ob.location = ob.location + p
    return ob


def clear_scene():
    """Remove every object/mesh/material/armature/action/image render leftovers."""
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials,
                 bpy.data.actions, bpy.data.cameras, bpy.data.lights):
        for d in list(coll):
            if d.users == 0:
                coll.remove(d)
    for img in list(bpy.data.images):
        if img.name.startswith('mantle_'):
            bpy.data.images.remove(img)
    _MATS.clear()
