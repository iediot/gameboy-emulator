//
// Created by edi on 7/20/26.
//
// touch screen layout shared by ios and android, compiles to nothing on desktop

#include "platform.h"
#if GB_MOBILE

#include <algorithm>
#include <cmath>
#include <string>
#include <filesystem>
#include "app.h"
#include "glass.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// implemented in ios_import.mm on ios, android_import.cpp on android
extern "C" void gb_present_document_picker(const char* dest_dir);
extern "C" bool gb_take_import_done();

#define GB_TOUCH_DEBUG 0 // set to 1 to tint the touch zones for alignment checks

namespace {
    // joypad bits: 0 right, 1 left, 2 up, 3 down, 4 a, 5 b, 6 select, 7 start
    enum { BIT_RIGHT = 0, BIT_LEFT, BIT_UP, BIT_DOWN, BIT_A, BIT_B, BIT_SELECT, BIT_START };

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
        ImVec2 a, b;   float face_r;
        ImVec2 select, start; ImVec2 pill_half;
    };

    Layout layout_for(int out_w, int out_h) {
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

        l.face_r = u * 0.082f;
        l.a = ImVec2(w * 0.845f, pad_y - u * 0.047f);
        l.b = ImVec2(w * 0.655f, pad_y + u * 0.047f);

        l.pill_half = ImVec2(u * 0.105f, u * 0.032f);
        l.select = ImVec2(w * 0.37f, pill_y);
        l.start  = ImVec2(w * 0.63f, pill_y);
        return l;
    }

    // hit areas run wider than the drawn shapes so a thumb landing on an edge still counts
    int control_at(const Layout& l, float x, float y) {
        float dx = x - l.pad.x, dy = y - l.pad.y;
        float arm = l.pad_arm * 1.15f, half = l.pad_half * 1.45f;
        bool vert = std::fabs(dx) <= half && std::fabs(dy) <= arm;
        bool horz = std::fabs(dy) <= half && std::fabs(dx) <= arm;
        if (vert && (!horz || std::fabs(dy) >= std::fabs(dx))) return dy < 0 ? BIT_UP : BIT_DOWN;
        if (horz) return dx < 0 ? BIT_LEFT : BIT_RIGHT;

        float fr = l.face_r * 1.3f;
        float ax = x - l.a.x, ay = y - l.a.y;
        if (ax * ax + ay * ay <= fr * fr) return BIT_A;
        float bx = x - l.b.x, by = y - l.b.y;
        if (bx * bx + by * by <= fr * fr) return BIT_B;

        float pw = l.pill_half.x * 1.25f, ph = l.pill_half.y * 1.9f;
        if (std::fabs(x - l.select.x) <= pw && std::fabs(y - l.select.y) <= ph) return BIT_SELECT;
        if (std::fabs(x - l.start.x)  <= pw && std::fabs(y - l.start.y)  <= ph) return BIT_START;
        return -1;
    }

    // same shape and palette as the cog and back buttons so the header reads as a pair
    bool letter_button(float cx, float cy, float r, const char* label) {
        ImGui::SetCursorScreenPos(ImVec2(cx - r, cy - r));
        bool clicked = ImGui::InvisibleButton("##category", ImVec2(r * 2.0f, r * 2.0f));
        bool hot = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = glass::fill(hot ? IM_COL32(87, 102, 5, 255) : IM_COL32(61, 71, 5, 255));
        dl->AddCircleFilled(ImVec2(cx, cy), r, bg, 40);
        glass::circle(dl, ImVec2(cx, cy), r);
        ImFont* font = ImGui::GetFont();
        float   size = r * 1.05f;
        ImVec2  ts   = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
        dl->AddText(font, size, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), label);
        return clicked;
    }

    void centred_label(ImDrawList* dl, ImVec2 c, float size, const char* text) {
        ImFont* font = ImGui::GetFont();
        ImVec2  ts   = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), kLabel, text);
    }

    void draw_controls(ImDrawList* dl, const Layout& l, const bool* held) {
        // the cross stays opaque and unclipped, the glass treatment reads badly on a plus
        // and any attempt to tile it leaves seams where the arms meet
        float cx = l.pad.x, cy = l.pad.y, r = l.pad_arm, t = l.pad_half, rd = t * 0.45f;
        dl->AddRectFilled(ImVec2(cx - t, cy - r), ImVec2(cx + t, cy + r), kPad, rd);
        dl->AddRectFilled(ImVec2(cx - r, cy - t), ImVec2(cx + r, cy + t), kPad, rd);
        if (held[BIT_UP])
            dl->AddRectFilled(ImVec2(cx - t, cy - r), ImVec2(cx + t, cy), kPadOn, rd, ImDrawFlags_RoundCornersTop);
        if (held[BIT_DOWN])
            dl->AddRectFilled(ImVec2(cx - t, cy), ImVec2(cx + t, cy + r), kPadOn, rd, ImDrawFlags_RoundCornersBottom);
        if (held[BIT_LEFT])
            dl->AddRectFilled(ImVec2(cx - r, cy - t), ImVec2(cx, cy + t), kPadOn, rd, ImDrawFlags_RoundCornersLeft);
        if (held[BIT_RIGHT])
            dl->AddRectFilled(ImVec2(cx, cy - t), ImVec2(cx + r, cy + t), kPadOn, rd, ImDrawFlags_RoundCornersRight);
        glass::cross(dl, l.pad, r, t, rd);
        dl->AddCircleFilled(l.pad, t * 0.5f, kPivot, 28);

        ImU32 b_col = held[BIT_B] ? kFaceOn : kFace;
        ImU32 a_col = held[BIT_A] ? kFaceOn : kFace;
        dl->AddCircleFilled(l.b, l.face_r, b_col, 40);
        glass::circle(dl, l.b, l.face_r);
        dl->AddCircleFilled(l.a, l.face_r, a_col, 40);
        glass::circle(dl, l.a, l.face_r);
        centred_label(dl, l.b, l.face_r * 0.85f, "B");
        centred_label(dl, l.a, l.face_r * 0.85f, "A");

        ImVec2 ph = l.pill_half;
        ImU32 sel_col = held[BIT_SELECT] ? kPillOn : kPill;
        ImU32 st_col  = held[BIT_START]  ? kPillOn : kPill;
        ImVec2 sel0(l.select.x - ph.x, l.select.y - ph.y), sel1(l.select.x + ph.x, l.select.y + ph.y);
        ImVec2 st0(l.start.x - ph.x, l.start.y - ph.y),    st1(l.start.x + ph.x, l.start.y + ph.y);
        dl->AddRectFilled(sel0, sel1, sel_col, ph.y);
        glass::rect(dl, sel0, sel1, ph.y);
        dl->AddRectFilled(st0, st1, st_col, ph.y);
        glass::rect(dl, st0, st1, ph.y);
        centred_label(dl, l.select, ph.y * 0.95f, "SELECT");
        centred_label(dl, l.start,  ph.y * 0.95f, "START");
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

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    Layout l = layout_for(out_w, out_h);

    SDL_SetRenderDrawColor(renderer, 0x17, 0x1a, 0x0d, 0xFF);
    SDL_RenderClear(renderer);

    bool held[8] = {};
    for (const auto& f : touch_buttons)
        if (f.second >= 0 && f.second < 8) held[f.second] = true;

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

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float inset = out_w * 0.018f;
    float round = out_w * 0.03f;
    dl->AddRectFilled(ImVec2(l.lcd_min.x - inset, l.lcd_min.y - inset),
                      ImVec2(l.lcd_max.x + inset, l.lcd_max.y + inset),
                      kFrame, round);
    // concentric with the frame, so the grey border keeps an even width round the corners
    dl->AddImageRounded((ImTextureID)texture, l.lcd_min, l.lcd_max,
                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, round - inset);
    draw_controls(dl, l, held);

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
        for (auto& h : touch_buttons) mem->set_button(h.second, false);
        touch_buttons.clear();
        state = AppState::MENU;
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

    SDL_RenderPresent(renderer);
    pace(kGbFps);
}

