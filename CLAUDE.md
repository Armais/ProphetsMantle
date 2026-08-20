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
    items.h/cpp         - 13 items (5 categories), item registry, heredable items
    rooms.h/cpp         - Tile-based rooms (20x11 tiles), floor map generation, 10 chapters
    combat.h/cpp        - AABB/circle collision, damage, projectiles, i-frames
  ui/
    hud.h/cpp           - In-game HUD (health, faith, miracle slots)
    menus.h/cpp         - Menu screens (title, pause, game over, ascension)
  systems/
    save.h/cpp          - Binary save/load with version header, mobile expiry
    progression.h/cpp   - Meta-progression upgrades (7 upgrades, offerings currency)
  platform/
    platform.h          - Platform detection, asset/save paths, touch/steam detection
assets/
  audio/music/          - Procedural placeholder loops (.wav, generated by tools/audio/gen_audio.py; loader falls back .ogg -> .wav)
  audio/sfx/            - Procedural placeholder SFX (.wav, same generator)
  sprites/              - Character sheets: 10 PNG (896x1920, 7 cols x 15 rows of 128x128 = 32 game units
                          x ASSET_SCALE 4, rows = AnimID, smooth alpha + 2px dark outline)
                          + tiles.png (terrain tileset, 64px cells = 16 units x4, 8 cols x 8 rows: rows 0-4 terrain/props/
                          pickups, row 5 floor families grass_a-d + stone_a-d, row 6 ground decals, row 7 shore
                          autotiling pieces; see TileSprite in game/rooms.h)
  fonts/                - LilitaOne-Regular.ttf (OFL) baked into glyph atlases at init (fallback: SDL debug text)
  data/                 - Game data (empty, pending)
  shaders/              - Shaders (empty, pending)
tools/
  blender/mantle_sprites/ - Python pipeline (Blender 4.1 + blender-mcp) that builds the 10 rigged
                            characters procedurally, animates them per AnimID and renders the sheets
                            (run.py: `main(only=[...])`, default quantize=False = smooth post_soft;
                            quantize=True = legacy pixel-art). FRAME=128, SS=2, feet baseline = row 112.
                            Stylisation in characters.py: HEAD_SCALE 1.45 (parts on the head bone scaled about
                            HEAD_PIVOT), LION_HEAD_SCALE, palette through parts.vivid() (SAT 1.35, skin 1.15).
                            tiles.py renders the terrain tileset (`tiles.main(SPRITES_DIR, TMP_DIR)`, CELL=64,
                            TILE_ROWS=8; `only=[...]` patches the existing tiles.png instead of blanking it);
                            its LAYOUT must match `TileSprite` in src/game/rooms.h. GROUND = opaque
                            top-down FLAT cells, DETAILS = transparent top-down decals, SMALL/PROPS/PROPS_TALL = 30 deg.
  blender/blend/          - Source .blend per character (not runtime assets)
  audio/gen_audio.py      - numpy synth for all SFX + music loops (`python tools/audio/gen_audio.py`)
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
| Rooms | `game/rooms.h` | `Room`, `FloorMap` | 16px tiles, 20x11 grid, grid navigation |
| Save | `systems/save.h` | `SaveSystem` | Binary format, version 1 |
| Progression | `systems/progression.h` | `ProgressionSystem` | 7 meta-upgrades |

### ECS Components

Entity is a `u32` index. Components tracked via `std::bitset`. Fixed arrays of size `MAX_ENTITIES = 512`:

`TransformComponent`, `SpriteComponent`, `AnimationComponent`, `HealthComponent`, `CombatComponent`, `MovementComponent`, `ColliderComponent`, `FaithComponent` (unused), `MiracleUserComponent` (unused), `EnemyAIComponent`, `ProjectileComponent`, `ItemComponent`, `LifetimeComponent`

### Scene State Machine

`GameScene` enum: `MainMenu → Playing → Paused → Choice → GameOver / Victory`

`Choice` is the "elige 1 de 3" screen (items or miracles) drawn over the frozen world.

With special `Transition` state for room changes and `Ascension` for Act I→II switch.

### Room System & Navigation

Each chapter (scene) contains a **grid of rooms** that the player navigates freely. When the player reaches any edge of the current room, they transition to the adjacent room in that direction (if one exists).

#### Room Grid Layout Per Chapter

Each chapter defines its own grid dimensions and room distribution:

