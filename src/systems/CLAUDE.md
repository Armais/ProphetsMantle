# `src/systems/` - guardado, logros y meta-progreso

Reglas transversales a todo `src/`: `../CLAUDE.md`.

## Guardado

El **estado de sala** del v9 (flags + celdas alteradas) y como se deriva al restaurar estan
en `src/game/CLAUDE.md`, seccion "Mapas FIJOS".

- **v12 (2026-10-03, El Santuario con rangos y la alforja)**: bloque nuevo AL FINAL (`upgrade_ranks[32]`, `tips_seen`, `belt_ids/belt_charges[4]` + `belt_selected`, `revives_left/rerolls_left/prayers_left` y `reserved_v12[8]` de holgura, como hizo la v7). `SaveDataV11` es la V8 anidada + los dos arrays de 36 (un `static_assert` vigila que el bloque v12 empiece justo donde acababa la v11). **Todas** las ramas de migracion llaman a `derive_v12`: cada bit de `upgrades_bought` entra con **rango 1** (nadie pierde lo que pago), el consumible que se llevaba pasa al **primer hueco de la alforja** con sus cargas, y los dones del intento quedan a `-1` (= "rellenar al Continuar"). `upgrades_bought` se sigue escribiendo (bit = rango > 0) para el SAVEDUMP y los guiones de siempre; `consumable_id/uses` quedan en el fichero solo para poder leer versiones viejas. `delete_save` borra alforja y dones (partida); rangos y avisos son meta. Probado con un guardado v11 escrito por la build anterior (mejoras 0/5/9/12 compradas, Agua x2 en la mano): `Save file v11 migrated to v12`, `rangos=100001000100100000000`, `alforja=9:2/...` y al Continuar la vida sale con el bonus del Santuario.
- **v11 (2026-09-24, mapa grande)**: `SAVE_MAX_ROOMS` 25 -> 36 (la rejilla de 6x5 + el interior, con holgura), asi que el fichero SI cambia de forma. `SaveDataV10` es la `SaveDataV8` anidada + los dos arrays de 25 (la V8 acaba en un u64 alineado: anidarla no mete relleno) y `migrate_v10` lee v9 y v10, conserva meta, equipo y capitulo y descarta el resumen de celdas (describe otra rejilla) con `current_room = -1`. Probado con un guardado v10 escrito por la build anterior: `Save file v10 migrated to v11`, la meta intacta en el SAVEDUMP y Continuar en la celda de inicio del mismo capitulo. Las pistas del jefe NO anaden campos: se derivan del estado de celdas que ya viaja.
- **v10 (2026-09-10, zona continua)**: MISMA forma de fichero que la v9; `room_flags[i]`/`room_open[i]` pasan a ser por CELDA de la zona (i = nodo, bits en local, stride 40) y el lector de v9 los pone a cero y deja `current_room = -1` (el capitulo rebobina a su celda de inicio: la celda del jefe cambio de forma y los secretos se reparten por coordenadas de zona, asi que un resumen v9 abriria tiles que no eran secretos). No hay `SaveDataV9`: los bytes no cambiaron.
- **Working**: save **v9** — chapter seed + room + HP/faith + items + consumable (con sus cargas) + los 3 slots de milagro + `unlocks` (contenido desbloqueado entre partidas) + `codex_events` (relatos vistos) + **el estado de sala** (v9: flags + celdas alteradas, ver la seccion de mapas fijos); auto-saves on every cleared room, and "Continuar" regenerates the same map, re-aplica el estado guardado (lo despejado sigue despejado, el secreto abierto sigue abierto) and drops the player in the saved room. El fichero es un volcado binario de `SaveData`, asi que **anadir un campo rompe el formato**: `SaveSystem::load` lleva lectores de compatibilidad (`SaveDataV3`..`SaveDataV8` + sus `migrate_*`) que suben las partidas viejas sin perderlas y reescriben el fichero en la version actual. Si vuelves a cambiar `SaveData`, sube la version y anade otro lector igual — no se tiran partidas. v1 y v2 si se rechazan.
- **v7 reservo de golpe TODOS los campos que iban a hacer falta**, aunque nacieran a cero: ajustes del jugador (`music_volume`, `sfx_volume`, `aim_assist`, `sensitivity`, `language`, `settings_flags`) y desbloqueos (`items_unlocked`, `skins_unlocked`, `skin_equipped`, `entitlements`). Cinco frentes distintos necesitaban persistir algo; subir version por separado habrian sido cinco migraciones que mantener. **Los ajustes ON/OFF viven en la mascara `settings_flags` (`SETTING_*`) y no en bools sueltos por una razon concreta: anadir un bit NO rompe el formato del fichero.** Hasta v6 no se guardaba **ningun** ajuste — el volumen que ponias volvia a su sitio en el siguiente arranque.
- Comando de autoplay **`savedump`**: vuelca partida + meta + ajustes al log en tres lineas `SAVEDUMP`. Es lo unico que permite comprobar desde un guion que una migracion no ha perdido nada, porque la meta entera no se ve en ninguna pantalla del juego. Guion: `scripts/guardado_check.txt` (migra un fichero viejo real, siembra meta, gana el cap. 10 y comprueba que `delete_save` deja la partida a cero y la meta intacta).
- **Caducidad móvil (GDD 4.4)**: `SaveData::save_timestamp` son **segundos desde la época** (`SDL_GetCurrentTime`); antes eran `SDL_GetTicks`, que arranca a cero en cada sesión y no medía días, y por eso `is_run_expired()` estaba escrita y no la llamaba nadie. A las 48 h la fila "Continuar" del menú pasa a leer **"Partida abandonada +N"**: aceptarla ingresa el 50 % de la tarifa del capítulo (`get_abandoned_offerings`, que ya NO es un porcentaje del banco de ofrendas — eso duplicaba el ahorro del jugador) y **borra** la partida conservando la meta-progresión. Solo se aplica con reglas de móvil (`Engine::mobile_rules`, que `--touch` enciende en PC). Los sellos antiguos (< 1e9) se dan por "sin fecha" y no caducan.
### Logros (`systems/achievements.h`, 2026-08-25)

