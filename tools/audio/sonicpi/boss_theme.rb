# MANTLE - TEMA DEL JEFE. Suena mientras la arena esta sellada, en los diez
# capitulos, asi que tiene que aguantar diez peleas sin cansar: por eso lleva
# forma —dos oleadas con una calma tensa en medio— y no un bucle de 30 s con
# todo a tope de principio a fin, que es lo que habia.
#
# Frigia dominante en RE, 132 BPM. Periodo: 32 compases de 4/4 = 58,1818 s.
# Forma: entra 0-3 | oleada 4-13 | la calma tensa 14-19 | oleada mayor 20-29 | 30-31
use_bpm 132

rt = 50
s  = sc(:phrygdom)
entra = (0...4)
ola1  = (4...14)
calma = (14...20)
ola2  = (20...30)
fin   = (30...32)

prog = [[0, 2, 4], [0, 2, 4], [1, 3, 5], [5, 0, 2]]
b1 = [0, nil, 0, 1, nil, 0, nil, nil]
b2 = [0, 1, 2, nil, 1, nil, 0, nil]
b3 = [4, nil, 3, nil, 2, 1, nil, 0]

live_loop :war do
  sd 5001
  32.times do |bar|
    if calma.include?(bar)
      # No es silencio: es un tambor solo, lejos, esperando.
      pk(:drum_tom_lo_soft, amp: 0.20, rate: 0.7, pan: -0.3)
      sleep 2
      pk(:drum_tom_lo_soft, amp: 0.13, rate: 0.72, pan: 0.28)
      sleep 2
    elsif entra.include?(bar) || fin.include?(bar)
      pk(:drum_tom_lo_hard, amp: 0.34, rate: 0.75, pan: -0.28)
      sleep 2
      pk(:drum_bass_hard, amp: 0.22, rate: 0.85, pan: 0.0)
      sleep 2
    else
      big = ola2.include?(bar)
      pk(:drum_tom_lo_hard, amp: big ? 0.50 : 0.40, rate: 0.78, pan: -0.3)
      sleep 1
      pk(:drum_bass_hard, amp: big ? 0.30 : 0.22, rate: 0.9, pan: 0.0)
      sleep 0.5
      pk(:tabla_ke2, amp: 0.14, rate: 1.2, pan: 0.42)
      sleep 0.5
      pk(:drum_tom_mid_hard, amp: big ? 0.34 : 0.26, rate: 0.85, pan: 0.3)
      sleep 1
      pk(:drum_bass_hard, amp: 0.18, rate: 0.95, pan: -0.15)
      sleep 1
    end
  end
end

live_loop :bass do
  sd 5002
  use_synth :bass_foundation
  use_synth_defaults attack: 0.02, sustain: 0.4, release: 0.25
  32.times do |bar|
    r = dg(rt, s, prog[bar % 4][0], -1)
    if calma.include?(bar)
      hp(r, amp: 0.26, sustain: 3, release: 1)
      sleep 4
    elsif entra.include?(bar) || fin.include?(bar)
      hp(r, amp: 0.34); sleep 2
      hp(r, amp: 0.22); sleep 2
    else
      8.times do |i|
        hp(r + (i == 6 ? 12 : 0), amp: i.even? ? 0.46 : 0.24)
        sleep 0.5
      end
    end
  end
end

with_fx :reverb, room: 0.8, mix: 0.34 do
  live_loop :pads do
    sd 5003
    use_synth :dsaw
    use_synth_defaults attack: 0.5, sustain: 1.6, release: 1.0, cutoff: 96,
                       detune: 0.2
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: calma.include?(bar) ? 0.12 : 0.20, pan: -0.35)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.42) unless calma.include?(bar)
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.76, mix: 0.32 do
  live_loop :lead do
    sd 5004
    use_synth :blade
    use_synth_defaults attack: 0.04, sustain: 0.18, release: 0.3, cutoff: 118,
                       vibrato_rate: 6.5, vibrato_depth: 0.15, pan: -0.22
    32.times do |bar|
      if ola1.include?(bar) || ola2.include?(bar)
        p = [b1, b2, b1, b3, b2, b1, b3, b2][(bar - 4) % 8]
        ph(rt, s, p, 0.5, ola2.include?(bar) ? 1 : 0,
           ola2.include?(bar) ? 0.42 : 0.32, {})
      else
        sleep 4
      end
    end
  end
end

# El coro solo en la calma: es lo que hace que la segunda oleada de miedo.
with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :choir do
    sd 5005
    32.times do |bar|
      if bar == 14 || bar == 17
        pk(:ambi_choir, amp: 0.26, rate: 0.5, pan: -0.5)
        pk(:ambi_choir, amp: 0.18, rate: 0.497, pan: 0.55)
      elsif bar == 20
        pk(:drum_splash_hard, amp: 0.30, rate: 0.85, pan: 0.35)
      end
      sleep 4
    end
  end
end

live_loop :aire do
  sd 5006
  32.times do |bar|
    a = if ola2.include?(bar) then 0.07
        elsif ola1.include?(bar) then 0.05
        else 0.025 end
    4.times do
      pk(:hat_cab, amp: a, rate: 1.4, pan: 0.5)
      sleep 1
    end
  end
end
