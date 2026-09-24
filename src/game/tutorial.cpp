// =============================================================================
// MANTLE - El tutorial: tabla de pasos, textos y sus tres salas
// =============================================================================
// Las decisiones de diseno y la regla de voz estan en tutorial.h. Aqui solo el contenido.

#include "game/tutorial.h"
#include "game/rooms.h"
#include "core/config.h"

#include <cstdlib>

// =============================================================================
// Tabla
// =============================================================================
// `target` > 1 = hay que repetir la accion esas veces. No hay relojes: TODOS los pasos se
// cierran haciendo lo que el cartel nombra (la decision de 2026-09-13, en tutorial.h).
const TutorialStep TUTORIAL_STEPS[] = {
    // Sala 1 - el cuerpo
    {TutorialGoal::Move,       0, 1},
    {TutorialGoal::Attack,     0, 2},
    {TutorialGoal::Secondary,  0, 1},
    {TutorialGoal::Dash,       0, 1},
    {TutorialGoal::Consumable, 0, 1},
    // Sala 2 - la fe
    {TutorialGoal::Miracle1,   1, 1},
    {TutorialGoal::Miracle2,   1, 1},
    {TutorialGoal::Miracle3,   1, 1},
    {TutorialGoal::Pray,       1, 1},
    // Sala 3 - el mundo
    {TutorialGoal::Secrets,    2, 2},
    {TutorialGoal::Chest,      2, 1},
    {TutorialGoal::Offering,   2, 1},
    {TutorialGoal::Map,        2, 1},
    {TutorialGoal::Pause,      2, 1},
    {TutorialGoal::CrossExit,  2, 1},
};
const int TUTORIAL_STEP_COUNT = static_cast<int>(sizeof(TUTORIAL_STEPS) / sizeof(TUTORIAL_STEPS[0]));

