#pragma once
#include "imgui/imgui.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>

namespace UI {

inline float g_AnimScale = 1.0f;
inline bool  AnimOff() { return g_AnimScale >= 50.0f; }

inline float Approach(float cur, float target, float speed) {
    if (AnimOff()) return target;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.05f) dt = 0.05f;
    return cur + (target - cur) * (1.0f - expf(-speed * dt * g_AnimScale));
}

inline float Spring(ImGuiID id, float target, float k = 260.0f, float d = 22.0f) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float* x = st->GetFloatRef(id, target);
    float* v = st->GetFloatRef(id ^ 0x5bd1e995u, 0.0f);
    if (AnimOff()) { *x = target; *v = 0.0f; return target; }
    const float dt = (std::min)(ImGui::GetIO().DeltaTime * g_AnimScale, 1.0f / 30.0f);
    const int   steps = 4;
    const float h = dt / steps;
    for (int i = 0; i < steps; i++) {
        const float a = k * (target - *x) - d * *v;
        *v += a * h;
        *x += *v * h;
    }
    if (fabsf(target - *x) < 0.0005f && fabsf(*v) < 0.001f) { *x = target; *v = 0.0f; }
    return *x;
}

inline float EaseOut(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    const float u = 1.0f - x;
    return 1.0f - u * u * u;
}

inline float TrackedWidth(ImFont* font, float size, const char* text, float tracking) {
    if (!font) font = ImGui::GetFont();
    const float scale = size / font->FontSize;
    float w = 0.0f;
    for (const char* p = text; *p; p++) w += font->GetCharAdvance((ImWchar)(unsigned char)*p) * scale + tracking;
    return *text ? w - tracking : 0.0f;
}

inline float TextTracked(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col, const char* text, float tracking) {
    if (!font) font = ImGui::GetFont();
    const float scale = size / font->FontSize;
    float x = pos.x;
    for (const char* p = text; *p; p++) {
        const char buf[2] = { *p, 0 };
        dl->AddText(font, size, ImVec2(x, pos.y), col, buf);
        x += font->GetCharAdvance((ImWchar)(unsigned char)*p) * scale + tracking;
    }
    return x - pos.x - tracking;
}

inline void DrawLogo(ImDrawList* dl, ImVec2 c, float size, ImU32 col) {
    const float h = size * 0.5f;
    auto P = [&](float x, float y) { return ImVec2(c.x + x * h, c.y + y * h); };
    const ImVec2 outer[4] = { P(0.0f, -0.92f), P(0.92f, 0.0f), P(0.0f, 0.92f), P(-0.92f, 0.0f) };
    dl->AddConvexPolyFilled(outer, 4, U32A(col, 0.16f * ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f));
    dl->AddPolyline(outer, 4, col, ImDrawFlags_Closed, (std::max)(1.2f, size * 0.055f));
    const ImVec2 inner[4] = { P(0.0f, -0.42f), P(0.42f, 0.0f), P(0.0f, 0.42f), P(-0.42f, 0.0f) };
    dl->AddConvexPolyFilled(inner, 4, col);
}

inline void FadeVerts(ImDrawList* dl, int firstVtx, float alpha) {
    if (alpha >= 0.999f) return;
    if (alpha < 0.0f) alpha = 0.0f;
    for (int i = firstVtx; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer[i];
        const ImU32 a = (ImU32)(((v.col >> IM_COL32_A_SHIFT) & 0xFF) * alpha + 0.5f);
        v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }
}

}
