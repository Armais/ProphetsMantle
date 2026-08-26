# MANTLE - Project Instructions for Claude

## Overview

MANTLE is a 2D top-down roguelike action game (smooth "3D figurine" look, Clash-Royale-like, NOT pixel art anymore) inspired by the biblical prophets Elijah (Elias) and Elisha (Eliseo). Built in C++17 with SDL3, it targets Desktop (Windows/Linux/Mac), Web (Emscripten), Android, and iOS. The game follows a two-act structure where Act I stars Elias and Act II stars Eliseo, connected by an "Ascension" transition where items and faith transfer between characters.

## Build System

- **Build tool**: CMake 3.24+ with Visual Studio 2022 generator on Windows
- **C++ standard**: C++17
- **Dependencies**: SDL3, SDL3_image, SDL3_mixer, SDL3_ttf (local sources in `build/_deps/`)
- **Static linking**: All SDL libraries link statically (`SDL3::SDL3-static`, etc.)
- **SDL3_mixer version**: 3.1.2 — uses the new `MIX_*` API (uppercase prefix), NOT the old `Mix_*` SDL2 API
- **SDL3_image version**: 3.4.0 — PNG and BMP formats only
- **SDL3_ttf version**: 3.2.2 (`build/_deps/sdl3_ttf-src`, vendored FreeType cloned into `external/freetype`, branch `VER-2-13-2-SDL`; HarfBuzz/PlutoSVG off). Only used at init to bake glyph atlases (`Renderer::bake_atlas`); `TTF_*` calls live in `render/renderer.cpp` only.

### How to build

```bash
# Configure (only needed once or after CMakeLists.txt changes)
cmake -B build -S .

# Build
cmake --build build --config Release
```

The executable is output to the project root (`MANTLE.exe`, see `RUNTIME_OUTPUT_DIRECTORY` in CMakeLists.txt) so that the relative `assets/` path resolves; `build/Release/MANTLE.exe` is a stale copy.

### Icono y logo (2026-08-26, rehecho el mismo dia)

**El primer icono lo rechazo el usuario** ("no se ven a Elias ni Eliseo ni ningun manto") y la
leccion vale mas que el arte: **un icono se juzga a 32 px, no en el render a 1024**. Aquel
tenia DOS motivos distintos —los profetas de cuerpo entero de 64 px para arriba, un pano con
llama de 48 para abajo— y a 32 px no se reconocia ninguno de los dos: una figura de pie mide
cuatro pixeles de ancho, y una tela colgada en el aire con fuego encima es un brasero.

Ahora es **UNA composicion y dos encuadres de ella**: Elias y Eliseo hombro con hombro, de los
hombros para arriba, con el manto cruzando en diagonal entre los dos (orillo de oro en el
bajo). `logo` = busto ancho (256/128/64); `mark` = el mismo busto recortado a las dos cabezas
(48/32/24/16). Lo que sobrevive al reducir es una CABEZA, y lo que distingue a los dos es
pelo y barba oscuros contra calva y naranja contra azul — no el detalle de la cara.

- **El gate es `python tools/branding/contact.py <png>...`**: una tira con el icono a
  16/24/32/48/64/128/256 px sobre fondo claro **y** oscuro, con la columna de 32 marcada. Se
  mira ANTES de dar nada por bueno, y por eso vive aparte de `make_icon.py` (que empaqueta lo
  ya decidido). Fue lo que descarto los otros tres bocetos, y **conviene no repetirlos**: el
  traspaso (Elias echando el manto sobre Eliseo) deja el pano colgando por delante y se lee
  como un escudo; el manto solo, con capucha y llama, sale campana o brasero; y una M
  tipografica aguanta 16 px pero renuncia a los dos profetas.
- Arte: `tools/blender/mantle_sprites/logo.py` (mismo pipeline que los personajes: Workbench,
  cavity, `post_soft`; el icono tiene que parecer el mismo muneco que se juega). El encuadre
  sale **cuadrado por el lado mayor**, asi que estrechar la caja del `mark` es justo lo que
  agranda las cabezas. Ojo con el cayado: son CUATRO objetos (`_staff` y, colgando de el,
  `_tip`/`_tip_core`/`_tip_halo`), y borrar solo el padre deja la punta encendida flotando
  sobre la cabeza del profeta.
- **Las fuentes NO van en `assets/`**: `logo.py` escribe `logo.png` y `mark_512.png` en
  `tools/branding/src/` porque no las carga el juego. En `assets/branding/` queda solo lo que
  se distribuye: `mantle.ico` (lo empotra `mantle.rc`) y `mark_256.png` (`SDL_SetWindowIcon`).
- Empaquetado: `python tools/branding/make_icon.py [preview]` escribe `mantle.ico` **a mano**
  (cabecera + una entrada de 16 bytes por tamano + el PNG de cada uno). `Image.save(sizes=)`
  no vale: mete la MISMA imagen reescalada en todos los tamanos y aqui hace falta un encuadre
  distinto por tamano. Ademas **redibuja el contorno en cada tamano**: el de Blender esta en
  pixeles del render (5 px en 512) y al bajar a 16 se evapora.
- `mantle.rc` en la raiz, anadido a `MANTLE_SOURCES` **solo bajo `if(WIN32)`** — eso da el
  icono del FICHERO. `SDL_SetWindowIcon` en `Engine::init` da el de la ventana y la barra de
  tareas, y cubre Linux y Mac. Para comprobar lo que vera el explorador, extraerlo del `.exe`
  ya compilado: `[System.Drawing.Icon]::ExtractAssociatedIcon($exe)` devuelve justo los 32 px.
- **El manto que cruza es una malla continua** (`logo._sheet`, bmesh), no piezas de `parts`:
  con una caja vertical salio una puerta de armario y con trece cajas en fila un xilofono —
  cada caja trae su bisel y su cavity y se leen como objetos sueltos. Lo que la hace parecer
  tela son los pliegues: ondular la malla en PROFUNDIDAD y dejar que el sombreado dibuje las
  luces. Sus parametros `tilt` (cruza en diagonal) y `flare` (estrecho arriba, ancho abajo)
  son lo que separa una prenda de un trapo tendido. Y **los dos extremos tienen que caber en
  el cuadro**: un pano que se sale por los dos lados pierde el largo y se lee como un tablon.
- Sin llama, a proposito: la que habia se apilaba encima del pano y era justo lo que lo
  convertia en brasero. Si algun dia vuelve el fuego, necesita un perfil curvo (un solido de
  revolucion): apilando esferas sale un huevo y con cajas afiladas, un avion de papel.

### IMPORTANT: Always build after finishing an implementation

**Every implementation ends with a compiled `MANTLE.exe` at the project root, ready to play.** Do not hand back edited sources and let the user discover they are running a stale binary — build, report the result, and only then close the microphase. If the build fails, fix it and build again.

`cmake` is NOT on the PATH in this environment. Use the Visual Studio copy:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
```

A full rebuild from cold takes minutes (SDL3 + mixer + ttf dependencies); incremental builds of `src/` are seconds. `build.bat` at the project root does the same thing.

### Important: SDL3 API differences from SDL2

This project uses **SDL3**, which has breaking API changes from SDL2. Common pitfalls:
- `SDL_GetNumTouchDevices()` does NOT exist. Use `SDL_GetTouchDevices(&count)` + `SDL_free()`.
- SDL3_mixer uses `MIX_Init()`, `MIX_CreateMixerDevice()`, `MIX_LoadAudio()`, `MIX_CreateTrack()`, `MIX_PlayTrack()`, `MIX_SetTrackGain()`, `MIX_DestroyAudio()`, `MIX_DestroyMixer()`, `MIX_Quit()`. There are NO `Mix_LoadWAV`, `Mix_Chunk`, `Mix_PlayChannel`, `Mix_Volume`, `MIX_MAX_VOLUME`, `Mix_AllocateChannels`, `Mix_OpenAudio`.
- SDL3_mixer uses properties for playback options: `SDL_CreateProperties()` + `SDL_SetNumberProperty()` + pass to `MIX_PlayTrack()`.
- Fire-and-forget SFX: use `MIX_PlayAudio(mixer, audio)`.
- Fades use sample frames, convert with `MIX_TrackMSToFrames()`.

## Architecture

### Data-Oriented Design (NOT OOP)

The entire codebase uses plain `struct`s with public members. There is NO inheritance, NO virtual functions, NO polymorphism. Components are stored in Struct-of-Arrays (SoA) for cache efficiency.

### Directory Structure

```
src/
  main.cpp              - Entry point (minimal bootstrap)
  core/
    types.h             - Type aliases (u8,u16,u32,u64,i32,f32,f64), Vec2, Rect, Color, Direction, randf()
    config.h            - All tunable game constants (namespace Config)
    engine.h/cpp        - Main game loop, scene state machine, subsystem composition
    ecs.h/cpp           - Entity Component System (512 max entities, bitset masks, free list)
  render/
    renderer.h/cpp      - SDL3 2D renderer: game units 320x180 drawn to a 1280x720 target (RENDER_SCALE 4 via
                          SDL_SetRenderScale), texture cache (linear; tileset nearest), TTF glyph atlases,
                          procedural shadow/vignette textures, letterbox to window
    sprites.h/cpp       - Animation definitions (AnimID enum, AnimDef, SpriteSheet)
    camera.h/cpp        - Smooth-follow camera with shake and room bounds clamping
  input/
    input.h/cpp         - Unified input: keyboard/mouse/gamepad/touch, action-based abstraction
  audio/
    audio.h/cpp         - SDL3_mixer music + SFX system (MIX_* API)
  game/
    player.h/cpp        - Player entity factory (Elias/Eliseo) and update logic
    faith.h/cpp         - "El Celo del Senor" faith system (6 tiers, damage multipliers, miracle gating)
    miracles.h/cpp      - 10 miracles (5 per prophet), cooldown/duration system
    enemies.h/cpp       - 8 enemy types with AI behaviors, spawning
    items.h/cpp         - 16 items (5 categories), item registry, heredables (el 15 es El Manto de Elias)
    rooms.h/cpp         - Tile-based rooms (exterior 40x22 tiles = 2x2 pantallas; arena de jefe 20x11),
                          bordes por bioma, floor map generation, 10 chapters
    combat.h/cpp        - AABB/circle collision, damage, projectiles, i-frames
  ui/
    hud.h/cpp           - In-game HUD (health, faith, miracle slots)
    menus.h/cpp         - Menu screens (title, pause, game over, ascension)
  systems/
    save.h/cpp          - Binary save/load with version header, mobile expiry (v6)
    progression.h/cpp   - Meta-progression upgrades (8 upgrades, offerings currency)
    achievements.h/cpp  - 18 logros (bitmask en SaveData; quinta pestaña del Códice)
  platform/
    platform.h          - Platform detection, asset/save paths, touch/steam detection
assets/
  audio/music/          - 10 loops sintetizados (.ogg Vorbis, tools/audio/gen_audio.py; el loader cae .ogg -> .wav)
  audio/sfx/            - 37 SFX sintetizados (.ogg, mismo generador; incluye variantes _2/_3 por sonido)
  sprites/cine/         - Prologue panels: 8 escenas x 3 capas (1600x720 PNG con alfa, ver ui/cinematic.h)
  sprites/vfx.png       - Hoja de efectos propia: 8 fotogramas x 7 filas, celda 128 px (ver spawn_fx)
  sprites/portraits.png - 28 celdas, 6x5, celda 96 px (Bestiario del Códice + los locutores del pozo)
  sprites/              - Character sheets: 12 PNG (896x1920, 7 cols x 15 rows of 128x128 = 32 game units
                          x ASSET_SCALE 4, rows = AnimID, smooth alpha + 2px dark outline)
                          + tiles.png (terrain tileset, 64px cells = 16 units x4, 8 cols x 9 rows: rows 0-4 terrain/props/
                          pickups, row 5 floor families grass_a-d + stone_a-d, row 6 ground decals, row 7 shore
                          autotiling pieces + cliff_rock + jar + crack, rows 7-8 cliff_rock_tall + door; see TileSprite in game/rooms.h)
  fonts/                - LilitaOne-Regular.ttf (OFL) baked into glyph atlases at init (fallback: SDL debug text)
  CREDITS.md            - Procedencia y licencia de TODO asset (se actualiza al meterlo, no despues)
