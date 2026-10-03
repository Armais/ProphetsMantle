"""Motor de las ESTAMPAS de los relatos (2026-10-03).

Peticion del usuario: «los eventos secundarios deben de tener un grafico representativo de
cada uno». Son 46 laminas (39 relatos del pozo + 4 paradas del Ascenso + 3 del epilogo) y se
pintan aqui, en 2D, con campos de distancia (SDF) evaluados con numpy — no en Blender.

Por que 2D y no un diorama mas de `cine.py`: una lamina de relato es una ESTAMPA de libro, no
un plano de cine. Lo que la hace leer es la composicion (cielo, contraluz, una figura y el
objeto del pasaje), y eso un SDF lo da limpio a cualquier resolucion: antialias exacto,
degradados, luz de borde (rim) hacia el sol, sombreado de cojin en los objetos, niebla por
distancia, brillo (bloom) y grano. Las figuras van SIN cara y a contraluz, como en un vitral:
se reconocen por el color del manto y lo que llevan (el cayado de fuego de Elias, el de luz
de Eliseo), igual que en el juego.

Sistema de coordenadas: la lamina mide W x H unidades (200 x 100, proporcion 2:1) con el
origen ARRIBA a la izquierda e Y hacia abajo. Todo se evalua sobre la caja de cada forma
(nunca sobre la lamina entera), asi que una escena con cientos de piezas sigue tardando
segundos.
"""
import math

import numpy as np

W, H = 200.0, 100.0


# =============================================================================
# Utilidades de color
# =============================================================================
def hexc(h, a=1.0):
    h = h.lstrip('#')
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)] + [a], dtype=np.float32)


def mix(a, b, t):
    return a + (b - a) * t


def lerp_col(c0, c1, t):
    c0 = hexc(c0) if isinstance(c0, str) else c0
    c1 = hexc(c1) if isinstance(c1, str) else c1
    return c0 + (c1 - c0) * t


def col(c):
    return hexc(c) if isinstance(c, str) else np.asarray(c, dtype=np.float32)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# =============================================================================
