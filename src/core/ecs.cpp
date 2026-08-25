// =============================================================================
// MANTLE - ECS World Implementation
// =============================================================================

#include "core/ecs.h"
#include "game/rooms.h"
#include "render/renderer.h"
#include "render/sprites.h"
#include <algorithm>
#include <cstring>

void ECSWorld::init() {
    clear();
}

void ECSWorld::clear() {
    entity_count = 0;
    free_list.clear();
    player = ENTITY_NONE;
    active_room = nullptr;
    popups.clear();
    hitstop = 0.0f;
    for (u32 i = 0; i < MAX_ENTITIES; i++) {
        masks[i].reset();
    }
}

// Entity ids are recycled through the free list, so a new entity can land on a slot whose
// component data still belongs to the previous owner. Every spawner would have to clear every
// field it does not set; instead the slot starts from defaults here. (This is what made the
// chapter boss look immune: a recycled ProjectileComponent kept hit_list/hit_count, so the new
// shot was "already_hit" on the boss id and was destroyed without dealing damage.)
void ECSWorld::reset_components(Entity e) {
    transforms[e]  = TransformComponent{};
    sprites[e]     = SpriteComponent{};
    animations[e]  = AnimationComponent{};
    healths[e]     = HealthComponent{};
    combats[e]     = CombatComponent{};
    movements[e]   = MovementComponent{};
    colliders[e]   = ColliderComponent{};
    enemy_ais[e]   = EnemyAIComponent{};
    projectiles[e] = ProjectileComponent{};
    items[e]       = ItemComponent{};
    lifetimes[e]   = LifetimeComponent{};
}

Entity ECSWorld::create() {
    Entity e;
    if (!free_list.empty()) {
        e = free_list.back();
        free_list.pop_back();
    } else {
        if (entity_count >= MAX_ENTITIES) {
            SDL_Log("ECS: Max entities reached (%d)", MAX_ENTITIES);
            return ENTITY_NONE;
        }
        e = entity_count++;
    }
    masks[e].reset();
    reset_components(e);
    return e;
}

void ECSWorld::destroy(Entity e) {
    if (e == ENTITY_NONE || e >= MAX_ENTITIES) return;
    masks[e].reset();
    free_list.push_back(e);
    if (e == player) player = ENTITY_NONE;
}

bool ECSWorld::is_alive(Entity e) const {
    return e != ENTITY_NONE && e < MAX_ENTITIES && masks[e].any();
}

bool ECSWorld::has(Entity e, Component c) const {
    return masks[e].test(static_cast<u32>(c));
}

void ECSWorld::add(Entity e, Component c) {
    masks[e].set(static_cast<u32>(c));
}

void ECSWorld::remove(Entity e, Component c) {
    masks[e].reset(static_cast<u32>(c));
}

// =============================================================================
// System: Movement
// =============================================================================
// Walkability test at an entity's ground point (transform + collider offset = feet)
static bool walkable_at(const ECSWorld& w, Vec2 p) {
    if (!w.active_room) return true;
    return w.active_room->is_walkable_world(p);
}

// Safety net: a body whose feet are already inside a solid tile (teleport, bad spawn, any
// future push) would otherwise be frozen forever, because every candidate move is also
// solid. Snap it to the nearest walkable tile centre instead. Logged so it shows up in
// autoplay logs if some system starts putting entities into walls again.
static void unstick(ECSWorld& w, u32 e, Vec2 foot) {
    if (!w.active_room) return;
    auto& t = w.transforms[e];
    Vec2 feet = t.position + foot;
    if (w.active_room->is_walkable_world(feet)) return;
    // Only in-room feet: the player legitimately steps off-grid when crossing an opening
    if (feet.x < 0.0f || feet.y < 0.0f ||
        feet.x >= Config::ROOM_TILES_X * Config::TILE_SIZE ||
        feet.y >= Config::ROOM_TILES_Y * Config::TILE_SIZE) return;
    Vec2 free_feet;
    if (w.active_room->nearest_walkable_world(feet, free_feet)) {
        SDL_Log("unstick: entity %u feet (%.0f,%.0f) inside solid tile -> (%.0f,%.0f)",
                e, feet.x, feet.y, free_feet.x, free_feet.y);
        t.position = free_feet - foot;
    }
}

