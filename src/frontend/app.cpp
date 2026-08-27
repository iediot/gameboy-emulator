//
// Created by edi on 5/23/26.
//

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "platform.h" // first so GB_DESKTOP is defined before the guard below
#if GB_DESKTOP
#include <nfd.h> // native desktop file dialog, no ios equivalent
#endif
#include "app.h"
#if GB_MOBILE
extern "C" void gb_present_document_picker(const char* dest_dir);
#endif
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "theme.h"
#include "glass.h"

// colour cartridges carry .gbc, monochrome ones .gb, both are just a rom to us
static bool is_rom_file(const std::filesystem::path& p) {
    std::string e = p.extension().string();
    for (char& c : e)
        c = (char)std::tolower((unsigned char)c);
    return e == ".gb" || e == ".gbc";
}

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
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_AudioSpec want{}, have{};
    want.freq     = 48000;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = nullptr;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_device)
        SDL_PauseAudioDevice(audio_device, 0);

    init_paths();

#if GB_MOBILE
    win_w = 600;
    win_h = 1000;
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
    create_video();

#if GB_DESKTOP
    // nfd
    NFD_Init();
#endif
    last_present = SDL_GetPerformanceCounter();
    scan_roms();
}

#if GB_DESKTOP && defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
static bool system_dark_theme() {
    bool dark = false;
    CFPropertyListRef v = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"),
                                                    kCFPreferencesAnyApplication);
    if (v) {
        if (CFGetTypeID(v) == CFStringGetTypeID())
            dark = CFStringCompare((CFStringRef)v, CFSTR("Dark"),
                                   kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        CFRelease(v);
    }
    return dark;
}
#else
static bool system_dark_theme() { return true; }
#endif

static bool position_on_a_display(int x, int y, int w, int h) {
    SDL_Rect win = {x, y, w, h};
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect bounds, out;
        if (SDL_GetDisplayUsableBounds(i, &bounds) != 0) continue;
        if (SDL_IntersectRect(&win, &bounds, &out) && out.w > 120 && out.h > 80)
            return true;
    }
    return false;
}

void App::create_video() {
    Uint32 win_flags = SDL_WINDOW_SHOWN;
#if GB_MOBILE
    win_flags |= SDL_WINDOW_ALLOW_HIGHDPI; // back the renderer at native pixels, not an upscaled buffer
#endif
#if GB_ANDROID
    // sdlactivity only switches on sticky immersive while the sdl window is fullscreen,
    // without this the status and navigation bars stay on top of the game
    win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
#if GB_DESKTOP
    win_flags |= SDL_WINDOW_RESIZABLE;
    if (hidpi) win_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    int px = SDL_WINDOWPOS_CENTERED, py = SDL_WINDOWPOS_CENTERED;
#if GB_DESKTOP
    if (have_win_pos && position_on_a_display(win_x, win_y, win_w, win_h)) {
        px = win_x;
        py = win_y;
    }
#endif
    window = SDL_CreateWindow("gameboy-emu", // window title
        px, py, win_w, win_h, win_flags);
#if GB_DESKTOP
    SDL_SetWindowMinimumSize(window, 480, 360);
    if (!bundled) {
        const std::string& icon_path = system_dark_theme() ? icon_dark_path : icon_light_path;
        if (SDL_Surface* icon = IMG_Load(icon_path.c_str())) {
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
        }
    }
#endif
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetVSync(renderer, vsync ? 1 : 0);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest); // keep the gameboy pixels sharp when scaled up
#if GB_MOBILE
#endif
    cartridge_sprite = IMG_LoadTexture(renderer, cartridge_path.c_str());
    if (cartridge_sprite) {
        SDL_SetTextureBlendMode(cartridge_sprite, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(cartridge_sprite, SDL_ScaleModeLinear);
    }
    trash_sprite = IMG_LoadTexture(renderer, trash_path.c_str());
    if (trash_sprite) {
        SDL_SetTextureBlendMode(trash_sprite, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(trash_sprite, SDL_ScaleModeLinear);
    }
    cartridge_shadow = nullptr;
    rect_shadow = nullptr;
    build_shadow();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
#if GB_DESKTOP
    SDL_StopTextInput();
#endif
#if GB_DESKTOP
    SDL_AddEventWatch(resize_watch, this);
#endif
}

void App::destroy_video() {
#if GB_DESKTOP
    SDL_DelEventWatch(resize_watch, this);
#endif
    SDL_GetWindowSize(window, &win_w, &win_h);
    SDL_GetWindowPosition(window, &win_x, &win_y);
    have_win_pos = true;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    for (SDL_Texture* cover : cover_list)
        if (cover)
            SDL_DestroyTexture(cover);
    cover_list.clear();
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(cartridge_sprite);
    SDL_DestroyTexture(trash_sprite);
    SDL_DestroyTexture(cartridge_shadow);
    SDL_DestroyTexture(rect_shadow);
    SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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
#if GB_MOBILE
    char* pref = SDL_GetPrefPath("com.iediot", "gbemu");
    std::string p = pref ? pref : "";
    if (pref) SDL_free(pref);
    rom_folder    = p + "game-roms/"; // writable copy so roms can be added and deleted
    mods_folder   = p + "mods/";
    settings_path = p + "settings.txt";

#if GB_ANDROID
    // assets live inside the apk, sdl_rwops resolves these relative paths through
    // the asset manager, so there is no base path to prefix them with
    cartridge_path = "cartridge.png";
    trash_path     = "trash.png";
    artwork_folder = "artworks/";
#else
    char* base = SDL_GetBasePath();
    std::string b = base ? base : "";
    if (base) SDL_free(base);
    cartridge_path = b + "cartridge.png";
    trash_path     = b + "trash.png";
    artwork_folder = b + "artworks/";      // read-only, shipped in the bundle
#endif

    // on first launch seed the writable folder with the roms shipped in the app
    std::error_code ec;
    std::filesystem::create_directories(rom_folder, ec);
    if (std::filesystem::is_empty(rom_folder, ec))
        for (const std::string& name : bundled_roms())
            copy_bundled_rom(name);
#else
    char* pref = SDL_GetPrefPath("com.iediot", "gbemu");
    std::string p = pref ? pref : "";
    if (pref) SDL_free(pref);
    settings_path = p + "settings.txt";
    mods_folder   = p + "mods/";

    // inside a .app the assets sit in Contents/Resources, a plain build off the
    // source tree keeps the old relative paths so running from the ide still works
    std::string res;
    char* base = SDL_GetBasePath();
    if (base) {
        std::error_code ec;
        std::string candidate = std::string(base) + "../Resources/";
        if (std::filesystem::exists(candidate + "artworks", ec))
            res = candidate;
        SDL_free(base);
    }

    bundled = !res.empty();
    if (bundled) {
        cartridge_path  = res + "cartridge.png";
        trash_path      = res + "trash.png";
        icon_light_path = res + "icon-mac-light.png";
        icon_dark_path  = res + "icon-mac-dark.png";
        artwork_folder  = res + "artworks/";
        rom_folder      = p + "game-roms/";

        std::error_code ec;
        std::filesystem::create_directories(rom_folder, ec);
        if (std::filesystem::is_empty(rom_folder, ec)) {
            for (const auto& e : std::filesystem::directory_iterator(res + "game-roms/", ec))
                if (is_rom_file(e.path()))
                    std::filesystem::copy_file(e.path(), rom_folder + e.path().filename().string(),
                                               std::filesystem::copy_options::skip_existing, ec);
        }
    } else {
        /* an installed build carries its assets next to the executable, which is the
           only layout a packaged linux tarball can rely on. a build run straight out of
           the source tree still finds them one level up, so running from the ide is
           unchanged. roms go to the writable pref path when installed, since nothing
           may be written back into an install directory */
        std::string exe;
        char* eb = SDL_GetBasePath();
        if (eb) { exe = eb; SDL_free(eb); }

        std::error_code aec;
        if (!exe.empty() && std::filesystem::exists(exe + "assets/artworks", aec)) {
            cartridge_path  = exe + "assets/sprites/cartridge.png";
            trash_path      = exe + "assets/sprites/trash.png";
            icon_light_path = exe + "assets/sprites/icon-mac-light.png";
            icon_dark_path  = exe + "assets/sprites/icon-mac-dark.png";
            artwork_folder  = exe + "assets/artworks/";
            rom_folder      = p + "game-roms/";
            std::filesystem::create_directories(rom_folder, aec);
        } else {
            cartridge_path  = "../assets/sprites/cartridge.png";
            trash_path      = "../assets/sprites/trash.png";
            icon_light_path = "../assets/sprites/icon-mac-light.png";
            icon_dark_path  = "../assets/sprites/icon-mac-dark.png";
            artwork_folder  = "../assets/artworks/";
            rom_folder      = "../roms/game-roms/";
        }
    }
#endif
}

static SDL_Texture* blur_to_texture(SDL_Renderer* renderer, std::vector<float>& a,
                                    int bw, int bh, int radius) {
    std::vector<float> b(bw * bh, 0.0f);
    float inv = 1.0f / (2 * radius + 1);
    auto blur_h = [&](const std::vector<float>& in, std::vector<float>& out) {
        for (int y = 0; y < bh; y++) {
            const float* r = &in[y * bw];
            float* o = &out[y * bw];
            float sum = 0.0f;
            for (int i = -radius; i <= radius; i++) sum += r[std::clamp(i, 0, bw - 1)];
            for (int x = 0; x < bw; x++) {
                o[x] = sum * inv;
                sum += r[std::clamp(x + radius + 1, 0, bw - 1)] - r[std::clamp(x - radius, 0, bw - 1)];
            }
        }
    };
    auto blur_v = [&](const std::vector<float>& in, std::vector<float>& out) {
        for (int x = 0; x < bw; x++) {
            float sum = 0.0f;
            for (int i = -radius; i <= radius; i++) sum += in[std::clamp(i, 0, bh - 1) * bw + x];
            for (int y = 0; y < bh; y++) {
                out[y * bw + x] = sum * inv;
                sum += in[std::clamp(y + radius + 1, 0, bh - 1) * bw + x]
                     - in[std::clamp(y - radius, 0, bh - 1) * bw + x];
            }
        }
    };
    for (int pass = 0; pass < 3; pass++) {
        blur_h(a, b);
        blur_v(b, a);
    }

    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, bw, bh, 32, SDL_PIXELFORMAT_RGBA32);
    if (!out) return nullptr;
    for (int y = 0; y < bh; y++) {
        Uint8* row = (Uint8*)out->pixels + y * out->pitch;
        for (int x = 0; x < bw; x++) {
            float v = std::min(1.0f, a[y * bw + x]);
            row[x * 4 + 0] = 0;
            row[x * 4 + 1] = 0;
            row[x * 4 + 2] = 0;
            row[x * 4 + 3] = (Uint8)(v * 255.0f);
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, out);
    SDL_FreeSurface(out);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    }
    return tex;
}

