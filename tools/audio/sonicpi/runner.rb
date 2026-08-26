# MANTLE session runner: escucha el codigo que manda el MCP (OSC /run-code al
# puerto de cues) y lo evalua tal cual, SIN crossfade ni reescritura de amp: —
# la grabacion necesita que la pista arranque limpia y determinista.
# Avisa al harness (record.py) por OSC cuando el segmento ha entrado, para que
# el corte del bucle se mida desde un instante conocido.

live_loop :mantle_mcp_listen do
  use_real_time
  d = sync "/osc*/run-code"
  code = d[0].to_s
  use_osc "127.0.0.1", 5052
  begin
    # eval SINCRONO: live_loop devuelve nada mas definirse, y asi un error de
    # sintaxis cae en este rescue en vez de perderse en un hilo anonimo.
    eval(code)
    osc "/mantle/started"
    puts "MANTLE runner: segmento en marcha (#{code.length} chars)"
  rescue Exception => e
    osc "/mantle/error", e.message.to_s[0, 200]
    puts "MANTLE runner ERROR: #{e.message}"
  end
end

puts "MANTLE runner listo: esperando /run-code"
