#pragma once

// =============================================================================
// MANTLE - Audio System (SDL3_mixer)
// =============================================================================
//
// Music tracks: siete globales + DOS por capitulo (la de juego y la de su
// cinematica y dialogo de apertura). Los temas por capitulo sustituyen a los
// tres viejos de ambiente (Exploration / EliasCombat / EliseoCombat), que
// repartian diez capitulos entre dos pistas.
//
// SFX categories (from GDD 6.3):
//   Combat, Miracles, UI, Ambient
//
// =============================================================================

#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include "core/types.h"

// =============================================================================
// Music IDs (from GDD soundtrack plan)
// =============================================================================
// Los diez capitulos del juego (Config::CHAPTERS_ACT_I + ACT_II). El
// static_assert de audio.cpp ata este numero al de config.h.
static constexpr int MUSIC_CHAPTERS = 10;

enum class MusicID : u32 {
    // --- Globales ---
    MainTheme = 0,   // pantalla de titulo
    Menu,            // El Santuario (meta-progreso entre partidas)
    BossTheme,       // mientras la arena del jefe esta sellada
    Victory,
    Death,
    Ascension,       // el carro de fuego: cinematica y paginas
    Prologue,        // "El Cielo Cerrado" (ui/cinematic.h)

    // --- Un tema de juego por capitulo ---
    // Los dos bloques van CONTIGUOS a proposito: se indexan con aritmetica
    // desde el numero de capitulo (chapter_theme / chapter_intro), igual que
    // las variantes consecutivas de SFX. No los reordenes ni metas nada en
    // medio.
    ChapterTheme1,
    ChapterTheme2,
    ChapterTheme3,
    ChapterTheme4,
    ChapterTheme5,
    ChapterTheme6,
    ChapterTheme7,
    ChapterTheme8,
    ChapterTheme9,
    ChapterTheme10,

    // --- Su variacion desnuda, para la cinematica y el dialogo de apertura ---
    ChapterIntro1,
    ChapterIntro2,
    ChapterIntro3,
    ChapterIntro4,
    ChapterIntro5,
    ChapterIntro6,
    ChapterIntro7,
    ChapterIntro8,
    ChapterIntro9,
    ChapterIntro10,

    COUNT
};

// Tema de juego del capitulo (1..10). Fuera de rango cae al del primero.
inline MusicID chapter_theme(int chapter) {
    if (chapter < 1 || chapter > MUSIC_CHAPTERS) chapter = 1;
    return static_cast<MusicID>(static_cast<u32>(MusicID::ChapterTheme1) + (chapter - 1));
}

// Tema de la cinematica / dialogo de apertura del capitulo (1..10).
inline MusicID chapter_intro(int chapter) {
    if (chapter < 1 || chapter > MUSIC_CHAPTERS) chapter = 1;
    return static_cast<MusicID>(static_cast<u32>(MusicID::ChapterIntro1) + (chapter - 1));
}

// =============================================================================
// SFX IDs (from GDD SFX list)
// =============================================================================
enum class SfxID : u32 {
    // Combat
    CayadoSwing1 = 0,
    CayadoSwing2,
    CayadoSwing3,
    CayadoHitEnemy,
    // Variantes. Tienen que quedar CONSECUTIVAS al sonido base: play_sfx_variant
    // suma un desplazamiento al id, igual que la terna CayadoSwing1..3.
    ProyectilFuego,
    ProyectilFuego2,
    ProyectilFuego3,
    ProyectilLuz,
    ProyectilLuz2,
    ProyectilLuz3,
    DashWoosh,
    EnemyHit,
    EnemyHit2,
    EnemyHit3,
    EnemyDeath,
    EnemyDeath2,
    CriticalHit,

    // Miracles
    MiracleCastFire,
    MiracleCastLight,
    MiracleRavens,
    MiracleHeal,
    MiracleUltimate,
    MiracleCooldownReady,

    // UI
    MenuSelect,
    MenuConfirm,
    MenuBack,
    ItemPickup,
    LevelUp,
    FaithGain,
    FaithLoss,
    Notification,

