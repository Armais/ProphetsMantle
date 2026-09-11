// =============================================================================
// MANTLE - Room System Implementation
// =============================================================================

#include "game/rooms.h"
#include "game/events.h"
#include "render/renderer.h"
#include <cstring>
#include <cmath>
#include <queue>

// Tope de `Room::tiles`, NO el tamano del mapa: eso es `Room::tiles_w` / `tiles_h`, que
// cambia por Room (zona = hasta 4x4 celdas, celda = 2x2 pantallas, interior = 1). Todo bucle
// sobre el mapa va contra los campos del Room; estos dos solo dimensionan arrays.
static constexpr int MAXW = Config::ZONE_TILES_X;
static constexpr int MAXH = Config::ZONE_TILES_Y;
static constexpr int TS = Config::TILE_SIZE;

// =============================================================================
// Helpers
// =============================================================================
static bool in_bounds(int x, int y, int w, int h) { return x >= 0 && x < w && y >= 0 && y < h; }

// Deterministic per-tile hash for visual variation
static u32 tile_hash(int x, int y, int salt) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u + static_cast<u32>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Is (tx,ty) part of the "apron" right inside an opening (kept free of obstacles)
static bool is_exit_apron(const Room& r, int tx, int ty) {
    if (r.door_north && r.is_gap_x(tx) && ty <= 2) return true;
    if (r.door_south && r.is_gap_x(tx) && ty >= r.tiles_h - 3) return true;
    if (r.door_west  && r.is_gap_y(ty) && tx <= 2) return true;
    if (r.door_east  && r.is_gap_y(ty) && tx >= r.tiles_w - 3) return true;
    return false;
}

// Free interior tile for decoration: not border, floor, not near centre, not in an apron
static bool is_placeable(const Room& room, int tx, int ty, int margin_from_center) {
    if (tx < 1 || tx >= room.tiles_w - 1 || ty < 1 || ty >= room.tiles_h - 1) return false;
    if (room.tiles[ty][tx] != TileType::Floor) return false;
    if (is_exit_apron(room, tx, ty)) return false;
    if (margin_from_center > 0) {
        if (std::abs(tx - (room.tiles_w / 2)) <= margin_from_center && std::abs(ty - (room.tiles_h / 2)) <= margin_from_center) return false;
    }
    return true;
}

static void scatter(Room& room, TileType t, int count, int margin) {
    for (int i = 0; i < count; i++) {
        for (int attempt = 0; attempt < 25; attempt++) {
            int tx = randi_range(1, room.tiles_w - 2);
            int ty = randi_range(1, room.tiles_h - 2);
            if (is_placeable(room, tx, ty, margin)) {
                room.tiles[ty][tx] = t;
                break;
            }
        }
    }
}

// Convert a tile into a walkable one while carving a trail (river -> bridge, obstacle -> path)
static void carve_tile(Room& room, int tx, int ty, bool mark_path) {
    if (!in_bounds(tx, ty, room.tiles_w, room.tiles_h)) return;
    TileType& t = room.tiles[ty][tx];
    switch (t) {
        case TileType::River:
        case TileType::Ford:
            t = TileType::Bridge; break;
        case TileType::Wall:
            if (tx == 0 || tx == room.tiles_w - 1 || ty == 0 || ty == room.tiles_h - 1) {
                // Border walls only get carved on openings (the trail ends inside a gap)
                if (mark_path) t = TileType::Path;
            } else {
                // Interior walls (buildings) yield to the trail
                t = mark_path ? TileType::Path : TileType::Floor;
            }
            break;
        case TileType::Tree:
        case TileType::Rock:
        case TileType::Pit:
        case TileType::Bush:
        case TileType::TallGrass:
        case TileType::Jar:
        case TileType::Crack:
        case TileType::Floor:
            if (mark_path) t = TileType::Path; else if (t != TileType::Floor) t = TileType::Floor;
            break;
        default:
            break;
    }
}

// Senda del centro a una abertura. NO es una recta: SERPENTEA. En una sala de una pantalla
// la L de antes pasaba desapercibida; en una de 2x2 dejaba una carretera perfectamente recta
// cortando el campo de lado a lado, que es lo contrario de un exterior.
//
// El serpenteo va en el tramo LARGO, el que de verdad recorre la sala. Ponerlo en el tramo
// de aproximacion no sirve de nada: con el centro ya alineado con el vano ese tramo tiene
// CERO pasos, y la senda salia dead-straight igual (asi fallo el primer intento). Los
// ultimos cuatro pasos van derechos, para que entre limpia por el vano.
static void carve_trail(Room& room, int x0, int y0, int x1, int y1) {
    int x = x0, y = y0;
    auto step_here = [&](int cx, int cy, bool horizontal) {
        carve_tile(room, cx, cy, true);
        // Clear the flanking tiles so the trail is comfortably 2-3 tiles wide
        if (horizontal) { carve_tile(room, cx, cy - 1, false); carve_tile(room, cx, cy + 1, false); }
        else            { carve_tile(room, cx - 1, cy, false); carve_tile(room, cx + 1, cy, false); }
    };
    const bool to_vertical_edge = (y1 == 0 || y1 == room.tiles_h - 1);
    if (to_vertical_edge) {
        const int dy = (y1 > y) ? 1 : -1;
        const int wander = std::max(0, std::abs(y1 - y) - 4);
        for (int i = 0; i < wander; i++) {
            step_here(x, y, false);
            y += dy;
            if (randi_range(0, 3) == 0) {
                int nx = x + (randi_range(0, 1) ? 1 : -1);
                if (nx > 1 && nx < room.tiles_w - 2) { step_here(nx, y, true); x = nx; }
            }
        }
        while (x != x1) { step_here(x, y, true);  x += (x1 > x) ? 1 : -1; }   // enfilar el vano
        while (y != y1) { step_here(x, y, false); y += dy; }                  // recta final
    } else {
        const int dx = (x1 > x) ? 1 : -1;
        const int wander = std::max(0, std::abs(x1 - x) - 4);
        for (int i = 0; i < wander; i++) {
            step_here(x, y, true);
            x += dx;
            if (randi_range(0, 3) == 0) {
                int ny = y + (randi_range(0, 1) ? 1 : -1);
                if (ny > 1 && ny < room.tiles_h - 2) { step_here(x, ny, false); y = ny; }
            }
        }
        while (y != y1) { step_here(x, y, false); y += (y1 > y) ? 1 : -1; }
        while (x != x1) { step_here(x, y, true);  x += dx; }
    }
    step_here(x, y, !to_vertical_edge);
}

// BFS over walkable tiles from (sx,sy). Fills `reach` (matriz al maximo).
static void flood_fill(const Room& room, int sx, int sy, bool reach[MAXH][MAXW]) {
    std::memset(reach, 0, sizeof(bool) * MAXW * MAXH);
    std::queue<std::pair<int, int>> q;
    if (!room.is_tile_walkable(sx, sy)) return;
    reach[sy][sx] = true;
    q.push({sx, sy});
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        const int dx[4] = {1, -1, 0, 0};
        const int dy[4] = {0, 0, 1, -1};
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!in_bounds(nx, ny, room.tiles_w, room.tiles_h) || reach[ny][nx]) continue;
            if (!room.is_tile_walkable(nx, ny)) continue;
            reach[ny][nx] = true;
            q.push({nx, ny});
        }
    }
}

