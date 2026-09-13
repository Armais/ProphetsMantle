# MANTLE cap. 8 (apertura) - La sunamita. El latido esta, tranquilo y entero:
# aqui el nino todavia vive. Menor melodico, sin campanas de vuelta.
# 16 compases a 68 BPM = 56,4706 s.
use_bpm 68

rt = 50
s  = sc(:melmin)
prog = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [4, 6, 1]]
s1 = [0, nil, 2, nil, 4, nil, 2, nil]
s2 = [4, nil, 3, 2, nil, 1, nil, nil]
s3 = [2, nil, 4, 5, nil, 4, 2, nil]

live_loop :heart do
  sd 2801
  16.times do |bar|
    a = (bar >= 3 && bar < 14) ? 0.24 : 0.13
    pk(:drum_bass_soft, amp: a, rate: 0.55, pan: 0.0)
    sleep 2
    pk(:drum_bass_soft, amp: a * 0.6, rate: 0.55, pan: 0.0)
    sleep 2
  end
end

with_fx :reverb, room: 0.9, mix: 0.46 do
  live_loop :pads do
    sd 2802
    use_synth :prophet
    use_synth_defaults attack: 2.0, sustain: 2.4, release: 2.0, cutoff: 102
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.19 : 0.07, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.07, pan: 0.38) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2803
  use_synth :bass_foundation
  use_synth_defaults attack: 0.08, sustain: 3.0, release: 1.2, amp: 0.30
  16.times do |bar|
    hp(dg(rt, s, prog[bar % 4][0], -1)) if bar >= 2 && bar % 2 == 0
    sleep 4
  end
end

with_fx :reverb, room: 0.92, mix: 0.5 do
  live_loop :voice do
    sd 2804
    use_synth :recorder
    use_synth_defaults attack: 0.22, sustain: 0.4, release: 0.9,
                       vibrato_rate: 4.6, vibrato_depth: 0.09, pan: -0.16
    16.times do |bar|
      if bar >= 5 && bar < 13
        ph(rt, s, [s1, s2, s1, s3, s2, s1, s3, s1][bar - 5], 0.5, 1, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.24 do
  live_loop :bell do
    sd 2805
    use_synth :pretty_bell
    use_synth_defaults release: 2.0, pan: 0.35
    16.times do |bar|
      hp(dg(rt, s, [0, 4, 2, 5][(bar / 2) % 4], 2),
         amp: (bar >= 6 && bar < 14) ? 0.12 : 0.06) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :air do
  sd 2806
  16.times do |bar|
    pk(:hat_cab, amp: (bar >= 4 && bar < 14) ? 0.04 : 0.018, rate: 1.4, pan: 0.46)
    sleep 4
  end
end
