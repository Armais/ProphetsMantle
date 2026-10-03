# `src/game/` - salas, mundo, objetos y enemigos

Reglas transversales a todo `src/`: `../CLAUDE.md`.

> **Antes de tocar `Room::generate`, `decorate` o `render_ground`, lee
> `docs/mundo-exterior.md`**: la referencia del sistema de salas (rejilla, transiciones,
> tipos de sala, tipos de tile), las salas de 2x2 pantallas, la camara, los bordes por
> bioma y las cinco deudas visuales del exterior.
> **Antes de tocar secretos, tinajas, grietas, huecos o interiores, lee
> `docs/secretos-e-interiores.md`.**
> **Antes de tocar `events.h`, `lore.h` o `scenes.h` (las escenas guionizadas: aperturas de
> capitulo y dialogos de jefe), lee `docs/encuentros-y-lore.md`.**
> Los efectos (`spawn_fx` / `spawn_vfx`) y el tinte por capitulo: `docs/efectos-y-paleta.md`.

### Mapas FIJOS: mundo constante, reparto por configuracion y guardado v9 (2026-08-27)

Mandato del usuario, literal: *"los mapas, la disposicion de los enemigos, son fijas. No
cada vez que juegues cambie. Las salas no se pueden generar aleatoriamente en runtime."* Y
al preguntarle por los encuentros fue MAS lejos que la recomendacion: *"TODOS los relatos
apareceran en TODAS las partidas. Ya no son semillas constantes, es configuracion del
juego."* El terreno y la colocacion de enemigos son siempre los mismos — entre partidas,
al Continuar y entre lanzamientos — y la historia no se sortea. Registrado tambien en
`GDD2.0.md` §3.4.

**Las tres piezas, en orden de dependencia:**

1. **Generacion PURA por (capitulo, sala).** El fallo de fondo era que `Room::generate`
   tiraba del `rand()` global que el gameplay drena entre medias (fase de la sacudida en
   renderer.cpp, drops en combat.cpp, burbujas en player.cpp): la misma sala de la misma
   semilla salia distinta segun cuando entraras y que hicieras antes. Ahora
   **`Engine::generate_pristine_room` es LA definicion de como se genera una sala**:
   re-siembra `srand(chapter_seed ^ ((sala+1) * 2654435761u))` justo antes de generar. La
   usan `load_room` (primera visita) y `capture_room_states` (la copia pristina del diff
   del guardado); si divergieran, el Continuar restauraria salas que no se jugaron.
   Verificado: 5 salas identicas tile a tile Y enemigo a enemigo en dos ordenes de visita
   distintos con combate entre medias.
2. **Semilla CONSTANTE por capitulo** (`Config::CHAPTER_SEEDS[10]`) y **reparto de salas
   por CONFIGURACION** (`CHAPTER_LAYOUT` en rooms.cpp: una cadena por capitulo, un caracter
   por celda — S inicio, B jefe, N normal, T tesoro, A altar, L elite, 1..4 la sala Event
   con el relato k-esimo del pozo). `FloorMap::generate` ya no consume NADA de `rand()`.
   Con el pozo completo colocado, las rejillas 4x3 pierden un Tesoro y un Altar (no caben
   12 tipos en 12 celdas; las 4x4 conservan 2+2). Cambiar una semilla o una letra de la
   tabla es una decision de DISENO: redibuja ese capitulo para todo el mundo y pudre las
   coordenadas pinneadas en `scripts/`.
3. **Guardado v9: el estado de sala persiste al Continuar** (decision del usuario). Un byte
   de flags por sala (visited/cleared/altar/cofre/encuentro/arco de salida) + **UN BIT por
   celda alterada** desde la generacion (`SaveData::room_open`, 110 bytes por sala). El
   diff se calcula al guardar REGENERANDO la sala en limpio y comparando — sin
   contabilidad aparte, caza cualquier mecanica que toque tiles. Al restaurar, el tile
   final se DERIVA del recien generado: todo va a `Floor` salvo cofre/encuentro/altar de
   Baal, que van a `Path`, y el arco del jefe lo replica `place_exit()`. **Si una mecanica
   nueva transforma tiles a OTRA cosa, hay que anadirla en `apply_saved_room_state` o el
   Continuar la perdera.** La derrota sigue rebobinando el capitulo entero (save_after_defeat
   borra el resumen). Migracion `SaveDataV8` + `migrate_v8` segun la convencion; verificada
   con el guardado real del usuario (25.972 ofrendas y toda la meta intactas).

**Zona continua (2026-09-10):** la unidad PURA sigue siendo la celda
(`Engine::generate_pristine_cell`, el `generate_pristine_room` de siempre), pero lo que se
juega es la ZONA: `Engine::build_pristine_zone` cose las celdas en un `Room` de tipo
`RoomType::Zone` (hasta 160x88) y `Room::open_seams` abre las costuras. El estado va por
celda (`Room::cells[i]`, `CellState`), los enemigos nacen y se reponen por cercania
(`Engine::update_cells`, `EnemyAIComponent::home_cell`) y la arena del jefe es un recinto
dentro de la celda B (`stamp_arena`, `Engine::enter_arena`). Referencia completa: la
seccion "Zona continua" de `docs/mundo-exterior.md`. El guardado sube a v10 (misma forma
que v9, resumen de salas descartado al migrar).

**Mapa grande, jefe en otro borde y pistas (2026-09-24):** rejillas 6x4 (6x5 en 5 y 10),
`CHAPTER_LAYOUT` rehecho con el jefe en un BORDE distinto por capitulo (N, O, E o S) y la
arena pegada a ese lado (`ArenaSide`); la celda B se entera por sus `door_*` de que lado
toca. El jefe ya no sale en el mapa: se aprende con pistas (`get_boss_intel`). Letra nueva
`.` = `RoomType::Rest` (paso tranquilo, sin enemigos). Referencia completa: la seccion
"Mapa grande" de `docs/mundo-exterior.md`. Guardado v11.

