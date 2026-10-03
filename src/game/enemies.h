#pragma once

// =============================================================================
// MANTLE - Enemy Definitions & AI (GDD 7.2)
// =============================================================================

#include "core/i18n.h"
#include "core/types.h"
#include "core/ecs.h"

// =============================================================================
// Enemy Type (from GDD)
// =============================================================================
enum class EnemyType : u32 {
    // Act I
    SoldadoDeAcab = 0,  // Melee directo
    ArqueroReal,        // Distancia
    ProfetaDeBaal,      // Caster, cura
    LeonDelDesierto,    // Rapido, salta

    // Act II
    SoldadoSirio,       // Melee resistente
    GeneralSirio,       // Buff aliados
    IdolatraDeRimon,    // Explota al morir (el brasero de brasas que carga)
    AguaCorrupta,       // Se divide

    COUNT
};

// =============================================================================
// Patron de jefe: cada capitulo pelea distinto (GDD 7.2 / 1-2 Reyes)
// =============================================================================
// Desde el 2026-10-03 cada patron es una ESTRATEGIA propia con fases y telegrafos (la
// implementacion y las cifras viven en game/bosses.cpp; el diseno, en src/game/CLAUDE.md,
// seccion "Los diez jefes"). El patron va 1:1 con el capitulo.
enum class BossPattern : u8 {
    CapitanDeAcab,    // Cap 1: carga en carril; contra el muro queda aturdido; escudo frontal
    SacerdoteBaal,    // Cap 2: idolos que le hacen intocable + lluvia de fuego
    ProfetasDeBaal,   // Cap 3: el corro le protege; ritual en el centro; "se sajaban"
    Desesperacion,    // Cap 4: viento -> terremoto -> fuego -> silbo apacible (1 R 19:11-12)
    AcabYJezabel,     // Cap 5: duo de verdad; ella le cura, el barre; el que queda se enfurece
    EspirituCorrupto, // Cap 6: charcas, se sumerge y emerge bajo el profeta, se divide
    Acreedor,         // Cap 7: la cadena tira de ti y cobra fe; la deuda se cierra
    LaMuerte,         // Cap 8: oscuridad; intocable de lejos; la guadana y su resuello
    Guiezi,           // Cap 9: senuelos; la plata le hace arrodillarse
    BenAdad           // Cap 10: artilleria de asedio, carro, flechas; la hueste celestial
};

// Lo que un jefe pone en la arena y NO es el jefe (EnemyAIComponent::prop_kind). Es hostil
// —se le dispara y cuenta como vivo— pero no va en la barra y muere con su dueno.
enum class BossProp : u8 {
    None = 0,
    Idol,     // idolo de Baal (cap. 2): mientras quede uno en pie, el sacerdote es intocable
    Decoy,    // senuelo de Guiezi (cap. 9): 1 de vida, dispara, revienta en plata
};

// Hojas propias de cada jefe (rooms.h get_boss_sheet). `own` = la hoja es del jefe; si es
// false el Engine paso la del enemigo base (la hoja del jefe aun no existe) y el jefe se
// dibuja como siempre, a 1,6 en vez de a 1,4.
struct BossSheets {
    u32  main        = TEXTURE_NONE;
    bool main_own    = false;
    u32  partner     = TEXTURE_NONE;   // Jezabel (cap. 5)
    bool partner_own = false;
};

struct EnemyDef {
    const char* name;
    f32 hp;
    f32 damage;
    f32 speed;
    AIBehavior behavior;
    f32 aggro_range;
    f32 attack_range;
    Color color;              // Placeholder color (also tint when no sprite sheet is loaded)
    const char* sprite_path;  // Sheet relative to assets/ (224x480, see render/sprites.h)
};

// Paleta del capitulo aplicada al ENEMIGO (no al terreno: de eso ya se ocupa
// `get_biome_style`). Es un tinte multiplicativo suave, no una mano de pintura: los ocho
// tipos siguen siendo reconocibles, pero un soldado sirio de la ciudad sitiada no se ve
// igual que uno del campamento del Jordan.
//
// Se hace TINTANDO y no con hojas alternativas a proposito. Una paleta por capitulo son
// ocho PNG mas de 896x1920 por variante, y una regla del proyecto es que `assets/` no
// crece. Un tinte cuesta cero bytes y lo pisa igual el destello del golpe.
inline Color get_enemy_chapter_tint(int chapter) {
    switch (chapter) {
        case 1:  return {255, 246, 233, 255};   // El Arroyo Querit: canon calido, casi neutro
        case 2:  return {226, 202, 240, 255};   // Sarepta: la sombra morada de Baal
        case 3:  return {206, 223, 246, 255};   // El Monte Carmelo: piedra fria de altura
        case 4:  return {250, 226, 178, 255};   // Horeb: polvo de desierto
        case 5:  return {226, 246, 209, 255};   // El Llamado: campos de labranza
        case 6:  return {202, 240, 206, 255};   // Jerico: el verde enfermo del agua corrupta
        case 7:  return {246, 216, 182, 255};   // La aldea pobre: barro y aceite
        case 8:  return {202, 209, 246, 255};   // La casa noble: indigo frio
        case 9:  return {246, 236, 202, 255};   // El campamento junto al Jordan: arena palida
        case 10: return {206, 216, 230, 255};   // La ciudad sitiada: acero
        default: return Color::white();
    }
}

