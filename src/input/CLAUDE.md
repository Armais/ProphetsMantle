# `src/input/` - controles, auto-mira y tactil

Reglas transversales a todo `src/`: `../CLAUDE.md`.

### Controls

**PC (Teclado):** WASD/Flechas=Mover, K (mantener)=Ataque en ráfaga hacia la ÚLTIMA DIRECCIÓN DE MOVIMIENTO (8 direcciones, `Player::last_move_dir`; el ratón NO apunta ni dispara), Espacio/J=Dash, Q=Milagro1, E=Milagro2, F=Milagro3 (solo con el tercer slot), **1-4 = usar ese hueco de la alforja** (`Action::Belt1..4`), R=usar el consumible ELEGIDO, **rueda del ratón / C = cambiar el elegido** (`BeltPrev/BeltNext`), I o clic derecho=arma, P=orar (Altar Portátil), Tab=Mapa, Esc=Pausa. En la pantalla de elección: A/D elegir, ENTER tomar. `Input::aim_axis` existe solo como override para el bot/scripts de autoplay. La opción "Auto-mira" (Opciones) activa el auto-apuntado del GDD también en PC (`--touch` lo fuerza para depurar).

**Gamepad:** Stick Izq=Mover, Stick Der=Apuntar, RT=Ataque, LT=Ataque secundario, A/X=Dash, RB=Milagro1, LB=Milagro2, **Y/△=Milagro3** (D-Pad↓ sigue valiendo), X/□=usar el consumible elegido, **D-Pad ←/→ = cambiar el elegido de la alforja** (solo eso: no mueven al profeta), D-Pad↑=Orar.
El GDD asignaba Y/△ a la vez a *Interactuar* y al tercer milagro; se resolvió a favor del
milagro porque **`Action::Interact` no la lee nadie** (el altar se activa al pisarlo y el cofre
al dispararle), y sin este botón el tercer slot solo se alcanzaba por la cruceta.

**Mobile (`ui/hud.cpp: draw_touch_controls` + `input/input.h: TouchButton`):** en un teléfono
el HUD **es** el mando: los tres milagros son a la vez sus tres botones (mismos centros,
definidos una sola vez en `Input::init_touch_layout`). Desde el 2026-10-03 los botones son
**redondos y van en arco alrededor de DASH**, que va grande en la esquina inferior derecha
(ver "La pasada de móvil", abajo): milagros 1-2-3 a 38 unidades (izquierda, diagonal, encima)
y USAR · ARMA · ORAR en un arco exterior. La pausa (`II`) va arriba, en el pilar derecho si
cabe. Joystick Izq virtual=Mover (nace donde baja el dedo y **le sigue**), Tap zona
derecha=Atacar (auto-aim), Hold+Drag ≥6 unidades en la mitad derecha=Apuntar manual
(`joystick_aim` → `aim_axis`, que apaga el auto-aim por sí solo), Tap en un milagro=lanzarlo
(al SOLTAR, no al pulsar) y Hold ≥0,4 s=su ficha, **tap en el minimapa=mapa grande** (y
cualquier toque lo cierra). Con el ajuste **Zurdo** todo se refleja: stick en la mitad
derecha, botones a la izquierda.
Todo esto vive tras `Input::touch_ui`, que **solo** se enciende en móvil o con `--touch`:
`has_touch()` se descartó como criterio porque el portátil de desarrollo declara pantalla
táctil y el HUD salía con botones de teléfono en una partida de teclado.

### La pasada de Android (2026-09-11): lo que cambió y por qué

**Horizontal forzado.** `SDL_HINT_ORIENTATIONS` + `screenOrientation="sensorLandscape"`. En
vertical los botones bajan a la banda negra (`render/CLAUDE.md`), y ese mecanismo sigue vivo
y verificado, pero **dormido**: en apaisado la banda es 0.

