#include "game/ground.h"
#include "game/rooms.h"
#include "render/renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// =============================================================================
// Constantes del trozo
// =============================================================================
namespace {

constexpr int CT  = 16;              // tiles por lado de trozo
constexpr int PPT = 32;              // px de textura por tile (2 px por unidad de juego)
constexpr int CPX = CT * PPT;        // 512
constexpr int MG  = 3;               // margen de tiles alrededor (los campos se emborronan)
constexpr int WIN = CT + 2 * MG;     // 22
constexpr int COARSE = 4;            // el ruido de baja frecuencia va a 1/4 de resolucion
constexpr int CGW = CPX / COARSE + 2;
constexpr int MAX_CHUNKS = 12;
constexpr f32 TSU = static_cast<f32>(Config::TILE_SIZE);
constexpr f32 CHUNK_UNITS = CT * TSU;          // 256 unidades
constexpr u32 GROUND_VERSION = 5;              // cambia la firma de todos los trozos al tocar el generador

struct Chunk {
    u32 tex  = TEXTURE_NONE;
    int cx   = -1, cy = -1;
    u64 key  = 0;
    u32 used = 0;
    bool valid = false;
};

Chunk g_chunks[MAX_CHUNKS];
u32   g_frame = 0;
int   g_built = 0;
f32   g_last_ms = 0.0f;
std::vector<u8> g_rgba;

// -----------------------------------------------------------------------------
// Ruido determinista
// -----------------------------------------------------------------------------
inline u32 hash3(i32 x, i32 y, u32 s) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline f32 hashf(i32 x, i32 y, u32 s) { return static_cast<f32>(hash3(x, y, s) & 0xFFFFu) / 65535.0f; }

inline f32 vnoise(f32 x, f32 y, u32 s) {
    const f32 fx = std::floor(x), fy = std::floor(y);
    const i32 ix = static_cast<i32>(fx), iy = static_cast<i32>(fy);
    f32 tx = x - fx, ty = y - fy;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const f32 a = hashf(ix, iy, s), b = hashf(ix + 1, iy, s);
    const f32 c = hashf(ix, iy + 1, s), d = hashf(ix + 1, iy + 1, s);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}
inline f32 fbm(f32 x, f32 y, u32 s, int oct) {
    f32 sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int o = 0; o < oct; o++) {
        sum += amp * vnoise(x, y, s + o * 31u);
        norm += amp;
        amp *= 0.5f;
        x *= 2.03f; y *= 2.03f;
    }
    return sum / norm;
}

