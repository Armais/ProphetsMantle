# MANTLE - death: 60 BPM, la menor, elegia. Pads oscuros que alternan la triada
# menor con su sombra un semitono/tono abajo (como el original), pedal grave,
# un golpe sordo por compas y la melodia descendente 4-3-2-0 en cuerda larga.
# Periodo: 8 compases = 32 pulsos = 32 s.
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

with_fx :reverb, room: 0.95, mix: 0.55 do
  live_loop :pads do
    use_synth :hollow
    use_synth_defaults attack: 1.8, sustain: 1.4, release: 1.6, cutoff: 95, amp: 0.9
    8.times do |bar|
      ch = (bar % 2 == 0) ? [45, 48, 52] : [43, 46, 50]
      play_chord ch, pan: -0.15
      play_chord ch.map { |n| n + 12 }, amp: 0.5, pan: 0.2
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.0, attack: 0.3, sustain: 3.0, release: 0.8, amp: 0.4
  8.times do
    play 33
    sleep 4
  end
end

live_loop :drums do
  8.times do
    sample :bd_boom, amp: 0.45, rate: 0.8
    sleep 4
  end
end

with_fx :reverb, room: 0.98, mix: 0.6 do
  live_loop :lead do
    use_synth :pluck
    use_synth_defaults sustain: 0.6, release: 1.8, coef: 0.55, pan: -0.05
    phr 57, ae, [4, nil, nil, 3, nil, nil, 2, nil, nil, nil, 0, nil, nil, nil, nil, nil], 0.5, 0, 0.5
    sleep 24
  end

  live_loop :echo_voice do
    use_synth :blade
    use_synth_defaults attack: 0.4, sustain: 0.8, release: 1.2, cutoff: 80,
                       vibrato_rate: 4.5, vibrato_depth: 0.15, vibrato_delay: 0.3, pan: 0.15
    sleep 16
    phr 57, ae, [4, nil, nil, nil, 3, nil, nil, nil, 2, nil, nil, nil, 0, nil, nil, nil], 0.5, 0, 0.28
    sleep 8
  end
end
