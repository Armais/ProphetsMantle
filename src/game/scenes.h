#pragma once

// =============================================================================
// MANTLE - Escenas guionizadas: la historia contada con el motor del gameplay
// =============================================================================
//
// Una escena es lo que estreno la audiencia con Acab (2026-09-09) y ahora tiene todo el
// juego (2026-09-10): un ESCENARIO montado con las piezas del gameplay (una sala del bioma,
// figuras de pie —`spawn_figure` / `spawn_npc_figure`—, el profeta delante, HUD apagado) y
// una CADENA de cajas de dialogo sin decision que se recorre en orden. ESC la salta entera.
//
// Hay dos clases, y las dos las lanza el Engine sin bit en el guardado (criterio del sitio
// de llamada, como las intros):
//
//   - APERTURA de capitulo (`Scene::opening_of`): se juega ANTES de la intro cinematica, en
//     la primera entrada natural (nueva partida, pasar de capitulo, el Ascenso). Cuenta lo
//     que en 1-2 Reyes pasa justo antes de lo que el capitulo juega, sin repetir lo que ya
//     narra la intro ni lo que el pozo de encuentros (events.h) juega dentro del capitulo.
//   - DIALOGO CON EL JEFE (`Scene::boss_of`): al entrar en la arena, con el jefe ya de pie
//     delante (escenario `Live`: no se monta nada, se congela lo que hay). Una vez por
//     partida y por capitulo (`Engine::boss_talk_seen`): al morir y reintentar no se repite.
//
// El TEXTO vive aqui y NO en `namespace Ev`: `SaveData::codex_events` es una mascara de 64
// bits y estas ~90 cajas no son relatos que se descubren, sino historia obligatoria. Viajan
// en `DialogueState::event_id` desplazadas `Sc::LINE_BASE`, y `get_dialogue_def` es quien
// las resuelve — `get_event_def` sigue siendo solo el pozo.
//
// Fidelidad: lo que es cita va con su referencia en `verse` (documental, no se pinta); lo
// inventado se apoya en el pasaje y se marca en el comentario de cada escena. Regla del
// 2026-08-30: Elias o Eliseo estan SIEMPRE en la escena — es el profeta quien esta de pie
// en el escenario. Los reyes que no tienen retrato propio (Joram, Josafat) llevan
// `Portrait::NONE`: usar la cara de Acab para su hijo seria mentir.
//
// Techo de texto: ~115 caracteres por pagina con retrato (4 lineas a 244 u), en ES y en EN.
// `scripts/escenas_check.txt` recorre las 20 escenas en los dos idiomas: cero TEXT TRUNCATED.
//
// =============================================================================

#include "core/i18n.h"
#include "core/types.h"
#include "game/events.h"
#include "game/rooms.h"

// =============================================================================
// Escenario
// =============================================================================
enum class StageKind : u8 {
    Live,       // la sala que hay (la arena del jefe): no se monta nada, se congela
    Interior,   // interior de silleria del bioma, una pantalla, camara clavada
    Exterior    // sala exterior del bioma con un claro en el centro para la escena
};

enum class SceneThen : u8 {
    ChapterIntro,   // al cerrar: intro cinematica del capitulo (o la carta), y a jugar
    Play            // al cerrar: a jugar directamente (los dialogos con el jefe)
};

// Quien esta de pie en el escenario, aparte del profeta.
enum class FigureKind : u8 {
    None,
    Acab,      // hoja acab.png
    Jezabel,   // hoja jezabel.png
    Enemy,     // hoja de un EnemyType (`index`): soldados, profetas de Baal, sirios...
    Npc        // celda de npcs.png (`index` = namespace Npc): la viuda, Guiezi, Abdias...
};

struct SceneFigure {
    FigureKind kind = FigureKind::None;
    int        index = 0;
    int        dx = 0, dy = 0;    // tiles desde el ancla del escenario
    Direction  facing = Direction::Down;
};

struct SceneDef {
    const char* tag;        // prefijo de las lineas de log ("Audiencia", "Apertura 3"...)
    const char* name;       // primera linea del log al abrirse
    int         chapter;    // capitulo al que pertenece (start_chapter previo bajo autoplay)
    StageKind   stage;
    int         stage_chapter;   // bioma del escenario; 0 = el del propio capitulo
    int         player_dx, player_dy;
    Direction   player_facing;
    SceneFigure figures[3];
    int         first_line, count;   // cadena: `Sc::` first_line .. first_line + count - 1
    SceneThen   then;
};

// =============================================================================
// Ids de escena
// =============================================================================
namespace Scene {
enum : int {
    AUDIENCIA = 0,                                                   // cap 1 (apertura)
    APERTURA_2, APERTURA_3, APERTURA_4, APERTURA_5,                  // Acto I
    APERTURA_6, APERTURA_7, APERTURA_8, APERTURA_9, APERTURA_10,     // Acto II
    JEFE_1, JEFE_2, JEFE_3, JEFE_4, JEFE_5,
    JEFE_6, JEFE_7, JEFE_8, JEFE_9, JEFE_10,
    COUNT
};
inline int opening_of(int chapter) {
    if (chapter == 1) return AUDIENCIA;
    if (chapter >= 2 && chapter <= 10) return APERTURA_2 + (chapter - 2);
    return -1;
}
inline int boss_of(int chapter) {
    return (chapter >= 1 && chapter <= 10) ? JEFE_1 + (chapter - 1) : -1;
}
}

// =============================================================================
// Ids de las cajas (lineas) de escena
// =============================================================================
namespace Sc {
constexpr int LINE_BASE = 1000;   // desplazamiento en DialogueState::event_id
enum : int {
    // --- La audiencia con Acab (1 Reyes 17:1-4), apertura del cap. 1 ---------------
    AUD_REPRENDE = 0, AUD_ACAB_DESPRECIA, AUD_JEZABEL_AMENAZA, AUD_SENTENCIA,
    AUD_ACAB_BURLA, AUD_JEZABEL_CAZA, AUD_PALABRA_QUERIT,
    // --- Aperturas 2..10 -----------------------------------------------------------
    AP2_PALABRA, AP2_ELIAS_AGUA, AP2_VIUDA_LENA, AP2_ELIAS_PAN, AP2_SACERDOTE,
    AP3_ACAB_TURBAS, AP3_ELIAS_TURBAS, AP3_PROFETA_BAAL, AP3_ACAB_SEA, AP3_ELIAS_FUEGO,
    AP4_MENSAJERO, AP4_ELIAS_MIEDO, AP4_CRIADO, AP4_ELIAS_HUYE,
    AP5_ELISEO_YUNTAS, AP5_ELIAS_HOREB, AP5_ELISEO_NABOT, AP5_ELIAS_HEREDAD,
    AP6_HIJOS_ESPIRITU, AP6_ELISEO_MANTO, AP6_HIJOS_JERICO, AP6_ELISEO_VAMOS,
    AP7_JORAM, AP7_JOSAFAT, AP7_ELISEO_QUE, AP7_JORAM_NO, AP7_ELISEO_TANEDOR,
    AP8_ELISEO_LLAMALA, AP8_SUNAMITA_PUEBLO, AP8_GUIEZI_HIJO, AP8_ELISEO_HIJO, AP8_SUNAMITA_BURLA,
    AP9_JORAM_CARTA, AP9_GUIEZI_PLATA, AP9_ELISEO_PROFETA, AP9_GUIEZI_ORO, AP9_ELISEO_NO_VENDE,
    AP10_ELISEO_AVISO, AP10_JORAM_COMO, AP10_ELISEO_QUIEN, AP10_CRIADO_JURADO, AP10_ELISEO_DOTAN,
    // --- Dialogos con el jefe 1..10 ------------------------------------------------
    J1_CAPITAN, J1_ELIAS, J1_CAPITAN_2,
    J2_SACERDOTE, J2_ELIAS, J2_SACERDOTE_2,
    J3_PROFETAS, J3_ELIAS_BURLA, J3_PROFETAS_2, J3_ELIAS_FIN,
    J4_DESESPERACION, J4_ELIAS, J4_DESESPERACION_2, J4_ELIAS_2,
    J5_ACAB, J5_ELIAS, J5_JEZABEL, J5_ELIAS_2,
    J6_ESPIRITU, J6_ELISEO, J6_ESPIRITU_2, J6_ELISEO_2,
    J7_ACREEDOR, J7_ELISEO, J7_ACREEDOR_2, J7_ELISEO_2,
    J8_MUERTE, J8_ELISEO, J8_MUERTE_2, J8_ELISEO_2,
    J9_GUIEZI, J9_ELISEO, J9_GUIEZI_2, J9_ELISEO_2,
    J10_BENADAD, J10_ELISEO, J10_BENADAD_2, J10_ELISEO_2,
    COUNT
};
}

