#pragma once

// =============================================================================
// MANTLE - Audio System (SDL3_mixer)
// =============================================================================
//
// Music tracks (from GDD 6.2):
//   Main Theme, Elias Combat, Eliseo Combat, Exploration,
//   Boss Theme, Menu, Victory, Death, Ascension
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
enum class MusicID : u32 {
    MainTheme = 0,
    EliasCombat,
    EliseoCombat,
    Exploration,
    BossTheme,
    Menu,
    Victory,
    Death,
    Ascension,
    COUNT
};

// =============================================================================
// SFX IDs (from GDD SFX list)
// =============================================================================
enum class SfxID : u32 {
    // Combat
    CayadoSwing1 = 0,
    CayadoSwing2,
    CayadoSwing3,
    CayadoHitEnemy,
    ProyectilFuego,
    ProyectilLuz,
    DashWoosh,
    EnemyHit,
    EnemyDeath,
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

    // Init / Shutdown
    bool init();
    void shutdown();

    // Music
    bool load_music(MusicID id, const char* path);
    void play_music(MusicID id, bool loop = true, f32 fade_in_ms = 1000.0f);
    void stop_music(f32 fade_out_ms = 500.0f);
    void pause_music();
    void resume_music();
    void set_music_volume(f32 volume);

    // SFX
    bool load_sfx(SfxID id, const char* path);
    void play_sfx(SfxID id, f32 volume_scale = 1.0f);
    void set_sfx_volume(f32 volume);

    // Convenience: load all from standard paths
    void load_all_music();
    void load_all_sfx();
};
