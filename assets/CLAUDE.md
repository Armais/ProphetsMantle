# `assets/` - lo que se distribuye con el juego

Quien GENERA todo esto es `tools/` (ver `tools/CLAUDE.md`): ni un asset se edita a mano.

```
assets/
  audio/music/          - 10 loops sintetizados (.ogg Vorbis, tools/audio/gen_audio.py; el loader cae .ogg -> .wav)
  audio/sfx/            - 37 SFX sintetizados (.ogg, mismo generador; incluye variantes _2/_3 por sonido)
  sprites/cine/         - Prologue panels: 8 escenas x 3 capas (1600x720 PNG con alfa, ver ui/cinematic.h)
  sprites/vfx.png       - Hoja de efectos propia: 8 fotogramas x 7 filas, celda 128 px (ver spawn_fx)
  sprites/portraits.png - 44 celdas, 6x8, celda 192 px (Bestiario, locutores del pozo y los nueve jefes)
  sprites/icons.png     - 32 iconos: 22 objetos (celda = ItemDef::id) + 10 milagros (22 + MiracleID), 8x4 de 128 px (render/sprites.h, Icon)
  sprites/estampas/     - 46 estampas de relato, ev_NN.png = id de Ev:: (768x384, tools/estampas), una viva a la vez
  sprites/boss_*.png    - 9 hojas de jefe (mismo formato que las de personaje, tools/blender/mantle_sprites/bosses.py)
  sprites/chariot.png   - el Carro de Fuego del milagro: filas este/sur/norte, columnas 0-3 capa back y 4-7 capa front (Engine::collect_chariot_draws)
  sprites/              - Character sheets: 12 PNG (896x1920, 7 cols x 15 rows of 128x128 = 32 game units
                          x ASSET_SCALE 4, rows = AnimID, smooth alpha + 2px dark outline)
                          + tiles.png (terrain tileset, 64px cells = 16 units x4, 8 cols x 9 rows: rows 0-4 terrain/props/
                          pickups, row 5 floor families grass_a-d + stone_a-d, row 6 ground decals, row 7 shore
                          autotiling pieces + cliff_rock + jar + crack, rows 7-8 cliff_rock_tall + door; see TileSprite in game/rooms.h)
  fonts/                - LilitaOne-Regular.ttf (OFL) baked into glyph atlases at init (fallback: SDL debug text)
  CREDITS.md            - Procedencia y licencia de TODO asset (se actualiza al meterlo, no despues)
  branding/             - mantle.ico (lo empotra mantle.rc) y mark_256.png (SDL_SetWindowIcon)
```

## Regla: `assets/` no crece si se puede evitar

Es lo que decide varias cosas del codigo: la paleta por capitulo es un **tinte** y no ocho
PNG mas por variante (`docs/efectos-y-paleta.md`), una skin de cayado es un **tinte** y no
una hoja nueva (`src/systems/CLAUDE.md`), y el audio se guarda en `.ogg` y no en `.wav`
(48 MB -> 24 MB, `tools/audio/CLAUDE.md`).

- No Steamworks (el usuario lo aparca en el backlog como "integracion steam"). El audio es
  sintesis propia (`.ogg`, ver seccion "Audio"); **no hay ni un asset de terceros salvo la
  tipografia Lilita One (OFL)** — procedencia completa en `assets/CREDITS.md`.

**`CREDITS.md` se actualiza al meter el asset, no despues.**

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
