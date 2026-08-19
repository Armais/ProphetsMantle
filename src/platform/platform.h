#pragma once

// =============================================================================
// MANTLE - Platform Detection & Abstraction
// =============================================================================

// Platform flags are set by CMake:
//   MANTLE_PLATFORM_DESKTOP
//   MANTLE_PLATFORM_WEB
//   MANTLE_PLATFORM_ANDROID
//   MANTLE_PLATFORM_IOS
//   MANTLE_PLATFORM_MOBILE  (Android OR iOS)
//   MANTLE_STEAM             (Desktop + Steamworks SDK present)

#include <SDL3/SDL.h>

enum class Platform {
    Desktop,
    Web,
    Android,
    iOS
};

inline Platform get_platform() {
#if defined(MANTLE_PLATFORM_ANDROID)
    return Platform::Android;
#elif defined(MANTLE_PLATFORM_IOS)
    return Platform::iOS;
#elif defined(MANTLE_PLATFORM_WEB)
    return Platform::Web;
#else
    return Platform::Desktop;
#endif
}

inline bool is_mobile() {
#if defined(MANTLE_PLATFORM_MOBILE)
    return true;
#else
    return false;
#endif
}

inline bool has_touch() {
    int count = 0;
    SDL_TouchID* devices = SDL_GetTouchDevices(&count);
    SDL_free(devices);
    return count > 0;
}

inline bool has_steam() {
#if defined(MANTLE_STEAM)
    return true;
#else
    return false;
#endif
}

// Platform-specific paths
inline const char* get_save_path() {
    return SDL_GetPrefPath("MantleGame", "MANTLE");
}

inline const char* get_asset_path() {
#if defined(MANTLE_PLATFORM_WEB)
    return "/assets/";
#elif defined(MANTLE_PLATFORM_ANDROID)
    return "";  // Android uses SDL_RWFromFile with asset manager
#else
    return "assets/";
#endif
}
