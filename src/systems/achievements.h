#pragma once

// =============================================================================
// MANTLE - Logros
// =============================================================================
//
// Sistema PROPIO, no un envoltorio de Steam. El GDD pide logros; Steam es una forma de
// enseñarlos, no el sistema. Hacerlo al revés ataría el juego a una plataforma, metería
// una dependencia externa y exigiría un appid que puede no existir — y MANTLE sale a
// Escritorio, Web, Android e iOS. Si algún día entra Steam, entra como un adaptador
// delgado tras `MANTLE_STEAM` que lea esta misma máscara.
//
// Forma: struct plano + bitmask, igual que systems/progression.h. Persiste en
// `SaveData::achievements` (32 bits = 32 logros como techo; hoy hay 18).
//
// REGLA DE ADMISIÓN: un logro que no se pueda comprobar con un guion de autoplay NO
// entra. Los 18 de aquí se desbloquean desde scripts/logros_check.txt.
//
// =============================================================================

#include "core/types.h"

enum class AchievementID : u32 {
    // --- Progresión --------------------------------------------------------
    PrimerAltar = 0,     // Derribar tu primer altar de Baal
    PrimerJefe,          // Derrotar al primer jefe de capítulo
    ActoII,              // Alcanzar el capítulo 6 (Eliseo)
    Ascension,           // Completar la Ascensión
    Victoria,            // Ganar el capítulo 10
    Epilogo,             // Ganar con la fe en Profeta Ungido (desbloquea "El Legado")

    // --- Fe ----------------------------------------------------------------
    FuegoDelCielo,       // Llegar a 100 de fe
    BajoLaRetama,        // Limpiar una sala con la fe por los suelos (< 10)

    // --- Combate -----------------------------------------------------------
    CienCaidos,          // 100 bajas acumuladas entre partidas
    SinRasguno,          // Limpiar una sala de combate sin recibir un golpe
    LeonDelDesierto,     // Abatir un león

    // --- Milagros ----------------------------------------------------------
    PrimerMilagro,       // Lanzar tu primer milagro
    DiezProdigios,       // Haber lanzado los diez milagros al menos una vez
    TriplePorcion,       // Llevar los tres slots de milagro ocupados a la vez
    EjercitoCelestial,   // Tener tres discípulos en pie a la vez

    // --- Objetos -----------------------------------------------------------
    ElManto,             // Recibir El Manto de Elías
    PrimeraReliquia,     // Equipar tu primera reliquia
    Coleccionista,       // Ocho objetos a la vez en una partida

    COUNT
};

struct AchievementDef {
    const char* name;
    const char* description;   // Cómo se consigue, en una línea
};

// Tabla como switch (convención del proyecto: los errores salen al compilar, no al jugar)
AchievementDef get_achievement_def(AchievementID id);

struct AchievementSystem {
    u32 unlocked = 0;    // Espejo de SaveData::achievements
    // El Engine consume esto para el aviso en pantalla. COUNT = no hay nada pendiente.
    AchievementID pending = AchievementID::COUNT;

    bool has(AchievementID id) const {
        return (unlocked >> static_cast<u32>(id)) & 1u;
    }

    // Devuelve true SOLO la primera vez. Es idempotente a propósito: las llamadas están
    // sembradas por todo el Engine y varias se repiten cada frame (llevar tres slots,
    // tener ocho objetos), así que la que avisa tiene que ser la primera y nada más.
    bool unlock(AchievementID id) {
        if (id >= AchievementID::COUNT || has(id)) return false;
        unlocked |= (1u << static_cast<u32>(id));
        pending = id;
        return true;
    }

    int count_unlocked() const {
        int n = 0;
        for (u32 i = 0; i < static_cast<u32>(AchievementID::COUNT); i++)
            if ((unlocked >> i) & 1u) n++;
        return n;
    }
};
