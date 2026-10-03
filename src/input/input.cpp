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
    relayout_touch();

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

// =============================================================================
// Layout tactil (2026-10-03)
// =============================================================================
// Posicion de cada boton RESPECTO DEL CENTRO DE DASH, a escala 1 y para diestros (el zurdo
// refleja la x). Dos arcos: los milagros a 38 unidades (180, 135 y 90 grados) y los
// pequeños a ~66 (170, 135 y 100), intercalados para que ninguno tape a otro. Comprobado a
// mano: el hueco mas estrecho entre dos bordes es de 3,2 unidades (M1-M2, M2-M3), mayor que
// cero con cualquier escala porque todo escala junto.
static const Vec2 TOUCH_OFFSETS[TOUCH_BUTTON_COUNT] = {
    {  0.0f,   0.0f},   // DASH
    {-65.0f, -11.5f},   // USAR
    {-38.0f,   0.0f},   // milagro 1
    {-27.0f, -27.0f},   // milagro 2
    {  0.0f, -38.0f},   // milagro 3
    {-46.7f, -46.7f},   // ARMA
    {-11.0f, -65.0f},   // ORAR
    {  0.0f,   0.0f},   // pausa: va aparte
};
static const f32 TOUCH_RADII[TOUCH_BUTTON_COUNT] = {
    TOUCH_DASH_R, TOUCH_MINOR_R, TOUCH_MIRACLE_R, TOUCH_MIRACLE_R, TOUCH_MIRACLE_R,
    TOUCH_MINOR_R, TOUCH_MINOR_R, 0.0f,
};
// Lo que el grupo se extiende desde el centro de DASH (izquierda / arriba), para acotar la
// escala: un "Tamaño 150%" en una pantalla pequeña no puede meter los botones en la mitad
// del stick ni subirlos hasta el minimapa.
static constexpr f32 TOUCH_REACH_X = 65.0f + TOUCH_MINOR_R;
static constexpr f32 TOUCH_REACH_Y = 65.0f + TOUCH_MINOR_R;

void Input::init_touch_layout(const TouchLayoutParams& p) {
    touch_params = p;
    const Rect sa = p.safe;
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);

    // Escala: el tamaño fisico (DPI) por el ajuste del jugador, acotada para que el grupo
    // quepa entre el centro de la pantalla y la esquina, y por debajo de la franja del
    // minimapa y el objetivo (y < 58 en apaisado).
    f32 s = p.dpi_scale * p.user_scale;
    const f32 room_x = (sa.w * 0.5f) - TOUCH_EDGE;
    const f32 room_y = sa.h - TOUCH_EDGE - 58.0f;
    const f32 need_x = TOUCH_REACH_X + TOUCH_DASH_R;
    const f32 need_y = TOUCH_REACH_Y + TOUCH_DASH_R;
    if (need_x * s > room_x) s = room_x / need_x;
    if (need_y * s > room_y && room_y > 0.0f) s = std::min(s, room_y / need_y);
    s = std::max(0.6f, std::min(1.8f, s));
    touch_scale = s;
    touch_slop  = TOUCH_HIT_SLOP * s;

    // Reflejo del zurdo alrededor del centro de la ZONA SEGURA (no del area de juego): con
    // un notch a un lado los dos grupos quedan igual de lejos de su borde.
    const f32 mx = sa.x * 2.0f + sa.w;
    auto mirror = [&](f32 x) { return p.lefty ? mx - x : x; };

    // DASH en la esquina inferior del lado de los botones
    const f32 dr = TOUCH_DASH_R * s;
    const Vec2 c = {mirror(sa.right() - TOUCH_EDGE * s - dr), sa.bottom() - TOUCH_EDGE * s - dr};
    static const Action ACTS[TOUCH_BUTTON_COUNT] = {
        Action::Dash, Action::Consumable, Action::Miracle1, Action::Miracle2, Action::Miracle3,
        Action::AttackSecondary, Action::Pray, Action::Pause,
    };
    for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
        TouchButton& b = touch_buttons[i];
        b.action = ACTS[i];
        b.slot   = (i >= TOUCH_BTN_MIRACLE && i < TOUCH_BTN_MIRACLE + 3) ? i - TOUCH_BTN_MIRACLE : -1;
        if (i == TOUCH_BTN_PAUSE) continue;
        const Vec2 off = TOUCH_OFFSETS[i] * s;
        b.radius = TOUCH_RADII[i] * s;
        b.center = {c.x + (p.lefty ? -off.x : off.x), c.y + off.y};
        b.rect   = {b.center.x - b.radius, b.center.y - b.radius, b.radius * 2.0f, b.radius * 2.0f};
    }

    // Pausa. Arriba, y en el PILAR del lado de los botones si cabe (un 20:9 tiene ~40
    // unidades de pilar): ahi no tapa nada del mundo ni del HUD. En 16:9 se queda arriba al
    // centro, en el hueco entre el panel de vitales y el minimapa, como siempre. Su tamaño
    // NO escala: es el unico boton que hay que encontrar sin mirar, y no debe moverse.
    {
        TouchButton& b = touch_buttons[TOUCH_BTN_PAUSE];
        const f32 PW = 26.0f, PH = 18.0f;   // ~9,4 x 6,5 mm, mas el margen del dedo
        const f32 pillar = p.lefty ? (0.0f - sa.x) : (sa.right() - W);
        Rect r = {W * 0.5f - PW * 0.5f, std::max(3.0f, sa.y + 3.0f), PW, PH};
        if (pillar >= PW + 6.0f) {
            const f32 px = p.lefty ? sa.x + (pillar - PW) * 0.5f
                                   : W + (pillar - PW) * 0.5f;
            r = {px, sa.y + 6.0f, PW, PH};
        }
        b.rect = r;
        b.radius = 0.0f;
        b.center = r.center();
    }

    // Stick de movimiento: su radio escala con los botones (mismo pulgar) y su reposo va en
    // la esquina contraria a DASH, a la misma altura, para que los dos pulgares descansen
    // simetricos.
    joy_radius = TOUCH_JOY_R * s;
    joystick_move.radius = joy_radius;
    joy_rest = {mirror(sa.x + TOUCH_EDGE * s + joy_radius + 8.0f * s), c.y - 4.0f * s};

    SDL_Log("Touch layout: area %.0f,%.0f %.0fx%.0f  escala %.2f (dpi %.2f x %.2f)%s  DASH (%.0f,%.0f) pausa (%.0f,%.0f)",
            sa.x, sa.y, sa.w, sa.h, s, p.dpi_scale, p.user_scale, p.lefty ? "  ZURDO" : "",
            c.x, c.y, touch_buttons[TOUCH_BTN_PAUSE].center.x, touch_buttons[TOUCH_BTN_PAUSE].center.y);
}

