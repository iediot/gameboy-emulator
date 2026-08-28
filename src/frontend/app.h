//
// Created by edi on 5/23/26.
//

#ifndef GAMEBOY_EMU_APP_H
#define GAMEBOY_EMU_APP_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "platform.h"
#include "core/memory.h"
#include "core/cpu.h"
#include "core/ppu.h"
#include "core/apu.h"

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

// touch overlay placement, x and y are the control's centre as a fraction of the output
// size so a layout survives a change of device, scale multiplies its default size
enum TouchControl { CTRL_DPAD, CTRL_A, CTRL_B, CTRL_START, CTRL_SELECT, CTRL_COUNT };
struct TouchPlacement { float x = 0.0f, y = 0.0f, scale = 1.0f; };

class App {
private:
    // private members
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_AudioDeviceID audio_device;
    float volume = 0.7f;
    bool cgb_enabled = true;
    bool dmg_colorize = true;
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
    std::unique_ptr<Apu> apu;
    std::vector<std::string> rom_list;
    std::vector<SDL_Texture*> cover_list;
    std::string rom_folder;      // where .gb files are read from
    std::string artwork_folder;  // where cover art .png files live
    std::string cartridge_path;  // the cartridge shell the covers sit in
    std::string trash_path;      // the delete button's icon
    SDL_Texture* trash_sprite = nullptr;
    std::string icon_light_path; // window icon for a light system theme
    std::string icon_dark_path;  // window icon for a dark system theme
    bool bundled = false;        // running from a .app, the system owns the icon then
    std::string settings_path;
    int battery_flush = 0;
    int rtc_flush = 0;    // an rtc cart is never idle, so its clock is written out rarely
    std::string save_path;       // the loaded cartridge's .sav, empty if it has no battery

    int carousel_index = 0;
    float carousel_pos = 0.0f;
    float carousel_drag_start = 0.0f;
    float carousel_target = 0.0f;
    float carousel_vel = 0.0f;
    // the library is split by cartridge type, 0 is game boy and 1 is game boy color
    int library_tab = 0;
    float tab_slide = 0.0f;      // eased position of the pill indicator
    // which shelf each rom is filed on, straight off its extension. that is how the
    // cartridge was sold, which is a different question from whether it runs in colour
    std::vector<uint8_t> rom_is_gbc;
    bool in_tab(int rom, int tab) const;
    // 0 follows the os, 1 forces light, 2 forces dark
    int theme_mode = 0;
    bool theme_dark = true;
    void apply_theme_colors();
    void sync_theme();
    void draw_iridescence(float w, float h, const SDL_Rect* keep_clear = nullptr);
    float iridescence = 0.0f;      // eased, 1 while a colour context is on screen
    void draw_library_tabs(float cx, float cy, float pill_w, float pill_h);
    int library_view(std::vector<int>& out) const;
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
    std::map<SDL_FingerID, int> touch_buttons; // live fingers to the joypad bits each one holds

    // touch overlay, see TouchPlacement, custom stays false until the editor is used
    TouchPlacement controls[CTRL_COUNT] = {};
    bool layout_custom = false;
    bool joystick_mode = false;
    SDL_FingerID stick_finger = 0;  // the finger that grabbed the stick, it keeps it until lifted
    bool stick_held = false;
    float stick_dx = 0.0f;          // live thumb offset, -1..1 of the base radius
    float stick_dy = 0.0f;
    bool editing_layout = false;
    int editing_pick = -1;
    float drag_grab_x = 0.0f;
    float drag_grab_y = 0.0f;
    bool snap_enabled = true;
    int drag_mode = 0;              // 0 none, 1 moving a control, 2 dragging the size slider
    float slider_v = 0.0f;          // knob position while dragging, free of the size clamp
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
    void load_battery_ram(const std::string& name);
    void refresh_palette();
    void save_battery_ram();
    void add_game();
    std::string mods_folder;              // one folder of patches per game
    std::string mod_rom;                  // the game the mods panel is showing
    std::vector<std::string> mod_list;
    std::vector<char> mod_on;
    bool mod_import = false;              // the pending picker is adding a patch, not a rom
    std::string mod_dir(const std::string& rom) const;
    void scan_mods(const std::string& rom);
    void save_mods();
    void add_mod();
    void draw_mods(float w, float h);
    float mods_scroll = 0.0f;
    bool mod_select = false;              // the list is picking patches to delete
    std::vector<char> mod_sel;
    bool trash_button(const char* id, float x, float y, float bw, float bh);
    void render_game();
    void handle_events();
    void setup_style();
    std::string normalize(std::string s);
    std::string closest_artwork(const std::string& rom_name, bool gbc);
    // the listing is a directory walk over more than a thousand covers, and closest
    // artwork needs it once per rom, so it is read once and kept
    const std::vector<std::string>& artwork_files(bool gbc);
    std::vector<std::string> artwork_cache_gb;
    std::vector<std::string> artwork_cache_gbc;
    std::string match_artwork(const std::string& target, bool gbc);
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
    void render_menu_mobile();
    void render_game_mobile();
    void handle_touch_mobile(const SDL_Event& event);
    void release_touches();
    bool layout_fits(int out_w, int out_h) const;
    bool fit_control(int which, int out_w, int out_h);
    void begin_layout_edit();
    void render_layout_editor();
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
