# Encuentros, relatos y retratos

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`events.h`, `lore.h`) y a la
caja de dialogo de **`src/ui/`**. El arte de los retratos y las vinetas lo genera
`tools/blender/` (ver su `CLAUDE.md`).

### Cada relato con su estampa, y la caja de encuentro rediseñada (2026-10-03)

Peticion del usuario: «los encuentros con los personajes secundarios tambien deben de tener
mejores graficos y los eventos secundarios deben de tener un grafico representativo de cada
uno». Cada id de `Ev::` (los 39 del pozo, las 4 paradas del Ascenso y las 3 del epilogo) tiene
su lamina en `assets/sprites/estampas/ev_NN.png` (`tools/estampas`, ver `tools/CLAUDE.md`).

- `Engine::open_dialogue` llama a `load_event_plate`: carga la del relato y descarga la
  anterior (solo hay una viva). Las cajas de escena (`Sc::LINE_BASE` en adelante) no llevan.
- `DialogueView::plate` + `open_t`: con lamina, `draw_dialogue` la enmarca ARRIBA en el hueco
  que deja la caja (alto 44-88, proporcion de la textura), con fundido y acercamiento de
  entrada (0,45 s), un brillo que recorre la moldura y un Ken Burns lentisimo mientras se lee.
  El titulo del relato va en una cinta roja sobre el borde inferior del marco y deja de
  repetirse en la placa del nombre.
- **La geometria del TEXTO no se movio ni una unidad** (mismo `text_x`, `text_w`, alto de
  caja): el gate de cero `TEXT TRUNCATED` de los guiones sigue valiendo tal cual. Lo que
  cambio es el aspecto: caja con filo dorado y degradado (`draw_text_box`), retrato con
  marco y halo, placa del nombre y banda de la opcion elegida en paneles redondeados.

### Las consecuencias se VEN, y obedecer sube la fe (2026-09-24)

Informe del usuario tras jugar el cap. 1: *"en la parte del arroyo, al seguir la voluntad de
Dios deberia aumentar tu fe y no lo hace"* y *"se tiene que ver claro que consecuencia tiene
cada dialogo"*. El motor SI sumaba +18 en «Quedarte donde te mandó», pero la caja solo
pintaba un "coste" escrito a mano (`EventOption::cost`, "-12 HP") y callaba lo que se ganaba;
ademas el goteo de fe por inactividad (-0,5/s a los 10 s sin atacar) se la comia enseguida.

- **Una sola tabla**: `get_event_effect(id, opcion)` al final de `events.h` (fe, dolor, vida
  maxima, dano, cura, escala del jefe, objeto y un `EventSpecial` para lo demas). La aplica
  `Engine::apply_event_choice` y la pinta la caja: `Engine::build_event_chips` la convierte
  en etiquetas de color ("+18 Fe" dorado, "-12 HP" rojo, lo que se gana en verde, un objeto
  en azul) junto a CADA opcion y, tras decidir, bajo el texto del resultado. `EventOption`
  perdio el campo `cost`: lo que se lee y lo que pasa ya no pueden divergir.
- **La regla**: seguir la voluntad de Dios SUBE la fe, en todos los relatos. La opcion fiel
  cuesta cuerpo (HP) y da fe; la mundana cuesta fe y da algo tangible (dano, vida, cura).
  Se voltearon las doce opciones fieles que restaban fe (el milagro "gastaba" fe: verter los
  cantaros del Carmelo, mandar a Naaman al Jordan, la harina en la olla, el palo en el
  Jordan, orar por los ojos del criado...) y las que daban MAS fe al desobedecer (ayunar ante
  el pan de los cuervos, no comer la torta del angel, parar el aceite, callar ante el rey).
  La despedida de Eliseo y el aposento sunamita no tienen opcion desobediente y se quedan.
- Decidir resetea la inactividad de la fe, y la escala del jefe usa `min` (Abdias ya no
  deshace el altar del Carmelo).
- El log dice lo mismo que la caja: `Event choice <id> opcion <n>: +18 Fe | -12 HP`.
- La caja de opciones mide lo que necesita (minimo 54, como la de texto): las etiquetas van
  a la derecha de la opcion si caben y, si no, en una segunda fila debajo.

