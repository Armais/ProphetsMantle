# `src/input/` - controles, auto-mira y tactil

Reglas transversales a todo `src/`: `../CLAUDE.md`.

### Controls

**PC (Teclado):** WASD/Flechas=Mover, K (mantener)=Ataque en ráfaga hacia la ÚLTIMA DIRECCIÓN DE MOVIMIENTO (8 direcciones, `Player::last_move_dir`; el ratón NO apunta ni dispara), Espacio/J=Dash, Q=Milagro1, E=Milagro2, F=Milagro3 (solo con el tercer slot), R=Consumible (usa el que llevas en `Player::consumable_slot`), Tab=Mapa, Esc=Pausa. En la pantalla de elección: A/D elegir, ENTER tomar. `Input::aim_axis` existe solo como override para el bot/scripts de autoplay. La opción "Auto-mira" (Opciones) activa el auto-apuntado del GDD también en PC (`--touch` lo fuerza para depurar).

**Gamepad:** Stick Izq=Mover, Stick Der=Apuntar, RT=Ataque, LT=Ataque secundario, A/X=Dash, RB=Milagro1, LB=Milagro2, **Y/△=Milagro3** (D-Pad↓ sigue valiendo), X/□=Consumible, D-Pad↑=Orar.
El GDD asignaba Y/△ a la vez a *Interactuar* y al tercer milagro; se resolvió a favor del
milagro porque **`Action::Interact` no la lee nadie** (el altar se activa al pisarlo y el cofre
al dispararle), y sin este botón el tercer slot solo se alcanzaba por la cruceta.

**Mobile (`ui/hud.cpp: draw_touch_controls` + `input/input.h: TouchButton`):** en un teléfono
el HUD **es** el mando: las tres celdas de milagro del HUD son a la vez sus tres botones
(mismos rects, definidos una sola vez en `Input::init_touch_layout`), y encima se apila la
**fila de acción** — de derecha a izquierda **DASH**, **USAR**, **ARMA**, **ORAR** (y 22x14
de **`II`** arriba al centro para la pausa). Joystick Izq virtual=Mover (nace donde baja el
dedo), Tap zona derecha=Atacar (auto-aim), Hold+Drag ≥6 unidades en la mitad derecha=Apuntar
manual (`joystick_aim` → `aim_axis`, que apaga el auto-aim por sí solo), Tap en un
milagro=lanzarlo (al SOLTAR, no al pulsar) y Hold ≥0,4 s=su ficha, **tap en el minimapa=mapa
grande** (y cualquier toque lo cierra).
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
| Historia, prólogo, Controles, Victoria | tocar en cualquier sitio = continuar |

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
  `menus_tactil_check.txt`), que cubren la lógica pero no el tamaño real del pulgar ni el DPI.
  Lo que habrá que calibrar en la primera prueba: **tamaño y opacidad de los botones por DPI**
  (22 unidades ≈ 8 mm en 6,5"), layout zurdo, `SDL_GetWindowSafeArea` (notch), si los botones
  deberían salirse al pilar en 20:9, y el stick derecho por defecto en móvil.
