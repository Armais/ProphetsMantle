# El mundo exterior

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`rooms.h/cpp`): leelo antes
de tocar `Room::generate`, `decorate`, `carve_trail` o `render_ground`.

## Zona continua (2026-09-10)

Decision del usuario: *"el mapa sea una exploracion libre, no por cuadrados"*. Un capitulo
es desde hoy **UN solo mapa** (`RoomType::Zone`): las celdas de la rejilla (40x22, la unidad
de generacion pura de los mapas fijos) se cosen en un lienzo de hasta 160x88 tiles, sin
anillos entre ellas, sin fundidos, con la camara siguiendo al profeta de punta a punta. Las
secciones de mas abajo que hablan de "salas", "transicion por bordes", `try_room_transition`,
`exits_locked` y `room_cache[i]` son HISTORIA: describen como se llego a la celda actual y
por que las cosas se cuentan como se cuentan, pero el motor ya no hace nada de eso.

**Piezas, en orden de dependencia:**

1. **La celda sigue siendo la unidad pura.** `Engine::generate_pristine_cell(nodo, out)` es
   el `generate_pristine_room` de siempre (siembra `chapter_seed ^ ((nodo+1)*2654435761)` y
   genera 40x22 en local). `Engine::build_pristine_zone` la llama para cada nodo de la
   rejilla y `Room::blit_cell` la pega en `(gx*40, gy*22)`; luego `Room::open_seams`. Es
   una funcion pura de (semilla, capitulo, rejilla): `Engine::pristine_zone()` la cachea y
   `capture_room_states` diffea contra ella (regenerar 16 celdas en cada guardado daria un
   tiron en mitad de la pelea). Cambiar el generador de UNA celda cambia solo esa celda;
   el resto de los mapas siguen siendo fijos.
2. **Costuras** (`open_seams`): las dos filas/columnas de anillo de cada costura interior
   pasan a suelo SI eran solidas (una senda que llega al borde se queda). En bioma
   `Natural` el vano ya ocupaba casi todo el lado y la franja irregular se salta el vano,
   asi que la costura **no se ve**: el campo es continuo. En `Masonry` (6, 8, 10) la
   silleria queda solo en el PERIMETRO y las costuras se abren igual; las antorchas que
   flanqueaban el vano quedan flanqueando el paso, y se lee como una puerta de ciudad.
   Deuda visual aceptada: los rios y las charcas se generan por celda y mueren una fila
   antes de la costura.
3. **Estado por celda** (`CellState` en `Room::cells[i]`, i = nodo de la rejilla): visited,
   cleared, altar_used, chest_opened, event_used, active, event_id, el nudo (hub) y los
   spawns en coordenadas de zona. Un interior o un escenario de escena son `cells[0]`. Los
   flags planos de `Room` han desaparecido; `exit_open`/`exit_tx/ty` siguen planos porque el
   arco es uno por zona.
4. **Celdas VIVAS** (`Engine::update_cells`, cada frame de Playing, antes de
   `update_room_state`): la celda bajo los pies del profeta pasa a ser `floor_map.current_room`
   (log `Entered cell N [gx,gy] (Tipo)`); una celda de combate a menos de media pantalla
   de su rectangulo siembra sus enemigos (`spawn_cell_enemies`, `EnemyAIComponent::home_cell`
   = la celda) y a mas de 1,2 pantallas los repone (`despawn_cell`: por celda de ORIGEN,
   nunca por posicion — persiguen a traves de las costuras). Limpieza: la primera celda
   activa con `count_living_enemies(i) == 0`, UNA por frame (`killall` vacia varias y
   `open_item_choice` no se abre dos veces). Las invocaciones, los hijos del Agua Corrupta y
   el corro de Baal heredan la celda de quien los engendra; `spawn` del autoplay usa la
   actual; un enemigo con `home_cell == -1` cuenta en todas y no se despawnea.
5. **La arena vive DENTRO de la celda B** (`stamp_arena`, en `Room::generate` para
   `RoomType::Boss`): recinto de una pantalla de suelo (20x11) pegado al borde de arriba
   de la celda, cerco de `Rock` en todos los biomas (un muro de un tile de grueso no se
   dibuja: `collect_props` solo pinta la cara de abajo de un bloque) y vano de 4 tiles en
   el cerco de abajo. El nudo de la celda B baja a cuatro filas del borde inferior, fuera
   del recinto; una senda recta une el vano con el nudo; si el estampado corta una senda,
   `carve_straight` repara con una L determinista por fuera del cerco. `Engine::enter_arena`
   se dispara cuando los pies pisan el suelo del recinto (`arena_rect_world`): nace el jefe
   (o el grupo), `arena_sealed = true` (un flag: `is_tile_walkable` y
   `is_tile_solid_for_projectiles` cierran el vano sin tocar tiles, asi que el diff del
   guardado no lo ve), banner, voz y el dialogo del jefe. La camara se clava al recinto CON
   su cerco (`arena_bounds_world`) mientras el profeta esta dentro y el jefe vive; los
   limites de `Camera` se DESLIZAN (`bounds_goal`) para que cruzar el vano no pegue un salto.
   Al caer el ultimo: `place_exit()` en la segunda fila del recinto.
