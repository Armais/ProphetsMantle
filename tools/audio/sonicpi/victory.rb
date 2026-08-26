# MANTLE - victory: 100 BPM, do mayor, fanfarria. El ney canta la fanfarria del
# original ([0,2,4,7 ... 14]) y la cuerda la responde una octava arriba en el
# compas 5. Pads calidos a dos bandas y percusion minima.
# Periodo: 8 compases = 32 pulsos = 19.2 s.
use_bpm 100

mj = [0, 2, 4, 5, 7, 9, 11]
prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

define :phr do |root, sc, pat, stp, oc, av|
  pat.each do |d|
    play dg(root, sc, d, oc), amp: av if d
    sleep stp
  end
end

with_fx :reverb, room: 0.85, mix: 0.4 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 0.8, sustain: 2.4, release: 1.0, cutoff: 88, amp: 0.3
    8.times do |bar|
      ch = prog[bar % 4]
      play_chord ch.map { |d| dg(60, mj, d, 0) }, pan: -0.24
      play_chord ch.map { |d| dg(60, mj, d, 0) }, pan: 0.3, amp: 0.22
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.4, attack: 0.04, sustain: 2.8, release: 0.6, amp: 0.45
  8.times do |bar|
    play dg(60, mj, prog[bar % 4][0], -2)
    sleep 4
  end
end

live_loop :drums do
  8.times do
    sample :bd_haus, amp: 0.5, rate: 0.9
    sleep 2
    sample :drum_snare_soft, amp: 0.25, pan: -0.14
    sleep 2
  end
end

fanfare = [0, 2, 4, 7, nil, 4, 7, 9, nil, 7, 9, 11, 14, nil, nil, nil]

with_fx :reverb, room: 0.9, mix: 0.45 do
  live_loop :lead do
    use_synth :blade
    use_synth_defaults attack: 0.06, sustain: 0.22, release: 0.35, cutoff: 98,
                       vibrato_rate: 5.5, vibrato_depth: 0.1, vibrato_delay: 0.12, pan: -0.14
    phr 60, mj, fanfare, 0.5, 0, 0.55
    sleep 8
    sleep 16
  end

  live_loop :answer do
    use_synth :pluck
    use_synth_defaults sustain: 0.25, release: 0.6, coef: 0.45, pan: 0.26
    sleep 16
    phr 60, mj, fanfare, 0.5, 1, 0.45
    sleep 8
  end
end
