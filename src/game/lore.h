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
// Registro (decision del usuario, 2026-09-10): arcaizante, el de la Reina-Valera 1960
// tambien en lo inventado; las citas, tal cual. Cada intro arranca DONDE TERMINA la
// apertura guionizada de su capitulo (game/scenes.h) y no repite ni un versiculo ni un
// hecho que la apertura acabe de jugar ni que el pozo (game/events.h) juegue despues.
// La guia de estilo completa: docs/encuentros-y-lore.md.
//
// =============================================================================

#include "core/i18n.h"
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
// Lo que dice el jefe al aparecer ya no vive aqui: son las cadenas de dialogo de
// game/scenes.h (`Scene::boss_of`).
struct ChapterLore {
    Scripture   verse;
    const char* lines[4];
    const char* objective;   // Lo que el profeta debe hacer aqui (el mapa lo pinta en UNA linea)
    const char* outro;       // Se muestra al cruzar el arco, tras el jefe
};

// =============================================================================
// Los diez capitulos
// =============================================================================
inline ChapterLore get_chapter_lore(int chapter) {
    switch (chapter) {
        case 1: return {
            {"1 Reyes 17:4", tr("Beberás del arroyo; y yo he mandado a los cuervos que te den allí de comer.", "Thou shalt drink of the brook; and I have commanded the ravens to feed thee there.")},
            // 17:1-3 ya no se repiten aqui: los acaba de jugar la audiencia con Acab
            // (scenes.h, Scene::AUDIENCIA) justo antes de esta intro.
            {tr("Los hombres del rey ya buscan tu rastro por todo Israel.", "The king's men already seek thy trail through all Israel."),
             nullptr, nullptr, nullptr},
            tr("Sobrevive junto al arroyo. Los cuervos te sustentarán.", "Survive by the brook. The ravens shall feed thee."),
            tr("Cayó el capitán. Y pasados algunos días se secó el arroyo, porque no había llovido sobre la tierra.", "The captain fell. And after a while the brook dried up, because there had been no rain in the land.")
        };
        case 2: return {
            {"1 Reyes 17:14", tr("La harina de la tinaja no escaseará, ni el aceite de la vasija disminuirá.", "The barrel of meal shall not waste, neither shall the cruse of oil fail.")},
            // 17:8-11 los acaba de jugar la apertura (la palabra, el agua, el bocado de pan):
            // aqui empieza lo que hay en la casa. El punado de harina (17:12) es el pozo.
            {tr("En casa de la viuda hay un puñado de harina en la tinaja, un poco de aceite en una vasija, y un hijo.", "In the widow's house there is a handful of meal in a barrel, a little oil in a cruse, and a son."),
             tr("Sarepta es tierra de Jezabel, y en Sarepta no se invoca otro nombre que el de Baal.", "Zarephath is Jezebel's country, and in Zarephath no name is called upon but Baal's."),
             nullptr, nullptr},
            tr("Vuelve a la casa de la viuda. Que la tinaja no se acabe.", "Return to the widow's house. Let the barrel not fail."),
            tr("El niño ha vuelto a la vida. «Ahora conozco que tú eres varón de Dios».", "The child liveth again. «Now I know that thou art a man of God».")
        };
        case 3: return {
            {"1 Reyes 18:21", tr("¿Hasta cuándo claudicaréis vosotros entre dos pensamientos?", "How long halt ye between two opinions?")},
            // «Ve, muestrate a Acab» (18:1) y la audiencia con Acab (18:17-19, 24) son la
            // apertura: la intro arranca en 18:20, con Israel subiendo al monte.
            {tr("Entonces Acab convocó a todos los hijos de Israel, y reunió a los profetas en el monte Carmelo.", "So Ahab sent unto all the children of Israel, and gathered the prophets together unto mount Carmel."),
             tr("Cuatrocientos cincuenta de Baal a un lado; al otro, un hombre solo.", "Four hundred and fifty of Baal on the one side; on the other, one man alone."),
             tr("Dos altares, dos bueyes, y ningún fuego encendido por mano de hombre.", "Two altars, two bullocks, and no fire kindled by the hand of man."),
             nullptr},
            tr("Que el pueblo vea de quién es el fuego.", "Let the people see whose the fire is."),
            tr("«¡Jehová es el Dios!». Y el cielo se oscureció, y hubo una gran lluvia.", "«The Lord, he is the God!». And the heaven was black, and there was a great rain.")
        };
        case 4: return {
            {"1 Reyes 19:12", tr("Y tras el fuego un silbo apacible y delicado.", "And after the fire a still small voice.")},
            // El juramento de Jezabel (19:2) y la huida a Beerseba (19:3) son la apertura; el
            // enebro (19:4-8) es el pozo. Aqui: el desierto y la cueva (19:8-9).
            {tr("Un día de camino por el desierto, y luego cuarenta días y cuarenta noches hasta Horeb, el monte de Dios.", "A day's journey into the wilderness, and then forty days and forty nights unto Horeb the mount of God."),
             tr("Y allí se metió en una cueva, donde pasó la noche.", "And he came thither unto a cave, and lodged there."),
             nullptr, nullptr},
            tr("«¿Qué haces aquí, Elías?»", "«What doest thou here, Elijah?»"),
            tr("«Vuelve por tu camino». Y bajó de Horeb: quedaban siete mil rodillas, y un labrador en Abel-mehola.", "«Go, return on thy way». And he came down from Horeb: seven thousand knees remained, and a ploughman in Abel-meholah.")
        };
        case 5: return {
            {"1 Reyes 19:19", tr("Y pasando Elías por delante de él, echó sobre él su manto.", "And Elijah passed by him, and cast his mantle upon him.")},
            // El llamamiento (19:19-20) es la apertura; la despedida (19:21) es el pozo.
            // Aqui: Eliseo que le sirve, y la vina de Nabot (21:1-16) contada en tres frases.
            {tr("Y Eliseo se levantó y fue tras Elías, y le servía.", "Then Elisha arose, and went after Elijah, and ministered unto him."),
             tr("Mientras, Acab codició la viña de Nabot, y Jezabel se la compró con dos testigos falsos y un montón de piedras.", "Meanwhile Ahab coveted Naboth's vineyard, and Jezebel bought it for him with two false witnesses and a heap of stones."),
             tr("Hoy baja el rey a tomar posesión de ella.", "To day the king goeth down to take possession of it."),
             nullptr},
            tr("Enfrenta al rey y a la reina en el campo que robaron.", "Face the king and the queen in the field they stole."),
            tr("«Donde lamieron los perros la sangre de Nabot, lamerán también tu sangre».", "«Where the dogs licked the blood of Naboth shall dogs lick thy blood».")
        };
        case 6: return {
            {"2 Reyes 2:21", tr("Yo sané estas aguas, y no habrá más en ellas muerte ni enfermedad.", "I have healed these waters; there shall not be from thence any more death or barrenness.")},
            // La orilla del Jordan (2:15) es la apertura; «las aguas son malas» (2:19) y la
            // vasija con sal son el pozo. Aqui: la subida a Jerico y lo que se dice de ella.
            {tr("Con el manto de Elías al hombro, Eliseo subió a Jericó, y los cincuenta detrás de él.", "With Elijah's mantle on his shoulder Elisha went up to Jericho, and the fifty behind him."),
             tr("La ciudad es hermosa de asiento; pero su agua mata lo que riega, y la tierra no da fruto.", "The city is pleasant to look upon; but its water killeth what it watereth, and the ground beareth no fruit."),
             tr("Dicen en Jericó que la ciudad se levantó sobre un juramento, y que algo quedó en el agua.", "They say in Jericho that the city was raised upon an oath, and that something was left in the water."),
             nullptr},
            tr("Sana el manantial de Jericó.", "Heal the spring of Jericho."),
            tr("Y fueron sanas las aguas hasta el día de hoy.", "So the waters were healed unto this day.")
        };
        case 7: return {
            {"2 Reyes 4:3", tr("Pide para ti vasijas prestadas de todos tus vecinos; y no pidas pocas.", "Borrow thee vessels abroad of all thy neighbours; borrow not a few.")},
            // La suplica de la viuda (4:1-4) es la apertura; el derramamiento (4:5-6), el
            // pozo. Aqui: la viuda pidiendo vasijas, y lo que pasa lejos de esta aldea.
            {tr("La viuda va de puerta en puerta pidiendo vasijas prestadas, y no pide pocas.", "The widow goeth from door to door borrowing vessels, and she borroweth not a few."),
             tr("El acreedor aguarda a la puerta con sus dos hombres, y cobra en carne.", "The creditor waiteth at the door with his two men, and he taketh his payment in flesh."),
             tr("Mientras, la guerra de Moab sigue en el sur, y hay hambre en Gilgal.", "Meanwhile the war with Moab goeth on in the south, and there is a dearth in Gilgal."),
             nullptr},
            tr("Reúne vasijas. Que el aceite no se detenga.", "Gather vessels. Let the oil not stay."),
            tr("«Vende el aceite y paga tus deudas; y tú y tus hijos vivid de lo que quede».", "«Sell the oil, and pay thy debt; and live thou and thy children of the rest».")
        };
        case 8: return {
            {"2 Reyes 4:34", tr("Y se tendió sobre el niño, y el cuerpo del niño entró en calor.", "And he stretched himself upon the child; and the flesh of the child waxed warm.")},
            // El aposento y la promesa (4:9-16) son la apertura. Aqui: el nino nace, crece y
            // muere (4:17-21). La madre en el Carmelo (4:25-30) es el pozo; el aposento
            // cerrado, el jefe; los siete estornudos, el outro.
            {tr("Y la mujer concibió, y dio a luz un hijo el año siguiente, en el tiempo que Eliseo le había dicho.", "And the woman conceived, and bare a son at that season that Elisha had said unto her."),
             tr("El niño creció. Y un día salió a su padre con los segadores, y dijo: «¡Ay, mi cabeza, mi cabeza!».", "The child grew. And one day he went out to his father to the reapers, and said: «My head, my head!»."),
             tr("Murió al mediodía sobre las rodillas de su madre. Ella lo acostó en la cama del varón de Dios, y cerró la puerta.", "He died at noon upon his mother's knees. She laid him on the bed of the man of God, and shut the door."),
             nullptr},
            tr("«Déjala, porque su alma está en amargura».", "«Let her alone; for her soul is vexed within her»."),
            tr("El niño estornudó siete veces, y abrió sus ojos.", "The child sneezed seven times, and opened his eyes.")
        };
        case 9: return {
            {"2 Reyes 5:10", tr("Ve y lávate siete veces en el Jordán, y serás limpio.", "Go and wash in Jordan seven times, and thou shalt be clean.")},
            // La carta del rey de Siria (5:5-8) es la apertura. La muchacha cautiva (5:2-3)
            // es el pozo: aqui solo se nombra, como el porque de la visita.
            {tr("Naamán, general del rey de Siria, era varón valeroso; pero leproso.", "Naaman, captain of the host of Syria, was a mighty man in valour; but he was a leper."),
             tr("Una muchacha de Israel, cautiva en su casa, habló del profeta de Samaria. Por eso vino con caballos, carros y una carta.", "A little maid of Israel, captive in his house, spake of the prophet in Samaria. Therefore he came with horses, chariots and a letter."),
             tr("Hoy sus carros se han parado a tu puerta, y él espera que salgas.", "To day his chariots have stood at thy door, and he waiteth for thee to come out."),
             nullptr},
            tr("Que el sirio se lave siete veces en el Jordán.", "Let the Syrian wash seven times in the Jordan."),
            tr("Y salió de delante de él leproso, blanco como la nieve.", "And he went out from his presence a leper as white as snow.")
        };
        case 10: return {
            {"2 Reyes 6:16", tr("No tengas miedo, porque más son los que están con nosotros que los que están con ellos.", "Fear not: for they that be with us are more than they that be with them.")},
            // Los avisos al rey (6:9-10) y «a Dotan» son la apertura. Aqui: Ben-adad sabe al
            // fin quien avisa (6:12), manda mirar (6:13) y cerca la ciudad de noche (6:14).
            {tr("«El profeta Eliseo está en Israel, y declara al rey las palabras que tú hablas en tu cámara más secreta».", "«Elisha, the prophet that is in Israel, telleth the king of Israel the words that thou speakest in thy bedchamber»."),
             tr("«Id, y mirad dónde está, para que yo envíe a prenderlo». Y le fue dicho: «He aquí que él está en Dotán».", "«Go and spy where he is, that I may send and fetch him». And it was told him: «Behold, he is in Dothan»."),
             tr("Entonces envió gente de a caballo, y carros, y un gran ejército; y vinieron de noche, y sitiaron la ciudad.", "Therefore sent he thither horses, and chariots, and a great host: and they came by night, and compassed the city about."),
             nullptr},
            tr("«Te ruego, Jehová, que abras sus ojos para que vea».", "«Lord, I pray thee, open his eyes that he may see»."),
            tr("Un estruendo de carros, y el campamento de Siria huyó al anochecer. Y un seah de flor de harina se vendió por un siclo a la puerta de Samaria.", "A noise of chariots, and the host of Syria fled in the twilight. And a measure of fine flour was sold for a shekel in the gate of Samaria.")
        };
        default: return {
            {"", nullptr}, {nullptr, nullptr, nullptr, nullptr}, "", ""
        };
    }
}

