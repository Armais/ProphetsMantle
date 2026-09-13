# MANTLE - libreria comun de las partituras (tools/audio/sonicpi/*.rb).
#
# record.py la inyecta POR EL SPIDER antes del runner, en cada captura. Las
# partituras llegan despues por el MCP (OSC al puerto de cues) y solo traen la
# musica: asi el mensaje del MCP se queda corto y los helpers no se copian
# doce veces. `define` persiste en la sesion entre jobs, que es lo que hace
# posible esta separacion.
#
# Lo que arregla respecto a la tanda de 2026-08-26:
#   - HUMANIZACION: `hp` desplaza cada evento +-9 ms con `time_warp` y le varia
#     la intensidad. Una rejilla exacta es lo que delata a la musica generada.
#     El desplazamiento va en time_warp y NO mueve el reloj logico, asi que el
#     periodo del bucle sigue cuadrando al milisegundo.
#   - DETERMINISMO: el jitter sale de `rand`, sembrado con `sd` al principio de
#     CADA vuelta del bucle. Asi el periodo 2 (el que graba record.py) es
#     identico al 3, al 4 y al que sonara en el juego, y la costura empalma.
#     Sin esto la humanizacion rompe el corte del bucle.
#   - ESCALAS CON NOMBRE: las diez pistas de capitulo ya no comparten modo.

define :sc do |name|
  {
    dorian:   [0, 2, 3, 5, 7, 9, 10],
    aeolian:  [0, 2, 3, 5, 7, 8, 10],
    phryg:    [0, 1, 3, 5, 7, 8, 10],
    phrygdom: [0, 1, 4, 5, 7, 8, 10],   # el unico modo de la tanda vieja
    lydian:   [0, 2, 4, 6, 7, 9, 11],
    major:    [0, 2, 4, 5, 7, 9, 11],
    mixo:     [0, 2, 4, 5, 7, 9, 10],
    melmin:   [0, 2, 3, 5, 7, 9, 11],
    hijaz:    [0, 1, 4, 5, 7, 8, 11],   # maqam Hijaz: la 7a mayor lo separa del frigio dominante
  }[name]
end

# Semilla de la vuelta. Va al principio del cuerpo de CADA live_loop.
define :sd do |n|
  use_random_seed n
end

# Grado de escala -> nota MIDI, con octavas por encima del septimo grado.
define :dg do |root, s, d, oc|
  root + 12 * oc + 12 * (d / s.length) + s[d % s.length]
end

# play humanizado: micro-desplazamiento y variacion de intensidad.
#
# OJO: pasa `amp:` SIEMPRE, asi que PISA el `amp:` de use_synth_defaults. Un
# `hp(nota)` sin amp suena a amp 1.0, no al default de la capa. Da el amp en
# la llamada; los use_synth_defaults valen para lo demas (attack, cutoff...).
define :hp do |n, o|
  o = o || {}
  a = (o[:amp] || 1.0) * (0.86 + rand * 0.28)
  time_warp((rand - 0.5) * 0.018) do
    play n, o.merge(amp: a)
  end
end

# Acorde humanizado: cada nota entra con su propio retraso, que es como suena
# una mano de verdad. Un play_chord clava las tres a la misma muestra.
define :hc do |ns, o|
  o = o || {}
  ns.each do |n|
    a = (o[:amp] || 1.0) * (0.86 + rand * 0.24)
    time_warp(rand * 0.026) do
      play n, o.merge(amp: a)
    end
  end
end

# Frase: lista de grados (nil = silencio), un paso de `stp` pulsos cada uno.
define :ph do |root, s, pat, stp, oc, av, o|
  pat.each do |d|
    hp(dg(root, s, d, oc), (o || {}).merge(amp: av)) if d
    sleep stp
  end
end

# Percusion humanizada. El golpe de un tambor varia mas de intensidad que una
# nota tenida, asi que el rango de amp es mas ancho.
define :pk do |name, o|
  o = o || {}
  a = (o[:amp] || 1.0) * (0.80 + rand * 0.38)
  time_warp((rand - 0.5) * 0.022) do
    sample name, o.merge(amp: a)
  end
end

# Compases de silencio, para la capa que descansa en una seccion.
define :skip do |nbars|
  sleep 4 * nbars
end

puts "MANTLE lib: helpers listos (sc dg sd hp hc ph pk skip)"
