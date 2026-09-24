// =============================================================================
// MANTLE - Enemy Implementation
// =============================================================================

#include "core/i18n.h"
#include "game/enemies.h"
#include "game/combat.h"
#include "game/faith.h"
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
    // Las hojas vienen a todo color, asi que el tinte solo pinta la PALETA DEL CAPITULO
    // (get_enemy_chapter_tint). El destello del golpe y el aura del General lo pisan en
    // ECSWorld::collect_draws, que arranca justo de este valor.
    world.sprites[e].tint = (texture_id == TEXTURE_NONE) ? def.color
                                                         : get_enemy_chapter_tint(chapter);

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
    world.enemy_ais[e].chapter = static_cast<u8>(std::max(1, chapter));
    world.enemy_ais[e].shoot_timer = randf_range(0.6f, 1.6f);
    world.enemy_ais[e].think_timer = randf_range(0.0f, 0.3f);
    world.enemy_ais[e].think_interval = 0.2f;

    return e;
}

Entity spawn_enemy(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id, int chapter) {
    return spawn_common(world, type, position, texture_id, chapter);
}

Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id,
                  int chapter, const char* boss_name, BossPattern pattern) {
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
    world.enemy_ais[e].boss_pattern = static_cast<u8>(pattern);
    world.enemy_ais[e].phase = 0;
    world.enemy_ais[e].special_timer = 3.0f;
    world.enemy_ais[e].partner = ENTITY_NONE;

    // La multitud del Carmelo y el asedio de Ben-adad pelean con numero, no con vida
    if (pattern == BossPattern::ProfetasDeBaal || pattern == BossPattern::BenAdad) {
        world.healths[e].max *= 0.75f;
        world.healths[e].current = world.healths[e].max;
    }
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

// =============================================================================
// Jefes: un patron por capitulo
// =============================================================================
// Utilidades compartidas. Todo lo que hacen sale del estado del mundo: ni el
// jugador ni el Engine entran aqui.
static int count_alive_enemies(const ECSWorld& world) {
    int n = 0;
    for (u32 i = 0; i < world.entity_count; i++) {
        if (!world.is_alive(i) || !is_hostile(world, i)) continue;
        if (world.has(i, Component::Health) && !world.healths[i].is_alive()) continue;
        if (world.enemy_ais[i].is_boss) continue;
        n++;
    }
    return n;
}

// Punto pisable cerca de `around` para invocaciones y teleports. Si ninguna de las ocho
// direcciones sirve devuelve `fallback` (nunca `around`: teleportar encima del jugador
// o dentro de un muro es peor que no teleportar).
static Vec2 free_spot_near(const ECSWorld& world, Vec2 around, f32 radius, int seed, Vec2 fallback) {
    for (int i = 0; i < 8; i++) {
        f32 a = (seed + i) * 0.7854f;
        Vec2 p = around + Vec2{std::cos(a), std::sin(a)} * radius;
        if (!world.active_room || world.active_room->is_walkable_world(p + Vec2{0, 8})) return p;
    }
    return fallback;
}

static void volley(ECSWorld& world, Entity e, Vec2 to_player, int count, f32 spread,
                   ProjectileKind kind, f32 dmg, f32 speed, f32 range) {
    f32 base = std::atan2(to_player.y, to_player.x);
    for (int i = 0; i < count; i++) {
        f32 a = base + (i - (count - 1) * 0.5f) * spread;
        shoot_at(world, e, {std::cos(a), std::sin(a)}, kind, dmg, speed, range);
    }
}

static void ring(ECSWorld& world, Entity e, Vec2 to_player, int count,
                 ProjectileKind kind, f32 dmg, f32 speed, f32 range) {
    f32 base = std::atan2(to_player.y, to_player.x);
    f32 step = 6.28318f / static_cast<f32>(count);
    for (int i = 0; i < count; i++) {
        f32 a = base + i * step;
        shoot_at(world, e, {std::cos(a), std::sin(a)}, kind, dmg, speed, range);
    }
}

static void chase(ECSWorld& world, Entity e, Vec2 to_player, f32 dist, f32 dt, f32 stop_at = 26.0f) {
    auto& ai = world.enemy_ais[e];
    auto& movement = world.movements[e];
    if (ai.think_timer <= 0.0f) {
        ai.think_timer = 0.25f;
        movement.velocity = (dist > stop_at) ? to_player * movement.speed : Vec2{0, 0};
    } else {
        ai.think_timer -= dt;
    }
}

// Invoca un secuaz si no hay demasiados en la sala. Devuelve la entidad creada.
static Entity summon(ECSWorld& world, Entity boss, EnemyType type, int max_alive, int chapter, int seed) {
    if (count_alive_enemies(world) >= max_alive) return ENTITY_NONE;
    Vec2 boss_pos = world.transforms[boss].position;
    Vec2 at = free_spot_near(world, boss_pos, 26.0f, seed, boss_pos);
    Entity s = spawn_enemy(world, type, at, world.enemy_textures[static_cast<u32>(type)], chapter);
    if (s != ENTITY_NONE) {
        world.enemy_ais[s].home_cell = world.enemy_ais[boss].home_cell;   // hereda la celda del invocador
        spawn_particles(world, at, {200, 120, 255, 255}, 8, 70.0f, 0.3f);
    }
    return s;
}

// =============================================================================
// El corro del Carmelo (1 R 18:26-29)
// =============================================================================
// Un sumo sacerdote (la cabeza, `is_boss`) y sus profetas (`boss_group`) "saltando
// alrededor del altar" (18:26). El capitulo termina cuando cae el ultimo: la sala se
// limpia por `count_living_enemies() == 0`, asi que un grupo encaja por construccion.
// "Los 450" se transmiten como REPOSICION — la cabeza rellena el corro desde el pueblo,
// uno cada pocos segundos —, no como numero en pantalla.
static constexpr int BAAL_RING_SLOTS  = 6;
static constexpr f32 BAAL_RING_RADIUS = 44.0f;
static constexpr f32 BAAL_RING_LEASH  = 120.0f;   // mas lejos del corro, lo persiguen

static f32 ring_slot_angle(int slot) { return slot * (6.28318f / BAAL_RING_SLOTS); }

int count_boss_group(const ECSWorld& world) {
    int n = 0;
    for (u32 i = 0; i < world.entity_count; i++) {
        if (!world.is_alive(i) || !world.has(i, Component::EnemyAI)) continue;
        if (!world.enemy_ais[i].boss_group) continue;
        if (world.has(i, Component::Health) && !world.healths[i].is_alive()) continue;
        n++;
    }
    return n;
}

// Primer hueco libre del corro (por fase): la reposicion ocupa el sitio del caido
static int free_ring_slot(const ECSWorld& world) {
    for (int s = 0; s < BAAL_RING_SLOTS; s++) {
        const f32 want = ring_slot_angle(s);
        bool taken = false;
        for (u32 i = 0; i < world.entity_count && !taken; i++) {
            if (!world.is_alive(i) || !world.has(i, Component::EnemyAI) || !world.enemy_ais[i].boss_group) continue;
            if (world.has(i, Component::Health) && !world.healths[i].is_alive()) continue;
            taken = std::fabs(world.enemy_ais[i].ring_phase - want) < 0.01f;
        }
        if (!taken) return s;
    }
    return -1;
}

static Entity spawn_baal_prophet(ECSWorld& world, Vec2 center, int slot, int chapter) {
    const f32 a = ring_slot_angle(slot);
    Vec2 want = center + Vec2{std::cos(a), std::sin(a)} * BAAL_RING_RADIUS;
    Vec2 at = want;
    if (world.active_room && !world.active_room->is_walkable_world(want + Vec2{0, 8}))
        at = free_spot_near(world, want, 18.0f, slot, center);
    Entity m = spawn_enemy(world, EnemyType::ProfetaDeBaal, at,
                           world.enemy_textures[static_cast<u32>(EnemyType::ProfetaDeBaal)], chapter);
    if (m == ENTITY_NONE) return m;
    auto& ai = world.enemy_ais[m];
    ai.boss_group   = true;
    ai.boss_pattern = static_cast<u8>(BossPattern::ProfetasDeBaal);
    ai.name         = tr("Profeta de Baal", "Prophet of Baal");
    ai.ring_center  = center;
    ai.ring_phase   = a;
    ai.phase        = 0;
    ai.shoot_timer  = 1.2f + 0.4f * slot;   // escalonados: el corro no dispara a la vez
    world.transforms[m].scale = {1.15f, 1.15f};
    world.healths[m].max *= 1.6f;
    world.healths[m].current = world.healths[m].max;
    SDL_Log("Baal prophet slot %d at (%.0f,%.0f)%s", slot, at.x, at.y,
            (at.x != want.x || at.y != want.y) ? " (recolocado)" : "");
    return m;
}

Entity spawn_boss_group(ECSWorld& world, EnemyType type, Vec2 position, Vec2 ring_center,
                        u32 texture_id, int chapter, const char* boss_name, BossPattern pattern) {
    Entity head = spawn_boss(world, type, position, texture_id, chapter, boss_name, pattern);
    if (head == ENTITY_NONE || pattern != BossPattern::ProfetasDeBaal) return head;
    // El nombre del GRUPO ("Los 450 Profetas de Baal") va en la barra; el de la cabeza, aqui
    world.enemy_ais[head].name = tr("Sumo sacerdote de Baal", "High priest of Baal");
    world.enemy_ais[head].ring_center = ring_center;
    // Cinco de pie al abrir la pelea; el sexto hueco lo rellena la cabeza
    for (int s = 0; s < BAAL_RING_SLOTS - 1; s++) spawn_baal_prophet(world, ring_center, s, chapter);
    return head;
}

// Miembro del corro: baila alrededor del altar a velocidad angular constante con un
// vaiven de radio, tira un Darkbolt cuando el profeta esta a tiro, y al flaquear entra
// en el mismo frenesi que la cabeza (18:28: se hieren solos). Si el profeta se aleja del
// corro lo persigue — sin esto se le tirotea desde fuera sin que nadie se mueva.
static void update_baal_prophet(ECSWorld& world, Entity e, Vec2 player_pos, Vec2 to_player,
                                f32 dist, f32 dmg, f32 dt) {
    auto& ai = world.enemy_ais[e];
    auto& movement = world.movements[e];
    auto& health = world.healths[e];
    const bool frenzy = health.ratio() < 0.5f;
    if (frenzy && ai.phase == 0) {
        ai.phase = 1;
        movement.speed *= 1.35f;
        spawn_particles(world, world.transforms[e].position, {200, 40, 40, 255}, 12, 80.0f, 0.5f);
    }
    if (frenzy) health.current = std::max(1.0f, health.current - health.max * 0.012f * dt);

    if (player_pos.distance_to(ai.ring_center) > BAAL_RING_LEASH) {
        chase(world, e, to_player, dist, dt, 40.0f);
    } else {
        const f32 omega = frenzy ? 1.4f : 0.9f;
        const f32 a = ai.ring_phase + world.time * omega;
        const f32 r = BAAL_RING_RADIUS + 8.0f * std::sin(world.time * 3.0f + ai.ring_phase);
        Vec2 target = ai.ring_center + Vec2{std::cos(a), std::sin(a)} * r;
        Vec2 d = target - world.transforms[e].position;
        f32 l = d.length();
        movement.velocity = (l > 1.0f) ? d * (std::min(movement.speed, l * 4.0f) / l) : Vec2{0, 0};
    }
    if (dist < ai.attack_range * 1.5f && ai.shoot_timer <= 0.0f &&
        has_line_of_sight(world, world.transforms[e].position, player_pos)) {
        ai.shoot_timer = frenzy ? 1.8f : 2.6f;
        shoot_at(world, e, to_player, ProjectileKind::Darkbolt, dmg, 120.0f, 260.0f);
    }
}

// =============================================================================
// General Sirio: no pelea, arenga
// =============================================================================
// Reparte fuerza en vez de gastarla. Los valores base se guardan en el aliado la primera
// vez, asi que refrescar el aura NO acumula multiplicadores (un General con dos arengas
// seguidas dejaba soldados imparables). El aliado late en rojo mientras dura, para que el
// jugador entienda de un vistazo a quien hay que matar primero.
static int buff_allies(ECSWorld& world, Entity general) {
    constexpr f32 RADIUS   = 70.0f;
    constexpr f32 DURATION = 5.0f;
    Vec2 at = world.transforms[general].position;
    int touched = 0;

    for (u32 a = 0; a < world.entity_count; a++) {
        if (a == general || !world.is_alive(a)) continue;
        if (!is_hostile(world, a) || !world.has(a, Component::Transform)) continue;
        if (!world.has(a, Component::Health) || !world.healths[a].is_alive()) continue;
        if (world.enemy_ais[a].is_boss) continue;   // los jefes van a su propio balance
        if (world.transforms[a].position.distance_to(at) > RADIUS) continue;

        auto& ally = world.enemy_ais[a];
        if (ally.buff_timer <= 0.0f) {
            ally.base_damage = world.has(a, Component::Combat)   ? world.combats[a].damage  : 0.0f;
            ally.base_speed  = world.has(a, Component::Movement) ? world.movements[a].speed : 0.0f;
            if (ally.base_damage > 0.0f) world.combats[a].damage  = ally.base_damage * 1.30f;
            if (ally.base_speed  > 0.0f) world.movements[a].speed = ally.base_speed  * 1.15f;
        }
        ally.buff_timer = DURATION;
        touched++;
    }

    if (touched > 0) {
        spawn_vfx(world, at, TileSprite::Star, {255, 90, 70, 210}, 22.0f, 0.55f, 2.0f, 140.0f);
        spawn_particles(world, at, {255, 120, 90, 255}, 6 + touched * 2, 70.0f, 0.4f);
    }
    return touched;
}

static void update_boss_ai(ECSWorld& world, Entity e, Vec2 player_pos, Vec2 to_player,
                           f32 dist, f32 dmg, f32 dt) {
    auto& ai = world.enemy_ais[e];
    auto& movement = world.movements[e];
    auto& health = world.healths[e];
    const f32 ratio = health.ratio();
    const int chapter = 1 + static_cast<int>(ai.boss_pattern);   // el patron va 1:1 con el capitulo
    ai.special_timer -= dt;

    switch (static_cast<BossPattern>(ai.boss_pattern)) {

    // --- Cap 1: el capitan pelea a la carga y llama refuerzos cuando flaquea ---
    case BossPattern::CapitanDeAcab:
        chase(world, e, to_player, dist, dt);
        if (ai.leap_timer <= 0.0f) {
            ai.leap_timer = 3.0f;
            movement.knockback = to_player * 300.0f;
            if (world.has(e, Component::Animation)) play_anim(world.animations[e], AnimID::Dash, true);
        } else if (ai.leap_timer < 0.5f) {
            movement.velocity = {0, 0};   // telegrafia
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 2.6f;
            volley(world, e, to_player, 3, 0.24f, ProjectileKind::Arrow, dmg * 0.55f, 170.0f, 240.0f);
        }
        if (ratio < 0.5f && ai.phase == 0) {
            ai.phase = 1;
            for (int i = 0; i < 3; i++) summon(world, e, EnemyType::SoldadoDeAcab, 6, chapter, i * 3);
        }
        break;

    // --- Cap 2: el sacerdote no pelea, oficia: invoca y se cura ---
    case BossPattern::SacerdoteBaal:
        chase(world, e, to_player, dist, dt, 70.0f);
        if (dist < 60.0f) movement.velocity = to_player * -movement.speed * 0.7f;   // guarda distancia
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 2.2f;
            volley(world, e, to_player, 5, 0.20f, ProjectileKind::Darkbolt, dmg * 0.5f, 150.0f, 280.0f);
        }
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 7.0f;
            summon(world, e, EnemyType::ProfetaDeBaal, 4, chapter, 2);
            health.heal(health.max * 0.06f);
            spawn_particles(world, world.transforms[e].position, {180, 90, 220, 255}, 10, 60.0f, 0.4f);
        }
        break;

    // --- Cap 3: la multitud. 2a fase: "se sajaban con cuchillos" (1 Re 18:28) ---
    case BossPattern::ProfetasDeBaal: {
        chase(world, e, to_player, dist, dt, 60.0f);
        bool frenzy = (ratio < 0.5f);
        if (frenzy && ai.phase == 0) {
            ai.phase = 1;
            movement.speed *= 1.35f;
            spawn_particles(world, world.transforms[e].position, {200, 40, 40, 255}, 16, 90.0f, 0.5f);
        }
        if (frenzy) health.current = std::max(1.0f, health.current - health.max * 0.012f * dt);  // se hieren solos
        // Repone el corro desde el pueblo (los 450): UNO cada 5 s, al hueco del caido.
        // Nacen ya como miembros (`boss_group`) para que cuenten en la barra.
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = frenzy ? 3.0f : 5.0f;
            const int slot = free_ring_slot(world);
            if (slot >= 0 && count_boss_group(world) < BAAL_RING_SLOTS) {
                Entity m = spawn_baal_prophet(world, ai.ring_center, slot, chapter);
                if (m != ENTITY_NONE) {
                    world.enemy_ais[m].home_cell = ai.home_cell;   // cuenta en la celda del jefe
                    spawn_particles(world, world.transforms[m].position, {200, 120, 255, 255}, 8, 70.0f, 0.3f);
                }
            }
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = frenzy ? 1.6f : 3.0f;
            ring(world, e, to_player, frenzy ? 10 : 6, ProjectileKind::Darkbolt, dmg * 0.45f, 120.0f, 260.0f);
        }
        break;
    }

    // --- Cap 4: viento, terremoto, fuego... y el silbo apacible (1 Re 19:11-12) ---
    case BossPattern::Desesperacion: {
        u8 want = (ratio > 0.75f) ? 0 : (ratio > 0.50f) ? 1 : (ratio > 0.25f) ? 2 : 3;
        if (want != ai.phase) {
            ai.phase = want;
            spawn_particles(world, world.transforms[e].position, {230, 230, 255, 255}, 18, 110.0f, 0.5f);
        }
        if (ai.phase == 3) {
            // El silbo: deja de atacar. Ya no es un enemigo, es un silencio que hay que cruzar.
            movement.velocity = movement.velocity * 0.90f;
            health.invulnerable = false;
            break;
        }
        chase(world, e, to_player, dist, dt);
        if (ai.phase == 0 && ai.shoot_timer <= 0.0f) {          // viento: anillos que empujan
            ai.shoot_timer = 2.4f;
            ring(world, e, to_player, 12, ProjectileKind::Darkbolt, dmg * 0.4f, 110.0f, 320.0f);
        }
        if (ai.phase == 1) {                                     // terremoto: cargas seguidas
            if (ai.leap_timer <= 0.0f) {
                ai.leap_timer = 2.2f;
                movement.knockback = to_player * 340.0f;
                if (world.has(e, Component::Animation)) play_anim(world.animations[e], AnimID::Dash, true);
            }
        }
        if (ai.phase == 2 && ai.shoot_timer <= 0.0f) {           // fuego: conos rapidos
            ai.shoot_timer = 1.1f;
            volley(world, e, to_player, 5, 0.18f, ProjectileKind::Fireball, dmg * 0.5f, 190.0f, 280.0f);
        }
        break;
    }

    // --- Cap 5: el duo. Jezabel entra con Acab y el que queda se enfurece ---
    case BossPattern::AcabYJezabel: {
        if (ai.phase == 0) {
            ai.phase = 1;
            ai.partner = summon(world, e, EnemyType::ProfetaDeBaal, 99, chapter + 2, 4);
            if (ai.partner != ENTITY_NONE && world.has(ai.partner, Component::Health)) {
                world.healths[ai.partner].max *= 3.0f;
                world.healths[ai.partner].current = world.healths[ai.partner].max;
                world.enemy_ais[ai.partner].name = "Jezabel";
                world.transforms[ai.partner].scale = {1.3f, 1.3f};
            }
        }
        bool alone = (ai.partner == ENTITY_NONE) || !world.is_alive(ai.partner) ||
                     (world.has(ai.partner, Component::Health) && !world.healths[ai.partner].is_alive());
        if (alone && ai.phase == 1) {
            ai.phase = 2;
            movement.speed *= 1.3f;
            spawn_particles(world, world.transforms[e].position, {255, 80, 60, 255}, 16, 100.0f, 0.5f);
        }
        chase(world, e, to_player, dist, dt, 60.0f);
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = (ai.phase == 2) ? 1.2f : 2.2f;
            volley(world, e, to_player, (ai.phase == 2) ? 5 : 3, 0.22f,
                   ProjectileKind::Arrow, dmg * 0.55f, 185.0f, 300.0f);
        }
        break;
    }

    // --- Cap 6: el agua corrupta se parte en dos cada vez que cede ---
    case BossPattern::EspirituCorrupto: {
        chase(world, e, to_player, dist, dt);
        u8 want = (ratio > 0.66f) ? 0 : (ratio > 0.33f) ? 1 : 2;
        if (want > ai.phase) {
            ai.phase = want;
            summon(world, e, EnemyType::AguaCorrupta, 10, chapter + 1, 3);
            summon(world, e, EnemyType::AguaCorrupta, 10, chapter + 1, 6);
            world.transforms[e].scale = {1.6f - 0.15f * ai.phase, 1.6f - 0.15f * ai.phase};
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 1.8f;
            ring(world, e, to_player, 7, ProjectileKind::Darkbolt, dmg * 0.5f, 135.0f, 250.0f);
        }
        break;
    }

    // --- Cap 7: el acreedor cobra. Cada ronda le devuelve vida ---
    case BossPattern::Acreedor:
        chase(world, e, to_player, dist, dt);
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 1.9f;
            volley(world, e, to_player, 4, 0.30f, ProjectileKind::Darkbolt, dmg * 0.55f, 165.0f, 270.0f);
        }
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 6.0f;
            health.heal(health.max * 0.05f);
            summon(world, e, EnemyType::SoldadoSirio, 5, chapter, 2);
            spawn_particles(world, world.transforms[e].position, {230, 200, 90, 255}, 10, 70.0f, 0.35f);
        }
        break;

    // --- Cap 8: la Muerte no cede a distancia. Hay que tenderse sobre el niño (2 Re 4:34) ---
    case BossPattern::LaMuerte: {
        constexpr f32 CLOSE = 34.0f;
        bool reachable = (dist < CLOSE);
        health.invulnerable = !reachable;
        // Huye del cuerpo a cuerpo: acercarse es toda la pelea
        if (reachable) movement.velocity = to_player * -movement.speed;
        else           chase(world, e, to_player, dist, dt, 60.0f);
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = reachable ? 2.6f : 1.5f;
            ring(world, e, to_player, 9, ProjectileKind::Darkbolt, dmg * 0.5f, 125.0f, 300.0f);
        }
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 8.0f;
            summon(world, e, EnemyType::IdolatraDeRimon, 5, chapter, 4);
        }
        break;
    }

    // --- Cap 9: Guiezi corrió tras la plata; ahora corre de ti (2 Re 5:20-27) ---
    case BossPattern::Guiezi:
        chase(world, e, to_player, dist, dt, 90.0f);
        if (dist < 80.0f) movement.velocity = to_player * -movement.speed;
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 4.0f;
            Vec2 from = world.transforms[e].position;
            spawn_particles(world, from, {150, 220, 150, 255}, 12, 90.0f, 0.35f);
            world.transforms[e].position = free_spot_near(world, player_pos, 62.0f,
                                                          static_cast<int>(world.time * 7.0f), from);
            spawn_particles(world, world.transforms[e].position, {150, 220, 150, 255}, 12, 90.0f, 0.35f);
            Vec2 aim = (player_pos - world.transforms[e].position).normalized();
            volley(world, e, aim, 3, 0.26f, ProjectileKind::Arrow, dmg * 0.6f, 200.0f, 260.0f);
            ai.shoot_timer = 1.2f;
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 2.0f;
            volley(world, e, to_player, 2, 0.32f, ProjectileKind::Arrow, dmg * 0.5f, 190.0f, 260.0f);
        }
        break;

    // --- Cap 10: el asedio. Siria no se acaba: hay que derribar a Ben-adad ---
    case BossPattern::BenAdad:
        chase(world, e, to_player, dist, dt, 80.0f);
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 4.5f;
            summon(world, e, EnemyType::SoldadoSirio, 9, chapter, 1);
            summon(world, e, EnemyType::SoldadoSirio, 9, chapter, 5);
            if (ratio < 0.5f) summon(world, e, EnemyType::GeneralSirio, 9, chapter, 3);
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 2.0f;
            volley(world, e, to_player, 7, 0.16f, ProjectileKind::Arrow, dmg * 0.45f, 175.0f, 320.0f);
        }
        if (ai.leap_timer <= 0.0f) {
            ai.leap_timer = 6.0f;
            ring(world, e, to_player, 12, ProjectileKind::Darkbolt, dmg * 0.4f, 120.0f, 300.0f);
        }
        break;
    }
}

