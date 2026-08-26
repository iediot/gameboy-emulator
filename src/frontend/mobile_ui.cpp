//
// Created by edi on 7/20/26.
//
// touch screen layout shared by ios and android, compiles to nothing on desktop

#include "platform.h"
#if GB_MOBILE

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include "app.h"
#include "theme.h"
#include "glass.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// implemented in ios_import.mm on ios, android_import.cpp on android
extern "C" void gb_present_document_picker(const char* dest_dir);
extern "C" bool gb_take_import_done();

#define GB_TOUCH_DEBUG 0 // set to 1 to tint the touch zones for alignment checks

namespace {
    // joypad bits: 0 right, 1 left, 2 up, 3 down, 4 a, 5 b, 6 select, 7 start
    enum { BIT_RIGHT = 0, BIT_LEFT, BIT_UP, BIT_DOWN, BIT_A, BIT_B, BIT_SELECT, BIT_START };

    // a finger now holds a mask rather than one bit, so a joystick diagonal can press two
    constexpr float kPi = 3.14159265f;
    constexpr float kScaleMin = 0.65f;
    constexpr float kScaleMax = 1.75f;
    constexpr int MASK_NONE = 0;
    inline int bit_mask(int bit) { return 1 << bit; }

    constexpr ImU32 kBg     = IM_COL32(0x17, 0x1A, 0x0D, 0xFF);
    constexpr ImU32 kFrame  = IM_COL32(0x27, 0x2B, 0x1A, 0xFF);
    constexpr ImU32 kPad    = IM_COL32(0x3C, 0x41, 0x30, 0xFF);
    constexpr ImU32 kPadOn  = IM_COL32(0x6B, 0x73, 0x58, 0xFF);
    constexpr ImU32 kPivot  = IM_COL32(0x2A, 0x2E, 0x22, 0xFF);
    constexpr ImU32 kFace   = IM_COL32(0x4F, 0x6E, 0x12, glass::kFill);
    constexpr ImU32 kFaceOn = IM_COL32(0x7B, 0xA0, 0x22, glass::kFill);
    constexpr ImU32 kPill   = IM_COL32(0x4A, 0x4F, 0x3E, glass::kFill);
    constexpr ImU32 kPillOn = IM_COL32(0x77, 0x7F, 0x64, glass::kFill);
    constexpr ImU32 kLabel  = IM_COL32(0xD8, 0xDC, 0xC6, 0xFF);

    // every control is placed off the output size, render and hit testing both read this
    // so the drawn shapes and the touch areas can never drift apart
    struct Layout {
        ImVec2 lcd_min, lcd_max;
        ImVec2 pad;    float pad_arm, pad_half;
        ImVec2 a, b;   float a_r, b_r;
        ImVec2 select, start;
        ImVec2 start_half, select_half;
    };

    Layout layout_for(int out_w, int out_h, const TouchPlacement* place = nullptr) {
        float w = (float)out_w, h = (float)out_h;
        Layout l;

        // the screen keeps its aspect but never takes more than its share of a squat
        // display, otherwise the joypad below it runs out of room
        float lcd_h = std::min(w * 0.94f * 144.0f / 160.0f, h * 0.46f);
        float lcd_w = lcd_h * 160.0f / 144.0f;
        // clears the back button rather than sitting at a fixed fraction, both are
        // sized off the same radius so they never collide on any device
        float br  = std::max(w, h) * 0.0245f;
        float top = br * 2.4f + h * 0.03f + br * 1.6f;
        l.lcd_min = ImVec2((w - lcd_w) * 0.5f, top);
        l.lcd_max = ImVec2(l.lcd_min.x + lcd_w, l.lcd_min.y + lcd_h);

        float rest    = h - l.lcd_max.y;
        float pad_y   = l.lcd_max.y + rest * 0.42f;
        float pill_y  = l.lcd_max.y + rest * 0.80f;

        // control sizes come off the narrower of the width and the space under the
        // screen, x positions stay on the width so the layout still spans the device
        float u = std::min(w, h * 0.45f);

        l.pad      = ImVec2(w * 0.25f, pad_y);
        l.pad_arm  = u * 0.155f;
        l.pad_half = l.pad_arm * 0.34f;

        l.a_r = l.b_r = u * 0.082f;
        l.a = ImVec2(w * 0.845f, pad_y - u * 0.047f);
        l.b = ImVec2(w * 0.655f, pad_y + u * 0.047f);

        l.start_half = l.select_half = ImVec2(u * 0.105f, u * 0.032f);
        l.select = ImVec2(w * 0.37f, pill_y);
        l.start  = ImVec2(w * 0.63f, pill_y);

        if (!place)
            return l;

        // a custom layout overrides the defaults, keeping the screen where it is, and
        // every control carries its own position and size
        l.pad      = ImVec2(w * place[CTRL_DPAD].x, h * place[CTRL_DPAD].y);
        l.pad_arm  = u * 0.155f * place[CTRL_DPAD].scale;
        l.pad_half = l.pad_arm * 0.34f;

        l.a   = ImVec2(w * place[CTRL_A].x, h * place[CTRL_A].y);
        l.a_r = u * 0.082f * place[CTRL_A].scale;
        l.b   = ImVec2(w * place[CTRL_B].x, h * place[CTRL_B].y);
        l.b_r = u * 0.082f * place[CTRL_B].scale;

        l.start       = ImVec2(w * place[CTRL_START].x, h * place[CTRL_START].y);
        l.start_half  = ImVec2(u * 0.105f * place[CTRL_START].scale,
                               u * 0.032f * place[CTRL_START].scale);
        l.select      = ImVec2(w * place[CTRL_SELECT].x, h * place[CTRL_SELECT].y);
        l.select_half = ImVec2(u * 0.105f * place[CTRL_SELECT].scale,
                               u * 0.032f * place[CTRL_SELECT].scale);
        return l;
    }