// =============================================================================
// Prologo / Ascension / Epilogo (paginas de la pantalla de historia)
// =============================================================================
inline ChapterLore get_prologue_lore() {
    return {
        {"1 Reyes 16:33", tr("Acab hizo más para provocar la ira de Jehová que todos los reyes de Israel que antes de él habían sido.", "Ahab did more to provoke the Lord to anger than all the kings of Israel before him.")},
        {tr("Israel ha olvidado a su Dios.", "Israel hath forgotten its God."),
         tr("Acab reina en Samaria, y Jezabel su reina ha llenado la tierra de altares a Baal.", "Ahab reigneth in Samaria, and Jezebel his queen hath filled the land with altars to Baal."),
         tr("Los profetas de Jehová son perseguidos, y los que quedan se esconden en cuevas.", "The prophets of the Lord are hunted, and those that remain hide in caves."),
         tr("Solo un hombre de Tisbe de Galaad se atreve a estar delante del rey.", "Only one man of Tishbe of Gilead dareth to stand before the king.")},
        "", ""
    };
}

// =============================================================================
// Prologo cinematico "El Cielo Cerrado" (ui/cinematic.h): 8 estampas, ~80 s.
//
// Cuenta lo mismo que get_prologue_lore() pero en imagenes: el reino partido, Acab,
// Jezabel y la entrada de Baal, la persecucion, el llamamiento de Elias en el monte,
// el juramento ante el rey, los cuervos de Querit y el titulo.
//
// `ref` sigue siendo solo atribucion documental: NO se pinta (ver Scripture arriba).
// =============================================================================
struct PrologueScene {
    const char* ref;         // Documental, no se muestra
    const char* lines[3];    // Hasta 3 frases; nullptr = fin
};

