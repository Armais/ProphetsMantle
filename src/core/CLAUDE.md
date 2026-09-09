# `src/core/` - motor, escenas, tacto e i18n

Reglas transversales a todo `src/`: `../CLAUDE.md`.

### Scene State Machine

`GameScene` enum: `Intro → MainMenu → Playing → Paused → Choice → GameOver / Victory`

`Intro` es el prólogo cinemático (ver abajo). Al añadir un valor a `GameScene` hay que
ampliar **tres** switches a la vez: `Engine::update`, `Engine::render` y
`Engine::update_music` — este último corre antes del switch de escena y pisa cualquier
`play_music` manual, así que una escena sin `case` ahí se queda muda.

`Choice` is the "elige 1 de 3" screen (items or miracles) drawn over the frozen world.

With special `Transition` state for room changes and `Ascension` for Act I→II switch.

El fundido de sala:

- `Engine::room_fade` (0.3 s black veil under the HUD) plays on every `load_room`; particles are tinted `TileSprite::Star` sprites.
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
y se cambia en Opciones o con el comando de autoplay `lang 0|1`. **No recarga nada**: el
texto se pide con `tr()` en el momento de dibujar, asi que el cambio se ve en el frame
siguiente.

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
