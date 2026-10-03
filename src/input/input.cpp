// =============================================================================
// MANTLE - Input System Implementation
// =============================================================================

#include "input/input.h"
#include "platform/platform.h"
#include <cstring>
#include <cmath>
#include <algorithm>

void Input::init() {
    std::memset(actions, 0, sizeof(actions));
    std::memset(actions_prev, 0, sizeof(actions_prev));

    aim_mode = is_mobile() ? Config::AimMode::On : Config::AimMode::Off;
    // Solo en movil (o con --touch). has_touch() NO sirve de criterio: este portatil de
    // desarrollo declara un dispositivo tactil y el HUD salia con botones de telefono en
    // una partida de teclado.
    touch_ui = is_mobile();
    init_touch_layout(0.0f);

    // Try to open first available gamepad
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&count);
    if (joysticks && count > 0) {
        gamepad = SDL_OpenGamepad(joysticks[0]);
        if (gamepad) {
            gamepad_id = joysticks[0];
            SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(gamepad));
        }
    }
    SDL_free(joysticks);

    // Vibrador del telefono. Solo en movil: este portatil declara dispositivos hapticos
    // que no son los del jugador, y abrirlos en escritorio seria vibrar lo que no toca.
    if (is_mobile()) {
        if (SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
            int hcount = 0;
            SDL_HapticID* haptics = SDL_GetHaptics(&hcount);
            if (haptics && hcount > 0) {
                haptic = SDL_OpenHaptic(haptics[0]);
                if (haptic && !SDL_InitHapticRumble(haptic)) {
                    SDL_Log("Haptic: rumble unsupported (%s)", SDL_GetError());
                    SDL_CloseHaptic(haptic);
                    haptic = nullptr;
                }
                if (haptic) SDL_Log("Haptic device opened: %s", SDL_GetHapticName(haptic));
            }
            SDL_free(haptics);
        } else {
            SDL_Log("Haptic subsystem unavailable: %s", SDL_GetError());
        }
    }
}

void Input::shutdown() {
    if (haptic) {
        SDL_CloseHaptic(haptic);
        haptic = nullptr;
    }
}

// Los rects tienen que casar con lo que dibuja HUD::draw_miracle_slots / draw_touch_controls.
// La celda de milagro i vive en x = 320 - 28 - i*27, y = 180 - 36, de 22x22.
void Input::init_touch_layout(f32 band_below) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 S = 22.0f;
    touch_band = band_below;
    // En VERTICAL los botones se bajan a la banda negra (GDD 4.5). Hace falta sitio para
    // las dos filas (22 + 12 de hueco + 22 = 56) mas 6 de respiro; si no cabe, se quedan
    // donde siempre, dentro del area de juego.
    const bool in_band = (band_below >= 62.0f);
    const f32 H = in_band ? static_cast<f32>(Config::GAME_HEIGHT) + band_below - 6.0f
                          : static_cast<f32>(Config::GAME_HEIGHT);
    // Fila de arriba, de derecha a izquierda: DASH, USAR, ARMA, ORAR. A 68 y no a 64 del
    // borde para dejar libre la franja de las placas de coste de los milagros. ARMA y ORAR
    // solo se encienden si el profeta lleva arma secundaria / puede rezar
    // (Input::set_touch_enabled), asi que en la mayoria de las partidas la fila son dos.
    touch_buttons[TOUCH_BTN_DASH]   = {{W -  28.0f, H - 68.0f, S, S}, Action::Dash,            -1};
    touch_buttons[TOUCH_BTN_CONS]   = {{W -  55.0f, H - 68.0f, S, S}, Action::Consumable,      -1};
    touch_buttons[TOUCH_BTN_WEAPON] = {{W -  82.0f, H - 68.0f, S, S}, Action::AttackSecondary, -1};
    touch_buttons[TOUCH_BTN_PRAY]   = {{W - 109.0f, H - 68.0f, S, S}, Action::Pray,            -1};
    // Fila de abajo: las tres celdas de milagro del HUD, tal cual
    for (int i = 0; i < Config::MAX_MIRACLE_SLOTS && i < 3; i++)
        touch_buttons[TOUCH_BTN_MIRACLE + i] = {{W - 28.0f - i * 27.0f, H - 36.0f, S, S},
                                static_cast<Action>(static_cast<u32>(Action::Miracle1) + i), i};
    // Pausa: arriba al centro, en el hueco que dejan el panel de vitales (izquierda) y el
    // minimapa (derecha). Va SIEMPRE dentro del area de juego, tambien en vertical: es el
    // unico boton que hay que poder encontrar sin mirar.
    touch_buttons[TOUCH_BTN_PAUSE]  = {{W * 0.5f - 11.0f, 3.0f, S, 14.0f}, Action::Pause, -1};
}