// =============================================================================
// Biome decoration
// =============================================================================
static void decorate(Room& room, const BiomeStyle& biome) {
    // Cuantas PANTALLAS mide esta sala. Todos los contadores del bioma estan pensados para
    // una, asi que en una sala exterior (2x2) hay que multiplicarlos o el campo se queda
    // desnudo: tres arboles repartidos en cuatro pantallas no decoran nada.
    const int screens = std::max(1, (room.tiles_w * room.tiles_h) /
                                    (Config::SCREEN_TILES_X * Config::SCREEN_TILES_Y));
    // River: 2 tiles wide, serpentine, kept to one side of the centre band so the
    // north/south trails never run along it. East/west trails cross it once (bridge).
    if (biome.river) {
        bool left = randi_range(0, 1) == 0;
        // En PROPORCION al ancho: con los limites clavados para una sala de 20 tiles, en una
        // de 40 el rio se quedaba siempre en el cuarto izquierdo.
        int lo = left ? 2 : room.tiles_w * 3 / 5;
        int hi = left ? room.tiles_w / 4 : room.tiles_w * 3 / 4;
        int col = randi_range(lo, hi);
        // Long meanders: the course drifts one column every other row (instead of a
        // per-row jitter that read as a sawtooth) and the bed breathes between 2 and 3 wide
        int width = randi_range(2, 3);
        int drift = randi_range(0, 1) ? 1 : -1;
        for (int y = 1; y < room.tiles_h - 1; y++) {
            if ((y & 1) == 0) {
                if (randi_range(0, 3) == 0) drift = -drift;
                col += drift;
                if (col < lo) { col = lo; drift = 1; }
                if (col > hi) { col = hi; drift = -1; }
                if (randi_range(0, 2) == 0) width = (width == 2 ? 3 : 2);
            }
            for (int i = 0; i < width; i++) {
                int rx = col + i;
                if (rx > 0 && rx < room.tiles_w - 1) room.tiles[y][rx] = TileType::River;
            }
        }
        // 1-2 natural fords (the second one at least 2 rows away so both are real crossings)
        int fords = randi_range(1, 2);
        int first_fy = -10;
        for (int i = 0; i < fords; i++) {
            int fy = randi_range(2, room.tiles_h - 3);
            for (int tries = 0; tries < 8 && std::abs(fy - first_fy) < 2; tries++) fy = randi_range(2, room.tiles_h - 3);
            if (i == 0) first_fy = fy;
            for (int x = 1; x < room.tiles_w - 1; x++)
                if (room.tiles[fy][x] == TileType::River) room.tiles[fy][x] = TileType::Ford;
        }
    }

    // Shallow puddles: walkable, but wading through them costs speed (Config::WATER_SPEED_MULT).
    // Only in river biomes, and only on plain floor so trails and props stay untouched.
    // La forma NO puede ser un rectangulo: una caja de 2x2 con su ribera alrededor se lee
    // como una piscina, con su bordillo y todo. Es una elipse con el borde mordido, que es
    // justo lo que el autotileado de ribera (ver render_ground) necesita para dibujar una
    // curva en vez de un escalon.
    if (biome.river) {
        // MENOS charcas y mas grandes, no mas agua: una elipse de radio 2,8 son unos 15
        // tiles, y con los 8-16 charcos de antes la sala salia inundada. Con este radio cada
        // una ocupa 4-6 tiles, o sea lo mismo que sumaban las cajas de 2x2 de antes.
        // Radio MINIMO de 1,6: por debajo de eso la elipse cabe en una cruz de cinco tiles y
        // la charca sale con forma de pieza de tetris. Y una menos, para que el agua total no
        // suba al agrandarlas.
        // Pocas y MEDIANAS. Con cuatro de radio 2,5 se juntaban en un lago y el agua deja de
        // ser un charco para ser terreno lento: el profeta vadeaba media sala (medido con la
        // sonda de tacto — 94 tiros y cero bajas en el tramo de arqueros, con el bot metido en
        // el agua). Y por debajo de radio 1,3 la elipse cabe en una cruz de cinco tiles y sale
        // con forma de pieza de tetris. La ventana buena es estrecha.
        int puddles = std::max(2, screens - 1);
        for (int i = 0; i < puddles; i++) {
            int cx = randi_range(4, room.tiles_w - 6), cy = randi_range(4, room.tiles_h - 6);
            f32 rx = 1.3f + randf() * 0.6f, ry = 1.1f + randf() * 0.5f;
            int reach = static_cast<int>(rx > ry ? rx : ry) + 1;
            for (int y = cy - reach; y <= cy + reach; y++) {
                for (int x = cx - reach; x <= cx + reach; x++) {
                    if (x < 1 || x >= room.tiles_w - 1 || y < 1 || y >= room.tiles_h - 1) continue;
                    if (room.tiles[y][x] != TileType::Floor) continue;
                    f32 dx = static_cast<f32>(x - cx) / rx, dy = static_cast<f32>(y - cy) / ry;
                    // El mordisco va por tile y es DETERMINISTA: con un randf() por celda la
                    // charca cambiaria de forma en cada regeneracion de la misma semilla.
                    f32 bite = 0.34f * static_cast<f32>(tile_hash(x, y, i * 7 + 3) % 100) / 100.0f;
                    if (dx * dx + dy * dy + bite < 1.0f) room.tiles[y][x] = TileType::Water;
                }
            }
        }
    }

    // Buildings: small stone blocks (2x2 / 3x2) that break lines of sight in villages/cities
    // Tambien en proporcion al area: dos bloques sueltos en una sala de 2x2 no se leen
    // como una aldea, se leen como dos trozos de muro huerfanos en mitad del campo.
    // Y en MANZANAS, no sueltos. Un rectangulo de ladrillo solo en mitad del campo se lee
    // como un trozo de muro huerfano; lo que se lee como poblado es un grupo de casas con una
    // CALLE entre ellas. Cada manzana reparte hasta 6 solares en una rejilla con calle de 1-2
    // tiles, y deja alguno vacio para que no salga un damero.
    int want_buildings = biome.buildings * screens;
    for (int district = 0; district < 14 && want_buildings > 0; district++) {
        int cols = randi_range(2, 3), rows = randi_range(1, 2);
        int lot_w = randi_range(2, 3), lot_h = 2;
        int street = randi_range(1, 2);
        int dw = cols * lot_w + (cols - 1) * street;
        int dh = rows * lot_h + (rows - 1) * street;
        if (dw > room.tiles_w - 6 || dh > room.tiles_h - 6) continue;
        int dx = randi_range(2, room.tiles_w - 3 - dw);
        int dy = randi_range(2, room.tiles_h - 3 - dh);
        bool ok = true;
        for (int y = dy - 1; y <= dy + dh && ok; y++)
            for (int x = dx - 1; x <= dx + dw && ok; x++)
                if (!is_placeable(room, x, y, 2)) ok = false;
        if (!ok) continue;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols && want_buildings > 0; c++) {
                if (randi_range(0, 4) == 0) continue;      // un solar vacio rompe el damero
                int bx = dx + c * (lot_w + street), by = dy + r * (lot_h + street);
                for (int y = by; y < by + lot_h; y++)
                    for (int x = bx; x < bx + lot_w; x++)
                        room.tiles[y][x] = TileType::Wall;
                want_buildings--;
            }
        }
    }

    // HUECOS, y van AQUI ARRIBA a proposito: necesitan seis por cinco tiles rasos y con el
    // mapa ya sembrado de arboles, rocas, matorral, tinajas y grietas no cabe ni uno (probado:
    // cero huecos en un capitulo entero). `TileType::Pit` llevaba desde el primer commit con
    // pintado y colision y NADIE lo colocaba: el Carro de Fuego no tenia nada que cruzar. Van solo en bioma rocoso, y
    // cada uno con su recompensa AL OTRO LADO y a la vista — un precipicio sin nada detras
    // es un muro con otra textura, y un premio que no se ve hasta tener la llave no invita a
    // volver. Si una senda pasa por encima, `carve_tile` lo rellena: la ruta principal nunca
    // depende de tener el milagro.
    // Ni en la sala de INICIO ni en la arena del jefe. El hueco es una pieza de
    // exploracion: en la de inicio es donde se aprende a andar (y donde corre la sonda de
    // tacto, que se quedaba clavada detras de un precipicio), y en la arena solo seria una
    // trampa en una pelea que ya va justa de sitio.
    if (biome.rocky &&
        room.type != RoomType::Start && room.type != RoomType::Boss) {
        // Uno o dos POR SALA, y NO por pantalla: un hueco es una pieza de diseño, no
        // decorado. Escalandolo por area salian 24 tiles de precipicio en una sala y el bot
        // de la sonda de tacto se quedaba clavado detras (arqueros de 0,86 a 6,91 s).
        for (int g = 0; g < randi_range(1, 2); g++) {
            for (int attempt = 0; attempt < 30; attempt++) {
                int bx = randi_range(3, std::max(4, room.tiles_w - 7));
                int by = randi_range(3, std::max(4, room.tiles_h - 6));
                bool ok = true;
                for (int y = by - 1; y <= by + 3 && ok; y++)
                    for (int x = bx - 1; x <= bx + 4 && ok; x++)
                        if (!is_placeable(room, x, y, 3)) ok = false;
                if (!ok) continue;
                for (int y = by; y < by + 3; y++)
                    for (int x = bx; x < bx + 2; x++)
                        room.tiles[y][x] = TileType::Pit;
                room.tiles[by + 1][bx + 3] = randi_range(0, 1) ? TileType::Jar : TileType::Crack;
                break;
            }
        }
    }

    auto jitter = [&](int base) { return base <= 0 ? 0 : std::max(0, base * screens + randi_range(-1, 2)); };
    scatter(room, TileType::Tree,      jitter(biome.trees),  3);
    scatter(room, TileType::Rock,      jitter(biome.rocks),  2);
    scatter(room, TileType::Bush,      jitter(biome.bushes), 1);
    scatter(room, TileType::TallGrass, jitter(biome.grass),  1);

    // Secretos de bioma (GDD2.0 fase 4). El gate del diseño es que se VEAN antes de poder
    // abrirse, y por eso la tinaja y la grieta ESTORBAN el paso: el jugador tropieza con
    // ellas mucho antes de tener la llave. Y van donde tienen sentido — tinajas donde hay
    // casas, grietas donde hay roca —, con un par sueltos en el resto para que ningun
    // capitulo se quede sin nada que investigar.
    const bool poblado = biome.buildings > 0;
    const bool rocoso  = biome.rocky;   // terreno, no borde: los caps. 1/3/4 cierran en Natural y siguen siendo roca
    // Contados POR SALA. Escalados por area salian once grietas y dieciseis tinajas, y la
    // grieta PARA LOS TIROS: con ese numero, la mitad de los disparos del profeta se comian
    // el paisaje. Un secreto tiene que ser un hallazgo, no un seto.
    scatter(room, TileType::Jar,   poblado ? randi_range(4, 7) : randi_range(1, 3), 1);
    scatter(room, TileType::Crack, rocoso  ? randi_range(2, 4) : randi_range(0, 2), 2);
}

// =============================================================================
// Room::generate
// =============================================================================
// Un INTERIOR no se genera por biomas: se monta con una PLANTILLA. Es lo contrario del
// exterior a proposito — ahi el generador reparte terreno, y aqui lo que hay es una habitacion
// hecha, con sus muros, sus pilares y lo que sea que guarde dentro.
static void build_interior(Room& room, int chapter) {
    room.tiles_w = Config::SCREEN_TILES_X;
    room.tiles_h = Config::SCREEN_TILES_Y;
    room.border  = BorderStyle::Masonry;   // aqui el muro SI significa algo
    std::memset(room.tiles, static_cast<int>(TileType::Floor), sizeof(room.tiles));

    const int W = room.tiles_w, H = room.tiles_h;
    for (int x = 0; x < W; x++) { room.tiles[0][x] = TileType::Wall; room.tiles[H - 1][x] = TileType::Wall; }
    for (int y = 0; y < H; y++) { room.tiles[y][0] = TileType::Wall; room.tiles[y][W - 1] = TileType::Wall; }

    // La puerta, en el centro del muro sur: es por donde se entro y por donde se sale.
    room.door_tx = W / 2;
    room.door_ty = H - 1;
    room.has_doorway = true;
    room.tiles[room.door_ty][room.door_tx] = TileType::Doorway;

    // Pilares: dos filas simetricas. Son lo que hace que la habitacion tenga PLANTA y no sea
    // una caja vacia, y ademas cortan la linea de vision, que es para lo que sirve un muro.
    for (int i = 0; i < 2; i++) {
        int px = (i == 0) ? 4 : W - 5;
        room.tiles[3][px] = TileType::Wall;
        room.tiles[H - 4][px] = TileType::Wall;
    }

    // Antorchas flanqueando la puerta y el fondo
    room.tiles[H - 2][room.door_tx - 2] = TileType::Torch;
    room.tiles[H - 2][room.door_tx + 2] = TileType::Torch;

    // Lo que guarda: altar en los capitulos impares, cofre en los pares. Que un interior
    // SIEMPRE pague es lo que hace que valga la pena buscar la puerta; no lleva enemigos
    // porque el premio de explorar no puede ser otra pelea.
    room.tiles[2][W / 2] = (chapter % 2) ? TileType::AltarGod : TileType::Chest;

    // Tinajas contra las paredes: el secreto mas barato, y aqui es donde de verdad se
    // esperan (2 Reyes 4:1-7 pasa entero dentro de una casa).
    for (int i = 0; i < 5; i++) {
        int jx = 2 + (i * (W - 5)) / 4;
        int jy = (i % 2) ? 2 : H - 3;
        if (room.tiles[jy][jx] == TileType::Floor) room.tiles[jy][jx] = TileType::Jar;
    }
}

