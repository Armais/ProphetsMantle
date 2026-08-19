// =============================================================================
// MANTLE — El Manto del Profeta
// =============================================================================
//
// Twin-Stick Roguelike / Action
// "Vive la era mas oscura de Israel como los profetas Elias y Eliseo"
//
// Platforms: Web (Emscripten), Steam (PC), Android, iOS
// Engine: Custom C++ + SDL3
//
// Debug flags:
//   --script <file>   drive the game from an autoplay script (see core/engine.h)
//   --shots <dir>     directory for F12 / scripted screenshots (default: screenshots)
//   --touch           force the mobile auto-aim on desktop (debug)
//
// =============================================================================

#include "core/engine.h"
#include <cstring>

int main(int argc, char* argv[]) {
    Engine engine;

    const char* script = nullptr;
    bool touch_mode = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) script = argv[++i];
        else if (std::strcmp(argv[i], "--shots") == 0 && i + 1 < argc) engine.screenshot_dir = argv[++i];
        else if (std::strcmp(argv[i], "--touch") == 0) touch_mode = true;   // debug: mobile auto-aim on desktop
    }

    if (!engine.init("MANTLE - El Manto del Profeta")) {
        SDL_Log("FATAL: Failed to initialize engine");
        return 1;
    }

    if (script) engine.autoplay.load(script);
    if (touch_mode) {
        engine.input.auto_aim_enabled = true;
        engine.menus.settings_auto_aim = true;
        SDL_Log("--touch: mobile auto-aim enabled");
    }

    engine.run();
    engine.shutdown();

    return 0;
}
