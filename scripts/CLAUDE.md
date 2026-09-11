# `scripts/` - autoplay y guiones de verificacion

Se lanzan con `MANTLE.exe --script scripts/<fichero>.txt`. **`exit 0` no es un gate**: lo
que falla solo se ve leyendo las lineas del log que cada guion declara como criterio.

## Comandos de autoplay

- **Debug**: `--script <file>` autoplay (wait/hold/press/aim/shot/walk/bot/goto/chapter/killall/spawn/god/seed/give/learn/faith/cards/mcards/stats/say/quit; `intro` lanza el prólogo y `cine n` salta a una de sus 8 estampas y para el avance automático, para poder fotografiarla; `escena <id> [1]` fuerza una escena guionizada de `game/scenes.h` (0 = la audiencia con Acab, 1-9 = aperturas de los caps. 2-10, 10-19 = diálogos con el jefe de los caps. 1-10; todas suprimidas bajo autoplay) y la deja PARADA como `chintro` (confirm pasa de página, cancel la salta entera) o, con el `1`, avanzando sola; una apertura al cerrarse lanza la intro de su capítulo y un jefe carga antes su arena con el jefe de pie y al cerrarse suelta la pelea; `audiencia` = `escena 0`; `chapter n` jumps to a chapter to check biomes/bosses; `event <capítulo> [índice]` abre un encuentro del pozo de ese capítulo sin caminar hasta su sala (desde los mapas fijos las salas Event colocan el pozo COMPLETO en celdas fijas, pero abrirlos por comando sigue siendo lo único que permite verificar el pozo entero en dos idiomas sin jugarse diez capítulos); loguea id, locutor, retrato y PNJ; `seed <n>` es desde los mapas fijos un override PEGAJOSO de compatibilidad (pisa `Config::CHAPTER_SEEDS` toda la sesión; sin él, el guion juega el mapa REAL del juego); `spawn <enemyType> <n>` siembra enemigos de un tipo concreto junto al jugador — sin él, comprobar una conducta del Acto II depende de que el capítulo la sortee; `feel <etiqueta>` imprime el TRAMO de tacto desde la marca anterior (TTK, golpes recibidos, tiros, daño); `fx <id> [tamaño] [duración]` suelta un efecto de la hoja propia delante del profeta; `give <itemId>`, `learn <miracleId> <slot>` (slot 0..2), `faith <n>`, `cards`/`mcards` open the choice screens — con ellos se puede fotografiar cualquier efecto sin jugar hasta él; `press weapon` = ataque secundario, `press m3` = tercer milagro). Añadidos al cerrar el plan del GDD: `ascend` (dispara la Ascensión sin rematar al jefe del cap. 5 — es el único camino al Manto), `advance` (cierra el capítulo; en el 10 es lo que dispara la victoria), `agesave <horas>` (envejece el guardado para probar la caducidad de 48 h), `unlocks <n>` (fija `SaveData::unlocks`, para volver a bloquear el epílogo), `meta <id>` (compra una mejora de El Santuario sin ofrendas), `pit` (coloca al profeta al borde de un hueco y mirando hacia el), `collect` (atrae las recogidas de la sala), `door` (deja al profeta al lado de la puerta de la sala) y `touch`/`drag`/`untouch <x> <y> [dedo]` (toques sintéticos en coordenadas de juego: el ratón NO genera eventos `SDL_EVENT_FINGER_*`, así que sin ellos los controles táctiles solo se podían probar en un teléfono) más sus gemelos **`touchpx`/`dragpx`/`untouchpx <px> <py> [dedo]`**, en **píxeles de ventana** (ver más abajo). `stats` imprime además una segunda línea `LOADOUT` con profeta, slots, recarga y objetos. `bot n` termina solo si no hay enemigos en la sala. `killall` lista primero y mata después, para que los hijos del Agua Corrupta no caigan en el mismo barrido, y **sus bajas cuentan** en el marcador (`combat_events` se vacía después de leerse, no antes de llenarse). Un `shot` **no suelta** lo que se estuviera manteniendo: `hold map 0.6` + `shot` es la única forma de fotografiar el mapa grande. F12 screenshot (`screenshots/`, 1280x720), room ASCII dump in the log while a script runs. Put a `wait 0.1` after each `shot` so the capture lands before the next command.

## Zona continua (2026-09-10)