// La arena del jefe DENTRO de la celda B (zona continua, 2026-09-10): un recinto de una
// pantalla de suelo (20x11) con un cerco de piedras y un vano de 4 tiles abajo, pegado al
// borde de arriba de la celda. Se ve desde fuera antes de entrar y no hay sala aparte. El
// cerco es ROCA en todos los biomas: un muro de un tile de grueso no se dibuja (collect_props
// solo pinta la cara de abajo de un bloque), y un cerco de piedras se lee igual de bien en
// el Carmelo que en Jerico.
static void stamp_arena(Room& r) {
    const int x0 = (r.tiles_w - Config::ARENA_TILES_X) / 2 - 1;   // columna del cerco izquierdo (9)
    const int y0 = 0;                                              // fila del cerco de arriba (= borde de la celda)
    const int x1 = x0 + Config::ARENA_TILES_X + 1;                 // cerco derecho (30)
    const int y1 = y0 + Config::ARENA_TILES_Y + 1;                 // cerco de abajo (12)
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const bool ring = (x == x0 || x == x1 || y == y0 || y == y1);
            if (!ring) { r.tiles[y][x] = TileType::Floor; continue; }
            // El cerco de arriba coincide con el borde de la celda cuando esta va en la
            // primera fila de la rejilla (todos los jefes hoy): ese ya es solido y se deja.
            if (y == y0 && !r.door_north) continue;
            r.tiles[y][x] = TileType::Rock;
        }
    }
    // El vano, en el cerco de abajo y centrado: 4 tiles de senda
    const int gx0 = r.tiles_w / 2 - Config::ARENA_GAP / 2;
    for (int x = gx0; x < gx0 + Config::ARENA_GAP; x++) r.tiles[y1][x] = TileType::Path;
    r.has_arena    = true;
    r.arena_x0     = x0;
    r.arena_y0     = y0;
    r.arena_gap_x0 = gx0;
    r.arena_gap_x1 = gx0 + Config::ARENA_GAP - 1;
    r.arena_gap_y  = y1;
}

// Senda en L SIN sorteo, por fuera del recinto: por la fila del nudo hasta la columna del
// vano y de ahi al borde. Solo para la celda del jefe cuando el estampado de la arena corto
// una senda; una recta determinista no consume rand() y no puede pisar el cerco.
static void carve_straight(Room& r, int hx, int hy, int gx, int gy) {
    const int cx = (gx <= 0) ? 1 : (gx >= r.tiles_w - 1) ? r.tiles_w - 2 : gx;
    const int cy = (gy <= 0) ? 1 : (gy >= r.tiles_h - 1) ? r.tiles_h - 2 : gy;
    for (int x = hx; x != cx; x += (cx > hx) ? 1 : -1) {
        carve_tile(r, x, hy, true); carve_tile(r, x, hy - 1, false); carve_tile(r, x, hy + 1, false);
    }
    for (int y = hy; y != cy; y += (cy > hy) ? 1 : -1) {
        carve_tile(r, cx, y, true); carve_tile(r, cx - 1, y, false); carve_tile(r, cx + 1, y, false);
    }
    carve_tile(r, cx, cy, true);
    carve_tile(r, gx, gy, true);
}

