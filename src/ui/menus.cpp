// =============================================================================
// MANTLE - Menu System Implementation
// =============================================================================

#include "ui/menus.h"
#include "core/config.h"
#include "core/i18n.h"
#include "game/items.h"

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
    // La estampa (Elias, Eliseo y el manto, misma composicion que el icono) va sobre el
    // degradado y DEBAJO de la vineta, que es lo que la integra con el fondo.
    if (title_bg != TEXTURE_NONE)
        renderer.draw_sprite_rect(title_bg, {0, 0, 1280, 720}, {0, 0, W, H}, {255, 255, 255, 255});

    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 150});

    // Velo elipsoidal suave tras la columna del menu: los bustos flanquean los lados,
    // pero el manto cruza por detras de las ultimas filas y sin esto el texto pelea con
    // el orillo de oro. shadow_tex es justo un degradado blando, no hace falta mas.
    if (title_bg != TEXTURE_NONE && renderer.shadow_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {36, 44, 248, 144}, {8, 6, 14, 165});

    // Title at 3x with a drop shadow
    const f32 title_w = renderer.measure_text("MANTLE", 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f + 1.5f, 22.0f + 1.5f, {0, 0, 0, 170}, 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f, 22.0f, Color::elias_gold(), 3.0f);

    // Subtitle
    renderer.draw_text_centered(tr("El Manto del Profeta", "The Prophet's Mantle"), 56, {180, 160, 120, 200});
    renderer.draw_text_centered(tr("Desafía reyes. Destruye altares. Hereda el manto.", "Defy kings. Tear down altars. Inherit the mantle."), 70, {120, 105, 80, 200}, 0.75f);

    // Menu options. Con 7 filas hay que apretar el interlineado: a 11 unidades la ultima
    // se comia la linea de ofrendas.
    option_count = 7;
    const char* options[] = {tr("Nueva Partida", "New Game"), tr("Continuar", "Continue"), tr("El Santuario", "The Sanctuary"), tr("El Códice", "The Codex"),
                             tr("Prólogo", "Prologue"), tr("Opciones", "Options"), tr("Controles", "Controls")};

    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 130, 120, 255};

        // Gray out "Continuar" if no save exists
        if (i == 1 && !has_continue) {
            c = {70, 65, 60, 150};
        }

        const char* prefix = (i == selected_option) ? "> " : "  ";

        char buf[64];
        // Una partida caducada ya no se continua: lo unico que queda de ella son ofrendas.
        // La recompensa va EN la fila: entre las 7 filas, las ofrendas y la ayuda de teclas
        // no queda hueco para una linea suelta en 180 unidades de alto.
        if (i == 1 && has_continue && continue_expired) {
            snprintf(buf, sizeof(buf), tr("%sPartida abandonada  +%d", "%sAbandoned run  +%d"), prefix, abandoned_offerings);
            if (i != selected_option) c = {150, 118, 100, 235};
        } else {
            snprintf(buf, sizeof(buf), "%s%s", prefix, options[i]);
        }
        renderer.draw_text_centered(buf, 87.0f + i * 10.2f, c, 0.95f);
    }


    // Ofrendas acumuladas: es lo que se gasta en El Santuario
    if (offerings > 0) {
        char buf[48];
        snprintf(buf, sizeof(buf), tr("%d ofrendas", "%d offerings"), offerings);
        renderer.draw_text_centered(buf, 160, {200, 175, 115, 220}, 0.75f);
    }
    renderer.draw_text_centered(tr("[ENTER] Seleccionar", "[ENTER] Select"), 170, {100, 90, 80, 180}, 0.8f);
}

void MenuSystem::render_pause(Renderer& renderer) {
    // Semi-transparent overlay
    renderer.draw_rect({0, 0,
        static_cast<f32>(Config::GAME_WIDTH),
        static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 150});

    renderer.draw_text_centered(tr("PAUSA", "PAUSED"), 50, Color::white(), 2);

    option_count = 4;
    const char* options[] = {tr("Reanudar", "Resume"), tr("El Códice", "The Codex"), tr("Opciones", "Options"), tr("Salir", "Quit")};

    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{180, 180, 180, 255};
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", (i == selected_option) ? "> " : "  ", options[i]);
        renderer.draw_text_centered(buf, 80.0f + i * 14.0f, c);
    }
}

