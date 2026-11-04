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
// VOZ (pasada del 2026-09-10, veredicto del usuario: «suenan a IA»): el registro es el de
// la Reina-Valera tambien en lo inventado — vosotros, «he aqui», «senor mio», «tu
// siervo», «varon de Dios», «Vive Jehova» —, y cada personaje habla como en 1-2 Reyes, no
// con sentencias. Una sola sentencia bimembre por ESCENA, ninguna caja cerrada con
// aforismo salvo que sea versiculo, nadie narra lo que se ve en pantalla, ningun aparte
// omnisciente, y la unica exclamacion del juego es el grito ritual de Baal. La guia
// completa y la espina de cada capitulo: docs/encuentros-y-lore.md.
//
// Techo de texto: cada `lines[i]` es UNA pagina; con retrato caben 4 lineas a 244 u, sin
// retrato 3 a 280 u, en ES y en EN. `scripts/escenas_check.txt` recorre las 20 escenas en
// los dos idiomas: cero TEXT TRUNCATED.
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
    // El jefe responde a lo JUGADO: si el jugador resolvio ese relato del pozo en esta
    // partida (Engine::event_resolved_in_run), la PRIMERA caja es `alt_first_line` en vez
    // de `first_line`; el resto de la cadena no cambia. -1 = sin variante.
    int         alt_first_line = -1;
    int         alt_event      = -1;   // id de `namespace Ev`
    // PASEO CINEMATICO dentro de la cadena: tras la parada `walk_after` (0-based) el motor
    // lleva al profeta andando hasta su sitio en vez de abrir la caja siguiente, y solo
    // entonces sigue. Es lo que convierte un corro plantado desde la primera caja en un
    // ENCUENTRO: las figuras ya estan de pie y es el quien llega. -1 = sin paseo, que es
    // lo que dicen las otras 19 escenas sin escribir una letra.
    // De donde sale, en tiles RELATIVOS a player_dx/dy; el destino es el player_dx/dy de
    // siempre, igual que en la entrada del jefe: si fuera otro, la caja siguiente
    // teletransportaria al profeta y el corte volveria.
    int         walk_after   = -1;
    int         walk_from_dx = 0;
    int         walk_from_dy = 0;
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
// El ORDEN del enum dentro de cada bloque ES la cadena (`SceneDef::first_line` + `count`):
// un `case` que falte en get_scene_line hace que open_dialogue salte la caja en silencio
// (contar `parada k/N` en el log de escenas_check).
namespace Sc {
constexpr int LINE_BASE = 1000;   // desplazamiento en DialogueState::event_id
enum : int {
    // --- La audiencia con Acab (1 Reyes 17:1-4), apertura del cap. 1 ---------------
    AUD_REPRENDE = 0, AUD_ACAB_DESPRECIA, AUD_JEZABEL_AMENAZA, AUD_SENTENCIA,
    AUD_ACAB_BURLA, AUD_JEZABEL_CAZA, AUD_PALABRA_QUERIT,
    // --- Aperturas 2..10 -----------------------------------------------------------
    AP2_PALABRA, AP2_ELIAS_AGUA, AP2_VIUDA_AGUA, AP2_ELIAS_PAN, AP2_SACERDOTE,
    AP3_PALABRA, AP3_ACAB_TURBAS, AP3_ELIAS_TURBAS, AP3_PROFETA_BAAL, AP3_ELIAS_FUEGO, AP3_ACAB_SEA,
    AP4_MENSAJERO, AP4_ELIAS_VAMONOS, AP4_CRIADO, AP4_ELIAS_HUYE,
    AP5_YUNTAS, AP5_ELISEO_BESAR, AP5_ELIAS_VUELVE, AP5_ELISEO_BUEYES,
    AP6_HIJOS_ESPIRITU, AP6_ELISEO_MANTO, AP6_HIJOS_JERICO, AP6_ELISEO_VAMOS,
    AP7_VIUDA, AP7_ELISEO_QUE_TIENES, AP7_VIUDA_ACEITE, AP7_GUIEZI, AP7_ELISEO_VASIJAS,
    AP8_SUNAMITA_APOSENTO, AP8_ELISEO_LLAMALA, AP8_SUNAMITA_PUEBLO, AP8_GUIEZI_HIJO, AP8_ELISEO_HIJO, AP8_SUNAMITA_BURLA,
    AP9_JORAM_CARTA, AP9_GUIEZI_PLATA, AP9_ELISEO_PROFETA, AP9_GUIEZI_PRESENTE, AP9_ELISEO_SIN_PLATA,
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
    // --- Variantes: primera caja del jefe cuando el pozo se jugo (SceneDef::alt_*) ------
    // Al FINAL a proposito: anadir aqui no mueve ningun id de arriba.
    J1_CAPITAN_ALT, J3_PROFETAS_ALT,
    COUNT
};
}

