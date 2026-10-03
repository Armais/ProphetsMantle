"""Piezas de las estampas: cielo, cordilleras, agua, vegetacion, casas, figuras, animales y
objetos del pasaje. Todo sobre `core` (SDF + numpy).

Convenciones:
- Las coordenadas son de lamina (200 x 100, Y hacia abajo).
- Las figuras se apoyan por los PIES: `figure(cv, x, y_pies, alto, ...)`.
- `rim` es la luz de borde hacia `cv.sun`: lo que hace que una silueta a contraluz lea.
"""
import math

import numpy as np

from .core import (W, H, Shape, Canvas, col, hexc, lerp_col, mix, smoothstep, fbm, noise1,
                   circle, ellipse, rect, capsule, poly, polyline, bezier, draw, shadow,
                   glow_spot, light_rays, particles, sd_circle)


# =============================================================================
# Cielo y luz
# =============================================================================
def sky(cv, stops, horizon=62.0):
    """Degradado vertical por paradas [(y, color), ...] sobre la lamina entera."""
    reg = cv.region(0, 0, W, H)
    sl, X, Y = reg
    ys = [s[0] for s in stops]
    cs = [col(s[1])[:3] for s in stops]
    out = np.zeros(X.shape + (3,), dtype=np.float32)
    for i in range(len(stops) - 1):
        y0, y1 = ys[i], ys[i + 1]
        t = np.clip((Y - y0) / max(y1 - y0, 1e-6), 0, 1)[..., None]
        m = ((Y >= y0) & (Y < y1))[..., None] if i < len(stops) - 2 else (Y >= y0)[..., None]
        out = np.where(m, cs[i] * (1 - t) + cs[i + 1] * t, out)
    out = np.where((Y < ys[0])[..., None], cs[0], out)
    # una pizca de ruido en el cielo para que no sea un degradado de catalogo
    n = fbm(X * 0.05, Y * 0.12, cv.seed + 3, 3)
    out = out * (1.0 + (n[..., None] - 0.5) * 0.05)
    cv.rgb[sl] = out


def sun(cv, x, y, r, color='#fff4d6', halo='#ffd27a', halo_r=60.0, amount=0.9):
    cv.sun = (x, y)
    glow_spot(cv, x, y, halo_r, halo, amount * 0.55, power=2.2)
    glow_spot(cv, x, y, halo_r * 0.35, halo, amount * 0.5, power=1.6)
    draw(cv, circle(x, y, r), color, glow=0.9 * amount, soft=0.3)


def moon(cv, x, y, r, color='#eef2ff', halo='#9fb4ff'):
    cv.sun = (x, y)
    glow_spot(cv, x, y, r * 7, halo, 0.35, power=2.4)
    draw(cv, circle(x, y, r), color, glow=0.5)
    draw(cv, circle(x + r * 0.35, y - r * 0.2, r * 0.85), lerp_col(color, '#b8c4e8', 0.35), alpha=0.25)


def stars(cv, n=90, y_max=55.0, seed=11, color='#ffffff'):
    particles(cv, n, (0, 0, W, y_max), color, size=(0.12, 0.38), seed=seed, glow=0.8, alpha=(0.25, 1.0))


def cloud(cv, x, y, w, h, color, shade_col=None, seed=0, alpha=0.95, rim=0.6, rim_col=None, soft=0.6):
    """Cumulo: union suave de circulos, iluminado desde el sol."""
    rng = np.random.default_rng(seed)
    n = max(5, int(w / 6))
    s = None
    for i in range(n):
        t = i / (n - 1)
        cx = x - w / 2 + t * w
        rr = h * (0.45 + 0.55 * math.sin(math.pi * t)) * rng.uniform(0.75, 1.1)
        cy = y - rr * 0.35 + rng.uniform(-1, 1) * h * 0.1
        c = circle(cx, cy, rr)
        s = c if s is None else s.smooth(c, h * 0.3)
    base = rect(x - w / 2 - h, y, x + w / 2 + h, y + h * 2)
    s = s.cut(base)
    draw(cv, s, color, grad=('v', col(color), col(shade_col or color)), rim=rim,
         rim_col=rim_col or '#fff6e0', rim_w=2.2, alpha=alpha, soft=soft)


def cloud_band(cv, y, h, color, alpha=0.5, seed=4, x0=-10, x1=210):
    """Estrato: una franja alargada y desigual (atardeceres, cielo cerrado)."""
    xs = np.linspace(x0, x1, 120)
    top = y - h * (0.4 + 0.6 * noise1(xs * 0.05, seed, 3))
    bot = y + h * (0.2 + 0.5 * noise1(xs * 0.07, seed + 1, 3))
    pts = list(zip(xs, top)) + list(zip(xs[::-1], bot[::-1]))
    draw(cv, poly(pts), color, alpha=alpha, soft=1.2)


# =============================================================================
# Terreno
# =============================================================================
def ridge_shape(fn_y, x0=-2.0, x1=W + 2.0, y_min=0.0, y_max=H + 2.0):
    """Forma bajo una linea de perfil y = fn_y(x) (todo lo que queda por DEBAJO)."""
    def f(X, Y):
        yr = fn_y(X)
        dy = np.gradient(yr, axis=1) if yr.ndim == 2 and yr.shape[1] > 1 else 0.0
        k = max(1e-6, 1.0)
        slope = dy / np.maximum(np.gradient(X, axis=1), 1e-6) if yr.ndim == 2 and yr.shape[1] > 1 else 0.0
        return (yr - Y) / np.sqrt(1.0 + slope * slope) * k
    return Shape(f, (x0, y_min, x1, y_max))


def mountains(cv, base, amp, color, *, seed=1, freq=0.025, sharp=0.55, haze=0.0, rim=0.0,
              grad_to=None, octaves=5, peaks=None, soft=0.0, tex=0.0, fog=0.45):
    """Cordillera: perfil de ruido con picos. `peaks` = [(x, alto, ancho)] fuerza cumbres."""
    def prof(X):
        n = fbm(X * freq, np.full_like(X, seed * 1.7), seed, octaves)
        ridged = 1.0 - np.abs(n * 2.0 - 1.0)
        y = base - amp * (n * (1 - sharp) + ridged * sharp)
        if peaks:
            for (px, ph, pw) in peaks:
                y = np.minimum(y, base - ph * np.exp(-((X - px) / pw) ** 2) - amp * 0.2 * n)
        return y
    top = base - amp * 1.4 - (max(p[1] for p in peaks) if peaks else 0)
    s = ridge_shape(prof, y_min=top)
    if grad_to is not None:
        grad = ('vy', base - amp, H, col(color), col(grad_to))
    elif cv.haze is not None and fog > 0:
        # niebla que se posa en el valle: la base de cada cordillera se aclara hacia el aire
        grad = ('vy', base - amp * 0.9, base + 2, col(color), lerp_col(color, cv.haze, fog))
    else:
        grad = None
    draw(cv, s, color, grad=grad, haze=haze, rim=rim, rim_w=1.6, soft=soft, tex=tex, tex_scale=0.15)


def profile(cv, fn, color, *, grad_to=None, haze=0.0, rim=0.6, rim_w=1.8, tex=0.2, tex_scale=0.12,
            tex_aniso=(0.25, 2.5), y_min=0.0, soft=0.0, fog=0.0):
    """Masa bajo el perfil y = fn(X) (paredes de canon, montes a medida). Textura de estratos
    (ruido estirado en horizontal) y luz de borde en la cresta."""
    s = ridge_shape(fn, y_min=y_min)
    if grad_to is not None:
        grad = ('v', col(color), col(grad_to))
    elif fog > 0 and cv.haze is not None:
        grad = ('v', col(color), lerp_col(color, cv.haze, fog))
    else:
        grad = None
    draw(cv, s, color, grad=grad, haze=haze, rim=rim, rim_w=rim_w, tex=tex, tex_scale=tex_scale,
         tex_aniso=tex_aniso, soft=soft)


def canyon_fn(center, half_w, base, height, *, seed=0, power=1.6, rough=6.0, freq=0.09, skew=0.0):
    """Perfil en V de un canon: alto en los lados, se abre hacia `center`."""
    def fn(X):
        d = np.clip((np.abs(X - center) - half_w * 0.25) / half_w, 0.0, None)
        side = np.where(X < center, 1.0 + skew, 1.0 - skew)
        n = fbm(X * freq, np.full_like(X, 2.3 + seed), seed, 5)
        ridged = 1.0 - np.abs(n * 2 - 1)
        return base - height * np.clip(d, 0, 1.6) ** power * side - rough * ridged
    return fn


def wall_fn(x_high, x_low, base, height, *, seed=0, rough=5.0, ledges=4, power=1.3, freq=0.07):
    """Una pared que sube de `x_low` (al ras de `base`) a `x_high` (alto `height`), con cornisas
    (escalones suaves) y un filo de cresta irregular."""
    def fn(X):
        t = np.clip((X - x_low) / (x_high - x_low), 0.0, 1.4)
        n = fbm(X * freq, np.full_like(X, 1.3 + seed), seed, 5)
        ridged = 1.0 - np.abs(n * 2 - 1)
        stepped = np.floor(t * ledges + n * 0.8) / ledges
        tt = t * 0.65 + stepped * 0.35
        return base - height * np.clip(tt, 0, None) ** power - rough * ridged * np.clip(t * 3, 0, 1)
    return fn


def hills(cv, base, amp, color, *, seed=2, freq=0.018, haze=0.0, rim=0.0, grad_to=None, tex=0.08):
    def prof(X):
        n = fbm(X * freq, np.full_like(X, seed * 0.9), seed, 3)
        return base - amp * n
    s = ridge_shape(prof, y_min=base - amp * 1.2)
    grad = ('vy', base - amp, H, col(color), col(grad_to)) if grad_to is not None else None
    draw(cv, s, color, grad=grad, haze=haze, rim=rim, rim_w=1.4, tex=tex, tex_scale=0.2)


def ground(cv, y, top_col, bottom_col, *, wave=1.2, seed=3, tex=0.10):
    def prof(X):
        return y - wave * fbm(X * 0.04, np.full_like(X, 3.1), seed, 3)
    draw(cv, ridge_shape(prof, y_min=y - wave * 1.5), top_col,
         grad=('vy', y, H, col(top_col), col(bottom_col)), tex=tex, tex_scale=0.35)


