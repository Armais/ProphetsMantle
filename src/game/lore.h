#pragma once

// =============================================================================
// MANTLE - Lore biblico (1 y 2 Reyes, Reina-Valera 1960)
// =============================================================================
//
// Tabla de datos pura: citas, narracion por capitulo y referencias por objeto,
// milagro y enemigo. Mismo patron que miracles.h / enemies.h: structs planos y
// switch como tabla de consulta, sin asignaciones ni estado.
//
// Todo el texto es UTF-8 (MSVC necesita /utf-8, ver CMakeLists.txt) y solo usa
// codepoints del charset horneado en render/renderer.h (GLYPH_EXTRA).
//
// =============================================================================

#include "core/types.h"
#include "game/miracles.h"
#include "game/enemies.h"

// =============================================================================
// Cita de la Escritura
// =============================================================================
// `ref` NO se muestra en ninguna pantalla: queda como atribucion documental de donde
// sale cada cita. Lo que lee el jugador es siempre `text`, sin numero de capitulo ni
// de versiculo (decision de diseno: el texto biblico se lee como narracion, no como cita).
struct Scripture {
    const char* ref;    // "1 Reyes 17:4" — solo documental
    const char* text;   // Cita (puede ser nullptr: solo referencia)
};

// =============================================================================
// Narracion de un capitulo. `lines` termina en nullptr.
// =============================================================================
struct ChapterLore {
    Scripture   verse;
    const char* lines[4];
    const char* objective;   // Lo que el profeta debe hacer aqui
    const char* outro;       // Se muestra al cruzar el arco, tras el jefe
    const char* boss_line;   // Lo que dice el jefe al aparecer
};