int Input::touch_button_at(Vec2 p) const {
    int best = -1;
    f32 best_d = touch_slop;
    for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
        const TouchButton& b = touch_buttons[i];
        if (!b.enabled) continue;
        const f32 d = b.edge_distance(p);
        if (d <= best_d) { best = i; best_d = d; }
    }
    return best;
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
        if (b.deny_t > 0.0f) b.deny_t = std::max(0.0f, b.deny_t - dt);
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

    // La mitad del stick: la izquierda, o la derecha con el ajuste de zurdo.
    const bool move_half = in_move_half(touch_pos);

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
        //    disparaba tambien el cayado. Con margen de dedo: si dos margenes se solapan,
        //    gana el boton con el borde mas cerca.
        const int bi = touch_button_at(touch_pos);
        if (bi >= 0 && !touch_buttons[bi].down) {
            TouchButton& b = touch_buttons[bi];
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
        if (bi >= 0) return;   // ese boton ya lo tiene otro dedo: este no dispara de propina

        if (move_half && !joystick_move.active) {
            // Mitad del stick: el joystick de movimiento nace donde baja el dedo
            joystick_move.active = true;
            joystick_move.origin = touch_pos;
            joystick_move.current = touch_pos;
            joystick_move.finger_id = event.tfinger.fingerID;
            joystick_move.radius = joy_radius;
        }
        else if (!move_half) {
            // La otra mitad: tap to attack (auto-aim direction). Si el dedo ARRASTRA, el
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

        // Un dedo que se sale del boton lo suelta: si no, se quedaba pulsado para siempre.
        // Con el DOBLE de margen que al pulsar: un pulgar que se apoya rueda un poco, y
        // soltar el DASH por rozar el borde es lo que se siente como "no responde".
        for (auto& b : touch_buttons) {
            if (!b.down || b.finger != event.tfinger.fingerID) continue;
            if (b.edge_distance(touch_pos) <= touch_slop * 2.0f) continue;
            b.down = false;
            b.hold = 0.0f;
            if (b.slot < 0) actions[static_cast<u32>(b.action)] = false;
        }

        if (joystick_move.active && event.tfinger.fingerID == joystick_move.finger_id) {
            joystick_move.current = touch_pos;
            joystick_move.follow();   // la base va detras del dedo si este se pasa del radio
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
