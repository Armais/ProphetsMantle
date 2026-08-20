#pragma once

// =============================================================================
// MANTLE - Menu Screens
// =============================================================================

#include "render/renderer.h"
#include "input/input.h"

enum class MenuScreen : u8 {
    Title,
    Settings,
    Controls,
    PauseMenu,
    GameOver,
    Victory
    // La Ascension ya no es un menu: se narra en GameScene::Story (ui/story.h)
};

struct MenuSystem {
    MenuScreen current = MenuScreen::Title;
    int selected_option = 0;
    int option_count = 0;

    // Continue / Settings state
    bool has_continue = false;
    int  offerings    = 0;   // Se muestra en el titulo (moneda de El Santuario)
    MenuScreen settings_origin = MenuScreen::Title;
    f32 settings_music_vol = 0.8f;
    f32 settings_sfx_vol   = 1.0f;
    bool settings_auto_aim = false;   // Mirrors Input::auto_aim_enabled (on by default on mobile)

    // Run stats shown on Game Over / Victory
    int stats_chapter = 1;
    int stats_kills   = 0;
    f32 stats_time    = 0.0f;

    void update(Input& input);
    void render(Renderer& renderer);

private:
    void render_title(Renderer& renderer);
    void render_pause(Renderer& renderer);
    void render_game_over(Renderer& renderer);
    void render_victory(Renderer& renderer);
    void render_settings(Renderer& renderer);
    void render_controls(Renderer& renderer);
};
