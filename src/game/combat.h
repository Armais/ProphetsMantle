#pragma once

// =============================================================================
// MANTLE - Combat System (Collision Detection & Resolution)
// =============================================================================

#include "core/ecs.h"

// Celdas del tileset compartido (game/rooms.h). Declarada aqui para no arrastrar
// todo rooms.h en la cabecera de combate.
enum class TileSprite : u8;

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
    int  boss_hits     = 0;   // ...de los cuales, en un jefe (sacudida propia)
    int  enemy_kills   = 0;
    int  boss_kills    = 0;
    int  hearts_picked = 0;
    int  orbs_picked   = 0;
    // QUE ha muerto este frame, como mascara de EnemyType (bit por tipo). El Engine le
    // pone su sonido —un leon y una vasija de agua corrupta no pueden morir con el mismo
    // golpe seco— y de ahi salen los logros por tipo. Es una MASCARA y no "el ultimo que
    // cayo" porque en una barrida mueren varios a la vez: guardando solo el ultimo, matar
    // un leon y un soldado en el mismo frame borraba el leon.
    u32  kill_mask     = 0;
    void reset() { *this = CombatEvents{}; }
};

// Projectile visuals (cells of the shared tileset)
enum class ProjectileKind : u8 { Fireball, Lightbolt, Arrow, Darkbolt };

// Efectos de la hoja propia `assets/sprites/vfx.png` (8 fotogramas x 7 filas, celda de
// 128 px = 32 unidades). El orden DEBE coincidir con `LAYOUT` en
// tools/blender/mantle_sprites/vfx.py.
enum class FxID : u8 { Impact, Explosion, Heal, Sparks, Smoke, Holy, Ring, COUNT };

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

// Un sprite del tileset que vive `life` segundos y se apaga. Con `grow` se abre como un
// anillo, con `spin` gira, con `velocity` se desplaza. Es la pieza con la que cada milagro
// tiene su propia firma visual sin necesitar arte nuevo (ver Player::cast_miracle).
Entity spawn_vfx(ECSWorld& world, Vec2 position, TileSprite sprite, Color tint,
                 f32 size_units, f32 life, f32 grow = 0.0f, f32 spin = 0.0f,
                 Vec2 velocity = {0, 0}, i32 z_order = 9);

// Efecto ANIMADO de la hoja propia. Se diferencia de spawn_vfx en tres cosas:
//   1. Tiene 8 fotogramas de verdad (AnimationComponent), no un icono que crece y se apaga.
//   2. La celda mide 32 unidades, no las 16 del tileset: `size_units` escala contra 32.
//   3. Sin la hoja cargada NO dibuja nada y devuelve ENTITY_NONE — quien quiera un
//      respaldo que llame ademas a spawn_vfx.
// `tint` multiplica: la hoja esta pintada casi en blanco a proposito para que el mismo
// impacto valga naranja para Elias y azul para Eliseo.
Entity spawn_fx(ECSWorld& world, Vec2 position, FxID id, Color tint,
                f32 size_units, f32 duration = 0.35f, i32 z_order = 9);

// Best enemy to shoot at from `from`, using the GDD auto-aim priority (4.3):
// 1) a boss, 2) an enemy currently attacking, 3) the closest one, 4) the lowest HP
// (used for mobile auto-aim and by the autoplay bot). Returns ENTITY_NONE if none in range.
Entity pick_target(const ECSWorld& world, Vec2 from, f32 max_range = 220.0f);
