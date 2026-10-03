# `src/ui/` - HUD, menus, Codice y cinematicas

Reglas transversales a todo `src/`: `../CLAUDE.md`.

> **Antes de tocar `ui/cinematic.h/cpp`, lee `docs/cinematicas.md`**: el prologo, las 10
> intros de capitulo y la cinematica del Ascenso.
> La caja de dialogo con retrato y sus limites de ancho: `docs/encuentros-y-lore.md`. Las
> etiquetas de consecuencia de cada opcion (`EffectChip`, `DialogueView::chips`) salen de
> `get_event_effect` via `Engine::build_event_chips`: nunca se escriben a mano en la vista.

## Caja de encuentro con estampa (2026-10-03)

`draw_dialogue` con `DialogueView::plate` pinta la lamina del relato enmarcada arriba (marco
dorado `draw_gold_frame`, cinta del titulo `draw_title_ribbon`) y la caja de texto debajo
(`draw_text_box`). Todo con `draw_panel`/`draw_disc`: ni un PNG para el marco. **La geometria
del texto es la de siempre a proposito** (cero `TEXT TRUNCATED`); el detalle de por que y de
la carga de la lamina esta en `docs/encuentros-y-lore.md`.

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
  5 y 10 tienen una fila de celdas mas (6x5 desde el 2026-09-24) y con una constante se
  salian por abajo.
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

### Puntos por explorar, jefe oculto e iconos propios (2026-09-24)

Tres peticiones del usuario de una tacada: *"en el minimapa deben aparecer tambien los puntos
que se deben explorar"*, *"la ubicacion del jefe ... no debe ser visible desde el inicio"* y
*"cada objeto y milagro debe tener su propio icono"*.

- **Puntos de interes** (`zone_pois` en `hud.cpp`): cofre, altar, relato, elite y la puerta
  del interior, cada uno EN SU TILE (`CellState::poi_tx/ty`, que apunta `place_special` al
  generar; la elite va en el nudo). Tres estados, los mismos en los dos mapas: celda sin
  descubrir = **"?"** en el centro de la celda (dice QUE hay algo, no QUE); celda vista =
  chincheta de su color (en el mapa grande, con su icono del tileset dentro); hecho = la
  chincheta apagada y pequena. La celda del jefe no lleva "?".
- **El jefe segun lo que se sabe** (`get_boss_intel`, `game/rooms.h`): nivel 0 sin nada y
  "Jefe: ???"; nivel 1 el RUMOR (`boss_rumor_circle`: anillo rojo discontinuo con su "?", NO
  centrado en la arena) y "Jefe: ~NO" hacia el centro del anillo; nivel 2 la chincheta roja
  de siempre y "Jefe: 2N 1E". `zone_visibility` ya no entrevee la celda del jefe de serie.
  La linea de ayuda del mapa grande cambia con el nivel.
- **Contadores** arriba a la derecha del mapa grande: "Explorado N/M · Hallazgos N/M ·
  Pistas N/2" (fuera del panel de vitales, que acaba en x = 126).
- **El minimapa crece a 12x7 unidades por celda** (`HUD::minimap_rect`, sin separacion):
  6 celdas son 72 de ancho y la fila de objetos (hasta x = 223) no llega. `draw_objective`
  se cuelga del rect, no de `grid_h * 8`.
- **Iconos propios** (`render/sprites.h`, `namespace Icon`; hoja `assets/sprites/icons.png`,
  `tools/blender/mantle_sprites/icons.py`): `set_icon_sheet` la fija el Engine al cargar y
  `draw_icon(renderer, idx, dst)` devuelve false sin hoja, para que cada sitio caiga a su
  icono generico del tileset. La usan la fila de objetos, la celda del arma, las celdas de
  milagro, los botones tactiles USAR / ARMA, las cartas de eleccion (`ChoiceCard::icon_idx`,
  a 25 unidades), la ficha del Codice (Objetos y Milagros, en el hueco del retrato) y El
  Santuario, que desde el 2026-10-03 tiene celdas PROPIAS (`Icon::SANCT_*`, 32..50).

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
  plena luz en un telefono un cambio de tono no se ve. **Ojo: esto vale en PC.** En movil no
  hay cursor desde el 2026-09-12 y ese mismo aspecto lo lleva el boton que esta bajo el dedo
  (ver "En movil no hay cursor", mas abajo).
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
precedente que las pestanas del Codice y la placa del mapa) y devuelve el ancho de **SU**
texto + 14 de relleno, con techo `GAME_WIDTH - 24`.