// =============================================================================
// Los diez capitulos
// =============================================================================
inline ChapterLore get_chapter_lore(int chapter) {
    switch (chapter) {
        case 1: return {
            {"1 Reyes 17:4", "Beberás del arroyo; y yo he mandado a los cuervos que te sustenten allí."},
            {"Elías el tisbita dijo a Acab: «Vive Jehová, no habrá lluvia ni rocío en estos años, sino por mi palabra».",
             "Y la palabra de Jehová vino a él: escóndete junto al arroyo de Querit.",
             "Los hombres del rey ya buscan tu rastro por todo Israel.",
             nullptr},
            "Sobrevive junto al arroyo. Los cuervos te sustentarán.",
            "El capitán ha caído. Pero el arroyo se seca: no ha llovido sobre la tierra.",
            "¡El rey te quiere vivo, profeta! ¡Vivo, o en pedazos!"
        };
        case 2: return {
            {"1 Reyes 17:14", "La harina de la tinaja no escaseará, ni el aceite de la vasija disminuirá."},
            {"«Levántate, vete a Sarepta de Sidón, y mora allí».",
             "Sarepta es tierra de Jezabel: aquí Baal es el único nombre que se pronuncia.",
             "Una viuda recoge dos leños para cocer su último puñado de harina.",
             nullptr},
            "Halla a la viuda. Que la tinaja no se acabe.",
            "El niño ha vuelto a la vida. «Ahora conozco que tú eres varón de Dios».",
            "¡Sarepta es de Baal! ¡Y tú serás su ofrenda!"
        };
        case 3: return {
            {"1 Reyes 18:21", "¿Hasta cuándo claudicaréis vosotros entre dos pensamientos?"},
            {"Tres años sin lluvia. Israel entero sube al monte Carmelo.",
             "Cuatrocientos cincuenta profetas de Baal contra un solo hombre.",
             "Dos altares, dos bueyes, y ningún fuego encendido por mano humana.",
             nullptr},
            "Que el pueblo vea de quién es el fuego.",
            "«¡Jehová es el Dios!». Y el cielo se oscureció, y hubo una gran lluvia.",
            "¡Baal, respóndenos! ¡Respóndenos! (Y se sajaban con cuchillos.)"
        };
        case 4: return {
            {"1 Reyes 19:12", "Y tras el fuego un silbo apacible y delicado."},
            {"Jezabel juró: «Así me hagan los dioses si mañana no he puesto tu alma como la de uno de ellos».",
             "Elías huyó al desierto, se sentó bajo un enebro y pidió morirse.",
             "Cuarenta días y cuarenta noches hasta Horeb, el monte de Dios.",
             nullptr},
            "«¿Qué haces aquí, Elías?»",
            "No en el viento, ni en el terremoto, ni en el fuego. Siete mil rodillas no se doblaron ante Baal.",
            "Basta ya, Jehová; quítame la vida, porque no soy yo mejor que mis padres."
        };
        case 5: return {
            {"1 Reyes 19:19", "Y pasando Elías por delante de él, echó sobre él su manto."},
            {"Eliseo araba con doce yuntas de bueyes cuando el manto cayó sobre sus hombros.",
             "Y Acab codició la viña de Nabot, y Jezabel hizo que lo apedrearan.",
             "«¿Has matado, y también has despojado?»",
             nullptr},
            "Enfrenta al rey y a la reina en el campo que robaron.",
            "«Donde lamieron los perros la sangre de Nabot, lamerán también tu sangre».",
            "¿Me has hallado, enemigo mío?"
        };
        case 6: return {
            {"2 Reyes 2:21", "Yo sané estas aguas, y no habrá más en ellas muerte ni enfermedad."},
            {"«La situación de esta ciudad es buena, pero las aguas son malas y la tierra estéril».",
             "Eliseo pidió una vasija nueva, y sal dentro de ella.",
             "La corrupción brota del manantial y se extiende por las calles de Jericó.",
             nullptr},
            "Sana el manantial de Jericó.",
            "Y fueron sanas las aguas hasta el día de hoy.",
            "Esta agua es mía. Bebe de ella."
        };
        case 7: return {
            {"2 Reyes 4:3", "Pide para ti vasijas prestadas de todos tus vecinos; y no pidas pocas."},
            {"«Mi marido ha muerto, y el acreedor ha venido para llevarse a mis dos hijos por siervos».",
             "«¿Qué tienes en casa?». «Ninguna cosa, sino una vasija de aceite».",
             "El acreedor recorre la aldea cobrando en carne.",
             nullptr},
            "Reúne vasijas. Que el aceite no se detenga.",
            "«Vende el aceite y paga tus deudas; y tú y tus hijos vivid de lo que quede».",
            "La deuda se paga con hijos o con sangre."
        };
        case 8: return {
            {"2 Reyes 4:34", "Y se tendió sobre el niño, y el cuerpo del niño entró en calor."},
            {"Una mujer importante de Sunem le hizo un aposento con cama, mesa, silla y candelero.",
             "El hijo que Eliseo le prometió cayó diciendo: «¡Ay, mi cabeza!».",
             "Y murió sobre las rodillas de su madre al mediodía.",
             nullptr},
            "«Déjala, porque su alma está en amargura».",
            "El niño estornudó siete veces, y abrió sus ojos.",
            "Todo lo que respira es mío. También el hijo prometido."
        };
        case 9: return {
            {"2 Reyes 5:10", "Ve y lávate siete veces en el Jordán, y serás limpio."},
            {"Naamán, general del rey de Siria, era varón valeroso; pero leproso.",
             "Vino con caballos y carros, con plata, oro y vestidos, esperando un espectáculo.",
             "«¿Abana y Farfar, ríos de Damasco, no son mejores que las aguas de Israel?»",
             nullptr},
            "Que el orgulloso se moje siete veces.",
            "Su carne se volvió como la de un niño. Pero Guiezi corrió tras la plata.",
            "La lepra de Naamán se te pegará a ti y a tu descendencia para siempre."
        };
        case 10: return {
            {"2 Reyes 6:16", "No tengas miedo, porque más son los que están con nosotros que los que están con ellos."},
            {"El rey de Siria se enfureció: alguien revelaba cada uno de sus planes.",
             "«Eliseo declara al rey de Israel las palabras que tú hablas en tu cámara».",
             "Y de noche cercaron Dotán con gente, con carros y un gran ejército.",
             nullptr},
            "«Te ruego, Jehová, que abras sus ojos para que vea».",
            "El monte estaba lleno de gente de a caballo y de carros de fuego alrededor de Eliseo.",
            "Toda Siria contra un solo hombre. Ríndete, profeta."
        };
        default: return {
            {"", nullptr}, {nullptr, nullptr, nullptr, nullptr}, "", "", ""
        };
    }
}