// =============================================================================
// Discipulos (ver enemies.h)
// =============================================================================
Entity spawn_disciple(ECSWorld& world, Vec2 position, u32 texture_id, f32 life_seconds,
                      f32 hp, f32 damage) {
    // Se monta sobre el chasis del soldado —hoja, animaciones y collider ya cuadran—
    // y se le sobrescribe todo lo que importa. Igual que hace spawn_boss.
    Entity e = spawn_common(world, EnemyType::SoldadoDeAcab, position, texture_id, 1);
    if (e == ENTITY_NONE) return e;

    world.healths[e].max = hp;
    world.healths[e].current = hp;
    world.combats[e].damage = damage;
    world.movements[e].speed = 165.0f;
    world.sprites[e].tint = {200, 225, 255, 255};   // frio y luminoso: no se confunde con un sirio

    auto& ai = world.enemy_ais[e];
    ai.ally           = true;
    ai.name           = tr("Discípulo", "Disciple");
    ai.aggro_range    = 190.0f;
    ai.attack_range   = 14.0f;
    ai.think_interval = 0.12f;   // reacciona antes que un enemigo: es el que persigue
    ai.think_timer    = randf_range(0.0f, 0.12f);
    ai.life_left      = life_seconds;
    return e;
}

Entity spawn_figure(ECSWorld& world, Vec2 position, u32 texture_id, Direction facing) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;
    world.transforms[e].facing   = facing;

    const AnimID anim = facing_anim(AnimID::IdleDown, facing);
    world.add(e, Component::Animation);
    play_anim(world.animations[e], anim, true);

    // Fila horneada: sin system_animation, el src_rect que se escribe aqui es el que se pinta.
    world.add(e, Component::Sprite);
    const f32 fs = static_cast<f32>(SHEET_FRAME_SIZE);
    world.sprites[e].texture_id = texture_id;
    world.sprites[e].src_rect   = {0.0f, static_cast<f32>(get_anim_def(anim).row) * fs, fs, fs};
    world.sprites[e].z_order    = 5;
    return e;
}

