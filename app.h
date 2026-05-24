//
// Created by edi on 5/23/26.
//

#ifndef GAMEBOY_EMU_APP_H
#define GAMEBOY_EMU_APP_H

#include <memory>
#include <string>
#include <vector>
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
    std::string rom_folder;

    // private methods
    void scan_roms();
    void load_rom(const std::string& name);
    void render_game();
    void handle_events();
    void setup_style();
    std::string normalize(std::string s);
    std::string closest_artwork(const std::string& rom_name);
    std::string display_name(const std::string& s);
    void render_menu();
public:
    // constructor
    App();
    // destructor
    ~App();
    // run method
    void run();
};

#endif //GAMEBOY_EMU_APP_H