- **Un ancho por FILA en columna** (medir el texto mas largo de la pantalla dejaba "Salir"
  con media pildora de aire a cada lado solo porque "Nueva Partida" estaba tres filas mas
  arriba). En la **rejilla** del titulo es al reves y hay uno solo para las siete: ahi el
  ancho lo manda la columna y uno por fila haria una escalera dentro de ella.
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

## En movil no hay cursor (tercera prueba del APK, 2026-09-12)

El usuario, con el APK en la mano: *"no tiene sentido en la version movil que el boton
seleccionado salga de un color, ya que en movil no es como en ordenador que vas cambiando
sino que directamente tocas el boton que quieres"*. Es la regla de la capa, y vale para
**todas** las pantallas de lista: titulo, pausa, derrota, selector de idioma, Opciones, El
Santuario y El Codice.

**Cursor y ESTADO no son lo mismo, y solo se va el cursor.** El cursor dice "aqui esta la
flecha", que en un telefono no significa nada. El estado dice algo del contenido y se queda
siempre: la **pestaña activa** del Codice, el **COMPRADO** verde y el coste en oro o rojo de
El Santuario, el color de rareza de un objeto, el `[*]` de un logro y el valor entre
corchetes de cada fila de Opciones.

**Lo que sustituye al resaltado es la pulsacion**: lo que hay bajo el dedo se ilumina
mientras el dedo sigue apoyado (`MenuSystem::press_active/press_pos`, `CodexScreen` igual,
escritos por `Engine::render` desde `Input::ui_press`). Sacar el dedo del boton lo apaga y
cancela el toque, que es la conducta de cualquier movil. **No crece**: con `sel = -1`
`menu_button_rect` no aplica `BTN_GROW`, o sea que el sitio donde tocar no se mueve.

**Arrastrar mueve la VENTANA, no el cursor.** Sin esto, quitar el resaltado dejaba un cursor
invisible que el arrastre paseaba a ciegas: el jugador arrastra para ver mas filas y de paso
cambia cual se va a ajustar o que ficha se lee. Vale en Opciones, Codice y Santuario. En PC
no cambia: W/S mueven el cursor y la ventana lo sigue.

### El titulo, en rejilla de dos columnas

Siete opciones en una columna dan un paso de 10,9 unidades = **4 mm** de dedo en un movil de
6,5" (Android pide 9). Y en apaisado sobraba todo el ancho. En tactil el titulo pasa a
**2 columnas x 4 filas** (`menu_grid_hit` / `menu_grid_button`), celda de 92x25 = **34x9,2
mm**, orden **por columnas** — izq. Nueva Partida / Continuar / El Santuario / El Codice,
dcha. Prologo / Opciones / Controles — de modo que el mapeo indice -> accion de
`Engine::update` no se toca ni una linea. Las **ofrendas** ocupan el hueco de la septima
celda, sin panel, para que no se lean como un octavo boton. Fuera en tactil: el lema largo
(cae justo donde empieza la rejilla) y el pie "Toca una opcion", que explica lo obvio en
cuanto cada opcion tiene el tamaño de un dedo.

**Las dos columnas cuelgan del CENTRO, no de los bordes** (`GRID_GAP` = 12 de canal), y
dentro de su celda el boton se arrima al centro en vez de centrarse: la izquierda por su
filo derecho y la derecha por el izquierdo. Con las celdas pegadas al borde pasaban las dos
cosas de las que se quejo el usuario a la vez — los botones se echaban sobre la cara de los
profetas, que en esta pantalla flanquean el fondo, y entre columna y columna quedaban 55
unidades de aire muerto justo en el medio. Ahora el bloque entero vive en la franja libre
entre los dos bustos (**x 72..245**) y el unico hueco es el canal. La **zona del dedo sigue
siendo la celda entera**, tambien por fuera del boton, asi que arrimarlo no cuesta area
tocable.