inline PrologueScene get_prologue_scene(int i) {
    switch (i) {
        case 0: return {"1 Reyes 12", {
            tr("Después de Salomón, el reino se partió en dos.", "After Solomon, the kingdom was torn in two."),
            tr("Israel al norte. Judá al sur.", "Israel to the north. Judah to the south."),
            tr("Y rey tras rey, el norte fue olvidando al Dios que lo sacó de Egipto.", "And king after king, the north forgot the God who brought it out of Egypt.")}};
        case 1: return {"1 Reyes 16:30", {
            tr("Entonces reinó Acab.", "Then Ahab reigned."),
            tr("Hizo lo malo ante los ojos del Señor, más que todos los reyes antes de él.", "He did evil in the sight of the Lord, more than all the kings before him."),
            nullptr}};
        case 2: return {"1 Reyes 16:31-32", {
            tr("Tomó por mujer a Jezabel, hija del rey de los sidonios.", "He took to wife Jezebel, daughter of the king of the Sidonians."),
            tr("Con ella entró Baal en Israel: templo, altar, y una imagen en la casa del rey.", "With her Baal entered Israel: temple, altar, and an image in the king's house."),
            nullptr}};
        case 3: return {"1 Reyes 18:4", {
            tr("Jezabel mandó matar a los profetas del Señor.", "Jezebel ordered the prophets of the Lord to be killed."),
            tr("Los que quedaron se escondieron en cuevas, de cien en cien, con pan y agua.", "Those who remained hid in caves, a hundred at a time, with bread and water."),
            nullptr}};
        case 4: return {"1 Reyes 17:1", {
            tr("Nadie lo esperaba. No venía de palacio, ni del templo.", "No one expected him. He came not from the palace, nor from the temple."),
            tr("Elías, de Tisbé de Galaad, subió al monte a solas.", "Elijah, of Tishbe of Gilead, went up the mountain alone."),
            tr("Y el Señor le habló.", "And the Lord spoke to him.")}};
        case 5: return {"1 Reyes 17:1", {
            tr("«Vive el Señor, Dios de Israel, delante de quien estoy:", "«As the Lord God of Israel liveth, before whom I stand:"),
            tr("no habrá rocío ni lluvia en estos años, sino por mi palabra.»", "there shall not be dew nor rain these years, but according to my word.»"),
            nullptr}};
        case 6: return {"1 Reyes 17:3-6", {
            tr("«Escóndete junto al arroyo de Querit.", "«Hide thyself by the brook Cherith."),
            tr("Beberás del arroyo, y he mandado a los cuervos que allí te sustenten.»", "Thou shalt drink of the brook, and I have commanded the ravens to feed thee there.»"),
            tr("Obedeció. Y comió de la mano de los cuervos.", "He obeyed. And he ate from the hand of the ravens.")}};
        default: return {"", {nullptr, nullptr, nullptr}};
    }
}

