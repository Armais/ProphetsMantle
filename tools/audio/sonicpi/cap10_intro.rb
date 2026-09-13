# MANTLE cap. 10 (apertura) - Samaria sitiada. La ciudad con hambre: frigia,
# marcha lenta y el coro callado. La liberacion no se anuncia en la carta; se
# gana dentro. 16 compases a 92 BPM = 41,7391 s.
use_bpm 92

rt = 52
s  = sc(:phryg)
prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]
w1 = [0, nil, 1, nil, 0, nil, nil, nil]
w2 = [0, 1, 2, nil, 1, 0, nil, nil]
w3 = [4, nil, 3, 2, nil, 1, 0, nil]

live_loop :march do
  sd 3101
  16.times do |bar|
    a = (bar >= 3 && bar < 14) ? 0.26 : 0.14
    pk(:drum_tom_lo_soft, amp: a, rate: 0.75, pan: -0.3)
    sleep 2
    pk(:drum_tom_mid_soft, amp: a * 0.6, rate: 0.85, pan: 0.26)
    sleep 2
  end
end

with_fx :reverb, room: 0.88, mix: 0.44 do
  live_loop :pads do
    sd 3102
    use_synth :prophet
    use_synth_defaults attack: 1.4, sustain: 2.2, release: 1.6, cutoff: 104
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.20 : 0.08, pan: -0.35)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.42) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 3103
  use_synth :bass_foundation
  use_synth_defaults attack: 0.05, sustain: 2.6, release: 1.0, amp: 0.34
  16.times do |bar|
    hp(dg(rt, s, prog[bar % 4][0], -1)) if bar >= 2 && bar % 2 == 0
    sleep 4
  end
end

with_fx :reverb, room: 0.84, mix: 0.4 do
  live_loop :horn do
    sd 3104
    use_synth :blade
    use_synth_defaults attack: 0.12, sustain: 0.32, release: 0.55, cutoff: 112,
                       vibrato_rate: 5.5, vibrato_depth: 0.12, pan: -0.2
    16.times do |bar|
      if bar >= 5 && bar < 13
        ph(rt, s, [w1, w2, w1, w3, w2, w1, w3, w1][bar - 5], 0.5, 0, 0.32, {})
      else
        sleep 4
      end
    end
  end
end

live_loop :air do
  sd 3105
  16.times do |bar|
    pk(:hat_cab, amp: bar >= 4 ? 0.045 : 0.02, rate: 1.35, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.02, rate: 1.5, pan: -0.36)
    sleep 2
  end
end