void MenuSystem::render_game_over(Renderer& renderer) {
    renderer.clear({15, 8, 8, 255});

    renderer.draw_text_centered(tr("DERROTA", "DEFEAT"), 44, {192, 57, 43, 255}, 2);
    renderer.draw_text_centered(tr("El profeta ha caído", "The prophet has fallen"), 72, {160, 120, 120, 255});

    char buf[80];
    snprintf(buf, sizeof(buf), tr("Capítulo %d  ·  %d enemigos  ·  %d:%02d", "Chapter %d  ·  %d enemies  ·  %d:%02d"),
             stats_chapter, stats_kills, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 92, {170, 150, 130, 255}, 0.9f);

    // Menu de verdad: morir y volver al menu por error costaba la partida entera
    option_count = 2;
    const char* options[] = {tr("Reintentar capítulo", "Retry chapter"), tr("Volver al menú", "Back to menu")};
    for (int i = 0; i < option_count; i++) {
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 122, 112, 255};
        char line[64];
        snprintf(line, sizeof(line), "%s%s", (i == selected_option) ? "> " : "  ", options[i]);
        renderer.draw_text_centered(line, 118.0f + i * 14.0f, c);
    }

    renderer.draw_text_centered(tr("Tu partida sigue guardada al principio del capítulo", "Your run is still saved at the chapter start"),
                                152, {125, 110, 98, 210}, 0.7f);
    renderer.draw_text_centered(tr("[W/S] elegir   [ENTER] aceptar   [R] reintentar", "[W/S] choose   [ENTER] accept   [R] retry"),
                                168, {100, 90, 80, 190}, 0.75f);
}

void MenuSystem::render_victory(Renderer& renderer) {
    renderer.clear({18, 24, 44, 255});

    renderer.draw_text_centered(tr("EL LEGADO", "THE LEGACY"), 40, {245, 225, 150, 255}, 2);
    renderer.draw_text_centered(tr("Israel ha visto la mano de Dios", "Israel has seen the hand of God"), 72, {200, 200, 220, 255});
    renderer.draw_text_centered(tr("Elías y Eliseo cumplieron su llamado", "Elijah and Elisha fulfilled their calling"), 86, {160, 170, 200, 220});

    char buf[64];
    snprintf(buf, sizeof(buf), tr("%d enemigos  ·  tiempo %d:%02d", "%d enemies  ·  time %d:%02d"),
             stats_kills, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 110, {170, 180, 200, 255}, 0.9f);
    if (victory_note)
        renderer.draw_text_centered(victory_note, 128, {190, 160, 110, 225}, 0.75f);
    renderer.draw_text_centered(tr("[ENTER] Volver al menú", "[ENTER] Back to menu"), 150, {120, 130, 150, 220});
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

    renderer.draw_text_centered(tr("OPCIONES", "OPTIONS"), 26, Color::elias_gold(), 2);

    // Las filas se montan en una TABLA de ids y no por indice fijo: la del stick derecho
    // solo existe en tactil, y sin esto anadir o quitar una fila desplazaba el significado
    // de todas las de abajo tanto al pintar como al ajustar (update_settings).
    option_count = 0;
    for (int r = 0; r < ROW_COUNT; r++) {
        if (r == ROW_RSTICK && !settings_show_rstick) continue;
        // La fila del cayado solo aparece cuando hay MAS DE UNA skin desbloqueada: una
        // fila que solo ofrece lo que ya llevas puesto no es una opcion, es ruido.
        if (r == ROW_SKIN && (settings_skins_mask & ~1u) == 0) continue;
        settings_rows[option_count++] = r;
    }
    if (selected_option >= option_count) selected_option = option_count - 1;

    // A 15 unidades de paso, 8 filas desde y=52 acaban en 157 y dejan la ayuda en 166.
    for (int i = 0; i < option_count; i++) {
        const int row = settings_rows[i];
        Color c = (i == selected_option) ? Color::elias_gold() : Color{140, 130, 120, 255};
        f32 y = 52.0f + i * 15.0f;
        const char* prefix = (i == selected_option) ? "> " : "  ";
        const char* label =
            row == ROW_MUSIC  ? tr("Música", "Music") :
            row == ROW_SFX    ? tr("Efectos", "Effects") :
            row == ROW_AIM    ? tr("Auto-mira", "Auto-aim") :
            row == ROW_SENS   ? tr("Sensibilidad", "Sensitivity") :
            row == ROW_RUMBLE ? tr("Vibración", "Rumble") :
            row == ROW_RSTICK ? tr("Stick dcho.", "Right stick") :
            row == ROW_SKIN   ? tr("Cayado", "Staff") :
            row == ROW_LANG   ? tr("Idioma", "Language") :
                                tr("Volver", "Back");
        const char* si = tr("  SÍ  ", "  ON  ");
        const char* no = tr("  NO  ", " OFF  ");
        char buf[96];

        if (row == ROW_MUSIC || row == ROW_SFX) {
            // Barra de volumen: "Etiqueta  [||||......] XX%"
            f32 v = (row == ROW_MUSIC) ? settings_music_vol : settings_sfx_vol;
            int pct = static_cast<int>(v * 100.0f + 0.5f);
            int filled = static_cast<int>(v * 10.0f + 0.5f);
            char bar[12];
            for (int b = 0; b < 10; b++) bar[b] = (b < filled) ? '|' : '.';
            bar[10] = '\0';
            snprintf(buf, sizeof(buf), "%s%-12s [%s] %3d%%", prefix, label, bar, pct);
        } else if (row == ROW_AIM) {
            // Tres estados (GDD 4.2). "Asistido" no es un punto medio decorativo: es el
            // unico modo en el que el disparo sigue siendo del jugador y aun asi perdona
            // el error de apuntar.
            const char* mode = (settings_aim_mode == 2) ? si
                             : (settings_aim_mode == 1) ? tr("ASIST.", "ASSIST") : no;
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, mode);
        } else if (row == ROW_SENS) {
            char bar[12];
            for (int b = 0; b < 10; b++) bar[b] = (b < settings_sensitivity) ? '|' : '.';
            bar[10] = '\0';
            snprintf(buf, sizeof(buf), "%s%-12s [%s] %2d", prefix, label, bar, settings_sensitivity);
        } else if (row == ROW_RUMBLE) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, settings_rumble ? si : no);
        } else if (row == ROW_RSTICK) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, settings_rstick ? si : no);
        } else if (row == ROW_SKIN) {
            StaffSkin sk = get_staff_skin(static_cast<u32>(settings_skin));
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, tr(sk.name, sk.name_en));
        } else if (row == ROW_LANG) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label,
                     I18N::language_label(static_cast<Language>(settings_language)));
        } else {
            snprintf(buf, sizeof(buf), "%s%s", prefix, label);
        }
        renderer.draw_text_centered(buf, y, c);
    }

    renderer.draw_text_centered(tr("[</>] Ajustar   [ENTER] Volver",
                                   "[</>] Adjust   [ENTER] Back"), 168, {100, 90, 80, 180});
}

