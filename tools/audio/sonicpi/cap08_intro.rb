# MANTLE cap. 8 - DIALOGO Y CARTA DE APERTURA (la sunamita).
# "Haganos un pequeno aposento de paredes". Pieza propia: cuerda frotada
# (:dsaw) sobre acordes de :tri, sin percusion y mucho mas lenta que el tema
# que va de ney agudo con latido. Menor melodico en RE.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 50            # RE3
s  = sc(:melmin)

prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

d1 = [0, nil, nil, 2, nil, nil, 4, nil]
d2 = [4, nil, nil, 5, nil, 4, nil, nil]
d3 = [6, nil, 5, nil, 4, nil, 2, nil]
d4 = [2, nil, nil, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.88, mix: 0.5 do
  live_loop :fondo do
    sd 3381
    use_synth :tri
    use_synth_defaults attack: 1.6, sustain: 1.5, release: 2.2, cutoff: 86, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.17
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: 0.32)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3382
    use_synth :subpulse
    use_synth_defaults attack: 0.3, sustain: 2.2, release: 1.2
    raiz = [0, 5, 3, 4]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.11 : 0.16)
      sleep 4
    end
  end

  live_loop :cuerda do
    sd 3383
    use_synth :dsaw
    use_synth_defaults attack: 0.45, sustain: 0.55, release: 1.3, detune: 0.07,
                       cutoff: 92, res: 0.18
    skip 3
    [d1, d2, d3, d4, d2, d3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.24, 0.27, 0.30, 0.22, 0.26, 0.20][i], pan: -0.2)
    end
    skip 7
  end

  live_loop :candil do
    sd 3384
    use_synth :kalimba
    use_synth_defaults release: 1.2
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 2, 1, 2].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 3 : 2), amp: 0.24 - k * 0.03,
             pan: 0.2 + k * 0.1)
          sleep 1
        end
      elsif bar == 5
        hp(dg(rt, s, 0, 4), amp: 0.16, pan: -0.2); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 3), amp: 0.11, pan: 0.15); sleep 4
      else
        sleep 4
      end
    end
  end
  # Campana: con la kalimba sola la pista se queda por debajo del 2% sobre
  # 2 kHz y el centroide se hunde en los graves.
  live_loop :brillo do
    sd 3385
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