void Room::generate(RoomType room_type, int chap, int difficulty, int event, bool with_doorway) {
    has_doorway = false;
    // Los reseteos van ANTES de la rama del interior: generate() corre sobre una Room
    // REUTILIZADA (current_room_data), y sin ellos el interior heredaba el estado de la sala
    // anfitriona — rezar en el altar de fuera dejaba `altar_used` puesto y el altar de dentro
    // (el premio del interior) nacia ya usado. Lo mismo con el cofre, el encuentro y, peor,
    // `enemy_spawns`: la lista de la anfitriona dentro de una sala de una pantalla.
    // Una celda, un interior o un escenario son UNA celda de estado: cells[0].
    type = room_type;
    chapter = chap;
    cells_x = cells_y = cell_count = 1;
    cells[0] = CellState{};
    cells[0].event_id = event;   // sin el reset, una celda Event generada tras resolver otro
                                 // encuentro nacia ya "resuelta" y su relato no se abria nunca
    has_arena = false;
    arena_sealed = false;
    boss_cell = -1;
    exit_open = false;
    enemy_spawns.clear();
    if (room_type == RoomType::Interior) {
        cells[0].cleared = true;      // no hay nada que limpiar en un interior
        build_interior(*this, chap);
        cell_w = tiles_w;
        cell_h = tiles_h;
        cells[0].hub_tx = tiles_w / 2;
        cells[0].hub_ty = tiles_h - 2;   // delante de la puerta
        return;
    }

    const BiomeStyle biome = get_biome_style(chapter);

    // Toda celda exterior mide 2x2 pantallas, la del jefe tambien: su arena es un RECINTO
    // dentro de la celda (stamp_arena), no una sala aparte de una pantalla.
    tiles_w = Config::ROOM_TILES_X;
    tiles_h = Config::ROOM_TILES_Y;
    cell_w = tiles_w;
    cell_h = tiles_h;
    border  = biome.border;      // ANTES de is_gap_x/is_gap_y: de el depende el ancho del vano

    // Fill with floor
    std::memset(tiles, static_cast<int>(TileType::Floor), sizeof(tiles));

    // Borde. `Cliff` y `Masonry` cierran con pared (`TileType::Wall`); `Natural` cierra con
    // el propio terreno — roca suelta y arboles secos, que bloquean igual pero se leen como
    // que el campo sigue mas alla. Dejarlo ABIERTO no es opcion: fuera de la rejilla
    // `is_tile_walkable` deja pasar, y sin sala vecina `try_room_transition` devuelve al
    // profeta de un tiron, que es un muro invisible y se siente peor que el muro.
    auto edge_tile = [&](int x, int y) {
        if (border != BorderStyle::Natural) return TileType::Wall;
        // Mitad y mitad: una fila de rocas todas iguales vuelve a leerse como una tapia,
        // solo que de mamposteria redonda. El arbol es el del bioma (palmera en el desierto).
        return (tile_hash(x, y, chapter + 913) % 2 == 0) ? TileType::Tree : TileType::Rock;
    };
    for (int x = 0; x < tiles_w; x++) {
        if (!(door_north && is_gap_x(x))) tiles[0][x] = edge_tile(x, 0);
        if (!(door_south && is_gap_x(x))) tiles[tiles_h - 1][x] = edge_tile(x, tiles_h - 1);
    }
    for (int y = 0; y < tiles_h; y++) {
        if (!(door_west && is_gap_y(y))) tiles[y][0] = edge_tile(0, y);
        if (!(door_east && is_gap_y(y))) tiles[y][tiles_w - 1] = edge_tile(tiles_w - 1, y);
    }

    // Segunda franja hacia dentro, IRREGULAR: unas veces otro bloqueo, otras matorral
    // pisable, la mayoria nada. Sin ella el borde natural sigue siendo una linea recta de
    // una sola fila, o sea un muro con otra textura. Se salta las aberturas para no
    // estrechar la salida, y solo pisa suelo raso (las sendas y el rio van despues).
    if (border == BorderStyle::Natural) {
        auto fringe = [&](int x, int y, bool on_gap) {
            if (on_gap || tiles[y][x] != TileType::Floor) return;
            u32 hs = tile_hash(x, y, chapter + 41);
            if      (hs % 5 == 0) tiles[y][x] = edge_tile(x, y);
            else if (hs % 5 == 1) tiles[y][x] = ((hs >> 3) & 1) ? TileType::Bush : TileType::TallGrass;
        };
        for (int x = 1; x < tiles_w - 1; x++) {
            fringe(x, 1,            door_north && is_gap_x(x));
            fringe(x, tiles_h - 2,  door_south && is_gap_x(x));
        }
        for (int y = 1; y < tiles_h - 1; y++) {
            fringe(1,           y,  door_west && is_gap_y(y));
            fringe(tiles_w - 2, y,  door_east && is_gap_y(y));
        }
    }

    // Biome decoration. La celda del jefe se decora como cualquier otra: la arena se
    // estampa encima y limpia lo suyo (y los huecos ya la evitan por tipo).
    if (room_type == RoomType::Normal || room_type == RoomType::Elite ||
        room_type == RoomType::Start  || room_type == RoomType::Treasure ||
        room_type == RoomType::Altar  || room_type == RoomType::Event ||
        room_type == RoomType::Boss) {
        decorate(*this, biome);
    }

    // Trails from a hub near the centre to every opening: guarantees the exits are
    // reachable and gives the player an obvious route. The hub jitters a little on
    // combat rooms so the cross of paths is not identical every room.
    // El nudo de las sendas se descentra en TODAS las celdas menos la del jefe. Con el
    // clavado en el centro, las cuatro sendas formaban una cruz perfecta — y en una sala de
    // 2x2 una cruz perfecta se ve entera de un vistazo. En la celda del jefe el nudo baja
    // a cuatro filas del borde de abajo: fuera del recinto, bajo su vano.
    int hx = (tiles_w / 2), hy = (tiles_h / 2);
    if (room_type != RoomType::Boss) {
        hx += randi_range(-tiles_w / 8, tiles_w / 8);
        hy += randi_range(-tiles_h / 8, tiles_h / 8);
    } else {
        hy = tiles_h - 4;
    }
    if (door_north) carve_trail(*this, hx, hy, (tiles_w / 2), 0);
    if (door_south) carve_trail(*this, hx, hy, (tiles_w / 2), tiles_h - 1);
    if (door_west)  carve_trail(*this, hx, hy, 0, (tiles_h / 2));
    if (door_east)  carve_trail(*this, hx, hy, tiles_w - 1, (tiles_h / 2));
    // Keep the true centre walkable too (start-of-run spawn / boss arena reference)
    carve_tile(*this, (tiles_w / 2), (tiles_h / 2), false);

    // La arena, DESPUES de las sendas (lo que cruzara el recinto queda limpio) y una senda
    // recta del vano al nudo; la reachability de mas abajo repara lo que el estampado corte.
    if (room_type == RoomType::Boss) {
        stamp_arena(*this);
        carve_trail(*this, hx, hy, hx, arena_gap_y + 1);
    }

    // Torches flanking the openings on "built" biomes (nunca en campo abierto: alli el
    // vano ocupa casi todo el lado y flanquearlo deja las antorchas en las esquinas)
    if (biome.torches && border != BorderStyle::Natural) {
        if (door_north) { tiles[1][gap_x0() - 1] = TileType::Torch; tiles[1][gap_x1() + 1] = TileType::Torch; }
        if (door_south) { tiles[tiles_h - 2][gap_x0() - 1] = TileType::Torch; tiles[tiles_h - 2][gap_x1() + 1] = TileType::Torch; }
        if (door_west)  { tiles[gap_y0() - 1][1] = TileType::Torch; tiles[gap_y1() + 1][1] = TileType::Torch; }
        if (door_east)  { tiles[gap_y0() - 1][tiles_w - 2] = TileType::Torch; tiles[gap_y1() + 1][tiles_w - 2] = TileType::Torch; }
    }

    // El prop de una sala especial YA NO va clavado en el centro. En una sala de 2x2 el
    // profeta aparece justo ahi, asi que el encuentro saltaba antes de haber visto a nadie y
    // el cofre se abria sin haberlo buscado. Ahora esta a unos tiles del nudo, sobre suelo
    // pisable y con al menos dos vecinos libres (que no caiga en un bolsillo cerrado por
    // arboles); la marca flotante del PNJ lo delata de lejos, que es medio motivo para ir.
    auto place_special = [&](TileType t) {
        int sx = (tiles_w / 2), sy = (tiles_h / 2);
        if (tiles_w > Config::SCREEN_TILES_X) {
            for (int attempt = 0; attempt < 40; attempt++) {
                int tx = (tiles_w / 2) + randi_range(-tiles_w / 4, tiles_w / 4);
                int ty = (tiles_h / 2) + randi_range(-tiles_h / 4, tiles_h / 4);
                if (!in_bounds(tx, ty, tiles_w, tiles_h)) continue;
                if (std::abs(tx - tiles_w / 2) < 5 && std::abs(ty - tiles_h / 2) < 3) continue;
                TileType cur = tiles[ty][tx];
                if (cur != TileType::Floor && cur != TileType::Path) continue;
                int open = 0;
                if (is_tile_walkable(tx - 1, ty)) open++;
                if (is_tile_walkable(tx + 1, ty)) open++;
                if (is_tile_walkable(tx, ty - 1)) open++;
                if (is_tile_walkable(tx, ty + 1)) open++;
                if (open < 2) continue;
                sx = tx; sy = ty; break;
            }
        }
        tiles[sy][sx] = t;
    };
    if (room_type == RoomType::Altar)    place_special(TileType::AltarGod);
    if (room_type == RoomType::Treasure) place_special(TileType::Chest);
    if (room_type == RoomType::Event)    place_special(TileType::Event);
    if (room_type == RoomType::Start) {
        // A Baal altar to smash for a first taste of faith
        tiles[(tiles_h / 2) - 2][(tiles_w / 2) + 4] = TileType::AltarBaal;
    }

    // La PUERTA al interior, si esta sala lo tiene. Va aqui, con el mapa ya terminado y las
    // sendas trazadas: puesta antes, `carve_trail` la araba como cualquier obstaculo. Y se
    // apoya en la senda (`Path`) a proposito — una puerta en mitad del descampado no se
    // encuentra, y el gate de la fase 4 vale igual aqui: lo que se abre tiene que verse.
    if (with_doorway) {
        auto try_door = [&](TileType on) {
            for (int attempt = 0; attempt < 200 && !has_doorway; attempt++) {
                int dx = randi_range(3, tiles_w - 4);
                int dy = randi_range(3, tiles_h - 4);
                if (tiles[dy][dx] != on) continue;
                if (std::abs(dx - tiles_w / 2) < 3 && std::abs(dy - tiles_h / 2) < 3) continue;
                tiles[dy][dx] = TileType::Doorway;
                door_tx = dx; door_ty = dy; has_doorway = true;
            }
        };
        try_door(TileType::Path);
        try_door(TileType::Floor);   // sin senda cerca, vale suelo raso
    }

    // Reachability from the centre (used to place enemies where they can actually fight).
    // En la celda del jefe, desde el NUDO: su centro cae dentro del recinto.
    static bool reach[MAXH][MAXW];   // 14 KB: fuera de la pila
    const bool boss = (room_type == RoomType::Boss);
    const int rx = boss ? hx : (tiles_w / 2), ry = boss ? hy : (tiles_h / 2);
    flood_fill(*this, rx, ry, reach);

    // Fallback: if any opening is unreachable, carve straight to it (should not happen). En
    // la celda del jefe la senda de reparacion es una L determinista por fuera del recinto.
    auto ensure = [&](bool door, int gx, int gy) {
        if (!door || reach[gy][gx]) return;
        if (boss) carve_straight(*this, rx, ry, gx, gy);
        else      carve_trail(*this, rx, ry, gx, gy);
        flood_fill(*this, rx, ry, reach);
    };
    ensure(door_north, (tiles_w / 2), 0);
    ensure(door_south, (tiles_w / 2), tiles_h - 1);
    ensure(door_west, 0, (tiles_h / 2));
    ensure(door_east, tiles_w - 1, (tiles_h / 2));

    // Enemies for combat rooms
    if (room_type == RoomType::Normal || room_type == RoomType::Elite) {
        // Density grows with the chapter: ch1 = 2-4, ch5 = 4-10 (rooms used to top out at 8)
        int lo = 2 + difficulty / 2;
        int hi = std::min(3 + difficulty * 3 / 2, Config::MAX_ENEMIES_PER_ROOM);
        // ...y con el AREA. Esa cuenta estaba pensada para una sala de UNA pantalla: tal
        // cual, en una de 2x2 los mismos cuatro soldados se pierden en el descampado.
        const int screens = std::max(1, (tiles_w * tiles_h) /
                                        (Config::SCREEN_TILES_X * Config::SCREEN_TILES_Y));
        // 3/4 del area, no el area entera: a densidad completa los diez o dieciseis
        // enemigos convergen todos a la vez y la sala deja de ser un campo para ser una
        // horda. A la mitad (lo primero que se probo) la sonda de tacto media 2,5 s de TTK
        // porque el bot se pasaba el rato ANDANDO: sobraba campo y faltaba gente.
        if (screens > 1) {
            lo = std::min(lo * screens * 3 / 4, Config::MAX_ENEMIES_PER_ROOM);
            hi = std::min(hi * screens * 3 / 4, Config::MAX_ENEMIES_PER_ROOM);
        }
        int enemy_count = randi_range(lo, std::max(lo, hi));
        if (room_type == RoomType::Elite) enemy_count += 2;

        for (int i = 0; i < enemy_count; i++) {
            EnemySpawn spawn;
            spawn.position = tile_to_world((tiles_w / 2) + (i % 2 ? 4 : -4), (tiles_h / 2) + (i % 3) - 1);  // fallback

            for (int attempt = 0; attempt < 40; attempt++) {
                int tx = randi_range(1, tiles_w - 2);
                int ty = randi_range(1, tiles_h - 2);
                if (!reach[ty][tx]) continue;
                if (std::abs(tx - (tiles_w / 2)) <= 3 && std::abs(ty - (tiles_h / 2)) <= 2) continue;   // not on top of the player
                if (is_exit_apron(*this, tx, ty)) continue;                          // not camping the doors
                TileType t = tiles[ty][tx];
                if (t == TileType::Bridge || t == TileType::Ford) continue;
                spawn.position = tile_to_world(tx, ty);
                break;
            }

            // Enemy type based on chapter (Act I: 0-3, Act II: 4-7), weighted to melee
            int roll = randi_range(0, 9);
            int idx;
            if (roll < 4)      idx = 0;   // soldier
            else if (roll < 6) idx = 1;   // archer / general
            else if (roll < 8) idx = 2;   // prophet / leper
            else               idx = 3;   // lion / water
            if (chapter <= 2 && idx == 3) idx = 0;      // no lions before ch3
            if (chapter > 5) idx += 4;
            spawn.type = static_cast<EnemyType>(idx);
            enemy_spawns.push_back(spawn);
        }
    }

    cells[0].hub_tx = hx;
    cells[0].hub_ty = hy;
}

// =============================================================================
// Secretos: abrirlos y que esconden
// =============================================================================
bool Room::tile_hides_loot(int tx, int ty) const {
    // Uno de cada cuatro. Con menos no compensa buscar; con mas deja de ser un hallazgo y
    // pasa a ser una cosecha. Va por `tile_hash` y no por sorteo: la misma semilla tiene que
    // dar el mismo mapa de hallazgos, y asi no hay NADA que guardar.
    return (tile_hash(tx, ty, chapter + 7717) % 100) < 26;
}

int Room::open_secrets(Vec2 center, f32 radius, u32 kinds, std::vector<Vec2>& out_loot) {
    // floor(), no division entera: un centro con coordenada negativa truncaria hacia 0 y la
    // caja de busqueda se desplazaria un tile (misma convencion que is_walkable_world).
    const int cx = static_cast<int>(std::floor(center.x / TS));
    const int cy = static_cast<int>(std::floor(center.y / TS));
    const int reach = static_cast<int>(radius) / TS + 1;
    int opened = 0;
    for (int y = cy - reach; y <= cy + reach; y++) {
        for (int x = cx - reach; x <= cx + reach; x++) {
            if (x < 1 || x >= tiles_w - 1 || y < 1 || y >= tiles_h - 1) continue;
            Vec2 c = tile_to_world(x, y);
            if (c.distance_to(center) > radius) continue;
            u32 kind = 0;
            switch (tiles[y][x]) {
                case TileType::Bush:
                case TileType::TallGrass: kind = SECRET_BUSH;  break;
                case TileType::Jar:       kind = SECRET_JAR;   break;
                case TileType::Crack:     kind = SECRET_CRACK; break;
                case TileType::Water:     kind = SECRET_WATER; break;
                default: break;
            }
            if (kind == 0 || (kinds & kind) == 0) continue;
            tiles[y][x] = TileType::Floor;
            opened++;
            if (tile_hides_loot(x, y)) out_loot.push_back(c);
        }
    }
    return opened;
}

