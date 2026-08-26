# MANTLE - elias_combat: 136 BPM, mi frigia dominante, fuego. Bajo en corcheas
# con saltos de octava en 3 y 7, bombo a negras, cajas en 2 y 4, charles en
# semicorcheas y riffs de cuerda agresiva doblados una tercera arriba en las
# frases impares. Periodo: 16 compases = 64 pulsos = 28.24 s.
use_bpm 136

pd = [0, 1, 4, 5, 7, 8, 10]
prog = [[0, 2, 4], [0, 2, 4], [1, 3, 5], [4, 6, 1]]

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
  use_synth_defaults divisor: 1, depth: 2.2, attack: 0.01, sustain: 0.28, release: 0.12, amp: 0.55
  16.times do |bar|
    base = dg(52, pd, prog[bar % 4][0], -1)
    8.times do |e|
      play base + ((e == 3 || e == 7) ? 12 : 0)
      sleep 0.5
    end
  end
end

with_fx :reverb, room: 0.6, mix: 0.22 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 0.6, sustain: 2.8, release: 0.6, cutoff: 86, amp: 0.17
    16.times do |bar|
      play_chord prog[bar % 4].map { |d| dg(52, pd, d, 0) }
      sleep 4
    end
  end
end

live_loop :drums do
  16.times do |bar|
    4.times do |b|
      sample :bd_klub, amp: 0.9, rate: 0.95
      sample :drum_snare_hard, amp: 0.4 if b == 1 || b == 3
      sleep 1
    end
  end
end

live_loop :hats do
  32.times do
    sample :drum_cymbal_closed, amp: 0.15, pan: 0.45
    sleep 0.25
    sample :drum_cymbal_closed, amp: 0.09, pan: 0.3
    sleep 0.25
  end
end

live_loop :toms do
  16.times do |bar|
    if bar % 4 == 3
      sleep 3
      sample :drum_tom_mid_soft, amp: 0.6, pan: -0.5
      sleep 0.5
      sample :drum_tom_lo_soft, amp: 0.6, pan: 0.5
      sleep 0.5
    else
      sleep 4
    end
  end
end

r1 = [0, nil, 0, 1, nil, 4, 5, nil]
r2 = [4, nil, 4, 5, 6, nil, 7, 6]
r3 = [6, 7, 6, 5, 4, nil, 1, 0]
r4 = [7, nil, 6, nil, 4, 5, 4, 1]

with_fx :reverb, room: 0.65, mix: 0.25 do
  live_loop :lead do
    use_synth :prophet
    use_synth_defaults attack: 0.02, sustain: 0.2, release: 0.15, cutoff: 105, amp: 0.5, pan: -0.2
    [r1, r2, r3, r4, r1, r2, r3, r4].each do |p|
      sleep 2
      phr 52, pd, p, 0.5, 1, 0.5
      sleep 2
    end
  end

  live_loop :lead2 do
    use_synth :pluck
    use_synth_defaults sustain: 0.15, release: 0.3, coef: 0.3, pan: 0.34
    [r1, r2, r3, r4, r1, r2, r3, r4].each_with_index do |p, ph|
      sleep 2
      if ph % 2 == 1
        phr 52, pd, p.map { |d| d ? d + 2 : nil }, 0.5, 1, 0.3
      else
        sleep 4
      end
      sleep 2
    end
  end
end
