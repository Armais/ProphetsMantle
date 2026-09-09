# Efectos, retratos y paleta por capitulo

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`combat.h`: `spawn_fx` /
`spawn_vfx`; `enemies.h`: el tinte por capitulo). Como se generan las hojas:
`tools/blender/CLAUDE.md`.

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

## Los diez milagros: lo que los distingue es el efecto

- **Miracle VFX**: the sheets have a single `CastMiracle` row, so what tells the ten powers apart is the on-screen effect, not the pose. **Para efectos nuevos usa `spawn_fx` (hoja propia animada), no `spawn_vfx`**; este último se queda para los iconos sueltos del tileset y como respaldo. `spawn_vfx` (`game/combat.h`) drops a tileset icon that fades/grows/spins via `LifetimeComponent` (`total`/`fade`/`grow`/`spin`, applied in `system_lifetime`); `vfx_ring` / `vfx_column` / `vfx_pillar` in `player.cpp` compose it. Only use loose ICONS (Star/Orb/Fireball/Lightbolt…): terrain cells like `WaterA` are opaque squares and look like blocks. Lasting miracles (aura of Doble Porcion, bubbles of Sanar Aguas, trail of Carro de Fuego) emit from `Player::update` on `aura_timer`/`chariot_timer`. Altar Consumido raises `Player::flash_request` → `Engine::world_flash`, a white veil over the world under the HUD.
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