**El letterbox también vale para el dedo.** Los eventos `SDL_EVENT_FINGER_*` vienen
normalizados a la **ventana entera**. `Input::touch_vp` (que escribe el Engine desde
`Renderer::viewport_norm()`) los pasa a unidades de juego. Sin esa resta, en 20:9 los botones
se dibujaban en x 1992..2124 px y respondían en 2190..2352 — dentro del pilar negro. El ratón
ya pasaba por `window_to_game`; esto es su equivalente. **Un dedo en la banda da valores
fuera de 0..320 / 0..180 a propósito**: es lo que el modo vertical ya asumía.

**`TouchButton::enabled` manda en el dibujo Y en el hit-test.** ARMA y ORAR solo existen si
el profeta lleva arma secundaria / puede rezar (`Input::set_touch_enabled`, que rellena
`Engine::update` con las **mismas** condiciones que usa el gameplay). Antes el hit-test
probaba los cinco rects aunque el HUD no pintara ninguno, y quedaban **huecos muertos** donde
el dedo ni pulsaba ni disparaba. Las posiciones son FIJAS: beberse el consumible deja un
hueco en la fila en vez de mover DASH, que es lo que rompería la memoria del pulgar.

**El swipe de dos dedos ya no existe.** Chocaba con la postura normal de juego (un dedo en el
stick + otro apuntando). Lo sustituyen el botón `II` y el **botón atrás de Android**
(`SDL_SCANCODE_AC_BACK`, mapeado a `Pause` + `Cancel` como ESC — SDL3 lo entrega como tecla,
así que sin mapearlo no hacía nada en absoluto).

**Nada de leyendas de teclado en táctil**: fuera las chapas `Q/E/F`, `R` e `I`, y "TAB: mapa"
pasa a "Toca: mapa". Un botón que anuncia una tecla que el aparato no tiene es peor que uno
mudo.

### Menús con el dedo (2026-09-11)

Hasta esta pasada **toda** la navegación era `MoveUp/Down/Confirm/Cancel` y el táctil no
emitía ninguna de las cuatro: en un teléfono no se podía ni pulsar "Nueva Partida". El
mecanismo es deliberadamente pequeño, para que ningún menú tenga que saber que existe el
táctil:

- **`Input::take_tap(Vec2&)`**: un dedo que baja y sube a menos de `TOUCH_TAP_SLOP` (6
  unidades). Se consume una vez y `pre_update` lo tira si nadie lo recogió.
- **`Input::pulse(Action)`**: pulsación de UN frame. El menú traduce el tap al idioma que ya
  hablaba (`Confirm`, `MoveLeft/Right`) y su código no cambia.
- **`Input::touch_gameplay`** (= `scene == Playing`): fuera de la partida el dedo no crea
  stick ni dispara ni pulsa botones. Las **SUELTAS se procesan siempre**: si no, el dedo que
  estaba disparando al abrir la pausa se quedaba pulsado al volver.
- Con el juego parado, **arrastrar recorre la lista** (`TOUCH_SCROLL_STEP` = 10 unidades por
  paso, scroll natural). Y un arrastre nunca cuenta como tap.

Reglas de interacción, decididas por el usuario:

| Pantalla | Toque |
|---|---|
| Título / Pausa / Game Over | la fila que se toca se **elige y se confirma** |
| Opciones | mitad **izquierda** de la fila baja el valor, **derecha** lo sube; "Volver" confirma |
| El Santuario | tocar una mejora la compra; arrastrar recorre |
| El Códice | tocar pestaña = cambiar; tocar fila = **seleccionar** (no cierra); "< Volver" al pie |
| Cartas (3 de 1) y encuentros | **dos toques**: el primero elige, el segundo decide |
| Historia, prólogo, intro de capítulo | tocar en cualquier sitio = continuar; el botón **SALTAR** de arriba a la derecha salta el tramo |
| Controles, Victoria | tocar en cualquier sitio = continuar |

Los rects salen **siempre de la misma fórmula que pinta** (`MenuSystem::mark_row`,
`CodexScreen::tab_rects/row_rects`, `HUD::minimap_rect`, `choice_card_rect`,
`dialogue_option_rect`). Ese contrato ya existía para los botones del HUD y es la razón de que
el dibujo y el hit-test no puedan separarse.

### Vibración de verdad en el móvil (2026-09-11)

