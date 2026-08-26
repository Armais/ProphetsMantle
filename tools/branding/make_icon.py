"""tools/branding/src/{logo,mark_512}.png -> assets/branding/{mantle.ico,mark_256.png}.

    python tools/branding/make_icon.py            # escribe el .ico y el PNG de ventana
    python tools/branding/make_icon.py preview    # ademas, una tira con todos los tamanos

El arte lo genera Blender (tools/blender/mantle_sprites/logo.py). Esto solo empaqueta.

Dos cosas que NO son obvias:

1. **El .ico se escribe a mano.** `Image.save(sizes=[...])` mete la MISMA imagen reescalada
   en todos los tamanos, y aqui hace falta arte distinto: `logo` (los dos profetas) de 64 px
   para arriba, y `mark` (el manto con la llama) de 48 para abajo. Dos figuras a 16x16 son
   una mancha parda. El formato es trivial: una cabecera, una entrada de 16 bytes por
   tamano y el PNG de cada uno detras — Windows acepta payload PNG dentro del .ico desde
   Vista.

2. **El contorno se vuelve a dibujar en cada tamano.** El de Blender esta en pixeles del
   render (5 px en una imagen de 512), asi que al bajar a 16 se queda en 0,16 px: se
   evapora justo donde mas falta hace. Se redibuja despues de reescalar, con el grosor
   pensado para ese tamano.
"""
import os
import struct
import tempfile
import sys

from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BRAND = os.path.join(ROOT, 'assets', 'branding')     # lo que se distribuye
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')   # las fuentes

# tamano -> encuadre del que sale. Las dos piezas son la MISMA composicion (el busto a dos):
# `logo` con hombros y `mark` recortado a las dos cabezas. El corte esta en 64 porque por
# debajo de ahi el encuadre ancho empasta y hace falta el cerrado, no otro motivo.
SIZES = [(256, 'logo'), (128, 'logo'), (64, 'logo'),
         (48, 'mark'), (32, 'mark'), (24, 'mark'), (16, 'mark')]

OUTLINE = (26, 18, 22, 255)


def _outline(img, px):
    """Contorno oscuro de `px` pixeles alrededor del alfa, dibujado DEBAJO de la imagen."""
    if px <= 0:
        return img
    a = img.split()[3]
    grown = a.filter(ImageFilter.MaxFilter(px * 2 + 1))
    ring = Image.new('RGBA', img.size, OUTLINE)
    ring.putalpha(grown)
    out = Image.alpha_composite(ring, img)
    return out


def _fit(src, size):
    """Reescala manteniendo proporcion y centra en un cuadrado transparente."""
    w, h = src.size
    k = size / float(max(w, h))
    r = src.resize((max(1, int(round(w * k))), max(1, int(round(h * k)))), Image.LANCZOS)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.paste(r, ((size - r.size[0]) // 2, (size - r.size[1]) // 2))
    return out


def build(preview=False):
    src = {'logo': Image.open(os.path.join(SRC, 'logo.png')).convert('RGBA'),
           'mark': Image.open(os.path.join(SRC, 'mark_512.png')).convert('RGBA')}

    frames = []
    for size, which in SIZES:
        # Margen del 6%: sin el, el contorno se come el borde del cuadro al dilatar
        img = _fit(src[which], int(size * 0.94))
        img = _outline(img, 1 if size <= 32 else 2)
        img = _fit(img, size)
        frames.append((size, img))

    # --- .ico a mano ---------------------------------------------------------
    blobs = []
    for size, img in frames:
        import io as _io
        buf = _io.BytesIO()
        img.save(buf, format='PNG', optimize=True)
        blobs.append(buf.getvalue())

    ico = os.path.join(BRAND, 'mantle.ico')
    header = struct.pack('<HHH', 0, 1, len(frames))
    offset = len(header) + 16 * len(frames)
    entries = b''
    for (size, _), blob in zip(frames, blobs):
        d = 0 if size >= 256 else size          # 0 significa 256 en el formato
        entries += struct.pack('<BBBBHHII', d, d, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    with open(ico, 'wb') as f:
        f.write(header + entries + b''.join(blobs))

    # --- PNG para SDL_SetWindowIcon ------------------------------------------
    win = os.path.join(BRAND, 'mark_256.png')
    _fit(_outline(_fit(src['mark'], 240), 2), 256).save(win)

    print('mantle.ico  %d tamanos (%s)  %d bytes'
          % (len(frames), ', '.join(str(s) for s, _ in SIZES), os.path.getsize(ico)))
    print('mark_256.png %d bytes' % os.path.getsize(win))

    if preview:
        strip = Image.new('RGBA', (sum(s for s, _ in SIZES) + 8 * len(SIZES), 272),
                          (245, 243, 240, 255))
        x = 4
        for size, img in frames:
            strip.paste(img, (x, 8), img)
            x += size + 8
        # A un directorio temporal: es una ayuda para mirar los siete tamanos de un vistazo,
        # no un asset, y assets/ no crece con material de depuracion.
        p = os.path.join(tempfile.gettempdir(), 'mantle_icon_preview.png')
        strip.save(p)
        print('preview -> %s' % p)
    return ico


if __name__ == '__main__':
    build(preview='preview' in sys.argv)
