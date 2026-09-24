# MANTLE cap. 1 - CINEMATICA, ESTAMPA B (el escondite: los cuervos bajan por la
# franja de cielo y los soldados de Acab pasan de largo).
#
# Aqui aparece el PULSO. Es lo que separa esta estampa de la A, que no tiene
# ninguno: el capitulo se pone en marcha. Cuerda pulsada que sube, tambor en
# blancas y el graznido de los cuervos, que es literal (1 Reyes 17:6).
#
# Dorico en RE. Tampoco usa el ney: el ney es la voz del capitulo jugado.
#
# Periodo: 4 compases de 4/4 a 80 BPM = 12,000 s exactos (la escena dura 9 s).
use_bpm 80

rt = 50
s  = sc(:dorian)

# La figura que empuja: sube grado a grado y se abre al final.
f1 = [0, nil, 2, nil, 4, nil, 5, nil]
f2 = [4, nil, 5, nil, 7, nil, nil, nil]
f3 = [7, nil, 6, nil, 4, nil, 2, nil]
f4 = [4, nil, 2, nil, 0, nil, -2, nil]

with_fx :reverb, room: 0.82, mix: 0.45 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3206
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  # --- pad corto: acordes que respiran, no un drone --------------------------
  live_loop :pad do
    sd 3201
    use_synth :prophet
    use_synth_defaults attack: 0.6, sustain: 3.6, release: 2.2, cutoff: 86, res: 0.2
    prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]
    4.times do |bar|
      a = [0.17, 0.20, 0.22, 0.19][bar]
      hc(prog[bar].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.25)
      sleep 4
    end
  end

  # --- cuerda pulsada: la voz de esta estampa --------------------------------
  live_loop :cuerda do
    sd 3202
    use_synth :pluck
    use_synth_defaults coef: 0.16, cutoff: 118
    [f1, f2, f3, f4].each_with_index do |pat, bar|
      av = [0.36, 0.44, 0.48, 0.38][bar]
      ph(rt, s, pat, 0.5, 1, av, pan: 0.3)
    end
  end

  # --- tambor en blancas: entra en el compas 1 y se va en el 3 ---------------
  live_loop :tambor do
    sd 3203
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        2.times do |h|
          amp = [nil, 0.30, 0.40, 0.26][bar]
          pk :drum_tom_lo_soft, amp: amp * (h == 0 ? 1.0 : 0.75),
                                rate: 0.92, pan: -0.15
          sleep 2
        end
      end
    end
  end

  # --- chispas: tres campanadas agudas. Sin ellas la pista no llega al 4% de
  #     energia sobre 2 kHz que pide el analizador ----------------------------
  live_loop :chispas do
    sd 3205
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.2, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 2), amp: 0.20, pan: 0.35); sleep 1.5
    hp(dg(rt, s, 6, 2), amp: 0.16, pan: -0.3); sleep 2.5
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: 0.1);  sleep 1.5
    hp(dg(rt, s, 4, 3), amp: 0.14, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 3), amp: 0.12, pan: 0.25); sleep 4

  end

  # --- los cuervos -----------------------------------------------------------
  live_loop :cuervos do
    sd 3204
    sleep 3
    pk :misc_crow, rate: 0.88, amp: 0.20, pan: -0.55
    sleep 4.5
    pk :misc_crow, rate: 1.06, amp: 0.14, pan: 0.6
    sleep 4.5
  end
end
