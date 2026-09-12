#pragma once

// =============================================================================
// MANTLE - Unified Input System
// =============================================================================
//
// Abstracts keyboard, mouse, gamepad, and touch into a single action system.
// Maps from GDD section 4.2-4.3:
//
// PC:     WASD/Mouse/Gamepad
// Mobile: Virtual joystick + touch buttons
//
// =============================================================================

#include <SDL3/SDL.h>
#include "core/types.h"
#include "core/config.h"

// =============================================================================
// Actions (game-level, device-agnostic)
// =============================================================================
enum class Action : u32 {
    // Movement
    MoveUp = 0,
    MoveDown,
    MoveLeft,
    MoveRight,

    // Combat
    Attack,         // K / RT / Tap zona derecha (dispara hacia la direccion de movimiento)
    AttackSecondary,// Click Der / LT
    Dash,           // Espacio / A(Xbox) / Boton Dash
    Miracle1,       // Q / RB / Boton Milagro 1
    Miracle2,       // E / LB / Boton Milagro 2
    Miracle3,       // F / Y(Xbox) / Boton Milagro 3 (solo con la mejora "Doble Porcion")
    Consumable,     // R / X

    // UI
    Pause,          // Esc / Start / Boton II / Atras de Android
    Map,            // Tab / Select
    Interact,       // Y
    Pray,           // D-Pad Up
    Confirm,        // Enter / A
    Cancel,         // Esc / B

    COUNT
};

constexpr u32 ACTION_COUNT = static_cast<u32>(Action::COUNT);

// =============================================================================
// Touch: Virtual Joystick
// =============================================================================
struct VirtualJoystick {
    bool active    = false;
    Vec2 origin    = {0, 0};  // Where finger touched down
    Vec2 current   = {0, 0};  // Current finger position
    f32  radius    = 40.0f;   // Max deflection in game units
    SDL_FingerID finger_id = 0;

    Vec2 get_axis() const {
        if (!active) return {0, 0};
        Vec2 delta = current - origin;
        f32 len = delta.length();
        if (len < 3.0f) return {0, 0};  // Dead zone
        if (len > radius) delta = delta.normalized() * radius;
        return delta / radius;  // Normalized -1..1
    }
};

// =============================================================================
// Touch: botones en pantalla (GDD 4.2)
// =============================================================================
// Rects en UNIDADES DE JUEGO (320x180), las mismas que dibuja ui/hud.cpp: en un
// telefono el HUD ES el mando, asi que las tres celdas de milagro del HUD son a la vez
// los tres botones de milagro. Dash y consumible se apilan justo encima, en la unica
// franja de la derecha que no pisa ni el minimapa (arriba) ni la barra de jefe (centro
// abajo, x 80..240).
//
//                    [ II ]                 y 3..17  (arriba, centrado)
//   [Orar] [Arma] [Cons] [Dash]    y 112..134
//           [F]   [ E ]  [ Q ]     y 144..166
//
// ORAR y ARMA solo existen si el profeta puede rezar / lleva arma secundaria: por eso
// cada boton lleva `enabled`, que apaga a la vez el DIBUJO y el HIT-TEST. Antes el
// hit-test probaba los cinco rects aunque el HUD no pintara ninguno, y quedaban huecos
// muertos donde el dedo no hacia nada ni disparaba.
struct TouchButton {
    Rect   rect   = {0, 0, 0, 0};
    Action action = Action::COUNT;
    int    slot   = -1;     // slot de milagro (0..2); -1 = no es un milagro
    SDL_FingerID finger = 0;
    bool   down   = false;
    bool   enabled = true;  // false = ni se dibuja ni responde
    f32    hold   = 0.0f;   // segundos pulsado
};

// dash, consumible, milagro 1-3, arma secundaria, orar, pausa
constexpr int TOUCH_BUTTON_COUNT = 8;
constexpr int TOUCH_BTN_DASH     = 0;
constexpr int TOUCH_BTN_CONS     = 1;
constexpr int TOUCH_BTN_MIRACLE  = 2;   // 2, 3, 4
constexpr int TOUCH_BTN_WEAPON   = 5;
constexpr int TOUCH_BTN_PRAY     = 6;
constexpr int TOUCH_BTN_PAUSE    = 7;
constexpr int TOUCH_MAX_FINGERS  = 4;
constexpr f32 TOUCH_HOLD_INFO    = 0.4f; // hold sobre un milagro = ficha, no lanzarlo
constexpr f32 TOUCH_DRAG_AIM     = 6.0f; // unidades que hay que arrastrar para apuntar
constexpr f32 TOUCH_TAP_SLOP     = 6.0f; // un dedo que se mueve mas que esto no es un tap
constexpr f32 TOUCH_SCROLL_STEP  = 10.0f;// unidades de arrastre por paso de lista

