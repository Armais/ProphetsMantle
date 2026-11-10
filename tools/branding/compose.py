"""El arte del icono (2026-11-10): Elias con la llama en alto sobre el fuego del cielo.

    from compose import Art
    art = Art()                         # lee tools/branding/src/heroe_1024.png + heroe.json
    art.full(512)                       # cuadrado a sangre (fondo + figura + llama)
    art.badge(256)                      # el mismo, en losa redondeada (Windows, ventana)
    art.foreground(432), art.background(432)   # las dos capas del adaptativo de Android

La FIGURA la renderiza Blender (`logo.main_heroe`: el muneco del juego, EEVEE, contorno); todo
lo demas se pinta aqui, en numpy, porque es lo que da el drama y en 3D no salia:

  - el FONDO: el fuego del cielo que baja (1 Reyes 18:38). Degradado radial centrado en la
    llama —blanco-oro, naranja, carmesi, casi negro en las esquinas— con rayos que salen de
    ella y ascuas. Es lo que hace que el icono "brille" en una pantalla de inicio llena de
    iconos planos, y lo que separa la figura del fondo sin depender del contorno.
  - la LLAMA del cayado: una gota con nucleo blanco, cuerpo naranja y borde rojo, cimbreada
    por el viento, con su resplandor aditivo. En 3D era un huevo amarillo plano.
  - el "light wrap": el fondo derrama su luz sobre el borde de la figura (la integra; sin el,
    parece un recorte pegado).

Reglas de icono que manda esto (ver tools/branding/CLAUDE.md): un solo foco, diagonal (llama
arriba a la derecha -> cara -> manto abajo a la izquierda), contraste fuerte, nada de texto,
y ENCUADRE POR TAMANO: de 48 px para abajo no cabe el busto entero y se recorta a cara y
llama (`crop_for`). El gate es la tira de contact.py / make_icon.py preview a 16-256 px.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'src')

# Paleta del fuego del cielo (de dentro afuera). Es la del juego: el oro de la fe
# (Color de los rotulos), el naranja de Elias y el carmesi de los cielos del Carmelo.
# La luz se queda CERCA de la llama y cae pronto a carmesi oscuro: la tunica de Elias es
# naranja, y con un fondo naranja hasta media altura la figura se fundia con el a 32 px.
FIRE_STOPS = [(0.00, (255, 243, 200)),
              (0.06, (255, 206, 96)),
              (0.15, (246, 136, 44)),
              (0.28, (190, 58, 26)),
              (0.46, (104, 20, 22)),
              (0.72, (52, 10, 20)),
              (1.00, (22, 6, 14))]

# Encuadre por tamano: (cx, cy, lado) en fracciones del render. A 64 px o mas cabe el busto
# con su manto; a 48 y menos la cara y la llama tienen que ocupar casi todo.
CROPS = {'wide': (0.53, 0.47, 0.90),
         'mid': (0.56, 0.42, 0.70),
         'tight': (0.555, 0.39, 0.60)}


def crop_for(size):
    if size >= 64:
        return 'wide'
    if size >= 40:
        return 'mid'
    return 'tight'


def _lerp_stops(t, stops):
    t = np.clip(t, 0.0, 1.0)
    out = np.zeros(t.shape + (3,), np.float32)
    for (t0, c0), (t1, c1) in zip(stops[:-1], stops[1:]):
        m = (t >= t0) & (t <= t1)
        k = ((t - t0) / max(t1 - t0, 1e-6))[m][:, None]
        out[m] = np.array(c0, np.float32) * (1 - k) + np.array(c1, np.float32) * k
    return out


def _box1(x, r, axis):
    """Media movil de ancho 2r+1 a lo largo de `axis` (bordes con relleno de ceros)."""
    if r < 1:
        return x
    k = 2 * r + 1
    pad = [(r + 1, r) if i == axis else (0, 0) for i in range(x.ndim)]
    c = np.cumsum(np.pad(x, pad, mode='edge'), axis=axis, dtype=np.float64)
    n = x.shape[axis]
    hi = np.take(c, np.arange(k, k + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return ((hi - lo) / k).astype(np.float32)


def _blur(arr, radius):
    """Desenfoque casi gaussiano de un canal float: tres pasadas de caja por eje (PIL no
    desenfoca imagenes en coma flotante)."""
    if radius <= 0:
        return arr
    r = max(1, int(round(radius / 1.73)))
    out = arr.astype(np.float32)
    for _ in range(3):
        out = _box1(_box1(out, r, 0), r, 1)
    return out


def _teardrop(xx, yy, cx, cy, w, h, lean):
    """Campo de la gota de llama: <= 1 dentro. Base redonda abajo, punta arriba que se va
    de lado (`lean`, el viento). Coordenadas en pixeles."""
    v = (cy - yy) / h                       # 0 en el centro de la base, 1 en la punta
    x = xx - cx - lean * np.clip(v, 0, None) ** 1.6 * h
    # Ancho: lleno en la base y afinandose hacia la punta (perfil de llama)
    prof = np.where(v < 0, np.sqrt(np.clip(1 - (v / 0.42) ** 2, 0, 1)),
                    np.clip(1 - v, 0, 1) ** 0.75)
    half = w * 0.5 * np.maximum(prof, 1e-4)
    inside_v = (v > -0.42) & (v < 1.0)
    d = np.where(inside_v, np.abs(x) / half, 9.0)
    return d


class Art:
    def __init__(self, src_dir=SRC):
        fig = Image.open(os.path.join(src_dir, 'heroe_1024.png')).convert('RGBA')
        with open(os.path.join(src_dir, 'heroe.json')) as f:
            meta = json.load(f)
        # El render mira a la izquierda con la llama a la izquierda: se VOLTEA. Mirando a la
        # derecha y con la llama arriba a la derecha la diagonal se lee hacia delante.
        self.fig = fig.transpose(Image.FLIP_LEFT_RIGHT)
        self.flame = (1.0 - meta['flame'][0], meta['flame'][1])
        self.head = (1.0 - meta['head'][0], meta['head'][1])
        self.eyes = [(1.0 - e[0], e[1]) for e in meta.get('eyes', [])]
        self.N = fig.size[0]
        self._cache = {}

    # ------------------------------------------------------------------ capas a 1024
    def _bg(self):
        if 'bg' not in self._cache:
            self._cache['bg'] = self._bg_window(0.0, 0.0, 1.0, self.N)
        return self._cache['bg']

    def _bg_window(self, x0, y0, side, px):
        """El fuego del cielo evaluado en una ventana cualquiera del plano del render
        (coordenadas normalizadas: 0..1 es el render). Es procedural, asi que se puede pedir
        MAS ALLA del render: el lienzo de 108 dp del adaptativo de Android es mas grande que
        lo que la figura ocupa."""
        yy, xx = np.mgrid[0:px, 0:px].astype(np.float32)
        u = x0 + (xx + 0.5) / px * side                 # coordenadas normalizadas
        v = y0 + (yy + 0.5) / px * side
        fx, fy = self.flame[0], self.flame[1] - 0.05
        dx, dy = u - fx, v - fy
        d = np.sqrt(dx * dx + dy * dy)
        base = _lerp_stops(d / 1.05, FIRE_STOPS)
        # Rayos: 14 haces que salen de la llama, mas fuertes cerca y que se apagan lejos
        ang = np.arctan2(dy, dx)
        rays = (0.5 + 0.5 * np.cos(ang * 14.0 + 0.6)) ** 3.0
        rays2 = (0.5 + 0.5 * np.cos(ang * 9.0 - 1.1)) ** 5.0
        k = np.exp(-d * 2.2)[..., None]
        base = base * (1.0 + (0.22 * rays + 0.12 * rays2)[..., None] * k)
        # Vineta: lo lejano a la llama se hunde (el ojo va a la luz)
        cx, cy = u - 0.5, v - 0.5
        vig = 1.0 - 0.55 * np.clip(np.sqrt(cx * cx + cy * cy) * 1.25 - 0.25, 0, 1) ** 1.6
        base = base * vig[..., None]
        # Ascuas: chispas que suben desde la llama (deterministas)
        rng = np.random.RandomState(1918)
        glow = np.zeros((px, px), np.float32)
        scale = px / side                                # pixeles por unidad normalizada
        for _ in range(46):
            t = rng.rand()
            r = 0.08 + 0.85 * t
            a = math.radians(rng.uniform(150.0, 300.0))
            ex = fx + math.cos(a) * r * 0.9 + rng.uniform(-0.04, 0.04)
            ey = fy + math.sin(a) * r * 0.7 + rng.uniform(-0.04, 0.04)
            rad = rng.uniform(2.2, 5.5) / 1024.0 * (1.2 - t * 0.6) * scale
            pxx, pyy = (ex - x0) * scale, (ey - y0) * scale
            rad = max(rad, 0.45)
            xa, xb = int(max(0, pxx - rad * 4)), int(min(px, pxx + rad * 4 + 1))
            ya, yb = int(max(0, pyy - rad * 4)), int(min(px, pyy + rad * 4 + 1))
            if xb <= xa or yb <= ya:
                continue
            sx, sy = xx[ya:yb, xa:xb] - pxx, yy[ya:yb, xa:xb] - pyy
            glow[ya:yb, xa:xb] += np.exp(-(sx * sx + sy * sy) / (2 * rad * rad)) * (1.0 - 0.6 * t)
        base = base + glow[..., None] * np.array((255, 196, 96), np.float32) * 0.9
        return np.clip(base, 0, 255)

    def _flame(self):
        """(rgb premultiplicado, alfa) de la llama pintada y su resplandor aditivo."""
        if 'flame' in self._cache:
            return self._cache['flame']
        N = self.N
        yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
        fx, fy = self.flame[0] * N, self.flame[1] * N
        H, W = 0.27 * N, 0.165 * N
        lean = -0.22                       # el viento la tumba hacia la izquierda (hacia la cara)
        layers = [(1.00, (226, 58, 18)), (0.80, (250, 112, 26)), (0.60, (255, 170, 52)),
                  (0.40, (255, 222, 120)), (0.22, (255, 250, 222))]
        rgb = np.zeros((N, N, 3), np.float32)
        alpha = np.zeros((N, N), np.float32)
        for s, col in layers:
            # cada capa es la gota mas pequena y un poco mas abajo: el nucleo vive en la base
            d = _teardrop(xx, yy, fx, fy - H * 0.06 * (1 - s), W * s, H * s, lean)
            a = np.clip((1.0 - d) * 6.0, 0, 1)
            rgb = rgb * (1 - a[..., None]) + np.array(col, np.float32) * a[..., None]
            alpha = np.maximum(alpha, a)
        # Dos lenguas que se escapan a los lados (la llama vive, no es una gota quieta)
        for ox, oy, s, ln in ((-0.30, 0.10, 0.42, -0.5), (0.26, 0.16, 0.34, 0.1)):
            d = _teardrop(xx, yy, fx + ox * W, fy + oy * H, W * s, H * s * 0.9, ln)
            a = np.clip((1.0 - d) * 5.0, 0, 1) * (1 - alpha)
            rgb = rgb * (1 - a[..., None]) + np.array((250, 118, 30), np.float32) * a[..., None]
            alpha = np.maximum(alpha, a)
        # Las capas se funden (sin esto la llama sale a bandas, como un mapa de cotas)
        rgb = np.dstack([_blur(rgb[..., i] * alpha, N * 0.007) for i in range(3)])
        soft_a = _blur(alpha, N * 0.007)
        rgb = np.where(soft_a[..., None] > 1e-4, rgb / np.maximum(soft_a[..., None], 1e-4), 0)
        alpha = np.maximum(alpha * 0.85, soft_a)
        halo = _blur(alpha, N * 0.045) * 0.95 + _blur(alpha, N * 0.12) * 0.55
        self._cache['flame'] = (rgb, alpha, halo)
        return self._cache['flame']

    def _figure(self):
        a = np.asarray(self.fig, np.float32)
        return a[..., :3], a[..., 3] / 255.0

    def _light_wrap(self, rgb, fa, bg):
        """Luz de borde DIRECCIONAL: el filo que mira a la llama se calienta (el fondo le
        derrama su luz) y el que le da la espalda se enfria (cielo). Calido contra frio es el
        contraste que separa una figura naranja de un fondo rojo cuando ya no hay pixeles
        para el contorno."""
        N = self.N
        soft = _blur(fa, N * 0.010)
        edge = np.clip(fa - soft, 0, 1) * 2.2
        gy, gx = np.gradient(_blur(fa, N * 0.006))
        nrm = np.sqrt(gx * gx + gy * gy) + 1e-6
        nx, ny = -gx / nrm, -gy / nrm                      # normal hacia FUERA de la figura
        yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
        tx, ty = self.flame[0] * N - xx, self.flame[1] * N - yy
        tl = np.sqrt(tx * tx + ty * ty) + 1e-6
        facing = (nx * tx + ny * ty) / tl                  # 1 = mira a la llama
        lit = _blur(bg.mean(axis=2), N * 0.02) / 255.0
        kw = np.clip(edge * np.clip(facing, 0, 1) * (0.5 + lit), 0, 0.85)[..., None]
        kc = np.clip(edge * np.clip(-facing, 0, 1) * 0.75, 0, 0.6)[..., None]
        warm = np.array((255, 196, 120), np.float32)
        cool = np.array((150, 190, 255), np.float32)
        out = rgb * (1 - kw) + warm * kw
        return out * (1 - kc) + cool * kc

    # ------------------------------------------------------------------ productos
    def _compose(self, with_bg=True):
        """(rgba float 0..255) a 1024 del render: fondo + figura + llama + resplandor."""
        key = 'comp_%d' % with_bg
        if key in self._cache:
            return self._cache[key]
        bg = self._bg()
        frgb, fa = self._figure()
        frgb = self._light_wrap(frgb, fa, bg)
        lrgb, la, halo = self._flame()
        glow_col = np.array((255, 170, 70), np.float32)
        if with_bg:
            out = bg * (1 - fa[..., None]) + frgb * fa[..., None]
            out = out * (1 - la[..., None]) + lrgb * la[..., None]
            out = out + halo[..., None] * glow_col * 0.55          # aditivo: la llama alumbra
            res = np.dstack([np.clip(out, 0, 255), np.full(fa.shape, 255.0, np.float32)])
        else:
            # Capa delantera del adaptativo: figura + llama + halo con su propio alfa
            a = np.clip(fa + la * (1 - fa), 0, 1)
            col = frgb * fa[..., None] + lrgb * (la * (1 - fa))[..., None]
            # La llama alumbra TAMBIEN dentro de su silueta (el nucleo se quema a blanco); en el
            # cuadrado a sangre lo hace la suma del resplandor sobre todo, aqui hay que pedirlo.
            col = col + (halo * la * (1 - fa))[..., None] * glow_col * 0.55
            ha = np.clip(halo * 0.65, 0, 1) * (1 - a)
            a2 = a + ha
            col = col + glow_col * ha[..., None]
            col = np.where(a2[..., None] > 1e-4, col / np.maximum(a2[..., None], 1e-4), 0)
            res = np.dstack([np.clip(col, 0, 255), np.clip(a2, 0, 1) * 255.0])
        self._cache[key] = res
        return res

    def _crop(self, arr, which, size):
        cx, cy, side = CROPS[which]
        N = self.N
        s = side * N
        x0, y0 = cx * N - s / 2, cy * N - s / 2
        im = Image.fromarray(arr.astype(np.uint8), 'RGBA')
        im = im.crop((int(round(x0)), int(round(y0)), int(round(x0 + s)), int(round(y0 + s))))
        return im.resize((size, size), Image.LANCZOS)

    def _to_px(self, p, which, size):
        cx, cy, side = CROPS[which]
        return ((p[0] - (cx - side / 2)) / side * size, (p[1] - (cy - side / 2)) / side * size)

    def _finish(self, out, which, size):
        """Lo que se pinta ya en pixeles de destino: la sombra del tercio de abajo (el ojo va a
        la cara y a la llama, no al pecho) y el destello de fuego en los ojos, con un radio
        minimo para que a 32 px siga siendo un pixel vivo."""
        h = out.shape[0]
        yy, xx = np.mgrid[0:h, 0:h].astype(np.float32)
        cx, cy, side = CROPS[which]
        ny = (cy - side / 2) + (yy + 0.5) / h * side          # y normalizada del render
        shade = 1.0 - 0.42 * np.clip((ny - 0.62) / 0.30, 0, 1) ** 1.3
        out = out * shade[..., None]
        r = max(0.55, 0.0048 * self.N * size / (side * self.N))
        for e in self.eyes:
            ex, ey = self._to_px((e[0] + 0.004, e[1] - 0.003), which, size)
            g = np.exp(-((xx - ex) ** 2 + (yy - ey) ** 2) / (2 * r * r))
            out = out + g[..., None] * np.array((255, 214, 150), np.float32) * 0.85
        return out

    def _at(self, size, which):
        """Composicion A ESE TAMANO: fondo, figura y llama se reducen por separado y el
        contorno de la figura se REDIBUJA en pixeles de destino (el del render son 9 px de
        1024: a 32 px es un tercio de pixel y se evapora, que es la leccion del icono viejo)."""
        key = ('at', size, which)
        if key in self._cache:
            return self._cache[key]
        bg = np.dstack([self._bg(), np.full((self.N, self.N), 255.0, np.float32)])
        frgb, fa = self._figure()
        frgb = self._light_wrap(frgb, fa, self._bg())
        lrgb, la, halo = self._flame()
        B = np.asarray(self._crop(bg, which, size), np.float32)[..., :3]
        F = np.asarray(self._crop(np.dstack([frgb, fa * 255.0]), which, size), np.float32)
        L = np.asarray(self._crop(np.dstack([lrgb, la * 255.0]), which, size), np.float32)
        Hh = np.asarray(self._crop(np.dstack([np.zeros_like(lrgb), np.clip(halo, 0, 1) * 255.0]),
                                   which, size), np.float32)[..., 3] / 255.0
        fa_s = F[..., 3] / 255.0
        la_s = L[..., 3] / 255.0
        if size <= 128:
            # Contorno oscuro de 1 px (2 a 65-128) alrededor de la figura, por debajo de ella
            px = 1 if size <= 64 else 2
            grown = np.asarray(Image.fromarray((fa_s * 255).astype(np.uint8), 'L')
                               .filter(ImageFilter.MaxFilter(px * 2 + 1)), np.float32) / 255.0
            ring = np.clip(grown - fa_s, 0, 1) * (0.85 if size <= 32 else 0.7)
            B = B * (1 - ring[..., None]) + np.array((26, 8, 10), np.float32) * ring[..., None]
        out = B * (1 - fa_s[..., None]) + F[..., :3] * fa_s[..., None]
        out = out * (1 - la_s[..., None]) + L[..., :3] * la_s[..., None]
        out = out + Hh[..., None] * np.array((255, 170, 70), np.float32) * 0.55
        out = self._finish(out, which, size)
        im = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')
        if size <= 32:
            im = im.filter(ImageFilter.UnsharpMask(radius=0.7, percent=45, threshold=1))
        self._cache[key] = im
        return im

    def full(self, size, which=None):
        """Cuadrado a sangre (Play Store, icono heredado de Android, base de la losa)."""
        which = which or crop_for(size)
        if size <= 128:
            return self._at(size, which).copy()
        im = self._crop(self._compose(True), which, size)
        a = np.asarray(im, np.float32).copy()
        a[..., :3] = self._finish(a[..., :3], which, size)
        return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')

    def badge(self, size, which=None, radius=0.205):
        """La losa redondeada (Windows .ico y ventana): el arte a sangre con esquinas
        redondeadas, filo oscuro y un brillo fino arriba. Se dibuja a 4x y se reduce, para que
        el borde salga suave hasta a 16 px."""
        which = which or crop_for(size)
        S = max(64, size * 4)
        if size <= 128:
            # el arte se compone A SU TAMANO (contorno redibujado) y solo la mascara va a 4x
            art = self._at(size, which).resize((S, S), Image.NEAREST)
        else:
            art = self.full(S, which)
        a = np.asarray(art, np.float32).copy()
        yy, xx = np.mgrid[0:S, 0:S].astype(np.float32) + 0.5
        r = radius * S
        # Distancia al borde de un rectangulo redondeado (negativa dentro)
        qx = np.abs(xx - S / 2) - (S / 2 - r)
        qy = np.abs(yy - S / 2) - (S / 2 - r)
        dist = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2) + np.minimum(np.maximum(qx, qy), 0) - r
        inside = np.clip(-dist, 0, 1)
        # Filo: banda oscura de ~2.5% del lado, y un brillo calido de 1.2% arriba
        band = max(1.0, S * 0.022)
        rim = np.clip(1 - (-dist) / band, 0, 1) * inside
        a[..., :3] = a[..., :3] * (1 - 0.55 * rim[..., None]) + np.array((40, 8, 6), np.float32) * 0.55 * rim[..., None]
        top = np.clip(1 - (-dist) / (band * 0.6), 0, 1) * inside * np.clip(1 - yy / (S * 0.5), 0, 1)
        a[..., :3] = a[..., :3] + 70 * top[..., None]
        a[..., 3] = 255 * inside
        im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')
        return im.resize((size, size), Image.LANCZOS)

    # ---------------------------------------------------------------- Android adaptativo
    # Lienzo de 108 dp; el launcher ensena los 72 centrales con SU mascara y solo garantiza el
    # circulo de 66. La figura (cara y llama) va dentro de ese circulo: `ADAPT` es la ventana
    # del render que cae en el lienzo entero, calculada para que el encuadre 'mid' (cara +
    # llama) ocupe los 66 dp centrales.
    ADAPT = (0.555, 0.43, 0.70 * 108.0 / 66.0)

    def background(self, size):
        """Capa de fondo del adaptativo: el fuego del cielo, a sangre, sin figura."""
        cx, cy, side = self.ADAPT
        bg = self._bg_window(cx - side / 2, cy - side / 2, side, size)
        return Image.fromarray(bg.astype(np.uint8), 'RGB').convert('RGBA')

    def foreground(self, size):
        """Capa delantera del adaptativo: figura, llama, resplandor y el destello de los ojos,
        con alfa. Lo que cae fuera del render queda transparente."""
        cx, cy, side = self.ADAPT
        fg = self._compose(False)
        N = self.N
        s = side * N
        x0, y0 = cx * N - s / 2, cy * N - s / 2
        im = Image.fromarray(fg.astype(np.uint8), 'RGBA')
        im = im.crop((int(round(x0)), int(round(y0)), int(round(x0 + s)), int(round(y0 + s))))
        im = im.resize((size, size), Image.LANCZOS)
        a = np.asarray(im, np.float32).copy()
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
        r = max(0.6, 0.0048 * N * size / s)
        for e in self.eyes:
            ex = (e[0] + 0.004 - (cx - side / 2)) / side * size
            ey = (e[1] - 0.003 - (cy - side / 2)) / side * size
            g = np.exp(-((xx - ex) ** 2 + (yy - ey) ** 2) / (2 * r * r)) * (a[..., 3] / 255.0)
            a[..., :3] = a[..., :3] + g[..., None] * np.array((255, 214, 150), np.float32) * 0.85
        return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')