# Ruido (determinista)
# =============================================================================
def _hash(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def value_noise(x, y, seed=0):
    """Ruido de valor 2D suave (x, y arrays float)."""
    xi = np.floor(x).astype(np.int64)
    yi = np.floor(y).astype(np.int64)
    xf = x - xi
    yf = y - yi
    u = xf * xf * (3 - 2 * xf)
    v = yf * yf * (3 - 2 * yf)
    a = _hash(xi, yi, seed)
    b = _hash(xi + 1, yi, seed)
    c = _hash(xi, yi + 1, seed)
    d = _hash(xi + 1, yi + 1, seed)
    return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v


def fbm(x, y, seed=0, octaves=4, lac=2.0, gain=0.5):
    s = np.zeros_like(x, dtype=np.float32)
    amp, f, norm = 1.0, 1.0, 0.0
    for o in range(octaves):
        s += amp * value_noise(x * f, y * f, seed + o * 17)
        norm += amp
        amp *= gain
        f *= lac
    return s / norm


def noise1(x, seed=0, octaves=4):
    """Ruido 1D (perfiles de cordilleras)."""
    x = np.asarray(x, dtype=np.float64)
    return fbm(x, np.full_like(x, 0.37 + seed * 0.13), seed, octaves)


# =============================================================================
# Lienzo
# =============================================================================
class Canvas:
    """Lienzo RGB float con supermuestreo `ss`. Las coordenadas son de lamina (200x100)."""

    def __init__(self, px_w=768, ss=2, seed=1):
        self.px_w = px_w
        self.ss = ss
        self.pw = px_w * ss
        self.ph = int(round(self.pw * H / W))
        self.k = self.pw / W                     # pixeles (supermuestreados) por unidad
        self.rgb = np.zeros((self.ph, self.pw, 3), dtype=np.float32)
        self.glow = np.zeros((self.ph, self.pw, 3), dtype=np.float32)   # luz que florece
        self.seed = seed
        self.sun = (150.0, 30.0)                 # para la luz de borde
        self.light = np.array([-0.45, -0.65, 0.62], dtype=np.float32)
        self.light /= np.linalg.norm(self.light)
        self.haze = None                         # color de niebla (aire)

    # --- region de pixeles de una caja en unidades ---------------------------
    def region(self, x0, y0, x1, y1, pad=1.0):
        k = self.k
        i0 = max(0, int(math.floor((y0 - pad) * k)))
        i1 = min(self.ph, int(math.ceil((y1 + pad) * k)))
        j0 = max(0, int(math.floor((x0 - pad) * k)))
        j1 = min(self.pw, int(math.ceil((x1 + pad) * k)))
        if i1 <= i0 or j1 <= j0:
            return None
        ys = (np.arange(i0, i1, dtype=np.float32) + 0.5) / k
        xs = (np.arange(j0, j1, dtype=np.float32) + 0.5) / k
        X, Y = np.meshgrid(xs, ys)
        return (slice(i0, i1), slice(j0, j1)), X, Y

    def px(self):
        """Tamano de un pixel de SALIDA en unidades (el ancho del antialias)."""
        return 1.0 / self.k * self.ss * 0.5

    # --- pintar -------------------------------------------------------------
    def paint(self, sl, cov, rgb):
        """Compone `rgb` (h,w,3 o 3) con cobertura `cov` (h,w) sobre el lienzo."""
        cov = np.clip(cov, 0.0, 1.0)[..., None]
        self.rgb[sl] = self.rgb[sl] * (1.0 - cov) + rgb * cov

    def add_glow(self, sl, amount_rgb):
        self.glow[sl] += amount_rgb

    def fill_all(self, rgb):
        self.rgb[:] = col(rgb)[:3]


# =============================================================================
# SDF primitivos (devuelven distancia firmada: negativa dentro)
# =============================================================================
def sd_circle(X, Y, cx, cy, r):
    return np.hypot(X - cx, Y - cy) - r


def sd_ellipse(X, Y, cx, cy, rx, ry):
    # aproximacion de Inigo Quilez (suficiente para pintar)
    px, py = (X - cx) / rx, (Y - cy) / ry
    k0 = np.hypot(px, py)
    k1 = np.hypot(px / rx, py / ry)
    return np.where(k1 > 1e-6, k0 * (k0 - 1.0) / np.maximum(k1, 1e-6), -min(rx, ry))


def sd_box(X, Y, cx, cy, hw, hh, r=0.0):
    qx = np.abs(X - cx) - hw + r
    qy = np.abs(Y - cy) - hh + r
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def sd_segment(X, Y, ax, ay, bx, by):
    pax, pay = X - ax, Y - ay
    bax, bay = bx - ax, by - ay
    h = np.clip((pax * bax + pay * bay) / max(bax * bax + bay * bay, 1e-9), 0.0, 1.0)
    return np.hypot(pax - bax * h, pay - bay * h), h


def sd_capsule(X, Y, ax, ay, bx, by, r0, r1=None):
    """Capsula de radio que pasa de r0 (en a) a r1 (en b)."""
    r1 = r0 if r1 is None else r1
    d, h = sd_segment(X, Y, ax, ay, bx, by)
    return d - (r0 + (r1 - r0) * h)


def sd_poly(X, Y, pts):
    """Poligono cerrado (lista de (x, y)). Distancia exacta, signo por paridad."""
    pts = np.asarray(pts, dtype=np.float32)
    n = len(pts)
    d = np.full(X.shape, 1e9, dtype=np.float32)
    inside = np.zeros(X.shape, dtype=bool)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % n]
        dd, _ = sd_segment(X, Y, ax, ay, bx, by)
        d = np.minimum(d, dd)
        cond = ((ay > Y) != (by > Y)) & (X < (bx - ax) * (Y - ay) / (by - ay + 1e-12) + ax)
        inside ^= cond
    return np.where(inside, -d, d)


