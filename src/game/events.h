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

#include "core/types.h"
#include "game/lore.h"

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
};

inline EventDef get_event_def(int chapter) {
    switch (chapter) {
        case 1: return {
            "Los cuervos de Querit",
            {"Al amanecer bajan los cuervos, y traen pan y carne en el pico.",
             "Nadie en Israel come hoy: la sequía que pediste es tuya también.",
             nullptr},
            {"1 Reyes 17:6", nullptr},
            {{"Comer lo que traen", nullptr,
              "Comes de la mano de Dios. Las fuerzas vuelven enteras."},
             {"Ayunar un día más", nullptr,
              "El hambre aprieta, pero la confianza crece."}}
        };
        case 2: return {
            "La viuda de Sarepta",
            {"«Vive Jehová tu Dios, que no tengo pan cocido.",
             "Solo un puñado de harina y un poco de aceite en una vasija.",
             "Lo comeremos mi hijo y yo, y nos moriremos»."},
            {"1 Reyes 17:12", nullptr},
            {{"Darle tu último pan", "-15 HP",
              "La tinaja no se vació. Ni se vaciará hasta que llueva sobre la tierra."},
             {"Seguir tu camino", "-5 Fe",
              "Sigues andando. El llanto del niño te acompaña un buen trecho."}}
        };
        case 3: return {
            "El altar del Carmelo",
            {"Doce piedras, una por cada tribu. La leña está puesta y el buey encima.",
             "Todo Israel mira. Los profetas de Baal llevan medio día gritando en vano.",
             "«Llenad cuatro cántaros de agua, y derramadlos sobre el holocausto»."},
            {"1 Reyes 18:33", nullptr},
            {{"Verter doce cántaros", "-20 Fe",
              "El agua corre y llena la zanja. Cuando caiga el fuego, nadie podrá dudar."},
             {"Encender tú el fuego", nullptr,
              "Prendes la leña con tus manos. Arde rápido, y no dice nada de Dios."}}
        };
        case 4: return {
            "La boca de la cueva",
            {"«Sal fuera, y ponte en el monte delante de Jehová».",
             "Pasa un viento grande que rompe los montes; Jehová no está en el viento.",
             "Y tras el viento un terremoto, y tras el terremoto un fuego."},
            {"1 Reyes 19:11-12", nullptr},
            {{"Esperar en silencio", nullptr,
              "Tras el fuego, un silbo apacible y delicado. Y cubriste tu rostro con el manto."},
             {"Salir de la cueva ya", nullptr,
              "Sales al monte. El aire frío calma el cuerpo, pero no el alma."}}
        };
        case 5: return {
            "Eliseo hijo de Safat",
            {"Ara con doce yuntas de bueyes, y él está con la última.",
             "Le echas encima el manto, y él lo entiende sin que digas nada.",
             "«Te ruego que me dejes besar a mi padre y a mi madre, y luego te seguiré»."},
            {"1 Reyes 19:20", nullptr},
            {{"Dejarlo todo atrás", "consumible",
              "Mató la yunta, la coció con los aperos y dio de comer al pueblo. Ya no hay vuelta."},
             {"Dejarle despedirse", nullptr,
              "Vuelve a su casa una última vez. La despedida también es obediencia."}}
        };
        case 6: return {
            "Los hombres de Jericó",
            {"«La situación de esta ciudad es buena, como mi señor ve.",
             "Mas las aguas son malas, y la tierra estéril».",
             "Te traen una vasija nueva, y sal dentro de ella."},
            {"2 Reyes 2:19-20", nullptr},
            {{"Echar la sal en el manantial", "-10 Fe",
              "«Yo sané estas aguas». Y fueron sanas hasta el día de hoy."},
             {"Beber del agua igualmente", "-10 HP",
              "El agua raspa la garganta. Pero la ciudad ve que no la temes."}}
        };
        case 7: return {
            "La mujer del profeta muerto",
            {"«Tu siervo mi marido ha muerto, y tú sabes que él temía a Jehová.",
             "Y ha venido el acreedor para tomarse dos hijos míos por siervos».",
             "No tiene nada en casa, sino una vasija de aceite."},
            {"2 Reyes 4:1", nullptr},
            {{"Pedir muchas vasijas", "-20 HP",
              "El aceite no se detuvo hasta que no quedó vasija vacía en toda la aldea."},
             {"Pedir unas pocas", nullptr,
              "Llenáis tres vasijas. Alcanza para hoy, y no para la deuda."}}
        };
        case 8: return {
            "La sunamita",
            {"«He aquí, yo entiendo que este que siempre pasa por nuestra casa",
             "es varón santo de Dios.",
             "Hagámosle un pequeño aposento de paredes, con cama, mesa, silla y candelero»."},
            {"2 Reyes 4:9-10", nullptr},
            {{"Aceptar el aposento", "-15 HP máx",
              "Descansas bajo techo por primera vez en años. Si caes aquí, alguien te levantará."},
             {"Rechazar la hospitalidad", nullptr,
              "«Yo habito en medio de mi pueblo». Duermes fuera, y duermes en paz."}}
        };
        case 9: return {
            "Naamán, general de Siria",
            {"Ha venido con caballos y carros, y se ha parado a tu puerta.",
             "«He aquí, yo decía para mí: Saldrá él luego, e invocando el nombre de Jehová",
             "alzará su mano y tocará el lugar, y sanará la lepra»."},
            {"2 Reyes 5:11", nullptr},
            {{"Mandarle al Jordán", "-25 Fe",
              "Se zambulló siete veces, y su carne se volvió como la de un niño. Y tú con él."},
             {"Sanarlo con tu mano", nullptr,
              "La lepra cede a tu contacto. Naamán te alaba a ti, y no a Dios."}}
        };
        case 10: return {
            "El criado de Eliseo",
            {"«¡Ah, señor mío! ¿Qué haremos?».",
             "La ciudad está cercada de gente de a caballo y de carros por todas partes.",
             "Y él tiembla, porque solo ve lo que se ve."},
            {"2 Reyes 6:15-17", nullptr},
            {{"Orar por sus ojos", "-20 Fe",
              "«Más son los que están con nosotros». Y el monte se llenó de carros de fuego."},
             {"Confiar en tu brazo", nullptr,
              "Afilas el ánimo y el arma. El criado sigue temblando."}}
        };
        default: return {
            "", {nullptr, nullptr, nullptr}, {"", nullptr},
            {{nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr}}
        };
    }
}
