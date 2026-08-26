# MANTLE - exploration: 104 BPM, mi frigia dominante. Bajo caminante en
# corcheas con salto de octava, arpegio en compases impares y frases de ney
# cada 4 compases (fijas: el original las sorteaba con semilla 3, aqui se
# escriben para que el bucle sea determinista y el corte del periodo cuadre).
# Periodo: 16 compases = 64 pulsos = 36.92 s.
use_bpm 104

pd = [0, 1, 4, 5, 7, 8, 10]
prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

define :phr do |root, sc, pat, stp, oc, av|
  pat.each do |d|
    play dg(root, sc, d, oc), amp: av if d
    sleep stp
  end
end

with_fx :reverb, room: 0.75, mix: 0.3 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 1.0, sustain: 2.2, release: 1.0, cutoff: 80, amp: 0.22
    16.times do |bar|
      play_chord prog[bar % 4].map { |d| dg(52, pd, d, 0) }
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.6, attack: 0.02, amp: 0.5
  16.times do |bar|
    r = dg(52, pd, prog[bar % 4][0], -1)
    4.times do
      play r, sustain: 0.35, release: 0.15
      sleep 0.5
      play r + 12, sustain: 0.12, release: 0.1, amp: 0.2
      sleep 0.5
    end
  end
end

live_loop :drums do
  16.times do
    sample :bd_haus, amp: 0.7, rate: 0.9
    sleep 1
    sample :drum_snare_soft, amp: 0.32, pan: -0.12
    sleep 1
    sample :bd_haus, amp: 0.6, rate: 0.9
    sleep 1
    sample :drum_snare_soft, amp: 0.32, pan: -0.12
    sleep 1
  end
end

live_loop :hats do
  16.times do
    sample :drum_cymbal_closed, amp: 0.16, pan: 0.38
    sleep 0.5
    sample :drum_cymbal_closed, amp: 0.1, pan: 0.38
    sleep 0.5
  end
end

with_fx :echo, phase: 0.5, decay: 2, mix: 0.2 do
  live_loop :arp do
    use_synth :pluck
    use_synth_defaults sustain: 0.2, release: 0.5, coef: 0.35
    16.times do |bar|
      ch = prog[bar % 4]
      if bar % 2 == 1
        [ch[0], ch[1], ch[2], ch[1] + 7, ch[2], ch[1], ch[0], ch[2]].each_with_index do |d, i|
          play dg(52, pd, d, 1), amp: 0.28, pan: -0.45 + 0.9 * (i / 7.0)
          sleep 0.5
        end
      else
        sleep 4
      end
    end
  end
end

f1 = [0, nil, 2, nil, 4, 5, nil, 4, nil, 2, nil, 1, 0, nil, nil, nil]
f2 = [1, nil, 4, nil, 6, nil, 5, 4, nil, 2, 4, nil, 2, nil, 1, nil]
f3 = [2, 4, nil, 5, nil, 7, nil, 6, 5, nil, 4, 2, nil, 1, 0, nil]
f4 = [0, nil, 1, 2, nil, 4, nil, 6, nil, 5, 4, nil, 2, nil, 1, nil]

with_fx :reverb, room: 0.85, mix: 0.4 do
  live_loop :lead do
    use_synth :blade
    use_synth_defaults attack: 0.1, sustain: 0.18, release: 0.3, cutoff: 96,
                       vibrato_rate: 5.5, vibrato_depth: 0.1, vibrato_delay: 0.12, pan: -0.15
    [f1, f2, f3, f4].each do |p|
      sleep 4
      phr 52, pd, p, 0.5, 1, 0.45
      sleep 4
    end
  end
end
