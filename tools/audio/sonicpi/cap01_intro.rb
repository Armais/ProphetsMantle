# MANTLE cap. 1 (apertura) - el MISMO tema de cap01.rb, desnudo: sin tambor,
# sin cabasa y con el bajo convertido en pedal. Suena en la cinematica y en la
# carta de apertura, asi que cuando el capitulo empieza de verdad el jugador ya
# reconoce la melodia. Es leitmotiv, no una pista distinta.
#
# Periodo: 16 compases de 4/4 a 72 BPM = 53,3333 s.
# Forma:   entra el drone | el ney canta (2-9) | kalimba (8-13) | se apaga
use_bpm 72

rt = 50
s  = sc(:dorian)
prog = [[0, 2, 4], [5, 0, 2], [3, 5, 0], [4, 6, 1]]

n1 = [0, nil, 1, 2, nil, 1, nil, nil]
n2 = [2, nil, 3, 4, nil, 3, 2, nil]
n3 = [4, nil, 3, 2, nil, 1, nil, nil]
n4 = [1, nil, 0, nil, -1, nil, 0, nil]

with_fx :reverb, room: 0.92, mix: 0.55 do
  live_loop :drone do
    sd 2101
    use_synth :hollow
    use_synth_defaults attack: 3.5, sustain: 6, release: 5, cutoff: 88, res: 0.2
    16.times do |bar|
      if bar % 4 == 0
        a = (bar == 0 || bar == 12) ? 0.20 : 0.11
        hp(rt - 12, amp: a * 0.8, pan: -0.5)
        hp(rt, amp: a * 0.75, pan: 0.55)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.85, mix: 0.45 do
  live_loop :pads do
    sd 2102
    use_synth :prophet
    use_synth_defaults attack: 2.0, sustain: 2.0, release: 2.0, cutoff: 104
    16.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, s, d, 0) }
      if bar >= 2 && bar < 14
        hc(ch, amp: 0.20, pan: -0.32)
        hc(ch.map { |n| n + 12 }, amp: 0.09, pan: 0.38)
      else
        hc(ch, amp: 0.04, pan: -0.28)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :ney do
    sd 2103
    use_synth :recorder
    use_synth_defaults attack: 0.18, sustain: 0.3, release: 0.7, pan: -0.15
    16.times do |bar|
      if bar >= 4 && bar < 12
        ph(rt, s, [n1, n2, n1, n3, n4, n2, n3, n1][bar - 4], 0.5, 1, 0.40, {})
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 4, mix: 0.26 do
  live_loop :kalimba do
    sd 2104
    use_synth :kalimba
    use_synth_defaults release: 1.6, pan: 0.3
    16.times do |bar|
      ch = prog[bar % 4]
      if bar >= 8 && bar < 14
        hp(dg(rt, s, ch[0], 2), amp: 0.18)
        sleep 2
        hp(dg(rt, s, ch[2], 2), amp: 0.13)
        sleep 2
      else
        hp(dg(rt, s, ch[0], 2), amp: 0.05)
        sleep 4
      end
    end
  end
end

# Un hilo de cabasa muy lejos: es el aire de la pieza. Sin el, una apertura
# sin percusion se queda en el 1-2% de energia sobre 2 kHz y suena tapada.
live_loop :air do
  sd 2106
  16.times do |bar|
    pk(:hat_cab, amp: (bar >= 4 && bar < 14) ? 0.05 : 0.02, rate: 1.4, pan: 0.45)
    sleep 2
    pk(:hat_cab, amp: 0.02, rate: 1.5, pan: -0.35)
    sleep 2
  end
end

with_fx :reverb, room: 0.95, mix: 0.6 do
  live_loop :crows do
    sd 2105
    16.times do |bar|
      pk(:misc_crow, amp: 0.14, rate: 0.8, pan: -0.6) if bar == 1
      pk(:misc_crow, amp: 0.09, rate: 0.74, pan: 0.66) if bar == 14
      sleep 4
    end
  end
end
