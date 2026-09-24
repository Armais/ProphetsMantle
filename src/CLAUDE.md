# `src/` - reglas transversales del motor

Valen para CUALQUIER capa. Lo especifico de cada una esta en su propio `CLAUDE.md`:
`core/`, `render/`, `input/`, `audio/`, `game/`, `ui/`, `systems/`.
(`platform/` es un solo fichero y lo cubre "Platform Macros" en el `CLAUDE.md` de la raiz.)

## Faccion: `EnemyAI` no significa "enemigo"

Un **discípulo** lleva `EnemyAIComponent` como cualquier otro combatiente pero con
**`ally = true`**: es la única marca de facción del juego, y no gasta ni un componente nuevo del
presupuesto del ECS. La regla que hay que respetar al tocar cualquier bucle:

> **`world.has(e, Component::EnemyAI)` NO significa "es un enemigo".** Para eso está
> `is_hostile(world, e)` (y su pareja `is_ally`), en `core/ecs.h`.

El sistema completo de Discipulos esta en `src/core/CLAUDE.md`.

## Reciclado de entidades

- **Entity recycling**: `ECSWorld::create()` calls `reset_components()` — ids come back through the free list, and stale component data (a projectile's `hit_list` still naming the boss) silently broke damage. Never assume a spawner clears every field it does not set.

## `npcs.png` NO es una hoja de animacion

Las doce hojas de personaje son 7x15 fotogramas (una fila por `AnimID`) y `system_animation`
las recorre. **`npcs.png` no**: es UNA fila de 16 personajes DISTINTOS (2048x128), y avanzar
su `current_frame` no anima a nadie — cambia de persona.

Por eso existe **`SpriteComponent::fixed_src`** (`core/ecs.h`): con el puesto,
`system_animation` deja el `src_rect` como lo horneo quien creo la entidad. Lo pone
`spawn_npc_figure`. La invariante que se rompio (2026-09-15): se daba por hecho que
`system_animation` "no corre en Dialogue", y desde que las aperturas tienen paseo cinematico
—que llama a `world.update`— si corre. La viuda de Sarepta cambiaba de personaje cada 0,15 s
delante del jugador. `spawn_figure` (Acab, Jezabel, enemigos) **no** lleva la marca: esas si
son hojas completas y que se les anime el idle durante el paseo es lo correcto.

## Sin referencias biblicas en pantalla

- **Sin referencias biblicas en pantalla**: el texto de cada cita se muestra tal cual, pero `Scripture::ref` ("1 Reyes 17:4") NO se pinta en ningun sitio — ni historia, ni cartas de eleccion, ni encuentros, ni Codice. El campo sigue en `game/lore.h` solo como atribucion documental. Al anadir texto nuevo, no metas la referencia en el `text`.

## Diagnostico

`SDL_Log()` para toda la salida de diagnostico: es lo unico que un guion de autoplay puede
leer como gate (ver `scripts/CLAUDE.md`).
