"""tools/branding/src/mark_512.png -> android/app/src/main/res/mipmap-*/

    python tools/branding/make_android_icons.py            # escribe los iconos
    python tools/branding/make_android_icons.py preview    # ademas, la tira a 48 dp

Escribe DOS familias, porque Android tiene dos:

- `mipmap-*dpi/ic_launcher.png`      icono heredado, 48 dp. Android 7 y anteriores (minSdk 21).
- `mipmap-*dpi/ic_launcher_{foreground,background}.png` + `mipmap-anydpi-v26/ic_launcher.xml`
  icono ADAPTATIVO, 108 dp de lienzo. Android 8 en adelante.

**Por que hace falta el adaptativo** (2026-09-11, el usuario: "el icono se ve pequeno"): desde
API 26 el launcher no dibuja el `mipmap` heredado tal cual — lo mete dentro de su mascara y lo
ENCOGE, ademas de ponerle fondo. El arte acaba ocupando una fraccion del cuadro. Con icono
adaptativo el lienzo es de 108 dp y la mascara ensena los 72 dp centrales: el arte que llena
esos 72 dp llena el icono entero.

Y una trampa propia de este arte: `mark_512.png` sale de Blender con 122 px de alfa vacio
ARRIBA (bbox (0,122,512,512)). Encuadrarlo por el lienzo, como se hacia antes, dejaba el
dibujo pegado al borde de abajo y con una banda muerta del 24% encima — o sea, pequeno y
descentrado antes incluso de que el launcher lo tocara. Por eso `_crop` va primero, siempre.

Reusa `_fit` y `_outline` de make_icon.py: el contorno se REDIBUJA en cada tamano porque el
del render de Blender esta en pixeles de una imagen de 512 y al bajar a 48 se evapora.
"""
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import _fit, _outline, SRC, ROOT   # noqa: E402

RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')

# carpeta -> factor de densidad (mdpi 1x ... xxxhdpi 4x)
DENSITIES = [('mipmap-mdpi', 1), ('mipmap-hdpi', 1.5), ('mipmap-xhdpi', 2),
             ('mipmap-xxhdpi', 3), ('mipmap-xxxhdpi', 4)]

LEGACY_DP = 48        # el icono heredado se dibuja a 48 dp
CANVAS_DP = 108       # lienzo del adaptativo
SAFE_DP = 72          # lo que la mascara deja ver, y donde tiene que caber TODO

# Fondo: el pardo oscuro de la paleta del juego (Color::baal_dark, 44/24/16), con el centro
# un punto mas claro. Plano se ve muerto a 48 dp; el degradado le da volumen sin competir con
# el naranja de Elias ni el azul de Eliseo.
BG_IN = (74, 46, 30)
BG_OUT = (30, 18, 13)


def _crop(src):
    """Recorta al alfa. Sin esto el arte hereda los 122 px muertos del render."""
    box = src.getbbox()
    return src.crop(box) if box else src


def _canvas(size, art, art_side, outline_px):
    """`art` centrado en un cuadrado transparente de `size`, escalado a `art_side`."""
    img = _outline(_fit(art, art_side), outline_px)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.paste(img, ((size - img.size[0]) // 2, (size - img.size[1]) // 2), img)
    return out


def _background(size):
    """Degradado radial. Se dibuja a 1/4 y se reescala: mas rapido y sin bandas."""
    n = max(8, size // 4)
    bg = Image.new('RGB', (n, n))
    px = bg.load()
    c = (n - 1) / 2.0
    for y in range(n):
        for x in range(n):
            d = min(1.0, (((x - c) ** 2 + (y - c) ** 2) ** 0.5) / (c * 1.35))
            t = d * d
            px[x, y] = tuple(int(BG_IN[i] + (BG_OUT[i] - BG_IN[i]) * t) for i in range(3))
    return bg.resize((size, size), Image.BICUBIC).convert('RGBA')


def build(preview=False):
    art = _crop(Image.open(os.path.join(SRC, 'mark_512.png')).convert('RGBA'))

    for folder, k in DENSITIES:
        out_dir = os.path.join(RES, folder)
        os.makedirs(out_dir, exist_ok=True)

        # --- heredado (Android <= 7): margen del 6%, como el .ico ---------------
        legacy_px = int(round(LEGACY_DP * k))
        leg = _canvas(legacy_px, art, int(legacy_px * 0.94),
                      max(1, int(round(legacy_px / 48.0))))
        leg.save(os.path.join(out_dir, 'ic_launcher.png'))

        # --- adaptativo (Android >= 8) -----------------------------------------
        canvas_px = int(round(CANVAS_DP * k))
        safe_px = int(round(SAFE_DP * k))
        fg = _canvas(canvas_px, art, safe_px, max(1, int(round(canvas_px / 108.0))))
        fg.save(os.path.join(out_dir, 'ic_launcher_foreground.png'))
        _background(canvas_px).save(os.path.join(out_dir, 'ic_launcher_background.png'))

        print('%-18s heredado %3d  adaptativo %3d (seguro %3d)'
              % (folder, legacy_px, canvas_px, safe_px))

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