Entity spawn_npc_figure(ECSWorld& world, Vec2 position, u32 npc_texture, int cell) {
    Entity e = world.create();
    if (e == ENTITY_NONE) return e;

    world.add(e, Component::Transform);
    world.transforms[e].position = position;
    world.transforms[e].facing   = Direction::Down;

    // Animation solo para que collect_draws la trate como figura (sombra y orden por pies).
    world.add(e, Component::Animation);
    play_anim(world.animations[e], AnimID::IdleDown, true);

    // npcs.png es UNA fila de 16 personajes distintos, no una hoja de animacion: si
    // system_animation avanzara la celda (y corre, durante el paseo cinematico de las
    // aperturas), el PNJ cambiaria de persona cada 0,15 s. fixed_src la clava.
    world.add(e, Component::Sprite);
    world.sprites[e].texture_id = npc_texture;
    world.sprites[e].src_rect   = Npc::src(cell);
    world.sprites[e].fixed_src  = true;
    world.sprites[e].z_order    = 5;
    return e;
}

// Persigue al hostil que elegiria el auto-apuntado; sin ninguno, escolta al profeta.
// El golpe no se da aqui: lo resuelve la colision (game/combat.cpp).
static void update_ally_ai(ECSWorld& world, Entity e, Vec2 player_pos, f32 dt) {
    auto& ai = world.enemy_ais[e];
    auto& transform = world.transforms[e];

    bool dead = world.has(e, Component::Health) && !world.healths[e].is_alive();
    if (ai.life_left > 0.0f) ai.life_left -= dt;
    // Se va cuando se le agota la invocacion o cuando lo derriban: en los dos casos
    // desaparece del todo (no deja cadaver ni cuenta para nada).
    if (dead || (ai.life_left < 0.0f)) {
        spawn_particles(world, transform.position, {190, 220, 255, 255}, 8, 70.0f, 0.4f);
        world.destroy(e);
        return;
    }
    if (!world.has(e, Component::Movement)) return;
    auto& movement = world.movements[e];

    ai.think_timer -= dt;
    if (ai.think_timer > 0.0f) return;
    ai.think_timer = ai.think_interval;

    Entity target = pick_target(world, transform.position, ai.aggro_range);
    Vec2 goal = (target != ENTITY_NONE) ? world.transforms[target].position : player_pos;
    f32 dist = transform.position.distance_to(goal);
    // Sin enemigo a la vista se queda a media distancia del profeta en vez de pegarsele
    f32 stop = (target != ENTITY_NONE) ? 9.0f : 24.0f;
    if (dist > stop) {
        Vec2 dir = (goal - transform.position).normalized();
        movement.velocity = dir * movement.speed;
    } else {
        movement.velocity = movement.velocity * 0.25f;
    }
}

