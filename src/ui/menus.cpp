// =============================================================================
// MANTLE - Menu System Implementation
// =============================================================================

#include "ui/menus.h"
#include "core/config.h"

void MenuSystem::update(Input& input) {
    if (input.is_action_just_pressed(Action::MoveUp)) {
        selected_option = (selected_option - 1 + option_count) % std::max(1, option_count);
    }
    if (input.is_action_just_pressed(Action::MoveDown)) {
        selected_option = (selected_option + 1) % std::max(1, option_count);
    }
}

void MenuSystem::render(Renderer& renderer) {
    switch (current) {
        case MenuScreen::Title:     render_title(renderer); break;
        case MenuScreen::PauseMenu: render_pause(renderer); break;
        case MenuScreen::GameOver:  render_game_over(renderer); break;
        case MenuScreen::Victory:   render_victory(renderer); break;
        case MenuScreen::Settings:  render_settings(renderer); break;
        case MenuScreen::Controls:  render_controls(renderer); break;
        default: break;
    }
}

void MenuSystem::render_title(Renderer& renderer) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);

    // Vertical gradient (warm ember at the top, night at the bottom) instead of a flat fill
    renderer.clear({10, 8, 16, 255});
    constexpr int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        f32 k = 1.0f - static_cast<f32>(i) / BANDS;
        renderer.draw_rect({0, H * i / BANDS, W, H / BANDS + 0.5f},
                           {static_cast<u8>(58 * k), static_cast<u8>(30 * k), static_cast<u8>(34 * k), 255});
    }
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 150});

    // Title at 3x with a drop shadow
    const f32 title_w = renderer.measure_text("MANTLE", 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f + 1.5f, 22.0f + 1.5f, {0, 0, 0, 170}, 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f, 22.0f, Color::elias_gold(), 3.0f);

    // Subtitle
    renderer.draw_text_centered("El Manto del Profeta", 56, {180, 160, 120, 200});
    renderer.draw_text_centered("Desafía reyes. Destruye altares. Hereda el manto.", 70, {120, 105, 80, 200}, 0.75f);

    // Menu options
    option_count = 6;
    const char* options[] = {"Nueva Partida", "Continuar", "El Santuario", "El Códice",
                             "Opciones", "Controles"};

    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 130, 120, 255};

        // Gray out "Continuar" if no save exists
        if (i == 1 && !has_continue) {
            c = {70, 65, 60, 150};
        }

        const char* prefix = (i == selected_option) ? "> " : "  ";

        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", prefix, options[i]);
        renderer.draw_text_centered(buf, 90.0f + i * 11.0f, c, 0.95f);
    }

    // Ofrendas acumuladas: es lo que se gasta en El Santuario
    if (offerings > 0) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%d ofrendas", offerings);
        renderer.draw_text_centered(buf, 158, {200, 175, 115, 220}, 0.75f);
    }
    renderer.draw_text_centered("[ENTER] Seleccionar", 168, {100, 90, 80, 180}, 0.8f);
}

void MenuSystem::render_pause(Renderer& renderer) {
    // Semi-transparent overlay
    renderer.draw_rect({0, 0,
        static_cast<f32>(Config::GAME_WIDTH),
        static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 150});

    renderer.draw_text_centered("PAUSA", 50, Color::white(), 2);

    option_count = 4;
    const char* options[] = {"Reanudar", "El Códice", "Opciones", "Salir"};

    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{180, 180, 180, 255};
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", (i == selected_option) ? "> " : "  ", options[i]);
        renderer.draw_text_centered(buf, 80.0f + i * 14.0f, c);
    }
}

void MenuSystem::render_game_over(Renderer& renderer) {
    renderer.clear({15, 8, 8, 255});

    renderer.draw_text_centered("DERROTA", 44, {192, 57, 43, 255}, 2);
    renderer.draw_text_centered("El profeta ha caído", 72, {160, 120, 120, 255});

    char buf[80];
    snprintf(buf, sizeof(buf), "Capítulo %d  ·  %d enemigos  ·  %d:%02d",
             stats_chapter, stats_kills, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 92, {170, 150, 130, 255}, 0.9f);

    // Menu de verdad: morir y volver al menu por error costaba la partida entera
    option_count = 2;
    const char* options[] = {"Reintentar capítulo", "Volver al menú"};
    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 122, 112, 255};
        char line[64];
        snprintf(line, sizeof(line), "%s%s", (i == selected_option) ? "> " : "  ", options[i]);
        renderer.draw_text_centered(line, 118.0f + i * 14.0f, c);
    }

    renderer.draw_text_centered("Tu partida sigue guardada al principio del capítulo",
                                152, {125, 110, 98, 210}, 0.7f);
    renderer.draw_text_centered("[W/S] elegir   [ENTER] aceptar   [R] reintentar",
                                168, {100, 90, 80, 190}, 0.75f);
}

