#pragma once

// =============================================================================
// MANTLE - Enemy Definitions & AI (GDD 7.2)
// =============================================================================

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
enum class BossPattern : u8 {
    CapitanDeAcab,    // Cap 1: cargas en linea, pide refuerzos a media vida
    SacerdoteBaal,    // Cap 2: invoca profetas menores y se cura
    ProfetasDeBaal,   // Cap 3: la multitud; 2a fase "se sajaban con cuchillos"
    Desesperacion,    // Cap 4: viento -> terremoto -> fuego -> silbo apacible
    AcabYJezabel,     // Cap 5: duo; al caer uno, el otro se enfurece
    EspirituCorrupto, // Cap 6: se divide al perder vida
    Acreedor,         // Cap 7: cobra en sangre (se cura con cada ronda)
    LaMuerte,         // Cap 8: intocable de lejos; solo cede en cuerpo a cuerpo
    Guiezi,           // Cap 9: teleporta y dispara al llegar
    BenAdad           // Cap 10: asedio, oleadas de sirios sin fin
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

inline EnemyDef get_enemy_def(EnemyType type) {
    switch (type) {
        case EnemyType::SoldadoDeAcab:
            return {"Soldado de Acab", 20, 8, 88, AIBehavior::MeleeDirect, 190, 16, {41, 128, 185, 255}, "sprites/soldado_acab.png"};
        case EnemyType::ArqueroReal:
            return {"Arquero Real", 15, 10, 66, AIBehavior::Ranged, 220, 70, {230, 126, 34, 255}, "sprites/arquero_real.png"};
        case EnemyType::ProfetaDeBaal:
            return {"Profeta de Baal", 35, 12, 58, AIBehavior::Caster, 200, 90, {142, 68, 173, 255}, "sprites/profeta_baal.png"};
        case EnemyType::LeonDelDesierto:
            return {"León del Desierto", 40, 18, 118, AIBehavior::FastLeaper, 200, 60, {241, 196, 15, 255}, "sprites/leon_desierto.png"};
        case EnemyType::SoldadoSirio:
            return {"Soldado Sirio", 25, 10, 78, AIBehavior::MeleeResistant, 190, 16, {22, 160, 133, 255}, "sprites/soldado_sirio.png"};
        case EnemyType::GeneralSirio:
            return {"General Sirio", 60, 18, 64, AIBehavior::BuffAllies, 220, 80, {192, 57, 43, 255}, "sprites/general_sirio.png"};
        case EnemyType::IdolatraDeRimon:
            return {"Idólatra de Rimón", 20, 8, 96, AIBehavior::ExplodeOnDeath, 200, 12, {150, 90, 170, 255}, "sprites/idolatra_rimon.png"};
        case EnemyType::AguaCorrupta:
            return {"Agua Corrupta", 35, 10, 70, AIBehavior::SplitOnDeath, 190, 20, {44, 62, 80, 255}, "sprites/agua_corrupta.png"};
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

// Chapter boss: same sprite family, much tougher, bigger. `pattern` decide como pelea.
Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id,
                  int chapter, const char* boss_name, BossPattern pattern);

// =============================================================================
// Enemy AI Update
// =============================================================================
void update_enemy_ai(ECSWorld& world, f32 dt);

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
