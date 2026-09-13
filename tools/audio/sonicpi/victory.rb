# MANTLE - VICTORIA. Corta a proposito: suena al cerrar un capitulo y el
# jugador quiere seguir, no escuchar una pieza. Hijaz en MI (el modo del tema
# principal) subiendo a mayor en la mitad final.
# Periodo: 12 compases de 4/4 a 92 BPM = 31,3043 s.
use_bpm 92

rt = 52
define :esc do |bar| (bar >= 6) ? sc(:major) : sc(:hijaz) end
prog = [[0, 2, 4], [3, 5, 0], [4, 6, 1], [0, 2, 4]]

live_loop :golpes do
  sd 6001
  12.times do |bar|
    if bar < 2
      pk(:drum_tom_lo_hard, amp: 0.44, rate: 0.8, pan: -0.25)
      sleep 1
      pk(:drum_bass_hard, amp: 0.28, rate: 0.9, pan: 0.0)
      sleep 1
      pk(:drum_tom_mid_hard, amp: 0.30, rate: 0.9, pan: 0.28)
      sleep 2
    elsif bar >= 10
      pk(:drum_tom_lo_soft, amp: 0.16, rate: 0.75, pan: -0.3)
      sleep 4
    else
      pk(:drum_tom_lo_hard, amp: 0.38, rate: 0.8, pan: -0.28)
      sleep 2
      pk(:tabla_ke2, amp: 0.14, rate: 1.2, pan: 0.4)
      sleep 1
      pk(:drum_bass_hard, amp: 0.22, rate: 0.92, pan: 0.0)
      sleep 1
    end
  end
end

with_fx :reverb, room: 0.88, mix: 0.44 do
  live_loop :pads do
    sd 6002
    use_synth :prophet
    use_synth_defaults attack: 0.5, sustain: 2.0, release: 1.4, cutoff: 112
    12.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, esc(bar), d, 0) }
      hc(ch, amp: bar >= 10 ? 0.12 : 0.24, pan: -0.34)
      hc(ch.map { |n| n + 12 }, amp: 0.10, pan: 0.42)
      sleep 4
    end
  end
end

live_loop :bass do
  sd 6003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.03, sustain: 1.4, release: 0.6, amp: 0.42
  12.times do |bar|
    hp(dg(rt, esc(bar), prog[bar % 4][0], -1))
    sleep 2
    hp(dg(rt, esc(bar), prog[bar % 4][0], -1) + 7, amp: 0.2)
    sleep 2
  end
end

with_fx :reverb, room: 0.95, mix: 0.58 do
  live_loop :choir do
    sd 6004
    12.times do |bar|
      if bar == 0 || bar == 6
        pk(:ambi_choir, amp: 0.32, rate: 0.75, pan: -0.45)
        pk(:ambi_choir, amp: 0.24, rate: 0.752, pan: 0.5)
      end
      sleep 4
    end
  end
end

with_fx :echo, phase: 0.5, decay: 3, mix: 0.22 do
  live_loop :campanas do
    sd 6005
    use_synth :pretty_bell
    use_synth_defaults release: 2.0, pan: 0.4
    12.times do |bar|
      [0, 2, 4, 2].each_with_index do |d, i|
        hp(dg(rt, esc(bar), d, 2), amp: bar >= 10 ? 0.08 : 0.15) if i.even? || bar >= 6
        sleep 1
      end
    end
  end
end

with_fx :reverb, room: 0.8, mix: 0.36 do
  live_loop :lira do
    sd 6006
    use_synth :pluck
    use_synth_defaults sustain: 0.3, release: 0.8, coef: 0.4, pan: 0.22
    12.times do |bar|
      if bar >= 2 && bar < 10
        ph(rt, esc(bar), [[0, nil, 2, 4, nil, 2, 0, nil],
                          [4, nil, 5, 4, nil, 2, nil, nil],
                          [2, 4, nil, 5, nil, 4, 2, nil],
                          [0, nil, 4, nil, 2, nil, 0, nil]][(bar - 2) % 4],
           0.5, 0, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

live_loop :aire do
  sd 6007
  12.times do |bar|
    4.times do
      pk(:hat_cab, amp: bar >= 10 ? 0.025 : 0.06, rate: 1.4, pan: 0.5)
      sleep 1
    end
  end
end
