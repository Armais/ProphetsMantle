# MANTLE - boss_theme: 150 BPM, re frigia dominante, implacable. Bajo en
# corcheas que muerde el semitono (grados 0/1), bombo a negras con fantasma en
# el 0.75, cascada de toms en compases impares y el riff del original:
# [0,-,1,0,6,-,4,-,0,-,1,0,3,4,3,1], alternando +4. 16 compases = 25.6 s.
use_bpm 150

pd = [0, 1, 4, 5, 7, 8, 10]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

define :phr do |root, sc, pat, stp, oc, av|
  pat.each do |d|
    play dg(root, sc, d, oc), amp: av if d
    sleep stp
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 2.6, attack: 0.01, sustain: 0.24, release: 0.1, amp: 0.6
  16.times do |bar|
    base = dg(50, pd, 0, -1)
    alt  = dg(50, pd, 1, -1)
    8.times do |e|
      play((e == 2 || e == 6) ? alt : base)
      sleep 0.5
    end
  end
end

with_fx :reverb, room: 0.6, mix: 0.2 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 0.4, sustain: 3.0, release: 0.5, cutoff: 90, amp: 0.2
    16.times do |bar|
      ch = (bar % 2 == 0) ? [0, 1, 4] : [0, 3, 6]
      play_chord ch.map { |d| dg(50, pd, d, 0) }
      sleep 4
    end
  end
end

live_loop :drums do
  16.times do
    4.times do |b|
      sample :bd_klub, amp: 1.0, rate: 0.9
      sample :drum_snare_hard, amp: 0.5 if b == 1 || b == 3
      sleep 0.5
      sample :drum_snare_hard, amp: 0.3 if b == 3
      sleep 0.25
      sample :bd_klub, amp: 0.55, rate: 0.9
      sleep 0.25
    end
  end
end

live_loop :hats do
  32.times do
    sample :drum_cymbal_closed, amp: 0.13, pan: 0.4
    sleep 0.25
    sample :drum_cymbal_closed, amp: 0.13, pan: -0.34
    sleep 0.25
  end
end

live_loop :toms do
  16.times do |bar|
    if bar % 2 == 1
      sleep 2.5
      sample :drum_tom_hi_soft, amp: 0.7, pan: -0.55
      sleep 0.5
      sample :drum_tom_mid_soft, amp: 0.7, pan: 0.0
      sleep 0.5
      sample :drum_tom_lo_soft, amp: 0.8, pan: 0.55
      sleep 0.5
    else
      sleep 4
    end
  end
end

riff = [0, nil, 1, 0, 6, nil, 4, nil, 0, nil, 1, 0, 3, 4, 3, 1]

with_fx :distortion, distort: 0.25, mix: 0.4 do
  with_fx :reverb, room: 0.6, mix: 0.22 do
    live_loop :lead do
      use_synth :dsaw
      use_synth_defaults attack: 0.02, sustain: 0.18, release: 0.12, cutoff: 108, detune: 0.15
      8.times do |ph|
        p = (ph % 2 == 0) ? riff : riff.map { |d| d ? d + 4 : nil }
        phr 50, pd, p, 0.5, 1, 0.42
      end
    end
  end
end
