# MANTLE - eliseo_combat: 128 BPM, sol dorico. La misma osamenta que el combate
# de Elias pero mas luminosa: dorico en vez de frigia dominante, pluck y blade
# en vez de sierra, caja blanda. Periodo: 16 compases = 64 pulsos = 30 s.
use_bpm 128

dr = [0, 2, 3, 5, 7, 9, 10]
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
  use_synth_defaults divisor: 1, depth: 1.6, attack: 0.01, sustain: 0.3, release: 0.12, amp: 0.5
  16.times do |bar|
    base = dg(55, dr, prog[bar % 4][0], -1)
    8.times do |e|
      play base + ((e == 3 || e == 7) ? 12 : 0)
      sleep 0.5
    end
  end
end

with_fx :reverb, room: 0.7, mix: 0.28 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 0.8, sustain: 2.6, release: 0.6, cutoff: 84, amp: 0.18
    16.times do |bar|
      play_chord prog[bar % 4].map { |d| dg(55, dr, d, 0) }
      sleep 4
    end
  end
end

live_loop :drums do
  16.times do |bar|
    4.times do |b|
      sample :bd_haus, amp: 0.8, rate: 1.0
      sample :drum_snare_soft, amp: 0.42 if b == 1 || b == 3
      sleep 1
    end
  end
end

live_loop :hats do
  32.times do
    sample :drum_cymbal_closed, amp: 0.13, pan: 0.35
    sleep 0.25
    sample :drum_cymbal_closed, amp: 0.08, pan: 0.2
    sleep 0.25
  end
end

live_loop :shaker do
  16.times do
    sample :tabla_ke1, amp: 0.14, rate: 1.5, pan: -0.35
    sleep 2
    sample :tabla_ke1, amp: 0.1, rate: 1.6, pan: -0.35
    sleep 2
  end
end

r1 = [0, nil, 2, 4, nil, 5, 4, nil]
r2 = [4, nil, 5, 6, nil, 7, nil, 6]
r3 = [7, 6, nil, 5, 4, nil, 2, 1]
r4 = [2, nil, 4, nil, 5, 4, 2, 0]

with_fx :reverb, room: 0.75, mix: 0.32 do
  live_loop :lead do
    use_synth :blade
    use_synth_defaults attack: 0.05, sustain: 0.18, release: 0.25, cutoff: 100,
                       vibrato_rate: 5.5, vibrato_depth: 0.08, vibrato_delay: 0.1, pan: -0.18
    [r1, r2, r3, r4, r1, r2, r3, r4].each do |p|
      sleep 2
      phr 55, dr, p, 0.5, 1, 0.45
      sleep 2
    end
  end

  live_loop :lead2 do
    use_synth :pluck
    use_synth_defaults sustain: 0.15, release: 0.35, coef: 0.4, pan: 0.3
    [r1, r2, r3, r4, r1, r2, r3, r4].each_with_index do |p, ph|
      sleep 2
      if ph % 2 == 1
        phr 55, dr, p.map { |d| d ? d + 2 : nil }, 0.5, 1, 0.28
      else
        sleep 4
      end
      sleep 2
    end
  end
end
