//
// Created by edi on 7/20/26.
//
// ios-only screen layout, the whole file compiles to nothing on desktop

#include "platform.h"
#if GB_IOS

#include <algorithm>
#include <cmath>
#include <string>
#include "app.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#define GB_TOUCH_DEBUG 0 // set to 1 to tint the touch zones for alignment checks

namespace {
    // the new bezel sprite is authored at iphone resolution, everything below is in this space
    constexpr float kDesignW = 1290.0f;
    constexpr float kDesignH = 2796.0f;
    const SDL_Rect kLcd = {32, 368, 1226, 1075}; // the black screen area on the sprite

    // invisible joypad zones, laid over the sprite buttons in the 1290x2796 space
    // joypad bits: 0 right, 1 left, 2 up, 3 down, 4 a, 5 b, 6 select, 7 start
    struct TouchZone { SDL_Rect area; int bit; };
    const TouchZone kZones[] = {
        {{249, 1764, 126, 138}, 2}, // up
        {{249, 2058, 126, 138}, 3}, // down
        {{101, 1900, 139, 162}, 1}, // left
        {{382, 1900, 139, 162}, 0}, // right
        {{996, 1826, 184, 184}, 4}, // a
        {{764, 1946, 184, 184}, 5}, // b
        {{382, 2450, 192, 118}, 6}, // select
        {{698, 2450, 192, 118}, 7}, // start
    };

    // the joypad bit under a bezel-space point, or -1 for none
    int zone_at(float x, float y) {
        for (const auto& z : kZones)
            if (x >= z.area.x && x < z.area.x + z.area.w &&
                y >= z.area.y && y < z.area.y + z.area.h)
                return z.bit;
        return -1;
    }

    // the back-to-menu button, in screen pixels up in the top letterbox
    SDL_Rect back_button(int out_w, int out_h) {
        return {(int)(out_w * 0.04f), (int)(out_h * 0.045f), (int)(out_w * 0.20f), (int)(out_h * 0.05f)};
    }

    // scale the sprite to cover the whole screen with a little overscan so its green edges bleed
    // off and no letterbox seam shows, render and touch must use this same transform to stay aligned
    constexpr float kOverscan = 1.04f;
    float cover_scale(int out_w, int out_h) {
        return std::max(out_w / kDesignW, out_h / kDesignH) * kOverscan;
    }
}

// draws the running game, the bezel is letterboxed into the real screen and the lcd sits on its green area
void App::render_game_ios() {
    // convert the framebuffer to argb, same olive palette as the desktop path
    uint32_t pixels[144 * 160];
    for (int y = 0; y < 144; y++)
        for (int x = 0; x < 160; x++)
            switch (ppu->framebuffer[y][x]) {
                case 0: pixels[y * 160 + x] = 0xFF627102; break; // darkest shade
                case 1: pixels[y * 160 + x] = 0xFF4D5802; break; // slightly lighter shade
                case 2: pixels[y * 160 + x] = 0xFF364002; break; // lighter shade
                case 3: pixels[y * 160 + x] = 0xFF1F2701; break; // light shade
            }
    SDL_UpdateTexture(texture, nullptr, pixels, 160 * 4);

    // imgui's renderer backend leaves a hidpi scale/viewport/clip on the renderer, fully clear it
    // before raw drawing, scale is set last so nothing undoes it
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    // cover the device with the bezel keeping its aspect, overscanning so the edges bleed off
    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    float scale = cover_scale(out_w, out_h);
    float off_x = (out_w - kDesignW * scale) * 0.5f;
    float off_y = (out_h - kDesignH * scale) * 0.5f;
    SDL_Rect bezel = {(int)off_x, (int)off_y, (int)(kDesignW * scale), (int)(kDesignH * scale)};

    // the lcd rect scaled and offset the same way as the bezel
    SDL_Rect lcd = {(int)(off_x + kLcd.x * scale),
                    (int)(off_y + kLcd.y * scale),
                    (int)(kLcd.w * scale),
                    (int)(kLcd.h * scale)};

    SDL_SetRenderDrawColor(renderer, 0x17, 0x1a, 0x0d, 0xFF); // dark olive letterbox bars
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, gameboy_sprite, nullptr, &bezel);
    SDL_RenderCopy(renderer, texture, nullptr, &lcd);

