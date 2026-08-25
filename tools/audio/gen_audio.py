"""Audio sintetizado de MANTLE: SFX + musica en bucle -> assets/audio/{sfx,music}/*.ogg

    python tools/audio/gen_audio.py            # todo
    python tools/audio/gen_audio.py sfx        # solo sfx
    python tools/audio/gen_audio.py music      # solo musica
    python tools/audio/gen_audio.py all wav    # dejar los .wav sin comprimir (depuracion)

Sintesis con numpy y nada mas. La SALIDA es .ogg (Vorbis) via ffmpeg: los mismos 40
sonidos ocupaban 48 MB en WAV, que no viajan ni a Web ni a movil. `Audio::load_*` prueba
.ogg y cae a .wav, asi que las dos formas funcionan; el .wav queda solo como depuracion.
Sin ffmpeg en el PATH se escriben .wav y se avisa.

Instrumentos (lo que separa esto de un banco de ondas): las cuerdas son Karplus-Strong
—una cuerda de verdad, no una pila de senos—, la melodia principal puede ir a `ney`
(flauta con aire y vibrato), cada voz tiene su sitio en el estereo, y todo pasa por un
compresor de bus. Ademas los eventos llevan microdesplazamiento y variacion de
intensidad: una rejilla perfecta es lo que delata a la musica generada.
"""
import math
import os
import shutil
import subprocess
import sys
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SFX_DIR = os.path.join(ROOT, 'assets', 'audio', 'sfx')
MUSIC_DIR = os.path.join(ROOT, 'assets', 'audio', 'music')
SR = 44100
MSR = 32000   # music sample rate (smaller files)

rng = np.random.default_rng(1234)


# =============================================================================
# Helpers
# =============================================================================
def t_axis(dur, sr=SR):
    return np.arange(int(dur * sr)) / sr


def env_adsr(n, sr, a=0.005, d=0.05, s=0.6, r=0.05):
    a_n, d_n, r_n = int(a * sr), int(d * sr), int(r * sr)
    s_n = max(0, n - a_n - d_n - r_n)
    e = np.concatenate([
        np.linspace(0, 1, a_n, endpoint=False),
        np.linspace(1, s, d_n, endpoint=False),
        np.full(s_n, s),
        np.linspace(s, 0, r_n),
    ])
    if len(e) < n:
        e = np.concatenate([e, np.zeros(n - len(e))])
    return e[:n]


def env_exp(n, sr, tau):
    return np.exp(-np.arange(n) / (tau * sr))


def sine(freq, dur, sr=SR):
    t = t_axis(dur, sr)
    if np.isscalar(freq):
        return np.sin(2 * np.pi * freq * t)
    phase = np.cumsum(freq) / sr
    return np.sin(2 * np.pi * phase)


def sweep(f0, f1, dur, sr=SR, curve=1.0):
    t = t_axis(dur, sr)
    k = (t / dur) ** curve
    return f0 * (f1 / f0) ** k


def noise(dur, sr=SR):
    return rng.uniform(-1, 1, int(dur * sr))


def lowpass(x, cutoff, sr=SR):
    """One-pole lowpass."""
    rc = 1.0 / (2 * np.pi * cutoff)
    dt = 1.0 / sr
    alpha = dt / (rc + dt)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc += alpha * (x[i] - acc)
        y[i] = acc
    return y


def lowpass_fast(x, cutoff, sr=SR):
    """Vectorised approximation: cascaded moving averages."""
    w = max(1, int(sr / cutoff / 2))
    k = np.ones(w) / w
    y = np.convolve(x, k, mode='same')
    return np.convolve(y, k, mode='same')


def highpass(x, cutoff, sr=SR):
    return x - lowpass_fast(x, cutoff, sr)


def place(buf, x, s):
    e = min(len(buf), s + len(x))
    if s < len(buf):
        buf[s:e] += x[:e - s]


def norm(x, peak=0.9):
    m = np.max(np.abs(x)) if len(x) else 1.0
    return x / m * peak if m > 1e-6 else x


def soft_clip(x, drive=1.25):
    """Gentle saturation: rounds transients off instead of clipping them square."""
    return np.tanh(x * drive) / np.tanh(drive)


def _comb(x, sr, delay_ms, g):
    """Feedback comb y[n] = x[n] + g*y[n-d], vectorised by folding the signal into
    blocks of d samples (each phase is an independent one-pole recursion)."""
    d = max(1, int(sr * delay_ms / 1000.0))
    pad = (-len(x)) % d
    y = np.concatenate([x, np.zeros(pad)]).reshape(-1, d)
    for i in range(1, y.shape[0]):
        y[i] += g * y[i - 1]
    return y.reshape(-1)[:len(x)]


# Dos juegos de retardos primos entre si: uno por canal. Una sala real no llega igual a
# los dos oidos, y con el MISMO juego en ambos la correlacion L/R se queda en 0,98 —o
# sea, mono con paneo pintado encima—. Cambiar solo la mezcla no basta: hay que cambiar
# los retardos.
ROOM_L = ((29.7, 0.78), (37.1, 0.74), (41.1, 0.70), (43.7, 0.66))
ROOM_R = ((31.3, 0.77), (35.9, 0.75), (44.3, 0.69), (47.1, 0.65))


def reverb(x, sr, mix=0.22, decay=0.62, damp=4200, room=ROOM_L):
    """Sala tipo Schroeder: cuatro peines con retardos primos entre si, amortiguados.
    Es el espacio que le faltaba al sintetizador seco (todo sonaba grabado en una caja)."""
    if len(x) < sr // 10:
        return x
    wet = np.zeros_like(x)
    for ms, g in room:
        wet += _comb(x, sr, ms, g * decay)
    wet = lowpass_fast(wet / 4.0, damp, sr)
    return (1.0 - mix) * x + mix * wet


def widen(x, sr, ms=14.0):
    """Mono -> stereo with a short opposite-phase delay. The delay wraps around so a
    looping track stays seamless."""
    d = max(1, int(sr * ms / 1000.0))
    if len(x) <= d:
        return np.stack([x, x], axis=1)
    delayed = np.concatenate([x[-d:], x[:-d]])
    return np.stack([x * 0.88 + delayed * 0.12, x * 0.88 - delayed * 0.12], axis=1)


FFMPEG = shutil.which('ffmpeg')
KEEP_WAV = False          # `python gen_audio.py all wav` lo enciende