void MenuSystem::render_victory(Renderer& renderer) {
    renderer.clear({18, 24, 44, 255});

    renderer.draw_text_centered("EL LEGADO", 40, {245, 225, 150, 255}, 2);
    renderer.draw_text_centered("Israel ha visto la mano de Dios", 72, {200, 200, 220, 255});
    renderer.draw_text_centered("Elías y Eliseo cumplieron su llamado", 86, {160, 170, 200, 220});

    char buf[64];
    snprintf(buf, sizeof(buf), "%d enemigos  ·  tiempo %d:%02d",
             stats_kills, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 110, {170, 180, 200, 255}, 0.9f);
    renderer.draw_text_centered("[ENTER] Volver al menú", 150, {120, 130, 150, 220});
}

void MenuSystem::render_settings(Renderer& renderer) {
    // Background depends on origin
    if (settings_origin == MenuScreen::PauseMenu) {
        // Semi-transparent overlay (game is visible behind)
        renderer.draw_rect({0, 0,
            static_cast<f32>(Config::GAME_WIDTH),
            static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 180});
    } else {
        renderer.clear({15, 12, 25, 255});
    }

    renderer.draw_text_centered("OPCIONES", 30, Color::elias_gold(), 2);

    option_count = 4;
    const char* labels[] = {"Música", "Efectos", "Auto-mira", "Volver"};
    f32 volumes[] = {settings_music_vol, settings_sfx_vol, 0.0f, 0.0f};

    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 130, 120, 255};
        f32 y = 66.0f + i * 16.0f;

        if (i == 2) {
            // Auto-aim toggle (on by default on mobile, off on desktop)
            char buf[64];
            snprintf(buf, sizeof(buf), "%s%-8s [%s]",
                     (i == selected_option) ? "> " : "  ",
                     labels[i], settings_auto_aim ? "  SI  " : "  NO  ");
            renderer.draw_text_centered(buf, y, c);
        } else if (i < 2) {
            // Volume bar: "Label  [||||......] XX%"
            int pct = static_cast<int>(volumes[i] * 100.0f + 0.5f);
            int filled = static_cast<int>(volumes[i] * 10.0f + 0.5f);

            char bar[12];
            for (int b = 0; b < 10; b++) {
                bar[b] = (b < filled) ? '|' : '.';
            }
            bar[10] = '\0';

            char buf[64];
            snprintf(buf, sizeof(buf), "%s%-8s [%s] %3d%%",
                     (i == selected_option) ? "> " : "  ",
                     labels[i], bar, pct);
            renderer.draw_text_centered(buf, y, c);
        } else {
            // "Volver" option
            char buf[32];
            snprintf(buf, sizeof(buf), "%s%s",
                     (i == selected_option) ? "> " : "  ", labels[i]);
            renderer.draw_text_centered(buf, y, c);
        }
    }

    renderer.draw_text_centered("[</>] Ajustar   [ENTER] Volver", 140, {100, 90, 80, 180});
}

void MenuSystem::render_controls(Renderer& renderer) {
    renderer.clear({15, 12, 25, 255});

    renderer.draw_text_centered("CONTROLES", 20, Color::elias_gold(), 2);

    Color label = {200, 180, 140, 255};
    Color value = {140, 130, 120, 255};

    renderer.draw_text_centered("Movimiento", 50, label);
    renderer.draw_text_centered("WASD / Flechas", 62, value);

    renderer.draw_text_centered("Ataque", 74, label);
    renderer.draw_text_centered("K (mantener) — dispara hacia donde te mueves", 86, value, 0.9f);

    renderer.draw_text_centered("Arma secundaria", 100, label);
    renderer.draw_text_centered("I / clic derecho — Honda o Vara, si la llevas", 112, value, 0.9f);

    renderer.draw_text_centered("Dash: ESPACIO / J     Consumible: R     Orar: P", 130, value, 0.8f);
    renderer.draw_text_centered("Milagros: Q / E / F     Mapa: TAB     Pausa: ESC", 142, value, 0.8f);
    renderer.draw_text_centered("Objetivo: limpia salas, llega al JEFE y cruza el arco", 156, label, 0.75f);

    renderer.draw_text_centered("[ENTER/ESC] Volver", 170, {100, 90, 80, 180}, 0.85f);
}