void App::build_shadow() {
    SDL_Surface* raw = IMG_Load(cartridge_path.c_str());
    if (raw) {
        SDL_Surface* src = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(raw);
        if (src) {
            const int pad = 80;
            int sw = src->w, sh = src->h;
            int bw = sw + pad * 2, bh = sh + pad * 2;
            std::vector<float> a(bw * bh, 0.0f);
            for (int y = 0; y < sh; y++) {
                const Uint8* row = (const Uint8*)src->pixels + y * src->pitch;
                for (int x = 0; x < sw; x++)
                    a[(y + pad) * bw + (x + pad)] = row[x * 4 + 3] / 255.0f;
            }
            SDL_FreeSurface(src);
            cartridge_shadow = blur_to_texture(renderer, a, bw, bh, 20);
            if (cartridge_shadow) {
                shadow_pad_x = (float)pad / (float)sw;
                shadow_pad_y = (float)pad / (float)sh;
            }
        }
    }

    const int core = 256, rpad = 72, corner = 12;
    int rw = core + rpad * 2;
    std::vector<float> m(rw * rw, 0.0f);
    for (int y = 0; y < core; y++) {
        for (int x = 0; x < core; x++) {
            float dx = 0.0f, dy = 0.0f;
            if (x < corner)             dx = (float)(corner - x);
            else if (x >= core - corner) dx = (float)(x - (core - corner - 1));
            if (y < corner)             dy = (float)(corner - y);
            else if (y >= core - corner) dy = (float)(y - (core - corner - 1));
            float d = std::sqrt(dx * dx + dy * dy);
            m[(y + rpad) * rw + (x + rpad)] = (d > (float)corner) ? 0.0f : 1.0f;
        }
    }
    rect_shadow = blur_to_texture(renderer, m, rw, rw, 18);
    if (rect_shadow) rect_pad = (float)rpad / (float)core;
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
        } else if (key == "vsync") {
            int v; if (f >> v) vsync = (v != 0);
        } else if (key == "hidpi") {
            int v; if (f >> v) hidpi = (v != 0);
        } else if (key == "cartridge") {
            int v; if (f >> v) render_cartridge = (v != 0);
        } else if (key == "volume") {
            float v; if (f >> v && v >= 0.0f && v <= 1.0f) volume = v;
        } else if (key == "cgb") {
            int v; if (f >> v) cgb_enabled = (v != 0);
        } else if (key == "theme") {
            int v; if (f >> v && v >= 0 && v <= 2) theme_mode = v;
        } else if (key == "dmgcolor") {
            int v; if (f >> v) dmg_colorize = (v != 0);
        } else if (key == "joystick") {
            int v;
            if (f >> v) {
#if GB_MOBILE
                joystick_mode = (v != 0);
#endif
            }
        } else if (key == "snap") {
            int v;
            if (f >> v) {
#if GB_MOBILE
                snap_enabled = (v != 0);
#endif
            }
        } else if (key == "control") {
            int i; float x, y, sc;
            if (f >> i >> x >> y >> sc && i >= 0 && i < CTRL_COUNT) {
#if GB_MOBILE
                controls[i] = {x, y, sc};
                layout_custom = true;
#endif
            }
        } else if (key == "window") {
            int ww, wh;
            if (f >> ww >> wh && ww >= 480 && wh >= 360 && ww <= 16384 && wh <= 16384) {
                win_w = ww;
                win_h = wh;
            }
        } else if (key == "windowpos") {
            int wx, wy;
            if (f >> wx >> wy) {
                win_x = wx;
                win_y = wy;
                have_win_pos = true;
            }
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
    f << "vsync " << (vsync ? 1 : 0) << "\n";
    f << "hidpi " << (hidpi ? 1 : 0) << "\n";
    f << "cartridge " << (render_cartridge ? 1 : 0) << "\n";
    f << "volume " << volume << "\n";
    f << "cgb " << (cgb_enabled ? 1 : 0) << "\n";
    f << "theme " << theme_mode << "\n";
    f << "dmgcolor " << (dmg_colorize ? 1 : 0) << "\n";
#if GB_MOBILE
    f << "joystick " << (joystick_mode ? 1 : 0) << "\n";
    f << "snap " << (snap_enabled ? 1 : 0) << "\n";
    if (layout_custom)
        for (int i = 0; i < CTRL_COUNT; i++)
            f << "control " << i << " " << controls[i].x << " " << controls[i].y
              << " " << controls[i].scale << "\n";
#endif
#if GB_DESKTOP
    if (window) {
        int ww = win_w, wh = win_h, wx = win_x, wy = win_y;
        SDL_GetWindowSize(window, &ww, &wh);
        SDL_GetWindowPosition(window, &wx, &wy);
        f << "window " << ww << " " << wh << "\n";
        f << "windowpos " << wx << " " << wy << "\n";
    }
#endif
    for (int i = 0; i < 8; i++)
        f << "key " << i << " " << (long long)keybinds[i] << "\n";
}

// destructor
App::~App() {
    save_battery_ram();
    destroy_video();
    ImGui::DestroyContext();
#if GB_DESKTOP
    // nfd
    NFD_Quit();
#endif
    SDL_Quit();
}

// run the cpu and ppu in lockstep
void App::run() {
    AppState prev_state = state;
    while (true) {
        handle_events();

#if GB_MOBILE
        // ios forbids gpu work in the background, so pause the whole loop until we return
        if (!active) {
            SDL_Delay(150);
            continue;
        }
#endif

#if GB_MOBILE
        if (editing_layout) {
            render_layout_editor();
        } else
#endif
        if (state == AppState::PLAYING) {
            // step until a frame is ready
            // the settings panel pauses the game rather than running it underneath
            if (!settings_open) {
                /* the cap is the safety net for a frame that never lands, which is what
                   happens whenever the lcd is off. in double speed the cpu runs twice as
                   fast while the ppu keeps its own clock, so a frame costs twice as many
                   cpu cycles and a fixed cap would cut every frame in half */
                uint64_t frame_start = cpu->total_cycles;
                while (!ppu->frame_ready &&
                       cpu->total_cycles - frame_start < (mem->double_speed ? 140448u : 70224u)) {
                    cpu->step();
                }
                ppu->frame_ready = false;
                if (++battery_flush >= 60) {
                    battery_flush = 0;
                    save_battery_ram();
                }
            }
            if (!apu->samples.empty()) {
                if (SDL_GetQueuedAudioSize(audio_device) <= 16384) {
                    // hearing is logarithmic, so a linear scale barely drops until the very
                    // end of its travel. cubing it spreads the quiet half over most of the bar
                    if (volume < 0.999f) {
                        float gain = volume * volume * volume;
                        for (int16_t& sample : apu->samples)
                            sample = (int16_t)(sample * gain);
                    }
                    SDL_QueueAudio(audio_device, apu->samples.data(),
                                   (Uint32)(apu->samples.size() * sizeof(int16_t)));
                }
                apu->samples.clear();
            }
            render_game();
        } else {
            render_menu();
        }

        if (state != prev_state) {
            if (prev_state == AppState::PLAYING)
                save_battery_ram();
            ImGui::GetIO().ClearInputKeys();
            ImGui::GetIO().ClearInputMouse();
            prev_state = state;
        }

        if (video_reset) {
            video_reset = false;
            destroy_video();
            create_video();
            scan_roms();
            ImGui::GetIO().ClearInputKeys();
        }
    }
}

// .gb files on the left shelf, .gbc on the right, one home each
bool App::in_tab(int rom, int tab) const {
    uint8_t gbc = (rom < (int)rom_is_gbc.size()) ? rom_is_gbc[rom] : 0;
    return (gbc ? 1 : 0) == tab;
}

// the rom indices belonging to whichever half of the library is on screen
int App::library_view(std::vector<int>& out) const {
    out.clear();
    for (int i = 0; i < (int)rom_list.size(); i++)
        if (in_tab(i, library_tab))
            out.push_back(i);
    return (int)out.size();
}

/* the library is split across a segmented pill. the indicator slides between the two
   halves rather than cutting, and it carries the olive of the rest of the ui on the mono
   side, easing into a muted two tone on the colour side so the shelves read apart at a
   glance without breaking the palette */
