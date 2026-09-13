# MANTLE cap. 2 - SAREPTA. La casa de la viuda: hambre, un punado de harina y
# hospitalidad. Eolico en LA, 88 BPM, oud (cuerda pulsada) de solista y
# darbuka en la palma de la mano. Calido y de puertas adentro, al reves del
# cap. 1, que es puro exterior vacio.
#
# Periodo: 32 compases de 4/4 a 88 BPM = 87,2727 s.
# Forma:   intro 0-3 | A 4-11 | B 12-19 | A' 20-27 | salida 28-31
use_bpm 88

rt = 57            # LA3
s  = sc(:aeolian)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

o1 = [0, nil, 2, nil, 1, 0, nil, nil]
o2 = [2, 3, nil, 4, nil, 3, 2, nil]
o3 = [4, nil, 3, 4, nil, 5, nil, 4]
o4 = [2, nil, 1, nil, 0, nil, nil, nil]

# --- organo de casa: el lecho tibio ----------------------------------------
with_fx :reverb, room: 0.7, mix: 0.34 do
  live_loop :room_tone do
    sd 1201
    use_synth :organ_tonewheel
    use_synth_defaults attack: 1.6, sustain: 2.2, release: 1.8, amp: 0.12
    32.times do |bar|
      # Nunca calla del todo: es el lecho que cose el final con el principio.
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (intro.include?(bar) || out.include?(bar)) ? 0.09 : 0.13, pan: -0.4)
      hc([ch[0] + 12], amp: 0.06, pan: 0.45)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1202
  use_synth :bass_foundation
  use_synth_defaults attack: 0.04, sustain: 1.4, release: 0.7, amp: 0.38
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      sleep 4
    else
      r = dg(rt, s, prog[bar % 4][0], -1)
      hp(r)
      sleep 2
      hp(pa2.include?(bar) ? r + 7 : r, amp: 0.2)
      sleep 2
    end
  end
end

# --- oud: el solista. Canta en A y A' --------------------------------------
with_fx :reverb, room: 0.68, mix: 0.3 do
  live_loop :oud do
    sd 1203
    use_synth :pluck
    use_synth_defaults sustain: 0.25, release: 0.9, coef: 0.55, pan: -0.22
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        ph(rt, s, [o1, o2, o1, o3, o4, o2, o3, o1][(bar - 4) % 8], 0.5, 0,
           pa2.include?(bar) ? 0.42 : 0.34, {})
      else
        sleep 4
      end
    end
  end
end

# --- armonicos de cuerda: brillo, y el contracanto de B --------------------
with_fx :echo, phase: 0.5, decay: 2.5, mix: 0.24 do
  live_loop :harm do
    sd 1204
    32.times do |bar|
      if pb.include?(bar)
        pk(:guit_harmonics, amp: 0.20, rate: 1.0, pan: 0.4)
        sleep 2
        pk(:guit_harmonics, amp: 0.12, rate: 1.5, pan: 0.28)
        sleep 2
      elsif intro.include?(bar) || out.include?(bar)
        pk(:guit_harmonics, amp: 0.11, rate: bar.even? ? 0.75 : 1.5, pan: 0.35)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- flauta humilde: solo en B, donde calla el oud -------------------------
with_fx :reverb, room: 0.86, mix: 0.42 do
  live_loop :flute do
    sd 1205
    use_synth :hollow
    use_synth_defaults attack: 0.2, sustain: 0.5, release: 0.8, cutoff: 96, pan: -0.12
    32.times do |bar|
      if pb.include?(bar)
        ph(rt, s, [o2, o4, o3, o1][(bar - 12) % 4], 1.0, 1, 0.26, {})
      else
        sleep 4
      end
    end
  end
end

# --- darbuka: la palma en el borde del parche ------------------------------
live_loop :darbuka do
  sd 1206
  32.times do |bar|
    if pa.include?(bar) || pa2.include?(bar)
      big = pa2.include?(bar)
      pk(:tabla_na, amp: big ? 0.42 : 0.30, rate: 0.95, pan: -0.3)
      sleep 1
      pk(:tabla_te1, amp: 0.16, rate: 1.05, pan: 0.22)
      sleep 0.5
      pk(:tabla_te1, amp: 0.11, rate: 1.15, pan: 0.3)
      sleep 0.5
      pk(:tabla_na_s, amp: big ? 0.34 : 0.24, rate: 0.9, pan: -0.18)
      sleep 1
      pk(:tabla_ke1, amp: 0.13, rate: 1.3, pan: 0.4)
      sleep 1
    else
      sleep 4
    end
  end
end

# --- palmas: la densidad extra de A' ---------------------------------------
live_loop :claps do
  sd 1207
  32.times do |bar|
    if pa2.include?(bar)
      sleep 1
      pk(:perc_snap, amp: 0.13, rate: 0.9, pan: 0.5)
      sleep 2
      pk(:perc_snap2, amp: 0.10, rate: 1.0, pan: -0.45)
      sleep 1
    else
      sleep 4
    end
  end
end