// =============================================================================
// Prologo / Ascension / Epilogo (paginas de la pantalla de historia)
// =============================================================================
inline ChapterLore get_prologue_lore() {
    return {
        {"1 Reyes 16:33", "Acab hizo más para provocar la ira de Jehová que todos los reyes de Israel que antes de él habían sido."},
        {"Israel ha olvidado a su Dios.",
         "Acab reina en Samaria, y Jezabel su reina ha llenado la tierra de altares a Baal.",
         "Los profetas de Jehová son perseguidos, y los que quedan se esconden en cuevas.",
         "Solo un hombre de Tisbe de Galaad se atreve a estar delante del rey."},
        "", "", ""
    };
}

// Tres paginas: la peticion, el torbellino, el manto.
inline ChapterLore get_ascension_lore(int page) {
    switch (page) {
        case 0: return {
            {"2 Reyes 2:9", "Pide lo que quieras que haga por ti, antes que yo sea quitado de ti."},
            {"Elías y Eliseo cruzaron el Jordán en seco, golpeando las aguas con el manto.",
             "«Te ruego que una doble porción de tu espíritu sea sobre mí».",
             "«Cosa difícil has pedido. Si me vieres cuando fuere quitado de ti, te será hecho».",
             nullptr},
            "", "", ""
        };
        case 1: return {
            {"2 Reyes 2:11", "Un carro de fuego con caballos de fuego apartó a los dos; y Elías subió al cielo en un torbellino."},
            {"Y aconteció que yendo ellos y hablando, he aquí un carro de fuego.",
             "Y Eliseo lo veía, y clamaba: «¡Padre mío, padre mío, carro de Israel y su gente de a caballo!».",
             "Y nunca más le vio.",
             nullptr},
            "", "", ""
        };
        default: return {
            {"2 Reyes 2:14", "¿Dónde está Jehová, el Dios de Elías? Y golpeó las aguas, y se apartaron."},
            {"Eliseo alzó el manto que se le había caído a Elías, y volvió al Jordán.",
             "Golpeó las aguas con él, y se apartaron a un lado y a otro, y pasó.",
             "El manto no era suyo. Le fue prestado.",
             nullptr},
            "", "", ""
        };
    }
}

inline ChapterLore get_epilogue_lore() {
    return {
        {"2 Reyes 13:21", "Cuando llegó a tocar el muerto los huesos de Eliseo, revivió, y se levantó sobre sus pies."},
        {"Elías nunca murió: subió en un torbellino, y su manto quedó en la tierra.",
         "Eliseo murió, y lo sepultaron. Y aun sus huesos daban vida.",
         "Israel volvió a olvidar. Y volvió a haber profetas.",
         "El manto sigue cayendo sobre hombros nuevos."},
        "", "", ""
    };
}

// =============================================================================
// Referencias por objeto / milagro / enemigo
// =============================================================================
inline Scripture get_item_scripture(u32 item_id) {
    switch (item_id) {
        case 0:  return {"1 Samuel 17:40", "Tomó cinco piedras lisas del arroyo, y su honda en su mano."};
        case 1:  return {"Números 17:8",   "La vara de Aarón había reverdecido, y echado flores y almendras."};
        case 2:  return {"1 Reyes 19:6",   "A su cabecera una torta cocida sobre las ascuas, y una vasija de agua."};
        case 3:  return {"1 Reyes 19:16",  "Y ungirás a Eliseo hijo de Safat, para que sea profeta en tu lugar."};
        case 4:  return {"Éxodo 12:11",    "Vuestros lomos ceñidos, vuestro calzado en vuestros pies."};
        case 5:  return {"Éxodo 16:31",    "Era como semilla de culantro, blanco, y su sabor como de hojuelas con miel."};
        case 6:  return {"Éxodo 31:18",    "Dos tablas del testimonio, escritas con el dedo de Dios."};
        case 7:  return {"Éxodo 25:10",    "Harán también un arca de madera de acacia."};
        case 8:  return {"Salmo 104:15",   "El vino que alegra el corazón del hombre."};
        case 9:  return {"2 Reyes 5:14",   "Se zambulló siete veces en el Jordán, y su carne se volvió como la de un niño."};
        case 10: return {"1 Reyes 18:38",  "Entonces cayó fuego de Jehová, y consumió el holocausto."};
        case 11: return {"Josué 4:7",      "Estas piedras servirán de memorial a los hijos de Israel para siempre."};
        case 12: return {"1 Reyes 18:38",  "Consumió la leña, las piedras y el polvo, y aun lamió el agua de la zanja."};
        case 13: return {"1 Reyes 17:16",  "La harina de la tinaja no escaseó, ni el aceite de la vasija menguó."};
        case 14: return {"2 Reyes 4:4",    "Echarás aceite en todas las vasijas, y pondrás aparte la que esté llena."};
        default: return {"", nullptr};
    }
}