void App::draw_library_tabs(float cx, float cy, float pill_w, float pill_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r   = pill_h * 0.5f;
    float seg = pill_w * 0.5f;
    ImVec2 p0(cx - pill_w * 0.5f, cy - r);
    ImVec2 p1(cx + pill_w * 0.5f, cy + r);

    for (int i = 0; i < 2; i++) {
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + seg * i, p0.y));
        if (ImGui::InvisibleButton("##libtab", ImVec2(seg, pill_h)) && library_tab != i) {
            library_tab = i;
            carousel_pos = carousel_target = 0.0f;
            carousel_vel = 0.0f;
        }
        ImGui::PopID();
    }

    dl->AddRectFilled(p0, p1, glass::fill(theme::at().surface), r);

    // a frame rate independent ease so the slide feels the same on 60 and 120 hz
    float dt = ImGui::GetIO().DeltaTime;
    tab_slide += ((float)library_tab - tab_slide) * (1.0f - std::exp(-14.0f * dt));
    if (std::fabs((float)library_tab - tab_slide) < 0.001f)
        tab_slide = (float)library_tab;

    float pad = pill_h * 0.10f;
    ImVec2 i0(p0.x + pad + seg * tab_slide, p0.y + pad);
    ImVec2 i1(i0.x + seg - pad * 2.0f, p1.y - pad);

    dl->AddRectFilled(i0, i1, glass::fill(theme::at().accent), r - pad);
    // the same field the page carries, clipped inside the indicator, so selecting the
    // colour shelf lights the pill with it too
    if (tab_slide > 0.002f) {
        // blob radii scale with the area handed in, so a pill sized area gives dots.
        // feed it something screen sized and clip back to the indicator
        // the pill is small, so its blobs are driven off a far bigger virtual area than
        // itself, otherwise they come out as specks rather than broad washes of colour
        iri::field_rounded(dl, i0, i1, r - pad, (float)ImGui::GetTime(), tab_slide * 0.5f, 3.2f);
    }
    glass::rect(dl, i0, i1, r - pad);
    glass::rect(dl, p0, p1, r, 0, false);

    const char* names[2] = {"game boy", "color"};
    for (int i = 0; i < 2; i++) {
        // the label brightens as the indicator arrives under it
        float lit = 1.0f - std::fabs(tab_slide - (float)i);
        lit = std::max(0.0f, lit);
        int a = (int)(120 + 135 * lit);
        ImVec2 ts = ImGui::CalcTextSize(names[i]);
        ImU32 tc = theme::at().text;
        dl->AddText(ImVec2(p0.x + seg * i + (seg - ts.x) * 0.5f, cy - ts.y * 0.5f),
                    (tc & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT), names[i]);
    }
}

// find the games inside the game path and the closest matching cover for each
void App::scan_roms() {
    rom_list.clear();
    cover_list.clear();
    rom_is_gbc.clear();
    for (const auto& entry : std::filesystem::directory_iterator(rom_folder)) {
        if (is_rom_file(entry.path())) {
            rom_list.push_back(entry.path().filename().string());
            /* the shelf comes from the extension, which is how the cartridge was sold.
               the header only says whether it can do colour, and it cannot tell a game
               boy game with colour added from a colour game that also runs on a dmg,
               both carry 0x80. whether it actually renders in colour is decided from
               the header later, in Memory::load_rom */
            std::string ext = entry.path().extension().string();
            for (char& c : ext)
                c = (char)std::tolower((unsigned char)c);
            rom_is_gbc.push_back(ext == ".gbc" ? 1 : 0);

            bool gbc = ext == ".gbc";
            std::string path = closest_artwork(entry.path().stem().string(), gbc);
            if (path.empty())
                cover_list.push_back(nullptr);
            else {
                SDL_Texture* cover = IMG_LoadTexture(renderer, path.c_str());
                if (cover) {
                    SDL_SetTextureBlendMode(cover, SDL_BLENDMODE_BLEND); // so alpha fades every cover, not just ones shipping an alpha channel
                    SDL_SetTextureScaleMode(cover, SDL_ScaleModeLinear);
                }
                cover_list.push_back(cover);
            }
        }
    }
}

// an ips patch is a list of records: a 3 byte offset, a 2 byte length, then that many
// bytes, or a zero length meaning a 2 byte run count and one byte to repeat
static void apply_ips(std::vector<uint8_t>& rom, const std::vector<uint8_t>& p) {
    if (p.size() < 8 || p[0] != 'P' || p[1] != 'A' || p[2] != 'T' || p[3] != 'C' || p[4] != 'H')
        return;
    size_t i = 5;
    while (i + 3 <= p.size()) {
        if (p[i] == 'E' && p[i + 1] == 'O' && p[i + 2] == 'F')
            return;
        if (i + 5 > p.size())
            return;
        size_t off = ((size_t)p[i] << 16) | ((size_t)p[i + 1] << 8) | p[i + 2];
        size_t len = ((size_t)p[i + 3] << 8) | p[i + 4];
        i += 5;
        if (len == 0) {
            if (i + 3 > p.size())
                return;
            size_t run = ((size_t)p[i] << 8) | p[i + 1];
            uint8_t val = p[i + 2];
            i += 3;
            if (rom.size() < off + run)
                rom.resize(off + run, 0);
            std::fill_n(rom.begin() + (long)off, run, val);
        } else {
            if (i + len > p.size())
                return;
            if (rom.size() < off + len)
                rom.resize(off + len, 0);
            std::copy_n(p.begin() + (long)i, len, rom.begin() + (long)off);
            i += len;
        }
    }
}

// loads the rom, moved from main
void App::load_rom(const std::string& name) {
    // rebuild the emulator
    mem = std::make_unique<Memory>();
    ppu = std::make_unique<Ppu>(*mem);
    apu = std::make_unique<Apu>();
    cpu = std::make_unique<Cpu>(*mem, *ppu, *apu);
    mem->apu = apu.get();

    std::ifstream rom_file(rom_folder + name, std::ios::binary);
    if (!rom_file) {
        std::cerr << "Could not open: " << name << "\n";
        return;
    }
    std::vector<uint8_t> rom_data{std::istreambuf_iterator<char>(rom_file),
        std::istreambuf_iterator<char>()};
    scan_mods(name);
    for (size_t i = 0; i < mod_list.size(); i++) {
        if (!mod_on[i])
            continue;
        std::ifstream pf(mod_dir(name) + mod_list[i], std::ios::binary);
        if (!pf)
            continue;
        std::vector<uint8_t> patch{std::istreambuf_iterator<char>(pf),
            std::istreambuf_iterator<char>()};
        apply_ips(rom_data, patch);
    }

    mem->cgb_enabled = cgb_enabled;
    mem->dmg_colorize = dmg_colorize;
    mem->load_rom(rom_data);
    cpu->apply_boot_state();

    load_battery_ram(name);
    state = AppState::PLAYING;
}

// the colour table is chosen once at load, so a toggle has to redo that for the game
// already running, a cgb cartridge has its own palettes and is left alone
void App::refresh_palette() {
    if (!mem)
        return;
    mem->cgb_enabled = cgb_enabled;
    mem->dmg_colorize = dmg_colorize;
    if (mem->cgb_mode)
        return;
    if (dmg_colorize)
        mem->apply_compat_palette();
    else
        mem->compat_palette = false;
}

void App::load_battery_ram(const std::string& name) {
    save_path.clear();
    if (!mem->has_battery || mem->external_ram.empty())
        return;
    save_path = rom_folder + std::filesystem::path(name).stem().string() + ".sav";

    std::ifstream f(save_path, std::ios::binary);
    if (!f)
        return;
    std::vector<uint8_t> saved{std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()};
    size_t n = std::min(saved.size(), mem->external_ram.size());
    std::copy_n(saved.begin(), n, mem->external_ram.begin());
    mem->ram_dirty = false;
}

void App::save_battery_ram() {
    if (save_path.empty() || !mem || !mem->ram_dirty || mem->external_ram.empty())
        return;
    std::ofstream f(save_path, std::ios::binary | std::ios::trunc);
    if (!f)
        return;
    f.write(reinterpret_cast<const char*>(mem->external_ram.data()),
            (std::streamsize)mem->external_ram.size());
    mem->ram_dirty = false;
}

// the renderer of the games inside the actual emulator
void App::render_game() {
#if GB_MOBILE
    render_game_mobile(); // letterboxed layout lives in ios_ui.cpp
    return;
#endif
    SDL_UpdateTexture(texture, nullptr, ppu->framebuffer, 160 * 4);

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
    sync_theme();
    // the field fills the letterbox around the lcd, never the picture itself
    draw_iridescence(io.DisplaySize.x, io.DisplaySize.y, &dst);
    draw_settings(io.DisplaySize.x, io.DisplaySize.y);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    SDL_RenderPresent(renderer);
    if (!in_live_resize) pace(kGbFps);
}

std::string App::mod_dir(const std::string& rom) const {
    return mods_folder + std::filesystem::path(rom).stem().string() + "/";
}

void App::scan_mods(const std::string& rom) {
    mod_rom = rom;
    mod_list.clear();
    mod_on.clear();

    std::error_code ec;
    std::string dir = mod_dir(rom);
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        std::string ext = e.path().extension().string();
        for (char& c : ext)
            c = (char)std::tolower((unsigned char)c);
        if (ext == ".ips")
            mod_list.push_back(e.path().filename().string());
    }
    std::sort(mod_list.begin(), mod_list.end());
    mod_on.assign(mod_list.size(), 0);
    mod_sel.assign(mod_list.size(), 0);
    mod_select = false;

    std::ifstream f(dir + "enabled.txt");
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        for (size_t i = 0; i < mod_list.size(); i++)
            if (mod_list[i] == line)
                mod_on[i] = 1;
    }
}

void App::save_mods() {
    std::error_code ec;
    std::filesystem::create_directories(mod_dir(mod_rom), ec);
    std::ofstream f(mod_dir(mod_rom) + "enabled.txt", std::ios::trunc);
    for (size_t i = 0; i < mod_list.size(); i++)
        if (mod_on[i])
            f << mod_list[i] << "\n";
}

