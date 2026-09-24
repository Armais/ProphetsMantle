# MANTLE cap. 7 - DIALOGO Y CARTA DE APERTURA (el aceite).
# "Tu sierva ninguna cosa tiene en casa, sino una vasija de aceite". Pieza
# propia: voz tenida (:blade) sobre acordes de :tri, sin percusion y a la
# mitad de velocidad que el tema, que va de salterio y laud a 96 BPM.
# Mayor en SOL.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
use_bpm 80

rt = 55            # SOL3
s  = sc(:major)

prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

b1 = [0, nil, nil, 2, nil, nil, 4, nil]
b2 = [4, nil, nil, 2, nil, 1, nil, nil]
b3 = [5, nil, 4, nil, 2, nil, 0, nil]
b4 = [2, nil, nil, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.84, mix: 0.46 do
  live_loop :fondo do
    sd 3371
    use_synth :tri
    use_synth_defaults attack: 1.4, sustain: 1.5, release: 2.0, cutoff: 88, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.14
          else 0.17
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.3)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3372
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.0, release: 1.1
    raiz = [0, 5, 3, 4]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.15 : 0.23)
      sleep 4
    end
  end

  live_loop :voz do
    sd 3373
    use_synth :blade
    use_synth_defaults attack: 0.5, sustain: 1.0, release: 1.6, cutoff: 108,
                       vibrato_rate: 4.2
    skip 3
    [b1, b2, b3, b4, b2, b3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 2, [0.22, 0.25, 0.28, 0.20, 0.24, 0.18][i], pan: 0.15)
    end
    skip 7
  end

  live_loop :campana do
    sd 3374
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.2, cutoff: 118
    16.times do |bar|
      if bar >= 9 && bar <= 13
        [0, 2, 1, 2].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 3 : 2), amp: 0.22 - k * 0.03,
             pan: -0.2 - k * 0.08)
          sleep 1
        end
      elsif bar == 4
        hp(dg(rt, s, 0, 3), amp: 0.16, pan: 0.25); sleep 4
      elsif bar == 15
        hp(dg(rt, s, 0, 2), amp: 0.11, pan: -0.15); sleep 4
      else
        sleep 4
      end
    end
  end
end
