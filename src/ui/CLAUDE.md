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

## Pulido tras la primera prueba del APK (2026-09-11)

El usuario jugo el APK en su movil y saco cuatro fallos que ninguna verificacion sintetica
habia dado. Dos son de esta capa.

### Que se VE que los milagros se tocan

Tres senales, y las tres hacian falta porque al quitar la chapa `Q/E/F` en la pasada de
Android la celda se quedo **muda**: era un adorno del HUD y nadie adivinaba que era un boton.

1. **Latido** (`draw_miracle_slots`): la celda lanzable AHORA respira en oro — aureola por
   fuera del marco y el marco interpolando, `0,5 + 0,5*sin(t*4,2)`. A 0,7 Hz se lee como
   brillo; mas rapido seria el parpadeo que ya se descarto con el destello de impacto. **Lo
   bloqueado y lo que recarga NO laten**: un boton que late y no hace nada miente.
2. **Etiqueta** bajo la celda, en tactil, en el hueco donde iba la chapa de tecla: la
   PRIMERA palabra del milagro ("Fuego", "Cuervos", "Sanar"), que es lo que distingue una
   celda de otra. Se **mide** con `measure_text` contra `S + 4`, se prueba a 0,55 y luego a
   0,45, y solo si no cabe cae a un "MILAGRO" generico en la primera celda. `draw_text` no
   recorta: lo que no cabe se planta encima de la celda vecina.
3. `HUD::draw_message` **admite dos lineas** partidas por un salto de linea (lo pide la pista de
   milagro del Engine, que tiene que decir "toca para lanzarlo" y "manten para ver que
   hace"). Un mensaje de una sola linea sale exactamente igual que antes.

**Hallazgo de diseno: la rama `lock_cost` es codigo muerto.** "Tengo el nivel pero no me
llega la fe" no puede pasar: los tiers se abren a 10 / 25 / 50 / 75 y sus milagros cuestan
5-8 / 12-15 / 20-25 / 30, o sea que el mas caro de cada tier vale siempre menos que el
umbral que lo desbloquea. Si algun dia se retoca la tabla de costes, esa rama vuelve a la
vida sola; hasta entonces, los estados reales son tres: lanzable, en recarga y sin nivel.
Guion: **`scripts/milagro_pista_check.txt`** (`--touch`).

### El mapa pinta el TERRENO, no una rejilla de salas

"El mapa se ve cuadriculado" era literal: `draw_minimap` seguia dibujando el modelo VIEJO
—una celda por sala, con separacion— cuando desde `add8a30` un capitulo es una **zona
continua** de hasta 160x88 tiles. El mapa describia una estructura que ya no existe.

- **`draw_zone_map`** (estatica, en `hud.cpp`) pinta el mapa grande tile a tile por
  **familias**: suelo / camino / agua / agua honda / solido / hueco / puerta, mas un color
  propio para lo que sale una vez (cofre, altar de Dios, altar de Baal, relato, arco de
  salida). Veinticinco tipos con veinticinco colores a 1,4 unidades por tile es ruido.
- **Se cosen los tramos horizontales del mismo color**: 160x88 son 14.080 tiles y uno a uno
  serian 14.080 rects por frame con el mapa abierto. El terreno viene en manchas, asi que
  el dibujo sale identico con unos pocos cientos.
- **La escala se DIVIDE, no se clava**: `min(AREA_W/tiles_w, AREA_H/tiles_h)`. Los capitulos
  5 y 10 tienen una fila de celdas mas (4x4) y con una constante se salian por abajo.
- **Niebla por celda** (`cells[i].visited`): lo pisado a todo color, lo entrevisto —
  adyacente, jefe, inicio, u **Ojos Abiertos** — al 42%, lo demas negro. Es lo que hace que
  el mapa cuente una exploracion en vez de ensenarlo todo.
- **El punto del profeta va en su tile de verdad** (`HUDContext::player_pos`), no en una
  casilla encendida. Dentro de un interior `player_in_zone` es false y se marca el centro de
  su celda de FUERA: la posicion del interior no significa nada en el mapa del capitulo.
- **Dos colisiones de sitio que hubo que resolver mirando, no razonando**: el mapa se dibuja
  **el ultimo** de `HUD::render` (antes el panel de vitales le cortaba la esquina y un
  mensaje se le plantaba encima), empieza en `y = 31` (justo bajo ese panel, que llega a
  30,75) y **el objetivo del capitulo bajo al pie**, porque arriba se montaba sobre el
  porcentaje de fe.
- **El minimapa pequeno se queda en rejilla** —a 10x7 unidades por celda el terreno no dice
  nada— pero **sin separacion entre celdas**: los huecos eran los que lo contaban como salas
  sueltas. Su rect exterior no se mueve ni una unidad a proposito, porque lo comparte
  `Input::touch_minimap` (`HUD::minimap_rect`): las celdas se ensanchan hasta llenarlo.

Guion: **`scripts/mapa_check.txt`**.
