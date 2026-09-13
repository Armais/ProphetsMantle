# `src/core/` - motor, escenas, tacto e i18n

Reglas transversales a todo `src/`: `../CLAUDE.md`.

### Scene State Machine

`GameScene` enum: `Intro → MainMenu → Playing → Paused → Choice → GameOver / Victory`

`Intro` es el prólogo cinemático (ver abajo). Al añadir un valor a `GameScene` hay que
ampliar **tres** switches a la vez: `Engine::update`, `Engine::render` y
`Engine::update_music` — este último corre antes del switch de escena y pisa cualquier
`play_music` manual, así que una escena sin `case` ahí se queda muda.

`Choice` is the "elige 1 de 3" screen (items or miracles) drawn over the frozen world.

**El toggle de la pausa se anulaba a sí mismo** (arreglado el 2026-09-13). `Engine::update`
resuelve la escena en el `switch` y DESPUÉS comprueba `Playing && just_pressed(Pause)` para
pausar. Con ESC estando en pausa el `case Paused` reanudaba y esa comprobación volvía a
pausar **en el mismo frame**: ESC abría la pausa y no la cerraba nunca. La guarda es
`scene_at_frame_start`, la escena con la que entra el frame. Salió al hacer comprobable el
paso de la pausa del tutorial, que es justo el que manda pulsarla.

With special `Transition` state for room changes and `Ascension` for Act I→II switch.

El fundido de sala:

- `Engine::room_fade` (0.3 s black veil under the HUD) plays on chapter start and on the interior door (zona continua: ya no hay fundido entre celdas); particles are tinted `TileSprite::Star` sprites.
### Tacto / *game feel* (`Config::Feel`, pasada del 2026-08-25)

Todos los tunables de tacto viven en **un solo sitio**, `namespace Config::Feel` de
`core/config.h`. Ninguno toca daño, vida ni cadencia: gobiernan cuánto **pesa** un golpe.

- **Hit-stop escalado por el mordisco**: `apply_damage` congela el mundo
  `HITSTOP_MIN + (daño/vida máx) * HITSTOP_PER_BITE`, con suelo para jefes y valor fijo al
  recibir. Se toma el **máximo, nunca la suma**: un milagro de área que toca a ocho
  enemigos tiene que pesar como el peor golpe, no como ocho. Antes solo lo tenían los jefes.
- **Sacudida con envolvente y curva por evento**: `Renderer::shake_camera(mag, dur, freq)`
  (y su sobrecarga que toma un `Config::Feel::Shake`). El decaimiento ya no es *por frame*
  —a 30 fps duraba el doble— sino una envolvente cuadrática sobre `shake_timer/duration`, y
  el desplazamiento son **dos senos inconmensurables** en vez de ruido blanco, que a
  320x180 se leía como parpadeo del bordillo. Una sacudida **no pisa a otra más fuerte que
  siga viva**. Presets: `SHAKE_HIT` / `BOSS_HIT` / `PLAYER_HIT` / `KILL` / `BOSS_KILL` /
  `EXPLOSION` / `ALTAR` / `DASH`.
- **Destello del impacto**: `HIT_FLASH_TIME` 0,12 → 0,075 s y entra **casi blanco**
  virando a rojo al apagarse (`ECSWorld::collect_draws`). El rojo plano y largo se leía
  como "está envenenado", no como "le acabo de dar".
- **El disparo sale de algún sitio**: fogonazo en la boca del cayado y `SHOT_RECOIL` en el
  propio profeta. El retroceso **no pisa un knockback mayor**: el golpe recibido manda.
- **Muerte del enemigo**: pavesas + anillo + el cadáver **sale despedido** en la dirección
  del golpe (el `Movement` ya no se quita al morir: sin colisionador no estorba, y la IA
  salta los cadáveres por vida, no por componente) y se **apaga** en el último
  `DEATH_FADE_FRAC` de su animación en vez de esfumarse. El sonido depende de **qué**
  muere (`CombatEvents::kill_type`): el Agua Corrupta se derrama, el Idólatra se lleva su
  brasero, y león/profeta/general suenan más pesados.