### Espinas de los caps. 1, 2 y 4, y el jefe que responde al pozo (2026-09-10, noche)

Veredicto del usuario al jugar el cap. 1: *el dialogo se repite con la cinematica*. Verificado:
la ultima caja de la audiencia («Beberas del arroyo; y yo he mandado a los cuervos...», 17:4)
y la PRIMERA linea de la intro del cap. 1 eran identicas byte a byte, con dos segundos entre
medias; el mismo patron estaba en el cap. 3 (la convocatoria, 18:20, ultima caja de la
apertura Y linea 0 de la intro). La regla que ya declaraba `lore.h` se aplico ahora a los
caps. 1, 2 y 4, que no tenian columna vertebral escrita:

> **El prologo ANUNCIA, la apertura JUEGA, la intro SITUA, el pozo MUESTRA, el jefe responde a
> lo jugado, la salida deja al profeta donde lo recoge la apertura siguiente.** Cada versiculo
> o hecho se dice en UN solo eslabon por capitulo.

- **Cap. 1** (17:1-7 + 18:10): prologo estampa 7 = 17:2-3 anunciado («la palabra lo mando al
  oriente, a un arroyo escondido»; ya no narra la comida de los cuervos; conserva su arte) ->
  audiencia 16:30-33, 18:4, 17:1, 18:19, 18:10 (Jezabel), 17:2-4 -> intro **17:5** («Y el fue
  e hizo conforme a la palabra de Jehova») + una linea que situa el cauce -> pozo: cuervos
  (17:6), rastreador (18:10, «Acab ha hecho jurar a reinos enteros»), arroyo seco (17:7) ->
  jefe: el capitan YA NO cita 18:10; base «todo Israel te busca... segui el unico arroyo» y
  **variante** si el rastreador se resolvio («mi explorador siguio el agua; yo segui a mi
  explorador», vale para las dos opciones) -> salida: el arroyo callado, sin cuervos y sin
  palabra todavia (17:8-9 es la apertura del 2). Fuera de la salida: «cayo el capitan» (el
  banner lo dice) y el arroyo seco (pozo).
- **Cap. 2** (17:8-24): apertura 17:8-11 -> intro 17:14 + la casa junto a la puerta y «el
  sacerdote de la puerta no te ha quitado el ojo» (fuera el punado de harina, que es 17:12 =
  el pozo, y «Sarepta es de Baal», que lo dicen el sacerdote de la apertura y el jefe) -> pozo
  17:12, 17:14-16, 17:17-24 -> jefe 17:9 / 17:1 / 18:4 -> salida 17:24 (afirma el canon aunque
  el pozo eligiera consolar a la madre).
- **Cap. 3** (18): apertura 18:1, 18:17-19, 18:24, 18:20 -> intro **18:22** («solo yo he
  quedado profeta de Jehova; mas de los profetas de Baal hay 450») + Acab en su carro + 18:23
  (fuera la convocatoria y 18:21, que es «Todo el pueblo en el monte») -> pozo 18:33, 18:3-4
  (Abdias), 18:21, 18:41-46 («Despues del fuego», con la marca) -> jefe: abre **el sumo
  sacerdote** por todos (el jefe es un GRUPO desde hoy) sin la cifra, y **variante** si se
  jugo a Abdias («esconde profetas en las cuevas de este monte; iremos tambien por ellos»,
  vale para las dos opciones) -> salida 18:39 («Jehova es el Dios», sobre su rostro) y Elias
  a la puerta de Jezreel esa noche, empapado: NI fuego NI lluvia, que ya se jugaron.
- **Cap. 4** (19:1-18): apertura 19:1-2, 19:3 («un dia de camino») -> intro **19:9** («y alli
  se metio en una cueva») + Horeb de noche (fuera «un dia de camino / cuarenta dias», que son
  la apertura y el enebro, y 19:12, que era el resultado de «La boca de la cueva» palabra por
  palabra) -> pozo 19:11-12, 19:4-8, 19:15-16, 19:18 -> jefe 19:4 como eco reconocido, 19:10,
  19:2 como eco, 17:1 -> salida: baja de Horeb y el labrador de Abel-mehola «todavia no sabe
  nada» (fuera «vuelve por tu camino» y los siete mil, dos relatos del pozo).