void update_enemy_ai(ECSWorld& world, f32 dt) {
    Entity player = world.player;
    if (player == ENTITY_NONE) return;

    Vec2 player_pos = world.transforms[player].position;

    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e)) continue;
        if (!world.has(e, Component::EnemyAI) || !world.has(e, Component::Transform)) continue;
        // Los discipulos van antes de los filtros de cadaver: son ellos quienes tienen que
        // retirarse del tablero al caer o al agotarse la invocacion.
        if (world.enemy_ais[e].ally) { update_ally_ai(world, static_cast<Entity>(e), player_pos, dt); continue; }
        if (!world.has(e, Component::Movement)) continue;   // corpses
        if (world.has(e, Component::Health) && !world.healths[e].is_alive()) continue;

        auto& ai = world.enemy_ais[e];
        auto& transform = world.transforms[e];
        auto& movement = world.movements[e];

        // Timers tick every frame
        ai.shoot_timer -= dt;
        ai.leap_timer  -= dt;

        // El aura del General Sirio caduca: se devuelven los valores guardados
        if (ai.buff_timer > 0.0f) {
            ai.buff_timer -= dt;
            if (ai.buff_timer <= 0.0f) {
                if (ai.base_damage > 0.0f && world.has(e, Component::Combat))
                    world.combats[e].damage = ai.base_damage;
                if (ai.base_speed > 0.0f && world.has(e, Component::Movement))
                    world.movements[e].speed = ai.base_speed;
                ai.base_damage = 0.0f;
                ai.base_speed = 0.0f;
            }
        }

        f32 dist = transform.position.distance_to(player_pos);
        Vec2 to_player = (player_pos - transform.position).normalized();
        f32 dmg = world.has(e, Component::Combat) ? world.combats[e].damage : 8.0f;

        // ---- Boss: un patron propio por capitulo (game/enemies.h BossPattern) ----
        if (ai.is_boss) {
            update_boss_ai(world, e, player_pos, to_player, dist, dmg, dt);
            continue;
        }
        // ---- Corro del jefe (cap. 3): miembros con barra compartida ----
        if (ai.boss_group) {
            update_baal_prophet(world, e, player_pos, to_player, dist, dmg, dt);
            continue;
        }

        // ---- Regular enemies think at intervals ----
        ai.think_timer -= dt;
        ai.special_timer -= dt;   // fuera de los jefes solo lo usa la arenga del General
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
                // Arengar es su ataque de verdad; dispara menos que antes para que
                // sumar aura y rafaga no dispare la dificultad del cap. 9-10 de golpe.
                if (dist < ai.aggro_range && ai.special_timer <= 0.0f) {
                    // Sin nadie a quien arengar no gasta la arenga: lo vuelve a intentar pronto
                    ai.special_timer = buff_allies(world, e) > 0 ? 4.0f : 1.0f;
                }
                if (dist < ai.aggro_range && ai.shoot_timer <= 0.0f) {
                    ai.shoot_timer = 3.6f;
                    volley(world, e, to_player, 2, 0.30f, ProjectileKind::Arrow, dmg * 0.55f, 180.0f, 220.0f);
                }
                break;
        }
    }
}