    // the stick reports one of eight octants, the four diagonals holding two bits at once
    int stick_mask(float dx, float dy, float dead) {
        if (dx * dx + dy * dy < dead * dead)
            return MASK_NONE;
        float a = std::atan2(dy, dx) + kPi;              // 0..2pi, 0 pointing left
        int octant = (int)((a + kPi / 8.0f) / (kPi / 4.0f)) & 7;
        static const int kOctants[8] = {
            bit_mask(BIT_LEFT),
            bit_mask(BIT_LEFT)  | bit_mask(BIT_UP),
            bit_mask(BIT_UP),
            bit_mask(BIT_UP)    | bit_mask(BIT_RIGHT),
            bit_mask(BIT_RIGHT),
            bit_mask(BIT_RIGHT) | bit_mask(BIT_DOWN),
            bit_mask(BIT_DOWN),
            bit_mask(BIT_DOWN)  | bit_mask(BIT_LEFT)
        };
        return kOctants[octant];
    }

    // half extents of each control's touch box, used for the editor's collision rules
    ImVec2 control_half(const Layout& l, int which) {
        switch (which) {
            case CTRL_DPAD:  return ImVec2(l.pad_arm, l.pad_arm);
            case CTRL_A:     return ImVec2(l.a_r, l.a_r);
            case CTRL_B:     return ImVec2(l.b_r, l.b_r);
            case CTRL_START: return l.start_half;
            default:         return l.select_half;
        }
    }

    ImVec2 control_centre(const Layout& l, int which) {
        switch (which) {
            case CTRL_DPAD:  return l.pad;
            case CTRL_A:     return l.a;
            case CTRL_B:     return l.b;
            case CTRL_START: return l.start;
            default:         return l.select;
        }
    }

    // hit areas run wider than the drawn shapes so a thumb landing on an edge still counts
    int control_at(const Layout& l, float x, float y, bool stick) {
        float dx = x - l.pad.x, dy = y - l.pad.y;
        if (stick) {
            float r = l.pad_arm * 1.15f;
            if (dx * dx + dy * dy <= r * r)
                return stick_mask(dx, dy, l.pad_arm * 0.30f);
        } else {
            float arm = l.pad_arm * 1.15f, half = l.pad_half * 1.45f;
            bool vert = std::fabs(dx) <= half && std::fabs(dy) <= arm;
            bool horz = std::fabs(dy) <= half && std::fabs(dx) <= arm;
            if (vert && (!horz || std::fabs(dy) >= std::fabs(dx)))
                return bit_mask(dy < 0 ? BIT_UP : BIT_DOWN);
            if (horz)
                return bit_mask(dx < 0 ? BIT_LEFT : BIT_RIGHT);
        }

        float ar = l.a_r * 1.3f, brr = l.b_r * 1.3f;
        float ax = x - l.a.x, ay = y - l.a.y;
        if (ax * ax + ay * ay <= ar * ar) return bit_mask(BIT_A);
        float bx = x - l.b.x, by = y - l.b.y;
        if (bx * bx + by * by <= brr * brr) return bit_mask(BIT_B);

        if (std::fabs(x - l.select.x) <= l.select_half.x * 1.25f &&
            std::fabs(y - l.select.y) <= l.select_half.y * 1.9f) return bit_mask(BIT_SELECT);
        if (std::fabs(x - l.start.x) <= l.start_half.x * 1.25f &&
            std::fabs(y - l.start.y) <= l.start_half.y * 1.9f) return bit_mask(BIT_START);
        return MASK_NONE;
    }