Un capitulo es UN mapa (`docs/mundo-exterior.md`, "Zona continua"), y eso cambia como se
escribe un guion:

- **`walk x y` va en coordenadas de ZONA.** La celda `(gx,gy)` empieza en `(gx*640, gy*352)`;
  la de inicio es (2,2) en las rejillas 4x3 (origen (1280,704)) y (2,3) en las 4x4 (5 y 10:
  (1280,1056)). Toda coordenada "de sala" clavada antes del 2026-09-10 esta podrida: sumale el
  origen de su celda.
- **`goto <celda|tipo>` teletransporta al NUDO de la celda** (sus enemigos nacen al frame
  siguiente, determinista). **El jefe se entra ANDANDO**: `goto boss` deja bajo el recinto y
  hace falta `walk 1600 150` (la celda B es (2,0) en todos los capitulos; el vano esta en
  (1568..1632, 192..208)). Sin el walk no nace el jefe y `bot`/`killall` no tienen a quien.
  `escena <jefe>` lo hace solo.
- **`pit`** busca el hueco MAS CERCANO al profeta (la zona tiene varios).
- Logs nuevos: `Zone built: WxH cells (...), boss cell N, arena yes, door yes`, `Entered cell
  N [gx,gy] (Tipo)` al cruzar una costura, `Arena sealed: boss cell N`, `Cell N cleared
  (Tipo)`, `Cell N despawned (n entities)`, `Baal prophet slot k at (x,y)`. Se conservan
  `Loaded room N [gx,gy] (Cap) - Tipo [n enemies]` (en cada `goto`, arranque y vuelta de
  puerta/escena) y el volcado ASCII de 40x22 por celda. Ya no existe `Room transition`.
- Guiones nuevos o reescritos ese dia: `mapas_check` (costura, perimetros, arena desde
  fuera / sellada / arco), `carmelo_check` (el jefe en grupo), `feel_check` tramo 6 (cruzar
  una costura no carga nada), y los repinneados `feel_baseline`, `logros_check`,
  `audio_check`, `mira_check`, `secretos_check`, `vfx_check`, `guardado_check` (v10).

## Guiones

- **Guiones de verificación** en `scripts/`: `lore_check.txt` y `pulido_check.txt` (regresión, deben pasar siempre), **`feel_baseline.txt`** (MEDIDA de tacto, no captura: se ejecuta antes y después de tocar `Config::Feel` y se comparan las líneas `FEEL`), `feel_check.txt`, `audio_check.txt` (criterio negativo: cero `Failed to load`), `logros_check.txt` (desbloquea 18/18 desde guardado limpio), `vfx_check.txt` (efectos, retratos y paleta), `encuentros_check.txt` (los **38** encuentros del pozo, en ES y EN; su gate es el log, no las capturas), `hint_check.txt`, `actoII_check.txt` (conductas del Acto II), `codice_check.txt` (las 5 pestañas del Códice), **`secretos_check.txt`** (secretos de bioma y los tres milagros-llave), **`interiores_check.txt`** (entrar y salir de un interior sin perder el estado de fuera), `milagros_check.txt` (Ojos Abiertos, cargas del consumible, tercer slot y guardar/cargar), `intro_check.txt` (las 8 estampas del prólogo), **`audiencia_check.txt`** (la audiencia con Acab: orden audiencia → intro → Playing, ES/EN, ESC y Continuar; gates del log), **`escenas_check.txt`** (las 20 escenas guionizadas —10 aperturas, 10 diálogos de jefe— paradas con captura y completas en ES y EN; cero TEXT TRUNCATED es el gate que manda), **`intros_check.txt`** (las 10 intros cinemáticas de capítulo en ES y EN; sus gates son del log — ver la sección de intros), `ascension_check.txt` (Manto de Elías + milagro heredado), `caducidad_check.txt`, `tactil_check.txt`, **`tactil_ancho_check.txt`** y **`menus_tactil_check.txt`** (**todos piden `--touch`**), `epilogo_check.txt` (las dos ramas de la victoria) y `discipulos_check.txt`.
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

## Táctil: por qué hay DOS familias de comandos de toque (2026-09-11)

