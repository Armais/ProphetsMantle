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
    LeprosoEnloquecido, // Explota al morir
    AguaCorrupta,       // Se divide

    COUNT
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
            return {"Leon del Desierto", 40, 18, 118, AIBehavior::FastLeaper, 200, 60, {241, 196, 15, 255}, "sprites/leon_desierto.png"};
        case EnemyType::SoldadoSirio:
            return {"Soldado Sirio", 25, 10, 78, AIBehavior::MeleeResistant, 190, 16, {22, 160, 133, 255}, "sprites/soldado_sirio.png"};
        case EnemyType::GeneralSirio:
            return {"General Sirio", 60, 18, 64, AIBehavior::BuffAllies, 220, 80, {192, 57, 43, 255}, "sprites/general_sirio.png"};
        case EnemyType::LeprosoEnloquecido:
            return {"Leproso Enloquecido", 20, 8, 96, AIBehavior::ExplodeOnDeath, 200, 12, {39, 174, 96, 255}, "sprites/leproso.png"};
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

// Chapter boss: same sprite family, much tougher, bigger, volleys of projectiles.
Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id,
                  int chapter, const char* boss_name);

// =============================================================================
// Enemy AI Update
// =============================================================================
void update_enemy_ai(ECSWorld& world, f32 dt);