6. **Interiores y escenarios**: `FloorMap::room_cache` son DOS ranuras (0 = la zona mientras
   se esta en un interior o en un escenario de escena, 1 = el interior mientras se esta
   fuera). `load_room(nodo)` despacha: nodo interior -> ranura 1 (cacheando antes la zona);
   nodo de rejilla -> construye la zona si no esta (`zone_built`) o la recupera de la ranura
   0 si lo activo no es la zona, y `enter_cell` teletransporta al nudo (o a `spawn_tx/ty` al
   volver por la puerta). El fundido (`room_fade`) queda para el arranque del capitulo y la
   puerta. `use_doorway` vuelve a la puerta en coordenadas de zona (la puerta se coso con su
   celda: `Room::door_tx/ty` planos en la zona).
7. **Guardado v10** (`save.h`): misma forma de fichero que la v9; `room_flags[i]` /
   `room_open[i]` son por CELDA (i = nodo, bits en local, stride 40) y el interior va en su
   nodo. Como la celda del jefe cambio de forma y los secretos se reparten ahora por
   coordenadas de zona (`tile_hides_loot` hashea la zona), un resumen v9 aplicado tal cual
   abriria tiles que no eran secretos: la migracion lo pone a cero y `current_room = -1`
   (celda de inicio). Al Continuar, `apply_saved_cell_state` corre sobre la zona recien
   construida y el interior con estado se pre-genera en la ranura 1.
8. **Pintado por vista**: `render_ground` y `collect_props` reciben el `Rect` visible y
   solo recorren la vista mas un tile (dos por arriba, por los props altos). `tile_hash`
   recibe coordenadas de zona, asi que el reparto de decorados y de hallazgos secretos se
   redistribuyo (determinista igual).
9. **Autoplay**: `goto <celda|tipo>` teletransporta al nudo; el jefe se entra ANDANDO
   (`goto boss` + `walk 1600 150`: el vano esta en (1568..1632, 192..208) porque la celda
   B es (2,0) en todos los capitulos); `walk` va en coordenadas de ZONA (celda (gx,gy) =
   +(gx*640, gy*352)); `pit` busca el hueco MAS CERCANO; `escena` de un jefe llama a
   `enter_arena` tras `load_room`. Logs nuevos: `Zone built`, `Entered cell`, `Arena
   sealed`, `Cell N cleared/despawned`. Los gates `Loaded room N [gx,gy] (Cap) - Tipo [n
   enemies]` y el volcado ASCII de 40x22 se conservan por celda.

## Mapa grande, el jefe en otro borde y sus pistas (2026-09-24)

Tres decisiones del usuario: *"el mapa es muy pequeno, tiene que ser mas grande"*, *"la
ubicacion del jefe debe cambiar en cada capitulo y no debe ser visible desde el inicio, sino
que lo aprendes a medida que vas avanzando"* y *"en el minimapa deben aparecer tambien los
puntos que se deben explorar"*.

1. **Rejillas 6x4, y 6x5 en los caps. 5 y 10** (`ChapterDef::grid_w/h`): el doble de celdas.
   `Config::ZONE_CELLS_X/Y` = 6x5 (zona de hasta 240x110 tiles, 26 KB por `Room`, los tres
   estaticos), `FloorMap::MAX_ROOMS` = 36 (30 + el interior, con holgura) y guardado v11. El
   mapa del minimapa y la textura del mapa grande no cambiaron de forma: dividen por el
   tamano real.
2. **`CHAPTER_LAYOUT` nuevo**: el jefe en un BORDE distinto por capitulo (1 NE, 2 O, 3 N,
   4 E, 5 S, 6 NO, 7 E, 8 S, 9 O, 10 N) y el inicio a 6-7 celdas de el. Por capitulo: 2
   tesoros, 2 altares, 3 elites (arrimadas al jefe: son su guardia y dan pistas), 4 pasos
   tranquilos (`.` = `RoomType::Rest`: sin enemigos, se decora como cualquier otra) y el
   resto combate; 3/3/4/5 en las 6x5. Todos los relatos del pozo, como siempre.
3. **La arena, pegada al lado que da al borde** (`ArenaSide`, `stamp_arena(r, side)`): la
   celda B sabe de que lado toca por sus `door_*` (sin vecino arriba = North, si no oeste,
   este, abajo). North es la de siempre (vano abajo); en West/East el recinto va pegado al
   borde lateral y el vano en el cerco que mira al resto de la celda; en South, pegado abajo
   y con el vano al este. **El jefe (el ancla) sigue en la fila de arriba del suelo** en los
   cuatro casos: las diez escenas de jefe y la entrada no notan de que lado se entra. El vano
   es un rectangulo (`arena_gap_x0..x1, y0..y1`) y la banda roja del sellado sale vertical u
   horizontal segun el.