tools/
  blender/mantle_sprites/ - Python pipeline (Blender 4.1 + blender-mcp) that builds the 10 rigged
                            characters procedurally, animates them per AnimID and renders the sheets
                            (run.py: `main(only=[...])`, default quantize=False = smooth post_soft;
                            quantize=True = legacy pixel-art). FRAME=128, SS=2, feet baseline = row 112.
                            Stylisation in characters.py: HEAD_SCALE 1.45 (parts on the head bone scaled about
                            HEAD_PIVOT), LION_HEAD_SCALE, palette through parts.vivid() (SAT 1.35, skin 1.15).
                            Faces: `characters._face()` (sclera + iris + brow + nose + mouth, EYE_*/BROW_TILT
                            constants, `eyes`/`brow`/`brow_color`/`mouth` keys per SPEC) hangs off the `head`
                            bone, so hair/hood/helmet MUST leave z>=1.60 of the head clear or the rostro
                            disappears again. Smoothing: `parts.box(round=..., taper=...)` bevels every edge
                            (opt-in, so tiles.py stays crisp) and `parts.tube(caps=, rim_round=)` gives open
                            rings (helmet band, hood cowl) and soft cap edges.
                            tiles.py renders the terrain tileset (`tiles.main(SPRITES_DIR, TMP_DIR)`, CELL=64,
                            TILE_ROWS=9; `only=[...]` patches the existing tiles.png instead of blanking it);
                            its LAYOUT must match `TileSprite` in src/game/rooms.h. GROUND = opaque
                            top-down FLAT cells, DETAILS = transparent top-down decals, SMALL/PROPS/PROPS_TALL = 30 deg.
                            cine.py renders the 8 prologue panels (`cine.main(SPRITES_DIR, TMP_DIR, only=[n])`,
                            `cine.flatten(n, bg=<cielo de la escena>)` para verlas compuestas). Es el UNICO camino
                            con camara ANCHA y en PERSPECTIVA: las hojas y el tileset usan orto porque un sprite
                            tiene que casar con la rejilla, pero en orto no hay horizonte y los montes lejanos
                            flotan sobre el suelo. Escala: 1 unidad de Blender = 1 METRO (humano 1.75).
                            **`setup_cine` va PRIMERO en cada escena** y el decorado se cuelga de la camara:
                            `frame_at(y, safe)` da la banda `(x0,x1,zb,zt)` visible en ese plano, `near_y(z, bite)`
                            la distancia a la que el borde inferior corta el suelo (lo que se apoye MAS LEJOS se ve
                            entero y flota en mitad del panel en vez de entrar por el borde), `lift(zb,zt,k)` la
                            altura para asomar `k` dentro del cuadro, y `spread(n,x0,x1)` reparte. `SAFE_NEAR` 0.78
                            = el borde de PANTALLA (el motor solo ensena el 80% central del panel) menos el paneo;
                            recorta SOLO en X, porque en vertical el panel se ve entero. Colocar el primer plano a
                            ojo es lo que dejo cinco de las ocho capas NEAR en blanco (2026-08-21).
                            `render_panel` loguea la cobertura de cada capa: <3% = perdida fuera del cuadro,
                            MID >90% = tapa el cielo y la capa lejana entera. Mirar ese numero SIEMPRE.
                            Mas reglas: el suelo (`ground(far=)`) tiene que ACABAR antes de lo que haya en la capa
                            lejana o lo tapa; para que se vea cielo hace falta `elev < atan(18/lens * 0.45)` (con
                            lens 45 son 11,4 grados: por encima, el cuadro entero cae bajo el horizonte y la capa
                            media sale 100% opaca); `flat_layer(FAR)` (luz FLAT, sin cavity ni specular) para
                            cordilleras y humo, o el cavity subraya cada esfera solapada ("collar de perlas") — NO
                            para silleria, ahi el cavity es justo lo que dibuja las juntas; Workbench con luz de
                            estudio ACLARA cualquier plano oscuro hasta gris medio, asi que un vano negro se hace
                            con geometria hundida (jambas + sofito + fondo), no bajando el color; los props de
                            tiles.py van a escala de tile (una antorcha mide 1,85 m: `scale=2.4` la deja en 4,4 m,
                            saliendose por el techo); las poses se aplican en el espacio del armature y X negativo
                            lleva el brazo AL FRENTE (-90 = horizontal, -180 = vertical), pero girar el brazo del
                            cayado (`hand.R`) mas alla de la horizontal VUELCA el baculo y deja la llama a la
                            altura de la cintura, y de frente a camara un brazo alzado pasa por delante de la cara;
                            y `render.post_soft` SOLO vale para marcos cuadrados (`reshape(px, ss, px, ss)`) — los
                            paneles usan `render.post_soft_rect`.
  blender/blend/          - Source .blend per character (not runtime assets)
  blender/mantle_sprites/vfx.py       - hoja de efectos (EEVEE + emision, ver seccion propia)
  blender/mantle_sprites/portraits.py - las 28 celdas (Workbench, encuadre por bounding box;
                                        19 retratos de personaje + 3 viñetas de escena en `SCENES`)
  audio/gen_audio.py      - sintetizador numpy de TODO el audio -> .ogg via ffmpeg
                            (`python tools/audio/gen_audio.py`; `all wav` deja los .wav).
                            Cuerdas Karplus-Strong, flauta ney, dos buses estereo,
                            humanizacion y compresor de bus. Ver seccion "Audio".
```

### Key Subsystems

| Subsystem | Header | State struct | Notes |
|-----------|--------|-------------|-------|
| Engine | `core/engine.h` | `Engine` | Owns all subsystems, runs main loop |
| ECS | `core/ecs.h` | `ECSWorld` | 13 component types, 512 max entities |
| Renderer | `render/renderer.h` | `Renderer` | 320x180 render target, integer scaling |
| Input | `input/input.h` | `Input` | Action enum, device-agnostic |
| Audio | `audio/audio.h` | `Audio` | MIX_Mixer*, MIX_Track*, MIX_Audio* |
| Faith | `game/faith.h` | `FaithSystem` | Core mechanic, 6 tiers |
| Combat | `game/combat.h` | (free functions) | O(n^2) collision, projectiles |
| Rooms | `game/rooms.h` | `Room`, `FloorMap` | tiles de 16 unidades; `Room::tiles_w/tiles_h` (40x22 exterior, 20x11 arena) |
| Save | `systems/save.h` | `SaveSystem` | Binary format, version 1 |
| Progression | `systems/progression.h` | `ProgressionSystem` | 8 meta-upgrades |
| Achievements | `systems/achievements.h` | `AchievementSystem` | 18 logros, bitmask en el guardado |

### ECS Components

Entity is a `u32` index. Components tracked via `std::bitset`. Fixed arrays of size `MAX_ENTITIES = 512`:

`TransformComponent`, `SpriteComponent`, `AnimationComponent`, `HealthComponent`, `CombatComponent`, `MovementComponent`, `ColliderComponent`, `FaithComponent` (unused), `MiracleUserComponent` (unused), `EnemyAIComponent`, `ProjectileComponent`, `ItemComponent`, `LifetimeComponent`

### Scene State Machine

`GameScene` enum: `Intro → MainMenu → Playing → Paused → Choice → GameOver / Victory`

`Intro` es el prólogo cinemático (ver abajo). Al añadir un valor a `GameScene` hay que
ampliar **tres** switches a la vez: `Engine::update`, `Engine::render` y
`Engine::update_music` — este último corre antes del switch de escena y pisa cualquier
`play_music` manual, así que una escena sin `case` ahí se queda muda.

`Choice` is the "elige 1 de 3" screen (items or miracles) drawn over the frozen world.

With special `Transition` state for room changes and `Ascension` for Act I→II switch.

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

## Code Conventions

### Naming
- **Types/structs/enums**: `PascalCase` (`FaithSystem`, `MusicID`, `RoomType`)
- **Functions/methods**: `snake_case` (`play_music`, `get_damage_multiplier`)
- **Variables/members**: `snake_case` (`music_volume`, `current_music`)
- **Constants**: `UPPER_SNAKE_CASE` in `Config::` namespace (`GAME_WIDTH`, `FAITH_MAX`)
- **Enum values**: `PascalCase` (`MusicID::MainTheme`, `SfxID::CayadoSwing1`)
- **In-game names**: Spanish (`CuervosDeQuerit`, `SiervoFiel`, `HondaDeDavid`)

### Patterns
- Section comments with `// ====...====` dividers
- `#pragma once` for include guards
- Many systems implement logic inline in `.h` files (faith, miracles, items, sprites, camera)
- Corresponding `.cpp` files exist even when empty (for future use)
- `SDL_Log()` for all diagnostic output
- `static_cast<u32>(EnumValue)` for enum-to-index conversion
- Switch statements as lookup tables (`get_miracle_def()`, `get_enemy_def()`, `get_anim_def()`)
- Factory functions for entity creation (`spawn_enemy()`, `spawn_projectile()`, `init_as_elias()`)
- TODO comments mark incomplete features

### What NOT to do
- Do NOT use OOP patterns (inheritance, virtual, polymorphism)
- Do NOT use `new`/`delete` — structs are value types or owned by ECSWorld arrays
- Do NOT add SDL2-style API calls — this is SDL3
- Do NOT use `std::shared_ptr` or other smart pointers — the architecture is flat data
- Do NOT add external dependencies without discussion — the project statically links everything
- Do NOT create `.md` documentation files unless explicitly asked

## Referencia de diseño: THE LEGEND OF ZELDA: A LINK TO THE PAST

**La referencia del juego es *A Link to the Past*** (decisión del usuario, 2026-08-25). No es
una comparación suelta: es el criterio con el que se resuelve cualquier duda de diseño de
mundo, exploración o ritmo. Lo que significa en concreto:

- **El exterior no tiene muros.** En ALTTP el límite del campo son acantilados, agua, matorral
  y vallas — nunca sillería. Los muros existen **dentro** de palacios, cuevas y casas. MANTLE
  hacía justo lo contrario: `Room::generate` rodeaba de `TileType::Wall` hasta el desierto. El
  borde lo decide el **bioma** (`BiomeStyle::border`), no el generador.
- **El exterior SCROLLEA; los interiores van sala a sala.** Zona exterior grande con la cámara
  siguiendo al profeta; interior = una pantalla con la cámara clavada — que es lo que el juego
  hacía siempre, y sigue saliendo gratis: con la sala a 1 pantalla el clamp de `Camera` no deja
  mover nada.
- **Se investiga porque hay secretos.** Matorral que se corta, piedras que se levantan, grietas,
  cuevas ocultas, cofres fuera del camino. Una sala que solo lleva al jefe no motiva a nadie.
- **Los milagros son las llaves de exploración**, como el guante o el gancho: Fuego del Cielo
  abre grietas y quema matorral, Sanar Aguas seca charcas, Ojos Abiertos revela lo invisible (ya
  existe), Carro de Fuego cruza huecos. Es lo que hace que valga la pena **volver**.
- **Los PNJ están de pie en el mundo** y se les habla. No son un icono del tileset.
- **Un objetivo claro y un mapa que lo señala** (`ChapterLore::objective` ya existe).

## Game Design Document

**Vigente: `GDD2.0.md`, en la raíz del repo.** Se versiona con el código y es la única fuente
de verdad para diseño.

**OBSOLETO: `../IdeaJuegoEliasEliseo.pdf` (GDD v1.0, Febrero 2026).** Está implementado de cabo
a rabo y ya no dirige nada; se conserva como documento histórico. Lo que sigue en esta sección
son las tablas del v1.0 que el código **aún refleja** (cifras de personaje, fe, milagros,
enemigos, controles): valen como referencia de lo implementado, no como plan. Si el v1.0 y
`GDD2.0.md` se contradicen, manda `GDD2.0.md`.

### Elevator Pitch

"Vive la era mas oscura de Israel como los profetas Elias y Eliseo. Desafia reyes, destruye altares de Baal, y cuando Elias ascienda al cielo, recoge su manto y continua su legado con el doble de su poder."

### Design Pillars

- **Fe como Recurso**: Faith is mana, shield, and divine connection
- **Milagros como Mecanica**: Abilities based on real biblical miracles
- **Legado Heredado**: Elias->Eliseo transition transfers power and progress
- **Accesible pero Profundo**: Easy to learn, hard to master

### Core Loop (20-35 min run)

1. EXPLORAR habitacion/zona
2. COMBATIR enemigos con Cayado + Milagros
3. RECOLECTAR Fe, Items, Reliquias
4. ELEGIR mejora entre 3 opciones
5. AVANZAR a siguiente habitacion
6. BOSS al final del capitulo
7. META-PROGRESO: Desbloquear items/personajes

### Two-Act Structure & Chapters

**ACTO I: ELIAS — El Profeta de Fuego**

