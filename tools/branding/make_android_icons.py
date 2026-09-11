"""tools/branding/src/mark_512.png -> android/app/src/main/res/mipmap-*/ic_launcher.png

    python tools/branding/make_android_icons.py            # escribe los cinco PNG
    python tools/branding/make_android_icons.py preview     # ademas, una tira a 48 dp

Reusa `_fit` y `_outline` de make_icon.py: el contorno se REDIBUJA en cada tamano porque el
del render de Blender esta en pixeles de una imagen de 512 y al bajar a 48 se evapora.

Por que `mark` (las dos cabezas) y no `logo` (el busto con hombros): un icono de launcher se
juzga a 48 dp, y ese es justo el corte que make_icon.py ya usa para el .ico — de 48 para
abajo, el encuadre ancho empasta. La regla de tools/branding/CLAUDE.md es mirarlo AL TAMANO
FINAL, no al render de 1024.

Densidades de Android: el icono se dibuja a 48 dp, asi que el PNG de cada carpeta es
48 dp x el factor de su densidad (mdpi 1x ... xxxhdpi 4x).
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import _fit, _outline, SRC, ROOT   # noqa: E402

RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')

# carpeta -> lado en pixeles (48 dp por el factor de densidad)
DENSITIES = [('mipmap-mdpi', 48), ('mipmap-hdpi', 72), ('mipmap-xhdpi', 96),
             ('mipmap-xxhdpi', 144), ('mipmap-xxxhdpi', 192)]


def build(preview=False):
    src = Image.open(os.path.join(SRC, 'mark_512.png')).convert('RGBA')

    frames = []
    for folder, size in DENSITIES:
        # Margen del 6%, igual que el .ico: sin el, el contorno se come el borde al dilatar
        img = _fit(src, int(size * 0.94))
        # Grosor proporcional: 1 px a 48 se ve igual que 4 px a 192
        img = _outline(img, max(1, int(round(size / 48.0))))
        img = _fit(img, size)

        out_dir = os.path.join(RES, folder)
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, 'ic_launcher.png')
        img.save(path)
        print('%-18s %3dx%-3d  %s' % (folder, size, size, path))
        frames.append((size, img))

    if preview:
        # A 48 dp, que es como se ve en el launcher: el render grande siempre miente
        strip = Image.new('RGBA', (48 * len(frames) + 8 * (len(frames) + 1), 64), (32, 28, 34, 255))
        x = 8
        for _, img in frames:
            strip.paste(img.resize((48, 48), Image.LANCZOS), (x, 8), img.resize((48, 48), Image.LANCZOS))
            x += 48 + 8
        p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'android_icons_preview.png')
        strip.save(p)
        print('preview a 48 dp:', p)


if __name__ == '__main__':
    build(preview=('preview' in sys.argv))
