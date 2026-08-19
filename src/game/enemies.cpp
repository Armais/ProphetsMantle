// =============================================================================
// MANTLE - Enemy Implementation
// =============================================================================

#include "game/enemies.h"
#include "game/combat.h"
#include "game/rooms.h"
#include "platform/platform.h"
#include "core/config.h"
#include "render/sprites.h"
#include <cmath>

static Entity spawn_common(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id, int chapter) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    auto def = get_enemy_def(type);
    // Scaling inside each act (chapter 1 & 6 = base of their act) plus an act-II step, so
    // Eliseo does not start act II against chapter-1 stats (bot runs cleared ch10 at ~85% HP).
    int step = (chapter - 1) % 5;
    f32 hp_mult  = 1.0f + 0.28f * step;
    f32 dmg_mult = 1.0f + 0.18f * step;
    if (chapter > Config::CHAPTERS_ACT_I) { hp_mult *= 1.35f; dmg_mult *= 1.20f; }

    // Transform
    world.add(e, Component::Transform);
    world.transforms[e].position = position;

    // Sprite (sheets are authored in full color -> no tint; placeholder keeps def.color)
    world.add(e, Component::Sprite);
    world.sprites[e].texture_id = texture_id;
    world.sprites[e].src_rect = {0, 0, static_cast<f32>(SHEET_FRAME_SIZE), static_cast<f32>(SHEET_FRAME_SIZE)};
    world.sprites[e].z_order = 5;
    world.sprites[e].tint = (texture_id == TEXTURE_NONE) ? def.color : Color::white();

    // Animation
    world.add(e, Component::Animation);
    world.animations[e].frame_count = 4;
    world.animations[e].frame_time = 0.12f;

    // Health
    world.add(e, Component::Health);
    world.healths[e].max = def.hp * hp_mult;
    world.healths[e].current = world.healths[e].max;

    // Combat
    world.add(e, Component::Combat);
    world.combats[e].damage = def.damage * dmg_mult;

    // Movement
    world.add(e, Component::Movement);
    f32 speed = def.speed;
    if (is_mobile()) speed *= Config::MOBILE_ENEMY_SPEED_MULT;
    world.movements[e].speed = speed;
    world.movements[e].friction = 2.5f;   // AI re-steers every think tick; keep momentum between ticks

    // Collider
    world.add(e, Component::Collider);
    world.colliders[e].shape = ColliderComponent::Shape::Circle;
    world.colliders[e].radius = 7.0f;
    world.colliders[e].offset = {0, 5};   // Feet: sheet baseline is frame row 28 (+12 from centre)

    // AI
    world.add(e, Component::EnemyAI);
    world.enemy_ais[e].behavior = def.behavior;
    world.enemy_ais[e].aggro_range = def.aggro_range;
    world.enemy_ais[e].attack_range = def.attack_range;
    world.enemy_ais[e].name = def.name;
    world.enemy_ais[e].enemy_type = static_cast<u32>(type);
    world.enemy_ais[e].shoot_timer = randf_range(0.6f, 1.6f);
    world.enemy_ais[e].think_timer = randf_range(0.0f, 0.3f);
    world.enemy_ais[e].think_interval = 0.2f;

    return e;
}

Entity spawn_enemy(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id, int chapter) {
    return spawn_common(world, type, position, texture_id, chapter);
}

Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id,
                  int chapter, const char* boss_name) {
    Entity e = spawn_common(world, type, position, texture_id, chapter);
    if (e == ENTITY_NONE) return e;

    auto def = get_enemy_def(type);
    f32 hp = 200.0f + 50.0f * chapter;
    world.healths[e].max = hp;
    world.healths[e].current = hp;
    world.combats[e].damage = def.damage * 1.25f;
    world.movements[e].speed = std::max(50.0f, def.speed * 0.72f);
    world.transforms[e].scale = {1.6f, 1.6f};
    world.colliders[e].radius = 11.0f;
    world.colliders[e].offset = {0, 8};
    world.enemy_ais[e].is_boss = true;
    world.enemy_ais[e].name = boss_name ? boss_name : def.name;
    world.enemy_ais[e].aggro_range = 400.0f;
    world.enemy_ais[e].shoot_timer = 2.0f;
    world.enemy_ais[e].leap_timer = 3.5f;
    return e;
}

