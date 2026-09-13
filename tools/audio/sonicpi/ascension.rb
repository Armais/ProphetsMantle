# MANTLE - LA ASCENSION. "He aqui un carro de fuego con caballos de fuego
# apartó a los dos, y Elias subio al cielo en un torbellino" (2 Reyes 2:11).
# Es la pieza que va HACIA ARRIBA: la fundamental sube un grado cada cuatro
# compases y el coro entra cuando el carro aparece. Termina en mayor, con el
# manto ya en el suelo.
# Hijaz en MI. Periodo: 24 compases de 4/4 a 76 BPM = 75,7895 s.
use_bpm 76

rt = 52
subida = [0, 0, 1, 1, 2, 2]        # un grado cada 4 compases
define :esc do |bar| (bar >= 16) ? sc(:major) : sc(:hijaz) end
define :raiz do |bar| subida[[bar / 4, 5].min] end

with_fx :reverb, room: 0.94, mix: 0.55 do
  live_loop :pads do
    sd 8001
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 2.4, release: 2.0, cutoff: 110
    24.times do |bar|
      r = raiz(bar)
      ch = [dg(rt, esc(bar), r, 0), dg(rt, esc(bar), r + 2, 0), dg(rt, esc(bar), r + 4, 0)]
      hc(ch, amp: (bar < 4 || bar >= 20) ? 0.15 : 0.22, pan: -0.34)
      hc(ch.map { |n| n + 12 }, amp: 0.10, pan: 0.42) if bar >= 4
      sleep 4
    end
  end
end

live_loop :bass do
  sd 8002
  use_synth :bass_foundation
  use_synth_defaults attack: 0.05, sustain: 2.4, release: 1.0, amp: 0.40
  24.times do |bar|
    # El bajo tambien en la entrada y la salida: sin el, los dos extremos del
    # bucle no empalman (7,5 dB de escalon).
    hp(dg(rt, esc(bar), raiz(bar), -2), amp: bar < 4 ? 0.24 : 0.42)
    sleep 4
  end
end

# --- el carro: el tambor arranca lento y acelera hasta el torbellino ------
live_loop :carro do
  sd 8003
  24.times do |bar|
    if bar < 6
      pk(:drum_tom_lo_soft, amp: 0.18, rate: 0.7, pan: -0.3)
      sleep 4
    elsif bar < 12
      pk(:drum_tom_lo_hard, amp: 0.32, rate: 0.75, pan: -0.3)
      sleep 2
      pk(:drum_tom_mid_soft, amp: 0.18, rate: 0.85, pan: 0.28)
      sleep 2
    elsif bar < 20
      4.times do |i|
        pk(i.even? ? :drum_tom_lo_hard : :drum_tom_mid_hard,
           amp: 0.36, rate: 0.8 + 0.05 * i, pan: i.even? ? -0.32 : 0.34)
        sleep 1
      end
    else
      pk(:drum_splash_soft, amp: 0.22, rate: 0.8, pan: 0.3) if bar == 20
      pk(:drum_tom_lo_soft, amp: 0.16, rate: 0.7, pan: -0.3)
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.96, mix: 0.62 do
  live_loop :choir do
    sd 8004
    24.times do |bar|
      if bar >= 8 && bar % 4 == 0
        a = bar >= 16 ? 0.34 : 0.24
        pk(:ambi_choir, amp: a, rate: bar >= 16 ? 0.75 : 0.5, pan: -0.48)
        pk(:ambi_choir, amp: a * 0.7, rate: bar >= 16 ? 0.752 : 0.502, pan: 0.52)
      end
      sleep 4
    end
  end
end

with_fx :reverb, room: 0.9, mix: 0.48 do
  live_loop :fuego do
    sd 8005
    use_synth :blade
    use_synth_defaults attack: 0.08, sustain: 0.2, release: 0.5, cutoff: 118,
                       vibrato_rate: 6.0, vibrato_depth: 0.14, pan: -0.2
    24.times do |bar|
      if bar >= 10 && bar < 22
        r = raiz(bar)
        [r, r + 2, r + 4, r + 2, r + 5, r + 4, r + 2, nil].each do |d|
          hp(dg(rt, esc(bar), d, 1), amp: 0.32) if d
          sleep 0.5
        end
      else
        sleep 4
      end
    end
  end
end

with_fx :echo, phase: 1.0, decay: 4, mix: 0.26 do
  live_loop :campanas do
    sd 8006
    use_synth :pretty_bell
    use_synth_defaults release: 2.4, pan: 0.4
    24.times do |bar|
      hp(dg(rt, esc(bar), raiz(bar) + (bar >= 16 ? 4 : 0), 2),
         amp: bar < 4 ? 0.07 : 0.15) if bar % 2 == 0
      sleep 4
    end
  end
end

live_loop :aire do
  sd 8007
  24.times do |bar|
    a = bar >= 12 ? 0.07 : (bar >= 4 ? 0.045 : 0.02)
    4.times do
      pk(:hat_cab, amp: a, rate: 1.4, pan: 0.5)
      sleep 1
    end
  end
end
