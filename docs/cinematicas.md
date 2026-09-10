# Cinematicas: prologo, intros de capitulo y el Ascenso

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/ui/`** (`cinematic.h/cpp`). El arte lo
genera `tools/blender/mantle_sprites/cine.py` (ver `tools/blender/CLAUDE.md`).

### Prólogo cinemático "El Cielo Cerrado" (`ui/cinematic.h`)

Lo que se ve **al arrancar**, en lugar del menú: ocho estampas de ~10 s (reino partido →
Acab → Jezabel y Baal → la persecución → Dios habla a Elías en el monte → «ni rocío ni
lluvia» ante el rey → los cuervos de Querit → título). `main.cpp` llama a
`Engine::start_intro()` **solo si no hay `--script`**: el guion de autoplay se carga
*después* de `init()`, así que dentro de `init` el flag todavía es `false`, y los seis
guiones de `scripts/` dan por hecho que arranca el menú. También es la fila "Prólogo" del
menú principal (`option_count = 7`).

Cada estampa son **3 PNG de 1600x720** en `assets/sprites/cine/NN_nombre_L0..L2.png`
(= 400x180 unidades a `ASSET_SCALE 4`, o sea 80 unidades de holgura para el paneo), con
alfa, que el motor mueve a distinta velocidad: `L0` lejos ~0.30, `L1` medio 1.0, `L2`
cerca ~1.50. **El cielo NO viene de Blender**: se pinta como degradado de 90 bandas
(a 320x180 no se ve el bandeado) y así se puede teñir por escena. Efectos en engine
(`CineFx`): haz de luz —dibujado **entre** L0 y L1, encima lavaba al profeta—, fogonazo,
temblor local (no `shake_camera`: movería también cielo y texto y asomarían los bordes),
pavesas y título. Avanza con ENTER/ataque, ESC lo salta entero.

No es una extensión de `StorySystem`: aquella es una pila de páginas de texto que avanza
con ENTER, sin reloj ni imagen.

El arte lo genera `tools/blender/mantle_sprites/cine.py` (ver abajo). Música:
`MusicID::Prologue` → `prologue.wav`, 88 s, generada por `music_prologue()` en
`tools/audio/gen_audio.py`.

### Intros cinemáticas de capítulo (2026-08-29)

Cada capítulo abre con una animación que cuenta su historia — el mismo sistema del
prólogo (`CinematicSystem`), con **una estampa por capítulo y DOS `CineScene`**: el
plano A lleva el rótulo "CAPÍTULO N" (dorado en el Acto I, azul en el II) + versículo +
dos líneas, y el B deshace el paneo con la línea final + el objetivo. Por defecto los dos
planos van sobre el mismo `art` (las texturas se cachean por path, así que el segundo
sale gratis), pero **el plano B puede llevar estampa propia** (`ChapterCine::art_b`,
2026-09-09): la lleva el cap. 1 (`31_cap01b_escondite` — Elías de pie en el fondo de la
garganta bajo los cuervos, dos soldados de Acab pasando de largo por la cresta), porque
su plano A repite la estampa 7 del prólogo y el jugador veía a Elías arrodillado en el
río tres veces seguidas. En `cine.py`, `PANELS[31..40]` = plano B del capítulo `idx-30`.
El texto sale ÍNTEGRO de `get_chapter_lore` — no se escribió una palabra nueva — y
`Scripture::ref` sigue sin pintarse.

- **`cinematic_chapter(c, capitulo)` + tabla `CHAPTER_CINE[10]`** (cinematic.cpp), al
  lado de `cinematic_prologue`. `art = nullptr` en la tabla = "estampa no entregada":
  `cinematic_chapter_ready()` devuelve false y el Engine cae solo a la carta de texto.
- **`ChapterAnnounce { None, Card, Cinematic }`** sustituye al bool de `start_chapter`
  (un enum class no convierte desde bool: los 6 llamantes se revisaron al compilar).
  `Cinematic` = solo la PRIMERA entrada natural: `advance_chapter`, `finish_ascension`
  y la nueva partida (que ya NO lee el prólogo de TEXTO: duplicaba al cinemático recién
  visto). El reintento y el Continuar usan `Card` — repetir la intro en cada muerte
  cansa. Sin bit en el guardado: el criterio es el sitio de llamada.
  **Nueva partida desde el 2026-09-09**: `start_new_run` → `start_scene(Scene::AUDIENCIA)`
  (la audiencia con Acab, 1 R 17:1-4, guionizada sobre el palacio con `GameScene::Dialogue`)
  → `finish_scene` → intro cinemática del cap. 1 (o la carta si falta la estampa). **Desde el
  2026-09-10 todos los capítulos abren así**: `start_chapter(n, Cinematic)` lanza primero la
  APERTURA guionizada (`Scene::opening_of(n)`, `game/scenes.h`) y es `finish_scene` quien
  lanza la intro. Bajo autoplay las escenas se suprimen; `escena <id> [1]` las fuerza y las
  deja paradas, ≈ `chintro` (confirm pasa de página, cancel la salta entera), o avanzando
  solas con el `1`. Ver `docs/encuentros-y-lore.md`.
- **`CineThen { MainMenu, Play }`** (`Engine::cine_then`): a dónde va `GameScene::Intro`
  al terminar. El case de `update_music` bifurca por él: prólogo → `Prologue`; intro de
  capítulo → el ambiente del acto (la decisión de no cortar la música entre capítulos).
- **Bajo autoplay NINGUNA entrada natural dispara la intro** (se loguea `Intro de
  capitulo N suprimida (autoplay): carta de texto`): los 32 guiones cronometrarían mal
  con 20 s de cine en medio (precedente: el Ascenso jugable vs guardado_check). El
  único camino con `--script` es el comando **`chintro <n>`**, que la deja PARADA como
  `cine` (plano B con `press confirm`, salir con `press cancel`). **Jugando sin script
  la sustitución sí ocurre** — si en una prueba con guion "se siguen viendo las
  cartas", es esto, no un fallo.
- **Fix del bug `art_loaded`**: `begin()` no lo reseteaba y `start_intro` solo cargaba
  arte `if (!art_loaded)` — la SEGUNDA cinemática de una sesión (repetir "Prólogo"
  desde el menú) salía sin capas. Ahora `begin()` lo resetea y `load_art` se llama
  siempre (la caché por path del Renderer lo hace gratis).
- **VRAM**: `Renderer::unload_texture(path)` + `CinematicSystem::unload_art()` al
  terminar una intro de capítulo — sin esto una partida completa acumulaba ~140 MB de
  capas vistas una vez (el juego sale a móvil). El id destruido queda como hueco
  estable (`textures[id].texture = nullptr`, los draws ya lo saltan). El prólogo NO se
  descarga: el menú lo reusa.
- Las 10 estampas: `PANELS[11..20]` en cine.py (`NN_capNN_lugar_L{0,1,2}.png`), +16 MB
  en assets (40,6 MB total; la cuantización PIL queda como palanca opcional). Lecciones
  de autoría que costaron pasadas y NO conviene repetir: una **ridge lejana pequeña
  FLOTA como un zepelín** (la esfera entera queda sobre la línea de vista del borde del
  suelo — se entierra con `z` negativa, o se tapa con una fila de dunas delante); un
  **vano oscuro no se hace con color** (Workbench aclara cualquier plano frontal — se
  hace PEQUEÑO y enmarcado en dos pasos, como la puerta de Jezabel; el `round` alto en
  el box del hueco es lo que le mete el degradado de "pizarra"); las **paredes de cañón
  con `cliff_face` no funcionan de lejos** (en FLAT son tapia lisa, en MID bloque
  flotante — laderas de silueta como scene_querit + agujas con cavity); una **yunta de
  espaldas enseña las ancas** (de perfil, con el buey lejano adelantado media cabeza); y
  el humo/las tiendas lejanas se colocan CON la panza hundida o levitan.

Guion: **`scripts/intros_check.txt`**. Gates: `Cinematica de capitulo N` x2 para
N=1..10 (pasadas ES y EN), cero `Cinematic: falta la capa`, cero `TEXT TRUNCATED`,
`suprimida` >= 1 (el fallback existe) y `Unloaded texture` tras cada intro.

### La cinematica del Ascenso (2026-08-30)

El carro de fuego (2 Reyes 2:8-14) se ve como CINEMATICA: **seis escenas sobre cuatro
estampas nuevas** (`PANELS[21..24]` en cine.py: el Jordan abierto A/B, el carro de fuego,
el torbellino y el manto caido A/B; `cinematic_ascension` en ui/cinematic.cpp). Corre
**entre la cuarta parada del Ascenso y `finish_ascension`** — la unica cinematica sin
mundo cargado debajo (las paradas van sobre negro) — con `CineThen::FinishAscension`:
saltarla con ESC **tambien** reparte la herencia (es presentacion, nunca la puerta del
Manto). Mantiene `MusicID::Ascension` sonando y descarga sus capas al salir.

- **El texto es EXACTAMENTE el de `get_ascension_lore`** (las tres paginas que leia
  StorySystem) mas la linea de la herencia como cierre: ni una palabra nueva. Cuando la
  cinematica se ve, las paginas de texto **no se leen despues** (precedente del prologo:
  no repetir lo recien visto); bajo autoplay o sin estampas, el fallback ES el texto de
  siempre (`Cinematica del Ascenso suprimida (...)` en el log) y los guiones no ven cambio.
- Comando de autoplay **`asccine [escena]`**: fuerza la cinematica PARADA (como `chintro`)
  y **solo mira** — `cine_then = Play`, salir de ella no reparte nada; el camino real
  sigue siendo `ascend` + las cuatro paradas.
- Lecciones de autoria que costaron pasadas: los **muros de agua** del Jordan partido son
  el RIO entero represado — olas de esfera semienterradas en cascada cruzando el cauce —
  y no tapias paralelas al agua (sofas), ni lajas verticales (silos), ni llevan casquete
  de espuma (tinaja con boca); el **caballo de fuego** lleva el cuello de UN tubo arqueado
  (cajas giradas dejan la cabeza suelta) y el carro separa rueda de ORO y caja roja (del
  mismo color eran un borron); el **torbellino** es una columna ESBELTA y lejana (r 1-2,4 m
  — con radios de 2-5 m era una maceta que se tragaba a Elias, que va DELANTE del tubo,
  no dentro); la **estela de humo se descarto dos veces** (un tubo de `smoke()` a esa
  distancia no baja de un brazo de grosor: las pavesas del motor hacen ese trabajo); el
  **manto caido** son esferas aplastadas solapadas (las lajas planas con punta alzada
  salian un trineo con aleron); y en POSE_CRY el brazo del cayado se queda BAJO (-25):
  a -70 el baculo se volcaba en horizontal y le cruzaba la cara a Eliseo.

Guion: **`scripts/asccine_check.txt`**. Gates: `Cinematica del Ascenso (asccine, escena 0)`
x2 (ES y EN), cero `falta la capa`, cero `TEXT TRUNCATED`, `Unloaded texture` tras salir,
`suprimida (autoplay)` en el flujo real y `le seguiste 3 de 3 veces, doble porcion=si`
con el LOADOUT de siempre (la herencia no se ha movido).