// =============================================================================
// Room::place_exit — chapter exit arch at the top centre (after the boss)
// =============================================================================
void Room::place_exit() {
    exit_open = true;
    int ex, ey;
    if (has_arena) {   // en el suelo de la arena, segunda fila
        ex = arena_x0 + 1 + Config::ARENA_TILES_X / 2;
        ey = arena_y0 + 2;
    } else {
        ex = (tiles_w / 2);
        ey = 2;
    }
    exit_tx = ex; exit_ty = ey;
    for (int x = ex - 1; x <= ex + 1; x++)
        for (int y = ey - 1; y <= ey + 1; y++)
            if (in_bounds(x, y, tiles_w, tiles_h) && x > 0 && x < tiles_w - 1 && y > 0 && y < tiles_h - 1 &&
                !in_arena_ring(x, y))
                tiles[y][x] = TileType::Path;
    tiles[ey][ex] = TileType::Exit;
}

// =============================================================================
// Celdas y arena (zona continua)
// =============================================================================
int Room::cell_of_tile(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= tiles_w || ty >= tiles_h) return -1;
    if (cell_count <= 1) return 0;
    const int c = (ty / cell_h) * cells_x + (tx / cell_w);
    return (c >= 0 && c < cell_count) ? c : -1;
}

int Room::cell_of_world(Vec2 p) const {
    return cell_of_tile(static_cast<int>(std::floor(p.x / TS)), static_cast<int>(std::floor(p.y / TS)));
}

Rect Room::cell_rect_world(int cell) const {
    if (cell_count <= 1 || cell < 0 || cell >= cell_count) return {0, 0, world_w(), world_h()};
    const int gx = cell % cells_x, gy = cell / cells_x;
    return {static_cast<f32>(gx * cell_w * TS), static_cast<f32>(gy * cell_h * TS),
            static_cast<f32>(cell_w * TS), static_cast<f32>(cell_h * TS)};
}

Rect Room::arena_rect_world() const {
    return {static_cast<f32>((arena_x0 + 1) * TS), static_cast<f32>((arena_y0 + 1) * TS),
            static_cast<f32>(Config::ARENA_TILES_X * TS), static_cast<f32>(Config::ARENA_TILES_Y * TS)};
}

Rect Room::arena_bounds_world() const {
    return {static_cast<f32>(arena_x0 * TS), static_cast<f32>(arena_y0 * TS),
            static_cast<f32>((Config::ARENA_TILES_X + 2) * TS), static_cast<f32>((Config::ARENA_TILES_Y + 2) * TS)};
}

void Room::arena_anchor(int& tx, int& ty) const {
    if (has_arena) {
        tx = arena_x0 + 1 + Config::ARENA_TILES_X / 2;
        ty = arena_y0 + 1 + 2;
    } else {
        tx = tiles_w / 2;
        ty = 3;
    }
}

bool Room::is_arena_gap(int tx, int ty) const {
    return has_arena && ty == arena_gap_y && tx >= arena_gap_x0 && tx <= arena_gap_x1;
}

bool Room::in_arena_ring(int tx, int ty) const {
    if (!has_arena) return false;
    const int x1 = arena_x0 + Config::ARENA_TILES_X + 1, y1 = arena_y0 + Config::ARENA_TILES_Y + 1;
    if (tx < arena_x0 || tx > x1 || ty < arena_y0 || ty > y1) return false;
    return tx == arena_x0 || tx == x1 || ty == arena_y0 || ty == y1;
}

void Room::blit_cell(const Room& c, int gx, int gy, int index) {
    if (index < 0 || index >= Config::ZONE_CELLS_MAX) return;
    const int ox = gx * cell_w, oy = gy * cell_h;
    for (int y = 0; y < c.tiles_h && oy + y < tiles_h; y++)
        for (int x = 0; x < c.tiles_w && ox + x < tiles_w; x++)
            tiles[oy + y][ox + x] = c.tiles[y][x];
    CellState& cs = cells[index];
    cs = CellState{};
    cs.event_id = c.cells[0].event_id;
    cs.hub_tx = ox + c.cells[0].hub_tx;
    cs.hub_ty = oy + c.cells[0].hub_ty;
    cs.spawns = c.enemy_spawns;
    const Vec2 off = {static_cast<f32>(ox * TS), static_cast<f32>(oy * TS)};
    for (auto& sp : cs.spawns) sp.position = sp.position + off;
    if (c.has_doorway) {
        has_doorway = true;
        door_tx = ox + c.door_tx;
        door_ty = oy + c.door_ty;
    }
    if (c.has_arena) {
        has_arena    = true;
        boss_cell    = index;
        arena_x0     = ox + c.arena_x0;
        arena_y0     = oy + c.arena_y0;
        arena_gap_x0 = ox + c.arena_gap_x0;
        arena_gap_x1 = ox + c.arena_gap_x1;
        arena_gap_y  = oy + c.arena_gap_y;
    }
}

// Las COSTURAS entre celdas: las dos filas/columnas de anillo de cada costura interior
// pasan a suelo si eran solidas. La franja irregular (fila/columna 1 de cada celda) se
// queda: una linea suelta de rocas y matorral con huecos, como los setos de ALTTP. Las
// sendas ya empalman solas (las dos celdas tallan hasta su borde por el mismo vano). El
// cerco de la arena no se toca.
void Room::open_seams() {
    for (int gx = 1; gx < cells_x; gx++) {
        const int xs[2] = {gx * cell_w - 1, gx * cell_w};
        for (int y = 1; y < tiles_h - 1; y++)
            for (int x : xs)
                if (!in_arena_ring(x, y) && !is_tile_walkable(x, y)) tiles[y][x] = TileType::Floor;
    }
    for (int gy = 1; gy < cells_y; gy++) {
        const int ys[2] = {gy * cell_h - 1, gy * cell_h};
        for (int x = 1; x < tiles_w - 1; x++)
            for (int y : ys)
                if (!in_arena_ring(x, y) && !is_tile_walkable(x, y)) tiles[y][x] = TileType::Floor;
    }
}

// =============================================================================
// Room::find_free_tile_near — nearest walkable tile (spiral search)
// =============================================================================
bool Room::find_free_tile_near(int tx, int ty, int& out_x, int& out_y) const {
    for (int r = 0; r < 6; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                int x = tx + dx, y = ty + dy;
                if (x < 1 || x >= tiles_w - 1 || y < 1 || y >= tiles_h - 1) continue;
                if (is_tile_walkable(x, y)) { out_x = x; out_y = y; return true; }
            }
        }
    }
    return false;
}

// =============================================================================
// Room::is_tile_walkable
// =============================================================================
bool Room::is_tile_walkable(int tx, int ty) const {
    // Fuera del mapa no hay nada que pisar (la zona cierra con el borde del bioma; las
    // transiciones por borde ya no existen)
    if (tx < 0 || tx >= tiles_w || ty < 0 || ty >= tiles_h) return false;
    // El vano de la arena, sellado mientras el jefe vive: un flag, no tiles
    if (arena_sealed && is_arena_gap(tx, ty)) return false;

    switch (tiles[ty][tx]) {
        case TileType::Floor:
        case TileType::Water:
        case TileType::DoorNorth:
        case TileType::DoorSouth:
        case TileType::DoorEast:
        case TileType::DoorWest:
        case TileType::Bush:
        case TileType::TallGrass:
        case TileType::Chest:
        case TileType::AltarBaal:
        case TileType::AltarGod:
        case TileType::Path:
        case TileType::Ford:
        case TileType::Bridge:
        case TileType::Exit:
        case TileType::Torch:
        case TileType::Event:
        case TileType::Doorway:
            return true;
        default:
            // Wall, Pit, Tree, Rock, River, Jar, Crack = not walkable. La tinaja y la
            // grieta ESTORBAN a proposito: un secreto que no se interpone no se ve.
            return false;
    }
}

bool Room::is_tile_solid_for_projectiles(int tx, int ty) const {
    if (tx < 0 || tx >= tiles_w || ty < 0 || ty >= tiles_h) return false;
    if (arena_sealed && is_arena_gap(tx, ty)) return true;   // ni se pisa ni se atraviesa a tiros
    switch (tiles[ty][tx]) {
        case TileType::Wall:
        case TileType::Tree:
        case TileType::Rock:
            return true;
        // La grieta NO para los tiros aunque no se pise: son dos masas BAJAS con una
        // hendidura, y el disparo pasa por encima. Cuando si los paraba, la sonda de tacto
        // media 94 tiros y CERO bajas en el tramo de arqueros — un secreto tiene que ser un
        // hallazgo, no cobertura para el enemigo.
        default:
            return false;
    }
}

// =============================================================================
// Room::tile_to_world
// =============================================================================
Vec2 Room::tile_to_world(int tx, int ty) const {
    return {
        static_cast<f32>(tx * TS + TS / 2),
        static_cast<f32>(ty * TS + TS / 2)
    };
}

// =============================================================================
// Room::is_walkable_world / nearest_walkable_world
// The ONLY world->tile walkability test: every system (movement, body push,
// spawns) must use it with the entity's feet point so they agree on what is solid.
// =============================================================================
bool Room::is_walkable_world(Vec2 p) const {
    int tx = static_cast<int>(std::floor(p.x / TS));
    int ty = static_cast<int>(std::floor(p.y / TS));
    return is_tile_walkable(tx, ty);
}

bool Room::is_pit_world(Vec2 p) const {
    int tx = static_cast<int>(std::floor(p.x / TS));
    int ty = static_cast<int>(std::floor(p.y / TS));
    if (tx < 0 || tx >= tiles_w || ty < 0 || ty >= tiles_h) return false;
    return tiles[ty][tx] == TileType::Pit;
}

bool Room::is_slow_world(Vec2 p) const {
    int tx = static_cast<int>(std::floor(p.x / TS));
    int ty = static_cast<int>(std::floor(p.y / TS));
    if (!in_bounds(tx, ty, tiles_w, tiles_h)) return false;
    return tiles[ty][tx] == TileType::Water;
}