    void centred_label(ImDrawList* dl, ImVec2 c, float size, const char* text) {
        ImFont* font = ImGui::GetFont();
        ImVec2  ts   = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), kLabel, text);
    }

    // the stick sits where the cross would, its thumb offset toward whatever is held
    // the thumb follows the finger anywhere inside the base, the eight octants are only
    // what gets reported to the joypad
    void draw_stick(ImDrawList* dl, const Layout& l, float dx, float dy, bool active) {
        float r = l.pad_arm;
        dl->AddCircleFilled(l.pad, r, kPad, 48);
        glass::circle(dl, l.pad, r);

        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f) { dx /= len; dy /= len; len = 1.0f; }

        float travel = r * 0.42f;
        ImVec2 thumb(l.pad.x + dx * travel, l.pad.y + dy * travel);
        float tr = r * 0.46f;
        dl->AddCircleFilled(thumb, tr, active ? kPadOn : kPivot, 40);
        glass::circle(dl, thumb, tr);
    }

    void draw_controls(ImDrawList* dl, const Layout& l, const bool* held, bool stick,
                       float sx, float sy, bool stick_on) {
        if (stick) {
            draw_stick(dl, l, sx, sy, stick_on);
        } else {
        // the cross stays opaque and unclipped, the glass treatment reads badly on a plus
        // and any attempt to tile it leaves seams where the arms meet
        float cx = l.pad.x, cy = l.pad.y, r = l.pad_arm, t = l.pad_half, rd = t * 0.45f;
        dl->AddRectFilled(ImVec2(cx - t, cy - r), ImVec2(cx + t, cy + r), kPad, rd);
        dl->AddRectFilled(ImVec2(cx - r, cy - t), ImVec2(cx + r, cy + t), kPad, rd);
        // only the tip beyond the centre square lights up, and it ramps from the resting
        // colour where it leaves the square to the full one at the end of the arm
        auto lit_arm = [&](ImVec2 p0, ImVec2 p1, ImDrawFlags corners, ImVec2 from, ImVec2 to) {
            int v0 = dl->VtxBuffer.Size;
            dl->AddRectFilled(p0, p1, kPadOn, rd, corners);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size,
                                                          from, to, kPad, kPadOn);
        };
        if (held[BIT_UP])
            lit_arm(ImVec2(cx - t, cy - r), ImVec2(cx + t, cy - t), ImDrawFlags_RoundCornersTop,
                    ImVec2(cx, cy - t), ImVec2(cx, cy - r));
        if (held[BIT_DOWN])
            lit_arm(ImVec2(cx - t, cy + t), ImVec2(cx + t, cy + r), ImDrawFlags_RoundCornersBottom,
                    ImVec2(cx, cy + t), ImVec2(cx, cy + r));
        if (held[BIT_LEFT])
            lit_arm(ImVec2(cx - r, cy - t), ImVec2(cx - t, cy + t), ImDrawFlags_RoundCornersLeft,
                    ImVec2(cx - t, cy), ImVec2(cx - r, cy));
        if (held[BIT_RIGHT])
            lit_arm(ImVec2(cx + t, cy - t), ImVec2(cx + r, cy + t), ImDrawFlags_RoundCornersRight,
                    ImVec2(cx + t, cy), ImVec2(cx + r, cy));
        glass::cross(dl, l.pad, r, t, rd);
        dl->AddCircleFilled(l.pad, t * 0.5f, kPivot, 28);
        }

        auto face = [&](ImVec2 c, float r, bool on, const char* label) {
            ImU32 col = on ? kFaceOn : kFace;
            dl->AddCircleFilled(c, r, col, 40);
            glass::circle(dl, c, r);
            centred_label(dl, c, r * 0.85f, label);
        };
        face(l.b, l.b_r, held[BIT_B], "B");
        face(l.a, l.a_r, held[BIT_A], "A");

        auto pill = [&](ImVec2 c, ImVec2 e, bool on, const char* label) {
            ImU32 col = on ? kPillOn : kPill;
            ImVec2 p0(c.x - e.x, c.y - e.y), p1(c.x + e.x, c.y + e.y);
            dl->AddRectFilled(p0, p1, col, e.y);
            glass::rect(dl, p0, p1, e.y);
            centred_label(dl, c, e.y * 0.95f, label);
        };
        pill(l.select, l.select_half, held[BIT_SELECT], "SELECT");
        pill(l.start,  l.start_half,  held[BIT_START],  "START");
    }

    // ios reports the window in points and the renderer in pixels, so the ratio between
    // them is the ui scale, android reports both in pixels and carries it in the density
    float ui_scale(int out_w, int win_w) {
#if GB_ANDROID
        (void)out_w;
        (void)win_w;
        float ddpi = 160.0f;
        if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) != 0 || ddpi <= 0.0f)
            ddpi = 160.0f;
        return std::max(1.0f, ddpi / 160.0f);
#else
        return (win_w > 0) ? (float)out_w / win_w : 1.0f;
#endif
    }
}