`Input::rumble` tenía una sola vía, `SDL_RumbleGamepad`: en un teléfono sin mando era una
llamada vacía. Ahora, **solo en móvil**, `Input::init` abre el vibrador del aparato
(`SDL_INIT_HAPTIC` → `SDL_GetHaptics` → `SDL_OpenHaptic` → `SDL_InitHapticRumble`) y `rumble`
toca los dos. `rumble_enabled` sigue siendo la **única** puerta del ajuste y `Input::shutdown`
(desde `Engine::shutdown`) cierra el háptico. En escritorio no cambia nada: `is_mobile()` es
falso y este portátil declara hápticos que no son los del jugador.

### Ciclo de vida (Android / iOS)

`Engine::handle_events` atiende `WILL_ENTER_BACKGROUND` / `TERMINATING` (pausa si se estaba
jugando, **guarda** —el sistema puede matar la app sin avisar otra vez— y llama a
`Input::release_touch()`, porque Android no entrega los `FINGER_UP` al irse) y
`DID_ENTER_FOREGROUND` (rearma `last_time`, o el primer frame de vuelta traería un dt
enorme). El audio no se toca: SDL ya pausa el dispositivo por su cuenta.

**Auto-Aim (Mobile):** Prioridad: 1) Enemigo atacandote, 2) Mas cercano, 3) Menor HP (finishing), 4) Jefe en pantalla

### Auto-mira (el ajuste de tres estados)

- **Auto-mira de TRES estados** (`Config::AimMode`), que es lo que pide el GDD y no un
  interruptor. Sustituye a `Input::auto_aim_enabled`, que era binario; quien solo necesite
  saber si el tiro se fija tiene `Input::auto_aim_locks()`.
  - **Off**: tu direccion y punto.
  - **Assisted**: tu direccion, corregida `AIM_ASSIST_PULL` (65%) hacia el objetivo **solo
    si cae dentro de `AIM_ASSIST_CONE_DEG`** (22 grados ~ la mitad del sector de una de las
    8 direcciones del teclado: corrige el error de apuntar, no elige objetivo por ti).
  - **On**: con las manos quietas fija el objetivo de `pick_target`. Un apuntado manual
    explicito lo pisa igual que antes.
- La decision de a donde va el tiro esta en **`Player::resolve_aim`, un solo sitio**. Antes
  el ataque principal y el secundario llevaban cada uno su copia de la misma logica.

### Segundo joystick tactil (GDD 4.2 "opcional", 2026-08-25)

`SETTING_TOUCH_RSTICK` -> `Input::touch_right_stick`. **La diferencia es una sola cosa, y
es la que separa un twin-stick de un hold+drag**: con la opcion apagada el dedo derecho se
comporta como siempre (tap = disparo con auto-mira, y hay que arrastrar `TOUCH_DRAG_AIM`
unidades para que el apuntado registre); con ella encendida el stick **nace ya activo y
apunta desde el primer pixel**, y se dibuja en pantalla.

- **El reparto de zona con los botones ya estaba resuelto y hay que no romperlo**: en
  `process_touch`, los `touch_buttons` se comprueban ANTES que el "tap = ataque" y hacen
  `return`. Un dedo que baja dentro de una celda de milagro lanza el milagro y **nunca** se
  convierte en stick. Verificado en el guion.
- Usa la **Sensibilidad** del ajuste de usuario: divide el radio del joystick y el umbral
  de arrastre.
- **Se llama "Joystick de apuntar"** ("Aim joystick") desde el 2026-10-03, decision del
  usuario. Antes era "Stick dcho.", jerga de mando que no decia que hacia; se descarto
  "Apuntado manual" porque apuntar a mano existe en TODAS las plataformas (es lo que hace la
  Auto-mira en OFF, o arrastrar sin este ajuste) y el nombre tiene que decir lo que solo pasa
  en tactil: que aparece un joystick en pantalla para apuntar.
- **La fila de Opciones solo existe en tactil**, y por eso las filas de esa pantalla pasaron
  a montarse sobre una **tabla de ids** (`MenuSystem::SettingRow` + `settings_rows[]`) en
  vez de por indice fijo: sin ella, quitar una fila desplazaba el significado de todas las
  de abajo **tanto al pintar como al ajustar**. `Engine::update_settings` decide por ID.

