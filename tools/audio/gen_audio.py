"""Procedural placeholder audio for MANTLE: SFX + looping music -> assets/audio/{sfx,music}/*.wav

    python tools/audio/gen_audio.py            # everything
    python tools/audio/gen_audio.py sfx        # only sfx
    python tools/audio/gen_audio.py music      # only music

Pure numpy synthesis (no external deps). Files are 16-bit PCM WAV; SDL3_mixer loads them
directly (Audio::load_* falls back from .ogg to .wav).
"""
import math
import os
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


def reverb(x, sr, mix=0.22, decay=0.62, damp=4200):
    """Schroeder-ish room: four combs with coprime delays, damped. Adds the space the
    dry synth was missing (everything used to sound like it was recorded in a box)."""
    if len(x) < sr // 10:
        return x
    wet = np.zeros_like(x)
    for ms, g in ((29.7, 0.78), (37.1, 0.74), (41.1, 0.70), (43.7, 0.66)):
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


def save(path, x, sr=SR):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.clip(x, -1, 1)
    stereo = (x.ndim == 2)
    data = (x * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(2 if stereo else 1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(data.tobytes())
    print('  ', os.path.relpath(path, ROOT), f'{len(x)/sr:.2f}s', '(stereo)' if stereo else '')


def note(freq, dur, sr=SR, harmonics=(1.0, 0.4, 0.2, 0.1), tau=0.25, a=0.004):
    """Plucked/bell-ish tone: harmonics with exponential decay + tiny attack."""
    t = t_axis(dur, sr)
    x = np.zeros_like(t)
    for i, amp in enumerate(harmonics):
        x += amp * np.sin(2 * np.pi * freq * (i + 1) * t) * np.exp(-t / (tau / (1 + i * 0.6)))
    n_a = max(1, int(a * sr))
    x[:n_a] *= np.linspace(0, 1, n_a)
    return x


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


SFX = {
    'cayado_swing_1': lambda: norm(highpass(lowpass_fast(noise(0.14), 3000), 400) * env_exp(int(0.14 * SR), SR, 0.05), 0.4),
    'cayado_swing_2': lambda: norm(highpass(lowpass_fast(noise(0.12), 3500), 500) * env_exp(int(0.12 * SR), SR, 0.045), 0.4),
    'cayado_swing_3': lambda: norm(highpass(lowpass_fast(noise(0.16), 2800), 350) * env_exp(int(0.16 * SR), SR, 0.055), 0.4),
    'cayado_hit_enemy': sfx_enemy_hit,
    'proyectil_fuego': sfx_fire_shot,
    'proyectil_luz': sfx_light_shot,
    'dash_woosh': sfx_dash,
    'enemy_hit': sfx_enemy_hit,
    'enemy_death': sfx_enemy_death,
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
    def __init__(self, bpm, bars, sr=MSR, beats_per_bar=4):
        self.sr = sr
        self.bpm = bpm
        self.beat = 60.0 / bpm
        self.bars = bars
        self.length = bars * beats_per_bar * self.beat
        self.buf = np.zeros(int(self.length * sr) + sr)   # + tail (trimmed to loop later)
        self.bpb = beats_per_bar

    def add(self, x, at_beat, gain=1.0):
        s = int(at_beat * self.beat * self.sr)
        e = min(len(self.buf), s + len(x))
        if s < len(self.buf):
            self.buf[s:e] += gain * x[:e - s]

    def render(self):
        # Reverb first (on the buffer *including* its tail), then wrap that tail into the
        # start: the room decay crosses the loop point instead of being cut off.
        x = reverb(self.buf, self.sr, mix=0.24)
        n = int(self.length * self.sr)
        y = x[:n].copy()
        tail = x[n:]
        y[:len(tail)] += tail
        return widen(soft_clip(norm(y, 0.82)), self.sr)

    # --- voices ---------------------------------------------------------------
    def pluck(self, m, dur_beats, tau=0.35, gain=0.5, harmonics=(1, 0.5, 0.25, 0.12)):
        return note(midi(m), dur_beats * self.beat, self.sr, harmonics=harmonics, tau=tau) * gain

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


def melody_from_pattern(song, root, scale, pattern, start_beat, step=0.5, octave=1, gain=0.45, tau=0.3):
    """pattern: list of scale degrees or None (rest); step in beats."""
    for i, dgr in enumerate(pattern):
        if dgr is None:
            continue
        m = deg(root, scale, dgr, octave)
        song.add(song.pluck(m, step * 1.8, tau=tau, gain=gain), start_beat + i * step)


def music_main_theme():
    # 84 BPM, E phrygian dominant, contemplative: pad + sparse plucked melody + frame drum
    s = Song(84, 16)
    root = 52  # E3
    chords = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]]   # degrees
    for bar in range(16):
        ch = chords[bar % 4]
        ms = [deg(root, PHRYGIAN_DOM, d, 0) for d in ch]
        s.add(s.pad(ms, 4, gain=0.16), bar * 4)
        s.add(s.bass(deg(root, PHRYGIAN_DOM, ch[0], -1), 3.5, gain=0.35), bar * 4)
        # frame drum: soft kick 1 & 3, shaker 8ths
        s.add(s.kick(0.45), bar * 4)
        s.add(s.kick(0.35), bar * 4 + 2.5)
        for e in range(8):
            s.add(s.shaker(0.09), bar * 4 + e * 0.5)
    pat_a = [0, None, 1, 2, None, 4, None, 3, 2, None, 1, None, 0, None, None, None]
    pat_b = [4, None, 5, 6, None, 7, None, 6, 5, None, 4, None, 2, None, 1, None]
    pat_c = [7, None, 6, 5, None, 4, None, 3, 4, None, 2, None, 1, None, 0, None]
    for i, pat in enumerate([pat_a, pat_b, pat_a, pat_c] * 2):
        melody_from_pattern(s, root, PHRYGIAN_DOM, pat, i * 8, step=0.5, octave=1, gain=0.42, tau=0.45)
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
        s.add(s.snare(0.32), bar * 4 + 1)
        s.add(s.snare(0.32), bar * 4 + 3)
        for e in range(8):
            s.add(s.hat(0.12 if e % 2 else 0.18), bar * 4 + e * 0.5)
        # arpeggio 16ths on odd bars
        if bar % 2 == 1:
            arp = [ch[0], ch[1], ch[2], ch[1] + 7, ch[2], ch[1], ch[0], ch[2]]
            for i, d in enumerate(arp):
                s.add(s.pluck(deg(root, scale, d, 1), 0.5, tau=0.18, gain=0.22), bar * 4 + i * 0.5)
    # lead phrases every 4 bars
    for ph in range(4):
        pat = [r.choice([0, 1, 2, 3, 4, 5, 6, None, None]) for _ in range(16)]
        pat[0] = ph % 3
        melody_from_pattern(s, root, scale, pat, ph * 16 + 4, step=0.5, octave=1, gain=0.36, tau=0.3)
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
            s.add(s.hat(0.1 if e % 2 else 0.16, 0.04), bar * 4 + e * 0.25)
        if bar % 4 == 3:
            s.add(s.tom(150), bar * 4 + 3.0)
            s.add(s.tom(110), bar * 4 + 3.5)
    for ph in range(8):
        pat = [r.choice([0, 2, 4, 5, 6, 7, None]) for _ in range(8)]
        pat[0] = [0, 4, 6, 7][ph % 4]
        melody_from_pattern(s, root, scale, pat, ph * 8 + 2, step=0.5, octave=1, gain=0.34, tau=0.2)
        if ph % 2:
            melody_from_pattern(s, root, scale, [d + 2 if d is not None else None for d in pat], ph * 8 + 2, step=0.5, octave=1, gain=0.18, tau=0.2)
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
            s.add(s.hat(0.12, 0.035), bar * 4 + e * 0.25)
        if bar % 2 == 1:
            s.add(s.tom(170, 0.7), bar * 4 + 2.5)
            s.add(s.tom(120, 0.7), bar * 4 + 3.0)
            s.add(s.tom(90, 0.8), bar * 4 + 3.5)
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
        s.add(s.pad([deg(root, MAJOR, d, 0) for d in ch], 4, gain=0.18), bar * 4)
        s.add(s.bass(deg(root, MAJOR, ch[0], -2), 3.5, gain=0.35), bar * 4)
        s.add(s.kick(0.5), bar * 4)
        s.add(s.snare(0.25), bar * 4 + 2)
    fanfare = [0, 2, 4, 7, None, 4, 7, 9, None, 7, 9, 11, 14, None, None, None]
    melody_from_pattern(s, root, MAJOR, fanfare, 0, step=0.5, octave=0, gain=0.5, tau=0.5)
    melody_from_pattern(s, root, MAJOR, fanfare, 16, step=0.5, octave=1, gain=0.4, tau=0.5)
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


MUSIC = {
    'main_theme': music_main_theme,
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
    if what in ('all', 'sfx'):
        print('SFX:')
        for name, fn in SFX.items():
            x = fn()
            mix = SFX_REVERB.get(name)
            if mix:
                # Room tail needs somewhere to ring out
                x = reverb(np.concatenate([x, np.zeros(int(0.45 * SR))]), SR, mix=mix, decay=0.5)
                x = norm(x, 0.9)
            save(os.path.join(SFX_DIR, name + '.wav'), soft_clip(x, 1.1), SR)
    if what in ('all', 'music'):
        print('MUSIC:')
        for name, fn in MUSIC.items():
            save(os.path.join(MUSIC_DIR, name + '.wav'), fn(), MSR)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'all')
