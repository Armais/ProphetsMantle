# MANTLE - menu: variante del tema principal, mas desnuda (en gen_audio.py el
# menu ERA el tema principal tal cual; aqui se queda la misma armonia y la misma
# melodia pero sin percusion, para que el menu respire distinto del titulo).
# 84 BPM, mi frigia dominante, 16 compases = 64 pulsos = 45.71 s.
use_bpm 84

pd = [0, 1, 4, 5, 7, 8, 10]

define :dg do |root, sc, d, oc|
  root + 12 * oc + 12 * (d / sc.length) + sc[d % sc.length]
end

define :phr do |root, sc, pat, stp, oc, av|
  pat.each do |d|
    play dg(root, sc, d, oc), amp: av if d
    sleep stp
  end
end

pa = [0, nil, 1, 2, nil, 4, nil, 3, 2, nil, 1, nil, 0, nil, nil, nil]
pb = [4, nil, 5, 6, nil, 7, nil, 6, 5, nil, 4, nil, 2, nil, 1, nil]
pc = [7, nil, 6, 5, nil, 4, nil, 3, 4, nil, 2, nil, 1, nil, 0, nil]

with_fx :reverb, room: 0.9, mix: 0.5 do
  live_loop :pads do
    use_synth :prophet
    use_synth_defaults attack: 1.6, sustain: 1.6, release: 1.6, cutoff: 78, amp: 0.3
    16.times do |bar|
      ch = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]][bar % 4]
      play_chord ch.map { |d| dg(52, pd, d, 0) }, pan: -0.2
      play_chord ch.map { |d| dg(52, pd, d, 0) }, pan: 0.25, amp: 0.22
      sleep 4
    end
  end
end

live_loop :bass do
  use_synth :fm
  use_synth_defaults divisor: 1, depth: 1.2, attack: 0.08, sustain: 3.0, release: 0.7, amp: 0.42
  16.times do |bar|
    play dg(52, pd, [0, 3, 1, 0][bar % 4], -1)
    sleep 4
  end
end

with_fx :reverb, room: 0.95, mix: 0.55 do
  live_loop :lead do
    use_synth :blade
    use_synth_defaults attack: 0.18, sustain: 0.22, release: 0.5, cutoff: 90,
                       vibrato_rate: 5.0, vibrato_depth: 0.14, vibrato_delay: 0.2, pan: -0.1
    [pa, pb, pa, pc, pa, pb, pa, pc].each do |p|
      phr 52, pd, p, 0.5, 1, 0.42
    end
  end
end

# Arpegio suave en vez de tambores: el menu se mueve, pero no marcha.
with_fx :echo, phase: 0.75, decay: 3, mix: 0.25 do
  live_loop :arp do
    use_synth :pluck
    use_synth_defaults sustain: 0.25, release: 0.8, coef: 0.35, amp: 0.2
    16.times do |bar|
      ch = [[0, 2, 4], [3, 5, 0], [1, 3, 5], [0, 2, 4]][bar % 4]
      [ch[0], ch[1], ch[2], ch[1]].each_with_index do |d, i|
        play dg(52, pd, d, 1), pan: -0.35 + 0.23 * i
        sleep 1
      end
    end
  end
end