def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


def bbox_of(pts, pad=0.0):
    pts = np.asarray(pts)
    return (pts[:, 0].min() - pad, pts[:, 1].min() - pad, pts[:, 0].max() + pad, pts[:, 1].max() + pad)


# =============================================================================
# Formas: una forma = (funcion sdf(X, Y), caja)
# =============================================================================
class Shape:
    def __init__(self, fn, box):
        self.fn = fn
        self.box = box

    def __or__(self, o):            # union
        return Shape(lambda X, Y: np.minimum(self.fn(X, Y), o.fn(X, Y)), _ubox(self.box, o.box))

    def smooth(self, o, k):
        return Shape(lambda X, Y: smin(self.fn(X, Y), o.fn(X, Y), k), _ubox(self.box, o.box))

    def cut(self, o):               # resta
        return Shape(lambda X, Y: np.maximum(self.fn(X, Y), -o.fn(X, Y)), self.box)

    def inter(self, o):
        return Shape(lambda X, Y: np.maximum(self.fn(X, Y), o.fn(X, Y)), self.box)

    def grow(self, r):
        b = self.box
        return Shape(lambda X, Y: self.fn(X, Y) - r, (b[0] - r, b[1] - r, b[2] + r, b[3] + r))


def _ubox(a, b):
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def circle(cx, cy, r):
    return Shape(lambda X, Y: sd_circle(X, Y, cx, cy, r), (cx - r, cy - r, cx + r, cy + r))


def ellipse(cx, cy, rx, ry):
    return Shape(lambda X, Y: sd_ellipse(X, Y, cx, cy, rx, ry), (cx - rx, cy - ry, cx + rx, cy + ry))


def rect(x0, y0, x1, y1, r=0.0):
    cx, cy, hw, hh = (x0 + x1) / 2, (y0 + y1) / 2, abs(x1 - x0) / 2, abs(y1 - y0) / 2
    return Shape(lambda X, Y: sd_box(X, Y, cx, cy, hw, hh, r), (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))


def capsule(ax, ay, bx, by, r0, r1=None):
    r1 = r0 if r1 is None else r1
    R = max(r0, r1)
    return Shape(lambda X, Y: sd_capsule(X, Y, ax, ay, bx, by, r0, r1),
                 (min(ax, bx) - R, min(ay, by) - R, max(ax, bx) + R, max(ay, by) + R))


def poly(pts):
    pts = [tuple(p) for p in pts]
    return Shape(lambda X, Y: sd_poly(X, Y, pts), bbox_of(pts))


def polyline(pts, r0, r1=None, k=0.0):
    """Trazo de capsulas encadenadas (ramas, cuerdas, cuernos). Radio interpolado."""
    r1 = r0 if r1 is None else r1
    n = len(pts) - 1
    shapes = []
    for i in range(n):
        ra = r0 + (r1 - r0) * i / n
        rb = r0 + (r1 - r0) * (i + 1) / n
        shapes.append(capsule(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], ra, rb))
    s = shapes[0]
    for o in shapes[1:]:
        s = s.smooth(o, k) if k > 0 else (s | o)
    return s


def bezier(p0, p1, p2, p3=None, n=16):
    """Puntos de una curva de Bezier cuadratica (3 puntos) o cubica (4)."""
    pts = []
    for i in range(n + 1):
        t = i / n
        if p3 is None:
            x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]
            y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]
        else:
            x = ((1 - t) ** 3 * p0[0] + 3 * (1 - t) ** 2 * t * p1[0] + 3 * (1 - t) * t * t * p2[0]
                 + t ** 3 * p3[0])
            y = ((1 - t) ** 3 * p0[1] + 3 * (1 - t) ** 2 * t * p1[1] + 3 * (1 - t) * t * t * p2[1]
                 + t ** 3 * p3[1])
        pts.append((x, y))
    return pts