**Un ancho de boton para las siete, no uno por fila**: en rejilla, medir cada rotulo hace
una escalera dentro de la columna. Y como el ancho lo manda la columna y no el texto,
`draw_menu_button` **baja la escala** del rotulo que no quepa (`draw_text` no recorta): la
etiqueta de la partida caducada es la candidata natural a desbordarse.

### El resto de repartos en tactil

| Pantalla | PC | Tactil |
|---|---|---|
| Idioma | 94 / 17 / 13 | 92 / **28** / 22 |
| Pausa | 72 / 15 / 12 (5 filas desde el 2026-10-03) | 50 / **24** / 19, titulo a y=22 |
| Derrota | 121 / 13,5 / 11 (3 filas + desglose encima) | 117 / **20,5** / 17 |
| Opciones | 52 / 15, placa 14 | 48 / **21**, placa **19**, ventana de 6 |
| Santuario | paso 16, 7 filas + detalle (2026-10-03) | paso **20**, **5** filas + detalle |
| Codice | paso 10, 8 filas | paso **15**, **5** filas, pestañas de 16 |
| Pregunta del tutorial | 94 / 17 / 13 | 92 / **26** / 20, top **78** |

La **zona del dedo** en tactil ya no sale del texto de cada fila (`menu_hit_rect_wide`, de
x=16 a 304): "Salir" tiene que ser tan facil de tocar como "Reintentar capitulo". Alto =
paso completo, asi que no queda ni una franja muerta entre dos botones.

**Opciones tenia un desbordamiento de antes**: con 9 filas (stick derecho + cayado
desbloqueado) la ultima caia en y=172 y su placa llegaba a 182,8, fuera de la pantalla y
encima de la ayuda. La ventana de 6 lo cierra de paso.

**El panel del Codice no decia de que entrada hablaba.** En cuatro de las cinco pestañas no
aparecia el nombre por ningun lado (Capitulos pinta "Relatos X/Y", Bestiario y Milagros
nada, Objetos solo la descripcion; solo Logros lo llevaba), y sin fila resaltada la pantalla
se quedaba muda. En tactil el nombre encabeza el panel; en PC no, porque lo dice el cursor y
`wrap_text` trunca en silencio lo que no cabe.

**El Santuario duplicaba a mano los rects que pinta** (y=48, paso 14,5, "Volver" en 150):
era el unico sitio del juego que rompia la regla "el rect sale de la misma formula que
pinta", y en cuanto cambio el paso el hit-test habria apuntado a filas que no estan ahi, en
silencio. Ahora `render_sanctuary` apunta `Engine::sanctuary_row_rects` y `update_sanctuary`
los lee.

Logs de gate: **`Menu tap: pantalla=P fila=F`** y **`Santuario tap: fila=F`** (`-1` = ninguna).
Un toque que fallaba era mudo, y por eso un guion con las coordenadas podridas pasaba sin
que saltara nada.

### El boton de SALTAR parecia no funcionar (2026-09-12)

El usuario, con el APK: *"al inicio del capitulo 1, cuando le das a saltar, hay un delay y
parece que el boton falla al tocarlo"*. No fallaba: eran dos cosas sumadas, y las dos se
vieron mirando las capturas.

1. **El capitulo 1 arranca con DOS pantallas seguidas** — la audiencia con Acab
   (`Scene::AUDIENCIA`, escena guionizada) y, al cerrarse, la intro cinematica del capitulo.
   Como el salto es **tramo a tramo** (decision del usuario), el primer toque llevaba a otra
   pantalla estatica con el mismo boton de SALTAR **en el mismo pixel**: indistinguible de
   "no ha pasado nada". Encima la intro entra con su propio fundido de 0,65 s.
2. **El boton no daba ninguna señal al tocarlo**, y el tap se resuelve al SOLTAR el dedo.

Arreglos, sin tocar la regla de tramo a tramo (el usuario la mantuvo):

