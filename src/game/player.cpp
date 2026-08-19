// =============================================================================
// MANTLE - Player Implementation
// =============================================================================

#include "game/player.h"
#include "input/input.h"
#include "game/faith.h"
#include "game/combat.h"
#include "game/items.h"
#include "core/config.h"
#include "render/sprites.h"
#include <algorithm>

void Player::update(ECSWorld& world, Input& input, FaithSystem& faith, f32 dt) {
    if (entity == ENTITY_NONE || !world.is_alive(entity)) return;

    auto& transform = world.transforms[entity];
    auto& movement  = world.movements[entity];
    auto& combat    = world.combats[entity];

    // ---- Movement ----
    // (no flip_x: sprite sheets author all 4 directions, animation row follows facing)
    Vec2 move = input.get_move_axis();
    if (move.length_sq() > 0.01f) {
        movement.velocity = move.normalized() * movement.speed;
        last_move_dir = move.normalized();   // 8-way; also the shooting direction
        faith.reset_inactivity();
    }

    // ---- Dash ----
    if (input.is_action_just_pressed(Action::Dash) &&
        !movement.is_dashing && movement.dash_cooldown <= 0.0f) {

        Vec2 dash_dir = move.length_sq() > 0.01f ? move.normalized() : Vec2{0, -1};
        movement.is_dashing = true;
        movement.dash_dir = dash_dir;
        movement.dash_timer = Config::DASH_DURATION;
        movement.dash_cooldown = Config::DASH_COOLDOWN;

        // I-frames during dash
        world.healths[entity].iframes_timer = Config::IFRAMES_DURATION;

        // TODO: play dash SFX
    }

    // ---- Attack ----
    if (input.is_action_pressed(Action::Attack) && combat.can_attack()) {
        combat.attack_timer = combat.attack_speed;
        faith.reset_inactivity();

        // Apply faith damage multiplier (+ Doble Porcion, + Vino Nuevo)
        f32 actual_damage = combat.damage * faith.get_damage_multiplier();
        if (double_portion_timer > 0.0f) actual_damage *= 2.0f;
        if (damage_buff_timer > 0.0f)    actual_damage *= 1.25f;

        // Shots go where the player last moved (8-way). `aim_axis` is only written by the
        // autoplay bot / scripts (no mouse or right-stick aiming in normal play).
        Vec2 aim = input.get_aim_axis();
        if (aim.length_sq() < 0.01f && input.auto_aim_enabled) {
            // Mobile (GDD 4.3): aiming with a thumb is unreliable, so the shot snaps to the
            // priority target when there is one; otherwise it falls back to the move direction.
            Entity target = pick_target(world, transform.position);
            if (target != ENTITY_NONE) {
                input.auto_aim_target = world.transforms[target].position;
                aim = input.auto_aim_target - transform.position;
            }
        }
        if (aim.length_sq() < 0.01f) aim = last_move_dir;
        aim = aim.normalized();

        // Face the aim direction and play the attack animation (restart on every shot)
        Direction aim_dir = direction_from_vec(aim);
        if (aim_dir != Direction::None) transform.facing = aim_dir;
        play_anim(world.animations[entity], facing_anim(AnimID::AttackDown, transform.facing), true);

        ProjectileKind kind = (character == CharacterType::Elias) ? ProjectileKind::Fireball
                                                                  : ProjectileKind::Lightbolt;
        spawn_projectile(world, transform.position + aim * 6.0f, aim, actual_damage, true, kind);
        shots_fired++;
    }

    // ---- Miracles ----
    for (int i = 0; i < active_slot_count; i++) {
        miracle_slots[i].update(dt);

        Action miracle_action = (i == 0) ? Action::Miracle1 : Action::Miracle2;
        if (input.is_action_just_pressed(miracle_action)) {
            auto& slot = miracle_slots[i];
            if (slot.is_empty()) continue;
            auto def = get_miracle_def(slot.miracle);
            if (def.tier > faith.get_max_miracle_tier()) {
                last_event = MiracleEvent::NeedFaith;
                continue;
            }
            if (def.faith_cost > faith.current) {
                last_event = MiracleEvent::NeedFaith;
                continue;
            }
            if (slot.try_cast(faith.current, faith.get_max_miracle_tier())) {
                faith.modify(-def.faith_cost, true);   // miracle cost ignores the relic floor
                play_anim(world.animations[entity], AnimID::CastMiracle, true);
                cast_miracle(world, faith, slot.miracle);
                SDL_Log("Cast miracle: %s (cost: %.0f faith)", def.name, def.faith_cost);
            }
        }
    }

    // ---- Ongoing miracle effects ----
    if (heal_zone_timer > 0.0f) {
        heal_zone_timer -= dt;
        if (transform.position.distance_to(heal_zone_pos) < 34.0f) {
            world.healths[entity].heal(6.0f * dt);
        }
    }
    if (double_portion_timer > 0.0f) double_portion_timer -= dt;
    if (damage_buff_timer > 0.0f)    damage_buff_timer -= dt;
    // (the consumable key is handled by the Engine, which owns the ItemRegistry)

    // Position is NOT clamped here — tile collision (walls) blocks non-door edges,
    // and door tiles let the player cross the border to trigger room transitions.
}