4. **Nada talla el recinto**: `carve_tile` se niega a tocar `in_arena_box`, y la senda de
   reparacion de la celda B es un BFS que lo rodea (`carve_around`) en vez de la L de antes,
   que con la arena en un lateral cruzaba el recinto y le abria un boquete al cerco. El nudo
   de la celda B va seis tiles enfrente del vano (`boss_hub`).
5. **`Engine::check_zone`** inunda la zona recien construida desde el nudo de inicio y loguea
   `Zone check: vano ok, cerco entero, nudos N/N, puntos M/M, puerta ok -> OK`: vano
   alcanzable, cerco sin agujeros, todos los nudos, todos los puntos de interes y la puerta.
   Es el gate de `mapas_check` y salta al construir la zona que se juega (no la pristina).
6. **Pistas del jefe** (`get_boss_intel`, `game/rooms.h`): nivel 0 = nada; 1 pista = el
   RUMOR (un circulo que contiene la arena, desplazado hacia el centro del mapa entre media y
   una celda: si estuviera centrado seria el sitio exacto con otro dibujo); 2 pistas = el
   sitio. Pistas = relatos resueltos + elites despejadas + altares rezados. Se ve SIN pistas
   al pisar una celda vecina de la del jefe (desde ahi se ve la arena), al entrar en ella o
   con Ojos Abiertos. Es una funcion PURA del estado de celdas que ya viaja en el guardado,
   asi que un Continuar sabe lo mismo sin campos nuevos. `Engine::update_boss_intel` solo
   AVISA al subir de nivel (banner "PISTA" con el rumbo, log `Boss intel A -> B`), y con
   `boss_intel_shown = -1` (lo pone `start_chapter`) toma el nivel sin avisar al cargar.
7. **Puntos de interes en los dos mapas** (`src/ui/CLAUDE.md`): "?" en lo no descubierto,
   chincheta por tipo en lo visto, apagada en lo hecho.

Guiones: `goto rest`; `walks x y` (relativo al origen de la celda de INICIO) y `walkarena d`
(a `d` unidades del centro del vano, positivo hacia dentro, valga el lado que valga) para
que un cambio de `CHAPTER_LAYOUT` no pudra las coordenadas; `intel` loguea `INTEL nivel=N
pistas=M visto=V celda_jefe=C`. Re-pinneados ese dia: `mapas_check` (reescrito: los diez
Zone check, las cuatro arenas, el rumor), `logros_check` (18/18; la Retama pasa a la celda
18), `secretos_check`, `entrada_jefe_check` (el cap. 6 para el paseo saltado: el `hold up`
necesita un vano que mire al sur), `guardado_check`, `interiores_check` (interior 24),
`mira_check`, `feel_check`, `feel_baseline`, `audio_check`, `carmelo_check`.

Riesgos conocidos, para no redescubrirlos: `took_damage_this_room` y el "+2 Fe sin dano"
pierden exactitud con celdas activas solapadas (se resetea al activar cada celda: aproximacion
aceptada); Ojos Abiertos marca solo enemigos de celdas activas; guardar en mitad de la pelea
del jefe deja al Continuar en el nudo de la celda B y la arena se vuelve a disparar con su
dialogo (`boss_talk_seen` no se guarda), como pasaba antes con la sala.

Gates: `scripts/mapas_check.txt` (costura, perimetros, arena desde fuera / sellada / arco),
`feel_check` (tramo 6: cruzar una costura no carga nada), `interiores_check` (7 -> 12 -> 7),
`carmelo_check` (el grupo nace al pisar la arena).

### Room System & Navigation

Each chapter (scene) contains a **grid of rooms** that the player navigates freely. When the player reaches any edge of the current room, they transition to the adjacent room in that direction (if one exists).

#### Room Grid Layout Per Chapter

Each chapter defines its own grid dimensions and room distribution:

| Cap | Grid | Total Rooms | Layout Notes |
|-----|------|-------------|--------------|
| 1-4 | 6x4 | 24 | Jefe en un borde distinto por capitulo (ver "Mapa grande") |
| 5 | 6x5 | 30 | Cierra el Acto I |
| 6-9 | 6x4 | 24 | |
| 10 | 6x5 | 30 | Cierra el Acto II |

(Hasta el 2026-09-24: 4x3 y 4x4, con el jefe siempre arriba en el centro.)

Y **cada sala mide 2x2 pantallas** (40x22 tiles, 640x352 unidades), asi que un capitulo son
~48 pantallas contra las 9 de antes. La excepcion es la **arena del jefe: una pantalla**.

#### Room Transition Mechanic

