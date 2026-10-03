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
//   --window WxH      force the window size (20:9 phone: 2400x1080, portrait: 540x960)
//   --dpi N           pretend the screen has N dots per inch (touch control size by DPI)
//
// =============================================================================

#include "core/engine.h"
// SDL_main.h hace `#define main SDL_main`. En Android es obligatorio: SDLActivity busca el
// simbolo SDL_main por dlsym dentro de libmain.so y muere si no lo encuentra.
#include <SDL3/SDL_main.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Estatico: con la zona continua un Room son ~15 KB (160x88 tiles) y el Engine lleva
    // tres (la activa y las dos ranuras de cache). Fuera de la pila.
    static Engine engine;

    const char* script = nullptr;
    bool touch_mode = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) script = argv[++i];
        else if (std::strcmp(argv[i], "--shots") == 0 && i + 1 < argc) engine.screenshot_dir = argv[++i];
        else if (std::strcmp(argv[i], "--touch") == 0) touch_mode = true;   // debug: mobile auto-aim on desktop
        // `--window 540x960`: ventana de proporcion forzada. Existe para poder probar la
        // orientacion VERTICAL del GDD 4.5 sin un telefono delante.
        // `--dpi 420`: finge la densidad de un telefono para probar el tamaño de los
        // controles tactiles por DPI en un escritorio (Engine::touch_dpi_scale).
        else if (std::strcmp(argv[i], "--dpi") == 0 && i + 1 < argc) engine.forced_dpi = static_cast<f32>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--window") == 0 && i + 1 < argc) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                engine.forced_win_w = w;
                engine.forced_win_h = h;
            }
        }
    }

    if (!engine.init("MANTLE - El Manto del Profeta")) {
        SDL_Log("FATAL: Failed to initialize engine");
        return 1;
    }

    // Con --script no hay prologo: los guiones de scripts/ esperan el menu al arrancar.
    // Va aqui y no al final de Engine::init porque el guion se carga DESPUES de init.
    if (script) engine.autoplay.load(script);
    // Primer arranque: manda el selector de idioma, y el prologo espera a que se conteste.
    else if (engine.menus.current == MenuScreen::Language) engine.intro_after_language = true;
    else        engine.start_intro();
    if (touch_mode) {
        engine.input.aim_mode = Config::AimMode::On;
        engine.menus.settings_aim_mode = static_cast<int>(Config::AimMode::On);
        // Tambien las demas reglas de movil: controles tactiles en pantalla y caducidad
        // de la partida a las 48 h
        engine.input.touch_ui = true;
        engine.mobile_rules = true;
        engine.refresh_continue_state();
        SDL_Log("--touch: mobile rules enabled (auto-aim, caducidad 48 h)");
    }

    engine.run();
    engine.shutdown();

    return 0;
}
