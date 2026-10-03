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
    TutorialAsk,   // "¿Quieres el tutorial?" — sale al pulsar Nueva Partida (game/tutorial.h)
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
    // Hay algo que se puede comprar YA en El Santuario: el boton lleva una marca dorada.
    // Lo rellena el Engine (refresh_continue_state); el menu no conoce la progresion.
    bool sanctuary_ready = false;
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
    // Primera fila de la ventana. Solo en tactil: alli la placa mide 19 unidades de alto
    // para que se pueda tocar y las nueve filas ya no caben en la pantalla.
    int settings_scroll = 0;
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
    // `touch_hints` es ademas la marca de "estamos en un telefono" para TODA la capa: en
    // movil no se pinta ningun cursor, porque no se navega, se toca el boton que se quiere.
    bool touch_hints = false;
    // El boton que hay bajo el dedo se ilumina mientras el dedo sigue apoyado. Lo escribe
    // el Engine desde Input::ui_press, y es lo que sustituye al resaltado de seleccion.
    bool press_active = false;
    Vec2 press_pos    = {0, 0};
    // Se esta jugando el tutorial: la ultima fila de la pausa deja de ser "Salir" y pasa a
    // ser la salida del tutorial, que en PC es la unica (ESC ya es la pausa que enseña).
    bool in_tutorial  = false;

    void update(Input& input);
    void render(Renderer& renderer);

    // Reparto vertical de una columna de botones. Hay dos por pantalla porque el telefono
    // pide el doble de alto por boton que el escritorio y el sitio es el mismo.
    struct Layout { f32 top, pitch, h; };

private:
    // Pinta una columna de botones, apunta sus zonas tactiles e ilumina la que este bajo
    // el dedo. La comparten Idioma, Pausa y Derrota, que solo se diferencian en el reparto.
    void draw_button_column(Renderer& renderer, const char* const* labels, int n,
                            Layout pc, Layout touch, f32 text_scale, int disabled_idx = -1);
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
    void render_tutorial_ask(Renderer& renderer);
};
