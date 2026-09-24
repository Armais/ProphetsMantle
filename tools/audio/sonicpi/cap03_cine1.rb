# MANTLE cap. 3 - CINEMATICA, ESTAMPA A (el Monte Carmelo a mediodia, el altar
# y todo Israel mirando). Textura sin pulso, pero tensa: no es la calma de
# Querit sino el calor antes del fuego. Frigia dominante en MI.
# El tema del capitulo es lira (:pluck) con :blade y :prophet; aqui no suena
# ninguno.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 52            # MI3
s  = sc(:phrygdom)

with_fx :reverb, room: 0.9, mix: 0.55 do
  live_loop :calor do
    sd 3121
    use_synth :dark_ambience
    use_synth_defaults attack: 2.0, sustain: 5.0, release: 4.0, cutoff: 80, res: 0.18
    4.times do |bar|
      a = [0.26, 0.31, 0.33, 0.24][bar]
      hp(rt - 12, amp: a, pan: -0.4)
      hp(rt - 11, amp: a * 0.45, pan: 0.45)   # la 2a menor: el roce de este modo
      sleep 4
    end
  end

  live_loop :coro do
    sd 3122
    4.times do |bar|
      pk(:ambi_choir, rate: 0.42 + bar * 0.06, amp: [0.16, 0.28, 0.34, 0.14][bar],
         pan: bar.even? ? -0.3 : 0.35)
      sleep 4
    end
  end

  # Roce de platillo: la brasa que ya esta ahi antes de que caiga el fuego.
  live_loop :brasa do
    sd 3123
    4.times do |bar|
      pk(:drum_cymbal_pedal, rate: 0.7, amp: [0.05, 0.09, 0.12, 0.06][bar],
         pan: 0.2)
      sleep 4
    end
  end

  live_loop :campana do
    sd 3124
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.8, cutoff: 118
    hp(dg(rt, s, 0, 2), amp: 0.15, pan: 0.15); sleep 3
    hp(dg(rt, s, 1, 2), amp: 0.26, pan: -0.3); sleep 2
    hp(dg(rt, s, 4, 2), amp: 0.30, pan: 0.3);  sleep 2.5
    hp(dg(rt, s, 0, 3), amp: 0.28, pan: -0.15); sleep 2
    hp(dg(rt, s, 3, 2), amp: 0.22, pan: 0.4);  sleep 2.5
    hp(dg(rt, s, 4, 3), amp: 0.16, pan: 0.0);  sleep 2
    hp(dg(rt, s, 0, 2), amp: 0.10, pan: -0.25); sleep 2
  end
end
