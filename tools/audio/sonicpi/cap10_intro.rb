# MANTLE cap. 10 - DIALOGO Y CARTA DE APERTURA (Samaria).
# "Jehova, te ruego que abras sus ojos para que vea". Pieza propia: laud
# (:pluck) sobre acordes de :tri, sin percusion y a bastante menos de la mitad
# de velocidad que el tema, que es una marcha de metal a 112 BPM.
# Frigio en MI, el modo con el que arranca el capitulo antes de girar a mayor.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 52            # MI3
s  = sc(:phryg)

prog = [[0, 2, 4], [1, 3, 5], [3, 5, 0], [0, 2, 4]]

l1 = [0, nil, 1, nil, 3, nil, nil, nil]
l2 = [4, nil, 3, nil, 1, nil, 0, nil]
l3 = [5, nil, 4, nil, 3, nil, 1, nil]
l4 = [0, nil, nil, 1, nil, 0, nil, nil]

with_fx :reverb, room: 0.88, mix: 0.5 do
  live_loop :fondo do
    sd 3401
    use_synth :tri
    use_synth_defaults attack: 1.5, sustain: 1.5, release: 2.1, cutoff: 84, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.18
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: 0.33)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3402
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.2, release: 1.2
    raiz = [0, 1, 3, 0]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.12 : 0.17)
      sleep 4
    end
  end

  live_loop :laud do
    sd 3403
    use_synth :pluck
    use_synth_defaults coef: 0.2, cutoff: 112
    skip 3
    [l1, l2, l3, l4, l2, l3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.30, 0.34, 0.38, 0.28, 0.32, 0.24][i], pan: -0.25)
    end
    skip 7
  end

  live_loop :campana do
    sd 3404
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.4, cutoff: 118
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 1, 2, 1].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 4 : 3), amp: 0.24 - k * 0.03,
             pan: 0.22 + k * 0.09)
          sleep 1
        end
      elsif bar == 4
        hp(dg(rt, s, 0, 4), amp: 0.22, pan: -0.25); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 3), amp: 0.15, pan: 0.15); sleep 4
      else
        sleep 4
      end
    end
  end
end