// swipe carousel, one big cover framed at a time with arrows, a title and a play button
void App::render_menu_ios() {
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
            show_debug = (cover_list[r_new] == nullptr); // jump to whichever list the new rom lands in
            int local = 0;
            for (int i = 0; i < r_new; i++)
                if ((cover_list[i] != nullptr) != show_debug)
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

    ImGui::SetCursorPos(ImVec2(0, h * 0.09f)); // low enough to clear the dynamic island
    centre_text("gameboy-emu");

    // split the library: games have cover art, debug/test roms do not; a button flips between them
    std::vector<int> view;
    for (int i = 0; i < (int)rom_list.size(); i++)
        if ((cover_list[i] != nullptr) != show_debug)
            view.push_back(i);
    int count = (int)view.size();

    // category toggle on the left of the header, the cog sits opposite it on the right:
    // "d" for the debug/test roms, "g" for games
    float hdr_r = std::max(w, h) * 0.0245f;
    float hdr_y = hdr_r * 2.4f + h * 0.03f;
    if (letter_button(hdr_r * 1.55f, hdr_y, hdr_r, show_debug ? "g" : "d")) {
        show_debug = !show_debug;
        carousel_pos = carousel_target = 0.0f; carousel_vel = 0.0f;
    }

    if (count == 0) {
        ImGui::SetCursorPos(ImVec2(0, h * 0.45f));
        centre_text(show_debug ? "no debug roms" : "no games bundled");
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
                    dl->AddRectFilled(a0, a1, IM_COL32(0x3d, 0x47, 0x03, 255), round);
                    std::string nm = display_name(rom_list[cd.r]);
                    ImFont* fnt = ImGui::GetFont();
                    float   fsz = ImGui::GetFontSize();
                    float   wrap = (a1.x - a0.x) * 0.88f;
                    ImVec2  ts = fnt->CalcTextSizeA(fsz, FLT_MAX, wrap, nm.c_str());
                    dl->AddText(fnt, fsz,
                                ImVec2((a0.x + a1.x) * 0.5f - ts.x * 0.5f,
                                       (a0.y + a1.y) * 0.5f - ts.y * 0.5f),
                                IM_COL32(0xE6, 0xED, 0xC7, 255), nm.c_str(), nullptr, wrap);
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
                dl->AddRectFilled(s0, s1, IM_COL32(0x3d, 0x47, 0x03, 255), round);
                std::string nm = display_name(rom_list[cd.r]);
                ImFont* fnt = ImGui::GetFont();
                float   fsz = ImGui::GetFontSize();
                float   wrap = slot_w * 0.88f;
                ImVec2  ts = fnt->CalcTextSizeA(fsz, FLT_MAX, wrap, nm.c_str());
                dl->AddText(fnt, fsz,
                            ImVec2((s0.x + s1.x) * 0.5f - ts.x * 0.5f,
                                   (s0.y + s1.y) * 0.5f - ts.y * 0.5f),
                            IM_COL32(0xE6, 0xED, 0xC7, 255), nm.c_str(), nullptr, wrap);
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

    draw_settings(w, h);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0x17, 0x1a, 0x0f, 0xFF); // menu background, so any edge blends in
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    pace((double)kFpsCaps[fps_index]);
}

// turns finger touches into joypad presses and drives the back button, supports several fingers at once
void App::handle_touch_ios(const SDL_Event& event) {
    if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERUP && event.type != SDL_FINGERMOTION)
        return;

    int out_w, out_h;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    float px = event.tfinger.x * out_w; // tfinger coords are normalised to the window
    float py = event.tfinger.y * out_h;

    // the round back button up top is an imgui item now, keep fingers there out of the joypad
    float br = std::max((float)out_w, (float)out_h) * 0.0245f;
    float bcx = br * 1.55f, bcy = br * 2.4f + out_h * 0.03f;
    if ((px - bcx) * (px - bcx) + (py - bcy) * (py - bcy) <= (br * 1.6f) * (br * 1.6f))
        return;

    int bit = control_at(layout_for(out_w, out_h), px, py);
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

#endif // GB_MOBILE