- **Movimiento**: el profeta acelera y frena (`MOVE_ACCEL` / `MOVE_BRAKE`). Ojo — la
  fricción de `system_movement` está a **0 para el jugador** (`Player::init_as_*`): las dos
  curvas peleándose recortaban un tercio de la velocidad punta. El **dash deja estela** y
  se pinta con brillo mientras dura, que es lo único que hace legible su invulnerabilidad
  (el parpadeo de i-frames está apagado justo mientras esquivas).
- **Coyote time** (`COYOTE_TIME`): tras cargar una sala no se encadena otra transición.
- **Rumble** (`Input::rumble`, `SDL_RumbleGamepad`): impacto, golpe recibido, dash, bajas y
  jefe. Sin mando es una llamada vacía, así que el motor la pide sin preguntar.

**`spawn_vfx`: `grow` está en unidades de ESCALA por segundo, no de tamaño.** Los valores
del código rondan 0,25–3; poner 30 hace que la escala cruce el cero, el sprite se dé la
vuelta y crezca sin freno hasta tapar la sala. `system_lifetime` ya lo sujeta en 0.

**Medir antes de tocar**: `scripts/feel_baseline.txt` (comando `feel <etiqueta>` del
autoplay, que imprime el **tramo** desde la última marca: TTK, golpes recibidos, tiros,
daño hecho/recibido). Cada tramo arranca con `killall` — sin eso el bot arrastra
supervivientes del tramo anterior y el TTK sale mezclado — y con la semilla fijada la
medida es **reproducible bit a bit**. Resultado de esta pasada, antes → después:
soldados 0,88 → 0,87 s; leones 1,37 → 1,37 s; arqueros 0,79 → 0,87 s; golpes recibidos
0 en todos. Esas siguen siendo las cifras buenas. Guion propio: `scripts/feel_check.txt`.

**La sonda depende del ESTADO DEL GUARDADO, y eso costo una investigación entera**: con las
ocho mejoras de El Santuario compradas el profeta sale con +10 HP, +2 de daño, +10% de
velocidad y 60 de fe, y los leones bajaban de 1,37 a **1,09 s** — que leído a secas parece
una regresión del motor y no lo es. Por eso `feel_baseline.txt` empieza ahora con
**`defaults` + `metareset`**, y solo así es reproducible bit a bit. Comprobado además que
el ajuste de auto-mira NO mueve la medida: `aimmode` 0, 1 y 2 dan cifras idénticas, porque
el bot apunta a mano y los tres modos se reducen a usar ese vector.

### Localizacion: el juego entero en ES + EN (2026-08-25)

**Mecanismo: `tr("es", "en")`** (`core/i18n.h`), con las dos versiones **juntas en el punto
de uso**. Se descarto a proposito el clasico enum `StringID` + tabla por idioma: los dos
dan el error en COMPILACION, pero con `tr` el error es **mas duro** (falta un ARGUMENTO, no
una entrada en una tabla que nadie mira) y quien lee o revisa el codigo ve el original y su
traduccion **en la misma linea**, que es donde se detecta que una traduccion esta mal — a
cambio de no inventar ni mantener sincronizados ~600 ids. `en` vacio o nulo cae al espanol.

El idioma vive en `SaveData::language` (v7), se aplica en `Engine::apply_settings_from_save`
y se cambia en Opciones o con el comando de autoplay `lang 0|1`.

**El selector del primer arranque** (`MenuScreen::Language`, 2026-09-12). Antes de nada, el
juego pregunta el idioma una sola vez.

- Es la **unica pantalla que NO pasa por `tr()`**: quien todavia no ha elegido no puede leer
  una pregunta escrita en el idioma que no es el suyo. Rotulo y pie van en los dos a la vez y
  cada boton lleva el nombre de su lengua EN esa lengua.
