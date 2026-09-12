#pragma once

// =============================================================================
// MANTLE - Menu Screens
// =============================================================================

#include "render/renderer.h"
#include "input/input.h"

enum class MenuScreen : u8 {
    Language,   // Selector del PRIMER arranque: se ve una vez, antes del titulo
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

    // Fondo del titulo (Elias, Eliseo y el manto). Lo carga el Engine; TEXTURE_NONE = el
    // titulo se queda con su degradado de siempre.
    u32 title_bg = TEXTURE_NONE;

    // Continue / Settings state
    bool has_continue = false;
    // Caducidad movil (GDD 4.4): a las 48 h la partida deja de poder continuarse y la fila
    // "Continuar" pasa a ofrecer las ofrendas de la partida abandonada.
    bool continue_expired     = false;
    int  abandoned_offerings  = 0;
    int  offerings    = 0;   // Se muestra en el titulo (moneda de El Santuario)
    MenuScreen settings_origin = MenuScreen::Title;
    // Espejo de los ajustes (GDD 4.2). La fuente de la verdad es SaveData (v7); el menu
    // escribe aqui y el Engine los reparte con apply_settings_from_save(). MenuSystem no
    // conoce ni el guardado ni Input a proposito: solo pinta y navega.
    f32  settings_music_vol   = 0.8f;
    f32  settings_sfx_vol     = 1.0f;
    int  settings_aim_mode    = 0;    // Config::AimMode: 0 OFF / 1 Asistido / 2 ON
    int  settings_sensitivity = 5;    // 1..10
    bool settings_rumble      = true;
    bool settings_rstick      = false;   // segundo joystick virtual (solo tactil)
    bool settings_show_rstick = false;   // esa fila solo se pinta en tactil

    // Filas de Opciones. Van por ID y no por indice fijo porque la del stick derecho solo
    // existe en tactil: sin la tabla, quitar una fila desplazaba el significado de todas
    // las de abajo, tanto al pintar como al ajustar (Engine::update_settings).
    enum SettingRow { ROW_MUSIC, ROW_SFX, ROW_AIM, ROW_SENS, ROW_RUMBLE,
                      ROW_RSTICK, ROW_SKIN, ROW_LANG, ROW_BACK, ROW_COUNT };
    int settings_rows[ROW_COUNT] = {};   // fila visible i -> SettingRow
    int settings_row_at(int i) const {
        return (i >= 0 && i < option_count) ? settings_rows[i] : ROW_BACK;
    }
    int  settings_language    = 0;    // Language: 0 ES / 1 EN
    int  settings_skin        = 0;    // skin del cayado equipada (cosmetico)
    u32  settings_skins_mask  = 1;    // bits de las skins desbloqueadas (la 0 siempre)

    // Run stats shown on Game Over / Victory
    int stats_chapter = 1;
    int stats_kills   = 0;
    f32 stats_time    = 0.0f;
    // Linea extra de la pantalla de victoria: por que no se ha visto el epilogo.
    // nullptr = se vio (o aun no aplica).
    const char* victory_note = nullptr;

    // Toque en los menus (movil). Cada render_* apunta el rect de SUS filas y `update`
    // busca ahi el tap: una sola formula, la misma que pinta. Tocar una fila la elige Y la
    // confirma; en Opciones, la mitad izquierda baja el valor y la derecha lo sube.
    static constexpr int MAX_ROW_RECTS = 9;
    Rect row_rects[MAX_ROW_RECTS] = {};
    int  row_rect_count = 0;
    MenuScreen rect_screen = MenuScreen::Title;   // pantalla a la que pertenecen los rects
    // Las lineas de ayuda hablan de teclas o de dedos. Lo pone el Engine desde
    // Input::touch_ui: "[ENTER] Seleccionar" en un telefono no ayuda a nadie.
    bool touch_hints = false;

    void update(Input& input);
    void render(Renderer& renderer);

private:
    // Apunta el rect de la fila i tal como se acaba de dibujar (y = linea, step = paso).
    void mark_row(int i, f32 y, f32 step);
    // Igual, pero con el rect exacto que se ha dibujado. Lo usan las pantallas con
    // BOTONES: la zona que responde al dedo tiene que ser la que se ve, no una franja
    // inventada de borde a borde.
    void mark_row_rect(int i, Rect r);
public:

private:
    void render_language(Renderer& renderer);
    void render_title(Renderer& renderer);
    void render_pause(Renderer& renderer);
    void render_game_over(Renderer& renderer);
    void render_victory(Renderer& renderer);
    void render_settings(Renderer& renderer);
    void render_controls(Renderer& renderer);
};
