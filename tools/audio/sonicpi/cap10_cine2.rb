# MANTLE cap. 10 - CINEMATICA, ESTAMPA B ("mas son los que estan con nosotros
# que los que estan con ellos"): el monte lleno de gente de a caballo y carros
# de fuego. Entra el PULSO y es una marcha que ACELERA dentro del bucle, igual
# que hace el tema del capitulo pero en 10 segundos.
# Voz de ney (:recorder) sobre :hollow; el tema usa metal y coro, no estos.
# Frigio en MI.
#
# Periodo: 4 compases de 4/4 a 96 BPM = 10,000 s exactos.
use_bpm 96

rt = 52
s  = sc(:phryg)

m1 = [0, nil, nil, 1, nil, nil, 3, nil]
m2 = [4, nil, 3, nil, 4, nil, 5, nil]
m3 = [7, nil, 5, nil, 4, nil, 3, nil]
m4 = [4, nil, 1, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.8, mix: 0.42 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3305
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.13, 0.17, 0.19, 0.15][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :fondo do
    sd 3301
    use_synth :hollow
    use_synth_defaults attack: 0.5, sustain: 3.0, release: 1.8, cutoff: 80, res: 0.22
    4.times do |bar|
      hc([dg(rt, s, 0, 0), dg(rt, s, 2, 1), dg(rt, s, 4, 1)],
         amp: [0.14, 0.17, 0.19, 0.15][bar], pan: -0.3)
      sleep 4
    end
  end

  live_loop :ney do
    sd 3302
    use_synth :recorder
    use_synth_defaults attack: 0.08, sustain: 0.2, release: 0.45
    [m1, m2, m3, m4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.40, 0.48, 0.54, 0.40][bar], pan: 0.28)
    end
  end

  # La marcha: negras, corcheas, y el ultimo compas a tresillos.
  live_loop :marcha do
    sd 3303
    4.times do |bar|
      case bar
      when 0
        sleep 4
      when 1
        4.times { |k| pk(:drum_tom_lo_hard, amp: 0.17 * (k.even? ? 1.0 : 0.6),
                         rate: 1.2, hpf: 85, pan: -0.12); sleep 1 }
      when 2
        8.times { |k| pk(:drum_tom_lo_hard, amp: 0.21 * (k.even? ? 1.0 : 0.55),
                         rate: 1.2, hpf: 85, pan: -0.12); sleep 0.5 }
      else
        12.times { |k| pk(:drum_tom_lo_hard, amp: 0.19 * (k % 3 == 0 ? 1.0 : 0.5),
                          rate: 1.22, hpf: 85, pan: -0.12); sleep 4.0 / 12 }
      end
    end
  end

  live_loop :fuego do
    sd 3304
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.0, cutoff: 124
    sleep 4
    4.times do |k|
      hp(dg(rt, s, [0, 3, 4, 0][k], k == 3 ? 4 : 3), amp: 0.30 - k * 0.03,
         pan: -0.3 + k * 0.2)
      sleep 1
    end
    sleep 4
    hp(dg(rt, s, 4, 4), amp: 0.22, pan: 0.2); sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.16, pan: -0.1); sleep 2
  end
  # Roce de cabasa: energia por encima de 2 kHz en todo el bucle. Las campanas
  # solas no llegan porque solo suenan tres veces.
  live_loop :roce do
    sd 3306
    4.times do |bar|
      8.times do
        pk(:hat_cab, amp: bar == 0 ? 0.08 : 0.13, rate: 1.7, pan: 0.4)
        sleep 0.5
      end
    end
  end

end
