# `scripts/` - autoplay y guiones de verificacion

Se lanzan con `MANTLE.exe --script scripts/<fichero>.txt`. **`exit 0` no es un gate**: lo
que falla solo se ve leyendo las lineas del log que cada guion declara como criterio.

## Comandos de autoplay

- **Debug**: `--script <file>` autoplay (wait/hold/press/aim/shot/walk/bot/goto/chapter/killall/spawn/god/seed/give/learn/faith/cards/mcards/stats/say/quit; `intro` lanza el prólogo y `cine n` salta a una de sus 8 estampas y para el avance automático, para poder fotografiarla; `chapter n` jumps to a chapter to check biomes/bosses; `event <capítulo> [índice]` abre un encuentro del pozo de ese capítulo sin caminar hasta su sala (desde los mapas fijos las salas Event colocan el pozo COMPLETO en celdas fijas, pero abrirlos por comando sigue siendo lo único que permite verificar el pozo entero en dos idiomas sin jugarse diez capítulos); loguea id, locutor, retrato y PNJ; `seed <n>` es desde los mapas fijos un override PEGAJOSO de compatibilidad (pisa `Config::CHAPTER_SEEDS` toda la sesión; sin él, el guion juega el mapa REAL del juego); `spawn <enemyType> <n>` siembra enemigos de un tipo concreto junto al jugador — sin él, comprobar una conducta del Acto II depende de que el capítulo la sortee; `feel <etiqueta>` imprime el TRAMO de tacto desde la marca anterior (TTK, golpes recibidos, tiros, daño); `fx <id> [tamaño] [duración]` suelta un efecto de la hoja propia delante del profeta; `give <itemId>`, `learn <miracleId> <slot>` (slot 0..2), `faith <n>`, `cards`/`mcards` open the choice screens — con ellos se puede fotografiar cualquier efecto sin jugar hasta él; `press weapon` = ataque secundario, `press m3` = tercer milagro). Añadidos al cerrar el plan del GDD: `ascend` (dispara la Ascensión sin rematar al jefe del cap. 5 — es el único camino al Manto), `advance` (cierra el capítulo; en el 10 es lo que dispara la victoria), `agesave <horas>` (envejece el guardado para probar la caducidad de 48 h), `unlocks <n>` (fija `SaveData::unlocks`, para volver a bloquear el epílogo), `meta <id>` (compra una mejora de El Santuario sin ofrendas), `pit` (coloca al profeta al borde de un hueco y mirando hacia el), `collect` (atrae las recogidas de la sala), `door` (deja al profeta al lado de la puerta de la sala) y `touch`/`drag`/`untouch <x> <y> [dedo]` (toques sintéticos en coordenadas de juego: el ratón NO genera eventos `SDL_EVENT_FINGER_*`, así que sin ellos los controles táctiles solo se podían probar en un teléfono). `stats` imprime además una segunda línea `LOADOUT` con profeta, slots, recarga y objetos. `bot n` termina solo si no hay enemigos en la sala. `killall` lista primero y mata después, para que los hijos del Agua Corrupta no caigan en el mismo barrido, y **sus bajas cuentan** en el marcador (`combat_events` se vacía después de leerse, no antes de llenarse). Un `shot` **no suelta** lo que se estuviera manteniendo: `hold map 0.6` + `shot` es la única forma de fotografiar el mapa grande. F12 screenshot (`screenshots/`, 1280x720), room ASCII dump in the log while a script runs. Put a `wait 0.1` after each `shot` so the capture lands before the next command.

## Guiones

- **Guiones de verificación** en `scripts/`: `lore_check.txt` y `pulido_check.txt` (regresión, deben pasar siempre), **`feel_baseline.txt`** (MEDIDA de tacto, no captura: se ejecuta antes y después de tocar `Config::Feel` y se comparan las líneas `FEEL`), `feel_check.txt`, `audio_check.txt` (criterio negativo: cero `Failed to load`), `logros_check.txt` (desbloquea 18/18 desde guardado limpio), `vfx_check.txt` (efectos, retratos y paleta), `encuentros_check.txt` (los **38** encuentros del pozo, en ES y EN; su gate es el log, no las capturas), `hint_check.txt`, `actoII_check.txt` (conductas del Acto II), `codice_check.txt` (las 5 pestañas del Códice), **`secretos_check.txt`** (secretos de bioma y los tres milagros-llave), **`interiores_check.txt`** (entrar y salir de un interior sin perder el estado de fuera), `milagros_check.txt` (Ojos Abiertos, cargas del consumible, tercer slot y guardar/cargar), `intro_check.txt` (las 8 estampas del prólogo), **`intros_check.txt`** (las 10 intros cinemáticas de capítulo en ES y EN; sus gates son del log — ver la sección de intros), `ascension_check.txt` (Manto de Elías + milagro heredado), `caducidad_check.txt` y `tactil_check.txt` (**los dos piden `--touch`**), `epilogo_check.txt` (las dos ramas de la victoria) y `discipulos_check.txt`.
**Comandos de autoplay nuevos**: `lang <0|1>`, `aimmode <0|1|2>` y **`defaults`** (ajustes
de fabrica). `defaults` no es un lujo: las filas que **ciclan** (auto-mira, vibracion,
idioma) no tienen un valor absoluto al que llevarlas con `press`, asi que sin el un guion
que las toca da un resultado distinto en cada vuelta segun como quedaran en la anterior.
Por lo mismo **`feel_baseline.txt` empieza ahora con `defaults`**: los ajustes sobreviven
entre sesiones y con la auto-mira en "Asistido" las cifras se movian y parecia una
regresion del motor.