**El jefe responde al pozo (motor):** `SceneDef::alt_first_line` + `alt_event` (scenes.h).
`Engine::scene_next` usa la variante como PRIMERA caja si `event_resolved_in_run(alt_event)`;
el resto de la cadena no cambia, y los ids de las variantes van al FINAL del enum `Sc` para
no mover ninguno. `event_resolved_in_run` NO lee `codex_events` (es el Codice: persiste entre
partidas y un jefe respondería a un relato jugado en otra run): lee `Engine::run_events_done`
(bit al cerrar cualquier relato, tambien por el comando `event`; se pone a cero al empezar
partida y al Continuar) o `event_used` de la sala Event que cuenta ese relato (lo que
sobrevive al Continuar: start_chapter rehidrata room_cache desde el v9). Hoy solo lo usan
los jefes 1 (rastreador) y 3 (Abdias); el mecanismo queda para el resto. El log dice `parada
1/N (id X) variante`, y `escenas_check` tiene una cuarta pasada que lo exige (gate 8).

### Voz humana y coherencia capitulo a capitulo (2026-09-10, tarde)

Veredicto del usuario al jugar las escenas guionizadas: *los dialogos suenan roboticos, a
IA*, y *a partir del cap. 2 la historia se contradice* entre aperturas, intros, pozo y jefes.
Se reescribieron las tres tablas de texto (`scenes.h`, `lore.h`, `events.h`) con dos
decisiones suyas: **registro arcaizante, tipo Reina-Valera 1960 tambien en lo inventado**
(KJV en ingles; las citas, verbatim), y **la apertura del cap. 7 es la viuda del profeta
(2 R 4:1-4)** en vez de los tres reyes de Edom, que se funden en «El valle de las zanjas».
Esta seccion SUPERA la lista de aperturas de la seccion siguiente.

**Guia de estilo (ES y EN):**

- Registro del libro: vosotros, «he aqui», «senor mio», «tu siervo / tu sierva», «varon de
  Dios», «Vive Jehova». Frases cortas, preguntas directas, juramentos, sustantivos concretos
  (harina, odre, yunta, vasijas). En ingles, thou/thee/thy en el trato singular.
- **Una voz por personaje**: Elias seco y sin explicar lo que siente (jura y pregunta, nunca
  describe la escena); Eliseo «senor mio» con Elias y autoridad tranquila en el Acto II
  (los demas le llaman «hijo de Safat»); Acab rey ofendido con ironia pesada; Jezabel fria y
  breve, sin exclamaciones; Guiezi cuenta el dinero; la viuda y la mujer del profeta cansadas
  y concretas; la sunamita digna, no pide nada; Joram quejica; Josafat pregunta por Jehova;
  los profetas de Baal orgullosos; la Muerte / el Espiritu / la Desesperacion cortos y frios,
  sin adivinar el futuro; «La palabra de Jehova» solo cita, sin retrato.
- **Prohibido**: mas de una sentencia bimembre por ESCENA («El aceite se acaba. Yo no»);
  cadenas «no es A, sino B»; cerrar una caja con aforismo salvo que sea versiculo; narrar lo
  visible («una viuda recogiendo dos lenos», dicho por Elias); apartes omniscientes («es la
  primera de tres veces que lo va a decir»); exclamaciones fuera del grito ritual de Baal;
  lexico moderno («espectaculo», «plan», «carta» por mensajero); cifras de anos de sequia
  fuera de «al tercer ano» (18:1).
- **Coherencia**: la intro NUNCA repite un versiculo ni un hecho que la apertura acaba de
  jugar (arranca donde la apertura termina); el jefe NUNCA estrena algo que el pozo ya jugo
  ni da por hecha una opcion del pozo (Dotan, el baculo: sus lineas valen para las dos); un
  relato posterior al jefe lleva **marca de flash-forward en su primera frase** («Despues del
  fuego», «Muerto Acab», «El ultimo dia del cerco», ademas de «Siete anos despues» y «Roto el
  cerco»); los outros afirman el canon y solo se tocan cuando duplican.
