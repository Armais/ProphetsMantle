# MANTLE cap. 4 - HOREB. "No estaba Jehova en el viento... ni en el terremoto...
# ni en el fuego. Y tras el fuego un silbo apacible y delicado" (1 Reyes 19:11-12).
# La forma ES el pasaje: viento, terremoto, fuego y por fin el silbo, que es una
# sola voz aguda y limpia. Eolico en DO#, 60 BPM, sin un solo golpe de compas:
# es el unico tema del juego sin pulso.
#
# Periodo: 32 compases de 4/4 a 60 BPM = 128,000 s exactos. Es el mas largo a
# proposito: aqui el jugador anda por una ladera gris sin nada que hacer deprisa.
use_bpm 60

rt = 49            # DO#3
s  = sc(:aeolian)
viento = (0...8)
tierra = (8...14)
fuego  = (14...20)
silbo  = (20...28)
out    = (28...32)

# --- el lecho: nunca para en toda la pieza ---------------------------------
with_fx :reverb, room: 0.96, mix: 0.6 do
  live_loop :wind do
    sd 1401
    use_synth :dark_ambience
    use_synth_defaults attack: 4, sustain: 6, release: 5, cutoff: 84, res: 0.15
    32.times do |bar|
      if bar % 2 == 0
        a = silbo.include?(bar) ? 0.10 : (out.include?(bar) ? 0.15 : 0.13)
        # rt-12 son 46 Hz: por debajo del paso alto de bus, o sea inaudible y
        # ademas invisible para la mezcla. El drone vive una octava mas arriba.
        hp(rt, amp: a, pan: -0.6)
        hp(rt + 7, amp: a * 0.6, pan: 0.62)
        hp(rt - 12, amp: a * 0.5, pan: -0.2)
      end
      sleep 4
    end
  end
end

# --- viento de verdad: el sample, lejos y ancho ----------------------------
with_fx :reverb, room: 0.95, mix: 0.55 do
  live_loop :gusts do
    sd 1402
    32.times do |bar|
      if viento.include?(bar) && bar % 2 == 1
        pk(:ambi_dark_woosh, amp: 0.22, rate: 0.6, pan: -0.5)
      elsif out.include?(bar) && bar % 2 == 1
        pk(:ambi_dark_woosh, amp: 0.22, rate: 0.55, pan: 0.55)
      end
      sleep 4
    end
  end
end

# --- el terremoto: lo mas grave y lo mas fuerte de la pieza ----------------
# El tema no tenia bajo: drone, fuego y silbo viven todos de 110 Hz arriba y
# la pista salia con el 4,7% de energia bajo 200 Hz.
live_loop :bass do
  sd 1408
  use_synth :bass_foundation
  use_synth_defaults attack: 0.5, sustain: 3.5, release: 2.0
  32.times do |bar|
    # En TODOS los compases: si solo suena en los pares, la pista alterna 6 dB.
    a = silbo.include?(bar) ? 0.24 : 0.32
    hp(dg(rt, s, silbo.include?(bar) ? 0 : 4, -1), amp: bar.even? ? a : a * 0.85)
    sleep 4
  end
end

live_loop :quake do
  sd 1403
  32.times do |bar|
    if tierra.include?(bar)
      pk(:drum_roll, amp: 0.17, rate: 0.55, pan: -0.3)
      sleep 2
      pk(:drum_heavy_kick, amp: 0.18, rate: 0.7, pan: 0.2)
      sleep 1
      pk(:drum_tom_lo_hard, amp: 0.16, rate: 0.6, pan: -0.35)
      sleep 1
    else
      sleep 4
    end
  end
end

# --- el fuego: agitado y agudo, pero sin melodia: no es la voz -------------
with_fx :reverb, room: 0.8, mix: 0.4 do
  live_loop :flames do
    sd 1404
    use_synth :blade
    use_synth_defaults attack: 0.05, sustain: 0.1, release: 0.35, cutoff: 120,
                       vibrato_rate: 7.5, vibrato_depth: 0.2, pan: 0.25
    32.times do |bar|
      if fuego.include?(bar)
        8.times do |i|
          hp(dg(rt, s, [4, 6, 5, 7, 4, 6, 8, 5][i], 1), amp: 0.13)
          sleep 0.5
        end
      else
        sleep 4
      end
    end
  end
end

# --- EL SILBO: una sola voz, limpia, sin reverb largo ni vibrato ----------
# Lo unico que queda cuando se van el viento, el terremoto y el fuego.
with_fx :reverb, room: 0.7, mix: 0.25 do
  live_loop :whisper do
    sd 1405
    use_synth :sine
    use_synth_defaults attack: 0.6, sustain: 1.2, release: 1.4, pan: -0.1
    32.times do |bar|
      if silbo.include?(bar)
        hp(dg(rt, s, [0, 2, 4, 2, 1, 0, -2, 0][bar - 20], 2), amp: 0.34)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- aire: un hilo de cabasa. Hasta el tema sin pulso necesita algo arriba --
live_loop :aire do
  sd 1407
  32.times do |bar|
    pk(:hat_cab, amp: silbo.include?(bar) ? 0.20 : 0.13, rate: 1.45, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.09, rate: 1.55, pan: -0.38)
    sleep 2
  end
end

# --- gotas de campana: el agudo que reparte brillo por toda la pieza -------
with_fx :echo, phase: 1.5, decay: 5, mix: 0.3 do
  live_loop :drops do
    sd 1406
    use_synth :pretty_bell
    use_synth_defaults release: 2.2, pan: 0.4
    32.times do |bar|
      if bar % 2 == 1
        hp(dg(rt, s, [0, 4, 2, 5][(bar / 2) % 4], 3), amp: silbo.include?(bar) ? 0.26 : 0.18)
      end
      sleep 4
    end
  end
end
