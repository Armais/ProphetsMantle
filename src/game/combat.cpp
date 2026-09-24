// =============================================================================
// MANTLE - Combat System Implementation
// =============================================================================

#include "game/combat.h"
#include "game/enemies.h"
#include "game/faith.h"
#include "game/rooms.h"
#include "core/config.h"
#include "render/sprites.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>

static constexpr f32 PLAYER_HIT_IFRAMES = 1.0f;   // Invulnerability after taking a hit
static constexpr f32 HIT_FLASH_TIME     = Config::Feel::HIT_FLASH_TIME;
static constexpr f32 PLAYER_KNOCKBACK   = 160.0f;
static constexpr f32 ENEMY_KNOCKBACK    = 90.0f;

bool check_aabb_overlap(Vec2 pos_a, Vec2 size_a, Vec2 pos_b, Vec2 size_b,
                        Vec2& normal, f32& overlap) {
    f32 dx = pos_b.x - pos_a.x;
    f32 dy = pos_b.y - pos_a.y;
    f32 ox = (size_a.x + size_b.x) * 0.5f - std::abs(dx);
    f32 oy = (size_a.y + size_b.y) * 0.5f - std::abs(dy);

    if (ox <= 0 || oy <= 0) return false;

    if (ox < oy) {
        overlap = ox;
        normal = {dx > 0 ? 1.0f : -1.0f, 0.0f};
    } else {
        overlap = oy;
        normal = {0.0f, dy > 0 ? 1.0f : -1.0f};
    }
    return true;
}

bool check_circle_overlap(Vec2 pos_a, f32 radius_a, Vec2 pos_b, f32 radius_b,
                          Vec2& normal, f32& overlap) {
    Vec2 diff = pos_b - pos_a;
    f32 dist = diff.length();
    f32 min_dist = radius_a + radius_b;

    if (dist >= min_dist) return false;
    if (dist < 0.001f) { normal = {1.0f, 0.0f}; overlap = min_dist; return true; }

    normal = diff / dist;
    overlap = min_dist - dist;
    return true;
}

// -----------------------------------------------------------------------------
// Push an entity by delta unless that would put its FEET inside a solid tile.
// Same reference point as system_movement (pos + collider.offset) — the old centre-based
// test let body separation shove the player's feet into a River tile and freeze them there.
// Sliding: if the full push is blocked, try each axis alone.
static void push(ECSWorld& world, Entity e, Vec2 delta) {
    Vec2& pos = world.transforms[e].position;
    if (!world.active_room) { pos = pos + delta; return; }
    Vec2 foot = world.has(e, Component::Collider) ? world.colliders[e].offset : Vec2{0, 0};
    const Room& room = *world.active_room;
    Vec2 full = pos + delta;
    if (room.is_walkable_world(full + foot))      { pos = full; return; }
    Vec2 only_x = {pos.x + delta.x, pos.y};
    if (room.is_walkable_world(only_x + foot))    { pos = only_x; return; }
    Vec2 only_y = {pos.x, pos.y + delta.y};
    if (room.is_walkable_world(only_y + foot))    { pos = only_y; return; }
    // Fully blocked: stay put
}

static void knock(ECSWorld& world, Entity target, Vec2 dir, f32 strength) {
    if (!world.has(target, Component::Movement)) return;
    if (dir.length_sq() < 0.0001f) return;
    world.movements[target].knockback = dir.normalized() * strength;
}