// =============================================================================
// Items (choice screen / inheritance)
// =============================================================================
const char* Player::grant_item(ECSWorld& world, FaithSystem& faith, const ItemDef& def, bool record) {
    if (entity == ENTITY_NONE || !world.is_alive(entity)) return "";
    if (record) items.push_back(def.id);

    // Flat stat bonuses (every category can carry them)
    if (def.hp_bonus != 0.0f && world.has(entity, Component::Health)) {
        world.healths[entity].max += def.hp_bonus;
        world.healths[entity].heal(def.hp_bonus);
    }
    if (world.has(entity, Component::Combat)) {
        world.combats[entity].damage += def.damage_bonus;
        world.combats[entity].attack_speed =
            std::max(0.08f, world.combats[entity].attack_speed + def.attack_speed_bonus);
    }
    if (def.speed_bonus != 0.0f && world.has(entity, Component::Movement))
        world.movements[entity].speed += def.speed_bonus;
    if (def.faith_bonus != 0.0f) faith.modify(def.faith_bonus);

    switch (def.category) {
        case ItemCategory::Relic:
            relic_slot = def.id;
            // Relic effects that are not flat stats
            if (def.id == 6) faith.floor_value = 25.0f;   // Tablas de la Ley
            if (def.id == 7) miracle_power = 1.5f;        // Arca del Pacto
            break;
        case ItemCategory::Weapon:
            // No secondary-attack input yet: the weapon acts as its flat damage bonus and
            // shows in the HUD. TODO: hook AttackSecondary when weapons get their own logic.
            weapon_slot = def.id;
            break;
        case ItemCategory::Consumable:
            consumable_slot = def.id;   // Replaces whatever was held
            break;
        default:
            break;
    }
    return def.description;
}

bool Player::use_consumable(ECSWorld& world, const ItemRegistry& registry) {
    if (consumable_slot == UINT32_MAX) return false;
    const ItemDef* def = registry.get(consumable_slot);
    if (!def) { consumable_slot = UINT32_MAX; return false; }

    switch (def->id) {
        case 8:  damage_buff_timer = 30.0f; break;                                  // Vino Nuevo
        case 9:  if (world.has(entity, Component::Health)) world.healths[entity].heal(30.0f); break;
        default: break;
    }
    // Consumables are removed from the inventory list too, so re-rolls can offer them again
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i] == consumable_slot) { items.erase(items.begin() + i); break; }
    }
    consumable_slot = UINT32_MAX;
    return true;
}

// =============================================================================
// Miracle effects
// =============================================================================
static void ring_of_bolts(ECSWorld& world, Vec2 center, int count, ProjectileKind kind,
                          f32 damage, f32 speed, f32 range, bool pierce) {
    for (int i = 0; i < count; i++) {
        f32 a = (6.2831853f * i) / count;
        Entity p = spawn_projectile(world, center, {std::cos(a), std::sin(a)}, damage, true, kind, speed, range);
        if (p != ENTITY_NONE) world.projectiles[p].pierce = pierce;
    }
}

