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

#ifdef __APPLE__
#include <SDL.h>
#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

enum class AppState { MENU, PLAYING };

class App
{
private:
    // private members
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Texture* gameboy_sprite;
    SDL_Rect screen_area;
    AppState state;
    std::unique_ptr<Memory> mem;
    std::unique_ptr<Cpu> cpu;
    std::unique_ptr<Ppu> ppu;
    std::vector<std::string> rom_list;
    std::vector<SDL_Texture*> cover_list;
    int selected_rom;
    int carousel_index = 0;      // currently framed cover on the ios menu
    std::string rom_folder;      // where .gb files are read from
    std::string artwork_folder;  // where cover art .png files live
    std::string sprite_path;     // the gameboy bezel sprite
#if GB_IOS
    std::map<SDL_FingerID, int> touch_buttons; // live fingers to the joypad bit each one holds
    bool active = true; // false while backgrounded, we must not touch the gpu then
#endif

    // private methods
    void init_paths();
    void scan_roms();
    void load_rom(const std::string& name);
    void render_game();
    void handle_events();
    void setup_style();
    std::string normalize(std::string s);
    std::string closest_artwork(const std::string& rom_name);
    std::string display_name(const std::string& s);
    void render_menu();
#if GB_IOS
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
};

#endif //GAMEBOY_EMU_APP_H