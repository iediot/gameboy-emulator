//
// Created by edi on 5/23/26.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include "platform.h" // first so GB_DESKTOP is defined before the guard below
#if GB_DESKTOP
#include <nfd.h> // native desktop file dialog, no ios equivalent
#endif
#include "app.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

static const int   kFpsCaps[6]  = {30, 60, 120, 144, 240, 0};
static const char* kFpsNames[6] = {"30", "60", "120", "144", "240", "unlimited"};
static const double kGbFps = 59.7275;

#if GB_DESKTOP
static int SDLCALL resize_watch(void* data, SDL_Event* e) {
    if (e->type == SDL_WINDOWEVENT &&
        (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         e->window.event == SDL_WINDOWEVENT_EXPOSED))
        ((App*)data)->live_resize();
    return 0;
}
#endif

// constructor
App::App() : state(AppState::MENU), selected_rom(-1) {
    // sdl
    SDL_Init(SDL_INIT_VIDEO);
    init_paths();
    Uint32 win_flags = SDL_WINDOW_SHOWN;
    int win_w = 600, win_h = 1000;
#if GB_IOS
    win_flags |= SDL_WINDOW_ALLOW_HIGHDPI; // back the renderer at native pixels, not an upscaled buffer
#endif
#if GB_DESKTOP
    win_flags |= SDL_WINDOW_RESIZABLE;
    win_w = 1280;
    win_h = 720;
#endif
    window = SDL_CreateWindow("GBEmulator", // window title
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // center the window
        win_w, win_h, win_flags);
#if GB_DESKTOP
    SDL_SetWindowMinimumSize(window, 480, 360);
#endif
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest); // keep the gameboy pixels sharp when scaled up
    gameboy_sprite = nullptr;
#if GB_IOS
    gameboy_sprite = IMG_LoadTexture(renderer, sprite_path.c_str());
#endif
    keybinds[0] = SDLK_RIGHT;
    keybinds[1] = SDLK_LEFT;
    keybinds[2] = SDLK_UP;
    keybinds[3] = SDLK_DOWN;
    keybinds[4] = SDLK_z;
    keybinds[5] = SDLK_x;
    keybinds[6] = SDLK_BACKSPACE;
    keybinds[7] = SDLK_RETURN;
    load_settings();
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    setup_style();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
#if GB_DESKTOP
    // nfd
    NFD_Init();
    SDL_AddEventWatch(resize_watch, this);
#endif

    last_present = SDL_GetPerformanceCounter();
    scan_roms();
}

void App::pace(double fps) {
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t now = SDL_GetPerformanceCounter();
    if (fps <= 0.0) {
        last_present = now;
        return;
    }
    uint64_t target = last_present + (uint64_t)((double)freq / fps);
    while (now < target) {
        uint64_t left_ms = ((target - now) * 1000) / freq;
        if (left_ms > 1) SDL_Delay((Uint32)(left_ms - 1));
        now = SDL_GetPerformanceCounter();
    }
    last_present = (now > target + freq / 10) ? now : target;
}

// macos runs a modal loop while the window is being dragged, so the main loop is
// blocked, the event watch fires inside that loop and lets us keep drawing
void App::live_resize() {
#if GB_DESKTOP
    if (in_live_resize) return;
    in_live_resize = true;
    if (state == AppState::PLAYING) render_game();
    else                            render_menu();
    in_live_resize = false;
#endif
}