// -----------------------------------------------------------------------------
static bool has_line_of_sight(const ECSWorld& world, Vec2 from, Vec2 to) {
    if (!world.active_room) return true;
    Vec2 d = to - from;
    f32 len = d.length();
    if (len < 1.0f) return true;
    Vec2 step = d / len * 8.0f;
    int n = static_cast<int>(len / 8.0f);
    Vec2 p = from;
    for (int i = 0; i < n; i++) {
        p += step;
        int tx = static_cast<int>(p.x) / Config::TILE_SIZE;
        int ty = static_cast<int>(p.y) / Config::TILE_SIZE;
        if (world.active_room->is_tile_solid_for_projectiles(tx, ty)) return false;
    }
    return true;
}

static void shoot_at(ECSWorld& world, Entity e, Vec2 dir, ProjectileKind kind, f32 dmg, f32 speed, f32 range) {
    Vec2 from = world.transforms[e].position + Vec2{0, 2};
    spawn_projectile(world, from, dir, dmg, false, kind, speed, range);
    if (world.has(e, Component::Animation)) {
        Direction facing = direction_from_vec(dir);
        if (facing != Direction::None) world.transforms[e].facing = facing;
        AnimID a = (kind == ProjectileKind::Arrow) ? facing_anim(AnimID::AttackDown, world.transforms[e].facing)
                                                   : AnimID::CastMiracle;
        play_anim(world.animations[e], a, true);
    }
}

