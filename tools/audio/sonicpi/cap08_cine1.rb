# MANTLE cap. 8 - CINEMATICA, ESTAMPA A (la sunamita: interior del aposento
# luz de candil, el nino en la cama de Eliseo). Textura sin pulso, en menor
# melodico, que es el modo con el que arranca el capitulo antes de girar a
# mayor cuando el nino despierta.
# El tema usa ney agudo y campanas sobre :prophet; aqui, ninguno.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 50            # RE3
s  = sc(:melmin)

with_fx :reverb, room: 0.92, mix: 0.58 do
  live_loop :aposento do
    sd 3181
    use_synth :dark_ambience
    use_synth_defaults attack: 2.6, sustain: 5.0, release: 4.2, cutoff: 76, res: 0.14
    4.times do |bar|
      a = [0.15, 0.26, 0.34, 0.19][bar]
      hp(rt - 12, amp: a, pan: 0.35)
      hp(rt - 5,  amp: a * 0.6, pan: -0.4)
      sleep 4
    end
  end

  live_loop :coro do
    sd 3182
    4.times do |bar|
      pk(:ambi_choir, rate: 0.44 + bar * 0.05, amp: [0.10, 0.28, 0.36, 0.10][bar],
         pan: bar.even? ? -0.3 : 0.32)
      sleep 4
    end
  end

  # El candil: notas sueltas, muy separadas. Es el material agudo de la pista.
  live_loop :candil do
    sd 3183
    use_synth :kalimba
    use_synth_defaults release: 1.8
    hp(dg(rt, s, 0, 3), amp: 0.16, pan: 0.2);   sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.22, pan: -0.25); sleep 2.5
    hp(dg(rt, s, 6, 3), amp: 0.26, pan: 0.15);  sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.24, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.18, pan: 0.35);  sleep 2
    hp(dg(rt, s, 4, 3), amp: 0.12, pan: 0.0);   sleep 4
  end
  # Campana: la kalimba sola deja la pista en el 0% sobre 2 kHz, porque casi
  # toda su energia esta en la fundamental.
  live_loop :brillo do
    sd 3185
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: -0.3); sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.26, pan: 0.25); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.18, pan: 0.1);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.12, pan: -0.2); sleep 4.5
  end

end
