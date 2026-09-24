# MANTLE cap. 8 - CINEMATICA, ESTAMPA B ("y el nino estornudo siete veces").
# Entra el PULSO y es un latido: dos golpes de bombo por compas, el corazon
# que vuelve. Voz de laud (:pluck) sobre :hollow. En el tema del capitulo el
# latido se para en la seccion B; aqui arranca.
#
# Periodo: 4 compases de 4/4 a 80 BPM = 12,000 s exactos.
use_bpm 80

rt = 50
s  = sc(:melmin)

l1 = [0, nil, nil, 2, nil, nil, nil, nil]
l2 = [4, nil, 5, nil, 4, nil, 2, nil]
l3 = [6, nil, 5, nil, 4, nil, 2, nil]
l4 = [0, nil, nil, 2, nil, 0, nil, nil]

with_fx :reverb, room: 0.84, mix: 0.46 do
  live_loop :fondo do
    sd 3281
    use_synth :hollow
    use_synth_defaults attack: 0.8, sustain: 3.0, release: 2.0, cutoff: 82, res: 0.2
    4.times do |bar|
      hc([dg(rt, s, 0, 0), dg(rt, s, 2, 1), dg(rt, s, 4, 1)],
         amp: [0.13, 0.16, 0.18, 0.14][bar], pan: -0.3)
      sleep 4
    end
  end

  live_loop :laud do
    sd 3282
    use_synth :pluck
    use_synth_defaults coef: 0.12, cutoff: 122
    [l1, l2, l3, l4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.40, 0.48, 0.52, 0.38][bar], pan: 0.3)
    end
  end

  # El latido: para en el primer compas y vuelve, cada vez mas seguido.
  live_loop :latido do
    sd 3283
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.13, 0.17, 0.11][bar]
        d = [nil, 2.6, 2.2, 2.4][bar]
        pk(:drum_bass_soft, amp: a, rate: 0.88, hpf: 78, pan: 0.0);       sleep d
        pk(:drum_bass_soft, amp: a * 0.55, rate: 0.88, hpf: 78, pan: 0.0); sleep 4 - d
      end
    end
  end

  live_loop :chispas do
    sd 3284
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.0, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 4), amp: 0.40, pan: 0.3);   sleep 2
    hp(dg(rt, s, 6, 4), amp: 0.34, pan: -0.25); sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.42, pan: 0.1);   sleep 2
    hp(dg(rt, s, 4, 4), amp: 0.30, pan: -0.15); sleep 2
    hp(dg(rt, s, 2, 4), amp: 0.26, pan: 0.2);   sleep 4
  end
  # Roce de cabasa: energia por encima de 2 kHz en todo el bucle. Las campanas
  # solas no llegan porque solo suenan tres veces.
  live_loop :roce do
    sd 3285
    4.times do |bar|
      8.times do
        pk(:hat_cab, amp: bar == 0 ? 0.08 : 0.13, rate: 1.7, pan: 0.4)
        sleep 0.5
      end
    end
  end

end