// resolve rom, cover and sprite paths per platform, ios reads them from the app bundle
void App::init_paths() {
#if GB_IOS
    char* base = SDL_GetBasePath();
    char* pref = SDL_GetPrefPath("com.iediot", "gbemu");
    std::string b = base ? base : "";
    std::string p = pref ? pref : "";
    if (base) SDL_free(base);
    if (pref) SDL_free(pref);
    sprite_path    = b + "emu-sprite.png"; // full-res ios bezel, the old gameboy.png stays unused
    artwork_folder = b + "artworks/";      // read-only, shipped in the bundle
    rom_folder     = p + "game-roms/";     // writable copy so roms can be added and deleted
    settings_path  = p + "settings.txt";

    // on first launch seed the writable folder with the roms shipped in the bundle
    std::error_code ec;
    std::filesystem::create_directories(rom_folder, ec);
    if (std::filesystem::is_empty(rom_folder, ec)) {
        for (const auto& e : std::filesystem::directory_iterator(b + "game-roms/", ec))
            if (e.path().extension() == ".gb")
                std::filesystem::copy_file(e.path(), rom_folder + e.path().filename().string(),
                                           std::filesystem::copy_options::skip_existing, ec);
    }
#else
    sprite_path    = "../sprites/gameboy.png";
    artwork_folder = "../artworks/";
    rom_folder     = "../roms/game-roms/";

    char* pref = SDL_GetPrefPath("com.iediot", "gbemu");
    settings_path = std::string(pref ? pref : "") + "settings.txt";
    if (pref) SDL_free(pref);
#endif
}

void App::load_settings() {
    std::ifstream f(settings_path);
    if (!f) return;
    std::string key;
    while (f >> key) {
        if (key == "scale") {
            int v; if (f >> v && v >= 0 && v <= 2) scale_mode = (ScaleMode)v;
        } else if (key == "fps") {
            int v; if (f >> v && v >= 0 && v <= 5) fps_index = v;
        } else if (key == "key") {
            int i; long long v;
            if (f >> i >> v && i >= 0 && i < 8) keybinds[i] = (SDL_Keycode)v;
        } else {
            std::string skip;
            std::getline(f, skip);
        }
    }
}

void App::save_settings() {
    std::ofstream f(settings_path, std::ios::trunc);
    if (!f) return;
    f << "scale " << (int)scale_mode << "\n";
    f << "fps " << fps_index << "\n";
    for (int i = 0; i < 8; i++)
        f << "key " << i << " " << (long long)keybinds[i] << "\n";
}