Guiones: **`scripts/ajustes_check.txt`** (las seis filas, en los dos idiomas, y persisten)
y **`scripts/mira_check.txt`**. Este ultimo mide lo que `feel` **no puede** medir: el bot
apunta a mano y perfecto, asi que los tres modos le dan el mismo TTK (comprobado: identico
con `aimmode` 0, 1 y 2). Lo que separa los modos es la GEOMETRIA, asi que dispara UNA vez
con el profeta mirando a un sitio fijo y el enemigo a un angulo conocido. **Reescrito el
2026-08-27** tras la reforma del mundo, y sus tres reglas nuevas estan comentadas en el
propio guion: la semilla FIJADA (sin ella cada pasada genera otra sala y a veces un arbol
cae en el carril de tiro — parecia una regresion de la mira y era el decorado), el rumbo
con un `hold` de 0,06 s (el steering del walk deja ~3 grados de deriva que se comian el
margen del cono), y el disparo EN EL MISMO TICK que el `spawn` (con cualquier espera el
soldado anda y el angulo deriva alrededor del borde del cono de 22 grados). Medido hoy:
dentro del cono OFF hace **0** de dano y Asistido **13**; fuera del cono Asistido hace
**0** y ON **13**. O sea que "Asistido" no es ninguno de los otros dos.

### La bateria completa tras la reforma del mundo (2026-08-27)

Se corrieron los 32 guiones de `scripts/` de una tacada y el saldo son **dos bugs de motor
y cuatro guiones podridos** — todos con la misma causa raiz: la reforma del 2026-08-26
(salas de 2x2 pantallas, rejilla 4x3, Ascenso jugable) dejo atras a quien daba por hecho
el mundo viejo. La leccion que engloba a las demas: **exit 0 no es un gate.** Los 32
guiones salian con exit 0; lo que fallaba solo se ve leyendo las lineas del log que cada
guion declara como criterio.

- **Los proyectiles morian al salir de la PANTALLA, no de la sala** (`system_projectiles`,
  ecs.cpp): el descarte usaba `GAME_WIDTH/GAME_HEIGHT` (320x180), correcto cuando una sala
  media una pantalla y letal con salas de 640x352 — cualquier tiro (del profeta o de un
  arquero) disparado fuera del cuarto superior izquierdo se evaporaba en su primer frame.
  Lo cazo `mira_check` con la punteria PERFECTA y cero dano. Ahora acota por
  `Room::world_w()/world_h()`. Este arreglo **movio la sonda de tacto entera** (las cifras
  nuevas estan en la seccion del mundo exterior) y explica parte de la varianza salvaje de
  los arqueros entre semillas (0,86-24,77 s) que se midio el dia 26.
- **La red `unstick` no consultaba `gap_crosser`** y el Carro de Fuego no cruzaba huecos
  (detalle en la seccion de secretos).
- **Cuatro guiones asumian el mundo viejo** y se reescribieron: `logros_check` (el altar de
  Baal ya no esta en (232,56); las salas 4/6 pasaron a Altar/Tesoro y los logros de sala no
  saltaban; el jefe ya no es el indice 1 → `goto boss`; el Ascenso de 4 paradas y el
  epilogo recorrible se comian los comandos de la victoria con las esperas cortas),
  `mira_check` (ver su seccion), `audio_check` (`goto 1` caia en una Elite y el grito de
  jefe no sonaba; el Ascenso corto se comia la voz de Eliseo) y `secretos_check` (un
  segundo `god` — es un TOGGLE — apagaba el del arranque, y limpiar la Elite abre la CARTA
  de recompensa: hasta que el autoplay la toma solo a los 0,7 s, `pit` y `press m1` se
  descartan en silencio).
- Trampas de guion que conviene recordar: **la fe que dan las bajas rompe "Bajo la
  Retama"** — el killall de una sala de 7 enemigos suma +9 (+3 si hay profeta) y el motor
  anade su +2 de "sala sin dano" ANTES del check `< 10`, asi que el guion espera un golpe
  a proposito para anular ese +2; y las **Tablas de la Ley ponen suelo de fe 25**, asi que
  todo lo que necesite fe baja va ANTES de dar la reliquia.
- Los ocho `unstick` de entidades enemigas que salen en los logs (spawns pegados a un
  solido) son la red funcionando, no un fallo: solo `unstick: entity 0` (el profeta)
  merece investigacion.
