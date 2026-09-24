# MANTLE cap. 1 - CINEMATICA, ESTAMPA A ("Jehova vive... no habra lluvia").
# La primera de las DOS estampas: rotulo, versiculo y el canon de Querit al
# amanecer. Es textura, no tema: ni pulso ni melodia reconocible, para que
# cuando entre la estampa B (que si tiene pulso) se note el corte.
#
# NO comparte un solo timbre con cap01.rb: alli el fondo es :hollow y la voz
# el ney (:recorder); aqui el fondo es :dark_ambience y la voz un coro de
# sample con campana. Dorico en RE, el modo del capitulo: la familia se
# mantiene, el sonido no.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
# Arco: compas 1 casi vacio -> 2-3 abre el coro -> 4 se apaga. La escena dura
# 10,5 s, o sea que el bucle no llega a repetir: lo que importa es el arranque.
use_bpm 60

rt = 50            # RE3
s  = sc(:dorian)

with_fx :reverb, room: 0.95, mix: 0.62 do
  # --- el canon: dos notas quietas, una quinta abierta -----------------------
  live_loop :aire do
    sd 3101
    use_synth :dark_ambience
    use_synth_defaults attack: 2.6, sustain: 5.5, release: 4.5, cutoff: 78, res: 0.15
    4.times do |bar|
      a = [0.24, 0.30, 0.32, 0.22][bar]
      hp(rt - 12, amp: a, pan: -0.45)
      hp(rt - 5,  amp: a * 0.7, pan: 0.5)   # la quinta
      sleep 4
    end
  end

  # --- el coro, a lo lejos: entra en el 2 y se retira en el 4 ----------------
  live_loop :coro do
    sd 3102
    4.times do |bar|
      if bar == 1 || bar == 2
        pk :ambi_choir, rate: bar == 1 ? 0.52 : 0.62,
                        amp: bar == 1 ? 0.30 : 0.36, pan: bar == 1 ? -0.3 : 0.35
      elsif bar == 3
        pk :ambi_choir, rate: 0.44, amp: 0.17, pan: 0.0
      end
      sleep 4
    end
  end

  # --- campanas: las unicas notas con nombre, y el aire de la pista ----------
  live_loop :campana do
    sd 3103
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.2, cutoff: 115
    # compas 0: nada. 1: la tonica. 2: la tercera y la septima. 3: la tonica
    # una octava mas arriba, ya apagandose.
    hp(dg(rt, s, 0, 2), amp: 0.16, pan: 0.0);  sleep 2.5
    hp(dg(rt, s, 4, 2), amp: 0.12, pan: 0.3);  sleep 1.5
    hp(dg(rt, s, 0, 2), amp: 0.34, pan: 0.25); sleep 2
    hp(dg(rt, s, 4, 2), amp: 0.24, pan: -0.2); sleep 2
    hp(dg(rt, s, 2, 3), amp: 0.30, pan: -0.35); sleep 1.5
    hp(dg(rt, s, 6, 2), amp: 0.28, pan: 0.4);  sleep 1.0
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: 0.1);  sleep 1.5
    hp(dg(rt, s, 4, 2), amp: 0.14, pan: -0.1); sleep 2
    hp(dg(rt, s, 0, 2), amp: 0.10, pan: 0.2);  sleep 2
  end

  # --- el arroyo: un hilo de agua muy flojo bajo todo ------------------------
  live_loop :agua do
    sd 3104
    4.times do |bar|
      pk :ambi_haunted_hum, rate: 0.8, amp: bar == 0 ? 0.05 : 0.09,
                            cutoff: 100, pan: 0.15
      sleep 4
    end
  end
end