inline EnemyDef get_enemy_def(EnemyType type) {
    switch (type) {
        case EnemyType::SoldadoDeAcab:
            return {tr("Soldado de Acab", "Soldier of Ahab"), 20, 8, 88, AIBehavior::MeleeDirect, 190, 16, {41, 128, 185, 255}, "sprites/soldado_acab.png"};
        case EnemyType::ArqueroReal:
            return {tr("Arquero Real", "Royal Archer"), 15, 10, 66, AIBehavior::Ranged, 220, 70, {230, 126, 34, 255}, "sprites/arquero_real.png"};
        case EnemyType::ProfetaDeBaal:
            return {tr("Profeta de Baal", "Prophet of Baal"), 35, 12, 58, AIBehavior::Caster, 200, 90, {142, 68, 173, 255}, "sprites/profeta_baal.png"};
        case EnemyType::LeonDelDesierto:
            return {tr("León del Desierto", "Desert Lion"), 40, 18, 118, AIBehavior::FastLeaper, 200, 60, {241, 196, 15, 255}, "sprites/leon_desierto.png"};
        case EnemyType::SoldadoSirio:
            return {tr("Soldado Sirio", "Syrian Soldier"), 25, 10, 78, AIBehavior::MeleeResistant, 190, 16, {22, 160, 133, 255}, "sprites/soldado_sirio.png"};
        case EnemyType::GeneralSirio:
            return {tr("General Sirio", "Syrian General"), 60, 18, 64, AIBehavior::BuffAllies, 220, 80, {192, 57, 43, 255}, "sprites/general_sirio.png"};
        case EnemyType::IdolatraDeRimon:
            return {tr("Idólatra de Rimón", "Idolater of Rimmon"), 20, 8, 96, AIBehavior::ExplodeOnDeath, 200, 12, {150, 90, 170, 255}, "sprites/idolatra_rimon.png"};
        case EnemyType::AguaCorrupta:
            return {tr("Agua Corrupta", "Corrupt Water"), 35, 10, 70, AIBehavior::SplitOnDeath, 190, 20, {44, 62, 80, 255}, "sprites/agua_corrupta.png"};
        default:
            return {"???", 10, 5, 50, AIBehavior::MeleeDirect, 80, 16, {128, 128, 128, 255}, nullptr};
    }
}

// =============================================================================
// Enemy Spawning
// =============================================================================
// texture_id: sheet from Engine::enemy_textures (TEXTURE_NONE = tinted placeholder)
// chapter scales HP/damage a little so later chapters stay threatening.
Entity spawn_enemy(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id = TEXTURE_NONE,
                   int chapter = 1);

// Jefe del capitulo: hoja propia (o la del enemigo base si aun no existe), mucha mas vida,
// mas grande. `pattern` decide como pelea (game/bosses.cpp).
Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id, bool own_sheet,
                  int chapter, const char* boss_name, BossPattern pattern);

// Jefe con SEQUITO. Crea la cabeza con `spawn_boss` y, si el patron lo pide, sus miembros
// (`EnemyAIComponent::boss_group`) con barra compartida: el corro del Carmelo (cap. 3,
// 1 R 18:26-29) bailando alrededor de `ring_center`, o Jezabel junto a Acab (cap. 5).
// Devuelve la cabeza. Para los demas patrones es exactamente `spawn_boss`.
Entity spawn_boss_group(ECSWorld& world, EnemyType type, Vec2 position, Vec2 ring_center,
                        const BossSheets& sheets, int chapter, const char* boss_name,
                        BossPattern pattern);
// Miembros del grupo vivos (sin la cabeza)
int count_boss_group(const ECSWorld& world);

// IA de jefes, miembros de grupo y props (bosses.cpp). update_enemy_ai la llama para toda
// entidad con `is_boss`, `boss_group` o `prop_kind`.
void update_boss_entity(ECSWorld& world, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist,
                        f32 dmg, f32 dt);
// Cae un jefe, un miembro o un prop: limpia sus peligros y, si era el jefe, sus props y su
// plata. Lo llama on_enemy_death_behavior.
void on_boss_death(ECSWorld& world, Entity e, Vec2 pos);
// El aviso de la barra cuando el jefe es intocable (cada patron por un motivo distinto)
const char* boss_invulnerable_hint(BossPattern pattern);