// Tres paginas: la peticion, el torbellino, el manto.
// `asked_double` = en la cuarta parada del Ascenso (events.h, Ev::ASC_JORDAN) se pidio la
// doble porcion (Engine::ascent_double). Si no, la pagina 0 no puede decir que se pidio:
// Eliseo pidio solo verle partir, y eso es lo que se le dio (2 Reyes 2:10).
inline ChapterLore get_ascension_lore(int page, bool asked_double = true) {
    switch (page) {
        case 0: return {
            {"2 Reyes 2:9", tr("Pide lo que quieras que haga por ti, antes que yo sea quitado de ti.", "Ask what I shall do for thee, before I be taken away from thee.")},
            {tr("Elías y Eliseo cruzaron el Jordán en seco, golpeando las aguas con el manto.", "Elijah and Elisha crossed the Jordan dry-shod, smiting the waters with the mantle."),
             asked_double
                 ? tr("«Te ruego que una doble porción de tu espíritu sea sobre mí».", "«I pray thee, let a double portion of thy spirit be upon me».")
                 : tr("«Déjame verte partir, señor mío. No pido más».", "«Let me see thee go, my lord. I ask no more»."),
             asked_double
                 ? tr("«Cosa difícil has pedido. Si me vieres cuando fuere quitado de ti, te será hecho».", "«Thou hast asked a hard thing. If thou see me when I am taken from thee, it shall be so».")
                 : tr("«Lo que pediste te será dado, y nada más».", "«What thou hast asked shall be given thee, and nothing more»."),
             nullptr},
            "", ""
        };
        case 1: return {
            {"2 Reyes 2:11", tr("Un carro de fuego con caballos de fuego apartó a los dos; y Elías subió al cielo en un torbellino.", "A chariot of fire and horses of fire parted them both; and Elijah went up by a whirlwind into heaven.")},
            {tr("Y aconteció que yendo ellos y hablando, he aquí un carro de fuego.", "And it came to pass, as they still went on and talked, that behold, a chariot of fire."),
             tr("Y Eliseo lo veía, y clamaba: «¡Padre mío, padre mío, carro de Israel y su gente de a caballo!».", "And Elisha saw it, and cried: «My father, my father, the chariot of Israel, and the horsemen thereof!»."),
             tr("Y nunca más le vio.", "And he saw him no more."),
             nullptr},
            "", ""
        };
        default: return {
            {"2 Reyes 2:14", tr("¿Dónde está Jehová, el Dios de Elías? Y golpeó las aguas, y se apartaron.", "Where is the Lord God of Elijah? And he smote the waters, and they parted.")},
            {tr("Eliseo alzó el manto que se le había caído a Elías, y volvió al Jordán.", "Elisha took up the mantle that fell from Elijah, and went back to the Jordan."),
             tr("Golpeó las aguas con él, y se apartaron a un lado y a otro, y pasó.", "He smote the waters with it, and they parted hither and thither, and he went over."),
             tr("El manto no era suyo. Le fue prestado.", "The mantle was not his own. It was lent to him."),
             nullptr},
            "", ""
        };
    }
}