// =============================================================================
// Las cajas
// =============================================================================
// Mismo struct que el pozo (EventDef) para que la caja de dialogo no sepa de donde viene el
// texto. `options[0].label == nullptr` es lo que la hace cerrar sola, sin decision.
#define SC_NO_OPTIONS {{nullptr, nullptr}, {nullptr, nullptr}}

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
    // huir. Lo inventado se apoya en el libro: el altar y la imagen de Asera y la sidonia
    // Jezabel (16:30-33), Jezabel que ya mata profetas (18:4), los 450 de Baal (18:19),
    // Acab que lo busca por todos los reinos (18:10) y el manto de pelo (2 R 1:8). La
    // sentencia de 17:1 va tal cual la RV1909, en la cuarta caja.
    // =========================================================================
    case Sc::AUD_REPRENDE: return {
        tr("Elías", "Elijah"),
        {tr("Acab, hijo de Omri. He venido de Galaad a decirte lo que ya sabes.",
            "Ahab, son of Omri. I am come from Gilead to tell thee what thou knowest already."),
         tr("Has hecho lo malo ante los ojos de Jehová, más que todos los que fueron antes de ti.",
            "Thou hast done evil in the sight of the Lord, above all that were before thee."),
         tr("Levantaste altar a Baal en Samaria, y plantaste una imagen de Asera; y los altares de Jehová están derribados.",
            "Thou hast reared up an altar for Baal in Samaria, and made a grove; and the altars of the Lord are thrown down.")},
        {"1 Reyes 16:30-33", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AUD_ACAB_DESPRECIA: return {
        tr("Acab", "Ahab"),
        {tr("¿Quién ha dejado entrar a este? Un tisbita de Galaad, con manto de pelo y el polvo del camino, viene a reprender al rey.",
            "Who let this man in? A Tishbite of Gilead, in a hairy mantle and the dust of the road, come to rebuke the king."),
         tr("Samaria tiene paz, tiene grano y tiene muros nuevos. Sidón nos manda cedro y púrpura, y Baal manda la lluvia.",
            "Samaria hath peace, and corn, and new walls. Sidon sendeth us cedar and purple, and Baal sendeth the rain."),
         nullptr},
        {"1 Reyes 16:31", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AUD_JEZABEL_AMENAZA: return {
        tr("Jezabel", "Jezebel"),
        {tr("Déjalo, señor mío. En Sidón sabemos qué se hace con los que hablan contra los dioses.",
            "Let him be, my lord. In Sidon we know what is done with those who speak against the gods."),
         tr("Los profetas de tu Dios ya han aprendido a callar, Elías. Vuélvete a tu monte.",
            "Thy God's prophets have learned to hold their peace, Elijah. Get thee back to thy mountain."),
         nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::AUD_SENTENCIA: return {
        tr("Elías", "Elijah"),
        {tr("Tú dices que Baal da la lluvia. Pues que la dé.",
            "Thou sayest that Baal giveth the rain. Then let him give it."),
         tr("Vive Jehová Dios de Israel, delante del cual estoy, que no habrá lluvia ni rocío en estos años, sino por mi palabra.",
            "As the Lord God of Israel liveth, before whom I stand, there shall not be dew nor rain these years, but according to my word."),
         nullptr},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AUD_ACAB_BURLA: return {
        tr("Acab", "Ahab"),
        {tr("¿Sequía por tu palabra? Baal tiene cuatrocientos cincuenta profetas en Israel, y tú eres un hombre solo.",
            "Drought, by thy word? Baal hath four hundred and fifty prophets in Israel, and thou art one man."),
         tr("Vete. Y cuando llueva, mis hombres te traerán para que lo veas.",
            "Go. And when the rain cometh, my men shall bring thee back to see it."),
         nullptr},
        {"1 Reyes 18:19", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AUD_JEZABEL_CAZA: return {
        tr("Jezabel", "Jezebel"),
        {tr("No hará falta esperar a la lluvia, señor mío. Galaad no es tan grande, y mis jinetes saben buscar.",
            "There is no need to wait for the rain, my lord. Gilead is not so large, and my horsemen know how to search."),
         nullptr, nullptr},
        {"1 Reyes 18:10", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::AUD_PALABRA_QUERIT: return {
        tr("La palabra de Jehová", "The word of the Lord"),
        {tr("Apártate de aquí, y vuélvete al oriente, y escóndete en el arroyo de Querit, que está frente al Jordán.",
            "Get thee hence, and turn thee eastward, and hide thyself by the brook Cherith, that is before Jordan."),
         tr("Beberás del arroyo; y yo he mandado a los cuervos que te den allí de comer.",
            "Thou shalt drink of the brook; and I have commanded the ravens to feed thee there."),
         nullptr},
        {"1 Reyes 17:2-4", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };

    // =========================================================================
    // Cap. 2 - A la puerta de Sarepta (1 R 17:8-11). Termina en 17:11: el ultimo punado
    // de harina (17:12) y la tinaja son del pozo. El sacerdote que vigila desde la puerta
    // es invencion apoyada en «Sarepta es tierra de Jezabel» y en 18:4; la viuda no da
    // cifras de anos de sequia (la unica cifra del juego es «al tercer ano», 18:1).
    // =========================================================================
    case Sc::AP2_PALABRA: return {
        tr("La palabra de Jehová", "The word of the Lord"),
        {tr("Levántate, vete a Sarepta de Sidón, y mora allí.",
            "Arise, get thee to Zarephath, which belongeth to Zidon, and dwell there."),
         tr("He aquí yo he mandado allí a una mujer viuda que te sustente.",
            "Behold, I have commanded a widow woman there to sustain thee."),
         nullptr},
        {"1 Reyes 17:8-9", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP2_ELIAS_AGUA: return {
        tr("Elías", "Elijah"),
        {tr("Ruégote que me traigas una poca de agua en un vaso, para que beba.",
            "Fetch me, I pray thee, a little water in a vessel, that I may drink."),
         nullptr, nullptr},
        {"1 Reyes 17:10", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP2_VIUDA_AGUA: return {
        tr("La viuda de Sarepta", "The widow of Zarephath"),
        {tr("Agua sí hay, aunque poca: el pozo de la ciudad se seca, y Baal no responde. Espera aquí, que te la traigo.",
            "Water there is, though little: the well of the city faileth, and Baal answereth not. Wait here, and I will bring it thee."),
         nullptr, nullptr},
        {"1 Reyes 17:11", nullptr}, SC_NO_OPTIONS, Portrait::VIUDA_SAREPTA
    };
    case Sc::AP2_ELIAS_PAN: return {
        tr("Elías", "Elijah"),
        {tr("Te ruego que me traigas también un bocado de pan en tu mano.",
            "Bring me, I pray thee, a morsel of bread in thine hand."),
         nullptr, nullptr},
        {"1 Reyes 17:11", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP2_SACERDOTE: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Manto de pelo y polvo de Galaad. Sé quién eres, tisbita: el que cerró el cielo sobre Israel.",
            "A hairy mantle, and the dust of Gilead. I know who thou art, Tishbite: he that shut up the heaven over Israel."),
         tr("Aquí vigila Baal, y yo por él. Mira bien a quién pides pan en Sarepta.",
            "Here Baal keepeth watch, and I for him. Take heed whom thou askest bread of in Zarephath."),
         nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };

    // =========================================================================
    // Cap. 3 - «Ve, muestrate a Acab» (18:1) y Elias ante Acab (1 R 18:17-19, 24): el
    // encuentro que convoca el Carmelo. El profeta de Baal responde a la CONVOCATORIA
    // (los 450 de 18:19) y Elias propone el fuego despues; Acab cierra con el «sea» de
    // 18:20, y lo que el quiere es lluvia (18:1). La burla del profeta es invencion.
    // =========================================================================
    case Sc::AP3_PALABRA: return {
        tr("La palabra de Jehová", "The word of the Lord"),
        {tr("Ve, muéstrate a Acab, y yo haré llover sobre la faz de la tierra.",
            "Go, shew thyself unto Ahab; and I will send rain upon the earth."),
         nullptr, nullptr},
        {"1 Reyes 18:1", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP3_ACAB_TURBAS: return {
        tr("Acab", "Ahab"),
        {tr("¿Eres tú el que turbas a Israel?",
            "Art thou he that troubleth Israel?"),
         tr("Te he buscado por todos los reinos, y a cada uno le hice jurar que no te habían hallado. Y ahora vienes tú a mí.",
            "I have sought thee through every kingdom, and taken an oath of each that they found thee not. And now thou comest to me."),
         nullptr},
        {"1 Reyes 18:17, 10", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::AP3_ELIAS_TURBAS: return {
        tr("Elías", "Elijah"),
        {tr("Yo no he alborotado a Israel, sino tú y la casa de tu padre, dejando los mandamientos de Jehová, y siguiendo a los Baales.",
            "I have not troubled Israel; but thou, and thy father's house, in that ye have forsaken the Lord, and followed Baalim."),
         tr("Envía pues ahora y júntame a todo Israel en el monte de Carmelo.",
            "Now therefore send, and gather to me all Israel unto mount Carmel."),
         tr("Y los cuatrocientos y cincuenta profetas de Baal, y los cuatrocientos profetas de los bosques, que comen de la mesa de Jezabel.",
            "And the four hundred and fifty prophets of Baal, and the four hundred prophets of the groves, which eat at Jezebel's table.")},
        {"1 Reyes 18:18-19", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP3_PROFETA_BAAL: return {
        tr("Profeta de Baal", "Prophet of Baal"),
        {tr("¿Al Carmelo? Baal es señor de la lluvia y jinete de las nubes. Somos cuatrocientos cincuenta, tisbita, y tú uno.",
            "To Carmel? Baal is lord of the rain and rider of the clouds. We are four hundred and fifty, Tishbite, and thou art one."),
         nullptr, nullptr},
        {"1 Reyes 18:19", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::AP3_ELIAS_FUEGO: return {
        tr("Elías", "Elijah"),
        {tr("Invocad luego vosotros el nombre de vuestro dios, y yo invocaré el nombre de Jehová.",
            "And call ye on the name of your gods, and I will call on the name of the Lord."),
         tr("Y el Dios que respondiere por fuego, ése sea Dios.",
            "And the God that answereth by fire, let him be God."),
         nullptr},
        {"1 Reyes 18:24", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP3_ACAB_SEA: return {
        tr("Acab", "Ahab"),
        {tr("Sea. Que suba todo Israel al Carmelo. Trae tu fuego, tisbita; lo que yo quiero de tu Dios es la lluvia.",
            "So be it. Let all Israel go up to Carmel. Bring thy fire, Tishbite; what I want of thy God is the rain."),
         nullptr, nullptr},
        {"1 Reyes 18:20", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };

    // =========================================================================
    // Cap. 4 - El mensajero de Jezabel (1 R 19:1-3), en Jezreel, la manana despues del
    // Carmelo. Elias no explica lo que siente: se va (19:3). Beerseba y «dejo alli a su
    // criado» son 19:3; el enebro (19:4) es el pozo y la intro no lo repite.
    // =========================================================================
    case Sc::AP4_MENSAJERO: return {
        tr("El mensajero de Jezabel", "Jezebel's messenger"),
        {tr("Palabra de la reina, tisbita. Acab le ha contado lo del Carmelo: el fuego, y los profetas degollados junto al Cisón.",
            "Word from the queen, Tishbite. Ahab hath told her of Carmel: the fire, and the prophets slain by the brook Kishon."),
         tr("«Así me hagan los dioses, y así me añadan, si mañana a estas horas yo no haya puesto tu persona como la de uno de ellos».",
            "«So let the gods do to me, and more also, if I make not thy life as the life of one of them by to morrow about this time»."),
         nullptr},
        {"1 Reyes 19:1-2", nullptr}, SC_NO_OPTIONS, Portrait::OFICIAL_REAL
    };
    case Sc::AP4_ELIAS_VAMONOS: return {
        tr("Elías", "Elijah"),
        {tr("Vámonos. Esta noche no dormimos en Jezreel.",
            "Let us go. To night we sleep not in Jezreel."),
         nullptr, nullptr},
        {"1 Reyes 19:3", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP4_CRIADO: return {
        tr("Su criado", "His servant"),
        {tr("¿A dónde, señor mío? Todo Jezreel ha oído el juramento; ninguna casa te abrirá esta noche.",
            "Whither, my lord? All Jezreel hath heard the oath; no house will open to thee to night."),
         nullptr, nullptr},
        {"1 Reyes 19:3", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP4_ELIAS_HUYE: return {
        tr("Elías", "Elijah"),
        {tr("A Beerseba de Judá. Allí te quedas tú.",
            "To Beersheba of Judah. There thou stayest."),
         tr("Yo sigo al desierto, un día de camino. Solo.",
            "I go on into the wilderness, a day's journey. Alone."),
         nullptr},
        {"1 Reyes 19:3-4", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // =========================================================================
    // Cap. 5 - El llamamiento de Eliseo (1 R 19:19-20), junto a las doce yuntas. La
    // primera caja no tiene retrato: es la escena (el manto echado al pasar), y lo demas
    // es 19:20 tal cual. La despedida (19:21) es el pozo; Nabot (21) lo cuenta la intro.
    // El manto sigue en los hombros de Elias hasta 2 R 2:13.
    // =========================================================================
    case Sc::AP5_YUNTAS: return {
        tr("Junto a las doce yuntas", "By the twelve yoke"),
        {tr("Eliseo hijo de Safat ara con la última de las doce yuntas. Elías pasa por delante de él, y echa sobre él su manto.",
            "Elisha the son of Shaphat ploweth with the last of twelve yoke. Elijah passeth by him, and casteth his mantle upon him."),
         nullptr, nullptr},
        {"1 Reyes 19:19", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP5_ELISEO_BESAR: return {
        tr("Eliseo hijo de Safat", "Elisha the son of Shaphat"),
        {tr("Te ruego que me dejes besar a mi padre y a mi madre, y luego te seguiré.",
            "Let me, I pray thee, kiss my father and my mother, and then I will follow thee."),
         nullptr, nullptr},
        {"1 Reyes 19:20", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO_LABRADOR
    };
    case Sc::AP5_ELIAS_VUELVE: return {
        tr("Elías", "Elijah"),
        {tr("Ve, vuelve; ¿qué te he hecho yo?",
            "Go back again: for what have I done to thee?"),
         nullptr, nullptr},
        {"1 Reyes 19:20", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::AP5_ELISEO_BUEYES: return {
        tr("Eliseo hijo de Safat", "Elisha the son of Shaphat"),
        {tr("Hay un par de bueyes que matar, y un pueblo al que dar de comer. Antes de la noche voy tras de ti, señor mío.",
            "There is a yoke of oxen to slay, and a people to feed. Before nightfall I come after thee, my lord."),
         nullptr, nullptr},
        {"1 Reyes 19:21", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO_LABRADOR
    };

    // =========================================================================
    // Cap. 6 - A la orilla del Jordan (2 R 2:15), recien cruzado: los hijos de los
    // profetas de Jerico, que lo vieron desde enfrente (2:7), lo reconocen. El rio
    // abierto dos veces es 2:8 y 2:14; los cincuenta que insisten (2:16-18) son el pozo.
    // =========================================================================
    case Sc::AP6_HIJOS_ESPIRITU: return {
        tr("Los hijos de los profetas", "The sons of the prophets"),
        {tr("Lo vimos desde la orilla, enfrente: el río se abrió dos veces. Una para los dos, y otra para ti solo.",
            "We saw it from the bank, over against you: the river opened twice. Once for you both, and once for thee alone."),
         tr("El espíritu de Elías reposó sobre Eliseo.",
            "The spirit of Elijah doth rest on Elisha."),
         nullptr},
        {"2 Reyes 2:7, 15", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP6_ELISEO_MANTO: return {
        tr("Eliseo", "Elisha"),
        {tr("Levantaos. Este manto se le cayó a él; nadie me lo dio.",
            "Rise up. This mantle fell from him; no man gave it me."),
         tr("Elías ya no está. El Dios de Elías, sí.",
            "Elijah is gone. The God of Elijah is not."),
         nullptr},
        {"2 Reyes 2:13-14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP6_HIJOS_JERICO: return {
        tr("Los hijos de los profetas", "The sons of the prophets"),
        {tr("Ven a Jericó, señor. Cincuenta de los nuestros te esperan allí, y la ciudad tiene un mal que contarte: sus aguas.",
            "Come to Jericho, master. Fifty of us wait for thee there, and the city hath an evil to tell thee of: its waters."),
         nullptr, nullptr},
        {"2 Reyes 2:15-16", nullptr}, SC_NO_OPTIONS, Portrait::HIJO_PROFETAS
    };
    case Sc::AP6_ELISEO_VAMOS: return {
        tr("Eliseo", "Elisha"),
        {tr("Vamos a Jericó. Y no me miréis así: el carro se lo llevó a él, no a mí.",
            "Let us go to Jericho. And look not on me so: the chariot took him, not me."),
         nullptr, nullptr},
        {"2 Reyes 2:11", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 7 - La suplica de la viuda del profeta (2 R 4:1-4), en su casa (decision del
    // usuario, 2026-09-10: los tres reyes de Edom y el tanedor pasan al relato «El valle
    // de las zanjas», events.h). Todo es 4:1-4 salvo Guiezi, que es invencion: el
    // acreedor a la puerta con dos hombres es lo que la viuda dice en 4:1, y la ley
    // (Exodo 21:7; Levitico 25:39) es lo que le da los ninos. El derramamiento (4:5-6) es
    // el pozo, y «vende el aceite» (4:7) el outro.
    // =========================================================================
    case Sc::AP7_VIUDA: return {
        tr("La mujer del profeta", "The prophet's wife"),
        {tr("Tu siervo mi marido es muerto; y tú sabes que tu siervo era temeroso de Jehová.",
            "Thy servant my husband is dead; and thou knowest that thy servant did fear the Lord."),
         tr("Y ha venido el acreedor para tomarse dos hijos míos por siervos.",
            "And the creditor is come to take unto him my two sons to be bondmen."),
         nullptr},
        {"2 Reyes 4:1", nullptr}, SC_NO_OPTIONS, Portrait::MUJER_PROFETA
    };
    case Sc::AP7_ELISEO_QUE_TIENES: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Qué te haré yo? Declárame qué tienes en casa.",
            "What shall I do for thee? Tell me, what hast thou in the house?"),
         nullptr, nullptr},
        {"2 Reyes 4:2", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP7_VIUDA_ACEITE: return {
        tr("La mujer del profeta", "The prophet's wife"),
        {tr("Tu sierva ninguna cosa tiene en casa, sino una botija de aceite.",
            "Thine handmaid hath not any thing in the house, save a pot of oil."),
         nullptr, nullptr},
        {"2 Reyes 4:2", nullptr}, SC_NO_OPTIONS, Portrait::MUJER_PROFETA
    };
    case Sc::AP7_GUIEZI: return {
        tr("Guiezi", "Gehazi"),
        {tr("Señor mío, el acreedor está a la puerta con dos hombres. La ley le da los niños, y no hay con qué pagarle.",
            "My lord, the creditor is at the door with two men. The law giveth him the children, and there is nothing to pay him with."),
         nullptr, nullptr},
        {"2 Reyes 4:1", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP7_ELISEO_VASIJAS: return {
        tr("Eliseo", "Elisha"),
        {tr("Ve, y pide para ti vasos prestados de todos tus vecinos, vasos vacíos, no pocos.",
            "Go, borrow thee vessels abroad of all thy neighbours, even empty vessels; borrow not a few."),
         tr("Entra, y cierra la puerta tras ti y tras tus hijos; echa en todas las vasijas, y la que esté llena, ponla aparte.",
            "Go in, and shut the door upon thee and upon thy sons; pour out into all those vessels, and set aside that which is full."),
         nullptr},
        {"2 Reyes 4:3-4", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 8 - El aposento de Sunem: el ofrecimiento y la promesa (2 R 4:9-16). En 4:9-10
    // la sunamita se lo dice a su marido; aqui se lo dice al profeta, que es quien esta
    // en escena. Los anos en el aposento y la madre en el Carmelo son el pozo; la ultima
    // frase de ella es invencion que mira a 4:28 («¿Pedi yo hijo a mi senor?»).
    // =========================================================================
    case Sc::AP8_SUNAMITA_APOSENTO: return {
        tr("La sunamita", "The Shunammite"),
        {tr("Yo entiendo que este que siempre pasa por nuestra casa, es varón de Dios santo.",
            "I perceive that this is an holy man of God, which passeth by us continually."),
         tr("Te hemos hecho un pequeño aposento de paredes, con cama, mesa, silla y candelero, para cuando vinieres a nosotros.",
            "We have made thee a little chamber on the wall, with a bed, a table, a stool and a candlestick, for when thou comest to us."),
         nullptr},
        {"2 Reyes 4:9-10", nullptr}, SC_NO_OPTIONS, Portrait::SUNAMITA
    };
    case Sc::AP8_ELISEO_LLAMALA: return {
        tr("Eliseo", "Elisha"),
        {tr("Guiezi, dile: He aquí tú has estado solícita por nosotros con todo este esmero; ¿qué quieres que haga por ti?",
            "Gehazi, say unto her: Behold, thou hast been careful for us with all this care; what is to be done for thee?"),
         tr("¿Necesitas que hable por ti al rey, o al general del ejército?",
            "Wouldest thou be spoken for to the king, or to the captain of the host?"),
         nullptr},
        {"2 Reyes 4:13", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP8_SUNAMITA_PUEBLO: return {
        tr("La sunamita", "The Shunammite"),
        {tr("Yo habito en medio de mi pueblo.",
            "I dwell among mine own people."),
         nullptr, nullptr},
        {"2 Reyes 4:13", nullptr}, SC_NO_OPTIONS, Portrait::SUNAMITA
    };
    case Sc::AP8_GUIEZI_HIJO: return {
        tr("Guiezi", "Gehazi"),
        {tr("Señor mío, ella no tiene hijo, y su marido es viejo.",
            "My lord, verily she hath no child, and her husband is old."),
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
         tr("Yo no te he pedido un hijo. Si me lo das, no me lo quites después.",
            "I asked not a son of thee. If thou give him me, take him not from me after."),
         nullptr},
        {"2 Reyes 4:16, 28", nullptr}, SC_NO_OPTIONS, Portrait::SUNAMITA
    };

    // =========================================================================
    // Cap. 9 - La carta del rey de Siria (2 R 5:5-8), en el palacio de Samaria. Eliseo
    // le habla al rey a la cara (5:8, en segunda persona). Las cuentas de Guiezi y su
    // pregunta por el presente son invencion que prepara al jefe del capitulo (5:20).
    // =========================================================================
    case Sc::AP9_JORAM_CARTA: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("«Sabe por ellas que yo envío a ti mi siervo Naamán, para que lo sanes de su lepra». Con el sello de Ben-adad, rey de Siria.",
            "«I have sent Naaman my servant to thee, that thou mayest recover him of his leprosy». Under the seal of Ben-hadad, king of Syria."),
         tr("¿Soy yo Dios, que mate y dé vida? Considerad ahora, y ved cómo busca ocasión contra mí.",
            "Am I God, to kill and to make alive? Consider, I pray you, and see how he seeketh a quarrel against me."),
         nullptr},
        {"2 Reyes 5:6-7", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP9_GUIEZI_PLATA: return {
        tr("Guiezi", "Gehazi"),
        {tr("Con la carta trae diez talentos de plata, seis mil piezas de oro y diez mudas de vestidos. Con la mitad se compran olivares, señor mío.",
            "With the letter he bringeth ten talents of silver, six thousand pieces of gold and ten changes of raiment. Half of it would buy oliveyards, my lord."),
         nullptr, nullptr},
        {"2 Reyes 5:5", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP9_ELISEO_PROFETA: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Por qué has rasgado tus vestidos, rey? Venga ahora a mí, y sabrá que hay profeta en Israel.",
            "Wherefore hast thou rent thy clothes, O king? Let him come now to me, and he shall know that there is a prophet in Israel."),
         nullptr, nullptr},
        {"2 Reyes 5:8", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP9_GUIEZI_PRESENTE: return {
        tr("Guiezi", "Gehazi"),
        {tr("¿Y el presente, señor mío? Lo envía un rey, y a un rey no se le devuelve.",
            "And the present, my lord? A king sendeth it, and to a king a gift is not returned."),
         nullptr, nullptr},
        {"2 Reyes 5:20", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP9_ELISEO_SIN_PLATA: return {
        tr("Eliseo", "Elisha"),
        {tr("Ve a la puerta y dile al sirio que venga. La plata, que se la quede.",
            "Go to the gate and bid the Syrian come. The silver, let him keep it."),
         nullptr, nullptr},
        {"2 Reyes 5:16", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Cap. 10 - El rey avisado (2 R 6:9-13): los avisos de Eliseo al rey de Israel y el
    // juramento del rey de Siria. El criado ya no es Guiezi (5:27): la celda se reusa con
    // el nombre generico de 6:15, como hace el pozo. Ben-adad sabe quien avisa (6:12)
    // en la intro, no aqui.
    // =========================================================================
    case Sc::AP10_ELISEO_AVISO: return {
        tr("Eliseo", "Elisha"),
        {tr("Mira que no pases por tal lugar, porque los sirios van allí.",
            "Beware that thou pass not such a place; for thither the Syrians are come down."),
         tr("Manda a tus hombres por el otro camino.",
            "Send thy men by the other way."),
         nullptr},
        {"2 Reyes 6:9-10", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP10_JORAM_COMO: return {
        tr("Joram, rey de Israel", "Jehoram, king of Israel"),
        {tr("Ni una ni dos veces, varón de Dios. Ben-adad muda su campamento de noche, y tú lo sabes al alba. ¿Quién te lo dice?",
            "Not once nor twice, man of God. Ben-hadad moveth his camp by night, and thou knowest it at dawn. Who telleth thee?"),
         nullptr, nullptr},
        {"2 Reyes 6:10", nullptr}, SC_NO_OPTIONS, Portrait::NONE
    };
    case Sc::AP10_ELISEO_QUIEN: return {
        tr("Eliseo", "Elisha"),
        {tr("El que oye lo que Ben-adad habla en su cámara. Yo solo lo repito.",
            "He that heareth what Ben-hadad speaketh in his bedchamber. I do but repeat it."),
         nullptr, nullptr},
        {"2 Reyes 6:12", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::AP10_CRIADO_JURADO: return {
        tr("El criado de Eliseo", "Elisha's servant"),
        {tr("Señor mío, dicen que el rey de Siria ha jurado prenderte, y que sus jinetes preguntan por ti en cada aldea.",
            "My lord, they say the king of Syria hath sworn to take thee, and his horsemen ask for thee in every village."),
         nullptr, nullptr},
        {"2 Reyes 6:13", nullptr}, SC_NO_OPTIONS, Portrait::CRIADO_ELISEO
    };
    case Sc::AP10_ELISEO_DOTAN: return {
        tr("Eliseo", "Elisha"),
        {tr("A Dotán, y que nos hallen.",
            "To Dothan, and let them find us."),
         nullptr, nullptr},
        {"2 Reyes 6:13", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // =========================================================================
    // Dialogos con el jefe. El jefe habla primero y el profeta responde; la ultima caja
    // es la que abre la pelea. Ninguna caja da por hecha una opcion del pozo (Dotan, el
    // baculo): sus lineas valen para las dos elecciones.
    // =========================================================================
    // Cap. 1 - El Capitan de Acab (1 R 18:10). Cincuenta hombres: 2 R 1:9. NO cita 18:10
    // («Acab ha hecho jurar a los reinos»): esa frase la dice ya el rastreador del pozo. Y si
    // el jugador resolvio al rastreador, la primera caja es la variante (J1_CAPITAN_ALT): el
    // capitan llego siguiendo a su explorador — vale para las dos opciones del relato (el
    // que bebio y se fue tambien pudo contar donde corre el agua).
    case Sc::J1_CAPITAN: return {
        tr("Capitán de Acab", "Ahab's Captain"),
        {tr("Todo Israel te busca, tisbita, y te he hallado yo: seguí el único arroyo que aún lleva agua.",
            "All Israel seeketh thee, Tishbite, and it is I that have found thee: I followed the one brook that yet hath water."),
         tr("El rey te quiere vivo, profeta. Vivo, o en pedazos.",
            "The king will have thee alive, prophet. Alive, or in pieces."),
         nullptr},
        {"1 Reyes 18:10", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_CAPITAN
    };
    case Sc::J1_CAPITAN_ALT: return {
        tr("Capitán de Acab", "Ahab's Captain"),
        {tr("Mi explorador siguió el agua hasta este cañón, y yo he seguido a mi explorador. Aquí estás, tisbita.",
            "My scout followed the water into this canyon, and I have followed my scout. Here thou art, Tishbite."),
         tr("El rey te quiere vivo, profeta. Vivo, o en pedazos.",
            "The king will have thee alive, prophet. Alive, or in pieces."),
         nullptr},
        {"1 Reyes 18:10", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_CAPITAN
    };
    case Sc::J1_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Cada mañana bajan los cuervos con pan y carne. ¿Y el que los manda va a soltarme en tu mano?",
            "Every morning the ravens come down with bread and flesh. And shall he that sendeth them give me up into thy hand?"),
         tr("Vive Jehová, delante de quien estoy.",
            "As the Lord liveth, before whom I stand."),
         nullptr},
        {"1 Reyes 17:6", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J1_CAPITAN_2: return {
        tr("Capitán de Acab", "Ahab's Captain"),
        {tr("Cincuenta hombres por un tisbita. Al rey le parecerán pocos.",
            "Fifty men for one Tishbite. The king will count them too few."),
         nullptr, nullptr},
        {"2 Reyes 1:9", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_CAPITAN
    };

    // Cap. 2 - El Sacerdote de Baal. La amenaza a la viuda se apoya en 18:4.
    case Sc::J2_SACERDOTE: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Baal está airado, y en Sarepta todos saben con quién. Sarepta es de Baal, tisbita.",
            "Baal is wroth, and all Zarephath knoweth with whom. Zarephath is Baal's, Tishbite."),
         nullptr, nullptr},
        {"1 Reyes 17:9", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_SACERDOTE
    };
    case Sc::J2_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Tu dios no ha cerrado el cielo, y no será tu dios quien lo abra.",
            "Thy god shut not the heaven, and it is not thy god that shall open it."),
         nullptr, nullptr},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J2_SACERDOTE_2: return {
        tr("Sacerdote de Baal", "Priest of Baal"),
        {tr("Sidón sabe qué hacer con las viudas que esconden profetas. Cuando acabe contigo, iré a su casa.",
            "Sidon knoweth what to do with widows that hide prophets. When I have done with thee, I go to her house."),
         nullptr, nullptr},
        {"1 Reyes 18:4", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_SACERDOTE
    };

    // Cap. 3 - Los 450 profetas de Baal (1 R 18:26-29, 36). El jefe es un GRUPO (sumo
    // sacerdote + corro, enemies.cpp spawn_boss_group): abre el sumo sacerdote hablando
    // por todos y responde el corro en plural. Su grito ritual es la UNICA exclamacion del
    // juego; la burla de Elias es 18:27 tal cual.
    case Sc::J3_PROFETAS: return {
        tr("El sumo sacerdote de Baal", "The high priest of Baal"),
        {tr("¡Baal, respóndenos! ¡Respóndenos!",
            "O Baal, hear us! Hear us!"),
         tr("Somos muchos, y desde la mañana clamamos. El altar sigue frío. Pero Baal oirá.",
            "We are many, and since morning we have cried. The altar is yet cold. But Baal will hear."),
         nullptr},
        {"1 Reyes 18:26", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_SUMO_SACERDOTE
    };
    // Variante si el jugador se encontro con Abdias camino del monte (Ev::ABDIAS): el sumo
    // sacerdote sabe de los cien escondidos. Vale para las dos opciones (subirlos al Carmelo
    // o dejarlos en las cuevas): el no sabe si han subido.
    case Sc::J3_PROFETAS_ALT: return {
        tr("El sumo sacerdote de Baal", "The high priest of Baal"),
        {tr("¡Baal, respóndenos! ¡Respóndenos!",
            "O Baal, hear us! Hear us!"),
         tr("Abdías, el mayordomo, esconde profetas de Jehová en las cuevas de este monte. Cuando Baal hable, iremos también por ellos.",
            "Obadiah the steward hideth prophets of the Lord in the caves of this mount. When Baal hath spoken, we go for them also."),
         nullptr},
        {"1 Reyes 18:4, 26", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_SUMO_SACERDOTE
    };
    case Sc::J3_ELIAS_BURLA: return {
        tr("Elías", "Elijah"),
        {tr("Gritad en alta voz, que dios es: quizá está conversando, o tiene algún empeño, o va de camino.",
            "Cry aloud: for he is a god; either he is talking, or he is pursuing, or he is in a journey."),
         tr("Tal vez duerme, y hay que despertarle.",
            "Or peradventure he sleepeth, and must be awaked."),
         nullptr},
        {"1 Reyes 18:27", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J3_PROFETAS_2: return {
        tr("Los profetas de Baal", "The prophets of Baal"),
        {tr("Nos sajaremos con cuchillos y con lanzas hasta chorrear la sangre, y Baal bajará.",
            "We will cut ourselves with knives and lancets till the blood gush out, and Baal will come down."),
         nullptr, nullptr},
        {"1 Reyes 18:28", nullptr}, SC_NO_OPTIONS, P_PROFETA_BAAL
    };
    case Sc::J3_ELIAS_FIN: return {
        tr("Elías", "Elijah"),
        {tr("No hubo voz, ni quien respondiese, ni quien escuchase.",
            "There was neither voice, nor any to answer, nor any that regarded."),
         tr("Ha pasado el mediodía. Ahora hablará el Dios de Abraham, de Isaac y de Israel.",
            "Noon is past. Now shall the God of Abraham, of Isaac, and of Israel speak."),
         nullptr},
        {"1 Reyes 18:29, 36", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 4 - Tu propia desesperacion (1 R 19:4, 10), en la cueva de Horeb. Habla con la
    // cara de Elias a proposito: es su propia voz, y abre con el eco del enebro (que el
    // pozo ya jugo) reconocido como eco. La respuesta es 19:10 tal cual; el cierre de
    // Elias es el juramento de 17:1, que es lo unico que le queda.
    case Sc::J4_DESESPERACION: return {
        tr("Tu propia desesperación", "Your own despair"),
        {tr("Basta ya, Jehová; quítame la vida. Lo dijiste bajo el enebro, y era verdad.",
            "It is enough; now, O Lord, take away my life. Thou saidst it under the juniper, and it was true."),
         tr("Un día de fuego en el Carmelo, y luego cuarenta de huida. ¿De qué sirvió?",
            "One day of fire on Carmel, and then forty of flight. What did it serve?"),
         nullptr},
        {"1 Reyes 19:4", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_DESESPERACION
    };
    case Sc::J4_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Sentido he un vivo celo por Jehová Dios de los ejércitos; porque los hijos de Israel han dejado tu alianza, han derribado tus altares,",
            "I have been very jealous for the Lord God of hosts: for the children of Israel have forsaken thy covenant, thrown down thine altars,"),
         tr("y han muerto a cuchillo tus profetas; y yo solo he quedado, y me buscan para quitarme la vida.",
            "and slain thy prophets with the sword; and I, even I only, am left; and they seek my life, to take it away."),
         nullptr},
        {"1 Reyes 19:10", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J4_DESESPERACION_2: return {
        tr("Tu propia desesperación", "Your own despair"),
        {tr("Sólo tú. Jezabel lo juró, y lo cumplirá: mañana serás como uno de ellos.",
            "Thou only. Jezebel hath sworn it, and will keep it: to morrow thou shalt be as one of them."),
         tr("Échate al fondo de la cueva, tisbita. Aquí nadie te busca.",
            "Lie thee down in the back of the cave, Tishbite. Here no man seeketh thee."),
         nullptr},
        {"1 Reyes 19:2", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_DESESPERACION
    };
    case Sc::J4_ELIAS_2: return {
        tr("Elías", "Elijah"),
        {tr("Vive Jehová, delante de quien estoy. Aquí estoy, y en pie.",
            "As the Lord liveth, before whom I stand. Here I am, and on my feet."),
         nullptr, nullptr},
        {"1 Reyes 17:1", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 5 - Acab y Jezabel en la vina de Nabot (1 R 21:20-23). Lo de Jezabel («yo
    // escribi las cartas») es 21:8 dicho por ella; Jezabel no exclama nunca.
    case Sc::J5_ACAB: return {
        tr("Acab", "Ahab"),
        {tr("¿Me has hallado, enemigo mío?",
            "Hast thou found me, O mine enemy?"),
         tr("La viña es mía por sello y por sentencia. Nabot blasfemó contra Dios y el rey; dos testigos lo juraron, y la ciudad lo apedreó.",
            "The vineyard is mine by seal and by sentence. Naboth blasphemed God and the king; two witnesses swore it, and the city stoned him."),
         nullptr},
        {"1 Reyes 21:20", nullptr}, SC_NO_OPTIONS, Portrait::ACAB
    };
    case Sc::J5_ELIAS: return {
        tr("Elías", "Elijah"),
        {tr("Hete encontrado, porque te has vendido a mal hacer delante de Jehová.",
            "I have found thee: because thou hast sold thyself to work evil in the sight of the Lord."),
         tr("He aquí yo traigo mal sobre ti, y barreré tu posteridad.",
            "Behold, I will bring evil upon thee, and will take away thy posterity, and will cut off from Ahab every male in Israel."),
         nullptr},
        {"1 Reyes 21:20-21", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };
    case Sc::J5_JEZABEL: return {
        tr("Jezabel", "Jezebel"),
        {tr("Dos testigos, tisbita. Yo escribí las cartas, y yo puse el sello del rey.",
            "Two witnesses, Tishbite. I wrote the letters, and I set the king's seal."),
         tr("Los perros de Jezreel comen lo que yo les echo.",
            "The dogs of Jezreel eat what I cast them."),
         nullptr},
        {"1 Reyes 21:8", nullptr}, SC_NO_OPTIONS, Portrait::JEZABEL
    };
    case Sc::J5_ELIAS_2: return {
        tr("Elías", "Elijah"),
        {tr("También de ti ha hablado Jehová, Jezabel: los perros te comerán en el muro de Jezreel.",
            "And of thee also hath the Lord spoken, Jezebel: the dogs shall eat thee by the wall of Jezreel."),
         nullptr, nullptr},
        {"1 Reyes 21:23", nullptr}, SC_NO_OPTIONS, Portrait::ELIAS
    };

    // Cap. 6 - El Espiritu de Corrupcion (1 R 16:34; 2 R 2:21). Que un espiritu hable es
    // licencia del jefe, no del libro; lo de Hiel y sus hijos lo conto ya el pozo (16:34).
    case Sc::J6_ESPIRITU: return {
        tr("Espíritu de Corrupción", "Spirit of Corruption"),
        {tr("Yo soy lo que Hiel enterró bajo los cimientos. Bebe de esta agua, hijo de Safat.",
            "I am what Hiel buried under the foundations. Drink of this water, son of Shaphat."),
         nullptr, nullptr},
        {"1 Reyes 16:34", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_ESPIRITU
    };
    case Sc::J6_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Este manto abrió el Jordán esta mañana. Tu manantial no es más ancho que el Jordán.",
            "This mantle opened the Jordan this morning. Thy spring is no wider than the Jordan."),
         nullptr, nullptr},
        {"2 Reyes 2:14", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J6_ESPIRITU_2: return {
        tr("Espíritu de Corrupción", "Spirit of Corruption"),
        {tr("Elías subió en el carro. ¿Y tú quién eres, labrador de Abel-mehola?",
            "Elijah went up in the chariot. And who art thou, ploughman of Abel-meholah?"),
         nullptr, nullptr},
        {"2 Reyes 2:11", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_ESPIRITU
    };
    case Sc::J6_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("Nadie. Pero el Dios de Elías no subió en el carro, y de estas aguas ha dicho: no habrá más en ellas muerte.",
            "No man. But the God of Elijah went not up in the chariot, and of these waters he hath said: there shall be no more death in them."),
         nullptr, nullptr},
        {"2 Reyes 2:21", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 7 - El Acreedor (2 R 4:1-7), a la puerta cerrada de la viuda. Que la ley le da
    // los hijos por siervos es lo que la viuda dice en 4:1; los dos hombres, lo que dijo
    // Guiezi en la apertura. «Vende el aceite y paga» (4:7) se lo dice Eliseo a la viuda:
    // es el outro, no algo que se le recite al acreedor.
    case Sc::J7_ACREEDOR: return {
        tr("El Acreedor", "The Creditor"),
        {tr("El profeta murió debiendo. La ley me da a sus dos hijos por siervos, y traigo dos hombres para llevárselos.",
            "The prophet died in debt. The law giveth me his two sons for bondmen, and I have brought two men to take them."),
         nullptr, nullptr},
        {"2 Reyes 4:1", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_ACREEDOR
    };
    case Sc::J7_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Ese hombre temía a Jehová, y su viuda ha cerrado la puerta. Lo que hay dentro no es tuyo.",
            "That man feared the Lord, and his widow hath shut the door. What is within is not thine."),
         nullptr, nullptr},
        {"2 Reyes 4:1-2", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J7_ACREEDOR_2: return {
        tr("El Acreedor", "The Creditor"),
        {tr("Vasijas prestadas y una viuda a puerta cerrada. Cuando se abra, entrarán mis hombres.",
            "Borrowed vessels and a widow behind a shut door. When it openeth, my men go in."),
         nullptr, nullptr},
        {"2 Reyes 4:5", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_ACREEDOR
    };
    case Sc::J7_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("Cuando se abra, saldrá aceite para pagarte hasta el último siclo. Aparta de esa puerta.",
            "When it openeth, there shall come out oil to pay thee to the last shekel. Stand off from that door."),
         nullptr, nullptr},
        {"2 Reyes 4:7", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 8 - La Muerte (2 R 4:19-21, 27, 33), en el aposento cerrado. Sin retrato: no es
    // nadie. El nino que grito entre los segadores lo conto la intro; aqui la Muerte solo
    // dice donde estaba. La mencion del nino de Sarepta (1 R 17:22) es su burla a Eliseo.
    // Eliseo no da por hecho el baculo (4:29-31 es una opcion del pozo).
    case Sc::J8_MUERTE: return {
        tr("La Muerte", "Death"),
        {tr("Todo lo que respira es mío. También el hijo que prometiste.",
            "All that breatheth is mine. The son thou didst promise also."),
         tr("Su madre lo acostó en tu cama y cerró la puerta. Yo ya estaba dentro, hijo de Safat.",
            "His mother laid him on thy bed and shut the door. I was already within, son of Shaphat."),
         nullptr},
        {"2 Reyes 4:20-21", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_MUERTE
    };
    case Sc::J8_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Su alma está en amargura, y Jehová me lo encubrió. Pero he venido.",
            "Her soul is vexed within her, and the Lord hid it from me. But I am come."),
         nullptr, nullptr},
        {"2 Reyes 4:27", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J8_MUERTE_2: return {
        tr("La Muerte", "Death"),
        {tr("¿Y qué traes, hijo de Safat? Elías devolvió un niño en Sarepta. Tú aún no has devuelto a nadie.",
            "And what bringest thou, son of Shaphat? Elijah gave back a child in Zarephath. Thou hast given back no man yet."),
         nullptr, nullptr},
        {"1 Reyes 17:22", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_MUERTE
    };
    case Sc::J8_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("He cerrado la puerta tras los dos. Aquí estamos el niño, tú, y Jehová.",
            "I have shut the door upon us both. Here are the child, and thou, and the Lord."),
         tr("Y voy a orar a Jehová.",
            "And I will pray unto the Lord."),
         nullptr},
        {"2 Reyes 4:33", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 9 - Guiezi el codicioso (2 R 5:20-27). Todo lo que dicen los dos es del
    // pasaje, salvo la rabia de Guiezi contra Naaman, que es la que 5:20 le supone.
    case Sc::J9_GUIEZI: return {
        tr("Guiezi", "Gehazi"),
        {tr("Tu siervo no ha ido a ninguna parte.",
            "Thy servant went no whither."),
         tr("Sólo hasta la puerta, a despedir al sirio. Se fue con la carne nueva y los carros llenos.",
            "Only to the gate, to see the Syrian off. He went away with his new flesh and his chariots full."),
         nullptr},
        {"2 Reyes 5:25", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_GUIEZI
    };
    case Sc::J9_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("¿No estaba también allí mi corazón, cuando el hombre volvió de su carro a recibirte?",
            "Went not mine heart with thee, when the man turned again from his chariot to meet thee?"),
         tr("Dos talentos de plata y dos vestidos, escondidos en la casa.",
            "Two talents of silver and two changes of garments, hid in the house."),
         nullptr},
        {"2 Reyes 5:23-26", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J9_GUIEZI_2: return {
        tr("Guiezi", "Gehazi"),
        {tr("Naamán se ríe de nosotros con su carne nueva. Tú no quisiste tomar de su mano lo que era tuyo. Yo sí.",
            "Naaman laugheth at us with his new flesh. Thou wouldest not take of his hand what was thine. I would."),
         nullptr, nullptr},
        {"2 Reyes 5:20", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_GUIEZI
    };
    case Sc::J9_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("¿Es tiempo de tomar plata, y de tomar vestidos, olivares, viñas, ovejas, bueyes, siervos y siervas?",
            "Is it a time to receive money, and to receive garments, and oliveyards, and vineyards, and sheep, and oxen, and menservants, and maidservants?"),
         tr("La lepra de Naamán se te pegará a ti, y a tu simiente para siempre.",
            "The leprosy therefore of Naaman shall cleave unto thee, and unto thy seed for ever."),
         nullptr},
        {"2 Reyes 5:26-27", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };

    // Cap. 10 - Ben-adad de Siria (2 R 6:16, 22-25; 7:1). El precio de la cabeza de asno
    // es 6:25; la harina a un siclo, 7:1, que es la profecia que el principe (pozo) niega.
    // Lo de Dotan vale para las dos opciones del pozo (los jinetes entraron ciegos en
    // Samaria y salieron vivos: 6:19-23).
    case Sc::J10_BENADAD: return {
        tr("Ben-adad de Siria", "Ben-hadad of Syria"),
        {tr("Mis jinetes entraron ciegos en Samaria y salieron vivos por tu mano. Hoy no hay mesa puesta, profeta.",
            "My horsemen went blind into Samaria and came out alive by thy hand. There is no table laid to day, prophet."),
         tr("Toda Siria está a la puerta de Samaria por un solo hombre. Entrégate, y la ciudad comerá mañana.",
            "All Syria is at the gate of Samaria for one man. Yield thyself, and the city shall eat to morrow."),
         nullptr},
        {"2 Reyes 6:22-24", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_BENADAD
    };
    case Sc::J10_ELISEO: return {
        tr("Eliseo", "Elisha"),
        {tr("Más son los que están con nosotros que los que están con ellos.",
            "They that be with us are more than they that be with them."),
         nullptr, nullptr},
        {"2 Reyes 6:16", nullptr}, SC_NO_OPTIONS, Portrait::ELISEO
    };
    case Sc::J10_BENADAD_2: return {
        tr("Ben-adad de Siria", "Ben-hadad of Syria"),
        {tr("Desde aquí no se ve ningún carro de fuego. Se ve el hambre de Samaria: una cabeza de asno por ochenta siclos de plata.",
            "From here no chariot of fire is seen. What is seen is the hunger of Samaria: an ass's head for fourscore pieces of silver."),
         nullptr, nullptr},
        {"2 Reyes 6:25", nullptr}, SC_NO_OPTIONS, Portrait::BOSS_BENADAD
    };
    case Sc::J10_ELISEO_2: return {
        tr("Eliseo", "Elisha"),
        {tr("En Dotán tampoco los viste, y estaban allí.",
            "At Dothan thou sawest them not either, and there they were."),
         tr("Mañana a estas horas valdrá el seah de flor de harina un siclo, a la puerta de Samaria.",
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
// Las figuras de una escena `Live` se retiran al cerrarse (finish_scene). Desde el
// 2026-10-03 el jefe del cap. 5 SON Acab y Jezabel (dos entidades de verdad, de pie a dx -2
// y +2 del ancla: game/bosses.cpp spawn_boss_group), asi que su escena ya no planta
// figuras: hablan los mismos que luego pelean, con sus hojas acab.png y jezabel.png.
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
    // La cadena esta partida por sentido y el paseo cae en la juntura: la primera caja es
    // DIOS (la palabra de Jehova, 17:8-9) y las cuatro siguientes son el ENCUENTRO (el
    // agua, el pan, el sacerdote). Plantar a los tres juntos desde la primera caja hacia
    // que el encuentro pareciera un spawn; ahora Elias sale al sur y CAMINA hasta ellos,
    // que ya estan en su sitio y no se mueven.
    case Scene::APERTURA_2: return {
        "Apertura 2", "Apertura del capitulo 2: a la puerta de Sarepta (1 Reyes 17:8-11)", 2,
        StageKind::Exterior, 2, 0, 1, Direction::Up,
        // El sacerdote estaba a dx 5 y al final del paseo la camara lo dejaba justo detras del
        // boton de SALTAR: se corrio una columna adentro (visto en la captura, no calculado).
        {{FK::Npc, Npc::VIUDA_SAREPTA, -1, -2, Direction::Down}, {FK::Enemy, E_PROFETA_BAAL, 4, -3, Direction::Down}, none},
        Sc::AP2_PALABRA, 5, SceneThen::ChapterIntro,
        -1, -1,                  // sin variante de primera caja
        // Paseo tras la caja de la palabra. Sale al SUROESTE, no en linea recta al sur: al
        // sur la camara topa con el borde de la sala y la caja de dialogo lo tapaba entero
        // (el mismo choque que la arena del jefe). Desde aqui se le ve de pie mientras habla
        // Jehova, y la viuda y el sacerdote entran en cuadro conforme el camina.
        0, -9, 2
    };
    // Al pie del Carmelo: Acab con dos profetas de Baal a los flancos.
    case Scene::APERTURA_3: return {
        "Apertura 3", "Apertura del capitulo 3: Elias ante Acab (1 Reyes 18:1, 17-24)", 3,
        StageKind::Exterior, 3, 0, 1, Direction::Up,
        {{FK::Acab, 0, 0, -2, Direction::Down}, {FK::Enemy, E_PROFETA_BAAL, -3, -2, Direction::Down},
         {FK::Enemy, E_PROFETA_BAAL, 3, -2, Direction::Down}},
        Sc::AP3_PALABRA, 6, SceneThen::ChapterIntro
    };
    // Jezreel (el valle: campos del cap. 5): el mensajero de la reina, y el criado detras.
    case Scene::APERTURA_4: return {
        "Apertura 4", "Apertura del capitulo 4: el mensajero de Jezabel (1 Reyes 19:1-3)", 4,
        StageKind::Exterior, 5, 0, 1, Direction::Up,
        {{FK::Enemy, E_SOLDADO_ACAB, 0, -2, Direction::Down}, {FK::Npc, Npc::HIJO_PROFETAS, 2, 1, Direction::Left}, none},
        Sc::AP4_MENSAJERO, 4, SceneThen::ChapterIntro
    };
    // Los campos de Abel-mehola: Eliseo, labrador, junto a Elias.
    case Scene::APERTURA_5: return {
        "Apertura 5", "Apertura del capitulo 5: el llamamiento de Eliseo (1 Reyes 19:19-20)", 5,
        StageKind::Exterior, 5, 0, 0, Direction::Left,
        {{FK::Npc, Npc::ELISEO_LABRADOR, -2, 0, Direction::Right}, none, none},
        Sc::AP5_YUNTAS, 4, SceneThen::ChapterIntro
    };
    // La orilla del Jordan (ribera del cap. 9): dos de los hijos de los profetas.
    case Scene::APERTURA_6: return {
        "Apertura 6", "Apertura del capitulo 6: a la orilla del Jordan (2 Reyes 2:15)", 6,
        StageKind::Exterior, 9, 0, 1, Direction::Up,
        {{FK::Npc, Npc::HIJO_PROFETAS, -1, -2, Direction::Down}, {FK::Npc, Npc::HIJO_PROFETAS, 2, -2, Direction::Down}, none},
        Sc::AP6_HIJOS_ESPIRITU, 4, SceneThen::ChapterIntro
    };
    // La casa de la viuda del profeta (interior del cap. 7): ella al fondo, Guiezi al lado.
    case Scene::APERTURA_7: return {
        "Apertura 7", "Apertura del capitulo 7: la viuda del profeta (2 Reyes 4:1-4)", 7,
        StageKind::Interior, 7, 0, 1, Direction::Up,
        {{FK::Npc, Npc::MUJER_PROFETA, -1, -2, Direction::Down}, {FK::Npc, Npc::CRIADO_ELISEO, 2, 0, Direction::Left}, none},
        Sc::AP7_VIUDA, 5, SceneThen::ChapterIntro
    };
    // El aposento de Sunem: la sunamita al fondo, Guiezi al lado.
    case Scene::APERTURA_8: return {
        "Apertura 8", "Apertura del capitulo 8: el aposento de Sunem (2 Reyes 4:9-16)", 8,
        StageKind::Interior, 8, 0, 1, Direction::Up,
        {{FK::Npc, Npc::SUNAMITA, -1, -2, Direction::Down}, {FK::Npc, Npc::CRIADO_ELISEO, 2, 0, Direction::Left}, none},
        Sc::AP8_SUNAMITA_APOSENTO, 6, SceneThen::ChapterIntro
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
    case Scene::JEFE_1:  return {"Jefe 1",  "Dialogo con el jefe 1: el Capitan de Acab (1 Reyes 18:10)",       1,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J1_CAPITAN,       3, SceneThen::Play,
                                 Sc::J1_CAPITAN_ALT, Ev::RASTREADOR_DE_ACAB};
    case Scene::JEFE_2:  return {"Jefe 2",  "Dialogo con el jefe 2: el Sacerdote de Baal (1 Reyes 18:4)",      2,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J2_SACERDOTE,     3, SceneThen::Play};
    case Scene::JEFE_3:  return {"Jefe 3",  "Dialogo con el jefe 3: los profetas de Baal (1 Reyes 18:26-29)",  3,  StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J3_PROFETAS,      4, SceneThen::Play,
                                 Sc::J3_PROFETAS_ALT, Ev::ABDIAS};
    case Scene::JEFE_4:  return {"Jefe 4",  "Dialogo con el jefe 4: tu propia desesperacion (1 Reyes 19:4-10)", 4, StageKind::Live, 0, 0, 3, Direction::Up, {none, none, none}, Sc::J4_DESESPERACION, 4, SceneThen::Play};
    case Scene::JEFE_5:  return {"Jefe 5",  "Dialogo con el jefe 5: Acab y Jezabel (1 Reyes 21:20-23)",        5,  StageKind::Live, 0, 0, 3, Direction::Up,
                                 {none, none, none}, Sc::J5_ACAB, 4, SceneThen::Play};
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
