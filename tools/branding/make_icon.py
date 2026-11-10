"""El icono del juego -> assets/branding/{mantle.ico, mark_256.png}.

    python tools/branding/make_icon.py            # escribe el .ico y el PNG de ventana
    python tools/branding/make_icon.py preview    # ademas, la tira de contacto (16..256 px)

Desde el 2026-11-10 el arte es el de `compose.py`: Elias en contrapicado con la llama en alto
sobre el fuego del cielo, en losa redondeada (la figura la renderiza Blender con
`logo.main_heroe`, ver tools/branding/CLAUDE.md). Esto solo empaqueta.

Dos cosas que NO son obvias y siguen valiendo:

1. **El .ico se escribe a mano.** `Image.save(sizes=[...])` mete la MISMA imagen reescalada
   en todos los tamanos, y aqui cada tamano lleva su encuadre (de 48 px para abajo, cara y
   llama; de 64 para arriba, el busto con el manto) y su contorno redibujado. El formato es
   trivial: una cabecera, una entrada de 16 bytes por tamano y el PNG de cada uno detras —
   Windows acepta payload PNG dentro del .ico desde Vista.

2. **El contorno se vuelve a dibujar en cada tamano** (`compose.Art._at`): el del render son
   9 px de 1024, y a 32 px se evapora justo donde mas falta hace.

`_fit` y `_outline` se quedan: los usa `contact.py` para juzgar bocetos sueltos.
"""
import io
import os
import struct
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compose import Art                               # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BRAND = os.path.join(ROOT, 'assets', 'branding')     # lo que se distribuye
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')   # las fuentes

SIZES = [256, 128, 64, 48, 32, 24, 16]

OUTLINE = (26, 18, 22, 255)


def _outline(img, px):
    """Contorno oscuro de `px` pixeles alrededor del alfa, dibujado DEBAJO de la imagen."""
    if px <= 0:
        return img
    a = img.split()[3]
    grown = a.filter(ImageFilter.MaxFilter(px * 2 + 1))
    ring = Image.new('RGBA', img.size, OUTLINE)
    ring.putalpha(grown)
    return Image.alpha_composite(ring, img)


def _fit(src, size):
    """Reescala manteniendo proporcion y centra en un cuadrado transparente."""
    w, h = src.size
    k = size / float(max(w, h))
    r = src.resize((max(1, int(round(w * k))), max(1, int(round(h * k)))), Image.LANCZOS)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.paste(r, ((size - r.size[0]) // 2, (size - r.size[1]) // 2))
    return out


def write_ico(frames, path):
    blobs = []
    for _, img in frames:
        buf = io.BytesIO()
        img.save(buf, format='PNG', optimize=True)
        blobs.append(buf.getvalue())
    header = struct.pack('<HHH', 0, 1, len(frames))
    offset = len(header) + 16 * len(frames)
    entries = b''
    for (size, _), blob in zip(frames, blobs):
        d = 0 if size >= 256 else size          # 0 significa 256 en el formato
        entries += struct.pack('<BBBBHHII', d, d, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    with open(path, 'wb') as f:
        f.write(header + entries + b''.join(blobs))


def preview(art, frames, path=None):
    """La tira de contacto: cada tamano A TAMANO REAL y ampliado x4 (nearest) sobre fondo
    claro y oscuro, con la columna de 32 px marcada — el tamano que decide (el explorador de
    Windows, la barra de tareas)."""
    pad = 10
    big = [16, 24, 32, 48]
    w = pad + sum(s + pad for s in SIZES) + sum(s * 4 + pad for s in big) + pad
    h = pad * 3 + 256 * 2
    sheet = Image.new('RGBA', (w, h), (70, 68, 76, 255))
    d = ImageDraw.Draw(sheet)
    d.rectangle([0, 0, w, pad + 256 + pad // 2], fill=(242, 239, 233, 255))
    d.rectangle([0, pad + 256 + pad // 2, w, h], fill=(28, 26, 32, 255))
    for row, y in enumerate((pad, pad * 2 + 256)):
        x = pad
        for size, img in frames:
            sheet.alpha_composite(img, (x, y + 256 - size))
            if size == 32:
                d.rectangle([x - 3, y + 256 - size - 3, x + size + 2, y + 256 + 2],
                            outline=(255, 214, 110, 255), width=1)
            x += size + pad
        for s in big:
            img = dict(frames)[s].resize((s * 4, s * 4), Image.NEAREST)
            sheet.alpha_composite(img, (x, y + 256 - s * 4))
            x += s * 4 + pad
    path = path or os.path.join(tempfile.gettempdir(), 'mantle_icon_preview.png')
    sheet.convert('RGB').save(path)
    return path


def build(show=False):
    art = Art()
    frames = [(s, art.badge(s)) for s in SIZES]
    ico = os.path.join(BRAND, 'mantle.ico')
    write_ico(frames, ico)
    # PNG para SDL_SetWindowIcon (barra de tareas y ventana, tambien en Linux y Mac)
    win = os.path.join(BRAND, 'mark_256.png')
    dict(frames)[256].save(win, optimize=True)
    print('mantle.ico  %d tamanos (%s)  %d bytes'
          % (len(frames), ', '.join(str(s) for s in SIZES), os.path.getsize(ico)))
    print('mark_256.png %d bytes' % os.path.getsize(win))
    if show:
        # A un directorio temporal: es una ayuda para mirar, no un asset (assets/ no crece)
        print('preview -> %s' % preview(art, frames))
    return ico


if __name__ == '__main__':
    build(show='preview' in sys.argv)
