//
// slight glass finish shared by every button on every platform: a translucent face, a
// hairline border, and a highlight that runs the flat top edge and dies off round the corners
//

#ifndef GAMEBOY_EMU_GLASS_H
#define GAMEBOY_EMU_GLASS_H

#include <algorithm>
#include <cmath>
#include "imgui.h"

namespace glass {
    constexpr float kPi     = 3.14159265f;
    constexpr int   kFill   = 0x80;                        // how opaque a button face is
    constexpr ImU32 kBorder = IM_COL32(255, 255, 255, 52);
    constexpr int   kRim    = 64;                          // peak of the highlight
    constexpr int   kArc    = 8;                           // segments per rounded corner

    // one logical pixel, so the border stays the same apparent weight at any density
    inline float hairline() {
        return std::max(1.0f, ImGui::GetIO().FontGlobalScale);
    }

    inline ImU32 fill(ImU32 col) {
        return (col & ~IM_COL32_A_MASK) | ((ImU32)kFill << IM_COL32_A_SHIFT);
    }

    // a strip of thickness t walking pts, each point carrying its own alpha, so the
    // highlight can hold steady along an edge and fade out wherever it is told to
    inline void taper(ImDrawList* dl, const ImVec2* pts, const ImVec2* offs,
                      const float* alpha, int n) {
        ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        dl->PrimReserve((n - 1) * 6, n * 2);
        unsigned int base = dl->_VtxCurrentIdx;
        for (int i = 0; i < n; i++) {
            ImU32 c = IM_COL32(255, 255, 255, (int)(kRim * alpha[i]));
            dl->PrimWriteVtx(pts[i], uv, c);
            dl->PrimWriteVtx(ImVec2(pts[i].x + offs[i].x, pts[i].y + offs[i].y), uv, c);
        }
        for (int i = 0; i < n - 1; i++) {
            unsigned int k = base + i * 2;
            dl->PrimWriteIdx((ImDrawIdx)k);       dl->PrimWriteIdx((ImDrawIdx)(k + 1));
            dl->PrimWriteIdx((ImDrawIdx)(k + 3)); dl->PrimWriteIdx((ImDrawIdx)k);
            dl->PrimWriteIdx((ImDrawIdx)(k + 3)); dl->PrimWriteIdx((ImDrawIdx)(k + 2));
        }
    }

    // full strength across the flat part of the top edge, falling to nothing as the
    // corners turn away, so it never stops on a visible end
    inline void top_light(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rd, float t) {
        float w = p1.x - p0.x, h = p1.y - p0.y;
        if (w < t * 4.0f) return;
        rd = std::min(rd, std::min(w, h) * 0.5f);

        const int n = (kArc + 1) * 2;
        ImVec2 pts[n], offs[n];
        float  alpha[n];

        // a square cornered button has no arc to hide the ends in, so it gets a short
        // fade over the outer eighth instead
        float flat = std::max(rd, w * 0.08f);
        for (int s = 0; s < 2; s++) {
            ImVec2 c = s ? ImVec2(p1.x - rd, p0.y + rd) : ImVec2(p0.x + rd, p0.y + rd);
            for (int i = 0; i <= kArc; i++) {
                float u = (float)i / kArc;
                int   k = s * (kArc + 1) + i;
                float a = s ? 1.0f - u : u;
                alpha[k] = a;
                if (rd < 1.0f) {
                    float x = s ? p1.x - flat * (1.0f - u) : p0.x + flat * u;
                    pts[k]  = ImVec2(x, p0.y);
                    offs[k] = ImVec2(0.0f, t);
                } else {
                    float ang = s ? kPi * 1.5f + kPi * 0.5f * u : kPi + kPi * 0.5f * u;
                    float cs = std::cos(ang), sn = std::sin(ang);
                    pts[k]  = ImVec2(c.x + cs * rd, c.y + sn * rd);
                    offs[k] = ImVec2(-cs * t, -sn * t);
                }
            }
        }
        taper(dl, pts, offs, alpha, n);
    }

    inline void top_light_arc(ImDrawList* dl, ImVec2 c, float r, float t) {
        const int n = 26;
        ImVec2 pts[n], offs[n];
        float  alpha[n];
        for (int i = 0; i < n; i++) {
            float u = (float)i / (n - 1);
            float a = kPi * 1.12f + kPi * 0.76f * u;
            float cs = std::cos(a), sn = std::sin(a);
            pts[i]   = ImVec2(c.x + cs * r, c.y + sn * r);
            offs[i]  = ImVec2(-cs * t, -sn * t);
            alpha[i] = std::sin(kPi * u);
        }
        taper(dl, pts, offs, alpha, n);
    }

    inline void rect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding,
                     ImDrawFlags flags = 0, bool light = true) {
        float t = hairline();
        dl->AddRect(p0, p1, kBorder, rounding, flags, t);
        if (light) top_light(dl, p0, p1, rounding, t);
    }

    inline void circle(ImDrawList* dl, ImVec2 c, float r, bool light = true) {
        float t = hairline();
        dl->AddCircle(c, r - t * 0.5f, kBorder, 0, t);
        if (light) top_light_arc(dl, c, r - t * 0.5f, t);
    }

    // outline of a plus, traced in one closed path so it has no seams where the arms meet
    inline void cross(ImDrawList* dl, ImVec2 c, float arm, float half, float rd) {
        float x0 = c.x - arm, x1 = c.x + arm, y0 = c.y - arm, y1 = c.y + arm;
        float l = c.x - half, rr = c.x + half, u = c.y - half, d = c.y + half;

        dl->PathArcToFast(ImVec2(l + rd, y0 + rd), rd, 6, 9);
        dl->PathArcToFast(ImVec2(rr - rd, y0 + rd), rd, 9, 12);
        dl->PathLineTo(ImVec2(rr, u));
        dl->PathArcToFast(ImVec2(x1 - rd, u + rd), rd, 9, 12);
        dl->PathArcToFast(ImVec2(x1 - rd, d - rd), rd, 0, 3);
        dl->PathLineTo(ImVec2(rr, d));
        dl->PathArcToFast(ImVec2(rr - rd, y1 - rd), rd, 0, 3);
        dl->PathArcToFast(ImVec2(l + rd, y1 - rd), rd, 3, 6);
        dl->PathLineTo(ImVec2(l, d));
        dl->PathArcToFast(ImVec2(x0 + rd, d - rd), rd, 3, 6);
        dl->PathArcToFast(ImVec2(x0 + rd, u + rd), rd, 6, 9);
        dl->PathLineTo(ImVec2(l, u));
        dl->PathStroke(kBorder, ImDrawFlags_Closed, hairline());
    }

    inline bool button(const char* label, const ImVec2& size = ImVec2(0, 0)) {
        bool clicked = ImGui::Button(label, size);
        rect(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
             ImGui::GetStyle().FrameRounding);
        return clicked;
    }
}

#endif //GAMEBOY_EMU_GLASS_H