struct TouchFinger {
    SDL_FingerID id     = 0;
    bool         active = false;
    Vec2         start  = {0, 0};
    Vec2         pos    = {0, 0};
};

// =============================================================================
// Input State
// =============================================================================
struct Input {
    // Action state (current and previous frame)
    bool actions[ACTION_COUNT]      = {};
    bool actions_prev[ACTION_COUNT] = {};

    // Analog axes
    Vec2 move_axis  = {0, 0};   // Left stick / WASD / joystick izq
    Vec2 aim_axis   = {0, 0};   // Aim override used ONLY by autoplay scripts/bot (normal play: shots follow movement)
    Vec2 mouse_pos  = {0, 0};   // Mouse position in window coordinates
    Vec2 mouse_game = {0, 0};   // Mouse position in game coordinates (320x180), set by Engine

    // Gamepad
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID gamepad_id = 0;

    // Vibrador del telefono (SDL3 trae backend haptico en Android). Solo se abre en movil:
    // en escritorio la vibracion sigue siendo cosa del mando y nada cambia.
    SDL_Haptic* haptic = nullptr;

    // Touch (mobile)
    VirtualJoystick joystick_move;
    VirtualJoystick joystick_aim;  // Apuntado manual: hold + drag en la mitad derecha

    // Controles tactiles en pantalla. `touch_ui` los enciende (movil, pantalla tactil
    // detectada o --touch): sin el, process_touch se comporta como siempre y en
    // escritorio no cambia nada.
    bool        touch_ui = false;
    TouchButton touch_buttons[TOUCH_BUTTON_COUNT];
    TouchFinger touch_fingers[TOUCH_MAX_FINGERS];
    // Pulsaciones de un solo frame (tap sobre un boton de milagro). Se sueltan solas en
    // el pre_update siguiente: un tap no dura lo que dura el dedo encima.
    bool touch_pulse[ACTION_COUNT] = {};
    int  touch_info_slot   = -1;      // slot cuya ficha se esta mostrando (hold), -1 = ninguna
    bool touch_aim_active  = false;   // hay un dedo apuntando a mano en la mitad derecha
    SDL_FingerID touch_aim_finger = 0;

    // Solo en `Playing` hay stick, ataque por tap y botones. Con esto apagado el dedo no
    // mueve al profeta desde un menu, pero las SUELTAS se siguen procesando siempre: si no,
    // el dedo que estaba disparando al pausar se quedaba pulsado al volver.
    bool touch_gameplay = false;

    // Tap para la UI: un dedo que baja y sube casi en el mismo sitio. Lo consume
    // `take_tap`, y `pre_update` lo tira si nadie lo recogio (un tap no dura dos frames).
    bool tap_pending = false;
    Vec2 tap_pos     = {0, 0};
    Vec2 tap_down    = {0, 0};
    SDL_FingerID tap_finger = 0;
    bool tap_active  = false;
    f32  scroll_accum = 0.0f;      // arrastre vertical acumulado con el juego parado

    // Minimapa tocable: tocarlo abre/cierra el mapa grande. El rect lo escribe Engine con
    // HUD::minimap_rect, para que dibujo y hit-test salgan de la MISMA formula.
    Rect touch_minimap  = {0, 0, 0, 0};
    bool touch_map_open = false;

    // Viewport del renderer en fracciones de ventana. Los eventos de dedo de SDL estan
    // normalizados a la VENTANA, no al area de juego: sin esta transformacion un movil de
    // 20:9 responde a los botones ~200 px a la derecha de donde se dibujan. El raton ya
    // pasaba por Renderer::window_to_game; esto es su equivalente para el tactil.
    // {0,0,1,1} = sin letterbox (lo que hacia el codigo viejo).
    Rect touch_vp = {0.0f, 0.0f, 1.0f, 1.0f};
    void set_touch_viewport(Rect vp) {
        if (vp.w > 0.0f && vp.h > 0.0f) touch_vp = vp;
    }

    // Ajustes del jugador (GDD 4.2). Los escribe Engine::apply_settings_from_save desde
    // SaveData; Input no sabe que existe un fichero de guardado.
    //
    // `aim_mode`: Off / Assisted / On (ver Config::AimMode). Sustituye al viejo
    // `auto_aim_enabled`, que era binario y no podia expresar el estado intermedio que
    // pide el GDD. Quien solo quiera saber si el tiro se FIJA tiene auto_aim_locks().
    Config::AimMode aim_mode = Config::AimMode::Off;
    Vec2  auto_aim_target  = {0, 0};