#if GB_TOUCH_DEBUG
    // tint the joypad zones so their placement over the bezel can be eyeballed
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0x60);
    for (const auto& z : kZones) {
        SDL_Rect r = {(int)(off_x + z.area.x * scale), (int)(off_y + z.area.y * scale),
                      (int)(z.area.w * scale), (int)(z.area.h * scale)};
        SDL_RenderFillRect(renderer, &r);
    }
#endif

    // back-to-menu control, only the chevron is drawn, the touch area itself stays invisible
    SDL_Rect back = back_button(out_w, out_h);
    SDL_SetRenderDrawColor(renderer, 0x8a, 0x99, 0x3f, 0xFF); // muted green from the sprite palette
    int cx = back.x + back.w / 4, cy = back.y + back.h / 2, s = back.h / 3;
    for (int t = -3; t <= 3; t++) { // stack offset lines so the strokes read as a bold arrow
        SDL_RenderDrawLine(renderer, cx + s, cy - s + t, cx - s, cy + t);
        SDL_RenderDrawLine(renderer, cx - s, cy + t, cx + s, cy + s + t);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(8);
}

// swipe carousel, one big cover framed at a time with arrows, a title and a play button
void App::render_menu_ios() {
    // clear any hidpi scale imgui's backend left set last frame, else it renders into a corner
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    // imgui's hidpi auto-scaling renders this into a corner, so drive it in raw device pixels:
    // display size = full output, framebuffer scale = 1 (fills unconditionally), font scaled up to match
    ImGuiIO& io = ImGui::GetIO();
    int out_w, out_h, win_w, win_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    SDL_GetWindowSize(window, &win_w, &win_h);
    io.DisplaySize = ImVec2((float)out_w, (float)out_h);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.FontGlobalScale = (win_w > 0) ? (float)out_w / win_w : 1.0f;

    ImGui::NewFrame();
    float w = io.DisplaySize.x;
    float h = io.DisplaySize.y;

    // zero padding so SetCursorPos maps straight onto the 0..w, 0..h screen,
    // no rounding or border so the fullscreen window has no visible outline at the edges
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("gameboy-emu", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar);

    // drops a line of text horizontally centred at the current y
    auto centre_text = [&](const char* s) {
        float tw = ImGui::CalcTextSize(s).x;
        ImGui::SetCursorPosX((w - tw) * 0.5f);
        ImGui::TextUnformatted(s);
    };

    ImGui::SetCursorPos(ImVec2(0, h * 0.09f)); // low enough to clear the dynamic island
    centre_text("gameboy-emu");

    // split the library: games have cover art, debug/test roms do not; a button flips between them
    std::vector<int> view;
    for (int i = 0; i < (int)rom_list.size(); i++)
        if ((cover_list[i] != nullptr) != show_debug)
            view.push_back(i);
    int count = (int)view.size();

    // category toggle, sits just under the title
    float toggle_w = w * 0.5f;
    ImGui::SetCursorPos(ImVec2((w - toggle_w) * 0.5f, h * 0.15f));
    if (ImGui::Button(show_debug ? "back to games" : "debug roms", ImVec2(toggle_w, h * 0.05f))) {
        show_debug = !show_debug;
        carousel_pos = 0.0f;
    }

    if (count == 0) {
        ImGui::SetCursorPos(ImVec2(0, h * 0.45f));
        centre_text(show_debug ? "no debug roms" : "no games bundled");
    } else {
        // layout of the cover band, all in device pixels
        float cover = w * 0.60f;      // size of the centred cover
        float cover_cx = w * 0.5f;    // horizontal centre of the band
        float cover_cy = h * 0.42f;   // vertical centre of the band
        float spacing = w * 0.72f;    // distance between neighbouring covers

        // a horizontal swipe over the whole band is the only way to move between entries
        ImGui::SetCursorPos(ImVec2(0, cover_cy - cover * 0.75f));
        ImGui::InvisibleButton("swipe", ImVec2(w, cover * 1.5f));
        if (ImGui::IsItemActivated())
            carousel_drag_start = carousel_pos;
        if (ImGui::IsItemActive())
            carousel_pos = carousel_drag_start - ImGui::GetMouseDragDelta(0, 0.0f).x / spacing;

        // keep it in range, and once the finger lifts ease towards the nearest entry
        carousel_pos = std::clamp(carousel_pos, 0.0f, (float)(count - 1));
        if (!ImGui::IsItemActive()) {
            float target = std::round(carousel_pos);
            carousel_pos += (target - carousel_pos) * std::min(1.0f, io.DeltaTime * 12.0f);
            if (std::abs(target - carousel_pos) < 0.001f) carousel_pos = target;
        }
        int centre = (int)std::lround(carousel_pos);
        carousel_index = view[centre];

        // draw covers farthest first so the centred one lands on top, side ones shrink and fade
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 org = ImGui::GetWindowPos();
        const int order[] = {-2, 2, -1, 1, 0};
        for (int o : order) {
            int k = centre + o;
            if (k < 0 || k >= count) continue;
            int r = view[k];
            float d = std::min(std::abs(k - carousel_pos), 1.0f);
            float sz = cover * (1.0f - d * 0.30f);
            int a = (int)(255 * (1.0f - d * 0.55f));
            float cx = cover_cx + (k - carousel_pos) * spacing;
            ImVec2 p0(org.x + cx - sz * 0.5f, org.y + cover_cy - sz * 0.5f);
            ImVec2 p1(p0.x + sz, p0.y + sz);
            if (cover_list[r]) {
                dl->AddImage((ImTextureID)cover_list[r], p0, p1,
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, a));
            } else {
                dl->AddRectFilled(p0, p1, IM_COL32(0x3d, 0x47, 0x03, a), 10.0f);
                std::string nm = display_name(rom_list[r]);
                ImVec2 ts = ImGui::CalcTextSize(nm.c_str());
                dl->AddText(ImVec2((p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f),
                            IM_COL32(0xE6, 0xED, 0xC7, a), nm.c_str());
            }
        }

        // title of the centred entry
        ImGui::SetCursorPos(ImVec2(0, cover_cy + cover * 0.60f));
        centre_text(display_name(rom_list[view[centre]]).c_str());

        // play button
        float play_w = w * 0.5f;
        ImGui::SetCursorPos(ImVec2((w - play_w) * 0.5f, cover_cy + cover * 0.60f + h * 0.055f));
        if (ImGui::Button("play", ImVec2(play_w, h * 0.07f)))
            load_rom(rom_list[view[centre]]);

        // page indicator, dots while the list is short, a counter once it grows
        std::string page;
        if (count <= 15)
            for (int i = 0; i < count; i++) page += (i == centre) ? " *" : " .";
        else
            page = std::to_string(centre + 1) + " / " + std::to_string(count);
        ImGui::SetCursorPos(ImVec2(0, cover_cy + cover * 0.60f + h * 0.14f));
        centre_text(page.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0x17, 0x1a, 0x0f, 0xFF); // menu background, so any edge blends in
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(16); // cap the menu at ~60fps instead of spinning the cpu
}

// turns finger touches into joypad presses and drives the back button, supports several fingers at once
void App::handle_touch_ios(const SDL_Event& event) {
    if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERUP && event.type != SDL_FINGERMOTION)
        return;

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    float px = event.tfinger.x * out_w; // tfinger coords are normalised to the window
    float py = event.tfinger.y * out_h;

    // tapping the back button releases every held key and returns to the carousel
    SDL_Rect back = back_button(out_w, out_h);
    if (event.type == SDL_FINGERDOWN &&
        px >= back.x && px < back.x + back.w && py >= back.y && py < back.y + back.h) {
        for (auto& held : touch_buttons) mem->set_button(held.second, false);
        touch_buttons.clear();
        state = AppState::MENU;
        return;
    }

    // map the touch back into bezel space with the same cover transform the renderer uses
    float scale = cover_scale(out_w, out_h);
    float dx = (px - (out_w - kDesignW * scale) * 0.5f) / scale;
    float dy = (py - (out_h - kDesignH * scale) * 0.5f) / scale;
    int bit = zone_at(dx, dy);
    SDL_FingerID id = event.tfinger.fingerId;

    if (event.type == SDL_FINGERDOWN) {
        if (bit >= 0) { mem->set_button(bit, true); touch_buttons[id] = bit; }
    } else if (event.type == SDL_FINGERMOTION) {
        // a finger that slides off its zone onto another swaps the press over
        auto it = touch_buttons.find(id);
        int held = (it != touch_buttons.end()) ? it->second : -1;
        if (bit != held) {
            if (held >= 0) mem->set_button(held, false);
            if (bit >= 0) { mem->set_button(bit, true); touch_buttons[id] = bit; }
            else if (it != touch_buttons.end()) touch_buttons.erase(it);
        }
    } else { // SDL_FINGERUP
        auto it = touch_buttons.find(id);
        if (it != touch_buttons.end()) { mem->set_button(it->second, false); touch_buttons.erase(it); }
    }
}

#endif // GB_IOS
