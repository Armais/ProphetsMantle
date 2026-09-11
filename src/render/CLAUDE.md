# `src/render/` - renderer, escalado y look

Reglas transversales a todo `src/`: `../CLAUDE.md`.

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

### Orientacion VERTICAL en movil — DORMIDA (GDD 4.5, 2026-08-25)

> **Horizontal forzado desde 2026-09-11.** En movil `Engine::init` pide
> `SDL_HINT_ORIENTATIONS = "LandscapeLeft LandscapeRight"` y el manifest de Android nace con
> `screenOrientation="sensorLandscape"`, asi que `bottom_band_units()` devuelve 0 y los
> botones nunca bajan a la banda. **Nada de esta seccion se ha borrado y sigue compilado y
> verificado** (`scripts/vertical_check.txt` pasa): es el mecanismo que habria que despertar
> si algun dia vuelve el vertical, y es tambien lo que explica por que un dedo puede dar
> coordenadas FUERA de 0..320 / 0..180 sin que eso sea un error.


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

### El letterbox tambien vale para el DEDO (2026-09-11)

`Renderer::viewport_norm()` devuelve el viewport en **fracciones de ventana**, y el Engine se
lo pasa a `Input::set_touch_viewport` al crear la ventana y en cada `RESIZED`. Sin eso, los
eventos `SDL_EVENT_FINGER_*` —que SDL normaliza a la **ventana entera**, no al area de
juego— se leian ~29 unidades a la izquierda en un movil de 20:9, y la celda Q del HUD
lanzaba el milagro de la celda E. El raton ya pasaba por `window_to_game`; esto es su
equivalente para el tactil, y por eso **el hit-test sigue sin saber nada del letterbox**.

Guion: **`scripts/tactil_ancho_check.txt`** (`--touch --window 1200x540`, viewport
`{120, 0, 960, 540}`). Los comandos `touch/drag/untouch` de siempre **no** habrian cazado
esto: aplican la inversa exacta de la misma transformacion. Los nuevos
`touchpx/dragpx/untouchpx` hablan en **pixeles de ventana**, como un dedo.
