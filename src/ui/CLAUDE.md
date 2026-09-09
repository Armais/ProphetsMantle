# `src/ui/` - HUD, menus, Codice y cinematicas

Reglas transversales a todo `src/`: `../CLAUDE.md`.

> **Antes de tocar `ui/cinematic.h/cpp`, lee `docs/cinematicas.md`**: el prologo, las 10
> intros de capitulo y la cinematica del Ascenso.
> La caja de dialogo con retrato y sus limites de ancho: `docs/encuentros-y-lore.md`.

## Codice

- **El Códice** (`ui/codex.h`): cinco pestañas — Capítulos / Bestiario / **Milagros** / **Objetos** / **Logros** (ver la sección de Logros). La de milagros se llamaba "Reliquias" y listaba milagros (nombre equivocado); las reliquias son objetos y viven en la de Objetos, que lista los 16 de `ItemRegistry` (incluidos los dos `event_only`) con el color de rareza en la fila y, abajo, el efecto + el pasaje **sin referencia**. El contador "Descubierto X/Y" cuenta las cuatro pestañas y ahora cuadra con lo que se puede ver. El contador va arriba a la derecha y la ayuda de teclas abajo: en 180 unidades de alto no caben las dos bajo el panel del pasaje.

## HUD

- **Working**: HUD — minimap (TAB = big map), boss direction hint, chapter banners, messages, boss bar, miracle slots with cost/lock state, item row under the vitals (rarity-coloured frames, R marks the held consumable, número de cargas del consumible), floating damage numbers and a 0.05 s hit-stop on boss hits; audio (procedural music per scene + SFX, stereo with reverb; `Audio::load_*` falls back .ogg → .wav).
- **HUD** (`ui/hud.cpp`): framed glossy bars (`draw_bar`) with tileset icon badges (`draw_badge`, heart/orb/skull on a soft disc), miracle slots with bevel frame, cooldown sweep, key badge and orb+cost, boss bar with name plate, TTF text everywhere.
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
- La fila de **Auto-mira** y su semantica (Off / Asistido / On) esta en
  `src/input/CLAUDE.md`.

El mecanismo de i18n (`tr()`) esta en `src/core/CLAUDE.md`; los guiones que fotografian
estas pantallas en los dos idiomas, en `scripts/CLAUDE.md`.