void App::add_mod() {
    std::error_code ec;
    std::filesystem::create_directories(mod_dir(mod_rom), ec);
#if GB_MOBILE
    mod_import = true;
    gb_present_document_picker(mod_dir(mod_rom).c_str());
#else
    nfdchar_t* path = nullptr;
    nfdfilteritem_t filter[1] = {{"IPS patch", "ips"}};
    if (NFD_OpenDialog(&path, filter, 1, nullptr) == NFD_OKAY) {
        std::filesystem::copy_file(path,
            mod_dir(mod_rom) + std::filesystem::path(path).filename().string(),
            std::filesystem::copy_options::overwrite_existing, ec);
        NFD_FreePath(path);
        scan_mods(mod_rom);
    }
#endif
}

void App::draw_mods(float w, float h) {
#if GB_MOBILE
    float ui = std::max(1.0f, ImGui::GetIO().FontGlobalScale);
    int rows = 4;
    float pw = w * 0.86f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f * ui, 12.0f * ui));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * ui, 12.0f * ui));
#else
    float ui = 1.0f;
    int rows = 5;
    float pw = std::min(w * 0.62f, 440.0f);
#endif
    float pad = 22.0f * ui, btn_h = 40.0f * ui;
    float row_h = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    float head_h = ImGui::GetTextLineHeight() + pad;
    float ph = std::min(head_h + pad * 2.0f + rows * row_h + btn_h, h * 0.85f);

    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    if (ImGui::BeginPopupModal("mods", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBackground)) {

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImVec2 sheet1(wp.x + ws.x, wp.y + ws.y);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(wp, sheet1, theme::at().panel, 22.0f * ui);
        // the sheet is opaque, so the field goes onto it rather than behind it
        if (iridescence > 0.0f)
            iri::field_rounded(dl, wp, sheet1, 22.0f * ui,
                               (float)ImGui::GetTime(), iridescence * 0.5f);

        std::string title = display_name(mod_rom);
        ImVec2 ts = ImGui::CalcTextSize(title.c_str());
        dl->AddText(ImVec2(wp.x + (ws.x - ts.x) * 0.5f, wp.y + pad * 0.55f),
                    theme::at().text, title.c_str());
        float sep_y = wp.y + pad * 0.55f + ts.y + pad * 0.45f;
        dl->AddLine(ImVec2(wp.x + pad, sep_y), ImVec2(sheet1.x - pad, sep_y),
                    glass::border(), glass::hairline());

        float inner_w = ws.x - pad * 2.0f;
        float body_y  = sep_y - wp.y + pad * 0.6f;
        float body_h  = ws.y - body_y - pad - btn_h - ImGui::GetStyle().ItemSpacing.y;

        ImGui::SetCursorPos(ImVec2(pad, body_y));
        ImGui::BeginChild("mods_body", ImVec2(inner_w, body_h), false,
                          ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        if (mod_list.empty())
            ImGui::TextDisabled("no patches yet, add one below");

        float ctrl_gap = 14.0f * ui;
        float right_edge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        ImDrawList* bdl = ImGui::GetWindowDrawList();
        for (int i = 0; i < (int)mod_list.size(); i++) {
            ImGui::PushID(i);

            float fh  = ImGui::GetFrameHeight();
            float box = fh * 0.58f;
            float th  = fh * 0.62f;
            float tw  = th * 1.95f;
            ImVec2 rp = ImGui::GetCursorScreenPos();

            if (mod_select) {
                if (ImGui::InvisibleButton("##sel", ImVec2(box, fh)))
                    mod_sel[i] = mod_sel[i] ? 0 : 1;
                float by = rp.y + (fh - box) * 0.5f;
                ImVec2 b0(rp.x, by), b1(rp.x + box, by + box);
                bdl->AddRectFilled(b0, b1, mod_sel[i] ? theme::at().surface
                                                      : theme::at().track, box * 0.26f);
                glass::rect(bdl, b0, b1, box * 0.26f);
                if (mod_sel[i]) {
                    float t = std::max(1.6f, box * 0.16f);
                    bdl->AddLine(ImVec2(b0.x + box * 0.24f, b0.y + box * 0.52f),
                                 ImVec2(b0.x + box * 0.44f, b0.y + box * 0.72f),
                                 theme::at().accent, t);
                    bdl->AddLine(ImVec2(b0.x + box * 0.44f, b0.y + box * 0.72f),
                                 ImVec2(b0.x + box * 0.78f, b0.y + box * 0.28f),
                                 theme::at().accent, t);
                }
                ImGui::SameLine();
            }

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(mod_list[i].c_str());
            ImGui::SameLine();

            ImVec2 tp(right_edge - ctrl_gap - tw, rp.y);
            ImGui::SetCursorScreenPos(tp);
            if (ImGui::InvisibleButton("##on", ImVec2(tw, fh))) {
                mod_on[i] = mod_on[i] ? 0 : 1;
                save_mods();
            }
            bool hot = ImGui::IsItemHovered();
            bool on  = mod_on[i] != 0;
            float ty = tp.y + (fh - th) * 0.5f;
            ImU32 track = on ? (hot ? theme::at().accent : theme::at().button)
                             : (hot ? theme::at().surface : theme::at().track);
            bdl->AddRectFilled(ImVec2(tp.x, ty), ImVec2(tp.x + tw, ty + th), track, th * 0.5f);
            float kr = th * 0.5f - 3.0f;
            float kx = on ? tp.x + tw - kr - 3.0f : tp.x + kr + 3.0f;
            bdl->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), kr, IM_COL32(255, 255, 255, 255), 24);

            ImGui::PopID();
        }
        {
            ImGuiIO& sio = ImGui::GetIO();
            float cur     = ImGui::GetScrollY();
            float view_h  = ImGui::GetWindowSize().y;
            float content = ImGui::GetCursorPosY();
            float maxs    = std::max(0.0f, content - view_h);
            bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                                  ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (hovered && sio.MouseWheel != 0.0f) {
                float base = (std::abs(mods_scroll - cur) > 0.5f) ? mods_scroll : cur;
                mods_scroll = base - sio.MouseWheel * 60.0f;
            }
            mods_scroll = std::clamp(mods_scroll, 0.0f, maxs);
            if (std::abs(mods_scroll - cur) > 0.5f) {
                float next = cur + (mods_scroll - cur) * std::min(1.0f, sio.DeltaTime * 16.0f);
                if (std::abs(mods_scroll - next) < 0.5f) next = mods_scroll;
                ImGui::SetScrollY(next);
            } else {
                mods_scroll = cur;
            }
        }
        ImGui::EndChild();

        int picked = 0;
        for (size_t i = 0; i < mod_sel.size(); i++)
            picked += mod_sel[i] ? 1 : 0;

        float bw = (inner_w - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
        ImGui::SetCursorPos(ImVec2(pad, ws.y - pad - btn_h));
        if (glass::button("add", ImVec2(bw, btn_h)))
            add_mod();
        ImGui::SameLine();
        if (picked > 0) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.16f, 0.08f, 0.50f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.10f, 0.05f, 0.50f));
            if (glass::button("delete", ImVec2(bw, btn_h)))
                ImGui::OpenPopup("confirm_mod_delete");
            ImGui::PopStyleColor(3);
        } else if (glass::button("select", ImVec2(bw, btn_h))) {
            mod_select = !mod_select;
            std::fill(mod_sel.begin(), mod_sel.end(), 0);
        }
        ImGui::SameLine();
        if (glass::button("apply", ImVec2(bw, btn_h))) {
            save_mods();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        if (ImGui::BeginPopupModal("confirm_mod_delete", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoMove)) {
            std::string ask = "delete " + std::to_string(picked) +
                              (picked == 1 ? " patch ?" : " patches ?");
            ImGui::TextUnformatted(ask.c_str());
            ImGui::Dummy(ImVec2(0, h * 0.02f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
            if (glass::button("delete", ImVec2(bw, btn_h))) {
                std::error_code ec;
                for (size_t i = 0; i < mod_list.size(); i++)
                    if (mod_sel[i])
                        std::filesystem::remove(mod_dir(mod_rom) + mod_list[i], ec);
                scan_mods(mod_rom);
                save_mods();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (glass::button("cancel", ImVec2(bw, btn_h)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
#if GB_MOBILE
    ImGui::PopStyleVar(2);
#endif
}

bool App::trash_button(const char* id, float x, float y, float bw, float bh) {
    ImGui::SetCursorPos(ImVec2(x, y));
    bool clicked = ImGui::Button(id, ImVec2(bw, bh));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
    glass::rect(dl, p0, p1, ImGui::GetStyle().FrameRounding);

    if (trash_sprite) {
        int tw = 1, th = 1;
        SDL_QueryTexture(trash_sprite, nullptr, nullptr, &tw, &th);
        float fit = std::min(p1.x - p0.x, p1.y - p0.y) * 0.46f;
        float iw = fit, ih = fit;
        if (tw > th) ih = fit * (float)th / (float)tw;
        else         iw = fit * (float)tw / (float)th;
        ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
        dl->AddImage((ImTextureID)trash_sprite,
                     ImVec2(c.x - iw * 0.5f, c.y - ih * 0.5f),
                     ImVec2(c.x + iw * 0.5f, c.y + ih * 0.5f),
                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 235));
    }
    return clicked;
}

void App::add_game() {
#if GB_DESKTOP
    nfdchar_t* path = nullptr;
    nfdfilteritem_t filter[1] = {{"Game Boy ROM", "gb,gbc"}};
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
    ImU32 green = glass::fill(hot ? theme::at().accent : theme::at().button);
    ImU32 white = IM_COL32(255, 255, 255, 255);

    dl->AddCircleFilled(ImVec2(cx, cy), r, green, 40);
    glass::circle(dl, ImVec2(cx, cy), r);
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
    ImU32 green = glass::fill(hot ? theme::at().accent : theme::at().button);
    ImU32 white = IM_COL32(255, 255, 255, 255);

    dl->AddCircleFilled(ImVec2(cx, cy), r, green, 40);
    glass::circle(dl, ImVec2(cx, cy), r);
    float s = r * 0.30f;
    dl->PathLineTo(ImVec2(cx + s * 0.6f, cy - s));
    dl->PathLineTo(ImVec2(cx - s * 0.6f, cy));
    dl->PathLineTo(ImVec2(cx + s * 0.6f, cy + s));
    dl->PathStroke(white, 0, r * 0.14f);
    return clicked;
}

namespace {
    enum SettingsTab { TAB_DISPLAY, TAB_MENU, TAB_AUDIO, TAB_SYSTEM, TAB_CONTROLS, TAB_KEYBINDS };
#if GB_MOBILE
    // no display tab on mobile, screen fit, vsync and hidpi are all desktop only
    constexpr SettingsTab kSettingsTabs[] = {TAB_MENU, TAB_AUDIO, TAB_SYSTEM, TAB_CONTROLS};
    const char* const kSettingsNames[] = {"menu", "audio", "system", "controls"};
#else
    constexpr SettingsTab kSettingsTabs[] = {TAB_DISPLAY, TAB_MENU, TAB_AUDIO, TAB_SYSTEM, TAB_KEYBINDS};
    const char* const kSettingsNames[] = {"display", "menu", "audio", "system", "keybinds"};
#endif
    constexpr int kSettingsTabCount = (int)(sizeof(kSettingsTabs) / sizeof(kSettingsTabs[0]));
}

void App::draw_settings(float w, float h) {
#if GB_MOBILE
    float ui = std::max(1.0f, ImGui::GetIO().FontGlobalScale);
    float r = std::max(w, h) * 0.0245f;
    float m = r * 0.55f;
    float cog_y = r * 2.4f + h * 0.03f; // the ios menu already fills the bottom with add game
    float cog_x = w - m - r;            // opposite the category toggle on the left
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f * ui, 12.0f * ui));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * ui, 12.0f * ui));
#else
    float ui = 1.0f;
    float r = std::max(14.0f, std::min(w, h) * 0.030f);
    float m = r * 1.4f;
    float cog_y = h - m - r;
    float cog_x = m + r;
#endif
    if (cog_button(cog_x, cog_y, r)) {
        settings_open = true;
        settings_tab = (settings_tab < kSettingsTabCount) ? settings_tab : 0;
        ImGui::OpenPopup("settings");
    }
#if GB_DESKTOP
    // on mobile render_game_mobile draws its own back button and never reaches here, without
    // the guard the menu frame that load_rom flips to PLAYING draws one at the wrong y
    if (state == AppState::PLAYING && back_button(m + r, m + r, r))
        state = AppState::MENU;
#endif

    float pad = 22.0f * ui, tab_h = 32.0f * ui, tab_gap = 8.0f * ui, close_h = 40.0f * ui;
    float tab_text = 0.0f;
    for (int i = 0; i < kSettingsTabCount; i++)
        tab_text = std::max(tab_text, ImGui::CalcTextSize(kSettingsNames[i]).x);

    float row_h = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
#if GB_MOBILE
    int rows = 3;
    float pw = w * 0.86f;
#else
    int rows = 5;
    float pw = std::min(w * 0.62f, 440.0f);
#endif
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

        float tab_room = ws.x - 26.0f * ui * 2.0f - tab_gap * (kSettingsTabCount - 1);
        float tab_w    = std::min(tab_text + 34.0f * ui, tab_room / kSettingsTabCount);
        float strip_w  = tab_w * kSettingsTabCount + tab_gap * (kSettingsTabCount - 1);
        float tab_x0   = wp.x + (ws.x - strip_w) * 0.5f;

        for (int i = 0; i < kSettingsTabCount; i++) {
            float tx = tab_x0 + i * (tab_w + tab_gap);
            ImGui::SetCursorScreenPos(ImVec2(tx, wp.y));
            ImGui::PushID(i);
            bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tab_w, tab_h));
            bool hot = ImGui::IsItemHovered();
            ImGui::PopID();
            if (clicked) settings_tab = i;

            bool on = (settings_tab == i);
            ImU32 col = glass::fill(on  ? theme::at().accent
                                    : hot ? theme::at().surface_hi
                                          : theme::at().surface);
            dl->AddRectFilled(ImVec2(tx, wp.y), ImVec2(tx + tab_w, wp.y + tab_h + 26.0f * ui),
                              col, 12.0f * ui, ImDrawFlags_RoundCornersTop);
            glass::rect(dl, ImVec2(tx, wp.y), ImVec2(tx + tab_w, wp.y + tab_h),
                        12.0f * ui, ImDrawFlags_RoundCornersTop);
            ImVec2 ts = ImGui::CalcTextSize(kSettingsNames[i]);
            dl->PushClipRect(ImVec2(tx, wp.y), ImVec2(tx + tab_w, wp.y + tab_h), true);
            dl->AddText(ImVec2(tx + (tab_w - ts.x) * 0.5f, wp.y + (tab_h - ts.y) * 0.5f),
                        theme::at().text, kSettingsNames[i]);
            dl->PopClipRect();
        }

        ImVec2 sheet0(wp.x, wp.y + tab_h);
        ImVec2 sheet1(wp.x + ws.x, wp.y + ws.y);
        dl->AddRectFilled(sheet0, sheet1, theme::at().panel, 22.0f * ui);
        // the sheet is opaque, so the field goes onto it rather than behind it
        if (iridescence > 0.0f)
            iri::field_rounded(dl, sheet0, sheet1, 22.0f * ui,
                               (float)ImGui::GetTime(), iridescence * 0.5f);

        float inner_w = ws.x - pad * 2.0f;
        float body_y  = tab_h + pad;
        float body_h  = ws.y - body_y - pad * 2.0f - close_h;

        ImGui::SetCursorPos(ImVec2(pad, body_y));
        // the smooth scrolling below drives SetScrollY itself, imgui's own bar is both
        // redundant and picks up ChildRounding, which is what rounded off the top and
        // bottom of the list
        ImGui::BeginChild("settings_body", ImVec2(inner_w, body_h), false,
                          ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        ImVec2 body_min = ImGui::GetWindowPos();
        ImVec2 body_max = ImVec2(body_min.x + ImGui::GetWindowSize().x,
                                 body_min.y + ImGui::GetWindowSize().y);
        float ctrl_gap = 18.0f * ui;
        float ctrl_w = 132.0f * ui;
        float right_edge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        bool any_open = false;
            auto combo_row = [&](const char* text, const char* id,
                                 const char* const* items, int n, int cur) {
                int picked = cur;
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine();

                float cw = ctrl_w;
                float chh = ImGui::GetFrameHeight();
                ImVec2 cpos = ImGui::GetCursorScreenPos();
                cpos.x = right_edge - ctrl_gap - cw;
                ImGui::SetCursorScreenPos(cpos);
                bool covered = any_open;

                ImGui::SetNextItemWidth(cw);
                ImGui::SetNextWindowSizeConstraints(ImVec2(cw, 0.0f), ImVec2(cw, 99999.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
                bool open = ImGui::BeginCombo(id, "", ImGuiComboFlags_NoArrowButton);
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                bool hot_c = open || ImGui::IsItemHovered();
                if (open) {
                    any_open = true;
                    ImGui::SetWindowPos(ImVec2(cpos.x, cpos.y + chh));

                    float item_h = chh * 0.86f;
                    float top    = cpos.y + chh * 0.45f;
                    float bottom = cpos.y + chh + n * item_h;
                    // the open list floats over the panel, clipping it to the scrolling
                    // body cut the last options off the bottom, it only has to stay on screen
                    ImDrawList* pdl = ImGui::GetWindowDrawList();
                    pdl->PushClipRectFullScreen();
                    pdl->AddRectFilled(ImVec2(cpos.x - 1.0f, top),
                                       ImVec2(cpos.x + cw + 1.0f, bottom),
                                       theme::at().panel, 8.0f,
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
                                               hot_i ? theme::at().button
                                                     : theme::at().surface, 5.0f, fl);
                        }
                        ImVec2 its = ImGui::CalcTextSize(items[i]);
                        pdl->AddText(ImVec2(cpos.x + cw - 12.0f - its.x,
                                            ip.y + (item_h - its.y) * 0.5f),
                                     theme::at().text, items[i]);
                        ImGui::PopID();
                    }
                    ImGui::PopStyleVar();
                    pdl->PopClipRect();
                    ImGui::EndCombo();
                }

                if (!covered) {
                    ImDrawList* fg = ImGui::GetForegroundDrawList();
                    fg->PushClipRect(body_min, body_max, true);
                    ImU32 cbg = glass::fill(hot_c ? theme::at().accent
                                                  : theme::at().button);
                    fg->AddRectFilled(cpos, ImVec2(cpos.x + cw, cpos.y + chh), cbg, 8.0f);
                    glass::rect(fg, cpos, ImVec2(cpos.x + cw, cpos.y + chh), 8.0f);
                    ImVec2 cts = ImGui::CalcTextSize(items[cur]);
                    fg->AddText(ImVec2(cpos.x + cw - 12.0f - cts.x,
                                       cpos.y + (chh - cts.y) * 0.5f),
                                theme::at().text, items[cur]);
                    fg->PopClipRect();
                }
                return picked;
            };

            auto toggle_row = [&](const char* text, const char* id, bool on) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine();

                float fh = ImGui::GetFrameHeight();
                float th = fh * 0.70f;
                float tw = th * 1.95f;
                ImVec2 tp = ImGui::GetCursorScreenPos();
                tp.x = right_edge - ctrl_gap - tw;
                ImGui::SetCursorScreenPos(tp);
                bool hit = ImGui::InvisibleButton(id, ImVec2(tw, fh));
                bool hot_t = ImGui::IsItemHovered();
                float ty = tp.y + (fh - th) * 0.5f;
                ImDrawList* tdl = ImGui::GetWindowDrawList();
                // the off state needs the groove colour too, panel is the sheet behind it
                ImU32 track = on ? (hot_t ? theme::at().accent : theme::at().button)
                                 : (hot_t ? theme::at().surface : theme::at().track);
                tdl->AddRectFilled(ImVec2(tp.x, ty), ImVec2(tp.x + tw, ty + th), track, th * 0.5f);
                float kr = th * 0.5f - 3.0f;
                float kx = on ? tp.x + tw - kr - 3.0f : tp.x + kr + 3.0f;
                tdl->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), kr,
                                     IM_COL32(255, 255, 255, 255), 24);
                return hit;
            };

            auto slider_row = [&](const char* text, const char* id, float v) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine();

                float fh = ImGui::GetFrameHeight();
                float sw = ctrl_w;
                float th = fh * 0.34f;
                ImVec2 sp = ImGui::GetCursorScreenPos();
                sp.x = right_edge - ctrl_gap - sw;
                ImGui::SetCursorScreenPos(sp);
                ImGui::InvisibleButton(id, ImVec2(sw, fh));
                bool hot_s = ImGui::IsItemHovered() || ImGui::IsItemActive();
                if (ImGui::IsItemActive())
                    v = std::clamp((ImGui::GetIO().MousePos.x - sp.x) / sw, 0.0f, 1.0f);

                float ty = sp.y + (fh - th) * 0.5f;
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                // its own colour, not the sheet's, or the unfilled part of the bar
                // disappears into the panel behind it
                sdl->AddRectFilled(ImVec2(sp.x, ty), ImVec2(sp.x + sw, ty + th),
                                   theme::at().track, th * 0.5f);
                if (v > 0.0f)
                    sdl->AddRectFilled(ImVec2(sp.x, ty), ImVec2(sp.x + sw * v, ty + th),
                                       hot_s ? theme::at().accent
                                             : theme::at().button, th * 0.5f);
                sdl->AddCircleFilled(ImVec2(sp.x + sw * v, ty + th * 0.5f), fh * 0.26f,
                                     IM_COL32(255, 255, 255, 255), 24);
                return v;
            };

        // each case is braced, the row helpers above declare locals inside them
        switch (kSettingsTabs[settings_tab]) {
        case TAB_DISPLAY: {
#if GB_DESKTOP
            const char* modes[3] = {"normal", "crop", "stretch"};
            int fit = combo_row("screen fit", "##fit", modes, 3, (int)scale_mode);
            if (fit != (int)scale_mode) {
                scale_mode = (ScaleMode)fit;
                save_settings();
            }
#endif
#if GB_DESKTOP
            if (toggle_row("vsync", "##vsync", vsync)) {
                vsync = !vsync;
                SDL_RenderSetVSync(renderer, vsync ? 1 : 0);
                save_settings();
            }
#endif
#if GB_DESKTOP
            if (toggle_row("hidpi", "##hidpi", hidpi)) {
                hidpi = !hidpi;
                video_reset = true;
                settings_open = false;
                rebind_target = -1;
                ImGui::CloseCurrentPopup();
                save_settings();
            }
#endif
            break;
        }

        case TAB_MENU: {
            int fps = combo_row("menu frame cap", "##fps", kFpsNames, 6, fps_index);
            if (fps != fps_index) {
                fps_index = fps;
                save_settings();
            }

            if (toggle_row("render cartridge", "##cart", render_cartridge)) {
                render_cartridge = !render_cartridge;
                save_settings();
            }
            break;
        }

#if GB_MOBILE
        case TAB_CONTROLS: {
            if (toggle_row("joystick", "##stick", joystick_mode)) {
                joystick_mode = !joystick_mode;
                release_touches();
                save_settings();
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("overlay");
            ImGui::SameLine();
            ImVec2 bp = ImGui::GetCursorScreenPos();
            bp.x = right_edge - ctrl_gap - ctrl_w;
            ImGui::SetCursorScreenPos(bp);
            if (glass::button("edit layout", ImVec2(ctrl_w, 0))) {
                begin_layout_edit();
                settings_open = false;
                ImGui::CloseCurrentPopup();
            }
            break;
        }
#endif

        case TAB_AUDIO: {
            volume = slider_row("volume", "##volume", volume);
            if (ImGui::IsItemDeactivated())
                save_settings();
            break;
        }

        case TAB_SYSTEM: {
            static const char* kThemeNames[3] = {"auto", "light", "dark"};
            int tm = combo_row("theme", "##theme", kThemeNames, 3, theme_mode);
            if (tm != theme_mode) {
                theme_mode = tm;
                sync_theme();
                save_settings();
            }
            if (toggle_row("game boy color", "##cgb", cgb_enabled)) {
                cgb_enabled = !cgb_enabled;
                refresh_palette();
                save_settings();
            }
            if (toggle_row("colourise mono games", "##dmgcol", dmg_colorize)) {
                dmg_colorize = !dmg_colorize;
                refresh_palette();
                save_settings();
            }
            break;
        }

        default: {
            const char* names[8] = {"right", "left", "up", "down", "a", "b", "select", "start"};
            const int order[8] = {2, 3, 1, 0, 4, 5, 6, 7};
            for (int k = 0; k < 8; k++) {
                int i = order[k];
                ImGui::PushID(i);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(names[i]);
                ImGui::SameLine();
                bool waiting = (rebind_target == i);
                if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.47f, 0.03f, 0.50f));
                std::string label = waiting ? "press a key" : SDL_GetKeyName(keybinds[i]);
                for (char& ch : label) ch = (char)std::tolower((unsigned char)ch);
                if (!waiting) {
                    SDL_Keycode kc = keybinds[i];
                    if (kc == SDLK_UP || kc == SDLK_DOWN || kc == SDLK_LEFT || kc == SDLK_RIGHT)
                        label += " arrow";
                }
                ImVec2 bp = ImGui::GetCursorScreenPos();
                bp.x = right_edge - ctrl_gap - ctrl_w;
                ImGui::SetCursorScreenPos(bp);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(1.0f, 0.5f));
                if (glass::button(label.c_str(), ImVec2(ctrl_w, 0)))
                    rebind_target = waiting ? -1 : i;
                ImGui::PopStyleVar();
                if (waiting) ImGui::PopStyleColor();
                ImGui::PopID();
            }
            break;
        }
        }
        {
            ImGuiIO& sio = ImGui::GetIO();
            float cur     = ImGui::GetScrollY();
            float view_h  = ImGui::GetWindowSize().y;
            float content = ImGui::GetCursorPosY();
            float maxs    = std::max(0.0f, content - view_h);
            bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                                  ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (hovered && sio.MouseWheel != 0.0f) {
                float base = (std::abs(settings_scroll - cur) > 0.5f) ? settings_scroll : cur;
                settings_scroll = base - sio.MouseWheel * 60.0f;
            }
            settings_scroll = std::clamp(settings_scroll, 0.0f, maxs);
            if (std::abs(settings_scroll - cur) > 0.5f) {
                float next = cur + (settings_scroll - cur) * std::min(1.0f, sio.DeltaTime * 16.0f);
                if (std::abs(settings_scroll - next) < 0.5f) next = settings_scroll;
                ImGui::SetScrollY(next);
            } else {
                settings_scroll = cur;
            }
        }
        ImGui::EndChild();

        ImGui::SetCursorPos(ImVec2(pad, ws.y - pad - close_h));
        if (glass::button("close", ImVec2(inner_w, close_h))) {
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
#if GB_MOBILE
    ImGui::PopStyleVar(2);
#endif
}

