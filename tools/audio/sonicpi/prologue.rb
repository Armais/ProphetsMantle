# MANTLE - PROLOGO: "EL CIELO CERRADO". Elias ante Acab: "Vive Jehova, Dios de
# Israel, en cuya presencia estoy, que no habra lluvia ni rocio en estos anos,
# sino por mi palabra" (1 Reyes 17:1). Es un juramento, no una batalla: la
# pieza esta casi vacia y lo que suena es el aire seco y una voz sola.
#
# La ultima seccion es la sequia: las capas se van yendo y no vuelven, y solo
# queda el drone. Cuando arranca el capitulo 1, el cauce de Querit ya suena a
# agua por contraste.
#
# Frigia en MI. Periodo: 24 compases de 4/4 a 66 BPM = 87,2727 s.
use_bpm 66

rt = 52
s  = sc(:phryg)
antes   = (0...6)
juram   = (6...14)
sequia  = (14...24)

prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]
p1 = [0, nil, 1, nil, 0, nil, nil, nil]
p2 = [2, nil, 1, 0, nil, 1, nil, nil]
p3 = [4, nil, 3, 2, nil, 1, nil, 0]

with_fx :reverb, room: 0.96, mix: 0.6 do
  live_loop :aire_seco do
    sd 9001
    use_synth :dark_ambience
    use_synth_defaults attack: 4, sustain: 6, release: 5, cutoff: 82, res: 0.15
    24.times do |bar|
      if bar % 2 == 0
        hp(rt - 12, amp: 0.14, pan: -0.58)
        hp(dg(rt, s, 4, -1), amp: 0.08, pan: 0.6)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pads do
    sd 9002
    use_synth :prophet
    use_synth_defaults attack: 2.2, sustain: 2.2, release: 2.2, cutoff: 100
    24.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      a = if juram.include?(bar) then 0.20
          elsif antes.include?(bar) then 0.11
          elsif bar < 20 then 0.13
          else 0.06 end
      hc(ch, amp: a, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.07, pan: 0.4) if juram.include?(bar)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 9003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.1, sustain: 3.0, release: 1.4, amp: 0.34
  24.times do |bar|
    if bar >= 4 && bar < 20 && bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -1))
    end
    sleep 4
  end
end

# --- el juramento: una voz sola, y despues nada --------------------------
with_fx :reverb, room: 0.92, mix: 0.52 do
  live_loop :voz do
    sd 9004
    use_synth :recorder
    use_synth_defaults attack: 0.25, sustain: 0.4, release: 0.9,
                       vibrato_rate: 4.6, vibrato_depth: 0.1, pan: -0.18
    24.times do |bar|
      if juram.include?(bar)
        ph(rt, s, [p1, p2, p1, p3, p2, p1, p3, p1][bar - 6], 0.5, 1, 0.36, {})
      elsif bar >= 14 && bar < 18
        hp(dg(rt, s, 0, 1), amp: 0.18, sustain: 3, release: 1.5)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- tambor de marco: solo bajo el juramento ------------------------------
live_loop :marco do
  sd 9005
  24.times do |bar|
    if juram.include?(bar)
      pk(:tabla_dhec, amp: 0.34, rate: 0.68, pan: -0.3)
      sleep 3
      pk(:tabla_ghe5, amp: 0.18, rate: 0.8, pan: 0.25)
      sleep 1
    elsif bar == 14
      pk(:drum_splash_soft, amp: 0.20, rate: 0.7, pan: 0.3)
      sleep 4
    else
      sleep 4
    end
  end
end

with_fx :echo, phase: 2.0, decay: 6, mix: 0.3 do
  live_loop :campana do
    sd 9006
    use_synth :dull_bell
    use_synth_defaults release: 3.2, pan: 0.38
    24.times do |bar|
      hp(dg(rt, s, [0, 1, 0, 5][(bar / 2) % 4], 1),
         amp: sequia.include?(bar) ? 0.09 : 0.14) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :polvo do
  sd 9007
  24.times do |bar|
    pk(:hat_cab, amp: sequia.include?(bar) ? 0.02 : 0.035, rate: 1.5, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.018, rate: 1.6, pan: -0.4)
    sleep 2
  end
end