inline Scripture get_miracle_scripture(MiracleID id) {
    switch (id) {
        case MiracleID::CuervosDeQuerit:   return {"1 Reyes 17:6",  "Los cuervos le traían pan y carne por la mañana y por la tarde."};
        case MiracleID::PalabraDeSequia:   return {"1 Reyes 17:1",  "No habrá lluvia ni rocío en estos años, sino por mi palabra."};
        case MiracleID::FuegoDelCielo:     return {"1 Reyes 18:38", "Entonces cayó fuego de Jehová, y consumió el holocausto."};
        case MiracleID::CarroDeFuego:      return {"2 Reyes 2:11",  "Un carro de fuego con caballos de fuego apartó a los dos."};
        case MiracleID::AltarConsumido:    return {"1 Reyes 18:38", "Consumió la leña, las piedras y el polvo."};
        case MiracleID::SanarAguas:        return {"2 Reyes 2:21",  "Yo sané estas aguas, y no habrá más en ellas muerte."};
        case MiracleID::AceiteInagotable:  return {"2 Reyes 4:6",   "Cuando las vasijas fueron llenas, el aceite se detuvo."};
        case MiracleID::OjosAbiertos:      return {"2 Reyes 6:17",  "Jehová abrió los ojos del criado, y vio el monte lleno de carros de fuego."};
        case MiracleID::EjercitoCelestial: return {"2 Reyes 6:17",  "El monte estaba lleno de gente de a caballo y de carros de fuego."};
        case MiracleID::DoblePorcion:      return {"2 Reyes 2:9",   "Te ruego que una doble porción de tu espíritu sea sobre mí."};
        default:                           return {"", nullptr};
    }
}

// Entrada de bestiario: referencia y una linea que explica de donde sale el enemigo.
inline Scripture get_enemy_lore(EnemyType type) {
    switch (type) {
        case EnemyType::SoldadoDeAcab:
            return {"1 Reyes 18:10", "Acab hizo jurar a las naciones que no te habían hallado."};
        case EnemyType::ArqueroReal:
            return {"1 Reyes 22:34", "Un hombre disparó su arco a la ventura, e hirió al rey de Israel."};
        case EnemyType::ProfetaDeBaal:
            return {"1 Reyes 18:28", "Clamaban a grandes voces, y se sajaban con cuchillos hasta chorrear la sangre."};
        case EnemyType::LeonDelDesierto:
            return {"1 Reyes 13:24", "Y encontrándole un león en el camino, le mató."};
        case EnemyType::SoldadoSirio:
            return {"2 Reyes 6:14",  "Envió allá gente de a caballo, y carros, y un gran ejército."};
        case EnemyType::GeneralSirio:
            return {"2 Reyes 5:1",   "Naamán era varón grande delante de su señor; pero leproso."};
        case EnemyType::IdolatraDeRimon:
            return {"2 Reyes 5:18",  "Cuando mi señor entra en el templo de Rimón para adorar en él."};
        case EnemyType::AguaCorrupta:
            return {"2 Reyes 2:19",  "Las aguas son malas, y la tierra es estéril."};
        default:
            return {"", nullptr};
    }
}