void MenuSystem::render_controls(Renderer& renderer) {
    renderer.clear({15, 12, 25, 255});

    renderer.draw_text_centered(tr("CONTROLES", "CONTROLS"), 20, Color::elias_gold(), 2);

    Color label = {200, 180, 140, 255};
    Color value = {140, 130, 120, 255};

    renderer.draw_text_centered(tr("Movimiento", "Movement"), 50, label);
    renderer.draw_text_centered(tr("WASD / Flechas", "WASD / Arrows"), 62, value);

    renderer.draw_text_centered(tr("Ataque", "Attack"), 74, label);
    renderer.draw_text_centered(tr("K (mantener) — dispara hacia donde te mueves", "K (hold) — shoots where you move"), 86, value, 0.9f);

    renderer.draw_text_centered(tr("Arma secundaria", "Secondary weapon"), 100, label);
    renderer.draw_text_centered(tr("I / clic derecho — Honda o Vara, si la llevas", "I / right click — Sling or Rod, if you have one"), 112, value, 0.9f);

    renderer.draw_text_centered(tr("Dash: ESPACIO / J     Consumible: R     Orar: P", "Dash: SPACE / J     Consumable: R     Pray: P"), 130, value, 0.8f);
    renderer.draw_text_centered(tr("Milagros: Q / E / F     Mapa: TAB     Pausa: ESC", "Miracles: Q / E / F     Map: TAB     Pause: ESC"), 142, value, 0.8f);
    renderer.draw_text_centered(tr("Objetivo: limpia salas, llega al JEFE y cruza el arco", "Goal: clear rooms, reach the BOSS, cross the arch"), 156, label, 0.75f);

    renderer.draw_text_centered(tr("[ENTER/ESC] Volver", "[ENTER/ESC] Back"), 170, {100, 90, 80, 180}, 0.85f);
}
