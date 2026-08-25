#pragma once

// =============================================================================
// MANTLE - Encuentros biblicos (salas RoomType::Event)
// =============================================================================
//
// Una sala por capitulo con un personaje del relato y una decision con coste.
// Aqui solo vive el TEXTO; el efecto de cada opcion lo aplica
// Engine::apply_event_choice(capitulo, opcion) usando los sistemas existentes
// (fe, vida, danio, slots de milagro, items).
//
// =============================================================================

#include "core/i18n.h"
#include "core/types.h"
#include "game/lore.h"
#include "render/sprites.h"   // namespace Portrait

struct EventOption {
    const char* label;    // Lo que elige el jugador
    const char* cost;     // Etiqueta corta del coste ("-15 HP"), puede ser nullptr
    const char* result;   // Lo que se lee tras elegir
};

struct EventDef {
    const char* speaker;      // Quien habla ("La viuda de Sarepta")
    const char* lines[3];     // Lo que dice, terminado en nullptr
    Scripture   verse;        // Referencia del pasaje
    EventOption options[2];   // options[1].label == nullptr -> una sola opcion
    // Indice en `assets/sprites/portraits.png` (render/sprites.h). Portrait::NONE = sin
    // cara: la caja se dibuja exactamente igual que antes de que existiera este campo.
    // Va el ULTIMO y con valor por defecto a proposito: asi los encuentros que no lo
    // nombran siguen compilando tal cual, sin heredar el retrato 0 por accidente.
    int         portrait = Portrait::NONE;
};

