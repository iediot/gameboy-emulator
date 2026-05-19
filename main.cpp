#include <filesystem>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
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

int main()
{
    std::string cpu_test_path = "../roms/test-roms/cpu_instrs/individual/";
    std::string ppu_test_path = "../roms/ppu-test-rom/";
    std::string game_path = "../roms/game-roms/";

    std::vector<std::string> cpu_test_roms = {
        "01-special.gb",
        "02-interrupts.gb",
        "03-op sp,hl.gb",
        "04-op r,imm.gb",
        "05-op rp.gb",
        "06-ld r,r.gb",
        "07-jr,jp,call,ret,rst.gb",
        "08-misc instrs.gb",
        "09-op r,r.gb",
        "10-bit ops.gb",
        "11-op a,(hl).gb"
    };

    std::vector<std::string> game_rom = {
        "Tetris.gb"
    };

    std::vector<std::string> ppu_test_rom = {
        "dmg-acid2.gb"
    };

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GBEmulator", // window title
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // center the window
        600, 1000, SDL_WINDOW_SHOWN); // resolution for 4x scale
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_Event event;
    IMG_Init(IMG_INIT_PNG);
    SDL_Texture* gameboy_sprite = IMG_LoadTexture(renderer, "../sprites/gameboy.png");
    SDL_Rect screen_area = {142, 129, 330, 301};

    // change for debugging or testing purposes (game_rom / cpu_test_roms / ppu_test_rom)
    for (const auto& rom_name : ppu_test_rom) {
        Memory mem;
        Cpu cpu(mem);
        Ppu ppu(mem);

        // change for debugging or testing purposes (game_path / cpu_test_path / ppu_test_path)
        std::ifstream rom_file(ppu_test_path + rom_name, std::ios::binary);

        if (!rom_file) {
            std::cerr << "Could not open: " << rom_name << "\n";
            continue;
        }

        std::vector<uint8_t> rom_data{std::istreambuf_iterator<char>(rom_file),
            std::istreambuf_iterator<char>()};
        mem.loadRom(rom_data);

        int frames_after_done = 0;

        while (true) {
            uint8_t cycles = cpu.step(); // cycling through the cpu opcodes
            ppu.step(cycles); // cycling to get the ppu rendering

            if (frames_after_done > 60) {
                /* check if the current test is done and
                 * if so it breaks out of this inner
                 * loop and moves to the next rom
                 */
                // to have time to output the result of the test
                SDL_Delay(1000);
                break;
            }

            if (ppu.frame_ready) {
                uint32_t pixels[144 * 160]; // the array containing the pixels

                for (int y = 0; y < 144; y++)
                    for (int x = 0; x < 160; x++)
                        switch (ppu.framebuffer[y][x]) {
                            /* mapping the values calculated in
                             * the ppu to the actual colors to be displayed
                             */
                            case 0: {
                                pixels[y * 160 + x] = 0xFF627102; // white (in reality green)
                                break;
                            }
                            case 1: {
                                pixels[y * 160 + x] = 0xFF4D5802; // light gray (darker shade of green)
                                break;
                            }
                            case 2: {
                                pixels[y * 160 + x] = 0xFF364002; // dark gray (even darker shade of green)
                                break;
                            }
                            case 3: {
                                pixels[y * 160 + x] = 0xFF1F2701; // black (darkest shade of green)
                                break;
                            }
                        }

                SDL_UpdateTexture(texture, nullptr, pixels, 160 * 4);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, gameboy_sprite, nullptr, nullptr);
                SDL_RenderCopy(renderer, texture, nullptr, &screen_area);
                SDL_RenderPresent(renderer);

                // delay to make the tests running actually visible (i.e., frame pacing)
                SDL_Delay(8);

                ppu.frame_ready = false;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT)
                        std::exit(0);
                }
            }
        }
    }
}