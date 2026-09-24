# MANTLE cap. 4 - DIALOGO Y CARTA DE APERTURA (Horeb).
# "?Que haces aqui, Elias?". El tema del capitulo es el silbo (:sine) sobre
# :dark_ambience, en forma de viento/terremoto/fuego/silbo; esto es otra cosa:
# ney (:recorder) sobre acordes de :tri, sin percusion, muy quieto.
# Eolico en DO#.
#
# Periodo: 12 compases de 4/4 a 60 BPM = 48,000 s exactos.
use_bpm 60

rt = 49            # DO#3
s  = sc(:aeolian)

prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

n1 = [0, nil, nil, 2, nil, nil, nil, nil]
n2 = [4, nil, 3, nil, 2, nil, nil, nil]
n3 = [5, nil, nil, 4, nil, 2, nil, nil]
n4 = [2, nil, 0, nil, nil, nil, nil, nil]

with_fx :reverb, room: 0.92, mix: 0.56 do
  live_loop :fondo do
    sd 3331
    use_synth :tri
    use_synth_defaults attack: 2.0, sustain: 1.6, release: 2.6, cutoff: 94, res: 0.1
    12.times do |bar|
      a = case bar
          when 0, 1   then 0.11
          when 10, 11 then 0.09
          when 6..9   then 0.13
          else 0.17
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.4)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3332
    use_synth :subpulse
    use_synth_defaults attack: 0.3, sustain: 2.6, release: 1.4
    raiz = [0, 5, 3, 4]
    12.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: (bar < 2 || bar > 9) ? 0.07 : 0.10)
      sleep 4
    end
  end

  live_loop :ney do
    sd 3333
    use_synth :recorder
    use_synth_defaults attack: 0.14, sustain: 0.3, release: 0.7
    skip 2
    [n1, n2, n3, n4].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.26, 0.30, 0.32, 0.22][i], pan: 0.15)
    end
    skip 4
    ph(rt, s, n4, 0.5, 1, 0.14, pan: -0.1)
    skip 1
  end

  # El salterio: arpegios rapidos en la seccion B y el aire de la mezcla.
  # La campana: sin ella la pista se queda en el 0,1% sobre 2 kHz, porque la
  # kalimba sola no llega.
  live_loop :campana do
    sd 3335
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.6, cutoff: 118
    sleep 4
    hp(dg(rt, s, 0, 3), amp: 0.30, pan: -0.3); sleep 6
    hp(dg(rt, s, 4, 3), amp: 0.24, pan: 0.25); sleep 6
    hp(dg(rt, s, 2, 4), amp: 0.26, pan: 0.1);  sleep 8
    hp(dg(rt, s, 0, 4), amp: 0.28, pan: -0.2); sleep 8
    hp(dg(rt, s, 4, 4), amp: 0.22, pan: 0.15); sleep 8
    hp(dg(rt, s, 2, 3), amp: 0.18, pan: -0.1); sleep 8
  end

  live_loop :salterio do
    sd 3334
    use_synth :kalimba
    use_synth_defaults release: 1.0
    12.times do |bar|
      if bar < 6 || bar > 9
        skip 1
      else
        [0, 1, 2, 1].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 4 : 3), amp: 0.24 - k * 0.03,
             pan: 0.25 + k * 0.08)
          sleep 1
        end
      end
    end
  end
end