// destructor
App::~App() {
#if GB_DESKTOP
    SDL_DelEventWatch(resize_watch, this);
#endif
    // imgui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    // cover list cleanup
    for (SDL_Texture* cover : cover_list)
        if (cover)
            SDL_DestroyTexture(cover);
#if GB_DESKTOP
    // nfd
    NFD_Quit();
#endif
    // sdl
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(gameboy_sprite);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// run the cpu and ppu in lockstep
void App::run() {
    AppState prev_state = state;
    while (true) {
        handle_events();

#if GB_IOS
        // ios forbids gpu work in the background, so pause the whole loop until we return
        if (!active) {
            SDL_Delay(150);
            continue;
        }
#endif

        if (state == AppState::PLAYING) {
            // step until a frame is ready
            uint64_t frame_start = cpu->total_cycles;
            while (!ppu->frame_ready && cpu->total_cycles - frame_start < 70224) {
                cpu->step();
            }
            ppu->frame_ready = false;
            render_game();
        } else {
            render_menu();
        }

        if (state != prev_state) {
            ImGui::GetIO().ClearInputKeys();
            prev_state = state;
        }
    }
}

// find the games inside the game path and the closest matching cover for each
void App::scan_roms() {
    rom_list.clear();
    cover_list.clear();
    for (const auto& entry : std::filesystem::directory_iterator(rom_folder)) {
        if (entry.path().extension() == ".gb") {
            rom_list.push_back(entry.path().filename().string());
            std::string path = closest_artwork(entry.path().stem().string());
            if (path.empty())
                cover_list.push_back(nullptr);
            else {
                SDL_Texture* cover = IMG_LoadTexture(renderer, path.c_str());
                if (cover) SDL_SetTextureBlendMode(cover, SDL_BLENDMODE_BLEND); // so alpha fades every cover, not just ones shipping an alpha channel
                cover_list.push_back(cover);
            }
        }
    }
}

// loads the rom, moved from main
void App::load_rom(const std::string& name) {
    // rebuild the emulator
    mem = std::make_unique<Memory>();
    ppu = std::make_unique<Ppu>(*mem);
    cpu = std::make_unique<Cpu>(*mem, *ppu);

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

// the renderer of the games inside the actual emulator
void App::render_game() {
#if GB_IOS
    render_game_ios(); // letterboxed layout lives in ios_ui.cpp
    return;
#endif
    uint32_t pixels[144 * 160];
    for (int y = 0; y < 144; y++)
        for (int x = 0; x < 160; x++)
            // the palette is some kind of olive for a more nostalgic feeling
                switch (ppu->framebuffer[y][x]) {
        case 0:
                    pixels[y * 160 + x] = 0xFF627102; break; // darkest shade
        case 1:
                    pixels[y * 160 + x] = 0xFF4D5802; break; // slightly lighter shade
        case 2:
                    pixels[y * 160 + x] = 0xFF364002; break; // lighter shade
        case 3:
                    pixels[y * 160 + x] = 0xFF1F2701; break; // light shade
                }

    SDL_UpdateTexture(texture, nullptr, pixels, 160 * 4);

    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);

    SDL_Rect dst;
    if (scale_mode == ScaleMode::STRETCH) {
        dst = {0, 0, out_w, out_h};
    } else {
        float sx = (float)out_w / 160.0f;
        float sy = (float)out_h / 144.0f;
        float s = (scale_mode == ScaleMode::CROP) ? std::max(sx, sy) : std::min(sx, sy);
        int dw = (int)std::lround(160 * s);
        int dh = (int)std::lround(144 * s);
        dst = {(out_w - dw) / 2, (out_h - dh) / 2, dw, dh};
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##overlay", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    draw_settings(io.DisplaySize.x, io.DisplaySize.y);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

    SDL_RenderPresent(renderer);
    if (!in_live_resize) pace(kGbFps);
}

void App::add_game() {
#if GB_DESKTOP
    nfdchar_t* path = nullptr;
    nfdfilteritem_t filter[1] = {{"Game Boy ROM", "gb"}};
    if (NFD_OpenDialog(&path, filter, 1, nullptr) == NFD_OKAY) {
        std::filesystem::copy_file(path,
            rom_folder + std::filesystem::path(path).filename().string(),
            std::filesystem::copy_options::overwrite_existing);
        NFD_FreePath(path);
        scan_roms();
        carousel_pos = carousel_target = 0.0f;
        carousel_vel = 0.0f;
    }
#endif
}

bool App::cog_button(float cx, float cy, float r) {
    ImGui::SetCursorScreenPos(ImVec2(cx - r, cy - r));
    bool clicked = ImGui::InvisibleButton("##settings", ImVec2(r * 2.0f, r * 2.0f));
    bool hot = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 green = hot ? IM_COL32(87, 102, 5, 255) : IM_COL32(61, 71, 5, 255);
    ImU32 white = IM_COL32(255, 255, 255, 255);

    dl->AddCircleFilled(ImVec2(cx, cy), r, green, 40);
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 3.14159265f / 4.0f;
        float tx = cx + std::cos(a) * r * 0.42f;
        float ty = cy + std::sin(a) * r * 0.42f;
        dl->AddCircleFilled(ImVec2(tx, ty), r * 0.11f, white, 10);
    }
    dl->AddCircle(ImVec2(cx, cy), r * 0.34f, white, 24, r * 0.14f);
    dl->AddCircleFilled(ImVec2(cx, cy), r * 0.12f, green, 16);
    return clicked;
}

bool App::back_button(float cx, float cy, float r) {
    ImGui::SetCursorScreenPos(ImVec2(cx - r, cy - r));
    bool clicked = ImGui::InvisibleButton("##back", ImVec2(r * 2.0f, r * 2.0f));
    bool hot = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 green = hot ? IM_COL32(87, 102, 5, 255) : IM_COL32(61, 71, 5, 255);
    ImU32 white = IM_COL32(255, 255, 255, 255);

    dl->AddCircleFilled(ImVec2(cx, cy), r, green, 40);
    float s = r * 0.30f;
    dl->PathLineTo(ImVec2(cx + s * 0.6f, cy - s));
    dl->PathLineTo(ImVec2(cx - s * 0.6f, cy));
    dl->PathLineTo(ImVec2(cx + s * 0.6f, cy + s));
    dl->PathStroke(white, 0, r * 0.14f);
    return clicked;
}

void App::draw_settings(float w, float h) {
    float r = std::max(14.0f, std::min(w, h) * 0.030f);
    float m = r * 1.4f;
    if (cog_button(m + r, h - m - r, r)) {
        settings_open = true;
        ImGui::OpenPopup("settings");
    }
    if (state == AppState::PLAYING && back_button(m + r, m + r, r))
        state = AppState::MENU;

    const char* tabs[2] = {"display", "keybinds"};
    float pad = 22.0f, tab_h = 32.0f, tab_gap = 8.0f, close_h = 40.0f;
    float tab_w = std::max(ImGui::CalcTextSize(tabs[0]).x, ImGui::CalcTextSize(tabs[1]).x) + 34.0f;

    float row_h = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    int rows = 5;
    float pw = std::min(w * 0.62f, 440.0f);
    float ph = std::min(tab_h + pad * 3.0f + rows * row_h + close_h, h * 0.85f);
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    if (ImGui::BeginPopupModal("settings", &settings_open,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBackground)) {

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int i = 0; i < 2; i++) {
            float tx = wp.x + 26.0f + i * (tab_w + tab_gap);
            ImGui::SetCursorScreenPos(ImVec2(tx, wp.y));
            ImGui::PushID(i);
            bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tab_w, tab_h));
            bool hot = ImGui::IsItemHovered();
            ImGui::PopID();
            if (clicked) settings_tab = i;

            bool on = (settings_tab == i);
            ImU32 col = on  ? IM_COL32(87, 102, 5, 255)
                      : hot ? IM_COL32(72, 84, 5, 255)
                            : IM_COL32(48, 56, 4, 255);
            dl->AddRectFilled(ImVec2(tx, wp.y), ImVec2(tx + tab_w, wp.y + tab_h + 26.0f),
                              col, 12.0f, ImDrawFlags_RoundCornersTop);
            ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
            dl->AddText(ImVec2(tx + (tab_w - ts.x) * 0.5f, wp.y + (tab_h - ts.y) * 0.5f),
                        IM_COL32(0xE6, 0xED, 0xC7, 255), tabs[i]);
        }

        dl->AddRectFilled(ImVec2(wp.x, wp.y + tab_h), ImVec2(wp.x + ws.x, wp.y + ws.y),
                          ImGui::GetColorU32(ImGuiCol_WindowBg), 22.0f);

        float inner_w = ws.x - pad * 2.0f;
        float body_y  = tab_h + pad;
        float body_h  = ws.y - body_y - pad * 2.0f - close_h;

        ImGui::SetCursorPos(ImVec2(pad, body_y));
        ImGui::BeginChild("settings_body", ImVec2(inner_w, body_h), false);
        float label_w = ImGui::GetContentRegionAvail().x * 0.35f;
        float ctrl_gap = 18.0f;
        if (settings_tab == 0) {
            bool any_open = false;
            auto combo_row = [&](const char* text, const char* id,
                                 const char* const* items, int n, int cur) {
                int picked = cur;
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine(label_w);

                float cw = ImGui::GetContentRegionAvail().x - ctrl_gap;
                float chh = ImGui::GetFrameHeight();
                ImVec2 cpos = ImGui::GetCursorScreenPos();
                bool covered = any_open;

                ImGui::SetNextItemWidth(cw);
                ImGui::SetNextWindowSizeConstraints(ImVec2(cw, 0.0f), ImVec2(cw, 99999.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
                bool open = ImGui::BeginCombo(id, items[cur], ImGuiComboFlags_NoArrowButton);
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                if (open) {
                    any_open = true;
                    ImGui::SetWindowPos(ImVec2(cpos.x, cpos.y + chh));

                    float item_h = chh * 0.86f;
                    float top    = cpos.y + chh * 0.45f;
                    float bottom = cpos.y + chh + n * item_h;
                    ImDrawList* pdl = ImGui::GetWindowDrawList();
                    pdl->PushClipRectFullScreen();
                    pdl->AddRectFilled(ImVec2(cpos.x - 1.0f, top),
                                       ImVec2(cpos.x + cw + 1.0f, bottom),
                                       IM_COL32(33, 38, 3, 255), 8.0f,
                                       ImDrawFlags_RoundCornersBottom);

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                    for (int i = 0; i < n; i++) {
                        ImGui::PushID(i);
                        ImVec2 ip = ImGui::GetCursorScreenPos();
                        if (ImGui::InvisibleButton("##opt", ImVec2(cw, item_h))) {
                            picked = i;
                            ImGui::CloseCurrentPopup();
                        }
                        bool hot_i = ImGui::IsItemHovered();
                        if (hot_i || i == cur) {
                            float in = 3.0f;
                            float y0 = (i == 0)     ? top          : ip.y;
                            float y1 = (i == n - 1) ? bottom - in * 1.4f : ip.y + item_h;
                            ImDrawFlags fl = (i == n - 1) ? ImDrawFlags_RoundCornersBottom
                                                          : ImDrawFlags_RoundCornersNone;
                            pdl->AddRectFilled(ImVec2(cpos.x + in, y0),
                                               ImVec2(cpos.x + cw - in, y1),
                                               hot_i ? IM_COL32(61, 71, 5, 255)
                                                     : IM_COL32(46, 53, 4, 255), 5.0f, fl);
                        }
                        ImVec2 its = ImGui::CalcTextSize(items[i]);
                        pdl->AddText(ImVec2(cpos.x + 12.0f, ip.y + (item_h - its.y) * 0.5f),
                                     IM_COL32(0xE6, 0xED, 0xC7, 255), items[i]);
                        ImGui::PopID();
                    }
                    ImGui::PopStyleVar();
                    pdl->PopClipRect();
                    ImGui::EndCombo();
                }

                if (!covered) {
                    ImDrawList* fg = ImGui::GetForegroundDrawList();
                    fg->AddRectFilled(cpos, ImVec2(cpos.x + cw, cpos.y + chh),
                                      IM_COL32(61, 71, 5, 255), 8.0f);
                    ImVec2 cts = ImGui::CalcTextSize(items[cur]);
                    fg->AddText(ImVec2(cpos.x + 12.0f, cpos.y + (chh - cts.y) * 0.5f),
                                IM_COL32(0xE6, 0xED, 0xC7, 255), items[cur]);
                }
                return picked;
            };

            const char* modes[3] = {"normal", "crop", "stretch"};
            int fit = combo_row("screen fit", "##fit", modes, 3, (int)scale_mode);
            int fps = combo_row("menu frame cap", "##fps", kFpsNames, 6, fps_index);
            if (fit != (int)scale_mode || fps != fps_index) {
                scale_mode = (ScaleMode)fit;
                fps_index = fps;
                save_settings();
            }
        } else {
            const char* names[8] = {"right", "left", "up", "down", "a", "b", "select", "start"};
            for (int i = 0; i < 8; i++) {
                ImGui::PushID(i);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(names[i]);
                ImGui::SameLine(label_w);
                bool waiting = (rebind_target == i);
                if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.47f, 0.03f, 1.0f));
                const char* label = waiting ? "press a key" : SDL_GetKeyName(keybinds[i]);
                if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x - ctrl_gap, 0)))
                    rebind_target = waiting ? -1 : i;
                if (waiting) ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::SetCursorPos(ImVec2(pad, ws.y - pad - close_h));
        if (ImGui::Button("close", ImVec2(inner_w, close_h))) {
            settings_open = false;
            rebind_target = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else if (settings_open) {
        settings_open = false;
        rebind_target = -1;
    }
    ImGui::PopStyleVar(2);
}

