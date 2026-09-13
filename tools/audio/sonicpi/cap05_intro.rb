# MANTLE cap. 5 (apertura) - El Llamado. La flauta doble sola sobre el campo,
# sin shaker y sin bajo caminante. 16 compases a 80 BPM = 48,000 s.
use_bpm 80

rt = 53
s  = sc(:lydian)
prog = [[0, 2, 4], [4, 6, 1], [3, 5, 0], [1, 3, 5]]
m1 = [0, nil, 2, 4, nil, 2, nil, nil]
m2 = [4, nil, 5, 4, nil, 2, 0, nil]
m3 = [2, 4, nil, 6, nil, 4, 2, nil]
m4 = [0, nil, 1, 2, nil, 4, nil, nil]

with_fx :reverb, room: 0.84, mix: 0.42 do
  live_loop :pads do
    sd 2501
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 2.2, release: 1.8, cutoff: 110
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.19 : 0.08, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.4) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2502
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 2.6, release: 1.0
  16.times do |bar|
    if bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: bar >= 2 ? 0.38 : 0.18)
    end
    sleep 4
  end
end

with_fx :reverb, room: 0.88, mix: 0.44 do
  live_loop :flutes do
    sd 2503
    16.times do |bar|
      if bar >= 4 && bar < 12
        [m1, m2, m1, m3, m4, m2, m3, m1][bar - 4].each do |d|
          if d
            use_synth :recorder
            hp(dg(rt, s, d, 1), amp: 0.34, pan: -0.3, attack: 0.14,
               sustain: 0.3, release: 0.6)
            use_synth :hollow
            hp(dg(rt, s, d + 2, 1), amp: 0.16, pan: 0.34, attack: 0.18,
               sustain: 0.34, release: 0.6, cutoff: 102)
          end
          sleep 0.5
        end
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.22 do
  live_loop :campanita do
    sd 2506
    use_synth :pretty_bell
    use_synth_defaults release: 1.6, pan: 0.38
    16.times do |bar|
      hp(dg(rt, s, prog[bar % 4][2], 2), amp: bar >= 4 ? 0.12 : 0.06) if bar % 2 == 0
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.94, mix: 0.6 do
  live_loop :oxen do
    sd 2504
    16.times do |bar|
      pk(:drum_cowbell, amp: 0.07, rate: 0.7, pan: -0.55) if bar == 2
      pk(:drum_cowbell, amp: 0.05, rate: 0.66, pan: 0.6) if bar == 14
      sleep 4
    end
  end
end

live_loop :grass do
  sd 2505
  16.times do |bar|
    pk(:hat_cab, amp: (bar >= 4 && bar < 14) ? 0.10 : 0.045, rate: 1.45, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.05, rate: 1.55, pan: -0.36)
    sleep 2
  end
end