static void system_movement(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        if (!w.has(static_cast<Entity>(e), Component::Transform) ||
            !w.has(static_cast<Entity>(e), Component::Movement)) continue;

        auto& t = w.transforms[e];
        auto& m = w.movements[e];
        bool solid_body = w.has(static_cast<Entity>(e), Component::Collider) && !w.colliders[e].is_trigger;
        Vec2 foot = w.has(static_cast<Entity>(e), Component::Collider) ? w.colliders[e].offset : Vec2{0, 0};
        if (solid_body) unstick(w, e, foot);

        // Dash logic
        if (m.is_dashing) {
            m.dash_timer -= dt;
            if (m.dash_timer <= 0.0f) {
                m.is_dashing = false;
                m.velocity = {0, 0};
            } else {
                Vec2 old_pos = t.position;
                t.position += m.dash_dir * Config::DASH_SPEED * dt;

                // Tile collision during dash (slide like normal movement)
                if (!walkable_at(w, t.position + foot)) {
                    Vec2 try_x = {t.position.x, old_pos.y};
                    Vec2 try_y = {old_pos.x, t.position.y};
                    if (walkable_at(w, try_x + foot))      t.position = try_x;
                    else if (walkable_at(w, try_y + foot)) t.position = try_y;
                    else                                   t.position = old_pos;
                }
                continue;  // Skip normal movement during dash
            }
        }

        // Dash cooldown
        if (m.dash_cooldown > 0.0f) {
            m.dash_cooldown -= dt;
        }

        // Knockback impulse decays quickly
        if (m.knockback.length_sq() > 1.0f) {
            m.knockback = m.knockback * (1.0f / (1.0f + 14.0f * dt));
        } else {
            m.knockback = {0, 0};
        }

        // Apply velocity. Shallow water (TileType::Water) drags bodies down (GDD 7.1)
        Vec2 old_pos = t.position;
        f32 drag = (w.active_room && w.active_room->is_slow_world(t.position + foot))
                   ? Config::WATER_SPEED_MULT : 1.0f;
        t.position += (m.velocity * drag + m.knockback) * dt;

        // Tile collision check (feet point); slide along the free axis
        if (!walkable_at(w, t.position + foot)) {
            Vec2 try_x = {t.position.x, old_pos.y};
            Vec2 try_y = {old_pos.x, t.position.y};
            if (walkable_at(w, try_x + foot))      t.position = try_x;  // Slide along X
            else if (walkable_at(w, try_y + foot)) t.position = try_y;  // Slide along Y
            else                                   t.position = old_pos; // Fully blocked
        }

        // Non-player entities never leave the room (enemies must stay killable)
        if (static_cast<Entity>(e) != w.player) {
            t.position.x = std::max(12.0f, std::min(t.position.x, static_cast<f32>(Config::GAME_WIDTH) - 12.0f));
            t.position.y = std::max(12.0f, std::min(t.position.y, static_cast<f32>(Config::ROOM_TILES_Y * Config::TILE_SIZE) - 12.0f));
        }

        // Friction
        m.velocity = m.velocity * (1.0f / (1.0f + m.friction * dt));

        // Update facing direction
        if (m.velocity.length_sq() > 1.0f) {
            if (std::abs(m.velocity.x) > std::abs(m.velocity.y)) {
                t.facing = m.velocity.x > 0 ? Direction::Right : Direction::Left;
            } else {
                t.facing = m.velocity.y > 0 ? Direction::Down : Direction::Up;
            }
        }
    }
}

// =============================================================================
// System: Animation
// =============================================================================
// Pick the animation row from gameplay state (only for textured sprites).
// Priority: Death > Dash > one-shot in progress (Attack/Cast, pushed by events) > Walk > Idle
static void select_animation(ECSWorld& w, Entity e) {
    auto& anim = w.animations[e];
    if (w.sprites[e].texture_id == TEXTURE_NONE) return;

    if (w.has(e, Component::Health) && !w.healths[e].is_alive()) {
        play_anim(anim, AnimID::Death);
        return;
    }
    if (w.has(e, Component::Movement) && w.movements[e].is_dashing) {
        play_anim(anim, AnimID::Dash);
        return;
    }
    // Un EFECTO (Lifetime y sin vida propia) manda su propia fila: nadie se la elige.
    // Sin esto, al acabar su animacion `playing` se apaga, la comprobacion de abajo deja
    // de cortar y el efecto salta a la fila de Idle en su ultimo fotograma. Los cadaveres
    // NO entran aqui: tienen Health, y su rama de Death va antes.
    if (w.has(e, Component::Lifetime) && !w.has(e, Component::Health)) return;
    if (!anim.looping && anim.playing) return;   // let Attack/Cast/Dash finish

    Direction facing = w.has(e, Component::Transform) ? w.transforms[e].facing : Direction::Down;
    bool moving = w.has(e, Component::Movement) && w.movements[e].velocity.length_sq() > 1.0f;
    play_anim(anim, facing_anim(moving ? AnimID::WalkDown : AnimID::IdleDown, facing));
}