**`SDL_Log("Touch aim ON (right stick|drag threshold)")`, una linea por gesto**, es lo que
hace comprobable la opcion desde un guion. Verificarlo a ojo comparando capturas costo dos
pasadas perdidas: el generador pone **altares dorados** por el mapa y su adorno se confunde
con la mira. Y el umbral **no es fijo** — es `TOUCH_DRAG_AIM` entre la sensibilidad, que
vive en el guardado: con sensibilidad 8 baja de 6 a 4,2 unidades y un arrastre de 5 lo
cruzaba solo. Guion: `scripts/tactil_check.txt` ampliado (comando nuevo **`rstick 0|1`**,
que existe porque la fila de Opciones no esta en escritorio y un guion no puede navegar a
lo que no se pinta).

### Movil

- **Working**: mobile auto-aim (`pick_target` in `game/combat.h`, GDD priority boss > attacking > closest > lowest HP), shared with the autoplay bot; toggle in Opciones, `--touch` forces it on desktop. `pick_target` **solo devuelve hostiles**: con los discípulos en el mundo, apuntar a los tuyos sería lo primero que pasaría.
- **Pendiente (del usuario, no del agente)**: probar los controles táctiles en un **Android real**.
  Ya hay APK (`build_android.bat`, ver el `CLAUDE.md` de la raíz). Están verificados con eventos
  `SDL_EVENT_FINGER_*` sintéticos (`scripts/tactil_check.txt`, `tactil_ancho_check.txt`,
  `menus_tactil_check.txt`, `movil_check.txt`), que cubren la lógica pero no el pulgar real.
  De la lista de calibración de la primera prueba quedan resueltos (2026-10-03) el tamaño por
  DPI, la opacidad, el zurdo, la zona segura (notch/gestos) y el uso de los pilares del 20:9.
  Queda abierto el **stick derecho por defecto en móvil** (sigue apagado) y comprobar en el
  aparato que `SDL_GetWindowDisplayScale` da la densidad esperada: la línea
  `Touch layout: ... escala X (dpi Y x Z)` del logcat lo dice.

### Pulido tras la primera prueba del APK (2026-09-11)

- **`touch_gameplay` se apaga durante la entrada del jefe** (`Engine::boss_approach_t > 0`):
  ahi al profeta lo lleva el motor, no el dedo, asi que no se dibuja ni se atiende ningun
  control tactil. Un stick que no responde es peor que ninguno. El paseo se corta con
  `Action::Cancel`, que en Android es el boton atras. Detalle del estado: `src/core/CLAUDE.md`.
- **Las celdas de milagro vuelven a estar rotuladas** en tactil, con el nombre corto del
  milagro en el hueco donde iba la chapa `Q/E/F`. Al quitar esa chapa —correctamente, porque
  anunciaba teclas que un telefono no tiene— la celda se quedo muda y el usuario probo el APK
  sin saber que se tocaba. DASH, USAR, ARMA y ORAR llevan etiqueta; estas tambien. El detalle
  del dibujo esta en `src/ui/CLAUDE.md`.
- El rect del minimapa pequeno (`HUD::minimap_rect`, que `Input::touch_minimap` comparte para
  que tocarlo abra el mapa grande) **no ha cambiado** pese a que el minimapa se redibujo: las
  celdas se ensancharon hasta llenarlo en vez de mover el marco. Ese contrato sigue intacto.

### Pulido tras la segunda prueba del APK (2026-09-12)

**En un teléfono no había forma de saltarse nada.** El tap se traducía SIEMPRE a `Confirm`
(prólogo, intros de capítulo, cartas de historia, apertura guionizada): la única salida era el
botón atrás de Android, que en un teléfono por gestos no existe, y el botón `II` está apagado
fuera de `Playing`. Ahora hay un **botón de SALTAR** (`skip_button_rect` / `draw_skip_button`,
en `ui/hud.cpp`) que emite `Action::Cancel` — la acción con la que cada sistema ya sabía
saltarse entero, así que no hay camino nuevo que mantener.

