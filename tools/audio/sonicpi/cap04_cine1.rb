# MANTLE cap. 4 - CINEMATICA, ESTAMPA A (Horeb: noche azul cerrada sobre la
# boca de la cueva). Textura sin pulso: una nota tenida, el coro muy lejos y
# gotas de agua en la roca. El tema del capitulo es el silbo (:sine) sobre
# :dark_ambience con campanas; aqui no suena ninguno de los tres.
# Eolico en DO#.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 49            # DO#3
s  = sc(:aeolian)

with_fx :reverb, room: 0.96, mix: 0.66 do
  live_loop :cueva do
    sd 3131
    use_synth :hollow
    use_synth_defaults attack: 2.8, sustain: 5.0, release: 4.4, cutoff: 92, res: 0.15
    4.times do |bar|
      a = [0.08, 0.15, 0.20, 0.11][bar]
      hp(rt, amp: a, pan: -0.35)
      hp(rt - 5, amp: a * 0.6, pan: 0.4)
      sleep 4
    end
  end

  live_loop :coro do
    sd 3132
    4.times do |bar|
      pk(:ambi_choir, rate: 0.76 + bar * 0.06, amp: [0.10, 0.26, 0.34, 0.10][bar],
         pan: bar.even? ? 0.3 : -0.35)
      sleep 4
    end
  end

  # Gotas en la roca. Van en la octava 3: la kalimba concentra casi toda su
  # energia en la fundamental, y en la 2 no pasaba de los 2 kHz que el
  # analizador cuenta como aire (la pista salia con el 0,1%).
  live_loop :gotas do
    sd 3133
    use_synth :kalimba
    use_synth_defaults release: 1.6
    hp(dg(rt, s, 0, 3), amp: 0.30, pan: 0.3);   sleep 2.5
    hp(dg(rt, s, 4, 3), amp: 0.24, pan: -0.25); sleep 3
    hp(dg(rt, s, 2, 3), amp: 0.36, pan: 0.1);   sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.32, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 6, 3), amp: 0.26, pan: 0.35);  sleep 2
    hp(dg(rt, s, 4, 3), amp: 0.16, pan: 0.0);   sleep 4
  end

  # Y una campana detras de ellas, que es la que de verdad brilla.
  live_loop :campana do
    sd 3134
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    sleep 4
    hp(dg(rt, s, 0, 3), amp: 0.32, pan: -0.3); sleep 3
    hp(dg(rt, s, 4, 4), amp: 0.34, pan: 0.25); sleep 2
    hp(dg(rt, s, 2, 4), amp: 0.26, pan: 0.1);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.16, pan: -0.2); sleep 4
  end
end
