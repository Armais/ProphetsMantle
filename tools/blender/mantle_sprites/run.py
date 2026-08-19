"""Entry point. From Blender (or via blender-mcp execute_blender_code):

    import sys; sys.path.insert(0, r"<repo>/tools/blender")
    import importlib, mantle_sprites.run as r; importlib.reload(r)
    r.main(only=["elias"])          # or r.main() for all characters

Reload helper: r.reload_all() re-imports every submodule after edits.
"""
import importlib
import os
import tempfile
import time

import bpy

from . import parts, rig, anims, characters, render

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
SPRITES_DIR = os.path.join(ROOT, 'assets', 'sprites')
BLEND_DIR = os.path.join(ROOT, 'tools', 'blender', 'blend')
TMP_DIR = os.path.join(tempfile.gettempdir(), 'mantle_sprites')


def reload_all():
    for m in (parts, rig, anims, characters, render):
        importlib.reload(m)


def build_character(name, animate=True):
    """Clear the scene, build + animate one character. Returns char dict."""
    parts.clear_scene()
    char = characters.build(name)
    if animate:
        anims.animate(char)
    bpy.context.scene.frame_start = 0
    bpy.context.scene.frame_end = anims.anim_base('death') + anims.ANIM_DEFS['death']['frames']
    bpy.context.scene.frame_set(anims.anim_base('idle'))
    return char


def save_blend(name):
    os.makedirs(BLEND_DIR, exist_ok=True)
    path = os.path.join(BLEND_DIR, f'{name}.blend')
    bpy.ops.wm.save_as_mainfile(filepath=path, copy=True)
    bak = path + '1'
    if os.path.exists(bak):
        os.remove(bak)
    return path


def load_sheet(name):
    """Read assets/sprites/<name>.png back as a top-down float RGBA array."""
    import numpy as np
    img = bpy.data.images.load(os.path.join(SPRITES_DIR, f'{name}.png'), check_existing=False)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)[::-1].copy()


# (row, col) cells shown per character in the contact sheet
CONTACT_CELLS = [(0, 0), (2, 0), (1, 0), (4, 2), (6, 2), (8, 3), (10, 3), (12, 2), (13, 4), (14, 5)]


def contact_sheet(names=None, path=None, scale=1):
    """One row per character with a handful of representative frames."""
    import numpy as np
    names = names or characters.NAMES
    F = render.FRAME
    out = np.zeros((len(names) * F, len(CONTACT_CELLS) * F, 4), dtype=np.float32)
    for i, n in enumerate(names):
        sh = load_sheet(n)
        for j, (row, col) in enumerate(CONTACT_CELLS):
            out[i * F:(i + 1) * F, j * F:(j + 1) * F] = sh[row * F:(row + 1) * F, col * F:(col + 1) * F]
    path = path or os.path.join(TMP_DIR, 'contact_sheet.png')
    render.upscale_preview(out, path, scale=scale)
    return path


def main(only=None, do_render=True, preview=True, blend=True, quantize=False, outline=True):
    """quantize=False (default) -> smooth 128 px frames; True -> legacy pixel-art palette snap."""
    names = only or characters.NAMES
    results = {}
    for name in names:
        t0 = time.time()
        char = build_character(name)
        render.setup_scene(TMP_DIR)  # (re)creates the camera after clear_scene
        out = {}
        if do_render:
            sheet_path = os.path.join(SPRITES_DIR, f'{name}.png')
            sheet = render.render_sheet(char, sheet_path, quantize=quantize, outline=outline)
            out['sheet'] = sheet_path
            if preview:
                pv = os.path.join(TMP_DIR, f'{name}_preview.png')
                render.upscale_preview(sheet, pv, scale=1)
                out['preview'] = pv
        if blend:
            out['blend'] = save_blend(name)
        out['seconds'] = round(time.time() - t0, 1)
        results[name] = out
        print(name, out)
    return results