void App::handle_events() {
#if GB_MOBILE
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
#if GB_MOBILE
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

#if GB_DESKTOP
        if (event.type == SDL_WINDOWEVENT &&
            (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             event.window.event == SDL_WINDOWEVENT_MOVED))
            save_settings();
#endif

#if GB_MOBILE
        // stop rendering the moment the os tells us we are leaving the foreground, resume when back
        if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND)
            active = false;
        if (event.type == SDL_APP_DIDENTERFOREGROUND)
            active = true;
#endif

#if GB_ANDROID
        // android can drop the gl context while backgrounded, which invalidates every
        // texture we hold, so rebuild the whole video stack when it comes back
        if (event.type == SDL_RENDER_DEVICE_RESET || event.type == SDL_RENDER_TARGETS_RESET)
            video_reset = true;

        // the system back gesture would otherwise quit the app outright
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_AC_BACK) {
            if (settings_open) {
                settings_open = false;
                rebind_target = -1;
            } else if (state == AppState::PLAYING) {
                state = AppState::MENU;
            }
            continue;
        }
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
    #if GB_MOBILE
        if (editing_layout) {
            render_layout_editor();
        } else
#endif
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

#if GB_MOBILE
        // on ios the joypad and the back button are on-screen touch zones
        if (state == AppState::PLAYING)
            handle_touch_mobile(event);
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

    // every widget colour is derived from the live palette so the whole ui moves at once
    apply_theme_colors();
}