- **`draw_skip_button` recibe `pressed`** y se enciende en dorado mientras el dedo esta
  encima, igual que los botones de menu. El dato sale de `Input::ui_press` y lo reparte
  `Engine::render` a los tres sitios que comparten el boton: `CinematicSystem::skip_pressed`,
  `StorySystem::skip_pressed` y `DialogueView::skip_pressed`.
- **`Engine::scene_fade`**: velo negro de 0,22 s al saltar un tramo. Es un fundido de
  ENTRADA, no de salida — el tramo cambia en el acto y lo que se aclara es el nuevo — asi que
  hace visible el salto sin añadir ni un milisegundo de espera. Se dibuja **el ultimo** de
  `render`, por encima de los botones. Lo arman los tres caminos de salto: `finish_scene(true)`
  para la escena guionizada, y `CinematicSystem::skipped` / `StorySystem::skipped`, que
  distinguen "me han saltado" de "he terminado solo".

**Lo que este arreglo NO ha verificado**: el aparato. Se comprobo en PC con `--touch` y por
captura; en el telefono no habia dispositivo conectado por adb. Para probar de verdad en
Android hace falta el movil por USB — `adb shell input tap` genera toques del sistema, con el
letterbox y el DPI reales, y `adb logcat -s SDL/APP:V` enseña las lineas de gate.

## El tutorial: sus dos pantallas y su cartel (2026-09-12)

El sistema entero vive en `game/tutorial.h` (ahí están las decisiones de diseño). De esta
capa son tres piezas:

- **`MenuScreen::TutorialAsk`** — "¿Quieres jugar el tutorial?", dos botones, al pulsar Nueva
  Partida. Mismo fondo que el título (se viene de ahí y no hay por qué cortar la estampa).
  Los pies van a y=132/143 en PC y a **131/139 en táctil**, y **no más abajo**: el orillo del
  manto de la estampa cruza a partir de 146 y el texto apagado se perdía encima.
  **En táctil el presupuesto vertical es duro por los dos extremos** (2026-09-13). Con el
  reparto de antes (`{92, 28, 22}`) el segundo botón ocupaba y 120..142 y el pie caía en
  y=130: dentro del panel y **encima del rótulo "No, al capítulo 1"**, letra sobre letra. Lo
  vio el usuario jugando el APK. Reparto nuevo, con todo subido cuatro unidades: título 38,
  pregunta 56, subtítulo 68, botones **`{78, 26, 20}`** (tinta 78..98 y 104..124, zonas del
  dedo 75..101 y 101..127) y los pies en 131 y 139. En PC no se movió nada.
- **La pantalla de Controles estrena un botón**, "Jugar el tutorial", y con él deja de ser
  una pantalla sin filas: tocar el botón lo juega y tocar **fuera** vuelve, que es lo que
  hacía siempre. En PC, ENTER lo juega y ESC vuelve — es el segundo cambio de significado de
  ENTER en la capa, y por eso el pie lo dice. Lanzado desde aquí el tutorial vuelve al
  TÍTULO, no a una partida (`TutorialThen`): quien iba a repasar los controles no puede
  perder la partida que tenía a medias.
- **`Engine::render_tutorial_card`** — el cartel de dos líneas (instrucción arriba, voz del
  juego debajo). Va en **y 34..62**, la única franja libre: el panel de vitales llega a 30,75
  y los mensajes del HUD salen a 136. Mide con `measure_text` y baja la escala de lo que no
  quepa, como `draw_message`. En verde mientras dura el visto bueno del paso.

**El botón de SALTAR tiene aquí un rect propio** (`tutorial_skip_rect`, {196,4,52,14}) y es
la única excepción a "el botón de SALTAR no se mueve": el de siempre ({262,4}) **pisa el
minimapa**, y en el tutorial sí hay HUD debajo — el dedo abría el mapa grande en vez de
saltar. `draw_skip_button_at` es el mismo dibujo con el rect que le des.

