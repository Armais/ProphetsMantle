# MANTLE cap. 6 - DIALOGO Y CARTA DE APERTURA (Jerico).
# "El agua es mala y la tierra esteril". Pieza propia: voz tenida (:blade)
# sobre acordes de :tri, sin percusion, la mitad de rapida que el tema (que va
# de ney y cuerda frotada a 100 BPM). Frigio en SI, el modo del capitulo antes
# de que el agua sane.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 59            # SI2
s  = sc(:phryg)

prog = [[0, 2, 4], [1, 3, 5], [3, 5, 0], [0, 2, 4]]

b1 = [0, nil, nil, 1, nil, nil, 0, nil]
b2 = [3, nil, nil, 4, nil, 3, nil, nil]
b3 = [5, nil, 4, nil, 3, nil, 1, nil]
b4 = [0, nil, nil, nil, -2, nil, 0, nil]

with_fx :reverb, room: 0.9, mix: 0.52 do
  live_loop :fondo do
    sd 3361
    use_synth :tri
    use_synth_defaults attack: 1.6, sustain: 1.5, release: 2.2, cutoff: 84, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.18
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: 0.35)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3362
    use_synth :subpulse
    use_synth_defaults attack: 0.3, sustain: 2.2, release: 1.2
    raiz = [0, 1, 3, 0]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.16 : 0.24)
      sleep 4
    end
  end

  live_loop :voz do
    sd 3363
    use_synth :blade
    use_synth_defaults attack: 0.55, sustain: 1.0, release: 1.7, cutoff: 106,
                       vibrato_rate: 4.0
    skip 3
    [b1, b2, b3, b4, b2, b3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 2, [0.22, 0.26, 0.28, 0.20, 0.24, 0.18][i], pan: -0.2)
    end
    skip 7
  end

  live_loop :goteo do
    sd 3364
    use_synth :kalimba
    use_synth_defaults release: 1.1
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 1, 2, 1].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 3 : 2), amp: 0.26 - k * 0.03,
             pan: 0.25 + k * 0.08)
          sleep 1
        end
      elsif bar == 5
        hp(dg(rt, s, 0, 4), amp: 0.18, pan: -0.2); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 3), amp: 0.12, pan: 0.15); sleep 4
      else
        sleep 4
      end
    end
  end
  # Campana: con la kalimba sola la pista se queda por debajo del 2% sobre
  # 2 kHz y el centroide se hunde en los graves.
  live_loop :brillo do
    sd 3365
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.6, cutoff: 118
    sleep 4
    hp(dg(rt, s, 0, 3), amp: 0.20, pan: -0.3); sleep 8
    hp(dg(rt, s, 4, 3), amp: 0.15, pan: 0.25); sleep 12
    hp(dg(rt, s, 2, 4), amp: 0.24, pan: 0.1);  sleep 8
    hp(dg(rt, s, 0, 3), amp: 0.16, pan: -0.2); sleep 16
    hp(dg(rt, s, 4, 4), amp: 0.18, pan: 0.15); sleep 8
    hp(dg(rt, s, 0, 3), amp: 0.11, pan: 0.0);  sleep 8
  end

end