- **No se mueve entre pantallas**: `{262, 4, 52, 14}` siempre, como las posiciones fijas de la
  fila de acción. Y el rect sale de la misma fórmula que lo pinta, el contrato de esta capa.
- **Se salta TRAMO A TRAMO** (decisión del usuario): saltar la apertura del capítulo lleva a
  su intro, y esa se salta con otro toque. No hay botón que se lleve la cadena entera.
- **Un toque en cualquier OTRO sitio sigue avanzando**, que es lo que hacía antes. El hit-test
  es el mismo en los tres sitios (`tap_skip_or_advance` en `engine.cpp` para `Intro` y
  `Story`; dentro de `update_dialogue` para la escena, donde se atiende ANTES que nada para
  que valga también durante el paseo).
- **`Input::peek_tap`**: como `take_tap` pero sin consumir. Lo necesita quien tiene que mirar
  si el toque cae en un botón concreto antes de decidir quién se queda el tap.
- El botón **solo se dibuja en táctil**, y en la caja de diálogo solo si la escena se puede
  saltar de verdad (`DialogueView::skippable`): un encuentro del pozo no se salta, y un botón
  que no hace nada miente. En PC nada cambia salvo que el pie de una escena guionizada ya
  DICE que se puede saltar (`[ENTER] seguir    [ESC] saltar`): se podía desde siempre y
  nadie lo sabía.

**Quedaban cuatro leyendas de teclado en táctil** pese a la regla de arriba: el prólogo y las
intros (`cinematic.cpp`), las cartas de historia (`story.cpp`), las cartas de elección
(`hud.cpp`) y — ya resueltas en la pasada anterior — el Códice y el Santuario. `CinematicSystem`
y `StorySystem` reciben un `bool touch_ui` que el Engine escribe antes de renderizar, mismo
patrón que `MenuSystem::touch_hints`.

### Tercera prueba del APK: por que el dedo "no funcionaba bien" (2026-09-12)

El usuario volvio del APK con "el area tocable no funciona bien". No era una impresion:
habia cuatro fallos sumados, y el peor no se podia ver en el PC de desarrollo.

**Un toque se perdia entero si su frame no daba ningun paso de update.** `pre_update` tiraba
`tap_pending` al empezar cada frame, y el bucle de `Engine::run` es de paso fijo a 60 Hz: en
una pantalla de 90 o 120 Hz hay frames que acumulan menos de `FIXED_DT` y **no ejecutan
ningun paso**, asi que el tap moria sin que nadie lo leyera — a 120 Hz, uno de cada dos. Con
el teclado era peor: el `memcpy` de `actions_prev` igualaba el flanco y esa pulsacion no
volvia jamas. Ahora `pre_update` **no hace nada si desde la ultima vez no hubo un
`consume_presses`** (`Input::step_since_pre`), que es exactamente "nadie ha leido esto
todavia". Se descarto darle al tap una vida por tiempo: no arregla el flanco de teclado, y
el contador se decrementaria en `update_touch`, o sea dentro del paso que no corre.

- **Ningun guion puede cazarlo**: los `FINGER_*` sinteticos se inyectan DENTRO de `update`.
  Su unica sonda es el log **`Input: tap descartado sin leer (x,y)`**, que solo se emite
  fuera de la partida (dentro, lo normal es que nadie recoja el tap y seria puro ruido).

**El primer dedo se queda el tap.** `tap_active/tap_finger/tap_down` se rearmaban en CADA
bajada, asi que la palma apoyada o el otro pulgar robaban el tap y al soltar el dedo bueno
el id ya no coincidia. Ahora solo se rearma si el dueño **ya no esta vivo** en
`touch_fingers`, porque un `FINGER_UP` perdido (Android no los entrega al volver de segundo
plano) dejaria la pantalla muerta para siempre.

