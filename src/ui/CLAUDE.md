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
- **El minimapa pequeno tambien pinta terreno** (2026-09-12). Quedarse en rejilla fue un
  error: el usuario volvio a decir "sigue con el mismo estilo de celdas" mirando **esa**
  esquina. Ahora los dos mapas salen de `draw_zone_canvas`, asi que no hay forma de que uno
  vuelva a contar algo distinto del otro. Su rect exterior no se mueve ni una unidad a
  proposito, porque lo comparte `Input::touch_minimap` (`HUD::minimap_rect`).

### El mapa se PINTA en una textura (2026-09-12)

Pintar el terreno tile a tile seguia siendo un mosaico: 1,3 unidades por tile, trece colores
planos y la niebla recortada en rectangulos de celda. El usuario lo llamo "pixel art" y tenia
razon. El mapa ahora es **una textura generada por codigo** (nada nuevo en `assets/`, que es
la regla):

- **Mascaras suavizadas + muestreo bilineal** (`zone_terrain_texture`, 5 px por tile): una
  mascara binaria de solido / agua / camino / hueco, desenfocada a resolucion de tile, es un
  campo continuo; umbralizarlo da siluetas curvas en vez de escalones. Del **gradiente** de
  ese campo sale la luz (desde arriba-izquierda) y de su **filo**, el contorno de tinta.
- **Terreno y niebla van en texturas separadas, y es lo que evita el tiron**: el terreno solo
  depende del capitulo, asi que se genera UNA vez; la niebla cambia al pisar cada celda y es
  una textura de 8 px por celda que se estira con filtrado lineal (`zone_fog_texture`).
  Repintar medio megapixel al cruzar cada celda se nota; repintar 32x32 px, no.
- **Tres cosas se corrigieron mirando la captura, no razonando**: los **caminos** unen centros
  de celda y pintados con fuerza trazaban una reticula rectangular sobre el mapa entero (van
  al 48%); el **filo de la niebla** nacia siendo una recta por celda (se mueve con ruido, y
  solo el filo); y el rotulo del **JEFE** cuelga bajo su chincheta y no sobre el centro de su
  celda, que en el capitulo 5 lo dejaba detras del pin.

Guion: **`scripts/mapa_check.txt`**.

### Botones de menu (2026-09-12)

Una opcion de menu era una linea de texto centrada con `> ` delante. El usuario, mirando el
APK: "desde el punto de vista de un juego movil es horrible, no se distingue a que estas
haciendo clic". Ahora cada opcion es una pieza con forma: `Renderer::draw_panel` sirve un
rectangulo redondeado con bisel en 9 trozos (`Renderer::panel_tex`, generado en `init` como
`shadow_tex` y `vignette_tex`), y lo usan el titulo, la pausa, la derrota y las filas de
Opciones — y tambien el marco del mapa.

- La elegida cambia de **color, de tamano y lleva aro de foco**. Solo el tono no basta: a
  plena luz en un telefono un cambio de tono no se ve.
- Su texto va **claro**, no oscuro. `draw_text` siempre pone contorno oscuro al glifo, asi que
  letra oscura sobre dorado sale como un borron (se vio en la primera captura).
- La **zona tactil no crece con la seleccion** (`menu_hit_rect`): si creciera, el sitio donde
  hay que tocar se moveria bajo el dedo. `mark_row_rect` apunta el rect que se ha dibujado,
  para que el hit-test siga saliendo de la misma formula que el dibujo.

## Pulido tras la segunda prueba del APK (2026-09-12)

Otra vez cinco fallos que ninguna verificacion sintetica habia dado, y otra vez tres se
cerraron **mirando la captura**, no razonando. Tres son de esta capa.

### El boton de SALTAR y el aviso que faltaba

