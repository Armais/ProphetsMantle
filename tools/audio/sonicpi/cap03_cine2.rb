# MANTLE cap. 3 - CINEMATICA, ESTAMPA B ("y cayo fuego de Jehova"): las pavesas
# suben del altar. Entra el PULSO, y aqui es marcial: un tambor grande en
# negras que no estaba en la estampa A. Voz de ney (:recorder), que el tema de
# este capitulo no usa. Frigia dominante en MI.
#
# Periodo: 4 compases de 4/4 a 90 BPM = 10,6667 s exactos.
use_bpm 90

rt = 52
s  = sc(:phrygdom)

m1 = [0, nil, 1, nil, 0, nil, nil, nil]
m2 = [3, nil, 4, nil, 3, nil, 1, nil]
m3 = [4, nil, 5, nil, 4, nil, 3, nil]
m4 = [1, nil, 0, nil, nil, nil, nil, nil]

with_fx :reverb, room: 0.78, mix: 0.4 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3225
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.14, 0.19, 0.21, 0.16][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :fondo do
    sd 3221
    use_synth :hollow
    use_synth_defaults attack: 0.5, sustain: 3.2, release: 2.0, cutoff: 82, res: 0.2
    4.times do |bar|
      hc([dg(rt, s, 0, 1), dg(rt, s, 2, 1), dg(rt, s, 4, 0)],
         amp: [0.13, 0.18, 0.22, 0.15][bar], pan: -0.3)
      sleep 4
    end
  end

  live_loop :ney do
    sd 3222
    use_synth :recorder
    use_synth_defaults attack: 0.08, sustain: 0.2, release: 0.45
    [m1, m2, m3, m4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.32, 0.42, 0.46, 0.34][bar], pan: 0.25)
    end
  end

  # Tambor grande en negras: marcial, sin adornos.
  live_loop :tambor do
    sd 3223
    4.times do |bar|
      a = [0.14, 0.26, 0.34, 0.20][bar]
      4.times do |b|
        pk(:drum_tom_lo_hard, amp: a * (b.even? ? 1.0 : 0.62), rate: 1.15, pan: -0.1)
        sleep 1
      end
    end
  end

  # Pavesas: chispas agudas subiendo.
  live_loop :pavesas do
    sd 3224
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.1, cutoff: 122
    4.times do |bar|
      if bar >= 1
        [0, 2, 4, 6].each_with_index do |d, k|
          hp(dg(rt, s, d, 3), amp: 0.30 - k * 0.03, pan: -0.3 + k * 0.2)
          sleep 1
        end
      else
        sleep 4
      end
    end
  end
end
