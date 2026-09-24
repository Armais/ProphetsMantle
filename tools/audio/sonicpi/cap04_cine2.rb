# MANTLE cap. 4 - CINEMATICA, ESTAMPA B (el haz de luz: "un silbo apacible y
# delicado"). Entra el PULSO, y aqui es un latido lento de bombo: en el tema
# del capitulo no hay percusion ninguna, asi que es el contraste mas claro de
# los diez. Voz de laud (:pluck). Eolico en DO#.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 49
s  = sc(:aeolian)

l1 = [0, nil, nil, nil, 2, nil, nil, nil]
l2 = [4, nil, nil, 2, nil, nil, 0, nil]
l3 = [5, nil, 4, nil, 2, nil, nil, nil]
l4 = [0, nil, nil, nil, nil, nil, nil, nil]

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pad do
    sd 3231
    use_synth :prophet
    use_synth_defaults attack: 1.0, sustain: 3.4, release: 2.4, cutoff: 84, res: 0.2
    prog = [[0, 2, 4], [3, 5, 0], [5, 0, 2], [4, 6, 1]]
    4.times do |bar|
      hc(prog[bar].map { |d| dg(rt, s, d, 0) }, amp: [0.17, 0.20, 0.22, 0.18][bar],
         pan: -0.3)
      sleep 4
    end
  end

  live_loop :laud do
    sd 3232
    use_synth :pluck
    use_synth_defaults coef: 0.16, cutoff: 118
    [l1, l2, l3, l4].each_with_index do |pat, bar|
      ph(rt, s, pat, 0.5, 1, [0.34, 0.42, 0.46, 0.30][bar], pan: 0.3)
    end
  end

  live_loop :latido do
    sd 3233
    4.times do |bar|
      if bar == 0
        sleep 4
      else
        a = [nil, 0.30, 0.38, 0.26][bar]
        pk(:drum_bass_soft, amp: a, rate: 0.9, pan: 0.0);      sleep 2.5
        pk(:drum_bass_soft, amp: a * 0.6, rate: 0.9, pan: 0.0); sleep 1.5
      end
    end
  end

  live_loop :luz do
    sd 3234
    4.times do |bar|
      next sleep 4 if bar == 0
      4.times do
        pk(:hat_cab, amp: 0.09 + bar * 0.015, rate: 1.35, pan: -0.35)
        sleep 1
      end
    end
  end

  # El haz de luz, en campanas agudas: con el laud y el bombo solos la pista
  # se quedaba en el 3% sobre 2 kHz.
  live_loop :haz do
    sd 3235
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 1.6, cutoff: 122
    sleep 4
    hp(dg(rt, s, 4, 3), amp: 0.20, pan: 0.3);   sleep 2
    hp(dg(rt, s, 6, 3), amp: 0.15, pan: -0.25); sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.24, pan: 0.1);   sleep 2
    hp(dg(rt, s, 4, 4), amp: 0.14, pan: -0.15); sleep 2
    hp(dg(rt, s, 2, 4), amp: 0.11, pan: 0.2);   sleep 4
  end
end
