# MANTLE cap. 9 - DIALOGO Y CARTA DE APERTURA (Naaman).
# "?No son mejores Abana y Farfar, rios de Damasco?". El orgullo antes de
# doblarse. Pieza propia: cuerda frotada (:dsaw) sobre acordes de :tri, sin
# percusion y la mitad de rapida que el tema, que va de ney a 104 BPM.
# Mixolidio en LA.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 57            # LA3
s  = sc(:mixo)

prog = [[0, 2, 4], [6, 1, 3], [3, 5, 0], [4, 6, 1]]

d1 = [0, nil, nil, 2, nil, nil, 4, nil]
d2 = [6, nil, nil, 4, nil, 2, nil, nil]
d3 = [4, nil, 6, nil, 7, nil, 6, nil]
d4 = [4, nil, nil, 2, nil, nil, 0, nil]

with_fx :reverb, room: 0.86, mix: 0.48 do
  live_loop :fondo do
    sd 3391
    use_synth :tri
    use_synth_defaults attack: 1.4, sustain: 1.5, release: 2.0, cutoff: 88, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.17
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.32)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3392
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.0, release: 1.1
    raiz = [0, 6, 3, 4]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.15 : 0.23)
      sleep 4
    end
  end

  live_loop :cuerda do
    sd 3393
    use_synth :dsaw
    use_synth_defaults attack: 0.35, sustain: 0.5, release: 1.1, detune: 0.08,
                       cutoff: 94, res: 0.2
    skip 3
    [d1, d2, d3, d4, d2, d3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.24, 0.28, 0.32, 0.22, 0.26, 0.20][i], pan: 0.2)
    end
    skip 7
  end

  live_loop :gotas do
    sd 3394
    use_synth :kalimba
    use_synth_defaults release: 1.1
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 1, 2, 1].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 3 : 2), amp: 0.24 - k * 0.03,
             pan: -0.2 - k * 0.08)
          sleep 1
        end
      elsif bar == 4
        hp(dg(rt, s, 0, 4), amp: 0.16, pan: 0.25); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 3), amp: 0.11, pan: -0.15); sleep 4
      else
        sleep 4
      end
    end
  end
  # Campana: con la kalimba sola la pista se queda por debajo del 2% sobre
  # 2 kHz y el centroide se hunde en los graves.
  live_loop :brillo do
    sd 3395
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