**La última fila de la pausa cambia de rótulo** durante el tutorial (`MenuSystem::in_tutorial`):
"Dejar el tutorial" en vez de "Salir". En PC es su ÚNICA salida, porque ESC ya es la pausa
que el paso 14 enseña y no se le puede dar otro oficio. Dejarlo por ahí cuenta como SALTARLO,
no como terminarlo: sale directo, sin las dos páginas de cierre (`Engine::finish_tutorial`
recibe `completed`).

### Los joysticks son REDONDOS (2026-09-15)

Los dos sticks tactiles (`HUD::draw_touch_controls`) eran rects solidos y en el movil se
leian como dos cajas de dialogo puestas sobre el juego. Ahora van con `Renderer::draw_disc`
y `draw_ring` (hueco + aro + pulgar) y con los alphas bajados casi a la mitad: mover
60/90/170 donde antes 110/60/205. **Solo los sticks**: los botones de accion (DASH, USAR,
ARMA, ORAR) se quedan cuadrados a proposito —son botones, y el marco es lo que dice donde
acaba el area sensible— y la mira sigue siendo una cruz.

Los radios de DIBUJO viven en `hud.cpp`; el radio **logico** del stick es
`VirtualJoystick::radius = 40` (`src/input/input.h`) y no se toca: el stick no tiene
hit-test de rect, asi que redondear el dibujo no cambia ni un pixel de lo que responde.

**El HUD no puede mentir sobre lo que pinta**: durante el tutorial el minimapa muestra el
capítulo 1 —el que se va a jugar al salir—, así que `HUDContext::tutorial` cambia la línea del
objetivo por "Mapa del cap. 1" en vez de dar una dirección de jefe que aquí no significa nada.

## Lo que se compra y lo que se lleva, a la vista (2026-10-03)

El usuario: las ofrendas "no se entienden", lo comprado "no afecta" y los objetos "no son
logicos de usar". Buena parte era de esta capa: nada de eso se VEIA. Piezas nuevas:

- **Contador de ofrendas** (`HUD::draw_offerings`) bajo la fe, a la derecha del panel de
  vitales: moneda dorada + banco; al ingresar SALTA y dice "+N" (`HUDContext::offering_pop`).
  `offerings = -1` lo apaga (tutorial).
- **Dones junto a la vida**: un aliento dorado por cada Aliento de Vida que queda, y la
  **piedra** de la Piedra del Memorial a la derecha del panel (apagada con su barrido mientras
  recarga). En el mundo, el escudo es un aro de piedra a los pies (`Engine::render`).
- **La alforja** (`HUD::draw_belt`, `HUD::belt_cell_rect`): abajo a la izquierda en PC (14 u,
  la tecla 1-4 encima, "x2" de cargas, marco de oro en el ELEGIDO y "[R] <nombre>" al lado);
  en tactil sube a y=58, sin teclas (alli se usa con USAR, que lleva el icono del elegido y
  se mantiene para cambiar). "[P] Orar xN" encima cuando hay Altar Portatil.
- **Ficha de objeto obtenido** (`HUD::draw_pickup_card`, datos en `Engine::announce_item`):
  arriba al centro, entra deslizandose 4 s: icono, nombre en su color de rareza, QUE hace y
  COMO se usa ("A la alforja: pulsa [2] para usarlo", "Arma nueva: [I] o clic derecho").
- **Numeros flotantes con tipo** (`ECSWorld::DamagePopup::kind`): dano blanco/oro, curacion
  verde "+N", fe dorada "+N fe", quemadura naranja.
- `draw_message` baja a escala 0,6 si a 0,75 aun no cabe en 300 unidades.
- **Cartas de eleccion**: 92 de alto (eran 78) y el ajuste de lineas de `Renderer::wrap_text`
  (el de 40 bytes de este fichero cortaba en silencio las descripciones con tildes; se borro).
  Debajo, la placa **"[R] Echar suertes (N)"** cuando hay re-sorteos (`choice_reroll_rect`,
  dibujo y toque de la misma formula).