static void pickup(ECSWorld& world, FaithSystem& faith, Entity item, CombatEvents& events) {
    auto& it = world.items[item];
    Entity p = world.player;
    if (p == ENTITY_NONE) return;
    Vec2 ipos = world.transforms[item].position;
    switch (it.pickup) {
        case ItemComponent::Pickup::Heart:
            if (world.has(p, Component::Health)) world.healths[p].heal(it.value);
            events.hearts_picked++;
            spawn_particles(world, ipos, {240, 80, 90, 255}, 8, 50.0f, 0.4f);
            break;
        case ItemComponent::Pickup::FaithOrb:
            faith.modify(it.value);
            faith.reset_inactivity();
            events.orbs_picked++;
            spawn_particles(world, ipos, {250, 235, 170, 255}, 8, 50.0f, 0.4f);
            break;
        case ItemComponent::Pickup::Offering:
            // No toca ni vida ni fe: la ofrenda es META. El Engine la vuelca en
            // `progression.offerings` y la guarda, asi que sobrevive a morirse.
            world.offerings_found += static_cast<u32>(it.value);
            spawn_particles(world, ipos, {255, 214, 110, 255}, 10, 55.0f, 0.5f);
            break;
        default: break;
    }
    world.destroy(item);
}

void system_collision(ECSWorld& world, FaithSystem& faith, CombatEvents& events) {
    // Simple O(n^2) - fine for <100 entities in a room
    for (u32 i = 0; i < world.entity_count; i++) {
        if (!world.is_alive(i)) continue;
        if (!world.has(i, Component::Collider) ||
            !world.has(i, Component::Transform)) continue;

        for (u32 j = i + 1; j < world.entity_count; j++) {
            if (!world.is_alive(i)) break;   // i may have been destroyed inside the loop
            if (!world.is_alive(j)) continue;
            if (!world.has(j, Component::Collider) ||
                !world.has(j, Component::Transform)) continue;

            auto& col_a = world.colliders[i];
            auto& col_b = world.colliders[j];
            auto& pos_a = world.transforms[i].position;
            auto& pos_b = world.transforms[j].position;

            Vec2 normal = {0, 0};
            f32 overlap = 0.0f;
            bool hit = false;

            // Circle vs Circle (default for characters/enemies)
            if (col_a.shape == ColliderComponent::Shape::Circle &&
                col_b.shape == ColliderComponent::Shape::Circle) {
                hit = check_circle_overlap(
                    pos_a + col_a.offset, col_a.radius,
                    pos_b + col_b.offset, col_b.radius,
                    normal, overlap);
            }
            // AABB vs AABB
            else if (col_a.shape == ColliderComponent::Shape::AABB &&
                     col_b.shape == ColliderComponent::Shape::AABB) {
                hit = check_aabb_overlap(
                    pos_a + col_a.offset, col_a.size,
                    pos_b + col_b.offset, col_b.size,
                    normal, overlap);
            }

            if (!hit) continue;

            // --- Handle collision based on entity types (order-independent) ---
            bool i_proj   = world.has(i, Component::Projectile);
            bool j_proj   = world.has(j, Component::Projectile);
            // "enemy" = HOSTIL. Un discipulo lleva EnemyAI y no es un enemigo: ni hace
            // dano al jugador, ni lo recibe de el, ni lo empuja.
            bool i_enemy  = is_hostile(world, i);
            bool j_enemy  = is_hostile(world, j);
            bool i_ally   = is_ally(world, i);
            bool j_ally   = is_ally(world, j);
            bool i_player = (i == world.player);
            bool j_player = (j == world.player);
            bool i_item   = world.has(i, Component::Item) && world.items[i].type == ItemComponent::Type::Pickup;
            bool j_item   = world.has(j, Component::Item) && world.items[j].type == ItemComponent::Type::Pickup;

            // Player picks up items
            if (i_item && j_player) { pickup(world, faith, i, events); continue; }
            if (j_item && i_player) { pickup(world, faith, j, events); continue; }
            if (i_item || j_item) continue;

            // Friendly projectile hits enemy
            if ((i_proj && j_enemy && world.projectiles[i].friendly) ||
                (j_proj && i_enemy && world.projectiles[j].friendly)) {
                Entity proj = i_proj ? i : j;
                Entity enemy = i_proj ? j : i;
                auto& pr = world.projectiles[proj];
                if (pr.damage > 0.0f && !pr.already_hit(enemy)) {
                    pr.mark_hit(enemy);
                    knock(world, enemy, pr.direction, ENEMY_KNOCKBACK);
                    events.enemy_hits++;
                    if (world.enemy_ais[enemy].is_boss) events.boss_hits++;
                    if (apply_damage(world, proj, enemy, pr.damage)) on_enemy_killed(world, faith, enemy, events);
                }
                if (!pr.pierce) world.destroy(proj);
            }
            // Enemy projectile hits player
            else if (i_proj && j_player && !world.projectiles[i].friendly) {
                if (!world.healths[j].invulnerable && world.healths[j].iframes_timer <= 0.0f) {
                    apply_damage(world, i, j, world.projectiles[i].damage);
                    knock(world, j, world.projectiles[i].direction, PLAYER_KNOCKBACK * 0.6f);
                    faith.on_take_damage();
                    events.player_hits++;
                }
                world.destroy(i);
            }
            else if (j_proj && i_player && !world.projectiles[j].friendly) {
                if (!world.healths[i].invulnerable && world.healths[i].iframes_timer <= 0.0f) {
                    apply_damage(world, j, i, world.projectiles[j].damage);
                    knock(world, i, world.projectiles[j].direction, PLAYER_KNOCKBACK * 0.6f);
                    faith.on_take_damage();
                    events.player_hits++;
                }
                world.destroy(j);
            }
            // Proyectil hostil contra un discipulo
            else if ((i_proj && j_ally && !world.projectiles[i].friendly) ||
                     (j_proj && i_ally && !world.projectiles[j].friendly)) {
                Entity proj = i_proj ? i : j;
                Entity ally = i_proj ? j : i;
                apply_damage(world, proj, ally, world.projectiles[proj].damage);
                world.destroy(proj);
            }
            // Enemy touches player (contact damage)
            else if ((i_enemy && j_player) || (j_enemy && i_player)) {
                Entity enemy = i_enemy ? i : j;
                Entity plyr  = i_player ? i : j;
                Vec2 away = (world.transforms[plyr].position - world.transforms[enemy].position);
                if (world.has(enemy, Component::Combat) && world.combats[enemy].can_attack() &&
                    world.healths[plyr].iframes_timer <= 0.0f && !world.healths[plyr].invulnerable) {
                    apply_damage(world, enemy, plyr, world.combats[enemy].damage);
                    knock(world, plyr, away, PLAYER_KNOCKBACK);
                    knock(world, enemy, away * -1.0f, ENEMY_KNOCKBACK * 0.5f);
                    faith.on_take_damage();
                    events.player_hits++;
                    world.combats[enemy].attack_timer = world.enemy_ais[enemy].is_boss ? 1.4f : 1.0f;
                    if (world.has(enemy, Component::Animation)) {
                        play_anim(world.animations[enemy],
                                  facing_anim(AnimID::AttackDown, world.transforms[enemy].facing), true);
                    }
                }
                if (!col_a.is_trigger && !col_b.is_trigger) {
                    push(world, i, normal * (-overlap * 0.5f));
                    push(world, j, normal * ( overlap * 0.5f));
                }
            }
            // Discipulo golpea a un hostil al tocarlo. Es todo su cuerpo a cuerpo: no
            // hace falta codigo de ataque aparte, la colision ya es el golpe.
            else if ((i_ally && j_enemy) || (j_ally && i_enemy)) {
                Entity ally  = i_ally ? i : j;
                Entity enemy = i_ally ? j : i;
                Vec2 away = (world.transforms[enemy].position - world.transforms[ally].position);
                if (world.has(ally, Component::Combat) && world.combats[ally].can_attack()) {
                    knock(world, enemy, away, ENEMY_KNOCKBACK);
                    events.enemy_hits++;
                    if (world.enemy_ais[enemy].is_boss) events.boss_hits++;
                    if (apply_damage(world, ally, enemy, world.combats[ally].damage))
                        on_enemy_killed(world, faith, enemy, events);
                    world.combats[ally].attack_timer = 0.8f;
                    if (world.has(ally, Component::Animation))
                        play_anim(world.animations[ally],
                                  facing_anim(AnimID::AttackDown, world.transforms[ally].facing), true);
                }
                // Y el hostil le devuelve el golpe: un discipulo tiene vida propia y se
                // puede perder, no es un escudo invulnerable
                if (world.is_alive(enemy) && world.has(enemy, Component::Combat) &&
                    world.combats[enemy].can_attack() && world.has(ally, Component::Health) &&
                    world.healths[ally].is_alive()) {
                    apply_damage(world, enemy, ally, world.combats[enemy].damage);
                    world.combats[enemy].attack_timer = 1.0f;
                }
                push(world, i, normal * (-overlap * 0.5f));
                push(world, j, normal * ( overlap * 0.5f));
            }
            // Un discipulo y el jugador se atraviesan: empujarse mutuamente en un pasillo
            // era la forma mas rapida de convertir la ayuda en un estorbo.
            else if ((i_ally && j_player) || (j_ally && i_player)) {
                continue;
            }
            // Enemy vs Enemy / discipulo vs discipulo (push apart only)
            else if ((i_enemy || i_ally) && (j_enemy || j_ally)) {
                push(world, i, normal * (-overlap * 0.5f));
                push(world, j, normal * ( overlap * 0.5f));
            }
        }
    }
}