def _write_wav(path, x, sr):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.clip(x, -1, 1)
    stereo = (x.ndim == 2)
    data = (x * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(2 if stereo else 1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(data.tobytes())
    return stereo


def save(path, x, sr=SR, quality=4):
    """Escribe el WAV y, si hay ffmpeg, lo transcodifica a .ogg y borra el WAV.

    `quality` es la escala -q:a de libvorbis (0..10). 4 para musica, 3 para SFX: por
    encima de eso el fichero crece sin que se note, y por debajo los transitorios de los
    golpes empiezan a sonar a agua."""
    wav_path = os.path.splitext(path)[0] + '.wav'
    ogg_path = os.path.splitext(path)[0] + '.ogg'
    stereo = _write_wav(wav_path, x, sr)
    tag = '(stereo)' if stereo else ''
    if FFMPEG and not KEEP_WAV:
        r = subprocess.run([FFMPEG, '-y', '-loglevel', 'error', '-i', wav_path,
                            '-c:a', 'libvorbis', '-q:a', str(quality), ogg_path],
                           capture_output=True, text=True)
        if r.returncode == 0 and os.path.exists(ogg_path):
            wav_kb = os.path.getsize(wav_path) / 1024.0
            ogg_kb = os.path.getsize(ogg_path) / 1024.0
            os.remove(wav_path)
            print('  ', os.path.relpath(ogg_path, ROOT), f'{len(x)/sr:.2f}s',
                  f'{ogg_kb:.0f} KB (de {wav_kb:.0f})', tag)
            return
        print('   ffmpeg fallo en', os.path.basename(path), r.stderr.strip()[:120])
    if not FFMPEG:
        print('   [sin ffmpeg: se deja .wav]', end=' ')
    print('  ', os.path.relpath(wav_path, ROOT), f'{len(x)/sr:.2f}s', tag)


def compress(x, sr, thresh=0.42, ratio=3.2, attack_ms=8.0, release_ms=140.0, makeup=None):
    """Compresor de bus: un detector sobre el valor absoluto, ataque rapido y relajacion
    lenta. Es lo que separa "sintetizador crudo" de "mezcla": sin el, el bombo se come el
    resto y el pad desaparece en cuanto entra la percusion. Suave a proposito (3:1) — mas
    ratio empieza a bombear de forma audible en un bucle corto, que es justo donde se nota.

    El detector va DIEZMADO en bloques de 64 muestras. No es un detalle de rendimiento:
    con un filtro por muestra, la envolvente de relajacion (140 ms = ventana de 14.000
    muestras) sale a np.convolve sobre millones de muestras y el generador se cuelga
    minutos por pista. Con bloques son ~10.000 iteraciones y el resultado es el mismo:
    una envolvente de bus no necesita resolucion de muestra.
    """
    mono = x if x.ndim == 1 else np.mean(x, axis=1)
    n = len(mono)
    H = 64
    nb = max(1, n // H)
    blocks = np.abs(mono[:nb * H]).reshape(nb, H).max(axis=1)

    a = math.exp(-H / (attack_ms * 1e-3 * sr))
    r = math.exp(-H / (release_ms * 1e-3 * sr))
    env = np.empty(nb)
    acc = 0.0
    for i in range(nb):
        v = blocks[i]
        c = a if v > acc else r
        acc = v + c * (acc - v)
        env[i] = acc

    g = np.ones(nb)
    over = env > thresh
    g[over] = (thresh + (env[over] - thresh) / ratio) / env[over]
    # Interpolacion lineal a resolucion de muestra: un escalon cada 64 muestras se oiria
    # como un zumbido de 500 Hz montado sobre la musica.
    gain = np.interp(np.arange(n), np.arange(nb) * H + H * 0.5, g,
                     left=g[0], right=g[-1])

    if makeup is None:
        makeup = min(1.0 / (thresh + (1.0 - thresh) / ratio), 1.6)
    if x.ndim == 1:
        return x * gain * makeup
    return x * gain[:, None] * makeup


def ks_pluck(freq, dur, sr=SR, tau=0.9, damp=0.55, bright=6.0, gain=1.0):
    """Cuerda pulsada Karplus-Strong: ruido en un anillo de retardo que se filtra a cada
    vuelta. Es UNA CUERDA, con su inarmonicidad y su decaimiento por parcial; la pila de
    senos de `note()` suena siempre a campana de sintetizador.

    Se calcula por FILAS de un periodo (no muestra a muestra): la recursion
    y[i] = d*(k*y[i-N] + (1-k)*y[i-N-1]) solo acopla filas consecutivas, asi que son
    dur*freq iteraciones de numpy en vez de dur*sr en Python.
    """
    N = max(2, int(round(sr / max(20.0, freq))))
    n = max(N, int(dur * sr))
    rows = n // N + 2
    # Excitacion: ruido filtrado. Cuanto mas bajo `bright`, mas "pulpejo del dedo" y
    # menos "pua"; sin filtrar, el ataque es un clic digital.
    exc = lowpass_fast(rng.uniform(-1, 1, N), max(200.0, freq * bright), sr)
    exc *= np.linspace(1.0, 0.55, N)          # el ataque pesa mas que la cola del ciclo
    d = math.exp(-1.0 / max(1e-3, tau * freq))   # `tau` = segundos hasta 1/e
    y = np.zeros((rows, N))
    y[0] = exc
    for k in range(1, rows):
        prev = y[k - 1]
        lag = np.empty(N)
        lag[0] = y[k - 2][-1] if k >= 2 else 0.0
        lag[1:] = prev[:-1]
        y[k] = d * (damp * prev + (1.0 - damp) * lag)
    out = y.reshape(-1)[:n]
    # Cuerpo del instrumento: un realce suave alrededor del primer formante
    out = out * 0.85 + lowpass_fast(out, freq * 3.0, sr) * 0.35
    return norm(out, 0.95) * gain


def ney(freq, dur, sr=SR, gain=1.0, breath=0.30, vib=4.8, vib_cents=22.0):
    """Flauta de caña (ney): seno con vibrato, aire soplado en la boquilla y un ataque
    lento. Es la voz solista del Levante y no habia ninguna: todas las melodias salian
    por la misma cuerda pulsada."""
    n = int(dur * sr)
    t = np.arange(n) / sr
    # Vibrato que ENTRA (un instrumentista no lo aplica desde la primera muestra)
    vib_env = np.clip((t - 0.12) / 0.35, 0.0, 1.0)
    f = freq * 2 ** (vib_cents / 1200.0 * np.sin(2 * np.pi * vib * t) * vib_env)
    phase = np.cumsum(f) / sr
    x = np.sin(2 * np.pi * phase)
    x += 0.22 * np.sin(4 * np.pi * phase)          # 2o armonico: da cuerpo
    x += 0.08 * np.sin(6 * np.pi * phase)
    # Aire: ruido en banda alrededor del tono, que es lo que hace que suene a soplo
    air = highpass(lowpass_fast(rng.uniform(-1, 1, n), freq * 4.0, sr), freq * 0.8, sr)
    x = x * (1.0 - breath) + air * breath * 2.2
    env = env_adsr(n, sr, a=min(0.09, dur * 0.3), d=0.12, s=0.82, r=min(0.22, dur * 0.4))
    return norm(x * env, 0.9) * gain


# =============================================================================
# Voz: modelo fuente-filtro (GDD 6.4, "exclamaciones cortas en gameplay")
# =============================================================================
# Una voz no es un tono con envolvente: son DOS cosas independientes.
#   * la FUENTE: el pulso glotal, que da el tono y el genero (unos 110 Hz en un hombre
#     adulto, 210 en una mujer) y que NO es un seno — es un pulso con muchisimos
#     armonicos, y por eso aqui se genera como diente de sierra suavizado.
#   * el FILTRO: el tracto vocal, que resuena en tres o cuatro FORMANTES fijos. Son ellos,
#     y no el tono, los que hacen que una "a" suene a "a". Cambiar de vocal es mover los
#     formantes, no la frecuencia.
# Fingir esto con senos y ruido es lo que da el "sintetizador de los 80" que hay que
# evitar: suena a instrumento, no a persona.

# Formantes (F1, F2, F3) en Hz. Valores de manual para voz masculina; para la femenina se
# escalan, que es una aproximacion honesta y suficiente para un gruñido de medio segundo.
FORMANTS = {
    'a': (730, 1090, 2440),
    'e': (530, 1840, 2480),
    'i': (270, 2290, 3010),
    'o': (570,  840, 2410),
    'u': (300,  870, 2240),
}


def _resonator(x, f0, bw, sr=SR):
    """Filtro resonante de dos polos (un formante). Se aplica muestra a muestra en un
    bucle de numpy sobre el eje del tiempo porque es recursivo; con 0,5 s de audio son
    22.000 iteraciones y tarda milisegundos."""
    r = math.exp(-math.pi * bw / sr)
    c = 2.0 * r * math.cos(2.0 * math.pi * f0 / sr)
    d = -r * r
    y = np.zeros_like(x)
    y1 = y2 = 0.0
    g = (1.0 - c - d)          # ganancia para que la resonancia no dispare el nivel
    for i in range(x.shape[0]):
        v = g * x[i] + c * y1 + d * y2
        y2 = y1
        y1 = v
        y[i] = v
    return y


def glottal(f0, dur, sr=SR, jitter=0.02, shimmer=0.06):
    """Pulso glotal: diente de sierra suavizado con JITTER (el tono real nunca es
    exacto) y SHIMMER (la amplitud tampoco). Sin esos dos, la voz sale plana y robotica —
    es el mismo motivo por el que Song.add humaniza la rejilla de la musica."""
    n = int(dur * sr)
    t = np.arange(n) / sr
    wobble = 1.0 + jitter * lowpass_fast(rng.uniform(-1, 1, n), 12.0, sr)
    phase = np.cumsum(f0 * wobble) / sr
    frac = phase - np.floor(phase)
    x = 2.0 * frac - 1.0                       # diente de sierra
    x = lowpass_fast(x, 3600.0, sr)            # el pulso real no tiene esquinas
    amp = 1.0 + shimmer * lowpass_fast(rng.uniform(-1, 1, n), 9.0, sr)
    return x * amp, t


def voice(vowel='a', f0=115.0, dur=0.45, sr=SR, gain=1.0, female=False,
          bend=-0.18, breath=0.10, effort=0.0):
    """Una exclamacion corta. `bend` es cuanto CAE el tono a lo largo del sonido (en
    semitonos por segundo, negativo = cae): un grito de esfuerzo siempre baja al final, y
    sin eso suena a nota tenida. `effort` (0..1) aprieta el aire y sube el 3er formante,
    que es lo que se oye como "apretar los dientes"."""
    n = int(dur * sr)
    t = np.arange(n) / sr
    f = f0 * 2 ** (bend * t)
    src, _ = glottal(1.0, dur, sr)              # placeholder, se rehace abajo con f(t)
    # Fuente con tono variable (hay que integrar f(t), no basta con multiplicar)
    wobble = 1.0 + 0.02 * lowpass_fast(rng.uniform(-1, 1, n), 12.0, sr)
    phase = np.cumsum(f * wobble) / sr
    frac = phase - np.floor(phase)
    src = lowpass_fast(2.0 * frac - 1.0, 3600.0, sr)
    src *= 1.0 + 0.06 * lowpass_fast(rng.uniform(-1, 1, n), 9.0, sr)
    # Aire: el susurro que acompana a cualquier voz, mas cuanto mas esfuerzo
    src += (breath + 0.25 * effort) * lowpass_fast(rng.uniform(-1, 1, n), 5000.0, sr)

    f1, f2, f3 = FORMANTS.get(vowel, FORMANTS['a'])
    k = 1.17 if female else 1.0                 # tracto mas corto = formantes mas altos
    f3 = f3 * (1.0 + 0.10 * effort)
    y = (1.00 * _resonator(src, f1 * k, 80.0, sr)
       + 0.55 * _resonator(src, f2 * k, 110.0, sr)
       + 0.28 * _resonator(src, f3 * k, 160.0, sr))
    # Envolvente de una exclamacion: ataque casi instantaneo y caida larga
    env = np.exp(-t / max(0.06, dur * 0.55))
    na = max(1, int(0.012 * sr))
    env[:na] *= np.linspace(0.0, 1.0, na)
    return norm(soft_clip(y * env, 1.1), 0.85) * gain


def note(freq, dur, sr=SR, harmonics=(1.0, 0.4, 0.2, 0.1), tau=0.25, a=0.004):
    """Plucked/bell-ish tone: harmonics with exponential decay + tiny attack."""
    t = t_axis(dur, sr)
    x = np.zeros_like(t)
    for i, amp in enumerate(harmonics):
        x += amp * np.sin(2 * np.pi * freq * (i + 1) * t) * np.exp(-t / (tau / (1 + i * 0.6)))
    n_a = max(1, int(a * sr))
    x[:n_a] *= np.linspace(0, 1, n_a)
    return x


# -----------------------------------------------------------------------------
# Exclamaciones de gameplay (GDD 6.4)
# -----------------------------------------------------------------------------
# El GDD pide dos cosas y solo UNA es compatible con la regla de cero assets de terceros:
#   * SI: exclamaciones cortas, sintetizadas aqui con el modelo fuente-filtro de `voice`.
#   * NO sin orden explicita: cinematicas dobladas — exige TTS de terceros, o sea
#     dependencia externa y licencia.
#
# Las dos voces se separan por TONO y por VOCAL, no por efectos: Elias es grave y cerrado
# (f0 ~106, vocal 'a'/'o'), Eliseo mas alto y abierto (f0 ~138, vocal 'e'/'a'). Un jugador
# distingue eso a la primera; distinguir dos reverbs, no.
#
# Cada exclamacion tiene VARIANTES por el mismo motivo que los disparos: mantener el dash
# o encajar golpes seguidos con una sola toma suena a bucle roto (ver play_sfx_variant).

def _voz(vowel, f0, dur, bend, effort, breath, gain=0.62):
    """Exclamacion lista para el juego: la voz cruda mas un poco de sala. La ganancia
    baja (0,62) es deliberada — una voz al mismo nivel que un impacto tapa el combate."""
    v = voice(vowel, f0=f0, dur=dur, bend=bend, effort=effort, breath=breath)
    return norm(reverb(v, SR, mix=0.14, decay=0.45), 0.8) * gain


# Esfuerzo (dash): corto, seco y con el tono cayendo deprisa.
def voz_elias_esfuerzo(i=0):
    return _voz('a', 106 + i * 5, 0.26 + i * 0.02, -1.0 - i * 0.15, 0.55, 0.16)


def voz_eliseo_esfuerzo(i=0):
    return _voz('e', 138 + i * 6, 0.24 + i * 0.02, -0.9 - i * 0.15, 0.48, 0.14)


# Dolor (golpe recibido): mas largo, mas aire y mas caida.
def voz_elias_dolor(i=0):
    return _voz('o', 100 + i * 4, 0.40 + i * 0.03, -1.5 - i * 0.2, 0.75, 0.24)


def voz_eliseo_dolor(i=0):
    return _voz('a', 132 + i * 5, 0.38 + i * 0.03, -1.4 - i * 0.2, 0.68, 0.22)


# Al lanzar un milagro grande: no es un grito de dolor sino uno sostenido, casi cantado.
def voz_milagro(i=0):
    return _voz('a', 118 + i * 9, 0.62, -0.25, 0.35, 0.10, gain=0.58)


# Jefe: grave, largo y con mucho esfuerzo. Suena una vez por capitulo.
def voz_jefe():
    return _voz('a', 84, 1.00, -0.45, 0.92, 0.30, gain=0.72)


# =============================================================================
# SFX
# =============================================================================
def sfx_fire_shot():
    d = 0.16
    n = int(d * SR)
    x = 0.55 * lowpass_fast(noise(d), 1800) * env_exp(n, SR, 0.05)
    x += 0.5 * sine(sweep(520, 160, d), d) * env_exp(n, SR, 0.06)
    return norm(x, 0.7)


def sfx_light_shot():
    d = 0.16
    n = int(d * SR)
    x = 0.6 * sine(sweep(900, 1500, d), d) * env_exp(n, SR, 0.05)
    x += 0.3 * sine(sweep(1800, 3000, d), d) * env_exp(n, SR, 0.04)
    return norm(x, 0.6)


def sfx_enemy_hit():
    d = 0.09
    n = int(d * SR)
    x = 0.8 * sine(sweep(220, 90, d), d) * env_exp(n, SR, 0.03)
    x += 0.5 * lowpass_fast(noise(d), 3000) * env_exp(n, SR, 0.012)
    return norm(x, 0.8)


def sfx_enemy_death():
    d = 0.35
    n = int(d * SR)
    t = t_axis(d)
    saw = 2 * ((sweep(380, 70, d) * t) % 1.0) - 1
    x = 0.6 * lowpass_fast(saw, 1200) * env_exp(n, SR, 0.12)
    x += 0.5 * lowpass_fast(noise(d), 900) * env_exp(n, SR, 0.08)
    return norm(x, 0.85)


def sfx_player_hurt():
    d = 0.2
    n = int(d * SR)
    t = t_axis(d)
    sq = np.sign(np.sin(2 * np.pi * sweep(160, 70, d) * t))
    x = 0.5 * lowpass_fast(sq, 900) * env_exp(n, SR, 0.07)
    x += 0.4 * lowpass_fast(noise(d), 1500) * env_exp(n, SR, 0.04)
    return norm(x, 0.85)


def sfx_dash():
    d = 0.22
    n = int(d * SR)
    x = highpass(lowpass_fast(noise(d), 4000), 300) * env_adsr(n, SR, 0.02, 0.05, 0.5, 0.12)
    return norm(x, 0.5)


def sfx_pickup():
    x = np.concatenate([note(880, 0.09, tau=0.08), note(1318, 0.18, tau=0.15)])
    return norm(x, 0.6)


def sfx_faith_gain():
    seq = [659, 784, 988, 1319]
    x = np.zeros(int(0.5 * SR))
    for i, f in enumerate(seq):
        s = int(i * 0.08 * SR)
        n_ = note(f, 0.3, tau=0.18)
        place(x, n_, s)
    return norm(x, 0.6)


def sfx_faith_loss():
    x = np.concatenate([note(330, 0.12, tau=0.1, harmonics=(1, 0.6, 0.3)), note(247, 0.25, tau=0.2, harmonics=(1, 0.6, 0.3))])
    return norm(x, 0.55)


def sfx_miracle_fire():
    d = 0.7
    n = int(d * SR)
    x = 0.8 * lowpass_fast(noise(d), 2500) * env_adsr(n, SR, 0.03, 0.15, 0.4, 0.4)
    x += 0.9 * sine(sweep(160, 40, d), d) * env_exp(n, SR, 0.25)
    x += 0.3 * sine(sweep(900, 200, d), d) * env_exp(n, SR, 0.15)
    return norm(x, 0.9)


def sfx_miracle_light():
    x = np.zeros(int(0.8 * SR))
    for i, f in enumerate([523, 659, 784, 1047, 1319]):
        s = int(i * 0.06 * SR)
        n_ = note(f, 0.7, tau=0.35, harmonics=(1, 0.5, 0.25, 0.1, 0.05))
        place(x, n_, s)
    x += 0.2 * lowpass_fast(noise(0.8), 3000) * env_adsr(len(x), SR, 0.1, 0.2, 0.3, 0.4)
    return norm(x, 0.7)


def sfx_heal():
    x = np.zeros(int(0.5 * SR))
    for i, f in enumerate([523, 659, 784]):
        s = int(i * 0.1 * SR)
        n_ = note(f, 0.4, tau=0.25, harmonics=(1, 0.3, 0.1))
        place(x, n_, s)
    return norm(x, 0.55)


def sfx_ultimate():
    d = 1.2
    n = int(d * SR)
    x = 1.0 * sine(sweep(80, 30, d), d) * env_exp(n, SR, 0.5)
    x += 0.7 * lowpass_fast(noise(d), 1800) * env_adsr(n, SR, 0.02, 0.3, 0.5, 0.6)
    for i, f in enumerate([220, 277, 330, 440]):
        s = int(i * 0.12 * SR)
        n_ = note(f, 0.9, tau=0.4)
        place(x, 0.5 * n_, s)
    return norm(x, 0.95)


def sfx_menu_select():
    return norm(note(1200, 0.06, tau=0.04, harmonics=(1, 0.2)), 0.4)


def sfx_menu_confirm():
    return norm(np.concatenate([note(900, 0.07, tau=0.05), note(1350, 0.14, tau=0.1)]), 0.5)


def sfx_menu_back():
    return norm(np.concatenate([note(900, 0.07, tau=0.05), note(600, 0.14, tau=0.1)]), 0.5)


def sfx_notification():
    x = note(1568, 0.5, tau=0.3, harmonics=(1, 0.4, 0.15, 0.05))
    place(x, note(2093, 0.35, tau=0.25, harmonics=(1, 0.3, 0.1)), int(0.15 * SR))
    return norm(x, 0.5)


def sfx_door_open():
    # room cleared / exits unlocked: rising two-note chime + soft rumble
    x = np.zeros(int(0.7 * SR))
    for i, f in enumerate([392, 523, 659]):
        s = int(i * 0.09 * SR)
        n_ = note(f, 0.55, tau=0.3, harmonics=(1, 0.5, 0.2))
        place(x, n_, s)
    x += 0.25 * lowpass_fast(noise(0.7), 300) * env_adsr(len(x), SR, 0.05, 0.2, 0.3, 0.3)
    return norm(x, 0.6)


def sfx_level_up():
    # chapter complete fanfare
    seq = [(523, 0.0), (659, 0.12), (784, 0.24), (1047, 0.36), (784, 0.6), (1047, 0.72)]
    x = np.zeros(int(1.4 * SR))
    for f, st in seq:
        s = int(st * SR)
        n_ = note(f, 0.6, tau=0.3, harmonics=(1, 0.5, 0.3, 0.15))
        n_ += 0.5 * note(f / 2, 0.6, tau=0.3, harmonics=(1, 0.3))
        place(x, n_, s)
    return norm(x, 0.7)


def sfx_critical():
    d = 0.25
    n = int(d * SR)
    x = 0.9 * sine(sweep(140, 45, d), d) * env_exp(n, SR, 0.08)
    x += 0.6 * lowpass_fast(noise(d), 2000) * env_exp(n, SR, 0.03)
    return norm(x, 0.9)


def sfx_footsteps():
    d = 0.08
    n = int(d * SR)
    return norm(lowpass_fast(noise(d), 800) * env_exp(n, SR, 0.02), 0.3)


def sfx_fire_crackle():
    d = 1.0
    x = lowpass_fast(noise(d), 1500) * (0.4 + 0.6 * lowpass_fast(np.abs(noise(d)), 12))
    return norm(x, 0.35)


def sfx_wind():
    d = 3.0
    x = lowpass_fast(noise(d), 500) * (0.4 + 0.6 * np.abs(lowpass_fast(noise(d), 1.5)) * 3)
    return norm(x, 0.35)


def sfx_water():
    d = 2.0
    x = lowpass_fast(noise(d), 3000) * (0.6 + 0.4 * np.abs(lowpass_fast(noise(d), 4)) * 3)
    return norm(x, 0.3)


def variant(fn, semitones=0.0, gain=1.0):
    """Otra toma del mismo sonido: la fuente ya trae ruido distinto en cada llamada, y
    aqui ademas se le cambia el tono remuestreando.

    Es lo que evita el efecto metralleta: manteniendo K se disparan tres o cuatro tiros
    por segundo, y con UN solo fichero el oido oye una ametralladora en vez de un profeta
    lanzando fuego. El juego rota entre las variantes (Audio::play_sfx_variant).
    """
    def gen():
        x = fn()
        if abs(semitones) > 1e-6:
            ratio = 2 ** (semitones / 12.0)
            n = int(len(x) / ratio)
            idx = np.arange(n) * ratio
            i0 = np.floor(idx).astype(int)
            i1 = np.minimum(i0 + 1, len(x) - 1)
            frac = idx - i0
            x = x[i0] * (1 - frac) + x[i1] * frac
        return norm(x, 0.9) * gain
    return gen


SFX = {
    'cayado_swing_1': lambda: norm(highpass(lowpass_fast(noise(0.14), 3000), 400) * env_exp(int(0.14 * SR), SR, 0.05), 0.4),
    'cayado_swing_2': lambda: norm(highpass(lowpass_fast(noise(0.12), 3500), 500) * env_exp(int(0.12 * SR), SR, 0.045), 0.4),
    'cayado_swing_3': lambda: norm(highpass(lowpass_fast(noise(0.16), 2800), 350) * env_exp(int(0.16 * SR), SR, 0.055), 0.4),
    'cayado_hit_enemy': sfx_enemy_hit,
    'proyectil_fuego': sfx_fire_shot,
    'proyectil_fuego_2': variant(sfx_fire_shot, -1.5),
    'proyectil_fuego_3': variant(sfx_fire_shot, 2.0, 0.94),
    'proyectil_luz': sfx_light_shot,
    'proyectil_luz_2': variant(sfx_light_shot, 1.5),
    'proyectil_luz_3': variant(sfx_light_shot, -2.0, 0.94),
    'dash_woosh': sfx_dash,
    'enemy_hit': sfx_enemy_hit,
    'enemy_hit_2': variant(sfx_enemy_hit, -2.0),
    'enemy_hit_3': variant(sfx_enemy_hit, 2.5, 0.95),
    'enemy_death': sfx_enemy_death,
    'enemy_death_2': variant(sfx_enemy_death, -1.5),
    'critical_hit': sfx_critical,
    'miracle_cast_fire': sfx_miracle_fire,
    'miracle_cast_light': sfx_miracle_light,
    'miracle_ravens': sfx_heal,
    'miracle_heal': sfx_heal,
    'miracle_ultimate': sfx_ultimate,
    'miracle_cooldown_ready': lambda: norm(note(1046, 0.25, tau=0.15, harmonics=(1, 0.3)), 0.35),
    'menu_select': sfx_menu_select,
    'menu_confirm': sfx_menu_confirm,
    'menu_back': sfx_menu_back,
    'item_pickup': sfx_pickup,
    'level_up': sfx_level_up,
    'faith_gain': sfx_faith_gain,
    'faith_loss': sfx_player_hurt,
    'notification': sfx_notification,
    'footsteps_sand': sfx_footsteps,
    'footsteps_stone': sfx_footsteps,
    'fire_crackle': sfx_fire_crackle,
    'wind_desert': sfx_wind,
    'water_flow': sfx_water,
    'door_open': sfx_door_open,

    # --- Voz (GDD 6.4). Las variantes van CONSECUTIVAS, como el resto ---
    'voz_elias_esfuerzo_1':  lambda: voz_elias_esfuerzo(0),
    'voz_elias_esfuerzo_2':  lambda: voz_elias_esfuerzo(1),
    'voz_elias_esfuerzo_3':  lambda: voz_elias_esfuerzo(2),
    'voz_elias_dolor_1':     lambda: voz_elias_dolor(0),
    'voz_elias_dolor_2':     lambda: voz_elias_dolor(1),
    'voz_elias_dolor_3':     lambda: voz_elias_dolor(2),
    'voz_eliseo_esfuerzo_1': lambda: voz_eliseo_esfuerzo(0),
    'voz_eliseo_esfuerzo_2': lambda: voz_eliseo_esfuerzo(1),
    'voz_eliseo_esfuerzo_3': lambda: voz_eliseo_esfuerzo(2),
    'voz_eliseo_dolor_1':    lambda: voz_eliseo_dolor(0),
    'voz_eliseo_dolor_2':    lambda: voz_eliseo_dolor(1),
    'voz_eliseo_dolor_3':    lambda: voz_eliseo_dolor(2),
    'voz_milagro_1':         lambda: voz_milagro(0),
    'voz_milagro_2':         lambda: voz_milagro(1),
    'voz_jefe':              voz_jefe,
}


# =============================================================================
# Music: tiny sequencer
# =============================================================================
A4 = 440.0


def midi(n):
    return A4 * 2 ** ((n - 69) / 12.0)


# Scales (semitone offsets). Phrygian dominant = "oriental" flavour for the Levant setting.
PHRYGIAN_DOM = [0, 1, 4, 5, 7, 8, 10]
DORIAN = [0, 2, 3, 5, 7, 9, 10]
AEOLIAN = [0, 2, 3, 5, 7, 8, 10]
MAJOR = [0, 2, 4, 5, 7, 9, 11]


def deg(root, scale, degree, octave=0):
    """Scale degree (0-based, can exceed 7) -> midi note."""
    o, d = divmod(degree, len(scale))
    return root + 12 * (o + octave) + scale[d]


class Song:
    """Secuenciador minimo con DOS buses (izquierdo y derecho).

    El estereo ya no se finge al final con `widen`: cada voz se coloca donde le toca al
    anadirla (`pan` -1..1, potencia constante). Un bombo centrado y una percusion abierta
    a los lados es la mitad de la sensacion de "esto esta producido".
    """

    def __init__(self, bpm, bars, sr=MSR, beats_per_bar=4, humanize=0.006, seed=7):
        self.sr = sr
        self.bpm = bpm
        self.beat = 60.0 / bpm
        self.bars = bars
        self.length = bars * beats_per_bar * self.beat
        n = int(self.length * sr) + sr                    # + cola (se pliega al cerrar el bucle)
        self.buf_l = np.zeros(n)
        self.buf_r = np.zeros(n)
        self.bpb = beats_per_bar
        # Microdesplazamiento (segundos) e intensidad variable. Una rejilla exacta es lo
        # que delata a la musica generada: ningun percusionista cae dos veces en la misma
        # muestra. 0 lo apaga (util para depurar).
        self.humanize = humanize
        self.hrng = np.random.default_rng(seed)

    def add(self, x, at_beat, gain=1.0, pan=0.0, tight=False):
        """`pan`: -1 izquierda, +1 derecha. `tight` salta la humanizacion (pedales y
        notas tenidas que tienen que empezar exactamente en el compas)."""
        t = at_beat * self.beat
        if self.humanize > 0.0 and not tight:
            t += float(self.hrng.normal(0.0, self.humanize))
            gain *= 1.0 + float(self.hrng.normal(0.0, 0.09))
        s = max(0, int(t * self.sr))
        e = min(len(self.buf_l), s + len(x))
        if s >= len(self.buf_l) or e <= s:
            return
        p = max(-1.0, min(1.0, pan))
        ang = (p + 1.0) * 0.25 * math.pi               # potencia constante
        gl, gr = math.cos(ang) * gain, math.sin(ang) * gain
        seg = x[:e - s]
        self.buf_l[s:e] += gl * seg
        self.buf_r[s:e] += gr * seg

    def _finish(self, buf, mix, room=None):
        # Reverb primero (sobre el buffer CON su cola) y luego la cola se pliega al
        # principio: la reverberacion cruza el punto de bucle en vez de cortarse en seco.
        x = reverb(buf, self.sr, mix=mix, room=room or ROOM_L)
        n = int(self.length * self.sr)
        y = x[:n].copy()
        tail = x[n:]
        y[:len(tail)] += tail
        return y

    def render(self):
        # Los dos canales llevan reverb con mezcla ligeramente distinta: una sala real no
        # es simetrica, y de paso descorrelaciona las colas (mas ancho, sin fase rara).
        l = self._finish(self.buf_l, 0.24, ROOM_L)
        r = self._finish(self.buf_r, 0.27, ROOM_R)
        y = np.stack([l, r], axis=1)
        y = compress(norm(y, 0.75), self.sr)
        return soft_clip(norm(y, 0.86), 1.1)

    # --- voices ---------------------------------------------------------------
    def pluck(self, m, dur_beats, tau=0.35, gain=0.5, harmonics=None):
        """Cuerda pulsada. `harmonics` se acepta por compatibilidad y se ignora: la voz
        ya no es aditiva sino Karplus-Strong (ver ks_pluck)."""
        return ks_pluck(midi(m), dur_beats * self.beat, self.sr,
                        tau=max(0.25, tau * 2.2), gain=gain)

    def lead(self, m, dur_beats, gain=0.4, breath=0.28):
        """Voz solista de aire (ney)."""
        return ney(midi(m), dur_beats * self.beat, self.sr, gain=gain, breath=breath)

    def pad(self, ms, dur_beats, gain=0.18):
        d = dur_beats * self.beat
        t = t_axis(d, self.sr)
        x = np.zeros_like(t)
        for m in ms:
            f = midi(m)
            for det in (-0.4, 0.0, 0.4):
                ff = f * 2 ** (det / 100.0)
                x += (2 * ((ff * t) % 1.0) - 1) * 0.5      # saw
                x += np.sin(2 * np.pi * ff * t) * 0.5
        x = lowpass_fast(x, 900, self.sr)
        x *= env_adsr(len(x), self.sr, 0.25 * self.beat, 0.3, 0.8, 0.5 * self.beat)
        return x * gain

    def bass(self, m, dur_beats, gain=0.45):
        d = dur_beats * self.beat
        t = t_axis(d, self.sr)
        f = midi(m)
        x = np.sign(np.sin(2 * np.pi * f * t)) * 0.5 + np.sin(2 * np.pi * f * t) * 0.6
        x = lowpass_fast(x, 400, self.sr)
        x *= env_adsr(len(x), self.sr, 0.005, 0.1, 0.7, 0.08)
        return x * gain

    def kick(self, gain=0.9):
        d = 0.25
        n = int(d * self.sr)
        x = np.sin(2 * np.pi * np.cumsum(sweep(150, 45, d, self.sr)) / self.sr) * env_exp(n, self.sr, 0.09)
        return x * gain

    def snare(self, gain=0.45):
        d = 0.18
        n = int(d * self.sr)
        x = lowpass_fast(rng.uniform(-1, 1, n), 4000, self.sr) * env_exp(n, self.sr, 0.05)
        x += 0.5 * np.sin(2 * np.pi * 190 * t_axis(d, self.sr)) * env_exp(n, self.sr, 0.04)
        return x * gain

    def hat(self, gain=0.18, dur=0.05):
        n = int(dur * self.sr)
        x = highpass(rng.uniform(-1, 1, n), 6000, self.sr) * env_exp(n, self.sr, 0.015)
        return x * gain

    def tom(self, f, gain=0.6):
        d = 0.3
        n = int(d * self.sr)
        x = np.sin(2 * np.pi * np.cumsum(sweep(f * 1.6, f, d, self.sr)) / self.sr) * env_exp(n, self.sr, 0.12)
        return x * gain

    def shaker(self, gain=0.12):
        n = int(0.08 * self.sr)
        return lowpass_fast(highpass(rng.uniform(-1, 1, n), 4000, self.sr), 9000, self.sr) * env_exp(n, self.sr, 0.03) * gain


def melody_from_pattern(song, root, scale, pattern, start_beat, step=0.5, octave=1,
                        gain=0.45, tau=0.3, pan=0.0, voice='pluck'):
    """pattern: grados de la escala o None (silencio); `step` en pulsos.

    `voice`: 'pluck' (cuerda) o 'ney' (flauta de cana). El paneo separa una segunda voz
    de la principal: dos melodias en el mismo punto del estereo se emborronan."""
    for i, dgr in enumerate(pattern):
        if dgr is None:
            continue
        m = deg(root, scale, dgr, octave)
        x = (song.lead(m, step * 2.0, gain=gain) if voice == 'ney'
             else song.pluck(m, step * 1.8, tau=tau, gain=gain))
        song.add(x, start_beat + i * step, pan=pan)


def music_main_theme():
    # 84 BPM, E phrygian dominant, contemplative: pad + sparse plucked melody + frame drum
    s = Song(84, 16)
    root = 52  # E3
    chords = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]]   # degrees
    for bar in range(16):
        ch = chords[bar % 4]
        ms = [deg(root, PHRYGIAN_DOM, d, 0) for d in ch]
        s.add(s.pad(ms, 4, gain=0.16), bar * 4, pan=-0.25, tight=True)
        s.add(s.pad(ms, 4, gain=0.13), bar * 4 + 0.02, pan=0.30, tight=True)
        s.add(s.bass(deg(root, PHRYGIAN_DOM, ch[0], -1), 3.5, gain=0.35), bar * 4, tight=True)
        # frame drum: soft kick 1 & 3, shaker 8ths
        s.add(s.kick(0.45), bar * 4)
        s.add(s.kick(0.35), bar * 4 + 2.5)
        for e in range(8):
            s.add(s.shaker(0.09), bar * 4 + e * 0.5, pan=0.42 if e % 2 else -0.38)
    pat_a = [0, None, 1, 2, None, 4, None, 3, 2, None, 1, None, 0, None, None, None]
    pat_b = [4, None, 5, 6, None, 7, None, 6, 5, None, 4, None, 2, None, 1, None]
    pat_c = [7, None, 6, 5, None, 4, None, 3, 4, None, 2, None, 1, None, 0, None]
    # El tema principal lo canta el ney; la cuerda le dobla una octava abajo y muy bajita
    for i, pat in enumerate([pat_a, pat_b, pat_a, pat_c] * 2):
        melody_from_pattern(s, root, PHRYGIAN_DOM, pat, i * 8, step=0.5, octave=1,
                            gain=0.34, voice='ney', pan=-0.12)
        melody_from_pattern(s, root, PHRYGIAN_DOM, pat, i * 8, step=0.5, octave=0,
                            gain=0.16, tau=0.5, pan=0.22)
    return s.render()


def music_exploration(root=52, scale=PHRYGIAN_DOM, bpm=104, seed=3):
    s = Song(bpm, 16)
    r = np.random.default_rng(seed)
    prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]
    for bar in range(16):
        ch = prog[bar % 4]
        ms = [deg(root, scale, d, 0) for d in ch]
        s.add(s.pad(ms, 4, gain=0.12), bar * 4)
        for b in range(4):
            s.add(s.bass(deg(root, scale, ch[0], -1), 0.9, gain=0.4), bar * 4 + b)
            s.add(s.bass(deg(root, scale, ch[0], -1) + 12, 0.4, gain=0.18), bar * 4 + b + 0.5)
        s.add(s.kick(0.7), bar * 4)
        s.add(s.kick(0.6), bar * 4 + 2)
        s.add(s.snare(0.32), bar * 4 + 1, pan=-0.12)
        s.add(s.snare(0.32), bar * 4 + 3, pan=-0.12)
        for e in range(8):
            s.add(s.hat(0.12 if e % 2 else 0.18), bar * 4 + e * 0.5, pan=0.38)
        # arpeggio 16ths on odd bars
        if bar % 2 == 1:
            arp = [ch[0], ch[1], ch[2], ch[1] + 7, ch[2], ch[1], ch[0], ch[2]]
            for i, d in enumerate(arp):
                s.add(s.pluck(deg(root, scale, d, 1), 0.5, tau=0.18, gain=0.22),
                      bar * 4 + i * 0.5, pan=-0.45 + 0.9 * (i / 7.0))
    # lead phrases every 4 bars
    for ph in range(4):
        pat = [r.choice([0, 1, 2, 3, 4, 5, 6, None, None]) for _ in range(16)]
        pat[0] = ph % 3
        melody_from_pattern(s, root, scale, pat, ph * 16 + 4, step=0.5, octave=1,
                            gain=0.30, voice='ney', pan=-0.15)
    return s.render()


