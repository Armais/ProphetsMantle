// =============================================================================
// MANTLE - Audio System Implementation
// =============================================================================

#include "audio/audio.h"
#include "platform/platform.h"

bool Audio::init() {
    if (!MIX_Init()) {
        SDL_Log("Failed to init SDL_mixer: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer) {
        SDL_Log("Failed to create mixer: %s", SDL_GetError());
        return false;
    }

    // Create a dedicated track for music
    music_track = MIX_CreateTrack(mixer);
    if (!music_track) {
        SDL_Log("Failed to create music track: %s", SDL_GetError());
    }

    initialized = true;
    SDL_Log("Audio initialized: 44100Hz, 16-bit stereo");
    return true;
}

void Audio::shutdown() {
    if (!initialized) return;

    // Stop music
    if (music_track) {
        MIX_StopTrack(music_track, 0);
        MIX_DestroyTrack(music_track);
        music_track = nullptr;
    }

    // Free music audio
    for (u32 i = 0; i < static_cast<u32>(MusicID::COUNT); i++) {
        if (music_tracks[i]) {
            MIX_DestroyAudio(music_tracks[i]);
            music_tracks[i] = nullptr;
        }
    }

    for (int i = 0; i < SFX_TRACKS; i++) {
        if (sfx_tracks[i]) {
            MIX_StopTrack(sfx_tracks[i], 0);
            MIX_DestroyTrack(sfx_tracks[i]);
            sfx_tracks[i] = nullptr;
        }
    }

    // Free SFX audio
    for (u32 i = 0; i < static_cast<u32>(SfxID::COUNT); i++) {
        if (sfx_audio[i]) {
            MIX_DestroyAudio(sfx_audio[i]);
            sfx_audio[i] = nullptr;
        }
    }

    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }

    MIX_Quit();
    initialized = false;
    SDL_Log("Audio shut down");
}

// =============================================================================
// Music
// =============================================================================
// Try `path`; if missing, try the same name with a .wav extension (procedural placeholders)
static MIX_Audio* load_with_fallback(MIX_Mixer* mixer, const std::string& full_path, bool predecode) {
    MIX_Audio* audio = MIX_LoadAudio(mixer, full_path.c_str(), predecode);
    if (audio) return audio;
    size_t dot = full_path.find_last_of('.');
    if (dot != std::string::npos) {
        std::string alt = full_path.substr(0, dot) + ".wav";
        audio = MIX_LoadAudio(mixer, alt.c_str(), predecode);
        if (audio) return audio;
    }
    return nullptr;
}

bool Audio::load_music(MusicID id, const char* path) {
    if (!initialized) return false;

    std::string full_path = std::string(get_asset_path()) + "audio/music/" + path;
    MIX_Audio* audio = load_with_fallback(mixer, full_path, false);
    if (!audio) {
        SDL_Log("Failed to load music '%s': %s", full_path.c_str(), SDL_GetError());
        return false;
    }

    u32 idx = static_cast<u32>(id);
    if (music_tracks[idx]) MIX_DestroyAudio(music_tracks[idx]);
    music_tracks[idx] = audio;
    return true;
}

void Audio::play_music(MusicID id, bool loop, f32 fade_in_ms) {
    if (!initialized || !music_track) return;

    u32 idx = static_cast<u32>(id);
    if (!music_tracks[idx]) return;

    if (current_music == id) return;  // Already playing

    MIX_StopTrack(music_track, 0);
    MIX_SetTrackAudio(music_track, music_tracks[idx]);
    MIX_SetTrackGain(music_track, music_volume);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (loop) {
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    }
    if (fade_in_ms > 0.0f) {
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER,
                              static_cast<Sint64>(fade_in_ms));
    }
    MIX_PlayTrack(music_track, props);
    SDL_DestroyProperties(props);

    current_music = id;
}

void Audio::stop_music(f32 fade_out_ms) {
    if (!initialized || !music_track) return;

    Sint64 fade_frames = 0;
    if (fade_out_ms > 0.0f) {
        fade_frames = MIX_TrackMSToFrames(music_track, static_cast<Sint64>(fade_out_ms));
    }
    MIX_StopTrack(music_track, fade_frames);
    current_music = MusicID::COUNT;
}

void Audio::pause_music() {
    if (initialized && music_track) MIX_PauseTrack(music_track);
}

void Audio::resume_music() {
    if (initialized && music_track) MIX_ResumeTrack(music_track);
}

void Audio::set_music_volume(f32 volume) {
    music_volume = volume;
    if (initialized && music_track) MIX_SetTrackGain(music_track, volume);
}