`skip_button_rect()` / `draw_skip_button()` (`hud.cpp`, junto a `draw_dialogue`) son **una
sola pieza compartida** por las tres pantallas que la necesitan: la cinematica
(`CinematicSystem::render`), la carta de historia (`StorySystem::render`) y la caja de la
escena guionizada (`draw_dialogue`). Panel de 9 trozos como los botones de menu, rotulo
`SALTAR >>` — **no `▶▶`: el atlas de glifos no lo tiene** (`GLYPH_EXTRA` en `renderer.h`) y
saldria un hueco. El resto (quien lo atiende, por que emite `Cancel`, el tramo a tramo) esta
en `src/input/CLAUDE.md`.

`DialogueView::skippable` lo rellena el Engine con `scene_step >= 0`. Sin el, un encuentro del
pozo pintaria un boton que no hace nada. Y con `skippable` **en PC** el pie pasa de
`[ENTER] seguir` a `[ENTER] seguir    [ESC] saltar`: es el unico cambio de texto en PC de toda
la pasada, y arregla que la apertura de capitulo se pudiera saltar desde siempre sin que
nadie lo supiera.

### Los botones de menu se MIDEN

`BTN_W = 148` dejo de existir. 148 unidades sobre 320 de pantalla eran una pila de pildoras de
media pantalla dijera lo que dijera el texto ("Reanudar" igual de ancha que "Reintentar
capitulo"), y la fila de la partida caducada —`"Partida abandonada  +12345"`— **se desbordaba
y nadie lo habia visto**. Ahora `menu_button_width` mide con `Renderer::measure_text` (mismo
precedente que las pestanas del Codice y la placa del mapa) y devuelve el ancho del texto **mas
largo de la pantalla** + 22 de relleno, con suelo 72 y techo `GAME_WIDTH - 24`.

- **Un ancho por PANTALLA, no por fila**: uno por fila haria una escalera.
- La etiqueta de la partida caducada **se compone antes del bucle de medida**, o el boton mas
  ancho seria justo el que se sale.
- `BTN_GROW` y el margen tactil se conservan: la **zona del dedo sigue sin moverse con la
  seleccion** (`menu_hit_rect`), que era el punto de la pasada anterior.
- Afecta a Titulo, Pausa y Derrota, que comparten `draw_menu_button`. Opciones no: sus filas
  no son botones.

### La niebla del mapa es LUZ Y sombra, no solo velo

"Lo ya descubierto se ve oscuro" era literal y eran dos cosas sumadas en `zone_fog_texture`:

1. **El filo mordia hacia dentro.** El ruido del borde era simetrico (`c + n*0.85`, con
   `n ∈ [-0.5, 0.5]`), asi que sobre una celda pisada (`c = 0`) levantaba hasta 0,42 de
   oscuridad DENTRO de lo descubierto, y los dos desenfoques lo arrastraban ~2 px mas. Con
   8 px por celda eso es casi medio lado: la celda salia oscura salvo en el centro. Ahora el
   ruido va **solo hacia fuera** (`max(0, n)`) y antes de perturbarlo se **dilata lo claro**
   (minimo sobre vecindad de 2 px), de modo que la frontera nace fuera de la celda y son el
   ruido y el blur los que se comen ESE margen.
2. **"Descubierto" solo significaba "sin velo"**, y el terreno base ya es pardo apagado. La
   textura pasa a ser una capa de luz y sombra: con `f < 0,5` un **realce calido**
   (`rgb(250,238,210)`, alfa hasta 46) y con `f > 0,5` el velo oscuro de siempre hasta 252. Lo
   entrevisto se recalibro a 0,78 para quedar claramente entre los dos.

Sigue siendo **una textura y un draw**, la cache por firma no cambia, y como el mapa grande y
el minimapa salen los dos de `draw_zone_canvas`, el arreglo cae en los dos. Medido sobre la
captura: perfil horizontal plano de borde a borde de la celda pisada (~130 de brillo), celda
visitada 107 / adyacente 65 / nunca vista 22 en el centro de cada una.