static void system_animation(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        if (!w.has(static_cast<Entity>(e), Component::Animation) ||
            !w.has(static_cast<Entity>(e), Component::Sprite)) continue;

        select_animation(w, static_cast<Entity>(e));

        auto& anim = w.animations[e];
        auto& spr = w.sprites[e];

        if (anim.playing && anim.frame_count > 1) {
            anim.timer += dt;
            if (anim.timer >= anim.frame_time) {
                anim.timer -= anim.frame_time;
                anim.current_frame++;
                if (anim.current_frame >= anim.frame_count) {
                    anim.current_frame = anim.looping ? 0 : anim.frame_count - 1;
                    if (!anim.looping) anim.playing = false;
                }
            }
        }

        // Update sprite source rect based on current frame/row (always, so a freshly
        // selected row shows immediately)
        f32 frame_w = spr.src_rect.w;
        spr.src_rect.x = static_cast<f32>(anim.current_frame) * frame_w;
        spr.src_rect.y = static_cast<f32>(anim.row) * spr.src_rect.h;
    }
}

// =============================================================================
// System: Lifetime (auto-destroy + animacion de efecto: fundido, crecimiento, giro)
// =============================================================================
static void system_lifetime(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        Entity id = static_cast<Entity>(e);
        if (!w.has(id, Component::Lifetime)) continue;

        auto& life = w.lifetimes[e];
        life.remaining -= dt;
        if (life.remaining <= 0.0f) {
            w.destroy(id);
            continue;
        }

        if (life.grow != 0.0f && w.has(id, Component::Transform)) {
            // `grow` esta en unidades de ESCALA por segundo (0.25 abre un anillo, no 25).
            // El suelo a 0 no es cosmetico: una escala negativa da la vuelta al sprite y
            // lo hace crecer sin freno, y un VFX de 8 unidades acababa tapando la sala.
            auto& sc = w.transforms[e].scale;
            sc.x = std::max(0.0f, sc.x + life.grow * dt);
            sc.y = std::max(0.0f, sc.y + life.grow * dt);
        }
        if (life.spin != 0.0f && w.has(id, Component::Transform))
            w.transforms[e].rotation += life.spin * dt;
        if (life.fade && life.total > 0.0f && w.has(id, Component::Sprite)) {
            f32 k = std::max(0.0f, std::min(1.0f, life.remaining / life.total));
            w.sprites[e].tint.a = static_cast<u8>(255.0f * k);
        }
    }
}

// =============================================================================
// System: Projectiles
// =============================================================================
static void system_projectiles(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        if (!w.has(static_cast<Entity>(e), Component::Projectile) ||
            !w.has(static_cast<Entity>(e), Component::Transform)) continue;

        auto& proj = w.projectiles[e];
        auto& t = w.transforms[e];

        t.position += proj.direction * proj.speed * dt;
        t.rotation = std::atan2(proj.direction.y, proj.direction.x) * 57.2957795f;

        // Destroy if exceeded max range or out of room bounds
        f32 dist_sq = (t.position - proj.origin).length_sq();
        if (dist_sq > proj.max_range * proj.max_range ||
            t.position.x < -16 || t.position.x > Config::GAME_WIDTH + 16 ||
            t.position.y < -16 || t.position.y > Config::GAME_HEIGHT + 16) {
            w.destroy(static_cast<Entity>(e));
            continue;
        }

        // Solid tiles stop projectiles; player shots smash Baal altars
        if (w.active_room) {
            int tx = static_cast<int>(t.position.x) / Config::TILE_SIZE;
            int ty = static_cast<int>(t.position.y) / Config::TILE_SIZE;
            if (tx >= 0 && tx < Config::ROOM_TILES_X && ty >= 0 && ty < Config::ROOM_TILES_Y) {
                if (proj.friendly && w.active_room->tiles[ty][tx] == TileType::AltarBaal) {
                    w.active_room->tiles[ty][tx] = TileType::Path;
                    w.altars_smashed++;
                    w.destroy(static_cast<Entity>(e));
                    continue;
                }
            }
            if (w.active_room->is_tile_solid_for_projectiles(tx, ty)) {
                w.destroy(static_cast<Entity>(e));
                continue;
            }
        }
    }
}