def music_combat(root=52, scale=PHRYGIAN_DOM, bpm=136, seed=5):
    s = Song(bpm, 16)
    r = np.random.default_rng(seed)
    prog = [[0, 2, 4], [0, 2, 4], [1, 3, 5], [4, 6, 1]]
    for bar in range(16):
        ch = prog[bar % 4]
        base = deg(root, scale, ch[0], -1)
        # driving 8th bass with octave jumps
        for e in range(8):
            m = base + (12 if e in (3, 7) else 0)
            s.add(s.bass(m, 0.45, gain=0.5), bar * 4 + e * 0.5)
        s.add(s.pad([deg(root, scale, d, 0) for d in ch], 4, gain=0.1), bar * 4)
        s.add(s.kick(0.9), bar * 4)
        s.add(s.kick(0.9), bar * 4 + 1)
        s.add(s.kick(0.9), bar * 4 + 2)
        s.add(s.kick(0.9), bar * 4 + 3)
        s.add(s.snare(0.4), bar * 4 + 1)
        s.add(s.snare(0.4), bar * 4 + 3)
        for e in range(16):
            s.add(s.hat(0.1 if e % 2 else 0.16, 0.04), bar * 4 + e * 0.25,
                  pan=0.30 if e % 2 else 0.45)
        if bar % 4 == 3:
            s.add(s.tom(150), bar * 4 + 3.0, pan=-0.5)
            s.add(s.tom(110), bar * 4 + 3.5, pan=0.5)
    for ph in range(8):
        pat = [r.choice([0, 2, 4, 5, 6, 7, None]) for _ in range(8)]
        pat[0] = [0, 4, 6, 7][ph % 4]
        melody_from_pattern(s, root, scale, pat, ph * 8 + 2, step=0.5, octave=1,
                            gain=0.34, tau=0.2, pan=-0.2)
        if ph % 2:
            melody_from_pattern(s, root, scale, [d + 2 if d is not None else None for d in pat],
                                ph * 8 + 2, step=0.5, octave=1, gain=0.18, tau=0.2, pan=0.34)
    return s.render()