void App::handle_events() {
#if GB_IOS
    // the ios menu is laid out in device pixels, so point-based touch events are scaled up to match
    float ui_scale = 1.0f;
    {
        int ww, wh, ow, oh;
        SDL_GetWindowSize(window, &ww, &wh);
        SDL_GetRendererOutputSize(renderer, &ow, &oh);
        if (ww > 0) ui_scale = (float)ow / ww;
    }
#endif
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        bool mouse_event = (event.type == SDL_MOUSEMOTION ||
                            event.type == SDL_MOUSEBUTTONDOWN ||
                            event.type == SDL_MOUSEBUTTONUP ||
                            event.type == SDL_MOUSEWHEEL);
        if (state == AppState::MENU || settings_open || mouse_event) {
#if GB_IOS
            if (event.type == SDL_MOUSEMOTION) {
                event.motion.x = (int)(event.motion.x * ui_scale);
                event.motion.y = (int)(event.motion.y * ui_scale);
            } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                event.button.x = (int)(event.button.x * ui_scale);
                event.button.y = (int)(event.button.y * ui_scale);
            }
#endif
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (event.type == SDL_QUIT)
            std::exit(0);

#if GB_IOS
        // stop rendering the moment ios tells us we are leaving the foreground, resume when back
        if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND)
            active = false;
        if (event.type == SDL_APP_DIDENTERFOREGROUND)
            active = true;