// =============================================================================
// Textos
// =============================================================================
// Dos lineas por cartel: la instruccion y el sabor. El sabor es INVENTADO, en el registro
// del juego, y no finge versiculo (ver la nota de voz en tutorial.h).
//
// Techo de ancho: el cartel mide 236 unidades y el texto se mide con `measure_text` antes
// de pintarse, asi que lo que se pase de largo baja de escala en vez de desbordarse. Aun
// asi, la instruccion se escribe corta a proposito: es lo que hay que leer de un vistazo
// mientras un arquero te dispara.
TutorialText tutorial_text(TutorialGoal goal, bool touch) {
    switch (goal) {
        case TutorialGoal::Move:
            return {touch ? tr("Muévete: pulgar en la mitad izquierda",
                               "Move: thumb on the left half")
                          : tr("Muévete: W A S D o las flechas",
                               "Move: W A S D or the arrows"),
                    tr("Los pies del profeta conocen el desierto.",
                       "The prophet's feet know the wilderness.")};

        case TutorialGoal::Attack:
            return {touch ? tr("Rompe dos tinajas: toca la mitad derecha",
                               "Break two jars: tap the right half")
                          : tr("Rompe dos tinajas: mantén K",
                               "Break two jars: hold K"),
                    tr("Disparas hacia donde caminas: el cayado sigue al paso.",
                       "You shoot where you walk: the staff follows your step.")};

        case TutorialGoal::Secondary:
            return {touch ? tr("Usa la honda: botón ARMA",
                               "Use the sling: WEAPON button")
                          : tr("Usa la honda: I o clic derecho",
                               "Use the sling: I or right click"),
                    tr("La honda alcanza donde la mano no llega.",
                       "The sling reaches where the hand cannot.")};

        case TutorialGoal::Dash:
            // Aqui entra el arquero (Engine::tutorial_setup_step): esquivar con alguien
            // disparandote es lo que hace que el dash signifique algo.
            return {touch ? tr("Esquiva al arquero: botón DASH", "Dodge the archer: DASH button")
                          : tr("Esquiva al arquero: ESPACIO o J", "Dodge the archer: SPACE or J"),
                    tr("Mientras esquivas, nada te toca.",
                       "While you dodge, nothing touches you.")};

        case TutorialGoal::Consumable:
            return {touch ? tr("Bébete el agua: botón USAR",
                               "Drink the water: USE button")
                          : tr("Bébete el agua: R", "Drink the water: R"),
                    tr("Come y bebe, que el camino es largo.",
                       "Eat and drink, for the road is long.")};

        case TutorialGoal::Miracle1:
            return {touch ? tr("Lanza el primer milagro: su celda del HUD",
                               "Cast the first miracle: its HUD cell")
                          : tr("Lanza el primer milagro: Q", "Cast the first miracle: Q"),
                    tr("Cada milagro cuesta fe, y su precio está escrito en la celda.",
                       "Every miracle costs faith, and its price is written on the cell.")};

        case TutorialGoal::Miracle2:
            return {touch ? tr("Ahora el segundo: la celda de al lado",
                               "Now the second: the next cell")
                          : tr("Ahora el segundo: E", "Now the second: E"),
                    tr("Tres dones lleva el profeta, y no todos sirven para lo mismo.",
                       "Three gifts the prophet bears, and not all serve the same end.")};

        case TutorialGoal::Miracle3:
            return {touch ? tr("Y el tercero: la tercera celda",
                               "And the third: the third cell")
                          : tr("Y el tercero: F", "And the third: F"),
                    tr("El fuego del cielo pide fe alta: sin ella, la celda calla.",
                       "Fire from heaven asks for high faith: without it, the cell is silent.")};

        case TutorialGoal::Pray:
            return {tr("Ora en el altar: písalo",
                       "Pray at the altar: step onto it"),
                    tr("Orando se recobra la fe, y de orar vienen los milagros nuevos.",
                       "Praying restores faith, and from praying come new miracles.")};

        case TutorialGoal::Secrets:
            return {tr("Dispara al matorral y a la tinaja: dos escondrijos",
                       "Shoot the brush and the jar: two caches"),
                    tr("Lo que se esconde en el campo paga a quien se desvía.",
                       "What hides in the field pays the one who turns aside.")};

        case TutorialGoal::Chest:
            return {tr("Acércate al cofre",
                       "Walk up to the chest"),
                    tr("Un cofre cura, da fe y ofrece una bendición.",
                       "A chest heals, gives faith and offers a blessing.")};

        case TutorialGoal::Offering:
            return {tr("Recoge la ofrenda dorada",
                       "Pick up the golden offering"),
                    tr("La ofrenda no se pierde al morir: se gasta en El Santuario.",
                       "An offering survives your death: spend it in the Sanctuary.")};

        case TutorialGoal::Map:
            return {touch ? tr("Abre el mapa: toca el minimapa",
                               "Open the map: tap the minimap")
                          : tr("Abre el mapa: mantén TAB", "Open the map: hold TAB"),
                    tr("El mapa dice lo pisado, lo entrevisto y lo que aún es noche.",
                       "The map tells what you trod, what you glimpsed and what is still night.")};

        case TutorialGoal::Pause:
            return {touch ? tr("Pausa: el botón II de arriba",
                               "Pause: the II button up top")
                          : tr("Pausa: ESC", "Pause: ESC"),
                    tr("En la pausa vive el Códice, donde se anota todo lo que ves.",
                       "The Codex lives in the pause menu: everything you meet is written there.")};

        case TutorialGoal::CrossExit:
            return {tr("Cruza el arco: ahí acaba cada capítulo",
                       "Cross the arch: that is how a chapter ends"),
                    tr("Explora el camino, halla al jefe y cruza su arco.",
                       "Explore the road, find the boss and cross his arch.")};

        default:
            return {"", ""};
    }
}

TutorialText tutorial_door_text() {
    return {tr("Cruza la puerta del norte",
               "Cross the north door"),
            tr("Se abre lo que estaba cerrado, y hay más que ver.",
               "What was shut is opened, and there is more to see.")};
}

const char* tutorial_choice_title() {
    return tr("ELIGES UNA DE TRES", "YOU PICK ONE OF THREE");
}

