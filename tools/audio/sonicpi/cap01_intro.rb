# MANTLE cap. 1 - DIALOGO Y CARTA DE APERTURA.
#
# Reescrita el 2026-09-14. La version anterior era cap01.rb sin percusion: la
# MISMA melodia, el MISMO ney, la MISMA progresion y el mismo drone, o sea que
# el capitulo empezaba con su tema ya gastado y el jugador no oia ningun
# cambio al ponerse a jugar. Esta es una pieza propia:
#
#   - la voz es :blade (canto tenido), no el ney de cap01.rb
#   - el acompanamiento es un laud (:pluck grave) y no la kalimba
#   - la melodia va por saltos (quinta abajo, sexta arriba) frente al paso
#     ondulante de cap01, y a la mitad de velocidad
#   - sin percusion: debajo hay texto que leer
#
# Lo unico que comparte con el tema del capitulo es el modo (dorico en RE) y
# el centro tonal, que es justo lo que hace que suenen del mismo sitio sin
# sonar a lo mismo.
#
# Periodo: 12 compases de 4/4 a 60 BPM = 48,000 s exactos.
# Forma: 0-1 se abre | 2-5 canta | 6-9 el laud solo | 10-11 se apaga.
use_bpm 60

rt = 50            # RE3
s  = sc(:dorian)

# Acordes por compas (ciclo de 4). Mismo modo, otro giro: el tema del capitulo
# va I-IV-VI-V; este cae al VI antes de resolver.
prog = [[0, 2, 4], [3, 5, 0], [5, 0, 2], [4, 6, 1]]

# La voz. Ocho pasos de media pulsacion por compas, casi todo silencio: son
# notas largas, no una linea corrida.
v1 = [4, nil, nil, nil, 0, nil, nil, nil]
v2 = [nil, nil, 2, nil, nil, nil, 1, nil]
v3 = [5, nil, nil, nil, 4, nil, nil, nil]
v4 = [2, nil, nil, nil, nil, nil, 0, nil]

with_fx :reverb, room: 0.88, mix: 0.52 do
  # --- acordes tenidos: el suelo de la escena --------------------------------
  live_loop :fondo do
    sd 3301
    use_synth :tri
    use_synth_defaults attack: 1.8, sustain: 1.4, release: 2.4, cutoff: 84, res: 0.1
    12.times do |bar|
      a = case bar
          when 0, 1   then 0.10
          when 10, 11 then 0.08
          when 6..9   then 0.13
          else 0.17
          end
      hc(prog[bar % 4].map { |d| dg(rt, s, d, 0) }, amp: a, pan: -0.4)
      sleep 4
    end
  end

  # --- bajo: una nota por compas, nunca por debajo de la 43 (el HPF del
  #     pipeline se come todo lo que baje de 55 Hz) -----------------------------
  live_loop :bajo do
    sd 3302
    use_synth :subpulse
    use_synth_defaults attack: 0.25, sustain: 2.2, release: 1.4, amp: 1.0
    raiz = [0, 3, 5, 4]
    12.times do |bar|
      n = dg(rt, s, raiz[bar % 4], 0) - 5
      a = (bar < 2 || bar > 9) ? 0.13 : 0.20
      hp(n, amp: a, pan: 0.0)
      sleep 4
    end
  end

  # --- el laud: acompana en A y se queda SOLO en B --------------------------
  live_loop :laud do
    sd 3303
    use_synth :pluck
    use_synth_defaults coef: 0.18, cutoff: 115
    12.times do |bar|
      if bar < 2
        skip 1
      elsif bar <= 5
        # acompanamiento: la raiz y la quinta, sin prisa
        hp(dg(rt, s, prog[bar % 4][0], 1), amp: 0.30, pan: 0.35); sleep 2
        hp(dg(rt, s, prog[bar % 4][2], 1), amp: 0.22, pan: 0.45); sleep 2
      elsif bar <= 9
        # B: el laud solo, arpegiando el acorde entero hacia arriba
        [0, 1, 2, 1].each_with_index do |i, k|
          hp(dg(rt, s, prog[bar % 4][i], (k == 3 ? 2 : 1)),
             amp: 0.40 - k * 0.04, pan: 0.25 + k * 0.08)
          sleep 1
        end
      else
        hp(dg(rt, s, prog[bar % 4][0], 1), amp: 0.18, pan: 0.3); sleep 4
      end
    end
  end

  # --- la voz: solo en A (2-5) y una frase de despedida en el 10 ------------
  live_loop :voz do
    sd 3304
    use_synth :blade
    use_synth_defaults attack: 0.5, sustain: 0.9, release: 1.6, cutoff: 110, vibrato_rate: 4.2
    skip 2
    [v1, v2, v3, v4].each_with_index do |pat, i|
      ph(rt, s, pat, 0.5, 2, [0.24, 0.21, 0.27, 0.19][i], pan: 0.1)
    end
    skip 4
    ph(rt, s, v4, 0.5, 2, 0.13, pan: -0.1)
    skip 1
  end

  # --- campana: tres toques en toda la pista, y el aire de la mezcla --------
  live_loop :campana do
    sd 3305
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 2.8, cutoff: 118
    skip 1
    hp(dg(rt, s, 0, 3), amp: 0.20, pan: -0.3); sleep 6
    hp(dg(rt, s, 4, 2), amp: 0.14, pan: 0.2);  sleep 10
    hp(dg(rt, s, 2, 3), amp: 0.26, pan: 0.35); sleep 6
    hp(dg(rt, s, 6, 2), amp: 0.16, pan: -0.1); sleep 6
    hp(dg(rt, s, 0, 3), amp: 0.22, pan: 0.1);  sleep 8
    hp(dg(rt, s, 4, 3), amp: 0.14, pan: -0.25); skip 2
  end
end
