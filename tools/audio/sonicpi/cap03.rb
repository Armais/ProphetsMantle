# MANTLE cap. 3 - EL MONTE CARMELO. El desafio a los 450 profetas de Baal y el
# fuego que cae sobre el altar. Frigia dominante en MI (el modo de la tanda
# vieja, que AQUI si le toca: es el capitulo de la confrontacion), 96 BPM,
# coro y tambor marcial. Es el tema mas duro del Acto I a proposito.
#
# Periodo: 32 compases de 4/4 a 96 BPM = 80,000 s exactos.
# Forma:   tambor solo 0-3 | A 4-11 | B coro 12-19 | A' 20-27 | salida 28-31
use_bpm 96

rt = 52            # MI3
s  = sc(:phrygdom)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [4, 6, 1]]

f1 = [0, nil, 1, 0, nil, 4, nil, 3]
f2 = [4, nil, 3, 2, 1, nil, 0, nil]
f3 = [2, 3, 4, nil, 5, nil, 4, nil]
f4 = [0, nil, nil, 1, nil, 0, nil, nil]

# --- tambor marcial: abre el tema el solo ----------------------------------
live_loop :war_drum do
  sd 1301
  32.times do |bar|
    if out.include?(bar)
      # La marcha se aleja, pero no desaparece: si no, el bucle no cierra.
      pk(:drum_tom_lo_soft, amp: 0.16, rate: 0.8, pan: -0.28)
      sleep 2
      pk(:drum_tom_mid_soft, amp: 0.10, rate: 0.9, pan: 0.3)
      sleep 2
    else
      hard = pa2.include?(bar)
      pk(:drum_tom_lo_hard, amp: hard ? 0.50 : 0.36, rate: 0.8, pan: -0.3)
      sleep 1
      pk(:drum_tom_lo_soft, amp: 0.20, rate: 0.85, pan: 0.25)
      sleep 1
      pk(:drum_tom_lo_hard, amp: hard ? 0.42 : 0.30, rate: 0.78, pan: -0.22)
      sleep 1
      pk(:drum_tom_mid_soft, amp: 0.18, rate: 0.9, pan: 0.35)
      sleep 1
    end
  end
end

with_fx :reverb, room: 0.86, mix: 0.4 do
  live_loop :pads do
    sd 1302
    use_synth :prophet
    use_synth_defaults attack: 0.9, sustain: 2.2, release: 1.2, cutoff: 112
    32.times do |bar|
      # Intro y salida llevan el MISMO pad a media voz: con una de las dos
      # vacia el bucle da un escalon, y con las dos vacias se abre un hueco.
      if intro.include?(bar) || out.include?(bar)
        hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: 0.07, pan: -0.3)
        sleep 4
      else
        ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
        hc(ch, amp: 0.22, pan: -0.36)
        hc(ch.map { |n| n + 12 }, amp: 0.10, pan: 0.42)
        sleep 4
      end
    end
  end
end

live_loop :bass do
  sd 1303
  use_synth :bass_foundation
  use_synth_defaults attack: 0.03, sustain: 0.9, release: 0.4
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: 0.20)
      sleep 4
    else
      r = dg(rt, s, prog[bar % 4][0], -2)
      4.times do |i|
        hp(r, amp: i.even? ? 0.46 : 0.24)
        sleep 1
      end
    end
  end
end

# --- el coro de los 450: solo en B, y es el centro de la pieza -------------
with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :choir do
    sd 1304
    32.times do |bar|
      if pb.include?(bar) && bar % 2 == 0
        pk(:ambi_choir, amp: 0.34, rate: 0.5, pan: -0.5)
        pk(:ambi_choir, amp: 0.24, rate: 0.505, pan: 0.55)
      end
      sleep 4
    end
  end
end

# --- lira: el tema, en A y A' ----------------------------------------------
with_fx :reverb, room: 0.74, mix: 0.32 do
  live_loop :lyre do
    sd 1305
    use_synth :pluck
    use_synth_defaults sustain: 0.2, release: 0.6, coef: 0.4, pan: 0.26
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        ph(rt, s, [f1, f2, f1, f3, f4, f2, f3, f1][(bar - 4) % 8], 0.5, 0, 0.32, {})
      else
        sleep 4
      end
    end
  end
end

# --- el fuego: cuerda aguda que solo aparece en A' -------------------------
with_fx :reverb, room: 0.8, mix: 0.38 do
  live_loop :fire do
    sd 1306
    use_synth :blade
    use_synth_defaults attack: 0.08, sustain: 0.2, release: 0.4, cutoff: 118,
                       vibrato_rate: 6.0, vibrato_depth: 0.14, pan: -0.2
    32.times do |bar|
      if pa2.include?(bar)
        ph(rt, s, [f2, f3, f1, f4][(bar - 20) % 4], 0.5, 1, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

# --- platillo: los golpes de seccion ---------------------------------------
with_fx :reverb, room: 0.9, mix: 0.45 do
  live_loop :crash do
    sd 1307
    32.times do |bar|
      if bar == 4 || bar == 12 || bar == 20
        pk(:drum_splash_soft, amp: 0.26, rate: 0.9, pan: 0.4)
      elsif bar == 28
        pk(:drum_cymbal_soft, amp: 0.18, rate: 0.8, pan: -0.4)
      end
      sleep 4
    end
  end
end