def music_boss():
    s = Song(150, 16)
    root = 50  # D
    scale = PHRYGIAN_DOM
    for bar in range(16):
        base = deg(root, scale, 0, -1)
        alt = deg(root, scale, 1, -1)
        for e in range(8):
            m = base if e not in (2, 6) else alt
            s.add(s.bass(m, 0.4, gain=0.55), bar * 4 + e * 0.5)
        s.add(s.pad([deg(root, scale, d, 0) for d in ([0, 1, 4] if bar % 2 == 0 else [0, 3, 6])], 4, gain=0.14), bar * 4)
        for b in range(4):
            s.add(s.kick(1.0), bar * 4 + b)
            s.add(s.kick(0.6), bar * 4 + b + 0.75)
        s.add(s.snare(0.5), bar * 4 + 1)
        s.add(s.snare(0.5), bar * 4 + 3)
        s.add(s.snare(0.3), bar * 4 + 3.5)
        for e in range(16):
            s.add(s.hat(0.12, 0.035), bar * 4 + e * 0.25, pan=0.40 if e % 2 else -0.34)
        if bar % 2 == 1:
            s.add(s.tom(170, 0.7), bar * 4 + 2.5, pan=-0.55)
            s.add(s.tom(120, 0.7), bar * 4 + 3.0, pan=0.0)
            s.add(s.tom(90, 0.8), bar * 4 + 3.5, pan=0.55)
    riff = [0, None, 1, 0, 6, None, 4, None, 0, None, 1, 0, 3, 4, 3, 1]
    for ph in range(8):
        melody_from_pattern(s, root, scale, riff if ph % 2 == 0 else [d + 4 if d is not None else None for d in riff],
                            ph * 8, step=0.5, octave=1, gain=0.4, tau=0.15)
    return s.render()


