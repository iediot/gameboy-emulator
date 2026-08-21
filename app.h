//
// Created by edi on 5/23/26.
//

#ifndef GAMEBOY_EMU_APP_H
#define GAMEBOY_EMU_APP_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include "platform.h"
#include "memory.h"
#include "cpu.h"
#include "ppu.h"

#if defined(__APPLE__) || defined(__ANDROID__)
#include <SDL.h>
#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

enum class AppState { MENU, PLAYING };

inline constexpr int         kFpsCaps[6]  = {30, 60, 120, 144, 240, 0};
inline constexpr const char* kFpsNames[6] = {"30", "60", "120", "144", "240", "unlimited"};
inline constexpr double      kGbFps       = 59.7275;

enum class ScaleMode { NORMAL, CROP, STRETCH };

class App
{
private:
    // private members
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Texture* gameboy_sprite;
    SDL_Texture* cartridge_sprite;
    SDL_Texture* cartridge_shadow;
    SDL_Texture* rect_shadow;
    float shadow_pad_x = 0.0f;
    float shadow_pad_y = 0.0f;
    float rect_pad = 0.0f;
    AppState state;
    std::unique_ptr<Memory> mem;
    std::unique_ptr<Cpu> cpu;
    std::unique_ptr<Ppu> ppu;
    std::vector<std::string> rom_list;
    std::vector<SDL_Texture*> cover_list;
    int selected_rom;
    std::string rom_folder;      // where .gb files are read from
    std::string artwork_folder;  // where cover art .png files live
    std::string sprite_path;     // the gameboy bezel sprite
    std::string cartridge_path;  // the cartridge shell the covers sit in
    std::string icon_light_path; // window icon for a light system theme
    std::string icon_dark_path;  // window icon for a dark system theme
    bool bundled = false;        // running from a .app, the system owns the icon then
    std::string settings_path;   // where the scale mode, frame cap and keybinds are stored

    int carousel_index = 0;
    float carousel_pos = 0.0f;
    float carousel_drag_start = 0.0f;
    float carousel_target = 0.0f;
    float carousel_vel = 0.0f;
    bool show_debug = false;
    ScaleMode scale_mode = ScaleMode::NORMAL;
    bool settings_open = false;
    int settings_tab = 0;
    int rebind_target = -1;
    float settings_scroll = 0.0f;
    int fps_index = 1;
    bool vsync = true;
    bool hidpi = false;
    bool render_cartridge = true;
    bool video_reset = false;
    int win_w = 1280;
    int win_h = 720;
    int win_x = 0;
    int win_y = 0;
    bool have_win_pos = false;
    uint64_t last_present = 0;
    bool in_live_resize = false;
    SDL_Keycode keybinds[8];
#if GB_MOBILE
    std::map<SDL_FingerID, int> touch_buttons; // live fingers to the joypad bit each one holds
    bool active = true;             // false while backgrounded, we must not touch the gpu then
    std::vector<std::string> import_prev; // rom_list snapshot taken when the add-game picker opens
#endif

    // private methods
    void init_paths();
    void create_video();
    void destroy_video();
    void build_shadow();
    void load_settings();
    void save_settings();
    void scan_roms();
    void load_rom(const std::string& name);
    void add_game();
    void render_game();
    void handle_events();
    void setup_style();
    std::string normalize(std::string s);
    std::string closest_artwork(const std::string& rom_name);
    std::vector<std::string> artwork_files();
#if GB_MOBILE
    std::vector<std::string> bundled_roms();
    void copy_bundled_rom(const std::string& name);
#endif
    std::string display_name(const std::string& s);
    void render_menu();
    void pace(double fps);
    bool cog_button(float cx, float cy, float r);
    bool back_button(float cx, float cy, float r);
    void draw_settings(float w, float h);
#if GB_MOBILE
    // ios-only layout and touch input, implemented in ios_ui.cpp
    void render_menu_ios();
    void render_game_ios();
    void handle_touch_ios(const SDL_Event& event);
#endif
public:
    // constructor
    App();
    // destructor
    ~App();
    // run method
    void run();
    void live_resize();
};

#endif //GAMEBOY_EMU_APP_H