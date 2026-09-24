# MANTLE cap. 5 - CINEMATICA, ESTAMPA B (el manto sobre los hombros de Eliseo;
# la yunta sacrificada). Entra el PULSO: cencerro y tambor de mano, el paso de
# las doce yuntas. Voz de salterio (:kalimba) sobre :blade tenido; el tema del
# capitulo usa flauta, laud y :prophet, ninguno de ellos. Lidio en FA.
#
# Periodo: 4 compases de 4/4 a 90 BPM = 10,6667 s exactos.
use_bpm 90

rt = 53
s  = sc(:lydian)

k1 = [0, 2, 4, 2, nil, 3, nil, nil]
k2 = [4, nil, 3, 4, nil, 6, nil, nil]
k3 = [7, 6, 4, 3, nil, 2, nil, nil]
k4 = [0, nil, 2, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.8, mix: 0.42 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3255
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :fondo do
    sd 3251
    use_synth :blade
    use_synth_defaults attack: 0.8, sustain: 2.6, release: 1.8, cutoff: 90,
                       vibrato_rate: 3.8, vibrato_depth: 0.06
    4.times do |bar|
      hc([dg(rt, s, 0, 0), dg(rt, s, 2, 0), dg(rt, s, 4, 0)],
         amp: [0.13, 0.16, 0.18, 0.14][bar], pan: -0.3)
      sleep 4
    end
  end

  live_loop :salterio do
    sd 3252
    use_synth :kalimba
    use_synth_defaults release: 1.1
    [k1, k2, k3, k4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.34, 0.42, 0.46, 0.32][bar], pan: 0.3)
    end
  end

  # Las yuntas: el cencerro es el color del capitulo y aqui lleva el paso.
  live_loop :yuntas do
    sd 3253
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.22, 0.28, 0.18][bar]
        4.times do |b|
          pk(:drum_cowbell, amp: a * (b.even? ? 1.0 : 0.55), rate: 0.95,
             pan: b.even? ? -0.2 : 0.25)
          sleep 1
        end
      end
    end
  end

  live_loop :mano do
    sd 3254
    4.times do |bar|
      next sleep 4 if bar == 0
      pk(:drum_tom_hi_soft, amp: 0.24, rate: 1.0, pan: 0.1); sleep 1.5
      pk(:drum_tom_hi_soft, amp: 0.16, rate: 1.1, pan: -0.15); sleep 1
      pk(:drum_tom_hi_soft, amp: 0.20, rate: 0.95, pan: 0.2); sleep 1.5
    end
  end
end
