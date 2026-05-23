//
// Created by edi on 5/23/26.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "app.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

App::App() : state(AppState::MENU), selected_rom(-1), rom_folder("../roms/game-roms/") {
    // sdl
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("GBEmulator", // window title
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // center the window
        600, 1000, SDL_WINDOW_SHOWN); // resolution for 4x scale
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 160, 144);
    gameboy_sprite = IMG_LoadTexture(renderer, "../sprites/gameboy.png");
    screen_area = {142, 129, 330, 301};
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    setup_style();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    scan_roms();
}

App::~App() {
    // imgui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    // sdl
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(gameboy_sprite);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void App::scan_roms() {
    rom_list.clear();
    rom_titles.clear();
    for (const auto& entry : std::filesystem::directory_iterator(rom_folder)) {
        if (entry.path().extension() == ".gb") {
            rom_list.push_back(entry.path().filename().string());
            rom_titles.push_back(read_title(entry.path().filename().string()));
        }
    }
}

void App::load_rom(const std::string& name) {
    mem = std::make_unique<Memory>();
    cpu = std::make_unique<Cpu>(*mem);
    ppu = std::make_unique<Ppu>(*mem);

    std::ifstream rom_file(rom_folder + name, std::ios::binary);
    if (!rom_file) {
        std::cerr << "Could not open: " << name << "\n";
        return;
    }
    std::vector<uint8_t> rom_data{std::istreambuf_iterator<char>(rom_file),
        std::istreambuf_iterator<char>()};
    mem->loadRom(rom_data);

    state = AppState::PLAYING;
}

void App::run() {
    while (true) {
        handle_events();

        if (state == AppState::PLAYING) {
            // step until a frame is ready
            while (!ppu->frame_ready) {
                uint8_t cycles = cpu->step();
                ppu->step(cycles);
            }
            ppu->frame_ready = false;
            render_game();
        } else {
            render_menu();
        }
    }
}

void App::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // let imgui see every event
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            std::exit(0);

        // esc returns to menu
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE &&
            state == AppState::PLAYING) {
            state = AppState::MENU;
            continue;
            }

        // joypad only while playing
        if (state == AppState::PLAYING &&
            (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
            bool pressed = (event.type == SDL_KEYDOWN);
            switch (event.key.keysym.sym) {
                case SDLK_UP:
                    mem->set_button(2, pressed); break;
                case SDLK_DOWN:
                    mem->set_button(3, pressed); break;
                case SDLK_RIGHT:
                    mem->set_button(0, pressed); break;
                case SDLK_LEFT:
                    mem->set_button(1, pressed); break;
                case SDLK_w:
                    mem->set_button(2, pressed); break;
                case SDLK_s:
                    mem->set_button(3, pressed); break;
                case SDLK_d:
                    mem->set_button(0, pressed); break;
                case SDLK_a:
                    mem->set_button(1, pressed); break;
                case SDLK_z:
                    mem->set_button(4, pressed); break;
                case SDLK_x:
                    mem->set_button(5, pressed); break;
                case SDLK_BACKSPACE:
                    mem->set_button(6, pressed); break;
                case SDLK_RETURN:
                    mem->set_button(7, pressed); break;
            }
        }
    }
}

void App::render_game() {
    uint32_t pixels[144 * 160];
    for (int y = 0; y < 144; y++)
        for (int x = 0; x < 160; x++)
            switch (ppu->framebuffer[y][x]) {
        case 0: pixels[y * 160 + x] = 0xFF627102; break;
        case 1: pixels[y * 160 + x] = 0xFF4D5802; break;
        case 2: pixels[y * 160 + x] = 0xFF364002; break;
        case 3: pixels[y * 160 + x] = 0xFF1F2701; break;
            }

    SDL_UpdateTexture(texture, nullptr, pixels, 160 * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, gameboy_sprite, nullptr, nullptr);
    SDL_RenderCopy(renderer, texture, nullptr, &screen_area);
    SDL_RenderPresent(renderer);
    SDL_Delay(8);
}

void App::setup_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 12.0f;
    s.FrameRounding     = 8.0f;
    s.GrabRounding      = 8.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.FramePadding  = ImVec2(14, 10);
    s.ItemSpacing   = ImVec2(10, 10);
    s.WindowPadding = ImVec2(20, 20);
    s.FrameBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]      = ImVec4(0.09f, 0.10f, 0.06f, 1.00f);
    c[ImGuiCol_Button]        = ImVec4(0.24f, 0.28f, 0.02f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.40f, 0.02f, 1.00f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.18f, 0.21f, 0.02f, 1.00f);
    c[ImGuiCol_Text]          = ImVec4(0.90f, 0.93f, 0.78f, 1.00f);
    c[ImGuiCol_TitleBg]       = ImVec4(0.12f, 0.14f, 0.02f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.21f, 0.02f, 1.00f);
}

std::string App::read_title(const std::string& filename) {
    std::ifstream file(rom_folder + filename, std::ios::binary);
    if (!file) {
        return filename;
    }
    file.seekg(0x0134);
    char buffer[17] = {};
    file.read(buffer, 16);
    std::string title(buffer);
    if (title.empty())
        return filename;
    return title;
}

void App::render_menu() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(600, 1000));
    ImGui::Begin("gameboy-emu", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    for (size_t i = 0; i < rom_list.size(); i++) {
        if (ImGui::Button(rom_titles[i].c_str(), ImVec2(-1, 0)))
            load_rom(rom_list[i]);
    }
    ImGui::End();
    ImGui::Render();
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}