**Sistema propio, no un envoltorio de Steam.** El GDD pide logros; Steam es una forma de
enseñarlos. Empezar por el SDK ataría el juego a una plataforma, metería una dependencia
externa y exigiría un appid — y MANTLE sale a Escritorio, Web, Android e iOS. Steam queda
en el backlog del usuario como "integración steam"; si entra, entra como adaptador
delgado tras `MANTLE_STEAM` leyendo esta misma máscara.

Forma: `struct` plano + bitmask, igual que `progression.h`. **18 logros** (techo 32, que
es lo que cabe en `SaveData::achievements`). La tabla de definiciones es un `switch`
(`get_achievement_def`), como el resto de tablas del proyecto.

**Regla de admisión: un logro que no se pueda comprobar con un guion de autoplay NO
entra.** `scripts/logros_check.txt` desbloquea **los 18** desde un guardado limpio.

- **`Engine::award(id)` es el ÚNICO camino**: desbloquea, avisa con banner, suena y
  escribe el guardado. `AchievementSystem::unlock` devuelve `true` solo la primera vez, y
  eso es lo que permite sembrar llamadas por todo el Engine sin llevar contabilidad —
  varias se repiten cada frame (`Engine::check_achievements`: llevar tres slots, tener
  ocho objetos, tres discípulos en pie).
- `award` guarda con **`save(false)`**: conseguir un logro no resella la fecha, así que no
  le reinicia a nadie la caducidad de 48 h de la partida.
- Visibilidad: **quinta pestaña del Códice**. Un logro bloqueado **sí enseña su nombre y
  cómo se consigue** — es lo que da algo que perseguir; lo que se oculta con `???` en las
  otras pestañas es contenido, no metas. Los logros **no** cuentan para el contador
  "Descubierto X/Y" (ese mide contenido encontrado): tienen su propio "Logros N/18".

**Guardado v6.** La subida que el plan permitía, con `SaveDataV5` + `migrate_v5` al estilo
de los dos lectores anteriores. Campos nuevos: `achievements`, `miracles_cast` (máscara de
milagros lanzados alguna vez, para "Los Diez Prodigios") y `total_kills`. Quien venga de
v5 empieza a coleccionar desde cero salvo lo que el fichero **ya demuestra**: con el
epílogo desbloqueado se conceden Victoria y Epílogo, que no se inventa nada.

Tres cosas que se arreglaron por el camino y conviene no volver a romper:

- **`SaveSystem::delete_save` reinicia lo de la PARTIDA y deja intacto todo lo demás**, en
  vez de enumerar los campos meta que salvar. La versión anterior copiaba a mano la lista,
  y cada campo nuevo que nadie se acordara de añadir ahí se borraba en silencio al acabar
  una partida: así se perdían los logros y —peor— **`unlocks`**, o sea el epílogo que el
  juego promete desbloqueado "para siempre".
