# MANTLE - EL SANTUARIO (`MusicID::Menu`). Suena entre partidas, mientras el
# jugador gasta ofrendas. Es el unico sitio del juego donde no hay prisa ni
# peligro, asi que no hay percusion de compas: solo el lecho, la kalimba y una
# campana cada cuatro compases.
#
# Esta pista existia desde la primera tanda y NO la pedia nadie: el titulo
# usaba main_theme y el Santuario tambien. Ahora son dos sitios distintos.
#
# Eolico en SOL, 64 BPM. Periodo: 24 compases de 4/4 = 90,000 s exactos.
use_bpm 64

rt = 55
s  = sc(:aeolian)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
out   = (20...24)

prog = [[0, 2, 4], [3, 5, 0], [5, 0, 2], [4, 6, 1]]
q1 = [0, nil, 2, nil, 1, nil, nil, nil]
q2 = [2, nil, 4, nil, 3, 2, nil, nil]
q3 = [4, nil, 3, nil, 2, nil, 0, nil]

with_fx :reverb, room: 0.93, mix: 0.55 do
  live_loop :lecho do
    sd 4001
    use_synth :hollow
    use_synth_defaults attack: 2.5, sustain: 5, release: 4, cutoff: 90, res: 0.18
    24.times do |bar|
      if bar % 2 == 0
        hp(rt - 12, amp: 0.10, pan: -0.55)
        hp(rt, amp: 0.12, pan: 0.58)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.86, mix: 0.44 do
  live_loop :pads do
    sd 4002
    use_synth :prophet
    use_synth_defaults attack: 2.2, sustain: 2.4, release: 2.2, cutoff: 106
    24.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      quiet = intro.include?(bar) || out.include?(bar)
      hc(ch, amp: quiet ? 0.07 : 0.20, pan: -0.32)
      hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.4) unless quiet
      sleep 4
    end
  end
end

live_loop :bass do
  sd 4003
  use_synth :bass_foundation
  use_synth_defaults attack: 0.1, sustain: 3.2, release: 1.4, amp: 0.30
  24.times do |bar|
    hp(dg(rt, s, prog[bar % 4][0], -1)) if !intro.include?(bar) && bar % 2 == 0
    sleep 4
  end
end

with_fx :echo, phase: 1.5, decay: 5, mix: 0.28 do
  live_loop :kalimba do
    sd 4004
    use_synth :kalimba
    use_synth_defaults release: 1.8, pan: 0.3
    24.times do |bar|
      ch = prog[bar % 4]
      if pa.include?(bar) || pb.include?(bar)
        [ch[0], ch[2], ch[1] + 7].each do |d|
          hp(dg(rt, s, d, 2), amp: pb.include?(bar) ? 0.18 : 0.13)
          sleep 4.0 / 3
        end
      else
        hp(dg(rt, s, ch[0], 2), amp: 0.07)
        sleep 4
      end
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :voz do
    sd 4005
    use_synth :recorder
    use_synth_defaults attack: 0.3, sustain: 0.5, release: 1.0, pan: -0.18
    24.times do |bar|
      if pb.include?(bar)
        ph(rt, s, [q1, q2, q3, q1, q2, q3, q1, q2][(bar - 12) % 8], 0.5, 1, 0.30, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :campana do
    sd 4006
    use_synth :dull_bell
    use_synth_defaults release: 3.0, pan: 0.42
    24.times do |bar|
      hp(dg(rt, s, 0, 1), amp: 0.12) if bar % 4 == 0
      sleep 4
    end
  end
end

live_loop :aire do
  sd 4007
  24.times do |bar|
    pk(:hat_cab, amp: (pa.include?(bar) || pb.include?(bar)) ? 0.04 : 0.02,
       rate: 1.45, pan: 0.46)
    sleep 4
  end
end
