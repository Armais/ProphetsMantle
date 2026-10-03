"""Render de las estampas de los relatos.

    python -m tools.estampas.render                 # las 46
    python -m tools.estampas.render CUERVOS ABDIAS  # solo las que contengan esos textos
    python -m tools.estampas.render --contact       # ademas, hoja de contacto en tmp/

Salida: assets/sprites/estampas/ev_<id>.png (768x384, paleta de 256 colores con
tramado: el grano ya esta en la lamina y el tramado no se ve, y pesa un tercio).
El indice <id> es el de `Ev::` en src/game/events.h: el motor carga por ese numero.
"""
import os
import sys
import time
from multiprocessing import Pool

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from estampas import plates, core  # noqa: E402

OUT = os.path.join(ROOT, 'assets', 'sprites', 'estampas')
TMP = os.path.join(ROOT, 'tools', 'estampas', 'tmp')


def out_path(idx, name):
    return os.path.join(OUT, f'ev_{idx:02d}.png')


def render_one(item):
    idx, name = item
    t = time.time()
    try:
        fn = plates.PLATES[name]
        cv = core.Canvas(px_w=768, ss=2, seed=idx * 7 + 3)
        grade = fn(cv) or {}
        img = core.finish(cv, **grade)
        os.makedirs(OUT, exist_ok=True)
        core.save_png(img, out_path(idx, name))
    except Exception:
        import traceback
        return name, 'FALLA\n' + traceback.format_exc(limit=4)
    return name, f'{time.time() - t:5.1f} s'


def contact(names, path, cols=4, scale=0.5):
    from PIL import Image
    ims = [Image.open(out_path(plates.ORDER.index(n), n)).convert('RGB') for n in names
           if os.path.exists(out_path(plates.ORDER.index(n), n))]
    w, h = int(768 * scale), int(384 * scale)
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * w, rows * h), (20, 18, 16))
    for i, im in enumerate(ims):
        sheet.paste(im.resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)
    return path


def main(argv):
    want = [a for a in argv if not a.startswith('--')]
    names = [n for n in plates.ORDER if n in plates.PLATES and (not want or any(w in n for w in want))]
    items = [(plates.ORDER.index(n), n) for n in names]
    with Pool(min(4, max(1, len(items)))) as p:
        for name, dt in p.imap_unordered(render_one, items):
            print(f'{name:22s} {dt}', flush=True)
    if '--contact' in argv:
        print(contact(names, os.path.join(TMP, 'contact.png')))


if __name__ == '__main__':
    main(sys.argv[1:])