| Cap | Grid | Total Rooms | Layout Notes |
|-----|------|-------------|--------------|
| 1 | 3x3 | 9 | Start=bottom-center, Boss=top-center, river biome |
| 2 | 3x3 | 9 | Start=left-center, Boss=right-center |
| 3 | 4x3 | 12 | Vertical mountain ascent, Start=bottom, Boss=peak |
| 4 | 3x3 | 9 | Cave exploration, darker rooms |
| 5 | 4x3 | 12 | Open fields, wider grid |
| 6-10 | 3x3 to 4x4 | 9-16 | Scales with difficulty |

#### Room Transition Mechanic

- Player walks to the **top edge** (y ≤ 0) → load room at `(grid_x, grid_y - 1)`, player spawns at bottom
- Player walks to the **bottom edge** (y ≥ GAME_HEIGHT) → load room at `(grid_x, grid_y + 1)`, player spawns at top
- Player walks to the **left edge** (x ≤ 0) → load room at `(grid_x - 1, grid_y)`, player spawns at right
- Player walks to the **right edge** (x ≥ GAME_WIDTH) → load room at `(grid_x + 1, grid_y)`, player spawns at left
- If no adjacent room exists in that direction, the player is blocked by a full wall row/column on that border (no transition)

#### Border Openings Design

Every border is a cliff (`Wall`) except an **opening** on each connected side: `Room::GAP_X0..GAP_X1` (cols 7-12) on north/south, `GAP_Y0..GAP_Y1` (rows 3-7) on east/west. A dirt trail (`Path`) is carved from a hub near the centre to every opening (river → `Bridge`, obstacles cleared), so exits are always reachable and visually obvious. Combat rooms seal the openings (`exits_locked`, red pulsing bands, border tiles non-walkable) until every enemy is dead.

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
- **Tile map**: 20x11 grid with biome-specific decoration (trees, river, rocks, bushes, tall grass for Cap 1)
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

## Game Design Document (GDD)

Source: `../IdeaJuegoEliasEliseo.pdf` (GDD v1.0, Febrero 2026)

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
| Leproso Enloquecido | 20 | 8 | Explota al morir |
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

**Gamepad:** Stick Izq=Mover, Stick Der=Apuntar, RT=Ataque, LT=Ataque secundario, A/X=Dash, RB=Milagro1, LB=Milagro2, D-Pad↓=Milagro3, X/□=Consumible, Y/△=Interactuar, D-Pad↑=Orar

**Mobile:** Joystick Izq virtual=Mover, Tap zona derecha=Atacar (auto-aim), Hold+Drag=Apuntar manual, Boton Dash, Botones Milagro (Tap=usar, Hold=info), Swipe 2 dedos=Pausa

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

## Current State and TODOs