#endif

        if (rebind_target >= 0 && event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym != SDLK_ESCAPE) {
                keybinds[rebind_target] = event.key.keysym.sym;
                save_settings();
            }
            rebind_target = -1;
            continue;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            if (settings_open) {
                settings_open = false;
                continue;
            }
            if (state == AppState::PLAYING) {
                state = AppState::MENU;
                continue;
            }
        }

        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_TAB &&
            state == AppState::PLAYING && !settings_open) {
            scale_mode = (ScaleMode)(((int)scale_mode + 1) % 3);
            save_settings();
            continue;
        }

#if GB_IOS
        // on ios the joypad and the back button are on-screen touch zones
        if (state == AppState::PLAYING)
            handle_touch_ios(event);
#endif

        if (GB_DESKTOP && state == AppState::PLAYING && !settings_open &&
            (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
            bool pressed = (event.type == SDL_KEYDOWN);
            for (int i = 0; i < 8; i++)
                if (event.key.keysym.sym == keybinds[i])
                    mem->set_button(i, pressed);
        }
    }
}

// styling for the menu part
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
    s.PopupBorderSize = 0.0f;
    s.WindowBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]      = ImVec4(0.09f, 0.10f, 0.06f, 1.00f);
    c[ImGuiCol_Button]        = ImVec4(0.24f, 0.28f, 0.02f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.40f, 0.02f, 1.00f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.18f, 0.21f, 0.02f, 1.00f);
    c[ImGuiCol_Text]          = ImVec4(0.90f, 0.93f, 0.78f, 1.00f);
    c[ImGuiCol_TitleBg]       = ImVec4(0.12f, 0.14f, 0.02f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.21f, 0.02f, 1.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.24f, 0.28f, 0.02f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.34f, 0.40f, 0.02f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.18f, 0.21f, 0.02f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.15f, 0.01f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.34f, 0.40f, 0.02f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.40f, 0.47f, 0.03f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.24f, 0.28f, 0.02f, 1.00f);
    c[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.09f, 0.10f, 0.06f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.24f, 0.28f, 0.02f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.34f, 0.40f, 0.02f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.47f, 0.03f, 1.00f);
}

