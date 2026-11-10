"""El icono de Android -> android/app/src/main/res/mipmap-*/

    python tools/branding/make_android_icons.py            # escribe los iconos
    python tools/branding/make_android_icons.py preview    # ademas, la tira a 48/96 dp

Escribe DOS familias, porque Android tiene dos:

- `mipmap-*dpi/ic_launcher.png`      icono heredado, 48 dp. Android 7 y anteriores (minSdk 21).
  El launcher lo pinta TAL CUAL, asi que va en su losa redondeada (`Art.badge`).
- `mipmap-*dpi/ic_launcher_{foreground,background}.png` + `mipmap-anydpi-v26/ic_launcher.xml`
  icono ADAPTATIVO, 108 dp de lienzo. Android 8 en adelante.

**Por que hace falta el adaptativo** (2026-09-11, el usuario: "el icono se ve pequeno"): desde
API 26 el launcher no dibuja el `mipmap` heredado tal cual — lo mete dentro de su mascara y lo
ENCOGE, ademas de ponerle fondo. Con icono adaptativo el lienzo es de 108 dp, la mascara
ensena los 72 dp centrales y solo el circulo de 66 esta garantizado.

Desde el 2026-11-10 las dos capas salen de `compose.Art` (el icono dinamico, ver
tools/branding/CLAUDE.md): el FONDO es el fuego del cielo a sangre, evaluado en todo el lienzo
de 108 dp (es procedural: no se acaba en el borde del render), y el FRENTE es Elias con la
llama, colocado (`Art.ADAPT`) para que cara y llama caigan dentro del circulo seguro. Que las
dos capas vayan separadas no es solo obligacion del formato: los launchers con paralaje las
mueven por separado y la llama "flota" sobre el fuego.
"""
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import ROOT              # noqa: E402
from compose import Art                 # noqa: E402

RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')

# carpeta -> factor de densidad (mdpi 1x ... xxxhdpi 4x)
DENSITIES = [('mipmap-mdpi', 1), ('mipmap-hdpi', 1.5), ('mipmap-xhdpi', 2),
             ('mipmap-xxhdpi', 3), ('mipmap-xxxhdpi', 4)]

LEGACY_DP = 48        # el icono heredado se dibuja a 48 dp
CANVAS_DP = 108       # lienzo del adaptativo
SAFE_DP = 72          # lo que la mascara deja ver


def build(preview=False):
    art = Art()
    for folder, k in DENSITIES:
        out_dir = os.path.join(RES, folder)
        os.makedirs(out_dir, exist_ok=True)

        # --- heredado (Android <= 7): la losa, con un margen del 4% como los de sistema ---
        legacy_px = int(round(LEGACY_DP * k))
        inner = int(round(legacy_px * 0.92))
        leg = Image.new('RGBA', (legacy_px, legacy_px), (0, 0, 0, 0))
        b = art.badge(inner)
        leg.alpha_composite(b, ((legacy_px - inner) // 2, (legacy_px - inner) // 2))
        leg.save(os.path.join(out_dir, 'ic_launcher.png'), optimize=True)

        # --- adaptativo (Android >= 8) -----------------------------------------
        canvas_px = int(round(CANVAS_DP * k))
        art.background(canvas_px).convert('RGB').save(
            os.path.join(out_dir, 'ic_launcher_background.png'), optimize=True)
        art.foreground(canvas_px).save(
            os.path.join(out_dir, 'ic_launcher_foreground.png'), optimize=True)
        print('%-18s heredado %3d  adaptativo %3d' % (folder, legacy_px, canvas_px))

    # --- el XML que ata las dos capas ------------------------------------------
    any_dir = os.path.join(RES, 'mipmap-anydpi-v26')
    os.makedirs(any_dir, exist_ok=True)
    xml = ('<?xml version="1.0" encoding="utf-8"?>\n'
           '<!-- Icono adaptativo: el launcher recorta los 108 dp con SU mascara y ensena\n'
           '     los 72 dp centrales. Lo genera tools/branding/make_android_icons.py. -->\n'
           '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
           '    <background android:drawable="@mipmap/ic_launcher_background" />\n'
           '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
           '</adaptive-icon>\n')
    with open(os.path.join(any_dir, 'ic_launcher.xml'), 'w', encoding='utf-8') as f:
        f.write(xml)
    print('%-18s ic_launcher.xml' % 'mipmap-anydpi-v26')

    if preview:
        _preview()


# ============================================================================
# Tira de contacto: el icono YA MASCARADO, a 48 dp. El render grande miente
# (tools/branding/CLAUDE.md), y en Android miente dos veces: lo que se ve no es
# el PNG, es el PNG recortado por la mascara del launcher de turno.
# ============================================================================

MASKS = ['circulo', 'redondeado', 'squircle']


def _mask(kind, size):
    n = size * 4
    m = Image.new('L', (n, n), 0)
    d = ImageDraw.Draw(m)
    if kind == 'circulo':
        d.ellipse([0, 0, n - 1, n - 1], fill=255)
    elif kind == 'redondeado':
        d.rounded_rectangle([0, 0, n - 1, n - 1], radius=int(n * 0.22), fill=255)
    else:
        c = r = (n - 1) / 2.0
        e = 4.0
        px = m.load()
        for y in range(n):
            for x in range(n):
                if (abs(x - c) / r) ** e + (abs(y - c) / r) ** e <= 1.0:
                    px[x, y] = 255
    return m.resize((size, size), Image.LANCZOS)


def _preview():
    """%TEMP%/mantle_android_icons.png: las tres mascaras a 48 dp, sobre claro y oscuro."""
    import tempfile
    k = 4                                  # xxxhdpi: 432 de lienzo
    canvas = CANVAS_DP * k
    src_dir = os.path.join(RES, 'mipmap-xxxhdpi')
    bg = Image.open(os.path.join(src_dir, 'ic_launcher_background.png')).convert('RGBA')
    fg = Image.open(os.path.join(src_dir, 'ic_launcher_foreground.png')).convert('RGBA')
    full = Image.alpha_composite(bg, fg)
    # Lo que el launcher ensena: los 72 dp centrales del lienzo de 108
    off = (canvas - SAFE_DP * k) // 2
    view = full.crop((off, off, canvas - off, canvas - off))

    cells = []
    for kind in MASKS:
        for px in (48, 96):
            ic = view.resize((px, px), Image.LANCZOS)
            ic.putalpha(_mask(kind, px))
            cells.append((kind, px, ic))
    # Y el heredado, para comparar con lo que habia: Android lo encoge dentro de la mascara
    leg = Image.open(os.path.join(src_dir, 'ic_launcher.png')).convert('RGBA')
    for px in (48, 96):
        cells.append(('heredado', px, leg.resize((px, px), Image.LANCZOS)))

    pad = 12
    w = sum(c[1] + pad for c in cells) + pad
    strip = Image.new('RGBA', (w, 96 * 2 + pad * 3), (242, 239, 233, 255))
    ImageDraw.Draw(strip).rectangle([0, 96 + pad * 2, w, strip.size[1]], fill=(28, 26, 32, 255))
    x = pad
    for kind, px, ic in cells:
        strip.paste(ic, (x, pad + (96 - px)), ic)
        strip.paste(ic, (x, 96 + pad * 2 + (96 - px)), ic)
        x += px + pad
    p = os.path.join(tempfile.gettempdir(), 'mantle_android_icons.png')
    strip.save(p)
    print('tira a 48/96 dp (%s + heredado) -> %s' % (', '.join(MASKS), p))


if __name__ == '__main__':
    build(preview=('preview' in sys.argv))