// resolves the palette for this frame and eases the colour backdrop in and out. the os
// preference is polled rarely, reading it is a syscall and it almost never changes
void App::sync_theme() {
    static int poll = 0;
    static bool os_dark = true;
    if (poll-- <= 0) {
        os_dark = gb_system_dark();
        poll = 60;
    }

    bool want = (theme_mode == 0) ? os_dark : (theme_mode == 2);
    if (want != theme_dark || theme::at().dark != want) {
        theme_dark = want;
        theme::use(want);
        apply_theme_colors();
    }

    // a colour cartridge on screen, or the colour shelf in the menu
    bool colour_ctx = (state == AppState::PLAYING) ? (mem && mem->cgb_mode)
                                                   : (library_tab == 1);
    float dt = ImGui::GetIO().DeltaTime;
    iridescence += ((colour_ctx ? 1.0f : 0.0f) - iridescence) * (1.0f - std::exp(-6.0f * dt));
    if (iridescence < 0.002f) iridescence = 0.0f;
    if (iridescence > 0.998f) iridescence = 1.0f;
    theme::g_iridescence = iridescence;
}

// the blob field, drawn everywhere except the lcd itself so the picture stays untouched
void App::draw_iridescence(float w, float h, const SDL_Rect* keep_clear) {
    if (iridescence <= 0.0f)
        return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = (float)ImGui::GetTime();
    float strength = iridescence * (theme::at().dark ? 1.0f : 0.85f);

    if (!keep_clear) {
        iri::field(dl, ImVec2(0, 0), ImVec2(w, h), t, strength);
        return;
    }
    // four bands around the screen rect, so the field never paints over the game
    float x0 = (float)keep_clear->x, y0 = (float)keep_clear->y;
    float x1 = x0 + keep_clear->w,   y1 = y0 + keep_clear->h;
    const ImVec2 bands[4][2] = {
        {ImVec2(0, 0),   ImVec2(w, y0)},
        {ImVec2(0, y1),  ImVec2(w, h)},
        {ImVec2(0, y0),  ImVec2(x0, y1)},
        {ImVec2(x1, y0), ImVec2(w, y1)},
    };
    for (int i = 0; i < 4; i++) {
        if (bands[i][1].x - bands[i][0].x <= 0.5f || bands[i][1].y - bands[i][0].y <= 0.5f)
            continue;
        dl->PushClipRect(bands[i][0], bands[i][1], true);
        iri::field(dl, ImVec2(0, 0), ImVec2(w, h), t, strength);
        dl->PopClipRect();
    }
}

