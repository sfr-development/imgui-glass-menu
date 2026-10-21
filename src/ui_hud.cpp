#include "ui_hud.h"
#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include "ui_config.h"
#include "ui_gfx.h"
#include "ui_overlay.h"
#include "ui_registry.h"
#include "IconsLucide.h"
#include "imgui/imgui_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

HudConfig g_Hud;

using UI::Approach;
using UI::EaseOut;

static ImFont* Mono()    { return g_FontMono ? g_FontMono : ImGui::GetFont(); }
static ImFont* Section() { return g_FontSection ? g_FontSection : ImGui::GetFont(); }

static ImU32 Mix(ImU32 a, ImU32 b, float t) {
    return ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), ImClamp(t, 0.0f, 1.0f)));
}

void Hud_RenderInfoBar(int activeCount) {
    static float s_Vis = 0.0f;
    s_Vis = Approach(s_Vis, g_Hud.info ? 1.0f : 0.0f, 12.0f);
    if (s_Vis < 0.01f) return;
    const float a = s_Vis;

    static char   s_Fps[16] = "--";
    static double s_FpsT    = 0.0;
    static float  s_FpsVal  = 0.0f;
    const double now = ImGui::GetTime();
    if (now - s_FpsT > 0.25) { s_FpsT = now; s_FpsVal = ImGui::GetIO().Framerate; snprintf(s_Fps, sizeof(s_Fps), "%d", (int)(s_FpsVal + 0.5f)); }
    char clock[16];
    { SYSTEMTIME st; GetLocalTime(&st); snprintf(clock, sizeof(clock), "%02d:%02d", st.wHour, st.wMinute); }
    char active[8];
    snprintf(active, sizeof(active), "%d", activeCount);

    struct Seg { const char* value; const char* unit; ImFont* font; float size; ImU32 col; const char* icon; };
    Seg segs[4];
    int n = 0;
    const ImU32 fpsCol = s_FpsVal >= 90.0f ? Theme::Success : s_FpsVal >= 50.0f ? Theme::Warning : Theme::Danger;
    if (g_Hud.infoFps)     segs[n++] = { s_Fps,             "FPS",    Mono(),         12.5f, fpsCol,        nullptr };
    if (g_Hud.infoClock)   segs[n++] = { clock,             nullptr,  Mono(),         12.5f, Theme::Text,   nullptr };
    if (g_Hud.infoProfile) segs[n++] = { Profiles_Active(), nullptr,  ImGui::GetFont(), 13.0f, Theme::TextDim, ICON_LC_USER };
    if (g_Hud.infoActive)  segs[n++] = { active,            "ACTIVE", Mono(),         12.5f, Theme::Accent, nullptr };

    const float h = 30.0f, pad = 12.0f, gap = 11.0f;
    const float brandW = 18.0f + 6.0f + UI::TrackedWidth(Section(), 11.5f, UI_BrandTitle(), 1.8f);
    float segW[4];
    float w = pad + brandW;
    for (int i = 0; i < n; i++) {
        float sw = segs[i].font->CalcTextSizeA(segs[i].size, FLT_MAX, 0.0f, segs[i].value).x;
        if (segs[i].unit) sw += 4.0f + UI::TrackedWidth(Section(), 9.5f, segs[i].unit, 1.2f);
        if (segs[i].icon) sw += ImGui::CalcTextSize(segs[i].icon).x + 6.0f;
        segW[i] = sw;
        w += gap * 2.0f + 1.0f + sw;
    }
    w += pad;

    const ImVec2 size(w, h);
    static ImVec2 s_Pos(-1.0f, -1.0f);
    const ImVec2 target = UI_HudPos(g_Hud.infoX, g_Hud.infoY, size);
    if (s_Pos.x < 0.0f) s_Pos = target;
    s_Pos.x = Approach(s_Pos.x, target.x, 22.0f);
    s_Pos.y = Approach(s_Pos.y, target.y, 22.0f);

    const bool menuOpen = MenuAnim_IsVisible() && g_MenuAnim.alpha > 0.5f;
    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##infobar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (menuOpen && GImGui->OpenPopupStack.Size == 0) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = s_Pos, q(p.x + w, p.y + h);

    if (g_Hud.infoShadow > 0.005f) UIGfx_DrawShadow(ImGui::GetBackgroundDrawList(), p, q, h * 0.5f, a * g_Hud.infoShadow);
    UIGfx_DrawGlass(dl, p, q, h * 0.5f, a);
    dl->AddRectFilled(p, q, U32A(Theme::PrimaryBg, (Theme::GlassOn ? 0.62f : 0.9f) * a), h * 0.5f);
    dl->AddRect(p, q, U32A(Theme::Border, a), h * 0.5f);
    dl->AddLine(ImVec2(p.x + h * 0.5f, p.y + 0.5f), ImVec2(q.x - h * 0.5f, p.y + 0.5f), IM_COL32(255, 255, 255, (int)(18 * a)));

    float x = p.x + pad;
    const float cy = p.y + h * 0.5f;
    UI::DrawLogo(dl, ImVec2(x + 8.0f, cy), 15.0f, U32A(Theme::Accent, a));
    UI::TextTracked(dl, Section(), 11.5f, ImVec2(x + 24.0f, cy - 7.0f), U32A(Theme::Text, a), UI_BrandTitle(), 1.8f);
    x += brandW;
    for (int i = 0; i < n; i++) {
        x += gap;
        dl->AddLine(ImVec2(x, p.y + 8.0f), ImVec2(x, q.y - 8.0f), U32A(Theme::Border, a));
        x += 1.0f + gap;
        float sx = x;
        if (segs[i].icon) {
            dl->AddText(ImVec2(sx, cy - ImGui::GetFontSize() * 0.5f - 1.0f), U32A(Theme::TextFaint, a), segs[i].icon);
            sx += ImGui::CalcTextSize(segs[i].icon).x + 6.0f;
        }
        const ImVec2 vs = segs[i].font->CalcTextSizeA(segs[i].size, FLT_MAX, 0.0f, segs[i].value);
        dl->AddText(segs[i].font, segs[i].size, ImVec2(sx, cy - vs.y * 0.5f), U32A(segs[i].col, a), segs[i].value);
        if (segs[i].unit)
            UI::TextTracked(dl, Section(), 9.5f, ImVec2(sx + vs.x + 4.0f, cy - 5.0f), U32A(Theme::TextFaint, a), segs[i].unit, 1.2f);
        x += segW[i];
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    UI_HudDrag("##infobar_drag", s_Pos, size, &g_Hud.infoX, &g_Hud.infoY);
}

namespace {

struct KeyDef { const char* label; int vk; float w; };

const KeyDef kRow0[] = { {"ESC",VK_ESCAPE,1}, {"1",'1',1}, {"2",'2',1}, {"3",'3',1}, {"4",'4',1}, {"5",'5',1}, {"6",'6',1},
                         {"7",'7',1}, {"8",'8',1}, {"9",'9',1}, {"0",'0',1}, {"-",VK_OEM_MINUS,1}, {"=",VK_OEM_PLUS,1},
                         {"BACK",VK_BACK,2} };
const KeyDef kRow1[] = { {"TAB",VK_TAB,1.5f}, {"Q",'Q',1}, {"W",'W',1}, {"E",'E',1}, {"R",'R',1}, {"T",'T',1}, {"Y",'Y',1},
                         {"U",'U',1}, {"I",'I',1}, {"O",'O',1}, {"P",'P',1}, {"[",VK_OEM_4,1}, {"]",VK_OEM_6,1},
                         {"\\",VK_OEM_5,1.5f} };
const KeyDef kRow2[] = { {"CAPS",VK_CAPITAL,1.75f}, {"A",'A',1}, {"S",'S',1}, {"D",'D',1}, {"F",'F',1}, {"G",'G',1},
                         {"H",'H',1}, {"J",'J',1}, {"K",'K',1}, {"L",'L',1}, {";",VK_OEM_1,1}, {"'",VK_OEM_7,1},
                         {"ENTER",VK_RETURN,2.25f} };
const KeyDef kRow3[] = { {"SHIFT",VK_LSHIFT,2.25f}, {"Z",'Z',1}, {"X",'X',1}, {"C",'C',1}, {"V",'V',1}, {"B",'B',1},
                         {"N",'N',1}, {"M",'M',1}, {",",VK_OEM_COMMA,1}, {".",VK_OEM_PERIOD,1}, {"/",VK_OEM_2,1},
                         {"SHIFT",VK_RSHIFT,2.75f} };
const KeyDef kRow4[] = { {"CTRL",VK_LCONTROL,1.25f}, {"WIN",VK_LWIN,1.25f}, {"ALT",VK_LMENU,1.25f}, {"",VK_SPACE,6.25f},
                         {"ALT",VK_RMENU,1.25f}, {"FN",0,1.25f}, {"MENU",VK_APPS,1.25f}, {"CTRL",VK_RCONTROL,1.25f} };
const KeyDef* const kRows[5]  = { kRow0, kRow1, kRow2, kRow3, kRow4 };
const int           kRowN[5]  = { IM_ARRAYSIZE(kRow0), IM_ARRAYSIZE(kRow1), IM_ARRAYSIZE(kRow2), IM_ARRAYSIZE(kRow3), IM_ARRAYSIZE(kRow4) };
constexpr int       kKeyCount = IM_ARRAYSIZE(kRow0) + IM_ARRAYSIZE(kRow1) + IM_ARRAYSIZE(kRow2) + IM_ARRAYSIZE(kRow3) + IM_ARRAYSIZE(kRow4);
static_assert(kKeyCount <= 64, "per-key state arrays hold 64 keys");

const int kMouseVk[5] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };

float s_KeyP[64], s_KeyG[64];
bool  s_KeyDown[64];
float s_MouseP[5], s_MouseG[5];
bool  s_MouseDown[5];
float s_WheelUp = 0.0f, s_WheelDown = 0.0f;
bool  s_Demo = false;
double s_KeyTimes[64], s_ClickTimes[64];
int    s_KeyHead = 0, s_ClickHead = 0;

bool DemoDown(int vk, double t) {
    const double m = fmod(t, 4.0);
    switch (vk) {
        case 'W':         return m < 2.6;
        case 'A':         return m > 0.4 && m < 1.1;
        case 'D':         return m > 1.6 && m < 2.2;
        case VK_LSHIFT:   return m < 1.4;
        case VK_SPACE:    return m > 2.7 && m < 2.9;
        case 'R':         return m > 3.3 && m < 3.5;
        case VK_LBUTTON:  return fmod(t, 0.5) < 0.12 && m < 2.0;
        case VK_RBUTTON:  return m > 2.0 && m < 3.2;
        case VK_XBUTTON2: return m > 3.5 && m < 3.75;
    }
    return false;
}

bool IsDown(int vk, bool typing, double t) {
    if (!vk) return false;
    if (s_Demo) return DemoDown(vk, t);
    if (typing && vk != VK_LBUTTON && vk != VK_RBUTTON && vk != VK_MBUTTON && vk != VK_XBUTTON1 && vk != VK_XBUTTON2)
        return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

int CountRecent(const double* times, double now) {
    int n = 0;
    for (int i = 0; i < 64; i++) if (times[i] > 0.0 && now - times[i] <= 1.0) n++;
    return n;
}

void Keycap(ImDrawList* dl, ImVec2 a, ImVec2 b, const char* label, float p, float g, float s, float alpha, bool dim) {
    const float r = 4.0f * s, sink = 1.6f * s * p;
    if (g > 0.01f)
        for (int k = 3; k >= 1; k--) {
            const float e = k * 2.2f * s;
            dl->AddRectFilled(ImVec2(a.x - e, a.y - e + sink), ImVec2(b.x + e, b.y + e + sink),
                              U32A(Theme::Accent, 0.055f * g * alpha * (4 - k)), r + e);
        }
    dl->AddRectFilled(ImVec2(a.x, a.y + 2.0f * s), ImVec2(b.x, b.y + 2.0f * s), U32A(IM_COL32(6, 6, 8, 255), 0.75f * alpha), r);
    const ImVec2 ca(a.x, a.y + sink), cb(b.x, b.y + sink);
    const ImU32 base = Mix(U32A(Theme::SurfaceHi, 0.92f), U32A(Theme::Accent, 0.95f), p);
    dl->AddRectFilled(ca, cb, U32A(base, alpha * (dim ? 0.55f : 1.0f)), r);
    dl->AddRectFilledMultiColor(ImVec2(ca.x + 1, ca.y + 1), ImVec2(cb.x - 1, ca.y + (cb.y - ca.y) * 0.45f),
                                U32A(IM_COL32(255, 255, 255, 255), 0.07f * alpha), U32A(IM_COL32(255, 255, 255, 255), 0.07f * alpha),
                                U32A(IM_COL32(255, 255, 255, 255), 0.0f), U32A(IM_COL32(255, 255, 255, 255), 0.0f));
    dl->AddRect(ca, cb, U32A(Mix(Theme::Border, Theme::AccentHover, p), alpha), r);
    if (label && label[0]) {
        const bool  wide = strlen(label) > 1;
        const float fs   = (wide ? 8.5f : 11.0f) * s;
        const float tw   = UI::TrackedWidth(Section(), fs, label, wide ? 0.6f : 0.0f);
        const ImU32 tc   = U32A(Mix(dim ? Theme::TextFaint : Theme::TextDim, Theme::OnAccent, p), alpha);
        const float tx   = wide ? ca.x + 6.0f * s : (ca.x + cb.x - tw) * 0.5f;
        UI::TextTracked(dl, Section(), fs, ImVec2(tx, (ca.y + cb.y) * 0.5f - fs * 0.55f), tc, label, wide ? 0.6f : 0.0f);
    }
}

void MousePart(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, ImDrawFlags flags, float p, float alpha) {
    dl->AddRectFilled(a, b, U32A(Mix(U32A(Theme::SurfaceHi, 0.92f), U32A(Theme::Accent, 0.95f), p), alpha), rounding, flags);
    dl->AddRect(a, b, U32A(Mix(Theme::Border, Theme::AccentHover, p), alpha), rounding, flags);
}

}

void Hud_InputWheel(float delta) {
    if (delta > 0.0f) s_WheelUp = 1.0f;
    if (delta < 0.0f) s_WheelDown = 1.0f;
}

void Hud_InputDemo(bool on) { s_Demo = on; }

void Hud_RenderInputOverlay() {
    static float s_Vis = 0.0f;
    const bool show = g_Hud.input && (g_Hud.inputKeys || g_Hud.inputMouse);
    s_Vis = Approach(s_Vis, show ? 1.0f : 0.0f, 12.0f);
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;
    s_WheelUp   = ImMax(0.0f, s_WheelUp - dt * 3.0f);
    s_WheelDown = ImMax(0.0f, s_WheelDown - dt * 3.0f);
    if (s_Vis < 0.01f) return;

    const double now    = ImGui::GetTime();
    const bool   typing = io.WantTextInput;

    for (int i = 0, r = 0; r < 5; r++)
        for (int k = 0; k < kRowN[r]; k++, i++) {
            const bool down = IsDown(kRows[r][k].vk, typing, now);
            if (down && !s_KeyDown[i]) { s_KeyTimes[s_KeyHead] = now; s_KeyHead = (s_KeyHead + 1) & 63; }
            s_KeyDown[i] = down;
            s_KeyP[i] = Approach(s_KeyP[i], down ? 1.0f : 0.0f, down ? 40.0f : 16.0f);
            s_KeyG[i] = down ? 1.0f : ImMax(0.0f, s_KeyG[i] - dt * 2.4f);
        }
    for (int i = 0; i < 5; i++) {
        const bool down = IsDown(kMouseVk[i], typing, now);
        if (down && !s_MouseDown[i]) { s_ClickTimes[s_ClickHead] = now; s_ClickHead = (s_ClickHead + 1) & 63; }
        s_MouseDown[i] = down;
        s_MouseP[i] = Approach(s_MouseP[i], down ? 1.0f : 0.0f, down ? 40.0f : 16.0f);
        s_MouseG[i] = down ? 1.0f : ImMax(0.0f, s_MouseG[i] - dt * 2.4f);
    }

    const float s   = ImClamp(g_Hud.inputScale, 0.5f, 2.0f);
    const float U   = 25.0f * s, G = 3.0f * s, P = U + G, pad = 12.0f * s;
    const float kbW = g_Hud.inputKeys ? 15.0f * P - G : 0.0f;
    const float kbH = 5.0f * P - G;
    const float mW  = g_Hud.inputMouse ? 2.9f * P : 0.0f;
    const float gap = (g_Hud.inputKeys && g_Hud.inputMouse) ? 16.0f * s : 0.0f;
    const ImVec2 size(pad * 2.0f + kbW + gap + mW + (g_Hud.inputMouse ? 6.0f * s : 0.0f), pad * 2.0f + kbH);

    static ImVec2 s_Pos(-1.0f, -1.0f);
    const ImVec2 target = UI_HudPos(g_Hud.inputX, g_Hud.inputY, size);
    if (s_Pos.x < 0.0f) s_Pos = target;
    s_Pos.x = Approach(s_Pos.x, target.x, 22.0f);
    s_Pos.y = Approach(s_Pos.y, target.y, 22.0f);

    const bool menuOpen = MenuAnim_IsVisible() && g_MenuAnim.alpha > 0.5f;
    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##inputoverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (menuOpen && GImGui->OpenPopupStack.Size == 0) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float a  = s_Vis;
    const ImVec2 p = s_Pos, q(p.x + size.x, p.y + size.y);

    if (g_Hud.inputShadow > 0.005f) UIGfx_DrawShadow(ImGui::GetBackgroundDrawList(), p, q, 12.0f * s, a * g_Hud.inputShadow);
    UIGfx_DrawGlass(dl, p, q, 12.0f * s, a);
    dl->AddRectFilled(p, q, U32A(Theme::PrimaryBg, (Theme::GlassOn ? 0.58f : 0.9f) * a), 12.0f * s);
    dl->AddRect(p, q, U32A(Theme::Border, a), 12.0f * s);
    dl->AddLine(ImVec2(p.x + 12 * s, p.y + 0.5f), ImVec2(q.x - 12 * s, p.y + 0.5f), IM_COL32(255, 255, 255, (int)(20 * a)));
    dl->AddRectFilled(ImVec2(p.x + 14 * s, p.y), ImVec2(p.x + 54 * s, p.y + 2.0f), U32A(Theme::Accent, a));

    if (g_Hud.inputKeys) {
        int i = 0;
        for (int r = 0; r < 5; r++) {
            float x = p.x + pad;
            const float y = p.y + pad + r * P;
            for (int k = 0; k < kRowN[r]; k++, i++) {
                const KeyDef& kd = kRows[r][k];
                const float w = kd.w * P - G;
                Keycap(dl, ImVec2(x, y), ImVec2(x + w, y + U), kd.label, s_KeyP[i], s_KeyG[i], s, a, kd.vk == 0);
                x += kd.w * P;
            }
        }
    }

    if (g_Hud.inputMouse) {
        const float mx = p.x + pad + kbW + gap + 6.0f * s;
        const float stats = g_Hud.inputStats ? 1.15f * P : 0.0f;
        const float mh = kbH - stats;
        const ImVec2 m0(mx, p.y + pad), m1(mx + mW, p.y + pad + mh);
        const float cx = (m0.x + m1.x) * 0.5f, rad = mW * 0.5f, split = m0.y + mh * 0.44f;

        for (int b = 0; b < 5; b++) {
            if (s_MouseG[b] < 0.01f) continue;
            ImVec2 c;
            switch (b) {
                case 0:  c = ImVec2(cx - rad * 0.5f, m0.y + mh * 0.22f); break;
                case 1:  c = ImVec2(cx + rad * 0.5f, m0.y + mh * 0.22f); break;
                case 2:  c = ImVec2(cx, m0.y + mh * 0.2f); break;
                default: c = ImVec2(m0.x, m0.y + mh * (b == 3 ? 0.66f : 0.52f)); break;
            }
            for (int k = 3; k >= 1; k--) dl->AddCircleFilled(c, k * 7.0f * s, U32A(Theme::Accent, 0.06f * s_MouseG[b] * a * (4 - k)), 24);
        }

        dl->AddRectFilled(ImVec2(m0.x, m0.y + 2.5f * s), ImVec2(m1.x, m1.y + 2.5f * s), U32A(IM_COL32(6, 6, 8, 255), 0.75f * a), rad);
        dl->AddRectFilled(m0, m1, U32A(Theme::SurfaceHi, 0.75f * a), rad);
        dl->AddRect(m0, m1, U32A(Theme::Border, a), rad);
        MousePart(dl, m0, ImVec2(cx - 1.0f * s, split), rad, ImDrawFlags_RoundCornersTopLeft, s_MouseP[0], a);
        MousePart(dl, ImVec2(cx + 1.0f * s, m0.y), ImVec2(m1.x, split), rad, ImDrawFlags_RoundCornersTopRight, s_MouseP[1], a);
        UI::TextTracked(dl, Section(), 8.5f * s, ImVec2(cx - rad * 0.62f, split - 13.0f * s),
                        U32A(Mix(Theme::TextFaint, Theme::OnAccent, s_MouseP[0]), a), "L", 0.0f);
        UI::TextTracked(dl, Section(), 8.5f * s, ImVec2(cx + rad * 0.52f, split - 13.0f * s),
                        U32A(Mix(Theme::TextFaint, Theme::OnAccent, s_MouseP[1]), a), "R", 0.0f);
        const ImVec2 w0(cx - 4.0f * s, m0.y + mh * 0.11f), w1(cx + 4.0f * s, m0.y + mh * 0.30f);
        MousePart(dl, w0, w1, 4.0f * s, ImDrawFlags_RoundCornersAll, s_MouseP[2], a);
        for (int k = 1; k <= 3; k++) {
            const float gy = w0.y + (w1.y - w0.y) * k / 4.0f;
            dl->AddLine(ImVec2(w0.x + 2 * s, gy), ImVec2(w1.x - 2 * s, gy), U32A(Theme::Border, a), 1.0f);
        }
        if (s_WheelUp > 0.01f) {
            const ImVec2 t(cx, w0.y - 4.0f * s);
            dl->AddTriangleFilled(ImVec2(t.x, t.y - 4 * s), ImVec2(t.x - 4 * s, t.y), ImVec2(t.x + 4 * s, t.y), U32A(Theme::Accent, s_WheelUp * a));
        }
        if (s_WheelDown > 0.01f) {
            const ImVec2 t(cx, w1.y + 4.0f * s);
            dl->AddTriangleFilled(ImVec2(t.x, t.y + 4 * s), ImVec2(t.x + 4 * s, t.y), ImVec2(t.x - 4 * s, t.y), U32A(Theme::Accent, s_WheelDown * a));
        }
        const float tw = 7.0f * s, th = mh * 0.13f;
        const ImVec2 f0(m0.x - tw * 0.6f, m0.y + mh * 0.46f), b0(m0.x - tw * 0.6f, m0.y + mh * 0.62f);
        MousePart(dl, f0, ImVec2(f0.x + tw, f0.y + th), 3.0f * s, ImDrawFlags_RoundCornersAll, s_MouseP[4], a);
        MousePart(dl, b0, ImVec2(b0.x + tw, b0.y + th), 3.0f * s, ImDrawFlags_RoundCornersAll, s_MouseP[3], a);
        UI::TextTracked(dl, Section(), 7.5f * s, ImVec2(m0.x + 6.0f * s, f0.y + th * 0.5f - 4.0f * s),
                        U32A(Mix(Theme::TextFaint, Theme::Accent, s_MouseP[4]), a), "M5", 0.4f);
        UI::TextTracked(dl, Section(), 7.5f * s, ImVec2(m0.x + 6.0f * s, b0.y + th * 0.5f - 4.0f * s),
                        U32A(Mix(Theme::TextFaint, Theme::Accent, s_MouseP[3]), a), "M4", 0.4f);

        if (g_Hud.inputStats) {
            const float sy = p.y + pad + kbH - stats + 6.0f * s;
            char kps[8], cps[8];
            snprintf(kps, sizeof(kps), "%d", CountRecent(s_KeyTimes, now));
            snprintf(cps, sizeof(cps), "%d", CountRecent(s_ClickTimes, now));
            const float half = mW * 0.5f;
            const char* vals[2]  = { kps, cps };
            const char* units[2] = { "KPS", "CPS" };
            for (int k = 0; k < 2; k++) {
                const float colX = m0.x + k * half;
                const ImVec2 vs  = Mono()->CalcTextSizeA(14.0f * s, FLT_MAX, 0.0f, vals[k]);
                dl->AddText(Mono(), 14.0f * s, ImVec2(colX + (half - vs.x) * 0.5f, sy), U32A(Theme::Text, a), vals[k]);
                const float uw = UI::TrackedWidth(Section(), 8.0f * s, units[k], 1.0f);
                UI::TextTracked(dl, Section(), 8.0f * s, ImVec2(colX + (half - uw) * 0.5f, sy + vs.y + 1.0f * s),
                                U32A(Theme::TextFaint, a), units[k], 1.0f);
            }
            dl->AddLine(ImVec2(cx, sy + 2.0f * s), ImVec2(cx, sy + stats - 12.0f * s), U32A(Theme::Border, a));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    UI_HudDrag("##input_drag", s_Pos, size, &g_Hud.inputX, &g_Hud.inputY);
}

void Hud_InputCard() {
    UI::BeginCard("##hud_input", "INPUT OVERLAY");
    UI::Toggle("Show input overlay", &g_Hud.input);
    UI::Hint("60% keyboard and mouse with thumb buttons; pressed keys light up. Typing in text fields is never shown.");
    if (g_Hud.input) {
        UI::Toggle("Keyboard", &g_Hud.inputKeys);
        UI::Toggle("Mouse", &g_Hud.inputMouse);
        if (g_Hud.inputMouse) UI::Toggle("Keys / clicks per second", &g_Hud.inputStats);
        UI::Slider("Input overlay size", &g_Hud.inputScale, 0.6f, 1.6f, "%.2fx");
        UI::SliderPercent("Input overlay shadow", &g_Hud.inputShadow);
        if (UI::Button("Reset input overlay position", ICON_LC_ROTATE_CCW)) { g_Hud.inputX = 1.0f; g_Hud.inputY = 1.0f; }
    }
    UI::EndCard();
}

void Hud_RenderCrosshair() {
    if (!g_Hud.crosshair) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
    const ImU32 col = IM_COL32((int)(g_Hud.chColor[0] * 255), (int)(g_Hud.chColor[1] * 255),
                               (int)(g_Hud.chColor[2] * 255), (int)(g_Hud.chColor[3] * 255));
    const ImU32 outline = IM_COL32(0, 0, 0, 180);
    const float sz = g_Hud.chSize, gap = g_Hud.chGap, th = g_Hud.chThick;
    switch (g_Hud.chStyle) {
    case 0:
        if (g_Hud.chOutline) {
            dl->AddLine(ImVec2(c.x - sz - gap, c.y), ImVec2(c.x - gap, c.y), outline, th + 2.0f);
            dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + sz + gap, c.y), outline, th + 2.0f);
            dl->AddLine(ImVec2(c.x, c.y - sz - gap), ImVec2(c.x, c.y - gap), outline, th + 2.0f);
            dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + sz + gap), outline, th + 2.0f);
        }
        dl->AddLine(ImVec2(c.x - sz - gap, c.y), ImVec2(c.x - gap, c.y), col, th);
        dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + sz + gap, c.y), col, th);
        dl->AddLine(ImVec2(c.x, c.y - sz - gap), ImVec2(c.x, c.y - gap), col, th);
        dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + sz + gap), col, th);
        break;
    case 1:
        if (g_Hud.chOutline) dl->AddCircleFilled(c, sz * 0.5f + 1.0f, outline);
        dl->AddCircleFilled(c, sz * 0.5f, col);
        break;
    case 2:
        if (g_Hud.chOutline) dl->AddCircle(c, sz, outline, 0, th + 2.0f);
        dl->AddCircle(c, sz, col, 0, th);
        break;
    case 3:
        if (g_Hud.chOutline) {
            dl->AddLine(ImVec2(c.x - sz - gap, c.y), ImVec2(c.x - gap, c.y), outline, th + 2.0f);
            dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + sz + gap, c.y), outline, th + 2.0f);
            dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + sz + gap), outline, th + 2.0f);
        }
        dl->AddLine(ImVec2(c.x - sz - gap, c.y), ImVec2(c.x - gap, c.y), col, th);
        dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + sz + gap, c.y), col, th);
        dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + sz + gap), col, th);
        break;
    }
    if (g_Hud.chStyle != 1 && g_Hud.chDot) dl->AddCircleFilled(c, 1.5f, col);
}