    // Voz (GDD 6.4). Las variantes van CONSECUTIVAS: play_sfx_variant suma al id.
    // Elias y Eliseo tienen tomas propias — se separan por TONO y VOCAL (Elias grave y
    // cerrado, Eliseo mas alto y abierto), que es lo que un jugador distingue a la
    // primera; dos reverbs distintos, no.
    VozEliasEsfuerzo1,
    VozEliasEsfuerzo2,
    VozEliasEsfuerzo3,
    VozEliasDolor1,
    VozEliasDolor2,
    VozEliasDolor3,
    VozEliseoEsfuerzo1,
    VozEliseoEsfuerzo2,
    VozEliseoEsfuerzo3,
    VozEliseoDolor1,
    VozEliseoDolor2,
    VozEliseoDolor3,
    VozMilagro1,
    VozMilagro2,
    VozJefe,

    // Ambient
    FootstepsSand,
    FootstepsStone,
    FireCrackle,
    WindDesert,
    WaterFlow,
    DoorOpen,

    COUNT
};

// =============================================================================
// Audio System
// =============================================================================
struct Audio {
    MIX_Mixer* mixer = nullptr;

    // Music tracks
    MIX_Audio* music_tracks[static_cast<u32>(MusicID::COUNT)] = {};
    MIX_Track* music_track = nullptr;  // Single track for music playback

    // Nombre de fichero de cada pista, para el log de play_music. Es lo unico
    // que deja a un guion de autoplay comprobar QUE suena en cada capitulo.
    char music_names[static_cast<u32>(MusicID::COUNT)][32] = {};

    // SFX audio data + small track pool (per-play gain)
    MIX_Audio* sfx_audio[static_cast<u32>(SfxID::COUNT)] = {};
    static constexpr int SFX_TRACKS = 10;
    MIX_Track* sfx_tracks[SFX_TRACKS] = {};
    int sfx_track_next = 0;

    // State
    MusicID current_music = MusicID::COUNT;  // None playing
    f32 music_volume = 0.8f;
    f32 sfx_volume   = 1.0f;
    bool initialized = false;

    // Ducking: un SFX fuerte agacha la musica un instante y la deja subir. Sin esto, el
    // estallido de un milagro compite con la pista en vez de imponerse a ella.
    f32 duck = 0.0f;              // 0..1, cuanto esta agachada ahora mismo
    // Sorteo de la variante de SFX. NO usa rand(): ese es el generador de los mapas
    // (core/types.h), y el audio no puede mover la partida. Ver play_sfx_variant.
    u32 variant_rng = 0x9E3779B9u;
    static constexpr f32 DUCK_DEPTH   = 0.45f;   // fraccion de volumen que se cede
    static constexpr f32 DUCK_RELEASE = 2.2f;    // 1/s de recuperacion

    // Init / Shutdown
    bool init();
    void shutdown();

    // Music
    bool load_music(MusicID id, const char* path);
    // Carga `path` y, si no esta, `fallback`. Un capitulo cuyo .ogg todavia no
    // existe (tanda de musica a medias) o que no viajo al paquete se nota
    // mucho menos sonando a otra cosa que en silencio.
    bool load_music_or(MusicID id, const char* path, const char* fallback);
    void play_music(MusicID id, bool loop = true, f32 fade_in_ms = 1000.0f);
    void stop_music(f32 fade_out_ms = 500.0f);
    void pause_music();
    void resume_music();
    void set_music_volume(f32 volume);

    // SFX
    bool load_sfx(SfxID id, const char* path);
    void play_sfx(SfxID id, f32 volume_scale = 1.0f);
    // Una de `count` tomas consecutivas a partir de `first`, al azar. Manteniendo el
    // ataque se disparan tres o cuatro tiros por segundo: con un solo fichero el oido
    // oye una ametralladora, no a un profeta lanzando fuego.
    void play_sfx_variant(SfxID first, int count, f32 volume_scale = 1.0f);
    void set_sfx_volume(f32 volume);

    // Agacha la musica durante un momento (0..1). Lo llama play_sfx solo cuando el
    // sonido es lo bastante fuerte; el Engine solo tiene que llamar a update().
    void duck_music(f32 amount);
    void update(f32 dt);   // recuperacion del ducking

    // Convenience: load all from standard paths
    void load_all_music();
    void load_all_sfx();
};
