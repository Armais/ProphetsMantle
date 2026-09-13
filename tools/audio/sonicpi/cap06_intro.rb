# MANTLE cap. 6 (apertura) - Jerico. El pozo turbio, sin percusion y sin que el
# agua llegue a sanar: eso pasa DENTRO del capitulo, no en la carta. Se queda
# en frigio. 16 compases a 84 BPM = 45,7143 s.
use_bpm 84

rt = 59
s  = sc(:phryg)
prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]
j1 = [0, nil, 1, nil, 0, nil, nil, nil]
j2 = [2, nil, 1, 0, nil, 1, nil, nil]
j3 = [4, nil, 3, nil, 2, nil, 1, nil]

with_fx :reverb, room: 0.93, mix: 0.54 do
  live_loop :well do
    sd 2601
    use_synth :dark_ambience
    use_synth_defaults attack: 3, sustain: 4.5, release: 4, cutoff: 74, res: 0.2
    16.times do |bar|
      if bar % 2 == 0
        hp(rt - 24, amp: bar < 4 ? 0.17 : 0.13, pan: -0.58)
        hp(dg(rt, s, 4, -1), amp: 0.07, pan: 0.6)
      end
      sleep 4
    end
  end
end

# Sin bajo, la apertura salia con el 0,2% de energia bajo 200 Hz: el pozo y la
# cuerda frotada viven los dos en los medios.
live_loop :bass do
  sd 2607
  use_synth :bass_foundation
  use_synth_defaults attack: 0.08, sustain: 3.0, release: 1.2
  16.times do |bar|
    if bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: bar >= 2 ? 0.42 : 0.20)
    end
    sleep 4
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :bad_water do
    sd 2602
    16.times do |bar|
      pk(:ambi_glass_rub, amp: 0.18, rate: 0.55, pan: 0.45) if bar % 4 == 1
      pk(:ambi_soft_buzz, amp: 0.11, rate: 0.7, pan: -0.45) if bar % 4 == 3
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.8, mix: 0.38 do
  live_loop :strings do
    sd 2603
    use_synth :dsaw
    use_synth_defaults attack: 1.2, sustain: 2.0, release: 1.6, cutoff: 86,
                       detune: 0.15
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.17 : 0.07, pan: -0.34)
      hc(ch.map { |n| n + 12 }, amp: 0.07, pan: 0.42) if bar >= 4
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.86, mix: 0.44 do
  live_loop :lead do
    sd 2604
    use_synth :recorder
    use_synth_defaults attack: 0.16, sustain: 0.3, release: 0.6, pan: -0.2
    16.times do |bar|
      if bar >= 5 && bar < 13
        ph(rt, s, [j1, j2, j1, j3, j2, j1, j3, j1][bar - 5], 0.5, 1, 0.32, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.22 do
  live_loop :aguda do
    sd 2608
    use_synth :pretty_bell
    use_synth_defaults release: 1.8, pan: 0.42
    16.times do |bar|
      hp(dg(rt, s, 2, 3), amp: bar >= 3 ? 0.20 : 0.10) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :air do
  sd 2605
  16.times do |bar|
    pk(:hat_cab, amp: bar >= 4 ? 0.04 : 0.02, rate: 1.4, pan: 0.44)
    sleep 2
    pk(:hat_cab, amp: 0.12, rate: 1.5, pan: -0.34)
    sleep 2
  end
end
