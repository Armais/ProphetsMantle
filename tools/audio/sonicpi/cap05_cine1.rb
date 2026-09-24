# MANTLE cap. 5 - CINEMATICA, ESTAMPA A (El Llamado: dorado de dia sobre los
# campos de labor; Eliseo con las doce yuntas). Textura sin pulso, luminosa.
# El tema del capitulo es flauta doble (:recorder) con :pluck y :hollow; aqui
# no suena ninguno. Lidio en FA, el modo del capitulo.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 53            # FA3
s  = sc(:lydian)

with_fx :reverb, room: 0.9, mix: 0.54 do
  live_loop :campo do
    sd 3151
    use_synth :dark_ambience
    use_synth_defaults attack: 2.2, sustain: 5.0, release: 4.0, cutoff: 84, res: 0.12
    4.times do |bar|
      a = [0.24, 0.29, 0.31, 0.22][bar]
      hp(rt - 12, amp: a, pan: 0.35)
      hp(rt - 5,  amp: a * 0.6, pan: -0.4)
      sleep 4
    end
  end

  live_loop :coro do
    sd 3152
    4.times do |bar|
      pk(:ambi_choir, rate: 0.5 + bar * 0.06, amp: [0.16, 0.28, 0.34, 0.14][bar],
         pan: bar.even? ? -0.3 : 0.35)
      sleep 4
    end
  end

  # La 4a aumentada del lidio, en campanas: es el sonido del capitulo.
  live_loop :campana do
    sd 3153
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    hp(dg(rt, s, 0, 2), amp: 0.16, pan: 0.1);  sleep 2.5
    hp(dg(rt, s, 3, 2), amp: 0.28, pan: -0.3); sleep 2
    hp(dg(rt, s, 4, 2), amp: 0.24, pan: 0.3);  sleep 2.5
    hp(dg(rt, s, 0, 3), amp: 0.30, pan: -0.15); sleep 2
    hp(dg(rt, s, 3, 3), amp: 0.20, pan: 0.4);  sleep 2
    hp(dg(rt, s, 6, 2), amp: 0.18, pan: 0.0);  sleep 1.5
    hp(dg(rt, s, 4, 2), amp: 0.10, pan: -0.2); sleep 3.5
  end
end