- La respuesta se recuerda en un **bit de `SaveData::settings_flags`** (`SETTING_LANG_CHOSEN`),
  no en un campo nuevo: un bit mas en esa mascara no cambia el formato del fichero ni pide
  migracion. `defaults` lo limpia, asi que el selector vuelve a salir tras un reset.
- **El prologo espera a que se conteste.** Quien lanza el prologo es `main.cpp`, no `init`; con
  el selector en pantalla pone `Engine::intro_after_language` y es el propio selector quien
  llama a `start_intro()` al contestarse. Sin eso la pantalla no se veria NUNCA en un arranque
  real: el prologo tapaba el menu y `change_scene(MainMenu)` devuelve `menus.current` al Title.
- Vale con dedo y con teclado/mando sin codigo aparte: usa los mismos `mark_row_rect` /
  `menu_hit_rect` que el resto de menus, y `Cancel` (el boton atras de Android) acepta lo que
  este marcado — la primera pantalla del juego no puede tener una tecla muerta.
- Comando de guion **`langmenu`**: olvida la respuesta y abre el selector, para poder
  fotografiarlo sin borrar el guardado (`scripts/i18n_check.txt`, `ES_00` / `EN_00`).

**Los nombres de los profetas ya van traducidos** (Elias -> Elijah, Eliseo -> Elisha) en las
~115 cadenas donde aparecen: escenas, lore, encuentros, milagros, objetos, logros, capitulos y
meta-progreso. Lo que queda en castellano con esos nombres son identificadores y `SDL_Log`, que
no se pintan. Casi todo se pide con `tr()` en el momento de dibujar, asi que el cambio se ve
en el frame siguiente — pero **"no recarga nada" era falso** y costo un bug: lo que guarda el
puntero de `tr()` en una estructura de larga vida SI hay que reconstruirlo. Ver
"Cambiar el idioma EN CALIENTE", mas abajo.

Cobertura: **todo**. Menus, HUD, mapa, cartas de eleccion, mensajes y banners del Engine,
El Santuario, los cinco paneles del Codice, las cinco tablas de contenido (items, milagros,
enemigos, logros, mejoras), los **niveles de fe**, la tabla de capitulos de `rooms.h`, el
prologo, la historia, el lore y los diez encuentros. Detalles que se escapan si no se
buscan: la inicial del punto cardinal del aviso de jefe (Oeste -> **W**), y las etiquetas
de coste de las opciones de encuentro ("-15 HP máx").

Reglas que NO cambian con el idioma:
- **`Scripture::ref` sigue sin pintarse**, traducido o no. Las referencias de `lore.h` se
  quedan en espanol a proposito: son atribucion documental, no texto de pantalla.
- Los versiculos en ingles son una **traduccion del fragmento que se muestra**, con registro
  de Biblia inglesa clasica. No se declara ninguna edicion concreta.
- **Limite conocido**: el atlas se hornea con Lilita One (latino). ES/EN/PT/IT/FR entran sin
  tocar nada; **cirilico, griego y CJK no** — harian falta otra fuente y otro atlas.

Guion: **`scripts/i18n_check.txt`**, y su criterio no es "falta una traduccion" (eso ya lo
caza el compilador) sino el **otro** fallo, el que no se ve venir: `Renderer::wrap_text`
**trunca en silencio** lo que no cabe, y el ingles no siempre es mas corto. Fotografia las
mismas 14 pantallas en los dos idiomas (`ES_*` / `EN_*`) para compararlas: menu, Opciones,
Controles, Santuario, HUD, carta de objeto, encuentro (la caja mas apretada del juego) y
las cinco pestanas del Codice.

Para volver a tocar el texto en masa hay dos herramientas que merece la pena rehacer si se
pierden: un aplicador **por texto** (diccionario ES->EN) y otro **por INDICE** de literal.
El segundo existe porque transcribir a mano 230 frases en espanol acentuado para usarlas
como clave es la forma mas facil de introducir un error silencioso; por indice, el espanol
se toma **verbatim del fuente** y solo se escribe el ingles.

