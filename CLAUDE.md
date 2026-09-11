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

Detalle del icono, el logo y su empaquetado: `tools/branding/CLAUDE.md`.

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

Una linea por directorio; el detalle de cada uno vive en su propio `CLAUDE.md`.

```
src/main.cpp        - Entry point (minimal bootstrap)
src/core/           - types.h, config.h (namespace Config), engine.h/cpp (bucle + escenas), ecs.h/cpp
src/render/         - renderer.h/cpp (320x180 -> 1280x720), sprites.h/cpp, camera.h/cpp
src/input/          - input.h/cpp (teclado / raton / mando / tactil, acciones)
src/audio/          - audio.h/cpp (SDL3_mixer, API MIX_*)
src/game/           - player, faith, miracles, enemies, items, rooms, combat, events, lore
src/ui/             - hud, menus, codex, cinematic
src/systems/        - save (v9), progression, achievements
src/platform/       - platform.h (deteccion de plataforma, rutas de assets y guardado)
assets/             - audio/ sprites/ fonts/ branding/ + CREDITS.md    -> assets/CLAUDE.md
tools/              - blender/ audio/ branding/                        -> tools/CLAUDE.md
scripts/            - guiones de autoplay (--script)                   -> scripts/CLAUDE.md
docs/               - temas largos, se leen bajo demanda
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
| Rooms | `game/rooms.h` | `Room`, `FloorMap` | tiles de 16 unidades; **zona continua** por capitulo (`RoomType::Zone`, celdas de 40x22 cosidas hasta 160x88; arena del jefe dentro de la celda B); interior 20x11 |
| Save | `systems/save.h` | `SaveSystem` | Binary format, version 1 |
| Progression | `systems/progression.h` | `ProgressionSystem` | 8 meta-upgrades |
| Achievements | `systems/achievements.h` | `AchievementSystem` | 18 logros, bitmask en el guardado |

### ECS Components

Entity is a `u32` index. Components tracked via `std::bitset`. Fixed arrays of size `MAX_ENTITIES = 512`:

`TransformComponent`, `SpriteComponent`, `AnimationComponent`, `HealthComponent`, `CombatComponent`, `MovementComponent`, `ColliderComponent`, `FaithComponent` (unused), `MiracleUserComponent` (unused), `EnemyAIComponent`, `ProjectileComponent`, `ItemComponent`, `LifetimeComponent`

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

Las tablas del v1.0 que el codigo aun refleja (pitch, pilares, capitulos, personajes, fe,
milagros, enemigos, items, movil) estan en **`docs/gdd-v1-implementado.md`**: valen como
referencia de lo implementado, no como plan. Si el v1.0 y `GDD2.0.md` se contradicen, manda
`GDD2.0.md`.

## Platform Macros

Set by CMake, available in C++:
- `MANTLE_PLATFORM_DESKTOP` — Windows/Linux/Mac
- `MANTLE_PLATFORM_WEB` — Emscripten
- `MANTLE_PLATFORM_ANDROID` — Android
- `MANTLE_PLATFORM_IOS` — iOS
- `MANTLE_PLATFORM_MOBILE` — Android OR iOS
- `MANTLE_STEAM` — Steamworks SDK present

## Mapa de la memoria

Cada capa tiene su `CLAUDE.md` con sus reglas. Claude Code los carga solo al tocar ficheros
de ese directorio; **si vas a trabajar en una capa, leelo antes de editar.**

| Directorio | Que cubre su CLAUDE.md |
|---|---|
| `src/` | Reglas transversales del motor: reciclado de entidades, faccion (`is_hostile`), `Scripture::ref` |
| `src/core/` | Escenas, `Config::Feel` (tacto), i18n, discipulos en el ECS, `room_fade` |
| `src/game/` | Mapas fijos, estado del gameplay, objetos y reliquias, el Manto, fidelidad biblica |
| `src/ui/` | Codice, HUD, caja de dialogo, pantalla de Opciones (filas por id) |
| `src/render/` | Unidades de juego, escalado, look, orientacion vertical en movil |
| `src/systems/` | Guardado y migraciones, logros, meta-progreso, cosmeticos y monetizacion |
| `src/input/` | Controles (PC / mando / movil), auto-mira, joysticks tactiles |
| `src/audio/` | API `MIX_*`, variantes de SFX, ducking, voz (`play_voice`) |
| `assets/` | Que hay en cada carpeta, la regla de que `assets/` no crece, CREDITS.md |
| `scripts/` | Comandos de autoplay, los guiones de verificacion y sus gates |
| `tools/` | Indice de los tres pipelines (Blender, audio, branding) |

Temas largos, bajo demanda (los enlaza la capa que los necesita):
`docs/mundo-exterior.md`, `docs/secretos-e-interiores.md`, `docs/encuentros-y-lore.md`,
`docs/cinematicas.md`, `docs/ascenso-y-epilogo.md`, `docs/efectos-y-paleta.md`,
`docs/gdd-v1-implementado.md`