// La linea de la herencia que cierra el Ascenso (pie de story_ascension y ultima frase de
// cinematic_ascension). Sin doble porcion no hay milagro heredado (Engine::finish_ascension).
inline const char* get_ascension_inheritance_line(bool asked_double) {
    return asked_double
        ? tr("Heredas la mitad de su fe, su manto y dos milagros.", "Thou inheritest half his faith, his mantle and two miracles.")
        : tr("Heredas la mitad de su fe y su manto.", "Thou inheritest half his faith and his mantle.");
}

inline ChapterLore get_epilogue_lore() {
    return {
        {"2 Reyes 13:21", tr("Cuando llegó a tocar el muerto los huesos de Eliseo, revivió, y se levantó sobre sus pies.", "When the dead man touched the bones of Elisha, he revived, and stood up on his feet.")},
        {tr("Elías nunca murió: subió en un torbellino, y su manto quedó en la tierra.", "Elijah never died: he went up in a whirlwind, and his mantle stayed on the earth."),
         tr("Eliseo murió, y lo sepultaron. Y aun sus huesos daban vida.", "Elisha died, and they buried him. And even his bones gave life."),
         tr("Israel volvió a olvidar. Y volvió a haber profetas.", "Israel forgot again. And there were prophets again."),
         tr("El manto sigue cayendo sobre hombros nuevos.", "The mantle keeps falling on new shoulders.")},
        "", ""
    };
}