// =============================================================================
// Muerte con conducta: lo que hace un enemigo del Acto II al caer
// =============================================================================
static constexpr f32 BRASERO_FUSE    = 0.35f;   // telegrafia: sin ella el estallido se lee como daño injusto
static constexpr f32 BRASERO_RADIUS  = 28.0f;
static constexpr int SPLIT_MAX_ALIVE = 18;      // MAX_ENTITIES es un techo compartido con proyectiles y VFX
static constexpr u8  SPLIT_MAX_GEN   = 2;       // 1 padre -> 2 -> 4 y se acaba

void on_enemy_death_behavior(ECSWorld& world, Entity enemy, Vec2 pos) {
    if (!world.is_alive(enemy) || !is_hostile(world, enemy)) return;
    auto& ai = world.enemy_ais[enemy];
    if (ai.is_boss) return;   // los jefes ya mueren con su propio patron

    switch (ai.behavior) {

    // --- Idolatra de Rimon: al caer suelta el brasero y las brasas prenden. La mecha es
    //     el tiempo que tardan en cuajar; el estallido lo resuelve update_death_fuses ---
    case AIBehavior::ExplodeOnDeath: {
        ai.death_fuse = BRASERO_FUSE;
        // El cadaver debe durar mas que la mecha o no quedaria nada que estallar
        f32 keep = BRASERO_FUSE + 0.15f;
        if (!world.has(enemy, Component::Lifetime)) world.add(enemy, Component::Lifetime);
        world.lifetimes[enemy].remaining = std::max(world.lifetimes[enemy].remaining, keep);
        // Sobre el cuerpo, no debajo: el cadaver se dibuja por encima de un VFX a su misma altura
        spawn_vfx(world, pos - Vec2{0, 12.0f}, TileSprite::Orb, {255, 170, 60, 235}, 13.0f,
                  BRASERO_FUSE, 1.6f, 0.0f);
        spawn_particles(world, pos, {255, 150, 50, 255}, 6, 40.0f, BRASERO_FUSE);
        break;
    }

    // --- Agua Corrupta: se parte en dos aqui y ahora (los hijos han de contar como vivos
    //     antes de que el Engine compruebe si la sala esta despejada) ---
    case AIBehavior::SplitOnDeath: {
        if (ai.split_generation >= SPLIT_MAX_GEN) break;
        if (count_alive_enemies(world) >= SPLIT_MAX_ALIVE) break;

        EnemyType type = static_cast<EnemyType>(ai.enemy_type);
        int chapter = std::max(1, static_cast<int>(ai.chapter));
        u8  gen     = static_cast<u8>(ai.split_generation + 1);
        f32 shrink  = (gen == 1) ? 0.62f : 0.44f;

        int born = 0;
        for (int i = 0; i < 2; i++) {
            Vec2 at = free_spot_near(world, pos, 12.0f, i * 3 + 1, pos);
            Entity c = spawn_enemy(world, type, at, world.enemy_textures[static_cast<u32>(type)], chapter);
            if (c == ENTITY_NONE) break;
            world.enemy_ais[c].split_generation = gen;
            world.enemy_ais[c].home_cell = ai.home_cell;   // los hijos cuentan en la celda del padre
            world.healths[c].max *= 0.55f;
            world.healths[c].current = world.healths[c].max;
            world.combats[c].damage *= 0.70f;
            world.transforms[c].scale = {shrink, shrink};
            world.colliders[c].radius *= shrink;
            world.movements[c].speed *= 1.12f;   // mas pequeña, mas rapida
            born++;
        }
        if (born > 0) {
            spawn_vfx(world, pos, TileSprite::Orb, {90, 160, 220, 200}, 16.0f, 0.4f, 1.5f, 0.0f);
            spawn_particles(world, pos, {90, 160, 220, 255}, 12, 80.0f, 0.4f);
            SDL_Log("Agua Corrupta se divide: gen %u, %d hijos, %d vivos",
                    static_cast<unsigned>(gen), born, count_alive_enemies(world));
        }
        break;
    }

    default:
        break;
    }
}

