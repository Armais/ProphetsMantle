// =============================================================================
// MANTLE - Meta-Progression Implementation
// =============================================================================

#include "core/i18n.h"
#include "systems/progression.h"
#include <cstdio>
#include <initializer_list>

// Una fila del catalogo. Los costes van por rango y la lista dice cuantos rangos hay.
static MetaUpgrade make_upgrade(u32 id, MetaCategory cat, int icon, const char* name,
                                const char* desc, const char* quote,
                                std::initializer_list<int> costs) {
    MetaUpgrade u{id, name, desc, 0, false};
    u.category = cat;
    u.icon     = icon;
    u.quote    = quote;
    u.max_rank = 0;
    for (int c : costs) {
        if (u.max_rank >= META_MAX_RANK) break;
        u.costs[u.max_rank++] = c;
    }
    u.cost = u.costs[0];
    return u;
}

void ProgressionSystem::init() {
    upgrades.clear();
    using C = MetaCategory;

    // --- EL PROFETA: lo que llevas puesto desde la primera sala -------------------------
    // Iconos: celdas de icons.png reutilizadas (assets/ no crece): el cuervo, la antorcha,
    // las sandalias y el cuenco de ceniza.
    upgrades.push_back(make_upgrade(Meta::VIDA, C::Prophet, 22,
        tr("Provisión del Cuervo", "The Raven's Provision"),
        tr("Más vida máxima desde la primera sala.", "More max health from the first room."),
        tr("«Los cuervos le traían pan y carne por la mañana y por la tarde.»",
           "«The ravens brought him bread and meat in the morning and in the evening.»"),
        {40, 80, 130, 200, 300}));
    upgrades.push_back(make_upgrade(Meta::DANO, C::Prophet, 10,
        tr("Fuego Interior", "Inner Fire"),
        tr("Cada disparo del cayado hace más daño.", "Every staff shot deals more damage."),
        tr("«Su palabra quemaba como una antorcha.»", "«His word burned like a torch.»"),
        {75, 150, 260}));
    upgrades.push_back(make_upgrade(Meta::VELOCIDAD, C::Prophet, 4,
        tr("Sandalias Rápidas", "Swift Sandals"),
        tr("Caminas más deprisa.", "You walk faster."),
        tr("«Ciñó sus lomos y corrió delante de Acab.»", "«He girded up his loins and ran before Ahab.»"),
        {60, 120, 200}));
    upgrades.push_back(make_upgrade(Meta::FE, C::Prophet, 12,
        tr("Fe Renovada", "Renewed Faith"),
        tr("Empiezas la partida con más fe: milagros antes.", "You start with more faith: miracles sooner."),
        tr("«Vuelve a mirar siete veces.»", "«Go again seven times.»"),
        {100, 220}));

    // --- DONES: lo que cambia COMO se juega la partida ----------------------------------
    upgrades.push_back(make_upgrade(Meta::ALIENTO, C::Gifts, 27,
        tr("Aliento de Vida", "Breath of Life"),
        tr("Al caer, te levantas con media vida en vez de morir.", "When you fall, you rise with half your health instead of dying."),
        tr("«El alma del niño volvió a él, y revivió.»", "«The soul of the child came into him again, and he revived.»"),
        {180, 420}));
    upgrades.push_back(make_upgrade(Meta::PRIMICIAS, C::Gifts, 2,
        tr("Primicias", "Firstfruits"),
        tr("Empiezas la partida eligiendo un objeto entre tres.", "You start the run choosing one of three items."),
        tr("«Trajo al varón de Dios panes de primicias.»", "«He brought the man of God bread of the firstfruits.»"),
        {160, 320}));
    upgrades.push_back(make_upgrade(Meta::SUERTES, C::Gifts, 11,
        tr("Echar Suertes", "Casting Lots"),
        tr("Ante tres cartas que no te sirven, pides otras tres.", "Facing three cards you don't want, ask for three more."),
        tr("«La suerte se echa en el regazo; mas de Jehová es la decisión.»", "«The lot is cast into the lap; but the decision is the Lord's.»"),
        {70, 150, 260}));
    upgrades.push_back(make_upgrade(Meta::ALFORJA, C::Gifts, 19,
        tr("Alforja Ancha", "Wide Satchel"),
        tr("Más huecos en la alforja para llevar consumibles.", "More satchel slots to carry consumables."),
        tr("«Ciñe tus lomos y toma esta redoma en tu mano.»", "«Gird up thy loins, and take this box in thine hand.»"),
        {90, 200}));
    upgrades.push_back(make_upgrade(Meta::TINAJA, C::Gifts, 13,
        tr("La Tinaja de Sarepta", "The Jar of Zarephath"),
        tr("Cada capítulo empieza con Agua del Jordán en la alforja.", "Every chapter starts with Water of the Jordan in your satchel."),
        tr("«La harina de la tinaja no escaseará.»", "«The barrel of meal shall not waste.»"),
        {100, 210}));
    upgrades.push_back(make_upgrade(Meta::DIEZMO, C::Gifts, 7,
        tr("Diezmo", "Tithe"),
        tr("Cada intento te paga más ofrendas.", "Every attempt pays you more offerings."),
        tr("«Traed todos los diezmos al alfolí, y probadme.»", "«Bring ye all the tithes into the storehouse, and prove me.»"),
        {80, 170, 280}));
    upgrades.push_back(make_upgrade(Meta::ALTAR, C::Gifts, 26,
        tr("Altar Portátil", "Portable Altar"),
        tr("Oras donde estás, sin altar: +10 fe y +10 vida.", "Pray where you stand, no altar needed: +10 faith and +10 health."),
        tr("«Reparó el altar de Jehová que estaba arruinado.»", "«He repaired the altar of the Lord that was broken down.»"),
        {120, 240}));

    // --- ELISEO: el Acto II -----------------------------------------------------------
    upgrades.push_back(make_upgrade(Meta::HERENCIA, C::Elisha, 15,
        tr("Herencia Ampliada", "Broader Inheritance"),
        tr("En el Ascenso, Eliseo hereda más fe de Elías.", "At the Ascension, Elisha inherits more of Elijah's faith."),
        tr("«Que una doble porción de tu espíritu sea sobre mí.»", "«Let a double portion of thy spirit be upon me.»"),
        {150, 280}));
    upgrades.push_back(make_upgrade(Meta::DOBLE, C::Elisha, 31,
        tr("Doble Porción Mejorada", "Greater Double Portion"),
        tr("Eliseo empieza con tres huecos de milagro.", "Elisha starts with three miracle slots."),
        tr("«Tomó el manto de Elías, y golpeó las aguas.»", "«He took the mantle of Elijah, and smote the waters.»"),
        {200}));
    // "Los hijos de los profetas" (2 Reyes 2:3): el sistema de Discipulos es la fortaleza
    // propia de Eliseo, asi que se compra aqui y solo le sirve a el.
    upgrades.push_back(make_upgrade(Meta::ESCUELA, C::Elisha, 30,
        tr("La Escuela de Profetas", "The School of Prophets"),
        tr("Un discípulo camina y pelea siempre junto a Eliseo.", "A disciple always walks and fights beside Elisha."),
        tr("«Los hijos de los profetas salieron a recibirle.»", "«The sons of the prophets came forth to meet him.»"),
        {220}));

    // --- EL TESORO: objetos que entran en juego --------------------------------------
    // Nacen bloqueados (ItemDef::starts_locked). Las armas, pasivos y reliquias entran en el
    // sorteo de CARTAS; los Higos son consumible y desde la alforja salen en el MUNDO.
    MetaUpgrade arco = make_upgrade(Meta::ARCO, C::Treasure, 16,
        tr("El Arco del Rey", "The King's Bow"),
        tr("El Arco de Joás puede salir en las cartas: ráfaga de 3 saetas.", "Joash's Bow can appear in the cards: a volley of 3 arrows."),
        tr("«Saeta de salvación de Jehová.»", "«The arrow of the Lord's deliverance.»"), {90});
    arco.unlocks_item = 16;
    upgrades.push_back(arco);
    MetaUpgrade higos = make_upgrade(Meta::HIGOS, C::Treasure, 17,
        tr("El Remedio del Profeta", "The Prophet's Remedy"),
        tr("Los Higos de Ezequías aparecen por el mundo: curan toda la vida.", "Hezekiah's Figs appear in the world: they heal you fully."),
        tr("«Tomad masa de higos.»", "«Take a lump of figs.»"), {110});
    higos.unlocks_item = 17;
    upgrades.push_back(higos);
    MetaUpgrade panes = make_upgrade(Meta::PANES, C::Treasure, 21,
        tr("La Mesa de Eliseo", "Elisha's Table"),
        tr("Los Panes de Cebada pueden salir en las cartas.", "The Loaves of Barley can appear in the cards."),
        tr("«Da a la gente para que coma.»", "«Give unto the people, that they may eat.»"), {140});
    panes.unlocks_item = 21;
    upgrades.push_back(panes);
    MetaUpgrade reloj = make_upgrade(Meta::RELOJ, C::Treasure, 20,
        tr("La Sombra que Vuelve", "The Shadow that Returns"),
        tr("El Reloj de Acaz puede salir en las cartas: perdona una muerte por capítulo.", "The Dial of Ahaz can appear in the cards: forgives one death per chapter."),
        tr("«La sombra volvió atrás diez grados.»", "«The shadow went back ten degrees.»"), {260});
    reloj.unlocks_item = 20;
    upgrades.push_back(reloj);

    // --- ASPECTO: skins del cayado (GDD 4.x, "NO pay-to-win") ------------------------
    // No cambian ni una cifra: tinen el proyectil y el fogonazo. Se compran con ofrendas,
    // que es la moneda que ya existe, y NO con dinero.
    MetaUpgrade s1 = make_upgrade(Meta::SKIN_BRASA, C::Cosmetic, 10,
        tr("Brasa del Carmelo", "Ember of Carmel"),
        tr("Cayado en rojo brasa. Solo aspecto: se equipa en Opciones.", "Ember-red staff. Looks only: equip it in Options."),
        tr("«Cayó fuego de Jehová y consumió el holocausto.»", "«The fire of the Lord fell, and consumed the burnt sacrifice.»"), {80});
    s1.unlocks_skin = 1;
    upgrades.push_back(s1);
    MetaUpgrade s2 = make_upgrade(Meta::SKIN_JORDAN, C::Cosmetic, 10,
        tr("Luz del Jordán", "Light of the Jordan"),
        tr("Cayado en azul claro. Solo aspecto: se equipa en Opciones.", "Pale-blue staff. Looks only: equip it in Options."),
        tr("«Las aguas se apartaron a uno y otro lado.»", "«The waters parted hither and thither.»"), {80});
    s2.unlocks_skin = 2;
    upgrades.push_back(s2);
    MetaUpgrade s3 = make_upgrade(Meta::SKIN_HOREB, C::Cosmetic, 10,
        tr("Ceniza del Horeb", "Ash of Horeb"),
        tr("Cayado en gris violáceo. Solo aspecto: se equipa en Opciones.", "Violet-grey staff. Looks only: equip it in Options."),
        tr("«Y tras el fuego, un silbo apacible.»", "«And after the fire, a still small voice.»"), {80});
    s3.unlocks_skin = 3;
    upgrades.push_back(s3);
}