// =============================================================================
// Referencias por objeto / milagro / enemigo
// =============================================================================
inline Scripture get_item_scripture(u32 item_id) {
    switch (item_id) {
        case 0:  return {"1 Samuel 17:40", tr("Tomó cinco piedras lisas del arroyo, y su honda en su mano.", "He took five smooth stones out of the brook, and his sling was in his hand.")};
        case 1:  return {"Números 17:8",   tr("La vara de Aarón había reverdecido, y echado flores y almendras.", "Aaron's rod had budded, and brought forth buds, and bloomed blossoms, and yielded almonds.")};
        case 2:  return {"1 Reyes 19:6",   tr("A su cabecera una torta cocida sobre las ascuas, y una vasija de agua.", "At his head a cake baken on the coals, and a cruse of water.")};
        case 3:  return {"1 Reyes 19:16",  tr("Y ungirás a Eliseo hijo de Safat, para que sea profeta en tu lugar.", "And Elisha the son of Shaphat shalt thou anoint to be prophet in thy room.")};
        case 4:  return {"Éxodo 12:11",    tr("Vuestros lomos ceñidos, vuestro calzado en vuestros pies.", "Your loins girded, your shoes on your feet.")};
        case 5:  return {"Éxodo 16:31",    tr("Era como semilla de culantro, blanco, y su sabor como de hojuelas con miel.", "It was like coriander seed, white; and the taste of it like wafers made with honey.")};
        case 6:  return {"Éxodo 31:18",    tr("Dos tablas del testimonio, escritas con el dedo de Dios.", "Two tables of testimony, written with the finger of God.")};
        case 7:  return {"Éxodo 25:10",    tr("Harán también un arca de madera de acacia.", "And they shall make an ark of shittim wood.")};
        case 8:  return {"Salmo 104:15",   tr("El vino que alegra el corazón del hombre.", "Wine that maketh glad the heart of man.")};
        case 9:  return {"2 Reyes 5:14",   tr("Se zambulló siete veces en el Jordán, y su carne se volvió como la de un niño.", "He dipped himself seven times in Jordan, and his flesh came again like the flesh of a little child.")};
        case 10: return {"1 Reyes 18:38",  tr("Entonces cayó fuego de Jehová, y consumió el holocausto.", "Then the fire of the Lord fell, and consumed the burnt sacrifice.")};
        case 11: return {"Josué 4:7",      tr("Estas piedras servirán de memorial a los hijos de Israel para siempre.", "These stones shall be for a memorial unto the children of Israel for ever.")};
        case 12: return {"1 Reyes 18:38",  tr("Consumió la leña, las piedras y el polvo, y aun lamió el agua de la zanja.", "It consumed the wood, the stones and the dust, and licked up the water in the trench.")};
        case 13: return {"1 Reyes 17:16",  tr("La harina de la tinaja no escaseó, ni el aceite de la vasija menguó.", "The barrel of meal wasted not, neither did the cruse of oil fail.")};
        case 14: return {"2 Reyes 4:4",    tr("Echarás aceite en todas las vasijas, y pondrás aparte la que esté llena.", "Thou shalt pour out into all those vessels, and set aside that which is full.")};
        case 15: return {"2 Reyes 2:13",   tr("Alzó el manto que se le había caído a Elías, y volvió a la orilla del Jordán.", "He took up the mantle that fell from Elijah, and went back to the bank of Jordan.")};
        case 16: return {"2 Reyes 13:17",  tr("Saeta de salvación de Jehová, y saeta de salvación contra Siria.", "The arrow of the Lord's deliverance, and the arrow of deliverance from Syria.")};
        case 17: return {"2 Reyes 20:7",   tr("Tomad masa de higos. Y tomándola, la pusieron sobre la llaga, y sanó.", "Take a lump of figs. And they took and laid it on the boil, and he recovered.")};
        case 18: return {"2 Reyes 2:21",   tr("Echó la sal en ellas, y dijo: Yo sané estas aguas.", "He cast the salt in there, and said: I have healed these waters.")};
        case 19: return {"2 Reyes 1:8",    tr("Un varón que tenía vestido de pelo, y ceñía sus lomos con un cinto de cuero.", "A hairy man, and girt with a girdle of leather about his loins.")};
        case 20: return {"2 Reyes 20:11",  tr("E hizo volver la sombra por los grados que había descendido.", "And he brought the shadow back by the degrees it had gone down.")};
        case 21: return {"2 Reyes 4:43",   tr("Da a la gente para que coman, porque así ha dicho Jehová: Comerán, y sobrará.", "Give unto the people, that they may eat, for thus saith the Lord: They shall eat, and shall leave thereof.")};
        default: return {"", nullptr};
    }
}