- **`combat_events` se vacía DESPUÉS de leerlo**, no antes de llenarlo. Lo que escribe
  fuera de la ventana de lectura (el `killall` del autoplay, que corre antes en el mismo
  frame) sobrevive hasta que alguien lo lee, y si ese frame se congela por hit-stop —que
  ahora lo pone **cualquier** golpe, no solo los de jefe— espera al siguiente en vez de
  perderse. Con el reinicio delante, `killall` limpiaba la sala y el marcador de bajas no
  se enteraba.
- **`CombatEvents::kill_mask` es una máscara de `EnemyType`, no "el último que cayó".** En
  una barrida mueren varios a la vez: guardando solo el último, matar un león y un soldado
  en el mismo frame borraba el león. La mandan los logros por tipo y el sonido de muerte.

- **Backlog: "integración steam"** (decisión del usuario, 2026-08-25). Los logros ya funcionan
  en las cuatro plataformas sin Steam. Si algún día entra, entra como **adaptador delgado**
  tras `MANTLE_STEAM` que lea `SaveData::achievements` — no al revés. Necesita appid y
  permiso explícito para añadir la dependencia (`CLAUDE.md` la prohíbe sin discusión).
### Meta-progreso que DESBLOQUEA objetos (GDD 3.1 paso 7, 2026-08-25)

El GDD promete "desbloquear items/personajes" y hasta ahora las 8 mejoras de El Santuario
eran solo cifras: el unico desbloqueo entre partidas era el epilogo. Ahora son **12**, y
cuatro de ellas meten un objeto en el sorteo de cartas.

- **`MetaUpgrade::unlocks_item`** (ultimo campo, con valor por defecto `NO_ITEM`, para que
  las mejoras que no desbloquean nada compilen sin nombrarlo) e **`ItemDef::starts_locked`**.
  `ItemRegistry::roll_choices` recibe ahora la **mascara** `SaveData::items_unlocked` por
  parametro: `items.h` no conoce ni debe conocer el fichero de guardado.
- **Cuatro de los seis objetos nuevos nacen bloqueados** (Arco de Joas, Higos, Panes de
  Cebada y el Reloj de Acaz). Los otros dos entran desde el primer dia, para que el
  catalogo ampliado se note ya sin pagar nada.
- **`Engine::apply_meta_unlocks`** es idempotente y **solo pone bits, nunca los quita**: un
  objeto desbloqueado no se vuelve a bloquear solo. Se llama al comprar y al arrancar, y
  como `items_unlocked` es meta, **sobrevive a `delete_save`** — que es lo que hace que
  ahorrar ofrendas signifique algo entre partidas.
- **El panel de El Santuario SCROLLEA** (`SANCTUARY_ROWS = 7`, misma idea que la lista del
  Codice). Con 12 mejoras a 14,5 de paso ya no caben en 180 unidades de alto. **Dos
  columnas se descartaron**: las descripciones son frases enteras y a 150 unidades de ancho
  `wrap_text` las truncaria en silencio. Las flechas de scroll van en el **margen
  izquierdo** — centradas, la de arriba caia justo sobre la linea de "N ofrendas".
- **"Personajes" del GDD = cosmeticos** (decision del usuario, 2026-08-25): no hay un
  tercer profeta jugable ni "empezar como Eliseo"; las variantes visuales se compran aqui
  mismo y se documentan en la seccion de cosmeticos.

**`open_item_choice` loguea LO QUE OFRECE** (`Item choice: A / B / C`), no solo lo que se
toma: es lo unico que permite comprobar desde un guion que un objeto bloqueado no ha
entrado en el sorteo. Guion: **`scripts/meta_check.txt`**. Su gate NO es "no se ofrece
nada" — las dos reliquias no pueden estar las dos en la mano (un solo slot), asi que la que
quedo fuera sigue siendo candidata — sino **"en lo ofrecido no hay ninguno de los cuatro
bloqueados"**. Medido: bloqueados, tres tiradas solo dan reliquias; comprados, los cuatro
aparecen.

