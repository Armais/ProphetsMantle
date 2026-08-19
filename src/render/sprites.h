#pragma once

// =============================================================================
// MANTLE - Sprite Sheet & Animation Definitions
// =============================================================================

#include "core/types.h"
#include "core/config.h"
#include "core/ecs.h"

// Character sheets produced by tools/blender/mantle_sprites: 7 cols x 15 rows of 128x128
// (896x1920), rows in AnimID order, all 4 directions authored (no flip_x needed).

// =============================================================================
// Animation IDs (rows in spritesheet)
// =============================================================================
enum class AnimID : u32 {
    // Character animations
    IdleDown = 0,
    IdleUp,
    IdleLeft,
    IdleRight,
    WalkDown,
    WalkUp,
    WalkLeft,
    WalkRight,
    AttackDown,
    AttackUp,
    AttackLeft,
    AttackRight,
    Dash,
    CastMiracle,
    Death,
    COUNT
};

// =============================================================================
// Animation definition
// =============================================================================
struct AnimDef {
    u32 row;          // Row in spritesheet
    u32 frames;       // Number of frames
    f32 frame_time;   // Seconds per frame
    bool loop;
};

// Frame counts from GDD:
//   Idle:    4 frames
//   Walk:    6 frames
//   Attack:  4-6 frames
//   Special: 6-8 frames

inline AnimDef get_anim_def(AnimID id) {
    switch (id) {
        case AnimID::IdleDown:    return {0,  4, 0.15f, true};
        case AnimID::IdleUp:      return {1,  4, 0.15f, true};
        case AnimID::IdleLeft:    return {2,  4, 0.15f, true};
        case AnimID::IdleRight:   return {3,  4, 0.15f, true};
        case AnimID::WalkDown:    return {4,  6, 0.10f, true};
        case AnimID::WalkUp:      return {5,  6, 0.10f, true};
        case AnimID::WalkLeft:    return {6,  6, 0.10f, true};
        case AnimID::WalkRight:   return {7,  6, 0.10f, true};
        case AnimID::AttackDown:  return {8,  5, 0.06f, false};
        case AnimID::AttackUp:    return {9,  5, 0.06f, false};
        case AnimID::AttackLeft:  return {10, 5, 0.06f, false};
        case AnimID::AttackRight: return {11, 5, 0.06f, false};
        case AnimID::Dash:        return {12, 3, 0.05f, false};
        case AnimID::CastMiracle: return {13, 7, 0.08f, false};
        case AnimID::Death:       return {14, 6, 0.12f, false};
        default:                  return {0,  1, 0.10f, false};
    }
}

// Sheet layout constants (see tools/blender/mantle_sprites/render.py). A frame is
// 32 game units; the sheet holds ASSET_SCALE texels per unit (128 px cells at x4).
constexpr int SHEET_FRAME_UNITS = 32;
constexpr int SHEET_FRAME_SIZE = SHEET_FRAME_UNITS * Config::ASSET_SCALE;   // 128
constexpr int SHEET_COLS       = 7;
constexpr int SHEET_ROWS       = static_cast<int>(AnimID::COUNT);   // 15
constexpr int SHEET_WIDTH      = SHEET_COLS * SHEET_FRAME_SIZE;      // 896
constexpr int SHEET_HEIGHT     = SHEET_ROWS * SHEET_FRAME_SIZE;      // 1920

// =============================================================================
// Animation helpers
// =============================================================================

// Directional variant of a "Down" base animation (rows are Down, Up, Left, Right)
inline AnimID facing_anim(AnimID base_down, Direction d) {
    u32 offset = 0;
    switch (d) {
        case Direction::Up:    offset = 1; break;
        case Direction::Left:  offset = 2; break;
        case Direction::Right: offset = 3; break;
        default:               offset = 0; break;   // Down / None
    }
    return static_cast<AnimID>(static_cast<u32>(base_down) + offset);
}

// Switch an AnimationComponent to `id`. No-op if already playing that row unless `restart`.
inline void play_anim(AnimationComponent& anim, AnimID id, bool restart = false) {
    AnimDef def = get_anim_def(id);
    if (anim.row == def.row && !restart) return;
    anim.row = def.row;
    anim.frame_count = def.frames;
    anim.frame_time = def.frame_time;
    anim.looping = def.loop;
    anim.current_frame = 0;
    anim.timer = 0.0f;
    anim.playing = true;
}

inline f32 anim_duration(AnimID id) {
    AnimDef def = get_anim_def(id);
    return static_cast<f32>(def.frames) * def.frame_time;
}

// =============================================================================
// Sprite Sheet definition
// =============================================================================
struct SpriteSheet {
    u32 texture_id;
    int frame_width;    // Width of one frame in pixels
    int frame_height;   // Height of one frame in pixels
    int columns;        // Frames per row
    int rows;           // Total rows (animations)

    Rect get_frame_rect(u32 col, u32 row) const {
        return {
            static_cast<f32>(col * frame_width),
            static_cast<f32>(row * frame_height),
            static_cast<f32>(frame_width),
            static_cast<f32>(frame_height)
        };
    }
};
