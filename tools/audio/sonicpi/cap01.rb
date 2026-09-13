# MANTLE cap. 1 - EL ARROYO DE QUERIT. Soledad, agua entre las penas y nadie
# en un dia de camino. Dorico en RE (el modo, no el frigio dominante de toda
# la tanda vieja), 80 BPM, ney solo. El graznido de misc_crow es literal:
# los cuervos que lo alimentan (1 Reyes 17:6).
#
# Periodo: 32 compases de 4/4 a 80 BPM = 96,000 s exactos.
# Forma:   intro 0-3 | A 4-11 | B 12-19 | A' 20-27 | salida 28-31
# El arco es el punto: en la intro solo hay drone, en B se va la percusion y
# canta la kalimba, A' es lo mas lleno y la salida se vacia. La tanda vieja
# tenia TODAS las capas sonando los 16 compases (span de 1,2 dB).
use_bpm 80

rt = 50            # RE3
s  = sc(:dorian)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

# Grados de los acordes, por compas del ciclo de 4.
prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

# Frases del ney: 8 pasos de media pulsacion = 1 compas.
n1 = [0, nil, 1, 2, nil, 1, nil, nil]
n2 = [2, nil, 3, 4, nil, 3, 2, nil]
n3 = [4, nil, 3, 2, nil, 1, nil, nil]
n4 = [1, nil, 0, nil, -1, nil, 0, nil]

# --- drone: el agua. CONTINUO y con solape ---------------------------------
# La primera toma lo lanzaba cada 4 compases justo al limite de su release: el
# bucle se moria antes de cerrar y quedaban 2 s a -66 dBFS con 21,5 dB de salto
# en la costura. Ahora entra cada 2 compases (6 s) y vive 9,5, asi que siempre
# hay dos voces solapadas y el final empalma con el principio.
with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :drone do
    sd 1101
    use_synth :hollow
    use_synth_defaults attack: 2, sustain: 4, release: 3.5, cutoff: 92, res: 0.2
    32.times do |bar|
      if bar % 2 == 0
        hp(rt - 12, amp: 0.11, pan: -0.55)
        hp(rt, amp: 0.13, pan: 0.6)
      end
      sleep 4
    end
  end
end

# --- pads ------------------------------------------------------------------
with_fx :reverb, room: 0.82, mix: 0.38 do
  live_loop :pads do
    sd 1102
    use_synth :prophet
    # cutoff 108, no 82: las diez pistas viejas tenian el 0-1% de la energia
    # por encima de 2 kHz y por eso sonaban tapadas.
    use_synth_defaults attack: 1.4, sustain: 1.8, release: 1.6, cutoff: 108
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      if intro.include?(bar) || out.include?(bar)
        # A media voz, no en silencio: es lo que cose el final con el principio.
        hc(ch, amp: 0.05, pan: -0.25)
      else
        hc(ch, amp: 0.24, pan: -0.3)
        hc(ch.map { |n| n + 12 }, amp: 0.11, pan: 0.35)
      end
      sleep 4
    end
  end
end

# --- bajo ------------------------------------------------------------------
live_loop :bass do
  sd 1103
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 2.4, release: 1.0, amp: 0.42
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      sleep 4
    else
      hp(dg(rt, s, prog[bar % 4][0], -1), pan: 0.0)
      sleep 2
      hp(dg(rt, s, prog[bar % 4][0], -1) + 7, amp: 0.18) if pa2.include?(bar)
      sleep 2
    end
  end
end

# --- tambor de marco: muy espaciado, y callado en B ------------------------
live_loop :frame do
  sd 1104
  32.times do |bar|
    if pa.include?(bar) || pa2.include?(bar)
      big = pa2.include?(bar)
      pk(:tabla_dhec, amp: big ? 0.60 : 0.40, rate: 0.72, pan: -0.28)
      sleep 2.5
      pk(:tabla_ghe5, amp: big ? 0.34 : 0.22, rate: 0.8, pan: -0.2)
      sleep 1.5
    else
      sleep 4
    end
  end
end

# --- cabasa: en A floja y en A' al doble -----------------------------------
# Tambien suena, muy de lejos, en intro y salida: con la cabasa solo en 8 de
# los 32 compases, la pista se quedaba en el 2% de energia sobre 2 kHz y sonaba
# tapada (centroide de 254 Hz).
live_loop :shaker do
  sd 1105
  32.times do |bar|
    if pa2.include?(bar)
      4.times do
        pk(:hat_cab, amp: 0.15, rate: 1.25, pan: 0.5)
        sleep 0.5
        pk(:hat_cab, amp: 0.09, rate: 1.4, pan: 0.42)
        sleep 0.5
      end
    elsif pa.include?(bar)
      4.times do
        pk(:hat_cab, amp: 0.075, rate: 1.3, pan: 0.46)
        sleep 1
      end
    else
      pk(:hat_cab, amp: 0.02, rate: 1.45, pan: 0.4)
      sleep 2
      pk(:hat_cab, amp: 0.015, rate: 1.5, pan: -0.35)
      sleep 2
    end
  end
end

# --- ney: canta en A y A', calla en B --------------------------------------
with_fx :reverb, room: 0.88, mix: 0.42 do
  live_loop :ney do
    sd 1106
    use_synth :recorder
    use_synth_defaults attack: 0.14, sustain: 0.26, release: 0.5, pan: -0.18
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [n1, n2, n1, n3, n4, n2, n3, n1][(bar - 4) % 8]
        ph(rt, s, p, 0.5, 1, pa2.include?(bar) ? 0.48 : 0.32, {})
      else
        sleep 4
      end
    end
  end
end

# --- kalimba: el agua entre las penas. Solo en B, aguda ---------------------
with_fx :echo, phase: 0.75, decay: 3, mix: 0.22 do
  live_loop :kalimba do
    sd 1107
    use_synth :kalimba
    use_synth_defaults release: 1.2, pan: 0.32
    32.times do |bar|
      ch = prog[bar % 4]
      if pb.include?(bar)
        [ch[0], ch[2], ch[1] + 7, ch[2]].each do |d|
          hp(dg(rt, s, d, 2), amp: 0.21)
          sleep 1
        end
      elsif out.include?(bar) || intro.include?(bar)
        hp(dg(rt, s, ch[0], 2), amp: 0.07)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- los cuervos: dos graznidos lejanos en todo el bucle -------------------
with_fx :reverb, room: 0.95, mix: 0.55 do
  live_loop :crows do
    sd 1108
    32.times do |bar|
      if bar == 2
        pk(:misc_crow, amp: 0.16, rate: 0.85, pan: -0.65)
      elsif bar == 29
        pk(:misc_crow, amp: 0.11, rate: 0.78, pan: 0.7)
      end
      sleep 4
    end
  end
end
