# MANTLE - ascension: 76 BPM, la mayor, luminoso y ascendente. Dos mitades:
# la segunda sube la triada una octava (el carro que sube), arpegio de cuerda
# en corcheas recorriendo la escala y campana suave en los dos giros.
# Periodo: 8 compases = 32 pulsos = 25.26 s.
use_bpm 76

mj = [0, 2, 4, 5, 7, 9, 11]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 1.2, sustain: 1.8, release: 1.4, cutoff: 86, amp: 0.3
    8.times do |bar|
      lift = 12 * (bar / 4)
      play_chord [57 + lift, 61 + lift, 64 + lift], pan: -0.2
      play_chord [57 + lift, 61 + lift, 64 + lift], amp: 0.2, pan: 0.25
      sleep 4
    end
  end

  live_loop :shimmer do
    use_synth :blade
    use_synth_defaults attack: 1.5, sustain: 1.5, release: 1.5, cutoff: 100,
                       vibrato_rate: 4.0, vibrato_depth: 0.05, vibrato_delay: 0.5, amp: 0.14
    8.times do |bar|
      play 76 + 12 * (bar / 4), pan: 0.1
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.2, attack: 0.2, sustain: 3.0, release: 0.7, amp: 0.38
  8.times do
    play 45
    sleep 4
  end
end

with_fx :echo, phase: 0.5, decay: 3, mix: 0.25 do
  live_loop :arp do
    use_synth :pluck
    use_synth_defaults sustain: 0.3, release: 0.8, coef: 0.4
    8.times do |bar|
      8.times do |e|
        play dg(57, mj, (e * 2) % 7, 1 + (bar / 4)), amp: 0.3, pan: -0.3 + 0.086 * e
        sleep 0.5
      end
    end
  end
end

with_fx :reverb, room: 0.95, mix: 0.5 do
  live_loop :bells do
    use_synth :pretty_bell
    sleep 15.5
    play 88, amp: 0.2, pan: 0.3
    sleep 15.5
    play 93, amp: 0.18, pan: -0.3
    sleep 1
  end
end