- **Techos**: cada `lines[i]` es una pagina; con retrato 4 lineas a 244 u, sin retrato 3 a
  280 u; `objective` en una linea del mapa grande (sin wrap ni gate, ~70 caracteres). El gate
  real es cero `TEXT TRUNCATED` en los nueve guiones de texto.

**Espinas nuevas** (apertura -> intro -> pozo -> jefe -> outro):

- **Cap. 3**: apertura de SEIS cajas — la Palabra 18:1 («Ve, muestrate a Acab») al frente,
  Acab 18:17, Elias 18:18-19, el profeta de Baal que responde a la CONVOCATORIA, Elias 18:24
  (el reto del fuego) y Acab «sea» (18:20, y lo que el quiere es lluvia). Intro desde 18:20.
  «Abdias» pasa a ser el encuentro camino del monte con los cien profetas escondidos
  (18:3-4, 13: subirlos al Carmelo / dejarlos en las cuevas); «La nubecilla» se titula
  «Despues del fuego» y abre con la marca.
- **Cap. 5**: apertura = el llamamiento (19:19-20), cuatro cajas: «Junto a las doce yuntas»
  (sin retrato: el manto echado al pasar), Eliseo «dejame besar a mi padre», Elias «ve,
  vuelve», Eliseo «hay un par de bueyes que matar». El manto sigue en Elias hasta 2 R 2:13.
  Intro: Eliseo le sirve (19:21) y Nabot (21) en dos frases. Pozo: «La despedida de Eliseo»
  (19:21: comer con ellos / seguir sin esperarle, mismos efectos) y «La palabra de Jehova»
  sobre la vina (21:17-19) SIN Acab ni retrato ni PNJ (la sala pone la hoguera; Acab es el
  jefe). «El tercer capitan» abre con «Muerto Acab».
- **El Ascenso**: `get_ascension_lore(page, asked_double)`, `cinematic_ascension(c,
  asked_double)` y `story_ascension(s, asked_double)` bifurcan la peticion de la pagina 0 y
  la linea de la herencia (`get_ascension_inheritance_line`) por `Engine::ascent_double`; los
  tres llamantes del Engine (paginas, cinematica y `asccine`) pasan el valor de la sesion.
  Las paradas pierden los apartes («lo dice sin mirarte, y sigue andando»).
- **Cap. 6**: los hijos de los profetas lo vieron «desde la orilla, enfrente» (2:7); Eliseo
  no llama muerto a Elias. La intro no cita 2:19 (es el pozo) y «Los hombres de Jerico» ya NO
  sana el manantial (lleva la sal al manantial; sanarlo es el jefe y el outro).
- **Cap. 7**: apertura = la viuda del profeta en su casa (`StageKind::Interior`, bioma 7,
  `Npc::MUJER_PROFETA` y `Npc::CRIADO_ELISEO` como en la 8): 4:1, 4:2, «una vasija de
  aceite», Guiezi (el acreedor a la puerta con dos hombres; la ley le da los ninos) y 4:3-4.
  Intro: la viuda de puerta en puerta, el acreedor, y «la guerra de Moab sigue en el sur, y
  hay hambre en Gilgal» (prepara los relatos). Pozo: «La mujer del profeta muerto» es el
  derramamiento (4:5-6: llenar hasta la ultima vasija de la aldea / llenar las de casa y
  parar) y «El valle de las zanjas» absorbe a los tres reyes (3:9-23). El Acreedor habla de
  la ley y de sus dos hombres; Eliseo no le recita 4:7 (es el outro, a la viuda).
- **Cap. 8**: apertura = el ofrecimiento y la promesa (4:9-16), SEIS cajas: la sunamita
  4:9-10, Eliseo 4:13, «yo habito en medio de mi pueblo», Guiezi 4:14, Eliseo 4:16, «no hagas
  burla de tu sierva» + su ruego. Intro: nace (4:17), grita entre los segadores (4:18-19),
  muere y ella cierra la puerta (4:20-21). Pozo: «La sunamita» son los anos en el aposento y
  «El nino de Sunem» la madre en el Carmelo (4:25-30: ir tu / mandar a Guiezi con el baculo),
  sin resucitar: el aposento cerrado es la Muerte y los siete estornudos el outro. La Muerte
  no repite «¡ay, mi cabeza!» y Eliseo no da por hecho el baculo.
