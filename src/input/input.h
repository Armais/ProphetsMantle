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
#include <algorithm>
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
    Consumable,     // R / X  (usa el consumible ELEGIDO de la alforja)
    // La alforja (2026-10-03): 1-4 usan ese hueco directamente; la rueda del raton, la
    // cruceta izq/der del mando o mantener USAR en tactil cambian el elegido.
    Belt1,
    Belt2,
    Belt3,
    Belt4,
    BeltNext,
    BeltPrev,

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
        if (len < 2.5f) return {0, 0};  // Dead zone
        if (len > radius) delta = delta.normalized() * radius;
        return delta / radius;  // Normalized -1..1
    }
    // La base SIGUE al dedo cuando este se sale del radio (2026-10-03): es lo que hace un
    // stick flotante de juego movil. Sin esto, para cambiar de sentido habia que desandar
    // todo lo arrastrado de mas antes de que el profeta se diera la vuelta — con el dedo a
    // 60 unidades del origen, ir hacia el otro lado costaba 60 unidades de nada.
    void follow() {
        Vec2 delta = current - origin;
        f32 len = delta.length();
        if (len > radius) origin = current - delta.normalized() * radius;
    }
};

// =============================================================================
// Touch: botones en pantalla (GDD 4.2)
// =============================================================================
// En UNIDADES DE JUEGO, las mismas que dibuja ui/hud.cpp: en un telefono el HUD ES el
// mando, asi que las tres celdas de milagro son a la vez los tres botones de milagro.
//
// Layout de 2026-10-03 (el de un juego de accion movil): botones REDONDOS en dos arcos
// alrededor de DASH, que va grande en la esquina, donde descansa el pulgar derecho.
//
//                      (ORAR)
//               (ARMA)  [M3]
//          (USAR)   [M2]
//                [M1]    ( DASH )        [II] arriba: en el pilar derecho si cabe,
//                                             si no arriba al centro
//
// El grupo se ancla a la esquina de la ZONA SEGURA de la ventana, no del area de juego:
// en un 20:9 se mete en el pilar negro de la derecha (que antes no servia para nada) y en
// vertical baja a la banda de abajo. Con "zurdo" se refleja entero. Los rects se calculan
// aqui UNA SOLA VEZ y el HUD los reusa: ese contrato es lo que impide que el dibujo y el
// hit-test se separen.
//
// ORAR y ARMA solo existen si el profeta puede rezar / lleva arma secundaria: por eso
// cada boton lleva `enabled`, que apaga a la vez el DIBUJO y el HIT-TEST. Antes el
// hit-test probaba los cinco rects aunque el HUD no pintara ninguno, y quedaban huecos
// muertos donde el dedo no hacia nada ni disparaba.
struct TouchButton {
    Rect   rect   = {0, 0, 0, 0};   // caja envolvente (la de la pausa ES su forma)
    Action action = Action::COUNT;
    int    slot   = -1;     // slot de milagro (0..2); -1 = no es un milagro
    SDL_FingerID finger = 0;
    bool   down   = false;
    bool   enabled = true;  // false = ni se dibuja ni responde
    f32    hold   = 0.0f;   // segundos pulsado
    bool   hold_fired = false;   // USAR: el hold ya cambio de consumible (al soltar no se bebe)
    Vec2   center = {0, 0};      // centro del boton redondo
    f32    radius = 0.0f;        // radio DIBUJADO; 0 = boton rectangular (la pausa)
    f32    deny_t = 0.0f;        // > 0: se acaba de negar (milagro sin fe o en recarga): tiembla