**Dos slops, no uno.** `TOUCH_TAP_SLOP` (6 unidades ~ 2,2 mm) es lo que separa el tap de
ataque del apuntado manual y sigue igual EN PARTIDA. En un menu era demasiado fino: un
pulgar se desplaza mas que eso solo con apoyarse, el tap moria y el gesto se convertia en
paso de lista — el sintoma de "parece que hay que mover antes de tocar". En menus manda
**`TOUCH_UI_TAP_SLOP` = 14** (~5 mm), y `TOUCH_SCROLL_STEP` sube de 10 a **16** para quedar
POR ENCIMA del slop: si un paso de lista costara menos recorrido que el slop, el mismo
arrastre seria tap y scroll a la vez. Ademas cada paso de scroll mata el tap.

**Un paso de lista por evento, no todos los que quepan.** `pulse()` sobre la misma accion es
UNA pulsacion, asi que el `while` que emitia varios `MoveDown` de golpe perdia todos menos
el primero y un arrastre rapido recorria la mitad de lo que decia el dedo. Ahora es un `if`
y el resto se queda en `scroll_accum` para el evento siguiente (un dedo real manda 60-120
por segundo; un `drag` de guion, uno solo — de ahi que el guion arrastre en dos tramos).

**`Input::ui_press(Vec2&)`**: el dedo sigue apoyado y todavia cuenta como tap, con su
posicion ACTUAL (`tap_now`, apuntada en FINGER_DOWN y en FINGER_MOTION **antes** del test de
slop, porque en un menu la rama sale antes por el `return` de `!touch_gameplay`). Es lo que
ilumina el boton que hay debajo. Lo lee el Engine en **`render`**, no en `update`: a 120 Hz
hay frames sin paso y el resalte parpadearia al ritmo de los pasos en vez del dedo.

Guiones: `menus_tactil_check.txt` (re-pinneado entero) y el bloque de rejilla de
`tactil_ancho_check.txt` — ese es el unico que ve un desfase de letterbox, porque hasta la
rejilla todo lo tocable estaba centrado en x=160, que es justo el punto que el desfase no
mueve. Su gate es que un toque en el **pilar negro** (px 40) NO pulse nada: sin descontar el
letterbox caeria en x=10,7 unidades, dentro de la columna izquierda.

### `touch_ui_rect`: un botón de pantalla DENTRO de la partida (2026-09-12)

El tutorial (`game/tutorial.h`) trajo el primer botón que convive con el HUD y con el dedo
que dispara, y destapó dos cosas:

- **El rect de siempre del SALTAR pisa el minimapa.** `skip_button_rect()` es {262,4,52,14} y
  `HUD::minimap_rect` empieza en x=271: en las escenas no importa (ahí no hay HUD), pero en el
  tutorial el toque abría el mapa grande en vez de saltar. El del tutorial tiene rect propio,
  `tutorial_skip_rect()` = {196,4,52,14}, en el hueco entre el panel de vitales y el minimapa.
  Es la única excepción a "el botón de SALTAR no se mueve", y es por el HUD de debajo.
- **`Input::touch_ui_rect`**: se comprueba en `process_touch` ANTES del minimapa y del
  "tap = ataque", y a diferencia de los dos **no mata el tap** — quien lo dibuja lo recoge con
  `peek_tap`. Sin él, tocar ese botón disparaba el cayado de propina. {0,0,0,0} = no hay.
  Lo escribe `Engine::update` junto a `touch_minimap`, con las mismas condiciones.

Guion: **`scripts/tutorial_tactil_check.txt`** (`--touch`). Su gate visual es que **ningún
cartel del tutorial nombre una tecla** — mitad izquierda, mitad derecha, DASH, USAR, ARMA,
celdas del HUD — y el del log, `Tutorial: saltado en el paso N` al tocar (222,11).

### La alforja con el dedo (2026-10-03)

**USAR va al SOLTAR, como un milagro** (antes iba al bajar el dedo): hay que saber si fue un
toque o un mantener. Toque corto = usar el consumible elegido (`pulse(Consumable)`).
**Mantenerlo `TOUCH_HOLD_INFO` (0,4 s) = pasar al siguiente** (`pulse(BeltNext)`, y otra vez
cada 0,4 s mientras siga apoyado); al soltar tras un mantener no se bebe nada
(`TouchButton::hold_fired`). El boton lleva el icono y las cargas del ELEGIDO, y solo existe
con algo en la alforja (`set_touch_enabled(belt_count() > 0, ...)`). ORAR solo existe con
oraciones del Altar Portatil que gastar (`Engine::prayers_left`).