bool apply_damage(ECSWorld& world, Entity attacker, Entity target, f32 damage) {
    if (!world.has(target, Component::Health)) return false;

    auto& health = world.healths[target];
    if (!health.is_alive()) return false;
    // Un objetivo invulnerable (La Muerte del cap. 8 fuera del cuerpo a cuerpo) rebota el
    // golpe: ni parpadeo, ni hit-stop, ni numero flotante. Solo un destello frio.
    if (health.invulnerable) {
        if (world.has(target, Component::Transform))
            spawn_particles(world, world.transforms[target].position, {170, 160, 200, 255}, 3, 45.0f, 0.2f);
        return false;
    }
    f32 before = health.current;
    health.take_damage(damage);
    health.hit_flash = HIT_FLASH_TIME;
    bool is_boss = world.has(target, Component::EnemyAI) && world.enemy_ais[target].is_boss;
    // Boss hits are rare enough to log: a hit that subtracts nothing is the signature of the
    // "boss stops taking damage" class of bugs (stale i-frames / recycled projectile hit_list).
    if (is_boss) {
        SDL_Log("boss hit: dmg %.1f  hp %.0f -> %.0f/%.0f%s", damage, before, health.current,
                health.max, (health.current >= before) ? "  [NO DAMAGE]" : "");
    }

    // ---- Congelacion de impacto, escalada por el mordisco ----------------------------
    // Antes solo la tenian los jefes, y un soldado reventado de un tiro no se distinguia
    // de un roce. Se toma el MAXIMO, nunca la suma: un milagro de area que toca a ocho
    // enemigos a la vez tiene que pesar como el peor golpe, no como ocho.
    f32 dealt_now = before - health.current;
    if (dealt_now > 0.0f) {
        f32 bite = (health.max > 0.0f) ? dealt_now / health.max : 0.0f;
        f32 stop = Config::Feel::HITSTOP_MIN + bite * Config::Feel::HITSTOP_PER_BITE;
        if (is_boss)             stop = std::max(stop, Config::Feel::HITSTOP_BOSS);
        if (target == world.player) stop = Config::Feel::HITSTOP_PLAYER;
        world.hitstop = std::max(world.hitstop, std::min(stop, Config::Feel::HITSTOP_MAX));
        // Sonda de tacto (comando `feel`): solo mide.
        if (target == world.player) world.dmg_taken += dealt_now;
        else                        world.dmg_dealt += dealt_now;
    }
    // Floating number over the target (a few frames of feedback; capped so a nuke cannot flood)
    Vec2 tpos = world.has(target, Component::Transform) ? world.transforms[target].position : Vec2{0, 0};
    f32 dealt = before - health.current;
    if (dealt > 0.0f && target != world.player && world.popups.size() < 24)
        world.popups.push_back({tpos, dealt, 0.0f, is_boss});
    if (target == world.player) {
        health.iframes_timer = PLAYER_HIT_IFRAMES;
        spawn_particles(world, tpos, {230, 60, 50, 255}, 8, 80.0f, 0.4f);
        spawn_fx(world, tpos, FxID::Impact, {255, 120, 100, 235}, 22.0f, 0.24f);
    } else {
        spawn_particles(world, tpos, {255, 150, 70, 255}, 4, 60.0f, 0.3f);
        // El destello del golpe sale de la hoja propia; el tinte lo distingue del que
        // recibe el jugador sin necesitar otra fila de dibujo.
        spawn_fx(world, tpos, FxID::Impact, {255, 235, 195, 230}, 18.0f, 0.20f);
    }

    if (!health.is_alive()) {
        if (target != world.player) {
            // El final se telegrafia: mas pavesas, un anillo que se abre y el cadaver
            // saliendo despedido en la direccion del golpe. Antes desaparecia sin mas.
            spawn_particles(world, tpos, {255, 200, 120, 255}, 18, 105.0f, 0.55f);
            // Onda de la muerte: la hoja propia si esta cargada, y si no el icono del
            // tileset de siempre. Un efecto nuevo no puede dejar al juego sin feedback.
            if (spawn_fx(world, tpos, FxID::Ring, {255, 226, 180, 225}, 26.0f, 0.28f) == ENTITY_NONE)
                spawn_vfx(world, tpos, TileSprite::Star, {255, 225, 170, 210}, 9.0f, 0.22f, 3.0f, 220.0f);

            Vec2 away = {0, 0};
            if (attacker != ENTITY_NONE && world.is_alive(attacker) &&
                world.has(attacker, Component::Transform))
                away = tpos - world.transforms[attacker].position;
            if (away.length_sq() < 0.0001f) away = {0, -1};

            bool has_sheet = world.has(target, Component::Sprite) &&
                             world.sprites[target].texture_id != TEXTURE_NONE &&
                             world.has(target, Component::Animation);
            if (has_sheet) {
                // Leave an inert corpse for the Death animation, then auto-destroy.
                // Health stays at 0 so select_animation() picks AnimID::Death.
                // El Movement se CONSERVA (velocidad a cero) para que el empujon del
                // remate se vea: sin colisionador ya no estorba a nadie, y update_enemy_ai
                // salta los cadaveres por vida, no por componente.
                if (world.has(target, Component::Movement)) {
                    world.movements[target].velocity = {0, 0};
                    world.movements[target].knockback = away.normalized() * Config::Feel::DEATH_PUSH;
                }
                world.remove(target, Component::Collider);
                world.remove(target, Component::Combat);
                world.add(target, Component::Lifetime);
                world.lifetimes[target].remaining = anim_duration(AnimID::Death);
                world.lifetimes[target].total = anim_duration(AnimID::Death);
                // Keep EnemyAI (name/boss flag) for on_enemy_killed; AI skips dead entities
            } else {
                world.destroy(target);
            }
        }
        return true;
    }

    (void)attacker;
    return false;
}