// reduce a name to lower letters dropping parenthesis and such
std::string App::normalize(std::string s) {
    std::string out;
    int depth = 0;
    for (char ch : s) {
        if (ch == '(' || ch == '[') { depth++; continue; }
        if (ch == ')' || ch == ']') { if (depth > 0) depth--; continue; }
        if (depth > 0) continue;
        if (std::isalnum(static_cast<unsigned char>(ch)))
            out += std::tolower(static_cast<unsigned char>(ch));
    }
    return out;
}

// finds the closest match to the normalized rom name
std::string App::closest_artwork(const std::string& rom_name) {
    std::string target = normalize(rom_name);
    std::string best_path;
    size_t best_score = std::string::npos;

    for (const auto& entry : std::filesystem::directory_iterator(artwork_folder)) {
        if (entry.path().extension() != ".png") continue;

        std::string cand = normalize(entry.path().stem().string());
        if (cand.empty()) continue;

        if (cand == target)
            return entry.path().string();           // exact, done

        // accept only if one contains the other; score by length difference
        if (cand.find(target) != std::string::npos ||
            target.find(cand) != std::string::npos) {
            size_t score = (cand.size() > target.size())
                         ? cand.size() - target.size()
                         : target.size() - cand.size();
            if (score < best_score) {
                best_score = score;
                best_path = entry.path().string();
            }
            }
    }
    return best_path;   // empty string if nothing matched
}