inline EventDef get_event_def(int chapter) {
    switch (chapter) {
        case 1: return {
            tr("Los cuervos de Querit", "The ravens of Cherith"),
            {tr("Al amanecer bajan los cuervos, y traen pan y carne en el pico.", "At dawn the ravens come down, bread and flesh in their beaks."),
             tr("Nadie en Israel come hoy: la sequía que pediste es tuya también.", "No one in Israel eats today: the drought you asked for is yours too."),
             nullptr},
            {"1 Reyes 17:6", nullptr},
            {{tr("Comer lo que traen", "Eat what they bring"), nullptr,
              tr("Comes de la mano de Dios. Las fuerzas vuelven enteras.", "You eat from the hand of God. Your strength comes back whole.")},
             {tr("Ayunar un día más", "Fast one more day"), nullptr,
              tr("El hambre aprieta, pero la confianza crece.", "Hunger bites, but trust grows.")}},
            Portrait::CUERVOS_QUERIT
        };
        case 2: return {
            tr("La viuda de Sarepta", "The widow of Zarephath"),
            {tr("«Vive Jehová tu Dios, que no tengo pan cocido.", "«As the Lord thy God liveth, I have no baked bread."),
             tr("Solo un puñado de harina y un poco de aceite en una vasija.", "Only a handful of meal and a little oil in a cruse."),
             tr("Lo comeremos mi hijo y yo, y nos moriremos».", "My son and I shall eat it, and die».")},
            {"1 Reyes 17:12", nullptr},
            {{tr("Darle tu último pan", "Give her your last bread"), "-15 HP",
              tr("La tinaja no se vació. Ni se vaciará hasta que llueva sobre la tierra.", "The barrel did not empty. Nor will it, until rain falls on the land.")},
             {tr("Seguir tu camino", "Go on your way"), "-5 Fe",
              tr("Sigues andando. El llanto del niño te acompaña un buen trecho.", "You walk on. The child's crying follows you a long while.")}},
            Portrait::VIUDA_SAREPTA
        };
        case 3: return {
            tr("El altar del Carmelo", "The altar on Carmel"),
            {tr("Doce piedras, una por cada tribu. La leña está puesta y el buey encima.", "Twelve stones, one for each tribe. The wood is laid and the bullock upon it."),
             tr("Todo Israel mira. Los profetas de Baal llevan medio día gritando en vano.", "All Israel is watching. Baal's prophets have shouted in vain half a day."),
             tr("«Llenad cuatro cántaros de agua, y derramadlos sobre el holocausto».", "«Fill four barrels with water, and pour it on the burnt sacrifice».")},
            {"1 Reyes 18:33", nullptr},
            {{tr("Verter doce cántaros", "Pour twelve barrels"), "-20 Fe",
              tr("El agua corre y llena la zanja. Cuando caiga el fuego, nadie podrá dudar.", "The water runs and fills the trench. When the fire falls, no one can doubt.")},
             {tr("Encender tú el fuego", "Kindle the fire yourself"), nullptr,
              tr("Prendes la leña con tus manos. Arde rápido, y no dice nada de Dios.", "You light the wood with your hands. It burns fast, and says nothing of God.")}},
            Portrait::ALTAR_CARMELO
        };
        case 4: return {
            tr("La boca de la cueva", "The mouth of the cave"),
            {tr("«Sal fuera, y ponte en el monte delante de Jehová».", "«Go forth, and stand upon the mount before the Lord."),
             tr("Pasa un viento grande que rompe los montes; Jehová no está en el viento.", "A great wind rends the mountains; the Lord is not in the wind."),
             tr("Y tras el viento un terremoto, y tras el terremoto un fuego.", "And after the wind an earthquake, and after the earthquake a fire.")},
            {"1 Reyes 19:11-12", nullptr},
            {{tr("Esperar en silencio", "Wait in silence"), nullptr,
              tr("Tras el fuego, un silbo apacible y delicado. Y cubriste tu rostro con el manto.", "After the fire, a still small voice. And you covered your face with the mantle.")},
             {tr("Salir de la cueva ya", "Leave the cave now"), nullptr,
              tr("Sales al monte. El aire frío calma el cuerpo, pero no el alma.", "You step out to the mountain. The cold air calms the body, not the soul.")}},
            Portrait::BOCA_CUEVA
        };
        case 5: return {
            tr("Eliseo hijo de Safat", "Elisha the son of Shaphat"),
            {tr("Ara con doce yuntas de bueyes, y él está con la última.", "He is plowing with twelve yoke of oxen, and he is with the last of them."),
             tr("Le echas encima el manto, y él lo entiende sin que digas nada.", "You cast the mantle over him, and he understands without a word from you."),
             tr("«Te ruego que me dejes besar a mi padre y a mi madre, y luego te seguiré».", "«Let me, I pray thee, kiss my father and my mother, and then I will follow thee.")},
            {"1 Reyes 19:20", nullptr},
            {{tr("Dejarlo todo atrás", "Leave everything behind"), tr("consumible", "consumable"),
              tr("Mató la yunta, la coció con los aperos y dio de comer al pueblo. Ya no hay vuelta.", "He slew the yoke, boiled it with the gear and fed the people. There is no going back.")},
             {tr("Dejarle despedirse", "Let him say goodbye"), nullptr,
              tr("Vuelve a su casa una última vez. La despedida también es obediencia.", "He goes home one last time. Farewell is obedience too.")}},
            Portrait::ELISEO_LABRADOR
        };
        case 6: return {
            tr("Los hombres de Jericó", "The men of Jericho"),
            {tr("«La situación de esta ciudad es buena, como mi señor ve.", "«The situation of this city is pleasant, as my lord seeth."),
             tr("Mas las aguas son malas, y la tierra estéril».", "But the water is naught, and the ground barren»."),
             tr("Te traen una vasija nueva, y sal dentro de ella.", "They bring you a new cruse, with salt in it.")},
            {"2 Reyes 2:19-20", nullptr},
            {{tr("Echar la sal en el manantial", "Cast the salt into the spring"), "-10 Fe",
              tr("«Yo sané estas aguas». Y fueron sanas hasta el día de hoy.", "«I have healed these waters». And they were healed unto this day.")},
             {tr("Beber del agua igualmente", "Drink the water anyway"), "-10 HP",
              tr("El agua raspa la garganta. Pero la ciudad ve que no la temes.", "The water scrapes your throat. But the city sees you do not fear it.")}},
            Portrait::HOMBRE_JERICO
        };
        case 7: return {
            tr("La mujer del profeta muerto", "The dead prophet's wife"),
            {tr("«Tu siervo mi marido ha muerto, y tú sabes que él temía a Jehová.", "«Thy servant my husband is dead, and thou knowest he feared the Lord."),
             tr("Y ha venido el acreedor para tomarse dos hijos míos por siervos».", "And the creditor is come to take my two sons to be bondmen»."),
             tr("No tiene nada en casa, sino una vasija de aceite.", "She has nothing in the house, save a pot of oil.")},
            {"2 Reyes 4:1", nullptr},
            {{tr("Pedir muchas vasijas", "Ask for many vessels"), "-20 HP",
              tr("El aceite no se detuvo hasta que no quedó vasija vacía en toda la aldea.", "The oil did not stop until there was not an empty vessel left in the whole village.")},
             {tr("Pedir unas pocas", "Ask for a few"), nullptr,
              tr("Llenáis tres vasijas. Alcanza para hoy, y no para la deuda.", "You fill three vessels. Enough for today, not for the debt.")}},
            Portrait::MUJER_PROFETA
        };
        case 8: return {
            tr("La sunamita", "The Shunammite woman"),
            {tr("«He aquí, yo entiendo que este que siempre pasa por nuestra casa", "«Behold now, I perceive that this man which passeth by us continually"),
             tr("es varón santo de Dios.", "is a holy man of God."),
             tr("Hagámosle un pequeño aposento de paredes, con cama, mesa, silla y candelero».", "Let us make a little chamber with walls, and set for him a bed, a table, a stool and a candlestick».")},
            {"2 Reyes 4:9-10", nullptr},
            {{tr("Aceptar el aposento", "Accept the chamber"), tr("-15 HP máx", "-15 max HP"),
              tr("Descansas bajo techo por primera vez en años. Si caes aquí, alguien te levantará.", "You rest under a roof for the first time in years. If you fall here, someone will lift you.")},
             {tr("Rechazar la hospitalidad", "Refuse the hospitality"), nullptr,
              tr("«Yo habito en medio de mi pueblo». Duermes fuera, y duermes en paz.", "«I dwell among mine own people». You sleep outside, and you sleep in peace.")}},
            Portrait::SUNAMITA
        };
        case 9: return {
            tr("Naamán, general de Siria", "Naaman, captain of Syria"),
            {tr("Ha venido con caballos y carros, y se ha parado a tu puerta.", "He has come with horses and chariots, and stopped at your door."),
             tr("«He aquí, yo decía para mí: Saldrá él luego, e invocando el nombre de Jehová", "«Behold, I thought, He will surely come out to me, and call on the name of the Lord"),
             tr("alzará su mano y tocará el lugar, y sanará la lepra».", "and strike his hand over the place, and recover the leper».")},
            {"2 Reyes 5:11", nullptr},
            {{tr("Mandarle al Jordán", "Send him to the Jordan"), "-25 Fe",
              tr("Se zambulló siete veces, y su carne se volvió como la de un niño. Y tú con él.", "He dipped seven times, and his flesh came again like a child's. And yours with his.")},
             {tr("Sanarlo con tu mano", "Heal him with your hand"), nullptr,
              tr("La lepra cede a tu contacto. Naamán te alaba a ti, y no a Dios.", "The leprosy yields to your touch. Naaman praises you, and not God.")}},
            Portrait::NAAMAN
        };
        case 10: return {
            tr("El criado de Eliseo", "Elisha's servant"),
            {tr("«¡Ah, señor mío! ¿Qué haremos?».", "«Alas, my master! How shall we do?»."),
             tr("La ciudad está cercada de gente de a caballo y de carros por todas partes.", "The city is compassed with horses and chariots on every side."),
             tr("Y él tiembla, porque solo ve lo que se ve.", "And he trembles, because he sees only what can be seen.")},
            {"2 Reyes 6:15-17", nullptr},
            {{tr("Orar por sus ojos", "Pray for his eyes"), "-20 Fe",
              tr("«Más son los que están con nosotros». Y el monte se llenó de carros de fuego.", "«They that be with us are more». And the mountain filled with chariots of fire.")},
             {tr("Confiar en tu brazo", "Trust your own arm"), nullptr,
              tr("Afilas el ánimo y el arma. El criado sigue temblando.", "You sharpen your nerve and your weapon. The servant keeps trembling.")}},
            Portrait::CRIADO_ELISEO
        };
        default: return {
            "", {nullptr, nullptr, nullptr}, {"", nullptr},
            {{nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr}}
        };
    }
}