void on_enemy_killed(ECSWorld& world, FaithSystem& faith, Entity enemy, CombatEvents& events) {
    events.enemy_kills++;
    if (world.has(enemy, Component::EnemyAI) && world.enemy_ais[enemy].enemy_type < 32)
        events.kill_mask |= (1u << world.enemy_ais[enemy].enemy_type);
    Vec2 pos = world.has(enemy, Component::Transform) ? world.transforms[enemy].position : Vec2{160, 90};
    bool boss = false;
    u32 type = 0;
    u8 split_gen = 0;
    if (world.has(enemy, Component::EnemyAI)) {
        boss = world.enemy_ais[enemy].is_boss;
        type = world.enemy_ais[enemy].enemy_type;
        split_gen = world.enemy_ais[enemy].split_generation;
        // (EnemyAI stays on the corpse for depth/shadow; count_living_enemies checks health)
    }

    // Conducta de muerte del Acto II (explotar, dividirse). Va antes que los drops y
    // siempre antes de Engine::update_room_state: los hijos deben contar como vivos.
    on_enemy_death_behavior(world, enemy, pos);

    if (boss) {
        events.boss_kills++;
        faith.on_defeat_boss();
        spawn_pickup(world, pos + Vec2{-10, 0}, ItemComponent::Pickup::Heart, 25.0f);
        spawn_pickup(world, pos + Vec2{10, 0}, ItemComponent::Pickup::FaithOrb, 10.0f);
        return;
    }

    // Los hijos del Agua Corrupta no pagan como el padre: si no, dividirse seria farmear
    if (split_gen > 0) {
        faith.modify(0.25f);
        faith.reset_inactivity();
        return;
    }

    // Prophets of Baal are worth more faith (GDD: +3)
    if (type == 2) faith.on_kill_prophet(); else faith.modify(1.0f);
    faith.reset_inactivity();

    // Drops
    f32 roll = randf();
    if (roll < 0.16f)      spawn_pickup(world, pos, ItemComponent::Pickup::Heart, 8.0f);
    else if (roll < 0.40f) spawn_pickup(world, pos, ItemComponent::Pickup::FaithOrb, 4.0f);
}