// turns the file name into a formatted displayable name for the menu ui
std::string App::display_name(const std::string& s) {
    std::string out;
    int depth = 0;
    for (char ch : s) {
        if (ch == '(' || ch == '[') { depth++; continue; }
        if (ch == ')' || ch == ']') { if (depth > 0) depth--; continue; }
        if (depth > 0) continue;
        out += ch;
    }

    // drop a trailing gb extension if present
    if (out.size() >= 3 && out.substr(out.size() - 3) == ".gb")
        out.erase(out.size() - 3);

    // trim trailing spaces left behind by removed tags
    while (!out.empty() && out.back() == ' ')
        out.pop_back();

    // cap length adding ... when truncated
    if (out.size() > 21)
        out = out.substr(0, 21) + "...";

    return out;
}

void App::render_menu() {
#if GB_IOS
    render_menu_ios();
    return;
#endif
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    float w = io.DisplaySize.x;
    float h = io.DisplaySize.y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("gameboy-emu", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    auto centre_text = [&](const char* s, float y) {
        float tw = ImGui::CalcTextSize(s).x;
        ImGui::SetCursorPos(ImVec2((w - tw) * 0.5f, y));
        ImGui::TextUnformatted(s);
    };

    ImGui::SetCursorPos(ImVec2(w * 0.03f, h * 0.05f));
    ImGui::TextUnformatted("gameboy-emu");

    std::vector<int> view;
    for (int i = 0; i < (int)rom_list.size(); i++)
        if ((cover_list[i] != nullptr) != show_debug)
            view.push_back(i);
    int count = (int)view.size();

    float tog_w = w * 0.13f, tog_h = h * 0.07f;
    ImGui::SetCursorPos(ImVec2(w - tog_w - w * 0.03f, h * 0.04f));
    if (ImGui::Button(show_debug ? "games" : "test roms", ImVec2(tog_w, tog_h))) {
        show_debug = !show_debug;
        carousel_pos = carousel_target = 0.0f;
        carousel_vel = 0.0f;
    }

    if (count == 0) {
        centre_text(show_debug ? "no test roms" : "no games", h * 0.45f);
    } else {
        float cover    = h * 0.42f;
        float cover_cx = w * 0.5f;
        float cover_cy = h * 0.42f;
        float spacing  = cover * 0.55f;

        if (!settings_open) {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) carousel_vel += 3.5f;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  carousel_vel -= 3.5f;
            if (io.MouseWheel != 0.0f) carousel_vel += (io.MouseWheel > 0.0f ? -3.5f : 3.5f);
        }

        ImGui::SetCursorPos(ImVec2(0, cover_cy - cover * 0.75f));
        ImGui::InvisibleButton("swipe", ImVec2(w, cover * 1.5f));
        if (ImGui::IsItemActivated()) {
            carousel_drag_start = carousel_pos;
            carousel_vel = 0.0f;
        }
        if (ImGui::IsItemActive()) {
            float np = carousel_drag_start - ImGui::GetMouseDragDelta(0, 0.0f).x / spacing;
            if (io.DeltaTime > 0.0f)
                carousel_vel = carousel_vel * 0.3f + ((np - carousel_pos) / io.DeltaTime) * 0.7f;
            carousel_vel = std::max(-90.0f, std::min(90.0f, carousel_vel));
            carousel_pos = np;
        } else if (std::abs(carousel_vel) > 0.4f) {
            carousel_pos += carousel_vel * io.DeltaTime;
            carousel_vel *= std::exp(-3.5f * io.DeltaTime);
        } else {
            carousel_vel = 0.0f;
            float target = std::round(carousel_pos);
            carousel_pos += (target - carousel_pos) * std::min(1.0f, io.DeltaTime * 14.0f);
            if (std::abs(target - carousel_pos) < 0.001f) carousel_pos = target;
        }

        auto wrap = [count](int k) { return ((k % count) + count) % count; };
        int centre = wrap((int)std::lround(carousel_pos));
        int r_centre = view[centre];
        carousel_index = r_centre;

        if (!settings_open &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
            load_rom(rom_list[r_centre]);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 org = ImGui::GetWindowPos();
        int base = (int)std::floor(carousel_pos);
        struct Card { int r; float rel, cx, sz; int a; };
        std::vector<Card> cards;
        for (int k = base - 3; k <= base + 4; k++) {
            float rel = k - carousel_pos;
            if (std::abs(rel) > 3.2f) continue;
            float ad = std::min(std::abs(rel), 3.5f);
            cards.push_back({view[wrap(k)], rel,
                             cover_cx + rel * (cover * 0.62f),
                             cover * (1.0f - ad * 0.10f),
                             (int)(255 * std::max(0.0f, 1.0f - ad * 0.26f))});
        }
        std::sort(cards.begin(), cards.end(),
                  [](const Card& x, const Card& y) { return std::abs(x.rel) > std::abs(y.rel); });
        for (const Card& cd : cards) {
            float hs = cd.sz * 0.5f;
            ImVec2 p0(org.x + cd.cx - hs, org.y + cover_cy - hs);
            ImVec2 p1(org.x + cd.cx + hs, org.y + cover_cy + hs);
            if (cover_list[cd.r]) {
                dl->AddImage((ImTextureID)cover_list[cd.r], p0, p1,
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, cd.a));
            } else {
                dl->AddRectFilled(p0, p1, IM_COL32(0x3d, 0x47, 0x03, cd.a), 10.0f);
                std::string nm = display_name(rom_list[cd.r]);
                ImVec2 ts = ImGui::CalcTextSize(nm.c_str());
                dl->AddText(ImVec2(org.x + cd.cx - ts.x * 0.5f, org.y + cover_cy - ts.y * 0.5f),
                            IM_COL32(0xE6, 0xED, 0xC7, cd.a), nm.c_str());
            }
        }

        float title_y = cover_cy + cover * 0.62f;
        centre_text(display_name(rom_list[r_centre]).c_str(), title_y);

        float btn_h  = h * 0.09f;
        float play_w = w * 0.14f, del_w = w * 0.10f, gap = w * 0.015f;
        float row_x  = (w - (play_w + gap + del_w)) * 0.5f;
        float row_y  = title_y + h * 0.06f;

        ImGui::SetCursorPos(ImVec2(row_x, row_y));
        if (ImGui::Button("play", ImVec2(play_w, btn_h)))
            load_rom(rom_list[r_centre]);

        ImGui::SetCursorPos(ImVec2(row_x + play_w + gap, row_y));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.12f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.16f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.10f, 0.05f, 1.0f));
        if (ImGui::Button("delete", ImVec2(del_w, btn_h)))
            ImGui::OpenPopup("confirm_delete");
        ImGui::PopStyleColor(3);

        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        if (ImGui::BeginPopupModal("confirm_delete", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(("delete " + display_name(rom_list[r_centre]) + " ?").c_str());
            ImGui::Dummy(ImVec2(0, h * 0.02f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.06f, 1.0f));
            if (ImGui::Button("delete", ImVec2(w * 0.10f, btn_h))) {
                std::error_code ec;
                std::filesystem::remove(rom_folder + rom_list[r_centre], ec);
                scan_roms();
                carousel_pos = carousel_target = (centre > 0) ? (float)(centre - 1) : 0.0f;
                carousel_vel = 0.0f;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button("cancel", ImVec2(w * 0.10f, btn_h)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        std::string page = std::to_string(centre + 1) + " / " + std::to_string(count);
        centre_text(page.c_str(), row_y + btn_h + h * 0.025f);
    }

    float add_w = w * 0.16f;
    ImGui::SetCursorPos(ImVec2((w - add_w) * 0.5f, h * 0.85f));
    if (ImGui::Button("add game", ImVec2(add_w, h * 0.07f)))
        add_game();

    draw_settings(w, h);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0x17, 0x1a, 0x0f, 0xFF);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    if (!in_live_resize) pace((double)kFpsCaps[fps_index]);
}