- **Caps. 9 y 10**: Eliseo le habla al rey a la cara (5:8, segunda persona); la muchacha
  cautiva viene en la comitiva del general (licencia minima que la pone en escena); outro
  5:27 («salio de delante de el leproso, blanco como la nieve»). Joram sin marcador de tres
  avisos; intro 6:12-14 (Ben-adad sabe quien avisa, «en Dotan», el cerco de noche); «Los
  cuatro a la puerta» abre con «El ultimo dia del cerco»; Ben-adad vale para las dos opciones
  de Dotan; outro = el cerco roto (7:6-7, 16).
- **Motor**: `ChapterLore::boss_line` ha desaparecido (nadie lo leia desde que los jefes
  hablan en `scenes.h`). Guion: `scripts/escenas_check.txt` (waits de AP3 y AP8, captura
  `es07_ap7_viuda_profeta`, gate 7: contar `parada k/N`).

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
  - **Vuelve como APERTURA guionizada (2026-09-09)**, no como relato del pozo: el palacio
    montado con el motor del gameplay (interior de silleria, `spawn_figure` de Acab y
    Jezabel, Elias delante, HUD apagado) y SIETE cajas en orden, todas SIN decision
    (`options[0].label == nullptr` cierra la caja sola): Elias reprende (16:30-33), Acab lo
    desprecia, Jezabel lo amenaza (18:4), Elias sentencia (17:1 tal cual el prologo), Acab
    se burla (los 450 de 18:19), Jezabel promete la caceria (18:10) y cierra «La palabra
    del Señor» (17:2-4, sin retrato). En 17:1 Acab no responde; que aqui responda, y
    Jezabel con el, es decision del usuario (2026-09-09), y lo inventado se apoya en
    16:30-33, 18:4, 18:10, 18:19 y 2 R 1:8. Va ANTES de Querit, en orden, y por eso no
    rompe la regla; la intro del rio (lore.h) pierde sus dos primeras lineas (17:1 y
    17:2-3). Solo en Nueva partida; ESC la salta entera; bajo autoplay se suprime y la
    fuerza `audiencia` (`scripts/audiencia_check.txt`, 17 paginas por pasada). Desde el
    2026-09-10 es la escena 0 del sistema general (ver la seccion siguiente).

### Escenas guionizadas: aperturas de capitulo y dialogos de jefe (2026-09-10)

Mandato del usuario: *"haz lo mismo que hay en el capitulo 1 [...] al comienzo de cada
capitulo y a lo largo de todo el juego"* y *"antes de la pelea con cada jefe debe haber un
dialogo"*. La audiencia se generalizo en **`game/scenes.h`** y ahora hay **20 escenas**:

- **Que es una escena**: un `SceneDef` (escenario + cadena) y sus cajas (`get_scene_line`,
  mismo `EventDef` que el pozo, sin opciones). Escenarios (`StageKind`): `Interior` (silleria
  del bioma, una pantalla, ancla en (W/2, 4)), `Exterior` (sala `Normal` del bioma con un
  claro de 15x8 tiles en el centro para que la decoracion no tape a nadie; el bioma puede ser
  el de OTRO capitulo: Jezreel son los campos del cap. 5, la orilla del Jordan la ribera del
  cap. 9, el desierto de Edom el del cap. 4) y `Live` (la arena tal cual esta, con el jefe
  de pie). Figuras (`FigureKind`): las hojas de Acab y Jezabel, cualquier `EnemyType`
  (soldados, profetas de Baal) y las celdas de `npcs.png` via **`spawn_npc_figure`** (la
  viuda, Guiezi, los hijos de los profetas, el principe azul y plata como Josafat).
- **Aperturas 2-10** (`Scene::opening_of`), antes de la intro cinematica, en el orden del
  libro y sin repetir ni la intro ni el pozo (lista de la manana del 2026-09-10; las de los
  caps. 3, 5, 7 y 8 cambiaron esa misma tarde, ver la seccion anterior): Sarepta 17:8-11 (termina en 17:11: el punado de
  harina es el pozo) · Elias ante Acab 18:17-19, 24 · el mensajero de Jezabel 19:1-3 (la intro
  del cap. 4 pierde el juramento) · camino de Jezreel 19:21 + 21:1-16 contado por Eliseo (la
  unica forma de contar a Nabot con el profeta en escena; termina en 21:16, la palabra es el
  pozo) · la orilla del Jordan 2:15 · los tres reyes en Edom 3:9-15 (termina en el tanedor) ·
  el aposento de Sunem 4:12-16 · la carta de Ben-adad 5:5-8 · el rey avisado 6:9-13.