Entity pick_target(const ECSWorld& world, Vec2 from, f32 max_range) {
    Entity best = ENTITY_NONE;
    f32 best_score = -1e9f;
    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e) || !is_hostile(world, e) || !world.has(e, Component::Health)) continue;
        if (!world.healths[e].is_alive()) continue;
        f32 dist = world.transforms[e].position.distance_to(from);
        if (dist > max_range) continue;

        // Higher score wins: distance dominates, then "is attacking me", boss, and low HP
        f32 score = -dist;
        if (world.has(e, Component::Combat) && world.combats[e].attack_timer > 0.0f) score += 40.0f;
        if (world.enemy_ais[e].is_boss) score += 90.0f;
        score += (1.0f - world.healths[e].ratio()) * 25.0f;
        if (score > best_score) { best_score = score; best = static_cast<Entity>(e); }
    }
    return best;
}

Entity spawn_projectile(ECSWorld& world, Vec2 position, Vec2 direction,
                        f32 damage, bool friendly, ProjectileKind kind, f32 speed, f32 range) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;

    world.add(e, Component::Sprite);
    world.sprites[e].z_order = 8;
    if (world.tileset != TEXTURE_NONE) {
        TileSprite s = TileSprite::Fireball;
        switch (kind) {
            case ProjectileKind::Fireball:  s = TileSprite::Fireball;  break;
            case ProjectileKind::Lightbolt: s = TileSprite::Lightbolt; break;
            case ProjectileKind::Arrow:     s = TileSprite::Arrow;     break;
            case ProjectileKind::Darkbolt:  s = TileSprite::Darkbolt;  break;
            case ProjectileKind::Stone:     s = TileSprite::Rock;      break;
        }
        world.sprites[e].texture_id = world.tileset;
        world.sprites[e].src_rect = get_tile_src(s);
        if (kind == ProjectileKind::Stone) {
            // La roca del tileset es un peñasco de un tile: a 0,6 queda una piedra lisa de
            // ~10 unidades. A 0,45 (7 unidades) se veia como un borron gris en movimiento.
            world.transforms[e].scale = {0.6f, 0.6f};
        }
    } else {
        world.sprites[e].texture_id = TEXTURE_NONE;
        world.sprites[e].src_rect = {0, 0, 6, 6};
        world.sprites[e].tint = friendly ? Color{255, 200, 80, 255} : Color{200, 80, 200, 255};
    }

    world.add(e, Component::Projectile);
    world.projectiles[e].direction = direction.normalized();
    world.projectiles[e].origin = position;
    world.projectiles[e].speed = speed > 0.0f ? speed : Config::PROJECTILE_SPEED;
    world.projectiles[e].damage = damage;
    world.projectiles[e].max_range = range > 0.0f ? range : Config::PROJECTILE_RANGE;
    world.projectiles[e].friendly = friendly;
    world.projectiles[e].spin = (kind == ProjectileKind::Stone) ? 540.0f : 0.0f;

    world.add(e, Component::Collider);
    world.colliders[e].shape = ColliderComponent::Shape::Circle;
    world.colliders[e].radius = 4.0f;
    world.colliders[e].offset = {0, 0};
    world.colliders[e].is_trigger = true;

    world.add(e, Component::Lifetime);
    world.lifetimes[e].remaining = 4.0f;

    return e;
}

