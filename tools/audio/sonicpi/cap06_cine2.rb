# MANTLE cap. 6 - CINEMATICA, ESTAMPA B (la sal en la fuente: "yo sane estas
# aguas"). Entra el PULSO, y va cojo: los golpes caen en 3+3+2 corcheas, que
# es el 7/8 del capitulo metido en un compas de 4/4.
# Voz de laud (:pluck) sobre :prophet. El modo ya es dorico: el agua esta
# sana, y el tema del capitulo hara ese mismo giro al llegar a su seccion B.
#
# Periodo: 4 compases de 4/4 a 96 BPM = 10,000 s exactos.
use_bpm 96

rt = 59
s  = sc(:dorian)

p1 = [0, nil, nil, 2, nil, nil, 4, nil]
p2 = [4, nil, nil, 5, nil, nil, 4, nil]
p3 = [2, nil, nil, 4, nil, nil, 5, nil]
p4 = [0, nil, nil, 2, nil, nil, nil, nil]

with_fx :reverb, room: 0.8, mix: 0.44 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3265
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :pad do
    sd 3261
    use_synth :prophet
    use_synth_defaults attack: 0.7, sustain: 3.0, release: 2.0, cutoff: 86, res: 0.2
    prog = [[0, 2, 4], [3, 5, 0], [5, 0, 2], [4, 6, 1]]
    4.times do |bar|
      hc(prog[bar].map { |d| dg(rt, s, d, 0) }, amp: [0.17, 0.20, 0.22, 0.18][bar],
         pan: -0.3)
      sleep 4
    end
  end

  live_loop :laud do
    sd 3262
    use_synth :pluck
    use_synth_defaults coef: 0.18, cutoff: 116
    [p1, p2, p3, p4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.34, 0.42, 0.46, 0.32][bar], pan: 0.3)
    end
  end

  # 3+3+2: el paso cojo del capitulo.
  live_loop :cojo do
    sd 3263
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.28, 0.36, 0.24][bar]
        [1.5, 1.5, 1.0].each_with_index do |d, k|
          pk(:drum_tom_lo_soft, amp: a * (k == 0 ? 1.0 : 0.65), rate: 0.95,
             pan: k == 1 ? 0.25 : -0.15)
          sleep d
        end
      end
    end
  end

  live_loop :agua do
    sd 3264
    4.times do |bar|
      next sleep 4 if bar == 0
      8.times do
        pk(:hat_cab, amp: 0.08, rate: 1.5, pan: 0.35)
        sleep 0.5
      end
    end
  end
  # Chispas: sin material por encima de 2 kHz la pista no pasa el gate de aire
  # (el laud y los tambores se quedan todos en los medios).
  live_loop :chispas do
    sd 3266
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.2, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 3), amp: 0.20, pan: 0.3);   sleep 1.5
    hp(dg(rt, s, 6, 3), amp: 0.15, pan: -0.25); sleep 2.5
    hp(dg(rt, s, 0, 4), amp: 0.22, pan: 0.1);   sleep 1.5
    hp(dg(rt, s, 4, 4), amp: 0.14, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.11, pan: 0.2);   sleep 4
  end

end
