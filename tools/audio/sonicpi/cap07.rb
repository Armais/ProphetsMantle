# MANTLE cap. 7 - EL ACEITE. La viuda, el acreedor y la vasija que no se vacia
# hasta que no quedan vasijas (2 Reyes 4:1-7). Es el milagro mas domestico del
# juego, asi que el tema es de camara: SOL MAYOR, salterio y palmas, sin epica.
# El aceite se oye en el arpegio, que va SUMANDO notas seccion a seccion —dos,
# cuatro, seis— como las vasijas que se van llenando.
#
# Periodo: 32 compases de 4/4 a 96 BPM = 80,000 s exactos.
# Forma:   la casa 0-3 | A 4-11 | B 12-19 | A' 20-27 | se acaban las vasijas 28-31
use_bpm 96

rt = 55            # SOL3
s  = sc(:major)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]

a1 = [0, nil, 2, 4, nil, 2, 0, nil]
a2 = [4, nil, 2, 1, nil, 2, nil, nil]
a3 = [2, 4, nil, 5, nil, 4, 2, nil]
a4 = [0, nil, 1, 2, nil, 0, nil, nil]

with_fx :reverb, room: 0.62, mix: 0.28 do
  live_loop :room_pad do
    sd 1701
    use_synth :prophet
    use_synth_defaults attack: 1.0, sustain: 1.8, release: 1.4, cutoff: 106
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.07 : 0.18, pan: -0.3)
      hc(ch.map { |n| n + 12 }, amp: quiet ? 0.04 : 0.08, pan: 0.36) unless quiet
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1702
  use_synth :bass_foundation
  use_synth_defaults attack: 0.04, sustain: 1.2, release: 0.6
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: 0.22)
      sleep 4
    else
      r = dg(rt, s, prog[bar % 4][0], -2)
      hp(r, amp: 0.46); sleep 2
      hp(r + 7, amp: 0.24); sleep 2
    end
  end
end

# --- EL ACEITE: el arpegio suma una voz por seccion ------------------------
with_fx :echo, phase: 0.5, decay: 2.5, mix: 0.2 do
  live_loop :oil do
    sd 1703
    use_synth :kalimba
    use_synth_defaults release: 0.9, pan: 0.28
    32.times do |bar|
      ch = prog[bar % 4]
      n = if pa.include?(bar) then 2
          elsif pb.include?(bar) then 4
          elsif pa2.include?(bar) then 6
          else 1 end
      grados = [ch[0], ch[2], ch[1] + 7, ch[2] + 7, ch[0] + 7, ch[1] + 14][0, n]
      paso = 4.0 / n
      grados.each do |d|
        hp(dg(rt, s, d, 1), amp: out.include?(bar) ? 0.09 : 0.17)
        sleep paso
      end
    end
  end
end

# --- salterio: el tema ------------------------------------------------------
with_fx :reverb, room: 0.66, mix: 0.3 do
  live_loop :psaltery do
    sd 1704
    use_synth :pluck
    use_synth_defaults sustain: 0.2, release: 0.7, coef: 0.32, pan: -0.24
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        ph(rt, s, [a1, a2, a1, a3, a4, a2, a3, a1][(bar - 4) % 8], 0.5, 0,
           pa2.include?(bar) ? 0.40 : 0.28, {})
      else
        sleep 4
      end
    end
  end
end

# --- flauta: solo en B, la voz de la viuda ---------------------------------
with_fx :reverb, room: 0.84, mix: 0.4 do
  live_loop :widow do
    sd 1705
    use_synth :recorder
    use_synth_defaults attack: 0.15, sustain: 0.4, release: 0.6, pan: 0.16
    32.times do |bar|
      if pb.include?(bar)
        ph(rt, s, [a2, a4, a3, a1][(bar - 12) % 4], 1.0, 1, 0.30, {})
      else
        sleep 4
      end
    end
  end
end

# --- palmas: la casa llena de gente ----------------------------------------
live_loop :claps do
  sd 1706
  32.times do |bar|
    if pa2.include?(bar)
      sleep 1
      pk(:perc_snap, amp: 0.15, rate: 0.95, pan: 0.46)
      sleep 1
      pk(:perc_snap2, amp: 0.11, rate: 1.05, pan: -0.42)
      sleep 1
      pk(:perc_snap, amp: 0.09, rate: 1.15, pan: 0.3)
      sleep 1
    elsif pa.include?(bar)
      sleep 1
      pk(:perc_snap, amp: 0.10, rate: 0.95, pan: 0.44)
      sleep 3
    else
      pk(:hat_cab, amp: 0.03, rate: 1.5, pan: 0.4)
      sleep 4
    end
  end
end
