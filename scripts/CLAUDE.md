# `scripts/` - autoplay y guiones de verificacion

Se lanzan con `MANTLE.exe --script scripts/<fichero>.txt`. **`exit 0` no es un gate**: lo
que falla solo se ve leyendo las lineas del log que cada guion declara como criterio.

## Comandos de autoplay

- **Debug**: `--script <file>` autoplay (wait/hold/press/aim/shot/walk/bot/goto/chapter/killall/spawn/god/seed/give/learn/faith/cards/mcards/stats/say/quit; `intro` lanza el prólogo y `cine n` salta a una de sus 8 estampas y para el avance automático, para poder fotografiarla; `escena <id> [1]` fuerza una escena guionizada de `game/scenes.h` (0 = la audiencia con Acab, 1-9 = aperturas de los caps. 2-10, 10-19 = diálogos con el jefe de los caps. 1-10; todas suprimidas bajo autoplay) y la deja PARADA como `chintro` (confirm pasa de página, cancel la salta entera) o, con el `1`, avanzando sola; una apertura al cerrarse lanza la intro de su capítulo y un jefe carga antes su arena con el jefe de pie y al cerrarse suelta la pelea; `audiencia` = `escena 0`; `chapter n` jumps to a chapter to check biomes/bosses; `event <capítulo> [índice]` abre un encuentro del pozo de ese capítulo sin caminar hasta su sala (desde los mapas fijos las salas Event colocan el pozo COMPLETO en celdas fijas, pero abrirlos por comando sigue siendo lo único que permite verificar el pozo entero en dos idiomas sin jugarse diez capítulos); loguea id, locutor, retrato y PNJ; `seed <n>` es desde los mapas fijos un override PEGAJOSO de compatibilidad (pisa `Config::CHAPTER_SEEDS` toda la sesión; sin él, el guion juega el mapa REAL del juego); `spawn <enemyType> <n>` siembra enemigos de un tipo concreto junto al jugador — sin él, comprobar una conducta del Acto II depende de que el capítulo la sortee; `feel <etiqueta>` imprime el TRAMO de tacto desde la marca anterior (TTK, golpes recibidos, tiros, daño); `fx <id> [tamaño] [duración]` suelta un efecto de la hoja propia delante del profeta; `give <itemId>`, `learn <miracleId> <slot>` (slot 0..2), `faith <n>`, `cards`/`mcards` open the choice screens — con ellos se puede fotografiar cualquier efecto sin jugar hasta él; `press weapon` = ataque secundario, `press m3` = tercer milagro). Añadidos al cerrar el plan del GDD: `ascend` (dispara la Ascensión sin rematar al jefe del cap. 5 — es el único camino al Manto), `advance` (cierra el capítulo; en el 10 es lo que dispara la victoria), `agesave <horas>` (envejece el guardado para probar la caducidad de 48 h), `unlocks <n>` (fija `SaveData::unlocks`, para volver a bloquear el epílogo), `meta <id>` (compra una mejora de El Santuario sin ofrendas), `pit` (coloca al profeta al borde de un hueco y mirando hacia el), `collect` (atrae las recogidas de la sala), `door` (deja al profeta al lado de la puerta de la sala) y `touch`/`drag`/`untouch <x> <y> [dedo]` (toques sintéticos en coordenadas de juego: el ratón NO genera eventos `SDL_EVENT_FINGER_*`, así que sin ellos los controles táctiles solo se podían probar en un teléfono) más sus gemelos **`touchpx`/`dragpx`/`untouchpx <px> <py> [dedo]`**, en **píxeles de ventana** (ver más abajo). `stats` imprime además una segunda línea `LOADOUT` con profeta, slots, recarga y objetos. `bot n` termina solo si no hay enemigos en la sala. `killall` lista primero y mata después, para que los hijos del Agua Corrupta no caigan en el mismo barrido, **atraviesa lo intocable** (2026-10-03: jefes en fase invulnerable) y **sus bajas cuentan** en el marcador; **`bosshp <fracción> [quién]`** salta a una fase del jefe (ver "Los diez jefes", al final) (`combat_events` se vacía después de leerse, no antes de llenarse). Un `shot` **no suelta** lo que se estuviera manteniendo: `hold map 0.6` + `shot` es la única forma de fotografiar el mapa grande. F12 screenshot (`screenshots/`, 1280x720), room ASCII dump in the log while a script runs. Put a `wait 0.1` after each `shot` so the capture lands before the next command.

