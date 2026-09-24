// =============================================================================
// MANTLE - Logros: tabla de definiciones
// =============================================================================
//
// Los nombres salen del texto de 1-2 Reyes, como el resto del juego, y la descripción
// dice EXACTAMENTE lo que hay que hacer: un logro cuya condición hay que adivinar es un
// logro que nadie persigue. Sin referencias bíblicas en pantalla (regla del proyecto):
// el nombre evoca el pasaje, no lo cita.
//
// =============================================================================

#include "core/i18n.h"
#include "systems/achievements.h"

AchievementDef get_achievement_def(AchievementID id) {
    switch (id) {
        // --- Progresión ------------------------------------------------------
        case AchievementID::PrimerAltar:
            return {tr("El Fuego Responde", "The Fire Answers"),     tr("Derriba tu primer altar de Baal", "Tear down your first altar of Baal")};
        case AchievementID::PrimerJefe:
            return {tr("Ante el Rey", "Before the King"),           tr("Derrota al jefe de un capítulo", "Defeat a chapter boss")};
        case AchievementID::ActoII:
            return {tr("Las Aguas Sanadas", "The Waters Healed"),     tr("Llega al capítulo 6 como Eliseo", "Reach chapter 6 as Elisha")};
        case AchievementID::Ascension:
            return {tr("El Carro de Fuego", "The Chariot of Fire"),     tr("Completa la Ascensión", "Complete the Ascension")};
        case AchievementID::Victoria:
            return {tr("El Ejército Celestial", "The Heavenly Host"), tr("Termina el capítulo 10", "Finish chapter 10")};
        case AchievementID::Epilogo:
            return {tr("Doble Porción", "Double Portion"),         tr("Gana con la fe en Profeta Ungido", "Win with faith at Anointed Prophet")};

        // --- Fe ---------------------------------------------------------------
        case AchievementID::FuegoDelCielo:
            return {tr("Celo del Señor", "Zeal of the Lord"),        tr("Alcanza 100 de fe", "Reach 100 faith")};
        case AchievementID::BajoLaRetama:
            return {tr("Bajo la Retama", "Under the Juniper"),        tr("Vence a un grupo con menos de 10 de fe", "Beat a band of foes under 10 faith")};

        // --- Combate ----------------------------------------------------------
        case AchievementID::CienCaidos:
            return {tr("Cien Caídos", "A Hundred Fallen"),           tr("Abate 100 enemigos en total", "Bring down 100 enemies in total")};
        case AchievementID::SinRasguno:
            return {tr("Sin un Rasguño", "Without a Scratch"),        tr("Vence a un grupo sin recibir un golpe", "Beat a band of foes without a hit")};
        case AchievementID::LeonDelDesierto:
            return {tr("El León del Camino", "The Lion on the Road"),    tr("Abate un león del desierto", "Bring down a desert lion")};

        // --- Milagros ---------------------------------------------------------
        case AchievementID::PrimerMilagro:
            return {tr("La Palabra", "The Word"),            tr("Lanza tu primer milagro", "Cast your first miracle")};
        case AchievementID::DiezProdigios:
            return {tr("Los Diez Prodigios", "The Ten Wonders"),    tr("Lanza los diez milagros al menos una vez", "Cast all ten miracles at least once")};
        case AchievementID::TriplePorcion:
            return {tr("Triple Porción", "Triple Portion"),        tr("Lleva los tres slots de milagro ocupados", "Fill all three miracle slots")};
        case AchievementID::EjercitoCelestial:
            return {tr("Más son los que Están", "More Are With Us"), tr("Ten tres discípulos en pie a la vez", "Have three disciples standing at once")};

        // --- Objetos ----------------------------------------------------------
        case AchievementID::ElManto:
            return {tr("El Manto del Profeta", "The Prophet's Mantle"),  tr("Recoge El Manto de Elías", "Take up Elijah's Mantle")};
        case AchievementID::PrimeraReliquia:
            return {tr("Reliquia", "Relic"),              tr("Equipa tu primera reliquia", "Equip your first relic")};
        case AchievementID::Coleccionista:
            return {tr("El Arca", "The Ark"),               tr("Reúne ocho objetos en una partida", "Gather eight items in one run")};

        default:
            return {"???", "???"};
    }
}
