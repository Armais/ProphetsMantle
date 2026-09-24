# MANTLE cap. 7 - CINEMATICA, ESTAMPA B (las vasijas prestadas: el aceite no
# para hasta que se acaban). Entra el PULSO: palmas y un tambor de mano, que
# es la percusion de casa. Voz de cuerda frotada (:dsaw) sobre :hollow.
# Mayor en SOL.
#
# Periodo: 4 compases de 4/4 a 96 BPM = 10,000 s exactos.
use_bpm 96

rt = 55
s  = sc(:major)

v1 = [0, nil, 2, nil, 4, nil, nil, nil]
v2 = [4, nil, 5, nil, 4, nil, 2, nil]
v3 = [6, nil, 5, nil, 4, nil, 2, nil]
v4 = [0, nil, nil, 2, nil, nil, nil, nil]

with_fx :reverb, room: 0.76, mix: 0.4 do
  # --- bajo: la tonica como pedal. Sin el la pista se queda sin cuerpo (el
  #     analizador pide un 8% de la energia por debajo de 200 Hz). Una octava
  #     por debajo de la tonica, que en todos los capitulos cae entre 69 y
  #     123 Hz: por encima del filtro de 55 Hz del pipeline. --------------------
  live_loop :bajo do
    sd 3275
    use_synth :bass_foundation
    use_synth_defaults attack: 0.12, sustain: 2.8, release: 1.2, cutoff: 68
    4.times do |bar|
      d = bar == 3 ? 4 : 0
      hp(dg(rt, s, d, 0) - 12, amp: [0.26, 0.32, 0.34, 0.28][bar], pan: 0.0)
      sleep 4
    end
  end

  live_loop :fondo do
    sd 3271
    use_synth :hollow
    use_synth_defaults attack: 0.6, sustain: 3.0, release: 1.8, cutoff: 84, res: 0.2
    4.times do |bar|
      hc([dg(rt, s, 0, 0), dg(rt, s, 2, 0), dg(rt, s, 4, 0)],
         amp: [0.16, 0.19, 0.21, 0.17][bar], pan: -0.3)
      sleep 4
    end
  end

  live_loop :cuerda do
    sd 3272
    use_synth :dsaw
    use_synth_defaults attack: 0.2, sustain: 0.35, release: 0.9, detune: 0.08,
                       cutoff: 96, res: 0.2
    [v1, v2, v3, v4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.28, 0.34, 0.38, 0.26][bar], pan: 0.28)
    end
  end

  live_loop :palmas do
    sd 3273
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.16, 0.22, 0.14][bar]
        4.times do |b|
          pk(:perc_snap, amp: a * (b.even? ? 1.0 : 0.6), rate: 1.3,
             pan: b.even? ? -0.25 : 0.3)
          sleep 1
        end
      end
    end
  end

  live_loop :mano do
    sd 3274
    4.times do |bar|
      next sleep 4 if bar == 0
      pk(:drum_tom_hi_soft, amp: 0.24, rate: 1.05, pan: 0.1);  sleep 2
      pk(:drum_tom_hi_soft, amp: 0.15, rate: 1.15, pan: -0.2); sleep 2
    end
  end
  # Chispas: sin material por encima de 2 kHz la pista no pasa el gate de aire
  # (el laud y los tambores se quedan todos en los medios).
  live_loop :chispas do
    sd 3276
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.2, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 3), amp: 0.28, pan: 0.3);   sleep 1.5
    hp(dg(rt, s, 6, 3), amp: 0.15, pan: -0.25); sleep 2.5
    hp(dg(rt, s, 0, 4), amp: 0.30, pan: 0.1);   sleep 1.5
    hp(dg(rt, s, 4, 4), amp: 0.20, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.11, pan: 0.2);   sleep 4
  end

end