## Zona continua (2026-09-10)

Un capitulo es UN mapa (`docs/mundo-exterior.md`, "Zona continua"), y eso cambia como se
escribe un guion:

- **`walk x y` va en coordenadas de ZONA.** La celda `(gx,gy)` empieza en `(gx*640, gy*352)`.
  Desde el mapa grande (2026-09-24) la celda de inicio y la del jefe CAMBIAN por capitulo, asi
  que un guion nuevo no clava coordenadas absolutas: **`walks x y`** va relativo al origen de
  la celda de INICIO (x = tile*16+8 del volcado ASCII de esa celda, el que sale JUSTO ANTES de
  su linea `Loaded room`) y **`walkarena d`** a `d` unidades del centro del vano de la arena,
  positivo hacia dentro, sea North, West, East o South (lo viejo `walk 1600 150` es
  `walkarena 50`; `walk 1600 214`, `walkarena -14`).
- **`goto <celda|tipo>` teletransporta al NUDO de la celda** (sus enemigos nacen al frame
  siguiente, determinista). Tipos: `event`, `treasure`, `altar`, `elite`, `boss`, `start` y
  `rest` (paso tranquilo). **El jefe se entra ANDANDO**: `goto boss` deja enfrente del vano y
  hace falta `walkarena 50`. Sin el walk no nace el jefe y `bot`/`killall` no tienen a quien.
  `escena <jefe>` lo hace solo. Ojo con `hold up` para cruzar el vano: solo vale en arenas
  North (caps. 1, 3, 6 y 10).
- **`intel`** loguea el rastro del jefe: `INTEL nivel=N pistas=M visto=V celda_jefe=C`. Y la
  zona recien construida deja `Zone check: ... -> OK` (o `FALLA`, con la linea de lo que no se
  alcanza): es el gate de `mapas_check`.
- **`killall` no despeja una celda del Acto II a la primera**: el Agua Corrupta se parte al
  morir y sus hijos sobreviven al barrido. Tres `killall` con medio segundo entre medias.
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

