// =============================================================================
// MANTLE - Room System Implementation
// =============================================================================

#include "game/rooms.h"
#include "render/renderer.h"
#include <cstring>
#include <cmath>
#include <queue>

static constexpr int W = Config::ROOM_TILES_X;
static constexpr int H = Config::ROOM_TILES_Y;
static constexpr int TS = Config::TILE_SIZE;
static constexpr int CX = W / 2;   // 10
static constexpr int CY = H / 2;   // 5

// =============================================================================
// Helpers
// =============================================================================
static bool in_bounds(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

// Deterministic per-tile hash for visual variation
static u32 tile_hash(int x, int y, int salt) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u + static_cast<u32>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Is (tx,ty) part of the "apron" right inside an opening (kept free of obstacles)
static bool is_exit_apron(const Room& r, int tx, int ty) {
    if (r.door_north && Room::is_gap_x(tx) && ty <= 2) return true;
    if (r.door_south && Room::is_gap_x(tx) && ty >= H - 3) return true;
    if (r.door_west  && Room::is_gap_y(ty) && tx <= 2) return true;
    if (r.door_east  && Room::is_gap_y(ty) && tx >= W - 3) return true;
    return false;
}

// Free interior tile for decoration: not border, floor, not near centre, not in an apron
static bool is_placeable(const Room& room, int tx, int ty, int margin_from_center) {
    if (tx < 1 || tx >= W - 1 || ty < 1 || ty >= H - 1) return false;
    if (room.tiles[ty][tx] != TileType::Floor) return false;
    if (is_exit_apron(room, tx, ty)) return false;
    if (margin_from_center > 0) {
        if (std::abs(tx - CX) <= margin_from_center && std::abs(ty - CY) <= margin_from_center) return false;
    }
    return true;
}

static void scatter(Room& room, TileType t, int count, int margin) {
    for (int i = 0; i < count; i++) {
        for (int attempt = 0; attempt < 25; attempt++) {
            int tx = randi_range(1, W - 2);
            int ty = randi_range(1, H - 2);
            if (is_placeable(room, tx, ty, margin)) {
                room.tiles[ty][tx] = t;
                break;
            }
        }
    }
}

// Convert a tile into a walkable one while carving a trail (river -> bridge, obstacle -> path)
static void carve_tile(Room& room, int tx, int ty, bool mark_path) {
    if (!in_bounds(tx, ty)) return;
    TileType& t = room.tiles[ty][tx];
    switch (t) {
        case TileType::River:
        case TileType::Ford:
            t = TileType::Bridge; break;
        case TileType::Wall:
            if (tx == 0 || tx == W - 1 || ty == 0 || ty == H - 1) {
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
        case TileType::Floor:
            if (mark_path) t = TileType::Path; else if (t != TileType::Floor) t = TileType::Floor;
            break;
        default:
            break;
    }
}

// L-shaped trail from (x0,y0) to (x1,y1): 1 tile of Path in the middle, neighbours cleared
static void carve_trail(Room& room, int x0, int y0, int x1, int y1) {
    int x = x0, y = y0;
    auto step_here = [&](int cx, int cy, bool horizontal) {
        carve_tile(room, cx, cy, true);
        // Clear the flanking tiles so the trail is comfortably 2-3 tiles wide
        if (horizontal) { carve_tile(room, cx, cy - 1, false); carve_tile(room, cx, cy + 1, false); }
        else            { carve_tile(room, cx - 1, cy, false); carve_tile(room, cx + 1, cy, false); }
    };
    // Reach the target column/row first, then run straight into the opening
    // (north/south exits: horizontal leg first; east/west: vertical leg first)
    bool vertical_first = !(y1 == 0 || y1 == H - 1);
    if (vertical_first) {
        while (y != y1) { step_here(x, y, false); y += (y1 > y) ? 1 : -1; }
        while (x != x1) { step_here(x, y, true);  x += (x1 > x) ? 1 : -1; }
    } else {
        while (x != x1) { step_here(x, y, true);  x += (x1 > x) ? 1 : -1; }
        while (y != y1) { step_here(x, y, false); y += (y1 > y) ? 1 : -1; }
    }
    step_here(x, y, vertical_first ? false : true);
}

// BFS over walkable tiles from (sx,sy). Fills `reach` (W*H bools).
static void flood_fill(const Room& room, int sx, int sy, bool reach[H][W]) {
    std::memset(reach, 0, sizeof(bool) * W * H);
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
            if (!in_bounds(nx, ny) || reach[ny][nx]) continue;
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
    // River: 2 tiles wide, serpentine, kept to one side of the centre band so the
    // north/south trails never run along it. East/west trails cross it once (bridge).
    if (biome.river) {
        bool left = randi_range(0, 1) == 0;
        int lo = left ? 2 : 12;
        int hi = left ? 5 : 15;
        int col = randi_range(lo, hi);
        // Long meanders: the course drifts one column every other row (instead of a
        // per-row jitter that read as a sawtooth) and the bed breathes between 2 and 3 wide
        int width = randi_range(2, 3);
        int drift = randi_range(0, 1) ? 1 : -1;
        for (int y = 1; y < H - 1; y++) {
            if ((y & 1) == 0) {
                if (randi_range(0, 3) == 0) drift = -drift;
                col += drift;
                if (col < lo) { col = lo; drift = 1; }
                if (col > hi) { col = hi; drift = -1; }
                if (randi_range(0, 2) == 0) width = (width == 2 ? 3 : 2);
            }
            for (int i = 0; i < width; i++) {
                int rx = col + i;
                if (rx > 0 && rx < W - 1) room.tiles[y][rx] = TileType::River;
            }
        }
        // 1-2 natural fords (the second one at least 2 rows away so both are real crossings)
        int fords = randi_range(1, 2);
        int first_fy = -10;
        for (int i = 0; i < fords; i++) {
            int fy = randi_range(2, H - 3);
            for (int tries = 0; tries < 8 && std::abs(fy - first_fy) < 2; tries++) fy = randi_range(2, H - 3);
            if (i == 0) first_fy = fy;
            for (int x = 1; x < W - 1; x++)
                if (room.tiles[fy][x] == TileType::River) room.tiles[fy][x] = TileType::Ford;
        }
    }

    // Shallow puddles: walkable, but wading through them costs speed (Config::WATER_SPEED_MULT).
    // Only in river biomes, and only on plain floor so trails and props stay untouched.
    if (biome.river) {
        int puddles = randi_range(2, 4);
        for (int i = 0; i < puddles; i++) {
            int px = randi_range(2, W - 4), py = randi_range(2, H - 4);
            int pw = randi_range(1, 2), ph = randi_range(1, 2);
            for (int y = py; y < py + ph && y < H - 1; y++)
                for (int x = px; x < px + pw && x < W - 1; x++)
                    if (room.tiles[y][x] == TileType::Floor) room.tiles[y][x] = TileType::Water;
        }
    }

    // Buildings: small stone blocks (2x2 / 3x2) that break lines of sight in villages/cities
    for (int b = 0; b < biome.buildings; b++) {
        for (int attempt = 0; attempt < 40; attempt++) {
            int bw = randi_range(2, 3), bh = 2;
            int bx = randi_range(2, W - 3 - bw);
            int by = randi_range(2, H - 3 - bh);
            bool ok = true;
            for (int y = by - 1; y <= by + bh && ok; y++)
                for (int x = bx - 1; x <= bx + bw && ok; x++)
                    if (!is_placeable(room, x, y, 2)) ok = false;
            if (!ok) continue;
            for (int y = by; y < by + bh; y++)
                for (int x = bx; x < bx + bw; x++)
                    room.tiles[y][x] = TileType::Wall;
            break;
        }
    }

    auto jitter = [](int base) { return base <= 0 ? 0 : std::max(0, base + randi_range(-1, 2)); };
    scatter(room, TileType::Tree,      jitter(biome.trees),  3);
    scatter(room, TileType::Rock,      jitter(biome.rocks),  2);
    scatter(room, TileType::Bush,      jitter(biome.bushes), 1);
    scatter(room, TileType::TallGrass, jitter(biome.grass),  1);
}

// =============================================================================
// Room::generate
// =============================================================================
void Room::generate(RoomType room_type, int chap, int difficulty) {
    type = room_type;
    chapter = chap;
    cleared = false;
    visited = false;
    exits_locked = false;
    exit_open = false;
    altar_used = false;
    chest_opened = false;
    enemy_spawns.clear();

    const BiomeStyle biome = get_biome_style(chapter);

    // Fill with floor
    std::memset(tiles, static_cast<int>(TileType::Floor), sizeof(tiles));

    // Borders: cliffs everywhere except an opening (gap) on connected sides
    for (int x = 0; x < W; x++) {
        if (!(door_north && is_gap_x(x))) tiles[0][x] = TileType::Wall;
        if (!(door_south && is_gap_x(x))) tiles[H - 1][x] = TileType::Wall;
    }
    for (int y = 0; y < H; y++) {
        if (!(door_west && is_gap_y(y))) tiles[y][0] = TileType::Wall;
        if (!(door_east && is_gap_y(y))) tiles[y][W - 1] = TileType::Wall;
    }

    // Biome decoration for open rooms (boss arena stays mostly clear)
    if (room_type == RoomType::Normal || room_type == RoomType::Elite ||
        room_type == RoomType::Start  || room_type == RoomType::Treasure ||
        room_type == RoomType::Altar  || room_type == RoomType::Event) {
        decorate(*this, biome);
    } else if (room_type == RoomType::Boss) {
        BiomeStyle arena = biome;
        arena.river = false;
        arena.trees = 2; arena.rocks = 2; arena.bushes = 1; arena.grass = 3;
        decorate(*this, arena);
        // Clear the arena centre band
        for (int y = 2; y <= H - 3; y++)
            for (int x = 4; x <= W - 5; x++)
                if (tiles[y][x] != TileType::Floor) tiles[y][x] = TileType::Floor;
    }

    // Trails from a hub near the centre to every opening: guarantees the exits are
    // reachable and gives the player an obvious route. The hub jitters a little on
    // combat rooms so the cross of paths is not identical every room.
    int hx = CX, hy = CY;
    if (room_type == RoomType::Normal || room_type == RoomType::Elite) {
        hx = CX + randi_range(-2, 2);
        hy = CY + randi_range(-1, 1);
    }
    if (door_north) carve_trail(*this, hx, hy, CX, 0);
    if (door_south) carve_trail(*this, hx, hy, CX, H - 1);
    if (door_west)  carve_trail(*this, hx, hy, 0, CY);
    if (door_east)  carve_trail(*this, hx, hy, W - 1, CY);
    // Keep the true centre walkable too (start-of-run spawn / boss arena reference)
    carve_tile(*this, CX, CY, false);

    // Torches flanking the openings on "built" biomes
    if (biome.torches) {
        if (door_north) { tiles[1][GAP_X0 - 1] = TileType::Torch; tiles[1][GAP_X1 + 1] = TileType::Torch; }
        if (door_south) { tiles[H - 2][GAP_X0 - 1] = TileType::Torch; tiles[H - 2][GAP_X1 + 1] = TileType::Torch; }
        if (door_west)  { tiles[GAP_Y0 - 1][1] = TileType::Torch; tiles[GAP_Y1 + 1][1] = TileType::Torch; }
        if (door_east)  { tiles[GAP_Y0 - 1][W - 2] = TileType::Torch; tiles[GAP_Y1 + 1][W - 2] = TileType::Torch; }
    }

    // Special room props at the centre
    if (room_type == RoomType::Altar)    tiles[CY][CX] = TileType::AltarGod;
    if (room_type == RoomType::Treasure) tiles[CY][CX] = TileType::Chest;
    if (room_type == RoomType::Event)    tiles[CY][CX] = TileType::Event;
    if (room_type == RoomType::Start) {
        // A Baal altar to smash for a first taste of faith
        tiles[CY - 2][CX + 4] = TileType::AltarBaal;
    }

    // Reachability from the centre (used to place enemies where they can actually fight)
    bool reach[H][W];
    flood_fill(*this, CX, CY, reach);

    // Fallback: if any opening is unreachable, carve straight to it (should not happen)
    auto ensure = [&](bool door, int gx, int gy) {
        if (!door || reach[gy][gx]) return;
        carve_trail(*this, CX, CY, gx, gy);
        flood_fill(*this, CX, CY, reach);
    };
    ensure(door_north, CX, 0);
    ensure(door_south, CX, H - 1);
    ensure(door_west, 0, CY);
    ensure(door_east, W - 1, CY);

    // Enemies for combat rooms
    if (room_type == RoomType::Normal || room_type == RoomType::Elite) {
        // Density grows with the chapter: ch1 = 2-4, ch5 = 4-10 (rooms used to top out at 8)
        int lo = 2 + difficulty / 2;
        int hi = std::min(3 + difficulty * 3 / 2, Config::MAX_ENEMIES_PER_ROOM);
        int enemy_count = randi_range(lo, std::max(lo, hi));
        if (room_type == RoomType::Elite) enemy_count += 2;

        for (int i = 0; i < enemy_count; i++) {
            EnemySpawn spawn;
            spawn.position = tile_to_world(CX + (i % 2 ? 4 : -4), CY + (i % 3) - 1);  // fallback

            for (int attempt = 0; attempt < 40; attempt++) {
                int tx = randi_range(1, W - 2);
                int ty = randi_range(1, H - 2);
                if (!reach[ty][tx]) continue;
                if (std::abs(tx - CX) <= 3 && std::abs(ty - CY) <= 2) continue;   // not on top of the player
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
}

// =============================================================================
// Room::place_exit — chapter exit arch at the top centre (after the boss)
// =============================================================================
void Room::place_exit() {
    exit_open = true;
    int ex = CX, ey = 2;
    exit_tx = ex; exit_ty = ey;
    for (int x = ex - 1; x <= ex + 1; x++)
        for (int y = ey - 1; y <= ey + 1; y++)
            if (in_bounds(x, y) && x > 0 && x < W - 1 && y > 0 && y < H - 1)
                tiles[y][x] = TileType::Path;
    tiles[ey][ex] = TileType::Exit;
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
                if (x < 1 || x >= W - 1 || y < 1 || y >= H - 1) continue;
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
    // Out-of-bounds: allowed only through the openings (border tiles are walls otherwise)
    // so try_room_transition() can detect the crossing. Sealed while enemies are alive.
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return !exits_locked;

    if (exits_locked && (tx == 0 || tx == W - 1 || ty == 0 || ty == H - 1)) return false;

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
            return true;
        default:
            return false;  // Wall, Pit, Tree, Rock, River = not walkable
    }
}

bool Room::is_tile_solid_for_projectiles(int tx, int ty) const {
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return false;
    switch (tiles[ty][tx]) {
        case TileType::Wall:
        case TileType::Tree:
        case TileType::Rock:
            return true;
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

bool Room::is_slow_world(Vec2 p) const {
    int tx = static_cast<int>(std::floor(p.x / TS));
    int ty = static_cast<int>(std::floor(p.y / TS));
    if (!in_bounds(tx, ty)) return false;
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

void Room::render_ground(Renderer& renderer, u32 tileset, f32 time) const {
    const BiomeStyle biome = get_biome_style(chapter);
    constexpr f32 ts = static_cast<f32>(TS);
    const bool textured = (tileset != TEXTURE_NONE);
    const int water_frame = static_cast<int>(time * 2.0f);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            TileType t = tiles[y][x];
            f32 px = static_cast<f32>(x) * ts;
            f32 py = static_cast<f32>(y) * ts;
            Rect dst = {px, py, ts, ts};

            if (!textured) {
                Color c = fallback_color(t);
                if (c.a > 0 && t != TileType::Tree && t != TileType::Rock && t != TileType::Bush &&
                    t != TileType::Chest && t != TileType::AltarGod && t != TileType::AltarBaal &&
                    t != TileType::Torch && t != TileType::Exit && t != TileType::Event)
                    renderer.draw_rect(dst, c);
                continue;
            }

            u32 h = tile_hash(x, y, chapter);

            // Base floor (biome family: sand / meadow / paving, 4 variants) under everything
            // that is not water/border-wall/pit
            bool border_wall = (t == TileType::Wall) && (x == 0 || x == W - 1 || y == 0 || y == H - 1);
            if (!border_wall && !is_water_like(t) && t != TileType::Pit) {
                TileSprite fs = static_cast<TileSprite>(static_cast<u32>(biome.floor_base) + (h % 4));
                renderer.draw_sprite_rect(tileset, get_tile_src(fs), dst, biome.floor_tint);

                // Ground decals (tufts, pebbles, flowers, cracks...) on plain floor tiles only
                if (t == TileType::Floor) {
                    u32 h2 = tile_hash(x, y, chapter + 31);
                    if (static_cast<int>(h2 % 100) < biome.detail_pct) {
                        TileSprite ds = biome.details[(h2 >> 8) % 3];
                        bool flip = ((h2 >> 12) & 1) != 0;
                        renderer.draw_sprite_rect(tileset, get_tile_src(ds), dst, biome.floor_tint, flip);
                    }
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
                case TileType::Pit:
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Pit), dst, biome.floor_tint);
                    break;
                case TileType::TallGrass:
                    renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Grass), dst, biome.prop_tint);
                    break;
                case TileType::Wall: {
                    bool border = (x == 0 || x == W - 1 || y == 0 || y == H - 1);
                    if (y == 0) {
                        // Top border: tall stone face rising out of view
                        renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::CliffTall),
                                                  {px, py - ts, ts, ts * 2.0f}, biome.wall_tint);
                    } else if (border) {
                        renderer.draw_sprite_rect(tileset, get_tile_src(TileSprite::Cliff), dst, biome.wall_tint);
                    }
                    // Interior walls (buildings) are drawn as tall props in collect_props()
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
                    if (!in_bounds(nx, ny)) return false;   // outside the room: no bank on the border
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
    if (textured) {
        constexpr int BANDS = 5;
        constexpr f32 BAND_H = 0.8f;
        for (int x = 0; x < W; x++) {
            if (tiles[0][x] != TileType::Wall || tiles[1][x] == TileType::Wall) continue;
            for (int b = 0; b < BANDS; b++) {
                f32 k = 1.0f - static_cast<f32>(b) / BANDS;
                renderer.draw_rect({static_cast<f32>(x) * ts, ts + b * BAND_H, ts, BAND_H},
                                   {0, 0, 0, static_cast<u8>(72.0f * k * k)});
            }
        }
    }
}

void Room::collect_props(std::vector<DrawCmd>& out, u32 tileset, u32 shadow_tex) const {
    const BiomeStyle biome = get_biome_style(chapter);
    constexpr f32 ts = static_cast<f32>(TS);
    const bool textured = (tileset != TEXTURE_NONE);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            TileType t = tiles[y][x];
            f32 px = static_cast<f32>(x) * ts;
            f32 py = static_cast<f32>(y) * ts;
            u32 h = tile_hash(x, y, chapter + 77);

            TileSprite sprite = TileSprite::COUNT;
            Color tint = Color::white();
            bool tall = false;
            switch (t) {
                case TileType::Wall: {
                    // Interior wall block: the bottom row of the block draws a tall face that also
                    // covers the row above (blocks are 2 tall); upper rows draw nothing.
                    bool border = (x == 0 || x == W - 1 || y == 0 || y == H - 1);
                    if (border) break;
                    bool below_wall = (y + 1 < H - 1) && tiles[y + 1][x] == TileType::Wall;
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
    if (!exits_locked) return;
    constexpr f32 ts = static_cast<f32>(TS);
    f32 pulse = 0.55f + 0.45f * std::sin(time * 6.0f);
    Color glow = {220, 60, 40, static_cast<u8>(90 + 90 * pulse)};
    Color core = {255, 170, 90, static_cast<u8>(140 + 100 * pulse)};

    f32 gx0 = GAP_X0 * ts, gw = (GAP_X1 - GAP_X0 + 1) * ts;
    f32 gy0 = GAP_Y0 * ts, gh = (GAP_Y1 - GAP_Y0 + 1) * ts;
    if (door_north) { renderer.draw_rect({gx0, 0, gw, 6}, glow);  renderer.draw_rect({gx0, 2, gw, 2}, core); }
    if (door_south) { renderer.draw_rect({gx0, H * ts - 6, gw, 6}, glow); renderer.draw_rect({gx0, H * ts - 4, gw, 2}, core); }
    if (door_west)  { renderer.draw_rect({0, gy0, 6, gh}, glow);  renderer.draw_rect({2, gy0, 2, gh}, core); }
    if (door_east)  { renderer.draw_rect({W * ts - 6, gy0, 6, gh}, glow); renderer.draw_rect({W * ts - 4, gy0, 2, gh}, core); }
}

// =============================================================================
// FloorMap::generate — 2D grid layout
// =============================================================================
void FloorMap::generate(int chapter) {
    auto def = get_chapter_def(chapter);
    int gw = def.grid_w;
    int gh = def.grid_h;
    grid_w = gw;
    grid_h = gh;

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

    // Place Start at bottom-center, Boss at top-center
    int start_idx = (gh - 1) * gw + gw / 2;
    int boss_idx  = 0 * gw + gw / 2;  // row 0, center column

    rooms[start_idx].type = RoomType::Start;
    rooms[boss_idx].type  = RoomType::Boss;
    boss_room = boss_idx;

    // Place one Treasure and one Altar on random rooms (not Start/Boss)
    std::vector<int> candidates;
    for (int i = 0; i < room_count; i++) {
        if (i != start_idx && i != boss_idx) {
            candidates.push_back(i);
        }
    }

    if (candidates.size() >= 2) {
        int a = randi_range(0, static_cast<i32>(candidates.size()) - 1);
        int treasure_idx = candidates[a];
        candidates.erase(candidates.begin() + a);

        int b = randi_range(0, static_cast<i32>(candidates.size()) - 1);
        int altar_idx = candidates[b];
        candidates.erase(candidates.begin() + b);

        rooms[treasure_idx].type = RoomType::Treasure;
        rooms[altar_idx].type = RoomType::Altar;

        // Encuentro biblico del capitulo (game/events.h): uno por mapa, siempre presente.
        // Va antes que la Elite porque el relato pesa mas que una sala extra de combate.
        if (!candidates.empty()) {
            int e = randi_range(0, static_cast<i32>(candidates.size()) - 1);
            rooms[candidates[e]].type = RoomType::Event;
            candidates.erase(candidates.begin() + e);
        }

        // Place one Elite room if there are candidates left
        if (!candidates.empty()) {
            int c = randi_range(0, static_cast<i32>(candidates.size()) - 1);
            rooms[candidates[c]].type = RoomType::Elite;
        }
    }

    current_room = start_idx;

    SDL_Log("FloorMap: generated %dx%d grid (%d rooms), start=%d, boss=%d",
            gw, gh, room_count, start_idx, boss_idx);
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
