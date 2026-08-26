//
// the two palettes the app draws itself in, taken straight off the app icon: the light
// icon is dark olive shapes on a light olive field, the dark icon is the same two colours
// swapped. panels and controls stay dark olive in both, only the page behind them moves,
// which is why text on a control is pale either way
//

#ifndef GAMEBOY_EMU_THEME_H
#define GAMEBOY_EMU_THEME_H

#include <cmath>
#include "imgui.h"
#include "imgui_internal.h"

struct Theme {
    bool  dark;
    ImU32 page;        // the surface everything sits on
    ImU32 panel;       // settings sheet
    ImU32 surface;     // an inactive control
    ImU32 surface_hi;  // hovered
    ImU32 accent;      // active / selected
    ImU32 button;      // resting button face
    ImU32 text;        // sits on a control, pale in both themes
    ImU32 text_page;   // sits on the page, so it has to invert
    ImU32 text_dim;
    ImU32 placeholder; // cover stand-in for a game with no artwork
    ImU32 border;      // glass hairline
    int   rim;         // peak alpha of the glass top light
    bool  rim_dark;    // draw that light dark instead of white
};

namespace theme {
    inline const Theme kDark = {
        true,
        IM_COL32( 17,  22,   2, 255),
        IM_COL32( 28,  35,   4, 255),
        IM_COL32( 48,  56,   4, 255),
        IM_COL32( 72,  84,   5, 255),
        IM_COL32( 87, 102,   5, 255),
        IM_COL32( 61,  71,   5, 255),
        IM_COL32(0xE6, 0xED, 0xC7, 255),
        IM_COL32(0xE6, 0xED, 0xC7, 255),
        IM_COL32(0x9A, 0xA3, 0x78, 255),
        IM_COL32( 61,  71,   5, 255),
        IM_COL32(255, 255, 255, 52),
        64, false
    };

    inline const Theme kLight = {
        false,
        IM_COL32(119, 125,  70, 255),
        IM_COL32( 44,  53,   8, 255),
        IM_COL32( 54,  63,  12, 255),
        IM_COL32( 74,  86,  18, 255),
        IM_COL32( 92, 107,  16, 255),
        IM_COL32( 62,  72,  14, 255),
        IM_COL32(0xE6, 0xED, 0xC7, 255),
        IM_COL32( 40,  51,   2, 255),
        IM_COL32( 62,  72,  30, 255),
        IM_COL32( 62,  72,  14, 255),
        IM_COL32(255, 255, 255, 46),
        52, false
    };

    // whichever palette is live this frame, plus whether the colour backdrop is showing
    inline const Theme* g_active = &kDark;
    inline float g_iridescence = 0.0f;   // eased 0..1, how present the blobs are

    inline const Theme& at() { return *g_active; }
    inline void use(bool dark) { g_active = dark ? &kDark : &kLight; }
}

/* the colour shelf and any cartridge running in colour get the iridescent field the icon
   has: soft blobs of one colour each, enough of them that they keep crossing, and where
   two overlap they blend into a colour neither of them owns */
namespace iri {
    struct Blob {
        float x, y;      // home position, fraction of the area
        float r;         // radius, fraction of the area's smaller side
        float fx, fy;    // drift frequency
        float px, py;    // phase
        ImU32 col;
    };