void spawn_particles(ECSWorld& world, Vec2 position, Color color, int count, f32 speed, f32 life) {
    for (int i = 0; i < count; i++) {
        Entity e = world.create();
        if (e == ENTITY_NONE) return;
        world.add(e, Component::Transform);
        world.transforms[e].position = position + Vec2{randf_range(-3.0f, 3.0f), randf_range(-3.0f, 3.0f)};
        world.add(e, Component::Sprite);
        f32 s = randf_range(1.5f, 3.5f);
        if (world.tileset != TEXTURE_NONE) {
            // Spark sprite instead of a raw rect: tinted star, scaled down to `s` game units
            world.sprites[e].texture_id = world.tileset;
            world.sprites[e].src_rect = get_tile_src(TileSprite::Star);
            world.transforms[e].scale = {s / 16.0f, s / 16.0f};
            world.transforms[e].rotation = randf_range(0.0f, 360.0f);
        } else {
            world.sprites[e].texture_id = TEXTURE_NONE;
            world.sprites[e].src_rect = {0, 0, s, s};
        }
        world.sprites[e].tint = color;
        world.sprites[e].z_order = 9;
        world.add(e, Component::Movement);
        f32 a = randf_range(0.0f, 6.2831853f);
        f32 v = speed * randf_range(0.4f, 1.2f);
        world.movements[e].velocity = {std::cos(a) * v, std::sin(a) * v};
        world.movements[e].friction = 6.0f;
        world.add(e, Component::Lifetime);
        world.lifetimes[e].remaining = life * randf_range(0.6f, 1.3f);
    }
}

