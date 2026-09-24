# MANTLE - PROLOGO: "EL CIELO CERRADO". Elias ante Acab: "Vive Jehova, Dios de
# Israel, en cuya presencia estoy, que no habra lluvia ni rocio en estos anos
# sino por mi palabra" (1 Reyes 17:1). Es un juramento, no una batalla: la
# pieza esta casi vacia y lo que suena es el aire seco y una voz sola.
#
# La ultima seccion es la sequia: las capas se van yendo y no vuelven, y solo
# queda el drone. Cuando arranca el capitulo 1, el cauce de Querit ya suena a
# agua por contraste.
#
# Frigia en MI. Periodo: 24 compases de 4/4 a 66 BPM = 87,2727 s.
use_bpm 66

rt = 52
s  = sc(:phryg)
antes   = (0...6)
juram   = (6...14)
sequia  = (14...24)

prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]
p1 = [0, nil, 1, nil, 0, nil, nil, nil]
p2 = [2, nil, 1, 0, nil, 1, nil, nil]
p3 = [4, nil, 3, 2, nil, 1, nil, 0]

with_fx :reverb, room: 0.96, mix: 0.6 do
  live_loop :aire_seco do
    sd 9001
    use_synth :dark_ambience
    use_synth_defaults attack: 4, sustain: 6, release: 5, cutoff: 82, res: 0.15
    24.times do |bar|
      if bar % 2 == 0
        hp(rt - 12, amp: 0.18, pan: -0.58)
        hp(dg(rt, s, 4, -1), amp: 0.11, pan: 0.6)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pads do
    sd 9002
    use_synth :prophet
    use_synth_defaults attack: 2.2, sustain: 2.2, release: 2.2, cutoff: 100
    24.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      a = if juram.include?(bar) then 0.20
          elsif antes.include?(bar) then 0.11
          elsif bar < 20 then 0.13
          else 0.09 end
      hc(ch, amp: a, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.07, pan: 0.4) if juram.include?(bar)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 9003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.1, sustain: 3.0, release: 1.4, amp: 0.34
  24.times do |bar|
    if bar >= 4 && bar < 20 && bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -1))
    end
    sleep 4
  end
end

# --- el juramento: una voz sola, y despues nada --------------------------
with_fx :reverb, room: 0.92, mix: 0.52 do
  live_loop :voz do
    sd 9004
    use_synth :recorder
    use_synth_defaults attack: 0.25, sustain: 0.4, release: 0.9, pan: -0.18
    24.times do |bar|
      if bar >= 6 && bar < 10
        # Responde al violin en la primera mitad del juramento y despues calla:
        # antes cantaba los ocho compases y no dejaba sitio a nadie.
        ph(rt, s, [p1, p2, p1, p3][bar - 6], 0.5, 1, 0.32, {})
      elsif bar >= 14 && bar < 16
        hp(dg(rt, s, 0, 1), amp: 0.16, sustain: 3, release: 1.5)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- tambor de marco: solo bajo el juramento ------------------------------
live_loop :marco do
  sd 9005
  24.times do |bar|
    if juram.include?(bar)
      pk(:tabla_dhec, amp: 0.34, rate: 0.68, pan: -0.3)
      sleep 3
      pk(:tabla_ghe5, amp: 0.18, rate: 0.8, pan: 0.25)
      sleep 1
    elsif bar == 14
      pk(:drum_splash_soft, amp: 0.20, rate: 0.7, pan: 0.3)
      sleep 4
    else
      sleep 4
    end
  end
end

with_fx :echo, phase: 2.0, decay: 6, mix: 0.3 do
  live_loop :campana do
    sd 9006
    use_synth :dull_bell
    use_synth_defaults release: 3.2, pan: 0.38
    24.times do |bar|
      hp(dg(rt, s, [0, 1, 0, 5][(bar / 2) % 4], 1),
         amp: sequia.include?(bar) ? 0.13 : 0.15) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :polvo do
  sd 9007
  24.times do |bar|
    pk(:hat_cab, amp: sequia.include?(bar) ? 0.02 : 0.035, rate: 1.5, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.018, rate: 1.6, pan: -0.4)
    sleep 2
  end
end

# --- el violin: la voz del prologo ------------------------------------------
# En Sonic Pi NINGUN synth suena a violin de fabrica, y el unico que trae
# vibrato de verdad es :blade (comprobado en los .scsyndef: :dsaw, :saw, :tri
# y :recorder aceptan `vibrato_rate` sin rechistar pero lo IGNORAN, porque no
# existe en su synthdef). Asi que el violin son DOS capas ligadas a la vez:
#
#   :blade  el cuerpo y el vibrato, que entra medio segundo DESPUES del
#           ataque (vibrato_delay): ningun violinista vibra desde la primera
#           muestra.
#   :dsaw   por debajo y a un tercio de volumen, el rascado del arco: dos
#           sierras apenas desafinadas, con el ataque aun mas lento.
#
# Y lo que mas lo aleja de un teclado: el LEGATO. Las notas de una frase no se
# reatacan; se arranca el nodo una vez y se le cambia la altura con
# `control ... note:`, y `note_slide` hace el porta del dedo sobre la cuerda.
#
# Canta en tres sitios y calla en el resto: entra solo (compases 2-5), le
# responde el ney (6-9), se lleva el climax (10-13) y cierra la pieza solo
# sobre el drone de la sequia (16-17).
with_fx :reverb, room: 0.94, mix: 0.58 do
  with_fx :echo, phase: 1.5, decay: 4, mix: 0.12 do
    live_loop :violin do
      sd 9008

      # Un segmento LIGADO: [grado, pulsos]. El primero ataca; los demas solo
      # mueven la altura de los dos nodos que ya suenan.
      arco = lambda do |seg, a, pan|
        tot = seg.map { |e| e[1] }.sum
        n0  = dg(rt, s, seg[0][0], 1)
        va  = a * (0.9 + rand * 0.2)
        cuerpo = synth :blade, note: n0, amp: va, pan: pan,
                       attack: 0.34, decay: 0.3, decay_level: 0.92,
                       sustain: tot - 0.4, release: 1.3, cutoff: 101,
                       note_slide: 0.085,
                       vibrato_rate: 5.5, vibrato_depth: 0.25, vibrato_delay: 0.45
        crin   = synth :dsaw, note: n0, amp: va * 0.36, pan: pan + 0.1,
                       attack: 0.46, decay: 0.3, decay_level: 0.9,
                       sustain: tot - 0.5, release: 1.2, cutoff: 87, detune: 0.07,
                       note_slide: 0.085
        sleep seg[0][1]
        seg.drop(1).each do |g, d|
          n = dg(rt, s, g, 1)
          control cuerpo, note: n
          control crin,   note: n
          sleep d
        end
      end

      skip 2                                              # 0-1: solo el aire
      arco.call([[4, 2], [3, 1], [2, 1], [0, 3], [1, 1]], 0.24, -0.12)   # 2-3
      arco.call([[2, 2], [1, 2], [0, 4]],                 0.21, -0.05)   # 4-5
      skip 4                                              # 6-9: canta el ney
      arco.call([[5, 1], [6, 1], [7, 2], [9, 2], [7, 2]], 0.33, -0.14)   # 10-11
      arco.call([[6, 2], [5, 2], [2, 2], [0, 2]],         0.30, -0.08)   # 12-13
      skip 2                                              # 14-15
      arco.call([[4, 4], [0, 4]],                         0.19, -0.10)   # 16-17
      skip 6                                              # 18-23: la sequia
    end
  end
end
