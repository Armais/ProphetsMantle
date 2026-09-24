# MANTLE cap. 9 - CINEMATICA, ESTAMPA A (Naaman el sirio: dia claro sobre el
# Jordan, el carro del general parado en la orilla). Textura sin pulso, mucho
# mas abierta que las de los capitulos de Elias: aqui hay luz.
# El tema usa ney sobre :prophet; aqui no suena ninguno de los dos.
# Mixolidio en LA, el modo del capitulo.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 57            # LA3
s  = sc(:mixo)

with_fx :reverb, room: 0.9, mix: 0.54 do
  live_loop :rio do
    sd 3191
    use_synth :dark_ambience
    use_synth_defaults attack: 2.2, sustain: 5.0, release: 4.0, cutoff: 86, res: 0.12
    4.times do |bar|
      a = [0.15, 0.26, 0.34, 0.19][bar]
      hp(rt - 12, amp: a, pan: -0.4)
      hp(rt - 5,  amp: a * 0.62, pan: 0.42)
      sleep 4
    end
  end

  live_loop :coro do
    sd 3192
    4.times do |bar|
      pk(:ambi_choir, rate: 0.5 + bar * 0.05, amp: [0.10, 0.28, 0.36, 0.10][bar],
         pan: bar.even? ? 0.3 : -0.32)
      sleep 4
    end
  end

  # El agua: gotas de salterio, cada vez mas juntas.
  live_loop :gotas do
    sd 3193
    use_synth :kalimba
    use_synth_defaults release: 1.4
    hp(dg(rt, s, 0, 3), amp: 0.16, pan: 0.25);  sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.20, pan: -0.3);  sleep 2.5
    hp(dg(rt, s, 6, 3), amp: 0.26, pan: 0.1);   sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.24, pan: -0.15); sleep 1.5
    hp(dg(rt, s, 4, 4), amp: 0.20, pan: 0.35);  sleep 1.5
    hp(dg(rt, s, 2, 4), amp: 0.16, pan: -0.2);  sleep 1.5
    hp(dg(rt, s, 0, 3), amp: 0.10, pan: 0.0);   sleep 4
  end
  # Campana: la kalimba sola deja la pista en el 0% sobre 2 kHz, porque casi
  # toda su energia esta en la fundamental.
  live_loop :brillo do
    sd 3195
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: -0.3); sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.26, pan: 0.25); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.18, pan: 0.1);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.12, pan: -0.2); sleep 4.5
  end

end