// Botones contextuales: los que no aplican se apagan enteros (dibujo y hit-test).
void Input::set_touch_enabled(bool consumable, int miracle_slots, bool weapon, bool pray, bool pause) {
    bool on[TOUCH_BUTTON_COUNT] = {};
    on[TOUCH_BTN_DASH]   = true;
    on[TOUCH_BTN_CONS]   = consumable;
    on[TOUCH_BTN_WEAPON] = weapon;
    on[TOUCH_BTN_PRAY]   = pray;
    on[TOUCH_BTN_PAUSE]  = pause;
    for (int i = 0; i < Config::MAX_MIRACLE_SLOTS && i < 3; i++)
        on[TOUCH_BTN_MIRACLE + i] = (i < miracle_slots);
    for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
        if (touch_buttons[i].enabled == on[i]) continue;
        touch_buttons[i].enabled = on[i];
        // Apagarlo con el dedo encima lo dejaba pulsado para siempre
        if (!on[i] && touch_buttons[i].down) {
            touch_buttons[i].down = false;
            touch_buttons[i].hold = 0.0f;
            if (touch_buttons[i].slot < 0)
                actions[static_cast<u32>(touch_buttons[i].action)] = false;
        }
    }
}

// Todo el estado tactil al suelo. Android no entrega los FINGER_UP al irse a segundo
// plano, y sin esto el profeta volvia de la pausa corriendo y disparando.
void Input::release_touch() {
    for (auto& b : touch_buttons) {
        if (b.down && b.slot < 0) actions[static_cast<u32>(b.action)] = false;
        b.down = false;
        b.hold = 0.0f;
    }
    for (auto& f : touch_fingers) f.active = false;
    joystick_move.active = false;
    joystick_aim.active  = false;
    touch_aim_active     = false;
    aim_axis = {0, 0};
    move_axis = {0, 0};
    actions[static_cast<u32>(Action::Attack)] = false;
    tap_active   = false;
    tap_pending  = false;
    scroll_accum = 0.0f;
}

// Un tap sobre un milagro lo lanza; mantenerlo abre su ficha (GDD 4.2). Por eso el
// milagro NO se lanza al bajar el dedo: hay que saber antes cuanto ha durado.
void Input::update_touch(f32 dt) {
    if (!touch_ui) return;
    touch_info_slot = -1;
    for (auto& b : touch_buttons) {
        if (!b.down) { b.hold = 0.0f; b.hold_fired = false; continue; }
        b.hold += dt;
        if (b.slot >= 0 && b.hold >= TOUCH_HOLD_INFO) touch_info_slot = b.slot;
        // USAR mantenido: cambia al siguiente consumible de la alforja (y al soltar no se
        // bebe nada). Mantenerlo mas sigue pasando hueco a hueco cada 0,4 s.
        if (b.action == Action::Consumable && b.hold >= TOUCH_HOLD_INFO) {
            b.hold_fired = true;
            b.hold -= TOUCH_HOLD_INFO;
            pulse(Action::BeltNext);
        }
    }
}