// =============================================================================
// System: Pickups (bobbing + hit flash timers)
// =============================================================================
static void system_misc_timers(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        if (w.has(static_cast<Entity>(e), Component::Health)) {
            auto& h = w.healths[e];
            if (h.hit_flash > 0.0f) h.hit_flash -= dt;
        }
    }
}

// =============================================================================
// System: Combat timers
// =============================================================================
static void system_combat_timers(ECSWorld& w, f32 dt) {
    for (u32 e = 0; e < w.entity_count; e++) {
        if (!w.has(static_cast<Entity>(e), Component::Combat)) continue;
        auto& c = w.combats[e];
        if (c.attack_timer > 0.0f) c.attack_timer -= dt;
    }

    // I-frames
    for (u32 e = 0; e < w.entity_count; e++) {
        if (!w.has(static_cast<Entity>(e), Component::Health)) continue;
        auto& h = w.healths[e];
        if (h.iframes_timer > 0.0f) {
            h.iframes_timer -= dt;
            h.invulnerable = true;
        } else {
            h.invulnerable = false;
        }
    }
}

// =============================================================================
// Update all systems
// =============================================================================
void ECSWorld::update(f32 dt) {
    time += dt;
    if (enemy_slow_timer > 0.0f) enemy_slow_timer -= dt;
    system_movement(*this, dt);
    system_animation(*this, dt);
    system_combat_timers(*this, dt);
    system_misc_timers(*this, dt);
    system_projectiles(*this, dt);
    system_lifetime(*this, dt);
    // Enemy AI and collision are driven by the Engine (need faith / room context)
}

int ECSWorld::count_living_enemies() const {
    int n = 0;
    for (u32 e = 0; e < entity_count; e++) {
        if (!masks[e].any()) continue;
        if (!has(static_cast<Entity>(e), Component::EnemyAI)) continue;
        if (enemy_ais[e].ally) continue;   // los discipulos no cuentan: sellarian la sala
        if (has(static_cast<Entity>(e), Component::Health) && !healths[e].is_alive()) continue;
        n++;
    }
    return n;
}