    // El dedo cuenta con un margen (`slop`) por fuera de lo dibujado: un pulgar no acierta
    // al milimetro y un boton que exige precision de raton es un boton que falla.
    f32 edge_distance(Vec2 p) const {
        if (radius > 0.0f) return (p - center).length() - radius;
        f32 dx = std::max(rect.x - p.x, p.x - rect.right());
        f32 dy = std::max(rect.y - p.y, p.y - rect.bottom());
        return std::max(dx, dy);
    }
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
constexpr f32 TOUCH_TAP_SLOP     = 6.0f; // en PARTIDA: un dedo que se mueve mas no es un tap
// En un MENU el mismo umbral es demasiado fino: 6 unidades son ~2,2 mm y un pulgar se
// desplaza mas que eso solo con apoyarse, asi que el toque se convertia en paso de lista y
// no pulsaba nada ("parece que hay que mover antes de tocar"). En partida sigue en 6, que
// es lo que separa el tap de ataque del apuntado manual.
constexpr f32 TOUCH_UI_TAP_SLOP  = 14.0f;// en MENUS: ~5 mm de deriva permitida
// Por encima del slop de menu a proposito: si un paso de lista costara menos recorrido que
// el slop, el mismo arrastre seria tap Y scroll a la vez.
constexpr f32 TOUCH_SCROLL_STEP  = 16.0f;// unidades de arrastre por paso de lista

// Tamaños a escala 1, en unidades de juego. La escala 1 es la de un movil de 6,5" en 20:9,
// donde una unidad mide ~0,36 mm: DASH ~11,5 mm de diametro, milagros ~9,4 mm y los
// botones pequeños ~8 mm (Android pide 9 mm para lo que se toca a menudo, 7 como suelo).
// Engine::touch_dpi_scale corrige por el tamaño REAL de la pantalla y el ajuste "Tamaño de
// controles" multiplica encima.
constexpr f32 TOUCH_DASH_R       = 16.0f;
constexpr f32 TOUCH_MIRACLE_R    = 13.0f;
constexpr f32 TOUCH_MINOR_R      = 11.0f;
constexpr f32 TOUCH_EDGE         = 5.0f;   // aire entre el grupo y el borde de la zona segura
constexpr f32 TOUCH_JOY_R        = 22.0f;  // recorrido del stick hasta velocidad maxima (~8 mm)
constexpr f32 TOUCH_HIT_SLOP     = 4.0f;   // margen del dedo por fuera de cada boton
constexpr f32 TOUCH_DENY_TIME    = 0.35f;  // lo que tiembla un milagro que no se puede lanzar

// Donde y como se montan los controles. Lo rellena el Engine (zona segura y DPI) y los
// ajustes del jugador (tamaño, zurdo); Input::relayout_touch lo aplica.
struct TouchLayoutParams {
    // Zona de la VENTANA donde se puede tocar y ver, en unidades de juego con el origen en
    // la esquina del area de juego. Se SALE de 0..320 / 0..180 en cuanto hay pilares o
    // bandas negras, y se encoge con el notch y las zonas de gestos del sistema.
    Rect safe       = {0.0f, 0.0f, static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)};
    f32  dpi_scale  = 1.0f;   // tamaño fisico: 1 = el movil de referencia
    f32  user_scale = 1.0f;   // ajuste "Tamaño de controles" (0,7..1,5)
    bool lefty      = false;  // ajuste "Zurdo": stick a la derecha, botones a la izquierda
};

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
    Vec2 tap_now     = {0, 0};     // donde esta AHORA el dedo que todavia cuenta como tap
    SDL_FingerID tap_finger = 0;
    bool tap_active  = false;
    f32  scroll_accum = 0.0f;      // arrastre vertical acumulado con el juego parado

    // Un paso de update ha corrido desde el ultimo pre_update. Sin esto, un frame que no
    // ejecuta NINGUN paso (bucle de paso fijo a 60 Hz sobre una pantalla de 90 o 120 Hz)
    // destruia el tap y el flanco de teclado de ese frame sin que nadie los leyera: a
    // 120 Hz, uno de cada dos toques. Es el motivo de que en el movil los menus
    // respondieran a medias y en el PC no.
    bool step_since_pre = true;

    // Minimapa tocable: tocarlo abre/cierra el mapa grande. El rect lo escribe Engine con
    // HUD::minimap_rect, para que dibujo y hit-test salgan de la MISMA formula.
    Rect touch_minimap  = {0, 0, 0, 0};
    bool touch_map_open = false;

    // Boton de pantalla que NO es del HUD y que se atiende DENTRO de la partida: hoy solo el
    // SALTAR del tutorial, que vive sobre el mundo con el HUD debajo. Se comprueba antes que
    // el "tap = ataque" y hace return CONSERVANDO el tap, para que quien lo dibuja lo recoja
    // con peek_tap. Sin esto, tocarlo disparaba el cayado de paso. {0,0,0,0} = no hay.
    Rect touch_ui_rect = {0, 0, 0, 0};

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

    // Controles tactiles a la medida del jugador (2026-10-03). Los tres los escribe
    // Engine::apply_settings_from_save; `touch_opacity` solo lo lee el HUD al pintar.
    f32   touch_opacity    = 1.0f;    // 0,3..1: cuanto tapan los botones el mundo
    TouchLayoutParams touch_params;   // con lo que se monto el layout actual
    // Stick de movimiento EN REPOSO: donde se pinta su fantasma (la pista de "pon aqui el
    // pulgar") y su radio, que es tambien el recorrido hasta la velocidad maxima.
    Vec2  joy_rest         = {40.0f, 140.0f};
    f32   joy_radius       = TOUCH_JOY_R;
    f32   touch_slop       = TOUCH_HIT_SLOP;
    // El stick aparece donde baja el dedo en ESTA mitad: la izquierda, o la derecha en zurdo.
    bool  in_move_half(Vec2 p) const {
        const bool left = p.x < Config::GAME_WIDTH * 0.5f;
        return touch_params.lefty ? !left : left;
    }

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
    // Rects de los botones tactiles, en unidades de juego, a partir de la zona segura de la
    // ventana, la escala y el lado (ver TouchLayoutParams). Los rects se definen aqui UNA
    // SOLA VEZ y el HUD los reusa: ese contrato es lo que impide que el dibujo y el
    // hit-test se separen. Conserva el estado de cada boton (enabled, pulsado).
    void init_touch_layout(const TouchLayoutParams& p);
    void relayout_touch() { init_touch_layout(touch_params); }
    // El Engine: zona segura de la ventana (en unidades de juego) y escala por DPI.
    void set_touch_area(Rect safe, f32 dpi_scale) {
        touch_params.safe = safe;
        touch_params.dpi_scale = dpi_scale;
        relayout_touch();
    }
    // Los ajustes del jugador.
    void set_touch_prefs(f32 user_scale, f32 opacity, bool lefty) {
        touch_opacity = std::max(0.2f, std::min(1.0f, opacity));
        if (user_scale == touch_params.user_scale && lefty == touch_params.lefty) return;
        touch_params.user_scale = user_scale;
        touch_params.lefty = lefty;
        release_touch();   // cambiar el lado con un dedo puesto lo dejaria colgado
        relayout_touch();
    }
    // La escala que se ha aplicado de verdad (DPI x ajuste, acotada para que el grupo quepa).
    f32  touch_scale = 1.0f;
    // Un milagro que el jugador ha intentado lanzar sin poder: su boton tiembla. Lo llama
    // el Engine con el mismo evento que pinta el mensaje de "te falta fe".
    void deny_miracle(int slot) {
        if (slot < 0 || slot >= 3) return;
        touch_buttons[TOUCH_BTN_MIRACLE + slot].deny_t = TOUCH_DENY_TIME;
        SDL_Log("Touch deny: milagro %d", slot + 1);   // gate de scripts/movil_check.txt
    }
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
    // El dedo sigue apoyado y todavia cuenta como tap potencial, con su posicion ACTUAL.
    // Es lo que permite iluminar el boton que hay debajo mientras dura y apagarlo si el
    // dedo se va de paseo. En movil sustituye al resaltado de seleccion, que no significa
    // nada cuando se toca directamente el boton que se quiere.
    bool ui_press(Vec2& out) const {
        if (!tap_active) return false;
        out = tap_now;
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
    void update_touch(f32 dt);    // temporizadores de hold (ficha del milagro) y del temblor
    // El boton tactil (encendido) que hay bajo `p`, con el margen del dedo; -1 = ninguno. Si
    // dos margenes se solapan gana el boton cuyo BORDE este mas cerca.
    int  touch_button_at(Vec2 p) const;

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