    // Multiplicador de sensibilidad de apuntado (1.0 = ajuste 5). Cuanto MAS alto, menos
    // hay que arrastrar: divide el radio del joystick y el umbral de arrastre.
    f32   aim_sensitivity  = 1.0f;

    // Vibracion (GDD 4.2). Es la UNICA puerta: el motor llama a rumble() sin preguntar,
    // asi que apagarla aqui la apaga en todo el juego.
    bool  rumble_enabled   = true;

    // Segundo joystick virtual en la mitad derecha (GDD 4.2, "opcional"). Con el apagado,
    // el dedo derecho se comporta como siempre: tap = disparo con auto-mira, y hay que
    // ARRASTRAR TOUCH_DRAG_AIM unidades para que el apuntado registre. Con el encendido el
    // stick nace y apunta DESDE EL PRIMER PIXEL, y se ve en pantalla. Esa es toda la
    // diferencia, y es justo lo que separa un twin-stick de un hold+drag.
    bool  touch_right_stick = false;

    bool auto_aim_locks() const { return aim_mode == Config::AimMode::On; }

    // Deadzone for analog sticks
    f32 stick_deadzone = 0.15f;

    // Init
    void init();
    void shutdown();   // cierra el haptico; el mando lo cierra SDL_Quit
    // Rects de los botones tactiles, en unidades de juego. `band_below` son las unidades
    // de banda negra que hay bajo el area de juego (0 en apaisado): si caben, los botones
    // BAJAN a la banda, que es donde estan los pulgares en un movil en vertical y donde no
    // tapan el mundo. Los rects se definen aqui UNA SOLA VEZ y el HUD los reusa: ese
    // contrato es lo que impide que el dibujo y el hit-test se separen.
    void init_touch_layout(f32 band_below = 0.0f);
    f32  touch_band = 0.0f;       // la banda con la que se monto el layout actual
    // Enciende/apaga los botones contextuales. Lo llama Engine::update cada paso: el HUD
    // y el hit-test leen el MISMO `enabled`, asi que no pueden desincronizarse.
    void set_touch_enabled(bool consumable, int miracle_slots, bool weapon, bool pray, bool pause);

    // Consume el tap pendiente (coordenadas de juego). Devuelve false si no hay ninguno.
    // Igual que take_tap pero SIN consumirlo: lo necesita quien tiene que mirar si el
    // toque cae en un boton concreto antes de decidir quien se lo queda (el boton de
    // SALTAR en una escena guionizada, que se atiende antes que el resto de la pantalla).
    bool peek_tap(Vec2& out) const {
        if (!tap_pending) return false;
        out = tap_pos;
        return true;
    }

    bool take_tap(Vec2& out) {
        if (!tap_pending) return false;
        tap_pending = false;
        out = tap_pos;
        return true;
    }
    // Pulsacion de un solo frame, como un tap sobre un milagro. La usa la UI tactil para
    // hablar el mismo idioma que el teclado (Confirm, MoveLeft/Right...) sin que ningun
    // menu tenga que saber que existe el tactil.
    void pulse(Action a) {
        actions[static_cast<u32>(a)] = true;
        touch_pulse[static_cast<u32>(a)] = true;
    }
    // Suelta TODO el estado tactil. Al irse a segundo plano Android no entrega los
    // FINGER_UP y el profeta volvia corriendo y disparando.
    void release_touch();
    void update_touch(f32 dt);    // temporizadores de hold (ficha del milagro)

    // Per-frame
    void pre_update();
    void process_event(const SDL_Event& event);
    void post_update();

    // Marca las pulsaciones de este frame como "ya vistas". El bucle usa timestep fijo y
    // puede llamar a update() varias veces por frame: sin esto un solo ENTER se contaba
    // una vez por paso y se comia paginas enteras de la pantalla de historia.
    void consume_presses();

    // Action queries
    bool is_action_pressed(Action a) const;
    bool is_action_just_pressed(Action a) const;
    bool is_action_just_released(Action a) const;

    // Axis queries
    Vec2 get_move_axis() const;
    Vec2 get_aim_axis() const;

    // Device detection
    bool has_gamepad() const { return gamepad != nullptr; }

    // Rumble. `strength` 0..1, `ms` la duracion. En escritorio es el mando; en movil, el
    // vibrador del propio telefono. Sin ninguno de los dos es una llamada vacia, asi que
    // el motor puede pedirlo sin preguntar antes. `rumble_enabled` sigue siendo la UNICA
    // puerta del ajuste: apaga los dos caminos.
    void rumble(f32 strength, u32 ms);

private:
    void process_keyboard(const SDL_Event& event);
    void process_mouse(const SDL_Event& event);
    void process_gamepad(const SDL_Event& event);
    void process_touch(const SDL_Event& event);
    void update_axes();
    void apply_deadzone(Vec2& axis) const;
};