// draws the running game, the lcd sits up top with the joypad drawn beneath it
void App::render_game_mobile() {
    SDL_UpdateTexture(texture, nullptr, ppu->framebuffer, 160 * 4);

    // imgui's renderer backend leaves a hidpi scale/viewport/clip on the renderer, fully clear it
    // before raw drawing, scale is set last so nothing undoes it
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    Layout l = layout_for(out_w, out_h, layout_custom ? controls : nullptr);

    { ImU32 bg = theme::at().page;
      SDL_SetRenderDrawColor(renderer, (bg >> IM_COL32_R_SHIFT) & 0xFF,
                             (bg >> IM_COL32_G_SHIFT) & 0xFF,
                             (bg >> IM_COL32_B_SHIFT) & 0xFF, 0xFF); }
    SDL_RenderClear(renderer);

    bool held[8] = {};
    for (const auto& f : touch_buttons)
        for (int b = 0; b < 8; b++)
            if (f.second & bit_mask(b)) held[b] = true;

    // screen and joypad both go through the overlay draw list so they layer in one pass
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    io.DisplaySize = ImVec2((float)out_w, (float)out_h);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.FontGlobalScale = ui_scale(out_w, win_w);
    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##overlay", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

    sync_theme();
    // drawn first so the lcd and the joypad both sit on top of the field
    draw_iridescence(io.DisplaySize.x, io.DisplaySize.y);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float inset = out_w * 0.018f;
    float round = out_w * 0.03f;
    dl->AddRectFilled(ImVec2(l.lcd_min.x - inset, l.lcd_min.y - inset),
                      ImVec2(l.lcd_max.x + inset, l.lcd_max.y + inset),
                      kFrame, round);
    // concentric with the frame, so the grey border keeps an even width round the corners
    dl->AddImageRounded((ImTextureID)texture, l.lcd_min, l.lcd_max,
                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, round - inset);
    draw_controls(dl, l, held, joystick_mode, stick_dx, stick_dy, stick_held);

#if GB_TOUCH_DEBUG
    // walk the screen and dot every point that maps to a control, shows the real hit areas
    for (int y = (int)l.lcd_max.y; y < out_h; y += 12)
        for (int x = 0; x < out_w; x += 12)
            if (control_at(l, (float)x, (float)y) >= 0)
                dl->AddRectFilled(ImVec2((float)x, (float)y), ImVec2(x + 3.0f, y + 3.0f),
                                  IM_COL32(0xFF, 0x00, 0x00, 0x90));
#endif

    float br = std::max((float)out_w, (float)out_h) * 0.0245f;
    if (back_button(br * 1.55f, br * 2.4f + out_h * 0.03f, br)) {
        release_touches();
        state = AppState::MENU;
    }
    draw_settings((float)out_w, (float)out_h);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

    SDL_RenderPresent(renderer);
    pace(kGbFps);
}