**Flecos que importan:**

- `pending_seed` (Continuar, guarda la semilla de su partida) y `forced_seed` (comando
  `seed` del autoplay, PEGAJOSO para toda la sesion) siguen mandando sobre las constantes,
  en ese orden. **`seed` es ya solo compatibilidad de guiones**: un guion que quiera el
  mapa REAL del juego simplemente no lo usa (asi van `mira_check`, `secretos_check` y
  `feel_baseline` desde hoy; los demas conservan su semilla y con ella su mundo propio,
  determinista igual).
- `restore_room_state` -> `apply_saved_rooms` es el par de flags que hace que el resumen
  v9 SOLO se re-aplique en el arranque que es un Continuar: un capitulo nuevo, un
  reintento o la Ascension arrancan con las salas frescas.
- De regalo cayo un bug latente: `Room::generate` no reseteaba `event_used` al reutilizar
  `current_room_data`, asi que una sala Event generada justo despues de resolver otro
  encuentro nacia ya "resuelta" y su relato no se abria nunca.
- El comando `savedump` imprime una cuarta linea `SAVEDUMP salas i:flags ...
  celdas_abiertas=N`: es el gate de guardado_check para el estado de sala.
- La bateria post-cambio: 32/32 en verde por sus gates declarados. Ademas de los guiones
  re-pinneados, cayeron dos podredumbres nuevas: `guardado_check` media el "tras borrar"
  ANTES de confirmar en la pantalla de victoria (el epilogo recorrible de ~21 s se comia
  sus confirms — `delete_save` salta al CONFIRMAR en Victory, no al llegar), y
  `logros_check` necesitaba para "Bajo la Retama" una sala cuya fe de bajas quede en <= 9:
  con su semilla es la sala 1 (4 soldados + 1 arquero + 1 profeta = 8); la 8 trae 4
  profetas (fe 18) y ahi va el bloque del Leon, al que la composicion le da igual.

## Estado actual

- **Working**: movement (feet-point tile collision with axis sliding; `Room::is_walkable_world` is the single world→tile walkability test used by movement, `combat.cpp push()` body separation and spawns; `system_movement` has an `unstick()` safety net that snaps a body whose feet are inside a solid tile to the nearest walkable tile and logs it), shooting toward the last movement direction (hold K), projectiles with tileset sprites, enemy AI (melee packs, archers/casters/generals shoot with line-of-sight, lions leap), contact damage with player i-frames (1 s), knockback, hit flash, particles, camera shake, drops (hearts / faith orbs), faith system.
- **Conductas de muerte del Acto II** (`game/enemies.h`): `on_enemy_death_behavior` se llama desde `on_enemy_killed`, **antes** de `Engine::update_room_state`, para que lo que nazca al morir cuente ya como vivo (si no, la sala se declara limpia con enemigos dentro). El **Idólatra de Rimón** suelta al caer su brasero de brasas, que prende tras una mecha (`EnemyAIComponent::death_fuse`, 0.35 s de telegrafiado sobre el cadáver); `update_death_fuses` — que el Engine llama una vez por frame justo después de `system_collision` — la apura y detona: daño en radio 28 a jugador **y** enemigos. La cadena NO recursa: son dos pasadas (apurar mechas, luego detonar), así que una explosión que mata a otro Idólatra le enciende una mecha que se consume el frame siguiente. El **Agua Corrupta** se parte en 2 hijos al 55% de vida y escala (`split_generation`, máximo 2 generaciones, y no se divide con ≥18 enemigos vivos); los hijos dan fe reducida y ningún drop. El **General Sirio** arenga cada 4 s: +30% daño y +15% velocidad en radio 70 durante 5 s. Los valores base se guardan **una vez** en el aliado (`base_damage`/`base_speed`), así que refrescar el aura no acumula; el aliado late en rojo en `ECSWorld::collect_draws`. Sin nadie a quien arengar no gasta la arenga.
- **Working**: rooms — cliff borders with 6/5-tile openings on connected sides, trails carved from a jittered hub to every opening (guaranteed reachable; river → bridge, obstacles → path), rivers with fords, biome look per chapter (`get_biome_style`: floor family sand/meadow/paving via `floor_base`, 3 ground decals + `detail_pct` scattered by `tile_hash` on plain Floor tiles in `render_ground`, plus tints/props/buildings), estado por celda en `Room::cells` (zona continua, 2026-09-10; `FloorMap::room_cache` son dos ranuras: zona e interior), la arena del jefe sellada mientras vive (`Room::arena_sealed`, ya no se sellan salas), Start cell Baal altar (shoot it: +8 Fe), Treasure chest, Altar prayer, Elite reward.
- **Working**: progression — room clear detection, boss per chapter (`game/bosses.cpp`: una estrategia propia por capitulo con telegrafos de suelo, ver "Los diez jefes" al final; HP bar), exit arch after the boss (`Room::place_exit`), `advance_chapter()`, Ascension screen → Eliseo (faith halved, 2 slots), Victory after chapter 10, Game Over with `[R]` retry chapter.
- **Working**: miracles with effects (`Player::cast_miracle`) — all 10 are reachable **y cada uno hace lo suyo** (ya no queda ningun placeholder): praying at an `AltarGod` opens a 3-card miracle choice (tier + faith cost + which slot it replaces). Eliseo can learn Elias' miracles and casts them at 70% (`inherit`). Starting sets: Elias = **Cuervos de Querit (Q, Tier 1)** + Fuego del Cielo (E, ch4+); Eliseo = Sanar Aguas + Ejercito Celestial.
- **Ojos Abiertos** (2 Reyes 6:17): pone `Player::map_revealed` (todo el capitulo visible en el minimapa y en TAB, con sus etiquetas TESORO/ALTAR/ELITE/RELATO — lo lee `HUDContext::map_revealed` en `draw_minimap`) y `reveal_timer`, que durante la duracion suelta una marca dorada sobre cada enemigo y azul sobre cada drop desde `Player::update`. `Engine::start_chapter` apaga los dos: revela ESTE capitulo, no los siguientes.
- **Carro de Fuego VISIBLE** (2026-09-24, "tiene que aparecer un carro de fuego de verdad"):
  `Player::chariot_fx_*` lleva un carro con dos caballos de fuego (`assets/sprites/chariot.png`,
  `tools/blender/mantle_sprites/chariot.py`) pegado a los pies del profeta mientras dura la
  embestida y, al soltarlo, lo manda adelante (`CHARIOT_FX_SPEED`), lo sube (`CHARIOT_FX_RISE`)
  y lo apaga en `CHARIOT_FX_DEPART`. Lo pinta `Engine::collect_chariot_draws` en DOS capas
  (detras y delante del profeta en el orden por Y), asi que el sprite del jugador —Elias,
  Eliseo o su skin— va dentro de la caja. Solo es dibujo: dano, embestida y cruce de huecos
  son los de siempre. `place_player` lo apaga (un teletransporte no se lleva el carro).