void Input::pre_update() {
    // Si desde el ultimo pre_update no ha corrido NINGUN paso de update, nadie ha leido lo
    // que llego en ese frame: tirarlo era perder el toque entero. Pasa de forma sistematica
    // en cuanto la pantalla va por encima de 60 Hz — el bucle de Engine::run acumula
    // 1/120 s por frame y necesita 1/60 para dar un paso, asi que a 120 Hz la mitad de los
    // frames no ejecutan ninguno. Con el teclado era peor todavia: el memcpy de abajo
    // igualaba actions_prev a actions y el flanco de esa tecla no volvia nunca.
    if (!step_since_pre) return;
    step_since_pre = false;

    std::memcpy(actions_prev, actions, sizeof(actions));
    // Un toque que nadie recoge FUERA de la partida es sintoma de que el dibujo y el
    // hit-test no cuadran, y es lo unico que delata el fallo de arriba en un telefono de
    // verdad (los toques de un guion se inyectan DENTRO de update y nunca lo reproducen).
    // Dentro de la partida no se dice nada: ahi lo normal es que nadie lo recoja.
    if (tap_pending && !touch_gameplay)
        SDL_Log("Input: tap descartado sin leer (%.0f,%.0f)", tap_pos.x, tap_pos.y);
    tap_pending = false;   // un tap no sobrevive al frame en que se leyo
    // Las pulsaciones de un frame (tap tactil) se sueltan aqui, ya copiadas al historico
    for (u32 i = 0; i < ACTION_COUNT; i++) {
        if (!touch_pulse[i]) continue;
        actions[i] = false;
        touch_pulse[i] = false;
    }
}

void Input::post_update() {
    update_axes();
}

void Input::consume_presses() {
    step_since_pre = true;   // este paso ya ha leido lo que hubiera
    std::memcpy(actions_prev, actions, sizeof(actions));
}

// =============================================================================
// Event Processing
// =============================================================================
void Input::process_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            process_keyboard(event);
            break;

        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            process_mouse(event);
            break;

        // La rueda cambia el consumible elegido de la alforja (una muesca, un hueco)
        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.y > 0.0f)      pulse(Action::BeltPrev);
            else if (event.wheel.y < 0.0f) pulse(Action::BeltNext);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            process_gamepad(event);
            break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            if (!gamepad) {
                gamepad = SDL_OpenGamepad(event.gdevice.which);
                if (gamepad) {
                    gamepad_id = event.gdevice.which;
                    SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(gamepad));
                }
            }
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED: {
            if (gamepad && event.gdevice.which == gamepad_id) {
                SDL_CloseGamepad(gamepad);
                gamepad = nullptr;
                SDL_Log("Gamepad disconnected");
            }
            break;
        }

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
            process_touch(event);
            break;

        default:
            break;
    }
}

// =============================================================================
// Keyboard (from GDD 4.3)
// =============================================================================
void Input::process_keyboard(const SDL_Event& event) {
    bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
    if (event.key.repeat) return;  // Ignore key repeats

    switch (event.key.scancode) {
        case SDL_SCANCODE_W:      case SDL_SCANCODE_UP:
            actions[static_cast<u32>(Action::MoveUp)]    = pressed; break;
        case SDL_SCANCODE_S:      case SDL_SCANCODE_DOWN:
            actions[static_cast<u32>(Action::MoveDown)]  = pressed; break;
        case SDL_SCANCODE_A:      case SDL_SCANCODE_LEFT:
            actions[static_cast<u32>(Action::MoveLeft)]  = pressed; break;
        case SDL_SCANCODE_D:      case SDL_SCANCODE_RIGHT:
            actions[static_cast<u32>(Action::MoveRight)] = pressed; break;
        case SDL_SCANCODE_SPACE:  case SDL_SCANCODE_J:
            actions[static_cast<u32>(Action::Dash)]      = pressed; break;
        case SDL_SCANCODE_K:      actions[static_cast<u32>(Action::Attack)]    = pressed; break;
        // Arma secundaria (Honda / Vara). L y E ya son Miracle2, asi que va en I.
        case SDL_SCANCODE_I:      actions[static_cast<u32>(Action::AttackSecondary)] = pressed; break;
        case SDL_SCANCODE_P:      actions[static_cast<u32>(Action::Pray)]      = pressed; break;
        case SDL_SCANCODE_M:      case SDL_SCANCODE_Q:
            actions[static_cast<u32>(Action::Miracle1)]  = pressed; break;
        case SDL_SCANCODE_L:      case SDL_SCANCODE_E:
            actions[static_cast<u32>(Action::Miracle2)]  = pressed; break;
        // Tercer slot (mejora "Doble Porcion" de El Santuario): Q/E/F seguidas
        case SDL_SCANCODE_F:      actions[static_cast<u32>(Action::Miracle3)]  = pressed; break;
        case SDL_SCANCODE_R:      actions[static_cast<u32>(Action::Consumable)]= pressed; break;
        // La alforja: los numeros usan ese hueco, como el cinturon de pociones de un ARPG.
        // Teclado de arriba y teclado numerico valen igual.
        case SDL_SCANCODE_1: case SDL_SCANCODE_KP_1: actions[static_cast<u32>(Action::Belt1)] = pressed; break;
        case SDL_SCANCODE_2: case SDL_SCANCODE_KP_2: actions[static_cast<u32>(Action::Belt2)] = pressed; break;
        case SDL_SCANCODE_3: case SDL_SCANCODE_KP_3: actions[static_cast<u32>(Action::Belt3)] = pressed; break;
        case SDL_SCANCODE_4: case SDL_SCANCODE_KP_4: actions[static_cast<u32>(Action::Belt4)] = pressed; break;
        case SDL_SCANCODE_C:      actions[static_cast<u32>(Action::BeltNext)]  = pressed; break;
        case SDL_SCANCODE_TAB:    actions[static_cast<u32>(Action::Map)]       = pressed; break;
        // AC_BACK: el boton "atras" de Android llega como tecla (SDLActivity lo consume).
        // Hace lo mismo que ESC: pausa en juego, cancela en los menus.
        case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_AC_BACK:
                                  actions[static_cast<u32>(Action::Pause)]     = pressed;
                                  actions[static_cast<u32>(Action::Cancel)]    = pressed; break;
        case SDL_SCANCODE_RETURN: actions[static_cast<u32>(Action::Confirm)]   = pressed; break;
        default: break;
    }
}

