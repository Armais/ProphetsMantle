#pragma once

// =============================================================================
// MANTLE - Encuentros biblicos (salas RoomType::Event)
// =============================================================================
//
// Un POZO por capitulo, no un encuentro. Desde los mapas fijos (2026-08-27) el pozo COMPLETO
// se coloca en celdas fijas (`CHAPTER_LAYOUT` en rooms.cpp): toda partida puede ver todos los
// relatos, y quien no explora se los pierde.
//
// El pozo sigue el ORDEN DE 1-2 REYES (pasada de coherencia, 2026-08-31): nada que el prologo
// o un capitulo anterior ya conto se vuelve a jugar como presente FUERA DE ORDEN. Por eso el
// pozo del cap. 1 no tiene audiencia con Acab (Querit viene despues): la audiencia es la
// APERTURA guionizada del capitulo (game/scenes.h, `Scene::AUDIENCIA`, 2026-09-09), que se
// juega ANTES del arroyo, en el orden del libro. Desde el 2026-09-10 TODOS los capitulos
// abren asi (`Scene::opening_of`) y todos los jefes hablan antes de pelear (`Scene::boss_of`);
// lo que esas escenas cuentan tampoco se repite aqui. El cap. 6 no repite el cruce del
// Jordan (lo cuenta la cinematica del Ascenso), y Hazael y la uncion de Jehu (2 R 8-9)
// viven en el cap. 10 y no en el 9: en el libro van DESPUES del sitio de Samaria, y
// ponerlos antes mataba a Ben-adad una sala antes de pelear contra el.
//
// Aqui solo vive el TEXTO; el efecto de cada opcion lo aplica
// Engine::apply_event_choice(id, opcion) usando los sistemas existentes (fe, vida, danio,
// slots de milagro, items).
//
// Regla del proyecto: nada contradice 1-2 Reyes, y `Scripture::ref` NO se pinta nunca.
// Elegir contra el texto (encender tu el fuego del Carmelo, callar ante Acab) es una
// decision del JUGADOR en su partida, no una afirmacion sobre lo que paso.
//
// =============================================================================

#include "core/i18n.h"
#include "core/types.h"
#include "game/lore.h"
#include "render/sprites.h"   // namespace Portrait, namespace Npc

struct EventOption {
    const char* label;    // Lo que elige el jugador
    const char* cost;     // Etiqueta corta del coste ("-15 HP"), puede ser nullptr
    const char* result;   // Lo que se lee tras elegir
};

struct EventDef {
    const char* speaker;      // Quien habla ("La viuda de Sarepta")
    const char* lines[3];     // Lo que dice, terminado en nullptr
    Scripture   verse;        // Referencia del pasaje
    EventOption options[2];   // options[1].label == nullptr -> una sola opcion;
                              // options[0].label == nullptr -> SIN decision: la caja cierra sola
                              // (las cajas de escena de game/scenes.h)
    // Indice en `assets/sprites/portraits.png` (render/sprites.h). Portrait::NONE = sin
    // cara: la caja se dibuja exactamente igual que antes de que existiera este campo.
    // Va el ULTIMO y con valor por defecto a proposito: asi los encuentros que no lo
    // nombran siguen compilando tal cual, sin heredar el retrato 0 por accidente.
    int         portrait = Portrait::NONE;
    // Celda de `assets/sprites/npcs.png` (render/sprites.h): quien ESPERA en la sala, de pie
    // en el mundo. `Npc::NONE` para los encuentros que no son una persona sino la escena —
    // alli la sala marca el sitio con una hoguera, que es lo que habia siempre.
    int         npc = Npc::NONE;
};

// =============================================================================
// Ids de encuentro
// =============================================================================
// Son ESTABLES y estan agrupados por capitulo en orden: `get_chapter_events` devuelve el
// primero y cuantos hay, asi que el orden del enum ES la tabla. Un encuentro nuevo se anade
// AL FINAL DE SU CAPITULO y desplaza a los siguientes; eso es aceptable porque el pozo se
// sortea por partida y ningun id viaja en el guardado por su valor numerico (el Codice
// guarda una mascara, ver systems/save.h).
namespace Ev {
enum : int {
    // --- Acto I ----------------------------------------------------------
    CUERVOS_QUERIT = 0, RASTREADOR_DE_ACAB, ARROYO_SECO,                    // cap 1
    VIUDA_SAREPTA, TINAJA_HARINA, HIJO_DE_LA_VIUDA,                         // cap 2
    ALTAR_CARMELO, ABDIAS, DOS_PENSAMIENTOS, NUBECILLA,                     // cap 3
    BOCA_CUEVA, ENEBRO_ANGEL, TRES_UNCIONES, SIETE_MIL,                     // cap 4
    LLAMADO_ELISEO, VINA_NABOT, TERCER_CAPITAN,                             // cap 5
    // --- Acto II ---------------------------------------------------------
    AGUAS_JERICO, CINCUENTA_BUSCAN, MUCHACHOS_BETEL, HIEL_DE_JERICO,        // cap 6
    ACEITE_VIUDA, ZANJAS_MOAB, MUERTE_EN_LA_OLLA, PANES_CEBADA, REY_DE_MOAB,// cap 7
    APOSENTO_SUNAMITA, NINO_SUNAMITA, HACHA_QUE_FLOTA, TIERRAS_SUNAMITA,    // cap 8
    NAAMAN_PUERTA, MUCHACHA_CAUTIVA, TIERRA_DE_ISRAEL,                      // cap 9
    CRIADO_ELISEO, SIRIOS_DOTAN, CUATRO_LEPROSOS, PRINCIPE_SAMARIA,
    HAZAEL_BENADAD, UNCION_JEHU,                                            // cap 10
    // --- La Ascension (2 Reyes 2:1-14) -----------------------------------
    // No pertenecen a ningun capitulo: son las cuatro paradas del viaje, y se juegan
    // SEGUIDAS al acabar el Acto I. `event_chapter` devuelve 0 para ellas a proposito.
    ASC_GILGAL, ASC_BETEL, ASC_JERICO, ASC_JORDAN,
    // --- El epilogo: "El Legado" (2 Reyes 9; 13:14-21) -------------------
    // Tambien seguidas, y tambien fuera de todo capitulo. Cierra los dos arcos de los
    // profetas y NADA mas (decision del usuario, 2026-08-30: todo relato va anclado a
    // Elias o Eliseo): Jehu/Jezabel cumple la palabra de Elias sobre la vina de Nabot,
    // el arco de Joas es el lecho de muerte de Eliseo y los huesos su ultimo milagro.
    EPI_JEHU, EPI_ARCO_JOAS, EPI_HUESOS,
    COUNT
};

// Las paradas de cada cadena, en orden. Las cadenas no se sortean: se recorren.
// Ojo con el techo: `SaveData::codex_events` es un u64 y COUNT va por 46 de 64.
// Las escenas guionizadas (game/scenes.h) NO viven aqui: son ~90 cajas obligatorias y
// reventarian la mascara; viajan en DialogueState::event_id desplazadas Sc::LINE_BASE.
// Si el pozo vuelve a crecer mas alla de 64 ids hace falta una segunda mascara (v10).
constexpr int ASCENT_STOPS = 4;
constexpr int EPILOGUE_STOPS = 3;
}

// Pozo de un capitulo: ids `first` .. `first + count - 1`.
struct EventPool {
    int first;
    int count;
};

inline EventPool get_chapter_events(int chapter) {
    switch (chapter) {
        case 1:  return {Ev::CUERVOS_QUERIT,    3};
        case 2:  return {Ev::VIUDA_SAREPTA,     3};
        case 3:  return {Ev::ALTAR_CARMELO,     4};
        case 4:  return {Ev::BOCA_CUEVA,        4};
        case 5:  return {Ev::LLAMADO_ELISEO,    3};
        case 6:  return {Ev::AGUAS_JERICO,      4};
        case 7:  return {Ev::ACEITE_VIUDA,      5};
        case 8:  return {Ev::APOSENTO_SUNAMITA, 4};
        case 9:  return {Ev::NAAMAN_PUERTA,     3};
        case 10: return {Ev::CRIADO_ELISEO,     6};
        default: return {0, 0};
    }
}

// A que capitulo pertenece un id (0 si no es de ninguno). Lo pide el Codice para su
// contador "Relatos N/M" y el comando de autoplay.
inline int event_chapter(int id) {
    for (int ch = 1; ch <= 10; ch++) {
        EventPool p = get_chapter_events(ch);
        if (id >= p.first && id < p.first + p.count) return ch;
    }
    return 0;
}