static MetaUpgrade* find_upgrade(std::vector<MetaUpgrade>& v, u32 id) {
    for (auto& u : v) if (u.id == id) return &u;
    return nullptr;
}

bool ProgressionSystem::purchase(u32 upgrade_id) {
    MetaUpgrade* u = find_upgrade(upgrades, upgrade_id);
    if (!u || u->maxed()) return false;
    const int c = u->next_cost();
    if (offerings < c) return false;
    offerings -= c;
    u->rank++;
    u->unlocked = true;
    u->cost = u->next_cost();
    return true;
}

const MetaUpgrade* ProgressionSystem::get(u32 upgrade_id) const {
    for (auto& u : upgrades) if (u.id == upgrade_id) return &u;
    return nullptr;
}

bool ProgressionSystem::is_unlocked(u32 upgrade_id) const {
    const MetaUpgrade* u = get(upgrade_id);
    return u && u->rank > 0;
}

int ProgressionSystem::rank_of(u32 upgrade_id) const {
    const MetaUpgrade* u = get(upgrade_id);
    return u ? u->rank : 0;
}

void ProgressionSystem::add_offerings(int amount) {
    offerings += amount;
}

int ProgressionSystem::ranks_bought() const {
    int n = 0;
    for (const auto& u : upgrades) n += u.rank;
    return n;
}