**Comando `metareset`**: vuelve a bloquear mejoras, desbloqueos de objeto **y logros**. Sin
el, `meta_check` y `logros_check` solo servian la PRIMERA vez — las compras y los logros
persisten entre partidas (esa es su gracia) y `delete_save` no los borra por diseno, asi
que `logros_check` exigia borrar el fichero de guardado a mano. Misma razon que `defaults`
para los ajustes: **un guion de regresion que depende de como quedo el anterior no es una
regresion.**

### Cosmeticos y monetizacion (GDD 4.x, 2026-08-25)

El GDD mete **tres cosas distintas** en la misma tabla y hay que separarlas:

- **Premium 4,99 (el modelo recomendado): NO necesita una linea de codigo.** Es un precio
  en la ficha de la tienda. Documentado y cerrado.
- **Cosmeticos: hechos.** Sin ninguna dependencia externa y comprados con **ofrendas** —la
  moneda que ya existe— y no con dinero.
- **Free-to-Start (Acto I gratis / Acto II de pago): solo queda la PUERTA.**
  `SaveData::entitlements` **hoy se concede sola** (`0xFFFFFFFF` en `apply_meta_unlocks`).
  Es una puerta, no una caja: el dia que haya tienda, quien la rellene sera ella. **Integrar
  Google Play Billing, StoreKit o Steamworks NO se hace sin orden explicita del usuario** —
  son dependencias externas y `CLAUDE.md` las prohibe sin discusion previa.

**Una skin es un TINTE, no una hoja de sprites nueva** (`get_staff_skin` en `game/items.h`,
4 variantes). Es una decision de coste: una variante dibujada serian 8 PNG de 896x1920 mas
por skin, y una regla del proyecto es que `assets/` no crece si se puede evitar — el mismo
argumento que la paleta por capitulo de `get_enemy_chapter_tint`. Se tine lo que el jugador
ve **salir del cayado**: el proyectil y el fogonazo de la boca. **No dan ventaja**: el GDD
dice "NO pay-to-win" y aqui no cambia ni una cifra.

- Se compran en El Santuario (mejoras 12-14, 80 ofrendas cada una) via
  `MetaUpgrade::unlocks_skin` -> `SaveData::skins_unlocked`, y se equipan en la fila
  **"Cayado"** de Opciones, que **solo aparece cuando hay mas de una desbloqueada**: una
  fila que solo ofrece lo que ya llevas puesto no es una opcion, es ruido. El ciclo salta
  las bloqueadas.
- **Trampa que costo una pasada**: el `Player` se **rehace** al empezar partida
  (`player = Player{}`), asi que `player.staff_skin` hay que volver a ponerlo en
  `apply_meta_upgrades()` **y** en `finish_ascension` (la Ascension no pasa por ahi).
- Esto es tambien lo que el usuario decidio que significaran los **"personajes"** del GDD
  (2026-08-25): variantes cosmeticas, no un tercer profeta jugable.

Guion: `scripts/cosmetico_check.txt`. Ojo con la captura del disparo: a 280 unidades/s y
120 de alcance el proyectil dura menos de medio segundo, asi que `press attack` + `wait
0.08` + captura; con `hold attack 0.5` la foto llega tarde y sale la sala vacia.

### El Santuario REDISENADO: rangos, dones y la pantalla de fin (2026-10-03)

Queja del usuario, literal: *"el tema de las ofrendas no se entiende y lo que compras ahi no
afecta lo suficiente al gameplay. Lo suyo es que lo que comprases tuviese un efecto."* Eran
dos problemas, y los dos eran ciertos:

1. **No se entendia**: la ofrenda del suelo daba un "+3 ofrendas" que tapaba el centro de la
   pantalla y no decia para que servia; la derrota no mencionaba las ofrendas; El Santuario
   era una lista de 15 filas con una frase cada una.
2. **No afectaba**: casi todo era un +10 de algo, y ADEMAS **dos fugas**: `apply_meta_upgrades`
   solo corria al empezar partida y al reintentar, asi que al **Continuar** y al pasar a
   **Eliseo** se perdian la vida, el dano y la velocidad comprados. Y la paga de la derrota
   usaba `run_kills`, que no se reiniciaba al reintentar: cada derrota volvia a cobrar las
   bajas de todas las anteriores.