# =============================================================================
# Rellenos
# =============================================================================
def draw(cv, shape, color, *, grad=None, shade=0.0, bevel=3.0, rim=0.0, rim_col=None,
         rim_w=1.4, alpha=1.0, glow=0.0, glow_col=None, haze=0.0, tex=0.0, tex_scale=0.6,
         edge=None, edge_w=0.0, soft=0.0, emissive=False, tex_aniso=(1.0, 1.0)):
    """Pinta una forma.

    grad: None | ('v', c_top, c_bottom) | ('h', c_left, c_right) | ('r', cx, cy, r, c_in, c_out)
    shade: 0..1 cuanto sombreado de cojin (luz de arriba-izquierda) dentro de `bevel` unidades
    rim: intensidad de la luz de borde hacia `cv.sun`
    haze: 0..1 mezcla con el color del aire (lejania)
    tex: grano de la superficie (ruido) 0..1
    edge: color del contorno (linea interior de `edge_w`)
    soft: difumina el borde (unidades) para siluetas atmosfericas
    """
    b = shape.box
    pad = max(bevel, rim_w, soft, edge_w) + 1.5
    reg = cv.region(b[0], b[1], b[2], b[3], pad)
    if reg is None:
        return
    sl, X, Y = reg
    d = shape.fn(X, Y)
    aa = cv.px() + soft
    cov = np.clip(0.5 - d / aa, 0.0, 1.0) * alpha
    if not np.any(cov > 0):
        return
    c = col(color)[:3]
    base = np.broadcast_to(c, X.shape + (3,)).copy()
    if grad is not None:
        kind = grad[0]
        if kind == 'v':
            t = np.clip((Y - b[1]) / max(b[3] - b[1], 1e-6), 0, 1)[..., None]
            base = col(grad[1])[:3] * (1 - t) + col(grad[2])[:3] * t
        elif kind == 'h':
            t = np.clip((X - b[0]) / max(b[2] - b[0], 1e-6), 0, 1)[..., None]
            base = col(grad[1])[:3] * (1 - t) + col(grad[2])[:3] * t
        elif kind == 'r':
            _, gx, gy, gr, ci, co = grad
            t = np.clip(np.hypot(X - gx, Y - gy) / gr, 0, 1)[..., None]
            base = col(ci)[:3] * (1 - t) + col(co)[:3] * t
        elif kind == 'vy':   # vertical con limites propios (y0, y1)
            _, y0, y1, ct, cb = grad
            t = np.clip((Y - y0) / (y1 - y0), 0, 1)[..., None]
            base = col(ct)[:3] * (1 - t) + col(cb)[:3] * t
    if tex > 0:
        n = fbm(X * tex_scale * tex_aniso[0], Y * tex_scale * tex_aniso[1], cv.seed + 7, 4)
        base = base * (1.0 + (n[..., None] - 0.5) * tex)
    if shade > 0 or rim > 0:
        gy_, gx_ = np.gradient(d)
        gl = np.sqrt(gx_ * gx_ + gy_ * gy_) + 1e-6
        nx, ny = gx_ / gl, gy_ / gl
        if shade > 0:
            depth = np.clip(-d / bevel, 0.0, 1.0)
            s = 1.0 - depth
            nz = np.sqrt(np.clip(1.0 - s * s, 0, 1))
            Lx, Ly, Lz = cv.light
            lam = np.clip(nx * s * Lx + ny * s * Ly + nz * Lz, 0.0, 1.0)
            k = (0.55 + 0.6 * lam)
            base = base * (1.0 + (k[..., None] - 1.0) * shade)
        if rim > 0:
            sx, sy = cv.sun
            vx, vy = sx - X, sy - Y
            vl = np.hypot(vx, vy) + 1e-6
            facing = np.clip((nx * vx + ny * vy) / vl, 0.0, 1.0)
            band = smoothstep(rim_w, 0.0, -d) * (d < 0.5)
            rc = col(rim_col if rim_col is not None else '#fff1c8')[:3]
            r = (facing * band * rim)[..., None]
            base = base * (1 - r) + rc * r
    if edge is not None and edge_w > 0:
        e = smoothstep(edge_w + aa, edge_w - aa, -d)[..., None]
        base = base * (1 - e) + col(edge)[:3] * e
    if haze > 0 and cv.haze is not None:
        base = base * (1 - haze) + col(cv.haze)[:3] * haze
    cv.paint(sl, cov, base)
    if glow > 0:
        gc = col(glow_col if glow_col is not None else color)[:3]
        cv.add_glow(sl, (cov[..., None] * glow) * gc)


