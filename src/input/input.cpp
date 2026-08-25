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
    // Fila de arriba: consumible y dash, alineados con las celdas E y Q. A 68 y no a 64
    // del borde para dejar libre la franja de las placas de coste de los milagros.
    touch_buttons[0] = {{W - 28.0f, H - 68.0f, S, S}, Action::Dash,       -1};
    touch_buttons[1] = {{W - 55.0f, H - 68.0f, S, S}, Action::Consumable, -1};
    // Fila de abajo: las tres celdas de milagro del HUD, tal cual
    for (int i = 0; i < Config::MAX_MIRACLE_SLOTS && i < 3; i++)
        touch_buttons[2 + i] = {{W - 28.0f - i * 27.0f, H - 36.0f, S, S},
                                static_cast<Action>(static_cast<u32>(Action::Miracle1) + i), i};
}

// Un tap sobre un milagro lo lanza; mantenerlo abre su ficha (GDD 4.2). Por eso el
// milagro NO se lanza al bajar el dedo: hay que saber antes cuanto ha durado.
void Input::update_touch(f32 dt) {
    if (!touch_ui) return;
    touch_info_slot = -1;
    for (auto& b : touch_buttons) {
        if (!b.down) { b.hold = 0.0f; continue; }
        b.hold += dt;
        if (b.slot >= 0 && b.hold >= TOUCH_HOLD_INFO) touch_info_slot = b.slot;
    }
}

void Input::pre_update() {
    std::memcpy(actions_prev, actions, sizeof(actions));
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
        case SDL_SCANCODE_TAB:    actions[static_cast<u32>(Action::Map)]       = pressed; break;
        case SDL_SCANCODE_ESCAPE: actions[static_cast<u32>(Action::Pause)]     = pressed;
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
// Los dedos vivos, para el gesto de dos dedos (pausa). Cuatro sobran: el GDD no pide
// ningun gesto de mas de dos.
static int find_finger(TouchFinger* list, SDL_FingerID id, bool take_free) {
    for (int i = 0; i < TOUCH_MAX_FINGERS; i++)
        if (list[i].active && list[i].id == id) return i;
    if (!take_free) return -1;
    for (int i = 0; i < TOUCH_MAX_FINGERS; i++)
        if (!list[i].active) return i;
    return -1;
}

void Input::process_touch(const SDL_Event& event) {
    // Convert normalized touch coords to game resolution
    Vec2 touch_pos = {
        event.tfinger.x * Config::GAME_WIDTH,
        event.tfinger.y * Config::GAME_HEIGHT
    };

    bool left_half = (touch_pos.x < Config::GAME_WIDTH * 0.5f);

    if (event.type == SDL_EVENT_FINGER_DOWN) {
        int f = find_finger(touch_fingers, event.tfinger.fingerID, true);
        if (f >= 0) touch_fingers[f] = {event.tfinger.fingerID, true, touch_pos, touch_pos};

        // 1) Los botones se comprueban ANTES que el "tap = ataque": si no, cada milagro
        //    disparaba tambien el cayado.
        if (touch_ui) {
            for (auto& b : touch_buttons) {
                if (b.down || !b.rect.contains(touch_pos)) continue;
                b.down   = true;
                b.hold   = 0.0f;
                b.finger = event.tfinger.fingerID;
                // Dash y consumible responden al bajar el dedo; los milagros, al subirlo
                // (hay que distinguir el tap del hold, ver mas abajo).
                if (b.slot < 0) actions[static_cast<u32>(b.action)] = true;
                return;
            }
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
        if (f >= 0) touch_fingers[f].pos = touch_pos;

        // Swipe de dos dedos = pausa. Basta con que UNO de los dos recorra el umbral
        // mientras hay dos apoyados: pedir que los dos se muevan a la vez fallaba mas
        // que acertaba.
        if (touch_ui && !touch_swipe_fired) {
            int down_count = 0;
            for (auto& tf : touch_fingers) if (tf.active) down_count++;
            if (down_count >= 2 && f >= 0 &&
                (touch_fingers[f].pos - touch_fingers[f].start).length() > TOUCH_SWIPE_PAUSE) {
                actions[static_cast<u32>(Action::Pause)] = true;
                touch_pulse[static_cast<u32>(Action::Pause)] = true;
                touch_swipe_fired = true;
            }
        }

        // Un dedo que se sale del boton lo suelta: si no, se quedaba pulsado para siempre
        if (touch_ui) {
            for (auto& b : touch_buttons) {
                if (!b.down || b.finger != event.tfinger.fingerID) continue;
                if (b.rect.contains(touch_pos)) continue;
                b.down = false;
                b.hold = 0.0f;
                if (b.slot < 0) actions[static_cast<u32>(b.action)] = false;
            }
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
        int still_down = 0;
        for (auto& tf : touch_fingers) if (tf.active) still_down++;
        if (still_down == 0) touch_swipe_fired = false;

        if (touch_ui) {
            bool handled = false;
            for (auto& b : touch_buttons) {
                if (!b.down || b.finger != event.tfinger.fingerID) continue;
                if (b.slot >= 0 && b.hold < TOUCH_HOLD_INFO) {
                    // Tap corto sobre un milagro: se lanza ahora, un solo frame
                    actions[static_cast<u32>(b.action)] = true;
                    touch_pulse[static_cast<u32>(b.action)] = true;
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
    if (!rumble_enabled) return;
    if (!gamepad || strength <= 0.0f) return;
    f32 k = std::min(1.0f, strength);
    // Motor grave para el peso, agudo para el chasquido: los dos a la vez se sienten
    // como un golpe, solo el agudo se siente como una notificacion del movil.
    u16 low  = static_cast<u16>(0xFFFF * k * 0.85f);
    u16 high = static_cast<u16>(0xFFFF * k);
    SDL_RumbleGamepad(gamepad, low, high, ms);
}