    // more of them than there are hues, so they cross constantly and the colour where two
    // meet is a third one neither of them owns
    inline const Blob kBlobs[] = {
        {0.88f, 0.14f, 0.44f, 0.041f, 0.033f, 0.0f, 1.7f, IM_COL32( 60, 215, 200, 255)},
        {1.00f, 0.48f, 0.42f, 0.029f, 0.047f, 2.1f, 0.4f, IM_COL32(155,  90, 240, 255)},
        {0.80f, 0.80f, 0.46f, 0.037f, 0.026f, 4.0f, 2.9f, IM_COL32(235,  80, 170, 255)},
        {0.62f, 0.30f, 0.34f, 0.023f, 0.039f, 1.2f, 5.1f, IM_COL32(240, 205,  80, 255)},
        {0.96f, 0.92f, 0.38f, 0.045f, 0.031f, 3.3f, 3.8f, IM_COL32(245, 130,  45, 255)},
        {0.72f, 0.02f, 0.34f, 0.034f, 0.043f, 5.6f, 1.1f, IM_COL32(115, 225,  90, 255)},
        {0.52f, 0.92f, 0.34f, 0.027f, 0.036f, 0.7f, 4.4f, IM_COL32( 65, 120, 245, 255)},
        {0.42f, 0.52f, 0.30f, 0.031f, 0.029f, 2.8f, 0.9f, IM_COL32( 85, 195, 215, 255)},
        {0.30f, 0.14f, 0.26f, 0.025f, 0.041f, 3.9f, 2.2f, IM_COL32(215, 105, 225, 255)},
        {0.16f, 0.78f, 0.26f, 0.043f, 0.028f, 1.6f, 5.7f, IM_COL32( 90, 165, 245, 255)},
        {0.60f, 0.62f, 0.32f, 0.030f, 0.045f, 5.0f, 3.1f, IM_COL32(115, 230, 175, 255)},
        {0.86f, 0.40f, 0.30f, 0.036f, 0.024f, 2.4f, 4.9f, IM_COL32(250, 170,  60, 255)},
        {0.68f, 0.46f, 0.28f, 0.049f, 0.021f, 1.9f, 3.4f, IM_COL32(240,  90, 110, 255)},
        {0.92f, 0.66f, 0.32f, 0.022f, 0.050f, 4.6f, 0.2f, IM_COL32(100, 210, 240, 255)},
        {0.46f, 0.20f, 0.26f, 0.039f, 0.035f, 3.1f, 5.4f, IM_COL32(180, 230,  85, 255)},
        {0.78f, 0.60f, 0.30f, 0.026f, 0.044f, 0.4f, 2.6f, IM_COL32(200, 120, 250, 255)},
        {0.34f, 0.74f, 0.28f, 0.047f, 0.023f, 5.3f, 1.5f, IM_COL32( 70, 235, 165, 255)},
        {0.56f, 0.06f, 0.26f, 0.033f, 0.038f, 2.6f, 4.1f, IM_COL32(250, 200, 120, 255)},
    };
    constexpr int kCount = (int)(sizeof(kBlobs) / sizeof(kBlobs[0]));