def shadow(cv, cx, cy, rx, ry, strength=0.45, color='#000000'):
    """Sombra de contacto (mancha eliptica suave)."""
    reg = cv.region(cx - rx * 1.5, cy - ry * 1.5, cx + rx * 1.5, cy + ry * 1.5)
    if reg is None:
        return
    sl, X, Y = reg
    t = np.hypot((X - cx) / rx, (Y - cy) / ry)
    a = (1.0 - smoothstep(0.2, 1.3, t)) * strength
    cv.paint(sl, a, col(color)[:3])


def glow_spot(cv, cx, cy, r, color, amount=1.0, power=2.0):
    """Luz que florece (se suma en la capa de bloom y algo directo)."""
    reg = cv.region(cx - r, cy - r, cx + r, cy + r)
    if reg is None:
        return
    sl, X, Y = reg
    t = np.clip(1.0 - np.hypot(X - cx, Y - cy) / r, 0.0, 1.0) ** power
    c = col(color)[:3]
    cv.rgb[sl] += (t[..., None] * amount * 0.6) * c
    cv.add_glow(sl, (t[..., None] * amount * 0.4) * c)


def light_rays(cv, cx, cy, color, n=9, spread=1.1, length=140.0, strength=0.18, seed=3,
               angle=math.pi / 2, width=0.06):
    """Rayos de luz (god rays) en abanico desde (cx, cy) hacia `angle` (radianes, Y abajo)."""
    reg = cv.region(0, 0, W, H)
    sl, X, Y = reg
    dx, dy = X - cx, Y - cy
    r = np.hypot(dx, dy) + 1e-6
    a = np.arctan2(dy, dx)
    acc = np.zeros(X.shape, dtype=np.float32)
    rng = np.random.default_rng(seed)
    for i in range(n):
        ai = angle + (i / max(n - 1, 1) - 0.5) * spread + rng.uniform(-0.04, 0.04)
        wi = width * rng.uniform(0.5, 1.4)
        da = np.angle(np.exp(1j * (a - ai)))
        acc += np.exp(-(da / wi) ** 2) * rng.uniform(0.5, 1.0)
    fall = np.clip(1.0 - r / length, 0.0, 1.0) ** 1.5
    c = col(color)[:3]
    cv.rgb[sl] += (acc * fall * strength)[..., None] * c


def particles(cv, n, box, color, size=(0.3, 0.8), seed=5, glow=0.6, alpha=(0.4, 1.0), streak=None):
    """Motas, pavesas, estrellas o lluvia (streak=(dx, dy))."""
    rng = np.random.default_rng(seed)
    c = col(color)
    for i in range(n):
        x = rng.uniform(box[0], box[2])
        y = rng.uniform(box[1], box[3])
        r = rng.uniform(*size)
        a = rng.uniform(*alpha)
        if streak is not None:
            s = capsule(x, y, x + streak[0], y + streak[1], r * 0.5, r * 0.5)
        else:
            s = circle(x, y, r)
        draw(cv, s, c, alpha=a, glow=glow * a, soft=r * 0.6)


# =============================================================================
# Post: bloom, grado de color, vineta, grano y bajada a la resolucion final
# =============================================================================
def _blur(img, r):
    """Desenfoque gaussiano separable aproximado (tres cajas)."""
    out = img
    for _ in range(3):
        out = _box(out, r, 0)
        out = _box(out, r, 1)
    return out