// =============================================================================
// Las cajas
// =============================================================================
// Mismo struct que el pozo (EventDef) para que la caja de dialogo no sepa de donde viene el
// texto. `options[0].label == nullptr` es lo que la hace cerrar sola, sin decision.
#define SC_NO_OPTIONS {{nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr}}

inline EventDef get_scene_line(int line) {
    // Retratos de los jefes que son un EnemyType (las ocho primeras celdas del Bestiario)
    constexpr int P_PROFETA_BAAL = static_cast<int>(EnemyType::ProfetaDeBaal);
    constexpr int P_SOLDADO_SIRIO = static_cast<int>(EnemyType::SoldadoSirio);
    constexpr int P_AGUA_CORRUPTA = static_cast<int>(EnemyType::AguaCorrupta);
    switch (line) {

    // =========================================================================
    // La audiencia con Acab (1 R 17:1-4): siete cajas en orden. En 17:1 Acab no
    // responde; aqui SI responde, y Jezabel con el, por decision del usuario
    // (2026-09-09): el jugador tiene que ver a quien se enfrenta el profeta antes de
    // huir. Lo inventado se apoya en el libro: el altar y el bosque de Baal y la sidonia
    // Jezabel (16:30-33), Jezabel que ya mata profetas (18:4), los 450 de Baal (18:19),
    // Acab que lo busca por todos los reinos (18:10) y el manto de pelo (2 R 1:8). La
    // sentencia de 17:1 es la del prologo, tal cual, en la cuarta caja.
    // =========================================================================
    case Sc::AUD_REPRENDE: return {
        tr("Elías", "Elijah"),
        {tr("Acab, hijo de Omri. He cruzado el Jordán a pie para decirte lo que tú ya sabes.",
            "Ahab, son of Omri. I have crossed the Jordan on foot to tell you what you already know."),
         tr("Has hecho lo malo ante el Señor más que todos los reyes que hubo antes de ti.",
            "You have done evil in the sight of the Lord, more than all the kings who came before you."),
         tr("Levantaste altar a Baal en Samaria y plantaste su bosque. Y los altares del Señor están en ruinas.",
            "You raised an altar to Baal in Samaria and planted his grove. And the altars of the Lord lie in ruins.")},
        {"1 Reyes 16:30-33", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AUD_ACAB_DESPRECIA: return {
        tr("Acab", "Ahab"),
        {tr("¿Quién ha dejado pasar a este? Un tisbita de Galaad, manto de pelo y polvo del camino, viene a enseñar al rey.",
            "Who let this one in? A Tishbite of Gilead, in a hairy mantle and the dust of the road, come to lecture the king."),
         tr("Samaria tiene paz, tiene grano y tiene murallas nuevas. Cuando tu Dios haga otro tanto, te escucharé.",
            "Samaria has peace, it has grain, it has new walls. When your God does as much, I will listen."),
         tr("Sidón nos manda cedro y púrpura, y Baal, la lluvia. ¿Qué me manda el Dios de Galaad? A ti.",
            "Sidon sends us cedar and purple, and Baal sends the rain. What does the God of Gilead send me? You.")},
        {"1 Reyes 16:31", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AUD_JEZABEL_AMENAZA: return {
        tr("Jezabel", "Jezebel"),
        {tr("Déjalo, mi señor. En Sidón sabemos qué hacer con los que hablan contra los dioses.",
            "Let him be, my lord. In Sidon we know what to do with those who speak against the gods."),
         tr("Los profetas de tu Señor ya han aprendido a callar, Elías. Vuelve a tu monte antes de que aprendas tú.",
            "Your Lord's prophets have already learned silence, Elijah. Go back to your mountain before you learn it too."),
         nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::AUD_SENTENCIA: return {
        tr("Elías", "Elijah"),
        {tr("No es a mí a quien despreciáis, sino a Aquel delante de quien estoy. Israel lo olvidó; Él no ha olvidado a Israel.",
            "It is not me you despise, but Him before whom I stand. Israel forgot Him; He has not forgotten Israel."),
         tr("Dices que Baal da la lluvia. Pues que la dé. Que Israel entero vea de quién es el cielo.",
            "You say Baal gives the rain. Then let him give it. Let all Israel see whose sky it is."),
         tr("Vive el Señor, Dios de Israel, delante de quien estoy: no habrá rocío ni lluvia en estos años, sino por mi palabra.",
            "As the Lord God of Israel liveth, before whom I stand: there shall not be dew nor rain these years, but according to my word.")},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AUD_ACAB_BURLA: return {
        tr("Acab", "Ahab"),
        {tr("¿Sequía? ¿Por tu palabra? Baal tiene cuatrocientos cincuenta profetas en Israel. Tú eres uno solo.",
            "Drought? By your word? Baal has four hundred and fifty prophets in Israel. You are one man."),
         tr("Vete. Y cuando llueva, que lloverá, mis hombres te traerán de vuelta para que lo veas de rodillas.",
            "Go. And when the rain comes, and it will, my men will bring you back to watch it on your knees."),
         nullptr},
        {"1 Reyes 18:19", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AUD_JEZABEL_CAZA: return {
        tr("Jezabel", "Jezebel"),
        {tr("No hará falta esperar a la lluvia, mi señor. Mis jinetes saben buscar, y Galaad no es tan grande.",
            "No need to wait for the rain, my lord. My riders know how to search, and Gilead is not so large."),
         tr("Sidón no olvida una cara, profeta. Y Samaria ya ha aprendido de Sidón.",
            "Sidon does not forget a face, prophet. And Samaria has learned from Sidon."),
         nullptr},
        {"1 Reyes 18:10", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::AUD_PALABRA_QUERIT: return {
        tr("La palabra del Señor", "The word of the Lord"),
        {tr("Apártate de aquí, y vuélvete al oriente, y escóndete en el arroyo de Querit, que está frente al Jordán.",
            "Get thee hence, and turn thee eastward, and hide thyself by the brook Cherith, that is before Jordan."),
         tr("Beberás del arroyo, y he mandado a los cuervos que allí te sustenten.",
            "Thou shalt drink of the brook, and I have commanded the ravens to feed thee there."),
         nullptr},
        {"1 Reyes 17:2-4", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };

    // =========================================================================
    // Cap. 2 - A la puerta de Sarepta (1 R 17:8-11). Termina en 17:11: el ultimo punado
    // de harina (17:12) y la tinaja son del pozo. El sacerdote que mira desde la puerta
    // es invencion apoyada en «Sarepta es tierra de Jezabel» y en 18:4.
    // =========================================================================
    case Sc::AP2_PALABRA: return {
        tr("La palabra del Señor", "The word of the Lord"),
        {tr("Levántate, vete a Sarepta de Sidón, y mora allí.",
            "Arise, get thee to Zarephath, which belongeth to Zidon, and dwell there."),
         tr("He aquí yo he dado orden allí a una mujer viuda que te sustente.",
            "Behold, I have commanded a widow woman there to sustain thee."),
         nullptr},
        {"1 Reyes 17:8-9", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP2_ELIAS_AGUA: return {
        tr("Elías", "Elijah"),
        {tr("El Señor me trae a la tierra de Jezabel. Y a la puerta de la ciudad, una viuda recogiendo dos leños.",
            "The Lord brings me to Jezebel's own land. And at the city gate, a widow gathering two sticks."),
         tr("Te ruego que me traigas un poco de agua en un vaso, para que beba.",
            "Fetch me, I pray thee, a little water in a vessel, that I may drink."),
         nullptr},
        {"1 Reyes 17:10", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP2_VIUDA_LENA: return {
        tr("La viuda de Sarepta", "The widow of Zarephath"),
        {tr("¿Agua para un forastero? No ha llovido en Sidón en dos años. Baal calla, y el pozo está a medio secar.",
            "Water, for a stranger? No rain has fallen on Sidon in two years. Baal is silent, and the well is half dry."),
         tr("Pero se te ve más seco que el pozo. Espera aquí; te la traigo.",
            "But you look drier than the well. Wait here; I will bring it."),
         nullptr},
        {"1 Reyes 17:11", nullptr}, SC_NO_OPTIONS, Portrait::VIUDA_SAREPTA
    };
    case Sc::AP2_ELIAS_PAN: return {
        tr("Elías", "Elijah"),
        {tr("Tráeme también un bocado de pan en tu mano.",
            "Bring me, I pray thee, a morsel of bread in thine hand."),
         tr("Tú no me conoces. Pero el que me manda a tu puerta te conoce a ti.",
            "You do not know me. But the one who sends me to your door knows you."),
         nullptr},
        {"1 Reyes 17:11", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP2_SACERDOTE: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Manto de pelo y polvo de Galaad. Tú eres el que cerró el cielo sobre Israel, y ahora Sidón paga tu sequía.",
            "A hairy mantle, Gilead's dust. You shut the sky over Israel, and now Sidon pays for your drought."),
         tr("Sarepta es de Baal, tisbita. Y a Baal se le debe una ofrenda por cada año sin lluvia.",
            "Zarephath belongs to Baal, Tishbite. And Baal is owed an offering for every year without rain."),
         nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };

    // =========================================================================
    // Cap. 3 - Elias ante Acab (1 R 18:17-19, 24): el encuentro que convoca el Carmelo.
    // La burla del profeta de Baal y el «hoy gano yo» de Acab son invencion.
    // =========================================================================
    case Sc::AP3_ACAB_TURBAS: return {
        tr("Acab", "Ahab"),
        {tr("¿Eres tú el que turbas a Israel?",
            "Art thou he that troubleth Israel?"),
         tr("Tres años te busqué por todos los reinos, y ahora vienes tú a mí, con la tierra hecha polvo.",
            "Three years I sought you through every kingdom, and now you come to me, with the land turned to dust."),
         nullptr},
        {"1 Reyes 18:17", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AP3_ELIAS_TURBAS: return {
        tr("Elías", "Elijah"),
        {tr("Yo no he turbado a Israel, sino tú y la casa de tu padre, dejando al Señor y siguiendo a los baales.",
            "I have not troubled Israel, but thou and thy father's house, forsaking the Lord and following Baalim."),
         tr("Envía ahora y congrégame a todo Israel en el monte Carmelo.",
            "Send now, and gather to me all Israel unto mount Carmel."),
         tr("Y a los cuatrocientos cincuenta profetas de Baal, y a los de Asera, que comen de la mesa de Jezabel.",
            "And the four hundred and fifty prophets of Baal, and those of Asherah, which eat at Jezebel's table.")},
        {"1 Reyes 18:18-19", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP3_PROFETA_BAAL: return {
        tr("Profeta de Baal", "Prophet of Baal"),
        {tr("Señor de la tormenta, jinete de las nubes: así llaman a Baal de Sidón a Samaria. ¿Y tú lo retas con fuego?",
            "Lord of the storm, rider of the clouds: so they call Baal from Sidon to Samaria. And you dare him with fire?"),
         tr("Somos cuatrocientos cincuenta. Sube al monte, tisbita, y cuenta cuántos bajan.",
            "We are four hundred and fifty. Go up the mountain, Tishbite, and count how many come down."),
         nullptr},
        {"1 Reyes 18:19", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::AP3_ACAB_SEA: return {
        tr("Acab", "Ahab"),
        {tr("Sea. Que suba todo Israel y lo vea. Si Baal responde, se acabó el tisbita; si no responde, se acabó la sequía.",
            "So be it. Let all Israel go up and watch. If Baal answers, the Tishbite is finished; if not, the drought is."),
         tr("De un modo u otro, hoy gano yo.",
            "One way or the other, today I win."),
         nullptr},
        {"1 Reyes 18:20", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AP3_ELIAS_FUEGO: return {
        tr("Elías", "Elijah"),
        {tr("Invocad vosotros el nombre de vuestro dios, y yo invocaré el nombre del Señor.",
            "Call ye on the name of your god, and I will call on the name of the Lord."),
         tr("Y el Dios que responda por medio del fuego, ese es Dios.",
            "And the God that answereth by fire, let him be God."),
         nullptr},
        {"1 Reyes 18:24", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // =========================================================================
    // Cap. 4 - El mensajero de Jezabel (1 R 19:1-3), en Jezreel, la manana despues del
    // Carmelo. El miedo de Elias y la pregunta del criado son invencion; Beerseba y «dejo
    // alli a su criado» son 19:3. La intro del capitulo ya no repite el juramento.
    // =========================================================================
    case Sc::AP4_MENSAJERO: return {
        tr("El mensajero de Jezabel", "Jezebel's messenger"),
        {tr("Palabra de la reina, tisbita. Acab le ha contado lo del Carmelo: el fuego, y los profetas degollados en el Cisón.",
            "Word from the queen, Tishbite. Ahab told her of Carmel: the fire, and the prophets slain at the brook Kishon."),
         tr("«Así me hagan los dioses, si mañana a estas horas no he puesto tu vida como la de uno de ellos».",
            "«So let the gods do to me, and more also, if I make not thy life as the life of one of them by to morrow»."),
         nullptr},
        {"1 Reyes 19:1-2", nullptr}, SC_NO_OPTIONS, Portrait::OFICIAL_REAL
    };
    case Sc::AP4_ELIAS_MIEDO: return {
        tr("Elías", "Elijah"),
        {tr("Ayer cayó fuego del cielo a mi palabra. Y hoy tiemblo por la carta de una mujer.",
            "Yesterday fire fell from heaven at my word. And today I tremble at a woman's letter."),
         tr("Cuatrocientos cincuenta no me acobardaron. Una sola voz sí.",
            "Four hundred and fifty did not daunt me. One single voice has."),
         nullptr},
        {"1 Reyes 19:3", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP4_CRIADO: return {
        tr("Su criado", "His servant"),
        {tr("¿A dónde, mi señor? Jezreel entero ha oído el juramento. No hay casa que te abra la puerta esta noche.",
            "Where to, my lord? All Jezreel has heard the oath. No house will open its door to you tonight."),
         nullptr, nullptr},
        {"1 Reyes 19:3", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP4_ELIAS_HUYE: return {
        tr("Elías", "Elijah"),
        {tr("Al sur. A Beerseba de Judá, fuera del brazo de Acab. Allí te quedas tú.",
            "South. To Beersheba of Judah, beyond Ahab's reach. There you stay."),
         tr("Yo sigo solo al desierto. Un día de camino, y ya veremos si hay un segundo.",
            "I go on alone into the wilderness. A day's journey, and we shall see if there is a second."),
         nullptr},
        {"1 Reyes 19:3-4", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // =========================================================================
    // Cap. 5 - Camino de Jezreel (1 R 19:21; 21:1-16). Eliseo ya sigue a Elias; lo de
    // Nabot se cuenta en dialogo (la unica forma de contarlo con el profeta en escena,
    // regla del 2026-08-30). Termina en 21:16: la palabra de 21:17-19 es el pozo.
    // =========================================================================
    case Sc::AP5_ELISEO_YUNTAS: return {
        tr("Eliseo hijo de Safat", "Elisha the son of Shaphat"),
        {tr("Doce yuntas dejé en el surco, y un manto de pelo me pesa en los hombros más que el arado.",
            "Twelve yoke I left in the furrow, and a hairy mantle weighs on my shoulders more than the plough."),
         tr("Llevo tres días detrás de ti y todavía no me has dicho a dónde vamos.",
            "Three days I have walked behind you and you still have not told me where we go."),
         nullptr},
        {"1 Reyes 19:21", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO_LABRADOR
    };
    case Sc::AP5_ELIAS_HOREB: return {
        tr("Elías", "Elijah"),
        {tr("En Horeb el Señor me dio tres nombres: un rey para Siria, un rey para Israel y un profeta en mi lugar.",
            "At Horeb the Lord gave me three names: a king for Syria, a king for Israel, and a prophet in my room."),
         tr("El tuyo era el último. Lo que hagan los otros dos, lo verás tú, no yo.",
            "Yours was the last. What the other two do, you will see, not I."),
         nullptr},
        {"1 Reyes 19:15-16", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP5_ELISEO_NABOT: return {
        tr("Eliseo hijo de Safat", "Elisha the son of Shaphat"),
        {tr("Oye lo que se cuenta por los caminos. En Jezreel apedrearon a Nabot: dicen que blasfemó contra Dios y el rey.",
            "Hear what is said along the roads. In Jezreel they stoned Naboth: they say he cursed God and the king."),
         tr("Dos testigos, una carta con el sello del rey, y la viña que él no quiso vender ya no tiene dueño.",
            "Two witnesses, a letter under the king's seal, and the vineyard he would not sell now has no owner."),
         tr("Acab baja hoy a la viña a tomar posesión. Con la reina, y con la guardia.",
            "Ahab goes down to the vineyard today to take possession. With the queen, and with the guard.")},
        {"1 Reyes 21:8-16", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO_LABRADOR
    };
    case Sc::AP5_ELIAS_HEREDAD: return {
        tr("Elías", "Elijah"),
        {tr("Nabot no vendió la heredad de sus padres. Esa fue toda su blasfemia.",
            "Naboth would not sell the inheritance of his fathers. That was the whole of his blasphemy."),
         tr("Un sello de Acab, una carta de Jezabel. El Señor conoce esa letra.",
            "Ahab's seal, Jezebel's hand. The Lord knows that writing."),
         tr("Entonces allí lo hallará la palabra del Señor. Ven: en Jezreel hay una viña que visitar.",
            "Then there the word of the Lord will find him. Come: in Jezreel there is a vineyard to visit.")},
        {"1 Reyes 21:3", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // =========================================================================
    // Cap. 6 - A la orilla del Jordan (2 R 2:15), recien cruzado: los hijos de los
    // profetas de Jerico lo reconocen. Lo que dicen del rio abierto es 2:8 y 2:14 vistos
    // desde la orilla; los cincuenta que insisten (2:16-18) son el pozo.
    // =========================================================================
    case Sc::AP6_HIJOS_ESPIRITU: return {
        tr("Los hijos de los profetas", "The sons of the prophets"),
        {tr("Lo vimos desde Jericó: el río se abrió dos veces. Una para los dos, y otra para ti solo.",
            "We saw it from Jericho: the river opened twice. Once for the two of you, and once for you alone."),
         tr("El espíritu de Elías reposó sobre Eliseo.",
            "The spirit of Elijah doth rest on Elisha."),
         nullptr},
        {"2 Reyes 2:15", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP6_ELISEO_MANTO: return {
        tr("Eliseo", "Elisha"),
        {tr("No os inclinéis ante mí. Este manto se le cayó a él; no me lo dio nadie.",
            "Do not bow to me. This mantle fell from him; no one gave it to me."),
         tr("Elías ya no está. El Dios de Elías, sí. Es lo único que sé desde esta mañana.",
            "Elijah is gone. The God of Elijah is not. It is the one thing I know since this morning."),
         nullptr},
        {"2 Reyes 2:13-14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP6_HIJOS_JERICO: return {
        tr("Los hijos de los profetas", "The sons of the prophets"),
        {tr("Ven a Jericó, señor. Cincuenta de los nuestros te esperan allí, y la ciudad tiene una pena que contarte.",
            "Come to Jericho, master. Fifty of us wait for you there, and the city has a sorrow to tell you."),
         nullptr, nullptr},
        {"2 Reyes 2:15-16", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP6_ELISEO_VAMOS: return {
        tr("Eliseo", "Elisha"),
        {tr("Vamos. Y dejad de mirarme como se mira a un muerto: al muerto se lo llevó el carro.",
            "Let us go. And stop looking at me as one looks at a dead man: the chariot took the dead one."),
         nullptr, nullptr},
        {"2 Reyes 2:11", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 7 - Los tres reyes en el desierto de Edom (2 R 3:9-15). Joram y Josafat no
    // tienen retrato: llevan NONE a proposito (la cara de Acab para su hijo seria mentir).
    // Termina en el tanedor (3:15): las zanjas (3:16-23) son el pozo.
    // =========================================================================
    case Sc::AP7_JORAM: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("Siete días de rodeo por el desierto de Edom, y ni una gota para el ejército ni para las bestias.",
            "Seven days' march round the wilderness of Edom, and not a drop for the host nor for the cattle."),
         tr("¡Ah! El Señor ha juntado a tres reyes para entregarlos en manos de Moab.",
            "Alas! The Lord hath called these three kings together, to deliver them into the hand of Moab."),
         nullptr},
        {"2 Reyes 3:9-10", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP7_JOSAFAT: return {
        tr("Josafat, rey de Judá", "Jehoshaphat, king of Judah"),
        {tr("¿No hay aquí profeta del Señor, para que consultemos al Señor por medio de él?",
            "Is there not here a prophet of the Lord, that we may enquire of the Lord by him?"),
         tr("Dicen que aquí está Eliseo hijo de Safat, que servía a Elías. La palabra del Señor está con él.",
            "They say Elisha the son of Shaphat is here, who served Elijah. The word of the Lord is with him."),
         nullptr},
        {"2 Reyes 3:11-12", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP7_ELISEO_QUE: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Qué tengo yo contigo, rey de Israel? Ve a los profetas de tu padre y a los profetas de tu madre.",
            "What have I to do with thee, king of Israel? Get thee to the prophets of thy father, and of thy mother."),
         tr("Vive el Señor de los ejércitos, delante de quien estoy: si no fuera por Josafat, no te miraría ni te vería.",
            "As the Lord of hosts liveth, before whom I stand: were it not for Jehoshaphat, I would not look toward thee."),
         nullptr},
        {"2 Reyes 3:13-14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP7_JORAM_NO: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("No. El Señor juntó a estos tres reyes, y sin agua nos entrega a Moab a los tres. También a Judá.",
            "No. The Lord gathered these three kings, and without water he gives all three to Moab. Judah too."),
         nullptr, nullptr},
        {"2 Reyes 3:13", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP7_ELISEO_TANEDOR: return {
        tr("Eliseo", "Elisha"),
        {tr("Traedme ahora un tañedor. Cuando la mano del Señor venga sobre mí, hablaré.",
            "But now bring me a minstrel. When the hand of the Lord comes upon me, I will speak."),
         tr("Mientras tanto, callad los tres. En este desierto va a hablar alguien más que un rey.",
            "Till then, be silent, all three of you. In this wilderness someone greater than a king is about to speak."),
         nullptr},
        {"2 Reyes 3:15", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 8 - El aposento de Sunem (2 R 4:12-16): la promesa del hijo. Lo que sigue
    // (el nino muerto) lo narra la intro y lo juega el pozo. La ultima frase de la
    // sunamita es invencion que mira a 4:28 («¿Pedí yo hijo a mi señor?»).
    // =========================================================================
    case Sc::AP8_ELISEO_LLAMALA: return {
        tr("Eliseo", "Elisha"),
        {tr("Guiezi, llama a esta sunamita. Cada vez que paso por Sunem hay pan en su mesa y un cuarto con luz para mí.",
            "Gehazi, call this Shunammite. Every time I pass through Shunem there is bread on her table and a room for me."),
         tr("Dile: ¿Qué quieres que haga por ti? ¿Necesitas que hable por ti al rey, o al general del ejército?",
            "Say to her: What is to be done for thee? Shall I speak for thee to the king, or to the captain of the host?"),
         nullptr},
        {"2 Reyes 4:12-13", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP8_SUNAMITA_PUEBLO: return {
        tr("La sunamita", "The Shunammite"),
        {tr("Yo habito en medio de mi pueblo.",
            "I dwell among mine own people."),
         tr("No me falta nada que un rey pueda dar, varón de Dios. Lo que me falta no lo da un rey.",
            "I lack nothing a king can give, man of God. What I lack, no king gives."),
         nullptr},
        {"2 Reyes 4:13", nullptr}, SC_NO_OPTIONS, Portrait::SUNAMITA
    };
    case Sc::AP8_GUIEZI_HIJO: return {
        tr("Guiezi", "Gehazi"),
        {tr("Yo sé lo que le falta, señor. No tiene hijo, y su marido es viejo.",
            "I know what she lacks, master. Verily she hath no child, and her husband is old."),
         nullptr, nullptr},
        {"2 Reyes 4:14", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP8_ELISEO_HIJO: return {
        tr("Eliseo", "Elisha"),
        {tr("El año que viene, por este tiempo, abrazarás un hijo.",
            "About this season, according to the time of life, thou shalt embrace a son."),
         nullptr, nullptr},
        {"2 Reyes 4:16", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP8_SUNAMITA_BURLA: return {
        tr("La sunamita", "The Shunammite"),
        {tr("No, señor mío, varón de Dios, no hagas burla de tu sierva.",
            "Nay, my lord, thou man of God, do not lie unto thine handmaid."),
         tr("Una esperanza así pesa más que un aposento. Si me la das, no me la vuelvas a quitar.",
            "A hope like that weighs more than a chamber. If you give it to me, do not take it back."),
         nullptr},
        {"2 Reyes 4:16", nullptr}, SC_NO_OPTIONS, Portrait::SUNAMITA
    };

    // =========================================================================
    // Cap. 9 - La carta del rey de Siria (2 R 5:5-8), en el palacio de Samaria. Los ojos
    // de Guiezi puestos en el oro son invencion que prepara al jefe del capitulo (5:20).
    // =========================================================================
    case Sc::AP9_JORAM_CARTA: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("«Sabe que te envío a Naamán mi siervo, para que lo sanes de su lepra». Lo firma Ben-adad, rey de Siria.",
            "«I have sent Naaman my servant to thee, that thou mayest heal him of his leprosy». Signed: Ben-hadad of Syria."),
         tr("¿Soy yo Dios, que mate y dé vida? Mirad cómo busca ocasión contra mí.",
            "Am I God, to kill and to make alive? See how he seeketh a quarrel against me."),
         nullptr},
        {"2 Reyes 5:6-7", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP9_GUIEZI_PLATA: return {
        tr("Guiezi", "Gehazi"),
        {tr("Con la carta vienen diez talentos de plata, seis mil piezas de oro y diez vestidos. Ya están en la puerta.",
            "With the letter come ten talents of silver, six thousand pieces of gold and ten garments. They are at the gate."),
         nullptr, nullptr},
        {"2 Reyes 5:5", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP9_ELISEO_PROFETA: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Por qué ha rasgado el rey sus vestidos? Que venga ahora a mí, y sabrá que hay profeta en Israel.",
            "Why has the king rent his clothes? Let him come now to me, and he shall know there is a prophet in Israel."),
         nullptr, nullptr},
        {"2 Reyes 5:8", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP9_GUIEZI_ORO: return {
        tr("Guiezi", "Gehazi"),
        {tr("Seis mil piezas de oro, mi señor. Con la mitad se compran olivares; con la otra, siervos que los caven.",
            "Six thousand pieces of gold, master. Half would buy olive groves; the other half, servants to dig them."),
         nullptr, nullptr},
        {"2 Reyes 5:20", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP9_ELISEO_NO_VENDE: return {
        tr("Eliseo", "Elisha"),
        {tr("Lo que el Señor da no se vende, Guiezi. Recuérdalo cuando veas los carros.",
            "What the Lord gives is not for sale, Gehazi. Remember that when you see the chariots."),
         tr("Ve a la puerta y dile al sirio que venga. Sin plata.",
            "Go to the gate and tell the Syrian to come. Without the silver."),
         nullptr},
        {"2 Reyes 5:16", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 10 - El rey avisado (2 R 6:9-13): los avisos de Eliseo al rey de Israel y el
    // juramento del rey de Siria. El criado ya no es Guiezi (5:27): la celda se reusa con
    // el nombre generico de 6:15, como hace el pozo.
    // =========================================================================
    case Sc::AP10_ELISEO_AVISO: return {
        tr("Eliseo", "Elisha"),
        {tr("Guárdate de pasar por aquel lugar, rey, porque los sirios van a bajar allí.",
            "Beware that thou pass not such a place, king; for thither the Syrians are come down."),
         tr("Es la tercera vez. Manda a tus hombres por el otro camino.",
            "This is the third time. Send your men by the other road."),
         nullptr},
        {"2 Reyes 6:9-10", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP10_JORAM_COMO: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("Tres avisos, tres aciertos, varón de Dios. Ben-adad cambia el plan de noche y tú lo sabes al alba.",
            "Three warnings, three times right, man of God. Ben-hadad changes his plan by night and you know it by dawn."),
         tr("¿Quién te lo dice?",
            "Who tells you?"),
         nullptr},
        {"2 Reyes 6:10", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP10_ELISEO_QUIEN: return {
        tr("Eliseo", "Elisha"),
        {tr("El que oye lo que se habla en Damasco a puerta cerrada. Yo solo lo repito.",
            "The one who hears what is spoken in Damascus behind closed doors. I only repeat it."),
         nullptr, nullptr},
        {"2 Reyes 6:12", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP10_CRIADO_JURADO: return {
        tr("El criado de Eliseo", "Elisha's servant"),
        {tr("Señor, dicen que el rey de Siria ha jurado prenderte. Sus jinetes preguntan por ti en cada aldea hasta Dotán.",
            "Master, they say the king of Syria has sworn to take you. His horsemen ask for you in every village to Dothan."),
         nullptr, nullptr},
        {"2 Reyes 6:13", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP10_ELISEO_DOTAN: return {
        tr("Eliseo", "Elisha"),
        {tr("Que pregunten. Estaremos en Dotán, y no escondidos.",
            "Let them ask. We shall be in Dothan, and not in hiding."),
         tr("Un ejército por un solo hombre: Ben-adad ya ha contado mal una vez.",
            "An army for one man: Ben-hadad has already counted wrong once."),
         nullptr},
        {"2 Reyes 6:13-14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Dialogos con el jefe. El jefe habla primero y el profeta responde; la ultima caja
    // es la que abre la pelea. Las frases de `ChapterLore::boss_line` viven ahora aqui.
    // =========================================================================
    // Cap. 1 - El Capitan de Acab (1 R 18:10). Cincuenta hombres: 2 R 1:9.
    case Sc::J1_CAPITAN: return {
        tr("Capitán de Acab", "Ahab's Captain"),
        {tr("Acab ha jurado por todos los reinos que no te hallaron. Yo te hallé: bastó seguir el único arroyo con agua.",
            "Ahab made every kingdom swear they had not found you. I found you: I only followed the one brook with water."),
         tr("El rey te quiere vivo, profeta. Vivo, o en pedazos.",
            "The king wants you alive, prophet. Alive, or in pieces."),
         nullptr},
        {"1 Reyes 18:10", nullptr}, SC_NO_OPTIONS, Portrait::OFICIAL_REAL
    };
    case Sc::J1_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Cada mañana bajan los cuervos con pan y carne. ¿Crees que el que los manda me va a soltar de tu mano?",
            "Every morning the ravens bring bread and flesh. Do you think the one who sends them will let go of me now?"),
         tr("Vive el Señor delante de quien estoy. Ven a por el manto, capitán.",
            "As the Lord liveth, before whom I stand. Come and take the mantle, captain."),
         nullptr},
        {"1 Reyes 17:6", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J1_CAPITAN_2: return {
        tr("Capitán de Acab", "Ahab's Captain"),
        {tr("Cincuenta hombres por un tisbita. Al rey le parecerá poco.",
            "Fifty men for one Tishbite. The king will think it too few."),
         nullptr, nullptr},
        {"2 Reyes 1:9", nullptr}, SC_NO_OPTIONS, Portrait::OFICIAL_REAL
    };

    // Cap. 2 - El Sacerdote de Baal. La amenaza a la viuda se apoya en 18:4.
    case Sc::J2_SACERDOTE: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Tres años sin lluvia. Baal está airado, y en Sarepta todos saben con quién.",
            "Three years without rain. Baal is angry, and everyone in Zarephath knows with whom."),
         tr("¡Sarepta es de Baal! ¡Y tú serás su ofrenda!",
            "Zarephath belongs to Baal! And you will be its offering!"),
         nullptr},
        {"1 Reyes 17:9", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::J2_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Tu dios no ha cerrado el cielo. Y no será tu dios quien lo abra.",
            "Your god did not shut the sky. And it will not be your god who opens it."),
         tr("Y la viuda que me esconde tiene en su casa más Dios que tú en tu altar.",
            "And the widow who hides me has more God in her house than you have on your altar."),
         nullptr},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J2_SACERDOTE_2: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Sidón sabe qué hacer con las viudas que esconden profetas. Primero tú; después ella.",
            "Sidon knows what to do with widows who hide prophets. First you; then her."),
         nullptr, nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };

    // Cap. 3 - Los 450 profetas de Baal (1 R 18:26-29, 36).
    case Sc::J3_PROFETAS: return {
        tr("Los profetas de Baal", "The prophets of Baal"),
        {tr("¡Baal, respóndenos! ¡Respóndenos!",
            "O Baal, hear us! Hear us!"),
         tr("Desde la mañana hasta el mediodía hemos gritado, y el altar sigue frío. ¡Pero Baal oirá!",
            "From morning until noon we have cried, and the altar is still cold. But Baal will hear!"),
         nullptr},
        {"1 Reyes 18:26", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::J3_ELIAS_BURLA: return {
        tr("Elías", "Elijah"),
        {tr("Gritad en alta voz, porque dios es. Quizá está meditando, o tiene algún trabajo, o va de camino.",
            "Cry aloud: for he is a god. Either he is talking, or he is pursuing, or he is in a journey."),
         tr("Tal vez duerme, y hay que despertarle.",
            "Or peradventure he sleepeth, and must be awaked."),
         nullptr},
        {"1 Reyes 18:27", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J3_PROFETAS_2: return {
        tr("Los profetas de Baal", "The prophets of Baal"),
        {tr("¡Nos sajaremos con cuchillos y con lanzas hasta chorrear la sangre! ¡Y si Baal no baja, bajarás tú!",
            "We will cut ourselves with knives and lancets till the blood gushes! And if Baal does not come down, you will!"),
         nullptr, nullptr},
        {"1 Reyes 18:28", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::J3_ELIAS_FIN: return {
        tr("Elías", "Elijah"),
        {tr("Ni voz, ni quien respondiese, ni quien escuchase.",
            "There was neither voice, nor any to answer, nor any that regarded."),
         tr("Ya ha pasado el mediodía. Ahora le toca al Dios de Abraham, de Isaac y de Israel.",
            "Noon is past. Now it is the turn of the God of Abraham, of Isaac, and of Israel."),
         nullptr},
        {"1 Reyes 18:29, 36", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 4 - Tu propia desesperacion (1 R 19:4, 10). Habla con la cara de Elias a
    // proposito: es su propia voz. La respuesta es 19:10 tal cual; el cierre, invencion.
    case Sc::J4_DESESPERACION: return {
        tr("Tu propia desesperación", "Your own despair"),
        {tr("Basta ya, Señor. Quítame la vida, porque no soy yo mejor que mis padres.",
            "It is enough; now, O Lord, take away my life; for I am not better than my fathers."),
         tr("Un día de fuego, y luego cuarenta de huida. ¿De qué sirvió el Carmelo?",
            "One day of fire, then forty of flight. What was Carmel for?"),
         nullptr},
        {"1 Reyes 19:4", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J4_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("He sentido un vivo celo por el Señor. Los hijos de Israel han dejado tu pacto y derribado tus altares.",
            "I have been very jealous for the Lord. Israel has forsaken thy covenant and thrown down thine altars."),
         tr("Han matado a espada a tus profetas, y solo yo he quedado, y me buscan para quitarme la vida.",
            "They have slain thy prophets with the sword; and I, even I only, am left; and they seek my life."),
         nullptr},
        {"1 Reyes 19:10", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J4_DESESPERACION_2: return {
        tr("Tu propia desesperación", "Your own despair"),
        {tr("Solo tú. Jezabel lo juró y lo cumplirá: mañana serás como uno de ellos.",
            "You alone. Jezebel swore it and will keep it: tomorrow you will be as one of them."),
         tr("Túmbate bajo el enebro, tisbita. Nadie te busca ya en esta cueva.",
            "Lie down under the juniper, Tishbite. No one seeks you in this cave any more."),
         nullptr},
        {"1 Reyes 19:2", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J4_ELIAS_2: return {
        tr("Elías", "Elijah"),
        {tr("Solo, sí. Pero delante de quien estoy.",
            "Alone, yes. But before whom I stand."),
         tr("Y si esta es la última vez, que me halle en pie.",
            "And if this is the last time, let him find me on my feet."),
         nullptr},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 5 - Acab y Jezabel en la vina de Nabot (1 R 21:20-23). Lo de Jezabel
    // («yo escribí las cartas») es 21:8 dicho por ella.
    case Sc::J5_ACAB: return {
        tr("Acab", "Ahab"),
        {tr("¿Me has hallado, enemigo mío?",
            "Hast thou found me, O mine enemy?"),
         tr("La viña es mía por sello y por sentencia. Nabot blasfemó, dos testigos lo juraron, y la ciudad lo apedreó.",
            "The vineyard is mine by seal and sentence. Naboth blasphemed, two witnesses swore it, and the city stoned him."),
         nullptr},
        {"1 Reyes 21:20", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::J5_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Te he hallado, porque te has vendido a hacer lo malo delante del Señor.",
            "I have found thee: because thou hast sold thyself to work evil in the sight of the Lord."),
         tr("He aquí yo traigo mal sobre ti, y barreré tu posteridad, y cortaré de Acab hasta el último varón en Israel.",
            "Behold, I will bring evil upon thee, and take away thy posterity, and cut off from Ahab every male in Israel."),
         nullptr},
        {"1 Reyes 21:20-21", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J5_JEZABEL: return {
        tr("Jezabel", "Jezebel"),
        {tr("Dos testigos, tisbita. Yo escribí las cartas y yo puse el sello. Búscame a mí, si te atreves.",
            "Two witnesses, Tishbite. I wrote the letters and I set the seal. Come for me, if you dare."),
         tr("Los perros de Jezreel comen lo que yo les echo. Hoy les echo un profeta.",
            "The dogs of Jezreel eat what I throw them. Today I throw them a prophet."),
         nullptr},
        {"1 Reyes 21:8", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::J5_ELIAS_2: return {
        tr("Elías", "Elijah"),
        {tr("También de ti ha hablado el Señor, Jezabel: los perros te comerán en el muro de Jezreel.",
            "Of thee also hath the Lord spoken, Jezebel: the dogs shall eat thee by the wall of Jezreel."),
         nullptr, nullptr},
        {"1 Reyes 21:23", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 6 - El Espiritu de Corrupcion (1 R 16:34; 2 R 2:21). Que un espiritu hable es
    // licencia del jefe, no del libro; lo que dice de Hiel y de Josue es 16:34.
    case Sc::J6_ESPIRITU: return {
        tr("Espíritu de Corrupción", "Spirit of Corruption"),
        {tr("Hiel levantó esta ciudad sobre el juramento de Josué, y pagó con dos hijos. Yo soy lo que quedó en el agua.",
            "Hiel raised this city upon the oath of Joshua, and paid with two sons. I am what was left in the water."),
         tr("Esta agua es mía. Bebe de ella.",
            "This water is mine. Drink of it."),
         nullptr},
        {"1 Reyes 16:34", nullptr}, SC_NO_OPTIONS, P_AGUA_CORRUPTA
    };
    case Sc::J6_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("El manto que llevo abrió el Jordán esta mañana. Tu manantial no es más ancho que el Jordán.",
            "The mantle I carry opened the Jordan this morning. Your spring is no wider than the Jordan."),
         nullptr, nullptr},
        {"2 Reyes 2:14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J6_ESPIRITU_2: return {
        tr("Espíritu de Corrupción", "Spirit of Corruption"),
        {tr("Elías se fue en el carro. ¿Y tú quién eres, labrador? Doce yuntas no hacen un profeta.",
            "Elijah went up in the chariot. And who are you, ploughman? Twelve yoke do not make a prophet."),
         nullptr, nullptr},
        {"2 Reyes 2:11", nullptr}, SC_NO_OPTIONS, P_AGUA_CORRUPTA
    };
    case Sc::J6_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("Nadie. Pero el Dios de Elías no se fue en el carro. Y ha dicho de estas aguas: ni muerte ni esterilidad.",
            "No one. But Elijah's God did not go up in the chariot. Of these waters he has said: no more death nor barrenness."),
         nullptr, nullptr},
        {"2 Reyes 2:21", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 7 - El Acreedor (2 R 4:1, 7). Que la ley le da los hijos por siervos es lo
    // que la viuda dice en 4:1; «vende el aceite y paga» es 4:7.
    case Sc::J7_ACREEDOR: return {
        tr("El Acreedor", "The Creditor"),
        {tr("El profeta murió debiendo. La ley me da a sus dos hijos por siervos, y la ley está de mi parte.",
            "The prophet died in debt. The law gives me his two sons for bondmen, and the law is on my side."),
         tr("La deuda se paga con hijos o con sangre.",
            "A debt is paid in sons or in blood."),
         nullptr},
        {"2 Reyes 4:1", nullptr}, SC_NO_OPTIONS, P_SOLDADO_SIRIO
    };
    case Sc::J7_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Ese hombre temía al Señor. Su deuda la ha tomado el Señor, y el Señor paga en aceite, no en niños.",
            "That man feared the Lord. The Lord has taken up his debt, and the Lord pays in oil, not in children."),
         nullptr, nullptr},
        {"2 Reyes 4:1-2", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J7_ACREEDOR_2: return {
        tr("El Acreedor", "The Creditor"),
        {tr("El aceite se acaba. Yo no.",
            "Oil runs out. I do not."),
         nullptr, nullptr},
        {"2 Reyes 4:6", nullptr}, SC_NO_OPTIONS, P_SOLDADO_SIRIO
    };
    case Sc::J7_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("«Vende el aceite y paga tu deuda». Hasta la última vasija de la aldea, si hace falta. Sal de esta casa.",
            "«Sell the oil, and pay thy debt». Down to the last vessel in the village, if need be. Get out of this house."),
         nullptr, nullptr},
        {"2 Reyes 4:7", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 8 - La Muerte (2 R 4:19-20, 27, 31-33). Sin retrato: no es nadie. La mencion
    // del nino de Sarepta (1 R 17:22) es la burla que la Muerte le hace a Eliseo.
    case Sc::J8_MUERTE: return {
        tr("La Muerte", "Death"),
        {tr("Todo lo que respira es mío. También el hijo prometido.",
            "All that breathes is mine. The promised son as well."),
         tr("«¡Ay, mi cabeza!», dijo entre los segadores. Y al mediodía dormía sobre las rodillas de su madre.",
            "«My head, my head!», he said among the reapers. And at noon he slept on his mother's knees."),
         nullptr},
        {"2 Reyes 4:19-20", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::J8_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Su alma está en amargura, y el Señor me lo encubrió. Pero he venido.",
            "Her soul is vexed within her, and the Lord hid it from me. But I have come."),
         tr("Mi criado puso el báculo sobre el rostro del niño, y no hubo voz ni sentido. Yo no traigo un báculo.",
            "My servant laid the staff upon the child's face, and there was neither voice nor hearing. I bring no staff."),
         nullptr},
        {"2 Reyes 4:27, 31", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J8_MUERTE_2: return {
        tr("La Muerte", "Death"),
        {tr("¿Y qué traes, hijo de Safat? Elías devolvió un niño en Sarepta. Tú todavía no has devuelto a nadie.",
            "And what do you bring, son of Shaphat? Elijah gave back a child in Zarephath. You have given back no one yet."),
         nullptr, nullptr},
        {"1 Reyes 17:22", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::J8_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("Cerré la puerta tras los dos. Aquí solo estamos el niño, tú y el Señor.",
            "I shut the door upon us both. Here there are only the child, you and the Lord."),
         tr("Y de los tres, el único que se va eres tú.",
            "And of the three, the only one leaving is you."),
         nullptr},
        {"2 Reyes 4:33", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 9 - Guiezi el codicioso (2 R 5:20-27). Todo lo que dicen los dos es del
    // pasaje, salvo la rabia de Guiezi contra Naaman, que es la que 5:20 le supone.
    case Sc::J9_GUIEZI: return {
        tr("Guiezi", "Gehazi"),
        {tr("Tu siervo no ha ido a ninguna parte.",
            "Thy servant went no whither."),
         tr("Solo hasta la puerta, a despedir al sirio. Se fue contento: tierra de dos mulas y la carne de un niño.",
            "Only to the gate, to see the Syrian off. He left happy: two mules' burden of earth and a child's flesh."),
         nullptr},
        {"2 Reyes 5:25", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::J9_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("¿No estaba también allí mi corazón cuando el hombre volvió de su carro a recibirte?",
            "Went not mine heart with thee, when the man turned again from his chariot to meet thee?"),
         tr("Dos talentos de plata y dos vestidos, escondidos en la casa. Hasta la última pieza.",
            "Two talents of silver and two changes of garments, hidden in the house. Down to the last piece."),
         nullptr},
        {"2 Reyes 5:23-26", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J9_GUIEZI_2: return {
        tr("Guiezi", "Gehazi"),
        {tr("¡Naamán se ríe de nosotros con su carne nueva y sus carros llenos! Tú rechazaste lo que era tuyo. Yo no.",
            "Naaman laughs at us with his new flesh and his loaded chariots! You refused what was yours. I did not."),
         nullptr, nullptr},
        {"2 Reyes 5:20", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::J9_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Es tiempo de tomar plata, y vestidos, olivares y viñas, siervos y siervas?",
            "Is it a time to receive money, and garments, oliveyards and vineyards, menservants and maidservants?"),
         tr("La lepra de Naamán se te pegará a ti y a tu descendencia para siempre.",
            "The leprosy of Naaman shall cleave unto thee, and unto thy seed for ever."),
         nullptr},
        {"2 Reyes 5:26-27", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 10 - Ben-adad de Siria (2 R 6:16, 22-25; 7:1). El precio de la cabeza de asno
    // es 6:25; la harina a un siclo, 7:1, que es la profecia que el principe (pozo) niega.
    case Sc::J10_BENADAD: return {
        tr("Ben-adad de Siria", "Ben-hadad of Syria"),
        {tr("Cegaste a mis jinetes en Dotán y les diste de comer en Samaria. Hoy no hay mesa puesta, profeta.",
            "You blinded my horsemen at Dothan and fed them in Samaria. There is no table laid today, prophet."),
         tr("Toda Siria contra un solo hombre. Ríndete.",
            "All Syria against one man. Surrender."),
         nullptr},
        {"2 Reyes 6:22-24", nullptr}, SC_NO_OPTIONS, Portrait::GENERAL_SIRIO
    };
    case Sc::J10_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Más son los que están con nosotros que los que están con ellos. Se lo dije a mi criado, y lo vio.",
            "They that be with us are more than they that be with them. I told my servant, and he saw it."),
         nullptr, nullptr},
        {"2 Reyes 6:16-17", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J10_BENADAD_2: return {
        tr("Ben-adad de Siria", "Ben-hadad of Syria"),
        {tr("Tus carros de fuego no se ven desde aquí. Se ve el hambre de Samaria, y una cabeza de asno a ochenta siclos.",
            "No chariots of fire can be seen from here. Only Samaria's hunger, and an ass's head sold for eighty shekels."),
         nullptr, nullptr},
        {"2 Reyes 6:25", nullptr}, SC_NO_OPTIONS, Portrait::GENERAL_SIRIO
    };
    case Sc::J10_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("En Dotán tampoco los viste, y comiste de mi mesa.",
            "You did not see them at Dothan either, and you ate at my table."),
         tr("Mañana a estas horas, a la puerta de Samaria, un seah de flor de harina valdrá un siclo.",
            "To morrow about this time shall a measure of fine flour be sold for a shekel, in the gate of Samaria."),
         nullptr},
        {"2 Reyes 7:1", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    default: return {
        "", {nullptr, nullptr, nullptr}, {"", nullptr}, SC_NO_OPTIONS
    };
    }
}

#undef SC_NO_OPTIONS

// La caja de dialogo no sabe si lo que lee es un relato del pozo o una caja de escena:
// esto es lo unico que mira `DialogueState::event_id`.
inline EventDef get_dialogue_def(int event_id) {
    return (event_id >= Sc::LINE_BASE) ? get_scene_line(event_id - Sc::LINE_BASE)
                                       : get_event_def(event_id);
}

// =============================================================================
// Las escenas: escenario + cadena
// =============================================================================
// Ancla del escenario: en un interior (20x11) es (W/2, 4) —figuras en la fila 2 y el
// profeta en la 5, que la caja de dialogo (y=116) no tapa—; en un exterior (40x22) es el
// centro, con un claro de 15x8 tiles alrededor para que la decoracion no tape a nadie; en
// la arena (`Live`) es el tile del jefe (W/2, 3), y el profeta se planta tres filas por
// debajo, cara a cara: donde entro por el borde sur (fila 9) la caja lo tapaba entero.
// Las figuras de una escena `Live` se retiran al cerrarse (finish_scene): Acab y Jezabel
// flanquean a su arquero mientras hablan, y la pelea es con la guardia.
inline SceneDef get_scene_def(int id) {
    constexpr int E_SOLDADO_ACAB = static_cast<int>(EnemyType::SoldadoDeAcab);
    constexpr int E_PROFETA_BAAL = static_cast<int>(EnemyType::ProfetaDeBaal);
    using FK = FigureKind;
    const SceneFigure none{};
    switch (id) {
    // El palacio de Samaria: Acab y Jezabel de pie al fondo, Elias delante mirandolos.
    case Scene::AUDIENCIA: return {
        "Audiencia", "Audiencia con Acab (1 Reyes 17:1-4)", 1, StageKind::Interior, 1,
        0, 1, Direction::Up,
        {{FK::Acab, 0, -1, -2, Direction::Down}, {FK::Jezabel, 0, 1, -2, Direction::Down}, none},
        Sc::AUD_REPRENDE, 7, SceneThen::ChapterIntro
    };
    // La puerta de Sarepta: la viuda con sus lenos, y un sacerdote de Baal mirando.
    case Scene::APERTURA_2: return {
        "Apertura 2", "Apertura del capitulo 2: a la puerta de Sarepta (1 Reyes 17:8-11)", 2,
        StageKind::Exterior, 2, 0, 1, Direction::Up,
        {{FK::Npc, Npc::VIUDA_SAREPTA, -1, -2, Direction::Down}, {FK::Enemy, E_PROFETA_BAAL, 5, -3, Direction::Down}, none},
        Sc::AP2_PALABRA, 5, SceneThen::ChapterIntro
    };
    // Al pie del Carmelo: Acab con dos profetas de Baal a los flancos.
    case Scene::APERTURA_3: return {
        "Apertura 3", "Apertura del capitulo 3: Elias ante Acab (1 Reyes 18:17-19)", 3,
        StageKind::Exterior, 3, 0, 1, Direction::Up,
        {{FK::Acab, 0, 0, -2, Direction::Down}, {FK::Enemy, E_PROFETA_BAAL, -3, -2, Direction::Down},
         {FK::Enemy, E_PROFETA_BAAL, 3, -2, Direction::Down}},
        Sc::AP3_ACAB_TURBAS, 5, SceneThen::ChapterIntro
    };
    // Jezreel (el valle: campos del cap. 5): el mensajero de la reina, y el criado detras.
    case Scene::APERTURA_4: return {
        "Apertura 4", "Apertura del capitulo 4: el mensajero de Jezabel (1 Reyes 19:1-3)", 4,
        StageKind::Exterior, 5, 0, 1, Direction::Up,
        {{FK::Enemy, E_SOLDADO_ACAB, 0, -2, Direction::Down}, {FK::Npc, Npc::HIJO_PROFETAS, 2, 1, Direction::Left}, none},
        Sc::AP4_MENSAJERO, 4, SceneThen::ChapterIntro
    };
    // Los campos, camino de Jezreel: Eliseo a su lado.
    case Scene::APERTURA_5: return {
        "Apertura 5", "Apertura del capitulo 5: camino de Jezreel (1 Reyes 19:21; 21:1-16)", 5,
        StageKind::Exterior, 5, 0, 0, Direction::Left,
        {{FK::Npc, Npc::ELISEO_LABRADOR, -2, 0, Direction::Right}, none, none},
        Sc::AP5_ELISEO_YUNTAS, 4, SceneThen::ChapterIntro
    };
    // La orilla del Jordan (ribera del cap. 9): dos de los hijos de los profetas.
    case Scene::APERTURA_6: return {
        "Apertura 6", "Apertura del capitulo 6: a la orilla del Jordan (2 Reyes 2:15)", 6,
        StageKind::Exterior, 9, 0, 1, Direction::Up,
        {{FK::Npc, Npc::HIJO_PROFETAS, -1, -2, Direction::Down}, {FK::Npc, Npc::HIJO_PROFETAS, 2, -2, Direction::Down}, none},
        Sc::AP6_HIJOS_ESPIRITU, 4, SceneThen::ChapterIntro
    };
    // El desierto de Edom (cap. 4): Joram (hoja del rey), Josafat (azul y plata) y un soldado.
    case Scene::APERTURA_7: return {
        "Apertura 7", "Apertura del capitulo 7: los tres reyes en el desierto (2 Reyes 3:9-15)", 7,
        StageKind::Exterior, 4, 0, 1, Direction::Up,
        {{FK::Acab, 0, -2, -2, Direction::Down}, {FK::Npc, Npc::PRINCIPE_SAMARIA, 2, -2, Direction::Down},
         {FK::Enemy, E_SOLDADO_ACAB, 5, -1, Direction::Down}},
        Sc::AP7_JORAM, 5, SceneThen::ChapterIntro
    };
    // El aposento de Sunem: la sunamita al fondo, Guiezi al lado.
    case Scene::APERTURA_8: return {
        "Apertura 8", "Apertura del capitulo 8: el aposento de Sunem (2 Reyes 4:12-16)", 8,
        StageKind::Interior, 8, 0, 1, Direction::Up,
        {{FK::Npc, Npc::SUNAMITA, -1, -2, Direction::Down}, {FK::Npc, Npc::CRIADO_ELISEO, 2, 0, Direction::Left}, none},
        Sc::AP8_ELISEO_LLAMALA, 5, SceneThen::ChapterIntro
    };
    // El palacio de Samaria: el rey con la carta, Guiezi con los ojos en el oro.
    case Scene::APERTURA_9: return {
        "Apertura 9", "Apertura del capitulo 9: la carta del rey de Siria (2 Reyes 5:5-8)", 9,
        StageKind::Interior, 9, 0, 1, Direction::Up,
        {{FK::Acab, 0, -1, -2, Direction::Down}, {FK::Npc, Npc::CRIADO_ELISEO, 2, 0, Direction::Left}, none},
        Sc::AP9_JORAM_CARTA, 5, SceneThen::ChapterIntro
    };
    // Samaria (ciudad del cap. 10): el rey avisado y el criado.
    case Scene::APERTURA_10: return {
        "Apertura 10", "Apertura del capitulo 10: el rey avisado (2 Reyes 6:9-13)", 10,
        StageKind::Exterior, 10, 0, 1, Direction::Up,
        {{FK::Acab, 0, -1, -2, Direction::Down}, {FK::Npc, Npc::CRIADO_ELISEO, 2, 0, Direction::Left}, none},
        Sc::AP10_ELISEO_AVISO, 5, SceneThen::ChapterIntro
    };
    // Los jefes: la arena tal cual esta, el jefe de pie delante y el profeta donde entro.
    case Scene::JEFE_1:  return {"Jefe 1",  "Dialogo con el jefe 1: el Capitan de Acab (1 Reyes 18:10)",       1,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J1_CAPITAN,       3, SceneThen::Play};
    case Scene::JEFE_2:  return {"Jefe 2",  "Dialogo con el jefe 2: el Sacerdote de Baal (1 Reyes 18:4)",      2,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J2_SACERDOTE,     3, SceneThen::Play};
    case Scene::JEFE_3:  return {"Jefe 3",  "Dialogo con el jefe 3: los profetas de Baal (1 Reyes 18:26-29)",  3,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J3_PROFETAS,      4, SceneThen::Play};
    case Scene::JEFE_4:  return {"Jefe 4",  "Dialogo con el jefe 4: tu propia desesperacion (1 Reyes 19:4-10)", 4, StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J4_DESESPERACION, 4, SceneThen::Play};
    case Scene::JEFE_5:  return {"Jefe 5",  "Dialogo con el jefe 5: Acab y Jezabel (1 Reyes 21:20-23)",        5,  StageKind::Live, 0, 0, 3, Direction::Up,
                                 {{FK::Acab, 0, -2, 0, Direction::Down}, {FK::Jezabel, 0, 2, 0, Direction::Down}, none}, Sc::J5_ACAB, 4, SceneThen::Play};
    case Scene::JEFE_6:  return {"Jefe 6",  "Dialogo con el jefe 6: el Espiritu de Corrupcion (2 Reyes 2:21)", 6,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J6_ESPIRITU,      4, SceneThen::Play};
    case Scene::JEFE_7:  return {"Jefe 7",  "Dialogo con el jefe 7: el Acreedor (2 Reyes 4:1-7)",              7,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J7_ACREEDOR,      4, SceneThen::Play};
    case Scene::JEFE_8:  return {"Jefe 8",  "Dialogo con el jefe 8: la Muerte (2 Reyes 4:19-33)",              8,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J8_MUERTE,        4, SceneThen::Play};
    case Scene::JEFE_9:  return {"Jefe 9",  "Dialogo con el jefe 9: Guiezi (2 Reyes 5:20-27)",                 9,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J9_GUIEZI,        4, SceneThen::Play};
    case Scene::JEFE_10: return {"Jefe 10", "Dialogo con el jefe 10: Ben-adad de Siria (2 Reyes 6:24-7:1)",    10, StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J10_BENADAD,      4, SceneThen::Play};
    default: return {
        "", "", 0, StageKind::Live, 0, 0, 0, Direction::Down, {none, none, none}, 0, 0, SceneThen::Play
    };
    }
}