    /* the field composited to one colour at a point. the blobs are laid over each other
       exactly the way drawing them one at a time would, so where two cross the result is
       their blend rather than an average that just greys both of them out */
    inline ImU32 sample(ImVec2 p, ImVec2 a0, ImVec2 a1, float t, float strength, float unit) {
        float w = a1.x - a0.x, h = a1.y - a0.y;
        float outr = 0.0f, outg = 0.0f, outb = 0.0f, outa = 0.0f;
        float gain = theme::at().dark ? 1.25f : 1.20f;
        for (int b = 0; b < kCount; b++) {
            const Blob& s = kBlobs[b];
            float cx = a0.x + w * s.x + unit * 0.10f * std::sin(t * s.fx * 6.2831853f + s.px);
            float cy = a0.y + h * s.y + unit * 0.10f * std::cos(t * s.fy * 6.2831853f + s.py);
            float rr = unit * s.r * (1.0f + 0.06f * std::sin(t * 0.7f + s.px));
            float dx = p.x - cx, dy = p.y - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 >= rr * rr)
                continue;
            float u = std::sqrt(d2) / rr;
            // raised cosine, flat through the middle and meeting zero at the rim with no
            // slope, so a blob is a diffuse film and not a hot core with a ring round it
            float a = 0.5f * (1.0f + std::cos(3.14159265f * u))
                    * (62.0f / 255.0f) * strength * gain;
            if (a <= 0.0f)
                continue;
            float cr = (float)((s.col >> IM_COL32_R_SHIFT) & 0xFF);
            float cg = (float)((s.col >> IM_COL32_G_SHIFT) & 0xFF);
            float cb = (float)((s.col >> IM_COL32_B_SHIFT) & 0xFF);
            // source over destination, in premultiplied terms
            outr = cr * a + outr * (1.0f - a);
            outg = cg * a + outg * (1.0f - a);
            outb = cb * a + outb * (1.0f - a);
            outa = a + outa * (1.0f - a);
        }
        if (outa <= 0.0004f)
            return 0;
        float inv = 1.0f / outa;
        int r = (int)(outr * inv), g = (int)(outg * inv), bl = (int)(outb * inv);
        return IM_COL32(r > 255 ? 255 : r, g > 255 ? 255 : g, bl > 255 ? 255 : bl,
                        (int)(outa * 255.0f));
    }

    /* pulls a point onto a rounded rect if it sits outside one. the inner rect is the
       shape shrunk by its radius, so the nearest point on the outline is always the
       nearest point of that inner rect pushed back out by the radius */
    inline ImVec2 clamp_round(ImVec2 p, ImVec2 p0, ImVec2 p1, float r) {
        float kx = p.x < p0.x + r ? p0.x + r : (p.x > p1.x - r ? p1.x - r : p.x);
        float ky = p.y < p0.y + r ? p0.y + r : (p.y > p1.y - r ? p1.y - r : p.y);
        float dx = p.x - kx, dy = p.y - ky;
        float d2 = dx * dx + dy * dy;
        if (d2 <= r * r)
            return p;                       // already inside
        float inv = r / std::sqrt(d2);
        return ImVec2(kx + dx * inv, ky + dy * inv);
    }

    /* every shape is filled with one uniform grid, whatever its outline. rings fanned out
       from the middle sound tidier for a rounded rect but they make long thin wedges, and
       a blob interpolated across those breaks into streaks radiating from the centre.
       a rounded edge is held by fading each vertex out across the boundary instead, which
       needs no clip rectangle and so cannot spill into the corners either */
    inline void mesh(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding, float cell,
                     float t, float strength, float unit_scale) {
        float w = p1.x - p0.x, h = p1.y - p0.y;
        if (w <= 1.0f || h <= 1.0f)
            return;
        // blob size comes off the area they are laid in, so a small shape gets small
        // blobs unless it asks for a bigger one
        float unit = (rounding > 0.0f ? std::sqrt(w * h) : (w < h ? w : h)) * unit_scale;

        int lim = rounding > 0.0f ? 96 : 72;   // a round edge needs the finer mesh
        int cx = (int)(w / cell); cx = cx < 4 ? 4 : (cx > lim ? lim : cx);
        int cy = (int)(h / cell); cy = cy < 4 ? 4 : (cy > lim ? lim : cy);
        float cw = w / cx, ch = h / cy;

        // only the cells the live clip touches, so the four bands drawn round the screen
        // in game do not each rebuild the whole mesh
        ImVec2 kmin = dl->GetClipRectMin(), kmax = dl->GetClipRectMax();
        int i0 = (int)std::floor((kmin.x - p0.x) / cw), i1 = (int)std::ceil((kmax.x - p0.x) / cw);
        int j0 = (int)std::floor((kmin.y - p0.y) / ch), j1 = (int)std::ceil((kmax.y - p0.y) / ch);
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0;
        if (i1 > cx) i1 = cx; if (j1 > cy) j1 = cy;
        if (i1 <= i0 || j1 <= j0)
            return;

        int nx = i1 - i0, ny = j1 - j0;
        ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        dl->PrimReserve(nx * ny * 6, (nx + 1) * (ny + 1));
        unsigned int base = dl->_VtxCurrentIdx;
        for (int j = j0; j <= j1; j++) {
            for (int i = i0; i <= i1; i++) {
                ImVec2 q(p0.x + cw * i, p0.y + ch * j);
                /* every vertex that falls outside is moved onto the outline rather than
                   faded out. fading only resolves the edge where a vertex happens to sit,
                   which is what left the corners looking cut, whereas snapping puts the
                   mesh boundary exactly on the curve */
                if (rounding > 0.0f)
                    q = clamp_round(q, p0, p1, rounding);
                dl->PrimWriteVtx(q, uv, sample(q, p0, p1, t, strength, unit));
            }
        }
        for (int j = 0; j < ny; j++) {
            for (int i = 0; i < nx; i++) {
                unsigned int a = base + j * (nx + 1) + i, b = a + (nx + 1);
                dl->PrimWriteIdx((ImDrawIdx)a);       dl->PrimWriteIdx((ImDrawIdx)b);
                dl->PrimWriteIdx((ImDrawIdx)(b + 1));
                dl->PrimWriteIdx((ImDrawIdx)a);       dl->PrimWriteIdx((ImDrawIdx)(b + 1));
                dl->PrimWriteIdx((ImDrawIdx)(a + 1));
            }
        }
    }

    // a square area, plus the pale ramp the light theme needs to read against
    inline void field(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float t, float strength) {
        if (strength <= 0.003f || p1.x <= p0.x || p1.y <= p0.y)
            return;
        dl->PushClipRect(p0, p1, true);
        if (!theme::at().dark) {
            int a = (int)(120 * strength);
            dl->AddRectFilledMultiColor(p0, p1,
                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, a),
                IM_COL32(255, 255, 255, a), IM_COL32(255, 255, 255, 0));
        }
        mesh(dl, p0, p1, 0.0f, 40.0f, t, strength, 1.0f);
        dl->PopClipRect();
    }

    // the same field on a rounded shape, on a finer grid so the edge follows the round
    inline void field_rounded(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding,
                              float t, float strength, float unit_scale = 1.0f) {
        if (strength <= 0.003f || p1.x <= p0.x || p1.y <= p0.y)
            return;
        // the cell follows the corner radius, since that is what the mesh has to trace
        float cell = rounding / 6.0f;
        if (cell < 2.5f) cell = 2.5f;
        if (cell > 10.0f) cell = 10.0f;
        dl->PushClipRect(p0, p1, true);
        mesh(dl, p0, p1, rounding, cell, t, strength, unit_scale);
        dl->PopClipRect();
    }
}

#endif //GAMEBOY_EMU_THEME_H