// =============================================================================
// Mouse
// =============================================================================
void Input::process_mouse(const SDL_Event& event) {
    // The mouse never aims nor fires: shots follow the movement direction (see Player::update).
    // Position is still tracked for menus.
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        // Window coordinates; the Engine converts to game space each frame (mouse_game)
        mouse_pos = {event.motion.x, event.motion.y};
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
             event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        mouse_pos = {event.button.x, event.button.y};
        if (event.button.button == SDL_BUTTON_RIGHT) {
            actions[static_cast<u32>(Action::AttackSecondary)] = pressed;
        }
    }
}

// =============================================================================
// Gamepad (from GDD 4.3 - Xbox/PlayStation)
// =============================================================================
void Input::process_gamepad(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
        event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        bool pressed = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);

        switch (event.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:         // A/X - Dash
                actions[static_cast<u32>(Action::Dash)] = pressed;
                actions[static_cast<u32>(Action::Confirm)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_EAST:          // B/O - Cancel
                actions[static_cast<u32>(Action::Cancel)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_WEST:          // X/Square - Consumable
                actions[static_cast<u32>(Action::Consumable)] = pressed;
                break;
            // El GDD pone en Y/Triangulo dos cosas a la vez: "Interactuar" y el tercer
            // milagro. Se resuelve a favor del milagro — Action::Interact no la lee nadie
            // (no hay nada que "interactuar": el altar se activa al pisarlo y el cofre al
            // dispararle), mientras que sin este boton el tercer slot solo se alcanzaba por
            // la cruceta. D-Pad abajo se conserva como segunda tecla.
            case SDL_GAMEPAD_BUTTON_NORTH:         // Y/Triangulo - Milagro 3
                actions[static_cast<u32>(Action::Miracle3)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: // RB - Miracle 1
                actions[static_cast<u32>(Action::Miracle1)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  // LB - Miracle 2
                actions[static_cast<u32>(Action::Miracle2)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_START:
                actions[static_cast<u32>(Action::Pause)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_BACK:
                actions[static_cast<u32>(Action::Map)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_UP:
                actions[static_cast<u32>(Action::Pray)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      // D-Pad abajo - Milagro 3
                actions[static_cast<u32>(Action::Miracle3)] = pressed;
                break;
            // Cruceta izq/der: cambiar el consumible elegido de la alforja (X lo usa)
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                actions[static_cast<u32>(Action::BeltPrev)] = pressed;
                break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                actions[static_cast<u32>(Action::BeltNext)] = pressed;
                break;
            default: break;
        }
    }
    else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        f32 value = static_cast<f32>(event.gaxis.value) / 32767.0f;

        switch (event.gaxis.axis) {
            // Left stick - movement
            case SDL_GAMEPAD_AXIS_LEFTX:  move_axis.x = value; break;
            case SDL_GAMEPAD_AXIS_LEFTY:  move_axis.y = value; break;
            // Right stick: unused (shots follow the movement direction; `aim_axis` is reserved
            // for the autoplay bot / scripts)
            // Triggers - attack
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                actions[static_cast<u32>(Action::Attack)] = (value > 0.3f);
                break;
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                actions[static_cast<u32>(Action::AttackSecondary)] = (value > 0.3f);
                break;
        }
    }
}

// =============================================================================
// Touch (from GDD 4.2 - Mobile controls)
// =============================================================================
// Nombre del boton tactil, SOLO para el log. Es lo unico que hace comprobable desde un
// guion que un dedo ha pulsado ARMA y no ORAR: los dos son iconos sin texto y compararlos
// a ojo en una captura de 320x180 no distingue cual respondio.
static const char* touch_btn_name(int i) {
    switch (i) {
        case TOUCH_BTN_DASH:    return "DASH";
        case TOUCH_BTN_CONS:    return "USAR";
        case TOUCH_BTN_WEAPON:  return "ARMA";
        case TOUCH_BTN_PRAY:    return "ORAR";
        case TOUCH_BTN_PAUSE:   return "PAUSA";
        default:                return "MILAGRO";
    }
}

// Los dedos vivos. Cuatro sobran: el unico gesto de varios dedos es "stick + apuntar".
static int find_finger(TouchFinger* list, SDL_FingerID id, bool take_free) {
    for (int i = 0; i < TOUCH_MAX_FINGERS; i++)
        if (list[i].active && list[i].id == id) return i;
    if (!take_free) return -1;
    for (int i = 0; i < TOUCH_MAX_FINGERS; i++)
        if (!list[i].active) return i;
    return -1;
}

void Input::process_touch(const SDL_Event& event) {
    // El dedo viene normalizado a la VENTANA; hay que descontar el letterbox para caer en
    // las mismas unidades de juego que dibuja el HUD. Un dedo en la banda negra da valores
    // fuera de 0..320 / 0..180, que es exactamente lo que la banda inferior del modo
    // vertical ya asumia (ver render/CLAUDE.md).
    Vec2 touch_pos = {
        (event.tfinger.x - touch_vp.x) / touch_vp.w * Config::GAME_WIDTH,
        (event.tfinger.y - touch_vp.y) / touch_vp.h * Config::GAME_HEIGHT
    };

    bool left_half = (touch_pos.x < Config::GAME_WIDTH * 0.5f);

    if (event.type == SDL_EVENT_FINGER_DOWN) {
        int f = find_finger(touch_fingers, event.tfinger.fingerID, true);
        if (f >= 0) touch_fingers[f] = {event.tfinger.fingerID, true, touch_pos, touch_pos};

        // Candidato a tap: se confirma al SOLTAR si el dedo casi no se ha movido. Es lo
        // unico que los menus tactiles necesitan saber del tactil.
        //
        // El PRIMER dedo se queda el tap hasta que lo suelta o se va de paseo. Antes esto
        // se rearmaba en CADA bajada, asi que la palma apoyada o el otro pulgar robaban el
        // tap y al soltar el dedo bueno el id ya no coincidia: el toque no existia. Se
        // rearma igualmente si el dueño ya no esta vivo en la tabla, porque un FINGER_UP
        // perdido (Android no los entrega al volver de segundo plano) dejaria la pantalla
        // muerta para siempre.
        bool owner_alive = false;
        if (tap_active)
            for (const auto& tf : touch_fingers)
                if (tf.active && tf.id == tap_finger) { owner_alive = true; break; }
        if (!owner_alive) {
            tap_active   = true;
            tap_finger   = event.tfinger.fingerID;
            tap_down     = touch_pos;
            tap_now      = touch_pos;
            scroll_accum = 0.0f;
        }

        // Fuera de la partida (menus, historia, cartas) el dedo NO mueve al profeta ni
        // dispara: solo deja el tap para quien lo quiera.
        if (!touch_ui || !touch_gameplay) return;

        // El mapa grande se cierra con cualquier toque, y ese toque no hace nada mas.
        if (touch_map_open) {
            touch_map_open = false;
            tap_active = false;
            return;
        }
        // Boton de pantalla ajeno al HUD (el SALTAR del tutorial). Va ANTES del minimapa y
        // del "tap = ataque", y a diferencia de ellos NO mata el tap: quien lo pinta lo
        // recoge con peek_tap. Sin esto, tocarlo disparaba el cayado de propina.
        if (touch_ui_rect.w > 0.0f && touch_ui_rect.contains(touch_pos)) return;
        // Tocar el minimapa abre el mapa grande (mismo rect que dibuja el HUD).
        if (touch_minimap.w > 0.0f && touch_minimap.contains(touch_pos)) {
            touch_map_open = true;
            tap_active = false;
            return;
        }

        // 1) Los botones se comprueban ANTES que el "tap = ataque": si no, cada milagro
        //    disparaba tambien el cayado.
        for (int bi = 0; bi < TOUCH_BUTTON_COUNT; bi++) {
            TouchButton& b = touch_buttons[bi];
            if (!b.enabled || b.down || !b.rect.contains(touch_pos)) continue;
            SDL_Log("Touch button: %s", touch_btn_name(bi));
            b.down   = true;
            b.hold   = 0.0f;
            b.finger = event.tfinger.fingerID;
            tap_active = false;   // el boton se lo queda: no es un tap de menu
            // Dash y arma responden al bajar el dedo; los milagros, al subirlo (hay que
            // distinguir el tap del hold, ver mas abajo).
            if (b.slot < 0) {
                // Pausa y Orar son pulsaciones de UN frame: mantener el dedo encima no
                // puede pausar dos veces ni encadenar oraciones.
                if (b.action == Action::Pause || b.action == Action::Pray) pulse(b.action);
                // USAR va al SOLTAR, como un milagro: hay que saber si fue un toque (beber)
                // o un mantener (cambiar de consumible).
                else if (b.action == Action::Consumable) { b.hold_fired = false; }
                else actions[static_cast<u32>(b.action)] = true;
            }
            return;
        }

        if (left_half && !joystick_move.active) {
            // Left side: movement joystick
            joystick_move.active = true;
            joystick_move.origin = touch_pos;
            joystick_move.current = touch_pos;
            joystick_move.finger_id = event.tfinger.fingerID;
        }
        else if (!left_half) {
            // Right side: tap to attack (auto-aim direction). Si el dedo ARRASTRA, el
            // mismo gesto pasa a ser apuntado manual (joystick_aim -> aim_axis).
            actions[static_cast<u32>(Action::Attack)] = true;
            touch_aim_active = true;
            touch_aim_finger = event.tfinger.fingerID;
            // Con el stick derecho encendido el apuntado esta vivo desde que el dedo toca;
            // sin el hay que arrastrar TOUCH_DRAG_AIM unidades (ver FINGER_MOTION).
            joystick_aim.active = touch_right_stick;
            // Una linea por gesto, no por frame: es lo unico que hace COMPROBABLE desde un
            // guion si el apuntado se ha enganchado, sin depender de que la captura caiga
            // sobre suelo raso (dos intentos de verificarlo a ojo cayeron sobre un altar
            // del mapa, cuyo adorno se confunde con la mira).
            if (joystick_aim.active) SDL_Log("Touch aim ON (right stick)");
            joystick_aim.origin = touch_pos;
            joystick_aim.current = touch_pos;
            joystick_aim.finger_id = event.tfinger.fingerID;
        }
    }
    else if (event.type == SDL_EVENT_FINGER_MOTION) {
        int f = find_finger(touch_fingers, event.tfinger.fingerID, false);
        Vec2 prev = (f >= 0) ? touch_fingers[f].pos : touch_pos;
        if (f >= 0) touch_fingers[f].pos = touch_pos;

        // Donde esta AHORA el dedo del tap: es lo que ilumina el boton que hay debajo. Se
        // apunta ANTES del test de slop, porque en un menu esta rama se va por el `return`
        // de abajo y no llegaria nunca.
        if (tap_active && event.tfinger.fingerID == tap_finger) tap_now = touch_pos;

        // Un dedo que se pasea deja de ser un tap. En un menu se permite mas deriva: un
        // pulgar se desplaza al apoyarse y 6 unidades son ~2,2 mm.
        if (tap_active && event.tfinger.fingerID == tap_finger &&
            (touch_pos - tap_down).length() > (touch_gameplay ? TOUCH_TAP_SLOP : TOUCH_UI_TAP_SLOP)) {
            tap_active = false;
        }

        // Con el juego parado, arrastrar recorre la lista (Codice, Santuario). Scroll
        // natural: el dedo sube, la seleccion baja.
        if (!touch_ui || !touch_gameplay) {
            if (event.tfinger.fingerID == tap_finger) {
                scroll_accum += touch_pos.y - prev.y;
                // UN paso por evento, no todos los que quepan: `pulse` sobre la misma
                // accion es una pulsacion, asi que dos pasos en el mismo frame contaban
                // como uno y el resto se perdia (un arrastre rapido recorria la mitad de
                // lo que decia el dedo). Lo que sobra se queda en el acumulador y sale en
                // el evento siguiente, que con un dedo real llegan a 60-120 por segundo.
                if (scroll_accum <= -TOUCH_SCROLL_STEP) {
                    pulse(Action::MoveDown);
                    scroll_accum += TOUCH_SCROLL_STEP;
                    tap_active = false;   // lo que recorre la lista no pulsa ademas la fila
                } else if (scroll_accum >= TOUCH_SCROLL_STEP) {
                    pulse(Action::MoveUp);
                    scroll_accum -= TOUCH_SCROLL_STEP;
                    tap_active = false;
                }
            }
            return;
        }

        // Un dedo que se sale del boton lo suelta: si no, se quedaba pulsado para siempre
        for (auto& b : touch_buttons) {
            if (!b.down || b.finger != event.tfinger.fingerID) continue;
            if (b.rect.contains(touch_pos)) continue;
            b.down = false;
            b.hold = 0.0f;
            if (b.slot < 0) actions[static_cast<u32>(b.action)] = false;
        }

        if (joystick_move.active && event.tfinger.fingerID == joystick_move.finger_id) {
            joystick_move.current = touch_pos;
        }
        if (touch_aim_active && event.tfinger.fingerID == touch_aim_finger) {
            joystick_aim.current = touch_pos;
            // Hold + drag: pasado el umbral, el gesto apunta a mano y apaga el auto-aim
            if (!joystick_aim.active && !touch_right_stick &&
                (touch_pos - joystick_aim.origin).length() > TOUCH_DRAG_AIM / ((aim_sensitivity > 0.05f) ? aim_sensitivity : 1.0f)) {
                joystick_aim.active = true;
                SDL_Log("Touch aim ON (drag threshold)");
            }
        }
    }
    else if (event.type == SDL_EVENT_FINGER_UP) {
        int f = find_finger(touch_fingers, event.tfinger.fingerID, false);
        if (f >= 0) touch_fingers[f].active = false;

        // El tap se confirma aqui, tambien con el juego corriendo: es lo que lee la UI.
        if (tap_active && event.tfinger.fingerID == tap_finger &&
            (touch_pos - tap_down).length() <= (touch_gameplay ? TOUCH_TAP_SLOP : TOUCH_UI_TAP_SLOP)) {
            tap_pending = true;
            tap_pos     = tap_down;
        }
        if (event.tfinger.fingerID == tap_finger) {
            tap_active   = false;
            scroll_accum = 0.0f;
        }

        // Las SUELTAS se atienden siempre, tambien si el juego ya no esta en Playing: el
        // dedo que estaba disparando al abrir la pausa tiene que soltarse igual.
        if (touch_ui) {
            bool handled = false;
            for (auto& b : touch_buttons) {
                if (!b.down || b.finger != event.tfinger.fingerID) continue;
                if (b.slot >= 0 && b.hold < TOUCH_HOLD_INFO) {
                    // Tap corto sobre un milagro: se lanza ahora, un solo frame
                    pulse(b.action);
                } else if (b.action == Action::Consumable) {
                    if (!b.hold_fired) pulse(Action::Consumable);   // toque corto: beber
                    b.hold_fired = false;
                } else if (b.slot < 0) {
                    actions[static_cast<u32>(b.action)] = false;
                }
                b.down = false;
                b.hold = 0.0f;
                handled = true;
            }
            if (handled) return;
        }

        if (joystick_move.active && event.tfinger.fingerID == joystick_move.finger_id) {
            joystick_move.active = false;
        }
        if (touch_aim_active && event.tfinger.fingerID == touch_aim_finger) {
            touch_aim_active = false;
            joystick_aim.active = false;
            aim_axis = {0, 0};   // se suelta el dedo: vuelve el auto-aim
            actions[static_cast<u32>(Action::Attack)] = false;
        }
    }
}

// =============================================================================
// Axis calculation
// =============================================================================
void Input::update_axes() {
    // Reset axes each frame — only active input keeps them non-zero
    move_axis = {0, 0};

    // Keyboard overrides analog stick for movement
    Vec2 kb_move = {0, 0};
    if (actions[static_cast<u32>(Action::MoveLeft)])  kb_move.x -= 1.0f;
    if (actions[static_cast<u32>(Action::MoveRight)]) kb_move.x += 1.0f;
    if (actions[static_cast<u32>(Action::MoveUp)])    kb_move.y -= 1.0f;
    if (actions[static_cast<u32>(Action::MoveDown)])  kb_move.y += 1.0f;

    if (kb_move.length_sq() > 0.01f) {
        move_axis = kb_move.normalized();
    } else if (joystick_move.active) {
        move_axis = joystick_move.get_axis();
    }
    // Gamepad axis is already set from events

    // Apuntado manual tactil (hold + drag). Mientras dure, `aim_axis` no es cero y
    // Player::update deja de pedirle direccion al auto-aim.
    // La sensibilidad divide el radio: con 10 el pulgar llega al tope arrastrando menos de
    // la mitad que con 1. Se aplica aqui y no al crear el joystick para que cambiarla en
    // Opciones se note en el frame siguiente.
    if (joystick_aim.active) {
        f32 k = (aim_sensitivity > 0.05f) ? aim_sensitivity : 1.0f;
        VirtualJoystick scaled = joystick_aim;
        scaled.radius = joystick_aim.radius / k;
        aim_axis = scaled.get_axis();
    }

    apply_deadzone(move_axis);
    apply_deadzone(aim_axis);
}

void Input::apply_deadzone(Vec2& axis) const {
    f32 len = axis.length();
    if (len < stick_deadzone) {
        axis = {0, 0};
    } else {
        // Remap from deadzone..1 to 0..1
        f32 remapped = (len - stick_deadzone) / (1.0f - stick_deadzone);
        axis = axis.normalized() * remapped;
    }
}

// =============================================================================
// Action Queries
// =============================================================================
bool Input::is_action_pressed(Action a) const {
    return actions[static_cast<u32>(a)];
}

bool Input::is_action_just_pressed(Action a) const {
    u32 i = static_cast<u32>(a);
    return actions[i] && !actions_prev[i];
}

bool Input::is_action_just_released(Action a) const {
    u32 i = static_cast<u32>(a);
    return !actions[i] && actions_prev[i];
}

Vec2 Input::get_move_axis() const {
    return move_axis;
}

Vec2 Input::get_aim_axis() const {
    return aim_axis;
}

// =============================================================================
// Rumble
// =============================================================================
void Input::rumble(f32 strength, u32 ms) {
    // Puerta unica del ajuste "Vibracion" (GDD 4.2). En un telefono, no poder apagarla es
    // un defecto de accesibilidad y de bateria, no una preferencia estetica.
    if (!rumble_enabled || strength <= 0.0f) return;
    f32 k = std::min(1.0f, strength);
    if (gamepad) {
        // Motor grave para el peso, agudo para el chasquido: los dos a la vez se sienten
        // como un golpe, solo el agudo se siente como una notificacion del movil.
        u16 low  = static_cast<u16>(0xFFFF * k * 0.85f);
        u16 high = static_cast<u16>(0xFFFF * k);
        SDL_RumbleGamepad(gamepad, low, high, ms);
    }
    // El telefono: un solo motor, asi que la intensidad es el propio `k`.
    if (haptic) SDL_PlayHapticRumble(haptic, k, ms);
}