**Catalogo nuevo** (`progression.h/.cpp`, ids ESTABLES en `namespace Meta`: 0-14 los de
siempre, 15-20 nuevos). Cinco pestanas (`MetaCategory`): El Profeta, Dones, Eliseo, Tesoro y
Aspecto. Cada mejora tiene `max_rank` y un coste POR RANGO (`costs[]`), su PROPIO icono
(`Icon::SANCT_*`, celdas 32..50 de `icons.png`; ver abajo) y una cita. **Lo que da cada rango vive en UNA tabla**
(`meta_hp`, `meta_damage`, `meta_speed_mult`, `meta_start_faith`, `meta_inherit_ratio`,
`meta_revives`, `meta_rerolls`, `meta_offering_mult`, `meta_belt_slots`, `meta_jar_water`) que
lee el Engine al aplicarlo Y `meta_rank_text` al pintarlo: la cifra de la pantalla y la del
juego no pueden divergir.

**Iconos propios (2026-10-03).** Al principio El Santuario tomaba prestadas celdas de
objetos y milagros para no hacer crecer `assets/`, y el usuario lo cazo a la primera: la
antorcha de un objeto como "Fuego Interior", un cuenco de ceniza como "Fe Renovada", unas
tortas sueltas como "La Mesa de Eliseo" y la escalinata del Reloj como "La Sombra que
Vuelve". Aqui la regla de `assets/` cede: ahora hay 19 celdas propias (`icons.py`,
`SANCTUARY`), y el criterio es **el icono dice el EFECTO con un simbolo de juego y el nombre
lo viste**: corazon (vida, con el cuervo encima), hoguera (dano), sandalia con estelas
(velocidad), halo de luz (fe), corazon con el soplo (revivir), gavilla (primicias), dados
(otra tirada), alforja, tinaja rebosando, bolsa de monedas (diezmo), altar de cuernos con
sus varas, el manto como capa con luz (herencia), lampara de TRES llamas (la de Doble
Porcion es de dos), rollo con dos cayados (escuela), mesa de madera con panes y cantaro,
cuadrante solar con su sombra y la flecha de "atras", y el cayado con la llama de su color
para cada skin (el tinte de `draw_upgrade_icon` se quito: ya viene pintado). **Nunca la
celda de un objeto o milagro que el jugador conoce por otra cosa**; la excepcion son las
que desbloquean un objeto (Arco, Higos), que llevan el icono de ese objeto porque es justo
lo que vas a ver salir.

| Pestana | Mejora | Rangos | Efecto por rango |
|---|---|---|---|
| Profeta | Provision del Cuervo | 5 (40..300) | +10 de vida maxima |
| Profeta | Fuego Interior | 3 (75..260) | +2 de dano por disparo |
| Profeta | Sandalias Rapidas | 3 (60..200) | +7% de velocidad |
| Profeta | Fe Renovada | 2 (100, 220) | fe inicial 60 / 70 |
| Dones | **Aliento de Vida** | 2 (180, 420) | al caer te levantas con media vida, 1 / 2 veces por INTENTO |
| Dones | **Primicias** | 2 (160, 320) | carta de objeto al empezar; rango 2, otra al empezar el Acto II |
| Dones | **Echar Suertes** | 3 (70..260) | 1/2/3 re-sorteos de cartas por CAPITULO ([R] / X / placa) |
| Dones | **Alforja Ancha** | 2 (90, 200) | 3 / 4 huecos de alforja (base 2) |
| Dones | **La Tinaja de Sarepta** | 2 (100, 210) | 1 / 2 Aguas del Jordan al empezar cada capitulo |
| Dones | **Diezmo** | 3 (80..280) | +15% de ofrendas por intento |
| Dones | Altar Portatil | 2 (120, 240) | 1 / 2 oraciones sin altar por capitulo |
| Eliseo | Herencia Ampliada | 2 (150, 280) | Eliseo hereda 60% / 70% de la fe |
| Eliseo | Doble Porcion Mejorada | 1 | 3 huecos de milagro |
| Eliseo | La Escuela de Profetas | 1 | un discipulo con Eliseo |
| Tesoro | Arco / Higos / Panes / Reloj | 1 | el objeto entra en juego (los Higos, en el MUNDO) |
| Aspecto | tres skins | 1 | solo aspecto |

**Donde se aplica cada cosa** (y es la parte que no hay que romper):

- `Engine::apply_meta_upgrades`: lo que va EN EL CUERPO (vida, dano, velocidad, huecos de la
  alforja, tercer slot de Eliseo, skin). Se llama al **CREAR** al profeta: partida nueva,
  reintento, **Continuar** y **Ascension**. La fe inicial NO va ahi (Continuar trae la suya y
  el Ascenso la hereda): la ponen `start_new_run` y `retry_chapter`, y `save_after_defeat`
  guarda `meta_start_faith` como fe de arranque.
