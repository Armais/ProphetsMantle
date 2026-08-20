// =============================================================================
// MANTLE - Input System Implementation
// =============================================================================

#include "input/input.h"
#include "platform/platform.h"
#include <cstring>
#include <cmath>

void Input::init() {
    std::memset(actions, 0, sizeof(actions));
    std::memset(actions_prev, 0, sizeof(actions_prev));

    auto_aim_enabled = is_mobile();

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

void Input::pre_update() {
    std::memcpy(actions_prev, actions, sizeof(actions));
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
            case SDL_GAMEPAD_BUTTON_NORTH:         // Y/Triangle - Interact
                actions[static_cast<u32>(Action::Interact)] = pressed;
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
void Input::process_touch(const SDL_Event& event) {
    // Convert normalized touch coords to game resolution
    Vec2 touch_pos = {
        event.tfinger.x * Config::GAME_WIDTH,
        event.tfinger.y * Config::GAME_HEIGHT
    };

    bool left_half = (touch_pos.x < Config::GAME_WIDTH * 0.5f);

    if (event.type == SDL_EVENT_FINGER_DOWN) {
        if (left_half && !joystick_move.active) {
            // Left side: movement joystick
            joystick_move.active = true;
            joystick_move.origin = touch_pos;
            joystick_move.current = touch_pos;
            joystick_move.finger_id = event.tfinger.fingerID;
        }
        else if (!left_half) {
            // Right side: tap to attack (auto-aim direction)
            actions[static_cast<u32>(Action::Attack)] = true;
        }
    }
    else if (event.type == SDL_EVENT_FINGER_MOTION) {
        if (joystick_move.active && event.tfinger.fingerID == joystick_move.finger_id) {
            joystick_move.current = touch_pos;
        }
        if (joystick_aim.active && event.tfinger.fingerID == joystick_aim.finger_id) {
            joystick_aim.current = touch_pos;
        }
    }
    else if (event.type == SDL_EVENT_FINGER_UP) {
        if (joystick_move.active && event.tfinger.fingerID == joystick_move.finger_id) {
            joystick_move.active = false;
        }
        if (joystick_aim.active && event.tfinger.fingerID == joystick_aim.finger_id) {
            joystick_aim.active = false;
        }
        // Release attack on touch up
        actions[static_cast<u32>(Action::Attack)] = false;
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
