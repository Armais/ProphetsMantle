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
            {"1 Reyes 17:4", tr("Beberás del arroyo; y yo he mandado a los cuervos que te sustenten allí.", "Thou shalt drink of the brook; and I have commanded the ravens to feed thee there.")},
            // 17:1-3 ya no se repiten aqui: los acaba de jugar la audiencia con Acab
            // (scenes.h, Scene::AUDIENCIA) justo antes de esta intro.
            {tr("Los hombres del rey ya buscan tu rastro por todo Israel.", "The king's men are already hunting your trail through all Israel."),
             nullptr, nullptr, nullptr},
            tr("Sobrevive junto al arroyo. Los cuervos te sustentarán.", "Survive by the brook. The ravens will feed you."),
            tr("El capitán ha caído. Pero el arroyo se seca: no ha llovido sobre la tierra.", "The captain has fallen. But the brook dries up: no rain has fallen on the land."),
            tr("¡El rey te quiere vivo, profeta! ¡Vivo, o en pedazos!", "The king wants you alive, prophet! Alive, or in pieces!")
        };
        case 2: return {
            {"1 Reyes 17:14", tr("La harina de la tinaja no escaseará, ni el aceite de la vasija disminuirá.", "The barrel of meal shall not waste, neither shall the cruse of oil fail.")},
            {tr("«Levántate, vete a Sarepta de Sidón, y mora allí».", "«Arise, go to Zarephath of Sidon, and dwell there»."),
             tr("Sarepta es tierra de Jezabel: aquí Baal es el único nombre que se pronuncia.", "Zarephath is Jezebel's land: here Baal is the only name spoken aloud."),
             tr("Una viuda recoge dos leños para cocer su último puñado de harina.", "A widow gathers two sticks to bake her last handful of meal."),
             nullptr},
            tr("Halla a la viuda. Que la tinaja no se acabe.", "Find the widow. Let the barrel never empty."),
            tr("El niño ha vuelto a la vida. «Ahora conozco que tú eres varón de Dios».", "The child is alive again. «Now I know that thou art a man of God»."),
            tr("¡Sarepta es de Baal! ¡Y tú serás su ofrenda!", "Zarephath belongs to Baal! And you will be its offering!")
        };
        case 3: return {
            {"1 Reyes 18:21", tr("¿Hasta cuándo claudicaréis vosotros entre dos pensamientos?", "How long halt ye between two opinions?")},
            {tr("Al tercer año, la palabra vino a Elías en Sarepta: «Ve, muéstrate a Acab, y yo haré llover sobre la tierra».", "In the third year the word came to Elijah in Zarephath: «Go, shew thyself unto Ahab, and I will send rain upon the earth»."),
             tr("Israel entero sube al monte Carmelo: cuatrocientos cincuenta profetas de Baal contra un solo hombre.", "All Israel goes up to Mount Carmel: four hundred and fifty prophets of Baal against one man."),
             tr("Dos altares, dos bueyes, y ningún fuego encendido por mano humana.", "Two altars, two bullocks, and no fire kindled by human hand."),
             nullptr},
            tr("Que el pueblo vea de quién es el fuego.", "Let the people see whose the fire is."),
            tr("«¡Jehová es el Dios!». Y el cielo se oscureció, y hubo una gran lluvia.", "«The Lord, he is the God!». And the sky darkened, and there was a great rain."),
            tr("¡Baal, respóndenos! ¡Respóndenos! (Y se sajaban con cuchillos.)", "Baal, hear us! Hear us! (And they cut themselves with knives.)")
        };
        case 4: return {
            {"1 Reyes 19:12", tr("Y tras el fuego un silbo apacible y delicado.", "And after the fire a still small voice.")},
            // El juramento de Jezabel (19:2) ya no se repite aqui: lo acaba de traer su
            // mensajero en la apertura del capitulo (scenes.h, Scene::APERTURA_4).
            {tr("Elías huyó al desierto, se sentó bajo un enebro y pidió morirse.", "Elijah fled to the desert, sat under a juniper and asked to die."),
             tr("Cuarenta días y cuarenta noches hasta Horeb, el monte de Dios.", "Forty days and forty nights to Horeb, the mount of God."),
             nullptr},
            tr("«¿Qué haces aquí, Elías?»", "«What doest thou here, Elijah?»"),
            tr("No en el viento, ni en el terremoto, ni en el fuego. Siete mil rodillas no se doblaron ante Baal.", "Not in the wind, nor the earthquake, nor the fire. Seven thousand knees never bowed to Baal."),
            tr("Basta ya, Jehová; quítame la vida, porque no soy yo mejor que mis padres.", "It is enough; now, O Lord, take away my life, for I am not better than my fathers.")
        };
        case 5: return {
            {"1 Reyes 19:19", tr("Y pasando Elías por delante de él, echó sobre él su manto.", "And Elijah passed by him, and cast his mantle upon him.")},
            {tr("De Horeb bajó Elías con un encargo: ungir al que había de ser profeta en su lugar.", "From Horeb Elijah came down with a charge: to anoint the one who would be prophet in his room."),
             tr("Halló a Eliseo arando con doce yuntas de bueyes, y al pasar le echó el manto encima.", "He found Elisha plowing with twelve yoke of oxen, and passing by he cast his mantle upon him."),
             tr("Mientras, Acab codició la viña de Nabot, y Jezabel se la compró con sangre.", "Meanwhile Ahab coveted Naboth's vineyard, and Jezebel bought it for him with blood."),
             nullptr},
            tr("Enfrenta al rey y a la reina en el campo que robaron.", "Face the king and the queen in the field they stole."),
            tr("«Donde lamieron los perros la sangre de Nabot, lamerán también tu sangre».", "«Where the dogs licked the blood of Naboth shall dogs lick thy blood»."),
            tr("¿Me has hallado, enemigo mío?", "Hast thou found me, O mine enemy?")
        };
        case 6: return {
            {"2 Reyes 2:21", tr("Yo sané estas aguas, y no habrá más en ellas muerte ni enfermedad.", "I have healed these waters; there shall not be from thence any more death or barrenness.")},
            {tr("Con el manto de Elías al hombro, Eliseo volvió a cruzar el Jordán y subió a Jericó.", "With Elijah's mantle on his shoulder, Elisha crossed the Jordan again and went up to Jericho."),
             tr("«La situación de esta ciudad es buena, pero las aguas son malas y la tierra estéril».", "«The situation of this city is pleasant, but the water is naught and the ground barren»."),
             tr("La corrupción brota del manantial y se extiende por las calles de Jericó.", "The corruption springs from the well and spreads through the streets of Jericho."),
             nullptr},
            tr("Sana el manantial de Jericó.", "Heal the spring of Jericho."),
            tr("Y fueron sanas las aguas hasta el día de hoy.", "So the waters were healed unto this day."),
            tr("Esta agua es mía. Bebe de ella.", "This water is mine. Drink of it.")
        };
        case 7: return {
            {"2 Reyes 4:3", tr("Pide para ti vasijas prestadas de todos tus vecinos; y no pidas pocas.", "Borrow thee vessels abroad of all thy neighbours; borrow not a few.")},
            {tr("«Mi marido ha muerto, y el acreedor ha venido para llevarse a mis dos hijos por siervos».", "«My husband is dead, and the creditor is come to take my two sons to be bondmen»."),
             tr("«¿Qué tienes en casa?». «Ninguna cosa, sino una vasija de aceite».", "«What hast thou in the house?». «Nothing, save a pot of oil»."),
             tr("El acreedor recorre la aldea cobrando en carne.", "The creditor walks the village collecting in flesh."),
             nullptr},
            tr("Reúne vasijas. Que el aceite no se detenga.", "Gather vessels. Let the oil not stop."),
            tr("«Vende el aceite y paga tus deudas; y tú y tus hijos vivid de lo que quede».", "«Sell the oil, and pay thy debt; and live thou and thy children of the rest»."),
            tr("La deuda se paga con hijos o con sangre.", "A debt is paid in sons or in blood.")
        };
        case 8: return {
            {"2 Reyes 4:34", tr("Y se tendió sobre el niño, y el cuerpo del niño entró en calor.", "And he stretched himself upon the child; and the flesh of the child waxed warm.")},
            {tr("Una mujer importante de Sunem le hizo un aposento con cama, mesa, silla y candelero.", "A great woman of Shunem made him a chamber with a bed, a table, a stool and a candlestick."),
             tr("El hijo que Eliseo le prometió cayó diciendo: «¡Ay, mi cabeza!».", "The son Elisha promised her fell down crying: «My head, my head!»."),
             tr("Y murió sobre las rodillas de su madre al mediodía.", "And he died on his mother's knees at noon."),
             nullptr},
            tr("«Déjala, porque su alma está en amargura».", "«Let her alone; for her soul is vexed within her»."),
            tr("El niño estornudó siete veces, y abrió sus ojos.", "The child sneezed seven times, and opened his eyes."),
            tr("Todo lo que respira es mío. También el hijo prometido.", "All that breathes is mine. The promised son as well.")
        };
        case 9: return {
            {"2 Reyes 5:10", tr("Ve y lávate siete veces en el Jordán, y serás limpio.", "Go and wash in Jordan seven times, and thou shalt be clean.")},
            {tr("Naamán, general del rey de Siria, era varón valeroso; pero leproso.", "Naaman, captain of the host of Syria, was a mighty man; but he was a leper."),
             tr("Vino con caballos y carros, con plata, oro y vestidos, esperando un espectáculo.", "He came with horses and chariots, with silver, gold and raiment, expecting a spectacle."),
             tr("«¿Abana y Farfar, ríos de Damasco, no son mejores que las aguas de Israel?»", "«Are not Abana and Pharpar, rivers of Damascus, better than all the waters of Israel?»"),
             nullptr},
            tr("Que el orgulloso se moje siete veces.", "Let the proud man wash seven times."),
            tr("Su carne se volvió como la de un niño. Pero Guiezi corrió tras la plata.", "His flesh came again like a child's. But Gehazi ran after the silver."),
            tr("La lepra de Naamán se te pegará a ti y a tu descendencia para siempre.", "The leprosy of Naaman shall cleave unto thee, and unto thy seed for ever.")
        };
        case 10: return {
            {"2 Reyes 6:16", tr("No tengas miedo, porque más son los que están con nosotros que los que están con ellos.", "Fear not: for they that be with us are more than they that be with them.")},
            {tr("Ni la carne nueva de Naamán trajo la paz: Siria guerreaba contra Israel, y alguien revelaba cada uno de sus planes.", "Naaman's new flesh brought no peace: Syria warred against Israel, and someone revealed every one of his plans."),
             tr("«Eliseo declara al rey de Israel las palabras que tú hablas en tu cámara».", "«Elisha telleth the king of Israel the words that thou speakest in thy bedchamber»."),
             tr("Y de noche cercaron Dotán con gente, con carros y un gran ejército.", "And by night they compassed Dothan about with horses, chariots and a great host."),
             nullptr},
            tr("«Te ruego, Jehová, que abras sus ojos para que vea».", "«Lord, I pray thee, open his eyes that he may see»."),
            tr("El monte estaba lleno de gente de a caballo y de carros de fuego alrededor de Eliseo.", "The mountain was full of horses and chariots of fire round about Elisha."),
            tr("Toda Siria contra un solo hombre. Ríndete, profeta.", "All Syria against one man. Surrender, prophet.")
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
        {"1 Reyes 16:33", tr("Acab hizo más para provocar la ira de Jehová que todos los reyes de Israel que antes de él habían sido.", "Ahab did more to provoke the Lord to anger than all the kings of Israel before him.")},
        {tr("Israel ha olvidado a su Dios.", "Israel has forgotten its God."),
         tr("Acab reina en Samaria, y Jezabel su reina ha llenado la tierra de altares a Baal.", "Ahab reigns in Samaria, and Jezebel his queen has filled the land with altars to Baal."),
         tr("Los profetas de Jehová son perseguidos, y los que quedan se esconden en cuevas.", "The prophets of the Lord are hunted, and those who remain hide in caves."),
         tr("Solo un hombre de Tisbe de Galaad se atreve a estar delante del rey.", "Only one man from Tishbe of Gilead dares to stand before the king.")},
        "", "", ""
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
inline ChapterLore get_ascension_lore(int page) {
    switch (page) {
        case 0: return {
            {"2 Reyes 2:9", tr("Pide lo que quieras que haga por ti, antes que yo sea quitado de ti.", "Ask what I shall do for thee, before I be taken away from thee.")},
            {tr("Elías y Eliseo cruzaron el Jordán en seco, golpeando las aguas con el manto.", "Elijah and Elisha crossed the Jordan dry-shod, smiting the waters with the mantle."),
             tr("«Te ruego que una doble porción de tu espíritu sea sobre mí».", "«I pray thee, let a double portion of thy spirit be upon me»."),
             tr("«Cosa difícil has pedido. Si me vieres cuando fuere quitado de ti, te será hecho».", "«Thou hast asked a hard thing. If thou see me when I am taken from thee, it shall be so»."),
             nullptr},
            "", "", ""
        };
        case 1: return {
            {"2 Reyes 2:11", tr("Un carro de fuego con caballos de fuego apartó a los dos; y Elías subió al cielo en un torbellino.", "A chariot of fire and horses of fire parted them both; and Elijah went up by a whirlwind into heaven.")},
            {tr("Y aconteció que yendo ellos y hablando, he aquí un carro de fuego.", "And it came to pass, as they still went on and talked, that behold, a chariot of fire."),
             tr("Y Eliseo lo veía, y clamaba: «¡Padre mío, padre mío, carro de Israel y su gente de a caballo!».", "And Elisha saw it, and cried: «My father, my father, the chariot of Israel, and the horsemen thereof!»."),
             tr("Y nunca más le vio.", "And he saw him no more."),
             nullptr},
            "", "", ""
        };
        default: return {
            {"2 Reyes 2:14", tr("¿Dónde está Jehová, el Dios de Elías? Y golpeó las aguas, y se apartaron.", "Where is the Lord God of Elijah? And he smote the waters, and they parted.")},
            {tr("Eliseo alzó el manto que se le había caído a Elías, y volvió al Jordán.", "Elisha took up the mantle that fell from Elijah, and went back to the Jordan."),
             tr("Golpeó las aguas con él, y se apartaron a un lado y a otro, y pasó.", "He smote the waters with it, and they parted hither and thither, and he went over."),
             tr("El manto no era suyo. Le fue prestado.", "The mantle was not his own. It was lent to him."),
             nullptr},
            "", "", ""
        };
    }
}

inline ChapterLore get_epilogue_lore() {
    return {
        {"2 Reyes 13:21", tr("Cuando llegó a tocar el muerto los huesos de Eliseo, revivió, y se levantó sobre sus pies.", "When the dead man touched the bones of Elisha, he revived, and stood up on his feet.")},
        {tr("Elías nunca murió: subió en un torbellino, y su manto quedó en la tierra.", "Elijah never died: he went up in a whirlwind, and his mantle stayed on the earth."),
         tr("Eliseo murió, y lo sepultaron. Y aun sus huesos daban vida.", "Elisha died, and they buried him. And even his bones gave life."),
         tr("Israel volvió a olvidar. Y volvió a haber profetas.", "Israel forgot again. And there were prophets again."),
         tr("El manto sigue cayendo sobre hombros nuevos.", "The mantle keeps falling on new shoulders.")},
        "", "", ""
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