// =============================================================================
// SFX
// =============================================================================
bool Audio::load_sfx(SfxID id, const char* path) {
    if (!initialized) return false;

    std::string full_path = std::string(get_asset_path()) + "audio/sfx/" + path;
    MIX_Audio* audio = load_with_fallback(mixer, full_path, true);
    if (!audio) {
        SDL_Log("Failed to load SFX '%s': %s", full_path.c_str(), SDL_GetError());
        return false;
    }

    u32 idx = static_cast<u32>(id);
    if (sfx_audio[idx]) MIX_DestroyAudio(sfx_audio[idx]);
    sfx_audio[idx] = audio;
    return true;
}

void Audio::play_sfx(SfxID id, f32 volume_scale) {
    if (!initialized) return;

    u32 idx = static_cast<u32>(id);
    if (!sfx_audio[idx]) return;

    // Round-robin pool of SFX tracks so we can control gain per play
    if (!sfx_tracks[0]) {
        for (int i = 0; i < SFX_TRACKS; i++) sfx_tracks[i] = MIX_CreateTrack(mixer);
    }
    MIX_Track* track = sfx_tracks[sfx_track_next];
    sfx_track_next = (sfx_track_next + 1) % SFX_TRACKS;
    if (!track) {
        MIX_PlayAudio(mixer, sfx_audio[idx]);
        return;
    }
    MIX_SetTrackAudio(track, sfx_audio[idx]);
    MIX_SetTrackGain(track, sfx_volume * volume_scale);
    MIX_PlayTrack(track, 0);
}

void Audio::set_sfx_volume(f32 volume) {
    sfx_volume = volume;
}

// =============================================================================
// Bulk loading (standard file names)
// =============================================================================
void Audio::load_all_music() {
    load_music(MusicID::MainTheme,    "main_theme.ogg");
    load_music(MusicID::EliasCombat,  "elias_combat.ogg");
    load_music(MusicID::EliseoCombat, "eliseo_combat.ogg");
    load_music(MusicID::Exploration,  "exploration.ogg");
    load_music(MusicID::BossTheme,    "boss_theme.ogg");
    load_music(MusicID::Menu,         "menu.ogg");
    load_music(MusicID::Victory,      "victory.ogg");
    load_music(MusicID::Death,        "death.ogg");
    load_music(MusicID::Ascension,    "ascension.ogg");
}

void Audio::load_all_sfx() {
    load_sfx(SfxID::CayadoSwing1,       "cayado_swing_1.ogg");
    load_sfx(SfxID::CayadoSwing2,       "cayado_swing_2.ogg");
    load_sfx(SfxID::CayadoSwing3,       "cayado_swing_3.ogg");
    load_sfx(SfxID::CayadoHitEnemy,     "cayado_hit_enemy.ogg");
    load_sfx(SfxID::ProyectilFuego,     "proyectil_fuego.ogg");
    load_sfx(SfxID::ProyectilLuz,       "proyectil_luz.ogg");
    load_sfx(SfxID::DashWoosh,          "dash_woosh.ogg");
    load_sfx(SfxID::EnemyHit,           "enemy_hit.ogg");
    load_sfx(SfxID::EnemyDeath,         "enemy_death.ogg");
    load_sfx(SfxID::CriticalHit,        "critical_hit.ogg");
    load_sfx(SfxID::MiracleCastFire,    "miracle_cast_fire.ogg");
    load_sfx(SfxID::MiracleCastLight,   "miracle_cast_light.ogg");
    load_sfx(SfxID::MiracleRavens,      "miracle_ravens.ogg");
    load_sfx(SfxID::MiracleHeal,        "miracle_heal.ogg");
    load_sfx(SfxID::MiracleUltimate,    "miracle_ultimate.ogg");
    load_sfx(SfxID::MiracleCooldownReady,"miracle_cooldown_ready.ogg");
    load_sfx(SfxID::MenuSelect,         "menu_select.ogg");
    load_sfx(SfxID::MenuConfirm,        "menu_confirm.ogg");
    load_sfx(SfxID::MenuBack,           "menu_back.ogg");
    load_sfx(SfxID::ItemPickup,         "item_pickup.ogg");
    load_sfx(SfxID::LevelUp,            "level_up.ogg");
    load_sfx(SfxID::FaithGain,          "faith_gain.ogg");
    load_sfx(SfxID::FaithLoss,          "faith_loss.ogg");
    load_sfx(SfxID::Notification,       "notification.ogg");
    load_sfx(SfxID::FootstepsSand,      "footsteps_sand.ogg");
    load_sfx(SfxID::FootstepsStone,     "footsteps_stone.ogg");
    load_sfx(SfxID::FireCrackle,        "fire_crackle.ogg");
    load_sfx(SfxID::WindDesert,         "wind_desert.ogg");
    load_sfx(SfxID::WaterFlow,          "water_flow.ogg");
    load_sfx(SfxID::DoorOpen,           "door_open.ogg");
}
