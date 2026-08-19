#pragma once

// =============================================================================
// MANTLE - Combat System (Collision Detection & Resolution)
// =============================================================================

#include "core/ecs.h"

// =============================================================================
// Collision layers
// =============================================================================
namespace CollisionLayer {
    constexpr u32 Player     = 1 << 0;
    constexpr u32 Enemy      = 1 << 1;
    constexpr u32 PlayerProj = 1 << 2;
    constexpr u32 EnemyProj  = 1 << 3;
    constexpr u32 Wall       = 1 << 4;
    constexpr u32 Pickup     = 1 << 5;
}

// =============================================================================
// Collision result
// =============================================================================
struct CollisionResult {
    Entity a = ENTITY_NONE;
    Entity b = ENTITY_NONE;
    Vec2 normal   = {0, 0};
    f32 overlap   = 0.0f;
};

// Feedback the Engine reads after system_collision (camera shake, messages)
struct CombatEvents {
    int  player_hits   = 0;   // Times the player got hurt this frame
    int  enemy_hits    = 0;   // Player projectiles that connected
    int  enemy_kills   = 0;
    int  boss_kills    = 0;
    int  hearts_picked = 0;
    int  orbs_picked   = 0;
    void reset() { *this = CombatEvents{}; }
};

// Projectile visuals (cells of the shared tileset)
enum class ProjectileKind : u8 { Fireball, Lightbolt, Arrow, Darkbolt };

// =============================================================================
// Combat functions
// =============================================================================

// Check AABB vs AABB
bool check_aabb_overlap(Vec2 pos_a, Vec2 size_a, Vec2 pos_b, Vec2 size_b, Vec2& normal, f32& overlap);

// Check Circle vs Circle
bool check_circle_overlap(Vec2 pos_a, f32 radius_a, Vec2 pos_b, f32 radius_b, Vec2& normal, f32& overlap);

// Process all collisions in the world
void system_collision(ECSWorld& world, struct FaithSystem& faith, CombatEvents& events);

// Apply damage from entity A to entity B. Returns true if the target died from this hit.
bool apply_damage(ECSWorld& world, Entity attacker, Entity target, f32 damage);

// Enemy death rewards: faith, drops (called once per kill)
void on_enemy_killed(ECSWorld& world, struct FaithSystem& faith, Entity enemy, CombatEvents& events);

// Spawn a projectile
Entity spawn_projectile(ECSWorld& world, Vec2 position, Vec2 direction, f32 damage, bool friendly,
                        ProjectileKind kind = ProjectileKind::Fireball, f32 speed = 0.0f, f32 range = 0.0f);

// Spawn a pickup (heart / faith orb)
Entity spawn_pickup(ECSWorld& world, Vec2 position, ItemComponent::Pickup kind, f32 value);

// Burst of tiny square particles (visual only: no collider, short lifetime)
void spawn_particles(ECSWorld& world, Vec2 position, Color color, int count, f32 speed = 60.0f, f32 life = 0.35f);

// Best enemy to shoot at from `from`, using the GDD auto-aim priority (4.3):
// 1) a boss, 2) an enemy currently attacking, 3) the closest one, 4) the lowest HP
// (used for mobile auto-aim and by the autoplay bot). Returns ENTITY_NONE if none in range.
Entity pick_target(const ECSWorld& world, Vec2 from, f32 max_range = 220.0f);