// =============================================================================
// Las tres salas
// =============================================================================
// Se montan sobre una Room REUTILIZADA (Engine::current_room_data), igual que el escenario
// de una escena guionizada (Engine::start_scene): nada de esto entra en `floor_map` ni se
// guarda ni toca el mundo fijo de ningun capitulo.
//
// Las dos primeras son INTERIORES de una pantalla (camara clavada, sin donde perderse) y la
// tercera es campo abierto, que es donde el matorral, el minimapa y el arco significan algo.
// La puerta de salida va SIEMPRE en el centro del muro norte y nace cerrada:
// Engine::update_tutorial la abre al cerrarse el ultimo paso de la sala.

// Blancos de tiro de la sala 1, en fila y a la vista desde la entrada. Cinco: dos para el
// cayado, una para la honda y las de los lados para quien falle. Solo sobre suelo libre, para
// que replantar no pise nada de lo que haya en la sala.
void tutorial_plant_jars(Room& room) {
    for (int i = 0; i < 5; i++) {
        const int tx = 4 + i * 3;
        if (room.tiles[5][tx] == TileType::Floor) room.tiles[5][tx] = TileType::Jar;
    }
}

void tutorial_open_north_door(Room& room) {
    room.tiles[0][room.tiles_w / 2] = TileType::Doorway;
    room.has_doorway = true;
    room.door_tx = room.tiles_w / 2;
    room.door_ty = 0;
}

void tutorial_build_room(Room& room, int index, int chapter) {
    // Determinismo: `generate` tira del rand() global y el exterior lo usa para decorar. Con
    // la semilla clavada aqui las tres salas salen identicas en todas las partidas, que es
    // la misma regla que el mundo fijo de los capitulos (Engine::generate_pristine_room).
    std::srand(0x7501u + static_cast<unsigned>(index) * 2654435761u);

    if (index <= 1) {
        room.generate(RoomType::Interior, chapter, 1, -1, false);
        const int W = room.tiles_w, H = room.tiles_h;

        // Se vacia lo que build_interior dejo dentro (pilares, tinajas, y el altar o el cofre
        // del fondo): cada sala del tutorial pone SOLO lo que su paso necesita, y un pilar en
        // medio de la sala de combate tapa justo al blanco al que hay que disparar.
        for (int y = 1; y < H - 1; y++)
            for (int x = 1; x < W - 1; x++)
                room.tiles[y][x] = TileType::Floor;

        // Por donde se entro, cerrado: de aqui solo se sale por el norte y al completarlo.
        room.tiles[H - 1][W / 2] = TileType::Wall;
        room.tiles[0][W / 2]     = TileType::Wall;
        room.has_doorway = false;

        // Antorchas flanqueando la puerta que se va a abrir: es donde hay que mirar cuando
        // el ultimo paso de la sala se cierre.
        room.tiles[1][W / 2 - 2] = TileType::Torch;
        room.tiles[1][W / 2 + 2] = TileType::Torch;

        // FILA 5, y no la 2 o la 3: el cartel del tutorial ocupa y 34..62 unidades, o sea
        // justo esas dos filas, y tapaba entero lo que el paso manda mirar (se vio en la
        // primera captura: la tinaja del centro y el altar desaparecian detras del panel).
        if (index == 0) {
            tutorial_plant_jars(room);
        } else {
            // El altar del paso de la oracion. DESCENTRADO a proposito: en la columna del
            // profeta cae justo en la linea recta entre donde nace y la puerta del norte, y
            // se rezaba de paso —sin querer— al cruzar la sala.
            room.tiles[5][W / 2 + 4] = TileType::AltarGod;
        }
        room.cells[0].cleared = true;
        room.cells[0].visited = true;
        return;
    }

    // --- Sala 3: campo abierto (una celda, 40x22) --------------------------------------
    room.door_north = room.door_south = room.door_east = room.door_west = false;
    room.generate(RoomType::Normal, chapter, 1);
    const int W = room.tiles_w, H = room.tiles_h;

    // Un claro grande en la mitad de abajo: la decoracion del bioma es bonita, pero el paso
    // de los secretos exige saber CUAL es el matorral que hay que cortar.
    for (int y = H / 2 - 1; y < H - 2; y++)
        for (int x = 3; x < W - 3; x++)
            room.tiles[y][x] = TileType::Floor;

    const int cx = W / 2, cy = H - 5;   // por donde aparece el profeta

    // Los escondrijos del paso `Secrets`, en grupo (se leen como una mata, no como adorno
    // suelto) y a CUATRO tiles: un tiro alcanza 120 unidades (`Config::PROJECTILE_RANGE`),
    // o sea 7,5 tiles, y con el grupo mas lejos el segundo escondrijo no se podia abrir sin
    // caminar hasta el. Dos de ellos caen en la MISMA fila que el profeta: es lo que hace
    // que el paso se entienda sin explicar hacia donde apuntar.
    room.tiles[cy][cx - 4] = TileType::TallGrass;
    room.tiles[cy][cx - 5] = TileType::TallGrass;
    room.tiles[cy - 2][cx - 4] = TileType::Bush;
    room.tiles[cy - 2][cx - 5] = TileType::Bush;
    room.tiles[cy - 1][cx - 6] = TileType::Bush;
    room.tiles[cy + 1][cx - 6] = TileType::Jar;

    // El cofre, al otro lado: hay que cruzar el claro para llegar.
    room.tiles[cy - 1][cx + 6] = TileType::Chest;

    // El arco de salida del capitulo, al fondo y ya abierto. Es la SALIDA del tutorial
    // (paso `CrossExit`) y la pieza que hay que reconocer cuando caiga el primer jefe de
    // verdad. Pisarlo en un paso anterior no hace nada: Engine::update_room_state guarda
    // esa rama contra `tutorial.active`, asi que de aqui no sale ningun epilogo narrado.
    // Cerca y DESCENTRADO. Medido sobre la captura: con la camara pegada al borde de abajo
    // de la celda, todo lo que este a mas de dos tiles por encima del profeta cae detras del
    // cartel del tutorial (y 34..62), y el cartel del ultimo paso habla justo de este arco.
    room.exit_tx = cx + 4;
    room.exit_ty = cy - 2;
    room.exit_open = true;
    room.tiles[room.exit_ty][room.exit_tx] = TileType::Exit;

    room.has_doorway = false;
    room.cells[0].cleared = true;
    room.cells[0].visited = true;
}