void UI_RenderTab_Hud() {
    if (!UI::BeginColumns("##hud_cols")) return;

    UI::BeginCard("##hud_cross", "CROSSHAIR");
    UI::Toggle("Show crosshair", &g_Hud.crosshair);
    const char* styles[] = { "Cross", "Dot", "Circle", "T" };
    UI::Segmented("Crosshair style", &g_Hud.chStyle, styles, 4);
    UI::Slider("Crosshair size", &g_Hud.chSize, 2.0f, 30.0f, "%.0f px");
    if (g_Hud.chStyle == 0 || g_Hud.chStyle == 3) UI::Slider("Crosshair gap", &g_Hud.chGap, 0.0f, 20.0f, "%.0f px");
    if (g_Hud.chStyle != 1) UI::Slider("Crosshair thickness", &g_Hud.chThick, 1.0f, 5.0f, "%.1f px");
    if (g_Hud.chStyle != 1) UI::Toggle("Center dot", &g_Hud.chDot);
    UI::Toggle("Crosshair outline", &g_Hud.chOutline);
    UI::ColorRow("Crosshair color", g_Hud.chColor);
    UI::EndCard();

    UI::NextColumn();

    UI::BeginCard("##hud_info", "INFO BAR");
    UI::Toggle("Show info bar", &g_Hud.info);
    if (g_Hud.info) {
        UI::Toggle("FPS", &g_Hud.infoFps);
        UI::Toggle("Clock", &g_Hud.infoClock);
        UI::Toggle("Profile name", &g_Hud.infoProfile);
        UI::Toggle("Active feature count", &g_Hud.infoActive);
        UI::SliderPercent("Info bar shadow", &g_Hud.infoShadow);
        UI::Hint("Drag the bar while the menu is open; it snaps to the screen edges.");
        if (UI::Button("Reset bar position", ICON_LC_ROTATE_CCW)) { g_Hud.infoX = 0.0f; g_Hud.infoY = 0.0f; }
    }
    UI::EndCard();

    UI::BeginCard("##card_overlay", "ACTIVE FEATURES OVERLAY");
    if (UI::Toggle("Show overlay", &g_UI.overlay)) UIConfig_MarkDirty();
    if (UI::Toggle("Lock HUD positions", &g_UI.overlayLocked)) UIConfig_MarkDirty();
    UI::SliderPercent("Overlay shadow", &g_Hud.overlayShadow);
    UI::Hint("While the menu is open, drag the overlay, info bar or input overlay anywhere inside the marked frame.");
    if (UI::Button("Reset overlay position", ICON_LC_ROTATE_CCW)) {
        g_UI.overlayX = 0.0f;
        g_UI.overlayY = 0.28f;
        UIConfig_MarkDirty();
    }
    UI::EndCard();

    Hud_InputCard();

    UI::EndColumns();
}