bool Room::nearest_walkable_world(Vec2 feet, Vec2& out_feet) const {
    int tx = static_cast<int>(std::floor(feet.x / TS));
    int ty = static_cast<int>(std::floor(feet.y / TS));
    int fx, fy;
    if (!find_free_tile_near(tx, ty, fx, fy)) return false;
    out_feet = tile_to_world(fx, fy);
    return true;
}

// =============================================================================
// Rendering
// =============================================================================
static bool is_water_like(TileType t) {
    return t == TileType::River || t == TileType::Water || t == TileType::Ford || t == TileType::Bridge;
}

// Placeholder colours when the tileset is missing
static Color fallback_color(TileType t) {
    switch (t) {
        case TileType::Wall:      return {100, 80, 60, 255};
        case TileType::Pit:       return {20, 15, 10, 255};
        case TileType::Water:
        case TileType::River:     return {40, 100, 160, 255};
        case TileType::Ford:      return {120, 140, 160, 255};
        case TileType::Bridge:    return {150, 100, 50, 255};
        case TileType::Path:      return {110, 85, 55, 255};
        case TileType::AltarBaal: return {142, 68, 173, 255};
        case TileType::Jar:       return {186, 132, 74, 255};
        case TileType::Doorway:   return {70, 52, 38, 255};
        case TileType::Crack:     return {84, 72, 62, 255};
        case TileType::AltarGod:  return {241, 196, 15, 255};
        case TileType::Chest:     return {200, 160, 40, 255};
        case TileType::Tree:      return {34, 100, 34, 255};
        case TileType::Bush:      return {80, 160, 60, 255};
        case TileType::Rock:      return {120, 110, 100, 255};
        case TileType::TallGrass: return {90, 150, 70, 150};
        case TileType::Exit:      return {255, 230, 120, 255};
        case TileType::Torch:     return {240, 140, 40, 255};
        case TileType::Event:     return {255, 226, 150, 255};
        default:                  return {0, 0, 0, 0};
    }
}

// Rango de tiles que toca pintar para una vista (mundo): la vista mas un margen de un tile
// (dos por arriba: la cara alta del acantilado y los props altos cuelgan hacia arriba).
static void view_range(const Room& r, Rect view, int& x_lo, int& x_hi, int& y_lo, int& y_hi) {
    x_lo = std::max(0, static_cast<int>(std::floor(view.x / TS)) - 1);
    y_lo = std::max(0, static_cast<int>(std::floor(view.y / TS)) - 2);
    x_hi = std::min(r.tiles_w - 1, static_cast<int>(std::floor((view.x + view.w) / TS)) + 1);
    y_hi = std::min(r.tiles_h - 1, static_cast<int>(std::floor((view.y + view.h) / TS)) + 1);
}

void Room::render_ground(Renderer& renderer, u32 tileset, f32 time, Rect view) const {
    BiomeStyle biome = get_biome_style(chapter);
    if (type == RoomType::Interior) apply_interior_style(biome);
    constexpr f32 ts = static_cast<f32>(TS);
    const bool textured = (tileset != TEXTURE_NONE);
    const int water_frame = static_cast<int>(time * 2.0f);
    int x_lo, x_hi, y_lo, y_hi;
    view_range(*this, view, x_lo, x_hi, y_lo, y_hi);

    for (int y = y_lo; y <= y_hi; y++) {
        for (int x = x_lo; x <= x_hi; x++) {
            TileType t = tiles[y][x];
            f32 px = static_cast<f32>(x) * ts;
            f32 py = static_cast<f32>(y) * ts;
            Rect dst = {px, py, ts, ts};

            if (!textured) {
                Color c = fallback_color(t);
                if (c.a > 0 && t != TileType::Tree && t != TileType::Rock && t != TileType::Bush &&
                    t != TileType::Chest && t != TileType::AltarGod && t != TileType::AltarBaal &&
                    t != TileType::Torch && t != TileType::Exit && t != TileType::Event &&
                    t != TileType::Jar && t != TileType::Crack)
                    renderer.draw_rect(dst, c);
                continue;
            }

            u32 h = tile_hash(x, y, chapter);

            // Base floor (biome family: sand / meadow / paving, 4 variants) under everything
            // that is not water/border-wall/pit
            bool border_wall = (t == TileType::Wall) && (x == 0 || x == tiles_w - 1 || y == 0 || y == tiles_h - 1);
            if (!border_wall && !is_water_like(t) && t != TileType::Pit) {
                TileSprite fs = static_cast<TileSprite>(static_cast<u32>(biome.floor_base) + (h % 4));
                renderer.draw_sprite_rect(tileset, get_tile_src(fs), dst, biome.floor_tint);

                // Ground decals (tufts, pebbles, flowers, cracks...) on plain floor tiles only
                if (t == TileType::Floor) {
                    u32 h2 = tile_hash(x, y, chapter + 31);
                    if (static_cast<int>(h2 % 100) < biome.detail_pct) {
                        TileSprite ds = biome.details[(h2 >> 8) % 3];
                        bool flip = ((h2 >> 12) & 1) != 0;
                        // A tile completo y centrados, los decorados dibujan LA REJILLA: en una
                        // sala de 2x2 pantallas se ve un damero de manchas identicas. Se
                        // encogen y se descentran dentro de su propia celda — nunca fuera de
                        // ella, porque el suelo del tile de la derecha y el de abajo se pintan
                        // despues y los recortarian.
                        f32 k = 0.50f + static_cast<f32>((h2 >> 16) % 5) * 0.10f;
                        f32 dsz = ts * k;
                        f32 ox = static_cast<f32>((h2 >> 20) % 64) / 63.0f * (ts - dsz);
                        f32 oy = static_cast<f32>((h2 >> 26) % 64) / 63.0f * (ts - dsz);
                        f32 rot = static_cast<f32>((h2 >> 3) % 4) * 90.0f;
                        renderer.draw_sprite_rect(tileset, get_tile_src(ds),
                                                  {px + ox, py + oy, dsz, dsz},
                                                  biome.floor_tint, flip, rot);
                    }
                }

                // Orilla de la senda. El tile de camino es opaco hasta su borde, asi que
                // un trazado por celdas enteras se lee como una cadena de cuadrados sobre
                // la hierba. Aqui la tierra se DERRAMA sobre la celda vecina: una lengua de
                // fondo variable por lado, con el sprite del propio camino y alfa parcial.
                //
                // Se hace al DIBUJAR y no al generar: un fleco de tiles de verdad ensancha
                // la senda (probado, y la deja el doble de ancha) en vez de romperle el
                // contorno, y ademas cambiaria por donde se puede andar.
                if (t == TileType::Floor || t == TileType::TallGrass) {
                    const TileSprite ps = ((h >> 4) & 1) ? TileSprite::PathA : TileSprite::PathB;
                    auto spill = [&](int nx, int ny, int side) {
                        if (!in_bounds(nx, ny, tiles_w, tiles_h) || tiles[ny][nx] != TileType::Path) return;
                        u32 hs = tile_hash(x, y, chapter + 149 + side * 17);
                        f32 d = ts * (0.20f + static_cast<f32>(hs % 40) * 0.010f);   // fondo 0.20..0.59
                        Color c = biome.floor_tint;
                        c.a = static_cast<u8>(110 + (hs >> 8) % 90);
                        Rect r = (side == 0) ? Rect{px, py, ts, d}
                               : (side == 1) ? Rect{px + ts - d, py, d, ts}
                               : (side == 2) ? Rect{px, py + ts - d, ts, d}
                                             : Rect{px, py, d, ts};
                        renderer.draw_sprite_rect(tileset, get_tile_src(ps), r, c);
                    };
                    spill(x, y - 1, 0);
                    spill(x + 1, y, 1);
                    spill(x, y + 1, 2);
                    spill(x - 1, y, 3);
                }
            }

            switch (t) {
                case TileType::Path: {
                    TileSprite ps = ((h >> 4) & 1) ? TileSprite::PathA : TileSprite::PathB;
                    renderer.draw_sprite_rect(tileset, get_tile_src(ps), dst, biome.floor_tint);
                    break;
                }
                case TileType::River:
                case TileType::Water: {
                    TileSprite ws = ((water_frame + static_cast<int>(h & 1)) & 1) ? TileSprite::WaterA : TileSprite::WaterB;
                    renderer.draw_sprite_rect(tileset, get_tile_src(ws), dst, biome.water_tint);
                    break;
                }
                case TileType::Ford:
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Ford), dst, biome.water_tint);
                    break;
                case TileType::Bridge:
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Bridge), dst, Color::white());
                    break;
                case TileType::Pit: {
                    // El hueco es UNA mancha y no una fila de agujeros. A tile completo, una
                    // banda de tres por dos salia como seis ovalos identicos en rejilla — el
                    // mismo defecto que tenian los decorados del suelo. El fondo se pinta
                    // plano y la celda con relieve va SOLO donde el hueco termina.
                    renderer.draw_rect(dst, {17, 12, 10, 255});
                    auto pit_at = [&](int nx, int ny) {
                        return in_bounds(nx, ny, tiles_w, tiles_h) && tiles[ny][nx] == TileType::Pit;
                    };
                    // Labio de tierra SOLO en los lados que dan a suelo firme: es lo que hace
                    // que el hueco se lea hundido. La celda `TileSprite::Pit` no vale aqui —
                    // dibuja un ovalo por casilla, y en una banda de dos de ancho NO HAY ni un
                    // tile interior, asi que salian seis ovalos en rejilla.
                    const Color lip = {94, 70, 50, 235};
                    constexpr f32 L = 2.0f;
                    if (!pit_at(x, y - 1)) renderer.draw_rect({px, py, ts, L}, lip);
                    if (!pit_at(x, y + 1)) renderer.draw_rect({px, py + ts - L, ts, L}, lip);
                    if (!pit_at(x - 1, y)) renderer.draw_rect({px, py, L, ts}, lip);
                    if (!pit_at(x + 1, y)) renderer.draw_rect({px + ts - L, py, L, ts}, lip);
                    break;
                }
                case TileType::TallGrass:
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Grass), dst, biome.prop_tint);
                    break;
                case TileType::Wall: {
                    bool border = (x == 0 || x == tiles_w - 1 || y == 0 || y == tiles_h - 1);
                    // Un canon NO es una tapia. `Cliff*` son sillares (hiladas rectas) y eso es
                    // lo correcto en ciudad y palacio; el borde de acantilado usa la cara de
                    // ROCA, con las juntas desalineadas. Los edificios del interior siguen
                    // siendo silleria — los dibuja collect_props().
                    const bool rocky = (biome.border != BorderStyle::Masonry);
                    if (y == 0) {
                        // Top border: tall face rising out of view
                        renderer.draw_sprite_rect(tileset,
                                                  get_tile_src(rocky ? TileSprite::CliffRockTall : TileSprite::CliffTall),
                                                  {px, py - ts, ts, ts * 2.0f}, biome.wall_tint);
                    } else if (border) {
                        renderer.draw_sprite_rect(tileset,
                                                  get_tile_src(rocky ? TileSprite::CliffRock : TileSprite::Cliff),
                                                  dst, biome.wall_tint);
                    }
                    break;
                }
                default:
                    break;
            }

            // Shore autotiling: wet-sand banks with foam on every water edge that touches
            // land. Three cells authored for "land to the north" are rotated 90/180/270 to
            // cover the 12 blob-tiling cases, so the river reads as a continuous curve
            // instead of a staircase of squares.
            if (is_water_like(t) && t != TileType::Bridge) {
                auto land = [&](int nx, int ny) {
                    if (!in_bounds(nx, ny, tiles_w, tiles_h)) return false;   // outside the room: no bank on the border
                    return !is_water_like(tiles[ny][nx]);
                };
                // Sides and diagonals, clockwise from north
                const bool side[4] = { land(x, y - 1), land(x + 1, y), land(x, y + 1), land(x - 1, y) };
                const bool diag[4] = { land(x + 1, y - 1), land(x + 1, y + 1),
                                       land(x - 1, y + 1), land(x - 1, y - 1) };
                const Color st = biome.floor_tint;
                for (int s = 0; s < 4; s++) {
                    if (!side[s]) continue;
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::ShoreEdge), dst, st,
                                              false, static_cast<f32>(s) * 90.0f);
                }
                for (int s = 0; s < 4; s++) {   // outer corner: two adjacent sides are land
                    if (side[s] && side[(s + 1) & 3])
                        renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::ShoreOut), dst, st,
                                                  false, static_cast<f32>(s) * 90.0f);
                }
                for (int d = 0; d < 4; d++) {   // inner corner: only the diagonal is land
                    if (diag[d] && !side[d] && !side[(d + 1) & 3])
                        renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::ShoreIn), dst, st,
                                                  false, static_cast<f32>(d) * 90.0f);
                }
            }
        }
    }

    // Soft shadow under the top cliff face so it reads as raised. Five thin bands with a
    // quadratic falloff instead of two hard steps (4 units total, ~16 px at RENDER_SCALE 4).
    if (textured && y_lo <= 1) {
        constexpr int BANDS = 5;
        constexpr f32 BAND_H = 0.8f;
        for (int x = x_lo; x <= x_hi; x++) {
            if (tiles[0][x] != TileType::Wall || tiles[1][x] == TileType::Wall) continue;
            for (int b = 0; b < BANDS; b++) {
                f32 k = 1.0f - static_cast<f32>(b) / BANDS;
                renderer.draw_rect({static_cast<f32>(x) * ts, ts + b * BAND_H, ts, BAND_H},
                                   {0, 0, 0, static_cast<u8>(72.0f * k * k)});
            }
        }
    }
}