// imgui keeps one global style, so this is re-run whenever the palette changes
void App::apply_theme_colors() {
    const Theme& th = theme::at();
    auto v = [](ImU32 c, float a = 1.0f) {
        return ImVec4(((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                      ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                      ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f, a);
    };
    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_WindowBg]             = v(th.page);
    c[ImGuiCol_Button]               = v(th.button, 0.50f);
    c[ImGuiCol_ButtonHovered]        = v(th.surface_hi, 0.50f);
    c[ImGuiCol_ButtonActive]         = v(th.surface, 0.50f);
    c[ImGuiCol_Text]                 = v(th.text);
    c[ImGuiCol_TitleBg]              = v(th.panel);
    c[ImGuiCol_TitleBgActive]        = v(th.surface);
    c[ImGuiCol_FrameBg]              = v(th.surface);
    c[ImGuiCol_FrameBgHovered]       = v(th.surface_hi);
    c[ImGuiCol_FrameBgActive]        = v(th.button);
    c[ImGuiCol_PopupBg]              = v(th.panel);
    c[ImGuiCol_Header]               = v(th.surface_hi);
    c[ImGuiCol_HeaderHovered]        = v(th.accent);
    c[ImGuiCol_HeaderActive]         = v(th.surface);
    c[ImGuiCol_Border]               = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.78f);
    c[ImGuiCol_ScrollbarBg]          = v(th.page);
    c[ImGuiCol_ScrollbarGrab]        = v(th.surface);
    c[ImGuiCol_ScrollbarGrabHovered] = v(th.surface_hi);
    c[ImGuiCol_ScrollbarGrabActive]  = v(th.accent);
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
#if GB_ANDROID
// apk assets are not files on disk, sdl_rwops routes relative paths to the asset manager
static std::string read_asset(const std::string& name) {
    SDL_RWops* rw = SDL_RWFromFile(name.c_str(), "rb");
    if (!rw) return {};
    Sint64 sz = SDL_RWsize(rw);
    std::string out;
    if (sz > 0) {
        out.resize((size_t)sz);
        SDL_RWread(rw, out.data(), 1, (size_t)sz);
    }
    SDL_RWclose(rw);
    return out;
}

// assets cannot be enumerated, so the build writes a file listing them instead
static std::vector<std::string> asset_manifest(const char* name) {
    std::vector<std::string> out;
    std::string text = read_asset(name);
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty()) out.push_back(line);
        start = end + 1;
    }
    return out;
}
#endif

#if GB_MOBILE
std::vector<std::string> App::bundled_roms() {
#if GB_ANDROID
    return asset_manifest("game-roms.txt");
#else
    std::vector<std::string> out;
    std::error_code ec;
    char* base = SDL_GetBasePath();
    std::string b = base ? base : "";
    if (base) SDL_free(base);
    for (const auto& e : std::filesystem::directory_iterator(b + "game-roms/", ec))
        if (is_rom_file(e.path()))
            out.push_back(e.path().filename().string());
    return out;
#endif
}

void App::copy_bundled_rom(const std::string& name) {
#if GB_ANDROID
    std::string data = read_asset("game-roms/" + name);
    if (data.empty()) return;
    std::ofstream out(rom_folder + name, std::ios::binary | std::ios::trunc);
    out.write(data.data(), (std::streamsize)data.size());
#else
    char* base = SDL_GetBasePath();
    std::string b = base ? base : "";
    if (base) SDL_free(base);
    std::error_code ec;
    std::filesystem::copy_file(b + "game-roms/" + name, rom_folder + name,
                               std::filesystem::copy_options::skip_existing, ec);
#endif
}
#endif

// the two cover sets live apart, because a title released on both systems has a cover in
// each and they normalise to the same name
const std::vector<std::string>& App::artwork_files(bool gbc) {
    std::vector<std::string>& cache = gbc ? artwork_cache_gbc : artwork_cache_gb;
    if (!cache.empty())
        return cache;
#if GB_ANDROID
    cache = asset_manifest(gbc ? "artworks-gbc.txt" : "artworks-gb.txt");
#else
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(artwork_folder + (gbc ? "gbc/" : "gb/"), ec))
        if (e.path().extension() == ".png")
            cache.push_back(e.path().filename().string());
#endif
    return cache;
}

// searches one set, returns the file name only, empty when nothing is close enough
std::string App::match_artwork(const std::string& target, bool gbc) {
    std::string best_name;
    size_t best_score = std::string::npos;

    for (const std::string& file : artwork_files(gbc)) {
        std::string cand = normalize(file.substr(0, file.size() - 4));
        if (cand.empty()) continue;

        if (cand == target)
            return file;                            // exact, done

        // accept only if one contains the other; score by length difference
        if (cand.find(target) != std::string::npos ||
            target.find(cand) != std::string::npos) {
            size_t score = (cand.size() > target.size())
                         ? cand.size() - target.size()
                         : target.size() - cand.size();
            if (score < best_score) {
                best_score = score;
                best_name = file;
            }
        }
    }
    return best_name;
}

/* a rom looks in its own system's covers first. space invaders came out on both, and the
   two covers normalise to the same name, so one folder could only ever return whichever
   the directory happened to list first. the other set is still a fallback, for a game
   that only ever had a cover on one side */
std::string App::closest_artwork(const std::string& rom_name, bool gbc) {
    std::string target = normalize(rom_name);

    std::string hit = match_artwork(target, gbc);
    if (!hit.empty())
        return artwork_folder + (gbc ? "gbc/" : "gb/") + hit;

    hit = match_artwork(target, !gbc);
    if (!hit.empty())
        return artwork_folder + (gbc ? "gb/" : "gbc/") + hit;

    return {};
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

    // drop a trailing rom extension if present
    if (out.size() >= 4 && out.substr(out.size() - 4) == ".gbc")
        out.erase(out.size() - 4);
    else if (out.size() >= 3 && out.substr(out.size() - 3) == ".gb")
        out.erase(out.size() - 3);

    // trim trailing spaces left behind by removed tags
    while (!out.empty() && out.back() == ' ')
        out.pop_back();

    static const std::string articles[] = {", The", ", An", ", A"};
    for (const std::string& art : articles) {
        size_t at = out.find(art);
        if (at == std::string::npos) continue;
        size_t end = at + art.size();
        if (end != out.size() && out.compare(end, 3, " - ") != 0) continue;
        out = art.substr(2) + " " + out.substr(0, at) + out.substr(end);
        break;
    }

    return out;
}

