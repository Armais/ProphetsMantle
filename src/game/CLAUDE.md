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
- **Working**: progression — room clear detection, boss per chapter (`spawn_boss`: volleys, ring, charge, HP bar), exit arch after the boss (`Room::place_exit`), `advance_chapter()`, Ascension screen → Eliseo (faith halved, 2 slots), Victory after chapter 10, Game Over with `[R]` retry chapter.
- **Working**: miracles with effects (`Player::cast_miracle`) — all 10 are reachable **y cada uno hace lo suyo** (ya no queda ningun placeholder): praying at an `AltarGod` opens a 3-card miracle choice (tier + faith cost + which slot it replaces). Eliseo can learn Elias' miracles and casts them at 70% (`inherit`). Starting sets: Elias = **Cuervos de Querit (Q, Tier 1)** + Fuego del Cielo (E, ch4+); Eliseo = Sanar Aguas + Ejercito Celestial.
- **Ojos Abiertos** (2 Reyes 6:17): pone `Player::map_revealed` (todo el capitulo visible en el minimapa y en TAB, con sus etiquetas TESORO/ALTAR/ELITE/RELATO — lo lee `HUDContext::map_revealed` en `draw_minimap`) y `reveal_timer`, que durante la duracion suelta una marca dorada sobre cada enemigo y azul sobre cada drop desde `Player::update`. `Engine::start_chapter` apaga los dos: revela ESTE capitulo, no los siguientes.
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
