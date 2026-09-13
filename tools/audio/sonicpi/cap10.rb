# MANTLE cap. 10 - SAMARIA SITIADA. El hambre del cerco, los cuatro leprosos a
# la puerta y el ejercito sirio que huye de noche porque OYE estruendo de
# carros y caballos que no existen (2 Reyes 6:24-7:20). El capitulo final del
# juego, y el unico que va de la opresion a la liberacion: frigia en MI
# mientras dura el cerco, MAYOR cuando el campamento queda vacio, y una marcha
# que acelera de negras a corcheas.
#
# Periodo: 32 compases de 4/4 a 112 BPM = 68,5714 s.
# Forma:   el cerco 0-3 | A el hambre 4-11 | B los carros 12-19 |
#          A' el campamento vacio 20-27 | salida 28-31
use_bpm 112

rt = 52            # MI3
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

define :esc do |bar|
  (bar >= 20) ? sc(:major) : sc(:phryg)
end

prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]

w1 = [0, nil, 1, nil, 0, nil, nil, nil]
w2 = [0, 1, 2, nil, 1, 0, nil, nil]
w3 = [4, nil, 3, 2, nil, 1, 0, nil]
w4 = [0, 2, 4, nil, 5, nil, 4, nil]

# --- la marcha: negras en el cerco, corcheas cuando el miedo cambia de bando
live_loop :march do
  sd 2001
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      pk(:drum_tom_lo_soft, amp: 0.24, rate: 0.75, pan: -0.3)
      sleep 2
      pk(:drum_tom_mid_soft, amp: 0.16, rate: 0.85, pan: 0.25)
      sleep 2
    elsif pb.include?(bar)
      # Los carros: la marcha se desboca. Lo que oye el sirio y no existe.
      8.times do |i|
        pk(i.even? ? :drum_tom_lo_hard : :drum_tom_mid_hard,
           amp: 0.22 + 0.02 * (bar - 12), rate: 0.8 + 0.04 * (i % 4),
           pan: i.even? ? -0.35 : 0.38)
        sleep 0.5
      end
    else
      big = pa2.include?(bar)
      pk(:drum_tom_lo_hard, amp: big ? 0.44 : 0.32, rate: 0.78, pan: -0.3)
      sleep 1
      pk(:drum_tom_mid_soft, amp: 0.16, rate: 0.9, pan: 0.28)
      sleep 1
      pk(:drum_tom_lo_hard, amp: big ? 0.36 : 0.26, rate: 0.75, pan: -0.22)
      sleep 1
      pk(:drum_bass_hard, amp: big ? 0.24 : 0.16, rate: 0.9, pan: 0.0)
      sleep 1
    end
  end
end

with_fx :reverb, room: 0.85, mix: 0.4 do
  live_loop :pads do
    sd 2002
    use_synth :prophet
    use_synth_defaults attack: 0.8, sustain: 2.0, release: 1.2, cutoff: 110
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, esc(bar), d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.11 : 0.22, pan: -0.35)
      hc(ch.map { |n| n + 12 }, amp: 0.10, pan: 0.42) unless quiet
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.03, sustain: 0.8, release: 0.4
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, esc(bar), prog[bar % 4][0], -2), amp: 0.22)
      sleep 4
    else
      r = dg(rt, esc(bar), prog[bar % 4][0], -2)
      4.times do |i|
        hp(r, amp: i.even? ? 0.48 : 0.24)
        sleep 1
      end
    end
  end
end

# --- el coro: entra cuando el campamento aparece vacio --------------------
with_fx :reverb, room: 0.95, mix: 0.58 do
  live_loop :choir do
    sd 2004
    32.times do |bar|
      if pa2.include?(bar) && bar % 2 == 0
        pk(:ambi_choir, amp: 0.30, rate: 0.75, pan: -0.5)
        pk(:ambi_choir, amp: 0.22, rate: 0.752, pan: 0.55)
      elsif bar == 12
        pk(:ambi_dark_woosh, amp: 0.26, rate: 0.6, pan: 0.4)
      end
      sleep 4
    end
  end
end

# --- metal: el tema, duro en frigia y abierto en mayor -------------------
with_fx :reverb, room: 0.82, mix: 0.38 do
  live_loop :horn do
    sd 2005
    use_synth :blade
    use_synth_defaults attack: 0.1, sustain: 0.3, release: 0.5, cutoff: 116,
                       vibrato_rate: 5.5, vibrato_depth: 0.12, pan: -0.2
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [w1, w2, w1, w3, w4, w2, w3, w4][(bar - 4) % 8]
        ph(rt, esc(bar), p, 0.5, pa2.include?(bar) ? 1 : 0,
           pa2.include?(bar) ? 0.46 : 0.30, {})
      else
        sleep 4
      end
    end
  end
end

# --- campanas de victoria: solo al final, y es la primera vez en todo el
#     juego que suenan en modo mayor sostenido ---------------------------
with_fx :echo, phase: 0.75, decay: 3, mix: 0.22 do
  live_loop :bells do
    sd 2006
    use_synth :pretty_bell
    use_synth_defaults release: 2.0, pan: 0.42
    32.times do |bar|
      if pa2.include?(bar) || out.include?(bar)
        hp(dg(rt, sc(:major), [0, 4, 2, 5][bar % 4], 2),
           amp: out.include?(bar) ? 0.09 : 0.15)
      end
      sleep 4
    end
  end
end

live_loop :air do
  sd 2007
  32.times do |bar|
    a = if pa2.include?(bar) then 0.08
        elsif pa.include?(bar) || pb.include?(bar) then 0.05
        else 0.025 end
    4.times do
      pk(:hat_cab, amp: a, rate: 1.35, pan: 0.5)
      sleep 1
    end
  end
end