inline f32 clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline f32 sstep(f32 e0, f32 e1, f32 x) {
    const f32 t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

struct RGB { f32 r, g, b; };
inline RGB rgb(u32 hex) {
    return {static_cast<f32>((hex >> 16) & 0xFF) / 255.0f, static_cast<f32>((hex >> 8) & 0xFF) / 255.0f,
            static_cast<f32>(hex & 0xFF) / 255.0f};
}
inline RGB mix(RGB a, RGB b, f32 t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline RGB mul(RGB a, f32 k) { return {a.r * k, a.g * k, a.b * k}; }

// -----------------------------------------------------------------------------
// Estilo del suelo por capitulo
// -----------------------------------------------------------------------------
enum class GStyle : u8 { Earth, Sand, Rock, Meadow, Paving };

struct GroundLook {
    GStyle style;
    RGB base, alt, veg, path, path_dk, shallow, deep, foam, stone, mortar;
    f32 veg_amount;      // cuanta mancha verde (0 = nada)
    f32 pebbles;         // densidad de guijarros
};

GroundLook look_for(int chapter, bool interior) {
    if (interior) {
        // Losas de interior a la luz del candil: calidas y gastadas
        return {GStyle::Paving, rgb(0x8f7a62), rgb(0x7c6a56), rgb(0x5f6a44), rgb(0x8a7056), rgb(0x6a5440),
                rgb(0x4f7a88), rgb(0x23495a), rgb(0xd8ecf0), rgb(0x9a8672), rgb(0x3e3228), 0.05f, 0.15f};
    }
    switch (chapter) {
        case 1:  // Querit: tierra del canon, matas secas
            return {GStyle::Earth, rgb(0xa8845c), rgb(0x8c6c4a), rgb(0x7a7a3c), rgb(0xd2b48a), rgb(0xb09068),
                    rgb(0x4f93a6), rgb(0x1f5a70), rgb(0xe6f6f8), rgb(0x9a8a74), rgb(0x4a3a2a), 0.30f, 0.9f};
        case 2:  // Sarepta: arena palida de costa con ondas
            return {GStyle::Sand, rgb(0xd2bb8e), rgb(0xc0a37a), rgb(0x8a8a4a), rgb(0xe2cfa4), rgb(0xbfa57c),
                    rgb(0x57a2b4), rgb(0x22607a), rgb(0xf0fbfc), rgb(0xbfae90), rgb(0x6a5a44), 0.12f, 0.5f};
        case 3:  // Carmelo: roca gris olivacea con hierba de monte
            return {GStyle::Rock, rgb(0x8c8a78), rgb(0x76765f), rgb(0x6c7d4c), rgb(0xa8987a), rgb(0x8a7c62),
                    rgb(0x5a8ea0), rgb(0x23506a), rgb(0xe6f2f4), rgb(0x9a9888), rgb(0x4a4a40), 0.55f, 0.7f};
        case 4:  // Horeb: desierto rojizo y piedra
            return {GStyle::Rock, rgb(0x9c745e), rgb(0x84604e), rgb(0x7a7048), rgb(0x8a6450), rgb(0x74503e),
                    rgb(0x5a8a98), rgb(0x2a4a5a), rgb(0xe6f0f0), rgb(0x9a8070), rgb(0x4a3428), 0.10f, 1.0f};
        case 5:  // El Llamado: pradera de labranza
            return {GStyle::Meadow, rgb(0x7f9a4c), rgb(0x95aa5c), rgb(0x6a8a3e), rgb(0xa88a5a), rgb(0x86683e),
                    rgb(0x5aa0b0), rgb(0x23607a), rgb(0xeefafc), rgb(0x9a9478), rgb(0x4a4430), 1.0f, 0.25f};
        case 6:  // Jerico: losas de ciudad sobre arena, con el verde del oasis
            return {GStyle::Paving, rgb(0xa89670), rgb(0x968460), rgb(0x6f8a4a), rgb(0xc8b088), rgb(0xa89068),
                    rgb(0x5a9a8a), rgb(0x245a5a), rgb(0xe6f6f0), rgb(0xb8a688), rgb(0x5a4c3a), 0.35f, 0.3f};
        case 7:  // El aceite: aldea pobre, tierra batida
            return {GStyle::Earth, rgb(0xa48c6a), rgb(0x8c765a), rgb(0x7a7a44), rgb(0xbaa07a), rgb(0x947a5a),
                    rgb(0x5a90a0), rgb(0x235a6e), rgb(0xe6f4f6), rgb(0xa09078), rgb(0x4a3e30), 0.20f, 0.8f};
        case 8:  // Sunem: casa noble, losa clara y cesped de jardin
            return {GStyle::Paving, rgb(0xa8a080), rgb(0x9a906c), rgb(0x76964e), rgb(0xd2c09c), rgb(0xb09c7c),
                    rgb(0x5aa2b8), rgb(0x22607e), rgb(0xf0fbfc), rgb(0xd0c4ac), rgb(0x6a5e4c), 0.45f, 0.2f};
        case 9:  // El Jordan: riberas verdes y arena
            return {GStyle::Meadow, rgb(0x7a9852), rgb(0x9aa46a), rgb(0x5f8a3c), rgb(0xb2986a), rgb(0x8c7450),
                    rgb(0x5ab0b8), rgb(0x1f6a80), rgb(0xf0fcfc), rgb(0x9a9a80), rgb(0x4a4a36), 0.9f, 0.35f};
        case 10: // Samaria sitiada: losa oscura, tierra y escombro
            return {GStyle::Paving, rgb(0x8c8070), rgb(0x786c5c), rgb(0x5f6a44), rgb(0xa8967a), rgb(0x86745a),
                    rgb(0x4f7a88), rgb(0x23465a), rgb(0xd8ecf0), rgb(0x968e80), rgb(0x3e3a34), 0.15f, 0.9f};
        default:
            return {GStyle::Earth, rgb(0xa8845c), rgb(0x8c6c4a), rgb(0x7a7a3c), rgb(0xd2b48a), rgb(0xb09068),
                    rgb(0x4f93a6), rgb(0x1f5a70), rgb(0xe6f6f8), rgb(0x9a8a74), rgb(0x4a3a2a), 0.30f, 0.9f};
    }
}

// -----------------------------------------------------------------------------
// Tiles -> mascaras
// -----------------------------------------------------------------------------
enum Mask { M_PATH, M_WATER, M_DEEP, M_PIT, M_SOLID, M_GRASS, M_WALL, M_COUNT };

inline bool is_water_tile(TileType t) {
    return t == TileType::Water || t == TileType::River || t == TileType::Ford || t == TileType::Bridge;
}

void tile_masks(TileType t, f32 out[M_COUNT]) {
    for (int i = 0; i < M_COUNT; i++) out[i] = 0.0f;
    switch (t) {
        case TileType::Path:      out[M_PATH] = 1.0f; break;
        case TileType::Exit:      out[M_PATH] = 1.0f; break;
        case TileType::Event:     out[M_PATH] = 0.7f; break;
        case TileType::Chest:     out[M_PATH] = 0.6f; out[M_SOLID] = 0.5f; break;
        case TileType::AltarGod:
        case TileType::AltarBaal: out[M_PATH] = 0.6f; out[M_SOLID] = 0.8f; break;
        case TileType::Doorway:   out[M_PATH] = 1.0f; break;
        case TileType::Water:     out[M_WATER] = 1.0f; break;
        case TileType::River:     out[M_WATER] = 1.0f; out[M_DEEP] = 1.0f; break;
        case TileType::Ford:      out[M_WATER] = 1.0f; break;
        case TileType::Bridge:    out[M_WATER] = 1.0f; out[M_DEEP] = 0.6f; break;
        case TileType::Pit:       out[M_PIT] = 1.0f; break;
        case TileType::Wall:      out[M_SOLID] = 1.0f; out[M_WALL] = 1.0f; break;
        case TileType::Rock:
        case TileType::Jar:
        case TileType::Crack:     out[M_SOLID] = 1.0f; break;
        case TileType::Tree:      out[M_SOLID] = 0.8f; out[M_GRASS] = 0.6f; break;
        case TileType::Torch:     out[M_SOLID] = 0.4f; break;
        case TileType::Bush:      out[M_GRASS] = 1.0f; out[M_SOLID] = 0.3f; break;
        case TileType::TallGrass: out[M_GRASS] = 1.0f; break;
        default: break;
    }
}

// Firma de los tiles del trozo y su margen: si cambia uno, el trozo se rehace
u64 chunk_key(const Room& room, int cx, int cy) {
    u64 h = 1469598103934665603ull;
    auto mixin = [&](u64 v) { h ^= v; h *= 1099511628211ull; };
    mixin(GROUND_VERSION);
    mixin(static_cast<u64>(room.chapter));
    mixin(static_cast<u64>(room.type == RoomType::Interior ? 1 : 0));
    mixin(static_cast<u64>(room.tiles_w) << 16 | static_cast<u64>(room.tiles_h));
    const int x0 = cx * CT - MG, y0 = cy * CT - MG;
    for (int y = y0; y < y0 + WIN; y++) {
        for (int x = x0; x < x0 + WIN; x++) {
            const bool in = x >= 0 && y >= 0 && x < room.tiles_w && y < room.tiles_h;
            mixin(in ? static_cast<u64>(room.tiles[y][x]) : 255ull);
        }
    }
    return h;
}

// Desenfoque [1 2 1] separable, `passes` veces, sobre una mascara WIN x WIN
void blur(f32* m, int passes) {
    f32 tmp[WIN * WIN];
    for (int p = 0; p < passes; p++) {
        for (int y = 0; y < WIN; y++)
            for (int x = 0; x < WIN; x++) {
                const f32 l = m[y * WIN + std::max(0, x - 1)], c = m[y * WIN + x], r = m[y * WIN + std::min(WIN - 1, x + 1)];
                tmp[y * WIN + x] = (l + 2.0f * c + r) * 0.25f;
            }
        for (int y = 0; y < WIN; y++)
            for (int x = 0; x < WIN; x++) {
                const f32 u = tmp[std::max(0, y - 1) * WIN + x], c = tmp[y * WIN + x], d = tmp[std::min(WIN - 1, y + 1) * WIN + x];
                m[y * WIN + x] = (u + 2.0f * c + d) * 0.25f;
            }
    }
}

inline f32 bilinear(const f32* m, f32 fx, f32 fy) {
    fx = std::max(0.0f, std::min(static_cast<f32>(WIN - 1) - 0.001f, fx));
    fy = std::max(0.0f, std::min(static_cast<f32>(WIN - 1) - 0.001f, fy));
    const int x = static_cast<int>(fx), y = static_cast<int>(fy);
    const f32 tx = fx - x, ty = fy - y;
    const f32 a = m[y * WIN + x], b = m[y * WIN + x + 1], c = m[(y + 1) * WIN + x], d = m[(y + 1) * WIN + x + 1];
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// Voronoi: distancia al punto mas cercano y al segundo (celdas de `cell` unidades)
inline void voronoi(f32 x, f32 y, f32 cell, u32 seed, f32& d1, f32& d2, u32& id) {
    const f32 gx = x / cell, gy = y / cell;
    const i32 ix = static_cast<i32>(std::floor(gx)), iy = static_cast<i32>(std::floor(gy));
    d1 = 1e9f; d2 = 1e9f; id = 0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            const i32 cxi = ix + i, cyi = iy + j;
            const f32 px = cxi + 0.15f + 0.7f * hashf(cxi, cyi, seed);
            const f32 py = cyi + 0.15f + 0.7f * hashf(cxi, cyi, seed + 7u);
            const f32 dx = px - gx, dy = py - gy;
            const f32 d = dx * dx + dy * dy;
            if (d < d1) { d2 = d1; d1 = d; id = hash3(cxi, cyi, seed + 13u); }
            else if (d < d2) { d2 = d; }
        }
    d1 = std::sqrt(d1) * cell;
    d2 = std::sqrt(d2) * cell;
}

// -----------------------------------------------------------------------------
// Generar un trozo: un TRABAJO que se prepara una vez (mascaras y ruido grueso) y luego se
// pinta por filas. Lo visible se hace de una tacada; lo de alrededor, unas filas por frame
// con un presupuesto de tiempo, para que cruzar de trozo no de tirones en movil.
// -----------------------------------------------------------------------------
struct Job {
    bool active = false;
    int  slot = -1, cx = 0, cy = 0, next_row = 0;
    u64  key = 0;
    const Room* room = nullptr;
    GroundLook L{};
    u32  seed = 0;
    int  wx0 = 0, wy0 = 0;
    f32  ox = 0.0f, oy = 0.0f;
    f32  masks[M_COUNT][WIN * WIN];
    f32  n_macro[CGW * CGW], n_patch[CGW * CGW], n_edge[CGW * CGW];
    std::vector<u8> rgba;
};
Job g_job;   // el que se rellena por frames (prefetch)
Job g_now;   // el que se hace entero en el acto (visibles)

constexpr f32 UPP = 1.0f / (PPT / TSU);   // unidades por pixel (0,5)

void job_prepare(Job& J, const Room& room, int cx, int cy, u64 key, int slot) {
    const bool interior = room.type == RoomType::Interior;
    J.active = true; J.slot = slot; J.cx = cx; J.cy = cy; J.key = key; J.next_row = 0; J.room = &room;
    J.L = look_for(room.chapter, interior);
    J.seed = static_cast<u32>(room.chapter) * 7919u + (interior ? 104729u : 0u);
    J.wx0 = cx * CT - MG; J.wy0 = cy * CT - MG;
    J.ox = cx * CHUNK_UNITS; J.oy = cy * CHUNK_UNITS;
    for (int y = 0; y < WIN; y++)
        for (int x = 0; x < WIN; x++) {
            const int tx = J.wx0 + x, ty = J.wy0 + y;
            f32 v[M_COUNT];
            if (tx >= 0 && ty >= 0 && tx < room.tiles_w && ty < room.tiles_h) tile_masks(room.tiles[ty][tx], v);
            else { for (int i = 0; i < M_COUNT; i++) v[i] = 0.0f; v[M_SOLID] = 1.0f; }
            for (int i = 0; i < M_COUNT; i++) J.masks[i][y * WIN + x] = v[i];
        }
    blur(J.masks[M_PATH], 1);
    blur(J.masks[M_WATER], 1);
    blur(J.masks[M_DEEP], 2);
    blur(J.masks[M_PIT], 1);
    blur(J.masks[M_SOLID], 1);
    blur(J.masks[M_GRASS], 2);
    blur(J.masks[M_WALL], 4);
    for (int j = 0; j < CGW; j++)
        for (int i = 0; i < CGW; i++) {
            const f32 ux = J.ox + (i * COARSE) * UPP, uy = J.oy + (j * COARSE) * UPP;
            J.n_macro[j * CGW + i] = fbm(ux / 70.0f, uy / 70.0f, J.seed + 1u, 3);
            J.n_patch[j * CGW + i] = fbm(ux / 22.0f, uy / 22.0f, J.seed + 2u, 3);
            J.n_edge[j * CGW + i]  = fbm(ux / 7.0f, uy / 7.0f, J.seed + 3u, 2);
        }
    J.rgba.resize(static_cast<size_t>(CPX) * CPX * 4);
}

void job_rows(Job& J, int row_end) {
    const Room& room = *J.room;
    const GroundLook& L = J.L;
    const u32 seed = J.seed;
    const int wx0 = J.wx0, wy0 = J.wy0;
    const f32 ox = J.ox, oy = J.oy;
    const f32 upp = UPP;
    auto& masks = J.masks;
    std::vector<u8>& out = J.rgba;
    auto coarse = [&](const f32* g, int px, int py) {
        const f32 fx = static_cast<f32>(px) / COARSE, fy = static_cast<f32>(py) / COARSE;
        const int x = static_cast<int>(fx), y = static_cast<int>(fy);
        const f32 tx = fx - x, ty = fy - y;
        const f32 a = g[y * CGW + x], b = g[y * CGW + x + 1], c = g[(y + 1) * CGW + x], d = g[(y + 1) * CGW + x + 1];
        return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
    };
    const f32* n_macro = J.n_macro;
    const f32* n_patch = J.n_patch;
    const f32* n_edge  = J.n_edge;
    row_end = std::min(row_end, CPX);
    for (int py = J.next_row; py < row_end; py++) {
        const f32 uy = oy + (py + 0.5f) * upp;
        const f32 fty = uy / TSU - wy0 - 0.5f;
        for (int px = 0; px < CPX; px++) {
            const f32 ux = ox + (px + 0.5f) * upp;
            const f32 ftx = ux / TSU - wx0 - 0.5f;
            const f32 nm = coarse(n_macro, px, py);
            const f32 np = coarse(n_patch, px, py);
            const f32 ne = coarse(n_edge, px, py);
            const i32 wpx = static_cast<i32>(std::floor(ux * 2.0f)), wpy = static_cast<i32>(std::floor(uy * 2.0f));
            const f32 grain = hashf(wpx, wpy, seed + 5u) * 2.0f - 1.0f;

            const f32 f_path  = bilinear(masks[M_PATH], ftx, fty);
            const f32 f_water = bilinear(masks[M_WATER], ftx, fty);
            const f32 f_deep  = bilinear(masks[M_DEEP], ftx, fty);
            const f32 f_pit   = bilinear(masks[M_PIT], ftx, fty);
            const f32 f_solid = bilinear(masks[M_SOLID], ftx, fty);
            const f32 f_grass = bilinear(masks[M_GRASS], ftx, fty);
            const f32 f_wall  = bilinear(masks[M_WALL], ftx, fty);

            // --- base del terreno
            RGB c = mix(L.base, L.alt, sstep(0.30f, 0.72f, nm));
            f32 light = 1.0f;
            switch (L.style) {
                case GStyle::Sand: {
                    const f32 rip = std::sin((ux * 0.55f + uy * 0.22f) + nm * 9.0f);
                    light += rip * 0.035f;
                    break;
                }
                case GStyle::Rock: {
                    // Roca a dos escalas, y las grietas solo donde el ruido lo pide: en todas
                    // partes salia un damero de barro seco (primera captura).
                    f32 d1, d2; u32 id;
                    voronoi(ux, uy, 34.0f, seed + 17u, d1, d2, id);
                    const f32 crack = (1.0f - sstep(0.0f, 0.9f, d2 - d1)) * sstep(0.45f, 0.70f, np);
                    light *= 1.0f - crack * 0.16f;
                    light *= 0.95f + 0.10f * static_cast<f32>(id & 0xFF) / 255.0f;
                    voronoi(ux, uy, 11.0f, seed + 19u, d1, d2, id);
                    light *= 0.97f + 0.06f * static_cast<f32>(id & 0xFF) / 255.0f;
                    break;
                }
                case GStyle::Meadow: {
                    const f32 blades = vnoise(ux * 1.3f, uy * 0.35f, seed + 21u);
                    light *= 0.93f + 0.14f * blades;
                    break;
                }
                case GStyle::Paving: {
                    // Losas donde hay ciudad (cerca de los muros de las casas y en las manchas del
                    // ruido) y tierra batida entre medias: una zona enlosada entera era un tablero.
                    const f32 pav = (room.type == RoomType::Interior) ? 1.0f
                                  : sstep(0.42f, 0.62f, nm * 0.70f + f_wall * 1.2f);
                    if (pav > 0.0f) {
                        f32 d1, d2; u32 id;
                        voronoi(ux, uy, 12.0f, seed + 23u, d1, d2, id);
                        const f32 e = d2 - d1;
                        const f32 mortar = 1.0f - sstep(0.30f, 1.1f, e);
                        const f32 tone = static_cast<f32>(id & 0xFF) / 255.0f;
                        RGB stone = mix(L.stone, mix(L.base, L.alt, tone), 0.45f);
                        stone = mul(stone, 0.92f + 0.12f * tone);
                        stone = mul(stone, 0.95f + 0.07f * sstep(0.8f, 3.5f, e));
                        stone = mix(stone, L.mortar, mortar * 0.42f);
                        c = mix(c, stone, pav * 0.92f);
                    }
                    break;
                }
                case GStyle::Earth:
                default: {
                    const f32 clods = vnoise(ux * 0.45f, uy * 0.45f, seed + 25u);
                    light *= 0.95f + 0.10f * clods;
                    break;
                }
            }
            // manchas de vegetacion (mas alrededor de matas y hierba alta)
            const f32 veg = clamp01(sstep(0.58f - L.veg_amount * 0.22f, 0.78f - L.veg_amount * 0.22f, np) *
                                    L.veg_amount + f_grass * 0.55f);
            if (veg > 0.0f) {
                const f32 vt = vnoise(ux * 0.9f, uy * 0.9f, seed + 29u);
                c = mix(c, mul(L.veg, 0.86f + 0.24f * vt), veg * (L.style == GStyle::Paving ? 0.75f : 0.9f));
            }
            // guijarros: puntos sueltos a resolucion de 2 px
            if (L.pebbles > 0.0f) {
                const u32 hp = hash3(wpx >> 2, wpy >> 2, seed + 31u);
                if ((hp & 1023u) < static_cast<u32>(10.0f * L.pebbles)) {
                    const f32 t = static_cast<f32>((hp >> 10) & 0xFF) / 255.0f;
                    c = mix(c, t > 0.5f ? mul(L.stone, 1.15f) : mul(L.mortar, 1.3f), 0.55f);
                }
            }
            c = mul(c, light * (1.0f + grain * 0.045f));

            // --- senda: tierra pisada con borde organico y rodadas
            const f32 p = f_path + (ne - 0.5f) * 0.28f;
            // borde de tierra levantada por fuera (la senda se lee hundida en el terreno)
            const f32 berm = sstep(0.28f, 0.38f, p) * (1.0f - sstep(0.38f, 0.44f, p));
            c = mul(c, 1.0f - berm * 0.10f);
            const f32 pa = sstep(0.39f, 0.47f, p);
            if (pa > 0.0f) {
                RGB d = mix(L.path, L.path_dk, sstep(0.3f, 0.8f, np));
                const f32 rut = sstep(0.39f, 0.45f, p) * (1.0f - sstep(0.45f, 0.58f, p));
                const f32 track = 0.5f + 0.5f * std::sin((ux + uy * 0.3f) * 0.9f + ne * 4.0f);
                d = mul(d, (1.0f - rut * 0.22f) * (1.0f - track * 0.05f * sstep(0.5f, 0.9f, p)) *
                           (1.0f + grain * 0.07f));
                const u32 hs = hash3(wpx >> 1, wpy >> 1, seed + 37u);
                if ((hs & 1023u) < 6u) d = mul(L.stone, 1.05f);
                c = mix(c, d, pa);
            }

            // --- sombra de contacto al pie de lo solido
            const f32 ao = sstep(0.10f, 0.65f, f_solid);
            c = mul(c, 1.0f - 0.26f * ao);

            // --- hueco
            const f32 q = f_pit + (ne - 0.5f) * 0.14f;
            if (q > 0.30f) {
                const f32 lip = sstep(0.30f, 0.44f, q) * (1.0f - sstep(0.44f, 0.52f, q));
                c = mix(c, mul(L.path_dk, 0.75f), lip * 0.7f);
                const f32 in = sstep(0.46f, 0.54f, q);
                const f32 depth = sstep(0.5f, 0.95f, q);
                const RGB hole = mix(RGB{0.20f, 0.15f, 0.12f}, RGB{0.04f, 0.03f, 0.03f}, depth);
                c = mix(c, hole, in);
            }

            // --- agua: arena mojada, espuma en la orilla, de clara a honda
            const f32 w = f_water + (ne - 0.5f) * 0.10f;
            if (w > 0.26f) {
                const f32 wet = sstep(0.26f, 0.47f, w);
                c = mix(c, mul(mix(c, L.deep, 0.18f), 0.72f), wet * 0.65f);
                const f32 in = sstep(0.47f, 0.53f, w);
                if (in > 0.0f) {
                    const f32 depth = clamp01(sstep(0.52f, 0.98f, w) * 0.75f + f_deep * 0.45f);
                    RGB wc = mix(L.shallow, L.deep, depth);
                    wc = mul(wc, 0.96f + (np - 0.5f) * 0.12f + grain * 0.015f);
                    const f32 foam = sstep(0.47f, 0.505f, w) * (1.0f - sstep(0.505f, 0.57f, w));
                    wc = mix(wc, L.foam, foam * 0.75f);
                    c = mix(c, wc, in);
                }
            }

            u8* o = &out[(static_cast<size_t>(py) * CPX + px) * 4];
            o[0] = static_cast<u8>(clamp01(c.r) * 255.0f + 0.5f);
            o[1] = static_cast<u8>(clamp01(c.g) * 255.0f + 0.5f);
            o[2] = static_cast<u8>(clamp01(c.b) * 255.0f + 0.5f);
            o[3] = 255;
        }
    }

    J.next_row = row_end;
}

void job_finish(Job& J) {
    const Room& room = *J.room;
    const GroundLook& L = J.L;
    const u32 seed = J.seed;
    const int cx = J.cx, cy = J.cy;
    std::vector<u8>& out = J.rgba;
    // Vados: piedras planas sobre el agua (la celda del tileset traia su propio cuadrado de agua)
    for (int ty = cy * CT; ty < cy * CT + CT && ty < room.tiles_h; ty++)
        for (int tx = cx * CT; tx < cx * CT + CT && tx < room.tiles_w; tx++) {
            if (ty < 0 || tx < 0 || room.tiles[ty][tx] != TileType::Ford) continue;
            for (int k = 0; k < 3; k++) {
                const u32 hs = hash3(tx, ty, seed + 41u + k);
                const f32 sx = (tx - cx * CT) * PPT + 6.0f + static_cast<f32>(hs % 20) + k * 3.0f;
                const f32 sy = (ty - cy * CT) * PPT + 6.0f + static_cast<f32>((hs >> 8) % 20);
                const f32 r = 5.0f + static_cast<f32>((hs >> 16) % 4);
                for (int yy = static_cast<int>(sy - r - 2); yy <= static_cast<int>(sy + r + 2); yy++)
                    for (int xx = static_cast<int>(sx - r - 2); xx <= static_cast<int>(sx + r + 2); xx++) {
                        if (xx < 0 || yy < 0 || xx >= CPX || yy >= CPX) continue;
                        const f32 dx = (xx + 0.5f - sx) / r, dy = (yy + 0.5f - sy) / (r * 0.75f);
                        const f32 d = std::sqrt(dx * dx + dy * dy);
                        if (d > 1.15f) continue;
                        u8* o = &out[(static_cast<size_t>(yy) * CPX + xx) * 4];
                        RGB s = mul(L.stone, 1.05f - 0.25f * dy);
                        if (d > 0.95f) s = mul(L.mortar, 0.9f);
                        const f32 a = clamp01((1.15f - d) * 6.0f);
                        o[0] = static_cast<u8>(o[0] + (clamp01(s.r) * 255.0f - o[0]) * a);
                        o[1] = static_cast<u8>(o[1] + (clamp01(s.g) * 255.0f - o[1]) * a);
                        o[2] = static_cast<u8>(o[2] + (clamp01(s.b) * 255.0f - o[2]) * a);
                    }
            }
        }
}

Chunk* find_ready(const Room& room, int cx, int cy) {
    const u64 key = chunk_key(room, cx, cy);
    for (Chunk& c : g_chunks)
        if (c.valid && c.cx == cx && c.cy == cy && c.key == key) { c.used = g_frame; return &c; }
    return nullptr;
}

// Ranura para un trozo nuevo: una libre, o la que menos se ha usado, sin tocar las de ESTE
// frame (son las que se estan viendo) ni la que tiene reservada el trabajo en curso.
int pick_slot() {
    int best = -1;
    for (int i = 0; i < MAX_CHUNKS; i++) {
        if (g_job.active && g_job.slot == i) continue;
        if (!g_chunks[i].valid) return i;
        if (g_chunks[i].used == g_frame) continue;
        if (best < 0 || g_chunks[i].used < g_chunks[best].used) best = i;
    }
    return best;
}

Chunk* upload(Renderer& r, Job& J) {
    char name[32];
    std::snprintf(name, sizeof(name), "ground_chunk_%d", J.slot);
    Chunk& c = g_chunks[J.slot];
    c.tex = r.create_texture_from_rgba(name, CPX, CPX, J.rgba.data(), /*smooth=*/true);
    if (c.tex != TEXTURE_NONE) {
        if (TextureEntry* te = r.get_texture(c.tex)) SDL_SetTextureBlendMode(te->texture, SDL_BLENDMODE_NONE);
    }
    c.cx = J.cx; c.cy = J.cy; c.key = J.key; c.used = g_frame; c.valid = (c.tex != TEXTURE_NONE);
    J.active = false;
    g_built++;
    return c.valid ? &c : nullptr;
}

f32 ms_since(u64 t0) {
    return static_cast<f32>(SDL_GetPerformanceCounter() - t0) * 1000.0f / static_cast<f32>(SDL_GetPerformanceFrequency());
}

// Trozo visible que falta: entero, ahora. Solo pasa al entrar en una zona o al teletransportarse.
Chunk* build_now(Renderer& r, const Room& room, int cx, int cy) {
    if (g_job.active && g_job.cx == cx && g_job.cy == cy) g_job.active = false;   // lo pisa
    const int slot = pick_slot();
    if (slot < 0) return nullptr;
    const u64 t0 = SDL_GetPerformanceCounter();
    job_prepare(g_now, room, cx, cy, chunk_key(room, cx, cy), slot);
    job_rows(g_now, CPX);
    job_finish(g_now);
    Chunk* c = upload(r, g_now);
    g_last_ms = ms_since(t0);
    if (g_built <= 4 || g_last_ms > 40.0f)
        SDL_Log("Suelo: trozo %d,%d horneado en %.1f ms (ranura %d, %d en total)", cx, cy, g_last_ms, slot, g_built);
    return c;
}

// Trozo de alrededor: unas filas por frame, con presupuesto de tiempo
constexpr f32 PREFETCH_BUDGET_MS = 3.0f;

void prefetch_step(Renderer& r, const Room& room, int cx0, int cy0, int cx1, int cy1, int ncx, int ncy) {
    const u64 t0 = SDL_GetPerformanceCounter();
    if (!g_job.active) {
        // El siguiente que falte en el anillo de alrededor de la vista
        for (int cy = cy0 - 1; cy <= cy1 + 1 && !g_job.active; cy++)
            for (int cx = cx0 - 1; cx <= cx1 + 1 && !g_job.active; cx++) {
                if (cx < 0 || cy < 0 || cx >= ncx || cy >= ncy) continue;
                if (find_ready(room, cx, cy)) continue;
                const int slot = pick_slot();
                if (slot < 0) return;
                job_prepare(g_job, room, cx, cy, chunk_key(room, cx, cy), slot);
            }
        if (!g_job.active) return;
    }
    while (g_job.next_row < CPX && ms_since(t0) < PREFETCH_BUDGET_MS) job_rows(g_job, g_job.next_row + 16);
    if (g_job.next_row < CPX) return;
    // Al acabar, la sala puede haber cambiado debajo (otra zona, un tile abierto): se tira
    if (g_job.room != &room || chunk_key(room, g_job.cx, g_job.cy) != g_job.key) { g_job.active = false; return; }
    job_finish(g_job);
    upload(r, g_job);
}

}  // namespace

// =============================================================================
// API
// =============================================================================
void ground_invalidate() {
    for (Chunk& c : g_chunks) c.valid = false;
    g_job.active = false;
}

int ground_chunks_built() { return g_built; }
f32 ground_last_build_ms() { return g_last_ms; }

bool ground_draw_baked(Renderer& renderer, const Room& room, Rect view, f32 time) {
    g_frame++;
    const int ncx = (room.tiles_w + CT - 1) / CT, ncy = (room.tiles_h + CT - 1) / CT;
    int cx0 = static_cast<int>(std::floor(view.x / CHUNK_UNITS)), cx1 = static_cast<int>(std::floor((view.x + view.w) / CHUNK_UNITS));
    int cy0 = static_cast<int>(std::floor(view.y / CHUNK_UNITS)), cy1 = static_cast<int>(std::floor((view.y + view.h) / CHUNK_UNITS));
    cx0 = std::max(cx0, 0); cy0 = std::max(cy0, 0);
    cx1 = std::min(cx1, ncx - 1); cy1 = std::min(cy1, ncy - 1);
    if (cx1 < cx0 || cy1 < cy0) return false;

    // Los visibles se hacen YA (al entrar en una zona son 1-4; luego casi nunca hace falta)
    for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++) {
            Chunk* c = find_ready(room, cx, cy);
            if (!c) c = build_now(renderer, room, cx, cy);
            if (!c) return false;
            renderer.draw_sprite_rect(c->tex, {0.0f, 0.0f, static_cast<f32>(CPX), static_cast<f32>(CPX)},
                                      {cx * CHUNK_UNITS, cy * CHUNK_UNITS, CHUNK_UNITS, CHUNK_UNITS},
                                      Color::white());
        }

    // Y los de alrededor, a ratos, para que al cruzar ya esten hechos
    prefetch_step(renderer, room, cx0, cy0, cx1, cy1, ncx, ncy);

    // Destellos del agua: la textura es estatica, el brillo no. Un trazo claro por tile de
    // agua visible que aparece y se apaga a su ritmo (fase por tile).
    if (renderer.shadow_tex != TEXTURE_NONE) {
        const int tx0 = std::max(0, static_cast<int>(view.x / TSU) - 1);
        const int ty0 = std::max(0, static_cast<int>(view.y / TSU) - 1);
        const int tx1 = std::min(room.tiles_w - 1, static_cast<int>((view.x + view.w) / TSU) + 1);
        const int ty1 = std::min(room.tiles_h - 1, static_cast<int>((view.y + view.h) / TSU) + 1);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                const TileType t = room.tiles[ty][tx];
                if (t != TileType::Water && t != TileType::River) continue;
                const u32 h = hash3(tx, ty, 977u);
                const f32 ph = std::fmod(time * (0.35f + (h & 7) * 0.04f) + (h >> 8) % 100 * 0.01f, 1.0f);
                const f32 a = std::sin(ph * 3.14159265f);
                const f32 sx = tx * TSU + 2.0f + static_cast<f32>((h >> 4) % 10) + std::sin(time * 0.7f + tx) * 1.5f;
                const f32 sy = ty * TSU + 3.0f + static_cast<f32>((h >> 12) % 10);
                renderer.draw_sprite_rect(renderer.shadow_tex, {0.0f, 0.0f, 64.0f, 32.0f},
                                          {sx, sy, 6.0f + (h % 5), 1.1f},
                                          {235, 250, 255, static_cast<u8>(150.0f * a * a)});
            }
    }
    return true;
}
