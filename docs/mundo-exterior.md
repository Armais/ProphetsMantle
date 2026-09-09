# El mundo exterior

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`rooms.h/cpp`): leelo antes
de tocar `Room::generate`, `decorate`, `carve_trail` o `render_ground`.

### Room System & Navigation

Each chapter (scene) contains a **grid of rooms** that the player navigates freely. When the player reaches any edge of the current room, they transition to the adjacent room in that direction (if one exists).

#### Room Grid Layout Per Chapter

Each chapter defines its own grid dimensions and room distribution:

| Cap | Grid | Total Rooms | Layout Notes |
|-----|------|-------------|--------------|
| 1-4 | 4x3 | 12 | Start=bottom-center, Boss=top-center |
| 5 | 4x4 | 16 | Cierra el Acto I |
| 6-9 | 4x3 | 12 | |
| 10 | 4x4 | 16 | Cierra el Acto II |

Y **cada sala mide 2x2 pantallas** (40x22 tiles, 640x352 unidades), asi que un capitulo son
~48 pantallas contra las 9 de antes. La excepcion es la **arena del jefe: una pantalla**.

#### Room Transition Mechanic

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
  Reparto: 1 Cliff - 2 Natural - 3 Cliff - 4 Cliff - 5 Natural - 6 Masonry - 7 Natural -
  8 Masonry - 9 Natural - 10 Masonry.
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

Cifras buenas de referencia hoy (mapa real, tras los mapas fijos del 2026-08-27):
soldados **0,80 s**, leones **2,04 s**, arqueros **0,76 s**, 0 golpes recibidos.
Reproducibles bit a bit (dos pasadas identicas linea a linea). Las anteriores (0,93 /
3,10 / 1,23 con seed 20260825, y todas las previas) se midieron sobre OTRO mapa, asi que
no son comparables. Ojo con el tramo del jefe: `killall` deja la sala por limpia, asi
que el bot **se sale de la arena** por la abertura y el tramo se corta solo.

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