// =============================================================================
// Collect draw commands (sorted later by the renderer, feet-y = depth)
// =============================================================================
void ECSWorld::collect_draws(std::vector<DrawCmd>& out, Renderer& renderer) {
    for (u32 e = 0; e < entity_count; e++) {
        if (!has(static_cast<Entity>(e), Component::Transform) ||
            !has(static_cast<Entity>(e), Component::Sprite)) continue;

        auto& t = transforms[e];
        auto& s = sprites[e];
        if (!s.visible) continue;

        bool is_player = (static_cast<Entity>(e) == player);
        bool is_enemy  = has(static_cast<Entity>(e), Component::EnemyAI);
        bool is_proj   = has(static_cast<Entity>(e), Component::Projectile);
        bool is_pickup = has(static_cast<Entity>(e), Component::Item) &&
                         items[e].type == ItemComponent::Type::Pickup;

        Color tint = s.tint;
        if (has(static_cast<Entity>(e), Component::Health)) {
            const auto& h = healths[e];
            // Destello del impacto: entra casi blanco y vira a rojo al apagarse. Un rojo
            // plano y largo se leia como "esta envenenado", no como "le acabo de dar".
            if (h.hit_flash > 0.0f) {
                f32 k = std::max(0.0f, std::min(1.0f, h.hit_flash / Config::Feel::HIT_FLASH_TIME));
                tint = {255,
                        static_cast<u8>(110.0f + 135.0f * k),
                        static_cast<u8>( 80.0f + 155.0f * k), tint.a};
            }
            // Aura del General Sirio: el aliado buffeado late en rojo para que se
            // entienda de un vistazo que hay que matar al General primero.
            else if (is_enemy && enemy_ais[e].buff_timer > 0.0f) {
                f32 p = 0.5f + 0.5f * std::sin(time * 9.0f + static_cast<f32>(e));
                tint = {255, static_cast<u8>(150 - 40 * p), static_cast<u8>(120 - 40 * p), tint.a};
            }
            // Player i-frames: blink
            if (is_player && h.iframes_timer > 0.0f && !movements[e].is_dashing) {
                int phase = static_cast<int>(h.iframes_timer * 20.0f);
                if (phase % 2 == 0) tint.a = 110;
            }
            // El dash es invulnerable: que se VEA. El parpadeo de i-frames se apaga
            // durante el dash justo por esto, y hasta ahora no lo sustituia nada.
            if (is_player && has(static_cast<Entity>(e), Component::Movement) && movements[e].is_dashing)
                tint = {235, 245, 255, 235};
            // Cadaver: se apaga en el ultimo tramo de su animacion de muerte en vez de
            // esfumarse de golpe cuando expira el Lifetime.
            if (is_enemy && !h.is_alive() && has(static_cast<Entity>(e), Component::Lifetime)) {
                const auto& lf = lifetimes[e];
                f32 fade_at = lf.total * Config::Feel::DEATH_FADE_FRAC;
                if (lf.total > 0.0f && lf.remaining < fade_at)
                    tint.a = static_cast<u8>(255.0f * std::max(0.0f, lf.remaining / fade_at));
            }
        }

        f32 bob = 0.0f;
        if (is_pickup) bob = std::sin(time * 5.0f + items[e].bob_phase) * 1.5f;

        bool has_texture = (s.texture_id < renderer.textures.size() &&
                            renderer.textures[s.texture_id].texture != nullptr);

        DrawCmd cmd;
        // Depth: characters sort by their feet (12px below centre at scale 1); flat things by centre
        f32 feet = (is_player || is_enemy) ? 12.0f * t.scale.y : 0.0f;
        cmd.sort_y = t.position.y + feet;
        if (is_proj) cmd.sort_y += 8.0f;   // Projectiles fly above the ground line
        cmd.flip_x = s.flip_x;
        cmd.tint = tint;

        if (has_texture) {
            // src_rect is in texels (ASSET_SCALE per game unit); dst in game units
            const f32 inv = 1.0f / static_cast<f32>(Config::ASSET_SCALE);
            f32 w = s.src_rect.w * inv * t.scale.x;
            f32 h = s.src_rect.h * inv * t.scale.y;
            cmd.texture_id = s.texture_id;
            cmd.src = s.src_rect;
            cmd.dst = {t.position.x - w * 0.5f, t.position.y - h * 0.5f + bob, w, h};
            if (is_proj) {
                // Rotated draw needs the immediate path; keep depth by drawing after props via sort_y
                cmd.rotation = t.rotation;
            }
        } else {
            f32 base = (is_player || is_enemy) ? 12.0f : s.src_rect.w;
            f32 w = base * t.scale.x;
            f32 h = base * t.scale.y;
            cmd.texture_id = TEXTURE_NONE;
            cmd.dst = {t.position.x - w * 0.5f, t.position.y - h * 0.5f + bob, w, h};
            cmd.tint = is_player ? Color::elias_gold() : tint;
            cmd.outline = base >= 6.0f;   // tiny particles: no outline
        }
        out.push_back(cmd);

        // Soft shadow blob under characters (drawn first: very low sort key)
        if ((is_player || is_enemy) && has_texture) {
            DrawCmd sh;
            sh.sort_y = -1000.0f;
            f32 sw = 16.0f * t.scale.x, shh = 6.0f * t.scale.y;
            sh.dst = {t.position.x - sw * 0.5f, t.position.y + 12.0f * t.scale.y - shh * 0.5f - 0.5f, sw, shh};
            if (renderer.shadow_tex != TEXTURE_NONE) {
                sh.texture_id = renderer.shadow_tex;
                sh.src = {0, 0, 64, 32};
                sh.tint = {0, 0, 0, 110};
            } else {
                sh.texture_id = TEXTURE_NONE;
                sh.tint = {0, 0, 0, 70};
            }
            out.push_back(sh);
        }
    }
}