// Los SEIS escondrijos de la sala 3, por posicion: las mismas coordenadas que siembra
// `tutorial_build_room`. Contar props de la sala no vale — el bioma decora con matorral y
// un Fuego del Cielo lanzado a otra cosa cerraba el paso sin que el jugador lo entendiera.
int tutorial_secrets_opened(const Room& room) {
    const int cx = room.tiles_w / 2, cy = room.tiles_h - 5;
    const int spots[6][2] = {              // {tx, ty}
        {cx - 4, cy}, {cx - 5, cy},
        {cx - 4, cy - 2}, {cx - 5, cy - 2},
        {cx - 6, cy - 1}, {cx - 6, cy + 1},
    };
    int opened = 0;
    for (const auto& sp : spots) {
        const int tx = sp[0], ty = sp[1];
        if (tx < 0 || tx >= room.tiles_w || ty < 0 || ty >= room.tiles_h) continue;
        const TileType t = room.tiles[ty][tx];
        if (t != TileType::Jar && t != TileType::Bush && t != TileType::TallGrass) opened++;
    }
    return opened;
}

void tutorial_spawn_tile(int index, int& tx, int& ty) {
    if (index <= 1) {
        tx = Config::SCREEN_TILES_X / 2;
        ty = Config::SCREEN_TILES_Y - 3;
    } else {
        // La MISMA fila en la que `tutorial_build_room` siembra la hierba y el cofre: si el
        // profeta naciera una fila mas abajo, disparar de frente no abriria nada.
        tx = Config::ROOM_TILES_X / 2;
        ty = Config::ROOM_TILES_Y - 5;
    }
}
