# Encuentros, relatos y retratos

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`events.h`, `lore.h`) y a la
caja de dialogo de **`src/ui/`**. El arte de los retratos y las vinetas lo genera
`tools/blender/` (ver su `CLAUDE.md`).

### La historia en ORDEN: coherencia intro -> capitulos con 1-2 Reyes (2026-08-31)

Mandato del usuario: *"tiene que haber un seguimiento entre la intro y luego capitulo a
capitulo"* — su ejemplo: el encuentro con Acab en el cap. 1 no tiene sentido, porque Querit
viene DESPUES de esa conversacion en palacio. La regla que quedo (documentada en la cabecera
de events.h): **nada que el prologo o un capitulo anterior ya conto se vuelve a jugar como
presente**. Esta seccion SUPERA los recuentos de la del 2026-08-30. Lo que cambio:

- **Cap. 1: fuera la audiencia «ni rocio ni lluvia»** (1 R 17:1 es la estampa 6 del prologo).
  En su lugar, `Ev::RASTREADOR_DE_ACAB` (1 R 18:10): un explorador del rey baja al canon
  siguiendo el agua — la sequia jurada es lo que trae la caceria al arroyo, y desarmarlo
  anuncia al jefe (el Capitan de Acab). Reusa las celdas del oficial real (retrato 0 y
  `Npc::OFICIAL_REAL`): cero arte nuevo.
- **Cap. 6: fuera el cruce del Jordan** (2 R 2:13-14 lo cuenta la cinematica del Ascenso).
  En su lugar, `Ev::CINCUENTA_BUSCAN` (2 R 2:15-18): los cincuenta que insisten en buscar a
  Elias por montes y valles. La vineta `Portrait::MANTO_JORDAN` (celda 34) queda LIBRE pero
  dibujada — no reordenar la hoja.
- **Hazael y la uncion de Jehu (2 R 8-9) se mudaron del cap. 9 al FINAL del cap. 10**: en el
  libro van despues del sitio de Samaria, y ver a Ben-adad en su lecho de muerte un capitulo
  antes de pelearlo como jefe mentia sobre el orden. Hazael abre ahora con "roto el cerco"
  (mismo pacto de flash-forward que "Siete anos despues" del cap. 8) y la redoma de Jehu es el
  ultimo relato del juego: el epilogo la cobra en su primera escena. El cap. 9 recibe
  `Ev::TIERRA_DE_ISRAEL` (2 R 5:15-19, Naaman limpio: la tierra de dos mulas, el perdon de
  Rimon y el presente rechazado — la tentacion exacta de Guiezi, su jefe).
- **Bisagras en las intros** (lore.h): cap 3 abre con «Ve, muestrate a Acab» (18:1, lo que
  saca a Elias de Sarepta), cap 5 con el encargo de Horeb, cap 6 con el manto cruzando el
  Jordan (y ya no narra la sal ANTES de que el jugador decida echarla), cap 10 con la guerra
  siria tras la carne nueva de Naaman. Caps 2 y 4 ya empalmaban solos.
- **Numeros vigentes**: pozos **3/3/4/4/3/4/5/4/3/6 = 39** relatos; `Ev::COUNT` **46 de 64**;
  encuentros_check pide **78 aperturas**. `CHAPTER_LAYOUT` admite el digito **'6'**; el cap. 9
  volvio su '4' a 'T' (recupera el Tesoro perdido) y el cap. 10 pone '5' y '6' en dos 'N' —
  o sea que **el reparto de salas de los caps. 9 y 10 se redibujo** para todo el mundo, con la
  podredumbre habitual de coordenadas pinneadas si algun guion las tuviera alli.
- Los bits de `codex_events` a partir del viejo HAZAEL se desplazan — convencion aceptada y
  declarada en events.h (el Codice es informativo).

### Los relatos van SIEMPRE anclados a Elias o Eliseo (2026-08-30)