`touch/drag/untouch` hablan en **unidades de juego** y el inyector les aplica la inversa
exacta de la transformación que hace `Input::process_touch`. Eso los hace cómodos —las
coordenadas del guion son las mismas que las del HUD— y a la vez **ciegos al único fallo que
importaba**: si el dibujo y el hit-test se desplazan uno respecto del otro, los dos lados se
desplazan igual y el guion sigue pasando. Así se escapó durante meses que el táctil ignoraba
el letterbox: en un móvil de 20:9 los botones respondían ~200 px a la derecha de donde se
veían.

`touchpx/dragpx/untouchpx` hablan en **píxeles de ventana** (`tfinger = px / ventana`), que es
lo que hace un dedo de verdad. Son la única prueba de extremo a extremo. Guion:
**`scripts/tactil_ancho_check.txt`** (`--touch --window 1200x540`): toca la celda Q en los
píxeles EXACTOS en los que se dibuja y el gate es `Cast miracle: Cuervos de Querit` — con el
desfase salía `Fuego del Cielo`, que es el milagro de la celda de al lado.

**`scripts/menus_tactil_check.txt`** cubre lo otro que no existía: navegar los menús con el
dedo. Gates: `SAVEDUMP ajustes musica=0.40 sens=6` (mitad izquierda / derecha de una fila de
Opciones), un `Scene change` al tocar "Nueva Partida" y un `Item taken:` tras tocar DOS veces
la misma carta.

Y una lección de reparto: `tactil_check.txt` tuvo que mover su sonda de apuntado de
(232, 118) a (232, 100) al aparecer el botón ORAR. **La fila de acción ocupa y 112..134** y
los botones se comprueban antes que el "tap = ataque", así que cualquier sonda que caiga ahí
se la come un botón.

## Pulido tras la primera prueba del APK (2026-09-11)

Tres guiones nuevos y un comando nuevo, de los cuatro fallos que el usuario saco jugando el
APK en su movil:

- **`milagro_pista_check.txt`** (`--touch`): que se vea que los milagros se tocan. Gate: UNA
  sola linea `Miracle hint shown (chapter 6, ...)` —el capitulo 6 es el gate de verdad,
  porque con el codigo viejo la pista solo salia en el 1— y ninguna segunda. Las tres celdas
  solo existen con `meta 6` **antes** de `chapter 6`: las mejoras se aplican al CARGAR el
  capitulo, no al comprarlas. Y la pista se fotografia **dos veces** (`71a`/`71b`): el
  contador `miracle_hint_timer` solo corre mientras hay un milagro lanzable, asi que o sale
  en el frame siguiente a `faith 100` o exactamente 5 s despues, segun con cuanta fe arranque
  el capitulo. El mensaje dura 4,5 s, o sea que nunca la pillan las dos.
- **`entrada_jefe_check.txt`**: la entrada del jefe. Pide el comando nuevo **`entrada`**, que
  deja pasar el paseo y el dialogo del jefe (el autoplay los suprime porque los guiones
  cronometran las peleas). Gate: el ORDEN `Arena sealed` -> `boss approach start` ->
  `boss approach end (completo)` -> `Jefe N`, y jamas la escena antes del paseo; mas la rama
  de `press cancel`, que tiene que dar `boss approach end (saltado)`.
  Dos trampas que costaron una pasada cada una: **`walk` es bloqueante**, asi que con el
  destino dentro del recinto se come el paseo entero antes de que el guion pueda
  fotografiarlo (se anda hasta el BORDE del vano y se cruza con `hold up`); y **`chapter` se
  descarta EN SILENCIO fuera de `Playing`**, asi que hay que cerrar la escena del capitulo
  anterior (`press cancel`) antes de saltar al siguiente.
- **`mapa_check.txt`**: el mapa del capitulo pintando el terreno, en cinco capitulos, con
  niebla y con Ojos Abiertos.

**Correccion a la regla de `hold map` + `shot`.** El apano de que un `shot` no suelte lo que
se este manteniendo re-arma el `hold` **un solo paso fijo** (`ap.timer = FIXED_DT`). Si entre
ese paso y el render se cuela otro, la tecla ya esta suelta y la captura sale del juego
normal — de seis capturas de mapa, DOS salieron mal, y en dos sitios distintos en dos
pasadas. La receta robusta es poner **otro `hold map` DETRAS del `shot`**:

```
hold map 0.8
shot 6X_lo_que_sea
hold map 0.4
wait 0.2
```