// =============================================================================
// El pozo
// =============================================================================
inline EventDef get_event_def(int id) {
    switch (id) {

    // =========================================================================
    // Cap. 1 - El Arroyo Querit (1 Reyes 17:1-7)
    // =========================================================================
    case Ev::CUERVOS_QUERIT: return {
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
    // El juramento ante Acab (1 R 17:1) es el PROLOGO: aqui, en Querit, lo que hay es la
    // caceria que ese juramento desato. El rastreador viste el uniforme de la casa del rey
    // — misma celda que el Soldado de Acab del Bestiario y que el tercer capitan.
    case Ev::RASTREADOR_DE_ACAB: return {
        tr("El rastreador de Acab", "Ahab's tracker"),
        {tr("Un rastreador del rey baja al cañón siguiendo el agua, y se arrodilla a beber de tu arroyo.", "A king's tracker comes down the canyon following the water, and kneels to drink from your brook."),
         tr("La sequía que juraste seca los pozos de Israel: los que quedan tienen vigía.", "The drought you swore is drying the wells of Israel: those that remain are watched."),
         tr("«Acab ha hecho jurar a reinos enteros que no te han hallado». Aún no te ha visto.", "«Ahab hath taken an oath of whole kingdoms that they found thee not». He has not seen you yet.")},
        {"1 Reyes 18:10", nullptr},
        {{tr("Quedarte inmóvil entre las peñas", "Keep still among the rocks"), nullptr,
          tr("Bebe, llena el odre y se va monte arriba. El escondite de Dios aguanta lo que Dios quiera.", "He drinks, fills his skin and climbs away. God's hiding place holds as long as God wills.")},
         {tr("Salir y desarmarlo", "Step out and disarm him"), "-10 HP",
          tr("Lo mandas al desierto sin lanza y sin odre. Contará dónde corre el agua — y quién la guarda.", "You send him into the desert without spear or skin. He will tell where the water runs — and who keeps it.")}},
        Portrait::OFICIAL_REAL, Npc::OFICIAL_REAL
    };
    case Ev::ARROYO_SECO: return {
        tr("El arroyo seco", "The dried brook"),
        {tr("El agua que bebías baja hoy en un hilo, y mañana no bajará.", "The water you drank runs thin today, and tomorrow it will not run."),
         tr("Dios te escondió aquí. Dios no ha dicho todavía que te muevas.", "God hid you here. God has not yet said to move."),
         nullptr},
        {"1 Reyes 17:7", nullptr},
        {{tr("Quedarte donde te mandó", "Stay where he sent you"), "-12 HP",
          tr("Aguantas dos días con la boca seca, y al tercero el arroyo calla del todo. Sigues donde te mandó.", "You hold out two days dry-mouthed, and on the third the brook falls silent. You stay where he sent you.")},
         {tr("Buscar agua río abajo", "Look for water downstream"), tr("-6 Fe", "-6 Faith"),
          tr("Encuentras una poza turbia y bebes hasta hartarte. Nadie te mandó ir.", "You find a muddy pool and drink your fill. No one sent you.")}},
        Portrait::ARROYO_SECO
    };

    // =========================================================================
    // Cap. 2 - La Viuda de Sarepta (1 Reyes 17:8-24)
    // =========================================================================
    case Ev::VIUDA_SAREPTA: return {
        tr("La viuda de Sarepta", "The widow of Zarephath"),
        {tr("«Vive Jehová tu Dios, que no tengo pan cocido.", "«As the Lord thy God liveth, I have no baked bread."),
         tr("Solo un puñado de harina y un poco de aceite en una vasija.", "Only a handful of meal and a little oil in a cruse."),
         tr("Lo comeremos mi hijo y yo, y nos moriremos».", "My son and I shall eat it, and die».")},
        {"1 Reyes 17:12", nullptr},
        {{tr("Darle tu último pan", "Give her your last bread"), "-15 HP",
          tr("La tinaja no se vació. Ni se vaciará hasta que llueva sobre la tierra.", "The barrel did not empty. Nor will it, until rain falls on the land.")},
         {tr("Seguir tu camino", "Go on your way"), tr("-5 Fe", "-5 Faith"),
          tr("Sigues andando. El llanto del niño te acompaña un buen trecho.", "You walk on. The child's crying follows you a long while.")}},
        Portrait::VIUDA_SAREPTA, Npc::VIUDA_SAREPTA
    };
    case Ev::TINAJA_HARINA: return {
        tr("La tinaja y la vasija", "The barrel and the cruse"),
        {tr("Han pasado días. La mujer levanta la tapa y vuelve a sacar harina.", "Days have passed. The woman lifts the lid and draws meal again."),
         tr("No hay más que ayer, ni menos: hay lo justo, otra vez.", "There is no more than yesterday, nor less: there is enough, again."),
         tr("Fuera, la aldea entera reparte lo que le queda por cucharadas.", "Outside, the whole village doles out what is left by spoonfuls.")},
        {"1 Reyes 17:14-16", nullptr},
        {{tr("Tomar cada día lo justo", "Take each day what suffices"), nullptr,
          tr("Comes lo de un día y dejas la tinaja como estaba. Ni escaseó, ni disminuyó.", "You eat a day's portion and leave the barrel as it was. It did not waste, nor fail.")},
         {tr("Llenar sacos por si acaso", "Fill sacks, just in case"), tr("-15 Fe", "-15 Faith"),
          tr("Cargas grano para un mes. Pesa, y cada libra pesa contra lo que se te prometió.", "You load a month of grain. It weighs, and every pound weighs against the promise.")}},
        Portrait::VIUDA_SAREPTA, Npc::VIUDA_SAREPTA
    };
    case Ev::HIJO_DE_LA_VIUDA: return {
        tr("El hijo de la viuda", "The widow's son"),
        {tr("«¿Qué tengo yo contigo, varón de Dios? ¿Has venido a matar a mi hijo?».", "«What have I to do with thee, thou man of God? Art thou come to slay my son?»."),
         tr("El niño no respira. La madre te lo pone en los brazos y te mira.", "The child does not breathe. The mother lays him in your arms and looks at you."),
         tr("Subes al aposento alto. Nadie ha hecho nunca lo que vas a intentar.", "You carry him to the upper chamber. No one has ever done what you are about to try.")},
        {"1 Reyes 17:17-24", nullptr},
        {{tr("Tenderte sobre el niño", "Stretch yourself upon the child"), "-25 HP",
          tr("Tres veces, clamando. «Jehová Dios mío, vuelva el alma de este niño». Y volvió.", "Three times, crying out. «O Lord my God, let this child's soul come into him again». And it did.")},
         {tr("Consolar a la madre", "Comfort the mother"), tr("-10 Fe", "-10 Faith"),
          tr("Te quedas con ella hasta que amanece. Es todo lo que un hombre puede dar.", "You stay with her until dawn. It is all a man can give.")}},
        Portrait::VIUDA_SAREPTA, Npc::VIUDA_SAREPTA
    };

    // =========================================================================
    // Cap. 3 - El Monte Carmelo (1 Reyes 18)
    // =========================================================================
    case Ev::ALTAR_CARMELO: return {
        tr("El altar del Carmelo", "The altar on Carmel"),
        {tr("Doce piedras, una por cada tribu. La leña está puesta y el buey encima.", "Twelve stones, one for each tribe. The wood is laid and the bullock upon it."),
         tr("Todo Israel mira. Los profetas de Baal llevan medio día gritando en vano.", "All Israel is watching. Baal's prophets have shouted in vain half a day."),
         tr("«Llenad cuatro cántaros de agua, y derramadlos sobre el holocausto».", "«Fill four barrels with water, and pour it on the burnt sacrifice».")},
        {"1 Reyes 18:33", nullptr},
        {{tr("Verter doce cántaros", "Pour twelve barrels"), tr("-20 Fe", "-20 Faith"),
          tr("El agua corre y llena la zanja. Cuando caiga el fuego, nadie podrá dudar.", "The water runs and fills the trench. When the fire falls, no one can doubt.")},
         {tr("Encender tú el fuego", "Kindle the fire yourself"), nullptr,
          tr("Prendes la leña con tus manos. Arde rápido, y no dice nada de Dios.", "You light the wood with your hands. It burns fast, and says nothing of God.")}},
        Portrait::ALTAR_CARMELO
    };
    // Camino del monte, DESPUES de la audiencia con Acab (que es la apertura del capitulo):
    // Abdias ya no tiene que anunciar a Elias (18:7-16); lo que le queda son los cien
    // profetas que sigue escondiendo (18:3-4, 13).
    case Ev::ABDIAS: return {
        tr("Abdías, mayordomo del rey", "Obadiah, the king's steward"),
        {tr("«Yo temo a Jehová desde mi juventud. Cuando Jezabel mataba a los profetas, escondí a cien en dos cuevas, y los sustenté con pan y agua».", "«I fear the Lord from my youth. When Jezebel slew the prophets, I hid an hundred in two caves, and fed them with bread and water»."),
         tr("«Siguen allí, señor mío: cincuenta y cincuenta, sin ver el sol. ¿Qué hago hoy con ellos?».", "«They are there yet, my lord: fifty and fifty, seeing no sun. What shall I do with them to day?»."),
         nullptr},
        {"1 Reyes 18:3-4, 13", nullptr},
        {{tr("Que suban los cien al Carmelo", "Let the hundred go up to Carmel"), "-12 HP",
          tr("Suben de las cuevas con la cara blanca y se ponen entre el pueblo. Cien testigos más de lo que va a caer.", "They come up out of the caves white-faced and stand among the people. An hundred more witnesses of what is to fall.")},
         {tr("Que sigan en las cuevas", "Let them stay in the caves"), tr("-6 Fe", "-6 Faith"),
          tr("Los dejas donde Abdías los puso. Comparte contigo el pan de sus cuevas.", "You leave them where Obadiah put them. He shares with you the bread of his caves.")}},
        Portrait::ABDIAS, Npc::ABDIAS
    };
    case Ev::DOS_PENSAMIENTOS: return {
        tr("Todo el pueblo en el monte", "All the people on the mount"),
        {tr("Han subido de todas las aldeas y están ahí, esperando a ver quién gana.", "They have come up from every village and stand there, waiting to see who wins."),
         tr("«¿Hasta cuándo claudicaréis vosotros entre dos pensamientos?", "«How long halt ye between two opinions?"),
         tr("Si Jehová es Dios, seguidle; y si Baal, id en pos de él». Y no respondieron palabra.", "If the Lord be God, follow him; but if Baal, then follow him». And they answered not a word.")},
        {"1 Reyes 18:21", nullptr},
        {{tr("Obligarles a elegir hoy", "Make them choose today"), "-10 HP",
          tr("Gritas contra el viento hasta quedarte ronco. El silencio no te responde, pero se queda.", "You shout against the wind until you are hoarse. The silence does not answer, but it stays.")},
         {tr("Hacer el milagro y callar", "Work the sign and say nothing"), tr("-8 Fe", "-8 Faith"),
          tr("Que hable el fuego. Guardas el aliento para lo que viene después.", "Let the fire speak. You save your breath for what comes after.")}},
        Portrait::DOS_PENSAMIENTOS
    };
    // Cronologicamente va DESPUES del jefe (los profetas de Baal, 18:26-29): la primera
    // frase lleva la marca de flash-forward (pacto de «Siete anos despues» / «Roto el cerco»).
    case Ev::NUBECILLA: return {
        tr("Después del fuego", "After the fire"),
        {tr("Caído el fuego, estás en la cumbre del Carmelo con la cara entre las rodillas. Todavía no ha llovido.", "The fire fallen, you are on the top of Carmel with your face between your knees. Still no rain has come."),
         tr("Seis veces ha subido el criado a mirar el mar, y seis veces: «no hay nada».", "Six times the servant has gone up to look at the sea, and six times: «there is nothing»."),
         tr("A la séptima: «una pequeña nube como la palma de la mano».", "The seventh time: «a little cloud like a man's hand».")},
        {"1 Reyes 18:41-46", nullptr},
        {{tr("Correr delante del carro", "Run before the chariot"), "-20 HP",
          tr("Ciñes tus lomos y corres hasta Jezreel por delante de los caballos del rey.", "You gird up thy loins and run to Jezreel before the king's horses.")},
         {tr("Esperar a que pase la lluvia", "Wait out the rain"), tr("-10 Fe", "-10 Faith"),
          tr("El cielo se pone negro y llueve sobre Israel. Tú la ves caer desde el monte.", "The heaven grows black and rain falls on Israel. You watch it come down from the mount.")}},
        Portrait::NUBECILLA
    };

    // =========================================================================
    // Cap. 4 - La Cueva de Horeb (1 Reyes 19:1-18)
    // =========================================================================
    case Ev::BOCA_CUEVA: return {
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
    case Ev::ENEBRO_ANGEL: return {
        tr("Bajo el enebro", "Under the juniper"),
        {tr("Un día de camino por el desierto, y te sientas debajo del primer arbusto que da sombra.", "A day's journey into the wilderness, and you sit down under the first bush that gives shade."),
         tr("«Basta ya, Jehová, quítame la vida, porque no soy yo mejor que mis padres».", "«It is enough; now, O Lord, take away my life; for I am not better than my fathers»."),
         tr("Te duermes. Alguien te toca y hay una torta cocida y una vasija de agua a tu cabecera.", "You sleep. Someone touches you, and there is a cake baken and a cruse of water at your head.")},
        {"1 Reyes 19:4-8", nullptr},
        {{tr("Comer y volver a dormir", "Eat, and sleep again"), nullptr,
          tr("«Levántate y come, porque largo camino te resta». Con esa comida anduviste cuarenta días.", "«Arise and eat, because the journey is too great for thee». In that meat's strength you walked forty days.")},
         {tr("Seguir sin probar bocado", "Walk on without eating"), "-15 HP",
          tr("Andas hasta que el sol cae. La cabeza aguanta más que el cuerpo, pero no mucho más.", "You walk until the sun drops. The head lasts longer than the body, but not much longer.")}},
        Portrait::ENEBRO_ANGEL
    };
    case Ev::TRES_UNCIONES: return {
        tr("Las tres unciones", "The three anointings"),
        {tr("«Vuelve por tu camino, por el desierto de Damasco.", "«Go, return on thy way to the wilderness of Damascus."),
         tr("Ungirás a Hazael por rey de Siria, y a Jehú por rey sobre Israel,", "Anoint Hazael to be king over Syria, and Jehu to be king over Israel,"),
         tr("y a Eliseo hijo de Safat ungirás para que sea profeta en tu lugar».", "and Elisha the son of Shaphat shalt thou anoint to be prophet in thy room».")},
        {"1 Reyes 19:15-16", nullptr},
        {{tr("Ungir a los tres", "Anoint all three"), tr("-20 Fe", "-20 Faith"),
          tr("Pones aceite sobre dos reyes que aún no reinan y un profeta que aún ara. Lo que viene después ya no es tuyo.", "You pour oil on two kings who do not yet reign and a prophet still at the plough. What comes after is no longer yours.")},
         {tr("Ungir solo a Eliseo", "Anoint Elisha only"), nullptr,
          tr("Te quedas con la parte que entiendes. Los reyes vendrán igual, y sin ti.", "You keep the part you understand. The kings will come all the same, and without you.")}},
        Portrait::TRES_UNCIONES
    };
    case Ev::SIETE_MIL: return {
        tr("Los siete mil", "The seven thousand"),
        // 19:10 («solo yo he quedado») lo dice Elias ante el jefe (scenes.h): aqui, la respuesta.
        {tr("Dijiste: sólo yo he quedado. Y la voz apacible responde con una cuenta.", "You said: I only am left. And the still voice answers with a reckoning."),
         tr("«Y yo haré que queden en Israel siete mil,", "«Yet I have left me seven thousand in Israel,"),
         tr("cuyas rodillas no se doblaron ante Baal, y cuyas bocas no lo besaron».", "all the knees which have not bowed unto Baal, and every mouth which hath not kissed him».")},
        {"1 Reyes 19:18", nullptr},
        {{tr("Volver por el camino del desierto", "Go back by the wilderness"), "-10 HP",
          tr("No estabas solo. Nunca lo estuviste. Desandas el desierto con eso a cuestas.", "You were not alone. You never were. You walk the desert back with that on your shoulders.")},
         {tr("Quedarte en la cueva", "Stay in the cave"), tr("-10 Fe", "-10 Faith"),
          tr("Duermes al abrigo de la roca. Siete mil rodillas siguen firmes sin que tú lo veas.", "You sleep in the shelter of the rock. Seven thousand knees stand firm without your seeing it.")}},
        Portrait::SIETE_MIL
    };

    // =========================================================================
    // Cap. 5 - El Llamado (1 Reyes 19:19-21; 21; 2 Reyes 1)
    // =========================================================================
    // El llamamiento (19:19-20) es la apertura del capitulo; aqui, la despedida (19:21).
    case Ev::LLAMADO_ELISEO: return {
        tr("La despedida de Eliseo", "Elisha's farewell"),
        {tr("Eliseo se vuelve al surco, toma un par de bueyes, y los mata.", "Elisha turns back to the furrow, takes a yoke of oxen, and slays them."),
         tr("Con los aperos de los bueyes cuece la carne, y la da al pueblo, y comen.", "With the instruments of the oxen he boils the flesh, and gives it to the people, and they eat."),
         tr("«Siéntate con nosotros, señor mío. Después iré tras de ti».", "«Sit thou with us, my lord. After that I will go after thee».")},
        {"1 Reyes 19:21", nullptr},
        {{tr("Comer con ellos", "Eat with them"), tr("consumible", "consumable"),
          tr("Comes con ellos hasta que cae el sol. Después se levanta, y va tras de ti, y te sirve.", "You eat with them until the sun goes down. Then he arises, and goes after you, and ministers unto you.")},
         {tr("Seguir camino sin esperarle", "Go on without waiting for him"), nullptr,
          tr("Sigues solo. Te alcanza antes de la noche, con el olor del humo en la ropa.", "You go on alone. He overtakes you before night, with the smell of the smoke on his clothes.")}},
        Portrait::ELISEO_LABRADOR, Npc::ELISEO_LABRADOR
    };
    // La PALABRA sobre la vina (21:17-19), sin Acab presente: a Acab se le ve en el jefe
    // del capitulo (scenes.h, 21:20-23). Sin retrato ni PNJ: es la escena, y la sala pone la
    // hoguera. Sin retrato la caja tiene 3 lineas de 280 u.
    case Ev::VINA_NABOT: return {
        tr("La palabra de Jehová", "The word of the Lord"),
        {tr("«Levántate, desciende a encontrarte con Acab rey de Israel; he aquí él está en la viña de Nabot, a la cual ha descendido a tomar posesión».", "«Arise, go down to meet Ahab king of Israel; behold, he is in the vineyard of Naboth, whither he is gone down to possess it»."),
         tr("«Y le hablarás diciendo: Así ha dicho Jehová: ¿No mataste, y también has despojado?».", "«And thou shalt speak unto him, saying, Thus saith the Lord, Hast thou killed, and also taken possession?»."),
         tr("Es toda la palabra. Hay que bajar a decirla a la cara del rey, con la reina y la guardia al lado.", "That is the whole of the word. It must be carried down and said to the king's face, with the queen and the guard beside him.")},
        {"1 Reyes 21:17-19", nullptr},
        {{tr("Bajar con la palabra entera", "Go down with the whole word"), "-15 HP",
          tr("Bajas a Jezreel con todo lo que has oído, y lo llevas hasta la puerta de la viña.", "You go down to Jezreel with all you have heard, and carry it to the gate of the vineyard.")},
         {tr("Bajar y callar lo peor", "Go down and keep back the worst"), tr("-12 Fe", "-12 Faith"),
          tr("Bajas con la mitad de la palabra. Lo de los perros se te queda dentro.", "You go down with half the word. The part about the dogs stays inside you.")}}
    };
    case Ev::TERCER_CAPITAN: return {
        tr("El tercer capitán", "The third captain"),
        // Va DESPUES del jefe (Acab muere en 22:37): la primera frase lleva la marca.
        {tr("Muerto Acab, reinó Ocozías su hijo. Cayó por la celosía de su sala, y ha mandado consultar a Baal-zebub, dios de Ecrón.", "Ahab dead, Ahaziah his son reigned. He fell through the lattice of his upper chamber, and hath sent to enquire of Baal-zebub the god of Ekron."),
         tr("Dos cincuentenas han subido a por ti, y dos cincuentenas ha consumido el fuego.", "Two companies of fifty came up for you, and fire consumed two companies of fifty."),
         tr("El tercero sube solo, dobla la rodilla y no ordena nada: ruega por la vida de sus cincuenta.", "The third comes up alone, bows his knees and commands nothing: he begs for the life of his fifty.")},
        {"2 Reyes 1:13-15", nullptr},
        {{tr("Perdonarle y bajar con él", "Spare him and go down"), nullptr,
          tr("«Desciende con él, no tengas miedo de él». Bajas al palacio andando entre los que venían a prenderte.", "«Go down with him: be not afraid of him». You walk into the palace among the men sent to take you.")},
         {tr("Que baje fuego también sobre él", "Let fire fall on him too"), tr("-25 Fe", "-25 Faith"),
          tr("Cincuenta y uno más. Nadie volverá a subir a este monte, y nadie volverá a escucharte tampoco.", "Fifty-one more. No one will climb this hill again, and no one will listen to you again either.")}},
        Portrait::OFICIAL_REAL, Npc::OFICIAL_REAL
    };
    // =========================================================================
    // Cap. 6 - Las Aguas de Jerico (2 Reyes 2:13-25)
    // =========================================================================
    case Ev::AGUAS_JERICO: return {
        tr("Los hombres de Jericó", "The men of Jericho"),
        {tr("«La situación de esta ciudad es buena, como mi señor ve.", "«The situation of this city is pleasant, as my lord seeth."),
         tr("Mas las aguas son malas, y la tierra estéril».", "But the water is naught, and the ground barren»."),
         tr("Te traen una vasija nueva, y sal dentro de ella.", "They bring you a new cruse, with salt in it.")},
        {"2 Reyes 2:19-20", nullptr},
        // El manantial NO se sana aqui: sanarlo es el jefe del capitulo (el Espiritu de
        // Corrupcion, scenes.h) y «fueron sanas las aguas» es el outro.
        {{tr("Llevar la sal al manantial", "Carry the salt to the spring"), tr("-10 Fe", "-10 Faith"),
          tr("Tomas la vasija y la llevas al manantial. Lo que vive en el agua ya sabe que vienes.", "You take the cruse and carry it to the spring. What lives in the water knows already that you are coming.")},
         {tr("Beber del agua igualmente", "Drink the water anyway"), "-10 HP",
          tr("El agua raspa la garganta. Pero la ciudad ve que no la temes.", "The water scrapes your throat. But the city sees you do not fear it.")}},
        Portrait::HOMBRE_JERICO, Npc::HOMBRE_JERICO
    };
    // El cruce del Jordan con el manto (2 R 2:13-14) lo cuenta la cinematica del Ascenso:
    // aqui, en Jerico, lo que sigue es 2 R 2:15-18 — los cincuenta que quieren salir a
    // buscar a Elias por montes y valles.
    case Ev::CINCUENTA_BUSCAN: return {
        tr("Los cincuenta de Jericó", "The fifty of Jericho"),
        {tr("«El espíritu de Elías reposó sobre Eliseo». Los hijos de los profetas se inclinan hasta tierra.", "«The spirit of Elijah doth rest on Elisha». The sons of the prophets bow to the ground."),
         tr("«Hay aquí cincuenta varones fuertes: que vayan y busquen a tu señor.", "«Behold, there be with thy servants fifty strong men: let them go and seek thy master."),
         tr("Quizá lo ha levantado el Espíritu y lo ha echado en algún monte o en algún valle».", "Peradventure the Spirit hath taken him up, and cast him upon some mountain, or into some valley».")},
        {"2 Reyes 2:16-18", nullptr},
        {{tr("Ceder y dejarles buscar", "Yield, and let them search"), tr("-10 Fe", "-10 Faith"),
          tr("Insisten hasta avergonzarte, y cedes. Tres días buscan por montes y valles. «¿No os dije que no fueseis?».", "They urge you till you are ashamed, and you yield. Three days they search hill and valley. «Did I not say unto you, Go not?».")},
         {tr("«No enviéis»", "«Ye shall not send»"), nullptr,
          tr("Lo dices una vez y no lo repites. Lo que viste subir en el torbellino no está en ningún valle.", "You say it once and do not say it again. What you saw go up in the whirlwind lies in no valley.")}},
        Portrait::HIJO_PROFETAS, Npc::HIJO_PROFETAS
    };
    case Ev::MUCHACHOS_BETEL: return {
        tr("En el camino de Betel", "On the way up to Bethel"),
        {tr("Salen del pueblo detrás de ti y no vienen a preguntar nada.", "They come out of the city behind you, and they come to ask nothing."),
         tr("«¡Calvo, sube! ¡Calvo, sube!». Saben lo que le pasó a tu maestro y se ríen de ello.", "«Go up, thou bald head! Go up!». They know what became of your master, and they laugh at it."),
         tr("Se ríen del que subió en el torbellino, y de ti, que te quedaste con su manto.", "They laugh at him that went up in the whirlwind, and at you, that were left his mantle.")},
        {"2 Reyes 2:23-25", nullptr},
        {{tr("Seguir camino sin volverte", "Walk on without turning"), nullptr,
          tr("Subes al monte Carmelo con la burla a la espalda. Que la lleve quien la ha dicho.", "You go up to mount Carmel with the mockery behind you. Let it stay with those who said it.")},
         {tr("Volverte y maldecirlos", "Turn back and curse them"), tr("-20 Fe", "-20 Faith"),
          tr("Los maldijiste en el nombre de Jehová. Y salieron dos osos del monte, y despedazaron de ellos a cuarenta y dos muchachos.", "You cursed them in the name of the Lord. And there came forth two she bears out of the wood, and tare forty and two children of them.")}},
        Portrait::NONE
    };
    case Ev::HIEL_DE_JERICO: return {
        tr("Un anciano de Jericó", "An elder of Jericho"),
        {tr("«En días de Acab, Hiel de Bet-el reedificó esta ciudad sobre el juramento de Josué.", "«In the days of Ahab, Hiel of Bethel built this city again upon the oath of Joshua."),
         tr("Echó el cimiento sobre Abiram su primogénito, y sobre Segub su hijo menor puso las puertas».", "He laid the foundation in Abiram his firstborn, and set up the gates in his youngest son Segub»."),
         tr("Una ciudad levantada al precio de dos hijos. Por eso nadie se extraña de que sus aguas sean malas.", "A city raised at the price of two sons. No one wonders that its waters are naught.")},
        {"1 Reyes 16:34", nullptr},
        {{tr("Orar sobre los cimientos", "Pray over the foundations"), tr("-10 Fe", "-10 Faith"),
          tr("Nombras a los dos muchachos delante de Dios. La palabra jurada se cumplió entera; la misericordia también puede cumplirse.", "You name the two boys before God. The sworn word was fulfilled to the letter; mercy can be fulfilled too.")},
         {tr("Beber de sus aguas de paso", "Drink of its waters in passing"), "-8 HP",
          tr("El agua mala raspa. Lo que Hiel enterró sigue debajo, esperando la sal.", "The bad water scrapes. What Hiel buried is still underneath, waiting for the salt.")}},
        Portrait::HOMBRE_JERICO, Npc::HOMBRE_JERICO
    };

    // =========================================================================
    // Cap. 7 - El Aceite de la Viuda (2 Reyes 3; 4:1-7, 38-44)
    // =========================================================================
    // La suplica (4:1-4) es la apertura del capitulo; aqui, el derramamiento (4:5-6).
    case Ev::ACEITE_VIUDA: return {
        tr("La mujer del profeta muerto", "The dead prophet's wife"),
        {tr("Ha cerrado la puerta tras sí y tras sus hijos, y ellos le traen las vasijas, y ella echa el aceite.", "She has shut the door upon her and upon her sons, and they bring the vessels to her, and she pours out."),
         tr("«Tráeme aún otra vasija», dice a su hijo. Y él le trae la última que había en casa.", "«Bring me yet a vessel», she says to her son. And he brings her the last there was in the house."),
         tr("Las de casa están llenas, y el aceite sigue. Los vecinos tienen más.", "Those of the house are full, and the oil still runs. The neighbours have more.")},
        {"2 Reyes 4:5-6", nullptr},
        {{tr("Llenar hasta la última vasija de la aldea", "Fill to the last vessel in the village"), "-20 HP",
          tr("El aceite no se detuvo hasta que no quedó vasija vacía en toda la aldea.", "The oil stayed not until there was not an empty vessel left in the whole village.")},
         {tr("Llenar las de casa y parar", "Fill those of the house and stop"), nullptr,
          tr("«No hay más vasijas». Y el aceite se detuvo. Alcanza para hoy, y no para la deuda.", "«There is not a vessel more». And the oil stayed. Enough for to day, and not for the debt.")}},
        Portrait::MUJER_PROFETA, Npc::MUJER_PROFETA
    };
    // Los tres reyes en el desierto de Edom (3:9-15: Josafat que pregunta por un profeta, el
    // tanedor y la mano de Jehova) viven aqui desde el 2026-09-10 (decision del usuario: la
    // apertura del capitulo es la viuda del profeta). Sin retrato: es la escena.
    case Ev::ZANJAS_MOAB: return {
        tr("El valle de las zanjas", "The valley of ditches"),
        {tr("Siete días de rodeo por el desierto de Edom, y no queda agua para el ejército ni para las bestias. Josafat pregunta si hay aquí profeta de Jehová.", "Seven days' march about the wilderness of Edom, and no water for the host nor for the cattle. Jehoshaphat asks if there be here a prophet of the Lord."),
         tr("«Traedme ahora un tañedor». Y mientras el tañedor toca, la mano de Jehová viene sobre ti.", "«Bring me a minstrel». And as the minstrel plays, the hand of the Lord comes upon you."),
         tr("«Haced en este valle muchos estanques. No veréis viento, ni veréis lluvia; pero este valle será lleno de agua».", "«Make this valley full of ditches. Ye shall not see wind, neither shall ye see rain; yet that valley shall be filled with water».")},
        {"2 Reyes 3:9-23", nullptr},
        {{tr("Cavar el valle entero", "Dig the whole valley"), "-15 HP",
          tr("Cavan hasta la noche sin una nube encima. Al alba el valle está lleno, y rojo como sangre para los de Moab.", "They dig till night without a cloud above. At dawn the valley is full, and red as blood to the Moabites.")},
         {tr("Esperar a que llueva", "Wait for rain"), tr("-10 Fe", "-10 Faith"),
          tr("El cielo sigue limpio dos días más. Los reyes empiezan a mirarte de otra manera.", "The sky stays clear two more days. The kings begin to look at you differently.")}},
        Portrait::NONE
    };
    case Ev::MUERTE_EN_LA_OLLA: return {
        tr("La olla de los profetas", "The prophets' pot"),
        {tr("Hay hambre en la tierra y uno ha salido al campo a coger hierbas para el guiso.", "There is a dearth in the land, and one went out to gather herbs for the pottage."),
         tr("Trajo calabazas silvestres a falda llena, sin saber lo que eran.", "He brought wild gourds his lap full, and knew them not."),
         tr("«¡Varón de Dios, hay muerte en esa olla!». Y no la pudieron comer.", "«O thou man of God, there is death in the pot!». And they could not eat.")},
        {"2 Reyes 4:38-41", nullptr},
        {{tr("Echar harina en la olla", "Cast meal into the pot"), tr("-10 Fe", "-10 Faith"),
          tr("«Da de comer a la gente». Y no hubo más mal en la olla.", "«Pour out for the people». And there was no harm in the pot.")},
         {tr("Tirar el guisado", "Throw the pottage out"), "-8 HP",
          tr("Vuelcas la olla en la tierra. Nadie se envenena, y nadie cena.", "You empty the pot on the ground. No one is poisoned, and no one eats.")}},
        Portrait::HIJO_PROFETAS, Npc::HIJO_PROFETAS
    };
    case Ev::PANES_CEBADA: return {
        tr("El hombre de Baal-salisa", "The man of Baal-shalisha"),
        {tr("Ha venido con las primicias en el zurrón: veinte panes de cebada y grano nuevo.", "He has come with the firstfruits in his sack: twenty loaves of barley and new corn."),
         tr("«¿Cómo he de poner esto delante de cien hombres?».", "«What, should I set this before an hundred men?»."),
         nullptr},
        {"2 Reyes 4:42-44", nullptr},
        {{tr("Repartirlo entre los cien", "Set it before the hundred"), tr("-12 Fe", "-12 Faith"),
          tr("«Comerán, y sobrará». Comieron, y sobró conforme a la palabra de Jehová.", "«They shall eat, and shall leave thereof». They did eat, and left thereof, according to the word of the Lord.")},
         {tr("Guardarlos para el camino", "Keep them for the road"), tr("-8 Fe", "-8 Faith"),
          tr("Cargas los panes en el zurrón. Pesan poco y alimentan menos de lo que podrían.", "You load the loaves in your sack. They weigh little, and feed fewer than they might.")}},
        Portrait::HOMBRE_BAALSALISA, Npc::HOMBRE_BAALSALISA
    };
    case Ev::REY_DE_MOAB: return {
        tr("Sobre el muro de Moab", "On the wall of Moab"),
        {tr("Mesa, rey de Moab, ve la batalla perdida: probó a romper el cerco con setecientas espadas, y no pudo.", "Mesha, king of Moab, sees the battle lost: he tried to break through with seven hundred swords, and could not."),
         tr("Entonces toma a su hijo primogénito, el que había de reinar, y lo sube al muro, a la vista de los tres ejércitos.", "Then he takes his eldest son, that should have reigned, and brings him up on the wall, in the sight of the three armies."),
         tr("Lo que hace allí no debe hacerse. Y hubo grande enojo contra Israel.", "What he does there ought not to be done. And there was great indignation against Israel.")},
        {"2 Reyes 3:26-27", nullptr},
        {{tr("Levantar el sitio", "Raise the siege"), tr("-3 de daño", "-3 damage"),
          tr("Israel se aparta del muro y se vuelve a su tierra. Hay victorias que no se pueden cobrar.", "Israel departs from the wall and returns to its own land. There are victories no one can collect.")},
         {tr("Quedarte a tomar la ciudad", "Stay and take the city"), tr("-20 Fe", "-20 Faith"),
          tr("El muro te gana. Nadie que vio aquello peleó ya como antes, ni tú tampoco.", "The wall defeats you. No one who saw that fought as before again — nor do you.")}},
        Portrait::NONE
    };

    // =========================================================================
    // Cap. 8 - El Hijo Sunamita (2 Reyes 4:8-37; 6:1-7; 8:1-6)
    // =========================================================================
    // El ofrecimiento del aposento y la promesa (4:9-16) son la apertura del capitulo:
    // aqui, los anos que siguen (4:11, 18), con el nino ya corriendo por el patio.
    case Ev::APOSENTO_SUNAMITA: return {
        tr("La sunamita", "The Shunammite woman"),
        {tr("Pasan los años, y cada vez que pasas por Sunem hay pan en su mesa y una cama en el aposento de paredes.", "Years pass, and every time you pass by Shunem there is bread on her table and a bed in the chamber on the wall."),
         tr("El niño que prometiste corre por el patio, y su madre no te pide nada.", "The child you promised runs about the court, and his mother asks nothing of you."),
         nullptr},
        {"2 Reyes 4:11, 18", nullptr},
        {{tr("Quedarte en el aposento", "Stay in the chamber"), tr("-15 HP máx", "-15 max HP"),
          tr("Descansas bajo techo por primera vez en años. Si caes aquí, alguien te levantará.", "You rest under a roof for the first time in years. If you fall here, someone will lift you up.")},
         {tr("Seguir camino", "Go on your way"), nullptr,
          tr("Bendices la casa desde la puerta y sigues hacia el Carmelo. Duermes fuera, y duermes en paz.", "You bless the house from the door and go on toward Carmel. You sleep outside, and you sleep in peace.")}},
        Portrait::SUNAMITA, Npc::SUNAMITA
    };
    // La madre llega al Carmelo (4:25-30). El nino NO resucita aqui: el aposento cerrado
    // es el jefe del capitulo (la Muerte, scenes.h) y los siete estornudos son el outro.
    case Ev::NINO_SUNAMITA: return {
        tr("El niño de Sunem", "The child of Shunem"),
        {tr("La ves venir de lejos por el camino del Carmelo, sobre el asna, sin aflojar el paso.", "You see her afar off on the way to Carmel, upon the ass, slacking not her riding."),
         tr("«¿Pedí yo hijo a mi señor? ¿No dije que no me engañases?». Se ha abrazado a tus pies, y Guiezi quiere apartarla.", "«Did I desire a son of my lord? did I not say, Do not deceive me?». She has caught you by the feet, and Gehazi would thrust her away."),
         tr("«Vive Jehová, y vive tu alma, que no te dejaré».", "«As the Lord liveth, and as thy soul liveth, I will not leave thee».")},
        {"2 Reyes 4:25-31", nullptr},
        {{tr("Ir tú, detrás de ella", "Go yourself, after her"), "-25 HP",
          tr("Te levantas y la sigues. El camino hasta Sunem se hace largo, y la puerta del aposento está cerrada.", "You arise and follow her. The way to Shunem is long, and the door of the chamber is shut.")},
         {tr("Mandar a Guiezi con el báculo", "Send Gehazi with the staff"), tr("-15 Fe", "-15 Faith"),
          tr("«Toma mi báculo, y ponlo sobre el rostro del niño». Volvió a tu encuentro: no hubo voz ni sentido.", "«Take my staff, and lay it upon the face of the child». He came back to meet you: there was neither voice nor hearing.")}},
        Portrait::SUNAMITA, Npc::SUNAMITA
    };
    case Ev::HACHA_QUE_FLOTA: return {
        tr("El hacha prestada", "The borrowed axe"),
        {tr("Cortaban vigas junto al Jordán para levantar una casa más grande.", "They were felling beams by the Jordan to build a wider house."),
         tr("A uno se le soltó el hierro del mango y se le fue al agua.", "As one was felling a beam, the axe head fell into the water."),
         tr("«¡Ah, señor mío, era prestada!». Y no tiene con qué pagarla.", "«Alas, master! for it was borrowed». And he has nothing to pay it with.")},
        {"2 Reyes 6:1-7", nullptr},
        {{tr("Echar un palo al agua", "Cast a stick into the water"), tr("-8 Fe", "-8 Faith"),
          tr("Cortas un palo y lo echas donde cayó. El hierro salió a flote. «Tómalo».", "You cut down a stick and cast it in. The iron did swim. «Take it up to thee».")},
         {tr("Pagar el hacha de tu bolsa", "Pay for the axe yourself"), tr("-15 HP máx", "-15 max HP"),
          tr("Pagas al dueño lo que vale el hierro. Nadie ve un milagro, y nadie pierde su casa.", "You pay the owner what the iron is worth. No one sees a wonder, and no one loses his house.")}},
        Portrait::HIJO_PROFETAS, Npc::HIJO_PROFETAS
    };
    case Ev::TIERRAS_SUNAMITA: return {
        tr("Siete años después", "Seven years after"),
        {tr("La mujer de Sunem se fue a tierra de filisteos por el hambre que anunciaste.", "The woman of Shunem went to the land of the Philistines for the famine you foretold."),
         tr("Ha vuelto, y su casa y sus tierras están en otras manos.", "She has come back, and her house and her land are in other hands."),
         tr("Está a la puerta del rey pidiendo lo suyo, y el rey no sabe quién es.", "She is at the king's gate crying for her own, and the king does not know who she is.")},
        {"2 Reyes 8:1-6", nullptr},
        {{tr("Hablar por ella ante el rey", "Speak for her to the king"), tr("-10 Fe", "-10 Faith"),
          tr("El rey oye de tu boca lo del niño resucitado, y le devuelve la casa, la tierra y los frutos de siete años.", "The king hears from your mouth of the child restored, and gives her back house, land and all the fruits of seven years.")},
         {tr("Dejar que hable Guiezi", "Let Gehazi tell it"), nullptr,
          tr("Guiezi cuenta la historia mejor que nadie, y la cuenta como suya.", "Gehazi tells the story better than anyone, and tells it as his own.")}},
        Portrait::SUNAMITA, Npc::SUNAMITA
    };

    // =========================================================================
    // Cap. 9 - Naaman el Sirio (2 Reyes 5)
    // =========================================================================
    // Hazael y la uncion de Jehu (2 R 8-9) vivian aqui y se mudaron al cap. 10:
    // en el libro van DESPUES del sitio de Samaria, y ver a Ben-adad en su lecho
    // de muerte un capitulo antes de pelear contra el era mentir sobre el orden.
    case Ev::NAAMAN_PUERTA: return {
        tr("Naamán, general de Siria", "Naaman, captain of Syria"),
        {tr("Está a tu puerta con sus caballos y sus carros, y no se ha bajado.", "He stands at your door with his horses and his chariots, and has not come down."),
         tr("«He aquí, yo decía para mí: Saldrá él luego, e invocando el nombre de Jehová", "«Behold, I thought, He will surely come out to me, and call on the name of the Lord"),
         tr("alzará su mano y tocará el lugar, y sanará la lepra».", "and strike his hand over the place, and recover the leper».")},
        {"2 Reyes 5:11", nullptr},
        {{tr("Mandarle al Jordán", "Send him to the Jordan"), tr("-25 Fe", "-25 Faith"),
          tr("Se zambulló siete veces, y su carne se volvió como la de un niño. Y tú con él.", "He dipped seven times, and his flesh came again like a child's. And yours with his.")},
         {tr("Sanarlo con tu mano", "Heal him with your hand"), nullptr,
          tr("La lepra cede a tu contacto. Naamán te alaba a ti, y no a Dios.", "The leprosy yields to your touch. Naaman praises you, and not God.")}},
        Portrait::NAAMAN, Npc::NAAMAN
    };
    case Ev::MUCHACHA_CAUTIVA: return {
        tr("La muchacha cautiva", "The captive maid"),
        // Licencia minima: en el libro se queda en Damasco (5:2-3); aqui viene en la
        // comitiva, que es la unica forma de ponerla en escena con el profeta delante.
        {tr("Viene en la comitiva del general, detrás de los carros: una muchacha de Israel que se llevaron en una correría.", "She comes in the captain's train, behind the chariots: a maid of Israel, carried away in a raid."),
         tr("«Si rogase mi señor al profeta que está en Samaria, él lo sanaría de su lepra».", "«Would God my lord were with the prophet that is in Samaria! he would recover him of his leprosy»."),
         tr("Fue ella quien lo dijo, en Damasco, a la mujer de su señor. Por eso está aquí el sirio.", "It was she that said it, in Damascus, to her master's wife. That is why the Syrian is here.")},
        {"2 Reyes 5:2-4", nullptr},
        {{tr("Pedir que la devuelvan a su tierra", "Ask that she be sent home"), tr("-15 HP máx", "-15 max HP"),
          tr("Negocias su vuelta antes que nada. Sin ella no habría nada que negociar.", "You bargain for her return before anything else. Without her there would be nothing to bargain for.")},
         {tr("Recibir al sirio que viene", "Receive the Syrian who comes"), tr("-15 Fe", "-15 Faith"),
          tr("Atiendes al general y su comitiva. La muchacha vuelve a la cocina y nadie pregunta su nombre.", "You attend the captain and his train. The maid goes back to the kitchen, and no one asks her name.")}},
        Portrait::MUCHACHA_CAUTIVA, Npc::MUCHACHA_CAUTIVA
    };
    // La despedida de Naaman (5:15-19): la tentacion que aqui se rechaza — o no — es
    // exactamente la que pierde a Guiezi, el jefe de este capitulo.
    case Ev::TIERRA_DE_ISRAEL: return {
        tr("Naamán, limpio", "Naaman, made clean"),
        {tr("«He aquí, ahora conozco que no hay Dios en toda la tierra, sino en Israel». Vuelve del Jordán con piel de niño.", "«Behold, now I know that there is no God in all the earth, but in Israel». He comes back from the Jordan with the skin of a child."),
         tr("Te ofrece plata, oro y vestidos; y pide una carga de tierra de Israel, la de dos mulas, para no orar ya sobre otro suelo.", "He offers silver, gold and raiment; and asks two mules' burden of the earth of Israel, to pray on no other ground."),
         tr("«En esto perdone Jehová a tu siervo: cuando mi señor entre en el templo de Rimón, apoyado en mi mano».", "«In this thing the Lord pardon thy servant: when my master goeth into the house of Rimmon, leaning on my hand».")},
        {"2 Reyes 5:15-19", nullptr},
        {{tr("«Ve en paz»", "«Go in peace»"), nullptr,
          tr("Se lo dices como a un hombre nuevo. Carga su tierra y vuelve a Damasco sin quemar incienso a otro dios.", "You say it as to a new man. He loads his earth and returns to Damascus to burn incense to no other god.")},
         {tr("Aceptar su presente", "Take his present"), tr("-20 Fe", "-20 Faith"),
          tr("«Vive Jehová, que no lo tomaré» — y lo tomas. La plata pesa en el zurrón, y algo se apaga camino de casa.", "«As the Lord liveth, I will receive none» — and you receive it. The silver weighs in the sack, and something goes out on the way home.")}},
        Portrait::NAAMAN, Npc::NAAMAN
    };

    // =========================================================================
    // Cap. 10 - El Ejercito Celestial (2 Reyes 6:8 - 9:13)
    // =========================================================================
    // Dotan y el sitio de Samaria, y al final lo que el libro cuenta justo despues
    // y desemboca en el epilogo: Hazael (2 R 8) y la uncion de Jehu (2 R 9).
    case Ev::CRIADO_ELISEO: return {
        tr("El criado de Eliseo", "Elisha's servant"),
        {tr("«¡Ah, señor mío! ¿Qué haremos?».", "«Alas, my master! How shall we do?»."),
         tr("La ciudad está cercada de gente de a caballo y de carros por todas partes.", "The city is compassed with horses and chariots on every side."),
         tr("Y él tiembla, porque solo ve lo que se ve.", "And he trembles, because he sees only what can be seen.")},
        {"2 Reyes 6:15-17", nullptr},
        {{tr("Orar por sus ojos", "Pray for his eyes"), tr("-20 Fe", "-20 Faith"),
          tr("«Más son los que están con nosotros». Y el monte se llenó de carros de fuego.", "«They that be with us are more». And the mountain filled with chariots of fire.")},
         {tr("Confiar en tu brazo", "Trust your own arm"), nullptr,
          tr("Afilas el ánimo y el arma. El criado sigue temblando.", "You sharpen your nerve and your weapon. The servant keeps trembling.")}},
        Portrait::CRIADO_ELISEO, Npc::CRIADO_ELISEO
    };
    case Ev::SIRIOS_DOTAN: return {
        tr("El ejército ciego", "The blinded host"),
        {tr("Pediste ceguera para ellos y la tuvieron. Los has traído de la mano hasta Samaria.", "You asked blindness for them and they had it. You led them by the hand into Samaria."),
         tr("Ahora ven, y están dentro de la ciudad enemiga, desarmados y contados.", "Now they see, and they are inside the enemy city, unarmed and counted."),
         tr("«Padre mío, ¿los mataré? ¿Los mataré?», pregunta el rey dos veces.", "«My father, shall I smite them? shall I smite them?», the king asks twice.")},
        {"2 Reyes 6:18-23", nullptr},
        {{tr("Ponerles pan y agua", "Set bread and water before them"), tr("-4 de daño", "-4 damage"),
          tr("«Ponles pan y agua, y que se vuelvan a su señor». Comieron, se fueron, y no volvieron más las bandas de Siria.", "«Set bread and water before them, and let them go». They ate, they went, and the bands of Syria came no more.")},
         {tr("Herirlos ahora que puedes", "Smite them while you can"), tr("-25 Fe", "-25 Faith"),
          tr("Ni a los que cautivas con tu espada matas indefensos, y estos ni siquiera son tuyos. Las bandas de Siria volverán.", "You do not slay even your own captives defenceless, and these are not even yours. The bands of Syria will come again.")}},
        Portrait::GENERAL_SIRIO, Npc::GENERAL_SIRIO
    };
    case Ev::CUATRO_LEPROSOS: return {
        tr("Los cuatro a la puerta", "The four at the gate"),
        // Es el FIN del cerco (7:3-10), despues del jefe: la primera frase lleva la marca.
        {tr("El último día del cerco. Samaria lleva tanto tiempo sitiada que una cabeza de asno se paga a ochenta de plata.", "The last day of the siege. Samaria has been so long besieged that an ass's head sells for fourscore pieces of silver."),
         tr("«¿Para qué nos estamos aquí hasta que muramos?». Cuatro leprosos, a la caída de la tarde.", "«Why sit we here until we die?». Four lepers, at the twilight."),
         tr("Entraron al campamento sirio y no había nadie: plata, oro, vestidos y toda la comida del mundo.", "They came to the Syrian camp and there was no man: silver, gold, raiment, and all the food in the world.")},
        {"2 Reyes 7:3-10", nullptr},
        {{tr("Volver y contarlo a la ciudad", "Go back and tell the city"), tr("-15 HP máx", "-15 max HP"),
          tr("«No estamos haciendo bien. Hoy es día de buena nueva». Volvieron con hambre y con la noticia.", "«We do not well. This day is a day of good tidings». They went back hungry, and with the news.")},
         {tr("Esconder plata y vestidos", "Hide the silver and the raiment"), tr("-20 Fe", "-20 Faith"),
          tr("Escondéis lo de dos tiendas antes de decir nada. La ciudad espera un día más su pan.", "You hide the spoil of two tents before saying a word. The city waits one more day for its bread.")}},
        Portrait::LEPROSO_SAMARIA, Npc::LEPROSO_SAMARIA
    };
    case Ev::PRINCIPE_SAMARIA: return {
        tr("El príncipe de la puerta", "The lord of the gate"),
        {tr("Has dicho que mañana a estas horas la harina se venderá a peso de nada en la puerta de Samaria.", "You have said that tomorrow about this time meal shall sell for nothing in the gate of Samaria."),
         tr("El príncipe sobre cuyo brazo se apoya el rey se ríe delante de todos.", "The lord on whose hand the king leans laughs before them all."),
         tr("«Si Jehová hiciese ventanas en el cielo, ¿sería esto así?».", "«If the Lord would make windows in heaven, might this thing be?».")},
        {"2 Reyes 7:2", nullptr},
        {{tr("Repetírselo delante de todos", "Say it again before them all"), "-10 HP",
          tr("«Lo verás con tus ojos, mas no comerás de ello». Y el pueblo lo atropelló a la puerta.", "«Thou shalt see it with thine eyes, but shalt not eat thereof». And the people trode upon him in the gate.")},
         {tr("Dejar que dude", "Let him doubt"), tr("-8 Fe", "-8 Faith"),
          tr("No discutes con él. La harina bajará de precio, se ría quien se ría.", "You do not argue. The price of meal will fall, laugh who may.")}},
        Portrait::PRINCIPE_SAMARIA, Npc::PRINCIPE_SAMARIA
    };
    // Como "Siete años despues" en el cap. 8, este relato mira mas alla del jefe: roto el
    // cerco, el libro sigue en Damasco (2 R 8:7-15).
    case Ev::HAZAEL_BENADAD: return {
        tr("Hazael de Damasco", "Hazael of Damascus"),
        {tr("Roto el cerco, Ben-adad enfermó en Damasco, y manda a preguntarte si sanará. Cuarenta camellos de presente.", "The siege broken, Ben-hadad fell sick in Damascus, and sends to ask if he shall recover. Forty camels' burden of presents."),
         tr("Le sostienes la mirada al mensajero hasta que se avergüenza, y entonces lloras.", "You fix your eyes on the messenger until he is ashamed, and then you weep."),
         tr("«¿Por qué llora mi señor?». Lloras por lo que ese hombre va a hacerle a Israel.", "«Why weepeth my lord?». You weep for what that man will do to Israel.")},
        {"2 Reyes 8:7-15", nullptr},
        {{tr("Decirle la verdad entera", "Tell him the whole truth"), "-18 HP",
          tr("Le dices que será rey, y le dices lo que hará siendo rey. Se marcha sabiéndolo todo.", "You tell him he shall be king, and what he will do as king. He leaves knowing all of it.")},
         {tr("Callar lo que has visto", "Keep what you saw to yourself"), tr("-10 Fe", "-10 Faith"),
          tr("Le das la respuesta corta y le dejas ir. Lo que viene, vendrá igual.", "You give him the short answer and let him go. What is coming will come regardless.")}},
        Portrait::GENERAL_SIRIO, Npc::GENERAL_SIRIO
    };
    // El ultimo relato del juego: la redoma que enciende el epilogo (Jehu sale hacia
    // Jezreel en su primera escena).
    case Ev::UNCION_JEHU: return {
        tr("La redoma de aceite", "The box of oil"),
        {tr("Queda una unción de las tres que se mandaron en Horeb: Jehú, hijo de Nimsi, acampa en Ramot de Galaad.", "One anointing remains of the three commanded at Horeb: Jehu, the son of Nimshi, is encamped at Ramoth-gilead."),
         tr("«Ciñe tus lomos, toma esta redoma. Hazle levantar de entre sus hermanos, unge, abre la puerta y huye: no esperes».", "«Gird up thy loins, take this box of oil. Make him arise from among his brethren, anoint him, open the door and flee: tarry not»."),
         tr("El muchacho te mira. Lo que se derrame en ese cuarto no se podrá recoger.", "The young man looks at you. What is poured out in that chamber cannot be gathered up again.")},
        {"2 Reyes 9:1-13", nullptr},
        {{tr("Enviarle con la redoma", "Send him with the box"), tr("-15 Fe", "-15 Faith"),
          tr("Corrió, ungió y huyó, como se le dijo. «¿Por qué vino a ti ese loco?» — y luego las trompetas: «Jehú es rey».", "He ran, anointed, and fled, as he was told. «Wherefore came this mad fellow to thee?» — and then the trumpets: «Jehu is king».")},
         {tr("Guardar el aceite un día más", "Keep the oil one more day"), "-15 HP",
          tr("Cargas con lo que sabes un día entero, y pesa como pocas cargas. Lo dicho en Horeb no caduca: mañana correrá igual.", "You carry what you know a whole day, and few burdens weigh like it. What was said at Horeb does not expire: tomorrow it runs all the same.")}},
        Portrait::HIJO_PROFETAS, Npc::HIJO_PROFETAS
    };

    // =========================================================================
    // La Ascension (2 Reyes 2:1-14). Cuatro paradas SEGUIDAS, no una sala.
    // =========================================================================
    // Tres veces le dice que se quede y tres veces se niega; eso es lo que el pasaje repite,
    // y por eso aqui es una decision que se toma TRES VECES. Quedarse no rompe nada: acorta
    // la herencia, que es exactamente lo que significa no llegar hasta el final del camino.
    case Ev::ASC_GILGAL: return {
        tr("Camino de Betel", "On the way to Bethel"),
        {tr("«Quédate ahora aquí, porque Jehová me ha enviado a Betel».", "«Tarry here, I pray thee; for the Lord hath sent me to Bethel»."),
         tr("Lo dice sin mirarte, y sigue andando.", "He says it without looking at you, and walks on."),
         tr("Por el camino, los hijos de los profetas os miran pasar y no dicen nada.", "Along the way the sons of the prophets watch you pass, and say nothing.")},
        {"2 Reyes 2:2", nullptr},
        {{tr("«Vive Jehová, no te dejaré»", "«As the Lord liveth, I will not leave thee»"), nullptr,
          tr("Y descendieron ambos a Betel.", "So they went down to Bethel.")},
         {tr("Quedarte en Gilgal", "Stay in Gilgal"), tr("-10% de herencia", "-10% inheritance"),
          tr("Te quedas donde te dijo. Él sigue solo, y el camino se acorta para los dos.", "You stay where he told you. He goes on alone, and the road shortens for both.")}},
        Portrait::ELIAS
    };
    case Ev::ASC_BETEL: return {
        tr("Los hijos de los profetas", "The sons of the prophets"),
        {tr("«¿Sabes que Jehová te quitará hoy a tu señor de sobre tu cabeza?».", "«Knowest thou that the Lord will take away thy master from thy head today?»."),
         tr("«Sí, yo lo sé; callad».", "«Yea, I know it; hold ye your peace»."),
         tr("Y otra vez: «Quédate ahora aquí, que Jehová me ha enviado a Jericó».", "And again: «Tarry here, I pray thee; for the Lord hath sent me to Jericho».")},
        {"2 Reyes 2:3-4", nullptr},
        {{tr("«Vive Jehová, no te dejaré»", "«As the Lord liveth, I will not leave thee»"), nullptr,
          tr("Y vinieron a Jericó los dos, sin decirse nada en todo el camino.", "So they came to Jericho, and said nothing to each other the whole way.")},
         {tr("Quedarte en Betel", "Stay in Bethel"), tr("-10% de herencia", "-10% inheritance"),
          tr("Los cincuenta te hacen sitio. Desde aquí el camino a Jericó se ve entero.", "The fifty make room for you. From here the road to Jericho is in plain sight.")}},
        Portrait::NONE
    };
    case Ev::ASC_JERICO: return {
        tr("A la orilla del Jordán", "At the bank of the Jordan"),
        {tr("Cincuenta hijos de los profetas se pararon enfrente, a lo lejos.", "Fifty sons of the prophets stood to view afar off."),
         tr("Elías tomó su manto, lo dobló y golpeó las aguas.", "Elijah took his mantle, wrapped it together, and smote the waters."),
         tr("Se apartaron a un lado y a otro, y quedó el cauce en seco.", "They parted hither and thither, and the channel was dry.")},
        {"2 Reyes 2:6-8", nullptr},
        {{tr("Cruzar con él", "Cross over with him"), nullptr,
          tr("Pasáis ambos por lo seco. Al otro lado ya no hay nadie mirando.", "You both go over on dry ground. On the far side there is no one watching.")},
         {tr("Quedarte con los cincuenta", "Stay with the fifty"), tr("-10% de herencia", "-10% inheritance"),
          tr("Te quedas en la orilla. Le ves cruzar solo, y el manto con él.", "You stay on the bank. You watch him cross alone, and the mantle with him.")}},
        Portrait::NONE
    };
    case Ev::ASC_JORDAN: return {
        tr("«Pide lo que quieras»", "«Ask what I shall do for thee»"),
        {tr("«Pide lo que quieras que haga por ti, antes que yo sea quitado de ti».", "«Ask what I shall do for thee, before I be taken away from thee»."),
         tr("Se ha parado en medio del cauce seco, y espera tu respuesta.", "He has stopped in the midst of the dry channel, and waits for your answer."),
         nullptr},
        {"2 Reyes 2:9-10", nullptr},
        {{tr("«Una doble porción de tu espíritu»", "«A double portion of thy spirit»"), nullptr,
          tr("«Cosa difícil has pedido. Si me vieres cuando fuere quitado de ti, te será hecho».", "«Thou hast asked a hard thing. If thou see me when I am taken from thee, it shall be so».")},
         {tr("«Solo verte partir»", "«Only to see thee go»"), tr("sin milagro heredado", "no inherited miracle"),
          tr("Pides menos de lo que te ofrecían. Te será dado lo que pediste, y nada más.", "You ask less than was offered. You shall have what you asked, and nothing more.")}},
        Portrait::ELIAS
    };

    // =========================================================================
    // El epilogo: "El Legado" (2 Reyes 9; 13:14-21)
    // =========================================================================
    // Aqui la partida YA se ha acabado, asi que estas decisiones no mueven vida ni fe: no
    // queda partida que mover, e inventarle un premio mecanico a una escena de cierre seria
    // relleno. Lo que cambian es lo que se te cuenta — y la del arco cambia las ultimas
    // palabras del juego, que en este pasaje es exactamente lo que estaba en juego.
    case Ev::EPI_JEHU: return {
        tr("Jehú, hijo de Nimsi", "Jehu, son of Nimshi"),
        {tr("Le ungieron rey en Ramot de Galaad y salió hacia Jezreel sin decir a dónde iba.", "They anointed him king in Ramoth-gilead, and he drove to Jezreel telling no one whither."),
         tr("Jezabel se pintó los ojos, se atavió la cabeza y se asomó por la ventana.", "Jezebel painted her face, tired her head, and looked out at a window."),
         tr("«¿Quién está conmigo? ¿Quién?». Y dos o tres eunucos le miraron desde arriba.", "«Who is on my side? who?». And two or three eunuchs looked out to him.")},
        {"2 Reyes 9:30-37", nullptr},
        {{tr("Recordar la palabra sobre la viña", "Recall the word about the vineyard"), nullptr,
          tr("«En la heredad de Nabot comerán los perros la carne de Jezabel». Se cumplió hasta la última letra, veinte años después.", "«In the portion of Naboth shall dogs eat the flesh of Jezebel». It came to pass to the last letter, twenty years after.")},
         {tr("Callar y mirar", "Say nothing, and watch"), nullptr,
          tr("Miras sin decir nada. Se cumple igual, delante de tus ojos.", "You watch and say nothing. It comes to pass all the same, before your eyes.")}},
        Portrait::JEZABEL
    };
    case Ev::EPI_ARCO_JOAS: return {
        tr("Joás, rey de Israel", "Joash, king of Israel"),
        {tr("Eliseo está enfermo de la enfermedad de la que va a morir, y el rey llora sobre su rostro.", "Elisha is fallen sick of the sickness whereof he shall die, and the king weeps over his face."),
         tr("«Toma el arco». Y puso Eliseo sus manos sobre las manos del rey.", "«Take bow and arrows». And Elisha put his hands upon the king's hands."),
         tr("«Toma las saetas. Golpea la tierra». ¿Cuántas veces?", "«Take the arrows. Smite upon the ground». How many times?")},
        {"2 Reyes 13:14-19", nullptr},
        {{tr("Golpear hasta que te manden parar", "Smite until you are told to stop"), nullptr,
          tr("Golpeas cinco, seis veces, y no paras hasta que él te lo dice. Habrías consumido a Siria.", "You smite five, six times, and stop only when he tells you. You would have consumed Syria.")},
         {tr("Golpear tres veces", "Smite three times"), nullptr,
          tr("Golpeaste tres veces, y te detuviste. Y el varón de Dios se enojó: «ahora herirás a Siria solo tres veces».", "You smote thrice, and stayed. And the man of God was wroth: «now thou shalt smite Syria but thrice».")}},
        Portrait::NONE
    };
    case Ev::EPI_HUESOS: return {
        tr("El sepulcro de Eliseo", "The sepulchre of Elisha"),
        {tr("Murió Eliseo, y lo sepultaron. Entrado el año siguiente vinieron las bandas de Moab.", "And Elisha died, and they buried him. And the bands of the Moabites invaded the land at the coming in of the year."),
         tr("Sepultando unos hombres a otro, vieron la banda, y echaron el cadáver en el sepulcro de Eliseo.", "And as they were burying a man, they spied a band; and they cast the man into the sepulchre of Elisha."),
         tr("Y cuando tocó los huesos del profeta, revivió, y se levantó sobre sus pies.", "And when the man touched the bones of Elisha, he revived, and stood up on his feet.")},
        {"2 Reyes 13:20-21", nullptr},
        {{tr("Bajar la mirada", "Lower your eyes"), nullptr,
          tr("Revivió, y se levantó sobre sus pies. Y el profeta llevaba un año en el sepulcro.", "He revived, and stood up on his feet. And the prophet had been a year in the sepulchre.")},
         {nullptr, nullptr, nullptr}},
        Portrait::ELISEO
    };
    default: return {
        "", {nullptr, nullptr, nullptr}, {"", nullptr},
        {{nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr}}
    };
    }
}