// Marcas de los jefes: en el SUELO (aura, arco del escudo, "ahora si": el aro dorado) y
// por ENCIMA de los personajes (ataduras de los idolos y del corro, la curacion de
// Jezabel, la cadena del Acreedor, las estrellas del aturdido, la burbuja de intocable).
struct Renderer;
struct HazardArt;
void render_boss_ground(Renderer& renderer, const ECSWorld& world, const HazardArt& art);
void render_boss_overlays(Renderer& renderer, const ECSWorld& world, const HazardArt& art);

// =============================================================================
// Utilidades de IA compartidas (enemies.cpp las define; bosses.cpp las usa)
// =============================================================================
enum class ProjectileKind : u8;   // game/combat.h
bool   enemy_has_los(const ECSWorld& world, Vec2 from, Vec2 to);
void   enemy_shoot(ECSWorld& world, Entity e, Vec2 dir, ProjectileKind kind, f32 dmg, f32 speed, f32 range);
void   enemy_volley(ECSWorld& world, Entity e, Vec2 to_player, int count, f32 spread,
                    ProjectileKind kind, f32 dmg, f32 speed, f32 range);
void   enemy_ring(ECSWorld& world, Entity e, Vec2 to_player, int count,
                  ProjectileKind kind, f32 dmg, f32 speed, f32 range);
// Punto pisable cerca de `around` (o `fallback` si ninguna de las ocho direcciones vale)
Vec2   enemy_free_spot(const ECSWorld& world, Vec2 around, f32 radius, int seed, Vec2 fallback);
// Hostiles vivos que NO son el jefe (techo de invocaciones, de divisiones...)
int    count_alive_minions(const ECSWorld& world);

// =============================================================================
// Enemy AI Update
// =============================================================================
void update_enemy_ai(ECSWorld& world, f32 dt);

// -----------------------------------------------------------------------------
// Discipulos (Ejercito Celestial, 2 Reyes 6:17)
// -----------------------------------------------------------------------------
// Un discipulo es un combatiente con `EnemyAIComponent::ally` puesto: misma maquinaria
// de IA, movimiento y colision que un enemigo, otro bando. Persigue al hostil que
// elegiria el auto-apuntado y, sin ninguno cerca, escolta al profeta. Golpea por
// contacto (game/combat.cpp) y recibe golpes, asi que se puede perder.
// `life_seconds` > 0 = invocacion temporal, se desvanece al agotarse.
Entity spawn_disciple(ECSWorld& world, Vec2 position, u32 texture_id, f32 life_seconds,
                      f32 hp = 30.0f, f32 damage = 10.0f);

// -----------------------------------------------------------------------------
// Figuras inertes (escenas guionizadas: la audiencia con Acab)
// -----------------------------------------------------------------------------
// Una figura es alguien DE PIE en el mundo que ni pelea ni se mueve: Transform + Sprite +
// Animation y nada mas. Sin Health, Collider ni EnemyAI, `is_hostile`, `is_ally` y
// `count_living_enemies` la ignoran, y `load_room` la barre con todo lo demas. La fila de
// la hoja se HORNEA en `src_rect` al crearla porque `system_animation` no corre en
// GameScene::Dialogue, que es donde se la mira. Sombra y profundidad por pies las da
// ECSWorld::collect_draws (`is_figure`).
Entity spawn_figure(ECSWorld& world, Vec2 position, u32 texture_id, Direction facing);
// La misma figura pero desde una celda de `assets/sprites/npcs.png` (namespace Npc): la
// viuda, Guiezi, los hijos de los profetas... Una celda es un solo frame de 128 px con los
// pies en la fila 112, igual que un frame de hoja de personaje, asi que se pinta y se ordena
// exactamente como cualquier otra figura. No tiene direccion: mira siempre al frente.
Entity spawn_npc_figure(ECSWorld& world, Vec2 position, u32 npc_texture, int cell);

// =============================================================================
// Muerte con conducta (Acto II)
// =============================================================================
// Lo que hace un enemigo al caer, segun su AIBehavior. Vive aqui y no en combat.cpp
// para que la conducta de enemigo no se disperse por el modulo de combate.
// `pos` llega desde on_enemy_killed porque el cadaver puede haberse destruido ya.
//  - ExplodeOnDeath: enciende una mecha sobre el cadaver (la explosion la resuelve
//    update_death_fuses un tercio de segundo despues, para que se pueda esquivar).
//  - SplitOnDeath: parte al enemigo en dos hijos mas pequenos, aqui y ahora, para que
//    ya cuenten como vivos cuando el Engine compruebe si la sala esta despejada.
void on_enemy_death_behavior(ECSWorld& world, Entity enemy, Vec2 pos);

// Mechas encendidas: telegrafia y estallido del Idolatra de Rimon. El Engine la llama
// una vez por frame despues de system_collision (necesita fe/eventos para las bajas que
// cause) y antes de update_room_state (los muertos deben contar ya).
void update_death_fuses(ECSWorld& world, struct FaithSystem& faith,
                        struct CombatEvents& events, f32 dt);