The project compiles and runs and the full 10-chapter loop is playable end to end:
- **Working**: movement (feet-point tile collision with axis sliding; `Room::is_walkable_world` is the single world→tile walkability test used by movement, `combat.cpp push()` body separation and spawns; `system_movement` has an `unstick()` safety net that snaps a body whose feet are inside a solid tile to the nearest walkable tile and logs it), shooting toward the last movement direction (hold K), projectiles with tileset sprites, enemy AI (melee packs, archers/casters/generals shoot with line-of-sight, lions leap), contact damage with player i-frames (1 s), knockback, hit flash, particles, camera shake, drops (hearts / faith orbs), faith system.
- **Conductas de muerte del Acto II** (`game/enemies.h`): `on_enemy_death_behavior` se llama desde `on_enemy_killed`, **antes** de `Engine::update_room_state`, para que lo que nazca al morir cuente ya como vivo (si no, la sala se declara limpia con enemigos dentro). El **Leproso** enciende una mecha (`EnemyAIComponent::death_fuse`, 0.35 s de telegrafiado sobre el cadáver) y `update_death_fuses` — que el Engine llama una vez por frame justo después de `system_collision` — la apura y detona: daño en radio 28 a jugador **y** enemigos. La cadena NO recursa: son dos pasadas (apurar mechas, luego detonar), así que una explosión que mata a otro Leproso le enciende una mecha que se consume el frame siguiente. El **Agua Corrupta** se parte en 2 hijos al 55% de vida y escala (`split_generation`, máximo 2 generaciones, y no se divide con ≥18 enemigos vivos); los hijos dan fe reducida y ningún drop. El **General Sirio** arenga cada 4 s: +30% daño y +15% velocidad en radio 70 durante 5 s. Los valores base se guardan **una vez** en el aliado (`base_damage`/`base_speed`), así que refrescar el aura no acumula; el aliado late en rojo en `ECSWorld::collect_draws`. Sin nadie a quien arengar no gasta la arenga.
- **Working**: rooms — cliff borders with 6/5-tile openings on connected sides, trails carved from a jittered hub to every opening (guaranteed reachable; river → bridge, obstacles → path), rivers with fords, biome look per chapter (`get_biome_style`: floor family sand/meadow/paving via `floor_base`, 3 ground decals + `detail_pct` scattered by `tile_hash` on plain Floor tiles in `render_ground`, plus tints/props/buildings), room state persisted in `FloorMap::room_cache`, exits sealed while enemies live (`Room::exits_locked`), Start room Baal altar (shoot it: +8 Fe), Treasure chest, Altar prayer, Elite reward.
- **Working**: progression — room clear detection, boss per chapter (`spawn_boss`: volleys, ring, charge, HP bar), exit arch after the boss (`Room::place_exit`), `advance_chapter()`, Ascension screen → Eliseo (faith halved, 2 slots), Victory after chapter 10, Game Over with `[R]` retry chapter.
- **Working**: miracles with effects (`Player::cast_miracle`) — all 10 are reachable **y cada uno hace lo suyo** (ya no queda ningun placeholder): praying at an `AltarGod` opens a 3-card miracle choice (tier + faith cost + which slot it replaces). Eliseo can learn Elias' miracles and casts them at 70% (`inherit`). Starting sets: Elias = **Cuervos de Querit (Q, Tier 1)** + Fuego del Cielo (E, ch4+); Eliseo = Sanar Aguas + Ejercito Celestial.
- **Ojos Abiertos** (2 Reyes 6:17): pone `Player::map_revealed` (todo el capitulo visible en el minimapa y en TAB, con sus etiquetas TESORO/ALTAR/ELITE/RELATO — lo lee `HUDContext::map_revealed` en `draw_minimap`) y `reveal_timer`, que durante la duracion suelta una marca dorada sobre cada enemigo y azul sobre cada drop desde `Player::update`. `Engine::start_chapter` apaga los dos: revela ESTE capitulo, no los siguientes.
- **Aceite Inagotable** (2 Reyes 4:6): el consumible tiene cargas (`Player::consumable_charges`). Recoger otra vasija igual suma carga, una distinta reemplaza; `use_consumable` solo suelta el objeto cuando se gasta la ultima. El milagro da +2 cargas, y con las manos vacias regala un consumible con 2. El numero se pinta sobre la celda del HUD cuando es > 1. Va en el guardado (`SaveData::consumable_uses`).
- **Tres slots de milagro**: `Config::MAX_MIRACLE_SLOTS = 3` (Q / E / F; `Action::Miracle3` = tecla F, D-Pad abajo en mando). Elias sigue con 1 (2 desde el cap. 4); Eliseo con 2, y 3 con la mejora meta 6 de El Santuario. Todo lo que iteraba slots va contra la constante: `Player::miracle_slots`, la tabla `MIRACLE_KEYS` de `player.cpp`, `draw_miracle_slots` (la tercera celda cae en x≈238 de 320 y no pisa ni la caja de mensajes ni la barra de jefe), `open_miracle_choice`, el bot y el comando `learn`.
- **Miracle gating readability**: a Tier-3 miracle needs faith level *Siervo Fiel* (50), and the run starts at exactly 50, so equipping one in slot Q made it lock on the first hit taken and read as "I need 100% faith". The fix is threefold and must stay together: the Q slot always starts Tier 1; `draw_miracle_slots` splits `lock_tier` (padlock + the faith **required**, via `faith_required_for_tier`) from `lock_cost` (cost in red); and `Player::MiracleEvent` carries the reason (`NeedTier` / `NeedCost` / `OnCooldown`) so the Engine names it ("Fuego del Cielo: necesitas 50 de fe (Siervo Fiel)"). `draw_faith_bar` also ticks the 25/50/75 thresholds. A one-shot `[Q]` hint fires in ch. 1 after 5 castable seconds (`Engine::miracle_hint_*`).
- **Miracle VFX**: the sheets have a single `CastMiracle` row, so what tells the ten powers apart is the on-screen effect, not the pose. `spawn_vfx` (`game/combat.h`) drops a tileset icon that fades/grows/spins via `LifetimeComponent` (`total`/`fade`/`grow`/`spin`, applied in `system_lifetime`); `vfx_ring` / `vfx_column` / `vfx_pillar` in `player.cpp` compose it. Only use loose ICONS (Star/Orb/Fireball/Lightbolt…): terrain cells like `WaterA` are opaque squares and look like blocks. Lasting miracles (aura of Doble Porcion, bubbles of Sanar Aguas, trail of Carro de Fuego) emit from `Player::update` on `aura_timer`/`chariot_timer`. Altar Consumido raises `Player::flash_request` → `Engine::world_flash`, a white veil over the world under the HUD.
- **El Códice** (`ui/codex.h`): cuatro pestañas — Capítulos / Bestiario / **Milagros** / **Objetos**. La de milagros se llamaba "Reliquias" y listaba milagros (nombre equivocado); las reliquias son objetos y viven en la de Objetos, que lista los 15 de `ItemRegistry` (incluidos los dos `event_only`) con el color de rareza en la fila y, abajo, el efecto + el pasaje **sin referencia**. El contador "Descubierto X/Y" cuenta las cuatro pestañas y ahora cuadra con lo que se puede ver. El contador va arriba a la derecha y la ayuda de teclas abajo: en 180 unidades de alto no caben las dos bajo el panel del pasaje.
- **Working**: HUD — minimap (TAB = big map), boss direction hint, chapter banners, messages, boss bar, miracle slots with cost/lock state, item row under the vitals (rarity-coloured frames, R marks the held consumable, número de cargas del consumible), floating damage numbers and a 0.05 s hit-stop on boss hits; audio (procedural music per scene + SFX, stereo with reverb; `Audio::load_*` falls back .ogg → .wav).
- **HUD** (`ui/hud.cpp`): framed glossy bars (`draw_bar`) with tileset icon badges (`draw_badge`, heart/orb/skull on a soft disc), miracle slots with bevel frame, cooldown sweep, key badge and orb+cost, boss bar with name plate, TTF text everywhere.
- **Debug**: `--script <file>` autoplay (wait/hold/press/aim/shot/walk/bot/goto/chapter/killall/spawn/god/seed/give/learn/faith/cards/mcards/stats/say/quit; `chapter n` jumps to a chapter to check biomes/bosses; `spawn <enemyType> <n>` siembra enemigos de un tipo concreto junto al jugador — sin él, comprobar una conducta del Acto II depende de que el capítulo la sortee; `give <itemId>`, `learn <miracleId> <slot>` (slot 0..2), `faith <n>`, `cards`/`mcards` open the choice screens — con ellos se puede fotografiar cualquier efecto sin jugar hasta él; `press weapon` = ataque secundario, `press m3` = tercer milagro). `bot n` termina solo si no hay enemigos en la sala. `killall` lista primero y mata después, para que los hijos del Agua Corrupta no caigan en el mismo barrido. Un `shot` **no suelta** lo que se estuviera manteniendo: `hold map 0.6` + `shot` es la única forma de fotografiar el mapa grande. F12 screenshot (`screenshots/`, 1280x720), room ASCII dump in the log while a script runs. Put a `wait 0.1` after each `shot` so the capture lands before the next command.
- **Guiones de verificación** en `scripts/`: `lore_check.txt` y `pulido_check.txt` (regresión, deben pasar siempre), `hint_check.txt`, `actoII_check.txt` (conductas del Acto II), `codice_check.txt` (las 4 pestañas del Códice), `milagros_check.txt` (Ojos Abiertos, cargas del consumible, tercer slot y guardar/cargar).
- Rendering: `Room::render_ground` (floor/water/walls/shores) then a y-sorted `DrawCmd` list (`Room::collect_props` + `ECSWorld::collect_draws` → `Renderer::draw_sorted`), then vignette, then the room fade, then HUD.
- **Shores**: water tiles autotile their banks — 3 cells authored for "land to the north" (`TileSprite::ShoreEdge/Out/In`, tileset row 7) drawn rotated 90/180/270 from an 8-neighbour mask in `render_ground`, so the river reads as a continuous curve. River generation drifts one column every other row with a 2-3 wide bed.
- **Sin referencias biblicas en pantalla**: el texto de cada cita se muestra tal cual, pero `Scripture::ref` ("1 Reyes 17:4") NO se pinta en ningun sitio — ni historia, ni cartas de eleccion, ni encuentros, ni Codice. El campo sigue en `game/lore.h` solo como atribucion documental. Al anadir texto nuevo, no metas la referencia en el `text`.
- **Entity recycling**: `ECSWorld::create()` calls `reset_components()` — ids come back through the free list, and stale component data (a projectile's `hit_list` still naming the boss) silently broke damage. Never assume a spawner clears every field it does not set.
- `Engine::room_fade` (0.3 s black veil under the HUD) plays on every `load_room`; particles are tinted `TileSprite::Star` sprites.
- **Working**: items — `GameScene::Choice` offers 3 cards after Elite/Boss rooms, on opening the treasure chest and in 1 of 5 normal rooms (`open_item_choice`). `ItemRegistry::roll_choices` never repeats an owned item, so a run tops out at the 13 items. `Player::grant_item` applies HP/damage/speed/faith/cadence plus relics (Tablas → `FaithSystem::floor_value`, Arca → `miracle_power`) and consumables (R). Heredable items pass to Eliseo on the Ascension. Weapons (Honda/Vara) are a **secondary attack** on `Action::AttackSecondary` (tecla `I`): `Player::fire_secondary` — la Honda lanza una piedra que atraviesa con la animacion de disparo, la Vara barre en arco con `AnimID::CastMiracle` y cinco brotes verdes. Al recogerla el mensaje dice la tecla, y la celda del HUD lleva icono por arma + la `I` + el nombre.
- **Working**: save v4 — chapter seed + room + HP/faith + items + consumable (con sus cargas) + los 3 slots de milagro; auto-saves on every cleared room, and "Continuar" regenerates the same map and drops the player in the saved room (rooms are regenerated, so cleared ones have their enemies back). El fichero es un volcado binario de `SaveData`, asi que **anadir un campo rompe el formato**: `SaveSystem::load` lleva un lector de compatibilidad (`SaveDataV3` + `migrate_v3`) que sube las partidas v3 sin perderlas y reescribe el fichero en v4. Si vuelves a cambiar `SaveData`, sube la version y anade otro lector igual — no se tiran partidas. v1 y v2 si se rechazan.
- **Working**: mobile auto-aim (`pick_target` in `game/combat.h`, GDD priority boss > attacking > closest > lowest HP), shared with the autoplay bot; toggle in Opciones, `--touch` forces it on desktop.
- No Steamworks. Audio is still procedural (now with reverb/stereo, see tools/audio/gen_audio.py); no external assets.

### Pendiente: el "Leproso Enloquecido" no debería ser un enemigo (2026-08-20)

`EnemyType::LeprosoEnloquecido` viene del GDD, pero **contradice el texto bíblico** y hay
que cambiarlo:

- En 2 Reyes 5 Eliseo **sana** a Naamán el leproso; no pelea con él.
- En 2 Reyes 7:3-10 los cuatro leprosos a la puerta de Samaria son los que **descubren el
  campamento sirio abandonado y traen la buena noticia** a la ciudad. Son los héroes del
  capítulo 10, no carne de cañón.
- La lepra en estos libros es castigo (Guiezí, 2 Reyes 5:27) o prueba a sanar, nunca una
  turba enloquecida que se lanza contra el profeta y explota.

Qué hay que tocar cuando se arregle (el comportamiento en sí está bien, es el **quién**
lo que está mal): renombrar el `EnemyType` y su `EnemyDef` en `game/enemies.h`, la hoja
`assets/sprites/leproso.png` (y su receta en `tools/blender/mantle_sprites/`), la entrada
del bestiario en `game/lore.h`, y los `summon()` del patrón `LaMuerte` (cap. 8) y de
`ChapterDef` del cap. 8 en `game/rooms.h`. El `AIBehavior::ExplodeOnDeath` y toda la
maquinaria de la mecha (`death_fuse` / `update_death_fuses`) se conservan tal cual: solo
cambia el enemigo que la lleva. Candidatos que sí encajan con el Acto II: un idólatra de
Rimón (2 Reyes 5:18), una vasija/ídolo de Baal que revienta al caer, o un merodeador sirio
incendiario del asedio de Samaria. Los leprosos, si aparecen, deberían ser **NPC a los que
sanar** (encuentro de `game/events.h`), no enemigos.

También pendiente de ese arreglo: Guiezí es el jefe del cap. 9 con el nombre "Guiezí el
Leproso" — ahí sí es correcto (su lepra es el castigo por la codicia), pero conviene que
el texto deje claro que es la codicia lo que se combate, no la enfermedad.

## Platform Macros

Set by CMake, available in C++:
- `MANTLE_PLATFORM_DESKTOP` — Windows/Linux/Mac
- `MANTLE_PLATFORM_WEB` — Emscripten
- `MANTLE_PLATFORM_ANDROID` — Android
- `MANTLE_PLATFORM_IOS` — iOS
- `MANTLE_PLATFORM_MOBILE` — Android OR iOS
- `MANTLE_STEAM` — Steamworks SDK present