inline Scripture get_miracle_scripture(MiracleID id) {
    switch (id) {
        case MiracleID::CuervosDeQuerit:   return {"1 Reyes 17:6",  tr("Los cuervos le traían pan y carne por la mañana y por la tarde.", "The ravens brought him bread and flesh in the morning and in the evening.")};
        case MiracleID::PalabraDeSequia:   return {"1 Reyes 17:1",  tr("No habrá lluvia ni rocío en estos años, sino por mi palabra.", "There shall not be dew nor rain these years, but according to my word.")};
        case MiracleID::FuegoDelCielo:     return {"1 Reyes 18:38", tr("Entonces cayó fuego de Jehová, y consumió el holocausto.", "Then the fire of the Lord fell, and consumed the burnt sacrifice.")};
        case MiracleID::CarroDeFuego:      return {"2 Reyes 2:11",  tr("Un carro de fuego con caballos de fuego apartó a los dos.", "A chariot of fire and horses of fire parted them both.")};
        case MiracleID::AltarConsumido:    return {"1 Reyes 18:38", tr("Consumió la leña, las piedras y el polvo.", "It consumed the wood, the stones and the dust.")};
        case MiracleID::SanarAguas:        return {"2 Reyes 2:21",  tr("Yo sané estas aguas, y no habrá más en ellas muerte.", "I have healed these waters; there shall not be from thence any more death.")};
        case MiracleID::AceiteInagotable:  return {"2 Reyes 4:6",   tr("Cuando las vasijas fueron llenas, el aceite se detuvo.", "When the vessels were full, the oil stayed.")};
        case MiracleID::OjosAbiertos:      return {"2 Reyes 6:17",  tr("Jehová abrió los ojos del criado, y vio el monte lleno de carros de fuego.", "The Lord opened the eyes of the young man, and he saw the mountain full of chariots of fire.")};
        case MiracleID::EjercitoCelestial: return {"2 Reyes 6:17",  tr("El monte estaba lleno de gente de a caballo y de carros de fuego.", "The mountain was full of horses and chariots of fire.")};
        case MiracleID::DoblePorcion:      return {"2 Reyes 2:9",   tr("Te ruego que una doble porción de tu espíritu sea sobre mí.", "I pray thee, let a double portion of thy spirit be upon me.")};
        default:                           return {"", nullptr};
    }
}

