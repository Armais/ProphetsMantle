# MANTLE cap. 3 - DIALOGO Y CARTA DE APERTURA (Carmelo).
# "?Hasta cuando claudicareis entre dos pensamientos?". Pieza propia, no el
# tema desnudo: el tema es lira (:pluck) con :blade sobre :prophet y tambor
# marcial; esto es cuerda frotada (:dsaw) sobre acordes de :tri con un
# salterio agudo, y sin percusion: debajo hay texto que leer.
# Frigia dominante en MI, el modo del capitulo.
#
# Periodo: 16 compases de 4/4 a 80 BPM = 48,000 s exactos.
# Forma: 0-2 se abre | 3-8 la cuerda pregunta | 9-13 el salterio responde |
#        14-15 se apaga.
use_bpm 80

rt = 52            # MI3
s  = sc(:phrygdom)

prog = [[0, 2, 4], [1, 3, 5], [4, 6, 1], [0, 2, 4]]

# La pregunta: sube a la 2a menor y se queda ahi colgada, que es el sonido de
# este modo y de este capitulo.
q1 = [0, nil, nil, 1, nil, nil, 0, nil]
q2 = [3, nil, nil, 4, nil, nil, 3, nil]
q3 = [4, nil, 5, nil, 4, nil, 3, nil]
q4 = [1, nil, nil, nil, 0, nil, nil, nil]

with_fx :reverb, room: 0.88, mix: 0.5 do
  live_loop :fondo do
    sd 3321
    use_synth :tri
    use_synth_defaults attack: 1.4, sustain: 1.4, release: 2.0, cutoff: 88, res: 0.1
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
    sd 3322
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.0, release: 1.2
    raiz = [0, 1, 4, 0]
    16.times do |bar|
      hp(dg(rt, s, raiz[bar % 4], 0) - 12, amp: bar < 3 ? 0.12 : 0.18)
      sleep 4
    end
  end

  # --- la cuerda frotada: pregunta en la primera mitad ----------------------
  live_loop :cuerda do
    sd 3323
    use_synth :dsaw
    use_synth_defaults attack: 0.35, sustain: 0.5, release: 1.1, detune: 0.09,
                       cutoff: 92, res: 0.2
    skip 3
    [q1, q2, q3, q4, q2, q3].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 1, [0.26, 0.30, 0.34, 0.24, 0.28, 0.22][i], pan: 0.2)
    end
    skip 7
  end

  # --- el salterio: responde en la segunda, y es el aire de la mezcla -------
  live_loop :salterio do
    sd 3324
    use_synth :kalimba
    use_synth_defaults release: 1.2
    16.times do |bar|
      if bar < 9 || bar > 13
        if bar == 15
          hp(dg(rt, s, 0, 2), amp: 0.12, pan: 0.3)
          sleep 4
        else
          skip 1
        end
      else
        [0, 2, 1, 2].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], k == 3 ? 4 : 3),
             amp: 0.26 - k * 0.03, pan: 0.2 + k * 0.1)
          sleep 1
        end
      end
    end
  end

  # --- campana grave: marca la entrada de cada seccion ----------------------
  live_loop :campana do
    sd 3325
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.4, cutoff: 116
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: -0.25); sleep 8
    hp(dg(rt, s, 4, 3), amp: 0.16, pan: 0.25);  sleep 12
    hp(dg(rt, s, 0, 3), amp: 0.24, pan: 0.1);   sleep 8
    hp(dg(rt, s, 1, 4), amp: 0.18, pan: -0.3);  sleep 16
    hp(dg(rt, s, 4, 3), amp: 0.20, pan: 0.2);   sleep 12
    hp(dg(rt, s, 0, 4), amp: 0.14, pan: -0.1);  sleep 8
  end
end
