# MANTLE cap. 2 - CINEMATICA, ESTAMPA B ("un punado de harina en la tinaja").
# Entra el PULSO: es lo que la separa de la estampa A. La voz es el ney
# (:recorder), que en el tema de este capitulo no aparece (alli manda el oud).
# Eolico en LA.
#
# Periodo: 4 compases de 4/4 a 72 BPM = 13,3333 s exactos.
use_bpm 72

rt = 57
s  = sc(:aeolian)

m1 = [0, nil, nil, 2, nil, 3, nil, nil]
m2 = [3, nil, 4, nil, nil, 3, 2, nil]
m3 = [5, nil, 4, nil, 3, nil, 2, nil]
m4 = [2, nil, 0, nil, nil, nil, -1, nil]

with_fx :reverb, room: 0.8, mix: 0.44 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3215
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :pad do
    sd 3211
    use_synth :prophet
    use_synth_defaults attack: 0.7, sustain: 3.4, release: 2.2, cutoff: 84, res: 0.2
    prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]
    4.times do |bar|
      hc(prog[bar].map { |d| dg(rt, s, d, 0) }, amp: [0.17, 0.20, 0.22, 0.18][bar],
         pan: 0.3)
      sleep 4
    end
  end

  live_loop :ney do
    sd 3212
    use_synth :recorder
    use_synth_defaults attack: 0.1, sustain: 0.22, release: 0.5
    [m1, m2, m3, m4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.30, 0.38, 0.42, 0.32][bar], pan: -0.25)
    end
  end

  # Darbuka al trote, callada solo en el primer compas.
  live_loop :darbuka do
    sd 3213
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.26, 0.34, 0.24][bar]
        [0, 1.5, 2, 3].each_with_index do |t, k|
          pk :drum_tom_hi_soft, amp: a * (k == 0 ? 1.0 : 0.7), rate: 1.05, pan: 0.15
          sleep [1.5, 0.5, 1, 1][k]
        end
      end
    end
  end

  # Campanillas agudas: sin ellas la pista no llega al 4% sobre 2 kHz.
  live_loop :chispas do
    sd 3216
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.1, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 2), amp: 0.20, pan: 0.3);   sleep 1.5
    hp(dg(rt, s, 6, 2), amp: 0.15, pan: -0.25); sleep 2.5
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: 0.1);   sleep 1.5
    hp(dg(rt, s, 4, 3), amp: 0.14, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 3), amp: 0.11, pan: 0.2);   sleep 4
  end

  live_loop :cascara do
    sd 3214
    4.times do |bar|
      next sleep 4 if bar == 0
      8.times do
        pk :perc_snap, amp: 0.07, rate: 1.2, pan: -0.4
        sleep 0.5
      end
    end
  end
end