| Cap | Nombre | Bioma | Boss |
|-----|--------|-------|------|
| 1 | El Arroyo Querit | Canon/Arroyo | Capitan de Acab |
| 2 | La Viuda de Sarepta | Aldea fenicia | Sacerdote Baal + La Muerte |
| 3 | El Monte Carmelo | Montana | 450 Profetas de Baal |
| 4 | La Cueva de Horeb | Desierto/Cueva | Tu Propia Desesperacion |
| 5 | El Llamado | Campos | Acab y Jezabel |

**TRANSICION: EL ASCENSO** — Elias sube al cielo, el manto cae a Eliseo, "Doble porcion"

**ACTO II: ELISEO — El Profeta de la Gracia**

| Cap | Nombre | Bioma | Boss |
|-----|--------|-------|------|
| 6 | Las Aguas de Jerico | Ciudad corrupta | Espiritu de Corrupcion |
| 7 | El Aceite de la Viuda | Aldea pobre | El Acreedor |
| 8 | El Hijo Sunamita | Casa noble | La Muerte (Mayor) |
| 9 | Naaman el Sirio | Campamento/Jordan | Lepra + Guiezi |
| 10 | El Ejercito Celestial | Ciudad sitiada | Ben-Adad |

**EPILOGO: El Legado** (Desbloqueble)

### The Ascension (Central Mechanic)

When Act I ends, Eliseo INHERITS from Elias:
- 50% of accumulated Faith
- Items marked as "Heredable"
- Elias miracles (unlocked but weaker)
- El Manto de Elias (unique core item)
- Complete bestiary (enemy knowledge)

Eliseo GAINS new:
- His own unique miracles
- "Doble porcion": 2 miracle slots from start
- Superior healing abilities
- Expanded Disciples system

### Characters

| Aspecto | Elias | Eliseo |
|---------|-------|--------|
| Arquetipo | Ofensivo, explosivo | Versatil, sanador |
| Arma base | Cayado de Fuego | Cayado de Luz |
| Fortaleza | Dano masivo | Curacion, buffs |
| Debilidad | Poca curacion | Menos dano directo |
| HP Base | 80 | 100 |
| Velocidad | 210 | 190 |
| Dano base | 12 | 10 |
| Slots milagro | 1 (2 en cap 4+) | 2 desde inicio |

### Faith System — "El Celo del Senor" (0-100)

**Thresholds & Effects:**

| Nivel | Fe | Nombre | Efectos |
|-------|-----|--------|---------|
| 5 | 90-100 | Fuego del Cielo | +40% dano, milagros potenciados |
| 4 | 75-89 | Profeta Ungido | +25% dano, Tier 1-4 disponible |
| 3 | 50-74 | Siervo Fiel | +10% dano, Tier 1-3 |
| 2 | 25-49 | Fe Vacilante | Normal, solo Tier 1-2 |
| 1 | 10-24 | En el Desierto | -15% dano, solo Tier 1 |
| 0 | 0-9 | Bajo la Retama | -30% dano, sin milagros |

**Gain Faith:** Destruir altar Baal (+8), matar profeta falso (+3), habitacion sin dano (+2), orar en altar (+10), ayudar viuda (+5), derrotar jefe (+15)

**Lose Faith:** Recibir dano (-1/hit), huir de confrontacion (-5), usar milagro (variable), inactividad (-0.5/10s)

### Miracles System

**Elias (5 miracles):**

| Milagro | Coste | Efecto | Tier |
|---------|-------|--------|------|
| Cuervos de Querit | 5 | Cura 10 HP | 1 |
| Palabra de Sequia | 8 | Ralentiza 50% | 1 |
| Fuego del Cielo | 20 | AOE 80 dano | 3 |
| Carro de Fuego | 25 | Dash devastador | 3 |
| Altar Consumido | 30 | Destruye pantalla | 4 |

**Eliseo (5 miracles):**

| Milagro | Coste | Efecto | Tier |
|---------|-------|--------|------|
| Sanar Aguas | 5 | Zona cura 5 HP/s | 1 |
| Aceite Inagotable | 12 | Consumibles x2 | 2 |
| Ojos Abiertos | 15 | Revela ocultos | 2 |
| Ejercito Celestial | 22 | 3 guerreros 10s | 3 |
| Doble Porcion | 30 | Stats x2 por 15s | 4 |

### Enemies

**Acto I:**

| Enemigo | HP | Dano | Comportamiento |
|---------|----|------|----------------|
| Soldado de Acab | 20 | 8 | Melee directo |
| Arquero Real | 15 | 10 | Distancia |
| Profeta de Baal | 35 | 12 | Caster, cura |
| Leon del Desierto | 40 | 18 | Rapido, salta |

**Acto II:**

| Enemigo | HP | Dano | Comportamiento |
|---------|----|------|----------------|
| Soldado Sirio | 25 | 10 | Melee resistente |
| General Sirio | 60 | 18 | Buff aliados |
| Idólatra de Rimón | 20 | 8 | Explota al morir (su brasero) |
| Agua Corrupta | 35 | 10 | Se divide |

### Items System (5 categories)

| Categoria | Ejemplos | Efecto |
|-----------|----------|--------|
| Armas | Honda, Vara de Aaron | Ataque secundario |
| Pasivos | Pan de Cada Dia (+10 HP) | Permanente en run |
| Reliquias | Tablas de la Ley | Muy poderoso, 1 slot |
| Consumibles | Vino Nuevo (+25% dano) | Uso unico |
| Heredables | Antorcha del Altar | Pasan a Eliseo |

### Controls

**PC (Teclado):** WASD/Flechas=Mover, K (mantener)=Ataque en ráfaga hacia la ÚLTIMA DIRECCIÓN DE MOVIMIENTO (8 direcciones, `Player::last_move_dir`; el ratón NO apunta ni dispara), Espacio/J=Dash, Q=Milagro1, E=Milagro2, F=Milagro3 (solo con el tercer slot), R=Consumible (usa el que llevas en `Player::consumable_slot`), Tab=Mapa, Esc=Pausa. En la pantalla de elección: A/D elegir, ENTER tomar. `Input::aim_axis` existe solo como override para el bot/scripts de autoplay. La opción "Auto-mira" (Opciones) activa el auto-apuntado del GDD también en PC (`--touch` lo fuerza para depurar).

**Gamepad:** Stick Izq=Mover, Stick Der=Apuntar, RT=Ataque, LT=Ataque secundario, A/X=Dash, RB=Milagro1, LB=Milagro2, **Y/△=Milagro3** (D-Pad↓ sigue valiendo), X/□=Consumible, D-Pad↑=Orar.
El GDD asignaba Y/△ a la vez a *Interactuar* y al tercer milagro; se resolvió a favor del
milagro porque **`Action::Interact` no la lee nadie** (el altar se activa al pisarlo y el cofre
al dispararle), y sin este botón el tercer slot solo se alcanzaba por la cruceta.

**Mobile (`ui/hud.cpp: draw_touch_controls` + `input/input.h: TouchButton`):** en un teléfono
el HUD **es** el mando: las tres celdas de milagro del HUD son a la vez sus tres botones
(mismos rects, definidos una sola vez en `Input::init_touch_layout`), y encima se apilan
**USAR** (consumible) y **DASH** — la única franja de la derecha que no pisa el minimapa ni la
barra de jefe (x 80..240 abajo). Joystick Izq virtual=Mover (nace donde baja el dedo),
Tap zona derecha=Atacar (auto-aim), Hold+Drag ≥6 unidades en la mitad derecha=Apuntar manual
(`joystick_aim` → `aim_axis`, que apaga el auto-aim por sí solo), Tap en un milagro=lanzarlo
(al SOLTAR, no al pulsar) y Hold ≥0,4 s=su ficha, Swipe de 2 dedos=Pausa.
Todo esto vive tras `Input::touch_ui`, que **solo** se enciende en móvil o con `--touch`:
`has_touch()` se descartó como criterio porque el portátil de desarrollo declara pantalla
táctil y el HUD salía con botones de teléfono en una partida de teclado.

**Auto-Aim (Mobile):** Prioridad: 1) Enemigo atacandote, 2) Mas cercano, 3) Menor HP (finishing), 4) Jefe en pantalla

### Mobile Adaptations

- Twin-stick preciso → Auto-aim + override manual
- Enemigos ligeramente mas lentos (-10%)
- Auto-save al completar cada habitacion (persiste 48h, luego "abandonada" = 50% ofrendas)
- Sesiones 5-15 min optimo (vs 20-35 min PC)
- UI minimalista, info bajo demanda

### Rendering

- **Game units**: all gameplay/UI code works in a 320x180 logical space (`GAME_WIDTH/HEIGHT`); speeds, colliders, HUD layout, tile logic (`TILE_SIZE = 16` units) never see the pixel resolution.
- **Render target**: `GAME_WIDTH*RENDER_SCALE x GAME_HEIGHT*RENDER_SCALE` = **1280x720** (`Config::RENDER_SCALE = 4`); `Renderer::begin_frame` calls `SDL_SetRenderScale(4,4)` (per-target view state in SDL3) so every draw call stays in units; the target is then letterboxed (linear) into the window (default 1280x720).
- **Assets**: `Config::ASSET_SCALE = 4` texels per unit — character frames 128x128 px (32 units), tileset cells 64x64 (16 units). `SpriteComponent::src_rect` is in TEXELS; drawn size = `src.w / ASSET_SCALE * scale` (`ECSWorld::collect_draws`). Sheets use linear filtering; the tileset uses nearest (atlas cells abut, CLAMP is texture-wide).
- **Look**: smooth alpha + dark outline (Blender `post_soft`), Workbench cavity+specular shading, soft elliptical shadows (`Renderer::shadow_tex`) under characters/props, screen vignette (`vignette_tex`), TTF text with outline (`draw_text`, 8 units line height at scale 1, `measure_text` for layout). Text passes `scale > 1.25` use the large atlas.
- Color palette: Elias=naranjas,rojos,dorados / Eliseo=azules,verdes,blancos / Baal=morados,negros,rojos oscuros / Entornos=ocres,beiges,piedra
- **NO es pixel art, y el `nearest` que queda NO es estetica.** El GDD v1.0 decia "Pixel Art
  16-bit" y "nearest-neighbor scaling"; eso esta **superado** (decision del usuario,
  2026-08-25): el estilo es figurita 3D suave. En `src/` solo quedan dos usos de
  `SDL_SCALEMODE_NEAREST` y los dos estan comentados en el sitio: `Renderer::load_texture`
  con `smooth=false`, que es **solo para atlas** (el tileset — sus celdas se tocan y CLAMP
  es de textura entera, asi que en lineal sangrarian unas sobre otras), y el zoom de
  depuracion de `save_screenshot(scale>1)`, que nadie llama con scale>1. Todo lo que el
  jugador mira —hojas de personaje, efectos, retratos, paneles del prologo, atlas de
  glifos— va en **lineal**. Al medir en unidades de juego, dilo asi: los comentarios que
  decian "pixels/sec" o "distance in pixels" en `config.h` e `input.h` mentian (nada en
  gameplay ve la resolucion). En `tools/`, `render.render_sheet` ya **no** cuantiza por
  defecto (`quantize=False`), que es lo que run.py pedia desde siempre.

### El Ascenso y el epilogo, recorridos (GDD2.0 fases 5 y 6, 2026-08-26)

Los dos momentos que cierran cada acto eran **paginas de texto que se pasaban con ENTER**.
Ahora se RECORREN: una cadena de encuentros con decision, uno detras de otro, y la pagina de
cierre al final. Comparten maquinaria con el pozo de encuentros —que es justo lo que el plan
predijo— y por eso costaron poco: son ids de `namespace Ev` fuera de todo capitulo
(`event_chapter` devuelve 0 para ellos) mas un contador de paso en el Engine.

**El Ascenso (2 Reyes 2:1-14): CUATRO paradas.** Gilgal, Betel, el Jordan y «pide lo que
quieras». Tres veces le dice que se quede y tres veces hay que negarse — eso es lo que el
pasaje repite, asi que es lo que el jugador repite.

- **Quedarse no rompe nada: acorta la herencia**, que es exactamente lo que significa no llegar
  hasta el final del camino. Cada parada en la que te quedas atras resta un 10% a la fe
  heredada. **Seguirle las tres veces y pedir la doble porcion deja la herencia EXACTAMENTE
  como estaba** antes de que el Ascenso fuera jugable — esa era la condicion del track, y por
  eso `ascension_check` sigue midiendo lo mismo.
- **El Manto cae igual** (se le cayo a el, no te lo dio: 2 Reyes 2:13). Lo que se pide o no se
  pide es el **espiritu doblado**: sin la doble porcion no hay milagro heredado, «y si no, no»
  (2:10).