- **Equipo** (fila 1 de la pausa, `Engine::render_equipment`): cifras reales (vida, dano con el
  multiplicador de fe, cadencia en disparos/s, velocidad, fe, reliquia, arma), los dones del
  Santuario activos y lo que queda de cada uno, la alforja y TODOS los objetos con icono,
  nombre y efecto (W/S recorre si hay mas de seis). **Insertar la fila desplazo las de debajo**
  (Codice 2, Opciones 3, Salir 4): los guiones que bajaban por la pausa se re-pinnearon.
- **Derrota y victoria** pintan el desglose de ofrendas (`Engine::render_run_summary`) y la
  derrota tiene **El Santuario** como fila del medio. El titulo marca El Santuario con una
  moneda cuando hay algo comprable (`MenuSystem::sanctuary_ready`). El Santuario en si esta en
  `src/systems/CLAUDE.md`.

## Los controles tactiles, redondos y en la superposicion (2026-10-03)

El layout y por que esta en `src/input/CLAUDE.md` ("La pasada de móvil"). Lo de esta capa:

- **En tactil, `HUD::render` ya no pinta ni las celdas de milagro ni los botones**: los pinta
  `Engine::render` DESPUES del letterbox, dentro de `Renderer::begin_overlay`, con el MISMO
  `HUDContext` del frame (`touch_ctx`). En un 20:9 el grupo vive en el pilar negro y el
  objetivo de 320x180 no llega ahi. Van encima de todo (tambien del velo de un salto) salvo
  con el mapa grande abierto, que lo cierra cualquier toque. En PC nada cambia: celdas
  cuadradas con su Q/E/F.
- **`draw_round_button`** (estatica en `hud.cpp`) es el unico dibujo de boton tactil:
  sombra, placa, bisel, icono propio (o el del tileset), **barrido radial de recarga**
  (`Renderer::draw_pie`, con los segundos que faltan si pasan de 1), candado si falta nivel de
  fe, aro (oro que respira si se puede usar, rojo si se niega o esta cerrado, apagado si
  recarga), **rotulo DENTRO, abajo** sobre una franja oscura, y una chapa arriba a la derecha
  (coste en fe con su orbe, cargas de USAR, oraciones que quedan en ORAR). El rotulo va dentro
  y la chapa dentro de la caja del boton porque en el arco el hueco entre bordes es de ~3
  unidades: lo que se sale del circulo pisa al vecino. `RoundButtonLook` es lo que cada
  boton (DASH, USAR, un milagro...) rellena; `draw_round_button` solo sabe de formas.
- **Pulsado = se hunde** (radio x0,92, placa clara, aro blanco): es la confirmacion que da un
  boton fisico y la unica que se ve con el pulgar encima. **Negado = tiembla** en rojo
  (`TouchButton::deny_t`).
- **Opacidad del jugador**: todo alfa del dibujo tactil pasa por `op_a(a, op)`.
- **`HUD::draw_touch_preview`**: en Opciones, al tocar Tam. botones / Opacidad / Zurdo, todos
  los botones del layout NUEVO salen 1,6 s encima de la pantalla (`Engine::touch_preview_t`)
  con rotulos genericos (DASH, USAR, 1, 2, 3...). Sin ella se ajustaba a ciegas.
- **El aviso del HUD sube en tactil** (`draw_message`, y = 180 - 64 en vez de 180 - 44): un
  aviso largo quedaba con la cola debajo de USAR. Y por eso la **ficha del milagro** (mantener
  pulsado) baja de y=96 a **y=58**, o se plantaba encima del aviso.
- `HUDContext::dash_cd` (la recarga del dash que falta, 0..1) la rellena el Engine desde
  `MovementComponent::dash_cooldown`: el HUD no ve el ECS por dentro.
- **Opciones en tactil tiene 12 filas** (las cuatro de los controles: Tam. botones, Opacidad,
  Zurdo, Ahorro bat., detras de Joystick de apuntar; `MenuSystem::is_touch_row`), y por eso
  `MAX_ROW_RECTS` sube a 14. En PC siguen siendo las de siempre. La ventana de 6 filas que se
  arrastra ya existia.
- **Controles** (`render_controls`, tactil) describe el arco y dice que el tamaño y el zurdo
  estan en Opciones; las mitades cambian con `MenuSystem::touch_lefty`.