> **HISTORIA (hasta 2026-09-10).** Ya no hay transicion por bordes: el capitulo es una zona
> continua (ver arriba). Se conserva porque explica de donde vienen los vanos por lado y las
> sendas hasta el borde, que siguen generandose por celda.

- Player walks to the **top edge** (y ≤ 0) → load room at `(grid_x, grid_y - 1)`, player spawns at bottom
- Player walks to the **bottom edge** (y ≥ GAME_HEIGHT) → load room at `(grid_x, grid_y + 1)`, player spawns at top
- Player walks to the **left edge** (x ≤ 0) → load room at `(grid_x - 1, grid_y)`, player spawns at right
- Player walks to the **right edge** (x ≥ GAME_WIDTH) → load room at `(grid_x + 1, grid_y)`, player spawns at left
- If no adjacent room exists in that direction, the player is blocked by a full wall row/column on that border (no transition)

#### Border Openings Design

El borde lo decide el BIOMA (`BiomeStyle::border`, ver la seccion del mundo exterior): acantilado, terreno o silleria. La abertura de cada lado conectado la dan `Room::gap_x0()..gap_x1()` / `gap_y0()..gap_y1()`, que en bioma `Natural` se abren a casi todo el lado y en `Cliff`/`Masonry` dejan un vano estrecho centrado. A dirt trail (`Path`) is carved from a hub near the centre to every opening (river → `Bridge`, obstacles cleared), so exits are always reachable and visually obvious. Combat rooms seal the openings (`exits_locked`, red pulsing bands, border tiles non-walkable) until every enemy is dead.

- Interior `Wall` tiles are "buildings" (villages/cities), drawn as tall stone blocks in the sorted prop pass
- Door tile types (DoorNorth/South/East/West) are **not generated** but the enum values remain for potential future use

#### Room Types in the Grid

Not all rooms in the grid are identical. `FloorMap::generate()` distributes room types across the grid:

- **Start**: Player spawn point, no enemies, safe exploration
- **Normal**: Standard combat rooms (enemies must be cleared to unlock doors)
- **Elite**: Harder enemies, better rewards (marked differently on minimap)
- **Treasure**: Free item, no enemies
- **Altar**: Pray for faith bonus
- **Boss**: Chapter boss, always at the logical "end" of the grid (furthest from Start)

#### Per-Room Content

Each `Room` generates independently on first visit:
- **Tile map**: `tiles_w` x `tiles_h` (40x22 en exterior) con decoracion del bioma, y los contadores de props/enemigos escalados por AREA (ver la seccion del mundo exterior)
- **Openings**: cliff walls on every border except a centred gap on connected sides; trails lead from the centre to each gap. No door tiles are placed.
- **Enemies**: Spawned on walkable tiles, away from player entry point, type based on chapter
- **Tile collision**: Wall, Tree, Rock, River = solid (block movement). Bush, TallGrass = decorative (walkable)
- **State persistence**: Once generated, rooms retain their `cleared`/`visited` state for the run. Cleared rooms don't respawn enemies on re-entry.

#### Key Structs

- `FloorMap`: Holds the grid of `RoomNode`s for the chapter. Each `RoomNode` has `grid_x`, `grid_y`, `type`, and 4-directional `connections[]` (indices into the rooms array, -1 = no connection)
- `Room`: The active room's tile data, enemy spawns, door flags, generation logic. Only one `Room` is fully loaded at a time (`Engine::current_room_data`)
- `ECSWorld::active_room`: Pointer to the current `Room` for tile collision in `system_movement()`

#### Tile Types

| Tile | Solid | Color Placeholder | Description |
|------|-------|-------------------|-------------|
| Floor | No | Background clear | Empty walkable space |
| Wall | Yes | Brown `{100,80,60}` | Grid-exterior border only (connected sides are open Floor) |
| Tree | Yes | Dark green `{34,100,34}` | Obstacle with border |
| Rock | Yes | Gray `{120,110,100}` | Smaller obstacle (12x12) |
| River | Yes | Dark blue `{40,100,160}` | Impassable water (canyon) |
| Bush | No | Green `{80,160,60}` | Decorative (10x8 centered) |
| TallGrass | No | Semi-transparent green `{90,150,70,150}` | Decorative overlay |
| Pit | Yes | Near-black | Hole |
| Water | No | Blue (slow) | Walkable, slows movement |
| AltarBaal | No | Purple | Destructible, gives faith |
| AltarGod | No | Gold | Pray for faith |
| Chest | No | Gold | Loot container |
| Door* | No | Wooden `{160,120,60}` | DoorNorth/South/East/West (enum exists but not currently generated — open borders used instead) |

### Mundo exterior: salas de 2x2 pantallas, camara y bordes por bioma (2026-08-26)

Primera fase del `GDD2.0.md`, y la que arregla las dos quejas de fondo: los mapas eran
diminutos y habia muros donde no tocan. Referencia: *A Link to the Past* (ver la seccion de
arriba).