### La pasada de móvil (2026-10-03): mando de juego móvil de verdad

Petición del usuario: *"que la experiencia en la versión móvil sea profesional, cómoda,
óptima"*. Lo que cambió, por qué, y dónde está cada cosa:

**Botones redondos en arco alrededor de DASH** (`Input::init_touch_layout`,
`TOUCH_OFFSETS` en `input.cpp`). La fila de 22x22 de antes ponía ORAR a 103 unidades del
borde y todos a la misma distancia del suelo, con 5 de hueco: el pulgar derecho no descansa en
una fila, pivota desde la esquina. Ahora DASH (radio 16, ~11,5 mm) está donde descansa el
pulgar y el resto en dos arcos: milagros a 38 (180°, 135°, 90°) y USAR / ARMA / ORAR a ~66
(170°, 135°, 100°), intercalados. El hueco más estrecho entre dos bordes es 3,2 unidades
(comprobado a mano en el comentario de `TOUCH_OFFSETS`). Las posiciones siguen siendo FIJAS
por botón: beberse el último consumible deja un hueco, no mueve a nadie.

- **Hit-test circular con margen** (`TouchButton::edge_distance`, `Input::touch_button_at`):
  un dedo cuenta hasta `TOUCH_HIT_SLOP` (4 u, ~1,4 mm) por fuera de lo dibujado, y si dos
  márgenes se solapan gana el botón cuyo BORDE esté más cerca. Un dedo que ya pulsa un botón
  se suelta solo al salirse el DOBLE de ese margen (el pulgar rueda al apoyarse). Un dedo que
  cae en un botón que ya tiene otro dedo no dispara de propina.
- **`TouchButton::rect` sigue existiendo** como caja envolvente (la de la pausa ES su forma),
  `center` + `radius` es lo nuevo. El contrato de siempre aguanta: el HUD dibuja con los
  mismos centros con los que se prueba el dedo.

**El grupo se ancla a la ZONA SEGURA de la VENTANA, no al área de juego**
(`TouchLayoutParams::safe`, que rellena `Engine::refresh_touch_area`). Es la ventana entera
en unidades de juego (con sus pilares o bandas: en 20:9, x de -40 a 360) menos lo que el
sistema reserva (`SDL_GetWindowSafeArea`: notch, barra y zonas de gestos). Consecuencias:

