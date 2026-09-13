# MANTLE - TEMA PRINCIPAL (pantalla de titulo). Reescrito: la version de
# 2026-08-26 medía 2,0 dB de rango dinamico, centroide de 213 Hz y el 0,2% de
# la energia por encima de 2 kHz, o sea plana y tapada.
#
# Maqam HIJAZ en MI (0,1,4,5,7,8,11). Mantiene el centro tonal del tema viejo
# —es la melodia que el jugador asocia al juego— pero la 7a mayor lo saca del
# frigio dominante que compartian las diez pistas.
#
# Periodo: 32 compases de 4/4 a 80 BPM = 96,000 s exactos.
# Forma:   coro solo 0-3 | A lira 4-11 | B ney sin percusion 12-19 |
#          A' todo + tambor 20-27 | salida 28-31
use_bpm 80

rt = 52            # MI3
s  = sc(:hijaz)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]]

# El tema. Ocho pasos por compas.
t1 = [0, nil, 1, 2, nil, 1, 0, nil]
t2 = [2, nil, 3, 4, nil, 3, nil, nil]
t3 = [4, nil, 5, 4, nil, 2, 1, nil]
t4 = [1, nil, 0, nil, nil, nil, nil, nil]

# --- coro: abre y cierra el tema -------------------------------------------
with_fx :reverb, room: 0.94, mix: 0.55 do
  live_loop :choir do
    sd 3001
    32.times do |bar|
      if bar % 8 == 0 && !pb.include?(bar)
        a = intro.include?(bar) ? 0.30 : 0.16
        pk(:ambi_choir, amp: a, rate: 0.5, pan: -0.45)
        pk(:ambi_choir, amp: a * 0.7, rate: 0.502, pan: 0.5)
      end
      sleep 4
    end
  end
end

# --- pads -------------------------------------------------------------------
with_fx :reverb, room: 0.8, mix: 0.36 do
  live_loop :pads do
    sd 3002
    use_synth :prophet
    use_synth_defaults attack: 1.2, sustain: 2.0, release: 1.4, cutoff: 110
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      if intro.include?(bar)
        # A media voz bajo el coro: sin esto la intro se queda en huecos de
        # 24 s entre golpe y golpe de sample.
        hc(ch, amp: 0.10, pan: -0.28)
      else
        hc(ch, amp: out.include?(bar) ? 0.12 : 0.23, pan: -0.34)
        hc(ch.map { |n| n + 12 }, amp: 0.10, pan: 0.4)
      end
      sleep 4
    end
  end
end

# --- bajo -------------------------------------------------------------------
live_loop :bass do
  sd 3003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.05, sustain: 2.6, release: 1.0, amp: 0.40
  32.times do |bar|
    if intro.include?(bar)
      sleep 4
    else
      hp(dg(rt, s, prog[bar % 4][0], -1))
      sleep 4
    end
  end
end

# --- lira: el tema en cuerda pulsada, en A y A' ----------------------------
with_fx :reverb, room: 0.72, mix: 0.3 do
  live_loop :lyre do
    sd 3004
    use_synth :pluck
    use_synth_defaults sustain: 0.3, release: 0.8, coef: 0.42, pan: 0.24
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        ph(rt, s, [t1, t2, t1, t3, t1, t2, t3, t4][(bar - 4) % 8], 0.5, 0,
           pa2.include?(bar) ? 0.38 : 0.28, {})
      else
        sleep 4
      end
    end
  end
end

# --- ney: se queda solo en B ------------------------------------------------
with_fx :reverb, room: 0.9, mix: 0.45 do
  live_loop :ney do
    sd 3005
    use_synth :recorder
    use_synth_defaults attack: 0.13, sustain: 0.28, release: 0.55,
                       vibrato_rate: 5.2, vibrato_depth: 0.11, pan: -0.2
    32.times do |bar|
      if pb.include?(bar) || pa2.include?(bar)
        ph(rt, s, [t2, t3, t1, t4, t2, t3, t1, t4][(bar - 12) % 8], 0.5, 1,
           pb.include?(bar) ? 0.40 : 0.30, {})
      else
        sleep 4
      end
    end
  end
end

# --- campanas: el brillo que no tenia ninguna pista de la tanda vieja -------
with_fx :echo, phase: 1.5, decay: 4, mix: 0.2 do
  live_loop :bells do
    sd 3006
    use_synth :pretty_bell
    use_synth_defaults release: 2.2, pan: 0.45
    32.times do |bar|
      if bar % 4 == 2 && !intro.include?(bar)
        hp(dg(rt, s, prog[bar % 4][2], 2), amp: 0.13)
      end
      sleep 4
    end
  end
end

# --- tambor grande: entra en A, se va en B, vuelve con fuerza en A' --------
live_loop :drum do
  sd 3007
  32.times do |bar|
    if pa.include?(bar) || pa2.include?(bar)
      big = pa2.include?(bar)
      pk(:tabla_dhec, amp: big ? 0.62 : 0.40, rate: 0.7, pan: -0.25)
      sleep 2
      pk(:tabla_ghe3, amp: big ? 0.34 : 0.20, rate: 0.85, pan: -0.15)
      sleep 1
      pk(:tabla_ke2, amp: 0.16, rate: 1.1, pan: 0.3) if big
      sleep 1
    else
      sleep 4
    end
  end
end

# --- cabasa: densidad extra solo en A' -------------------------------------
live_loop :shaker do
  sd 3008
  32.times do |bar|
    if pa2.include?(bar)
      8.times do
        pk(:hat_cab, amp: 0.09, rate: 1.3, pan: 0.52)
        sleep 0.5
      end
    elsif pa.include?(bar) || pb.include?(bar)
      4.times do
        pk(:hat_cab, amp: 0.05, rate: 1.35, pan: 0.48)
        sleep 1
      end
    else
      pk(:hat_cab, amp: 0.025, rate: 1.4, pan: 0.44)
      sleep 4
    end
  end
end
