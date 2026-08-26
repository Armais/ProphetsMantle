# MANTLE - prologue: "El Cielo Cerrado", 60 BPM, la menor. 22 compases = 88 s,
# largo a proposito para cubrir las ocho estampas de la cinematica. El arco del
# original: pedal grave constante; los pads bajan cuando entra Baal (c.6),
# se abren en el monte (c.11) y reposan en Querit (c.17); tres golpes de tom en
# los giros del relato (c.5, 10, 16); cuerda pulsada desde que entra Elias
# (c.12) y el ney con la frase 0-4-3-2-0 sobre los compases 12-15.
use_bpm 60

ae = [0, 2, 3, 5, 7, 8, 10]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

define :phr do |root, sc, pat, stp, oc, av|
  pat.each do |d|
    play dg(root, sc, d, oc), amp: av if d
    sleep stp
  end
end

live_loop :pedal do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.0, attack: 0.25, sustain: 3.2, release: 0.6, amp: 0.42
  22.times do
    play 33
    sleep 4
  end
end

with_fx :reverb, room: 0.95, mix: 0.5 do
  live_loop :pads do
    use_synth :hollow
    use_synth_defaults attack: 1.6, sustain: 1.6, release: 1.4, cutoff: 92, amp: 0.85
    22.times do |bar|
      ch = if bar < 6
             [45, 48, 52]
           elsif bar < 11
             [44, 47, 51]
           elsif bar < 17
             [50, 53, 57]
           else
             [48, 52, 55]
           end
      play_chord ch, pan: -0.2
      play_chord ch.map { |n| n + 12 }, amp: 0.45, pan: 0.28
      sleep 4
    end
  end
end

live_loop :turns do
  22.times do |bar|
    sample :drum_tom_lo_hard, amp: 0.55, rate: 0.7 if bar == 5 || bar == 10 || bar == 16
    sleep 4
  end
end

with_fx :reverb, room: 0.9, mix: 0.45 do
  live_loop :plucks do
    use_synth :pluck
    use_synth_defaults sustain: 0.5, release: 1.2, coef: 0.5
    22.times do |bar|
      if bar >= 12
        4.times do |e|
          play dg(57, ae, (e * 3 + bar) % 7, 1), amp: 0.24, pan: -0.3 + 0.2 * e
          sleep 1
        end
      else
        sleep 4
      end
    end
  end

  live_loop :ney do
    use_synth :blade
    use_synth_defaults attack: 0.25, sustain: 0.45, release: 0.6, cutoff: 88,
                       vibrato_rate: 4.8, vibrato_depth: 0.14, vibrato_delay: 0.3, pan: -0.1
    sleep 48
    phr 57, ae, [0, nil, nil, nil, 4, nil, 3, nil, nil, nil, 2, nil, nil, nil, 0, nil], 1.0, 0, 0.4
    sleep 24
  end
end