- **Dialogos de jefe 1-10** (`Scene::boss_of`): al entrar en la arena, con el jefe ya
  spawneado y el mundo congelado; el jefe habla primero, el profeta responde, la ultima caja
  abre la pelea. Las `boss_line` de lore.h viven ahora dentro de estas cadenas. Con la cara
  de cada uno: la Desesperacion habla con el retrato de ELIAS (es su propia voz), la Muerte
  sin retrato, los que son un EnemyType con su celda del Bestiario, Guiezi con la de
  CRIADO_ELISEO. **Una vez por partida y capitulo** (`Engine::boss_talk_seen`): morir y
  reintentar no lo repite; Continuar en otra sesion si lo repite una vez. Se pide en
  `load_room` (`pending_scene`) y arranca en el siguiente paso de Playing, no en mitad de la
  transicion de sala.
- **Reyes sin retrato**: Joram y Josafat llevan `Portrait::NONE` a proposito (la cara de Acab
  para su hijo seria mentir); la hoja de Acab si vale como figura de rey.
- **Ids y Codice**: las cajas NO son `Ev`: ~90 cajas obligatorias reventarian la mascara u64
  de `codex_events`. Viajan en `DialogueState::event_id` desplazadas `Sc::LINE_BASE` (1000)
  y las resuelve `get_dialogue_def`; `open_dialogue` solo marca el Codice para ids `< Ev::COUNT`.
  La audiencia salio de `Ev` con el resto: **`Ev::COUNT` vuelve a 46 de 64**.
- Autoplay: todas suprimidas (`Apertura del capitulo N suprimida (autoplay)`, `Dialogo del
  jefe N suprimido (autoplay)`); `escena <id> [1]` las fuerza. Guion: `scripts/escenas_check.txt`.
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
- **Numeros vigentes**: pozos **3/3/4/4/3/4/5/4/3/6 = 39** relatos; `Ev::COUNT` **46 de 64**
  (las cajas de escena de `scenes.h` van fuera de la mascara y el Codice no las cuenta);
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

## La intro del capitulo 2 dejaba de repetir al dialogo (2026-09-12)

Las dos lineas de prosa de `get_chapter_lore(2)` decian lo que el jugador **acababa de ver en
pantalla**: "La viuda vive junto a la puerta de la ciudad, con un hijo y sin marido que traiga
grano" lo ensena la propia figura de la viuda y lo dice `AP2_VIUDA_AGUA`, y "El sacerdote de la
puerta no te ha quitado el ojo de encima" es literalmente `AP2_SACERDOTE`. Con la apertura
guionizada delante (2026-09-10), la intro se habia quedado narrando el dialogo anterior — el
mismo defecto que ya se corrigio en los capitulos 1, 3, 5, 7 y 8.

Se reescribieron **esas dos y solo esas**, con material que ninguna caja toca: **donde** esta
Sarepta y por que ahi. Sidon es la tierra de Etbaal, padre de Jezabel (1 R 16:31), asi que
Jehova esconde a su profeta en la casa de sus enemigos, y el hambre que anuncio en Samaria ya
ha cruzado la frontera.

Intactos, a proposito: el **verso** (17:14, la promesa de la tinaja), que no lo dice ninguna
caja de la apertura; el **objetivo** ("Vuelve a la casa de la viuda. Que la tinaja no se
acabe."), porque lo pintan el HUD y el mapa; y el **outro** del capitulo. `get_chapter_lore` lo
comparten la cinematica, la carta de texto y la ficha del Codice, asi que las tres dicen lo
nuevo — que es lo correcto.

Guiones: **`scripts/lore_check.txt`**, **`scripts/codice_check.txt`** e
**`scripts/i18n_check.txt`** (cero `TEXT TRUNCATED` en ES y EN: las dos lineas nuevas caben en
una sola linea de la carta).
