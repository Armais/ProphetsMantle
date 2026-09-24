# MANTLE cap. 2 - DIALOGO Y CARTA DE APERTURA (Sarepta).
# Pieza propia, NO el tema del capitulo desnudo: el tema es oud (:pluck) con
# organo y darbuka; esto es un salterio (:kalimba) sobre acordes de :tri, sin
# percusion, a la mitad de velocidad. Mismo modo (eolico en LA) y mismo centro
# tonal, que es lo que los hace del mismo sitio sin sonar a lo mismo.
#
# Periodo: 16 compases de 4/4 a 72 BPM = 53,3333 s exactos.
# Forma: 0-2 se abre | 3-8 canta el salterio | 9-13 le responde la voz |
#        14-15 se apaga.
use_bpm 72

rt = 57            # LA3
s  = sc(:aeolian)

prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [6, 1, 3]]

k1 = [0, 2, 4, 2, nil, 4, nil, nil]
k2 = [4, 5, 4, 2, nil, 0, nil, nil]
k3 = [2, 4, 5, 4, nil, 2, 0, nil]
k4 = [0, nil, -2, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.86, mix: 0.5 do
  live_loop :fondo do
    sd 3311
    use_synth :tri
    use_synth_defaults attack: 1.6, sustain: 1.6, release: 2.2, cutoff: 86, res: 0.1
    16.times do |bar|
      a = case bar
          when 0..2   then 0.11
          when 14, 15 then 0.09
          when 9..13  then 0.15
          else 0.18
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.35)
      sleep 4
    end
  end

  live_loop :bajo do
    sd 3312
    use_synth :subpulse
    use_synth_defaults attack: 0.3, sustain: 2.4, release: 1.2
    raiz = [0, 5, 3, 6]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.16 : 0.24)
      sleep 4
    end
  end

  # --- el salterio: la voz de la escena --------------------------------------
  live_loop :salterio do
    sd 3313
    use_synth :kalimba
    use_synth_defaults release: 1.4
    16.times do |bar|
      if bar < 3
        skip 1
      elsif bar <= 8
        ph(rt, s, [k1, k2, k3, k4][(bar - 3) % 4], 0.5, 1,
           [0.34, 0.40, 0.44, 0.30][(bar - 3) % 4], pan: 0.3)
      elsif bar <= 13
        # acompana en corcheas sueltas mientras responde la voz
        [0, 2, 4].each_with_index do |d, k|
          hp(dg(rt, s, prog[bar % 4][k], 1), amp: 0.20, pan: 0.35 + k * 0.06)
          sleep 1
        end
        sleep 1
      else
        hp(dg(rt, s, prog[bar % 4][0], 1), amp: 0.14, pan: 0.25)
        sleep 4
      end
    end
  end

  # --- la voz: solo en la segunda mitad --------------------------------------
  live_loop :voz do
    sd 3314
    use_synth :blade
    use_synth_defaults attack: 0.6, sustain: 1.2, release: 1.8, cutoff: 108,
                       vibrato_rate: 4.0
    skip 9
    [k2, k3, k1, k4].each_with_index do |pat, i|
      ph(rt, s, pat.map { |d| d && d - 2 }, 0.5, 2, [0.22, 0.26, 0.24, 0.18][i],
         pan: -0.15)
    end
    skip 3
  end

  # --- campana: el aire de la mezcla ----------------------------------------
  live_loop :campana do
    sd 3315
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.6, cutoff: 118
    sleep 8
    hp(dg(rt, s, 0, 3), amp: 0.18, pan: -0.3); sleep 12
    hp(dg(rt, s, 4, 3), amp: 0.14, pan: 0.3);  sleep 10
    hp(dg(rt, s, 2, 3), amp: 0.22, pan: 0.1);  sleep 12
    hp(dg(rt, s, 0, 3), amp: 0.12, pan: -0.2); sleep 22
  end
end
