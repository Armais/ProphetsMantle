# MANTLE cap. 10 - CINEMATICA, ESTAMPA A (Samaria sitiada: noche de asedio
# los carros de fuego todavia invisibles en el monte). Textura sin pulso, y la
# mas oscura de las diez: frigio en MI, la 2a menor sonando todo el rato.
# El tema del capitulo es metal y coro (:blade, :prophet) con campanas; aqui
# ninguno de ellos.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 52            # MI3
s  = sc(:phryg)

with_fx :reverb, room: 0.94, mix: 0.58 do
  live_loop :asedio do
    sd 3201
    use_synth :dark_ambience
    use_synth_defaults attack: 2.4, sustain: 5.0, release: 4.2, cutoff: 74, res: 0.18
    4.times do |bar|
      a = [0.15, 0.26, 0.34, 0.19][bar]
      hp(rt - 12, amp: a, pan: 0.38)
      hp(rt - 11, amp: a * 0.42, pan: -0.42)
      sleep 4
    end
  end

  live_loop :coro do
    sd 3202
    4.times do |bar|
      pk(:ambi_choir, rate: 0.36 + bar * 0.04, amp: [0.10, 0.28, 0.36, 0.10][bar],
         pan: bar.even? ? -0.3 : 0.34)
      sleep 4
    end
  end

  # Las brasas del campamento sirio, muy lejos.
  live_loop :brasas do
    sd 3203
    4.times do |bar|
      pk(:drum_cymbal_pedal, rate: 0.65, amp: [0.05, 0.09, 0.11, 0.06][bar],
         pan: 0.25)
      sleep 4
    end
  end

  live_loop :puntos do
    sd 3204
    use_synth :kalimba
    use_synth_defaults release: 1.6
    hp(dg(rt, s, 0, 3), amp: 0.15, pan: 0.2);   sleep 2.5
    hp(dg(rt, s, 1, 3), amp: 0.24, pan: -0.3);  sleep 2
    hp(dg(rt, s, 4, 3), amp: 0.26, pan: 0.1);   sleep 2.5
    hp(dg(rt, s, 0, 4), amp: 0.22, pan: -0.15); sleep 2
    hp(dg(rt, s, 1, 4), amp: 0.18, pan: 0.35);  sleep 2
    hp(dg(rt, s, 3, 3), amp: 0.14, pan: 0.0);   sleep 2
    hp(dg(rt, s, 0, 3), amp: 0.10, pan: -0.2);  sleep 3
  end
  # Campana: la kalimba sola deja la pista en el 0% sobre 2 kHz, porque casi
  # toda su energia esta en la fundamental.
  live_loop :brillo do
    sd 3205
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: -0.3); sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.26, pan: 0.25); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.18, pan: 0.1);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.12, pan: -0.2); sleep 4.5
  end

end