void Room::collect_props(std::vector<DrawCmd>& out, u32 tileset, u32 shadow_tex,
                         u32 npc_tex, f32 time, Rect view) const {
    BiomeStyle biome = get_biome_style(chapter);
    if (type == RoomType::Interior) apply_interior_style(biome);
    constexpr f32 ts = static_cast<f32>(TS);
    const bool textured = (tileset != TEXTURE_NONE);
    int x_lo, x_hi, y_lo, y_hi;
    view_range(*this, view, x_lo, x_hi, y_lo, y_hi);

    for (int y = y_lo; y <= y_hi; y++) {
        for (int x = x_lo; x <= x_hi; x++) {
            TileType t = tiles[y][x];
            f32 px = static_cast<f32>(x) * ts;
            f32 py = static_cast<f32>(y) * ts;
            u32 h = tile_hash(x, y, chapter + 77);

            // Encuentro: si el locutor es una PERSONA, aqui hay alguien de pie, y se dibuja
            // como cualquier personaje — misma celda de 32 unidades, misma sombra, mismo
            // orden por Y. Hasta esta pasada esto era la celda de ANTORCHA del tileset.
            // Si el locutor es una ESCENA (los cuervos, el altar del Carmelo, la boca de la
            // cueva) no hay nadie a quien dibujar: la hoguera de siempre marca el sitio, que
            // es lo honesto — ponerle una cara a un pasaje seria mentir sobre lo que se mira.
            if (t == TileType::Event && npc_tex != TEXTURE_NONE) {
                const int ci = cell_of_tile(x, y);
                const int ev = (ci >= 0) ? cells[ci].event_id : -1;
                const int who = (ev >= 0) ? get_event_def(ev).npc : Npc::NONE;
                if (who != Npc::NONE) {
                    const f32 feet = py + 13.0f;
                    if (shadow_tex != TEXTURE_NONE) {
                        DrawCmd sh;
                        sh.sort_y = -999.0f;
                        sh.texture_id = shadow_tex;
                        sh.src = {0, 0, 64, 32};
                        sh.dst = {px + ts * 0.5f - 8.0f, feet - 3.0f, 16.0f, 6.0f};
                        sh.tint = {0, 0, 0, 110};
                        out.push_back(sh);
                    }
                    DrawCmd n;
                    n.sort_y = feet;
                    n.texture_id = npc_tex;
                    n.src = Npc::src(who);
                    // Los pies de la hoja caen en la fila 112 de 128, o sea a 28 de las 32
                    // unidades: por eso el alto se cuelga de `feet` hacia arriba y no del
                    // borde del tile, y el PNJ pisa el suelo igual que el profeta.
                    n.dst = {px + ts * 0.5f - 16.0f, feet - 28.0f, 32.0f, 32.0f};
                    out.push_back(n);
                    // Marca flotante: se ve desde lejos que ahi espera alguien, que es medio
                    // motivo para desviarse del camino.
                    if (ci >= 0 && !cells[ci].event_used) {
                        DrawCmd m;
                        m.sort_y = feet + 0.1f;
                        m.texture_id = tileset;
                        m.src = get_tile_src(TileSprite::Star);
                        f32 bob = std::sin(time * 3.0f + static_cast<f32>(x)) * 1.5f;
                        m.dst = {px + ts * 0.5f - 4.0f, feet - 34.0f + bob, 8.0f, 8.0f};
                        m.tint = {255, 226, 150, 255};
                        out.push_back(m);
                    }
                    continue;
                }
            }

            TileSprite sprite = TileSprite::COUNT;
            Color tint = Color::white();
            bool tall = false;
            switch (t) {
                case TileType::Wall: {
                    // Interior wall block: the bottom row of the block draws a tall face that also
                    // covers the row above (blocks are 2 tall); upper rows draw nothing.
                    bool border = (x == 0 || x == tiles_w - 1 || y == 0 || y == tiles_h - 1);
                    if (border) break;
                    bool below_wall = (y + 1 < tiles_h - 1) && tiles[y + 1][x] == TileType::Wall;
                    if (below_wall) break;
                    sprite = TileSprite::CliffTall; tint = biome.wall_tint; tall = true;
                    break;
                }
                case TileType::Tree:      sprite = biome.tree_sprite; tint = biome.prop_tint; tall = true;
                                          if (biome.tree_sprite == TileSprite::Tree && (h % 5 == 0)) sprite = TileSprite::TreeDead;
                                          break;
                case TileType::Rock:      sprite = TileSprite::Rock;  tint = biome.prop_tint; break;
                case TileType::Bush:      sprite = TileSprite::Bush;  tint = biome.prop_tint; break;
                case TileType::Chest:     sprite = TileSprite::Chest; break;
                case TileType::AltarGod:  sprite = TileSprite::AltarGod;  tall = true; break;
                case TileType::AltarBaal: sprite = TileSprite::AltarBaal; tall = true; break;
                case TileType::Jar:       sprite = TileSprite::Jar;   tint = biome.prop_tint; break;
                case TileType::Doorway:   sprite = TileSprite::Door;  tall = true; break;
                case TileType::Crack:     sprite = TileSprite::Crack; tint = biome.prop_tint; break;
                case TileType::Torch:     sprite = TileSprite::Torch; tall = true; break;
                case TileType::Exit:      sprite = TileSprite::Exit;  tall = true; break;
                // Encuentro: una hoguera encendida marca donde espera el personaje. El
                // tileset no tiene celda de PNJ; el tinte calido la separa de una antorcha
                // normal y el dialogo pone el nombre. (Celda propia pendiente en tiles.py.)
                case TileType::Event:     sprite = TileSprite::Torch; tall = true;
                                          tint = {255, 226, 150, 255}; break;
                default: break;
            }
            if (sprite == TileSprite::COUNT) continue;

            // Soft ground shadow under free-standing props (not under wall blocks / cliffs)
            if (textured && shadow_tex != TEXTURE_NONE && t != TileType::Wall && t != TileType::Exit) {
                DrawCmd sh;
                sh.sort_y = -999.0f;
                sh.texture_id = shadow_tex;
                sh.src = {0, 0, 64, 32};
                f32 sw = tall ? 14.0f : 12.0f, shh = tall ? 5.0f : 4.0f;
                sh.dst = {px + ts * 0.5f - sw * 0.5f, py + 13.0f - shh * 0.5f, sw, shh};
                sh.tint = {0, 0, 0, 90};
                out.push_back(sh);
            }

            DrawCmd cmd;
            cmd.sort_y = py + 13.0f;      // ground contact row of the prop sprites
            cmd.flip_x = (h >> 8) & 1 && (t == TileType::Tree || t == TileType::Rock || t == TileType::Bush);
            if (textured) {
                cmd.texture_id = tileset;
                cmd.src = get_tile_src(sprite);
                cmd.dst = tall ? Rect{px, py - ts, ts, ts * 2.0f} : Rect{px, py, ts, ts};
                cmd.tint = tint;
            } else {
                cmd.texture_id = TEXTURE_NONE;
                cmd.dst = tall ? Rect{px + 2, py - 8, ts - 4, ts + 6} : Rect{px + 2, py + 2, ts - 4, ts - 4};
                cmd.tint = fallback_color(t);
                cmd.outline = true;
            }
            out.push_back(cmd);
        }
    }
}