// El estallido en si: dana a jugador y enemigos por igual (las brasas no distinguen).
static void detonate(ECSWorld& world, FaithSystem& faith, CombatEvents& events, Entity e) {
    Vec2 at = world.has(e, Component::Transform) ? world.transforms[e].position : Vec2{160, 90};

    // El cadaver ya no tiene CombatComponent: el dano sale del def con la misma escala
    // por capitulo que usa spawn_common.
    int chapter = std::max(1, static_cast<int>(world.enemy_ais[e].chapter));
    int step = (chapter - 1) % 5;
    f32 dmg = get_enemy_def(EnemyType::IdolatraDeRimon).damage * (1.0f + 0.18f * step) * 1.40f;
    if (chapter > Config::CHAPTERS_ACT_I) dmg *= 1.20f;

    // El anillo termina midiendo lo que mide el radio del estallido (~56 unidades)
    spawn_vfx(world, at, TileSprite::Star, {255, 190, 90, 230}, 18.0f, 0.45f, 4.5f, 190.0f);
    SDL_Log("Brasero de Rimon estalla en (%.0f,%.0f) dano %.1f  t=%.2f", at.x, at.y, dmg, world.time);
    spawn_particles(world, at, {255, 140, 40, 255}, 18, 130.0f, 0.5f);
    world.shake_request = std::max(world.shake_request, 3.0f);
    // El brasero prendido: estallido de verdad + humo que sube del cadaver
    spawn_fx(world, at, FxID::Explosion, {255, 214, 170, 240}, 58.0f, 0.42f);
    spawn_fx(world, at - Vec2{0, 6.0f}, FxID::Smoke, {190, 170, 155, 190}, 44.0f, 0.85f, 7);

    for (u32 v = 0; v < world.entity_count; v++) {
        if (v == e || !world.is_alive(v)) continue;
        if (!world.has(v, Component::Health) || !world.has(v, Component::Transform)) continue;
        if (!world.healths[v].is_alive()) continue;

        bool is_player = (static_cast<Entity>(v) == world.player);
        bool is_enemy  = is_hostile(world, v);
        bool is_disc   = is_ally(world, v);      // el brasero tampoco perdona a un discipulo
        if (!is_player && !is_enemy && !is_disc) continue;   // pickups y proyectiles no cuentan
        if (world.transforms[v].position.distance_to(at) > BRASERO_RADIUS) continue;

        Vec2 away = (world.transforms[v].position - at).normalized();
        if (is_player) {
            auto& h = world.healths[v];
            if (h.invulnerable || h.iframes_timer > 0.0f) continue;   // los i-frames mandan
            apply_damage(world, e, v, dmg);
            if (world.has(v, Component::Movement)) world.movements[v].knockback = away * 150.0f;
            faith.on_take_damage();
            events.player_hits++;
        } else if (is_disc) {
            // Un discipulo cae sin dar fe ni botin: no es un enemigo abatido
            if (world.has(v, Component::Movement)) world.movements[v].knockback = away * 90.0f;
            apply_damage(world, e, v, dmg);
        } else {
            if (world.has(v, Component::Movement)) world.movements[v].knockback = away * 90.0f;
            if (apply_damage(world, e, v, dmg)) on_enemy_killed(world, faith, v, events);
        }
    }
}

void update_death_fuses(ECSWorld& world, FaithSystem& faith, CombatEvents& events, f32 dt) {
    // Dos pasadas a proposito: primero se apuran las mechas, luego estallan. Asi una
    // explosion que mata a otro Idolatra le enciende una mecha nueva que NO se consume este
    // mismo frame: la cadena se reparte por frames en vez de recursar (una sala de braseros
    // colgaba el frame si el estallido llamaba al estallido).
    constexpr int MAX_PER_FRAME = 12;
    Entity ready[MAX_PER_FRAME];
    int n = 0;

    for (u32 e = 0; e < world.entity_count && n < MAX_PER_FRAME; e++) {
        if (!world.is_alive(e) || !world.has(e, Component::EnemyAI)) continue;
        auto& ai = world.enemy_ais[e];
        if (ai.death_fuse <= 0.0f) continue;
        ai.death_fuse -= dt;
        if (ai.death_fuse <= 0.0f) {
            ai.death_fuse = 0.0f;
            ready[n++] = static_cast<Entity>(e);
        }
    }

    for (int i = 0; i < n; i++) detonate(world, faith, events, ready[i]);
}
