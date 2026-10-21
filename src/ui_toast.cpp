#include "ui_toast.h"
#include "ui_theme.h"
#include "ui_layout.h"
#include "ui_gfx.h"
#include "imgui/imgui_internal.h"

#include <cstdio>
#include <cstring>

struct Toast {
    char  icon[8];
    char  title[48];
    char  detail[48];
    ImU32 color;
    float age;
    float y;
};

static const int   kMax      = 5;
static const float kLifetime = 2.2f;
static Toast s_Toasts[kMax];
static bool  s_Init = false;

void UI_Toast(const char* icon, const char* title, const char* detail, ImU32 color) {
    if (!s_Init) { for (Toast& t : s_Toasts) t.age = -1.0f; s_Init = true; }
    Toast* slot = nullptr;
    for (Toast& t : s_Toasts)
        if (t.age >= 0.0f && !strcmp(t.title, title)) { slot = &t; break; }
    if (!slot) {
        Toast* oldest = &s_Toasts[0];
        for (Toast& t : s_Toasts) {
            if (t.age < 0.0f) { slot = &t; break; }
            if (t.age > oldest->age) oldest = &t;
        }
        if (!slot) slot = oldest;
        slot->y = -40.0f;
    }
    snprintf(slot->icon,   sizeof(slot->icon),   "%s", icon ? icon : "");
    snprintf(slot->title,  sizeof(slot->title),  "%s", title);
    snprintf(slot->detail, sizeof(slot->detail), "%s", detail ? detail : "");
    slot->color = color;
    slot->age   = 0.0f;
}

void UI_RenderToasts() {
    if (!s_Init) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    ImFont* mono = g_FontMono ? g_FontMono : ImGui::GetFont();

    int order[kMax], n = 0;
    for (int i = 0; i < kMax; i++) if (s_Toasts[i].age >= 0.0f) order[n++] = i;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++)
            if (s_Toasts[order[b]].age < s_Toasts[order[a]].age) { int t = order[a]; order[a] = order[b]; order[b] = t; }

    for (int k = 0; k < n; k++) {
        Toast& t = s_Toasts[order[k]];
        t.age += io.DeltaTime;
        if (t.age > kLifetime) { t.age = -1.0f; continue; }

        const float target = 18.0f + k * 44.0f;
        t.y = UI::Approach(t.y, target, 16.0f);
        const float in  = UI::EaseOut(t.age / 0.22f);
        const float out = 1.0f - UI::EaseOut((t.age - (kLifetime - 0.3f)) / 0.3f);
        const float a   = ImMin(in, out);

        const ImVec2 ts = ImGui::CalcTextSize(t.title);
        const ImVec2 ds = mono->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, t.detail);
        const float  w  = 16 + 18 + 8 + ts.x + 12 + ds.x + 16;
        const float  h  = 34.0f;
        const ImVec2 p((io.DisplaySize.x - w) * 0.5f, t.y - (1.0f - in) * 10.0f);
        const ImVec2 q(p.x + w, p.y + h);

        UIGfx_DrawGlass(dl, p, q, h * 0.5f, a);
        dl->AddRectFilled(p, q, U32A(Theme::PrimaryBg, (UIGfx_BlurReady() ? 0.70f : 0.94f) * a), h * 0.5f);
        dl->AddRect(p, q, U32A(Theme::Border, a), h * 0.5f);
        dl->AddLine(ImVec2(p.x + h * 0.5f, p.y + 0.5f), ImVec2(q.x - h * 0.5f, p.y + 0.5f), IM_COL32(255, 255, 255, (int)(18 * a)));
        dl->AddText(ImVec2(p.x + 16, p.y + (h - ImGui::GetFontSize()) * 0.5f - 1.0f), U32A(t.color, a), t.icon);
        dl->AddText(ImVec2(p.x + 42, p.y + (h - ts.y) * 0.5f), U32A(Theme::Text, a), t.title);
        dl->AddText(mono, 12.0f, ImVec2(p.x + 42 + ts.x + 12, p.y + (h - ds.y) * 0.5f), U32A(t.color, a), t.detail);
        const float life = 1.0f - t.age / kLifetime;
        dl->AddLine(ImVec2(p.x + h * 0.5f, q.y - 1.0f), ImVec2(p.x + h * 0.5f + (w - h) * life, q.y - 1.0f), U32A(t.color, 0.6f * a), 1.5f);
    }
}
