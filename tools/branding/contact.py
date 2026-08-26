"""Tira de contacto de un icono A LOS TAMANOS EN QUE SE USA.

    python tools/branding/contact.py <a.png> [<b.png> ...]   -> %TEMP%/mantle_contact.png

El render a 1024 miente: el 2026-08-26 el usuario rechazo el icono de MANTLE porque a
32 px no se reconocia nada, y se habia dado por bueno mirandolo grande. Este gate va antes
de modelar nada, y por eso vive aparte de make_icon.py (que EMPAQUETA lo ya decidido).

Cada propuesta ocupa dos filas: fondo claro y fondo oscuro. Un icono que solo funciona
sobre uno de los dos no vale — el explorador de Windows y una barra de tareas en tema
oscuro son el mismo icono.

Reusa `_fit` y `_outline` de make_icon.py a proposito: el contorno redibujado por tamano
es parte de lo que se esta juzgando, no un retoque del empaquetado.
"""
import os
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import _fit, _outline                    # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FONT = os.path.join(ROOT, 'assets', 'fonts', 'LilitaOne-Regular.ttf')

SIZES = [16, 24, 32, 48, 64, 128, 256]
LIGHT = (242, 239, 233, 255)
DARK = (28, 26, 32, 255)
GATE = 32                                               # el tamano que decide

PAD = 14
LABEL_W = 228

# Nombre legible por fichero: 'sketch_a' no dice nada a quien mira la tira, y quien mira la
# tira es el que decide.
NAMES = {'sketch_a': 'A  busto a dos',
         'sketch_b': 'B  el traspaso',
         'sketch_c': 'C  el manto solo',
         'sketch_d': 'D  marca M'}


def _font(px):
    try:
        return ImageFont.truetype(FONT, px)
    except Exception:
        return ImageFont.load_default()


def _cell(src, size):
    """Un icono ya empaquetado: margen del 6% y contorno redibujado para ESE tamano."""
    img = _fit(src, int(size * 0.94))
    img = _outline(img, 1 if size <= 32 else 2)
    return _fit(img, size)


def build(paths, out=None):
    srcs = [(os.path.splitext(os.path.basename(p))[0], Image.open(p).convert('RGBA'))
            for p in paths]
    cols = [max(s, 40) for s in SIZES]
    row_h = max(SIZES) + PAD
    width = LABEL_W + sum(c + PAD for c in cols) + PAD
    height = PAD + 26 + len(srcs) * 2 * row_h + PAD

    sheet = Image.new('RGBA', (width, height), (70, 68, 76, 255))
    d = ImageDraw.Draw(sheet)
    f_hdr, f_lbl = _font(17), _font(19)

    x = LABEL_W
    for size, c in zip(SIZES, cols):
        txt = '%d px' % size
        w = d.textlength(txt, font=f_hdr)
        col = (255, 214, 110, 255) if size == GATE else (215, 212, 220, 255)
        d.text((x + (c - w) / 2, PAD), txt, font=f_hdr, fill=col)
        x += c + PAD

    y = PAD + 26
    for name, src in srcs:
        for bg, tag in ((LIGHT, 'claro'), (DARK, 'oscuro')):
            d.rectangle([LABEL_W - PAD // 2, y, width - PAD // 2, y + row_h - 4], fill=bg)
            d.text((PAD, y + row_h // 2 - 22), NAMES.get(name, name), font=f_lbl,
                   fill=(242, 240, 245, 255))
            d.text((PAD, y + row_h // 2 + 2), 'fondo ' + tag, font=f_hdr,
                   fill=(168, 165, 176, 255))
            x = LABEL_W
            for size, c in zip(SIZES, cols):
                cell = _cell(src, size)
                sheet.paste(cell, (x + (c - size) // 2, y + (row_h - 4 - size) // 2), cell)
                x += c + PAD
            y += row_h

    # El 32 px se marca: es el unico tamano que decide.
    x = LABEL_W
    for size, c in zip(SIZES, cols):
        if size == GATE:
            d.rectangle([x - 4, PAD + 24, x + c + 2, height - PAD // 2],
                        outline=(255, 214, 110, 255), width=2)
        x += c + PAD

    out = out or os.path.join(tempfile.gettempdir(), 'mantle_contact.png')
    sheet.convert('RGB').save(out)
    print('contacto %dx%d  %d propuestas  -> %s' % (width, height, len(srcs), out))
    return out


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    if not args:
        d0 = os.path.join(tempfile.gettempdir(), 'mantle_sprites', 'logo_sketches')
        args = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith('.png')]
    build(args)
