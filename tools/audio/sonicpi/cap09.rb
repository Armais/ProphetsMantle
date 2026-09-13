# MANTLE cap. 9 - EL JORDAN. Naaman, general de Siria y leproso, que viene con
# caballos y carros y se va limpio despues de zambullirse SIETE veces en un rio
# que le parecia poca cosa (2 Reyes 5). El tema va del orgullo al agua:
# mixolidio en LA, que es mayor pero con la septima baja —suena noble y a la
# vez sin cerrar del todo—, y en B hay siete zambullidas contadas, cada una mas
# limpia que la anterior.
#
# Periodo: 32 compases de 4/4 a 104 BPM = 73,8462 s.
# Forma:   el rio 0-3 | A el general 4-11 | B las siete 12-19 | A' limpio 20-27 | 28-31
use_bpm 104

rt = 57            # LA3
s  = sc(:mixo)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [4, 6, 1], [3, 5, 0], [0, 2, 4]]

r1 = [0, nil, 2, 4, nil, 2, nil, nil]
r2 = [4, 5, nil, 4, nil, 2, 0, nil]
r3 = [2, nil, 4, nil, 5, nil, 4, 2]
r4 = [0, nil, 6, nil, 0, nil, nil, nil]

# --- el agua: corcheas que no paran en toda la pieza -----------------------
with_fx :echo, phase: 0.25, decay: 2, mix: 0.18 do
  live_loop :river do
    sd 1901
    32.times do |bar|
      a = if pa2.include?(bar) then 0.075
          elsif pa.include?(bar) || pb.include?(bar) then 0.055
          else 0.03 end
      8.times do |i|
        pk(:hat_cab, amp: i.even? ? a : a * 0.6, rate: 1.3 + 0.1 * (i % 3),
           pan: i.even? ? 0.5 : -0.42)
        sleep 0.5
      end
    end
  end
end

with_fx :reverb, room: 0.8, mix: 0.36 do
  live_loop :pads do
    sd 1902
    use_synth :prophet
    use_synth_defaults attack: 1.0, sustain: 1.8, release: 1.4, cutoff: 108
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.06 : 0.21, pan: -0.33)
      hc(ch.map { |n| n + 12 }, amp: 0.09, pan: 0.4) unless quiet
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1903
  use_synth :bass_foundation
  use_synth_defaults attack: 0.04, sustain: 1.0, release: 0.5
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: 0.20)
      sleep 4
    else
      r = dg(rt, s, prog[bar % 4][0], -2)
      hp(r, amp: 0.44); sleep 1.5
      hp(r + 7, amp: 0.20); sleep 0.5
      hp(r, amp: 0.26); sleep 2
    end
  end
end

# --- el general: caballos y carros. Solo en A, y se va cuando se humilla ---
live_loop :horses do
  sd 1904
  32.times do |bar|
    if pa.include?(bar)
      pk(:drum_tom_mid_hard, amp: 0.30, rate: 0.9, pan: -0.3)
      sleep 1
      pk(:drum_tom_lo_soft, amp: 0.18, rate: 0.85, pan: 0.25)
      sleep 0.5
      pk(:drum_tom_mid_soft, amp: 0.14, rate: 1.0, pan: -0.2)
      sleep 0.5
      pk(:drum_tom_lo_hard, amp: 0.26, rate: 0.8, pan: 0.3)
      sleep 2
    else
      sleep 4
    end
  end
end

# --- LAS SIETE ZAMBULLIDAS: una por compas, cada una mas limpia ------------
with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :dips do
    sd 1905
    32.times do |bar|
      if pb.include?(bar) && bar < 19
        n = bar - 12                       # 0..6, las siete veces
        pk(:perc_swash, amp: 0.28, rate: 0.8 + 0.07 * n, pan: (n.even? ? -0.4 : 0.42))
        sleep 2
        pk(:elec_chime, amp: 0.05 + 0.02 * n, rate: 1.2, pan: 0.3)
        sleep 2
      else
        sleep 4
      end
    end
  end
end

# --- la flauta: el tema, humilde en A y abierto en A' ---------------------
with_fx :reverb, room: 0.88, mix: 0.44 do
  live_loop :flute do
    sd 1906
    use_synth :recorder
    use_synth_defaults attack: 0.12, sustain: 0.26, release: 0.5,
                       vibrato_rate: 5.0, vibrato_depth: 0.1, pan: -0.18
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [r1, r2, r1, r3, r4, r2, r3, r1][(bar - 4) % 8]
        ph(rt, s, p, 0.5, pa2.include?(bar) ? 1 : 0, pa2.include?(bar) ? 0.44 : 0.30, {})
      else
        sleep 4
      end
    end
  end
end
