"""Orthographic Workbench render -> numpy post -> 896x1920 sheet (128 px cells).

Post-processing: `post_soft` -- anti-aliased edges, no palette snap, thin anti-aliased
outline. That is THE style of the game: a smooth "3D figurine" drawn with linear
filtering at 4 texels per game unit.

`quantize=True` is the LEGACY 32 px path (palette snap + hard alpha + 1 px outline) from
the abandoned pixel-art era. It is kept only so an old sheet can be reproduced; nothing
that ships calls it (run.main defaults to quantize=False).
"""
import math
import os
import bpy
import numpy as np
from mathutils import Vector

from . import anims
from .parts import _MATS

FRAME = 128         # sprite cell size (px) = 32 game units x ASSET_SCALE 4
COLS, ROWS = 7, 15  # sheet grid (matches sprites.h)
SS = 2              # supersampling factor (render at FRAME*SS)

CAM_ELEV_DEG = 30.0     # camera elevation above horizontal
ORTHO_SCALE = 2.55      # world units across the (square) frame
FEET_ROW = 112          # sprite row where the ground contact (origin) must land (28/32 of the cell; collider +12 units)
OUTLINE = (0.07, 0.05, 0.09)
OUTLINE_PX = 2          # soft style: outline thickness in output pixels
SHADES = (0.55, 0.78, 1.0, 1.18)   # tones per material for palette quantisation (pixel style)
GAIN = 1.7                          # post brightness gain (Workbench studio light caps ~0.6)
LIGHT_MODE = 'STUDIO'               # 'STUDIO' or 'FLAT'


# =============================================================================
# Scene / camera
# =============================================================================
def setup_scene(tmp_dir):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.render.resolution_x = FRAME * SS
    sc.render.resolution_y = FRAME * SS
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.filter_size = 1.0
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    sc.render.image_settings.color_depth = '8'
    sc.render.image_settings.compression = 15
    sc.display_settings.display_device = 'sRGB'
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    sc.view_settings.exposure = 0.0
    sc.view_settings.gamma = 1.0
    sh = sc.display.shading
    sh.light = LIGHT_MODE
    sh.studio_light = 'Default'
    sh.color_type = 'MATERIAL'
    sh.show_shadows = True
    sh.shadow_intensity = 0.4
    # Clay/toon look: cavity darkens creases and brightens ridges, specular adds a soft sheen
    sh.show_cavity = True
    sh.cavity_type = 'BOTH'
    sh.cavity_ridge_factor = 1.0
    sh.cavity_valley_factor = 1.2
    sh.curvature_ridge_factor = 0.8
    sh.curvature_valley_factor = 0.8
    sh.show_object_outline = False
    sh.use_world_space_lighting = False   # light follows the camera: consistent for all 4 directions
    sh.show_specular_highlight = True
    sc.display.render_aa = '8'
    sc.display.viewport_aa = '8'
    sc.world = sc.world or bpy.data.worlds.new('World')
    os.makedirs(tmp_dir, exist_ok=True)
    sc.render.filepath = os.path.join(tmp_dir, 'frame.png')

    cam = bpy.data.objects.get('mantle_cam')
    if cam is None:
        cd = bpy.data.cameras.new('mantle_cam')
        cam = bpy.data.objects.new('mantle_cam', cd)
        sc.collection.objects.link(cam)
    cd = cam.data
    cd.type = 'ORTHO'
    cd.ortho_scale = ORTHO_SCALE
    cd.clip_start = 0.01
    cd.clip_end = 100.0
    # camera in front (-Y), elevated, looking at origin; shift places origin at FEET_ROW
    e = math.radians(CAM_ELEV_DEG)
    dist = 12.0
    cam.location = Vector((0.0, -math.cos(e) * dist, math.sin(e) * dist))
    cam.rotation_euler = (math.radians(90.0) - e, 0.0, 0.0)
    # positive shift_y moves the frame up => the origin appears lower in the image
    cd.shift_y = (FEET_ROW - FRAME * 0.5) / FRAME
    cd.shift_x = 0.0
    sc.camera = cam
    return cam


# =============================================================================
# Frame capture + pixel-art post
# =============================================================================
def _render_to_array():
    """Render current frame, return float RGBA array (H, W, 4), top-down."""
    sc = bpy.context.scene
    path = sc.render.filepath
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path, check_existing=False)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)[::-1].copy()