void Player::cast_miracle(ECSWorld& world, FaithSystem& faith, MiracleID id) {
    if (entity == ENTITY_NONE) return;
    Vec2 pos = world.transforms[entity].position;
    // Eliseo inherits Elias' miracles but casts them weaker (GDD: "unlocked but weaker")
    f32 inherit = (character == CharacterType::Eliseo && get_miracle_def(id).owner == Prophet::Elias)
                  ? 0.7f : 1.0f;
    f32 mult = faith.get_damage_multiplier() * miracle_power * inherit;   // Arca del Pacto: x1.5
    last_event = MiracleEvent::Cast;
    last_cast = id;

    switch (id) {
        case MiracleID::CuervosDeQuerit:
            world.healths[entity].heal(15.0f * inherit);
            break;

        case MiracleID::PalabraDeSequia:
            // Drought: every enemy moves at half speed while the timer runs (see update_enemy_ai)
            world.enemy_slow_timer = get_miracle_def(id).duration;
            break;

        case MiracleID::FuegoDelCielo: {
            // Divine fire: heavy damage to everything near the prophet + visual ring
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
                if (!world.healths[e].is_alive()) continue;
                Vec2 ep = world.transforms[e].position;
                if (ep.distance_to(pos) < 80.0f) {
                    if (apply_damage(world, entity, e, 80.0f * mult)) pending_kills.push_back(e);
                    if (world.has(e, Component::Movement))
                        world.movements[e].knockback = (ep - pos).normalized() * 200.0f;
                }
            }
            ring_of_bolts(world, pos, 14, ProjectileKind::Fireball, 0.0f, 240.0f, 80.0f, true);
            shake_request = 5.0f;
            break;
        }

        case MiracleID::CarroDeFuego: {
            // Devastating dash: long i-frame dash that leaves fire behind
            auto& m = world.movements[entity];
            Vec2 dir = m.velocity.length_sq() > 1.0f ? m.velocity.normalized() : Vec2{0, -1};
            m.is_dashing = true;
            m.dash_dir = dir;
            m.dash_timer = Config::DASH_DURATION * 2.5f;
            m.dash_cooldown = 0.0f;
            world.healths[entity].iframes_timer = 1.0f;
            for (int i = 0; i < 6; i++) {
                Entity p = spawn_projectile(world, pos, dir, 30.0f * mult, true, ProjectileKind::Fireball, 300.0f + i * 40.0f, 140.0f);
                if (p != ENTITY_NONE) world.projectiles[p].pierce = true;
            }
            shake_request = 3.0f;
            break;
        }

        case MiracleID::AltarConsumido:
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
                if (!world.healths[e].is_alive()) continue;
                f32 dmg = world.enemy_ais[e].is_boss ? 120.0f * mult : 1000.0f;
                if (apply_damage(world, entity, e, dmg)) pending_kills.push_back(e);
            }
            ring_of_bolts(world, pos, 24, ProjectileKind::Fireball, 0.0f, 300.0f, 200.0f, true);
            shake_request = 8.0f;
            break;

        case MiracleID::SanarAguas:
            heal_zone_pos = pos;
            heal_zone_timer = get_miracle_def(id).duration;
            break;

        case MiracleID::AceiteInagotable:
            world.healths[entity].heal(20.0f);
            faith.modify(6.0f);
            break;

        case MiracleID::OjosAbiertos:
            faith.modify(10.0f);
            world.healths[entity].iframes_timer = 3.0f;
            break;

        case MiracleID::EjercitoCelestial: {
            // Celestial host: piercing light bolts towards the nearest enemies (3 volleys spread)
            int fired = 0;
            for (u32 e = 0; e < world.entity_count && fired < 6; e++) {
                if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
                if (!world.healths[e].is_alive()) continue;
                Vec2 dir = (world.transforms[e].position - pos).normalized();
                Entity p = spawn_projectile(world, pos, dir, 30.0f * mult, true, ProjectileKind::Lightbolt, 220.0f, 260.0f);
                if (p != ENTITY_NONE) world.projectiles[p].pierce = true;
                fired++;
            }
            if (fired == 0) ring_of_bolts(world, pos, 8, ProjectileKind::Lightbolt, 25.0f * mult, 220.0f, 200.0f, true);
            else ring_of_bolts(world, pos, 8, ProjectileKind::Lightbolt, 15.0f * mult, 200.0f, 160.0f, true);
            break;
        }

        case MiracleID::DoblePorcion:
            double_portion_timer = get_miracle_def(id).duration;
            world.healths[entity].heal(20.0f);
            break;

        default:
            break;
    }
}
