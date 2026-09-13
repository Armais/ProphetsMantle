# MANTLE cap. 3 (apertura) - el Carmelo antes del fuego: el tema desnudo, con
# el tambor lejos y el coro entrando al final. 16 compases a 76 BPM = 50,5263 s.
use_bpm 76

rt = 52
s  = sc(:phrygdom)
prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [4, 6, 1]]
f1 = [0, nil, 1, 0, nil, 4, nil, 3]
f2 = [4, nil, 3, 2, 1, nil, 0, nil]
f3 = [2, 3, 4, nil, 5, nil, 4, nil]
f4 = [0, nil, nil, 1, nil, 0, nil, nil]

with_fx :reverb, room: 0.9, mix: 0.46 do
  live_loop :pads do
    sd 2301
    use_synth :prophet
    use_synth_defaults attack: 1.8, sustain: 2.2, release: 1.8, cutoff: 104
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      hc(ch, amp: (bar >= 2 && bar < 14) ? 0.20 : 0.08, pan: -0.34)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.4) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2302
  use_synth :bass_foundation
  use_synth_defaults attack: 0.06, sustain: 3.0, release: 1.0
  16.times do |bar|
    if bar % 2 == 0
      hp(dg(rt, s, prog[bar % 4][0], -2), amp: bar >= 2 ? 0.40 : 0.18)
    end
    sleep 4
  end
end

# El tambor de guerra, pero muy lejos: lo que viene todavia no ha empezado.
live_loop :far_drum do
  sd 2303
  16.times do |bar|
    if bar >= 6
      pk(:drum_tom_lo_soft, amp: 0.16, rate: 0.7, pan: -0.3)
      sleep 2
      pk(:drum_tom_mid_soft, amp: 0.10, rate: 0.8, pan: 0.26)
      sleep 2
    else
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.8, mix: 0.4 do
  live_loop :lyre do
    sd 2304
    use_synth :pluck
    use_synth_defaults sustain: 0.25, release: 0.8, coef: 0.4, pan: 0.22
    16.times do |bar|
      if bar >= 4 && bar < 12
        ph(rt, s, [f1, f2, f1, f3, f4, f2, f3, f1][bar - 4], 0.5, 0, 0.34, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :choir do
    sd 2305
    16.times do |bar|
      if bar == 10 || bar == 13
        pk(:ambi_choir, amp: 0.26, rate: 0.5, pan: -0.45)
        pk(:ambi_choir, amp: 0.18, rate: 0.503, pan: 0.5)
      end
      sleep 4
    end
  end
end

# La chispa antes del fuego: el unico agudo de la apertura.
with_fx :echo, phase: 1.0, decay: 3, mix: 0.2 do
  live_loop :chispa do
    sd 2307
    use_synth :pretty_bell
    use_synth_defaults release: 1.6, pan: 0.4
    16.times do |bar|
      hp(dg(rt, s, prog[bar % 4][2], 2), amp: bar >= 4 ? 0.12 : 0.06) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :air do
  sd 2306
  16.times do |bar|
    pk(:hat_cab, amp: bar >= 4 ? 0.10 : 0.045, rate: 1.35, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.05, rate: 1.5, pan: -0.34)
    sleep 2
  end
end