- **`Room::tiles_w` / `tiles_h`.** La sala deja de tener un tamano fijo. `Room::tiles` se
  dimensiona al MAXIMO (`Config::ROOM_TILES_X/Y` = 40x22) y cada sala dice cuanto ocupa de
  verdad. **Ningun bucle sobre el mapa puede volver a usar las constantes**: en `rooms.cpp`
  las viejas `static constexpr W/H` se llaman ahora `MAXW`/`MAXH` y solo dimensionan arrays.
- **La arena del jefe mide UNA pantalla** (`Config::SCREEN_TILES_X/Y` = 20x11). No es un
  detalle: en una sala de 2x2 con la camara siguiendo al profeta el jefe se sale del cuadro
  y la pelea se convierte en perseguir una barra de vida. Es tambien el molde de los
  interiores de la fase 3.
- **La camara se enciende.** `Camera` existia desde el primer commit y **no la llamaba
  nadie** — con la sala midiendo exactamente una pantalla no habia nada que seguir.
  `Engine::camera` sigue al profeta y recorta contra `Room::world_w()/world_h()`, asi que en
  una sala de una pantalla **se queda clavada sola** y el comportamiento de siempre sale
  gratis. Al entrar en una sala la camara SALTA (`camera.position = spawn_pos`): sin eso
  hacia un barrido desde la sala anterior por encima del fundido.
- **`Renderer::begin_world()` / `end_world()`.** Acotan la pasada de mundo, y **hacen falta
  de verdad**: `world_to_screen` se aplica a TODO lo que dibuja el Renderer —`draw_rect`,
  `draw_text` y por tanto el HUD entero—, asi que sin cerrarla el HUD se iria con el
  profeta. Dentro: suelo, props, entidades, barreras de salida y los numeros de dano (van
  sobre el enemigo, son mundo). Fuera: vinieta, fogonazo, fundido de sala y HUD. La
  SACUDIDA se queda global a proposito, como hasta ahora, para no mover el tacto ya medido.