// Celda de la hoja de efectos, en texeles. Ver tools/blender/mantle_sprites/vfx.py.
static constexpr f32 FX_CELL   = 128.0f;
static constexpr int FX_FRAMES = 8;

Entity spawn_fx(ECSWorld& world, Vec2 position, FxID id, Color tint,
                f32 size_units, f32 duration, i32 z_order) {
    if (world.vfx_sheet == TEXTURE_NONE || id >= FxID::COUNT) return ENTITY_NONE;
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;
    // La celda son 32 unidades de juego (128 texeles / ASSET_SCALE 4), NO las 16 del
    // tileset: escalar contra 16 dejaba todos los efectos al doble de lo pedido.
    f32 sc = size_units / (FX_CELL / static_cast<f32>(Config::ASSET_SCALE));
    world.transforms[e].scale = {sc, sc};

    world.add(e, Component::Sprite);
    world.sprites[e].texture_id = world.vfx_sheet;
    world.sprites[e].src_rect = {0.0f, static_cast<f32>(id) * FX_CELL, FX_CELL, FX_CELL};
    world.sprites[e].tint = tint;
    world.sprites[e].z_order = z_order;

    world.add(e, Component::Animation);
    auto& an = world.animations[e];
    an.row = static_cast<u32>(id);
    an.frame_count = FX_FRAMES;
    an.frame_time = duration / static_cast<f32>(FX_FRAMES);
    an.current_frame = 0;
    an.timer = 0.0f;
    an.looping = false;
    an.playing = true;

    world.add(e, Component::Lifetime);
    world.lifetimes[e].remaining = duration;
    world.lifetimes[e].total = duration;
    world.lifetimes[e].fade = true;   // el ultimo tramo se apaga en vez de desaparecer
    return e;
}

