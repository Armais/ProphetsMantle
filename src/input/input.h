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
    Consumable,     // R / X

    // UI
    Pause,          // Esc / Start / Swipe 2 dedos
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
    f32  radius    = 40.0f;   // Max deflection in pixels (base resolution)
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

    // Touch (mobile)
    VirtualJoystick joystick_move;
    VirtualJoystick joystick_aim;  // Optional second stick

    // Auto-aim (mobile)
    bool  auto_aim_enabled = false;
    Vec2  auto_aim_target  = {0, 0};

    // Deadzone for analog sticks
    f32 stick_deadzone = 0.15f;

    // Init
    void init();

    // Per-frame
    void pre_update();
    void process_event(const SDL_Event& event);
    void post_update();

    // Action queries
    bool is_action_pressed(Action a) const;
    bool is_action_just_pressed(Action a) const;
    bool is_action_just_released(Action a) const;

    // Axis queries
    Vec2 get_move_axis() const;
    Vec2 get_aim_axis() const;

    // Device detection
    bool has_gamepad() const { return gamepad != nullptr; }

private:
    void process_keyboard(const SDL_Event& event);
    void process_mouse(const SDL_Event& event);
    void process_gamepad(const SDL_Event& event);
    void process_touch(const SDL_Event& event);
    void update_axes();
    void apply_deadzone(Vec2& axis) const;
};