def music_menu():
    return music_main_theme()


def music_victory():
    s = Song(100, 8)
    root = 60
    prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]
    for bar in range(8):
        ch = prog[bar % 4]
        s.add(s.pad([deg(root, MAJOR, d, 0) for d in ch], 4, gain=0.18), bar * 4, pan=-0.24, tight=True)
        s.add(s.pad([deg(root, MAJOR, d, 0) for d in ch], 4, gain=0.14), bar * 4 + 0.02, pan=0.30, tight=True)
        s.add(s.bass(deg(root, MAJOR, ch[0], -2), 3.5, gain=0.35), bar * 4, tight=True)
        s.add(s.kick(0.5), bar * 4)
        s.add(s.snare(0.25), bar * 4 + 2, pan=-0.14)
    fanfare = [0, 2, 4, 7, None, 4, 7, 9, None, 7, 9, 11, 14, None, None, None]
    # La fanfarria la lleva el ney y la cuerda la responde al otro lado del estereo
    melody_from_pattern(s, root, MAJOR, fanfare, 0, step=0.5, octave=0, gain=0.42,
                        voice='ney', pan=-0.14)
    melody_from_pattern(s, root, MAJOR, fanfare, 16, step=0.5, octave=1, gain=0.36,
                        tau=0.5, pan=0.26)
    return s.render()