// swipe carousel, one big cover framed at a time with arrows, a title and a play button
void App::render_menu_mobile() {
    // a finished import drops a new rom into the folder, pick it up before drawing
    if (gb_take_import_done()) {
        scan_roms();
        int r_new = -1;
        for (int i = 0; i < (int)rom_list.size(); i++)
            if (std::find(import_prev.begin(), import_prev.end(), rom_list[i]) == import_prev.end()) {
                r_new = i;
                break;
            }
        if (r_new >= 0) {
            // a dual mode cart is on both shelves, so only move if the current one
            // would not show it at all
            if (!in_tab(r_new, library_tab))
                library_tab = in_tab(r_new, 0) ? 0 : 1;
            int local = 0;
            for (int i = 0; i < r_new; i++)
                if (in_tab(i, library_tab))
                    local++;
            carousel_pos = carousel_target = (float)local; carousel_vel = 0.0f;
        } else {
            carousel_pos = carousel_target = 0.0f; carousel_vel = 0.0f;
        }
    }

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
    io.FontGlobalScale = ui_scale(out_w, win_w);

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

    sync_theme();
    draw_iridescence(w, h);

    // text drawn straight onto the page has to invert with it, panels keep their own
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(theme::at().text_page));

    // the library is split by cartridge type, the pill sits centred in the header row
    // with the cog opposite it on the right
    std::vector<int> view;
    int count = library_view(view);

    float hdr_r = std::max(w, h) * 0.0245f;
    float hdr_y = hdr_r * 2.4f + h * 0.03f;
    draw_library_tabs(w * 0.5f, hdr_y, w * 0.56f, hdr_r * 1.6f);

    if (count == 0) {
        ImGui::SetCursorPos(ImVec2(0, h * 0.45f));
        centre_text(library_tab ? "no color games" : "no game boy games");
    } else {
        // vertical card stack, the selected game on top, swiping shuffles through and wraps around
        float cover = w * 0.58f;      // size of the top card
        float cover_cx = w * 0.5f;    // horizontal centre of the stack
        float cover_cy = h * 0.36f;   // where the top card sits
        float spacing = w * 0.30f;    // swipe distance that advances one card

        // swipe over the fan; real momentum means a hard flick coasts through many cards while a
        // gentle drag moves one, coasting to a halt under friction before settling on the nearest
        ImGui::SetCursorPos(ImVec2(0, h * 0.22f));
        ImGui::InvisibleButton("swipe", ImVec2(w, cover_cy + cover * 0.85f - h * 0.22f));
        if (ImGui::IsItemActivated()) {
            carousel_drag_start = carousel_pos;
            carousel_vel = 0.0f;
        }
        if (ImGui::IsItemActive()) {
            float np = carousel_drag_start - ImGui::GetMouseDragDelta(0, 0.0f).x / spacing;
            if (io.DeltaTime > 0.0f)
                carousel_vel = carousel_vel * 0.3f + ((np - carousel_pos) / io.DeltaTime) * 0.7f; // cards/sec
            carousel_vel = std::max(-90.0f, std::min(90.0f, carousel_vel));
            carousel_pos = np;
        } else if (std::abs(carousel_vel) > 0.4f) {
            carousel_pos += carousel_vel * io.DeltaTime;    // keep coasting after the finger lifts
            carousel_vel *= std::exp(-3.5f * io.DeltaTime); // friction bleeds the speed off
        } else {
            carousel_vel = 0.0f;                            // slow enough now, settle on the nearest card
            float target = std::round(carousel_pos);
            carousel_pos += (target - carousel_pos) * std::min(1.0f, io.DeltaTime * 14.0f);
            if (std::abs(target - carousel_pos) < 0.001f) carousel_pos = target;
        }

        // positions wrap so the list loops forever
        auto wrap = [count](int k) { return ((k % count) + count) % count; };
        int centre = wrap((int)std::lround(carousel_pos));
        int r_centre = view[centre];
        carousel_index = r_centre;

        // horizontal fan: selected game centred and on top, next games to the right, previous ones to the left
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 org = ImGui::GetWindowPos();
        int base = (int)std::floor(carousel_pos);
        struct Card { int r; float rel, cx, sz; };
        std::vector<Card> cards;
        int span = (int)std::ceil((w * 0.5f) / (cover * 0.30f)) + 2;
        for (int k = base - span; k <= base + span + 1; k++) {
            float rel = k - carousel_pos;            // <0 previous (left), >0 next (right)
            float ad = std::abs(rel);
            float sz = cover * std::max(0.45f, 1.0f - ad * 0.08f);
            float cx = cover_cx + rel * (cover * 0.30f);
            if (cx + sz * 0.5f < 0.0f || cx - sz * 0.5f > w) continue;
            cards.push_back({view[wrap(k)], rel, cx, sz});
        }
        // outermost first so the card sliding toward the centre rises on top while the old one slips under
        std::sort(cards.begin(), cards.end(), [](const Card& x, const Card& y) { return std::abs(x.rel) > std::abs(y.rel); });

        const float cart_ar = 700.0f / 800.0f;
        const float slot_x0 = 0.114f, slot_x1 = 0.882f;
        const float slot_y0 = 0.280f, slot_y1 = 0.896f;
        bool use_cart = render_cartridge && cartridge_sprite;
        ImU32 tint = IM_COL32(255, 255, 255, 255);
        for (const Card& cd : cards) {
            if (!use_cart) {
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
            dl->AddImage((ImTextureID)cartridge_sprite, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint);

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

        // title of the selected game
        float title_y = cover_cy + cover * 0.95f;
        ImGui::SetCursorPos(ImVec2(0, title_y));
        centre_text(display_name(rom_list[r_centre]).c_str());

        // play and delete, side by side
        float btn_h = h * 0.07f;
        float play_w = w * 0.34f, del_w = w * 0.22f, gap = w * 0.03f;
        float row_x = (w - (play_w + gap + del_w)) * 0.5f;
        float row_y = title_y + h * 0.05f;
        ImGui::SetCursorPos(ImVec2(row_x, row_y));
        if (glass::button("play", ImVec2(play_w, btn_h)))
            load_rom(rom_list[r_centre]);
        ImGui::SetCursorPos(ImVec2(row_x + play_w + gap, row_y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.16f, 0.08f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.10f, 0.05f, 0.50f));
        if (glass::button("delete", ImVec2(del_w, btn_h)))
            ImGui::OpenPopup("confirm_delete");
        ImGui::PopStyleColor(3);

        // a second, deliberate tap is required so a stray delete never wipes a game by accident
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(w * 0.05f, w * 0.05f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
        if (ImGui::BeginPopupModal("confirm_delete", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(("delete " + display_name(rom_list[r_centre]) + " ?").c_str());
            ImGui::Dummy(ImVec2(0, h * 0.02f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.06f, 0.50f));
            if (glass::button("delete", ImVec2(w * 0.32f, h * 0.07f))) {
                std::error_code ec;
                std::filesystem::remove(rom_folder + rom_list[r_centre], ec);
                scan_roms();
                float prev = (centre > 0) ? (float)(centre - 1) : 0.0f;
                carousel_pos = carousel_target = prev; carousel_vel = 0.0f;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (glass::button("cancel", ImVec2(w * 0.32f, h * 0.07f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        // page indicator
        std::string page = std::to_string(centre + 1) + " / " + std::to_string(count);
        ImGui::SetCursorPos(ImVec2(0, row_y + h * 0.09f));
        centre_text(page.c_str());
    }

    // add game, opens the ios file picker to import a rom into the writable folder
    float add_w = w * 0.6f;
    ImGui::SetCursorPos(ImVec2((w - add_w) * 0.5f, h * 0.86f));
    if (glass::button("add game", ImVec2(add_w, h * 0.06f))) {
        import_prev = rom_list;                         // snapshot so the new rom can be spotted afterwards
        gb_present_document_picker(rom_folder.c_str());
    }

    ImGui::PopStyleColor();
    draw_settings(w, h);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::Render();
    { ImU32 bg = theme::at().page;   // menu background, so any edge blends in
      SDL_SetRenderDrawColor(renderer, (bg >> IM_COL32_R_SHIFT) & 0xFF,
                             (bg >> IM_COL32_G_SHIFT) & 0xFF,
                             (bg >> IM_COL32_B_SHIFT) & 0xFF, 0xFF); }
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    pace((double)kFpsCaps[fps_index]);
}

// turns finger touches into joypad presses and drives the back button, supports several fingers at once
void App::release_touches() {
    stick_held = false;
    stick_dx = stick_dy = 0.0f;
    for (const auto& f : touch_buttons)
        for (int b = 0; b < 8; b++)
            if (f.second & bit_mask(b)) mem->set_button(b, false);
    touch_buttons.clear();
}

void App::handle_touch_mobile(const SDL_Event& event) {
    if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERUP && event.type != SDL_FINGERMOTION)
        return;

    // the settings panel pauses the game, so fingers belong to the ui while it is up
    if (settings_open || editing_layout) {
        release_touches();
        return;
    }

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    float px = event.tfinger.x * out_w; // tfinger coords are normalised to the window
    float py = event.tfinger.y * out_h;

    // the round back button up top is an imgui item now, keep fingers there out of the joypad
    float br = std::max((float)out_w, (float)out_h) * 0.0245f;
    float bcx = br * 1.55f, bcy = br * 2.4f + out_h * 0.03f;
    if ((px - bcx) * (px - bcx) + (py - bcy) * (py - bcy) <= (br * 1.6f) * (br * 1.6f))
        return;

    Layout l = layout_for(out_w, out_h, layout_custom ? controls : nullptr);
    SDL_FingerID id = event.tfinger.fingerId;

    // a finger that lands on the stick keeps it until it lifts, so sliding past the rim
    // steers instead of letting go, but a finger starting outside never grabs it
    int mask;
    if (joystick_mode && stick_held && id == stick_finger && event.type != SDL_FINGERUP) {
        mask = stick_mask(px - l.pad.x, py - l.pad.y, l.pad_arm * 0.30f);
    } else {
        mask = control_at(l, px, py, joystick_mode);
        if (joystick_mode && event.type == SDL_FINGERDOWN) {
            float dx = px - l.pad.x, dy = py - l.pad.y;
            float grab = l.pad_arm * 1.15f;
            if (dx * dx + dy * dy <= grab * grab) {
                stick_held = true;
                stick_finger = id;
            }
        }
    }
    if (stick_held && id == stick_finger) {
        if (event.type == SDL_FINGERUP) {
            stick_held = false;
            stick_dx = stick_dy = 0.0f;
        } else {
            stick_dx = (px - l.pad.x) / l.pad_arm;
            stick_dy = (py - l.pad.y) / l.pad_arm;
        }
    }

    auto apply = [&](int held, int wanted) {
        for (int b = 0; b < 8; b++) {
            bool was = held & bit_mask(b), now = wanted & bit_mask(b);
            if (was != now) mem->set_button(b, now);
        }
    };

    auto it = touch_buttons.find(id);
    int held = (it != touch_buttons.end()) ? it->second : MASK_NONE;

    if (event.type == SDL_FINGERUP) {
        apply(held, MASK_NONE);
        if (it != touch_buttons.end()) touch_buttons.erase(it);
        return;
    }
    // a finger that slides off its zone onto another swaps the press over
    if (mask != held) {
        apply(held, mask);
        if (mask == MASK_NONE) {
            if (it != touch_buttons.end()) touch_buttons.erase(it);
        } else {
            touch_buttons[id] = mask;
        }
    }
}

// controls may overlap each other, but none may leave the display or sit on or above
// the screen area
bool App::layout_fits(int out_w, int out_h) const {
    Layout l = layout_for(out_w, out_h, controls);
    float floor_y = l.lcd_max.y + out_w * 0.018f;
    ImVec2 c[CTRL_COUNT], e[CTRL_COUNT];
    for (int i = 0; i < CTRL_COUNT; i++) {
        c[i] = control_centre(l, i);
        e[i] = control_half(l, i);
    }
    for (int i = 0; i < CTRL_COUNT; i++) {
        if (c[i].x - e[i].x < 0.0f || c[i].x + e[i].x > (float)out_w) return false;
        if (c[i].y + e[i].y > (float)out_h) return false;
        if (c[i].y - e[i].y < floor_y) return false;
    }
    return true;
}

// a control that would grow past an edge is pushed back inside rather than refused,
// false only when it is too big to fit anywhere
bool App::fit_control(int which, int out_w, int out_h) {
    Layout l = layout_for(out_w, out_h, controls);
    ImVec2 c = control_centre(l, which), e = control_half(l, which);
    float floor_y = l.lcd_max.y + out_w * 0.018f;

    if (e.x * 2.0f > (float)out_w || e.y * 2.0f > (float)out_h - floor_y)
        return false;

    float nx = std::clamp(c.x, e.x, (float)out_w - e.x);
    float ny = std::clamp(c.y, floor_y + e.y, (float)out_h - e.y);
    controls[which].x += (nx - c.x) / (float)out_w;
    controls[which].y += (ny - c.y) / (float)out_h;
    return true;
}

// seeds the editor from whatever layout is on screen so nothing jumps when it opens
void App::begin_layout_edit() {
    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    Layout l = layout_for(out_w, out_h, layout_custom ? controls : nullptr);
    for (int i = 0; i < CTRL_COUNT; i++) {
        ImVec2 c = control_centre(l, i);
        controls[i].x = c.x / (float)out_w;
        controls[i].y = c.y / (float)out_h;
        if (!layout_custom) controls[i].scale = 1.0f;
    }
    layout_custom = true;
    editing_layout = true;
    editing_pick = -1;
    release_touches();
}

void App::render_layout_editor() {
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    Layout l = layout_for(out_w, out_h, controls);

    { ImU32 bg = theme::at().page;
      SDL_SetRenderDrawColor(renderer, (bg >> IM_COL32_R_SHIFT) & 0xFF,
                             (bg >> IM_COL32_G_SHIFT) & 0xFF,
                             (bg >> IM_COL32_B_SHIFT) & 0xFF, 0xFF); }
    SDL_RenderClear(renderer);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    io.DisplaySize = ImVec2((float)out_w, (float)out_h);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.FontGlobalScale = ui_scale(out_w, win_w);
    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##editor", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // the screen area is off limits, drawn so its boundary is obvious while dragging
    float inset = out_w * 0.018f;
    dl->AddRectFilled(ImVec2(l.lcd_min.x - inset, l.lcd_min.y - inset),
                      ImVec2(l.lcd_max.x + inset, l.lcd_max.y + inset),
                      kFrame, out_w * 0.03f);
    dl->AddRectFilled(l.lcd_min, l.lcd_max, IM_COL32(0x62, 0x71, 0x02, 0xFF),
                      out_w * 0.03f - inset);
    centred_label(dl, ImVec2((l.lcd_min.x + l.lcd_max.x) * 0.5f,
                             (l.lcd_min.y + l.lcd_max.y) * 0.5f),
                  out_w * 0.05f, "screen");

    bool held[8] = {};
    draw_controls(dl, l, held, joystick_mode, stick_dx, stick_dy, stick_held);

    // outline whichever control is being moved
    if (editing_pick >= 0) {
        ImVec2 c = control_centre(l, editing_pick), e = control_half(l, editing_pick);
        dl->AddRect(ImVec2(c.x - e.x, c.y - e.y), ImVec2(c.x + e.x, c.y + e.y),
                    IM_COL32(255, 255, 255, 200), 6.0f, 0, glass::hairline() * 1.5f);
    }

    // reset, the size slider and done share the band above the screen, clear of the status
    // bar at the top and of the home indicator at the bottom
    float br = std::max((float)out_w, (float)out_h) * 0.0245f;
    float bh = br * 1.5f;
    float bw = (float)out_w * 0.16f;
    float gap = (float)out_w * 0.02f;
    float bx = (float)out_w * 0.05f;
    float by = br * 2.4f + out_h * 0.03f - bh * 0.5f;
    float sx = bx + bw * 2.0f + gap * 2.0f;
    float sw = ((float)out_w * 0.95f - bw - gap) - sx;
    float sh = bh * 0.55f;
    float sy = by + (bh - sh) * 0.5f;

    // one gesture at a time, whatever the finger lands on owns it until it lifts
    if (ImGui::IsMouseClicked(0)) {
        drag_mode = 0;
        bool on_slider = editing_pick >= 0 &&
                         io.MousePos.x >= sx && io.MousePos.x <= sx + sw &&
                         std::fabs(io.MousePos.y - (sy + sh * 0.5f)) <= bh * 0.6f;
        if (on_slider) {
            drag_mode = 2;
        } else if (io.MousePos.y > l.lcd_max.y) {
            for (int i = 0; i < CTRL_COUNT; i++) {
                ImVec2 c = control_centre(l, i), e = control_half(l, i);
                if (std::fabs(io.MousePos.x - c.x) <= e.x &&
                    std::fabs(io.MousePos.y - c.y) <= e.y) {
                    editing_pick = i;
                    drag_grab_x = io.MousePos.x - c.x;
                    drag_grab_y = io.MousePos.y - c.y;
                    drag_mode = 1;
                    break;
                }
            }
            if (drag_mode == 0) editing_pick = -1;
        }
    }
    if (!ImGui::IsMouseDown(0))
        drag_mode = 0;

    if (drag_mode == 1 && editing_pick >= 0) {
        TouchPlacement keep = controls[editing_pick];
        float cx = io.MousePos.x - drag_grab_x;
        float cy = io.MousePos.y - drag_grab_y;

        // pull the control onto a match once it is close enough: sharing an axis with
        // another control, mirroring one across the display centre, or sitting at 45
        // degrees from one
        float snap = snap_enabled ? (float)out_w * 0.022f : 0.0f;
        float mid = (float)out_w * 0.5f;
        float gx = -1.0f, gy = -1.0f;
        bool mirrored = false;
        ImVec2 diag_from(0.0f, 0.0f);
        bool diag = false;

        for (int i = 0; i < CTRL_COUNT; i++) {
            if (i == editing_pick) continue;
            ImVec2 o = control_centre(l, i);
            if (gx < 0.0f && std::fabs(cx - o.x) < snap) { cx = o.x; gx = o.x; }
            if (gy < 0.0f && std::fabs(cy - o.y) < snap) { cy = o.y; gy = o.y; }
        }
        // the same gap on this one's outer side as the other has on its own
        for (int i = 0; i < CTRL_COUNT && gx < 0.0f; i++) {
            if (i == editing_pick) continue;
            float t = (float)out_w - control_centre(l, i).x;
            if (std::fabs(cx - t) < snap) { cx = t; gx = t; mirrored = true; }
        }
        if (gx < 0.0f && std::fabs(cx - mid) < snap) { cx = mid; gx = mid; }

        if (gx < 0.0f && gy < 0.0f) {
            for (int i = 0; i < CTRL_COUNT; i++) {
                if (i == editing_pick) continue;
                ImVec2 o = control_centre(l, i);
                float dx = cx - o.x, dy = cy - o.y;
                if (std::fabs(dx) > snap && std::fabs(std::fabs(dx) - std::fabs(dy)) < snap) {
                    float d = (std::fabs(dx) + std::fabs(dy)) * 0.5f;
                    cx = o.x + std::copysign(d, dx);
                    cy = o.y + std::copysign(d, dy);
                    diag_from = o;
                    diag = true;
                    break;
                }
            }
        }

        ImU32 guide = IM_COL32(255, 255, 255, 90);
        float hair = glass::hairline();
        if (gx >= 0.0f)
            dl->AddLine(ImVec2(gx, l.lcd_max.y), ImVec2(gx, (float)out_h), guide, hair);
        if (gy >= 0.0f)
            dl->AddLine(ImVec2(0.0f, gy), ImVec2((float)out_w, gy), guide, hair);
        if (mirrored)
            dl->AddLine(ImVec2(mid, l.lcd_max.y), ImVec2(mid, (float)out_h),
                        IM_COL32(255, 255, 255, 50), hair);
        if (diag)
            dl->AddLine(diag_from, ImVec2(cx, cy), guide, hair);

        float wx = cx / (float)out_w;
        float wy = cy / (float)out_h;
        controls[editing_pick].x = wx;
        controls[editing_pick].y = wy;
        // a blocked move still takes whichever axis is free, so a control slides
        // along an obstacle instead of sticking to it
        if (!layout_fits(out_w, out_h)) {
            controls[editing_pick] = keep;
            controls[editing_pick].x = wx;
            if (!layout_fits(out_w, out_h)) {
                controls[editing_pick] = keep;
                controls[editing_pick].y = wy;
                if (!layout_fits(out_w, out_h))
                    controls[editing_pick] = keep;
            }
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (glass::button("reset", ImVec2(bw, bh))) {
        layout_custom = false;
        begin_layout_edit();
    }
    ImGui::SetCursorScreenPos(ImVec2(bx + bw + gap, by));
    bool snap_lit = snap_enabled;   // the click below flips it, so remember what we pushed
    if (snap_lit) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.34f, 0.40f, 0.02f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.47f, 0.03f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.33f, 0.02f, 0.70f));
    }
    if (glass::button("snap", ImVec2(bw, bh))) {
        snap_enabled = !snap_enabled;
        save_settings();
    }
    if (snap_lit)
        ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(ImVec2((float)out_w * 0.95f - bw, by));
    if (glass::button("done", ImVec2(bw, bh))) {
        editing_layout = false;
        save_settings();
    }

    if (editing_pick >= 0) {
        if (drag_mode == 2) {
            // the knob follows the finger the whole way even where the size cannot,
            // so the slider never looks stuck short of its ends
            slider_v = std::clamp((io.MousePos.x - sx) / sw, 0.0f, 1.0f);
            float want = kScaleMin + slider_v * (kScaleMax - kScaleMin);
            TouchPlacement keep = controls[editing_pick];
            controls[editing_pick].scale = want;
            if (!fit_control(editing_pick, out_w, out_h)) {
                // only when it cannot fit at any position, then take the largest that can
                float lo = std::min(keep.scale, want), hi = want;
                for (int i = 0; i < 14; i++) {
                    float mid = (lo + hi) * 0.5f;
                    controls[editing_pick] = keep;
                    controls[editing_pick].scale = mid;
                    if (fit_control(editing_pick, out_w, out_h)) lo = mid; else hi = mid;
                }
                controls[editing_pick] = keep;
                controls[editing_pick].scale = lo;
                fit_control(editing_pick, out_w, out_h);
            }
        } else {
            slider_v = (controls[editing_pick].scale - kScaleMin) / (kScaleMax - kScaleMin);
        }

        float th = sh * 0.42f, ty = sy + (sh - th) * 0.5f;
        dl->AddRectFilled(ImVec2(sx, ty), ImVec2(sx + sw, ty + th),
                          theme::at().panel, th * 0.5f);
        dl->AddRectFilled(ImVec2(sx, ty), ImVec2(sx + sw * slider_v, ty + th),
                          theme::at().accent, th * 0.5f);
        dl->AddCircleFilled(ImVec2(sx + sw * slider_v, ty + th * 0.5f), sh * 0.42f,
                            IM_COL32(255, 255, 255, 255), 24);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    pace(kGbFps);
}

#endif // GB_MOBILE
