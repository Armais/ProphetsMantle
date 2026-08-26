"""Los 7 PNJ de los encuentros -> assets/sprites/npcs.png (7 col x 1 fil, celda 128 px).

    import mantle_sprites.npcs as np_; importlib.reload(np_)
    np_.main(r.SPRITES_DIR, r.TMP_DIR)
    np_.main(r.SPRITES_DIR, r.TMP_DIR, only=['sunamita'])   # PARCHEA solo esos

UNA fila, solo `Idle`. Los locutores de los encuentros no pelean, no andan y no mueren, asi
que una hoja de 15 filas (896x1920, ~700 KB) seria 14 filas que nadie carga — y una regla
del proyecto es que assets/ no crece si se puede evitar. Por eso siguen fuera de
`characters.SHEET_NAMES`.

La camara es la MISMA de las hojas de personaje (`render.setup_scene`: orto, 30 grados de
elevacion y `shift_y` puesto para que los pies caigan en la fila 112 de 128). Eso importa:
el PNJ se dibuja en el mundo junto a los sprites de personaje, y con otro encuadre se veria
flotando o hundido respecto a ellos.

El orden de `LAYOUT` tiene que coincidir con `namespace Npc` en src/render/sprites.h.
"""
import os

import bpy
import numpy as np

from . import render as R
from . import characters as C
from . import anims
from . import parts

CELL = R.FRAME       # 128 px = 32 unidades de juego a ASSET_SCALE 4
SS = R.SS

# Mismo orden que `namespace Npc` en src/render/sprites.h, y ese orden es el de CAPITULO
# (game/events.h). Los tres encuentros que no son una persona sino una escena — los cuervos,
# el altar del Carmelo y la boca de la cueva — no salen aqui: no tienen PNJ.
LAYOUT = ['viuda_sarepta', 'eliseo_labrador', 'hombre_jerico', 'mujer_profeta',
          'sunamita', 'naaman', 'criado_eliseo',
          # Pozo de encuentros: los que ESPERAN de pie en su sala. Mismas recetas que sus
          # retratos, asi que una cara nueva sale gratis aqui.
          'abdias', 'hijo_profetas', 'hombre_baalsalisa',
          'muchacha_cautiva', 'leproso_samaria', 'principe_samaria',
          # Estos tres YA tienen hoja de 896x1920 en assets/sprites/, pero el PNJ se dibuja
          # desde aqui y de ningun otro sitio: sin celda, el rey Acab en su sala saldria de
          # hoguera. Es una celda de 128 px, no una hoja de quince filas.
          'acab', 'general_sirio', 'soldado_acab']


def _load_sheet(path, w, h):
    if not os.path.exists(path):
        return None
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = 'sRGB'
    iw, ih = img.size
    px = np.empty(iw * ih * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    arr = px.reshape(ih, iw, 4)[::-1].copy()
    return arr if (ih == h and iw == w) else None


def main(sprites_dir, tmp_dir, only=None, log=print):
    W, H = CELL * len(LAYOUT), CELL
    out_path = os.path.join(sprites_dir, 'npcs.png')
    sheet = _load_sheet(out_path, W, H) if only else None
    if sheet is None:
        sheet = np.zeros((H, W, 4), dtype=np.float32)

    wanted = only or LAYOUT
    for i, name in enumerate(LAYOUT):
        if name not in wanted:
            continue
        parts.clear_scene()
        char = C.build(name)
        anims.animate(char)
        # `clear_scene` se lleva la camara por delante: se vuelve a montar por PNJ
        R.setup_scene(tmp_dir)
        bpy.context.scene.frame_set(anims.anim_base('idle'))
        raw = R._render_to_array()
        cell = R.post_soft(raw, CELL, SS, outline=True)
        sheet[:, i * CELL:(i + 1) * CELL] = cell
        log('  pnj %-16s cobertura %.1f%%' % (name, 100.0 * (cell[..., 3] > 0.05).mean()))

    parts.clear_scene()
    R.save_png(sheet, out_path, 'mantle_npcs')
    log('npcs.png -> %s (%dx%d)' % (out_path, W, H))
    return out_path
