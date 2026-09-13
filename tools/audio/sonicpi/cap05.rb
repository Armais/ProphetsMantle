# MANTLE cap. 5 - EL LLAMADO. Eliseo arando con doce yuntas cuando Elias le
# echa el manto encima (1 Reyes 19:19). Praderas, luz y una vocacion que llega
# sin avisar. LIDIO en FA: la cuarta aumentada es lo que hace que suene abierto
# y esperanzado, al contrario que los cuatro capitulos anteriores. El cencerro
# lejano son los bueyes, como los cuervos eran los del arroyo.
#
# Periodo: 32 compases de 4/4 a 92 BPM = 83,4783 s.
# Forma:   campo 0-3 | A flauta doble 4-11 | B el manto 12-19 | A' 20-27 | 28-31
use_bpm 92

rt = 53            # FA3
s  = sc(:lydian)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [4, 6, 1], [3, 5, 0], [1, 3, 5]]

m1 = [0, nil, 2, 4, nil, 2, nil, nil]
m2 = [4, nil, 5, 4, nil, 2, 0, nil]
m3 = [2, 4, nil, 6, nil, 4, 2, nil]
m4 = [0, nil, 1, 2, nil, 4, nil, nil]

with_fx :reverb, room: 0.8, mix: 0.36 do
  live_loop :pads do
    sd 1501
    use_synth :prophet
    use_synth_defaults attack: 1.1, sustain: 2.0, release: 1.5, cutoff: 112
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.11 : 0.22, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: quiet ? 0.05 : 0.10, pan: 0.38)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1502
  use_synth :bass_foundation
  use_synth_defaults attack: 0.05, sustain: 1.6, release: 0.8, amp: 0.38
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      sleep 4
    else
      r = dg(rt, s, prog[bar % 4][0], -1)
      hp(r)
      sleep 2
      hp(r + (pa2.include?(bar) ? 7 : 0), amp: 0.2)
      sleep 2
    end
  end
end

# --- flauta doble: dos canas en terceras, una por lado ---------------------
with_fx :reverb, room: 0.86, mix: 0.4 do
  live_loop :flutes do
    sd 1503
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [m1, m2, m1, m3, m4, m2, m3, m1][(bar - 4) % 8]
        av = pa2.include?(bar) ? 0.36 : 0.30
        p.each do |d|
          if d
            use_synth :recorder
            hp(dg(rt, s, d, 1), amp: av, pan: -0.3, attack: 0.12,
               sustain: 0.24, release: 0.45)
            use_synth :hollow
            hp(dg(rt, s, d + 2, 1), amp: av * 0.5, pan: 0.34, attack: 0.16,
               sustain: 0.3, release: 0.5, cutoff: 104)
          end
          sleep 0.5
        end
      else
        sleep 4
      end
    end
  end
end

# --- el manto: la cuerda que solo suena en B, grave y decidida ------------
with_fx :reverb, room: 0.88, mix: 0.44 do
  live_loop :mantle do
    sd 1504
    use_synth :pluck
    use_synth_defaults sustain: 0.4, release: 1.4, coef: 0.5, pan: 0.2
    32.times do |bar|
      if pb.include?(bar)
        ch = prog[bar % 4]
        hp(dg(rt, s, ch[0], 0), amp: 0.30)
        sleep 2
        hp(dg(rt, s, ch[2], 0), amp: 0.22)
        sleep 2
      else
        sleep 4
      end
    end
  end
end

# --- shaker de hierba: ligero y presente casi siempre ---------------------
live_loop :grass do
  sd 1505
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      pk(:hat_cab, amp: 0.04, rate: 1.5, pan: 0.45)
      sleep 2
      pk(:hat_cab, amp: 0.03, rate: 1.6, pan: -0.4)
      sleep 2
    else
      n = pa2.include?(bar) ? 8 : 4
      n.times do
        pk(:hat_cab, amp: pa2.include?(bar) ? 0.08 : 0.055, rate: 1.4, pan: 0.5)
        sleep 4.0 / n
      end
    end
  end
end

# --- las doce yuntas: un cencerro muy lejano ------------------------------
with_fx :reverb, room: 0.94, mix: 0.6 do
  live_loop :oxen do
    sd 1506
    32.times do |bar|
      pk(:drum_cowbell, amp: 0.07, rate: 0.7, pan: -0.55) if bar == 1 || bar == 25
      pk(:drum_cowbell, amp: 0.05, rate: 0.66, pan: 0.6) if bar == 30
      sleep 4
    end
  end
end
