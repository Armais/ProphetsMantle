# MANTLE cap. 4 (apertura) - Horeb. Aqui la version desnuda es casi la pieza
# entera: el viento y EL SILBO, sin terremoto y sin fuego. 16 compases a
# 56 BPM = 68,5714 s.
use_bpm 56

rt = 49
s  = sc(:aeolian)

with_fx :reverb, room: 0.96, mix: 0.62 do
  live_loop :wind do
    sd 2401
    use_synth :prophet
    use_synth_defaults attack: 3.5, sustain: 5, release: 4, res: 0.2
    16.times do |bar|
      if bar % 2 == 0
        # Como en cap04: rt-12 solo son 46 Hz y el paso alto de bus se los come.
        a = bar < 4 ? 0.14 : 0.11
        hp(rt, amp: a, pan: -0.6, cutoff: 100)
        hp(rt + 7, amp: a * 0.6, pan: 0.62, cutoff: 104)
        hp(rt - 12, amp: a * 0.8, pan: -0.2, cutoff: 96)
      end
      sleep 4
    end
  end
end

live_loop :bass do
  sd 2406
  use_synth :bass_foundation
  use_synth_defaults attack: 0.6, sustain: 3.5, release: 2.0
  16.times do |bar|
    hp(dg(rt, s, 0, -2), amp: bar < 4 ? 0.26 : 0.38) if bar % 2 == 0
    sleep 4
  end
end

with_fx :reverb, room: 0.95, mix: 0.55 do
  live_loop :gusts do
    sd 2402
    16.times do |bar|
      pk(:ambi_dark_woosh, amp: 0.18, rate: 0.55, pan: -0.5) if bar == 1
      pk(:ambi_dark_woosh, amp: 0.12, rate: 0.5, pan: 0.55) if bar == 13
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.7, mix: 0.28 do
  live_loop :whisper do
    sd 2403
    use_synth :sine
    use_synth_defaults attack: 0.7, sustain: 1.4, release: 1.6, pan: -0.1
    16.times do |bar|
      if bar >= 5 && bar < 14
        hp(dg(rt, s, [0, 2, 4, 2, 1, 0, -2, 0, 2][bar - 5], 2), amp: 0.26)
      end
      sleep 4
    end
  end
end

with_fx :echo, phase: 2.0, decay: 6, mix: 0.32 do
  live_loop :drops do
    sd 2404
    use_synth :pretty_bell
    use_synth_defaults release: 2.4, pan: 0.4
    16.times do |bar|
      hp(dg(rt, s, [0, 4, 2, 5][(bar / 2) % 4], 2),
         amp: (bar >= 5 && bar < 14) ? 0.24 : 0.15) if bar % 2 == 0
      sleep 4
    end
  end
end

with_fx :echo, phase: 1.0, decay: 3, mix: 0.22 do
  live_loop :aguda do
    sd 2407
    use_synth :pretty_bell
    use_synth_defaults release: 1.8, pan: 0.42
    16.times do |bar|
      hp(dg(rt, s, 4, 2), amp: bar >= 3 ? 0.16 : 0.08) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :aire do
  sd 2405
  16.times do |bar|
    pk(:hat_cab, amp: (bar >= 4 && bar < 14) ? 0.16 : 0.09, rate: 1.45, pan: 0.46)
    sleep 2
    pk(:hat_cab, amp: 0.06, rate: 1.55, pan: -0.38)
    sleep 2
  end
end
