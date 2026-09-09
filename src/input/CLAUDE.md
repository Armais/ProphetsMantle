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
(mismos rects, definidos una sola vez en `Input::init_touch_layout`), y encima se apilan
**USAR** (consumible) y **DASH** — la única franja de la derecha que no pisa el minimapa ni la
barra de jefe (x 80..240 abajo). Joystick Izq virtual=Mover (nace donde baja el dedo),
Tap zona derecha=Atacar (auto-aim), Hold+Drag ≥6 unidades en la mitad derecha=Apuntar manual
(`joystick_aim` → `aim_axis`, que apaga el auto-aim por sí solo), Tap en un milagro=lanzarlo
(al SOLTAR, no al pulsar) y Hold ≥0,4 s=su ficha, Swipe de 2 dedos=Pausa.
Todo esto vive tras `Input::touch_ui`, que **solo** se enciende en móvil o con `--touch`:
`has_touch()` se descartó como criterio porque el portátil de desarrollo declara pantalla
táctil y el HUD salía con botones de teléfono en una partida de teclado.

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
  Están verificados con eventos `SDL_EVENT_FINGER_*` sintéticos (`scripts/tactil_check.txt`), que
  cubren la lógica pero no el tamaño real del pulgar ni el DPI del dispositivo.