- **Fuego del Cielo que CAE** (2026-09-24, "la cinematica deberia ver como cae un fuego del
  cielo"; 2 Reyes 1:10): `cast_miracle` ya no hace dano en el acto. Apunta `Player::sky_fire_*`
  (una bola grande sobre el sitio donde se invoco y otra por enemigo del radio) y un aro rojo en
  el suelo; `Player::update_sky_fire` aplica dano, matorral y grietas cuando tocan suelo
  (`Config::SKY_FIRE_FALL` = 0,6 s) con hitstop y sacudida. `Engine::render_sky_fire` oscurece
  el cielo y pinta las bolas POR ENCIMA del mundo (con estela de fantasmas); el Engine hace
  sonar el golpe con `sky_fire_landed`. `place_player` lo cancela, como el carro.
- **La Honda dispara una PIEDRA** (`ProjectileKind::Stone`: la celda `Rock` a 0,6 que gira con
  `ProjectileComponent::spin`). Hasta el 2026-09-24 salia `Arrow` y se veia una flecha.
- **Los relatos se marcan con un "!"** (`Room::render_event_markers`, despues de
  `draw_sorted`): bocadillo sobre cada relato pendiente, PNJ o escena, y el titulo al
  acercarse. Un relato de ESCENA ya no es la antorcha (nadie la leia como interaccion): brilla
  en el suelo, se abre un aro (`ring_tex`) y suben chispas.
- **Aceite Inagotable** (2 Reyes 4:6): el consumible tiene cargas (`Player::consumable_charges`). Recoger otra vasija igual suma carga, una distinta reemplaza; `use_consumable` solo suelta el objeto cuando se gasta la ultima. El milagro da +2 cargas, y con las manos vacias regala un consumible con 2. El numero se pinta sobre la celda del HUD cuando es > 1. Va en el guardado (`SaveData::consumable_uses`).
- **Tres slots de milagro**: `Config::MAX_MIRACLE_SLOTS = 3` (Q / E / F; `Action::Miracle3` = tecla F, D-Pad abajo en mando). Elias sigue con 1 (2 desde el cap. 4); Eliseo con 2, y 3 con la mejora meta 6 de El Santuario. Todo lo que iteraba slots va contra la constante: `Player::miracle_slots`, la tabla `MIRACLE_KEYS` de `player.cpp`, `draw_miracle_slots` (la tercera celda cae en x≈238 de 320 y no pisa ni la caja de mensajes ni la barra de jefe), `open_miracle_choice`, el bot y el comando `learn`.
- **Miracle gating readability**: a Tier-3 miracle needs faith level *Siervo Fiel* (50), and the run starts at exactly 50, so equipping one in slot Q made it lock on the first hit taken and read as "I need 100% faith". The fix is threefold and must stay together: the Q slot always starts Tier 1; `draw_miracle_slots` splits `lock_tier` (padlock + the faith **required**, via `faith_required_for_tier`) from `lock_cost` (cost in red); and `Player::MiracleEvent` carries the reason (`NeedTier` / `NeedCost` / `OnCooldown`) so the Engine names it ("Fuego del Cielo: necesitas 50 de fe (Siervo Fiel)"). `draw_faith_bar` also ticks the 25/50/75 thresholds. A one-shot `[Q]` hint fires in ch. 1 after 5 castable seconds (`Engine::miracle_hint_*`).
- **Working**: items — `GameScene::Choice` offers 3 cards after Elite/Boss rooms, on opening the treasure chest and in 1 of 5 normal rooms (`open_item_choice`). `ItemRegistry::roll_choices` never repeats an owned item, so a run tops out at the 14 sorteables (los tres `event_only` no entran). `Player::grant_item` applies HP/damage/speed/faith/cadence plus relics (Tablas → `FaithSystem::floor_value`, Arca → `miracle_power`) and consumables (R). Heredable items pass to Eliseo on the Ascension. Weapons (Honda/Vara) are a **secondary attack** on `Action::AttackSecondary` (tecla `I`): `Player::fire_secondary` — la Honda lanza una piedra que atraviesa con la animacion de disparo, la Vara barre en arco con `AnimID::CastMiracle` y cinco brotes verdes. Al recogerla el mensaje dice la tecla, y la celda del HUD lleva icono por arma + la `I` + el nombre.
### Catalogo de objetos: 16 -> 22 (roadmap Beta "20+", 2026-08-25)

Se repartieron **donde el pool estaba flojo**, no a bulto: las ARMAS eran dos y los
CONSUMIBLES dos. La tercera RELIQUIA entra a proposito — con solo dos, el slot unico del
GDD 7.3 casi nunca obligaba a elegir. Todos salen de 1-2 Reyes y ninguno contradice el
texto (regla del proyecto; precedente: el Leproso Enloquecido).

| id | Objeto | Categoria | Efecto | Pasaje |
|----|--------|-----------|--------|--------|
| 16 | Arco de Joas | Arma | rafaga de 3 saetas abiertas 14 grados | 2 Reyes 13:17 |
| 17 | Higos de Ezequias | Consumible | **+15 HP MAXIMO** | 2 Reyes 20:7 (+20:6) |
| 18 | Sal de Eliseo | Consumible | **+20 Fe** | 2 Reyes 2:21 |
| 19 | Cinto de Cuero | Pasivo (heredable) | -12% de recarga de ataque | 2 Reyes 1:8 |
| 20 | El Reloj de Acaz | **Reliquia** | perdona una muerte por capitulo | 2 Reyes 20:11 |
| 21 | Panes de Cebada | Pasivo (heredable) | +18 HP, +4 Fe | 2 Reyes 4:43 |

Detalles que importan:

- **El Arco es una RAFAGA, no un tiro mas fuerte**: es lo unico que lo separa de la Honda,
  que manda una sola piedra que atraviesa. Tres saetas a 14 grados caben las tres en un
  cuerpo a bocajarro y barren un frente de lejos.
- **Los Higos suben el MAXIMO** y no curan y ya: el pasaje no acaba en que la llaga sane,
  acaba en "anadire a tus dias quince anos" (20:6). Y **la Sal es el unico consumible que
  devuelve FE**, que es el recurso del juego y hasta ahora solo se ganaba jugando. Por eso
  `Player::use_consumable` toma ahora `FaithSystem&`.
- **El Reloj de Acaz** reusa el revive que ya existia (`Engine::has_revive`, del encuentro
  sunamita) pero con **permiso propio** (`Engine::relic_revive_ready`): asi no depende del
  ORDEN en que se rehaga el equipo (concesion, reintento, carga, Ascension) y **no pisa** al
  perdon del encuentro — se pueden tener los dos. `SDL_Log("Revive used: ...")` dice cual
  salto, que es lo unico que hace comprobable desde un guion algo que solo se ve un
  instante en un banner.
- Lo que **escala** con el catalogo y se comprobo: `codex_items` es `u32` (22 de 32 bits,
  cabe), `SaveData::item_ids[24]` aguanta el techo de una run (19 sorteables + 3
  `event_only` = 22), el contador del Codice pasa a **50** (10 capitulos + 8 enemigos +
  10 milagros + 22 objetos) y la fila de objetos del HUD sube su tope de 12 a **20** celdas
  (220 de las 320 unidades de ancho: no llega al minimapa).

Guiones: **`scripts/items_check.txt`** (los seis, con `stats` antes/despues) y
`codice_check.txt` ampliado. Ojo en items_check: forzar la muerte para probar el Reloj pide
**~20 s de leones encima**, porque los i-frames de 1 s limitan el mordisco a unos 11 de
dano por segundo.

### Reliquias: un solo slot (GDD 7.3, 2026-08-25)

El GDD dice "muy poderoso, **1 slot**" y `items.h` lo documentaba, pero nada lo imponia:
se podian llevar las Tablas **y** el Arca y sus efectos se sumaban. La trampa es que
`Player::grant_item` **aplica** efectos permanentes, y para poder quitar una reliquia hay
que poder deshacerlos.

- **`Player::apply_relic_effects(faith)`**: los dos efectos de reliquia
  (`FaithSystem::floor_value` de las Tablas y `Player::miracle_power` del Arca) se
  **recalculan desde `relic_slot`** en vez de acumularse al conceder. Vuelve a su base y
  aplica lo que haya en el slot, asi que es **idempotente** y se puede llamar mil veces.
- **Se llama tambien al REHACER el equipo** (reintento de capitulo, Ascension y carga de
  partida), y eso arreglo un fallo real: en la Ascension **la fe no se reinicia**, asi que
  un Elias con las Tablas dejaba a Eliseo con el suelo de fe en 25 para siempre sin llevar
  la reliquia.
- La reliquia sustituida **sale de `items`**: deja de contar en la fila del HUD, en el
  guardado y en la herencia. Y conceder la misma reliquia dos veces ya **no la duplica** en
  la lista (en juego normal no pasa —`roll_choices` no ofrece lo que llevas— pero el
  comando `give` de los guiones si, y ahi se vio).
- **Legibilidad**, que es la mitad del trabajo: la carta de eleccion de una reliquia dice
  **"SUSTITUYE A \<la que llevas\>"** (el campo `ChoiceCard::extra`, que ya existia para
  esto), el aviso al tomarla dice **"X sustituye a Y"**, y la fila de objetos del HUD
  marca con un recuadro claro **cual llevas puesta** — sin eso no se distingue "la que me
  hace efecto" de "una legendaria mas".
- La linea `LOADOUT` de `stats` trae ahora **la reliquia y sus dos efectos**: son lo unico
  que no se deduce de la lista de objetos.

Guion: **`scripts/reliquias_check.txt`**. Fuerza la carta en vez de fiarla al sorteo — le
da todos los objetos sorteables MENOS el Arca, asi que el pool se queda con una sola carta.
**Ojo con la espera antes de la captura: en autoplay la carta se toma sola a los 0,7 s**
(`Engine::open_item_choice` fija `choice.auto_timer` para que un guion no se atasque).

### La Ascensión completa y El Manto de Elías

`Engine::finish_ascension` cierra las cinco herencias del GDD. Además de la fe (50 %, 60 % con
`Herencia Ampliada`), los objetos heredables y el bestiario:

- **El Manto de Elías** (`ItemRegistry` id **15**): `Passive` + `Legendary` + `heredable` +
  **`event_only`** — nunca sale en una carta de elección, se gana *solo* en la Ascensión. Su
  efecto no puede pisar al Arca (`miracle_power`) ni a las Tablas (`floor_value`), así que abre
  **el tercer slot de milagro** y baja la recarga al **75 %** (`Player::miracle_haste`, aplicado
  al FIJAR el cooldown en `MiracleSlot::try_cast`, no al descontarlo, para que el barrido del HUD
  y el contador digan lo mismo). Se concede en `grant_item`, que se vuelve a llamar al reintentar
  capítulo y al cargar: los dos efectos son **idempotentes** a propósito.
- **El milagro heredado**: el de mayor tier de los que Elías llevaba equipados viaja al slot que
  abre el Manto y Eliseo lo lanza al 70 % (el factor `inherit` de `Player::cast_miracle`, que ya
  existía). Eliseo conserva sus dos propios (Sanar Aguas + Ejército Celestial): por eso hacen
  falta tres slots.
- Se solapa a propósito con la mejora meta 6 (`Doble Porción Mejorada`), que sigue dando los 3
  slots a quien continúe una partida del Acto II sin pasar por la Ascensión en esa sesión.

### El epílogo "El Legado" es desbloqueble

Ganar el capítulo 10 **con la fe en Profeta Ungido (≥ 75)** desbloquea el epílogo, y queda
desbloqueado para siempre (`SaveData::unlocks & UNLOCK_EPILOGUE`). Quien gane con la fe por los
suelos se lleva igual su pantalla de victoria, con una nota que dice por qué no lo ha visto: la
victoria nunca se queda sin cierre. Las partidas v4 que ya tenían `runs_completed > 0` llegan con
el epílogo desbloqueado.

### Resuelto: el "Leproso Enloquecido" ya no existe (2026-08-20)

El GDD traía un `EnemyType::LeprosoEnloquecido` que **contradecía el texto bíblico** (en
2 Reyes 5 Eliseo *sana* al leproso; en 2 Reyes 7:3-10 los cuatro leprosos de Samaria son
los que traen la buena noticia — héroes, no carne de cañón). Se sustituyó por
**`EnemyType::IdolatraDeRimon`** (2 Reyes 5:18, «cuando mi señor entra en el templo de
Rimón para adorar en él»): mismas cifras (20 HP / 8 daño / `AIBehavior::ExplodeOnDeath`)
y misma maquinaria de mecha; lo que estalla ahora es el **brasero de brasas** que carga,
así que el VFX y las partículas pasaron de verde-peste a naranja-ascua. Sale en el cap. 8
y lo invoca el patrón `LaMuerte`. Hoja: `assets/sprites/idolatra_rimon.png` (receta
`characters.py`: túnica morada larga, capucha echada atrás con `hood_open=True` — calada
tapa la cara entera — y `censer=True`, el brasero colgado de `hand.L`). El jefe del cap. 9
pasó a llamarse "Guiezi el Codicioso": su lepra es el castigo, la codicia es lo que se
combate.

Si algún día aparecen leprosos, que sea como **NPC a los que sanar** (encuentro de
`game/events.h`), nunca como enemigos.

### El tutorial: mapa propio de tres salas (`tutorial.h/cpp`, 2026-09-12)

Se ofrece al pulsar **Nueva Partida** (pregunta de dos botones) y se juega en SU mapa, no
dentro del capítulo 1: no toca el mundo fijo de ningún capítulo ni entra en `floor_map`.
Las decisiones de diseño, tomadas por el usuario, están en la cabecera de `tutorial.h` y no
se re-discuten. Lo que hay que saber antes de tocarlo:

- **Corre sobre `GameScene::Playing`** con `tutorial.active` puesto. No es una escena nueva:
  el bloque de Playing son 400 líneas de combate, cámara, milagros y HUD que no tiene sentido
  duplicar. Las tres salas se montan sobre `current_room_data` igual que el escenario de una
  escena guionizada (`Engine::start_scene`), y se deshacen solas al salir.
- **15 pasos** (`TUTORIAL_STEPS`), y **ninguno se cierra solo** (2026-09-13): si el cartel
  nombra una acción, esa acción es lo que lo cierra. Ya no hay relojes. Lo puramente
  informativo (qué es la vida, qué es la fe, sus grados) dejó de ser un paso —no es una
  acción, es una explicación— y va al final en forma de diálogo (`story_tutorial_outro`).
  Las sondas están en `Engine::tutorial_goal_done` y **miran el mundo** en vez de que medio
  motor tenga que avisar. Cuatro no son evidentes: el milagro se detecta por el **flanco del
  enfriamiento** (`Player::last_event` lo limpia el bloque de Playing antes); las tinajas de
  la sala 1 **contando los tiles que quedan**; los escondrijos de la sala 3 por **posición de
  tile** (`tutorial_secrets_opened`, porque ahí el bioma decora con matorral y contar props
  cerraba el paso con dos tiros perdidos); y la pausa y el arco con **dos flags que el motor
  apunta a mano** (`pause_seen`, `exit_reached`), porque pasan con el juego fuera de Playing.
- **`update_tutorial` no evalúa NADA fuera de `GameScene::Playing`**, y eso es lo que pone el
  cofre en su orden: pisarlo llama a `open_item_choice`, que fija `state.scene` en el acto, así
  que el paso `Chest` se cierra al VOLVER de la carta, con ella ya en la mano. Antes salía
  después un cartel explicando "eliges una de tres" con la carta cerrada y nada que hacer;
  ahora la explicación va en el **título** de la propia carta (`tutorial_choice_title`). Lo
  mismo vale para el altar del paso `Pray` y para la pausa.
- **Cruzar el arco termina el tutorial** (paso `CrossExit`) y la rama del arco de
  `update_room_state` está guardada contra `tutorial.active`: sin esa guarda, caminar sobre él
  en cualquiera de los últimos pasos lanzaba el epílogo narrado del capítulo 1 y
  `advance_chapter()` con el tutorial todavía puesto.
- **`tutorial_setup_step` repone lo que el paso que se abre necesita EN EL MUNDO**, y eso no es
  un lujo desde que no hay relojes: un paso que mide un FLANCO de algo que el jugador pudo
  gastarse antes de que se lo pidieran deja el tutorial colgado PARA SIEMPRE. Van tres casos,
  y los tres por la misma razón:
  - **La fe, entera, en cada paso de milagro**: Fuego del Cielo es Tier 3 (pide 50) y cuesta
    20, así que llegar ahí con la fe mordida por el arquero dejaba el paso IMPOSIBLE.
  - **Las tinajas del paso de atacar** (2026-09-15, del feedback del tester): la sala 1 solo
    trae cinco y el paso pide romper dos, así que quien se entretiene a tiros durante el paso
    de moverse llegaba con cuatro o cinco rotas. Se replantan con `tutorial_plant_jars` — el
    mismo helper que usa `tutorial_build_room`, para no tener las coordenadas en dos sitios —
    y **acto seguido se REHACE `tutorial.prev_props`**: el bucle de `update_tutorial` lo fija
    ANTES de llamar aquí, así que sin ese recálculo el paso pediría romper más de la cuenta.
  - **La poción del paso del consumible**: solo hay una carga, dada al arrancar. Se repone
    solo si `consumable_slot == UINT32_MAX`, para no apilar una segunda a quien no se la haya
    bebido. Con el slot repuesto, el botón táctil USAR vuelve a dibujarse solo.
  Guion: `scripts/tutorial_gastado_check.txt`.
- **Al terminar salen dos páginas de cierre** (`story_tutorial_outro`, `ui/story.h`): la vida,
  la fe y sus grados, las ofrendas; y el camino que queda, con el objetivo del capítulo 1 al
  pie. Son además la pantalla que faltaba entre el tutorial y el diálogo de Acab y Jezabel.
  **Saltarlo no las enseña**: quien pulsa SALTAR pide salir, no leer dos pantallas más
  (`finish_tutorial(bool completed)`).
- **El equipo del tutorial NO viaja** y **el guardado tampoco se toca**: `start_tutorial`
  copia `SaveData` entera y `finish_tutorial` la restaura. Sale más barato que sembrar
  guardas por cada sitio que toca `total_kills`, `miracles_cast` o los logros al jugar. La
  ofrenda del paso 12 se cuenta pero **no se ingresa**.
- **No se puede perder**: `update_tutorial` sujeta la vida en 1 antes de que la comprobación
  de muerte la vea en cero. Los enemigos sí pegan, que es lo que hace que el dash signifique
  algo.
- **Tres cosas de la colocación se decidieron MIRANDO la captura, no razonando**: nada que el
  paso mande mirar puede ir en las filas 2-3 de un interior (ahí va el cartel, y=34..62) ni a
  más de dos tiles por encima del profeta en el exterior; el altar va **descentrado**, porque
  en la columna del profeta se rezaba sin querer al cruzar hacia la puerta; y los escondrijos
  van a **cuatro tiles**, porque un tiro alcanza 120 unidades (7,5 tiles) y más lejos no se
  podían abrir sin caminar hasta ellos.
- Guiones: `tutorial_check.txt` (los 15 pasos + las dos pantallas nuevas + el camino a la
  partida), `tutorial_tactil_check.txt` (`--touch`) y `tutorial_en_check.txt`.

### Los diez jefes: una estrategia por capitulo (2026-10-03)

Queja del usuario, literal: *"Los jefes deben de ser distintos a los enemigos principales,
cada jefe debe de tener una estrategia distinta."* Hasta hoy `spawn_boss` montaba cada jefe
sobre un enemigo comun (la misma hoja a 1,6) y `update_boss_ai` les daba casi los mismos
verbos: perseguir, rafaga o anillo de proyectiles, invocar. Ahora cada uno tiene su HOJA,
su MAQUINA DE ESTADOS y una VENTANA que el jugador aprende a castigar.

**Donde vive cada cosa:**

- `game/hazards.h/.cpp` — la caja de herramientas: peligros de suelo TELEGRAFIADOS en una
  piscina fija (`ECSWorld::hazards[64]`; `struct Hazard` en `core/ecs.h`; no son entidades).
  Seis tipos: `Circle` (aviso y estallido), `Lane` (rectangulo en un rumbo: cargas, cadena,
  lluvia de flechas), `Cone` (barrido), `Ring` (onda que se abre —o se CIERRA— con hueco),
  `Pool` (charca con vida, muerde con su cadencia) y `Push` (viento). Tres tramos: AVISO
  (`warn`: el relleno crece y el borde late mas deprisa segun se acaba), EFECTO (`active`: el
  unico tramo con dano) y una cola de 0,24 s sin dano para el fogonazo. `HazardTag` dice que
  significa el impacto para el dueno (`Chain` tira y cobra fe, `Debt` cobra fe, `Ripple` es
  solo dibujo y sigue al dueno). Un peligro muere con su dueno.
- `game/bosses.cpp` — los diez patrones, `spawn_boss` / `spawn_boss_group`, los props
  (idolos, senuelos), las marcas (`render_boss_ground` / `render_boss_overlays`) y
  `on_boss_death`. `enemies.cpp` se queda con los enemigos comunes y exporta sus utilidades
  (`enemy_shoot`, `enemy_volley`, `enemy_ring`, `enemy_free_spot`, `count_alive_minions`).
- `rooms.h` `get_boss_sheet(cap, cual)` — la hoja propia (`sprites/boss_*.png`; en el cap. 5
  `acab.png` + `jezabel.png`). `Engine::load_boss_sheets` la carga al empezar el capitulo y,
  si el fichero no esta, cae a la del enemigo base con UNA linea de log (`Boss sheet
  ausente`) y sin pasar por `load_texture`, que loguearia "Failed to load" (gate negativo de
  varios guiones). Con hoja propia el jefe va a 1,4; con la prestada, a 1,6 como antes.

**Reglas que valen para los diez** (y para un jefe once):

- El dano de un peligro al profeta respeta EXACTAMENTE lo mismo que un proyectil enemigo:
  `invulnerable`, `iframes_timer` (golpe y DASH) y el modo dios. Un peligro esquivado con
  i-frames ya no vuelve a morder aunque los i-frames se acaben a mitad: es lo que hace que
  dashear A TRAVES de un barrido funcione.
- `health.invulnerable` del jefe se escribe CADA frame en su patron (`system_combat_timers`
  lo resetea antes de la IA). `EnemyAIComponent::vuln_mult` > 1 es la VENTANA (aturdido, de
  rodillas, el silbo): `apply_damage` multiplica y en el suelo late un aro dorado. Intocable
  = burbuja violeta + barra gris + un aviso propio de cada patron (`boss_invulnerable_hint`
  -> `HUDContext::boss_invul_hint`; "acercate" solo tiene sentido para la Muerte).
- Aviso x1,2 y embestidas x0,9 en movil (`wk()` / `sk()` en bosses.cpp): la pelea no es mas
  dura en el telefono. El golpe de referencia de un peligro es `hit_unit(cap)` = 7,5 + 0,75
  por capitulo; los proyectiles siguen con el dano de siempre del jefe.
- Los PROPS (`EnemyAIComponent::prop_kind`: idolo, senuelo) son hostiles —se les dispara y
  cuentan como vivos— pero no van en la barra, no sueltan botin (el idolo da la fe de un
  profeta) y MUEREN CON SU DUENO (`on_boss_death`), igual que su plata y sus peligros: la
  arena se cierra por `count_living_enemies` como siempre. Las invocaciones topan contra los
  secuaces de SU celda (`count_boss_minions`), no contra los de toda la zona: con el recuento
  global, los enemigos de las celdas vecinas llenaban el techo y Ben-adad no traia oleadas.
- El ESCENARIO (`stage_of`): la parte de la arena que no tapa el HUD. Teletransportes, plata
  y el "guardar distancia" (`keep_range` tira hacia dentro) se quedan ahi: en las capturas
  Guiezi y el Acreedor se escondian bajo el minimapa o tras las celdas de milagro.
- La auto-mira (`pick_target`) castiga lo intocable con -600 (con -160 el bot seguia
  disparando al Sacerdote con los idolos en pie y la pelea duraba 111 s) y da a los
  senuelos de Guiezi el mismo +90 que al jefe: si no, el movil leeria la marca por el jugador.
- Los peligros se pintan en el SUELO (`render_hazards` justo despues de `render_ground`,
  antes de `draw_sorted`): bajo sombras, props y personajes. Todo con texturas procedurales
  blancas tintadas (`HazardArt`, generadas en `Engine::init`): disco con el borde vivo, una
  capa OSCURA debajo para que se lean sobre la arena clara del desierto, y bordes de GROSOR
  FIJO hechos con trozos de haz en rampa (el aro escalado con el radio quedaba de medio pixel
  en un circulo de 13 unidades, y los trozos de canto duro hacian costuras "|||").
- Se vacian en `load_room`, `place_player`, `start_scene` (escenario), al entrar en la arena,
  con `world.clear()` (reintento) y al morir el ultimo del jefe.

**Los diez** (vida = (200 + 50 x cap) x factor; "bot" = segundos con el bot de autoplay en
partida limpia con `god`, punteria perfecta: un jugador de verdad, esquivando, tarda el doble
o el triple):

| Cap | Jefe | Estrategia | Como se gana (la ventana) | Factor | Bot |
|---|---|---|---|---|---|
| 1 | Capitan de Acab (1 R 18:10) | CARGA en carril (0,8 s de aviso, 270 u/s). Contra el muro queda ATURDIDO 1,6 s (estrellas). ESCUDO: rebotan los tiros que entran por un frente de 120 grados que gira despacio (2,6 rad/s; arco azul en el suelo, "clinc"). Al 50% dos arqueros a los bordes y cargas encadenadas | De espaldas a un muro, apartarse del carril y castigar el aturdido (x1,5, sin escudo); o rodearle | 1,40 | 29 s |
| 2 | Sacerdote de Baal | Tres IDOLOS (celda del altar de Baal) con atadura morada: intocable mientras quede uno. Rotos, 7 s de ventana; 1,4 s antes de rebrotar se pintan sus sitios (y queman). LLUVIA DE FUEGO: 3 circulos (5 al 50%), uno siempre sobre el profeta; abanicos de rayos | Idolos primero (fe de profeta cada uno) y en la ventana, a el | 1,30 | 23 s |
| 3 | Los 450 (1 R 18:26-29) | El corro de siempre + EL CORRO LE PROTEGE: con 3 o mas de pie la cabeza es intocable (ataduras rojas). Repone uno cada 6 s (4 en el frenesi). RITUAL cada 11 s (8): el corro se para y clama, gran circulo de fuego en el altar (2,2 s) y onda con hueco. Frenesi al 50% | Ralear el corro durante el ritual (quietos, sin disparar) y rematar la cabeza antes de que lo rellene | 0,75 | 22 s |
| 4 | Tu Propia Desesperacion (1 R 19:11-12) | Fases por vida, cada cambio con fogonazo, sacudida, corona de su color y 1,2 s intocable: VIENTO (rafaga `Push` que cruza la arena + ondas con hueco), TERREMOTO (ondas de suelo + cinco grietas), FUEGO (tres lineas de circulos que MARCHAN hacia el profeta) y el SILBO APACIBLE: deja de atacar, dano x2 | Buscar el hueco (o dashear la onda), no pelear contra el viento | 1,70 | 21 s |
| 5 | Acab y Jezabel (1 R 21) | DOS entidades con barra compartida (Jezabel es `boss_group`). Acab: barrido de espada en abanico (0,6 s) si estas cerca —tras el, 0,7 s abierto— y carga corta. Jezabel: guarda distancia, se teletransporta lejos si te acercas (deja una maldicion), circulos malditos, rayos, y cada ~8 s CURA a Acab un 10% con una atadura verde que se CORTA si ella recibe dano. Al caer uno, el otro se enfurece (x1,3 velocidad, cadencias x0,65) | Cortar cada curacion: ella se queda quieta canalizando | 0,90 + 0,65 | 20 s |
| 6 | Espiritu de Corrupcion (2 R 2:19-22) | CHARCAS de 8 s por donde pasa. Cada ~9 s se SUMERGE: intocable, sombra con ondas que va hacia el profeta, y EMERGE donde estaba (circulo de 0,8 s) con una onda con hueco. Se parte al 66% y al 33%. Sanar Aguas disuelve las charcas que toca | Moverse cuando se sumerge y castigar la salida (1,1 s a x1,35) | 1,00 | 38 s |
| 7 | El Acreedor (2 R 4:1-7) | CADENA en carril (0,7 s): si alcanza, tira de ti, cobra 6 de fe y le cura un 3%. Tras cada lanzamiento 1,2 s recogiendola (x1,5). Abanicos de MONEDAS. Al 50% LA DEUDA: un anillo de monedas se cierra sobre ti (2,6 s; cobra 5 de fe). Cobradores (soldados sirios) cada 14 s | Salir del carril y castigar la recogida; salir del anillo antes de que se cierre | 1,30 | 20 s |
| 8 | La Muerte (2 R 4:32-35) | OSCURIDAD: solo hay luz alrededor del profeta (`ECSWorld::darkness_req` -> `Engine::render_darkness`; sus ojos brillan en lo oscuro). Intocable a mas de 40 u. Si te acercas, GUADANA en abanico de 140 grados (0,65 s); al 50% vuelve con un segundo tajo y la luz se estrecha. Idolatras de Rimon como sombras; anillos lentos si huyes | Tras el tajo queda SIN RESUELLO 1,6 s: tocable desde cualquier distancia | 1,00 | 36 s |
| 9 | Guiezi el Codicioso (2 R 5:20-27) | Se teletransporta cada 5,5 s (4,5) y deja hasta dos SENUELOS (su hoja, translucidos, 1 de vida, disparan, revientan en plata). El de verdad lleva chispas blancas de lepra. CODICIA cada 12 s: lanza tres talentos (circulos plateados) y corre a arrodillarse junto a cada uno (1,2 s, x1,75). Al 50% charcas de lepra donde se teletransporta | Aprender la marca y castigarle de rodillas; reventar un senuelo siembra plata que le tienta | 1,50 | 20 s |
| 10 | Ben-adad de Siria (2 R 6-7) | ARTILLERIA en patron sobre una rejilla 4x3 (columna libre, damero o dos filas: siempre queda camino), LLUVIA DE FLECHAS en tres carriles, CARRO en carril (330 u/s), oleadas de sirios por los bordes, generales al 60%. Al 30% LA HUESTE CELESTIAL (2 R 6:17-18): fogonazo, carros de fuego alrededor, sus soldados quedan CIEGOS 6 s (palidos, vagan, no atacan) y el rey se vuelve lento y vulnerable (x1,5 8 s, luego x1,2) | Leer los huecos de la artilleria y, cuando el cielo arde, ir a por el rey | 1,00 | 27 s |

**Cap. 5: la escena del jefe ya no planta figuras.** Acab y Jezabel SON el jefe (nacen a dx -2
y +2 del ancla, donde estaban sus figuras), asi que `get_scene_def(JEFE_5)` lleva
`{none, none, none}`: hablan los mismos que luego pelean. El texto no cambio.

**Fidelidad (regla del proyecto):** nada contradice 1-2 Reyes. Los idolos de Baal y el fuego
que no baja son 1 R 18; el silbo apacible es 19:12 y por eso esa fase NO ataca; la curacion de
Jezabel es licencia de juego sobre 1 R 21 (ella mueve los hilos: 21:7-8); el Acreedor cobra
la fe porque viene a cobrar en hijos (2 R 4:1); la Muerte cede a quien se tiende encima
(4:34); la plata de Guiezi son los dos talentos de 5:23; la ceguera de los sirios es 6:18.

**Comando nuevo** `bosshp <fraccion> [quien]`, y `killall` ahora atraviesa lo intocable: ver
`scripts/CLAUDE.md`. Guion: `scripts/jefes_check.txt`.