- Las decisiones **se apuntan, no se aplican**: la herencia se reparte entera en
  `finish_ascension`. Aplicar ahi la fe perdida seria descontarla dos veces, porque la fe
  heredada se calcula sobre la que tengas al llegar.

**El epilogo "El Legado" (2 Reyes 9; 13:14-21): TRES escenas.** Jehu y la muerte de Jezabel
—que cumple al pie de la letra la palabra de Elias sobre la vina de Nabot y cierra el arco del
Acto I—, el arco de Joas y los huesos de Eliseo, cuyo ultimo milagro ocurre cuando el profeta
lleva un ano muerto.

- Aqui la partida **ya se ha acabado**, asi que estas decisiones no mueven vida ni fe: no queda
  partida que mover, e inventarle un premio mecanico a una escena de cierre seria relleno. Lo
  que cambian es lo que se te cuenta — y **la del arco cambia las ultimas palabras del juego**
  (`story_epilogue(s, joash_full)`), que en ese pasaje es exactamente lo que estaba en juego.
- Sigue siendo **desbloqueble**: fe en Profeta Ungido (>=75) al ganar el capitulo 10. Quien gane
  con la fe por los suelos se lleva su pantalla de victoria con su nota, igual que antes.

**`DialogueState::dark`**: las dos cadenas se juegan **sobre negro** y no sobre el mundo. Pasan
fuera de una sala, y dejar detras la arena del jefe del capitulo 5 —o la del 10— seria mentir
sobre donde estas.

Guiones: `ascension_check.txt` y `epilogo_check.txt`, ampliados para fotografiar cada parada.
Gates del log: `Ascension: le seguiste 3 de 3 veces, doble porcion=si` y `Epilogo: el arco de
Joas hasta el final`. En autoplay el dialogo decide solo y **siempre toma la primera opcion**,
asi que el bot recorre el camino completo — que es justo el que tiene que dar la herencia de
siempre.

### Interiores con muros de verdad (GDD2.0 fase 3, 2026-08-26)

La otra mitad de la gramatica de *A Link to the Past*. El exterior dejo de tener muros en la
fase 1; aqui el muro vuelve **donde significa algo**: dentro de un edificio. Uno por capitulo,
con su nombre (`get_interior_name`): la cueva del arroyo, el templo de Baal, el palacio de Acab,
el aposento sunamita...

- **Un interior mide UNA pantalla**, y con eso la camara se queda clavada sola —`Camera` recorta
  contra `Room::world_w/h`—, o sea que el "interior sala a sala" del GDD **sale gratis**, igual
  que salio en la arena del jefe. Esa era toda la maquinaria que hacia falta y ya estaba puesta.
- **No se genera por biomas: se monta con una PLANTILLA** (`build_interior`). Es lo contrario del
  exterior a proposito: fuera el generador reparte terreno, dentro hay una habitacion hecha, con
  su anillo de muro, cuatro pilares que cortan la linea de vision, antorchas flanqueando la
  puerta, tinajas contra las paredes y **siempre un premio** (altar en capitulos impares, cofre
  en pares). **Sin enemigos**: el premio de explorar no puede ser otra pelea.
- **`apply_interior_style`**: dentro no hay hierba. Con el suelo del bioma, un interior se lee
  como un PATIO — el mismo arenal con flores y matas, solo que con muros alrededor. Se cambia la
  familia de suelo por la de silleria y los decorados por grietas, cascotes y moho.

**El enlace, que es donde estaba el riesgo del track:**

- El nodo del interior va **DETRAS de la rejilla** en `FloorMap::rooms`, con `grid_x/grid_y = -1`
  (asi `room_at(gx,gy)` no lo devuelve nunca). `RoomNode::interior_room` en la sala de fuera y
  `interior_of` en el interior.
- **El guardado NO cambia de formato**, que era la condicion de abandono: `SaveData::current_room`
  es un INDICE y la rejilla se regenera igual con la misma semilla, asi que guardar dentro de un
  interior funciona tal cual.
- **El minimapa no miente**: `draw_minimap` **salta** los nodos con `interior_of >= 0`, y estando
  dentro marca como actual **la sala de fuera** — dejar el mapa sin ninguna sala marcada es peor
  que aproximar.
- `Engine::use_doorway` guarda la sala en `room_cache` antes de irse, igual que una transicion
  normal: al volver, los enemigos muertos siguen muertos y el cofre abierto sigue abierto.
- `Engine::spawn_tx/spawn_ty` manda sobre `entry_direction`: por una puerta **no se llega por un
  borde**. Y la puerta respeta el **coyote time**, porque al salir se aparece justo debajo de la
  de fuera y sin esa gracia se volveria a entrar en bucle.

**El anfitrion del interior se elige SIN sortear** (`(chapter*7 + k) % room_count`, saltando la
sala de inicio y la del jefe). Con `randi_range` ahi, el sorteo entero se desplazaba y con el
**todos los mapas de la partida**: la sonda de tacto lo caza en el acto — el bot se salia de la
sala a media medida y los leones y los arqueros daban CERO bajas. Regla general que vale para
cualquier cosa que se anada a `FloorMap::generate`: **si no necesita azar, no lo gastes**.

La **puerta** (`TileType::Doorway`, celda `door` en las filas 7-8 del tileset) se coloca en la
sala de fuera **con el mapa ya terminado y las sendas trazadas** — puesta antes, `carve_trail` la
araba como cualquier obstaculo — y se apoya en la senda: una puerta en mitad del descampado no se
encuentra. El vano oscuro es **geometria hundida**, no un plano oscuro: con luz de estudio,
Workbench aclara cualquier plano oscuro hasta gris medio (misma leccion que la boca de la cueva
en portraits.py).

Guion: **`scripts/interiores_check.txt`**, con el comando de autoplay **`door`** — deja al
profeta **al lado** de la puerta, no encima (pisarla la dispara, y asi no se podria fotografiar
antes de cruzarla) y dice por donde entrar. Ojo: el tile de al lado tiene que estar **dentro** de
la sala, porque `is_tile_walkable` deja pasar fuera de la rejilla (lo necesita
`try_room_transition`) y la puerta del muro sur de un interior dejaba al profeta en el vacio.
Gate del log: la secuencia `Loaded room 7 -> 12 -> 7`.

### Secretos y milagros como llaves (GDD2.0 fase 4, 2026-08-26)

La tercera queja del usuario era que *"no se motiva al jugador a investigar"*. La fase 1 dio
sitio que recorrer; esto da **razones para desviarse**, con el mecanismo de *A Link to the Past*:
los milagros son las llaves, y lo que abren se ve mucho antes de tener con que abrirlo.

**El gate de diseño, que manda sobre todo lo demas: un secreto tiene que VERSE antes de poder
abrirse.** Si la grieta no se ve hasta que tienes el milagro, no hay motivo para volver — y
volver es todo el punto. Por eso la tinaja, la grieta y el hueco **estorban el paso**: el jugador
tropieza con ellos en el capitulo 1 y no puede abrirlos hasta el 3 o el 4.

| Llave | Abre | Pasaje |
|---|---|---|
| Un tiro cualquiera | tinaja (`TileType::Jar`) y matorral | — |
| **Fuego del Cielo** | matorral y **grietas** (`TileType::Crack`), radio 80 | 1 Reyes 18:38 |
| **Sanar Aguas** | **seca la charca**, radio 56 | 2 Reyes 2:21 |
| **Carro de Fuego** | **cruza el hueco** (`TileType::Pit`) mientras dura | 2 Reyes 2:11 |
| **Ojos Abiertos** | (ya existia) revela el mapa y marca enemigos y objetos | 2 Reyes 6:17 |

- **Todo pasa por `Room::open_secrets(centro, radio, mascara, out_loot)`**, un solo sitio. La
  mascara `SecretKind` es lo que decide que abre cada llave. Devuelve DONDE ha aparecido un
  hallazgo y no suelta la pieza: eso lo hace el Engine, para que haya **un solo lugar que decida
  que paga un secreto**.
- **La celda ES el estado.** La sala entera se cachea en `FloorMap::room_cache`, asi que abrir un
  secreto persiste durante la partida **sin tocar el formato del guardado** — que era el criterio
  de abandono del track. Y `Room::tile_hides_loot` va por `tile_hash`: la misma semilla esconde
  lo mismo en los mismos sitios, asi que tampoco hay nada que guardar.
- **La OFRENDA es lo mas probable (60%)**, y a proposito: vida y fe las suelta cualquier enemigo,
  asi que un secreto que pagara sobre todo en eso no daria ningun motivo para desviarse. Lo que
  hace que explorar de mas signifique algo es que pague **entre partidas**
  (`ItemComponent::Pickup::Offering` -> `progression.offerings`, guardado en el acto con
  `save(false)` para no re-sellar la fecha de caducidad). Y **que paga cada hallazgo sale de SU
  POSICION**, no de `randi_range`: sortearlo movería el generador de mapas (es el mismo `rand()`)
  y ademas un guion de regresion no podria exigir una ofrenda concreta.
- **`Entity ECSWorld::gap_crosser`**: quien puede cruzar huecos AHORA. Es una entidad y no un
  componente porque dura dos segundos, y `walkable_at` la mira antes de rendirse. Se apaga sola
  cuando acaba la embestida — si se quedara puesta, el profeta andaria sobre los precipicios el
  resto del capitulo.
- **El Carro de Fuego parado embiste hacia DONDE MIRAS** (`last_move_dir`), no hacia arriba. Con
  el `{0,-1}` que tenia, lanzarlo sin moverse salia siempre al norte — y siendo la unica llave
  que cruza un hueco, el hueco que tenias delante se quedaba sin cruzar.

**Densidad: contada POR SALA, no por area** — es la excepcion a la regla de la fase 1, y se
cerro midiendo. Escalando por area salian 11 grietas, 16 tinajas y **24 tiles de precipicio** en
una sola sala: la sonda de tacto se disparo a 12,6 s de TTK con el bot clavado detras de un
hueco. Valores buenos: 4-7 tinajas en poblado (1-3 fuera), 2-4 grietas en roca (0-2 fuera) y
**1-2 huecos, nunca en la sala de INICIO ni en la arena del jefe** (en la de inicio es donde se
aprende a andar y donde corre la sonda; en la arena solo seria una trampa en una pelea que ya va
justa de sitio). Y **la grieta NO para los tiros** aunque no se pise: son dos masas bajas con una
hendidura y el disparo pasa por encima — un secreto tiene que ser un hallazgo, no cobertura para
el enemigo.

**Dos celdas nuevas del tileset** (fila 7): `jar` y `crack`. La tinaja lleva perfil de PANZA (dos
troncos de cono opuestos) y no un cilindro — un cilindro con asas es un cubo de fregar —, y lo
que la lee como vasija a 16 unidades es el cuello estrecho y el borde volteado, antes que las
asas. La grieta se monta sobre la silueta del canto rodado PARTIDA EN DOS: con cajas salieron
dos maletas grises, y la hendidura tiene que ir **delante y llegar al suelo** (puesta detras la
tapaban las dos mitades y quedaba en una muesca en la coronilla).

**El hueco se pinta como UNA mancha**, no con la celda del tileset: `TileSprite::Pit` dibuja un
ovalo por casilla, y en una banda de dos de ancho **no hay ni un tile interior**, asi que salian
seis ovalos en rejilla. Se pinta un relleno oscuro plano y un labio de tierra SOLO en los lados
que dan a suelo firme, que es lo que lo lee como hundido.

**El objetivo del capitulo sale ya en el mapa grande** (`ChapterLore::objective`, que existia
desde el primer commit y no pintaba nadie: el mapa solo sabia decir "ve al rojo").

Guion: **`scripts/secretos_check.txt`**, con dos comandos de autoplay nuevos que existen porque
sin ellos la comprobacion depende de la suerte del mapa: **`pit`** (coloca al profeta al borde de
un hueco y MIRANDO hacia el) y **`collect`** (atrae todas las recogidas de la sala — un hallazgo
cae donde estaba el secreto y un guion no sabe donde fue). Gates del log: `Secret loot: N
hallazgo(s) ... (N ofrenda, N fe, N vida)`, `Secret offerings: +N (total M)` y `Chariot: crossing
a pit`.

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
- **Rejillas 4x3 / 4x4** y `FloorMap::MAX_ROOMS` 25. El reparto de tipos escala con el
  numero de salas (2 Tesoro, 2 Altar, 2 Elite en rejillas de 12+), **menos el Encuentro, que
  sigue siendo UNO**: `get_event_def` da un solo encuentro por capitulo, asi que dos salas
  Event contarian la misma escena dos veces. Sube a 2-3 cuando exista el pozo de encuentros
  (GDD2.0 fase 2), no antes.

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
cifra absoluta no significa nada por si sola: lo unico comparable son dos pasadas con la
**misma semilla** (por eso el guion la fija en `seed 20260825`) y **la misma generacion de
sala**. Cualquier cambio en `Room::generate` mueve el sorteo y con el la medida, sin que el
motor se haya tocado.