- **`BiomeStyle::border`** con tres valores. `Cliff` (canon, montana, cueva) y `Masonry`
  (ciudad, casa noble, campamento) cierran con `TileType::Wall`; **`Natural`** (desierto,
  campos, ribera) cierra con el propio terreno: roca y arboles del bioma en el anillo, mas
  una **franja irregular** hacia dentro (a veces otro bloqueo, a veces matorral pisable).
  Reparto (2026-09-10, "quita los muros en el capitulo uno y en todo escenario al aire
  libre"): 1 Natural - 2 Natural - 3 Natural - 4 Natural - 5 Natural - 6 Masonry -
  7 Natural - 8 Masonry - 9 Natural - 10 Masonry. `Cliff` ya no lo usa ningun exterior.
  - **TRAMPA**: `decorate` colocaba los huecos (Carro de Fuego) y las grietas SOLO con borde
    `Cliff`; cambiar el borde a secas los borraba de los caps. 1, 3 y 4. Por eso existe
    `BiomeStyle::rocky` (true en 1/3/4): el terreno rocoso es una propiedad aparte del borde.
  - **Dejar el borde ABIERTO no es opcion**: fuera de la rejilla `is_tile_walkable` deja
    pasar, y sin sala vecina `try_room_transition` devuelve al profeta de un tiron — un muro
    invisible, que se siente peor que el muro.
  - En `Natural` el lado conectado se abre **casi entero** (`gap_x0()..gap_x1()` = 2..w-3);
    un vano de seis tiles en mitad de un campo lo convierte en un corral con puerta. Las
    antorchas de `biome.torches` se apagan ahi por lo mismo: "flanquear" un vano que ocupa
    todo el lado las deja en las esquinas.
- **Las sendas SERPENTEAN** (`carve_trail`). En una sala de una pantalla la L de antes
  pasaba desapercibida; en una de 2x2 dejaba una carretera perfectamente recta cortando el
  campo de lado a lado. **El serpenteo va en el tramo LARGO**: puesto en el de aproximacion
  no hace nada, porque con el nudo ya alineado con el vano ese tramo tiene CERO pasos y la
  senda salia recta igual (asi fallo el primer intento). Los ultimos cuatro pasos van
  derechos para entrar limpio por el vano, y el nudo se descentra en todas las salas menos
  la del jefe — centrado, las cuatro sendas forman una cruz perfecta.
- **Todo lo que se contaba "por sala" se escala por AREA**: props, charcos, edificios y
  enemigos (`screens` en `decorate` y en `generate`). Con los contadores pensados para una
  pantalla, una sala de 2x2 salia desnuda. La densidad de enemigos va a **3/4 del area**, no
  al area entera: a densidad completa los dieciseis convergen a la vez y la sala deja de ser
  un campo para ser una horda; a la mitad sobraba campo y faltaba gente (medido).
  El **rio** tambien se coloca en proporcion (`tiles_w * 3 / 5`...): con los limites
  clavados para 20 tiles se quedaba siempre en el cuarto izquierdo.
- **Los props de sala especial ya no van en el centro** (`place_special`). El profeta
  aparece justo ahi al entrar, asi que el encuentro saltaba antes de haber visto a nadie y
  el cofre se abria sin haberlo buscado. Se colocan a unos tiles del nudo, sobre suelo
  pisable y con al menos dos vecinos libres (que no caigan en un bolsillo cerrado).
- **Rejillas 4x3 / 4x4** y `FloorMap::MAX_ROOMS` 25. El reparto de tipos dejo de sortearse
  el 2026-08-27: lo fija la tabla `CHAPTER_LAYOUT` de rooms.cpp (ver la seccion de mapas
  fijos). En 4x3: pozo de relatos completo (3-4 Event) + 1 Tesoro + 1 Altar + 2 Elite; en
  4x4: relatos completos + 2 Tesoro + 2 Altar + 2 Elite.

**El PNJ del encuentro ya no es una antorcha.** Literalmente lo era: `collect_props` dibujaba
`TileType::Event` con la celda `TileSprite::Torch`. Ahora hay una hoja propia
`assets/sprites/npcs.png` (16 celdas de 128 px, **una sola fila, solo `Idle`**, generada por
`tools/blender/mantle_sprites/npcs.py` con las recetas que ya existian para los retratos).

- Se dibuja **como prop ordenado por Y**, no como entidad del ECS. Es lo mismo en pantalla
  —sombra blanda, orden por profundidad, 32 unidades— y no gasta ni un componente ni una
  entidad de las 512: el locutor no pelea, no anda y no muere.
- Los pies de la hoja caen en la fila 112 de 128 igual que en las hojas de personaje, asi
  que el alto se cuelga de `feet` hacia arriba (`feet - 28`) y el PNJ pisa el suelo a la
  misma altura que el profeta.
- **`EventDef::npc`** (indice de `namespace Npc` en `render/sprites.h`) va el ultimo del
  struct y con valor por defecto, igual que `portrait`. `Npc::NONE` = este encuentro no es
  una persona sino la ESCENA (los cuervos, el altar del Carmelo, la boca de la cueva): ahi
  se queda la hoguera, que es lo honesto.
- Marca flotante dorada sobre la cabeza mientras el encuentro esta sin resolver: es lo que
  se ve de lejos, y medio motivo para desviarse.

**Comando de autoplay `goto <tipo>`** (`event`, `treasure`, `altar`, `elite`, `boss`,
`start`), ademas del indice de siempre. No es comodidad: el reparto de salas se mueve en
cuanto cambia una linea de `FloorMap::generate`, y un `goto 11` clavado deja de apuntar a la
sala que el guion queria comprobar **sin decir ni pio**. Guion: `scripts/mapas_check.txt`.

**La sonda de tacto depende del MAPA, no solo del motor.** `feel_baseline` corre en la sala
de inicio, que mide cuatro pantallas, asi que el bot se pasa el rato andando y el TTK lo
manda la distancia. Medido el 2026-08-26 con la MISMA build y solo cambiando la semilla:
soldados 2,68 - 3,34 s, leones **0,00 - 2,39 s**, arqueros **0,86 - 24,77 s**. O sea que la
cifra absoluta no significa nada por si sola: lo unico comparable son dos pasadas sobre
**el mismo mapa** y **la misma generacion de sala**. Desde los mapas fijos (2026-08-27) el
guion ya NO fija semilla: mide el mapa REAL del juego (`Config::CHAPTER_SEEDS[0]`), que es
constante — pero cualquier cambio en `Room::generate` o en esa semilla sigue moviendo la
medida sin que el motor de combate se haya tocado.

Cifras de referencia desde la ZONA CONTINUA (2026-09-10, mapa real; la celda de inicio del
cap. 1 cambio de decorado al abrir el borde a Natural, y la sonda corre en coordenadas de
zona): soldados **0,93 s**, leones **2,05 s**, arqueros **0,96 s**, 0 golpes recibidos.
Las anteriores (mapa real, tras los mapas fijos del 2026-08-27): soldados 0,80 s, leones
2,04 s, arqueros 0,76 s — ya no comparables.
Reproducibles bit a bit (dos pasadas identicas linea a linea). Las anteriores (0,93 /
3,10 / 1,23 con seed 20260825, y todas las previas) se midieron sobre OTRO mapa, asi que
no son comparables. Ojo con el tramo del jefe: `killall` deja la sala por limpia, asi
que el bot **se sale de la arena** por la abertura y el tramo se corta solo.

### El suelo se HORNEA por trozos (2026-10-03)

Peticion del usuario: «mejora todo, graficos e imagenes, a un nivel ultra profesional». Lo
que mas se ve del juego era una rejilla: cuatro variantes de baldosa sorteadas por tile (en el
Carmelo, Jerico, Sunem y Samaria un damero de losas grises), la senda hecha de manchas
repetidas y las charcas en escalera. El defecto era la CELDA, y no se arreglaba pintando
mejor la celda: `game/ground.h/.cpp` genera el suelo por codigo en texturas de TROZO.

- **Trozo = 16x16 tiles (256 unidades) a 2 px por unidad = 512x512 px.** Doce ranuras de 1 MB
  que se reciclan (LRU). Los trozos visibles se hacen en el acto (al entrar en una zona son
  1-4, ~20-50 ms cada uno en el contenedor de pruebas); los de alrededor se rellenan **por
  filas con un presupuesto de 3 ms por frame** (`prefetch_step`), asi que cruzar de trozo no
  da tirones. Cada trozo lleva la FIRMA de sus tiles + 3 de margen: si una charca se seca
  (Sanar Aguas) o se abre una grieta, ese trozo se rehace solo.
- **Campos suaves, no celdas**: por tile se ponen mascaras (senda, agua, agua honda, hueco,
  solido, hierba, muro), se emborronan a nivel de tile y se muestrean con bilineal + ruido. El
  umbral 0,5 cae en la linea entre tiles, asi que la orilla se curva SIN dejar de seguir la
  colision. Encima: agua de clara a honda con arena mojada y espuma, senda con borde levantado
  y rodadas, hueco con labio, sombra de contacto al pie de lo solido, vados con piedras planas.
- **Un estilo y una paleta por capitulo** (`look_for`): tierra (Querit, la aldea pobre), arena
  con ondas (Sarepta), roca con grietas a dos escalas (Carmelo, Horeb), pradera (El Llamado,
  el Jordan) y losas irregulares tipo Voronoi (Jerico, Sunem, Samaria e interiores). En las
  ciudades las losas van donde hay muros cerca y en manchas, con tierra batida entre medias:
  una zona enlosada entera era un tablero.
- **Lo que sigue saliendo del tileset**, encima del suelo horneado: muros y acantilados del
  borde, puente, hierba alta, decorados y todos los props. Sin tileset no se hornea nada y
  `render_ground` pinta las celdas de siempre (el camino viejo sigue entero detras de `baked`).
- El agua es una textura ESTATICA; lo que se mueve son destellos por tile de agua visible
  (`shadow_tex` estirada, fase por tile).
- Log: `Suelo: trozo X,Y horneado en N ms` (los cuatro primeros y los que pasan de 40 ms).
- **Trampa de guion que salio aqui**: un frame lento corre varios pasos de update seguidos, y
  un `shot` sin `wait` detras deja que el comando siguiente (`chapter N`) se ejecute ANTES del
  render: la captura sale con el capitulo siguiente. Es la regla de siempre (`wait 0.1` tras
  cada `shot`), solo que con el horneado se nota mas.

Guiones: `mapas_check`, `secretos_check` e `interiores_check` pasan sus gates.

**Tileset al doble y en lineal (mismo dia).** Junto al suelo horneado, los props del tileset
(arboles, palmeras, matas, rocas, hierba) se veian pixelados al lado de personajes suaves: el
atlas iba a 4 texels por unidad y en `nearest`. Ahora va a 8 (`TILESET_TEXELS_PER_UNIT`,
celda de 128 px) con un margen extruido de 2 px por celda (`TILESET_GUTTER`, paso 132) que
deja filtrarlo en lineal sin sangrado. `get_tile_src` ya devuelve el rectangulo con el margen
descontado; quien calcule a mano un `src` del tileset tiene que pasar por ahi.

### Ambiente: nubes, luz y motas (2026-10-03)

`game/ambience.h/.cpp`. Tres capas que no tocan el gameplay y que quitan la sensacion de
"foto quieta":

- **Sombras de nubes** (solo exterior), dentro de la pasada de mundo justo despues de
  `draw_sorted`: oscurecen el suelo Y lo que hay encima. Textura de ruido PERIODICO de 256 px
  que se repite cada 640 unidades y se desplaza con el viento del capitulo. **Las juntas van
  ajustadas a la rejilla de pixeles del objetivo**: con el borde en fraccion de pixel las dos
  baldosas pintan la misma fila y sale una raya oscura de lado a lado (paso, se vio y se mato).
- **Luz** en pantalla, bajo la vineta: calida (ADD) desde arriba a la izquierda —de donde
  vienen las sombras de los sprites— y penumbra fresca en la esquina contraria. Un color y una
  fuerza por capitulo (`get_amb`): atardecer naranja en el Carmelo, luna fria en Horeb, luz
  apagada en Jerico y Samaria. Los interiores, mas oscuros en las esquinas.
- **Motas** ancladas al mundo con envoltura alrededor de la camara (no se van con el profeta,
  nunca se acaban, sin estado): polvo (Querit, Sarepta, Horeb, la aldea pobre), vilanos que
  destellan (El Llamado, Sunem, el Jordan), ceniza que cae (Jerico, Samaria), pavesas que suben
  (Carmelo) y polvo quieto en los interiores.

Todo son texturas procedurales de `ambience_init` (log `Ambiente: texturas listas`); sin ellas
no se pinta nada.

### Deuda visual del exterior (2026-08-26)

Cinco defectos que **solo se ven desde que la sala mide cuatro pantallas**, y ninguno era un
fallo de logica: todos eran de legibilidad. La regla comun a los cinco es la misma — **a 4x,
cualquier cosa dibujada a tile completo y centrada dibuja la REJILLA**.

- **Los decorados del suelo se encogen y se descentran** dentro de su celda (0,50-0,90 del
  tile, desplazamiento y giro de 90 grados por `tile_hash`). A tile completo salia un damero
  de manchas identicas. **Nunca se salen de su propia celda**: el suelo del tile de la derecha
  y el de abajo se pintan despues y los recortarian.
- **Las charcas son elipses con el borde mordido**, no cajas. Una caja de 2x2 con su ribera
  alrededor se lee como una **piscina**, con bordillo y todo; una elipse ragged es justo lo que
  el autotileado de ribera necesita para dibujar una curva. El mordisco va por `tile_hash` y no
  por `randf()`, para que la misma semilla de la misma forma.
  **La ventana buena del radio es ESTRECHA y se cerro midiendo, no a ojo**: por debajo de 1,3
  la elipse cabe en una cruz de cinco tiles y la charca sale con forma de pieza de tetris; por
  encima de 2,5 las charcas se juntan en un LAGO, y el agua no es decorado — es terreno lento
  (`Config::WATER_SPEED_MULT`). Con cuatro charcas de radio 2,5 la sonda de tacto midio **94
  tiros y CERO bajas** en el tramo de arqueros, con el bot vadeando media sala. Valores buenos:
  `max(2, screens-1)` charcas, rx 1,3-1,9 y ry 1,1-1,6.
- **Los edificios van en MANZANAS**, con calle de 1-2 tiles entre solares y algun solar vacio.
  Un rectangulo de ladrillo solo en mitad del campo se lee como un trozo de muro huerfano; lo
  que se lee como poblado es un grupo de casas con una calle.
- **El acantilado deja de ser una tapia.** `TileSprite::CliffRock` / `CliffRockTall` (celdas
  nuevas, filas 7-8 del tileset) tienen las **juntas verticales y desalineadas**: cada columna
  corta sus bloques por su cuenta, con anchos desiguales, giros de +-11 grados y taper. `Cliff` /
  `CliffTall` siguen siendo silleria y ahi se quedan — en ciudad y palacio el ladrillo es lo
  correcto, y los edificios del interior los siguen usando. Lo elige `biome.border` en
  `render_ground`. Tres intentos que no conviene repetir: con bloques grandes salen losas lisas;
  sin junta (bloques solapados) sale una pared lisa con manchas; y las lascas en gris claro
  (`stone`) se leen como escombro pegado con cola.
- **La senda se DERRAMA sobre la celda vecina** (`spill` en `render_ground`): el sprite del
  camino sobre el tile de al lado, en una franja de fondo variable por lado y alfa parcial. El
  tile de camino es opaco hasta su borde, asi que un trazado por celdas enteras es una cadena de
  cuadrados marrones sobre la hierba. **Se hace al DIBUJAR, no al generar**: probado con un
  fleco de tiles de verdad, y lo que hace es ENSANCHAR la senda al doble en vez de romperle el
  contorno — ademas de cambiar por donde se puede andar.

**Subir `TILE_ROWS` obliga a re-renderizar el tileset ENTERO** (`tiles.main` sin `only`):
`_load_sheet` devuelve `None` si la forma no cuadra, y entonces un parcheo con `only` deja en
blanco las otras 50 celdas. El re-render completo cuesta ~5 s y no es identico bit a bit (el
antialiasing de Workbench varia entre sesiones), pero si lo es a ojo: se comprobo celda a celda.

Gate: las capturas de `scripts/mapas_check.txt`, antes y despues.

## Como se dibuja el mundo

- Rendering: `Room::render_ground` (floor/water/walls/shores) then a y-sorted `DrawCmd` list (`Room::collect_props` + `ECSWorld::collect_draws` → `Renderer::draw_sorted`), then vignette, then the room fade, then HUD.
- **Shores**: water tiles autotile their banks — 3 cells authored for "land to the north" (`TileSprite::ShoreEdge/Out/In`, tileset row 7) drawn rotated 90/180/270 from an 8-neighbour mask in `render_ground`, so the river reads as a continuous curve. River generation drifts one column every other row with a 2-3 wide bed.
