# MANTLE cap. 2 (apertura) - el tema de Sarepta desnudo: el organo de la casa,
# el oud solo y las palmas fuera. Periodo: 16 compases a 70 BPM = 54,8571 s.
use_bpm 70

rt = 57
s  = sc(:aeolian)
prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]
o1 = [0, nil, 2, nil, 1, 0, nil, nil]
o2 = [2, 3, nil, 4, nil, 3, 2, nil]
o3 = [4, nil, 3, 4, nil, 5, nil, 4]
o4 = [2, nil, 1, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.78, mix: 0.42 do
  live_loop :room_tone do
    sd 2201
    use_synth :organ_tonewheel
    use_synth_defaults attack: 2.0, sustain: 2.6, release: 2.2
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.15 : 0.10, pan: -0.38)
      hc([ch[0] + 12], amp: 0.05, pan: 0.44)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2202
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 2.8, release: 1.2
  16.times do |bar|
    if bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -2),
         amp: (bar >= 2 && bar < 14) ? 0.38 : 0.18)
    end
    sleep 4
  end
end

with_fx :reverb, room: 0.72, mix: 0.36 do
  live_loop :oud do
    sd 2203
    use_synth :pluck
    use_synth_defaults sustain: 0.3, release: 1.1, coef: 0.55, pan: -0.2
    16.times do |bar|
      if bar >= 4 && bar < 12
        ph(rt, s, [o1, o2, o1, o3, o4, o2, o3, o1][bar - 4], 0.5, 0, 0.36, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.26 do
  live_loop :harm do
    sd 2204
    16.times do |bar|
      if bar >= 8 && bar < 14
        pk(:guit_harmonics, amp: 0.18, rate: bar.even? ? 1.0 : 1.5, pan: 0.38)
      elsif bar % 4 == 1
        pk(:guit_harmonics, amp: 0.10, rate: 0.75, pan: 0.3)
      end
      sleep 4
    end
  end
end

# Una vasija de barro golpeada con el nudillo: la kalimba es lo unico agudo de
# la pieza, y sin ella la apertura salia con el 0,6% de energia sobre 2 kHz.
with_fx :echo, phase: 1.0, decay: 3, mix: 0.22 do
  live_loop :vasija do
    sd 2206
    use_synth :kalimba
    use_synth_defaults release: 1.3, pan: 0.32
    16.times do |bar|
      if bar >= 4 && bar < 14
        hp(dg(rt, s, prog[bar % 4][2], 2), amp: 0.20)
        sleep 2
        hp(dg(rt, s, prog[bar % 4][0], 2), amp: 0.14)
        sleep 2
      else
        hp(dg(rt, s, prog[bar % 4][0], 2), amp: 0.06)
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.2 do
  live_loop :campanita do
    sd 2207
    use_synth :pretty_bell
    use_synth_defaults release: 1.6, pan: 0.42
    16.times do |bar|
      hp(dg(rt, s, prog[bar % 4][2], 2), amp: bar >= 3 ? 0.14 : 0.07) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :air do
  sd 2205
  16.times do |bar|
    pk(:hat_cab, amp: (bar >= 4 && bar < 14) ? 0.11 : 0.045, rate: 1.4, pan: 0.45)
    sleep 2
    pk(:hat_cab, amp: 0.05, rate: 1.5, pan: -0.36)
    sleep 2
  end
end