int ProgressionSystem::ranks_total() const {
    int n = 0;
    for (const auto& u : upgrades) n += u.max_rank;
    return n;
}

const MetaUpgrade* ProgressionSystem::cheapest_affordable() const {
    const MetaUpgrade* best = nullptr;
    for (const auto& u : upgrades) {
        if (u.maxed() || u.category == MetaCategory::Cosmetic) continue;
        if (u.next_cost() > offerings) continue;
        if (!best || u.next_cost() < best->next_cost()) best = &u;
    }
    return best;
}

const MetaUpgrade* ProgressionSystem::cheapest_pending() const {
    const MetaUpgrade* best = nullptr;
    for (const auto& u : upgrades) {
        if (u.maxed() || u.category == MetaCategory::Cosmetic) continue;
        if (!best || u.next_cost() < best->next_cost()) best = &u;
    }
    return best;
}

void meta_rank_text(const MetaUpgrade& u, int r, char* buf, int n) {
    if (n <= 0) return;
    buf[0] = '\0';
    switch (u.id) {
        case Meta::VIDA:
            if (r <= 0) snprintf(buf, n, "%s", tr("Vida de siempre", "Base health"));
            else        snprintf(buf, n, tr("+%.0f de vida máxima", "+%.0f max health"), meta_hp(r));
            break;
        case Meta::DANO:
            if (r <= 0) snprintf(buf, n, "%s", tr("Daño de siempre", "Base damage"));
            else        snprintf(buf, n, tr("+%.0f de daño por disparo", "+%.0f damage per shot"), meta_damage(r));
            break;
        case Meta::VELOCIDAD:
            if (r <= 0) snprintf(buf, n, "%s", tr("Paso de siempre", "Base speed"));
            else        snprintf(buf, n, tr("+%.0f%% de velocidad", "+%.0f%% speed"), (meta_speed_mult(r) - 1.0f) * 100.0f);
            break;
        case Meta::FE:
            snprintf(buf, n, tr("Empiezas con %.0f de fe", "Start with %.0f faith"), meta_start_faith(r));
            break;
        case Meta::HERENCIA:
            snprintf(buf, n, tr("Eliseo hereda el %.0f%% de la fe", "Elisha inherits %.0f%% of the faith"),
                     meta_inherit_ratio(r) * 100.0f);
            break;
        case Meta::ALTAR:
            if (r <= 0)      snprintf(buf, n, "%s", tr("Solo en los altares", "Only at altars"));
            else if (r == 1) snprintf(buf, n, "%s", tr("1 oración por capítulo", "1 prayer per chapter"));
            else             snprintf(buf, n, tr("%d oraciones por capítulo", "%d prayers per chapter"), meta_altar_prayers(r));
            break;
        case Meta::DOBLE:
            snprintf(buf, n, "%s", r <= 0 ? tr("Eliseo: 2 huecos de milagro", "Elisha: 2 miracle slots")
                                         : tr("Eliseo: 3 huecos de milagro", "Elisha: 3 miracle slots"));
            break;
        case Meta::ESCUELA:
            snprintf(buf, n, "%s", r <= 0 ? tr("Eliseo camina solo", "Elisha walks alone")
                                         : tr("Un discípulo con Eliseo", "A disciple with Elisha"));
            break;
        case Meta::ALIENTO:
            if (r <= 0)      snprintf(buf, n, "%s", tr("Caer es caer", "A fall is a fall"));
            else if (r == 1) snprintf(buf, n, "%s", tr("Te levantas 1 vez por intento", "Rise once per attempt"));
            else             snprintf(buf, n, tr("Te levantas %d veces por intento", "Rise %d times per attempt"), meta_revives(r));
            break;
        case Meta::SUERTES:
            if (r <= 0)      snprintf(buf, n, "%s", tr("Las cartas son las que son", "The cards are what they are"));
            else if (r == 1) snprintf(buf, n, "%s", tr("1 re-sorteo por capítulo", "1 reroll per chapter"));
            else             snprintf(buf, n, tr("%d re-sorteos por capítulo", "%d rerolls per chapter"), meta_rerolls(r));
            break;
        case Meta::PRIMICIAS:
            if (r <= 0)      snprintf(buf, n, "%s", tr("Empiezas con las manos vacías", "You start empty-handed"));
            else if (r == 1) snprintf(buf, n, "%s", tr("Eliges un objeto al empezar", "Choose an item at the start"));
            else             snprintf(buf, n, "%s", tr("Y otro al empezar el Acto II", "And another at the start of Act II"));
            break;
        case Meta::DIEZMO:
            if (r <= 0) snprintf(buf, n, "%s", tr("Ofrendas de siempre", "Base offerings"));
            else        snprintf(buf, n, tr("+%.0f%% de ofrendas", "+%.0f%% offerings"), (meta_offering_mult(r) - 1.0f) * 100.0f);
            break;
        case Meta::ALFORJA:
            snprintf(buf, n, tr("%d huecos en la alforja", "%d satchel slots"), meta_belt_slots(r));
            break;
        case Meta::TINAJA:
            if (r <= 0)      snprintf(buf, n, "%s", tr("La tinaja está vacía", "The jar is empty"));
            else if (r == 1) snprintf(buf, n, "%s", tr("1 Agua del Jordán por capítulo", "1 Water of the Jordan per chapter"));
            else             snprintf(buf, n, tr("%d Aguas del Jordán por capítulo", "%d Waters of the Jordan per chapter"), meta_jar_water(r));
            break;
        default:
            // Desbloqueos de objeto y cosmeticos: un solo rango, si o no
            if (u.unlocks_skin != MetaUpgrade::NO_ITEM)
                snprintf(buf, n, "%s", r <= 0 ? tr("Sin estrenar", "Not yours yet")
                                             : tr("Tuyo: Opciones > Cayado", "Yours: Options > Staff"));
            else
                snprintf(buf, n, "%s", r <= 0 ? tr("Aún no está en juego", "Not in play yet")
                                             : tr("Ya está en juego", "Now in play"));
            break;
    }
}
