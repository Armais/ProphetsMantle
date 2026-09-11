#pragma once

// =============================================================================
// MANTLE - Camera System (follows player, room-locked, shake)
// =============================================================================

#include "core/types.h"
#include "core/config.h"

struct Camera {
    Vec2 position     = {0, 0};
    Vec2 target       = {0, 0};     // What to follow (player position)
    f32  follow_speed = 5.0f;       // Lerp speed
    bool locked       = true;       // Lock to room bounds

    // Room bounds (camera won't show outside these)
    Rect room_bounds  = {0, 0,
                         static_cast<f32>(Config::GAME_WIDTH),
                         static_cast<f32>(Config::GAME_HEIGHT)};
    // Hacia donde se DESLIZAN los limites. Con la zona continua el recorte cambia al entrar
    // y salir de la arena del jefe (arena <-> zona entera); sin deslizar, la camara pegaba
    // un salto de media pantalla al cruzar el vano.
    Rect bounds_goal  = room_bounds;

    void set_target(Vec2 pos) {
        target = pos;
    }

    // snap = true: los limites cambian en el acto (cargar un mapa, teletransportar)
    void set_room(Rect bounds, bool snap = true) {
        bounds_goal = bounds;
        if (snap) room_bounds = bounds;
    }

    void update(f32 dt) {
        // Los limites se deslizan hacia su meta
        const f32 k = std::min(1.0f, 8.0f * dt);
        room_bounds.x += (bounds_goal.x - room_bounds.x) * k;
        room_bounds.y += (bounds_goal.y - room_bounds.y) * k;
        room_bounds.w += (bounds_goal.w - room_bounds.w) * k;
        room_bounds.h += (bounds_goal.h - room_bounds.h) * k;

        // Smooth follow
        position = Vec2::lerp(position, target, follow_speed * dt);

        // Clamp to room bounds if locked
        if (locked) {
            f32 half_w = Config::GAME_WIDTH * 0.5f;
            f32 half_h = Config::GAME_HEIGHT * 0.5f;

            position.x = std::max(room_bounds.x + half_w,
                         std::min(position.x, room_bounds.right() - half_w));
            position.y = std::max(room_bounds.y + half_h,
                         std::min(position.y, room_bounds.bottom() - half_h));
        }
    }

    // For single-screen rooms (most common in MANTLE), just center on room
    void center_on_room() {
        position = room_bounds.center();
        target = position;
    }
};