def music_death():
    s = Song(60, 8)
    root = 45
    for bar in range(8):
        s.add(s.pad([root, root + 3, root + 7] if bar % 2 == 0 else [root - 2, root + 1, root + 5], 4, gain=0.2), bar * 4)
        s.add(s.bass(root - 12, 3.8, gain=0.3), bar * 4)
        s.add(s.kick(0.4), bar * 4)
    melody_from_pattern(s, root + 12, AEOLIAN, [4, None, None, 3, None, None, 2, None, None, None, 0, None, None, None, None, None], 0, step=0.5, octave=0, gain=0.4, tau=0.9)
    return s.render()


def music_ascension():
    s = Song(76, 8)
    root = 57
    for bar in range(8):
        ms = [root + 12 * (bar // 4), root + 4 + 12 * (bar // 4), root + 7 + 12 * (bar // 4)]
        s.add(s.pad(ms, 4, gain=0.22), bar * 4)
        s.add(s.bass(root - 12, 3.8, gain=0.25), bar * 4)
        for e in range(8):
            s.add(s.pluck(deg(root, MAJOR, (e * 2) % 7, 1 + (bar // 4)), 1.0, tau=0.5, gain=0.2), bar * 4 + e * 0.5)
    return s.render()


def music_prologue():
    """El Cielo Cerrado: la cinematica de apertura (~85 s a 60 BPM, 22 compases).

    Larga a proposito para cubrir las ocho estampas sin que se note el bucle, y sombria:
    pedal grave constante, pads menores que descienden y un pluck solitario que solo
    aparece cuando entra Elias (compas 12 en adelante).
    """
    s = Song(60, 22)
    root = 45
    for bar in range(22):
        # Pedal: la tierra bajo el reino partido, siempre la misma nota
        s.add(s.bass(root - 12, 3.9, gain=0.26), bar * 4, tight=True)
        # Los pads bajan de tono segun se cierra el cielo, y suben al final (Querit)
        if bar < 6:
            ch = [root, root + 3, root + 7]
        elif bar < 11:
            ch = [root - 1, root + 2, root + 6]        # entra Baal: se oscurece
        elif bar < 17:
            ch = [root + 5, root + 8, root + 12]       # el monte: se abre
        else:
            ch = [root + 3, root + 7, root + 10]
        s.add(s.pad(ch, 4, gain=0.20 + 0.03 * (bar % 3)), bar * 4, pan=-0.2, tight=True)
        s.add(s.pad(ch, 4, gain=0.15 + 0.02 * (bar % 3)), bar * 4 + 0.03, pan=0.28, tight=True)
        if bar in (5, 10, 16):
            s.add(s.tom(0.9, gain=0.30), bar * 4)      # los tres giros del relato
        if bar >= 12:
            for e in range(4):
                s.add(s.pluck(deg(root + 12, AEOLIAN, (e * 3 + bar) % 7, 1), 1.2,
                              tau=0.7, gain=0.16), bar * 4 + e, pan=-0.3 + 0.2 * e)
    melody_from_pattern(s, root + 12, AEOLIAN,
                        [0, None, None, None, 4, None, 3, None, None, None, 2, None,
                         None, None, 0, None],
                        48, step=1.0, octave=0, gain=0.26, voice='ney', pan=-0.1)
    return s.render()


MUSIC = {
    'main_theme': music_main_theme,
    'prologue': music_prologue,
    'menu': music_menu,
    'exploration': lambda: music_exploration(52, PHRYGIAN_DOM, 104, 3),
    'elias_combat': lambda: music_combat(52, PHRYGIAN_DOM, 136, 5),
    'eliseo_combat': lambda: music_combat(55, DORIAN, 128, 9),
    'boss_theme': music_boss,
    'victory': music_victory,
    'death': music_death,
    'ascension': music_ascension,
}


# Big, "important" one-shots get a touch of the same room as the music; short combat
# blips stay dry so they keep their snap.
SFX_REVERB = {
    'miracle_cast_fire': 0.30, 'miracle_cast_light': 0.32, 'miracle_ravens': 0.28,
    'miracle_heal': 0.28, 'miracle_ultimate': 0.38, 'miracle_cooldown_ready': 0.22,
    'level_up': 0.32, 'faith_gain': 0.26, 'critical_hit': 0.18, 'door_open': 0.24,
    'notification': 0.20, 'enemy_death': 0.14,
}


def main(what='all'):
    # `what` puede ser 'all', 'sfx', 'music' o un PREFIJO de nombre de SFX
    # (p. ej. `python tools/audio/gen_audio.py voz_`), para poder regenerar solo un grupo
    # sin reescribir los 47 ficheros que ya estan bien. El rng esta sembrado, asi que
    # regenerarlo todo daria lo mismo bit a bit, pero tarda minutos y no hace falta.
    prefix = None if what in ('all', 'sfx', 'music') else what
    if prefix:
        print('SFX (prefijo %r):' % prefix)
        for name, fn in SFX.items():
            if not name.startswith(prefix):
                continue
            x = fn()
            mix = SFX_REVERB.get(name)
            if mix:
                x = norm(reverb(np.concatenate([x, np.zeros(int(0.45 * SR))]), SR, mix=mix, decay=0.5), 0.9)
            save(os.path.join(SFX_DIR, name + '.ogg'), soft_clip(x, 1.1), SR, quality=3)
        return
    if what in ('all', 'sfx'):
        print('SFX:')
        for name, fn in SFX.items():
            x = fn()
            mix = SFX_REVERB.get(name)
            if mix:
                # Room tail needs somewhere to ring out
                x = reverb(np.concatenate([x, np.zeros(int(0.45 * SR))]), SR, mix=mix, decay=0.5)
                x = norm(x, 0.9)
            save(os.path.join(SFX_DIR, name + '.ogg'), soft_clip(x, 1.1), SR, quality=3)
    if what in ('all', 'music'):
        print('MUSIC:')
        for name, fn in MUSIC.items():
            save(os.path.join(MUSIC_DIR, name + '.ogg'), fn(), MSR, quality=4)


if __name__ == '__main__':
    if 'wav' in sys.argv[1:]:
        globals()["KEEP_WAV"] = True
    args = [a for a in sys.argv[1:] if a != 'wav']
    main(args[0] if args else 'all')