def river(cv, pts_l, pts_r, deep='#2a5a7a', shallow='#6fb0c8', sparkle='#e8f8ff', seed=6, bank='#5a4430',
          smooth=True):
    """Rio entre dos orillas (polilineas de igual longitud, de lejos a cerca). Las orillas se
    suavizan con Catmull-Rom: una orilla de cuatro tramos rectos es una regla, no un rio."""
    if smooth:
        pts_l, pts_r = _catmull(pts_l), _catmull(pts_r)
    pts = list(pts_l) + list(pts_r[::-1])
    s = poly(pts)
    b = s.box
    # orilla mojada (mas oscura) por fuera del agua
    draw(cv, s.grow(1.1), bank, alpha=0.55, soft=0.8)
    draw(cv, s, shallow, grad=('v', col(shallow), col(deep)))
    # reflejo del cielo en la mitad lejana y brillo del borde
    draw(cv, s.cut(s.grow(-0.6)), '#d8f0f8', alpha=0.35)
    rng = np.random.default_rng(seed)
    for i in range(int((b[3] - b[1]) * 1.6)):
        t = rng.uniform(0, 1) ** 0.8
        k = int(t * (len(pts_l) - 1))
        xl, yl = pts_l[k]
        xr, yr = pts_r[k]
        u = rng.uniform(0.15, 0.85)
        x = xl + (xr - xl) * u
        yy = yl + (yr - yl) * u
        ln = rng.uniform(0.6, 3.0) * (0.4 + t)
        draw(cv, capsule(x - ln, yy, x + ln, yy, 0.14 + 0.12 * t), sparkle, alpha=rng.uniform(0.2, 0.65),
             glow=0.25)