Cifras buenas de referencia hoy (seed 20260825, tras secretos y milagros-llave): soldados
**2,74 s**, leones **1,50 s**, arqueros **0,79 s**, 1 golpe recibido en todo el recorrido.
Ojo con el tramo del jefe: `killall` deja la sala por limpia, asi que el bot **se sale de la
arena** por la abertura y el tramo se corta solo.

## Current State and TODOs

The project compiles and runs and the full 10-chapter loop is playable end to end:
- **Working**: movement (feet-point tile collision with axis sliding; `Room::is_walkable_world` is the single world→tile walkability test used by movement, `combat.cpp push()` body separation and spawns; `system_movement` has an `unstick()` safety net that snaps a body whose feet are inside a solid tile to the nearest walkable tile and logs it), shooting toward the last movement direction (hold K), projectiles with tileset sprites, enemy AI (melee packs, archers/casters/generals shoot with line-of-sight, lions leap), contact damage with player i-frames (1 s), knockback, hit flash, particles, camera shake, drops (hearts / faith orbs), faith system.
- **Conductas de muerte del Acto II** (`game/enemies.h`): `on_enemy_death_behavior` se llama desde `on_enemy_killed`, **antes** de `Engine::update_room_state`, para que lo que nazca al morir cuente ya como vivo (si no, la sala se declara limpia con enemigos dentro). El **Idólatra de Rimón** suelta al caer su brasero de brasas, que prende tras una mecha (`EnemyAIComponent::death_fuse`, 0.35 s de telegrafiado sobre el cadáver); `update_death_fuses` — que el Engine llama una vez por frame justo después de `system_collision` — la apura y detona: daño en radio 28 a jugador **y** enemigos. La cadena NO recursa: son dos pasadas (apurar mechas, luego detonar), así que una explosión que mata a otro Idólatra le enciende una mecha que se consume el frame siguiente. El **Agua Corrupta** se parte en 2 hijos al 55% de vida y escala (`split_generation`, máximo 2 generaciones, y no se divide con ≥18 enemigos vivos); los hijos dan fe reducida y ningún drop. El **General Sirio** arenga cada 4 s: +30% daño y +15% velocidad en radio 70 durante 5 s. Los valores base se guardan **una vez** en el aliado (`base_damage`/`base_speed`), así que refrescar el aura no acumula; el aliado late en rojo en `ECSWorld::collect_draws`. Sin nadie a quien arengar no gasta la arenga.
- **Working**: rooms — cliff borders with 6/5-tile openings on connected sides, trails carved from a jittered hub to every opening (guaranteed reachable; river → bridge, obstacles → path), rivers with fords, biome look per chapter (`get_biome_style`: floor family sand/meadow/paving via `floor_base`, 3 ground decals + `detail_pct` scattered by `tile_hash` on plain Floor tiles in `render_ground`, plus tints/props/buildings), room state persisted in `FloorMap::room_cache`, exits sealed while enemies live (`Room::exits_locked`), Start room Baal altar (shoot it: +8 Fe), Treasure chest, Altar prayer, Elite reward.
- **Working**: progression — room clear detection, boss per chapter (`spawn_boss`: volleys, ring, charge, HP bar), exit arch after the boss (`Room::place_exit`), `advance_chapter()`, Ascension screen → Eliseo (faith halved, 2 slots), Victory after chapter 10, Game Over with `[R]` retry chapter.
- **Working**: miracles with effects (`Player::cast_miracle`) — all 10 are reachable **y cada uno hace lo suyo** (ya no queda ningun placeholder): praying at an `AltarGod` opens a 3-card miracle choice (tier + faith cost + which slot it replaces). Eliseo can learn Elias' miracles and casts them at 70% (`inherit`). Starting sets: Elias = **Cuervos de Querit (Q, Tier 1)** + Fuego del Cielo (E, ch4+); Eliseo = Sanar Aguas + Ejercito Celestial.
- **Ojos Abiertos** (2 Reyes 6:17): pone `Player::map_revealed` (todo el capitulo visible en el minimapa y en TAB, con sus etiquetas TESORO/ALTAR/ELITE/RELATO — lo lee `HUDContext::map_revealed` en `draw_minimap`) y `reveal_timer`, que durante la duracion suelta una marca dorada sobre cada enemigo y azul sobre cada drop desde `Player::update`. `Engine::start_chapter` apaga los dos: revela ESTE capitulo, no los siguientes.
- **Aceite Inagotable** (2 Reyes 4:6): el consumible tiene cargas (`Player::consumable_charges`). Recoger otra vasija igual suma carga, una distinta reemplaza; `use_consumable` solo suelta el objeto cuando se gasta la ultima. El milagro da +2 cargas, y con las manos vacias regala un consumible con 2. El numero se pinta sobre la celda del HUD cuando es > 1. Va en el guardado (`SaveData::consumable_uses`).
- **Tres slots de milagro**: `Config::MAX_MIRACLE_SLOTS = 3` (Q / E / F; `Action::Miracle3` = tecla F, D-Pad abajo en mando). Elias sigue con 1 (2 desde el cap. 4); Eliseo con 2, y 3 con la mejora meta 6 de El Santuario. Todo lo que iteraba slots va contra la constante: `Player::miracle_slots`, la tabla `MIRACLE_KEYS` de `player.cpp`, `draw_miracle_slots` (la tercera celda cae en x≈238 de 320 y no pisa ni la caja de mensajes ni la barra de jefe), `open_miracle_choice`, el bot y el comando `learn`.
- **Miracle gating readability**: a Tier-3 miracle needs faith level *Siervo Fiel* (50), and the run starts at exactly 50, so equipping one in slot Q made it lock on the first hit taken and read as "I need 100% faith". The fix is threefold and must stay together: the Q slot always starts Tier 1; `draw_miracle_slots` splits `lock_tier` (padlock + the faith **required**, via `faith_required_for_tier`) from `lock_cost` (cost in red); and `Player::MiracleEvent` carries the reason (`NeedTier` / `NeedCost` / `OnCooldown`) so the Engine names it ("Fuego del Cielo: necesitas 50 de fe (Siervo Fiel)"). `draw_faith_bar` also ticks the 25/50/75 thresholds. A one-shot `[Q]` hint fires in ch. 1 after 5 castable seconds (`Engine::miracle_hint_*`).
- **Miracle VFX**: the sheets have a single `CastMiracle` row, so what tells the ten powers apart is the on-screen effect, not the pose. **Para efectos nuevos usa `spawn_fx` (hoja propia animada), no `spawn_vfx`**; este último se queda para los iconos sueltos del tileset y como respaldo. `spawn_vfx` (`game/combat.h`) drops a tileset icon that fades/grows/spins via `LifetimeComponent` (`total`/`fade`/`grow`/`spin`, applied in `system_lifetime`); `vfx_ring` / `vfx_column` / `vfx_pillar` in `player.cpp` compose it. Only use loose ICONS (Star/Orb/Fireball/Lightbolt…): terrain cells like `WaterA` are opaque squares and look like blocks. Lasting miracles (aura of Doble Porcion, bubbles of Sanar Aguas, trail of Carro de Fuego) emit from `Player::update` on `aura_timer`/`chariot_timer`. Altar Consumido raises `Player::flash_request` → `Engine::world_flash`, a white veil over the world under the HUD.
- **El Códice** (`ui/codex.h`): cinco pestañas — Capítulos / Bestiario / **Milagros** / **Objetos** / **Logros** (ver la sección de Logros). La de milagros se llamaba "Reliquias" y listaba milagros (nombre equivocado); las reliquias son objetos y viven en la de Objetos, que lista los 16 de `ItemRegistry` (incluidos los dos `event_only`) con el color de rareza en la fila y, abajo, el efecto + el pasaje **sin referencia**. El contador "Descubierto X/Y" cuenta las cuatro pestañas y ahora cuadra con lo que se puede ver. El contador va arriba a la derecha y la ayuda de teclas abajo: en 180 unidades de alto no caben las dos bajo el panel del pasaje.
- **Working**: HUD — minimap (TAB = big map), boss direction hint, chapter banners, messages, boss bar, miracle slots with cost/lock state, item row under the vitals (rarity-coloured frames, R marks the held consumable, número de cargas del consumible), floating damage numbers and a 0.05 s hit-stop on boss hits; audio (procedural music per scene + SFX, stereo with reverb; `Audio::load_*` falls back .ogg → .wav).
- **HUD** (`ui/hud.cpp`): framed glossy bars (`draw_bar`) with tileset icon badges (`draw_badge`, heart/orb/skull on a soft disc), miracle slots with bevel frame, cooldown sweep, key badge and orb+cost, boss bar with name plate, TTF text everywhere.
- **Debug**: `--script <file>` autoplay (wait/hold/press/aim/shot/walk/bot/goto/chapter/killall/spawn/god/seed/give/learn/faith/cards/mcards/stats/say/quit; `intro` lanza el prólogo y `cine n` salta a una de sus 8 estampas y para el avance automático, para poder fotografiarla; `chapter n` jumps to a chapter to check biomes/bosses; `event <capítulo> [índice]` abre un encuentro del pozo de ese capítulo sin caminar hasta su sala — las salas `RoomType::Event` las sortea el generador y solo coloca 2-3 de las 3-4 que hay, así que sin él verificar los 35 encuentros dependería de la suerte del mapa; loguea id, locutor, retrato y PNJ; `spawn <enemyType> <n>` siembra enemigos de un tipo concreto junto al jugador — sin él, comprobar una conducta del Acto II depende de que el capítulo la sortee; `feel <etiqueta>` imprime el TRAMO de tacto desde la marca anterior (TTK, golpes recibidos, tiros, daño); `fx <id> [tamaño] [duración]` suelta un efecto de la hoja propia delante del profeta; `give <itemId>`, `learn <miracleId> <slot>` (slot 0..2), `faith <n>`, `cards`/`mcards` open the choice screens — con ellos se puede fotografiar cualquier efecto sin jugar hasta él; `press weapon` = ataque secundario, `press m3` = tercer milagro). Añadidos al cerrar el plan del GDD: `ascend` (dispara la Ascensión sin rematar al jefe del cap. 5 — es el único camino al Manto), `advance` (cierra el capítulo; en el 10 es lo que dispara la victoria), `agesave <horas>` (envejece el guardado para probar la caducidad de 48 h), `unlocks <n>` (fija `SaveData::unlocks`, para volver a bloquear el epílogo), `meta <id>` (compra una mejora de El Santuario sin ofrendas), `pit` (coloca al profeta al borde de un hueco y mirando hacia el), `collect` (atrae las recogidas de la sala), `door` (deja al profeta al lado de la puerta de la sala) y `touch`/`drag`/`untouch <x> <y> [dedo]` (toques sintéticos en coordenadas de juego: el ratón NO genera eventos `SDL_EVENT_FINGER_*`, así que sin ellos los controles táctiles solo se podían probar en un teléfono). `stats` imprime además una segunda línea `LOADOUT` con profeta, slots, recarga y objetos. `bot n` termina solo si no hay enemigos en la sala. `killall` lista primero y mata después, para que los hijos del Agua Corrupta no caigan en el mismo barrido, y **sus bajas cuentan** en el marcador (`combat_events` se vacía después de leerse, no antes de llenarse). Un `shot` **no suelta** lo que se estuviera manteniendo: `hold map 0.6` + `shot` es la única forma de fotografiar el mapa grande. F12 screenshot (`screenshots/`, 1280x720), room ASCII dump in the log while a script runs. Put a `wait 0.1` after each `shot` so the capture lands before the next command.
- **Guiones de verificación** en `scripts/`: `lore_check.txt` y `pulido_check.txt` (regresión, deben pasar siempre), **`feel_baseline.txt`** (MEDIDA de tacto, no captura: se ejecuta antes y después de tocar `Config::Feel` y se comparan las líneas `FEEL`), `feel_check.txt`, `audio_check.txt` (criterio negativo: cero `Failed to load`), `logros_check.txt` (desbloquea 18/18 desde guardado limpio), `vfx_check.txt` (efectos, retratos y paleta), `encuentros_check.txt` (los **35** encuentros del pozo, en ES y EN; su gate es el log, no las capturas), `hint_check.txt`, `actoII_check.txt` (conductas del Acto II), `codice_check.txt` (las 5 pestañas del Códice), **`secretos_check.txt`** (secretos de bioma y los tres milagros-llave), **`interiores_check.txt`** (entrar y salir de un interior sin perder el estado de fuera), `milagros_check.txt` (Ojos Abiertos, cargas del consumible, tercer slot y guardar/cargar), `intro_check.txt` (las 8 estampas del prólogo), `ascension_check.txt` (Manto de Elías + milagro heredado), `caducidad_check.txt` y `tactil_check.txt` (**los dos piden `--touch`**), `epilogo_check.txt` (las dos ramas de la victoria) y `discipulos_check.txt`.

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
- Rendering: `Room::render_ground` (floor/water/walls/shores) then a y-sorted `DrawCmd` list (`Room::collect_props` + `ECSWorld::collect_draws` → `Renderer::draw_sorted`), then vignette, then the room fade, then HUD.
- **Shores**: water tiles autotile their banks — 3 cells authored for "land to the north" (`TileSprite::ShoreEdge/Out/In`, tileset row 7) drawn rotated 90/180/270 from an 8-neighbour mask in `render_ground`, so the river reads as a continuous curve. River generation drifts one column every other row with a 2-3 wide bed.
- **Sin referencias biblicas en pantalla**: el texto de cada cita se muestra tal cual, pero `Scripture::ref` ("1 Reyes 17:4") NO se pinta en ningun sitio — ni historia, ni cartas de eleccion, ni encuentros, ni Codice. El campo sigue en `game/lore.h` solo como atribucion documental. Al anadir texto nuevo, no metas la referencia en el `text`.
- **Entity recycling**: `ECSWorld::create()` calls `reset_components()` — ids come back through the free list, and stale component data (a projectile's `hit_list` still naming the boss) silently broke damage. Never assume a spawner clears every field it does not set.
- `Engine::room_fade` (0.3 s black veil under the HUD) plays on every `load_room`; particles are tinted `TileSprite::Star` sprites.
- **Working**: items — `GameScene::Choice` offers 3 cards after Elite/Boss rooms, on opening the treasure chest and in 1 of 5 normal rooms (`open_item_choice`). `ItemRegistry::roll_choices` never repeats an owned item, so a run tops out at the 14 sorteables (los tres `event_only` no entran). `Player::grant_item` applies HP/damage/speed/faith/cadence plus relics (Tablas → `FaithSystem::floor_value`, Arca → `miracle_power`) and consumables (R). Heredable items pass to Eliseo on the Ascension. Weapons (Honda/Vara) are a **secondary attack** on `Action::AttackSecondary` (tecla `I`): `Player::fire_secondary` — la Honda lanza una piedra que atraviesa con la animacion de disparo, la Vara barre en arco con `AnimID::CastMiracle` y cinco brotes verdes. Al recogerla el mensaje dice la tecla, y la celda del HUD lleva icono por arma + la `I` + el nombre.
- **Working**: save **v8** — chapter seed + room + HP/faith + items + consumable (con sus cargas) + los 3 slots de milagro + `unlocks` (contenido desbloqueado entre partidas) + `codex_events` (relatos vistos); auto-saves on every cleared room, and "Continuar" regenerates the same map and drops the player in the saved room (rooms are regenerated, so cleared ones have their enemies back). El fichero es un volcado binario de `SaveData`, asi que **anadir un campo rompe el formato**: `SaveSystem::load` lleva dos lectores de compatibilidad (`SaveDataV3` + `migrate_v3` y `SaveDataV4` + `migrate_v4`) que suben las partidas v3 y v4 sin perderlas y reescriben el fichero en v5. Si vuelves a cambiar `SaveData`, sube la version y anade otro lector igual — no se tiran partidas. v1 y v2 si se rechazan.
- **v7 reservo de golpe TODOS los campos que iban a hacer falta**, aunque nacieran a cero: ajustes del jugador (`music_volume`, `sfx_volume`, `aim_assist`, `sensitivity`, `language`, `settings_flags`) y desbloqueos (`items_unlocked`, `skins_unlocked`, `skin_equipped`, `entitlements`). Cinco frentes distintos necesitaban persistir algo; subir version por separado habrian sido cinco migraciones que mantener. **Los ajustes ON/OFF viven en la mascara `settings_flags` (`SETTING_*`) y no en bools sueltos por una razon concreta: anadir un bit NO rompe el formato del fichero.** Hasta v6 no se guardaba **ningun** ajuste — el volumen que ponias volvia a su sitio en el siguiente arranque.
- Comando de autoplay **`savedump`**: vuelca partida + meta + ajustes al log en tres lineas `SAVEDUMP`. Es lo unico que permite comprobar desde un guion que una migracion no ha perdido nada, porque la meta entera no se ve en ninguna pantalla del juego. Guion: `scripts/guardado_check.txt` (migra un fichero viejo real, siembra meta, gana el cap. 10 y comprueba que `delete_save` deja la partida a cero y la meta intacta).
- **Caducidad móvil (GDD 4.4)**: `SaveData::save_timestamp` son **segundos desde la época** (`SDL_GetCurrentTime`); antes eran `SDL_GetTicks`, que arranca a cero en cada sesión y no medía días, y por eso `is_run_expired()` estaba escrita y no la llamaba nadie. A las 48 h la fila "Continuar" del menú pasa a leer **"Partida abandonada +N"**: aceptarla ingresa el 50 % de la tarifa del capítulo (`get_abandoned_offerings`, que ya NO es un porcentaje del banco de ofrendas — eso duplicaba el ahorro del jugador) y **borra** la partida conservando la meta-progresión. Solo se aplica con reglas de móvil (`Engine::mobile_rules`, que `--touch` enciende en PC). Los sellos antiguos (< 1e9) se dan por "sin fecha" y no caducan.
### `assets/data` y `assets/shaders`: borradas a propósito (2026-08-25)

Llevaban vacías desde el primer commit con la etiqueta "pending". Una carpeta vacía que
lleva meses ahí no es una tarea pendiente: es una decisión que nadie ha tomado. Tomada:

**`assets/shaders` — borrada.** SDL 3.5 *sí* permite shaders propios sobre `SDL_Renderer`
(`SDL_GPURenderState` / `SDL_SetGPURenderState`, desde 3.4), así que la razón no es "no se
puede". Es el precio: hace falta el **backend GPU** —y el juego crea el renderer por
defecto, `SDL_CreateRenderer(window, nullptr)`, que en Windows es D3D11— y hay que
compilar y **distribuir el mismo shader en tres formatos** (SPIR-V, DXIL y MSL), más un
camino alternativo para los backends que no lo soportan y para Emscripten. Eso es
rediseñar el renderer entero para un juego de 320x180 cuyos efectos de pantalla son dos
texturas procedurales (`Renderer::vignette_tex` y el fogonazo de `world_flash`). **Los
efectos a pantalla completa se hacen con texturas procedurales**, y así se queda.

**`assets/data` — borrada.** Las tablas de objetos, milagros, enemigos y capítulos son
`switch` **a propósito**: es la convención del proyecto y da los errores al compilar, no
al jugar. Sacarlas a JSON sería un paso atrás. Lo único que ganaría viviendo en datos es
el **texto** (lore, diálogos, páginas de historia), y precisamente por eso no se hace a
medias: cuando toque traducir hará falta un diseño de i18n de verdad — ids de cadena,
cobertura de glifos en el atlas para otros alfabetos, y qué pasa con la regla de que
`Scripture::ref` no se pinta nunca. Una carpeta vacía "reservada para eso" es el mismo
"pending" con otro nombre; el día que exista el diseño, `mkdir` cuesta cero.

### Efectos, retratos y paleta por capítulo (2026-08-25)

**Hoja de efectos propia** (`assets/sprites/vfx.png`, 8 fotogramas x 7 filas, celda de
128 px = 32 unidades). Antes TODO el vocabulario de efectos eran cuatro iconos del
tileset (`Star`/`Orb`/`Fireball`/`Lightbolt`) que además hacen de proyectil y de objeto.

- `spawn_fx(world, pos, FxID, tint, size_units, duration, z)` (`game/combat.h`) monta un
  efecto **animado de verdad**: 8 fotogramas con `AnimationComponent`. Convive con
  `spawn_vfx`, que sigue existiendo para los iconos del tileset.
- **`size_units` escala contra 32, no contra los 16 del tileset.** Es el error fácil:
  escalar contra 16 deja todos los efectos al doble.
- Sin la hoja cargada devuelve `ENTITY_NONE` y **no dibuja nada**, así que los sitios
  críticos llaman a `spawn_vfx` de respaldo (`if (spawn_fx(...) == ENTITY_NONE) …`). Un
  efecto nuevo no puede dejar al juego sin feedback.
- La hoja va con filtrado **suave**, al revés que el tileset: sus celdas son halos
  degradados y el vecino más próximo los convierte en escalones. No hay sangrado entre
  celdas porque cada fotograma acaba transparente en los bordes.
- `select_animation` **se salta las entidades con `Lifetime` y sin `Health`**: un efecto
  manda su propia fila. Sin eso, al acabar la animación `playing` se apaga, la guarda de
  arriba deja de cortar y el efecto salta a la fila de Idle en su último fotograma. Los
  cadáveres no entran ahí porque tienen `Health` y su rama de Death va antes.
- Depuración: comando de autoplay **`fx <id> [tamaño] [duración]`**, que suelta un efecto
  suelto delante del profeta. En plena pelea no hay forma de saber qué efecto es cuál.

El arte lo genera `tools/blender/mantle_sprites/vfx.py`, que **no usa nada del pipeline
de personajes** y por buenas razones:

- **EEVEE, no Workbench.** Workbench es un sombreador de sólidos: no tiene emisión, y un
  fogonazo sin emisión es una bola de plástico gris.
- **Sin contorno oscuro.** Los 2 px de `post_soft` hacen que un personaje se lea como
  figurita; en una chispa solo la convierten en pegatina.
- **El resplandor se hace en el post, no con el bloom de EEVEE.** Comprobado: con
  `film_transparent` el bloom pinta color fuera de la geometría pero **no escribe alfa**,
  así que el halo se recorta y desaparece. `_glow()` desenfoca brillo **y** alfa.
- **Un toro sale de canto.** `primitive_torus_add` lo deja en el plano XY, o sea de perfil
  para esta cámara: sin girarlo 90° en X sale una raya horizontal, no un aro. Y como la
  escala de Blender es local y se aplica antes de girar, el aplastado en pantalla se pide
  en el eje local **Y**.
- Un haz de luz es un **cono**, no una esfera con la Z multiplicada por diez: eso es una
  cápsula de farmacia.

**Retratos** (`assets/sprites/portraits.png`, 6x2, celda de 96 px = 24 unidades). Los doce
personajes en primer plano, y **sí** con el pipeline de personajes tal cual (Workbench,
cavity, contorno): un retrato tiene que parecer el mismo muñeco que se ve en la sala.
`tools/blender/mantle_sprites/portraits.py` **encuadra midiendo la caja** del personaje ya
construido, no con alturas fijas: el león es un cuadrúpedo y el Agua Corrupta una masa más
ancha que alta, y con medidas de humano salían diminutos o cortados.

Se usan en el **Bestiario del Códice** (`Portrait::src(index)` en `ui/codex.h`); los OCHO
PRIMEROS van en el orden de `EnemyType` a propósito, para indexar por tipo sin tabla de
traducción. `codex.render` acepta `TEXTURE_NONE` y se dibuja igual, sin retrato.

**Los encuentros (`game/events.h`) ya llevan retrato** — ver la sección propia más abajo.
(Lo que decía aquí antes, "se quedaron sin retrato porque ocho de sus diez locutores no
existen como modelo", dejó de ser cierto el 2026-08-25.)

**Paleta por capítulo** (`get_enemy_chapter_tint` en `game/enemies.h`, aplicada en
`spawn_common`): un tinte multiplicativo suave por capítulo, para que el mismo soldado
sirio no se vea igual en Jericó que en la ciudad sitiada. Se hace **tintando y no con
hojas alternativas** porque una paleta por capítulo son ocho PNG de 896x1920 más por
variante, y una regla del proyecto es que `assets/` no crece. Lo pisan igual el destello
del golpe y el aura del General, porque `collect_draws` arranca de `s.tint`. Medido:
el mismo uniforme pasa de B/G 0,77 (cap. 6) a 0,96 (cap. 10).

Guion: `scripts/vfx_check.txt` — los siete efectos aislados con `fx`, luego dentro del
juego, luego la paleta, y el Bestiario **al final**, porque salir del Códice deja el juego
en el menú de pausa y todo lo que exige estar jugando (`chapter`, `spawn`) se descarta ahí
en silencio.

### El pozo de encuentros: 10 -> 35 relatos (GDD2.0 fase 2, 2026-08-26)

Lo que el usuario puso por escrito: *"el flujo de la historia tiene que tener en cuenta TODO LO
REFERENTE A ELIAS Y ELISEO DEL LIBRO DE REYES"*. Habia **10** encuentros y ahora hay **35**, uno
por cada episodio que `GDD2.0.md` §2 enumera para los diez capitulos.

**Un POZO por capitulo, no un encuentro.** `get_event_def` toma ahora un **id** de
`namespace Ev` (game/events.h) y `get_chapter_events(capitulo)` devuelve `{primero, cuantos}`:
el orden del enum ES la tabla, y por eso los ids estan agrupados por capitulo. Un encuentro
nuevo se anade AL FINAL DE SU CAPITULO.

- **El generador coloca 2-3 salas `Event`** (`std::min(pool.count, room_count >= 16 ? 3 : 2)`)
  y reparte ids de una **bolsa barajada**: cada sala cuenta una escena DISTINTA. Con 3-4
  encuentros por capitulo y 2-3 salas, **en una sola partida no se ven todos ni queriendo** —
  que es exactamente lo que hace que explorar y repetir capitulo signifiquen algo.
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
- **Siete caras nuevas en `characters.py`**: `viuda_sarepta`, `eliseo_labrador`,
  `hombre_jerico`, `mujer_profeta`, `sunamita`, `naaman`, `criado_eliseo`. Existen **solo
  para su retrato**: no andan por el mundo, así que **no** tienen hoja de 896x1920. Eso lo
  garantiza `characters.SHEET_NAMES` — `NAMES` ya no es `list(SPECS.keys())`, porque si lo
  fuera un `run.main()` sin argumentos les fabricaría siete hojas de 15 filas que nadie
  carga.
- **Dos piezas nuevas del builder**, y las dos por una razón concreta:
  - **`headcloth`** (+ `headcloth_len='long'` para velo, + `headcloth_cord`). No vale
    reusar `hood`: la capucha es un cuello **redondo y cerrado** y el paño **cae a los
    lados de la cara**. Cinco de los siete llevan algo en la cabeza, y con una sola
    silueta se parecerían todos en un retrato de 32 unidades.
  - **`band`**: aro liso, sin puntas. `crown` y `diadem` llevan puntas, y unas puntas
    sobre la mujer de Sunem la convertirían en reina — y 2 Reyes 4:8 dice "mujer
    importante", no reina. La lleva ella (oro) y Naamán (oro).
  - Arrancan en z 1.700 como el casquete de pelo, así que **la regla de dejar libre
    z>=1.60 por delante sigue en pie**: la frente y las cejas se ven.
- **Lo que NO hizo falta**, contra lo que se sospechaba al planificarlo: props de mano
  (vasija de aceite, aguijada de bueyes). El encuadre del retrato corta por la cintura y
  todo lo que cuelga de la mano (z ~0.6-0.9) **queda fuera del cuadro**. Lo que distingue
  a un locutor de otro está de los hombros para arriba: paño, pelo, barba y color de túnica.
- **Las dos variantes tenían que distinguirse de su original de un vistazo**, y ese fue el
  gate: `eliseo_labrador` es la misma cara que `eliseo` pero sin el manto blanco y sin el
  cayado de luz (las dos cosas que aún no le han dado), y `naaman` es un noble sirio **sin
  casco ni penacho** — que es justo lo que lleva el General Sirio —, con pelo y barba
  negros a la vista, cinta de oro, teal en vez de rojo y túnica larga.
- Ojo con el color: la viuda de Sarepta y el criado salían los dos **beige** y no había
  forma de decir cuál era cuál. Se separan por **temperatura**: ella cálida (terracota
  gastada), él frío (paño pardo sobre túnica cruda).

**Las tres viñetas** (`SCENES` en `portraits.py`, no en `characters.py`: no son personajes
y solo existen como celda de retrato). Se encuadran distinto — una persona se encuadra por
la cara (`HEAD_FRAC`) y una escena tiene que entrar **entera**— y se miran con **13 grados
de picado** (`SCENE_ELEV`, `setup_scene_cam`): de frente puro la zanja del altar cae justo
detrás del altar y no se ve. Lo que costó tres pasadas y conviene no volver a intentar:

- **El cuervo va DE PERFIL**, mirando a `-X`. Es el único modelo del proyecto que no mira a
  `-Y`, y por una razón: de frente, un pájaro simétrico con algo en el pico sale con la
  comida cruzada sobre la cara y se lee como un babero o como un pico amarillo de tucán.
  De perfil el pico es una cuña que sobresale y el pan cuelga de su punta. Ojo también con
  el giro del pico: la caja ya está construida a lo largo de X, así que girarla 78 grados
  la pone **de pie** y el cuervo mira al cielo.
- **La zanja del altar va DELANTE**, no debajo. Puesta bajo las piedras —o detrás de un
  labio de tierra— el agua no se ve en absoluto, y el agua es justo lo que hace el pasaje.
- **El vano de la cueva es un fondo PLANO y hundido**, no una esfera oscura: una esfera se
  lee como una bola metida en el hueco, no como un hueco. Y la pared son **tres masas
  grandes**, no una corona de esferas iguales (el cavity subraya cada una y sale el "collar
  de perlas" que ya avisa la sección del tileset); lo que estrecha la boca va **pegado a
  las masas y de su mismo color**, o asoma dentro del vano como dos bultos flotando.

Guiones: **`scripts/encuentros_check.txt`** recorre los diez con el comando de autoplay
**`event <n>`** y fotografía cada uno dos veces (una línea del cuerpo y la decisión con sus
dos opciones). En los dos capítulos cuya línea es más larga se fotografían **las tres
líneas**, porque el truncado silencioso de `wrap_text` es el único fallo que no se ve venir.

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

### Voz sintetizada (GDD 6.4, 2026-08-25)

El GDD pide dos cosas y **solo una es compatible con la regla de cero assets de terceros**:
las **exclamaciones cortas de gameplay** se sintetizan aqui; las **cinematicas dobladas
NO**, porque exigen TTS de terceros — dependencia externa y licencia. Eso queda como
decision del usuario, sin hacer.

**Modelo fuente-filtro** (`gen_audio.py`), que es lo que separa una voz de un tono con
envolvente. Son dos cosas independientes:

- la **FUENTE** (`glottal`): el pulso glotal, que da tono y registro. **No es un seno** —
  es un pulso con muchisimos armonicos, aqui un diente de sierra suavizado, con **jitter**
  de tono y **shimmer** de amplitud. Sin esos dos la voz sale plana y robotica, por el
  mismo motivo por el que `Song.add` humaniza la rejilla de la musica.
- el **FILTRO** (`_resonator` x3 sobre `FORMANTS`): el tracto vocal resuena en tres
  formantes fijos, y **son ellos, no el tono, los que hacen que una "a" suene a "a"**.
  Cambiar de vocal es mover los formantes.

`voice(vowel, f0, dur, bend, effort, breath)`. **`bend` (caida del tono) no es un adorno**:
un grito de esfuerzo siempre baja al final y sin eso suena a nota tenida. `effort` aprieta
el aire y sube el 3er formante, que es lo que se oye como "apretar los dientes".

**15 ficheros, 132 KB** (`assets/` sigue en 25 MB). Cuatro exclamaciones — esfuerzo (dash),
dolor (golpe recibido), milagro y jefe — con **tomas propias para cada profeta**: Elias
grave y cerrado (f0 ~106, vocal 'a'/'o'), Eliseo mas alto y abierto (f0 ~138, 'e'/'a').
Se separan por **tono y vocal, no por efectos**: eso un jugador lo distingue a la primera;
dos reverbs distintos, no. Tres variantes por exclamacion, por la misma razon que los
disparos.

- **`Engine::play_voice` con `voice_cooldown` de 2,2 s** es lo unico que separa "el profeta
  se queja" de "el profeta no calla": con i-frames de 1 s se oiria un gemido por segundo.
  El grito de **jefe se salta la espera** — suena una vez por capitulo y es justo el
  momento en el que tiene que oirse.
- **Solo los milagros de tier 3+ llevan voz.** Si el profeta gritara tambien al curarse con
  los cuervos, la exclamacion dejaria de significar nada.
- `SDL_Log("Voice: ...")` por exclamacion: sin el, `audio_check` solo podia decir que el
  fichero se habia CARGADO, no que se hubiera oido.
- **`gen_audio.py` acepta ahora un PREFIJO** (`python tools/audio/gen_audio.py voz_`) para
  regenerar un grupo sin reescribir los 47 ficheros que ya estan bien.

Guion: `audio_check.txt` ampliado. Dos trampas que costo verlas: el bloque de voz al final
del guion **no sonaba** porque tras la victoria la escena ya no es `Playing` y los comandos
de partida se descartan en silencio; y la voz del milagro se la comia el `voice_cooldown`
de los golpes anteriores. Ademas, para el grito de jefe hay que entrar en su sala con
`goto`, y **el indice depende del capitulo** (`gw/2`: el cap. 1 es 3x3 -> sala 1; el cap. 3
es 4x3 -> sala 2).

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

### Orientacion VERTICAL en movil (GDD 4.5, 2026-08-25)

**El area de juego sigue siendo 320x180 y se centra.** Recortar el mundo visible cambiaria
el juego, no la presentacion. `Renderer::calculate_viewport` ya centraba **cualquier**
proporcion de ventana, asi que en vertical el mundo se ve entero sin tocar una linea; lo
unico que cambia es **donde caben los controles**.

- **`Renderer::begin_overlay()` / `end_overlay()`**: una pasada que dibuja sobre la VENTANA
  entera en vez de dentro del objetivo de 320x180, para poder pintar en las bandas negras.
  Las coordenadas siguen siendo unidades de juego con el origen en la esquina del area de
  juego, asi que un boton en y=190 cae 10 unidades por debajo del mundo — **que es
  exactamente lo que devuelve `window_to_game` para un dedo apoyado ahi**. Los dos lados
  usan el mismo sistema de coordenadas y por eso el hit-test no necesita saber nada.
  El desplazamiento se **suma** en `world_to_screen` (`ui_offset`) y no con
  `SDL_SetRenderViewport`: en SDL3 viewport y escala interactuan de forma facil de
  equivocar, y esto es una suma.
- **`Input::init_touch_layout(band_below)`**: si la banda de abajo da para las dos filas
  (22 + 12 + 22 + 6 = 62 unidades), los botones **bajan a la banda**, que es donde estan
  los pulgares y donde no tapan el mundo. Si no cabe, se quedan donde siempre. Se
  recalcula al crear la ventana y **al girar el telefono**
  (`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`).
- **El contrato "los rects se definen una sola vez" se ha REFORZADO, que era el punto**:
  `HUD::draw_miracle_slots` ya no recalcula la posicion de las celdas — **la toma de
  `Input::touch_buttons`** cuando hay tactil. Antes estaban en dos sitios y solo un
  comentario las mantenia iguales, y en vertical eso separo el dibujo del hit-test: la
  celda se veia dentro del mundo y se pulsaba 160 unidades mas abajo.
- En vertical, las celdas de milagro se pintan **tambien en la superposicion** (`HUD::render`
  se las salta): dentro del objetivo de 320x180 saldrian recortadas por el borde.
- **`Renderer::present()` se separo de `end_frame()`** para que entre los dos quepa la
  superposicion; y **la captura de pantalla va antes de `present()`** y lee la VENTANA
  cuando hay vertical — leyendo el objetivo interno saldria el mundo sin las bandas, que es
  justo lo que se quiere comprobar.

**`--window <ancho>x<alto>`** (main.cpp) fuerza la proporcion de la ventana: existe para
poder probar esto en un escritorio, sin un telefono delante. Guion:
`scripts/vertical_check.txt` (`--touch --window 540x960`). Con 540x960 la escala es 1,6875,
el area de juego mide 540x304 y quedan 328 px = **194 unidades** de banda arriba y abajo.

### Segundo joystick tactil (GDD 4.2 "opcional", 2026-08-25)

`SETTING_TOUCH_RSTICK` -> `Input::touch_right_stick`. **La diferencia es una sola cosa, y
es la que separa un twin-stick de un hold+drag**: con la opcion apagada el dedo derecho se
comporta como siempre (tap = disparo con auto-mira, y hay que arrastrar `TOUCH_DRAG_AIM`
unidades para que el apuntado registre); con ella encendida el stick **nace ya activo y
apunta desde el primer pixel**, y se dibuja en pantalla.

- **El reparto de zona con los botones ya estaba resuelto y hay que no romperlo**: en
  `process_touch`, los `touch_buttons` se comprueban ANTES que el "tap = ataque" y hacen
  `return`. Un dedo que baja dentro de una celda de milagro lanza el milagro y **nunca** se
  convierte en stick. Verificado en el guion.
- Usa la **Sensibilidad** del ajuste de usuario: divide el radio del joystick y el umbral
  de arrastre.
- **La fila de Opciones solo existe en tactil**, y por eso las filas de esa pantalla pasaron
  a montarse sobre una **tabla de ids** (`MenuSystem::SettingRow` + `settings_rows[]`) en
  vez de por indice fijo: sin ella, quitar una fila desplazaba el significado de todas las
  de abajo **tanto al pintar como al ajustar**. `Engine::update_settings` decide por ID.

**`SDL_Log("Touch aim ON (right stick|drag threshold)")`, una linea por gesto**, es lo que
hace comprobable la opcion desde un guion. Verificarlo a ojo comparando capturas costo dos
pasadas perdidas: el generador pone **altares dorados** por el mapa y su adorno se confunde
con la mira. Y el umbral **no es fijo** — es `TOUCH_DRAG_AIM` entre la sensibilidad, que
vive en el guardado: con sensibilidad 8 baja de 6 a 4,2 unidades y un arrastre de 5 lo
cruzaba solo. Guion: `scripts/tactil_check.txt` ampliado (comando nuevo **`rstick 0|1`**,
que existe porque la fila de Opciones no esta en escritorio y un guion no puede navegar a
lo que no se pinta).

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

### Ajustes del jugador (GDD 4.2) e i18n (2026-08-25)

**Los ajustes viven en `SaveData` (v7), no en memoria.** Hasta esta pasada NINGUNO se
guardaba: el volumen que ponias volvia a su sitio en el arranque siguiente. Son meta, asi
que sobreviven a `delete_save`. El camino es siempre el mismo y **`apply_settings_from_save`
es el unico que los aplica**: `SaveData` -> `Engine::load_settings_mirror` -> los campos
`MenuSystem::settings_*` (que solo pintan) -> `Engine::commit_settings_from_mirror` ->
`SaveData` -> `apply_settings_from_save` -> Audio / Input / i18n. `MenuSystem` no conoce ni
el guardado ni `Input` a proposito. Se guarda con **`save(false)`**: tocar un ajuste no
re-sella la fecha y por tanto no le reinicia a nadie la caducidad de 48 h.

Seis filas + Volver en Opciones (con 7 el paso baja a 15 unidades: a 16 la ultima caia
sobre la linea de ayuda):

- **Vibracion ON/OFF**: la puerta esta en `Input::rumble` y es **la unica**, porque el motor
  la llama sin preguntar. En movil, no poder apagarla es un defecto de accesibilidad y de
  bateria, no una preferencia estetica.
- **Sensibilidad 1-10** (`Config::Feel::SENSITIVITY_*`, 5 = x1.0): aqui no hay camara que
  apuntar, asi que **no** es una velocidad: divide el radio del joystick de apuntado tactil
  y el umbral `TOUCH_DRAG_AIM`, o sea **cuanto hay que arrastrar** para que el apuntado
  registre y llegue a su tope.
- **Auto-mira de TRES estados** (`Config::AimMode`), que es lo que pide el GDD y no un
  interruptor. Sustituye a `Input::auto_aim_enabled`, que era binario; quien solo necesite
  saber si el tiro se fija tiene `Input::auto_aim_locks()`.
  - **Off**: tu direccion y punto.
  - **Assisted**: tu direccion, corregida `AIM_ASSIST_PULL` (65%) hacia el objetivo **solo
    si cae dentro de `AIM_ASSIST_CONE_DEG`** (22 grados ~ la mitad del sector de una de las
    8 direcciones del teclado: corrige el error de apuntar, no elige objetivo por ti).
  - **On**: con las manos quietas fija el objetivo de `pick_target`. Un apuntado manual
    explicito lo pisa igual que antes.
- La decision de a donde va el tiro esta en **`Player::resolve_aim`, un solo sitio**. Antes
  el ataque principal y el secundario llevaban cada uno su copia de la misma logica.

**i18n** (`core/i18n.h`): el idioma es `Language` (ES base + EN) y el mecanismo es
**`tr("es", "en")`**, con las dos versiones juntas en el punto de uso. Se descarto a
proposito el enum `StringID` + tabla por idioma: los dos dan el error en compilacion, pero
con `tr` el error es aun mas duro (falta un ARGUMENTO) y quien revisa ve el original y la
traduccion en la misma linea, que es donde se detecta que una traduccion esta mal — a
cambio de no inventar ni sincronizar ~600 ids. `en` vacio cae al espanol. Limite conocido y
documentado: el atlas se hornea con Lilita One (latino), asi que ES/EN/PT/IT/FR entran sin
tocar nada y **cirilico/griego/CJK no**. Y `Scripture::ref` sigue sin pintarse, traducido o no.

**Comandos de autoplay nuevos**: `lang <0|1>`, `aimmode <0|1|2>` y **`defaults`** (ajustes
de fabrica). `defaults` no es un lujo: las filas que **ciclan** (auto-mira, vibracion,
idioma) no tienen un valor absoluto al que llevarlas con `press`, asi que sin el un guion
que las toca da un resultado distinto en cada vuelta segun como quedaran en la anterior.
Por lo mismo **`feel_baseline.txt` empieza ahora con `defaults`**: los ajustes sobreviven
entre sesiones y con la auto-mira en "Asistido" las cifras se movian y parecia una
regresion del motor.

Guiones: **`scripts/ajustes_check.txt`** (las seis filas, en los dos idiomas, y persisten)
y **`scripts/mira_check.txt`**. Este ultimo mide lo que `feel` **no puede** medir: el bot
apunta a mano y perfecto, asi que los tres modos le dan el mismo TTK (comprobado: identico
con `aimmode` 0, 1 y 2). Lo que separa los modos es la GEOMETRIA, asi que dispara UNA vez
con el profeta mirando a un sitio fijo y el enemigo a un angulo conocido — y la posicion se
fija con **`walk`, que es bloqueante, no con `hold up`**: cada `hold` corria al profeta un
poco mas y al tercer caso la geometria ya no era la misma (asi fallo la primera version).
Medido: dentro del cono OFF hace **0** de dano y Asistido **15**; fuera del cono Asistido
hace **0** y ON **15**. O sea que "Asistido" no es ninguno de los otros dos.

### Audio (`tools/audio/gen_audio.py` -> `.ogg`, pasada del 2026-08-25)

**Los 47 ficheros de `assets/audio` son `.ogg` (Vorbis), no `.wav`.** Los mismos sonidos
ocupaban **48 MB** en WAV y no viajaban ni a Web ni a movil; ahora `assets/` entero pesa
**24 MB**. `Audio::load_*` sigue cayendo a `.wav` si falta el `.ogg`, asi que las dos
formas funcionan, pero el `.wav` queda solo como depuracion
(`python tools/audio/gen_audio.py all wav`). La transcodificacion la hace **ffmpeg**, que
es herramienta de compilacion: no se distribuye ni se enlaza nada suyo, y sin el el
generador escribe `.wav` y avisa. SDL3_mixer descodifica Vorbis con **stb_vorbis**
compilado dentro (`SDLMIXER_VORBIS_STB=ON`), sin DLL externa.

Todo el audio es **sintesis propia con numpy** — ni una muestra de terceros. Lo que subio
el liston del sintetizador:

- **`ks_pluck`**: cuerda Karplus-Strong de verdad (ruido en un anillo de retardo que se
  filtra a cada vuelta) en lugar de la pila de senos de `note()`, que sonaba siempre a
  campana. Se calcula **por filas de un periodo**, no muestra a muestra: son `dur*freq`
  iteraciones de numpy en vez de `dur*sr` en Python.
- **`ney`**: flauta de cana con aire soplado y vibrato que *entra* — la voz solista del
  Levante, que no existia. `melody_from_pattern(..., voice='ney')`.
- **`Song` con dos buses**: el estereo ya no se finge al final con `widen`; cada voz se
  coloca al anadirla (`add(..., pan=)`, potencia constante) y cada canal lleva su propio
  juego de retardos de reverb (`ROOM_L` / `ROOM_R`). Aun asi la correlacion L/R se queda
  en ~0,98 porque bajo, bombo y pads van al centro: es **deliberado**, el juego sale a
  movil y ahi todo se reproduce en mono.
- **Humanizacion**: `Song.add` desplaza cada evento unos milisegundos y le varia la
  intensidad. Una rejilla exacta es lo que delata a la musica generada. `tight=True` la
  salta para pedales y notas tenidas, que tienen que caer clavadas en el compas.
- **`compress`**: compresor de bus suave (3:1). Su detector va **diezmado en bloques de
  64 muestras** — no es un detalle de rendimiento: con un filtro por muestra la
  envolvente de relajacion sale a `np.convolve` con ventana de 14.000 muestras sobre
  millones y el generador se cuelga **minutos por pista**.

**Variacion por disparo**: `Audio::play_sfx_variant(first, count, vol)` sortea entre
`count` tomas **consecutivas** en el enum (como la terna `CayadoSwing1..3` de siempre).
Hay tres de cada proyectil, tres de `EnemyHit` y dos de `EnemyDeath`. Sin esto, mantener
el ataque suena a ametralladora. **Ese sorteo usa un RNG propio (`Audio::variant_rng`),
nunca `rand()`**: `rand()` es el generador que siembra los mapas (`core/types.h: randf`),
asi que sortear ahi haria que el AUDIO moviera la partida — dos partidas con la misma
semilla divergirian segun cuantos sonidos hubieran sonado.

**Ducking**: un SFX con volumen >= 0,85 agacha la musica (`Audio::duck_music`) y
`Audio::update(dt)` la deja subir. Se aplica **solo** en `set_music_volume`, sobre la
ganancia de la pista: `music_volume` sigue siendo exactamente lo que el jugador puso en
Opciones. El Engine solo tiene que llamar a `audio.update(dt)`.

Guion: `scripts/audio_check.txt` (menu -> prologo -> combate -> muertes con sonido propio
-> milagros -> jefe -> Ascension -> victoria). Su criterio es **negativo**: cero lineas
`Failed to load` en el log. Procedencia y licencias de todo: `assets/CREDITS.md`.

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

- **Working**: mobile auto-aim (`pick_target` in `game/combat.h`, GDD priority boss > attacking > closest > lowest HP), shared with the autoplay bot; toggle in Opciones, `--touch` forces it on desktop. `pick_target` **solo devuelve hostiles**: con los discípulos en el mundo, apuntar a los tuyos sería lo primero que pasaría.

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

### Sistema de Discípulos (facción en el ECS)

Un **discípulo** lleva `EnemyAIComponent` como cualquier otro combatiente pero con
**`ally = true`**: es la única marca de facción del juego, y no gasta ni un componente nuevo del
presupuesto del ECS. La regla que hay que respetar al tocar cualquier bucle:

> **`world.has(e, Component::EnemyAI)` NO significa "es un enemigo".** Para eso está
> `is_hostile(world, e)` (y su pareja `is_ally`), en `core/ecs.h`.

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
- No Steamworks (el usuario lo aparca en el backlog como "integracion steam"). El audio es
  sintesis propia (`.ogg`, ver seccion "Audio"); **no hay ni un asset de terceros salvo la
  tipografia Lilita One (OFL)** — procedencia completa en `assets/CREDITS.md`.
- **Backlog: "integración steam"** (decisión del usuario, 2026-08-25). Los logros ya funcionan
  en las cuatro plataformas sin Steam. Si algún día entra, entra como **adaptador delgado**
  tras `MANTLE_STEAM` que lea `SaveData::achievements` — no al revés. Necesita appid y
  permiso explícito para añadir la dependencia (`CLAUDE.md` la prohíbe sin discusión).
- **Pendiente (del usuario, no del agente)**: probar los controles táctiles en un **Android real**.
  Están verificados con eventos `SDL_EVENT_FINGER_*` sintéticos (`scripts/tactil_check.txt`), que
  cubren la lógica pero no el tamaño real del pulgar ni el DPI del dispositivo.

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

## Platform Macros

Set by CMake, available in C++:
- `MANTLE_PLATFORM_DESKTOP` — Windows/Linux/Mac
- `MANTLE_PLATFORM_WEB` — Emscripten
- `MANTLE_PLATFORM_ANDROID` — Android
- `MANTLE_PLATFORM_IOS` — iOS
- `MANTLE_PLATFORM_MOBILE` — Android OR iOS
- `MANTLE_STEAM` — Steamworks SDK present
