# MANTLE cap. 6 - JERICO. "Las aguas son malas y la tierra es esteril"; Eliseo
# echa sal en el manantial y el agua sana (2 Reyes 2:19-22). El tema hace ESO:
# empieza en FRIGIO en SI, turbio y con acentos cojos de 3+3+2, y al sanar el
# agua se abre a DORICO sobre la misma tonica, con campanas limpias. Misma
# nota fundamental, otro mundo.
#
# Periodo: 32 compases de 4/4 a 100 BPM = 76,800 s exactos.
# Forma:   turbio 0-3 | A la ciudad 4-11 | B la sal 12-19 | A' sana 20-27 | 28-31
use_bpm 100

rt = 59            # SI2
malo  = sc(:phryg)
sano  = sc(:dorian)
intro = (0...4)
pa    = (4...12)
pb    = (12...20)
pa2   = (20...28)
out   = (28...32)

# La escala cambia a mitad de pieza: es el milagro, no un adorno.
define :esc do |bar|
  (bar >= 20) ? sc(:dorian) : sc(:phryg)
end

prog = [[0, 2, 4], [1, 3, 5], [0, 2, 4], [5, 0, 2]]

j1 = [0, nil, 1, nil, 0, nil, nil, nil]
j2 = [2, nil, 1, 0, nil, 1, nil, nil]
j3 = [4, nil, 3, nil, 2, nil, 1, nil]
j4 = [0, 2, nil, 4, nil, 2, 0, nil]

# --- el pozo: lecho turbio que se aclara en A' -----------------------------
with_fx :reverb, room: 0.92, mix: 0.5 do
  live_loop :well do
    sd 1601
    use_synth :dark_ambience
    use_synth_defaults attack: 2.5, sustain: 4, release: 3.5, res: 0.2
    32.times do |bar|
      if bar % 2 == 0
        limpio = bar >= 20
        hp(rt - 24, amp: limpio ? 0.12 : 0.17, pan: -0.58,
           cutoff: limpio ? 96 : 72)
        hp(dg(rt, esc(bar), 4, -1), amp: 0.08, pan: 0.6, cutoff: limpio ? 100 : 76)
      end
      sleep 4
    end
  end
end

# --- el agua mala: frotes y zumbidos, solo antes del milagro ---------------
with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :bad_water do
    sd 1602
    32.times do |bar|
      if bar < 20 && bar % 4 == 1
        pk(:ambi_glass_rub, amp: 0.20, rate: 0.55, pan: 0.45)
      elsif bar < 20 && bar % 4 == 3
        pk(:ambi_soft_buzz, amp: 0.12, rate: 0.7, pan: -0.45)
      end
      sleep 4
    end
  end
end

# --- cuerda frotada grave: la ciudad ---------------------------------------
with_fx :reverb, room: 0.78, mix: 0.34 do
  live_loop :strings do
    sd 1603
    use_synth :dsaw
    use_synth_defaults attack: 0.8, sustain: 1.6, release: 1.2, cutoff: 88,
                       detune: 0.15
    32.times do |bar|
      ch = prog[bar % 4].map { |d| dg(rt, esc(bar), d, 0) }
      if intro.include?(bar) || out.include?(bar)
        hc(ch, amp: 0.06, pan: -0.3)
      else
        hc(ch, amp: 0.19, pan: -0.34)
        hc(ch.map { |n| n + 12 }, amp: 0.08, pan: 0.4)
      end
      sleep 4
    end
  end
end

live_loop :bass do
  sd 1604
  use_synth :bass_foundation
  use_synth_defaults attack: 0.04, sustain: 0.8, release: 0.4, amp: 0.40
  32.times do |bar|
    if intro.include?(bar) || out.include?(bar)
      hp(dg(rt, esc(bar), prog[bar % 4][0], -2), amp: 0.20)
      sleep 4
    else
      # Una octava mas abajo que el resto de capitulos: SI2 se quedaba en 123 Hz
      # y la pista salia sin cuerpo (0,2% de energia bajo 200 Hz).
      r = dg(rt, esc(bar), prog[bar % 4][0], -2)
      # 3+3+2 corcheas: el paso cojo de una ciudad que no esta bien.
      hp(r, amp: 0.48); sleep 1.5
      hp(r, amp: 0.26); sleep 1.5
      hp(r + 12, amp: 0.18); sleep 1
    end
  end
end

live_loop :perc do
  sd 1605
  32.times do |bar|
    if pa.include?(bar) || pa2.include?(bar) || pb.include?(bar)
      big = pa2.include?(bar)
      pk(:tabla_na, amp: big ? 0.40 : 0.30, rate: 0.9, pan: -0.3)
      sleep 1.5
      pk(:tabla_te_ne, amp: 0.20, rate: 1.0, pan: 0.25)
      sleep 1.5
      pk(:tabla_ke2, amp: 0.14, rate: 1.2, pan: 0.4)
      sleep 1
    else
      sleep 4
    end
  end
end

# --- LA SAL: entra en B y ya no se va. Es el sonido del milagro ------------
with_fx :echo, phase: 0.75, decay: 4, mix: 0.3 do
  live_loop :salt do
    sd 1606
    32.times do |bar|
      if pb.include?(bar) && bar % 2 == 0
        pk(:elec_chime, amp: 0.13, rate: 1.0, pan: 0.3)
      elsif bar >= 20
        pk(:elec_chime, amp: bar < 28 ? 0.18 : 0.10, rate: 1.5, pan: -0.25)
      end
      sleep 4
    end
  end
end

# --- la voz del profeta: canta en A y, ya en dorico, en A' ----------------
with_fx :reverb, room: 0.86, mix: 0.42 do
  live_loop :lead do
    sd 1607
    use_synth :recorder
    use_synth_defaults attack: 0.12, sustain: 0.24, release: 0.45, pan: -0.2
    32.times do |bar|
      if pa.include?(bar) || pa2.include?(bar)
        p = [j1, j2, j1, j3, j4, j2, j3, j1][(bar - 4) % 8]
        ph(rt, esc(bar), p, 0.5, 1, pa2.include?(bar) ? 0.44 : 0.30, {})
      else
        sleep 4
      end
    end
  end
end
