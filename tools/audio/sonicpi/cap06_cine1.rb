# MANTLE cap. 6 - CINEMATICA, ESTAMPA A (Jerico: verdoso enfermo; el agua mala
# y la tierra esteril). Textura sin pulso y con el modo frigio sin arreglar
# todavia: el tema del capitulo es el que cambia de frigio a dorico cuando el
# agua sana, asi que aqui el agua sigue amarga.
# El tema usa ney (:recorder) y :dsaw sobre :dark_ambience; aqui, ninguno.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 59            # SI2
s  = sc(:phryg)

with_fx :reverb, room: 0.94, mix: 0.6 do
  live_loop :pozo do
    sd 3161
    use_synth :hollow
    use_synth_defaults attack: 2.4, sustain: 5.0, release: 4.2, cutoff: 90, res: 0.18
    4.times do |bar|
      a = [0.09, 0.16, 0.21, 0.12][bar]
      hp(rt - 12, amp: a, pan: -0.4)
      hp(rt - 11, amp: a * 0.4, pan: 0.45)    # la 2a menor del frigio: el agua mala
      sleep 4
    end
  end

  live_loop :coro do
    sd 3162
    4.times do |bar|
      pk(:ambi_choir, rate: 0.78 + bar * 0.06, amp: [0.10, 0.26, 0.34, 0.10][bar],
         pan: bar.even? ? 0.3 : -0.35)
      sleep 4
    end
  end

  live_loop :goteo do
    sd 3163
    use_synth :kalimba
    use_synth_defaults release: 1.5
    hp(dg(rt, s, 0, 3), amp: 0.16, pan: 0.25);  sleep 2
    hp(dg(rt, s, 1, 3), amp: 0.22, pan: -0.3);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.14, pan: 0.1);   sleep 2
    hp(dg(rt, s, 3, 3), amp: 0.26, pan: -0.15); sleep 2.5
    hp(dg(rt, s, 1, 4), amp: 0.20, pan: 0.35);  sleep 2
    hp(dg(rt, s, 0, 4), amp: 0.18, pan: 0.0);   sleep 2
    hp(dg(rt, s, 4, 3), amp: 0.10, pan: -0.25); sleep 2.5
  end
  # Campana: la kalimba sola deja la pista en el 0% sobre 2 kHz, porque casi
  # toda su energia esta en la fundamental.
  live_loop :brillo do
    sd 3165
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 118
    sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: -0.3); sleep 3
    hp(dg(rt, s, 4, 3), amp: 0.26, pan: 0.25); sleep 2.5
    hp(dg(rt, s, 2, 4), amp: 0.18, pan: 0.1);  sleep 3
    hp(dg(rt, s, 0, 3), amp: 0.12, pan: -0.2); sleep 4.5
  end

end