def _box(a, r, axis):
    if r < 1:
        return a
    c = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(a.ndim)], mode='edge'),
                  axis=axis, dtype=np.float64)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return ((hi - lo) / (2 * r + 1)).astype(np.float32)


def finish(cv, *, bloom=0.55, bloom_r=0.06, threshold=0.78, vignette=0.38, grain=0.025,
           sat=1.06, contrast=1.06, lift=None, gain=None, warm=0.0):
    img = cv.rgb.copy()
    # bloom: brillo de lo que pasa del umbral + la capa de glow
    lum = img.mean(axis=2, keepdims=True)
    bright = np.clip(img - threshold, 0, None) * (lum > threshold) + cv.glow
    small = bright[::4, ::4]
    r = max(1, int(cv.pw * bloom_r / 4 / 3))
    b1 = _blur(small, r)
    b2 = _blur(small, r * 3)
    bl = np.repeat(np.repeat(b1 * 0.6 + b2 * 0.6, 4, axis=0), 4, axis=1)[:cv.ph, :cv.pw]
    img = img + bl * bloom
    # grado
    if lift is not None:
        img = img + col(lift)[:3] * (1 - img) * 0.08
    if gain is not None:
        img = img * (0.85 + 0.3 * col(gain)[:3])
    if warm:
        img = img * np.array([1 + warm * 0.06, 1.0, 1 - warm * 0.08], dtype=np.float32)
    m = img.mean(axis=2, keepdims=True)
    img = m + (img - m) * sat
    img = (img - 0.5) * contrast + 0.5
    # tonemap suave (los brillos no se queman a blanco plano)
    img = np.where(img > 0.85, 0.85 + (1 - np.exp(-(img - 0.85) * 3.0)) * 0.15 / 1.0, img)
    # vineta
    yy, xx = np.mgrid[0:cv.ph, 0:cv.pw].astype(np.float32)
    nx = (xx / cv.pw - 0.5) * 2.0
    ny = (yy / cv.ph - 0.5) * 2.0
    v = 1.0 - vignette * smoothstep(0.35, 1.45, np.sqrt(nx * nx * 0.8 + ny * ny * 1.1))
    img = img * v[..., None]
    # bajar al tamano final (caja ss x ss)
    s = cv.ss
    h2, w2 = cv.ph // s, cv.pw // s
    img = img[:h2 * s, :w2 * s].reshape(h2, s, w2, s, 3).mean(axis=(1, 3))
    # grano
    if grain > 0:
        rng = np.random.default_rng(cv.seed + 99)
        g = rng.normal(0, grain, (h2, w2, 1)).astype(np.float32)
        img = img + g * (0.6 + 0.4 * img)
    return np.clip(img, 0.0, 1.0)


def save_png(img, path, quantize=True):
    from PIL import Image
    im = Image.fromarray((img * 255.0 + 0.5).astype(np.uint8), 'RGB')
    if quantize:
        im = im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    im.save(path, optimize=True)
    return im


# =============================================================================
# Figuras recortadas (tools/estampas/figuras.py): los munecos del juego, posados en Blender
# =============================================================================
_FIG_INDEX = None
_FIG_CACHE = {}
FIG_DIR = __import__('os').path.join(__import__('os').path.dirname(__file__), 'figs')


def _fig(key):
    import json
    import os
    global _FIG_INDEX
    if _FIG_INDEX is None:
        with open(os.path.join(FIG_DIR, 'index.json')) as f:
            _FIG_INDEX = json.load(f)
    if key not in _FIG_CACHE:
        from PIL import Image
        _FIG_CACHE[key] = Image.open(os.path.join(FIG_DIR, f'{key}.png')).convert('RGBA')
    return _FIG_CACHE[key], _FIG_INDEX[key]