Entity spawn_vfx(ECSWorld& world, Vec2 position, TileSprite sprite, Color tint,
                 f32 size_units, f32 life, f32 grow, f32 spin, Vec2 velocity, i32 z_order) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;

    world.add(e, Component::Sprite);
    world.sprites[e].z_order = z_order;
    world.sprites[e].tint = tint;
    if (world.tileset != TEXTURE_NONE) {
        world.sprites[e].texture_id = world.tileset;
        world.sprites[e].src_rect = get_tile_src(sprite);
        // Las celdas del tileset son de 16 unidades: la escala lleva el sprite al tamano pedido
        f32 s = size_units / static_cast<f32>(Config::TILE_SIZE);
        world.transforms[e].scale = {s, s};
    } else {
        world.sprites[e].texture_id = TEXTURE_NONE;
        world.sprites[e].src_rect = {0, 0, size_units, size_units};
    }

    if (velocity.length_sq() > 0.0f) {
        world.add(e, Component::Movement);
        world.movements[e].velocity = velocity;
        world.movements[e].friction = 2.0f;
    }

    world.add(e, Component::Lifetime);
    world.lifetimes[e].remaining = life;
    world.lifetimes[e].total = life;
    world.lifetimes[e].fade = true;
    world.lifetimes[e].grow = grow;
    world.lifetimes[e].spin = spin;
    return e;
}

Entity spawn_pickup(ECSWorld& world, Vec2 position, ItemComponent::Pickup kind, f32 value) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;

    world.add(e, Component::Sprite);
    if (world.tileset != TEXTURE_NONE) {
        world.sprites[e].texture_id = world.tileset;
        world.sprites[e].src_rect = get_tile_src(
            kind == ItemComponent::Pickup::Heart    ? TileSprite::Heart :
            kind == ItemComponent::Pickup::Offering ? TileSprite::Star  : TileSprite::Orb);
        // La ofrenda se tine de oro para que no se confunda con un orbe de fe: es lo unico
        // del suelo que te llevas puesto cuando la partida acaba.
        if (kind == ItemComponent::Pickup::Offering)
            world.sprites[e].tint = {255, 206, 90, 255};
    } else {
        world.sprites[e].texture_id = TEXTURE_NONE;
        world.sprites[e].src_rect = {0, 0, 8, 8};
        world.sprites[e].tint = kind == ItemComponent::Pickup::Heart    ? Color{220, 50, 50, 255}
                              : kind == ItemComponent::Pickup::Offering ? Color{255, 206, 90, 255}
                                                                        : Color{240, 230, 160, 255};
    }
    world.sprites[e].z_order = 4;

    world.add(e, Component::Item);
    world.items[e].type = ItemComponent::Type::Pickup;
    world.items[e].pickup = kind;
    world.items[e].value = value;
    world.items[e].bob_phase = randf_range(0.0f, 6.28f);

    world.add(e, Component::Collider);
    world.colliders[e].shape = ColliderComponent::Shape::Circle;
    world.colliders[e].radius = 7.0f;
    world.colliders[e].offset = {0, 0};
    world.colliders[e].is_trigger = true;

    world.add(e, Component::Lifetime);
    world.lifetimes[e].remaining = 14.0f;

    return e;
}