void update_enemy_ai(ECSWorld& world, f32 dt) {
    Entity player = world.player;
    if (player == ENTITY_NONE) return;

    Vec2 player_pos = world.transforms[player].position;

    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e)) continue;
        if (!world.has(e, Component::EnemyAI) || !world.has(e, Component::Transform)) continue;
        if (!world.has(e, Component::Movement)) continue;   // corpses
        if (world.has(e, Component::Health) && !world.healths[e].is_alive()) continue;

        auto& ai = world.enemy_ais[e];
        auto& transform = world.transforms[e];
        auto& movement = world.movements[e];

        // Timers tick every frame
        ai.shoot_timer -= dt;
        ai.leap_timer  -= dt;

        f32 dist = transform.position.distance_to(player_pos);
        Vec2 to_player = (player_pos - transform.position).normalized();
        f32 dmg = world.has(e, Component::Combat) ? world.combats[e].damage : 8.0f;

        // ---- Boss: chase + volleys + periodic charge (regardless of base behaviour) ----
        if (ai.is_boss) {
            if (ai.think_timer <= 0.0f) {
                ai.think_timer = 0.25f;
                if (dist > 26.0f) movement.velocity = to_player * movement.speed;
                else movement.velocity = {0, 0};
            } else {
                ai.think_timer -= dt;
            }
            if (ai.shoot_timer <= 0.0f) {
                ai.shoot_timer = 2.0f;
                f32 base = std::atan2(to_player.y, to_player.x);
                bool ring = (static_cast<int>(world.time) % 3 == 0);
                if (ring) {
                    for (int i = 0; i < 8; i++) {
                        f32 a = base + i * 0.7854f;
                        shoot_at(world, e, {std::cos(a), std::sin(a)}, ProjectileKind::Darkbolt, dmg * 0.6f, 130.0f, 300.0f);
                    }
                } else {
                    for (int i = -2; i <= 2; i++) {
                        f32 a = base + i * 0.26f;
                        shoot_at(world, e, {std::cos(a), std::sin(a)}, ProjectileKind::Darkbolt, dmg * 0.6f, 160.0f, 260.0f);
                    }
                }
            }
            // Charge: telegraphed by a short stop, then a burst towards the player
            if (ai.leap_timer <= 0.0f) {
                ai.leap_timer = 5.5f;
                movement.knockback = to_player * 240.0f;
                if (world.has(e, Component::Animation)) play_anim(world.animations[e], AnimID::Dash, true);
            } else if (ai.leap_timer < 0.5f) {
                movement.velocity = {0, 0};   // wind-up
            }
            continue;
        }

        // ---- Regular enemies think at intervals ----
        ai.think_timer -= dt;
        bool think = ai.think_timer <= 0.0f;
        if (think) ai.think_timer = ai.think_interval;
        // Drought slow: applied to whatever velocity this think tick decides
        struct SlowGuard { MovementComponent& m; bool on; ~SlowGuard() { if (on) m.velocity *= 0.5f; } }
            slow_guard{movement, think && world.enemy_slow_timer > 0.0f};

        switch (ai.behavior) {
            case AIBehavior::MeleeDirect:
            case AIBehavior::MeleeResistant:
            case AIBehavior::ExplodeOnDeath:
            case AIBehavior::SplitOnDeath:
                if (think) {
                    if (dist < ai.aggro_range) {
                        f32 s = movement.speed * (ai.behavior == AIBehavior::SplitOnDeath ? 0.85f : 1.0f);
                        // Slight arc so packs don't stack in a line
                        Vec2 perp = {-to_player.y, to_player.x};
                        f32 wob = std::sin(world.time * 2.0f + static_cast<f32>(e)) * 0.35f;
                        movement.velocity = (to_player + perp * wob).normalized() * s;
                    } else {
                        movement.velocity = {0, 0};
                    }
                }
                break;

            case AIBehavior::Ranged:
                if (think) {
                    if (dist < ai.attack_range * 0.7f) {
                        movement.velocity = to_player * -movement.speed * 0.8f;     // back off
                    } else if (dist < ai.aggro_range) {
                        Vec2 perp = {-to_player.y, to_player.x};
                        f32 side = (static_cast<int>(e) % 2 == 0) ? 1.0f : -1.0f;
                        movement.velocity = perp * movement.speed * 0.5f * side;
                        if (dist > ai.attack_range * 1.4f) movement.velocity += to_player * movement.speed * 0.6f;
                    } else {
                        movement.velocity = {0, 0};
                    }
                }
                if (dist < ai.aggro_range && ai.shoot_timer <= 0.0f && has_line_of_sight(world, transform.position, player_pos)) {
                    ai.shoot_timer = 1.7f;
                    shoot_at(world, e, to_player, ProjectileKind::Arrow, dmg, 200.0f, 240.0f);
                }
                break;

            case AIBehavior::Caster:
                if (think) {
                    if (dist < ai.attack_range * 0.5f) movement.velocity = to_player * -movement.speed;
                    else if (dist > ai.attack_range * 1.3f && dist < ai.aggro_range) movement.velocity = to_player * movement.speed * 0.7f;
                    else movement.velocity = {0, 0};
                }
                if (dist < ai.aggro_range && ai.shoot_timer <= 0.0f && has_line_of_sight(world, transform.position, player_pos)) {
                    ai.shoot_timer = 2.3f;
                    // Slow, homing-ish bolt: aims where the player is now (slow enough to dodge)
                    shoot_at(world, e, to_player, ProjectileKind::Darkbolt, dmg, 120.0f, 260.0f);
                }
                break;

            case AIBehavior::FastLeaper:
                if (think) {
                    if (dist < ai.aggro_range && dist > ai.attack_range) {
                        movement.velocity = to_player * movement.speed;
                    } else if (dist <= ai.attack_range) {
                        movement.velocity = {0, 0};   // crouch before the leap
                    } else {
                        movement.velocity = {0, 0};
                    }
                }
                if (dist < ai.attack_range * 1.5f && ai.leap_timer <= 0.0f) {
                    ai.leap_timer = 1.6f;
                    movement.knockback = to_player * 300.0f;   // the leap itself
                    if (world.has(e, Component::Animation))
                        play_anim(world.animations[e], AnimID::Dash, true);
                }
                break;

            case AIBehavior::BuffAllies:
                if (think) {
                    if (dist < ai.attack_range * 0.8f) movement.velocity = to_player * -movement.speed * 0.6f;
                    else if (dist < ai.aggro_range) movement.velocity = to_player * movement.speed * 0.5f;
                    else movement.velocity = {0, 0};
                }
                if (dist < ai.aggro_range && ai.shoot_timer <= 0.0f) {
                    ai.shoot_timer = 2.8f;
                    f32 base = std::atan2(to_player.y, to_player.x);
                    for (int i = -1; i <= 1; i++) {
                        f32 a = base + i * 0.35f;
                        shoot_at(world, e, {std::cos(a), std::sin(a)}, ProjectileKind::Arrow, dmg * 0.6f, 180.0f, 220.0f);
                    }
                }
                break;
        }
    }
}