void Room::render_exit_barriers(Renderer& renderer, f32 time) const {
    // La banda roja sobre el vano de la arena mientras esta sellada (las salidas de sala ya
    // no se sellan: no hay salas)
    if (!has_arena || !arena_sealed) return;
    constexpr f32 ts = static_cast<f32>(TS);
    f32 pulse = 0.55f + 0.45f * std::sin(time * 6.0f);
    Color glow = {220, 60, 40, static_cast<u8>(90 + 90 * pulse)};
    Color core = {255, 170, 90, static_cast<u8>(140 + 100 * pulse)};
    const f32 gx0 = arena_gap_x0 * ts, gw = (arena_gap_x1 - arena_gap_x0 + 1) * ts, gy = arena_gap_y * ts;
    renderer.draw_rect({gx0, gy + ts * 0.5f - 3.0f, gw, 6}, glow);
    renderer.draw_rect({gx0, gy + ts * 0.5f - 1.0f, gw, 2}, core);
}

// =============================================================================
// FloorMap::generate — 2D grid layout
// =============================================================================
// El reparto de salas es CONFIGURACION, no sorteo (mandato del usuario, 2026-08-27):
// "ya no valen valores aleatorios para la historia y esos aspectos del gameplay".
// Una cadena por capitulo, un caracter por celda de la rejilla, fila de ARRIBA primero
// (la fila 0 es la del jefe; la ultima, la de inicio). Leyenda:
//   S inicio   B jefe   N combate   T tesoro   A altar   L elite
//   1..6 = sala Event con el relato k-esimo del pozo del capitulo (orden de `namespace Ev`)
// TODOS los relatos del pozo estan colocados: en una partida se ven los 3-6 del capitulo,
// no un subconjunto sorteado. El precio es una sala de Tesoro y una de Altar menos en las
// rejillas 4x3 (no caben 12 tipos en 12 celdas) y alguna de combate menos donde el pozo
// llega a 5-6 (caps 7 y 10); las 4x4 conservan 2+2.
static const char* const CHAPTER_LAYOUT[10] = {
    // Cap 1 — El Arroyo Querit (4x3, 3 relatos)
    "TNB1"
    "2LNA"
    "N3SL",
    // Cap 2 — La Viuda de Sarepta (4x3, 3 relatos)
    "N1BT"
    "LAN2"
    "3NSL",
    // Cap 3 — El Monte Carmelo (4x3, 4 relatos)
    "1NB2"
    "LATL"
    "3NS4",
    // Cap 4 — La Cueva de Horeb (4x3, 4 relatos)
    "2LBN"
    "1TA3"
    "N4SL",
    // Cap 5 — El Llamado (4x4, 3 relatos)
    "NTB1"
    "LNAN"
    "2NLT"
    "NAS3",
    // Cap 6 — Las Aguas de Jerico (4x3, 4 relatos)
    "1NBL"
    "TA42"
    "N3SL",
    // Cap 7 — El Aceite de la Viuda (4x3, 5 relatos)
    "N2B1"
    "LAT3"
    "45SL",
    // Cap 8 — El Hijo Sunamita (4x3, 4 relatos)
    "1LB2"
    "TNAN"
    "L3S4",
    // Cap 9 — Naaman el Sirio (4x3, 3 relatos: Hazael y Jehu se mudaron al cap. 10,
    // que es donde el libro los pone; la celda liberada recupera el Tesoro perdido)
    "N1BT"
    "2LTA"
    "LNS3",
    // Cap 10 — El Ejercito Celestial (4x4, 6 relatos: el sitio entero mas Hazael y
    // la uncion de Jehu, que desembocan en el epilogo)
    "LNB1"
    "5AT2"
    "3L6A"
    "T4SN",
};

void FloorMap::generate(int chapter) {
    auto def = get_chapter_def(chapter);
    int gw = def.grid_w;
    int gh = def.grid_h;
    // La zona continua cose hasta 4x4 celdas (Config::ZONE_CELLS_*): una rejilla mayor
    // no cabe en `Room::tiles` y se recorta con aviso.
    if (gw > Config::ZONE_CELLS_X || gh > Config::ZONE_CELLS_Y) {
        SDL_Log("FloorMap: rejilla %dx%d del capitulo %d supera la zona (%dx%d) — se recorta",
                gw, gh, chapter, Config::ZONE_CELLS_X, Config::ZONE_CELLS_Y);
        gw = std::min(gw, Config::ZONE_CELLS_X);
        gh = std::min(gh, Config::ZONE_CELLS_Y);
    }
    grid_w = gw;
    grid_h = gh;
    zone_built = false;

    room_count = gw * gh;
    if (room_count > MAX_ROOMS) room_count = MAX_ROOMS;

    // Reset cache
    for (int i = 0; i < MAX_ROOMS; i++) {
        room_generated[i] = false;
        rooms[i] = RoomNode{};
    }

    // Create grid: index = gy * gw + gx
    for (int gy = 0; gy < gh; gy++) {
        for (int gx = 0; gx < gw; gx++) {
            int idx = gy * gw + gx;
            rooms[idx].grid_x = gx;
            rooms[idx].grid_y = gy;
            rooms[idx].type = RoomType::Normal;

            // Connect to neighbors
            if (gy > 0)      rooms[idx].connections[0] = (gy - 1) * gw + gx;  // North
            if (gy < gh - 1) rooms[idx].connections[1] = (gy + 1) * gw + gx;  // South
            if (gx < gw - 1) rooms[idx].connections[2] = gy * gw + (gx + 1);  // East
            if (gx > 0)      rooms[idx].connections[3] = gy * gw + (gx - 1);  // West
        }
    }

    // Reparto FIJO desde la tabla CHAPTER_LAYOUT (ver arriba): ni la posicion del inicio,
    // ni la del jefe, ni que sala es tesoro/altar/elite, ni que relato cuenta cada sala
    // Event salen ya de ningun sorteo. `randi_range` no se toca en toda esta funcion.
    int start_idx = (gh - 1) * gw + gw / 2;   // respaldo si la tabla no trae 'S'/'B'
    int boss_idx  = 0 * gw + gw / 2;

    const EventPool pool = get_chapter_events(chapter);
    int assigned = 0;
    const char* layout = (chapter >= 1 && chapter <= 10) ? CHAPTER_LAYOUT[chapter - 1] : nullptr;
    if (layout && static_cast<int>(SDL_strlen(layout)) != room_count) {
        SDL_Log("FloorMap: layout del capitulo %d mide %d celdas y la rejilla %d — se ignora",
                chapter, static_cast<int>(SDL_strlen(layout)), room_count);
        layout = nullptr;
    }
    if (layout) {
        for (int i = 0; i < room_count; i++) {
            switch (layout[i]) {
                case 'S': rooms[i].type = RoomType::Start;    start_idx = i; break;
                case 'B': rooms[i].type = RoomType::Boss;     boss_idx  = i; break;
                case 'T': rooms[i].type = RoomType::Treasure; break;
                case 'A': rooms[i].type = RoomType::Altar;    break;
                case 'L': rooms[i].type = RoomType::Elite;    break;
                case '1': case '2': case '3': case '4': case '5': case '6': {
                    int k = layout[i] - '1';
                    if (k < pool.count) {
                        rooms[i].type = RoomType::Event;
                        rooms[i].event_id = pool.first + k;
                        assigned++;
                    }
                    break;
                }
                default: break;   // 'N' (y cualquier cosa rara) = sala de combate normal
            }
        }
    }
    rooms[start_idx].type = RoomType::Start;
    rooms[boss_idx].type  = RoomType::Boss;
    boss_room = boss_idx;

    // Un INTERIOR por capitulo (GDD2.0 fase 3), colgado de una sala exterior cualquiera que
    // no sea la de inicio ni la del jefe. Su nodo va DETRAS de la rejilla: asi `room_at(gx,gy)`
    // nunca lo devuelve (le ponemos coordenadas -1) y el minimapa lo puede saltar, que es lo
    // que evita que el mapa mienta. Y como `SaveData::current_room` es un INDICE y la rejilla
    // se regenera igual con la misma semilla, guardar dentro de un interior funciona sin tocar
    // el formato del fichero.
    if (room_count < MAX_ROOMS) {
        // El anfitrion se elige SIN sortear: `(chapter*7) % room_count` y hacia delante hasta
        // encontrar una sala que no sea la de inicio ni la del jefe. Cambia por capitulo, que
        // es lo unico que hace falta, y NO consume del generador — con `randi_range` aqui, el
        // sorteo entero se desplazaba y con el todos los mapas de la partida (se vio en la
        // sonda de tacto: el bot se salia de la sala a media medida).
        int host = -1;
        for (int k = 0; k < room_count && host < 0; k++) {
            int i = (chapter * 7 + k) % room_count;
            if (i == start_idx || i == boss_idx) continue;
            host = i;
        }
        if (host >= 0) {
            int in_idx = room_count++;
            rooms[in_idx] = RoomNode{};
            rooms[in_idx].type = RoomType::Interior;
            rooms[in_idx].grid_x = -1;
            rooms[in_idx].grid_y = -1;
            rooms[in_idx].interior_of = host;
            rooms[host].interior_room = in_idx;
            SDL_Log("FloorMap: interior %d colgado de la sala %d", in_idx, host);
        }
    }

    current_room = start_idx;

    SDL_Log("FloorMap: generated %dx%d grid (%d rooms), start=%d, boss=%d, relatos=%d/%d",
            gw, gh, room_count, start_idx, boss_idx, assigned, pool.count);
}

// =============================================================================
// FloorMap::room_at — find room index by grid coordinates
// =============================================================================
int FloorMap::room_at(int gx, int gy) const {
    for (int i = 0; i < room_count; i++) {
        if (rooms[i].grid_x == gx && rooms[i].grid_y == gy) return i;
    }
    return -1;
}