def _palette():
    """Palette from all materials in the scene x shades (+ outline)."""
    cols = []
    for rgb in _MATS.values():
        for s in SHADES:
            c = np.clip(np.array(rgb, dtype=np.float32) * s, 0.0, 1.0)
            if s > 1.0:  # lighten toward white a bit instead of pure multiply
                c = np.clip(np.array(rgb) + (1.0 - np.array(rgb)) * (s - 1.0) * 0.8, 0, 1)
            cols.append(c)
    cols.append(np.array(OUTLINE, dtype=np.float32))
    return np.array(cols, dtype=np.float32)


def _dilate(mask, n):
    """Binary 4-neighbour dilation, n iterations."""
    m = mask.copy()
    for _ in range(n):
        nb = m.copy()
        nb[1:, :] |= m[:-1, :]
        nb[:-1, :] |= m[1:, :]
        nb[:, 1:] |= m[:, :-1]
        nb[:, :-1] |= m[:, 1:]
        m = nb
    return m


def _bleed(rgb, a, iters=3):
    """Copy RGB of covered pixels into neighbouring transparent pixels (straight alpha +
    linear filtering otherwise blends towards black at the edges = dark halo)."""
    rgb = rgb.copy()
    cov = a > 1e-3
    for _ in range(iters):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(rgb.shape[:2], dtype=np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            src = np.roll(cov, (dy, dx), axis=(0, 1))
            val = np.roll(rgb, (dy, dx), axis=(0, 1))
            acc += val * src[..., None]
            cnt += src
        fill = (~cov) & (cnt > 0)
        rgb[fill] = acc[fill] / cnt[fill][:, None]
        cov = cov | fill
    return rgb


def post_soft_rect(hi, w, h, ss, outline=True, gain=None, outline_px=None, bleed=True):
    """(h*ss, w*ss, 4) float -> (h, w, 4) float smooth image: soft alpha, no palette,
    anti-aliased dark outline (computed at render res, then box-filtered).

    Same look as post_soft, but for non-square frames (cinematic panels are 16:9).
    `bleed=False` skips the colour bleed: on an opaque layer there are no alpha edges to
    sangrar and it is the expensive step at panel resolution.
    """
    gain = GAIN if gain is None else gain
    outline_px = OUTLINE_PX if outline_px is None else outline_px
    a_hi = hi[..., 3]
    rgb_hi = hi[..., :3].copy()
    if outline and outline_px > 0:
        solid_hi = a_hi > 0.5
        ring = _dilate(solid_hi, outline_px * ss) & ~solid_hi
        rgb_hi[ring] = np.array(OUTLINE, dtype=np.float32) / max(gain, 1e-3)   # keep outline dark after gain
        a_hi = a_hi.copy()
        a_hi[ring] = 1.0
    prem = rgb_hi * a_hi[..., None]
    prem = prem.reshape(h, ss, w, ss, 3).mean(axis=(1, 3))
    a = a_hi.reshape(h, ss, w, ss).mean(axis=(1, 3))
    rgb = np.where(a[..., None] > 1e-4, prem / np.maximum(a[..., None], 1e-4), 0.0)
    rgb = np.clip(rgb * gain, 0.0, 1.0)
    if bleed:
        rgb = _bleed(rgb, a)
    out = np.zeros((h, w, 4), dtype=np.float32)
    out[..., :3] = rgb
    out[..., 3] = np.clip(a, 0.0, 1.0)
    return out


def post_soft(hi, px, ss, outline=True, gain=None, outline_px=None):
    """Square frame (sprite cells, tileset cells). See post_soft_rect."""
    return post_soft_rect(hi, px, px, ss, outline=outline, gain=gain, outline_px=outline_px)


def post_process(hi, palette=None, outline=True):
    """(FRAME*SS, FRAME*SS, 4) float -> (FRAME, FRAME, 4) float frame.
    palette=None -> soft style; palette given -> legacy pixel-art quantisation."""
    if palette is None:
        return post_soft(hi, FRAME, SS, outline=outline)
    h, w, _ = hi.shape
    a = hi[..., 3:4]
    prem = hi[..., :3] * a
    # box downscale
    prem = prem.reshape(FRAME, SS, FRAME, SS, 3).mean(axis=(1, 3))
    a = a.reshape(FRAME, SS, FRAME, SS, 1).mean(axis=(1, 3))
    rgb = np.where(a > 1e-4, prem / np.maximum(a, 1e-4), 0.0)
    rgb = np.clip(rgb * GAIN, 0.0, 1.0)
    solid = (a[..., 0] >= 0.5)
    out = np.zeros((FRAME, FRAME, 4), dtype=np.float32)
    if solid.any():
        px = rgb[solid]                                # (N,3)
        d = ((px[:, None, :] - palette[None, :, :]) ** 2).sum(axis=2)
        idx = d.argmin(axis=1)
        rgb_q = rgb.copy()
        rgb_q[solid] = palette[idx]
        rgb = rgb_q
    out[..., :3] = rgb
    out[..., 3] = solid.astype(np.float32)
    if outline:
        s = solid
        nb = np.zeros_like(s)
        nb[1:, :] |= s[:-1, :]
        nb[:-1, :] |= s[1:, :]
        nb[:, 1:] |= s[:, :-1]
        nb[:, :-1] |= s[:, 1:]
        edge = nb & ~s
        out[edge, :3] = OUTLINE
        out[edge, 3] = 1.0
    return out


# =============================================================================
# Sheet
# =============================================================================
def render_sheet(char, out_path, quantize=False, outline=True, log=print):
    sc = bpy.context.scene
    arm = char['arm']
    sheet = np.zeros((ROWS * FRAME, COLS * FRAME, 4), dtype=np.float32)
    pal = _palette() if quantize else None
    n_frames = 0
    clipped = []
    for row, key, d in anims.ROWS:
        arm.rotation_euler = (0.0, 0.0, math.radians(anims.DIR_ROT[d]))
        for col, f in enumerate(anims.render_frames(key)):
            sc.frame_set(f)
            hi = _render_to_array()
            fr = post_process(hi, pal, outline)
            y0, x0 = row * FRAME, col * FRAME
            sheet[y0:y0 + FRAME, x0:x0 + FRAME] = fr
            n_frames += 1
            solid = fr[..., 3] > 0.5
            edges = ''.join(t for t, hit in (('T', solid[0, :].any()), ('B', solid[-1, :].any()),
                                             ('L', solid[:, 0].any()), ('R', solid[:, -1].any())) if hit)
            if edges:
                clipped.append(f'{key}/{d}#{col}:{edges}')
        log(f'  row {row:2d} {key}/{d} done')
    arm.rotation_euler = (0.0, 0.0, 0.0)
    save_png(sheet, out_path, 'mantle_sheet_' + char['name'])
    log(f'{char["name"]}: {n_frames} frames -> {out_path}')
    if clipped:
        log(f'  WARNING clipped frames: {clipped}')
    return sheet


def save_png(arr_topdown, path, img_name):
    h, w, _ = arr_topdown.shape
    img = bpy.data.images.get(img_name)
    if img is not None:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(img_name, w, h, alpha=True, float_buffer=False)
    img.colorspace_settings.name = 'sRGB'
    img.alpha_mode = 'STRAIGHT'
    flat = np.ascontiguousarray(arr_topdown[::-1]).astype(np.float32).ravel()
    img.pixels.foreach_set(flat)
    img.filepath_raw = path
    img.file_format = 'PNG'
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save()
    bpy.data.images.remove(img)


def upscale_preview(sheet, path, scale=6, bg=(0.16, 0.16, 0.18), cell=None):
    """Nearest-neighbour upscaled preview with grid lines and opaque background."""
    cell = cell or FRAME
    h, w, _ = sheet.shape
    big = np.repeat(np.repeat(sheet, scale, axis=0), scale, axis=1)
    a = big[..., 3:4]
    rgb = big[..., :3] * a + np.array(bg, dtype=np.float32) * (1 - a)
    out = np.concatenate([rgb, np.ones_like(a)], axis=2)
    grid = (0.30, 0.30, 0.34, 1.0)
    for y in range(0, h * scale, cell * scale):
        out[y, :, :] = grid
    for x in range(0, w * scale, cell * scale):
        out[:, x, :] = grid
    save_png(out, path, 'mantle_preview')