Resumen del mecanismo, en el contexto de la pantalla de Opciones:

**i18n** (`core/i18n.h`): el idioma es `Language` (ES base + EN) y el mecanismo es
**`tr("es", "en")`**, con las dos versiones juntas en el punto de uso. Se descarto a
proposito el enum `StringID` + tabla por idioma: los dos dan el error en compilacion, pero
con `tr` el error es aun mas duro (falta un ARGUMENTO) y quien revisa ve el original y la
traduccion en la misma linea, que es donde se detecta que una traduccion esta mal — a
cambio de no inventar ni sincronizar ~600 ids. `en` vacio cae al espanol. Limite conocido y
documentado: el atlas se hornea con Lilita One (latino), asi que ES/EN/PT/IT/FR entran sin
tocar nada y **cirilico/griego/CJK no**. Y `Scripture::ref` sigue sin pintarse, traducido o no.

### Sistema de Discípulos (facción en el ECS)

La marca de faccion (`ally`) y la regla `is_hostile` estan en `src/CLAUDE.md`.

Ya están convertidos: `count_living_enemies` (si no, un discípulo **sella la sala para siempre**),
`pick_target`, el barrido de la Vara, todos los milagros de área, las marcas de Ojos Abiertos, el
`killall` del autoplay, el recuento de invocaciones de los jefes, la arenga del General Sirio y el
brasero del Idólatra. En `system_collision` (`game/combat.cpp`) la facción decide todo: el
discípulo **golpea por contacto** al hostil (no hay código de ataque aparte), lo recibe de vuelta
—se puede perder—, **atraviesa al jugador sin tocarlo** (empujarse en un pasillo convertía la
ayuda en un estorbo) y los proyectiles hostiles sí le hacen daño. Muere sin dar fe ni botín.

- **Ejército Celestial** (`MiracleID`, 2 Reyes 6:17) invoca ya **3 guerreros durante 10 s** —
  literal como pide el GDD, no una descarga de rayos como antes. Sin entidades libres vuelve la
  descarga como respaldo, para que el milagro nunca se quede en nada.
- **La Escuela de Profetas** (mejora meta **7**, 2 Reyes 2:3): Eliseo entra en cada sala con un
  discípulo. No viaja entre salas —al cambiar de sala el mundo se vacía—: se vuelve a presentar
  en `load_room`, que es lo que el jugador percibe como "permanente". Con 8 filas El Santuario
  pasó a un paso de 14,5 unidades: a 16 la última se salía por abajo.
- La IA vive en `update_ally_ai` (`game/enemies.cpp`), **antes** de los filtros de cadáver de
  `update_enemy_ai`: es el propio discípulo quien se retira del tablero al caer o al agotarse la
  invocación. Persigue lo que elegiría el auto-apuntado y, sin hostiles cerca, escolta al profeta
  a 24 unidades.

### La entrada del jefe (2026-09-11)

El usuario probo el APK y describio un **corte brusco al llegar al jefe**: se saltaba de
golpe del mundo a la escena guionizada. Faltaba el tramo de antes.

**Donde estaba la costura** (esto es lo que decidio que el track fuera barato): la arena del
jefe esta DENTRO de la misma zona — no hay carga de sala. `update_cells` ve los pies dentro
de `arena_rect_world()` y llama a `enter_arena()`, que nace el jefe, sella el vano y deja
`pending_scene`; el dialogo arranca en el siguiente paso de `Playing`. Todo en el mismo mapa.

- **`boss_approach_t > 0`** es un estado dentro de `Playing`, **no una escena**: el mundo
  sigue vivo y dibujandose. Se comprueba ANTES que `pending_scene`, o sea que el dialogo
  espera a que el profeta llegue.