- `refill_attempt_gifts` (por INTENTO: Aliento de Vida y el recuento de ofrendas) y
  `refill_chapter_gifts` (por CAPITULO: re-sorteos, oraciones, el agua de la tinaja, desde
  `start_chapter`). Lo gastado viaja en el guardado: salir al menu y Continuar no es un
  re-sorteo gratis. El comando `meta` del autoplay los rellena al momento si se compra en
  plena partida.
- El Aliento de Vida salta el **ultimo** en la muerte (despues del perdon sunamita y del Reloj
  de Acaz), con fogonazo, banner `ALIENTO DE VIDA` y `Revive used: Aliento de Vida (...)`.
- Las Primicias abren su carta en el primer paso de `Playing` (`primicias_pending`), detras de
  la audiencia y la intro; nunca en el tutorial ni al Continuar.

**Las ofrendas, legibles** (`Engine::award_offerings`, `bank_offerings`, `render_run_summary`):

- La paga es por INTENTO (`Engine::RunTally`: bajas, grupos vencidos, jefes y hallazgos desde
  el ultimo cobro): capitulo x12 + bajas + grupos x4 + jefes x25, x2 en la victoria, + Diezmo.
  Los hallazgos del suelo ya entraron al recogerse y se listan aparte, sin sumarse dos veces.
- La pantalla de **derrota** (y la de victoria) pinta el **desglose linea a linea** con su tic,
  el total contando, el banco ("no se pierden al morir") y **que se puede comprar ya** ("Ya
  puedes ofrecer: X" / "Te faltan N para: X", `cheapest_affordable` / `cheapest_pending`). Y
  tiene un boton **El Santuario** en medio (`sanctuary_return`): se gasta lo ganado y se vuelve
  a la derrota a reintentar, ya con la mejora puesta. Es el bucle de Hades y Dead Cells, y es
  lo que no se entendia.
- En partida, **contador de ofrendas** bajo la fe que salta y dice "+N" al ingresar (ya no
  hay mensaje en el centro); la primera ofrenda de la vida del guardado explica que es
  (`TIP_OFFERINGS`, `SaveData::tips_seen`, una sola vez).
- En el titulo, una **moneda que late** junto a "El Santuario" (y "algo te espera en El
  Santuario" bajo las ofrendas) cuando hay algo comprable (`MenuSystem::sanctuary_ready`).

**La pantalla** (`Engine::render_sanctuary`): pestanas arriba con un punto dorado si hay algo
comprable dentro, lista con icono / nombre / pepitas de rango / coste a la izquierda, y a la
derecha el DETALLE: icono grande, que hace, **"Ahora: ..." y "Siguiente: ..."** con la cifra,
el objeto que desbloquea (nombre en su color de rareza + su efecto), la cita apagada y el
boton **OFRECER N** (late si llega; "Te faltan N" en rojo si no; COMPLETO en verde). Comprar
hace destello en la fila, aro en el icono, "-N" bajo el banco y el aviso dentro del panel.
Teclado: A/D (o Q/E) pestana, W/S fila, ENTER ofrecer, ESC volver. Tactil: tocar pestana,
tocar fila, tocar OFRECER, "< Volver" abajo. Los rects los apunta el dibujo y los lee
`update_sanctuary` (la regla de siempre). Logs: `Santuario: <mejora> rango R/M (-coste,
quedan N) -> <efecto>` y `Santuario tap: N`.

Guion: **`scripts/santuario_check.txt`** (gates en su cabecera).

### Ajustes de los controles tactiles en la holgura de la v12 (2026-10-03)

`SaveData::touch_size_pct` y `touch_opacity_pct` ocupan los DOS PRIMEROS huecos de lo que era
`reserved_v12[8]` (ahora `reserved_v12[6]`): el fichero mide lo mismo, el `static_assert` de la
v12 no se mueve y **no hay migracion**. **Cero = valor de fabrica (100 %)**, que es justo lo que
traen los guardados v12 escritos antes. Zurdo y ahorro de bateria son bits de
`settings_flags` (`SETTING_TOUCH_LEFTY` = bit 3, `SETTING_BATTERY_SAVE` = bit 4). `defaults`
los devuelve a fabrica; `SAVEDUMP tactil tamano=.. opacidad=.. zurdo=.. ahorro=..` los vuelca.