Mandato del usuario, y es el que MANDA sobre cualquier ampliacion de historia: *"este juego va
sobre la historia de Elias y Eliseo, no de David ni de Salomon"*. El mismo dia se habia
estrenado una cobertura del libro ENTERO de Reyes (cadena "El Reino Partido" con 7 paradas de
1 R 1-16 antes del cap. 1, epilogo ampliado a 9 escenas hasta la caida de Jerusalen, +5
encuentros contemporaneos) y el usuario la rechazo al jugarla — la cadena inicial de David y
Salomon "no tiene ningun sentido". **Se revirtio lo que no era de los profetas**:

- **Fuera la cadena "El Reino Partido" entera**: ids `Ev::PRE_*`, maquinaria
  `pre_step`/`prechain_*` del Engine, el hook al cierre de la intro del cap. 1, el comando de
  autoplay `reino` y el guion `reyes_check.txt`. La unica mencion que queda del reino partido
  es la linea de contexto del prologo cinematico ("despues de Salomon, el reino se partio en
  dos"), que es contexto de la persecucion de Elias, no protagonismo.
- **Fuera "El profeta vendado" (1 R 20) y "Micaias" (1 R 22)** del pozo del cap. 5 (y con el
  primero, su leon de `apply_event_choice`): Elias no esta en esas escenas — el juego ponia al
  JUGADOR en el papel de otros profetas. Pozos ahora **3/3/4/4/3/4/5/4/4/4 = 38**; el layout
  del cap. 5 en `CHAPTER_LAYOUT` volvio sus celdas '4'/'5' a 'N' (el digito '5' sigue valiendo:
  lo usa el cap. 7).
- **El epilogo vuelve a 3 escenas** (`EPILOGUE_STOPS = 3`): Jehu/Jezabel (cumple la palabra de
  Elias sobre la vina de Nabot), el arco de Joas (el lecho de muerte de Eliseo, `joash_full`
  sigue cambiando las ultimas palabras) y los huesos de Eliseo. Con ~4,2 s por escena en
  autoplay la victoria con epilogo vuelve a ~18 s: guardado_check y logros_check esperan 22 s
  tras `advance` (con el epilogo de 9 eran 48).
- **El criterio para un relato nuevo**: entra si Elias o Eliseo estan en la escena, o si la
  escena cumple una palabra suya. Si quedaron: Hiel de Jerico (se le cuenta a Eliseo el porque
  de las aguas malas que va a sanar), el rey de Moab (la campana de 2 R 3 es de Eliseo) y la
  redoma de aceite (Eliseo envia al ungidor de Jehu).
- **Ids y guardado**: `Ev::COUNT` queda en **45 de un techo de 64** (38 + 4 Ascenso + 3
  epilogo; u64 `codex_events`, guardado v9 intacto). Quitar ids DESPLAZA los bits ya guardados
  — aceptado por convencion declarada en events.h (el Codice es informativo). El contador
  "Descubierto X/Y" itera `Ev::COUNT` y se ajusta solo.
- Guiones actualizados: encuentros_check (76 aperturas, cap 5 con 3), epilogo_check (3
  capturas), guardado_check y logros_check (espera 22 s); `reyes_check.txt` borrado.

### El pozo de encuentros: 10 -> 35 relatos (GDD2.0 fase 2, 2026-08-26)

Lo que el usuario puso por escrito: *"el flujo de la historia tiene que tener en cuenta TODO LO
REFERENTE A ELIAS Y ELISEO DEL LIBRO DE REYES"*. Habia **10** encuentros y ahora hay **35**, uno
por cada episodio que `GDD2.0.md` §2 enumera para los diez capitulos.

**Un POZO por capitulo, no un encuentro.** `get_event_def` toma ahora un **id** de
`namespace Ev` (game/events.h) y `get_chapter_events(capitulo)` devuelve `{primero, cuantos}`:
el orden del enum ES la tabla, y por eso los ids estan agrupados por capitulo. Un encuentro
nuevo se anade AL FINAL DE SU CAPITULO.

- **El generador coloca TODAS las escenas del pozo** (3-4 salas `Event` por capitulo) en las
  celdas que fija la tabla `CHAPTER_LAYOUT` (mandato de mundo fijo del 2026-08-27: la
  historia es configuracion, no sorteo — antes se colocaban 2-3 de una bolsa barajada y en
  una partida no se veian todos). Cada sala cuenta una escena DISTINTA; lo que hace que
  explorar siga pagando es que 3-4 salas del mapa son relato, no que se sorteen.
- El id viaja en `FloorMap::RoomNode::event_id` -> `Room::event_id` (cuarto parametro de
  `Room::generate`, con valor por defecto -1). **No se deduce del capitulo**: eso es lo que
  antes obligaba a que hubiera una sola sala Event por mapa.
- `DialogueState::event_id` y `Engine::apply_event_choice(event_id, opcion)`. El switch de
  efectos es **plano por id**, no por capitulo.
- `FloorMap::generate` loguea `relatos=N/M`: es lo unico que permite comprobar desde un guion
  que el reparto no se ha roto.

**Cada encuentro tiene una decision con coste real y ninguna de las dos opciones es gratis.**
Los que son MILAGRO (la tinaja, el niño resucitado, el hacha que flota) dan efecto de partida;
los que son decision MORAL (el tercer capitan de Ocozias, los sirios de Dotan) mueven la fe.
Dos detalles que valen para los que vengan:

- **Elegir contra el texto no contradice el texto.** "Encender tu el fuego del Carmelo",
  "callar ante Acab" o "maldecir a los muchachos de Betel" son decisiones del JUGADOR en su
  partida, no afirmaciones sobre lo que paso. La regla de fidelidad se rompe cuando el JUEGO
  afirma algo falso (el caso del "Leproso Enloquecido"), no cuando el jugador falla.
- **`add_damage` tiene suelo (4)**: los sirios de Dotan cobran su precio en DAÑO y no en fe —
  dejas ir a un ejercito que tenias contado—, y sin suelo la eleccion piadosa dejaba el cayado
  en nada.
- El objeto 21 (Panes de Cebada) **es** 2 Reyes 4:43, asi que repartir los panes te lo da.

**`Renderer::wrap_text` ya no trunca en silencio.** Era el unico fallo de texto que no se ve
venir —el ingles no siempre es mas corto y la caja de dialogo con retrato pierde ~35 unidades
de ancho—, y con 35 encuentros en dos idiomas revisarlo a ojo son 140 capturas. Ahora emite
**`TEXT TRUNCATED (N lineas de W u): "..."`**, una vez por texto y con techo de 96 (una caja se
redibuja cada frame: sin el techo saldrian sesenta lineas por segundo). El gate de cualquier
guion de texto pasa a ser **cero lineas de esas**. Comprobado: los 23 guiones de `scripts/`
pasan con cero, o sea que TODO el texto del juego cabe en ES y en EN.

**El Codice cuenta el relato** (`SaveData::codex_events`, mascara de 64 bits, **guardado v8**
con su `SaveDataV7` + `migrate_v7`): la pestana de Capitulos ensena **"Relatos N/M"** sobre el
pasaje, y el contador global pasa de 50 a **85** (10 capitulos + 8 enemigos + 10 milagros +
22 objetos + 35 relatos). Se marca al ABRIR el encuentro, no al resolverlo: el relato ya lo has
leido. `migrate_v7` deja `codex_events` a cero a proposito — el fichero viejo no guarda CUALES
se vieron, e inventarselo seria mentir en el contador.

**Retratos y PNJ (fase 3).** Seis locutores nuevos con receta en `characters.py`, y cada uno
existe para que se distinga de los demas a 24 unidades: `abdias` (el UNICO cano del juego),
`hijo_profetas` (el unico con la cabeza descubierta y el pelo largo suelto), `hombre_baalsalisa`
(gama de la mies), `muchacha_cautiva` (la UNICA mujer sin velo — una esclava no lleva velo de
casada, y eso dice lo que es), `leproso_samaria` (capucha abierta y ceniza; los cuatro leprosos
son los HEROES de 2 Reyes 7, nunca enemigos) y `principe_samaria` (azul y plata, para no
confundirlo con Naaman ni con el General Sirio). Ademas, **`acab`, `general_sirio` y
`soldado_acab` tienen ahora celda en `npcs.png`** aunque ya tengan hoja de personaje: el PNJ se
dibuja desde `npcs.png` **y de ningun otro sitio**, asi que sin celda el rey Acab en su sala
seria una hoguera.

**Las vinetas de los que son ESCENA**: siete encuentros mas llevan vineta en vez de cara (el
arroyo seco, los dos pensamientos, la nubecilla, el enebro, las tres unciones, los siete mil y
el manto sobre el Jordan). **Regla que costo tres pasadas y vale para cualquier vineta futura**:
la camara de vineta va casi de FRENTE (13 grados de picado), asi que **un hundido del terreno no
se ve** — lo tapa lo que tenga delante. Las que funcionan tienen por sujeto un OBJETO, no un
relieve: por eso el cuervo, el altar y la boca de la cueva salieron a la primera, y por eso el
arroyo seco acabo siendo un monton de cantos de rio con un ultimo charco, y no un cauce.

**Dos se quedan sin imagen a proposito** y llevan `Portrait::NONE`, que es un estado
**soportado** (la caja se dibuja identica a antes de que el campo existiera): "el valle de las
zanjas", porque una zanja es justo el relieve que esta camara no puede ensenar, y "en el camino
de Betel", porque lo que hay que mirar ahi es una burla y no una cosa.

### Retratos en los encuentros (2026-08-25)

Los diez encuentros de `game/events.h` llevan imagen, y son de **dos clases**. Siete
locutores son personas y tienen su **retrato**. Los otros tres —"Los cuervos de Querit"
(cap. 1), "El altar del Carmelo" (cap. 3) y "La boca de la cueva" (cap. 4)— **no son
personajes: son la escena**, y ponerles una cara sería mentir sobre lo que el jugador está
mirando, así que llevan una **viñeta** de lo que el pasaje describe. El campo es el mismo
(`EventDef::portrait`): una viñeta es una celda más de la hoja, no un sistema aparte.

- **`EventDef::portrait`** es un índice de `namespace Portrait`, con `Portrait::NONE` = sin
  cara. Va **el último del struct y con valor por defecto**: así los encuentros que no lo
  nombran compilan igual y **no heredan el retrato 0** (que sería el Soldado de Acab).
- **`namespace Portrait` vive ahora en `render/sprites.h`**, no en `ui/codex.h`. Lo nombran
  el Códice *y* los encuentros, y `game/` no puede incluir `ui/`.
- **La caja de diálogo** (`ui/hud.cpp: draw_dialogue`): retrato de **32 unidades** a la
  izquierda, y el cuerpo *y* las opciones se corren a su derecha. La celda mide 24 unidades
  (96 px): a más de ~34 se ve blanda, así que se dibuja a 32 y **no** se re-renderizó la
  hoja a 128 px. Con retrato la columna pierde ~35 unidades de ancho, y como
  `Renderer::wrap_text` **trunca en silencio** lo que no le cabe, el cuerpo pasa a admitir
  una **cuarta línea** (4 líneas + 2 opciones = 70 de las 78 unidades de la caja). Sin
  retrato todas las coordenadas son las de siempre: un encuentro sin cara se ve **idéntico**
  a antes de que el campo existiera.
- **Hoja**: `assets/sprites/portraits.png` paso de 6x2 a **6x4, 22 celdas**, y con el pozo de
  encuentros a **6x5, 28 celdas** (293 KB; `assets/` sigue en 25 MB). Los **ocho primeros siguen siendo `EnemyType`** — el Bestiario
  indexa por tipo sin tabla de traducción — y lo nuevo se **añade al final**: del 12 al 18
  las personas, en orden de capítulo, y del 19 al 21 las tres viñetas.
Guiones: **`scripts/encuentros_check.txt`** recorre los diez con el comando de autoplay
**`event <n>`** y fotografía cada uno dos veces (una línea del cuerpo y la decisión con sus
dos opciones). En los dos capítulos cuya línea es más larga se fotografían **las tres
líneas**, porque el truncado silencioso de `wrap_text` es el único fallo que no se ve venir.
