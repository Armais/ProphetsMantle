# MANTLE cap. 2 - CINEMATICA, ESTAMPA A (Sarepta: atardecer violeta sobre el
# mar fenicio, la viuda recogiendo dos lenos).
# Textura sin pulso. El tema del capitulo es oud (:pluck) con organo; aqui no
# suena ninguno de los dos: coro, campana y un fondo de mar.
# Eolico en LA, el modo del capitulo.
#
# Periodo: 4 compases de 4/4 a 60 BPM = 16,000 s exactos.
use_bpm 60

rt = 57            # LA3
s  = sc(:aeolian)

with_fx :reverb, room: 0.93, mix: 0.6 do
  live_loop :mar do
    sd 3111
    use_synth :dark_ambience
    use_synth_defaults attack: 2.4, sustain: 5.0, release: 4.2, cutoff: 76, res: 0.12
    4.times do |bar|
      a = [0.24, 0.29, 0.31, 0.22][bar]
      hp(rt - 12, amp: a, pan: 0.4)
      hp(rt - 7,  amp: a * 0.65, pan: -0.45)
      sleep 4
    end
  end

  live_loop :olas do
    sd 3112
    4.times do |bar|
      pk :ambi_swoosh, rate: 0.35, amp: [0.10, 0.14, 0.13, 0.09][bar],
                       cutoff: 104, pan: bar.even? ? -0.35 : 0.4
      sleep 4
    end
  end

  live_loop :coro do
    sd 3113
    4.times do |bar|
      if bar == 1
        pk :ambi_choir, rate: 0.48, amp: 0.30, pan: -0.25
      elsif bar == 2
        pk :ambi_choir, rate: 0.58, amp: 0.36, pan: 0.3
      end
      sleep 4
    end
  end

  # Campanas en octava alta: son el aire de la pista (el analizador mide sobre
  # 2 kHz, y una campana por debajo de esa raya no cuenta).
  live_loop :campana do
    sd 3114
    use_synth :pretty_bell
    use_synth_defaults attack: 0.01, release: 3.0, cutoff: 116
    hp(dg(rt, s, 0, 2), amp: 0.14, pan: 0.1);  sleep 3
    hp(dg(rt, s, 2, 2), amp: 0.30, pan: -0.3); sleep 2.5
    hp(dg(rt, s, 4, 2), amp: 0.26, pan: 0.35); sleep 2
    hp(dg(rt, s, 0, 3), amp: 0.24, pan: -0.15); sleep 2
    hp(dg(rt, s, 6, 2), amp: 0.20, pan: 0.25); sleep 2.5
    hp(dg(rt, s, 4, 2), amp: 0.12, pan: 0.0);  sleep 4
  end
end