def _catmull(pts, n=8):
    pts = [tuple(p) for p in pts]
    P = [pts[0]] + pts + [pts[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            x = 0.5 * (2 * p1[0] + (-p0[0] + p2[0]) * t + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
                       (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3)
            y = 0.5 * (2 * p1[1] + (-p0[1] + p2[1]) * t + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
                       (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3)
            out.append((x, y))
    out.append(pts[-1])
    return out


def cliff(cv, side, x_edge, y_top, y_bot, color, *, seed=0, rim=0.8, haze=0.0, depth=26.0, strata=True,
          dark=None):
    """Pared de canon a un lado (`side` -1 izquierda, 1 derecha): borde dentado vertical,
    cornisas y estratos horizontales. La cara que mira al sol se enciende con la luz de borde."""
    rng = np.random.default_rng(seed)
    ys = np.linspace(y_top, y_bot, 28)
    xs = []
    x = x_edge
    for i, yy in enumerate(ys):
        x = x_edge + side * (-(i / len(ys)) * depth * 0.25) + rng.uniform(-2.2, 2.2) + \
            (side * rng.uniform(1.5, 4.0) if i % 5 == 0 else 0)
        xs.append(x)
    edge = list(zip(xs, ys))
    far = -10 if side < 0 else W + 10
    pts = [(far, y_top - 30)] + [(edge[0][0], y_top - 30)] + edge + [(edge[-1][0], H + 2), (far, H + 2)]
    s = poly(pts)
    dark = dark or lerp_col(color, '#000000', 0.45)
    draw(cv, s, color, grad=('h', col(color), col(dark)) if side > 0 else ('h', col(dark), col(color)),
         rim=rim, rim_w=2.4, haze=haze, tex=0.25, tex_scale=0.18)
    if strata:
        for k in range(9):
            yy = y_top + (y_bot - y_top) * (k + rng.uniform(0.2, 0.8)) / 9
            x0 = far
            x1 = np.interp(yy, ys, xs) - side * rng.uniform(1, 3)
            draw(cv, capsule(min(x0, x1), yy, max(x0, x1), yy + rng.uniform(-0.6, 0.6), rng.uniform(0.15, 0.4)),
                 lerp_col(color, '#000000', 0.35), alpha=0.5, haze=haze)


def pool(cv, cx, cy, rx, ry, color='#3d7c96', rim_col='#bfe6f2'):
    draw(cv, ellipse(cx, cy, rx, ry), color, grad=('v', col(rim_col), col(color)))
    draw(cv, ellipse(cx - rx * 0.2, cy - ry * 0.25, rx * 0.5, ry * 0.18), '#ffffff', alpha=0.25, soft=0.5)


def rocks(cv, x, y, s, color='#8a7a68', seed=0, n=3, rim=0.5):
    rng = np.random.default_rng(seed)
    for i in range(n):
        rx = s * rng.uniform(0.6, 1.1)
        ry = s * rng.uniform(0.45, 0.7)
        cx = x + rng.uniform(-s, s) * 0.9
        cy = y - ry * 0.6 + rng.uniform(-0.3, 0.3) * s
        shadow(cv, cx, y + 0.3, rx * 1.1, ry * 0.35, 0.35)
        draw(cv, ellipse(cx, cy, rx, ry), color, shade=0.9, bevel=ry, rim=rim, tex=0.12)


def tuft(cv, x, y, h, color='#7a8a3a', n=5, seed=0, alpha=1.0):
    rng = np.random.default_rng(seed)
    for i in range(n):
        a = math.radians(-90 + rng.uniform(-35, 35))
        L = h * rng.uniform(0.6, 1.0)
        bx = x + rng.uniform(-h * 0.3, h * 0.3)
        tip = (bx + math.cos(a) * L, y + math.sin(a) * L)
        mid = (bx + math.cos(a) * L * 0.5 + rng.uniform(-0.6, 0.6), y + math.sin(a) * L * 0.5)
        draw(cv, polyline(bezier((bx, y), mid, tip, n=6), h * 0.07, 0.05), color, alpha=alpha, rim=0.5)


# =============================================================================
# Vegetacion
# =============================================================================
def palm(cv, x, y, h, trunk='#6b4a2e', leaf='#3f7a3a', lean=0.15, seed=0, rim=0.6):
    tip = (x + h * lean, y - h)
    trunk_pts = bezier((x, y), (x + h * lean * 0.2, y - h * 0.5), tip, n=10)
    shadow(cv, x, y + 0.4, h * 0.12, h * 0.03, 0.35)
    draw(cv, polyline(trunk_pts, h * 0.035, h * 0.022), trunk, shade=0.6, bevel=1.0, rim=rim, tex=0.3,
         tex_scale=2.0)
    rng = np.random.default_rng(seed)
    dark_leaf = lerp_col(leaf, '#000000', 0.3)
    for i in range(9):
        a = math.radians(-185 + i * (190 / 8) + rng.uniform(-8, 8))
        L = h * rng.uniform(0.36, 0.48)
        droop = h * (0.22 if abs(math.cos(a)) > 0.5 else 0.08)
        p1 = (tip[0] + math.cos(a) * L * 0.5, tip[1] + math.sin(a) * L * 0.5 - h * 0.06)
        p2 = (tip[0] + math.cos(a) * L, tip[1] + math.sin(a) * L * 0.3 + droop)
        pts = bezier(tip, p1, p2, n=14)
        c = leaf if i % 2 else dark_leaf
        draw(cv, polyline(pts, h * 0.012, h * 0.004), c, rim=rim)
        for k in range(2, len(pts) - 1):
            x0, y0 = pts[k]
            x1, y1 = pts[k + 1]
            dx, dy = x1 - x0, y1 - y0
            ln = math.hypot(dx, dy) + 1e-6
            nx, ny = -dy / ln, dx / ln
            lf = h * 0.11 * (1.0 - k / len(pts)) + h * 0.03
            for sgn in (1, -1):
                ex = x0 + nx * sgn * lf + dx / ln * lf * 0.6
                ey = y0 + ny * sgn * lf + dy / ln * lf * 0.6 + lf * 0.35
                draw(cv, capsule(x0, y0, ex, ey, h * 0.008, h * 0.002), c, rim=rim * 0.8)
    draw(cv, circle(tip[0], tip[1], h * 0.04), lerp_col(trunk, '#000000', 0.2))
    for k in range(3):
        draw(cv, circle(tip[0] + (k - 1) * h * 0.03, tip[1] + h * 0.04, h * 0.022), '#6a3a1a', shade=0.8)


def bush(cv, x, y, w, h, color='#4f6a2e', seed=0, rim=0.6, berries=None):
    """Mata redonda (el enebro / la retama de 1 R 19:4)."""
    rng = np.random.default_rng(seed)
    shadow(cv, x, y + 0.5, w * 0.6, h * 0.12, 0.4)
    s = None
    for i in range(9):
        cx = x + rng.uniform(-w * 0.45, w * 0.45)
        cy = y - h * rng.uniform(0.25, 0.75)
        r = min(w, h) * rng.uniform(0.25, 0.4)
        c = circle(cx, cy, r)
        s = c if s is None else s.smooth(c, r * 0.5)
    s = s.cut(rect(x - w, y, x + w, y + h))
    draw(cv, s, color, shade=0.9, bevel=min(w, h) * 0.4, rim=rim, tex=0.35, tex_scale=1.4)
    if berries:
        for i in range(10):
            draw(cv, circle(x + rng.uniform(-w * 0.4, w * 0.4), y - h * rng.uniform(0.2, 0.8), 0.35), berries)


def tree(cv, x, y, h, trunk='#5a4030', leaf='#5a7a3a', seed=0, rim=0.6, kind='olive'):
    """Olivo / acacia / sauce: tronco retorcido y copa de manchas."""
    rng = np.random.default_rng(seed)
    shadow(cv, x, y + 0.5, h * 0.35, h * 0.06, 0.4)
    tw = h * 0.06
    pts = bezier((x, y), (x - h * 0.08, y - h * 0.35), (x + h * 0.05, y - h * 0.6), n=8)
    draw(cv, polyline(pts, tw, tw * 0.6), trunk, shade=0.7, bevel=tw, rim=rim, tex=0.3, tex_scale=1.5)
    if kind == 'acacia':
        cy, rx, ry, n = y - h * 0.78, h * 0.55, h * 0.12, 7
    else:
        cy, rx, ry, n = y - h * 0.7, h * 0.4, h * 0.25, 9
    s = None
    for i in range(n):
        cx = x + rng.uniform(-rx, rx) * 0.8
        cyy = cy + rng.uniform(-ry, ry) * 0.6
        r = rng.uniform(0.35, 0.55) * (ry if kind == 'acacia' else ry * 1.2) + h * 0.05
        c = ellipse(cx, cyy, r * (1.7 if kind == 'acacia' else 1.0), r)
        s = c if s is None else s.smooth(c, r * 0.4)
    draw(cv, s, leaf, shade=0.9, bevel=h * 0.12, rim=rim, tex=0.4, tex_scale=1.0)


def reeds(cv, x0, x1, y, h, color='#7a8a4a', seed=0, n=30):
    rng = np.random.default_rng(seed)
    for i in range(n):
        x = rng.uniform(x0, x1)
        hh = h * rng.uniform(0.5, 1.0)
        bend = rng.uniform(-2, 2)
        draw(cv, polyline(bezier((x, y), (x + bend * 0.3, y - hh * 0.6), (x + bend, y - hh), n=6), 0.25, 0.08),
             color, rim=0.5)


# =============================================================================
# Arquitectura
# =============================================================================
def house(cv, x, y, w, h, wall='#c8a878', roof='#8a6a48', door=True, window=True, rim=0.5, haze=0.0,
          shade_side=True):
    """Casa de adobe de tejado plano, en tres cuartos (frente + costado en sombra)."""
    side = w * 0.35
    shadow(cv, x + side * 0.4, y + 0.5, w * 0.7, h * 0.12, 0.35)
    if shade_side:
        draw(cv, poly([(x + w / 2, y), (x + w / 2 + side, y - side * 0.45), (x + w / 2 + side, y - h - side * 0.45),
                       (x + w / 2, y - h)]), lerp_col(wall, '#2a1e18', 0.35), haze=haze, tex=0.15)
    draw(cv, rect(x - w / 2, y - h, x + w / 2, y), wall, rim=rim, haze=haze, tex=0.18, tex_scale=0.5,
         grad=('v', lerp_col(wall, '#ffffff', 0.08), lerp_col(wall, '#000000', 0.12)))
    draw(cv, poly([(x - w / 2 - 0.8, y - h), (x + w / 2 + 0.8, y - h), (x + w / 2 + side + 0.8, y - h - side * 0.45),
                   (x - w / 2 + side - 0.8, y - h - side * 0.45)]), roof, haze=haze, rim=rim)
    draw(cv, rect(x - w / 2 - 0.8, y - h - 0.6, x + w / 2 + 0.8, y - h + 0.8), lerp_col(roof, '#000000', 0.2),
         haze=haze)
    if door:
        draw(cv, rect(x - w * 0.12, y - h * 0.55, x + w * 0.12, y, r=w * 0.1), '#2a1c14', haze=haze)
    if window:
        draw(cv, rect(x + w * 0.22, y - h * 0.78, x + w * 0.36, y - h * 0.62), '#2a1c14', haze=haze)


def wall_city(cv, x0, x1, y, h, color='#b89a74', towers=3, rim=0.5, haze=0.0, gate=None, seed=0):
    """Muralla con torres y almenas (Jerico, Samaria, Moab)."""
    shadow(cv, (x0 + x1) / 2, y + 0.6, (x1 - x0) * 0.55, 1.5, 0.35)
    draw(cv, rect(x0, y - h, x1, y), color, rim=rim, haze=haze, tex=0.22, tex_scale=0.4,
         grad=('v', lerp_col(color, '#ffffff', 0.06), lerp_col(color, '#000000', 0.18)))
    # almenas
    step = 3.0
    xx = x0
    while xx < x1 - 1:
        draw(cv, rect(xx, y - h - 1.6, xx + step * 0.55, y - h + 0.2), color, rim=rim, haze=haze)
        xx += step
    for i in range(towers):
        tx = x0 + (x1 - x0) * (i + 0.5) / towers
        tw = h * 0.42
        draw(cv, rect(tx - tw, y - h * 1.35, tx + tw, y), lerp_col(color, '#ffffff', 0.04), rim=rim, haze=haze,
             tex=0.22, tex_scale=0.4)
        for k in range(3):
            cxk = tx - tw + tw * 2 * (k + 0.5) / 3
            draw(cv, rect(cxk - tw * 0.22, y - h * 1.35 - 1.6, cxk + tw * 0.22, y - h * 1.35 + 0.2), color,
                 rim=rim, haze=haze)
        draw(cv, rect(tx - 0.5, y - h * 1.1, tx + 0.5, y - h * 0.9), '#2a1c14', haze=haze)
    if gate is not None:
        gx = gate
        gw = h * 0.32
        gh = h * 0.72
        arch = rect(gx - gw, y - gh + gw, gx + gw, y + 0.2) | circle(gx, y - gh + gw, gw)
        frame = arch.grow(1.0)
        draw(cv, frame, lerp_col(color, '#000000', 0.25), haze=haze, rim=rim * 0.6)
        draw(cv, arch, '#2a1c14', grad=('v', col('#120c0a'), col('#3a2a1e')), haze=haze)
        for k in range(4):
            px = gx - gw + gw * 2 * (k + 0.5) / 4
            draw(cv, rect(px - 0.25, y - gh + gw * 0.6, px + 0.25, y), '#4a3424', alpha=0.6, haze=haze)


def tent(cv, x, y, w, h, color='#a8784a', stripe='#6a3a2a', rim=0.5, haze=0.0):
    shadow(cv, x, y + 0.5, w * 0.6, h * 0.1, 0.35)
    draw(cv, poly([(x - w / 2, y), (x, y - h), (x + w / 2, y)]), color, rim=rim, haze=haze, tex=0.15,
         grad=('h', lerp_col(color, '#ffffff', 0.1), lerp_col(color, '#000000', 0.25)))
    draw(cv, poly([(x - w * 0.12, y), (x, y - h * 0.55), (x + w * 0.12, y)]), '#2a1c14', haze=haze)
    draw(cv, polyline([(x - w * 0.42, y - h * 0.16), (x, y - h * 0.97), (x + w * 0.42, y - h * 0.16)], 0.3),
         stripe, haze=haze)


def altar(cv, x, y, w, h, stone='#9a8a74', wood='#6a4a2a', trench=False, water=False, rim=0.6):
    """Altar de doce piedras con lena encima (1 R 18:31-33)."""
    if trench:
        draw(cv, ellipse(x, y + 1.2, w * 0.95, h * 0.28), '#3a2c22')
        if water:
            draw(cv, ellipse(x, y + 1.0, w * 0.9, h * 0.22), '#5a9ab8', grad=('v', col('#bfe6f2'), col('#3d7c96')),
                 glow=0.15, glow_col='#9fd8ff')
    shadow(cv, x, y + 0.3, w * 0.6, h * 0.12, 0.45)
    rows = 3
    for r in range(rows):
        n = 5 - r
        rw = w * (1.0 - r * 0.18)
        for k in range(n):
            sx = x - rw / 2 + rw * (k + 0.5) / n
            sy = y - h * (r + 0.5) / rows
            draw(cv, ellipse(sx, sy, rw / n * 0.55, h / rows * 0.55), lerp_col(stone, '#ffffff', 0.05 * ((k + r) % 2)),
                 shade=0.9, bevel=h / rows * 0.4, rim=rim, tex=0.2)
    for k in range(5):
        ly = y - h - 0.8 - k * 0.9
        draw(cv, capsule(x - w * 0.42 + k * 0.4, ly, x + w * 0.42 - k * 0.3, ly - 0.2, 0.55), wood, shade=0.7,
             bevel=0.5, rim=rim)


def fire(cv, x, y, s, core='#fff2b0', mid='#ffb43a', outer='#e0461a', seed=0, glow=1.0):
    """Hoguera / llama: tres capas de lenguas y un resplandor que florece."""
    rng = np.random.default_rng(seed)
    glow_spot(cv, x, y - s * 0.6, s * 3.2, '#ff9a3a', 0.55 * glow)
    for layer, (c, k) in enumerate(((outer, 1.0), (mid, 0.72), (core, 0.45))):
        sh = None
        for i in range(5):
            dx = rng.uniform(-0.45, 0.45) * s * k
            hh = s * k * rng.uniform(0.8, 1.35)
            tongue = capsule(x + dx * 0.4, y - s * 0.15, x + dx, y - hh, s * 0.32 * k, s * 0.02)
            sh = tongue if sh is None else sh.smooth(tongue, s * 0.2)
        draw(cv, sh, c, glow=0.9 * glow, soft=0.25)


def cave_mouth(cv, x, y, w, h, rock='#6a5a50', dark='#120e10', rim=0.6, seed=0):
    """Boca de cueva en una pared de roca."""
    rng = np.random.default_rng(seed)
    mass = None
    for i in range(7):
        cx = x + rng.uniform(-w, w) * 0.9
        cy = y - h * rng.uniform(0.3, 1.0)
        r = w * rng.uniform(0.45, 0.7)
        c = ellipse(cx, cy, r, r * 0.8)
        mass = c if mass is None else mass.smooth(c, r * 0.4)
    mass = mass | rect(x - w * 1.4, y - h * 0.5, x + w * 1.4, y)
    draw(cv, mass, rock, shade=0.9, bevel=w * 0.25, rim=rim, tex=0.3, tex_scale=0.3)
    opening = ellipse(x, y - h * 0.32, w * 0.42, h * 0.45).cut(rect(x - w, y, x + w, y + h))
    draw(cv, opening, dark, grad=('v', col('#2a2024'), col(dark)))


def jar(cv, x, y, h, color='#b8743a', band='#7a3a1a', rim=0.6, shade=1.0, handles=True):
    """Tinaja / cantaro: perfil torneado."""
    w = h * 0.36
    prof = [(0.0, 0.34), (0.08, 0.30), (0.12, 0.26), (0.22, 0.38), (0.45, 0.50), (0.7, 0.46), (0.9, 0.34),
            (1.0, 0.26)]
    pts_r = [(x + w * p[1] * 2, y - h * p[0]) for p in prof]
    pts_r = [(x + (px - x), py) for px, py in pts_r]
    pts = [(x - (px - x), py) for px, py in pts_r[::-1]] + pts_r
    shadow(cv, x, y + 0.3, w * 1.1, h * 0.08, 0.45)
    if handles:
        for sgn in (1, -1):
            draw(cv, polyline(bezier((x + sgn * w * 0.55, y - h * 0.9), (x + sgn * w * 1.2, y - h * 0.85),
                                     (x + sgn * w * 0.9, y - h * 0.55), n=8), h * 0.04), color, shade=0.6, rim=rim)
    draw(cv, poly(pts), color, shade=shade, bevel=w * 0.8, rim=rim, tex=0.12)
    draw(cv, rect(x - w * 0.95, y - h * 0.62, x + w * 0.95, y - h * 0.56), band, alpha=0.8)
    draw(cv, ellipse(x, y - h * 0.995, w * 0.52, h * 0.035), '#2a1a10')


def cruse(cv, x, y, h, color='#d8b878', rim=0.6):
    """Vasija pequena de aceite."""
    jar(cv, x, y, h, color=color, band='#9a7a3a', rim=rim, handles=False)
    draw(cv, polyline(bezier((x + h * 0.15, y - h * 0.85), (x + h * 0.45, y - h * 0.8), (x + h * 0.32, y - h * 0.5),
                             n=6), h * 0.05), color, shade=0.5, rim=rim)


def bread(cv, x, y, w, color='#d0965a', rim=0.6):
    draw(cv, ellipse(x, y - w * 0.22, w * 0.5, w * 0.24), color, shade=1.0, bevel=w * 0.2, rim=rim, tex=0.2)
    for k in range(3):
        draw(cv, capsule(x - w * 0.22 + k * w * 0.2, y - w * 0.36, x - w * 0.14 + k * w * 0.2, y - w * 0.3, 0.25),
             lerp_col(color, '#ffffff', 0.25), alpha=0.7)


def pot(cv, x, y, w, color='#4a3a32', rim=0.6, steam=True, fire_under=True, seed=0):
    """Olla al fuego (2 R 4:38)."""
    if fire_under:
        fire(cv, x, y + 0.2, w * 0.35, seed=seed, glow=0.7)
    draw(cv, ellipse(x, y - w * 0.42, w * 0.55, w * 0.42).cut(rect(x - w, y - w * 0.95, x + w, y - w * 0.72)),
         color, shade=1.0, bevel=w * 0.3, rim=rim, tex=0.15)
    draw(cv, ellipse(x, y - w * 0.74, w * 0.46, w * 0.09), '#6a7a3a')
    if steam:
        for k in range(3):
            draw(cv, polyline(bezier((x - w * 0.2 + k * w * 0.2, y - w * 0.8), (x - w * 0.35 + k * w * 0.2, y - w * 1.2),
                                     (x - w * 0.1 + k * w * 0.2, y - w * 1.6), n=8), w * 0.05, w * 0.12),
                 '#ffffff', alpha=0.18, soft=0.8)


def coins(cv, x, y, n=7, r=0.9, color='#e8ecf2', seed=0, spread=3.0):
    rng = np.random.default_rng(seed)
    for i in range(n):
        cx = x + rng.uniform(-spread, spread)
        cy = y - rng.uniform(0, spread * 0.4)
        draw(cv, ellipse(cx, cy, r, r * 0.45), color, shade=0.8, bevel=r * 0.5, rim=0.6, glow=0.15)


def sack(cv, x, y, h, color='#a08a6a', rim=0.5):
    shadow(cv, x, y + 0.3, h * 0.45, h * 0.08, 0.4)
    draw(cv, ellipse(x, y - h * 0.42, h * 0.42, h * 0.45).smooth(capsule(x, y - h * 0.75, x, y - h * 1.0, h * 0.14, h * 0.1),
                                                                h * 0.12), color, shade=0.9, bevel=h * 0.3, rim=rim, tex=0.3)
    draw(cv, capsule(x - h * 0.16, y - h * 0.82, x + h * 0.16, y - h * 0.82, h * 0.035), '#4a3a2a')


def axe_head(cv, x, y, s, color='#6a7078', rim=0.8):
    sh = poly([(x - s * 0.5, y - s * 0.25), (x + s * 0.35, y - s * 0.4), (x + s * 0.55, y),
               (x + s * 0.35, y + s * 0.4), (x - s * 0.5, y + s * 0.25)])
    draw(cv, sh, color, shade=1.0, bevel=s * 0.25, rim=rim, edge='#d8e0e8', edge_w=0.3)
    draw(cv, circle(x - s * 0.25, y, s * 0.12), '#1a1a1e')


def chariot(cv, x, y, s, body='#c0392b', wheel='#e8a830', fire_col=None, facing=1, rim=0.6):
    """Carro de guerra de perfil (o de fuego, con `fire_col`)."""
    f = facing
    wr = s * 0.42
    wx = x
    draw(cv, poly([(x - f * s * 0.5, y - wr * 0.9), (x + f * s * 0.5, y - wr * 0.9), (x + f * s * 0.62, y - wr * 1.9),
                   (x - f * s * 0.35, y - wr * 1.65)]), body, shade=0.8, bevel=s * 0.15, rim=rim)
    draw(cv, polyline([(x + f * s * 0.5, y - wr * 1.2), (x + f * s * 1.4, y - wr * 1.05)], s * 0.04), '#5a3a1a')
    draw(cv, circle(wx, y - wr, wr), wheel, shade=0.6, bevel=wr * 0.2, rim=rim)
    draw(cv, circle(wx, y - wr, wr * 0.8), lerp_col(wheel, '#000000', 0.5))
    for k in range(6):
        a = k * math.pi / 3
        draw(cv, capsule(wx, y - wr, wx + math.cos(a) * wr * 0.8, y - wr + math.sin(a) * wr * 0.8, s * 0.035), wheel)
    draw(cv, circle(wx, y - wr, wr * 0.15), wheel, shade=0.8)
    if fire_col:
        fire(cv, x - f * s * 0.2, y - wr * 1.6, s * 0.7, glow=1.2)


# =============================================================================
# Animales (de perfil, `facing` 1 = mira a la derecha)
# =============================================================================
def raven(cv, x, y, s, facing=1, flying=False, bread_col=None, color='#1e1c24', rim=0.9, wing=0.0):
    f = facing
    body = ellipse(x, y, s * 0.55, s * 0.26)
    head = circle(x + f * s * 0.5, y - s * 0.12, s * 0.18)
    tail = poly([(x - f * s * 0.45, y - s * 0.05), (x - f * s * 1.0, y + s * 0.02), (x - f * s * 0.95, y + s * 0.18),
                 (x - f * s * 0.4, y + s * 0.15)])
    beak = poly([(x + f * s * 0.62, y - s * 0.18), (x + f * s * 0.98, y - s * 0.08), (x + f * s * 0.62, y - s * 0.03)])
    sh = body.smooth(head, s * 0.12) | tail
    if flying:
        up = s * (0.9 + wing)
        w1 = poly([(x - f * s * 0.2, y - s * 0.1), (x + f * s * 0.2, y - s * 0.12), (x - f * s * 0.1, y - up),
                   (x - f * s * 0.55, y - up * 0.85)])
        sh = sh | w1
        for k in range(4):
            px = x - f * s * (0.15 + k * 0.12)
            sh = sh | capsule(px, y - up * (0.7 + k * 0.05), px - f * s * 0.08, y - up * (1.0 + k * 0.03),
                              s * 0.05, s * 0.02)
    else:
        for sgn in (-1, 1):
            draw(cv, capsule(x + sgn * s * 0.1, y + s * 0.2, x + sgn * s * 0.12, y + s * 0.45, s * 0.03), '#3a3036')
    draw(cv, sh, color, shade=0.5, bevel=s * 0.15, rim=rim, rim_col='#c8d4ff', rim_w=0.8)
    draw(cv, beak, '#3a3236', rim=rim)
    draw(cv, circle(x + f * s * 0.56, y - s * 0.16, s * 0.035), '#ffffff', glow=0.3)
    if bread_col:
        draw(cv, ellipse(x + f * s * 1.0, y - s * 0.02, s * 0.2, s * 0.13), bread_col, shade=0.9, bevel=s * 0.1, rim=0.6)


def quadruped(cv, x, y, s, body='#8a6a4a', facing=1, kind='ox', rim=0.6, head_down=False, alpha=1.0,
              haze=0.0, gait=0.0, belly=None, mane=None):
    """Cuadrupedo de perfil con anatomia: pecho, vientre y grupa, patas de dos tramos con
    rodilla, cuello grueso y cabeza con hocico. `s` = alto a la cruz aproximado.
    kind: ox, donkey, horse, camel, bear, sheep. `gait` 0..1 abre el paso."""
    f = facing
    K = {
        #        largo  tronco  pata   cuello(len, ang)  cabeza  hocico
        'ox':     (1.15, 0.44, 0.40, (0.22, 0.25), 0.21, 0.16),
        'donkey': (0.95, 0.32, 0.50, (0.30, 0.95), 0.12, 0.11),
        'horse':  (1.05, 0.34, 0.58, (0.36, 1.00), 0.12, 0.12),
        'camel':  (1.05, 0.30, 0.78, (0.42, 0.55), 0.10, 0.10),
        'bear':   (1.00, 0.46, 0.34, (0.16, 0.30), 0.16, 0.12),
        'sheep':  (0.75, 0.34, 0.30, (0.14, 0.60), 0.11, 0.09),
    }[kind]
    L, th, leg, (nl, na), hr, sn = K
    L *= s
    th *= s
    leg *= s
    by = y - leg - th * 0.5
    dk = lerp_col(body, '#000000', 0.25)
    shadow(cv, x, y + 0.3, L * 0.62, s * 0.07, 0.4 * alpha)
    # patas de atras (lejanas, mas oscuras) y de delante
    def legs(far):
        out = None
        for k, (px, ph) in enumerate(((-0.36, 0.0), (0.34, 0.5))):
            phase = (ph + (0.25 if far else 0.0)) * math.pi * 2
            sw = math.sin(phase) * gait * 0.25
            hx = x + f * L * px + (f * L * 0.04 if far else 0.0)
            hip = (hx, by + th * 0.15)
            knee = (hx + f * leg * (0.12 + sw) * (1 if px > 0 else -0.6), y - leg * 0.48)
            foot = (hx + f * leg * (0.04 + sw * 1.6), y)
            up_r = s * (0.10 if kind in ('ox', 'bear') else 0.075)
            seg = capsule(hip[0], hip[1], knee[0], knee[1], up_r, up_r * 0.55).smooth(
                capsule(knee[0], knee[1], foot[0], foot[1], up_r * 0.5, up_r * 0.42), up_r * 0.3)
            out = seg if out is None else out | seg
        return out
    draw(cv, legs(True), dk, shade=0.6, bevel=s * 0.05, rim=rim * 0.6, alpha=alpha, haze=haze)
    # tronco: pecho + vientre + grupa
    chest = ellipse(x + f * L * 0.28, by, L * 0.26, th * 0.55)
    rump = ellipse(x - f * L * 0.28, by - th * 0.03, L * 0.25, th * 0.5)
    barrel = ellipse(x, by + th * 0.05, L * 0.36, th * 0.5)
    trunk = chest.smooth(rump, s * 0.12).smooth(barrel, s * 0.12)
    if kind == 'camel':
        trunk = trunk.smooth(ellipse(x - f * L * 0.02, by - th * 0.75, L * 0.2, th * 0.55), s * 0.1)
    if kind == 'bear':
        trunk = trunk.smooth(ellipse(x + f * L * 0.18, by - th * 0.35, L * 0.2, th * 0.35), s * 0.1)
    if kind == 'sheep':
        for k in range(7):
            trunk = trunk.smooth(circle(x + (k - 3) * L * 0.12, by - th * 0.25 * ((k % 2) - 0.3), th * 0.38), s * 0.06)
    # cuello y cabeza
    nx0, ny0 = x + f * L * 0.42, by - th * 0.15
    ang = na if not head_down else -0.6
    nx1 = nx0 + f * nl * s * math.cos(ang) * 1.1
    ny1 = ny0 - nl * s * math.sin(ang) * 1.1
    neck = capsule(nx0, ny0, nx1, ny1, th * 0.36, th * 0.24)
    if kind == 'camel':
        mid = (nx0 + f * nl * s * 0.7, ny0 + s * 0.05)
        neck = capsule(nx0, ny0, mid[0], mid[1], th * 0.3, th * 0.22).smooth(
            capsule(mid[0], mid[1], nx1 + f * s * 0.05, ny1 - s * 0.25, th * 0.22, th * 0.18), s * 0.05)
        nx1, ny1 = nx1 + f * s * 0.05, ny1 - s * 0.25
    head = ellipse(nx1 + f * hr * s * 0.45, ny1 + hr * s * 0.2, hr * s * 1.0, hr * s * 0.72)
    head = head.smooth(ellipse(nx1 + f * hr * s * 1.25, ny1 + hr * s * 0.55, sn * s * 0.75, sn * s * 0.55), s * 0.04)
    shape = trunk.smooth(neck, s * 0.07).smooth(head, s * 0.05)
    # cola
    tx = x - f * L * 0.5
    shape = shape | polyline(bezier((tx, by - th * 0.2), (tx - f * L * 0.1, by + th * 0.2),
                                    (tx - f * L * 0.06, by + th * (0.9 if kind in ('ox', 'horse', 'donkey') else 0.3)),
                                    n=6), s * 0.025, s * 0.015)
    # orejas / cuernos
    if kind in ('donkey', 'horse'):
        el = 0.22 if kind == 'donkey' else 0.12
        for k in (0.0, 0.15):
            shape = shape | capsule(nx1 - f * s * (0.02 + k * 0.4), ny1 - hr * s * 0.3, nx1 - f * s * (0.05 + k * 0.4),
                                    ny1 - hr * s * 0.3 - el * s, s * 0.03, s * 0.012)
    if kind == 'ox':
        shape = shape | polyline(bezier((nx1, ny1 - hr * s * 0.4), (nx1 + f * s * 0.08, ny1 - hr * s * 1.5),
                                        (nx1 + f * s * 0.2, ny1 - hr * s * 1.4), n=6), s * 0.03, s * 0.01)
    if kind in ('bear',):
        shape = shape | circle(nx1 - f * s * 0.02, ny1 - hr * s * 0.55, s * 0.05)
    draw(cv, shape, body, shade=0.8, bevel=s * 0.14, rim=rim, tex=0.22, tex_scale=1.0, alpha=alpha, haze=haze)
    if belly is not None:
        draw(cv, ellipse(x, by + th * 0.35, L * 0.32, th * 0.18), belly, alpha=0.5 * alpha, soft=1.0, haze=haze)
    if mane is not None or kind == 'horse':
        mc = mane if mane is not None else lerp_col(body, '#000000', 0.45)
        draw(cv, capsule(nx0 - f * s * 0.02, ny0 - th * 0.3, nx1 - f * s * 0.04, ny1 - hr * s * 0.5, s * 0.045, s * 0.03),
             mc, alpha=alpha, haze=haze, rim=rim * 0.5)
    draw(cv, legs(False), body, shade=0.7, bevel=s * 0.05, rim=rim, alpha=alpha, haze=haze)
    # ojo
    draw(cv, circle(nx1 + f * hr * s * 0.55, ny1 + hr * s * 0.0, s * 0.022), '#121010', alpha=alpha)


# =============================================================================
# Figuras humanas (sin cara, a contraluz: se leen por el manto y lo que llevan)
# =============================================================================
SKIN = '#c8905c'

POSES = {
    #          mano izq (dx, dy)    mano der (dx, dy) en fracciones del alto, desde el hombro
    'stand':  ((0.04, 0.30), (-0.04, 0.30)),
    'staff':  ((0.05, 0.30), (-0.14, 0.12)),
    'raise':  ((0.18, -0.18), (-0.18, -0.18)),
    'raise1': ((0.05, 0.30), (-0.16, -0.22)),
    'pray':   ((-0.04, 0.10), (0.04, 0.10)),
    'point':  ((0.05, 0.30), (-0.30, 0.02)),
    'reach':  ((0.22, 0.10), (-0.22, 0.10)),
    'carry':  ((0.12, 0.16), (-0.12, 0.16)),
    'mourn':  ((0.02, 0.0), (-0.02, 0.0)),
}


def figure(cv, x, y, h, robe='#a85a24', *, mantle=None, cloth=None, cloth_long=False, hood=None,
           skin=SKIN, pose='stand', staff=None, facing=1, hair=None, beard=None, crown=None, belt=None,
           kneel=False, sit=False, rim=1.0, rim_col=None, alpha=1.0, haze=0.0, helmet=None, plume=None,
           spear=False, shield=None, child=False, sash=None, hem=None, glow_tip=1.0, slim=False, backlit=0.0):
    """Figura de pie (o arrodillada / sentada) con los pies en (x, y) y alto `h`.

    facing: 1 = tres cuartos hacia la derecha, -1 hacia la izquierda, 0 = de frente.
    staff: None | 'fire' (Elias) | 'light' (Eliseo) | 'plain' | 'idol'
    """
    f = facing if facing != 0 else 1
    if backlit > 0:
        # a contraluz el cuerpo se apaga y el borde arde: es lo que da el aire de vitral
        dk = '#1a1214'
        k = 0.55 * backlit
        robe = lerp_col(robe, dk, k)
        mantle = lerp_col(mantle, dk, k) if mantle is not None else None
        cloth = lerp_col(cloth, dk, k) if cloth is not None else None
        hood = lerp_col(hood, dk, k) if hood is not None else None
        skin = lerp_col(skin, dk, k * 0.8)
        hair = lerp_col(hair, dk, k) if hair is not None else None
        beard = lerp_col(beard, dk, k) if beard is not None else None
        rim = rim * (1.0 + backlit)
    if kneel:
        hh = h * 0.70
    elif sit:
        hh = h * 0.66
    else:
        hh = h
    head_r = h * (0.072 if not child else 0.09)
    head_y = y - hh * 0.89
    sh_y = y - hh * 0.78
    sw = h * (0.13 if not slim else 0.115)
    hem_w = h * (0.22 if not kneel else 0.30) * (0.9 if slim else 1.0)
    lean = 0.04 * f * h if facing != 0 else 0.0
    if not (kneel or sit):
        shadow(cv, x, y + 0.3, h * 0.24, h * 0.04, 0.45 * alpha)
    else:
        shadow(cv, x, y + 0.3, h * 0.3, h * 0.05, 0.45 * alpha)

    # --- manto por detras
    if mantle is not None:
        mp = [(x - sw * 1.05 + lean, sh_y - h * 0.02), (x + sw * 1.05 + lean, sh_y - h * 0.02),
              (x + hem_w * 1.05, y - h * 0.05), (x - hem_w * 1.05, y - h * 0.05)]
        draw(cv, poly(mp), mantle, rim=rim * 0.8, rim_col=rim_col, alpha=alpha, haze=haze, tex=0.15,
             grad=('v', lerp_col(mantle, '#ffffff', 0.06), lerp_col(mantle, '#000000', 0.25)))
    # --- tunica: trapecio con el bajo curvo
    hw = hem_w
    hem_pts = [(x + hw * math.cos(t), y - h * 0.012 + abs(math.sin(t)) * h * 0.012) for t in np.linspace(0, math.pi, 9)]
    body = [(x - sw + lean, sh_y), (x + sw + lean, sh_y)] + hem_pts
    if kneel:
        body = [(x - sw + lean, sh_y), (x + sw + lean, sh_y), (x + hw, y - h * 0.02), (x + hw * 0.6, y),
                (x - hw, y), (x - hw * 1.05, y - h * 0.05)]
    torso = poly(body).smooth(ellipse(x + lean, sh_y + h * 0.02, sw * 1.05, h * 0.05), h * 0.03)
    draw(cv, torso, robe, shade=0.6, bevel=h * 0.08, rim=rim, rim_col=rim_col, alpha=alpha, haze=haze, tex=0.12,
         tex_scale=0.8, grad=('vy', sh_y, y, lerp_col(robe, '#ffffff', 0.06), lerp_col(robe, '#000000', 0.22)))
    # pliegues
    for k in (-1, 0, 1):
        px = x + k * hw * 0.45
        draw(cv, capsule(px + lean * 0.5, sh_y + h * 0.18, px * 1.0, y - h * 0.04, h * 0.006, h * 0.012),
             lerp_col(robe, '#000000', 0.3), alpha=0.35 * alpha, soft=0.4, haze=haze)
    if hem is not None:
        draw(cv, poly([(x - hw, y - h * 0.05), (x + hw, y - h * 0.05), (x + hw, y), (x - hw, y)]), hem, alpha=0.9 * alpha,
             haze=haze)
    if belt is not None:
        by = y - hh * 0.50
        draw(cv, rect(x - sw * 1.0 + lean * 0.6, by - h * 0.018, x + sw * 1.0 + lean * 0.6, by + h * 0.018, r=h * 0.01),
             belt, alpha=alpha, haze=haze, rim=rim * 0.5)
    if sash is not None:
        draw(cv, capsule(x - sw * 0.8 + lean, sh_y + h * 0.02, x + sw * 0.6, y - hh * 0.48, h * 0.02), sash,
             alpha=alpha, haze=haze)

    # --- brazos
    lh, rh = POSES.get(pose, POSES['stand'])
    shoulders = ((x + sw * 0.85 + lean, sh_y + h * 0.02), (x - sw * 0.85 + lean, sh_y + h * 0.02))
    hands = []
    for (sxp, syp), (dx, dy), side in zip(shoulders, (lh, rh), (1, -1)):
        dxx = dx * h * (f if facing != 0 else 1) if pose not in ('pray', 'mourn') else dx * h
        hx, hy = sxp + dxx, syp + dy * h
        if pose == 'mourn':
            hx, hy = x + lean * 1.5 + side * h * 0.025, head_y + h * 0.02
        elbow = ((sxp + hx) / 2 + side * h * 0.03, (syp + hy) / 2 + h * 0.03)
        arm = polyline([(sxp, syp), elbow, (hx, hy)], h * 0.042, h * 0.034)
        draw(cv, arm, lerp_col(robe, '#000000', 0.08), shade=0.5, bevel=h * 0.03, rim=rim, rim_col=rim_col,
             alpha=alpha, haze=haze)
        hands.append((hx, hy))
        draw(cv, circle(hx, hy, h * 0.028), skin, shade=0.6, bevel=h * 0.02, rim=rim * 0.6, alpha=alpha, haze=haze)

    # --- cayado / lanza
    if staff:
        hx, hy = hands[1]
        top = (hx + 0.0, head_y - h * 0.22)
        bot = (hx - f * h * 0.02, y)
        wood = '#5a3a1a' if staff != 'light' else '#7a5a3a'
        draw(cv, capsule(bot[0], bot[1], top[0], top[1], h * 0.014, h * 0.012), wood, rim=rim, alpha=alpha, haze=haze)
        if staff == 'fire':
            fire(cv, top[0], top[1] + h * 0.02, h * 0.07, glow=glow_tip)
        elif staff == 'light':
            glow_spot(cv, top[0], top[1], h * 0.35, '#bfe6ff', 0.7 * glow_tip)
            draw(cv, circle(top[0], top[1], h * 0.03), '#f4fbff', glow=1.2 * glow_tip)
        elif staff == 'idol':
            draw(cv, rect(top[0] - h * 0.03, top[1] - h * 0.03, top[0] + h * 0.03, top[1] + h * 0.03, r=h * 0.01),
                 '#7a1f1f', alpha=alpha)
    if spear:
        hx, hy = hands[1]
        draw(cv, capsule(hx, y, hx + f * h * 0.04, head_y - h * 0.35, h * 0.01), '#5a3a1a', alpha=alpha, haze=haze)
        draw(cv, poly([(hx + f * h * 0.04 - h * 0.02, head_y - h * 0.33), (hx + f * h * 0.045, head_y - h * 0.46),
                       (hx + f * h * 0.04 + h * 0.02, head_y - h * 0.33)]), '#c8ccd0', alpha=alpha, rim=rim, haze=haze)
    if shield is not None:
        hx, hy = hands[0]
        draw(cv, circle(hx, hy - h * 0.05, h * 0.11), shield, shade=0.8, bevel=h * 0.04, rim=rim, alpha=alpha,
             haze=haze, edge='#c9a24a', edge_w=h * 0.012)

    # --- cabeza
    hx0 = x + lean * 1.3
    draw(cv, capsule(hx0, head_y + head_r * 0.8, hx0, sh_y, h * 0.03), skin, alpha=alpha, haze=haze)
    if hair is not None and not (cloth is not None or hood is not None):
        draw(cv, circle(hx0 - f * head_r * 0.12, head_y - head_r * 0.12, head_r * 1.08), hair, alpha=alpha, haze=haze,
             rim=rim * 0.6)
    head = circle(hx0, head_y, head_r)
    if facing != 0:
        # perfil: nariz y menton hacia donde mira
        head = head.smooth(ellipse(hx0 + f * head_r * 0.92, head_y + head_r * 0.08, head_r * 0.22, head_r * 0.16),
                           head_r * 0.15)
    draw(cv, head, skin, shade=0.8, bevel=head_r * 0.6, rim=rim, rim_col=rim_col, alpha=alpha, haze=haze)
    if beard is not None:
        bd = ellipse(hx0 + f * head_r * 0.35, head_y + head_r * 0.62, head_r * 0.62, head_r * 0.62).smooth(
            ellipse(hx0 + f * head_r * 0.25, head_y + head_r * 1.05, head_r * 0.36, head_r * 0.5), head_r * 0.2)
        draw(cv, bd, beard, alpha=alpha, haze=haze, rim=rim * 0.6, rim_col=rim_col)
    if cloth is not None or hood is not None:
        c = cloth if cloth is not None else hood
        drop = h * (0.22 if cloth_long else 0.12)
        cap = circle(hx0 - f * head_r * 0.1, head_y - head_r * 0.15, head_r * 1.18)
        sides = poly([(hx0 - head_r * 1.15, head_y - head_r * 0.2), (hx0 + head_r * 1.15, head_y - head_r * 0.2),
                      (hx0 + head_r * 1.35, head_y + drop), (hx0 - head_r * 1.35, head_y + drop)])
        face = ellipse(hx0 + f * head_r * 0.25, head_y + head_r * 0.15, head_r * 0.62, head_r * 0.82)
        cl = cap.smooth(sides, head_r * 0.3).cut(face) if hood is None else cap.smooth(sides, head_r * 0.4).cut(face)
        draw(cv, cl, c, shade=0.6, bevel=head_r * 0.5, rim=rim, rim_col=rim_col, alpha=alpha, haze=haze, tex=0.1)
    if helmet is not None:
        draw(cv, ellipse(hx0, head_y - head_r * 0.35, head_r * 1.12, head_r * 0.85).cut(
            rect(hx0 - head_r * 2, head_y - head_r * 0.05, hx0 + head_r * 2, head_y + head_r * 2)), helmet, shade=0.8,
            bevel=head_r * 0.4, rim=rim, alpha=alpha, haze=haze)
        if plume is not None:
            draw(cv, ellipse(hx0 - f * head_r * 0.2, head_y - head_r * 1.15, head_r * 0.85, head_r * 0.35), plume,
                 alpha=alpha, haze=haze, rim=rim)
    if crown is not None:
        cy = head_y - head_r * 0.85
        pts = [(hx0 - head_r * 0.85, cy + head_r * 0.25)]
        for k in range(5):
            px = hx0 - head_r * 0.85 + head_r * 1.7 * (k + 0.5) / 5
            pts += [(px - head_r * 0.15, cy - head_r * 0.0), (px, cy - head_r * 0.45), (px + head_r * 0.15, cy - head_r * 0.0)]
        pts += [(hx0 + head_r * 0.85, cy + head_r * 0.25)]
        draw(cv, poly(pts), crown, shade=0.6, bevel=head_r * 0.2, rim=rim, alpha=alpha, haze=haze, glow=0.1)
    return hands


def elias(cv, x, y, h, **kw):
    kw.setdefault('staff', 'fire')
    return figure(cv, x, y, h, '#b8622a', mantle='#4a2c1a', hair='#2a1a12', beard='#2a1a12', belt='#5a3a1a', **kw)


def eliseo(cv, x, y, h, mantle='#e4ecef', **kw):
    kw.setdefault('staff', 'light')
    return figure(cv, x, y, h, '#3d6fb4', mantle=mantle, beard='#4a3628', belt='#3e9b5f', sash='#3e9b5f', **kw)


def lying(cv, x, y, L, color='#e8dcc0', skin=SKIN, rim=0.8, facing=1):
    """Alguien tendido (el nino, un muerto): capsula + cabeza."""
    f = facing
    shadow(cv, x, y + 0.3, L * 0.6, L * 0.07, 0.45)
    draw(cv, capsule(x - f * L * 0.42, y - L * 0.08, x + f * L * 0.28, y - L * 0.1, L * 0.11, L * 0.09), color,
         shade=0.8, bevel=L * 0.08, rim=rim, tex=0.1)
    draw(cv, circle(x + f * L * 0.42, y - L * 0.12, L * 0.09), skin, shade=0.8, bevel=L * 0.05, rim=rim)


def crowd(cv, x0, x1, y, h, colors, n=12, seed=0, haze=0.3, rim=0.6, alpha=1.0, jitter=0.12):
    """Gente a lo lejos: figuras pequenas sin detalle."""
    rng = np.random.default_rng(seed)
    xs = np.sort(rng.uniform(x0, x1, n))
    for i, xx in enumerate(xs):
        hh = h * rng.uniform(1 - jitter, 1 + jitter)
        c = colors[i % len(colors)]
        figure(cv, xx, y + rng.uniform(-0.5, 0.5), hh, c, cloth=lerp_col(c, '#e8dcc0', 0.5) if rng.random() < 0.5 else None,
               hair='#2a1a12', pose=rng.choice(['stand', 'stand', 'raise', 'pray']), facing=int(rng.choice([1, -1])),
               rim=rim, haze=haze, alpha=alpha)


# =============================================================================
# Interiores, mar, vinas y cosas de casa (2026-10-03, segunda tanda)
# =============================================================================
def interior(cv, wall='#7a5a44', floor='#5a4030', *, floor_y=72.0, beams=True, stones=True, seed=0,
             light='#ffb060', light_at=(100, 60), light_r=110.0, dark=0.55):
    """Aposento de piedra y adobe iluminado por un candil: pared, suelo, vigas y la penumbra."""
    reg = cv.region(0, 0, W, H)
    sl, X, Y = reg
    wall_c = col(wall)[:3]
    floor_c = col(floor)[:3]
    t = np.clip((Y - floor_y) / (H - floor_y), 0, 1)[..., None]
    base = np.where((Y < floor_y)[..., None], wall_c * (0.85 + 0.15 * (Y / floor_y))[..., None],
                    floor_c * (1.0 - 0.25 * t))
    n = fbm(X * 0.18, Y * 0.18, seed + 11, 4)
    base = base * (1.0 + (n[..., None] - 0.5) * 0.25)
    cv.rgb[sl] = base
    if stones:
        rng = np.random.default_rng(seed)
        rows = 9
        for r in range(rows):
            y0 = r * floor_y / rows
            off = rng.uniform(0, 10)
            x = -off
            while x < W:
                w = rng.uniform(9, 17)
                draw(cv, rect(x, y0, x + w, y0 + 0.35), lerp_col(wall, '#000000', 0.35), alpha=0.5)
                draw(cv, rect(x, y0, x + 0.35, y0 + floor_y / rows), lerp_col(wall, '#000000', 0.35), alpha=0.45)
                x += w
    draw(cv, rect(0, floor_y - 0.6, W, floor_y + 0.8), lerp_col(wall, '#000000', 0.5), alpha=0.8, soft=0.4)
    if beams:
        for k in range(5):
            bx = 10 + k * 45
            draw(cv, poly([(bx - 5, 0), (bx + 5, 0), (bx + 7, 6), (bx - 7, 6)]), '#3a2618', shade=0.6, rim=0.3)
    # penumbra: lo lejos del candil se hunde
    lx, ly = light_at
    d = np.hypot(X - lx, (Y - ly) * 1.3) / light_r
    shade_k = np.clip(d, 0, 1) ** 1.4 * dark
    cv.rgb[sl] = cv.rgb[sl] * (1 - shade_k[..., None])
    glow_spot(cv, lx, ly, light_r * 0.8, light, 0.28, power=1.8)


def window(cv, x, y, w, h, sky_top='#1a2244', sky_bot='#3a4a7a', frame='#3a2618', star=True, moon_=False,
           light=None):
    draw(cv, rect(x - w / 2 - 1.2, y - h / 2 - 1.2, x + w / 2 + 1.2, y + h / 2 + 1.2, r=1.5), frame, shade=0.6)
    reg = cv.region(x - w / 2, y - h / 2, x + w / 2, y + h / 2)
    sl, X, Y = reg
    t = np.clip((Y - (y - h / 2)) / h, 0, 1)[..., None]
    m = ((np.abs(X - x) <= w / 2) & (np.abs(Y - y) <= h / 2))[..., None]
    cv.rgb[sl] = np.where(m, col(sky_top)[:3] * (1 - t) + col(sky_bot)[:3] * t, cv.rgb[sl])
    if star:
        particles(cv, 8, (x - w / 2 + 1, y - h / 2 + 1, x + w / 2 - 1, y), '#ffffff', size=(0.1, 0.25), seed=3,
                  glow=0.8)
    if light:
        light_rays(cv, x, y - h / 2, light, n=5, spread=0.5, length=90, strength=0.12, angle=math.pi * 0.6,
                   width=0.08)
    draw(cv, rect(x - 0.5, y - h / 2, x + 0.5, y + h / 2), frame)
    draw(cv, rect(x - w / 2, y - 0.5, x + w / 2, y + 0.5), frame)


def oil_lamp(cv, x, y, s=3.0, glow=1.0):
    draw(cv, ellipse(x, y - s * 0.25, s * 0.9, s * 0.35), '#b8743a', shade=0.9, bevel=s * 0.3, rim=0.6)
    draw(cv, capsule(x + s * 0.6, y - s * 0.4, x + s * 1.0, y - s * 0.5, s * 0.12), '#b8743a')
    glow_spot(cv, x + s, y - s * 1.0, s * 18, '#ffb060', 0.55 * glow)
    draw(cv, capsule(x + s, y - s * 0.55, x + s, y - s * 1.35, s * 0.22, s * 0.04), '#ffd890', glow=1.4 * glow)
    draw(cv, capsule(x + s, y - s * 0.6, x + s, y - s * 1.0, s * 0.1, s * 0.03), '#ffffff', glow=1.0 * glow)


def bed(cv, x, y, w, h=5.0, wood='#5a3a24', cloth='#c8b898', rim=0.5, depth=None):
    """Lecho en tres cuartos: armazon, colchon con la manta caida por delante y almohada."""
    d = depth if depth is not None else w * 0.28
    shadow(cv, x, y + 0.5, w * 0.62, 1.8, 0.5)
    top_y = y - h * 1.2
    # armazon (frente) y patas
    draw(cv, rect(x - w / 2, y - h * 0.75, x + w / 2, y - h * 0.15, r=0.6), wood, shade=0.6, rim=rim, tex=0.2)
    for sx in (-1, 1):
        draw(cv, rect(x + sx * (w / 2 - 1.0) - 0.8, y - h * 0.3, x + sx * (w / 2 - 1.0) + 0.8, y + 0.3), wood,
             shade=0.5)
    # colchon: cara de arriba (paralelogramo) y frente con la manta
    top = poly([(x - w / 2 + 0.5, top_y), (x + w / 2 - 0.5, top_y), (x + w / 2 - 0.5 + d * 0.3, top_y - d * 0.35),
                (x - w / 2 + 0.5 + d * 0.3, top_y - d * 0.35)])
    draw(cv, top, lerp_col(cloth, '#ffffff', 0.12), shade=0.5, rim=rim, tex=0.1)
    front = rect(x - w / 2 + 0.3, top_y, x + w / 2 - 0.3, y - h * 0.55, r=0.8)
    draw(cv, front, cloth, shade=0.7, bevel=1.2, rim=rim, tex=0.15,
         grad=('v', lerp_col(cloth, '#ffffff', 0.05), lerp_col(cloth, '#000000', 0.25)))
    for k in range(4):
        fx = x - w / 2 + w * (k + 0.6) / 4.5
        draw(cv, capsule(fx, top_y + 0.6, fx + 0.6, y - h * 0.6, 0.18, 0.3), lerp_col(cloth, '#000000', 0.25),
             alpha=0.4, soft=0.3)
    draw(cv, ellipse(x - w / 2 + 4.5 + d * 0.15, top_y - d * 0.12, 3.8, 1.5), lerp_col(cloth, '#ffffff', 0.25),
         shade=0.9, bevel=1.0, rim=rim)
    # cabecero
    draw(cv, rect(x - w / 2 - 1.2, top_y - d * 0.5 - 4, x - w / 2 + 0.6, y - h * 0.2, r=0.6), wood, shade=0.6,
         rim=rim)
    return top_y - d * 0.15


def table(cv, x, y, w, h=7.0, wood='#6a4a2c', rim=0.5):
    shadow(cv, x, y + 0.4, w * 0.6, 1.2, 0.4)
    draw(cv, rect(x - w / 2, y - h, x + w / 2, y - h + 1.2, r=0.4), wood, shade=0.6, rim=rim)
    for sx in (-1, 1):
        draw(cv, rect(x + sx * (w / 2 - 1.2) - 0.5, y - h + 1, x + sx * (w / 2 - 1.2) + 0.5, y), wood, rim=rim)


def sea(cv, horizon, color_far='#9ab8c8', color_near='#2a5a7a', sparkle='#ffffff', seed=4, sun_x=None):
    reg = cv.region(0, horizon, W, H)
    sl, X, Y = reg
    t = np.clip((Y - horizon) / (H - horizon), 0, 1)[..., None]
    cv.rgb[sl] = col(color_far)[:3] * (1 - t) + col(color_near)[:3] * t
    rng = np.random.default_rng(seed)
    for i in range(160):
        yy = horizon + (H - horizon) * rng.uniform(0, 1) ** 1.6
        k = (yy - horizon) / (H - horizon)
        cx = rng.uniform(0, W) if sun_x is None or rng.random() < 0.5 else sun_x + rng.normal(0, 6 + 30 * k)
        ln = (0.5 + k * 4) * rng.uniform(0.5, 1.5)
        draw(cv, capsule(cx - ln, yy, cx + ln, yy, 0.08 + 0.2 * k), sparkle, alpha=rng.uniform(0.15, 0.55),
             glow=0.2 if sun_x is not None else 0.0)


def vines(cv, y0, y1, color='#4a6a2a', post='#5a3a1a', grapes='#5a2a5a', rows=5, seed=0):
    """Hileras de vina en perspectiva (la vina de Nabot)."""
    rng = np.random.default_rng(seed)
    for r in range(rows):
        t = r / (rows - 1)
        y = y0 + (y1 - y0) * t ** 1.3
        s = 0.35 + 0.65 * t
        x = -5 + rng.uniform(0, 8)
        while x < W + 5:
            draw(cv, capsule(x, y, x, y - 7 * s, 0.25 * s), post)
            bush(cv, x + 4 * s, y - 2.5 * s, 9 * s, 7 * s, color, seed=abs(int(x * 7 + r)), rim=0.5)
            if rng.random() < 0.6:
                for g in range(4):
                    draw(cv, circle(x + 4 * s + rng.uniform(-2, 2) * s, y - rng.uniform(1, 4) * s, 0.55 * s), grapes,
                         shade=0.8, bevel=0.4 * s)
            x += 11 * s


def tower(cv, x, y, w, h, color='#a88a6a', window_y=None, window_lit=False, rim=0.6, haze=0.0):
    shadow(cv, x, y + 0.5, w * 0.8, 1.5, 0.4)
    draw(cv, rect(x - w / 2, y - h, x + w / 2, y), color, rim=rim, haze=haze, tex=0.22, tex_scale=0.4,
         grad=('h', lerp_col(color, '#ffffff', 0.08), lerp_col(color, '#000000', 0.3)))
    for k in range(4):
        cx = x - w / 2 + w * (k + 0.5) / 4
        draw(cv, rect(cx - w * 0.08, y - h - 2.0, cx + w * 0.08, y - h + 0.2), color, rim=rim, haze=haze)
    if window_y is not None:
        wy = y - window_y
        draw(cv, rect(x - w * 0.15, wy - w * 0.25, x + w * 0.15, wy + w * 0.25, r=w * 0.12),
             '#ffcc70' if window_lit else '#1a1210', glow=0.8 if window_lit else 0.0)


def fire_chariot_2d(cv, x, y, s, facing=1, glow=1.0, seed=0):
    """Carro y caballos de fuego (2 R 2:11, 6:17): luz pura, sin madera ni metal."""
    f = facing
    for hk in range(2):
        hx = x + f * s * (1.3 + hk * 0.25)
        hy = y - s * 0.05 * hk
        quadruped(cv, hx, hy, s * 0.9, body='#ffb040', facing=f, kind='horse', rim=1.2)
        fire(cv, hx, hy - s * 0.55, s * 0.55, seed=seed + hk, glow=glow)
    chariot(cv, x, y, s, body='#ff8a2a', wheel='#ffd060', facing=f, fire_col='#ff8a2a', rim=1.2)
    glow_spot(cv, x + f * s, y - s * 0.6, s * 5, '#ff8a3a', 0.7 * glow)


def angel_light(cv, x, y, h):
    """Presencia del angel: columna de luz suave."""
    light_rays(cv, x, y - h * 1.6, '#fff4d8', n=7, spread=0.5, length=h * 2.5, strength=0.22,
               angle=math.pi / 2, width=0.05)
    glow_spot(cv, x, y - h * 0.5, h * 1.6, '#fff0c8', 0.6)


def bear(cv, x, y, s, facing=1, color='#4a3020', rim=0.8):
    quadruped(cv, x, y, s, body=color, facing=facing, kind='bear', rim=rim)


def camel(cv, x, y, s, facing=1, color='#c09060', rim=0.6, load=None):
    quadruped(cv, x, y, s, body=color, facing=facing, kind='camel', rim=rim)
    if load:
        draw(cv, ellipse(x - facing * s * 0.05, y - s * 1.25, s * 0.3, s * 0.18), load, shade=0.8, rim=rim)


def scroll_bundle(cv, x, y, s, color='#d8c8a0'):
    draw(cv, capsule(x - s, y, x + s, y, s * 0.3), color, shade=0.9, bevel=s * 0.2, rim=0.5)


def bones(cv, x, y, s, color='#e8e0cc'):
    for k in range(4):
        a = k * 0.8
        draw(cv, capsule(x + math.cos(a) * s, y + math.sin(a) * s * 0.3, x - math.cos(a) * s, y - math.sin(a) * s * 0.3,
                         s * 0.12), color, shade=0.8, rim=0.6)
    draw(cv, circle(x + s * 1.1, y - s * 0.2, s * 0.35), color, shade=0.9, bevel=s * 0.2, rim=0.6)


def sun_disk_eclipse(cv, x, y, r):
    """Cielo cerrado: sol velado tras bruma."""
    glow_spot(cv, x, y, r * 6, '#e8a060', 0.35)
    draw(cv, circle(x, y, r), '#f0c890', alpha=0.6, soft=0.8)


# =============================================================================
# Multitudes con los munecos del juego y hojas de sprites del juego
# =============================================================================
CROWD_PUEBLO = ['pueblo_1', 'pueblo_2', 'pueblo_3', 'pueblo_4', 'pueblo_5', 'pueblo_6', 'pueblo_7', 'pueblo_8']
CROWD_BAAL = ['baal_stand', 'baal_up', 'baal_up2']
CROWD_HIJOS = ['hijo_prof_stand_l', 'hijo_prof_stand_r', 'pueblo_2', 'pueblo_5']


def crowd_cut(cv, keys, x0, x1, y, h, n=10, seed=0, haze=0.2, backlit=0.0, rim=0.8, depth=4.0, tint=None):
    """Gente con figuras recortadas: de atras adelante, las de delante algo mas grandes."""
    from .core import cutout
    rng = np.random.default_rng(seed)
    items = []
    for i in range(n):
        yy = y + rng.uniform(0, depth)
        items.append((yy, rng.uniform(x0, x1), keys[rng.integers(len(keys))], bool(rng.integers(2))))
    for yy, xx, k, fl in sorted(items):
        t = (yy - y) / max(depth, 1e-6)
        cutout(cv, k, xx, yy, h * (0.92 + 0.16 * t), backlit=backlit, rim=rim, haze=haze * (1 - t * 0.5), flip=fl,
               shadow_k=0.3, tint=tint)


_SHEETS = {}


def sprite_cell(cv, path, box, x, y, h_units, *, flip=False, glow=0.0, glow_col='#ff9a3a', alpha=1.0, haze=0.0,
                anchor=(0.5, 1.0)):
    """Pega una celda de una hoja del juego (assets/sprites) con su `anchor` en (x, y)."""
    import os
    from PIL import Image
    from .core import _fig  # noqa: F401 (misma convencion de recorte)
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'assets', 'sprites'))
    if path not in _SHEETS:
        _SHEETS[path] = Image.open(os.path.join(root, path)).convert('RGBA')
    im = _SHEETS[path].crop(box)
    if flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    bb = im.getbbox()
    if bb is None:
        return
    im = im.crop(bb)
    k = h_units * cv.k / im.height
    w = max(2, int(im.width * k))
    hh = max(2, int(im.height * k))
    arr = np.asarray(im.convert('RGBa').resize((w, hh), Image.LANCZOS).convert('RGBA'), dtype=np.float32) / 255.0
    i0 = int(round(y * cv.k - hh * anchor[1]))
    j0 = int(round(x * cv.k - w * anchor[0]))
    ci0, cj0 = max(0, i0), max(0, j0)
    ci1, cj1 = min(cv.ph, i0 + hh), min(cv.pw, j0 + w)
    if ci1 <= ci0 or cj1 <= cj0:
        return
    sub = arr[ci0 - i0:ci1 - i0, cj0 - j0:cj1 - j0]
    rgb, a = sub[..., :3], sub[..., 3] * alpha
    if haze > 0 and cv.haze is not None:
        rgb = rgb * (1 - haze) + col(cv.haze)[:3] * haze
    cv.paint((slice(ci0, ci1), slice(cj0, cj1)), a, rgb)
    if glow > 0:
        cv.glow[ci0:ci1, cj0:cj1] += a[..., None] * glow * col(glow_col)[:3]


def fire_chariot(cv, x, y, h_units, flip=False, glow=0.8, seed=0):
    """El Carro de Fuego del juego (assets/sprites/chariot.png, fila este, capas back+front)."""
    sprite_cell(cv, 'chariot.png', (0, 0, 256, 256), x, y, h_units, flip=flip, glow=glow * 0.35)
    sprite_cell(cv, 'chariot.png', (1024, 0, 1280, 256), x, y, h_units, flip=flip, glow=glow * 0.35)
    glow_spot(cv, x, y - h_units * 0.45, h_units * 1.6, '#ff8a3a', 0.45 * glow)
    particles(cv, 10, (x - h_units, y - h_units * 1.2, x + h_units, y), '#ffc060', size=(0.15, 0.4), seed=seed,
              glow=1.0)


def trench(cv, pts, w, water='#d04a3a', glow_col='#ff6040', bank='#3a2418'):
    """Zanja (2 R 3:16-22): un surco curvo lleno de agua que refleja el cielo rojo."""
    s = polyline(pts, w * 0.6, w)
    draw(cv, s.grow(0.6), bank, alpha=0.7, soft=0.5)
    draw(cv, s, water, glow=0.35, glow_col=glow_col)
    draw(cv, polyline(pts, w * 0.15, w * 0.25), '#ffd0a0', alpha=0.4, glow=0.2)


def altar_stones(cv, x, y, w, h, stone='#9a8a74', wood='#6a4a2a', meat='#9a3a2a', trench_water=True, seed=0,
                 rim=0.7):
    """El altar del Carmelo (1 R 18:31-35): doce piedras toscas en tres hiladas, la lena, el
    buey en pedazos y la zanja llena de agua alrededor."""
    rng = np.random.default_rng(seed)
    if trench_water:
        draw(cv, ellipse(x, y + 1.6, w * 0.95, h * 0.30), '#2a1e18')
        draw(cv, ellipse(x, y + 1.3, w * 0.88, h * 0.22), '#5a9ab8', grad=('v', col('#d8f0f8'), col('#3d7c96')),
             glow=0.15, glow_col='#9fd8ff')
    shadow(cv, x, y + 0.3, w * 0.62, h * 0.12, 0.45)
    rows = [5, 4, 3]
    yy = y
    for r, n in enumerate(rows):
        rh = h / 3.0 * rng.uniform(0.9, 1.1)
        rw = w * (1.0 - r * 0.12)
        xs = np.cumsum(rng.uniform(0.8, 1.2, n))
        xs = xs / xs[-1] * rw
        x_prev = x - rw / 2
        for k in range(n):
            x_next = x - rw / 2 + xs[k]
            cx = (x_prev + x_next) / 2
            sw = (x_next - x_prev) / 2 * 0.98
            stone_col = lerp_col(stone, '#ffffff', rng.uniform(-0.12, 0.10))
            pts = []
            for a in np.linspace(0, 2 * math.pi, 9)[:-1]:
                rr = rng.uniform(0.85, 1.05)
                pts.append((cx + math.cos(a) * sw * rr, yy - rh / 2 + math.sin(a) * rh / 2 * rr))
            draw(cv, poly(pts).grow(-0.1), stone_col, shade=1.0, bevel=rh * 0.35, rim=rim, tex=0.3, tex_scale=0.8)
            x_prev = x_next
        yy -= rh * 0.92
    for k in range(6):
        ly = yy - 0.6 - k * 0.8
        x0 = x - w * 0.33 + rng.uniform(-1, 1)
        x1 = x + w * 0.33 + rng.uniform(-1, 1)
        draw(cv, capsule(x0, ly + rng.uniform(-0.3, 0.3), x1, ly, 0.5), wood, shade=0.7, bevel=0.5, rim=rim)
    for k in range(3):
        draw(cv, ellipse(x - w * 0.15 + k * w * 0.15, yy - 5.5, w * 0.08, 1.4), meat, shade=0.9, bevel=0.8, rim=rim)


def rock_cave(cv, x, y, w, h, rock='#5a4a50', dark='#060408', side=1, rim=0.7, seed=0, haze=0.0):
    """Pared de roca con la boca de la cueva (Horeb, el escondite de Abdias, el sepulcro)."""
    x_high = x + side * w * 1.6
    x_low = x - side * w * 1.4
    profile(cv, wall_fn(x_high, x_low, y, h * 1.5, seed=seed, rough=4, ledges=3, power=0.9), rock,
            grad_to=lerp_col(rock, '#000000', 0.5), rim=rim, tex=0.35, haze=haze)
    gw = w * 0.42
    gh = h * 0.70
    arch = (rect(x - gw, y - gh + gw * 0.9, x + gw, y + 0.5) | ellipse(x, y - gh + gw * 0.9, gw, gw * 0.95))
    draw(cv, arch.grow(1.2), lerp_col(rock, '#000000', 0.55), soft=0.6, haze=haze)
    draw(cv, arch, dark, grad=('v', col('#1a1418'), col(dark)), haze=haze * 0.5)
