# MANTLE - DERROTA. Corta y sin percusion: lo que suena cuando el profeta cae.
# Eolico en RE, el bajo baja un grado por compas y no vuelve a subir. La unica
# pista del juego que termina mas abajo de donde empieza.
# Periodo: 12 compases de 4/4 a 60 BPM = 48,000 s exactos.
use_bpm 60

rt = 50
s  = sc(:aeolian)
bajada = [0, 0, -1, -1, -2, -2, -3, -3, -4, -4, -5, -5]

with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :lecho do
    sd 7001
    use_synth :dark_ambience
    use_synth_defaults attack: 3, sustain: 5, release: 4, cutoff: 80, res: 0.15
    12.times do |bar|
      if bar % 2 == 0
        hp(dg(rt, s, bajada[bar], -1), amp: 0.16, pan: -0.5)
        hp(dg(rt, s, bajada[bar], 0), amp: 0.09, pan: 0.55)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pads do
    sd 7002
    use_synth :prophet
    use_synth_defaults attack: 2.0, sustain: 2.0, release: 2.4, cutoff: 94
    12.times do |bar|
      d = bajada[bar]
      hc([dg(rt, s, d, 0), dg(rt, s, d + 2, 0), dg(rt, s, d + 4, 0)],
         amp: bar >= 10 ? 0.10 : 0.18, pan: -0.3)
      sleep 4
    end
  end
end

with_fx :echo, phase: 2.0, decay: 6, mix: 0.3 do
  live_loop :campana do
    sd 7003
    use_synth :dull_bell
    use_synth_defaults release: 3.5, pan: 0.35
    12.times do |bar|
      hp(dg(rt, s, bajada[bar], 1), amp: bar >= 10 ? 0.10 : 0.17) if bar % 2 == 0
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.92, mix: 0.55 do
  live_loop :voz do
    sd 7004
    use_synth :recorder
    use_synth_defaults attack: 0.5, sustain: 0.8, release: 1.6, pan: -0.15
    12.times do |bar|
      if bar >= 3 && bar < 10
        hp(dg(rt, s, bajada[bar] + 4, 1), amp: 0.22)
        sleep 2
        hp(dg(rt, s, bajada[bar] + 2, 1), amp: 0.15)
        sleep 2
      else
        sleep 4
      end
    end
  end
end

live_loop :aire do
  sd 7005
  12.times do |bar|
    pk(:hat_cab, amp: bar < 10 ? 0.03 : 0.015, rate: 1.35, pan: 0.44)
    sleep 4
  end
end
