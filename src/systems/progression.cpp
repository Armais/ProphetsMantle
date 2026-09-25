// =============================================================================
// MANTLE - Meta-Progression Implementation
// =============================================================================

#include "core/i18n.h"
#include "systems/progression.h"

void ProgressionSystem::init() {
    upgrades.clear();

    upgrades.push_back({0, tr("Provisión del Cuervo", "The Raven's Provision"),
        tr("Empieza con +10 HP", "Start with +10 HP"), 50, false});
    upgrades.push_back({1, tr("Fuego Interior", "Inner Fire"),
        tr("Empieza con +2 de daño base", "Start with +2 base damage"), 75, false});
    upgrades.push_back({2, tr("Sandalias Rápidas", "Swift Sandals"),
        tr("Empieza con +10% velocidad", "Start with +10% speed"), 60, false});
    upgrades.push_back({3, tr("Fe Renovada", "Renewed Faith"),
        tr("Empieza con 60 Fe en lugar de 50", "Start with 60 Faith instead of 50"), 100, false});
    upgrades.push_back({4, tr("Herencia Ampliada", "Broader Inheritance"),
        tr("Eliseo hereda 60% Fe en lugar de 50%", "Elisha inherits 60% Faith instead of 50%"), 150, false});
    upgrades.push_back({5, tr("Altar Portátil", "Portable Altar"),
        tr("Puedes orar una vez por capítulo sin altar", "Pray once per chapter without an altar"), 120, false});
    upgrades.push_back({6, tr("Doble Porción Mejorada", "Greater Double Portion"),
        tr("Eliseo empieza con 3 slots de milagro", "Elisha starts with 3 miracle slots"), 200, false});
    // "Los hijos de los profetas" (2 Reyes 2:3): el sistema de Discipulos del GDD es la
    // fortaleza propia de Eliseo, asi que se compra aqui y solo le sirve a el.
    upgrades.push_back({7, tr("La Escuela de Profetas", "The School of Prophets"),
        tr("Eliseo camina siempre con un discípulo", "Elisha always walks with a disciple"), 220, false});

    // --- Mejoras que DESBLOQUEAN objetos (GDD 3.1, paso 7 del bucle) ---
    // Cuatro de los seis objetos del catalogo ampliado nacen bloqueados: hasta que se
    // compran no salen en ninguna carta de eleccion. Los otros dos (Sal de Eliseo, Cinto
    // de Cuero) entran desde el primer dia, para que el catalogo nuevo se note ya.
    upgrades.push_back({8,  tr("El Arco del Rey", "The King's Bow"),
        tr("Añade el Arco de Joás al sorteo", "Adds Joash's Bow to the pool"), 90, false, 16});
    upgrades.push_back({9,  tr("El Remedio del Profeta", "The Prophet's Remedy"),
        tr("Añade los Higos de Ezequías al sorteo", "Adds Hezekiah's Figs to the pool"), 110, false, 17});
    upgrades.push_back({10, tr("La Mesa de Eliseo", "Elisha's Table"),
        tr("Añade los Panes de Cebada al sorteo", "Adds the Loaves of Barley to the pool"), 140, false, 21});
    upgrades.push_back({11, tr("La Sombra que Vuelve", "The Shadow that Returns"),
        tr("Añade El Reloj de Acaz al sorteo", "Adds the Dial of Ahaz to the pool"), 260, false, 20});

    // --- Cosmeticos: skins del cayado (GDD 4.x, "NO pay-to-win") ---
    // No cambian ni una cifra: tinen el proyectil y el fogonazo. Se compran con ofrendas,
    // que es la moneda que ya existe, y NO con dinero: la parte de tienda del GDD queda
    // fuera hasta que el usuario la pida (metería una dependencia externa).
    upgrades.push_back({12, tr("Brasa del Carmelo", "Ember of Carmel"),
        tr("Cayado en rojo brasa (cosmético)", "Ember-red staff (cosmetic)"),
        80, false, MetaUpgrade::NO_ITEM, 1});
    upgrades.push_back({13, tr("Luz del Jordán", "Light of the Jordan"),
        tr("Cayado en azul claro (cosmético)", "Pale-blue staff (cosmetic)"),
        80, false, MetaUpgrade::NO_ITEM, 2});
    upgrades.push_back({14, tr("Ceniza del Horeb", "Ash of Horeb"),
        tr("Cayado en gris violáceo (cosmético)", "Violet-grey staff (cosmetic)"),
        80, false, MetaUpgrade::NO_ITEM, 3});
}

bool ProgressionSystem::purchase(u32 upgrade_id) {
    for (auto& u : upgrades) {
        if (u.id == upgrade_id && !u.unlocked && offerings >= u.cost) {
            offerings -= u.cost;
            u.unlocked = true;
            return true;
        }
    }
    return false;
}

bool ProgressionSystem::is_unlocked(u32 upgrade_id) const {
    for (auto& u : upgrades) {
        if (u.id == upgrade_id) return u.unlocked;
    }
    return false;
}

void ProgressionSystem::add_offerings(int amount) {
    offerings += amount;
}