- En un **20:9 el grupo vive en el pilar negro de la derecha**, que hasta ahora era espacio
  muerto, y el stick puede nacer en el de la izquierda (un dedo en x < 0 es "mitad
  izquierda"). El mundo queda mucho más despejado.
- La **pausa** sube al pilar (si caben 32 unidades) en vez de estar arriba al centro.
- En **vertical** (dormido) el grupo baja a la banda inferior por la misma regla, sin rama
  especial: desaparece `touch_band` y su umbral de 62.
- Se recalcula en `WINDOW_RESIZED` / `PIXEL_SIZE_CHANGED`, y además en
  `WINDOW_SAFE_AREA_CHANGED` y `DISPLAY_SCALE_CHANGED`.

**Tamaño por DPI** (`Engine::touch_dpi_scale`). La escala 1 es la de un 6,5" 20:9, donde una
unidad mide ~0,36 mm. En Android `SDL_GetWindowDisplayScale` es la densidad (dpi/160), y de
ahí sale lo que mide una unidad EN ESA pantalla; la escala lo devuelve al tamaño físico de la
referencia, acotada a 0,75..1,4 (una tablet de 11" da 0,49 → 0,75: sin el suelo los botones
saldrían diminutos en unidades, pero ya grandes en mm). En escritorio la escala es 1 salvo
con **`--dpi N`** (main.cpp), que finge una pantalla para probarlo.

**Ajustes del jugador** (Opciones, solo en táctil; `Input::set_touch_prefs`): **Tam. botones**
70..150 %, **Opacidad** 30..100 %, **Zurdo**. La escala final (DPI × tamaño) se acota además
para que el grupo quepa entre el centro de la zona segura y la esquina, y por debajo de y=58
(el minimapa y el objetivo): al 150 % en 20:9 queda en 1,27. Cambiar el lado o el tamaño
con un dedo puesto llama a `release_touch`, o el dedo se quedaría colgado de un botón que se
ha movido.

**Zurdo** = reflejo de todo el layout alrededor del centro de la zona segura (no del área de
juego: con un notch a un lado los dos grupos quedan igual de lejos de su borde) y
`Input::in_move_half`, que es lo que decide en `process_touch` qué mitad crea el stick.
`tutorial_text` y la pantalla de Controles (`MenuSystem::touch_lefty`) cambian
"izquierda"/"derecha" con él.

**El stick de movimiento** (`VirtualJoystick`):

- **Radio real = lo que se ve**: `joy_radius` = `TOUCH_JOY_R` (22 u, ~8 mm) × escala. Antes
  el radio lógico era 40 y la base se dibujaba de 16 con el pomo moviéndose 13: hacía falta
  arrastrar 14 mm para correr y el dibujo no decía la verdad. Ahora el pomo va 1:1 con el
  dedo hasta el tope.
- **La base sigue al dedo** (`VirtualJoystick::follow`) cuando este se pasa del radio. Sin
  eso, con el dedo 60 unidades más allá del origen había que desandarlas para girarse.
  Medido en `movil_check`: tras 100 u a la derecha la base queda a un radio exacto del dedo,
  y 10 u por detrás de ella el eje ya es (-0,45, 0).
- **Fantasma en reposo** (`Input::joy_rest`): base y pomo muy apagados en la esquina
  contraria a DASH, a su altura. Dice "el pulgar va aquí" sin texto; el stick sigue naciendo
  donde baje el dedo en toda la mitad.

**El "no se puede" se SIENTE**: un milagro que se intenta sin fe o en recarga hace temblar su
botón (`TouchButton::deny_t`, `TOUCH_DENY_TIME` 0,35 s, aro rojo) y da un toque corto de
vibración (`Input::deny_miracle`, desde el mismo evento que pinta el mensaje). Con el pulgar
encima, el mensaje de abajo no se ve; un botón que no responde sin decir nada se lee como "el
táctil falla". Log: `Touch deny: milagro N`.

**Atrás en el título sale de la app** (Android): la primera vez avisa ("Pulsa atrás otra vez
para salir", 2 s) y la segunda llama a `SDL_SendAndroidBackButton`, que es el "atrás" del
sistema (desde Android 12 manda la app al fondo). Antes no hacía nada en la raíz de la app.

Logs: `Touch layout: area X,Y WxH  escala S (dpi D x U)[  ZURDO]  DASH (x,y) pausa (x,y)` en
cada relayout. Guiones: **`movil_check.txt`** (`--touch --window 2400x1080`) y los
re-pinneados `tactil_check` (ahora con `tbtn`), `tactil_ancho_check` (la sonda en píxeles,
con el DASH en el pilar), `vertical_check` y `menus_tactil_check` (Opciones tiene 12 filas en
táctil). Comandos nuevos en `scripts/CLAUDE.md`.

### Apuntar y disparar con el mismo pulgar: comprobado (2026-10-03)

El dedo apoyado en la mitad de disparo mantiene el Ataque (rafaga a la cadencia del cayado)
y, si arrastra, el tiro sigue al arrastre: se apunta y se dispara a la vez, y se corrige sin
levantar el dedo. Medido con la auto-mira apagada en `scripts/apuntado_tactil_check.txt`.

**El primer tiro sale al BAJAR el dedo**, antes de que el arrastre exista, con la direccion
que haya en ese instante: con la auto-mira encendida (lo normal en movil) va al objetivo de
`pick_target`, que suele ser lo que se queria; con ella apagada, hacia donde mira el profeta.
Si algun dia molesta, la alternativa conocida es la de Brawl Stars (apuntar y disparar al
SOLTAR), pero cambia el tacto de todo el disparo tactil y no se ha tocado.