- **Guiones de verificación** en `scripts/`: `lore_check.txt` y `pulido_check.txt` (regresión, deben pasar siempre), **`feel_baseline.txt`** (MEDIDA de tacto, no captura: se ejecuta antes y después de tocar `Config::Feel` y se comparan las líneas `FEEL`), `feel_check.txt`, `audio_check.txt` (criterio negativo: cero `Failed to load`), **`musica_check.txt`** (el mapeo de las 47 pistas: `chintro N` deja la cinematica parada en su PRIMERA estampa y el `press confirm` siguiente pasa a la segunda, asi que el log tiene que cambiar de `capNN_cine1.ogg` a `capNN_cine2.ogg`; si las dos dieran el mismo indice, las dos estampas volverian a sonar igual), `logros_check.txt` (desbloquea 18/18 desde guardado limpio), `vfx_check.txt` (efectos, retratos y paleta), `encuentros_check.txt` (los **38** encuentros del pozo, en ES y EN; su gate es el log, no las capturas), `hint_check.txt`, `actoII_check.txt` (conductas del Acto II), `codice_check.txt` (las 5 pestañas del Códice), **`secretos_check.txt`** (secretos de bioma y los tres milagros-llave), **`interiores_check.txt`** (entrar y salir de un interior sin perder el estado de fuera), `milagros_check.txt` (Ojos Abiertos, cargas del consumible, tercer slot y guardar/cargar), `intro_check.txt` (las 8 estampas del prólogo), **`audiencia_check.txt`** (la audiencia con Acab: orden audiencia → intro → Playing, ES/EN, ESC y Continuar; gates del log), **`escenas_check.txt`** (las 20 escenas guionizadas —10 aperturas, 10 diálogos de jefe— paradas con captura y completas en ES y EN; cero TEXT TRUNCATED es el gate que manda), **`intros_check.txt`** (las 10 intros cinemáticas de capítulo en ES y EN; sus gates son del log — ver la sección de intros), `ascension_check.txt` (Manto de Elías + milagro heredado), `caducidad_check.txt`, `tactil_check.txt`, **`tactil_ancho_check.txt`** y **`menus_tactil_check.txt`** (**todos piden `--touch`**), `epilogo_check.txt` (las dos ramas de la victoria) y `discipulos_check.txt`.
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
(232, 118) a (232, 100) al aparecer el botón ORAR. Los botones se comprueban antes que el
"tap = ataque", así que cualquier sonda que caiga encima de uno (o de su margen de dedo, desde
2026-10-03) se la come el botón. Hoy el grupo es un arco alrededor de DASH (ver "La pasada de
movil", al final): en 16:9 y escala 1 ocupa de x≈223 a 320 y de y≈83 a 175.

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

## Los menus tactiles, tras la tercera prueba del APK (2026-09-12)

**Un toque que falla ya NO es mudo.** `SDL_Log("Menu tap: pantalla=%d fila=%d (x,y)")` sale
en cada toque de menu (`-1` = no acerto ninguna fila) y `Santuario tap: fila=%d` en El
Santuario. Antes no habia forma de notarlo: `menus_tactil_check` llevaba las coordenadas del
titulo podridas desde un cambio de paso anterior —tocaba y=90 para "Nueva Partida" cuando
esa y ya caia en "Continuar"— y el guion pasaba igual.

- **`menus_tactil_check.txt` re-pinneado entero** a la rejilla 2x4 del titulo y a la ventana
  de 6 filas de Opciones (las coordenadas van en su cabecera). Gates nuevos: una linea
  `Menu tap` por toque con la fila esperada, **cero `fila=-1`**, **cero
  `Input: tap descartado sin leer`** y **ni un `Menu tap` en el bloque de arrastre**.
- **El arrastre va en DOS `drag`**, no en uno largo: cada comando inyecta un solo evento de
  dedo y el motor da un paso de lista por evento a proposito (dos `pulse` de la misma accion
  en el mismo frame serian una sola pulsacion). Un dedo real manda 60-120 eventos por
  segundo y ahi el gesto es continuo.
- **Para fotografiar un boton pulsado hay que soltar el dedo FUERA**: tocar un menu es
  elegir Y confirmar, asi que soltandolo encima el guion se va a otra pantalla. `touch` ->
  `shot` -> `drag` lejos -> `shot` -> `untouch` da las dos capturas (pulsado y suelto) sin
  navegar, y de paso comprueba que sacar el dedo cancela el toque.
- **El primer toque del guion contesta al selector de idioma**, porque `defaults` olvida el
  idioma elegido y la ejecucion SIGUIENTE arranca en esa pantalla. Si el juego ya esta en el
  titulo, ese toque cae en el canal muerto entre columnas (x 156..164) y no hace nada.
- **`tactil_ancho_check.txt`** estrena un bloque de rejilla, y es el unico sitio donde el
  desfase de letterbox se puede cazar en un menu: hasta ahora todo lo tocable estaba
  centrado en x=160, que es justo el punto que el desfase no mueve. La sonda es el **filo
  interior de la columna derecha**, que es donde 16 unidades de error deciden: sin descontar
  el letterbox `x_malo = x*0,8 + 32`, o sea que x=167 (dentro de la celda, 166..258) se
  leeria como 165,6, en el canal muerto. Gates: `touchpx 621 381` da `fila=6` y el pilar
  negro (`touchpx 40 381`) da `fila=-1`.
- Ese guion **fija el idioma con `lang 0`**, porque su gate es el NOMBRE del milagro y los
  ajustes sobreviven entre sesiones: si la pasada anterior lo dejo en ingles, el gate busca
  "Cuervos de Querit" y el log dice "Ravens of Cherith". Y **contesta al selector de idioma**
  antes de nada, por lo mismo que `menus_tactil_check`.

## El tutorial (2026-09-12)

Tres guiones nuevos y dos comandos:

- **`tutorial [paso]`** lanza el tutorial desde donde sea y, con el número, salta DIRECTO a
  ese paso (1-based) con su sala ya montada — sin eso, fotografiar la sala 3 exige jugarse
  doce pasos. Vuelve al TÍTULO al acabar, para que el guion siga mandando en vez de caer en
  una partida.
- **`tutorialask`** abre la pregunta de Nueva Partida. Hace falta porque esa pregunta está
  **suprimida bajo autoplay a propósito**: treinta guiones esperan entrar a jugar con un
  `press confirm` en el título, y con la pregunta de por medio elegirían "Sí, enséñame".

Guiones: **`tutorial_check.txt`** (los 15 pasos, las dos pantallas nuevas y el camino
pregunta → tutorial → partida), **`tutorial_tactil_check.txt`** (`--touch`) y
**`tutorial_en_check.txt`** (los 16 pares `tr()` + las dos páginas de cierre, gate visual).

Gates de `tutorial_check`: las 15 líneas `Tutorial: paso N/15 cumplido` en orden,
`Tutorial: fin (paso 15 de 15)`, **UNA SOLA** `Scene change: 2 -> 5` y **después** del paso
15 (es la pantalla de cierre; cualquier otra sería el bug del arco, que lanzaba el epílogo
del capítulo 1 desde la sala 3), **cero `Chapter 1 complete`**, **cero `Secret offerings`**
(la ofrenda del paso 12 se cuenta pero no se ingresa), `Item choice [ELIGES UNA DE TRES]` al
abrir el cofre y, tras dejar el tutorial desde la pausa, un `LOADOUT` con **slots 1** y
**objetos: -** — el equipo del tutorial no viaja a la partida.

Desde el 2026-09-13 **ningún paso se cierra solo**, así que el bot tiene que hacer los quince:
abrir y cerrar la pausa (`press pause` x2) y caminar hasta el arco.

Seis trampas, cada una de una pasada:

- **`walk` sigue vivo al cambiar de sala** y empuja al profeta dentro de la sala nueva. En la
  sala 2 eso lo llevaba encima del altar y lo rezaba tres pasos antes de tiempo (se arregló
  descentrando el altar, pero el guion además le da un destino nuevo al entrar).
- **Un tiro alcanza 120 unidades** (`Config::PROJECTILE_RANGE`), 7,5 tiles: un blanco más
  lejos no se abre sin caminar hasta él, y el paso se quedaba colgado sin decir por qué.
- **Toda recogida caduca a los 14 s**, así que la ofrenda del paso 12 se siembra cuando ESE
  paso empieza y se repone si el jugador se entretiene.
- **No se dispara desde donde el profeta se quede al andar.** El acelerado no es simétrico y
  tras el paso de moverse queda a caballo entre dos columnas: desde ahí el tiro hacia arriba
  no encuentra ninguna tinaja y el paso se cuelga con las cinco intactas. Se camina a la
  columna de la tinaja (`walk 120 136` y `walk 216 136`) antes de cada ráfaga.
- **Un `press` dura DOS TICKS y se pierde si te están empujando.** Los tres milagros se
  intentan dos veces cada uno: el soldado que el paso planta como blanco empuja al profeta y
  el tercer milagro se perdía con la celda encendida y la fe al 96 %.
- **Entre las dos pulsaciones de la pausa NO cabe un `shot`.** Un `shot` re-arma lo que se
  estuviera manteniendo, y lo que se mantenía era Pause: la captura metía un tercer flanco y
  el juego se quedaba en pausa el resto del guion.

Y dos cosas que **no se pueden fotografiar** desde un guion: el título de la carta del cofre
(se abre a mitad del `walk` y bajo autoplay se toma sola a los 0,7 s — el gate es el log,
`Item choice [...]`) y las páginas de cierre con esperas largas (duran ~5 s enteras: el
`shot` va a 2,2 s y a 4,3 s del `walk`, o sale el título del juego).

## El idioma en caliente (2026-09-12)

**`scripts/i18n_caliente_check.txt`**. `tr()` devuelve un puntero al literal del idioma
activo EN EL MOMENTO DE LA LLAMADA, asi que lo que guarda ese puntero en una estructura de
larga vida se quedaba congelado: cambiar el idioma en Opciones dejaba "Honda de David" en una
partida en ingles. Lo arregla `Engine::relocalize` (detalle en `src/core/CLAUDE.md`).

Gate del log, y es el bueno porque no depende de mirar una captura: el `LOADOUT` de `stats`
**en la misma partida** pasa de "Honda de David, Agua del Jordan" a "David's Sling, Water of
the Jordan" con un `lang 1` de por medio, y vuelve con `lang 0`. Mas una linea
`i18n: registros re-traducidos (...)` por cambio. Las capturas cubren El Santuario (sus 15
mejoras, con el estado COMPRADO intacto) y el rotulo de la barra del jefe.

## Los cuatro arreglos del feedback del tester (2026-09-15)

Tres guiones nuevos, de las cuatro cosas que el tester saco jugando el APK:

- **`pnj_paseo_check.txt`**: que un PNJ no cambie de personaje a mitad de escena. La
  apertura del cap. 2 es la UNICA escena con paseo cinematico, y el paseo llama a
  `world.update`: `system_animation` avanzaba la celda del `src_rect` sobre `npcs.png`, que
  es **una fila de 16 personajes distintos**, asi que la viuda de Sarepta se convertia en
  otra persona cada 0,15 s y se quedaba congelada en la que tocara para las cuatro cajas
  siguientes. **Su gate es VISUAL y no hay otro**: diez capturas a 0,4 s durante el paseo
  (sin el arreglo no hay dos seguidas iguales) mas las cajas posteriores.
- **`tutorial_gastado_check.txt`**: gastar ANTES de que te lo pidan. El paso 2 mide tinajas
  rotas y el 5 el flanco del consumible, y como ningun paso se cierra solo, quien rompia las
  cinco tinajas o se bebia la pocion durante el paso de moverse dejaba el tutorial colgado
  para siempre. El guion hace el destrozo con el paso 1 todavia ABIERTO — es lo unico que
  prueba algo: en cuanto el paso 1 se cierra, `tutorial_setup_step` ya ha replantado.
  Detalle que costo una pasada: las tinajas de los extremos estan a 107 unidades y **desde
  el punto de nacimiento no se alcanzan** (el alcance es 120, pero el tiro no llega), asi
  que hay que arrimarse 144 unidades de las 220 que cierran el paso 1.

## Los diez jefes con estrategia propia (2026-10-03)

Diseno y cifras en `src/game/CLAUDE.md` ("Los diez jefes"). Un comando nuevo, un cambio en
uno viejo y un guion:

- **`bosshp <fraccion> [quien]`** fija la vida del jefe a una fraccion de su maximo para
  SALTAR A UNA FASE sin jugarse la pelea (las fases se miran por vida: 50% del Capitan, el
  frenesi del corro, las cuatro de la Desesperacion, la deuda, la hueste al 30%...). `quien`:
  0 = jefe y sequito (por defecto), 1 = solo la cabeza, 2 = solo el sequito (el corro,
  Jezabel). Con fraccion 0 los MATA por el camino normal (`apply_damage` + `on_enemy_killed`,
  con sus recompensas y su conducta de muerte), aunque esten intocables: `bosshp 0 2` tumba a
  Jezabel y Acab se enfurece. Loguea `Autoplay: bosshp F (quien Q) -> N`. El cambio de fase
  lo hace la IA en el frame siguiente: hace falta un `wait` antes de fotografiarlo.
- **`killall` atraviesa lo intocable**. Antes un `killall` en el momento malo dejaba vivo al
  sumo sacerdote (intocable mientras el corro tenga 3 de pie, y la lista mata en orden de
  id: la cabeza va antes que el corro), al Espiritu bajo el agua o al Sacerdote con sus
  idolos, y la arena no se cerraba nunca. Es lo que mantiene en pie el `killall` unico de
  `carmelo_check`.
- **`scripts/jefes_check.txt`**: entra en las diez arenas con `god` + `goto boss` +
  `walkarena 50`, deja correr cada pelea, salta de fase con `bosshp` y fotografia telegrafos,
  ventanas y cambios de fase (`jNN_*`). Sus gates son del log y estan en su cabecera:
  `Boss sheets cap N: propia` x10, `Boss <nombre> spawned ... hoja propia`, una linea
  `Hazard <Tipo> spawned (...)` de cada tipo, las lineas `Boss <nombre> phase N (...)`, los
  estados de cada estrategia ("stunned", "idolos derribados", "ritual del corro", "se
  sumerge", "la deuda se cierra", "sin resuello", "lanza la plata", "soldados cegados"...),
  diez `defeated in` + diez `Cell N cleared (Boss)` y cero `DESCARTADO` / `Failed to load`.
- Trampas: la pelea del cap. 5 con el botin de los cuatro jefes anteriores la tumba el bot
  en dos segundos, asi que `bosshp 0 2` va ANTES del `bot`; y el cap. 6 hay que matarlo con
  varios `killall` (los hijos del Agua Corrupta sobreviven al primero).
- **`escena <jefe>` asienta ya la camara en la arena.** En el juego real lo hace el paseo de
  entrada, que bajo el comando no corre: `escenas_check` fotografiaba las diez escenas de
  jefe desde el nudo de FUERA del recinto, sin jefe ni profeta en cuadro (es11..es20).
- Medir la duracion de una pelea: partida limpia (`defaults` + `metareset`), `chapter n`,
  `goto boss`, `walkarena 50`, `bot 160`; la linea `Boss <nombre> defeated in T s` la da
  (en el duo, `... (cae la ultima del duo)`). Cifras de hoy en `src/game/CLAUDE.md`.

## El Santuario, la alforja y los objetos (2026-10-03)

Comandos nuevos o cambiados:

- **`meta <id> [rango]`**: sin rango compra el MAXIMO de esa mejora (los ids son estables,
  `namespace Meta` en `systems/progression.h`). En plena partida rellena YA los dones que se
  gastan (oraciones, re-sorteos, Aliento, huecos de alforja); en el juego real se rellenan al
  empezar intento o capitulo. `metareset` ademas pone los rangos a 0 y olvida los avisos.
- **`offerings <n>`** fija el banco de ofrendas (para comprar por el camino REAL, con ENTER).
- **`dropcons <id>`** suelta un consumible (8 Vino, 9 Agua, 17 Higos, 18 Sal) 22 unidades por
  debajo del profeta; `collect` lo atrae.
- **`savenow`** guarda la partida en el acto, por el mismo camino que el autoguardado de una
  celda limpia (`populate_save_data`).
- `press belt1..belt4 / beltnext / beltprev`: la alforja. `press use` = el elegido.
- `stats` imprime una TERCERA linea, **`GIFTS`**: la alforja (`*` = elegido, `xN` cargas), lo
  que queda de Aliento / Suertes / oraciones, dano y velocidad reales, el escudo y el banco.
- `savedump` imprime una linea mas: `SAVEDUMP rangos=<un digito por mejora> avisos=0x..
  alforja=id:cargas/... dones=aliento/suertes/oraciones` (`-1` = "rellenar al Continuar").

**La pausa estrena "Equipo" en la fila 1**: Codice pasa a la 2, Opciones a la 3 y Salir a la
4. Se re-pinnearon los nueve guiones que bajaban por ella (`ascension_check`,
`audiencia_check`, `caducidad_check`, `codice_check`, `i18n_caliente_check`, `i18n_check`,
`milagros_check`, `tutorial_check`, `vfx_check`: un `press down` mas tras `press pause`,
marcado con un comentario). `meta_check` va a la pestana Tesoro en vez de bajar doce filas.

Guiones nuevos: **`santuario_check.txt`** (rangos por el camino real, cada don funcionando en
partida, la derrota con su desglose, El Santuario desde la derrota y que Continuar conserva lo
comprado) y **`alforja_check.txt`** (recoger / llena / usar por hueco / cambiar, los Higos, la
mecanica de cada pasivo y la alforja en el guardado). Sus gates estan en su cabecera.

**Ojo con los guardados nuevos**: en un `XDG_DATA_HOME` vacio el juego arranca en el selector
de IDIOMA, y los guiones que esperan el titulo navegan mal. La bateria se corre sobre una
copia de un guardado con el idioma ya contestado (`press confirm` + `quit` una vez).

## La pasada de movil (2026-10-03)

Comandos nuevos:

- **`tbtn <boton> <1|0>`**: baja (1) o sube (0) un dedo en el CENTRO del boton tactil, donde
  este con el layout ACTUAL (`dash`, `usar`, `m1`, `m2`, `m3`, `arma`, `orar`, `pausa`). Desde
  que los botones son redondos y su sitio depende de la pantalla, el tamaño y el zurdo, unas
  coordenadas fijas se pudririan en cada cambio. Es tan "ciego" como `touch` (aplica la misma
  transformacion que el dibujo): la sonda de extremo a extremo sigue siendo `touchpx`.
- **`touchprefs <tamaño%> <opacidad%> <zurdo 0|1>`** y **`battery <0|1>`**: los ajustes
  tactiles sin pasar por Opciones (sus filas solo existen en tactil).
- **`joylog`**: `JOY activo base dedo radio eje reposo` del stick de movimiento.
- **`fpsmark`**: `FPSMARK N fps pintados en T s (ahorro=A)` desde la marca anterior.
- `savedump` imprime `SAVEDUMP tactil tamano=.. opacidad=.. zurdo=.. ahorro=..`.

Guiones:

- **`movil_check.txt`** (`--touch --window 2400x1080`): el grupo en el pilar, el temblor del
  milagro negado, el stick que sigue al dedo, tamaño / opacidad / zurdo y la vista previa de
  Opciones. Gates en su cabecera.
- **`ritmo_check.txt`** (`--touch --window 640x360`): el ritmo de frames. Se mide en el
  TITULO porque el renderer por software del contenedor tarda ~75 ms por frame de partida (13
  fps con o sin el cambio): ahi no se ve nada. El techo de 60 tampoco se ve (el titulo llega a
  ~45), pero el de 30 del ahorro si, y sin `--touch` el ahorro no toca nada.
- Re-pinneados: `tactil_check` (botones por `tbtn`), `tactil_ancho_check` (milagro 1 en px
  1023,477 y DASH **en el pilar** en px 1137,477; el toque del pilar izquierdo cae ahora en la
  zona del stick y sigue sin pulsar nada), `vertical_check` (milagro 1 en (261,354), y el toque
  rapido SEPARADO de la captura: una captura a esa resolucion se lleva varios pasos y el toque
  pasaba a ser un mantener) y `menus_tactil_check` (Opciones tiene 12 filas en tactil:
  "Volver" es la 11 y se llega con seis tramos de arrastre).
- **Con `--touch`, las capturas de partida leen la VENTANA** (los controles van en la
  superposicion): con `--window 2400x1080` salen a 2400x1080 con los pilares.

### El dedo mantenido en los guiones, y `apuntado_tactil_check` (2026-10-03)

**El arnes soltaba el Ataque de un dedo sintetico.** `update_autoplay` pone a cero cada tick
las acciones que gobierna el guion (`driven`, Ataque incluido) y vuelve a afirmar solo lo que
mantiene un `hold`. Un `touch` mantenido en la mitad de disparo disparaba por tanto UNA vez y
nada mas: en un telefono el dedo apoyado mantiene la rafaga. Ahora, tras el reset, se
re-afirma el Ataque si hay dedo de disparo (`Input::touch_aim_active`) y DASH / ARMA si su
boton tactil sigue pulsado. Solo afecta al modo guion. Los guiones tactiles de siempre dan
las mismas lineas que antes.

**`apuntado_tactil_check.txt`** (`--touch`) responde a "¿se apunta y se dispara a la vez con
el pulgar derecho?" con la auto-mira APAGADA y la geometria de `mira_check`: arrastrar hacia
el soldado lo mata (3 tiros, 20 de dano), arrastrar al lado contrario hace 0, empezar mal y
corregir SIN soltar hace 13, y con el Joystick de apuntar, 13. Trampa que costo la primera
version: hay que llegar al puesto de tiro por el MISMO camino que `mira_check`
(`walks 536 200` -> `walks 536 312`); desde otro sitio el `walk` se atasca 94 unidades a la
izquierda y un arbol seco tapa la linea de tiro, y parecia que el apuntado no funcionaba.
`joylog` imprime ahora tambien `AIM dedo apuntando ataque aim_axis` del dedo de disparo.

## Destreza: Celo, criticos y esquiva perfecta (2026-11-10)

Diseno y cifras en `src/core/CLAUDE.md` ("Destreza"). Un comando nuevo, una linea mas en
`stats` y un guion:

- **`dodgetest`** suelta un proyectil HOSTIL encima del profeta, apuntandole. Con `press dash`
  en el MISMO tick (sin `wait` entre medias) es una esquiva perfecta segura; sin el dash es un
  golpe recibido, que es el control negativo. Es la unica forma de probar la ventana de
  0,15 s del dash sin fiarla a la punteria de un arquero. Loguea
  `Autoplay: dodgetest (..., dashing=0|1)`.
- `stats` imprime una CUARTA linea, **`SKILL`**: la racha en curso y su grado, la mejor racha
  del intento, las esquivas perfectas y los criticos.
- Lineas nuevas del log: `Perfect dodge #N: ...`, `Zeal: grado N (Fervor|Celo|Celo Ardiente)
  con M`, `Zeal: racha ROTA por un golpe (M, grado N)` y `Zeal: racha cerrada LIMPIA (M,
  grado N) -> +K ofrendas`.

**`scripts/destreza_check.txt`**: gates en su cabecera. Verificado el 2026-11-10: esquiva
perfecta solo con dash (`dashing=1` -> `Perfect dodge #1`; `dashing=0` -> racha rota y ni una
esquiva mas), grados 1 y 2 en la tanda de ocho soldados, criticos > 0 y el cierre limpio
cobrando ofrendas (que llegan al banco por el camino de siempre: `Secret offerings: +1`).

**`feel_baseline` cambia de cifras a proposito**: la racha (que viaja entre tramos si el
siguiente empieza antes de que se enfrie) y los criticos acortan el TTK. Cifras nuevas y la
medida del jefe del Carmelo en `src/core/CLAUDE.md`.
