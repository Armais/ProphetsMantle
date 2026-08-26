# MANTLE - main_theme: 84 BPM, mi frigia dominante, contemplativo.
# Reorquestacion en Sonic Pi del tema de gen_audio.py: pads + cuerda pulsada
# + tambor de marco, y el ney (blade con vibrato) cantando pat_a/b/a/c x2.
# Periodo: 16 compases de 4/4 = 64 pulsos = 45.71 s.
use_bpm 84

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

pa = [0, nil, 1, 2, nil, 4, nil, 3, 2, nil, 1, nil, 0, nil, nil, nil]
pb = [4, nil, 5, 6, nil, 7, nil, 6, 5, nil, 4, nil, 2, nil, 1, nil]
pc = [7, nil, 6, 5, nil, 4, nil, 3, 4, nil, 2, nil, 1, nil, 0, nil]

with_fx :reverb, room: 0.85, mix: 0.4 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 1.2, sustain: 2.0, release: 1.2, cutoff: 82, amp: 0.26
    16.times do |bar|
      ch = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]][bar % 4]
      play_chord ch.map { |d| dg(52, pd, d, 0) }, pan: -0.25
      play_chord ch.map { |d| dg(52, pd, d, 0) }, pan: 0.3, amp: 0.2
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.5, attack: 0.05, sustain: 2.8, release: 0.6, amp: 0.5
  16.times do |bar|
    play dg(52, pd, [0, 3, 1, 0][bar % 4], -1)
    sleep 4
  end
end

live_loop :drums do
  16.times do
    sample :tabla_ghe1, amp: 0.9, rate: 0.85
    sleep 2.5
    sample :tabla_ghe3, amp: 0.55, rate: 0.9
    sleep 1.5
  end
end

live_loop :shaker do
  16.times do |i|
    sample :tabla_ke1, amp: 0.16, rate: 1.4, pan: -0.4
    sleep 0.5
    sample :tabla_ke1, amp: 0.09, rate: 1.5, pan: 0.4
    sleep 0.5
  end
end

with_fx :reverb, room: 0.9, mix: 0.45 do
  live_loop :lead do
    use_synth :blade
    use_synth_defaults attack: 0.12, sustain: 0.2, release: 0.35, cutoff: 95,
                       vibrato_rate: 5.5, vibrato_depth: 0.12, vibrato_delay: 0.15, pan: -0.12
    [pa, pb, pa, pc, pa, pb, pa, pc].each do |p|
      phr 52, pd, p, 0.5, 1, 0.5
    end
  end
end

with_fx :reverb, room: 0.7, mix: 0.3 do
  live_loop :string do
    use_synth :pluck
    use_synth_defaults sustain: 0.3, release: 0.7, coef: 0.4, pan: 0.22
    [pa, pb, pa, pc, pa, pb, pa, pc].each do |p|
      phr 52, pd, p, 0.5, 0, 0.26
    end
  end
end
