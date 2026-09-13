# MANTLE cap. 9 (apertura) - El Jordan. El rio solo, antes de que llegue nadie:
# el agua en corcheas y la flauta. Sin caballos y sin las siete zambullidas.
# 16 compases a 88 BPM = 43,6364 s.
use_bpm 88

rt = 57
s  = sc(:mixo)
prog = [[0, 2, 4], [4, 6, 1], [3, 5, 0], [0, 2, 4]]
r1 = [0, nil, 2, 4, nil, 2, nil, nil]
r2 = [4, 5, nil, 4, nil, 2, 0, nil]
r3 = [2, nil, 4, nil, 5, nil, 4, 2]

with_fx :echo, phase: 0.25, decay: 2, mix: 0.2 do
  live_loop :river do
    sd 2901
    16.times do |bar|
      a = (bar >= 3 && bar < 14) ? 0.05 : 0.028
      8.times do |i|
        pk(:hat_cab, amp: i.even? ? a : a * 0.6, rate: 1.3 + 0.1 * (i % 3),
           pan: i.even? ? 0.5 : -0.42)
        sleep 0.5
      end
    end
  end
end

with_fx :reverb, room: 0.84, mix: 0.42 do
  live_loop :pads do
    sd 2902
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 2.2, release: 1.8, cutoff: 106
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.18 : 0.07, pan: -0.33)
      hc(ch.map { |n| n + 12 }, amp: 0.07, pan: 0.4) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2903
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 2.4, release: 1.0
  16.times do |bar|
    r = dg(rt, s, prog[bar % 4][0], -2)
    hp(r, amp: bar >= 2 ? 0.44 : 0.20)
    sleep 2
    hp(r, amp: 0.22)
    sleep 2
  end
end

with_fx :reverb, room: 0.9, mix: 0.48 do
  live_loop :flute do
    sd 2904
    use_synth :recorder
    use_synth_defaults attack: 0.14, sustain: 0.3, release: 0.6,
                       vibrato_rate: 5.0, vibrato_depth: 0.1, pan: -0.18
    16.times do |bar|
      if bar >= 5 && bar < 13
        ph(rt, s, [r1, r2, r1, r3, r2, r1, r3, r1][bar - 5], 0.5, 1, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :reverb, room: 0.92, mix: 0.5 do
  live_loop :dip do
    sd 2905
    16.times do |bar|
      pk(:perc_swash, amp: 0.18, rate: 0.85, pan: -0.4) if bar == 2
      pk(:perc_swash, amp: 0.12, rate: 0.95, pan: 0.42) if bar == 14
      sleep 4
    end
  end
end
