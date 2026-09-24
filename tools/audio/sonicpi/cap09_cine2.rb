# MANTLE cap. 9 - CINEMATICA, ESTAMPA B (las siete zambullidas: "y su carne se
# volvio como la de un nino"). Entra el PULSO: corcheas de agua con roce de
# cabasa y un tambor que cuenta. Voz de laud (:pluck) sobre :hollow.
# Mixolidio en LA.
#
# Periodo: 4 compases de 4/4 a 96 BPM = 10,000 s exactos.
use_bpm 96

rt = 57
s  = sc(:mixo)

l1 = [0, nil, 2, nil, 4, nil, nil, nil]
l2 = [4, nil, 6, nil, 4, nil, 2, nil]
l3 = [7, nil, 6, nil, 4, nil, 2, nil]
l4 = [0, nil, 2, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.78, mix: 0.42 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3295
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :fondo do
    sd 3291
    use_synth :hollow
    use_synth_defaults attack: 0.6, sustain: 3.0, release: 1.8, cutoff: 86, res: 0.2
    4.times do |bar|
      hc([dg(rt, s, 0, 0), dg(rt, s, 2, 0), dg(rt, s, 4, 0)],
         amp: [0.16, 0.19, 0.21, 0.17][bar], pan: 0.3)
      sleep 4
    end
  end

  live_loop :laud do
    sd 3292
    use_synth :pluck
    use_synth_defaults coef: 0.16, cutoff: 118
    [l1, l2, l3, l4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.32, 0.40, 0.44, 0.30][bar], pan: -0.28)
    end
  end

  live_loop :corriente do
    sd 3293
    4.times do |bar|
      if bar == 0
        4.times { pk(:hat_cab, amp: 0.07, rate: 1.45, pan: 0.35); sleep 1 }
      else
        8.times { pk(:hat_cab, amp: 0.08 + bar * 0.015, rate: 1.45, pan: 0.35); sleep 0.5 }
      end
    end
  end

  # El tambor cuenta las zambullidas: una, dos... siete en toda la vuelta.
  live_loop :cuenta do
    sd 3294
    golpes = [0, 2, 1, 2]   # 0+2+1+2 = 5 en el bucle, con dos mas en el ultimo
    4.times do |bar|
      n = golpes[bar]
      if n == 0
        sleep 4
      else
        paso = 4.0 / (bar == 3 ? n + 2 : n)
        (bar == 3 ? n + 2 : n).times do |k|
          pk(:drum_tom_lo_soft, amp: 0.30 - k * 0.02, rate: 0.95, pan: -0.1)
          sleep paso
        end
      end
    end
  end
  # Chispas: sin material por encima de 2 kHz la pista no pasa el gate de aire
  # (el laud y los tambores se quedan todos en los medios).
  live_loop :chispas do
    sd 3296
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.2, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 3), amp: 0.28, pan: 0.3);   sleep 1.5
    hp(dg(rt, s, 6, 3), amp: 0.21, pan: -0.25); sleep 2.5
    hp(dg(rt, s, 0, 4), amp: 0.30, pan: 0.1);   sleep 1.5
    hp(dg(rt, s, 4, 4), amp: 0.14, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.11, pan: 0.2);   sleep 4
  end

end
