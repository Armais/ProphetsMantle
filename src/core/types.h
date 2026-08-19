#pragma once

// =============================================================================
// MANTLE - Common Types & Math
// =============================================================================

#include <cmath>
#include <cstdint>
#include <algorithm>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using f32 = float;
using f64 = double;

// Sentinel for SpriteComponent::texture_id (no texture -> placeholder rendering)
constexpr u32 TEXTURE_NONE = 0xFFFFFFFFu;

// =============================================================================
// Vec2
// =============================================================================
struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(f32 s) const { return {x * s, y * s}; }
    Vec2 operator/(f32 s) const { return {x / s, y / s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(f32 s) { x *= s; y *= s; return *this; }

    f32 length() const { return std::sqrt(x * x + y * y); }
    f32 length_sq() const { return x * x + y * y; }

    Vec2 normalized() const {
        f32 len = length();
        if (len < 0.0001f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }

    f32 dot(Vec2 o) const { return x * o.x + y * o.y; }
    f32 distance_to(Vec2 o) const { return (*this - o).length(); }

    static Vec2 lerp(Vec2 a, Vec2 b, f32 t) {
        return a + (b - a) * t;
    }
};

// =============================================================================
// Rect (AABB)
// =============================================================================
struct Rect {
    f32 x, y, w, h;

    f32 left()   const { return x; }
    f32 right()  const { return x + w; }
    f32 top()    const { return y; }
    f32 bottom() const { return y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    bool overlaps(const Rect& o) const {
        return left() < o.right() && right() > o.left() &&
               top() < o.bottom() && bottom() > o.top();
    }

    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
};

// =============================================================================
// Color
// =============================================================================
struct Color {
    u8 r, g, b, a;

    static Color white()  { return {255, 255, 255, 255}; }
    static Color black()  { return {0, 0, 0, 255}; }
    static Color red()    { return {255, 0, 0, 255}; }
    static Color green()  { return {0, 255, 0, 255}; }
    static Color blue()   { return {0, 0, 255, 255}; }

    // Paleta MANTLE - Elias (fuego)
    static Color elias_orange() { return {230, 126, 34, 255}; }
    static Color elias_red()    { return {192, 57, 43, 255}; }
    static Color elias_gold()   { return {241, 196, 15, 255}; }

    // Paleta MANTLE - Eliseo (agua/luz)
    static Color eliseo_blue()  { return {52, 152, 219, 255}; }
    static Color eliseo_green() { return {46, 204, 113, 255}; }
    static Color eliseo_white() { return {236, 240, 241, 255}; }

    // Paleta MANTLE - Enemigos Baal
    static Color baal_purple()  { return {142, 68, 173, 255}; }
    static Color baal_dark()    { return {44, 24, 16, 255}; }
    static Color baal_red()     { return {148, 30, 30, 255}; }
};

// =============================================================================
// Direction (for top-down movement)
// =============================================================================
enum class Direction : u8 {
    None = 0,
    Up,
    Down,
    Left,
    Right
};

// Dominant-axis direction of a vector (None if ~zero)
inline Direction direction_from_vec(Vec2 v) {
    if (v.length_sq() < 0.0001f) return Direction::None;
    if (std::abs(v.x) > std::abs(v.y)) return v.x > 0.0f ? Direction::Right : Direction::Left;
    return v.y > 0.0f ? Direction::Down : Direction::Up;
}

// =============================================================================
// Random utilities
// =============================================================================
inline f32 randf() {
    return static_cast<f32>(rand()) / static_cast<f32>(RAND_MAX);
}

inline f32 randf_range(f32 min, f32 max) {
    return min + randf() * (max - min);
}

inline i32 randi_range(i32 min, i32 max) {
    return min + (rand() % (max - min + 1));
}