// Entrada de bestiario: referencia y una linea que explica de donde sale el enemigo.
inline Scripture get_enemy_lore(EnemyType type) {
    switch (type) {
        case EnemyType::SoldadoDeAcab:
            return {"1 Reyes 18:10", tr("Acab hizo jurar a las naciones que no te habían hallado.", "Ahab took an oath of the nations that they had not found thee.")};
        case EnemyType::ArqueroReal:
            return {"1 Reyes 22:34", tr("Un hombre disparó su arco a la ventura, e hirió al rey de Israel.", "A certain man drew a bow at a venture, and smote the king of Israel.")};
        case EnemyType::ProfetaDeBaal:
            return {"1 Reyes 18:28", tr("Clamaban a grandes voces, y se sajaban con cuchillos hasta chorrear la sangre.", "They cried aloud, and cut themselves with knives till the blood gushed out.")};
        case EnemyType::LeonDelDesierto:
            return {"1 Reyes 13:24", tr("Y encontrándole un león en el camino, le mató.", "And a lion found him on the road, and slew him.")};
        case EnemyType::SoldadoSirio:
            return {"2 Reyes 6:14",  tr("Envió allá gente de a caballo, y carros, y un gran ejército.", "He sent thither horses, and chariots, and a great host.")};
        case EnemyType::GeneralSirio:
            return {"2 Reyes 5:1",   tr("Naamán era varón grande delante de su señor; pero leproso.", "Naaman was a great man with his master; but he was a leper.")};
        case EnemyType::IdolatraDeRimon:
            return {"2 Reyes 5:18",  tr("Cuando mi señor entra en el templo de Rimón para adorar en él.", "When my master goeth into the house of Rimmon to worship there.")};
        case EnemyType::AguaCorrupta:
            return {"2 Reyes 2:19",  tr("Las aguas son malas, y la tierra es estéril.", "The water is naught, and the ground barren.")};
        default:
            return {"", nullptr};
    }
}
