# MANTLE cap. 5 - DIALOGO Y CARTA DE APERTURA (El Llamado).
# "Te ruego que me dejes besar a mi padre y a mi madre". Pieza propia: cuerda
# frotada (:dsaw) sobre acordes de :tri, sin percusion y a la mitad de
# velocidad que el tema, que va de flauta doble y laud. Lidio en FA.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 53            # FA3
s  = sc(:lydian)

prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]

d1 = [0, nil, nil, 2, nil, nil, 3, nil]
d2 = [4, nil, nil, 3, nil, 2, nil, nil]
d3 = [6, nil, 4, nil, 3, nil, 2, nil]
d4 = [0, nil, nil, nil, 2, nil, nil, nil]

with_fx :reverb, room: 0.86, mix: 0.48 do
  live_loop :fondo do
    sd 3351
    use_synth :tri
    use_synth_defaults attack: 1.5, sustain: 1.5, release: 2.1, cutoff: 88, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.18
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.35)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3352
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.2, release: 1.2
    raiz = [0, 3, 4, 0]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.12 : 0.18)
      sleep 4
    end
  end

  live_loop :cuerda do
    sd 3353
    use_synth :dsaw
    use_synth_defaults attack: 0.4, sustain: 0.5, release: 1.2, detune: 0.08,
                       cutoff: 94, res: 0.18
    skip 3
    [d1, d2, d3, d4, d2, d3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.24, 0.28, 0.32, 0.22, 0.26, 0.20][i], pan: 0.2)
    end
    skip 7
  end

  live_loop :campana do
    sd 3354
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.4, cutoff: 118
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 2, 4, 2].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i % 3], k == 3 ? 3 : 2), amp: 0.22 - k * 0.03,
             pan: 0.2 + k * 0.1)
          sleep 1
        end
      elsif bar == 4
        hp(dg(rt, s, 0, 3), amp: 0.18, pan: -0.25); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 2), amp: 0.12, pan: 0.2); sleep 4
      else
        sleep 4
      end
    end
  end
end
