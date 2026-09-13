# MANTLE cap. 8 - LA SUNAMITA. La mujer que hospeda al profeta, el hijo que le
# nace, el hijo que se le muere en el regazo y Eliseo tendido sobre el nino
# hasta que estornuda siete veces (2 Reyes 4:8-37).
#
# El LATIDO es el tema: un pulso grave y regular que en la seccion B se
# ralentiza y se para —ahi no hay percusion ninguna, y es el unico silencio
# ritmico del juego— y que vuelve en A', ya en MAYOR. Menor melodico en RE
# para el duelo, mayor para la vuelta: la misma melodia, otro final.
#
# Periodo: 32 compases de 4/4 a 76 BPM = 101,0526 s.
# Forma:   la casa 0-3 | A el nino 4-11 | B la muerte 12-19 | A' vuelve 20-27 | 28-31
use_bpm 76

rt = 50            # RE3
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

define :esc do |bar|
  (bar >= 20) ? sc(:major) : sc(:melmin)
end

prog = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [4, 6, 1]]

s1 = [0, nil, 2, nil, 4, nil, 2, nil]
s2 = [4, nil, 3, 2, nil, 1, nil, nil]
s3 = [2, nil, 4, 5, nil, 4, 2, nil]
s4 = [1, nil, 0, nil, nil, nil, nil, nil]

# --- EL LATIDO: se para en B ------------------------------------------------
live_loop :heart do
  sd 1801
  32.times do |bar|
    if pb.include?(bar)
      # Se va apagando hasta parar del todo en el compas 16.
      if bar < 16
        pk(:drum_bass_soft, amp: 0.30 - 0.08 * (bar - 12), rate: 0.55, pan: 0.0)
        sleep 2
        pk(:drum_bass_soft, amp: 0.18 - 0.05 * (bar - 12), rate: 0.55, pan: 0.0)
        sleep 2
      else
        sleep 4
      end
    elsif intro.include?(bar) || out.include?(bar)
      pk(:drum_bass_soft, amp: 0.16, rate: 0.55, pan: 0.0)
      sleep 2
      pk(:drum_bass_soft, amp: 0.10, rate: 0.55, pan: 0.0)
      sleep 2
    else
      fuerte = pa2.include?(bar)
      pk(:drum_bass_soft, amp: fuerte ? 0.38 : 0.26, rate: 0.55, pan: 0.0)
      sleep 1.5
      pk(:drum_bass_soft, amp: fuerte ? 0.22 : 0.15, rate: 0.55, pan: 0.0)
      sleep 2.5
    end
  end
end

with_fx :reverb, room: 0.88, mix: 0.42 do
  live_loop :pads do
    sd 1802
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 2.0, release: 1.8, cutoff: 104
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, esc(bar), d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.06 : (pb.include?(bar) ? 0.14 : 0.22), pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.38) unless quiet
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1803
  use_synth :bass_foundation
  use_synth_defaults attack: 0.08, sustain: 2.6, release: 1.2
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, esc(bar), prog[bar % 4][0], -2), amp: 0.20)
      sleep 4
    else
      hp(dg(rt, esc(bar), prog[bar % 4][0], -2), amp: 0.42)
      sleep 4
    end
  end
end

# --- la voz: ney agudo, sin palabras ---------------------------------------
with_fx :reverb, room: 0.9, mix: 0.48 do
  live_loop :voice do
    sd 1804
    use_synth :recorder
    use_synth_defaults attack: 0.2, sustain: 0.35, release: 0.8,
                       vibrato_rate: 4.8, vibrato_depth: 0.09, pan: -0.16
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [s1, s2, s1, s3, s4, s2, s3, s1][(bar - 4) % 8]
        ph(rt, esc(bar), p, 0.5, 1, pa2.include?(bar) ? 0.44 : 0.32, {})
      elsif pb.include?(bar) && bar >= 16
        # Sobre el silencio del latido queda una sola nota tenida.
        hp(dg(rt, esc(bar), 0, 1), amp: 0.20, sustain: 3, release: 1)
        sleep 4
      else
        sleep 4
      end
    end
  end
end

# --- campana: siete golpes en A', los siete estornudos del nino ------------
with_fx :echo, phase: 0.75, decay: 3, mix: 0.24 do
  live_loop :bell do
    sd 1805
    use_synth :pretty_bell
    use_synth_defaults release: 1.8, pan: 0.35
    32.times do |bar|
      if bar >= 20 && bar < 27
        hp(dg(rt, sc(:major), [0, 2, 4, 2, 5, 4, 0][bar - 20], 2), amp: 0.16)
      elsif intro.include?(bar) || out.include?(bar)
        hp(dg(rt, esc(bar), 0, 2), amp: 0.06)
      end
      sleep 4
    end
  end
end

# --- aire: hilo de cabasa, mas presente cuanto mas llena la seccion --------
live_loop :air do
  sd 1806
  32.times do |bar|
    a = if pa2.include?(bar) then 0.09
        elsif pa.include?(bar) then 0.05
        elsif pb.include?(bar) then 0.015
        else 0.025 end
    2.times do
      pk(:hat_cab, amp: a, rate: 1.35, pan: 0.48)
      sleep 2
    end
  end
end
