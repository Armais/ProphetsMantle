# MANTLE cap. 7 (apertura) - El aceite. La casa vacia antes del milagro: el
# salterio solo y una vasija (dos notas de kalimba), sin palmas. El arpegio se
# queda en dos voces a proposito: las vasijas todavia no se han llenado.
# 16 compases a 84 BPM = 45,7143 s.
use_bpm 84

rt = 55
s  = sc(:major)
prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]
a1 = [0, nil, 2, 4, nil, 2, 0, nil]
a2 = [4, nil, 2, 1, nil, 2, nil, nil]
a3 = [2, 4, nil, 5, nil, 4, 2, nil]

with_fx :reverb, room: 0.66, mix: 0.34 do
  live_loop :room_pad do
    sd 2701
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 2.2, release: 1.8, cutoff: 104
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.16 : 0.07, pan: -0.3)
      hc(ch.map { |n| n + 12 }, amp: 0.06, pan: 0.36) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2702
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 2.4, release: 1.0, amp: 0.28
  16.times do |bar|
    hp(dg(rt, s, prog[bar % 4][0], -1)) if bar >= 2 && bar % 2 == 0
    sleep 4
  end
end

with_fx :echo, phase: 0.75, decay: 3, mix: 0.22 do
  live_loop :oil do
    sd 2703
    use_synth :kalimba
    use_synth_defaults release: 1.2, pan: 0.28
    16.times do |bar|
      ch = prog[bar % 4]
      if bar >= 4
        hp(dg(rt, s, ch[0], 1), amp: 0.15)
        sleep 2
        hp(dg(rt, s, ch[2], 1), amp: 0.11)
        sleep 2
      else
        hp(dg(rt, s, ch[0], 1), amp: 0.07)
        sleep 4
      end
    end
  end
end

with_fx :reverb, room: 0.68, mix: 0.32 do
  live_loop :psaltery do
    sd 2704
    use_synth :pluck
    use_synth_defaults sustain: 0.25, release: 0.9, coef: 0.32, pan: -0.24
    16.times do |bar|
      if bar >= 5 && bar < 13
        ph(rt, s, [a1, a2, a1, a3, a2, a1, a3, a1][bar - 5], 0.5, 0, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

live_loop :air do
  sd 2705
  16.times do |bar|
    pk(:hat_cab, amp: bar >= 4 ? 0.04 : 0.018, rate: 1.45, pan: 0.44)
    sleep 4
  end
end
