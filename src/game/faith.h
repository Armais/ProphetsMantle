#pragma once

// =============================================================================
// MANTLE - Faith System "El Celo del Senor" (GDD 3.2)
// =============================================================================

#include "core/i18n.h"
#include "core/types.h"
#include "core/config.h"

// =============================================================================
// Faith Levels
// =============================================================================
enum class FaithLevel : u8 {
    BajoLaRetama  = 0,  // 0-9:    -30% dmg, no miracles
    EnElDesierto  = 1,  // 10-24:  -15% dmg, Tier 1 only
    FeVacilante   = 2,  // 25-49:  normal, Tier 1-2
    SiervoFiel    = 3,  // 50-74:  +10% dmg, Tier 1-3
    ProfetaUngido = 4,  // 75-89:  +25% dmg, Tier 1-4
    FuegoDelCielo = 5   // 90-100: +40% dmg, powered miracles
};

struct FaithLevelInfo {
    const char* name;
    f32 damage_mult;      // Multiplicador de dano
    int max_miracle_tier; // Tier maximo de milagro accesible
};

inline FaithLevelInfo get_faith_level_info(FaithLevel level) {
    switch (level) {
        case FaithLevel::BajoLaRetama:  return {tr("Bajo la Retama", "Under the Juniper"),  0.70f, 0};
        case FaithLevel::EnElDesierto:  return {tr("En el Desierto", "In the Wilderness"),  0.85f, 1};
        case FaithLevel::FeVacilante:   return {tr("Fe Vacilante", "Wavering Faith"),    1.00f, 2};
        case FaithLevel::SiervoFiel:    return {tr("Siervo Fiel", "Faithful Servant"),     1.10f, 3};
        case FaithLevel::ProfetaUngido: return {tr("Profeta Ungido", "Anointed Prophet"),  1.25f, 4};
        case FaithLevel::FuegoDelCielo: return {tr("Fuego del Cielo", "Fire from Heaven"), 1.40f, 4};
    }
    return {"???", 1.0f, 0};
}

// Fe minima a la que se desbloquea un tier de milagro, y el nivel que hay que alcanzar.
// El HUD las usa para decir "necesitas 50 (Siervo Fiel)" en vez de pintar el slot en rojo
// y dejar al jugador adivinando por que no puede lanzar el milagro.
inline f32 faith_required_for_tier(int tier) {
    switch (tier) {
        case 1:  return Config::FAITH_LEVEL_1;
        case 2:  return Config::FAITH_LEVEL_2;
        case 3:  return Config::FAITH_LEVEL_3;
        case 4:  return Config::FAITH_LEVEL_4;
        default: return 0.0f;
    }
}

inline const char* faith_level_name_for_tier(int tier) {
    switch (tier) {
        case 1:  return get_faith_level_info(FaithLevel::EnElDesierto).name;
        case 2:  return get_faith_level_info(FaithLevel::FeVacilante).name;
        case 3:  return get_faith_level_info(FaithLevel::SiervoFiel).name;
        case 4:  return get_faith_level_info(FaithLevel::ProfetaUngido).name;
        default: return "";
    }
}

// =============================================================================
// Faith System
// =============================================================================
struct FaithSystem {
    f32 current    = Config::FAITH_INITIAL;
    f32 max        = Config::FAITH_MAX;
    FaithLevel level = FaithLevel::SiervoFiel;

    // Inactivity tracking
    f32 inactivity_timer = 0.0f;
    f32 inactivity_threshold = 10.0f;  // seconds before faith decays

    // Lower bound set by the relic "Tablas de la Ley" (faith never drops below it)
    f32 floor_value = 0.0f;

    void init() {
        current = Config::FAITH_INITIAL;
        level = FaithLevel::SiervoFiel;
        inactivity_timer = 0.0f;
        floor_value = 0.0f;
    }

    void update(f32 dt) {
        // Inactivity decay
        inactivity_timer += dt;
        if (inactivity_timer >= inactivity_threshold) {
            modify(Config::FAITH_INACTIVITY_RATE * dt);
        }
        recalculate_level();
    }

    // `ignore_floor` = paying for a miracle: that cost is allowed to dip under the relic
    // floor, otherwise "Tablas de la Ley" would mean free miracles forever. The floor also
    // never *raises* faith (min with the current value), it only blocks passive losses.
    void modify(f32 amount, bool ignore_floor = false) {
        f32 lower = ignore_floor ? 0.0f : std::min(current, floor_value);
        current = std::max(lower, std::min(max, current + amount));
        recalculate_level();
    }

    void reset_inactivity() {
        inactivity_timer = 0.0f;
    }

    // Specific faith events (from GDD)
    void on_destroy_altar()   { modify(Config::FAITH_DESTROY_ALTAR);  reset_inactivity(); }
    void on_kill_prophet()    { modify(Config::FAITH_KILL_PROPHET);   reset_inactivity(); }
    void on_room_no_damage()  { modify(Config::FAITH_ROOM_NO_DAMAGE); }
    void on_pray()            { modify(Config::FAITH_PRAY_ALTAR);     reset_inactivity(); }
    void on_help_widow()      { modify(Config::FAITH_HELP_WIDOW);     reset_inactivity(); }
    void on_defeat_boss()     { modify(Config::FAITH_DEFEAT_BOSS);    reset_inactivity(); }
    void on_take_damage()     { modify(Config::FAITH_TAKE_DAMAGE);    reset_inactivity(); }
    void on_flee()            { modify(Config::FAITH_FLEE); }

    f32 get_damage_multiplier() const {
        return get_faith_level_info(level).damage_mult;
    }

    int get_max_miracle_tier() const {
        return get_faith_level_info(level).max_miracle_tier;
    }

    const char* get_level_name() const {
        return get_faith_level_info(level).name;
    }

    f32 get_ratio() const {
        return current / max;
    }

private:
    void recalculate_level() {
        if      (current >= Config::FAITH_LEVEL_5) level = FaithLevel::FuegoDelCielo;
        else if (current >= Config::FAITH_LEVEL_4) level = FaithLevel::ProfetaUngido;
        else if (current >= Config::FAITH_LEVEL_3) level = FaithLevel::SiervoFiel;
        else if (current >= Config::FAITH_LEVEL_2) level = FaithLevel::FeVacilante;
        else if (current >= Config::FAITH_LEVEL_1) level = FaithLevel::EnElDesierto;
        else                                        level = FaithLevel::BajoLaRetama;
    }
};