def cutout(cv, key, x, y, human_h, *, backlit=0.0, rim=1.0, rim_col=None, haze=0.0, flip=False,
           tint=None, shadow_k=0.45, alpha=1.0, glow=0.0, glow_col=None, light=1.0):
    """Pega la figura `key` con los PIES (el origen del armature) en (x, y). `human_h` es lo
    que mide un humano de 1,75 m A ESA DISTANCIA, en unidades de lamina: la escala la da la
    figura, no el recorte (un hombre arrodillado sale mas bajo solo).

    backlit: 0..1 la figura se apaga hacia la sombra (contraluz) y el borde se enciende
    rim:     luz de borde hacia `cv.sun`; tint: color de la luz ambiente que la multiplica
    """
    from PIL import Image
    im, info = _fig(key)
    if flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    k = info['mpp'] * human_h / 1.75 * cv.k        # px de lienzo por px de figura
    w = max(2, int(round(im.width * k)))
    h = max(2, int(round(im.height * k)))
    pm = im.convert('RGBa').resize((w, h), Image.LANCZOS).convert('RGBA')
    arr = np.asarray(pm, dtype=np.float32) / 255.0
    ox = info['origin_x'] if not flip else im.width - info['origin_x']
    x0 = x * cv.k - ox * k
    y0 = y * cv.k - info['feet_y'] * k
    # recorte al lienzo
    i0, j0 = int(round(y0)), int(round(x0))
    i1, j1 = i0 + h, j0 + w
    ci0, cj0 = max(0, i0), max(0, j0)
    ci1, cj1 = min(cv.ph, i1), min(cv.pw, j1)
    if ci1 <= ci0 or cj1 <= cj0:
        return
    sub = arr[ci0 - i0:ci1 - i0, cj0 - j0:cj1 - j0]
    rgb = sub[..., :3].copy()
    a = sub[..., 3] * alpha
    # sombra de contacto antes de pegar
    if shadow_k > 0:
        foot_w = human_h * 0.22
        shadow(cv, x, y + 0.2, foot_w, foot_w * 0.22, shadow_k)
    if tint is not None:
        rgb = rgb * (0.55 + 0.45 * col(tint)[:3]) if light >= 1.0 else rgb * col(tint)[:3]
    if light != 1.0:
        rgb = rgb * light
    if backlit > 0:
        dark = col('#1c1418')[:3]
        rgb = rgb * (1 - backlit * 0.62) + dark * backlit * 0.62 * 0.6
    if rim > 0:
        b = _blur(a[..., None], max(1, int(cv.k * 0.35)))[..., 0]
        gy, gx = np.gradient(b)
        gl = np.sqrt(gx * gx + gy * gy) + 1e-6
        nx, ny = -gx / gl, -gy / gl
        yy, xx = np.mgrid[ci0:ci1, cj0:cj1].astype(np.float32)
        sx, sy = cv.sun[0] * cv.k, cv.sun[1] * cv.k
        vx, vy = sx - xx, sy - yy
        vl = np.sqrt(vx * vx + vy * vy) + 1e-6
        facing = np.clip((nx * vx + ny * vy) / vl, 0.0, 1.0)
        edge = np.clip((1.0 - b) * 2.6, 0.0, 1.0) * (a > 0.05)
        r = np.clip(facing * edge * rim * (1.0 + backlit), 0.0, 1.0)[..., None]
        rc = col(rim_col if rim_col is not None else '#fff1c8')[:3]
        rgb = rgb * (1 - r) + rc * r
        cv.glow[ci0:ci1, cj0:cj1] += r * rc * 0.35
    if haze > 0 and cv.haze is not None:
        rgb = rgb * (1 - haze) + col(cv.haze)[:3] * haze
    sl = (slice(ci0, ci1), slice(cj0, cj1))
    cv.paint(sl, a, rgb)
    if glow > 0:
        gc = col(glow_col or '#ffffff')[:3]
        cv.glow[ci0:ci1, cj0:cj1] += a[..., None] * glow * gc