void App::render_menu() {
#if GB_MOBILE
    render_menu_mobile();
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

    sync_theme();
    draw_iridescence(w, h);

    // text drawn straight onto the page has to invert with it, panels keep their own
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(theme::at().text_page));

    std::vector<int> view;
    int count = library_view(view);

    if (count == 0) {
        centre_text(library_tab ? "no color games" : "no game boy games", h * 0.45f);
    } else {
        float cover    = h * 0.50f;
        float cover_cx = w * 0.5f;
        float cover_cy = h * 0.36f;
        float spacing  = cover * 0.55f;

        if (!settings_open) {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) carousel_vel += 3.5f;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  carousel_vel -= 3.5f;
            if (io.MouseWheel != 0.0f) carousel_vel += (io.MouseWheel > 0.0f ? -3.5f : 3.5f);
        }

        // starts below the tab pill, an overlap here silently ate every click on it
        ImGui::SetCursorPos(ImVec2(0, cover_cy - cover * 0.50f));
        ImGui::InvisibleButton("swipe", ImVec2(w, cover * 1.10f));
        if (ImGui::IsItemActivated()) {
            carousel_drag_start = carousel_pos;
            carousel_vel = 0.0f;
        }
        /* a hitched frame must not throw the carousel across the shelf, and a very short
           one must not divide into a huge flick velocity. clamping the step keeps the
           motion even however unevenly frames actually arrive */
        float dt = std::min(std::max(io.DeltaTime, 0.002f), 1.0f / 30.0f);

        if (ImGui::IsItemActive()) {
            float np = carousel_drag_start - ImGui::GetMouseDragDelta(0, 0.0f).x / spacing;
            carousel_vel = carousel_vel * 0.3f + ((np - carousel_pos) / dt) * 0.7f;
            carousel_vel = std::max(-90.0f, std::min(90.0f, carousel_vel));
            carousel_pos = np;
        } else if (std::abs(carousel_vel) > 0.4f) {
            carousel_pos += carousel_vel * dt;
            carousel_vel *= std::exp(-3.5f * dt);
        } else {
            carousel_vel = 0.0f;
            float target = std::round(carousel_pos);
            carousel_pos += (target - carousel_pos) * std::min(1.0f, dt * 14.0f);
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
        struct Card { int r; float rel, cx, sz; };
        std::vector<Card> cards;
        int span = (int)std::ceil((w * 0.5f) / (cover * 0.62f)) + 1;
        for (int k = base - span; k <= base + span + 1; k++) {
            float rel = k - carousel_pos;
            float cx  = cover_cx + rel * (cover * 0.62f);
            float sz  = cover * std::max(0.45f, 1.0f - std::abs(rel) * 0.10f);
            float half = sz * 0.5f;
            if (cx + half < 0.0f || cx - half > w) continue;
            cards.push_back({view[wrap(k)], rel, cx, sz});
        }
        std::sort(cards.begin(), cards.end(),
                  [](const Card& x, const Card& y) { return std::abs(x.rel) > std::abs(y.rel); });
        const float cart_ar = 700.0f / 800.0f;
        const float slot_x0 = 0.114f, slot_x1 = 0.882f;
        const float slot_y0 = 0.280f, slot_y1 = 0.896f;
        for (const Card& cd : cards) {
            ImU32 tint = IM_COL32(255, 255, 255, 255);

            if (!render_cartridge || !cartridge_sprite) {
                float hs = cd.sz * 0.5f;
                ImVec2 a0(org.x + cd.cx - hs, org.y + cover_cy - hs);
                ImVec2 a1(a0.x + cd.sz, a0.y + cd.sz);
                float round = cd.sz * 0.04f;
                if (cover_list[cd.r]) {
                    int tw = 1, th = 1;
                    SDL_QueryTexture(cover_list[cd.r], nullptr, nullptr, &tw, &th);
                    float src_ar = (float)tw / (float)th;
                    float aw = cd.sz, ah = cd.sz;
                    if (src_ar > 1.0f) ah = cd.sz / src_ar;
                    else               aw = cd.sz * src_ar;
                    a0 = ImVec2(org.x + cd.cx - aw * 0.5f, org.y + cover_cy - ah * 0.5f);
                    a1 = ImVec2(a0.x + aw, a0.y + ah);
                }
                if (rect_shadow) {
                    float px = (a1.x - a0.x) * rect_pad;
                    float py = (a1.y - a0.y) * rect_pad;
                    float drop = cd.sz * 0.035f;
                    dl->AddImage((ImTextureID)rect_shadow,
                                 ImVec2(a0.x - px, a0.y - py + drop),
                                 ImVec2(a1.x + px, a1.y + py + drop),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 170));
                }
                if (cover_list[cd.r]) {
                    dl->AddImageRounded((ImTextureID)cover_list[cd.r], a0, a1,
                                        ImVec2(0, 0), ImVec2(1, 1), tint, round);
                } else {
                    dl->AddRectFilled(a0, a1, theme::at().placeholder, round);
                    std::string nm = display_name(rom_list[cd.r]);
                    ImFont* fnt = ImGui::GetFont();
                    float   fsz = ImGui::GetFontSize();
                    float   wrap = (a1.x - a0.x) * 0.88f;
                    ImVec2  ts = fnt->CalcTextSizeA(fsz, FLT_MAX, wrap, nm.c_str());
                    dl->AddText(fnt, fsz,
                                ImVec2((a0.x + a1.x) * 0.5f - ts.x * 0.5f,
                                       (a0.y + a1.y) * 0.5f - ts.y * 0.5f),
                                theme::at().text, nm.c_str(), nullptr, wrap);
                }
                continue;
            }

            float cart_h = cd.sz;
            float cart_w = cd.sz * cart_ar;
            ImVec2 p0(org.x + cd.cx - cart_w * 0.5f, org.y + cover_cy - cart_h * 0.5f);
            ImVec2 p1(p0.x + cart_w, p0.y + cart_h);

            if (cartridge_shadow) {
                float px = cart_w * shadow_pad_x;
                float py = cart_h * shadow_pad_y;
                float drop = cart_h * 0.035f;
                dl->AddImage((ImTextureID)cartridge_shadow,
                             ImVec2(p0.x - px, p0.y - py + drop),
                             ImVec2(p1.x + px, p1.y + py + drop),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 170));
            }
            if (cartridge_sprite)
                dl->AddImage((ImTextureID)cartridge_sprite, p0, p1,
                             ImVec2(0, 0), ImVec2(1, 1), tint);

            ImVec2 s0(p0.x + cart_w * slot_x0, p0.y + cart_h * slot_y0);
            ImVec2 s1(p0.x + cart_w * slot_x1, p0.y + cart_h * slot_y1);
            float slot_w = s1.x - s0.x;
            float slot_h = s1.y - s0.y;
            float round  = slot_h * 0.06f;
            if (cover_list[cd.r]) {
                int tw = 1, th = 1;
                SDL_QueryTexture(cover_list[cd.r], nullptr, nullptr, &tw, &th);
                float src_ar  = (float)tw / (float)th;
                float slot_ar = slot_w / slot_h;
                float art_w = slot_w, art_h = slot_h;
                if (src_ar > slot_ar) art_h = slot_w / src_ar;
                else                  art_w = slot_h * src_ar;
                art_w *= 0.97f;
                art_h *= 0.97f;
                ImVec2 d0((s0.x + s1.x) * 0.5f - art_w * 0.5f, s0.y);
                ImVec2 d1(d0.x + art_w, d0.y + art_h);
                dl->AddImageRounded((ImTextureID)cover_list[cd.r], d0, d1,
                                    ImVec2(0, 0), ImVec2(1, 1), tint, round);
            } else {
                dl->AddRectFilled(s0, s1, theme::at().placeholder, round);
                std::string nm = display_name(rom_list[cd.r]);
                ImFont* fnt = ImGui::GetFont();
                float   fsz = ImGui::GetFontSize();
                float   wrap = slot_w * 0.88f;
                ImVec2  ts = fnt->CalcTextSizeA(fsz, FLT_MAX, wrap, nm.c_str());
                dl->AddText(fnt, fsz,
                            ImVec2((s0.x + s1.x) * 0.5f - ts.x * 0.5f,
                                   (s0.y + s1.y) * 0.5f - ts.y * 0.5f),
                            theme::at().text, nm.c_str(), nullptr, wrap);
            }
        }

        float title_y = cover_cy + cover * 0.58f;
        centre_text(display_name(rom_list[r_centre]).c_str(), title_y);

        float btn_h  = h * 0.09f;
        float gap    = w * 0.015f;
        float row_w  = w * 0.14f + gap + w * 0.10f;
        float del_w  = btn_h;
        float rest   = row_w - del_w - gap * 2.0f;
        float play_w = rest * 0.56f;
        float mods_w = rest - play_w;
        float row_x  = (w - row_w) * 0.5f;
        float row_y  = title_y + h * 0.05f;

        ImGui::SetCursorPos(ImVec2(row_x, row_y));
        if (glass::button("play", ImVec2(play_w, btn_h)))
            load_rom(rom_list[r_centre]);

        ImGui::SetCursorPos(ImVec2(row_x + play_w + gap, row_y));
        if (glass::button("mods", ImVec2(mods_w, btn_h))) {
            scan_mods(rom_list[r_centre]);
            ImGui::OpenPopup("mods");
        }

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.16f, 0.08f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.10f, 0.05f, 0.50f));
        if (trash_button("##delete", row_x + play_w + mods_w + gap * 2.0f, row_y, del_w, btn_h))
            ImGui::OpenPopup("confirm_delete");
        ImGui::PopStyleColor(3);

        draw_mods(w, h);

        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        if (ImGui::BeginPopupModal("confirm_delete", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(("delete " + display_name(rom_list[r_centre]) + " ?").c_str());
            ImGui::Dummy(ImVec2(0, h * 0.02f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
            if (glass::button("delete", ImVec2(w * 0.10f, btn_h))) {
                std::error_code ec;
                std::filesystem::remove(rom_folder + rom_list[r_centre], ec);
                scan_roms();
                carousel_pos = carousel_target = (centre > 0) ? (float)(centre - 1) : 0.0f;
                carousel_vel = 0.0f;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (glass::button("cancel", ImVec2(w * 0.10f, btn_h)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        std::string page = std::to_string(centre + 1) + " / " + std::to_string(count);
        centre_text(page.c_str(), row_y + btn_h + h * 0.02f);
    }

    float add_w = w * 0.16f;
    ImGui::SetCursorPos(ImVec2((w - add_w) * 0.5f, h * 0.88f));
    if (glass::button("add game", ImVec2(add_w, h * 0.07f)))
        add_game();

    // submitted last so it wins the hover over the swipe strip it overlaps
    draw_library_tabs(w * 0.5f, h * 0.052f, std::min(w * 0.34f, 320.0f), h * 0.055f);

    ImGui::PopStyleColor();
    draw_settings(w, h);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::Render();
    { ImU32 bg = theme::at().page;
      SDL_SetRenderDrawColor(renderer, (bg >> IM_COL32_R_SHIFT) & 0xFF,
                             (bg >> IM_COL32_G_SHIFT) & 0xFF,
                             (bg >> IM_COL32_B_SHIFT) & 0xFF, 0xFF); }
    SDL_RenderClear(renderer);
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderPresent(renderer);
    if (!in_live_resize) pace((double)kFpsCaps[fps_index]);
}