- El profeta se **empuja con velocidad**, nunca deslizando la posicion: asi `system_movement`
  le pone el rumbo y `system_animation` la fila de andar, exactamente igual que si caminara
  el jugador. Frena cerca del destino para no llegar derrapando.
- **El destino es el punto al que `start_scene` teletransporta al profeta**
  (`tile_to_world(anchor + def.player_dx/dy)`), y eso no es un detalle: si fuera otro, ese
  teletransporte se veria como un salto y el corte seguiria ahi, dos segundos mas tarde.
- `camera.set_room(arena_bounds, **false**)` en `enter_arena`: con `true` el recorte saltaba
  en el mismo frame en que nacia el jefe, y ese salto era la mitad del corte. Durante el
  paseo la camara ademas se **adelanta** hacia el jefe con una curva `4k(1-k)` (0 -> pico ->
  0), para volver al profeta justo al final.
- **Al acabar, la camara se ASIENTA** (`room_bounds = bounds_goal`): en `Dialogue` nadie la
  actualiza, asi que un paseo saltado a mitad dejaba el recorte a medio deslizar y la escena
  empezaba con la cabeza del jefe cortada.
- **Se puede saltar con `Cancel`** (el boton atras en Android). Y el paseo va **atado al
  dialogo**: quien vuelve tras morir ya lo vio (`boss_talk_seen`) y lo que quiere es pelear.
- Los tunables viven en `Config::Feel` (`BOSS_APPROACH_MAX/SPEED/LEAD`) porque son tacto, no
  reglas. Durante el paseo `input.touch_gameplay` es false: no se dibuja ni se atiende ningun
  control tactil, que un stick que no responde es peor que ninguno.

Guion: **`scripts/entrada_jefe_check.txt`**, con el comando de autoplay nuevo **`entrada`**.

### Cambiar el idioma EN CALIENTE (`Engine::relocalize`, 2026-09-12)

**`tr()` no copia nada: devuelve un puntero al literal del idioma activo en el momento de la
llamada.** Casi todo el juego lo llama al pintar, así que cambia solo; lo que se rompía es lo
que GUARDA ese puntero en una estructura de larga vida, porque se queda congelado en el
idioma de entonces. Cambiar el idioma en Opciones dejaba "Honda de David" en una partida en
inglés — el usuario lo llamó intolerable, y tenía razón.

`Engine::relocalize()` reconstruye los tres sitios donde eso pasa, y **la llaman los dos
caminos por los que el idioma puede cambiar**: `set_language` (selector del primer arranque y
comando `lang`) y `apply_settings_from_save` (la fila de Opciones):

- `ItemRegistry` — nombre y efecto de los 22 objetos (HUD, cartas de elección, Códice).
  `init()` pasó a ser **idempotente** (`items.clear()` al entrar): sin eso, la segunda
  llamada duplicaba el catálogo.
- `ProgressionSystem::upgrades` — las 15 mejoras de El Santuario. `init()` deja `unlocked` a
  cero, así que detrás va **`sync_progression_from_save()`** o se perderían las compradas.
- `Engine::boss_bar_name` — el rótulo de la barra del jefe que se esté peleando.

**Si algún día otra estructura de larga vida guarda un `tr()`, va ahí.** Lo transitorio (un
mensaje del HUD, una carta abierta, una página de historia, el resultado de un diálogo) no
cuenta: dura segundos y nadie cambia el idioma a mitad.

Nadie guarda un `const ItemDef*` de larga vida, y por eso reconstruir el vector es seguro; si
algún día alguien lo guardara, quedaría colgando.

Guion: **`scripts/i18n_caliente_check.txt`**. Su gate es el `LOADOUT` de `stats` **en la misma
partida**: "Honda de David, Agua del Jordán" → `lang 1` → "David's Sling, Water of the Jordan"
→ `lang 0` → otra vez el primero, más una línea `i18n: registros re-traducidos (...)` por
cambio. Las capturas cubren El Santuario y la barra del jefe.
