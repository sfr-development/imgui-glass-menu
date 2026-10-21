#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include "ui_config.h"
#include "ui_sound.h"
#include "ui_gfx.h"
#include "ui_registry.h"
#include "ui_palette.h"
#include "ui_toast.h"
#include "IconsLucide.h"
#include "imgui/imgui_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <objbase.h>
#include <string>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

#pragma comment(lib, "shell32.lib")

extern const unsigned int  LucideIcons_compressed_size;
extern const unsigned char LucideIcons_compressed_data[];

ImFont* g_FontBody    = nullptr;
ImFont* g_FontSidebar = nullptr;
ImFont* g_FontSection = nullptr;
ImFont* g_FontTitle   = nullptr;
ImFont* g_FontMono    = nullptr;
ImFont* g_FontBrand   = nullptr;

int g_ActiveTab = 0;

static const UI_NavItem  kNoPage   = { ICON_LC_LAYOUT_DASHBOARD, "Menu", "", "MENU", nullptr };
static const UI_NavItem* s_Nav      = &kNoPage;
static int               s_NavCount = 1;
static const char*       s_BrandTitle = "GLASS";
static const char*       s_BrandSub   = "MENU";
static const char*       s_LinkIcon   = nullptr;
static const char*       s_LinkLabel  = nullptr;
static const char*       s_LinkUrl    = nullptr;
static const char*       s_Status     = "ONLINE";

void UI_SetNavigation(const UI_NavItem* items, int count) {
    if (!items || count <= 0) { s_Nav = &kNoPage; s_NavCount = 1; return; }
    s_Nav = items; s_NavCount = count;
    if (g_ActiveTab >= count) g_ActiveTab = 0;
}
int  UI_TabCount() { return s_NavCount; }
int  UI_TabOf(void (*render)()) {
    for (int i = 0; i < s_NavCount; i++)
        if (s_Nav[i].render == render) return i;
    return -1;
}
void UI_SetBrand(const char* title, const char* subtitle) { s_BrandTitle = title ? title : ""; s_BrandSub = subtitle ? subtitle : ""; }
const char* UI_BrandTitle()    { return s_BrandTitle; }
const char* UI_BrandSubtitle() { return s_BrandSub; }
void UI_SetLink(const char* icon, const char* label, const char* url) { s_LinkIcon = icon; s_LinkLabel = label; s_LinkUrl = url; }
void UI_SetStatus(const char* text) { s_Status = text ? text : ""; }

static const UI_NavItem* FindNav(int tab) {
    return (tab >= 0 && tab < s_NavCount) ? &s_Nav[tab] : &s_Nav[0];
}

static void FadeDrawList(ImDrawList* dl, float alpha) {
    for (ImDrawVert& v : dl->VtxBuffer) {
        const ImU32 a = (ImU32)(((v.col >> IM_COL32_A_SHIFT) & 0xFF) * alpha + 0.5f);
        v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }
}

void UI_FadeWindowTree(ImGuiWindow* root, float alpha) {
    if (!root || alpha >= 0.999f) return;
    alpha = ImClamp(alpha, 0.0f, 1.0f);
    for (ImGuiWindow* w : GImGui->Windows)
        if (w->Active && w->RootWindow == root) FadeDrawList(w->DrawList, alpha);
}

bool UI_TabVisible(int tab) { return tab >= 0 && tab < s_NavCount; }

const char* UI_TabLabel(int tab) { return FindNav(tab)->label; }
const char* UI_TabIcon(int tab)  { return FindNav(tab)->icon; }
const char* UI_TabGroup(int tab) { return FindNav(tab)->group; }

static float s_TabTime   = 10.0f;
static float s_TabDir    = 1.0f;
static int   s_CardIndex = 0;
static float s_CardFade[8];
static int   s_CardDepth = 0;

static char  s_FocusLabel[64] = {};
static int   s_FocusTab       = -1;
static float s_FocusTime      = -1.0f;
static bool  s_FocusScroll    = false;
static float s_FocusScrollY   = 0.0f;

void UI_FocusSetting(int tab, const char* label) {
    if (tab != g_ActiveTab) s_TabDir = tab > g_ActiveTab ? 1.0f : -1.0f;
    g_ActiveTab = tab;
    snprintf(s_FocusLabel, sizeof(s_FocusLabel), "%s", label);
    s_FocusTab    = tab;
    s_FocusTime   = 0.0f;
    s_FocusScroll = true;
}

void UI_ToggleSidebar() {
    g_UI.sidebarCollapsed = !g_UI.sidebarCollapsed;
    UIConfig_MarkDirty();
    UISound_Play(Sfx::Click);
}

static ImU32 Mix(ImU32 a, ImU32 b, float t) {
    return ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), ImClamp(t, 0.0f, 1.0f)));
}

static ImFont* Mono() { return g_FontMono ? g_FontMono : ImGui::GetFont(); }

void UI_LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar lucideRange[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };

    auto pick = [](const char* a, const char* b) -> const char* {
        return GetFileAttributesA(a) != INVALID_FILE_ATTRIBUTES ? a : b;
    };
    const char* bodyFont    = pick("C:\\Windows\\Fonts\\SegUIVar.ttf",     "C:\\Windows\\Fonts\\segoeui.ttf");
    const char* displayFont = pick("C:\\Windows\\Fonts\\bahnschrift.ttf",  "C:\\Windows\\Fonts\\seguisb.ttf");
    const char* monoFont    = pick("C:\\Windows\\Fonts\\CascadiaMono.ttf", "C:\\Windows\\Fonts\\consola.ttf");
    const char* symFont     = "C:\\Windows\\Fonts\\seguisym.ttf";

    static const ImWchar extendedLatinCyrGreek[] = {
        0x0020, 0x00FF,
        0x0100, 0x017F,
        0x0180, 0x024F,
        0x0300, 0x036F,
        0x0370, 0x03FF,
        0x0400, 0x04FF,
        0x0500, 0x052F,
        0x1E00, 0x1EFF,
        0,
    };

    static const ImWchar symbolRanges[] = {
        0x2000, 0x206F,
        0x2070, 0x209F,
        0x20A0, 0x20CF,
        0x2100, 0x214F,
        0x2150, 0x218F,
        0x2190, 0x21FF,
        0x2200, 0x22FF,
        0x2300, 0x23FF,
        0x2460, 0x24FF,
        0x2500, 0x257F,
        0x2580, 0x259F,
        0x25A0, 0x25FF,
        0x2600, 0x26FF,
        0x2700, 0x27BF,
        0x2900, 0x297F,
        0x2B00, 0x2BFF,
        0xFB00, 0xFB4F,
        0xFE30, 0xFE4F,
        0xFF00, 0xFFEF,
        0,
    };

    auto loadFont = [&](float size, const char* path, float weight) -> ImFont* {
        ImFontConfig cfg;
        cfg.OversampleH = 3; cfg.OversampleV = 2; cfg.PixelSnapH = false;
        cfg.RasterizerMultiply = weight;
        ImFont* f = io.Fonts->AddFontFromFileTTF(path, size, &cfg, extendedLatinCyrGreek);
        if (!f) f = io.Fonts->AddFontDefault();

        {
            ImFontConfig sc;
            sc.MergeMode = true; sc.PixelSnapH = true;
            sc.OversampleH = 1; sc.OversampleV = 1;
            sc.GlyphMinAdvanceX = size * 0.6f;
            io.Fonts->AddFontFromFileTTF(symFont, size, &sc, symbolRanges);
        }

        ImFontConfig mc;
        mc.MergeMode = true; mc.PixelSnapH = true;
        mc.GlyphMinAdvanceX = size; mc.GlyphOffset = ImVec2(0, size * 0.15f);
        io.Fonts->AddFontFromMemoryCompressedTTF(
            (const void*)LucideIcons_compressed_data, (int)LucideIcons_compressed_size,
            size * 0.95f, &mc, lucideRange);
        return f;
    };

    g_FontBody    = loadFont(15.0f, bodyFont,    1.05f);
    g_FontSidebar = g_FontBody;
    g_FontSection = loadFont(12.5f, displayFont, 1.25f);
    g_FontTitle   = loadFont(26.0f, displayFont, 1.20f);
    g_FontBrand   = loadFont(19.0f, displayFont, 1.30f);
    g_FontMono    = loadFont(13.0f, monoFont,    1.05f);

    UIConfig_Load();
    UI_ApplyAppearance();
}

namespace UI {

static ImGuiID s_TipReq   = 0;
static ImGuiID s_TipId    = 0;
static char    s_TipText[640];
static const float kTipWrap = 340.0f;
static ImVec2  s_TipMin, s_TipMax;
static TipSide s_TipSide  = TipSide::Right;
static float   s_TipDelay = 0.35f;
static float   s_TipHover = 0.0f;
static float   s_TipAlpha = 0.0f;

void Tooltip(ImGuiID id, const char* text, ImVec2 anchorMin, ImVec2 anchorMax, TipSide side, float delay) {
    if (id != s_TipId) { s_TipId = id; s_TipHover = 0.0f; s_TipAlpha = 0.0f; }
    s_TipReq = id;
    snprintf(s_TipText, sizeof(s_TipText), "%s", text);
    s_TipMin = anchorMin; s_TipMax = anchorMax; s_TipSide = side; s_TipDelay = delay;
}

static void RenderTooltip() {
    const float dt = ImGui::GetIO().DeltaTime;
    if (s_TipReq) s_TipHover += dt; else s_TipHover = 0.0f;
    const bool show = s_TipReq != 0 && s_TipHover >= s_TipDelay;
    s_TipAlpha = Approach(s_TipAlpha, show ? 1.0f : 0.0f, show ? 18.0f : 26.0f);
    s_TipReq = 0;
    if (s_TipAlpha < 0.01f || !s_TipText[0]) return;

    ImDrawList*  dl = ImGui::GetForegroundDrawList();
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const ImVec2 ts = ImGui::CalcTextSize(s_TipText, nullptr, false, kTipWrap);
    const float  a  = s_TipAlpha, e = EaseOut(a);
    const ImVec2 sz(ts.x + 20.0f, ts.y + 12.0f);
    ImVec2 p;
    if (s_TipSide == TipSide::Right)
        p = ImVec2(s_TipMax.x + 12.0f - (1.0f - e) * 6.0f, (s_TipMin.y + s_TipMax.y) * 0.5f - sz.y * 0.5f);
    else
        p = ImVec2((s_TipMin.x + s_TipMax.x) * 0.5f - sz.x * 0.5f, s_TipMax.y + 10.0f - (1.0f - e) * 5.0f);
    p.x = ImClamp(p.x, 6.0f, ds.x - sz.x - 6.0f);
    p.y = ImClamp(p.y, 6.0f, ds.y - sz.y - 6.0f);
    const ImVec2 q(p.x + sz.x, p.y + sz.y);

    UIGfx_DrawShadow(dl, p, q, 6.0f, a * 0.5f);
    UIGfx_DrawGlass(dl, p, q, 6.0f, a);
    dl->AddRectFilled(p, q, U32A(Theme::Surface, (Theme::GlassOn ? 0.86f : 0.98f) * a), 6.0f);
    dl->AddRect(p, q, U32A(Theme::Border, a), 6.0f);
    const ImU32 ac = U32A(Theme::Surface, a);
    if (s_TipSide == TipSide::Right) {
        const float cy = (p.y + q.y) * 0.5f;
        dl->AddTriangleFilled(ImVec2(p.x - 5, cy), ImVec2(p.x + 1, cy - 5), ImVec2(p.x + 1, cy + 5), ac);
        dl->AddLine(ImVec2(p.x - 5, cy), ImVec2(p.x, cy - 5), U32A(Theme::Border, a));
        dl->AddLine(ImVec2(p.x - 5, cy), ImVec2(p.x, cy + 5), U32A(Theme::Border, a));
    } else {
        const float cx = ImClamp((s_TipMin.x + s_TipMax.x) * 0.5f, p.x + 10, q.x - 10);
        dl->AddTriangleFilled(ImVec2(cx, p.y - 5), ImVec2(cx - 5, p.y + 1), ImVec2(cx + 5, p.y + 1), ac);
        dl->AddLine(ImVec2(cx, p.y - 5), ImVec2(cx - 5, p.y), U32A(Theme::Border, a));
        dl->AddLine(ImVec2(cx, p.y - 5), ImVec2(cx + 5, p.y), U32A(Theme::Border, a));
    }
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.x + 10, p.y + 6), U32A(Theme::Text, a), s_TipText,
                nullptr, kTipWrap);
}

void SectionHeader(const char* label) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  sz = g_FontSection ? g_FontSection->FontSize : 12.0f;
    TextTracked(dl, g_FontSection, sz, p, Theme::TextFaint, label, 1.6f);
    ImGui::Dummy(ImVec2(0, sz + 2.0f));
}

static void FocusFx(const char* label, const ImRect& bb) {
    if (s_FocusTime < 0.0f || g_ActiveTab != s_FocusTab || strcmp(label, s_FocusLabel) != 0) return;
    if (s_FocusScroll) { s_FocusScrollY = bb.Min.y; s_FocusScroll = false; }
    const float t = s_FocusTime / 1.8f;
    if (t >= 1.0f) return;
    const float pulse = 0.55f + 0.45f * cosf(s_FocusTime * 9.0f);
    const float a = (1.0f - t) * pulse;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImRect r(ImVec2(bb.Min.x - 4, bb.Min.y - 2), ImVec2(bb.Max.x + 4, bb.Max.y + 2));
    dl->AddRectFilled(r.Min, r.Max, U32A(Theme::Accent, 0.10f * a), Theme::RadiusFrame);
    dl->AddRect(r.Min, r.Max, U32A(Theme::Accent, 0.85f * a), Theme::RadiusFrame, 0, 1.5f);
}

static void CardHeader(int index, const char* title) {
    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    const ImVec2 p     = ImGui::GetCursorScreenPos();
    const float  avail = ImGui::GetContentRegionAvail().x;
    const float  sz    = g_FontSection ? g_FontSection->FontSize : 12.0f;

    char num[8];
    snprintf(num, sizeof(num), "%02d", index + 1);
    dl->AddText(Mono(), 11.5f, ImVec2(p.x, p.y + 1.0f), Theme::Accent, num);
    const float nx = p.x + Mono()->CalcTextSizeA(11.5f, FLT_MAX, 0.0f, num).x + 10.0f;
    const float tw = TextTracked(dl, g_FontSection, sz, ImVec2(nx, p.y), Theme::TextDim, title, 1.6f);
    const float ly = p.y + sz * 0.55f;
    if (nx + tw + 12.0f < p.x + avail)
        dl->AddLine(ImVec2(nx + tw + 12.0f, ly), ImVec2(p.x + avail, ly), Theme::Separator, 1.0f);
    ImGui::Dummy(ImVec2(avail, sz + 8.0f));
}

bool BeginCard(const char* id, const char* title) {
    const int   idx = s_CardIndex++;
    const float e   = EaseOut((s_TabTime - idx * 0.045f) / 0.32f);

    if (s_CardDepth < IM_ARRAYSIZE(s_CardFade)) s_CardFade[s_CardDepth] = e;
    s_CardDepth++;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (1.0f - e) * 18.0f * s_TabDir,
                               ImGui::GetCursorPosY() + (1.0f - e) * 8.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(Glass(Theme::Surface, 0.50f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Theme::RadiusCard);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    const float width = ImGui::GetCurrentTable() ? -FLT_MIN : -28.0f;
    const bool open = ImGui::BeginChild(id, ImVec2(width, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    if (title) CardHeader(idx, title);
    return open;
}

void EndCard() {
    ImGuiWindow* w  = ImGui::GetCurrentWindow();
    const ImVec2 mn = w->Pos, mx = ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y);
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    float* h = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##cardhov"), 0.0f);
    *h = Approach(*h, hovered ? 1.0f : 0.0f, 12.0f);

    ImDrawList* dl = w->DrawList;
    dl->PushClipRect(mn, mx, true);
    dl->AddLine(ImVec2(mn.x + Theme::RadiusCard, mn.y + 0.5f), ImVec2(mx.x - Theme::RadiusCard, mn.y + 0.5f),
                IM_COL32(255, 255, 255, (int)(14 + 16 * *h)));
    if (*h > 0.01f) {
        const float L = 12.0f * *h;
        const ImU32 c = U32A(Theme::Accent, 0.9f * *h);
        dl->AddLine(ImVec2(mn.x + 1, mn.y + 1), ImVec2(mn.x + 1 + L, mn.y + 1), c, 1.5f);
        dl->AddLine(ImVec2(mn.x + 1, mn.y + 1), ImVec2(mn.x + 1, mn.y + 1 + L), c, 1.5f);
        dl->AddLine(ImVec2(mx.x - 1, mx.y - 1), ImVec2(mx.x - 1 - L, mx.y - 1), c, 1.5f);
        dl->AddLine(ImVec2(mx.x - 1, mx.y - 1), ImVec2(mx.x - 1, mx.y - 1 - L), c, 1.5f);
    }
    dl->PopClipRect();

    s_CardDepth--;
    if (s_CardDepth >= 0 && s_CardDepth < IM_ARRAYSIZE(s_CardFade) && s_CardFade[s_CardDepth] < 0.999f)
        FadeDrawList(dl, s_CardFade[s_CardDepth]);

    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 4));
}

static void DrawKeyChip(ImDrawList* dl, ImVec2 p, const char* text, ImU32 textCol, ImU32 bg, ImU32 border) {
    const ImVec2 ts = Mono()->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, text);
    const ImVec2 q(p.x + ts.x + 12, p.y + 18);
    dl->AddRectFilled(p, q, bg, 3.0f);
    dl->AddRect(p, q, border, 3.0f);
    dl->AddText(Mono(), 11.0f, ImVec2(p.x + 6, p.y + (18 - ts.y) * 0.5f), textCol, text);
}

static struct { float x, y, rowBottom; int frame; ImGuiWindow* win; bool used; } s_LastLabel = { 0, 0, 0, -1, nullptr, true };

static void ChangedMark(const void* ptr, ImVec2 dot, const ImRect& row, const char* label) {
    s_LastLabel = { dot.x - 7.0f, dot.y, row.Max.y, ImGui::GetFrameCount(), ImGui::GetCurrentWindow(), false };
    const int field = Registry_IndexOf(ptr);
    if (field < 0) return;
    const bool modified = Registry_IsModified(field);
    if (modified && row.Contains(ImGui::GetIO().MousePos) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        Registry_Reset(field);
        UISound_Play(Sfx::ToggleOff);
        UI_Toast(ICON_LC_ROTATE_CCW, label, "reset to default", Theme::Accent);
    }
}

static const float kModeW = 84.0f, kModeH = 18.0f;
static const float kTrackW = 30.0f, kTrackH = 14.0f;

static void DrawModeSwitch(ImDrawList* dl, ImGuiID id, ImVec2 p, bool hold, bool hovered, float alpha) {
    const float k  = Spring(id, hold ? 0.0f : 1.0f, 420.0f, 26.0f);
    const float kc = ImClamp(k, 0.0f, 1.0f);
    const float cy = p.y + kModeH * 0.5f;

    const ImVec2 t0(p.x + kModeW - kTrackW, cy - kTrackH * 0.5f), t1(p.x + kModeW, cy + kTrackH * 0.5f);
    dl->AddRectFilled(t0, t1, U32A(Mix(Theme::SecondaryBg, Mix(Theme::SurfaceHi, Theme::Accent, 0.35f), 0.4f + 0.6f * kc), alpha), kTrackH * 0.5f);
    dl->AddRect(t0, t1, U32A(hovered ? Theme::Accent : Theme::Border, alpha), kTrackH * 0.5f);
    dl->AddCircleFilled(ImVec2(t0.x + 7.0f, cy), 1.3f, U32A(Theme::TextFaint, 0.7f * alpha), 8);
    dl->AddCircleFilled(ImVec2(t1.x - 7.0f, cy), 1.3f, U32A(Theme::TextFaint, 0.7f * alpha), 8);
    const float kx = ImLerp(t0.x + 7.0f, t1.x - 7.0f, k);
    dl->AddCircleFilled(ImVec2(kx, cy + 0.5f), 6.0f, U32A(IM_COL32(0, 0, 0, 255), 0.35f * alpha), 16);
    dl->AddCircleFilled(ImVec2(kx, cy), 5.5f, U32A(Theme::Text, alpha), 16);
    dl->AddCircleFilled(ImVec2(kx, cy), 2.0f, U32A(Theme::Accent, alpha), 10);

    const char* names[2] = { "HOLD", "TOGGLE" };
    const float right = t0.x - 6.0f;
    for (int i = 0; i < 2; i++) {
        const float vis = i == 0 ? 1.0f - kc : kc;
        if (vis < 0.02f) continue;
        const float tw = TrackedWidth(g_FontSection, 10.0f, names[i], 0.8f);
        const float dy = (1.0f - vis) * (i == 0 ? -4.0f : 4.0f);
        TextTracked(dl, g_FontSection, 10.0f, ImVec2(right - tw, cy - 6.0f + dy),
                    U32A(Mix(Theme::TextDim, Theme::Accent, 0.55f), vis * alpha), names[i], 0.8f);
    }
}

static void ModeToast(const char* label, int vk, bool hold) {
    char title[48];
    const char* end = ImGui::FindRenderedTextEnd(label);
    snprintf(title, sizeof(title), "%.*s%s%s", (int)(end - label), label, vk ? "  -  " : "", vk ? Hotkey_Name(vk) : "");
    UI_Toast(hold ? ICON_LC_HAND : ICON_LC_TOGGLE_RIGHT, title,
             hold ? "HOLD - active while the key is held" : "TOGGLE - each press switches", Theme::Accent);
}

static const char* kModeTip = "Hold: active only while the key is held\nToggle: each press switches it on or off";

bool Toggle(const char* label, bool* v, int flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID id     = window->GetID(label);
    const ImVec2  textSz = ImGui::CalcTextSize(label, nullptr, true);
    const float   tw = 32.0f, th = 16.0f, rowH = 28.0f;
    const ImVec2  pos    = window->DC.CursorPos;
    const float   avail  = ImGui::GetContentRegionAvail().x;
    const ImRect  bb(ImVec2(pos.x - 6.0f, pos.y), ImVec2(pos.x + ImMax(avail, textSz.x + tw + 16.0f) + 6.0f, pos.y + rowH));

    ImGui::ItemSize(ImVec2(avail, rowH), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    const float cy = bb.GetCenter().y;
    const ImVec2 tMin(bb.Max.x - 6.0f - tw, cy - th * 0.5f);
    const ImVec2 tMax(bb.Max.x - 6.0f,      cy + th * 0.5f);

    const int   field     = Registry_IndexOf(v);
    const int   vk        = field >= 0 ? Hotkey_Get(field) : 0;
    const bool  capturing = field >= 0 && Hotkey_CapturingIndex() == field;
    const char* chipText  = capturing ? "PRESS KEY" : (vk ? Hotkey_Name(vk) : ICON_LC_KEYBOARD);
    const float chipW     = (vk || capturing) ? Mono()->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, chipText).x + 12.0f
                                              : ImGui::CalcTextSize(chipText).x + 12.0f;
    const ImRect chip(ImVec2(tMin.x - 10.0f - chipW, cy - 9.0f), ImVec2(tMin.x - 10.0f, cy + 9.0f));
    const bool   noMode   = (flags & ToggleFlags_NoHoldMode) != 0;
    if (noMode && field >= 0 && Hotkey_GetMode(field) == HotkeyMode::Hold) Hotkey_SetMode(field, HotkeyMode::Toggle);
    const bool   showMode = vk != 0 && !capturing && !noMode;
    const ImRect mode(ImVec2(chip.Min.x - 6.0f - kModeW, cy - kModeH * 0.5f), ImVec2(chip.Min.x - 6.0f, cy + kModeH * 0.5f));
    const bool   hold     = field >= 0 && Hotkey_GetMode(field) == HotkeyMode::Hold;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    const ImVec2 mouse  = ImGui::GetIO().MousePos;
    const bool overChip = field >= 0 && chip.Contains(mouse);
    const bool overMode = showMode && mode.Contains(mouse);
    if (pressed && overChip) {
        Hotkey_BeginCapture(field);
        UISound_Play(Sfx::Click);
        pressed = false;
    } else if (pressed && overMode) {
        Hotkey_SetMode(field, hold ? HotkeyMode::Toggle : HotkeyMode::Hold);
        UISound_Play(Sfx::Click);
        ModeToast(label, vk, !hold);
        pressed = false;
    } else if (pressed) {
        *v = !*v;
        ImGui::MarkItemEdited(id);
        UISound_Play(*v ? Sfx::ToggleOn : Sfx::ToggleOff);
    }

    ImGuiStorage* st = ImGui::GetStateStorage();
    float* hov = st->GetFloatRef(id + 1, 0.0f);
    float* prs = st->GetFloatRef(id + 2, 0.0f);
    *hov = Approach(*hov, hovered ? 1.0f : 0.0f, 16.0f);
    *prs = Approach(*prs, (held && !overChip && !overMode) ? 1.0f : 0.0f, 24.0f);
    const float te = Spring(id, *v ? 1.0f : 0.0f, 320.0f, 20.0f);
    const float tc = ImClamp(te, 0.0f, 1.0f);

    ImDrawList* dl = window->DrawList;
    if (*hov > 0.01f) dl->AddRectFilled(bb.Min, bb.Max, U32A(Theme::SurfaceHi, 0.45f * *hov), Theme::RadiusFrame);
    FocusFx(label, bb);

    dl->AddRectFilled(tMin, tMax, Mix(Theme::SurfaceHi, Theme::Accent, tc), 4.0f);
    dl->AddRect(tMin, tMax, Mix(Mix(Theme::Border, Theme::TextFaint, *hov * 0.5f), Theme::Accent, tc), 4.0f);

    const float ks = 10.0f, stretch = 4.0f * *prs;
    const float kx0 = ImLerp(tMin.x + 3.0f, tMax.x - 3.0f - ks, te);
    const ImVec2 kMin(kx0 - (*v ? stretch : 0.0f), cy - ks * 0.5f);
    const ImVec2 kMax(kx0 + ks + (*v ? 0.0f : stretch), cy + ks * 0.5f);
    dl->AddRectFilled(kMin, kMax, Mix(Theme::TextFaint, Theme::OnAccent, tc), 2.0f);

    float labelLimit = tMin.x - 10.0f;
    if (field >= 0) {
        if (capturing) {
            const float p = 0.6f + 0.4f * sinf((float)ImGui::GetTime() * 8.0f);
            DrawKeyChip(dl, chip.Min, chipText, U32A(Theme::Accent, p), U32A(Theme::Accent, 0.12f), U32A(Theme::Accent, 0.6f * p));
            labelLimit = chip.Min.x - 8.0f;
        } else if (vk) {
            const bool ch = overChip && hovered;
            DrawKeyChip(dl, chip.Min, chipText, ch ? Theme::Text : Theme::TextDim, Theme::SurfaceHi, ch ? Theme::Accent : Theme::Border);
            if (showMode) DrawModeSwitch(dl, id + 4, mode.Min, hold, overMode && hovered, 1.0f);
            labelLimit = (showMode ? mode.Min.x : chip.Min.x) - 8.0f;
        } else if (*hov > 0.01f) {
            const ImU32 c = U32A(overChip ? Theme::Accent : Theme::TextFaint, *hov);
            dl->AddRect(chip.Min, chip.Max, U32A(overChip ? Theme::Accent : Theme::Border, *hov), 3.0f);
            dl->AddText(ImVec2(chip.Min.x + 6, cy - ImGui::GetFontSize() * 0.5f - 1.0f), c, chipText);
        }
        if (overChip && hovered && !capturing)
            Tooltip(id + 3, vk ? "Click to rebind, Esc clears" : "Bind a hotkey", chip.Min, chip.Max, TipSide::Below);
        if (overMode && hovered)
            Tooltip(id + 5, kModeTip, mode.Min, mode.Max, TipSide::Below);
    }

    dl->PushClipRect(ImVec2(pos.x - 6.0f, bb.Min.y), ImVec2(labelLimit, bb.Max.y), true);
    dl->AddText(ImVec2(pos.x, cy - textSz.y * 0.5f), Mix(Theme::TextDim, Theme::Text, ImMax(tc, *hov)),
                label, ImGui::FindRenderedTextEnd(label));
    dl->PopClipRect();
    ChangedMark(v, ImVec2(ImMin(pos.x + textSz.x + 7.0f, labelLimit - 2.0f), cy), bb, label);
    return pressed;
}

bool Keybind(const char* label, int* vk, int* mode, bool enabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGui::PushID(label);
    const ImGuiID id    = ImGui::GetID("##kb");
    const ImVec2  pos   = window->DC.CursorPos;
    const float   avail = ImGui::GetContentRegionAvail().x;
    const float   rowH  = 32.0f;
    const ImRect  bb(ImVec2(pos.x - 6.0f, pos.y), ImVec2(pos.x + avail + 6.0f, pos.y + rowH));
    ImGui::ItemSize(ImVec2(avail, rowH), 0.0f);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }

    const float  cy        = bb.GetCenter().y;
    const bool   capturing = KeyCapture_Active(vk);
    const float  keyW      = 104.0f;
    const ImRect key(ImVec2(bb.Max.x - 6.0f - keyW, cy - 13.0f), ImVec2(bb.Max.x - 6.0f, cy + 13.0f));
    const ImRect sw(ImVec2(key.Min.x - 8.0f - kModeW, cy - kModeH * 0.5f), ImVec2(key.Min.x - 8.0f, cy + kModeH * 0.5f));

    bool hovered, held;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held) && enabled;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool overKey = key.Contains(mouse), overMode = mode && sw.Contains(mouse);
    const int  before  = *vk;
    if (pressed && overKey) {
        if (capturing) KeyCapture_Cancel(); else KeyCapture_Begin(vk);
        UISound_Play(Sfx::Click);
    } else if (pressed && overMode) {
        *mode = *mode == 0 ? 1 : 0;
        UISound_Play(Sfx::Click);
        ModeToast(label, *vk, *mode == 0);
    }
    if (!enabled && capturing) KeyCapture_Cancel();

    const float a = enabled ? 1.0f : 0.38f;
    float* hv = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.0f);
    *hv = Approach(*hv, (hovered && enabled) ? 1.0f : 0.0f, 16.0f);
    ImDrawList* dl = window->DrawList;
    if (*hv > 0.01f) dl->AddRectFilled(bb.Min, bb.Max, U32A(Theme::SurfaceHi, 0.45f * *hv), Theme::RadiusFrame);
    FocusFx(label, bb);

    const bool  isMouse = *vk == VK_RBUTTON || *vk == VK_MBUTTON || *vk == VK_XBUTTON1 || *vk == VK_XBUTTON2;
    const float pulse   = capturing ? 0.6f + 0.4f * sinf((float)ImGui::GetTime() * 8.0f) : 1.0f;
    const ImU32 border  = capturing ? U32A(Theme::Accent, 0.8f * pulse) : (overKey && hovered ? Theme::Accent : Theme::Border);
    const ImU32 fill    = capturing ? Mix(Theme::SecondaryBg, Theme::Accent, 0.12f) : Theme::SecondaryBg;
    dl->AddRectFilled(key.Min, key.Max, U32A(fill, a), Theme::RadiusFrame + 1.0f);
    dl->AddRect(key.Min, key.Max, U32A(border, a), Theme::RadiusFrame + 1.0f);
    dl->AddLine(ImVec2(key.Min.x + 3, key.Max.y), ImVec2(key.Max.x - 3, key.Max.y), U32A(Theme::Border, a), 1.5f);
    const char* icon = (isMouse && !capturing) ? ICON_LC_MOUSE : ICON_LC_KEYBOARD;
    const float fh   = ImGui::GetFontSize();
    dl->AddText(ImVec2(key.Min.x + 9, cy - fh * 0.5f - 1.0f), U32A(capturing ? Theme::Accent : Theme::TextFaint, a * pulse), icon);
    const char*  name = capturing ? "PRESS KEY" : (*vk ? Hotkey_Name(*vk) : "NONE");
    const ImVec2 ns   = Mono()->CalcTextSizeA(11.5f, FLT_MAX, 0.0f, name);
    dl->AddText(Mono(), 11.5f, ImVec2(key.Min.x + 30 + (keyW - 38 - ns.x) * 0.5f, cy - ns.y * 0.5f),
                U32A(capturing ? Theme::Accent : Theme::Text, a * pulse), name);

    if (mode) DrawModeSwitch(dl, ImGui::GetID("mode"), sw.Min, *mode == 0, overMode && hovered, a);

    const ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    const float  labelLimit = (mode ? sw.Min.x : key.Min.x) - 8.0f;
    dl->PushClipRect(bb.Min, ImVec2(labelLimit, bb.Max.y), true);
    dl->AddText(ImVec2(pos.x, cy - ts.y * 0.5f), U32A(Mix(Theme::TextDim, Theme::Text, *hv), a), label, ImGui::FindRenderedTextEnd(label));
    dl->PopClipRect();
    if (enabled) ChangedMark(vk, ImVec2(ImMin(pos.x + ts.x + 7.0f, labelLimit - 2.0f), cy), bb, label);

    if (hovered && enabled) {
        if (overKey)       Tooltip(id + 1, capturing ? "Press a key or mouse button, Esc cancels" : "Click, then press a key or mouse button",
                                   key.Min, key.Max, TipSide::Below);
        else if (overMode) Tooltip(id + 2, kModeTip, sw.Min, sw.Max, TipSide::Below);
    }
    ImGui::PopID();
    return *vk != before;
}

void Hint(const char* text) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems || !text || !text[0]) return;
    const float cursorY = window->DC.CursorPos.y;
    if (s_LastLabel.frame != ImGui::GetFrameCount() || s_LastLabel.used || s_LastLabel.win != window ||
        cursorY - s_LastLabel.rowBottom > 24.0f || cursorY < s_LastLabel.y)
        return;
    s_LastLabel.used = true;
    const ImVec2 c(s_LastLabel.x + 11.0f, s_LastLabel.y);
    const float r = 6.5f;
    const ImRect hit(ImVec2(c.x - r - 3, c.y - r - 3), ImVec2(c.x + r + 3, c.y + r + 3));
    const ImGuiID id = window->GetID(text);
    const bool hov = hit.Contains(ImGui::GetIO().MousePos) &&
                     ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    float* a = ImGui::GetStateStorage()->GetFloatRef(id ^ 0x6e1f0a5u, 0.0f);
    *a = Approach(*a, hov ? 1.0f : 0.0f, 18.0f);

    ImDrawList* dl = window->DrawList;
    const ImU32 col = Mix(Theme::TextFaint, Theme::Accent, *a);
    if (*a > 0.01f) dl->AddCircleFilled(c, r + 1.5f * *a, U32A(Theme::Accent, 0.14f * *a), 20);
    dl->AddCircle(c, r, col, 20, 1.2f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - 2.9f), 1.05f, col, 8);
    dl->AddLine(ImVec2(c.x, c.y - 0.9f), ImVec2(c.x, c.y + 3.4f), col, 1.4f);
    if (hov) Tooltip(id, text, hit.Min, hit.Max, TipSide::Right, 0.08f);
}

bool Button(const char* label, const char* icon) {
    ImGui::PushID(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x, h = 32.0f;
    const bool clicked = ImGui::InvisibleButton("##btn", ImVec2(w, h));
    const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    float* hv = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.0f);
    *hv = Approach(*hv, hov ? 1.0f : 0.0f, 16.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float in = act ? 1.0f : 0.0f;
    const ImVec2 a(p.x + in, p.y + in), b(p.x + w - in, p.y + h - in);
    dl->AddRectFilled(a, b, act ? Theme::Border : Mix(Glass(Theme::SurfaceHi, 0.8f), Theme::Border, *hv * 0.6f), Theme::RadiusFrame + 1.0f);
    dl->AddRect(a, b, Mix(Theme::Border, Theme::Accent, *hv * 0.7f), Theme::RadiusFrame + 1.0f);
    dl->AddLine(ImVec2(a.x + 4, a.y + 0.5f), ImVec2(b.x - 4, a.y + 0.5f), IM_COL32(255, 255, 255, 14));
    const ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    const float  iw = icon ? ImGui::CalcTextSize(icon).x + 8.0f : 0.0f;
    const float  x  = p.x + (w - ts.x - iw) * 0.5f, cy = p.y + h * 0.5f;
    if (icon) dl->AddText(ImVec2(x, cy - ImGui::GetFontSize() * 0.5f - 1.0f), Mix(Theme::TextFaint, Theme::Accent, *hv), icon);
    dl->AddText(ImVec2(x + iw, cy - ts.y * 0.5f), Mix(Theme::TextDim, Theme::Text, *hv), label, ImGui::FindRenderedTextEnd(label));
    if (clicked) UISound_Play(Sfx::Click);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 1));
    return clicked;
}

static bool HasLabel(const char* label) { return label[0] && !(label[0] == '#' && label[1] == '#'); }

static ImGuiID s_EditId = 0;
static char    s_EditBuf[32];
static bool    s_EditFocus = false;

static bool SliderCore(const char* label, ImGuiDataType type, void* v, const void* mn, const void* mx, const char* fmt,
                       const void* markPtr = nullptr) {
    ImGui::PushID(label);
    const ImGuiID key    = ImGui::GetID("state");
    const ImGuiID editId = ImGui::GetID("edit");
    ImGuiStorage* st     = ImGui::GetStateStorage();
    const bool wasActive = st->GetBool(key, false);
    bool changed = false;

    const bool  isF = type == ImGuiDataType_Float;
    const float fv  = isF ? *(float*)v : (float)*(int*)v;
    const float fmn = isF ? *(const float*)mn : (float)*(const int*)mn;
    const float fmx = isF ? *(const float*)mx : (float)*(const int*)mx;

    char val[64];
    if (isF) snprintf(val, sizeof(val), fmt, *(float*)v);
    else     snprintf(val, sizeof(val), fmt, *(int*)v);

    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    ImVec2 captionMin = rowStart, captionMax = rowStart;

    {
        const float x0 = ImGui::GetCursorPosX(), avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 vsz = Mono()->CalcTextSizeA(Mono()->FontSize, FLT_MAX, 0.0f, val);
        ImGui::TextColored(V4(Theme::TextDim), "%s", HasLabel(label) ? label : "");
        captionMin = ImGui::GetItemRectMin();
        captionMax = ImGui::GetItemRectMax();

        if (s_EditId == editId) {
            const float ew = 96.0f;
            ImGui::SameLine(x0 + avail - ew);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
            ImGui::PushFont(Mono());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, V4(Theme::SecondaryBg));
            ImGui::PushStyleColor(ImGuiCol_Border,  V4(Theme::Accent));
            ImGui::SetNextItemWidth(ew);
            if (s_EditFocus) { ImGui::SetKeyboardFocusHere(); s_EditFocus = false; }
            const bool enter = ImGui::InputText("##edit", s_EditBuf, sizeof(s_EditBuf),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsScientific);
            const bool commit = enter || ImGui::IsItemDeactivatedAfterEdit();
            if (commit) {
                const float nv = ImClamp((float)atof(s_EditBuf), fmn, fmx);
                if (isF) *(float*)v = nv; else *(int*)v = (int)lroundf(nv);
                changed = true;
                s_EditId = 0;
                UISound_Play(Sfx::ToggleOn);
            } else if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                s_EditId = 0;
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            ImGui::PopFont();
        } else {
            ImGui::SameLine(x0 + avail - vsz.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.5f);
            ImGui::PushFont(Mono());
            ImGui::TextColored(V4(wasActive ? Theme::Accent : Theme::Text), "%s", val);
            ImGui::PopFont();
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
                const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, b.y + 1), ImVec2(b.x, b.y + 1), U32A(Theme::Accent, 0.6f));
                if (ImGui::IsMouseDoubleClicked(0)) {
                    s_EditId = editId;
                    s_EditFocus = true;
                    if (isF) snprintf(s_EditBuf, sizeof(s_EditBuf), "%g", *(float*)v);
                    else     snprintf(s_EditBuf, sizeof(s_EditBuf), "%d", *(int*)v);
                }
            }
        }
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderScalar("##s", type, v, mn, mx, "")) changed = true;
    const bool active  = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActivated()) UISound_Play(Sfx::Click);
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    st->SetBool(key, active);

    float* hv = st->GetFloatRef(ImGui::GetID("hov"), 0.0f);
    *hv = Approach(*hv, active ? 1.0f : hovered ? 0.6f : 0.0f, 16.0f);

    const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
    const float  t  = ImClamp((fv - fmn) / (fmx - fmn), 0.0f, 1.0f);
    const float  ts = Spring(ImGui::GetID("pos"), t, 420.0f, 34.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = (r0.y + r1.y) * 0.5f;
    const float x0 = r0.x + 6.0f, x1 = r1.x - 6.0f;
    const float gx = ImLerp(x0, x1, ImClamp(ts, 0.0f, 1.0f));
    FocusFx(label, ImRect(rowStart, ImVec2(r1.x, r1.y)));
    dl->AddRectFilled(ImVec2(x0, cy - 2.0f), ImVec2(x1, cy + 2.0f), Theme::SurfaceHi, 2.0f);
    dl->AddRectFilled(ImVec2(x0, cy - 2.0f), ImVec2(gx, cy + 2.0f), Theme::Accent, 2.0f);
    if (*hv > 0.01f) dl->AddCircleFilled(ImVec2(gx, cy), 6.0f + 6.0f * *hv, U32A(Theme::Accent, 0.16f * *hv), 24);
    dl->AddCircleFilled(ImVec2(gx, cy), 6.0f + 1.0f * *hv, Theme::Text, 24);
    dl->AddCircleFilled(ImVec2(gx, cy), 2.2f, Theme::Accent, 12);

    if (active) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const ImVec2 bs = Mono()->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, val);
        const ImVec2 b0(gx - bs.x * 0.5f - 7, cy - 34), b1(gx + bs.x * 0.5f + 7, cy - 14);
        fg->AddRectFilled(b0, b1, Theme::Accent, 4.0f);
        fg->AddTriangleFilled(ImVec2(gx - 4, b1.y), ImVec2(gx + 4, b1.y), ImVec2(gx, b1.y + 4), Theme::Accent);
        fg->AddText(Mono(), 12.0f, ImVec2(b0.x + 7, b0.y + (20 - bs.y) * 0.5f), Theme::OnAccent, val);
    }

    if (HasLabel(label))
        ChangedMark(markPtr ? markPtr : v, ImVec2(captionMax.x + 7.0f, (captionMin.y + captionMax.y) * 0.5f),
                    ImRect(ImVec2(rowStart.x - 6.0f, rowStart.y), ImVec2(r1.x + 6.0f, r1.y)), label);

    ImGui::Dummy(ImVec2(0, 1));
    ImGui::PopID();
    return changed;
}

bool Slider(const char* label, float* v, float min, float max, const char* fmt) {
    return SliderCore(label, ImGuiDataType_Float, v, &min, &max, fmt);
}

bool SliderPercent(const char* label, float* v01) {
    float pct = *v01 * 100.0f;
    const float mn = 0.0f, mx = 100.0f;
    if (!SliderCore(label, ImGuiDataType_Float, &pct, &mn, &mx, "%.0f %%", v01)) return false;
    *v01 = pct / 100.0f;
    return true;
}

bool SliderInt(const char* label, int* v, int min, int max, const char* fmt) {
    return SliderCore(label, ImGuiDataType_S32, v, &min, &max, fmt);
}

static void Checker(ImDrawList* dl, ImVec2 a, ImVec2 b, float cell) {
    dl->AddRectFilled(a, b, IM_COL32(58, 58, 64, 255));
    int row = 0;
    for (float y = a.y; y < b.y; y += cell, row++)
        for (float x = a.x + ((row & 1) ? cell : 0.0f); x < b.x; x += cell * 2.0f)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(ImMin(x + cell, b.x), ImMin(y + cell, b.y)), IM_COL32(92, 92, 100, 255));
}

static void StripKnob(ImDrawList* dl, float x, float y0, float y1, ImU32 fill) {
    const ImVec2 a(x - 3.5f, y0 - 3.0f), b(x + 3.5f, y1 + 3.0f);
    dl->AddRectFilled(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), IM_COL32(0, 0, 0, 120), 3.0f);
    dl->AddRectFilled(a, b, IM_COL32(245, 245, 245, 255), 2.5f);
    dl->AddRectFilled(ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), fill, 1.5f);
}

static bool ColorPickerPanel(float col[4], const float orig[4]) {
    const float W = 240.0f;
    bool changed = false;
    ImDrawList*   dl = ImGui::GetWindowDrawList();
    ImGuiStorage* st = ImGui::GetStateStorage();

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);
    float* H = st->GetFloatRef(ImGui::GetID("##hue"), h);
    float* S = st->GetFloatRef(ImGui::GetID("##sat"), s);
    if (v > 0.002f && s > 0.002f) *H = h;
    if (v > 0.002f) *S = s;
    h = *H; s = *S;

    auto commit = [&](float nh, float ns, float nv) {
        *H = nh; *S = ns;
        ImGui::ColorConvertHSVtoRGB(nh, ns, nv, col[0], col[1], col[2]);
        changed = true;
    };

    {
        static const ImU32 kPresets[] = {
            IM_COL32(236, 233, 226, 255), IM_COL32(150, 150, 150, 255), IM_COL32(232, 163,  61, 255),
            IM_COL32(242,  92,  84, 255), IM_COL32(242, 140,  56, 255), IM_COL32(242, 211,  56, 255),
            IM_COL32(123, 201, 111, 255), IM_COL32( 79, 193, 233, 255), IM_COL32( 91, 141, 239, 255),
            IM_COL32(166, 124, 240, 255),
        };
        const int   n = IM_ARRAYSIZE(kPresets);
        const float sz = 18.0f, gap = (W - n * sz) / (n - 1);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        for (int i = 0; i < n; i++) {
            const ImVec2 a(p.x + i * (sz + gap), p.y), b(a.x + sz, a.y + sz);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(i);
            const bool clicked = ImGui::InvisibleButton("##preset", ImVec2(sz, sz));
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            const ImVec4 c = ImGui::ColorConvertU32ToFloat4(kPresets[i]);
            const bool same = fabsf(c.x - col[0]) + fabsf(c.y - col[1]) + fabsf(c.z - col[2]) < 0.01f;
            dl->AddRectFilled(a, b, kPresets[i], 4.0f);
            if (same || hov) dl->AddRect(ImVec2(a.x - 2.5f, a.y - 2.5f), ImVec2(b.x + 2.5f, b.y + 2.5f),
                                         same ? Theme::Text : U32A(Theme::Text, 0.45f), 6.0f, 0, 1.5f);
            if (clicked) {
                col[0] = c.x; col[1] = c.y; col[2] = c.z;
                ImGui::ColorConvertRGBtoHSV(c.x, c.y, c.z, *H, *S, v);
                h = *H; s = *S;
                changed = true;
                UISound_Play(Sfx::Click);
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sz + 12.0f));
    }

    {
        const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + W, a.y + 150.0f);
        ImGui::InvisibleButton("##sv", ImVec2(W, 150.0f));
        if (ImGui::IsItemActive()) {
            const ImVec2 m = ImGui::GetIO().MousePos;
            commit(h, ImSaturate((m.x - a.x) / W), 1.0f - ImSaturate((m.y - a.y) / 150.0f));
            ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);
            s = *S; h = *H;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        float hr, hg, hb;
        ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hr, hg, hb);
        const ImU32 hue = ImGui::ColorConvertFloat4ToU32(ImVec4(hr, hg, hb, 1.0f));
        dl->AddRectFilledMultiColor(a, b, IM_COL32_WHITE, hue, hue, IM_COL32_WHITE);
        dl->AddRectFilledMultiColor(a, b, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK);
        dl->AddRect(a, b, Theme::Border, 2.0f);
        const ImVec2 c(a.x + s * W, a.y + (1.0f - v) * 150.0f);
        const ImU32  cur = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.0f));
        dl->AddCircle(c, 8.0f, IM_COL32(0, 0, 0, 110), 24, 3.5f);
        dl->AddCircleFilled(c, 6.5f, cur, 24);
        dl->AddCircle(c, 7.0f, IM_COL32_WHITE, 24, 2.0f);
        ImGui::Dummy(ImVec2(0, 4));
    }

    {
        const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + W, a.y + 12.0f);
        ImGui::InvisibleButton("##hue", ImVec2(W, 12.0f));
        if (ImGui::IsItemActive()) {
            const float nh = ImClamp((ImGui::GetIO().MousePos.x - a.x) / W, 0.0f, 0.9999f);
            commit(nh, s, v);
            h = nh;
        }
        static const ImU32 kHue[7] = { IM_COL32(255,0,0,255), IM_COL32(255,255,0,255), IM_COL32(0,255,0,255), IM_COL32(0,255,255,255),
                                       IM_COL32(0,0,255,255), IM_COL32(255,0,255,255), IM_COL32(255,0,0,255) };
        for (int i = 0; i < 6; i++) {
            const float x0 = a.x + W * i / 6.0f, x1 = a.x + W * (i + 1) / 6.0f;
            dl->AddRectFilledMultiColor(ImVec2(x0, a.y), ImVec2(x1, b.y), kHue[i], kHue[i + 1], kHue[i + 1], kHue[i]);
        }
        dl->AddRect(a, b, Theme::Border, 2.0f);
        float hr, hg, hb;
        ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hr, hg, hb);
        StripKnob(dl, a.x + h * W, a.y, b.y, ImGui::ColorConvertFloat4ToU32(ImVec4(hr, hg, hb, 1.0f)));
        ImGui::Dummy(ImVec2(0, 6));
    }

    {
        const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + W, a.y + 12.0f);
        ImGui::InvisibleButton("##alpha", ImVec2(W, 12.0f));
        if (ImGui::IsItemActive()) {
            col[3] = ImSaturate((ImGui::GetIO().MousePos.x - a.x) / W);
            changed = true;
        }
        Checker(dl, a, b, 6.0f);
        const ImU32 c0 = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0.0f));
        const ImU32 c1 = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.0f));
        dl->AddRectFilledMultiColor(a, b, c0, c1, c1, c0);
        dl->AddRect(a, b, Theme::Border, 2.0f);
        StripKnob(dl, a.x + col[3] * W, a.y, b.y, ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])));
        ImGui::Dummy(ImVec2(0, 8));
    }

    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float  sh = 28.0f, swW = 64.0f;
        const ImVec2 a(p.x, p.y), m(p.x + swW * 0.5f, p.y + sh), b(p.x + swW, p.y + sh);
        Checker(dl, a, b, 7.0f);
        dl->AddRectFilled(a, m, ImGui::ColorConvertFloat4ToU32(ImVec4(orig[0], orig[1], orig[2], orig[3])), 4.0f, ImDrawFlags_RoundCornersLeft);
        dl->AddRectFilled(ImVec2(m.x, a.y), b, ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])), 4.0f, ImDrawFlags_RoundCornersRight);
        dl->AddRect(a, b, Theme::Border, 4.0f);
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton("##orig", ImVec2(swW * 0.5f, sh))) {
            for (int k = 0; k < 4; k++) col[k] = orig[k];
            changed = true;
            UISound_Play(Sfx::Click);
        }
        if (ImGui::IsItemHovered()) Tooltip(ImGui::GetID("##origtip"), "Restore the previous colour", a, m, TipSide::Below, 0.3f);

        static char s_Hex[16];
        static bool s_HexEditing = false;
        if (!s_HexEditing)
            snprintf(s_Hex, sizeof(s_Hex), "#%02X%02X%02X%02X",
                     (int)(col[0] * 255 + 0.5f), (int)(col[1] * 255 + 0.5f), (int)(col[2] * 255 + 0.5f), (int)(col[3] * 255 + 0.5f));
        ImGui::SetCursorScreenPos(ImVec2(b.x + 10.0f, p.y));
        ImGui::PushFont(Mono());
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, (sh - Mono()->FontSize) * 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, V4(Theme::SecondaryBg));
        ImGui::PushStyleColor(ImGuiCol_Border, V4(Theme::Border));
        ImGui::SetNextItemWidth(W - swW - 10.0f);
        const bool enter = ImGui::InputText("##hex", s_Hex, sizeof(s_Hex),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll);
        s_HexEditing = ImGui::IsItemActive();
        if (s_HexEditing)
            dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Theme::Accent, 4.0f);
        if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
            const char* hx = s_Hex[0] == '#' ? s_Hex + 1 : s_Hex;
            unsigned r = 0, g = 0, bl = 0, al = 255;
            const int n = sscanf_s(hx, "%02x%02x%02x%02x", &r, &g, &bl, &al);
            if (n >= 3) {
                col[0] = r / 255.0f; col[1] = g / 255.0f; col[2] = bl / 255.0f;
                if (n == 4) col[3] = al / 255.0f;
                changed = true;
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();
    }
    return changed;
}

bool ColorRow(const char* label, float col[4]) {
    ImGui::PushID(label);
    const ImVec2 pos   = ImGui::GetCursorScreenPos();
    const float  avail = ImGui::GetContentRegionAvail().x;
    const float  rowH  = 28.0f;
    const ImVec2 tsz   = ImGui::CalcTextSize(label);
    const ImRect row(ImVec2(pos.x - 6.0f, pos.y), ImVec2(pos.x + avail + 6.0f, pos.y + rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    FocusFx(label, row);

    ImGui::SetCursorScreenPos(row.Min);
    const bool open = ImGui::InvisibleButton("##row", row.GetSize());
    const bool hov  = ImGui::IsItemHovered();
    float* hv = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.0f);
    *hv = Approach(*hv, (hov || ImGui::IsPopupOpen("##picker")) ? 1.0f : 0.0f, 16.0f);
    if (*hv > 0.01f) dl->AddRectFilled(row.Min, row.Max, U32A(Theme::SurfaceHi, 0.45f * *hv), Theme::RadiusFrame);
    dl->AddText(ImVec2(pos.x, pos.y + (rowH - tsz.y) * 0.5f), Mix(Theme::TextDim, Theme::Text, 0.6f + 0.4f * *hv), label);

    const ImVec2 s0(pos.x + avail - 38.0f, pos.y + 5.0f), s1(pos.x + avail, pos.y + rowH - 5.0f);
    Checker(dl, s0, s1, 4.5f);
    const ImU32 cOpaque = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.0f));
    const ImU32 cAlpha  = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
    dl->AddRectFilled(s0, ImVec2((s0.x + s1.x) * 0.5f, s1.y), cOpaque, 4.0f, ImDrawFlags_RoundCornersLeft);
    dl->AddRectFilled(ImVec2((s0.x + s1.x) * 0.5f, s0.y), s1, cAlpha, 4.0f, ImDrawFlags_RoundCornersRight);
    dl->AddRect(s0, s1, Mix(Theme::Border, Theme::Accent, *hv), 4.0f);

    ChangedMark(col, ImVec2(pos.x + tsz.x + 7.0f, pos.y + rowH * 0.5f), row, label);

    float* orig = nullptr;
    {
        ImGuiStorage* st = ImGui::GetStateStorage();
        static float s_Orig[4];
        orig = s_Orig;
        if (open) {
            for (int k = 0; k < 4; k++) s_Orig[k] = col[k];
            st->SetFloat(ImGui::GetID("##openT"), 0.0f);
            ImGui::OpenPopup("##picker");
            UISound_Play(Sfx::Open);
        }
    }

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(s1.x + 6.0f, pos.y - 6.0f), ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginPopup("##picker")) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        float* t = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##openT"), 0.0f);
        *t = Approach(*t, 1.0f, 16.0f);
        const ImVec2 p = win->Pos, q(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y);
        ImDrawList* pdl = win->DrawList;
        pdl->ChannelsSplit(2);
        pdl->ChannelsSetCurrent(1);
        changed = ColorPickerPanel(col, orig);
        pdl->ChannelsSetCurrent(0);
        UIGfx_DrawShadow(ImGui::GetBackgroundDrawList(), p, q, 8.0f, *t * 0.7f);
        UIGfx_DrawGlass(pdl, p, q, 8.0f, 1.0f);
        pdl->AddRectFilled(p, q, Glass(Theme::Surface, 0.86f), 8.0f);
        pdl->AddRect(p, q, Theme::Border, 8.0f);
        pdl->AddLine(ImVec2(p.x + 8, p.y + 0.5f), ImVec2(q.x - 8, p.y + 0.5f), IM_COL32(255, 255, 255, 22));
        pdl->AddRectFilled(ImVec2(p.x + 14, p.y), ImVec2(p.x + 46, p.y + 2), cOpaque);
        pdl->ChannelsMerge();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        UI_FadeWindowTree(win, EaseOut(*t));
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(avail, rowH));
    ImGui::PopID();
    return changed;
}

struct DropState {
    ImGuiWindow* win   = nullptr;
    float        t     = 1.0f;
    int          vtx0  = 0;
    float        hoverY = -1.0f, selY = -1.0f, rowH = 30.0f;
};

static DropState DropBegin() {
    DropState d;
    d.win = ImGui::GetCurrentWindow();
    float* t = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##dd_t"), 0.0f);
    if (ImGui::IsWindowAppearing()) *t = 0.0f;
    *t = Approach(*t, 1.0f, 18.0f);
    d.t = *t;
    d.vtx0 = d.win->DrawList->VtxBuffer.Size;
    d.win->DrawList->ChannelsSplit(3);
    d.win->DrawList->ChannelsSetCurrent(2);
    return d;
}

static bool DropRow(DropState& d, const char* id, const char* icon, const char* text, bool selected, ImU32 tone = 0) {
    const float  w = ImGui::GetContentRegionAvail().x, h = d.rowH;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    const bool clicked = ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hov = ImGui::IsItemHovered();
    ImGui::PopID();
    if (hov) d.hoverY = p.y;
    if (selected) d.selY = p.y;

    ImDrawList* dl = d.win->DrawList;
    const float cy = p.y + h * 0.5f, fh = ImGui::GetFontSize();
    float x = p.x + 12.0f;
    if (selected) dl->AddRectFilled(ImVec2(p.x + 3.0f, p.y + 8.0f), ImVec2(p.x + 5.0f, p.y + h - 8.0f), Theme::Accent, 1.0f);
    if (icon) {
        dl->AddText(ImVec2(x, cy - fh * 0.5f - 1.0f), tone ? tone : (selected ? Theme::Accent : (hov ? Theme::TextDim : Theme::TextFaint)), icon);
        x += ImGui::CalcTextSize(icon).x + 9.0f;
    }
    const ImU32 col = tone ? tone : (selected || hov ? Theme::Text : Theme::TextDim);
    dl->PushClipRect(p, ImVec2(p.x + w - (selected ? 30.0f : 8.0f), p.y + h), true);
    dl->AddText(ImVec2(x, cy - fh * 0.5f), col, text);
    dl->PopClipRect();
    if (selected) {
        const ImVec2 cs = ImGui::CalcTextSize(ICON_LC_CHECK);
        dl->AddText(ImVec2(p.x + w - 12.0f - cs.x, cy - fh * 0.5f - 1.0f), Theme::Accent, ICON_LC_CHECK);
    }
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

static void DropSeparator(DropState& d) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;
    d.win->DrawList->AddLine(ImVec2(p.x + 6.0f, p.y + 4.0f), ImVec2(p.x + w - 6.0f, p.y + 4.0f), Theme::Separator);
    ImGui::Dummy(ImVec2(w, 8.0f));
}

static void DropCaption(DropState& d, const char* text) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  sz = 11.0f;
    TextTracked(d.win->DrawList, g_FontSection, sz, ImVec2(p.x + 10.0f, p.y + 4.0f), Theme::TextFaint, text, 1.6f);
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, sz + 10.0f));
}

static void DropEnd(DropState& d) {
    ImGuiWindow* w  = d.win;
    ImDrawList*  dl = w->DrawList;
    const ImVec2 p = w->Pos, q(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y);

    dl->ChannelsSetCurrent(1);
    {
        ImGuiStorage* st = ImGui::GetStateStorage();
        float* py = st->GetFloatRef(ImGui::GetID("##dd_py"), -1.0f);
        float* pa = st->GetFloatRef(ImGui::GetID("##dd_pa"), 0.0f);
        float* ph = st->GetFloatRef(ImGui::GetID("##dd_ph"), 0.0f);
        const float target = d.hoverY >= 0.0f ? d.hoverY : d.selY;
        if (target >= 0.0f) {
            const float rel = target - p.y;
            *py = (*py < 0.0f || d.t < 0.05f) ? rel : Approach(*py, rel, 30.0f);
        }
        *pa = Approach(*pa, target >= 0.0f ? 1.0f : 0.0f, 20.0f);
        *ph = Approach(*ph, d.hoverY >= 0.0f ? 1.0f : 0.0f, 20.0f);
        if (*pa > 0.01f && *py >= 0.0f) {
            const float pad = w->WindowPadding.x;
            const ImVec2 a(p.x + pad, p.y + *py), b(q.x - pad, p.y + *py + d.rowH);
            const ImU32 hoverCol = Mix(Theme::SurfaceHi, Theme::Border, 0.6f);
            dl->AddRectFilled(a, b, U32A(Mix(U32A(Theme::Accent, 1.0f), hoverCol, *ph), (0.14f + 0.81f * *ph) * *pa), 6.0f);
        }
    }

    dl->ChannelsSetCurrent(0);
    const float e = EaseOut(d.t);
    UIGfx_DrawShadow(ImGui::GetBackgroundDrawList(), p, q, 9.0f, 0.55f * e);
    UIGfx_DrawGlass(dl, p, q, 9.0f, 1.0f);
    dl->AddRectFilled(p, q, Glass(Theme::Surface, 0.90f), 9.0f);
    dl->AddRect(p, q, Theme::Border, 9.0f);
    dl->AddLine(ImVec2(p.x + 9, p.y + 0.5f), ImVec2(q.x - 9, p.y + 0.5f), IM_COL32(255, 255, 255, 22));
    dl->ChannelsMerge();

    const float off = (1.0f - e) * -6.0f;
    if (off < -0.05f)
        for (int i = d.vtx0; i < dl->VtxBuffer.Size; i++) dl->VtxBuffer[i].pos.y += off;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
}

static void DropPushStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 9.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
}
static void DropPopStyle() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

bool Combo(const char* label, int* v, const char* const items[], int count) {
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 capMin = start, capMax = start;
    if (HasLabel(label)) {
        ImGui::TextColored(V4(Theme::TextDim), "%s", label);
        capMin = ImGui::GetItemRectMin(); capMax = ImGui::GetItemRectMax();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    }

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x, h = 34.0f;
    const ImVec2 q(p.x + w, p.y + h);
    if (ImGui::InvisibleButton("##trigger", ImVec2(w, h))) {
        ImGui::OpenPopup("##dd");
        UISound_Play(Sfx::Click);
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool open    = ImGui::IsPopupOpen("##dd");
    ImGuiStorage* st = ImGui::GetStateStorage();
    float* hv = st->GetFloatRef(ImGui::GetID("##hv"), 0.0f);
    float* op = st->GetFloatRef(ImGui::GetID("##op"), 0.0f);
    *hv = Approach(*hv, hovered ? 1.0f : 0.0f, 16.0f);
    *op = Approach(*op, open ? 1.0f : 0.0f, 18.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = Theme::RadiusFrame + 2.0f;
    if (*op > 0.01f)
        dl->AddRect(ImVec2(p.x - 2, p.y - 2), ImVec2(q.x + 2, q.y + 2), U32A(Theme::Accent, 0.16f * *op), r + 2.0f, 0, 3.0f);
    dl->AddRectFilled(p, q, Mix(Glass(Theme::SecondaryBg, 0.75f), Theme::SurfaceHi, *hv * 0.45f), r);
    dl->AddRect(p, q, Mix(Mix(Theme::Border, Theme::TextFaint, *hv * 0.6f), Theme::Accent, *op), r);
    const char*  preview = (*v >= 0 && *v < count) ? items[*v] : "";
    const ImVec2 ts = ImGui::CalcTextSize(preview);
    dl->PushClipRect(p, ImVec2(q.x - 34.0f, q.y), true);
    dl->AddText(ImVec2(p.x + 12.0f, p.y + (h - ts.y) * 0.5f), Theme::Text, preview);
    dl->PopClipRect();
    {
        const ImVec2 c(q.x - 17.0f, p.y + h * 0.5f);
        const float  ang = *op * 3.14159265f, cs = cosf(ang), sn = sinf(ang);
        auto R = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };
        const ImVec2 pts[3] = { R(-4.5f, -2.0f), R(0.0f, 2.5f), R(4.5f, -2.0f) };
        dl->AddPolyline(pts, 3, Mix(Mix(Theme::TextFaint, Theme::Text, *hv), Theme::Accent, *op), 0, 1.6f);
    }

    bool changed = false;
    const float rowH = 30.0f, pad = 6.0f;
    const int   shown = ImMin(count, 8);
    const float listH = shown * (rowH + 2.0f) + pad * 2.0f;
    const bool  above = q.y + 6.0f + listH > ImGui::GetIO().DisplaySize.y - 8.0f;
    ImGui::SetNextWindowPos(above ? ImVec2(p.x, p.y - 6.0f) : ImVec2(p.x, q.y + 6.0f), ImGuiCond_Always, ImVec2(0.0f, above ? 1.0f : 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(w, listH));
    DropPushStyle();
    if (ImGui::BeginPopup("##dd", ImGuiWindowFlags_NoMove)) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        DropState d = DropBegin();
        d.rowH = rowH;
        for (int i = 0; i < count; i++) {
            ImGui::PushID(i);
            if (DropRow(d, "##item", nullptr, items[i], i == *v) && *v != i) { *v = i; changed = true; }
            ImGui::PopID();
            if (changed) { ImGui::CloseCurrentPopup(); break; }
        }
        DropEnd(d);
        ImGui::EndPopup();
        UI_FadeWindowTree(win, EaseOut(d.t));
    }
    DropPopStyle();
    if (changed) UISound_Play(Sfx::ToggleOn);

    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, h));
    FocusFx(label, ImRect(start, q));
    if (HasLabel(label))
        ChangedMark(v, ImVec2(capMax.x + 7.0f, (capMin.y + capMax.y) * 0.5f), ImRect(start, q), label);
    ImGui::PopID();
    return changed;
}

bool Segmented(const char* label, int* v, const char* const items[], int count) {
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 capMin = start, capMax = start;
    if (HasLabel(label)) {
        ImGui::TextColored(V4(Theme::TextDim), "%s", label);
        capMin = ImGui::GetItemRectMin(); capMax = ImGui::GetItemRectMax();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    }
    ImDrawList*  dl   = ImGui::GetWindowDrawList();
    const ImVec2 p    = ImGui::GetCursorScreenPos();
    const float  w    = ImGui::GetContentRegionAvail().x;
    const float  h    = 30.0f;
    const float  segW = (w - 6.0f) / count;
    FocusFx(label, ImRect(start, ImVec2(p.x + w, p.y + h)));
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), Glass(Theme::SecondaryBg, 0.7f), 5.0f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), Theme::Border, 5.0f);

    ImGuiStorage* st      = ImGui::GetStateStorage();
    const bool    valid   = *v >= 0 && *v < count;
    const ImGuiID lastId  = ImGui::GetID("last");
    if (valid) st->SetInt(lastId, *v);
    const int     shown   = valid ? *v : ImClamp(st->GetInt(lastId, 0), 0, count - 1);
    const float   ix      = Spring(ImGui::GetID("ix"), (float)shown, 300.0f, 24.0f);
    const float   ia      = Spring(ImGui::GetID("ia"), valid ? 1.0f : 0.0f, 300.0f, 24.0f);
    const float   ial     = ImClamp(ia, 0.0f, 1.0f);
    const ImVec2 iMin(p.x + 3.0f + segW * ix, p.y + 3.0f);
    const ImVec2 iMax(iMin.x + segW, p.y + h - 3.0f);
    if (ial > 0.01f) {
        dl->AddRectFilled(iMin, iMax, U32A(Theme::Accent, 0.16f * ial), 3.0f);
        dl->AddRect(iMin, iMax, U32A(Theme::Accent, 0.55f * ial), 3.0f);
    }

    bool changed = false;
    for (int i = 0; i < count; i++) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + 3.0f + segW * i, p.y + 3.0f));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(segW, h - 6.0f)) && *v != i) {
            *v = i; changed = true;
            UISound_Play(Sfx::Click);
        }
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec2 tsz = ImGui::CalcTextSize(items[i]);
        const float  sel = (1.0f - ImClamp(fabsf(ix - (float)i), 0.0f, 1.0f)) * ial;
        const ImU32  col = Mix(hov ? Theme::TextDim : Theme::TextFaint, Theme::Text, sel);
        dl->AddText(ImVec2(p.x + 3.0f + segW * i + (segW - tsz.x) * 0.5f, p.y + (h - tsz.y) * 0.5f), col, items[i]);
    }
    if (HasLabel(label))
        ChangedMark(v, ImVec2(capMax.x + 7.0f, (capMin.y + capMax.y) * 0.5f), ImRect(start, ImVec2(p.x + w, p.y + h)), label);
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopID();
    return changed;
}

bool Notice(const char* id, const char* icon, ImU32 tone, const char* title, const char* body,
            const char* const chips[], int chipCount, bool* dontShowAgain) {
    static float s_Anim = 0.0f;
    if (!ImGui::IsPopupOpen(id)) { s_Anim = 0.0f; return false; }
    s_Anim = Approach(s_Anim, 1.0f, 14.0f);
    const float e = EaseOut(s_Anim);

    const float  W  = 400.0f;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f + (1.0f - e) * 14.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.45f * e));

    bool dismissed = false;
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        ImDrawList*  dl  = win->DrawList;
        const ImVec2 p = win->Pos, q(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y);

        UIGfx_DrawGlass(dl, p, q, 10.0f, 1.0f);
        dl->AddRectFilled(p, q, Glass(Theme::Surface, 0.82f), 10.0f);
        dl->AddRect(p, q, Theme::Border, 10.0f);
        dl->AddLine(ImVec2(p.x + 10, p.y + 0.5f), ImVec2(q.x - 10, p.y + 0.5f), IM_COL32(255, 255, 255, 22));
        dl->AddRectFilled(ImVec2(p.x + 24, p.y), ImVec2(p.x + 64, p.y + 2), tone);

        const ImVec2 c = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(c, ImVec2(c.x + 40, c.y + 40), U32A(tone, 0.14f), 8.0f);
        dl->AddRect(c, ImVec2(c.x + 40, c.y + 40), U32A(tone, 0.5f), 8.0f);
        const ImVec2 is = ImGui::CalcTextSize(icon);
        dl->AddText(ImVec2(c.x + 20 - is.x * 0.5f, c.y + 20 - is.y * 0.5f - 2), tone, icon);
        ImFont* tf = g_FontBrand ? g_FontBrand : ImGui::GetFont();
        dl->AddText(tf, 17.0f, ImVec2(c.x + 54, c.y + 2), Theme::Text, title);
        dl->AddText(Mono(), 11.0f, ImVec2(c.x + 55, c.y + 24), tone, "BEFORE YOU JOIN A MATCH");
        ImGui::Dummy(ImVec2(W, 48));

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + W);
        ImGui::TextColored(V4(Theme::TextDim), "%s", body);
        ImGui::PopTextWrapPos();

        if (chipCount > 0) {
            ImGui::Dummy(ImVec2(0, 4));
            const ImVec2 start = ImGui::GetCursorScreenPos();
            float x = start.x, y = start.y;
            for (int i = 0; i < chipCount; i++) {
                const ImVec2 ts = Mono()->CalcTextSizeA(11.5f, FLT_MAX, 0.0f, chips[i]);
                const float  cw = ts.x + 16.0f;
                if (x + cw > start.x + W) { x = start.x; y += 28.0f; }
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cw, y + 22), Theme::SurfaceHi, 4.0f);
                dl->AddRect(ImVec2(x, y), ImVec2(x + cw, y + 22), Theme::Border, 4.0f);
                dl->AddText(Mono(), 11.5f, ImVec2(x + 8, y + (22 - ts.y) * 0.5f), Theme::Text, chips[i]);
                x += cw + 6.0f;
            }
            ImGui::Dummy(ImVec2(W, y - start.y + 22.0f));
        }

        ImGui::Dummy(ImVec2(0, 6));
        {
            const ImVec2 lp = ImGui::GetCursorScreenPos();
            dl->AddLine(lp, ImVec2(lp.x + W, lp.y), Theme::Separator);
            ImGui::Dummy(ImVec2(0, 6));
        }
        if (dontShowAgain) UI::Toggle("Don't show this again", dontShowAgain);

        ImGui::PushStyleColor(ImGuiCol_Button,        V4(Theme::Accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V4(Theme::AccentHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  V4(Theme::AccentDeep));
        ImGui::PushStyleColor(ImGuiCol_Text,          V4(Theme::OnAccent));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        const bool ok = ImGui::Button("Got it", ImVec2(W, 34));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        if (ok || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            UISound_Play(Sfx::Click);
            dismissed = true;
        }
        ImGui::EndPopup();
        UI_FadeWindowTree(win, e);
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    return dismissed;
}

bool BeginColumns(const char* id, int count) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, 0));
    const bool open = ImGui::BeginTable(id, count, ImGuiTableFlags_SizingStretchSame, ImVec2(-23.0f, 0.0f));
    ImGui::PopStyleVar();
    if (open) ImGui::TableNextColumn();
    return open;
}

void NextColumn() { ImGui::TableNextColumn(); }

void EndColumns() { ImGui::EndTable(); }

}

using UI::Approach;
using UI::EaseOut;
using UI::Spring;

static const float kSideOpen = 216.0f, kSideClosed = 68.0f;

static DWORD WINAPI OpenUrlThread(LPVOID url) {
    ShellExecuteA(nullptr, "open", (const char*)url, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

static void OpenUrlAsync(const char* url) {
    HANDLE t = CreateThread(nullptr, 0, OpenUrlThread, (LPVOID)url, 0, nullptr);
    if (t) CloseHandle(t);
}

static void DrawSidebar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    ImGui::BeginChild("##sidebar", ImVec2(width, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float  la = ImClamp((width - kSideClosed - 40.0f) / (kSideOpen - kSideClosed - 40.0f), 0.0f, 1.0f);

    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), Glass(Theme::PrimaryBg, 0.45f), Theme::RadiusWindow,
                      ImDrawFlags_RoundCornersLeft);

    {
        const float lx = ImLerp(wp.x + ws.x * 0.5f, wp.x + 36.0f, la);
        UI::DrawLogo(dl, ImVec2(lx, wp.y + 40), 30.0f, Theme::Accent);
        if (la > 0.01f) {
            const float bs = g_FontBrand ? g_FontBrand->FontSize : 19.0f;
            UI::TextTracked(dl, g_FontBrand, bs, ImVec2(wp.x + 60 - (1 - la) * 8, wp.y + 23), U32A(Theme::Text, la), s_BrandTitle, 2.2f);
            UI::TextTracked(dl, Mono(), 10.5f, ImVec2(wp.x + 61 - (1 - la) * 8, wp.y + 45), U32A(Theme::Accent, la), s_BrandSub, 3.2f);
        }
    }
    dl->AddLine(ImVec2(wp.x + 16, wp.y + 76), ImVec2(wp.x + ws.x - 16, wp.y + 76), Theme::Separator);

    const float  itemH    = 34.0f;
    const float  padX     = 10.0f;

    const float footerH     = 50.0f;
    const bool  showLink = s_LinkLabel && s_LinkUrl && ws.y >= 470.0f;
    const float navTop      = 82.0f;
    const float navBottom   = ws.y - footerH - (showLink ? 10.0f + 34.0f + 8.0f : 6.0f);
    const float navH        = ImMax(navBottom - navTop, itemH);

    ImGui::SetCursorPos(ImVec2(0.0f, navTop));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##navlist", ImVec2(width, navH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImDrawList*  ndl     = ImGui::GetWindowDrawList();
    const ImVec2 np      = ImGui::GetWindowPos();
    const float  scrollY = ImGui::GetScrollY();
    float        targetY = -1.0f;
    const char*  lastGroup = nullptr;

    ImGui::SetCursorPosY(6.0f);
    ndl->ChannelsSplit(2);
    ndl->ChannelsSetCurrent(1);

    for (int ti = 0; ti < s_NavCount; ti++) {
        const UI_NavItem& e = s_Nav[ti];
        if (!lastGroup || strcmp(lastGroup, e.group) != 0) {
            if (lastGroup) ImGui::Dummy(ImVec2(0, 10));
            const ImVec2 gp(np.x + 24, ImGui::GetCursorScreenPos().y);
            const float  sz = g_FontSection ? g_FontSection->FontSize : 12.0f;
            if (la > 0.01f) UI::TextTracked(dl, g_FontSection, sz, gp, U32A(Theme::TextFaint, la), e.group, 1.6f);
            if (la < 0.99f) {
                const float cx = np.x + width * 0.5f, cy = gp.y + sz * 0.5f;
                ndl->AddLine(ImVec2(cx - 10, cy), ImVec2(cx + 10, cy), U32A(Theme::Border, 1.0f - la));
            }
            ImGui::Dummy(ImVec2(0, sz + 2.0f));
            lastGroup = e.group;
        }

        ImGui::SetCursorPosX(padX);
        ImGui::PushID(e.label);
        const bool selected = (g_ActiveTab == ti);
        if (ImGui::InvisibleButton("##nav", ImVec2(width - padX * 2, itemH)) && !selected) {
            s_TabDir = ti > g_ActiveTab ? 1.0f : -1.0f;
            g_ActiveTab = ti;
            UISound_Play(Sfx::Click);
        }
        const bool  hovered = ImGui::IsItemHovered();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        if (hovered && la < 0.5f) UI::Tooltip(ImGui::GetItemID(), e.label, mn, mx, UI::TipSide::Right, 0.05f);

        float* hov = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hov"), 0.0f);
        float* sel = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("sel"), selected ? 1.0f : 0.0f);
        *hov = Approach(*hov, (hovered && !selected) ? 1.0f : 0.0f, 18.0f);
        *sel = Approach(*sel, selected ? 1.0f : 0.0f, 14.0f);
        if (*hov > 0.01f)
            ndl->AddRectFilled(mn, mx, U32A(Theme::SurfaceHi, 0.6f * *hov), Theme::RadiusFrame);

        if (selected) targetY = mn.y - np.y + scrollY;

        const float  shift = (3.0f * *hov + 2.0f * *sel) * la;
        const float  cy    = (mn.y + mx.y) * 0.5f;
        const float  fh    = ImGui::GetFontSize();
        const char*  icon  = e.icon;
        const ImVec2 isz   = ImGui::CalcTextSize(icon);
        const float  ix    = ImLerp((mn.x + mx.x) * 0.5f - isz.x * 0.5f, mn.x + 14 + shift, la);
        ndl->AddText(ImVec2(ix, cy - fh * 0.5f - 1.0f), Mix(Theme::TextFaint, Theme::Accent, *sel), icon);
        if (la > 0.01f)
            ndl->AddText(ImVec2(mn.x + 42 + shift, cy - fh * 0.5f), U32A(Mix(Theme::TextDim, Theme::Text, ImMax(*sel, *hov)), la), e.label);
        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 6.0f));

    const float pillY = Spring(ImGui::GetID("##pill"), targetY, 340.0f, 26.0f);
    ndl->ChannelsSetCurrent(0);
    {
        const ImVec2 pMin(np.x + padX, np.y + pillY - scrollY);
        const ImVec2 pMax(np.x + width - padX, pMin.y + itemH);
        const ImU32  a0 = U32A(Theme::Accent, 0.18f), a1 = U32A(Theme::Accent, la > 0.5f ? 0.0f : 0.12f);
        ndl->AddRectFilledMultiColor(pMin, pMax, a0, a1, a1, a0);
        ndl->AddRectFilled(ImVec2(pMin.x, pMin.y + 7), ImVec2(pMin.x + 2, pMax.y - 7), Theme::Accent);
    }
    ndl->ChannelsMerge();

    {
        const float maxY = ImGui::GetScrollMaxY();
        const ImU32 bg0 = Glass(Theme::PrimaryBg, 0.0f), bg1 = Glass(Theme::PrimaryBg, 0.95f);
        if (scrollY > 1.0f)
            ndl->AddRectFilledMultiColor(np, ImVec2(np.x + width, np.y + 18.0f), bg1, bg1, bg0, bg0);
        if (scrollY < maxY - 1.0f)
            ndl->AddRectFilledMultiColor(ImVec2(np.x, np.y + navH - 18.0f), ImVec2(np.x + width, np.y + navH), bg0, bg0, bg1, bg1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (showLink) {
        const float  bh  = 34.0f;
        const ImVec2 b0(wp.x + padX, wp.y + ws.y - 50.0f - 10.0f - bh);
        const ImVec2 b1(wp.x + ws.x - padX, b0.y + bh);
        ImGui::SetCursorScreenPos(b0);
        const bool clicked = ImGui::InvisibleButton("##link", ImVec2(b1.x - b0.x, bh));
        const bool hov     = ImGui::IsItemHovered();
        float* h = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##linkhov"), 0.0f);
        *h = Approach(*h, hov ? 1.0f : 0.0f, 16.0f);

        const ImU32  tone = Theme::Accent;
        const char*  icon = s_LinkIcon ? s_LinkIcon : ICON_LC_EXTERNAL_LINK;
        dl->AddRectFilled(b0, b1, U32A(tone, 0.10f + 0.14f * *h), Theme::RadiusFrame);
        dl->AddRect(b0, b1, U32A(tone, 0.30f + 0.45f * *h), Theme::RadiusFrame);
        const float  fh  = ImGui::GetFontSize();
        const float  cy  = (b0.y + b1.y) * 0.5f;
        const ImVec2 isz = ImGui::CalcTextSize(icon);
        const float  ix  = ImLerp((b0.x + b1.x) * 0.5f - isz.x * 0.5f, b0.x + 14.0f, la);
        dl->AddText(ImVec2(ix, cy - fh * 0.5f - 1.0f), Mix(tone, Theme::Text, *h), icon);
        if (la > 0.01f) {
            dl->AddText(ImVec2(b0.x + 42, cy - fh * 0.5f), U32A(Mix(Theme::TextDim, Theme::Text, *h), la), s_LinkLabel);
            const ImVec2 es = ImGui::CalcTextSize(ICON_LC_EXTERNAL_LINK);
            dl->AddText(ImVec2(b1.x - 12 - es.x + 2.0f * *h, cy - fh * 0.5f - 1.0f), U32A(Theme::TextFaint, la * (0.5f + 0.5f * *h)), ICON_LC_EXTERNAL_LINK);
        }
        if (hov) UI::Tooltip(ImGui::GetID("##linktip"), s_LinkUrl, b0, b1, UI::TipSide::Right, la < 0.5f ? 0.05f : 0.5f);
        if (clicked) {
            OpenUrlAsync(s_LinkUrl);
            UISound_Play(Sfx::ToggleOn);
            UI_Toast(icon, s_LinkLabel, "opening in browser", tone);
        }
    }

    {
        const float fy = wp.y + ws.y - 50.0f;
        dl->AddLine(ImVec2(wp.x + 16, fy), ImVec2(wp.x + ws.x - 16, fy), Theme::Separator);
        if (la > 0.01f) {
            const float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 2.6f);
            const ImVec2 dot(wp.x + 26, fy + 25);
            dl->AddCircleFilled(dot, 4.0f + 4.0f * pulse, U32A(Theme::Success, 0.22f * (1.0f - pulse) * la), 20);
            dl->AddCircleFilled(dot, 3.0f, U32A(Theme::Success, la), 12);
            UI::TextTracked(dl, Mono(), 11.0f, ImVec2(wp.x + 38, fy + 18), U32A(Theme::Success, la), s_Status, 1.4f);
        }
        const float bx = ImLerp(wp.x + ws.x * 0.5f - 13.0f, wp.x + ws.x - 40.0f, la);
        ImGui::SetCursorScreenPos(ImVec2(bx, fy + 12));
        const bool clicked = ImGui::InvisibleButton("##collapse", ImVec2(26, 26));
        const bool bh = ImGui::IsItemHovered();
        if (bh) UI::Tooltip(ImGui::GetItemID(), g_UI.sidebarCollapsed ? "Expand sidebar" : "Collapse sidebar",
                            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), UI::TipSide::Right, 0.25f);
        dl->AddRectFilled(ImVec2(bx, fy + 12), ImVec2(bx + 26, fy + 38), bh ? Theme::SurfaceHi : U32A(Theme::SurfaceHi, 0.0f), 4.0f);
        const char* ic = g_UI.sidebarCollapsed ? ICON_LC_CHEVRONS_RIGHT : ICON_LC_CHEVRONS_LEFT;
        const ImVec2 is = ImGui::CalcTextSize(ic);
        dl->AddText(ImVec2(bx + 13 - is.x * 0.5f, fy + 25 - is.y * 0.5f - 2), bh ? Theme::Text : Theme::TextFaint, ic);
        if (clicked) UI_ToggleSidebar();
    }

    dl->AddLine(ImVec2(wp.x + ws.x - 1, wp.y), ImVec2(wp.x + ws.x - 1, wp.y + ws.y), Theme::Separator);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

static void DrawKbd(ImDrawList* dl, ImVec2 p, const char* key, float alpha = 1.0f) {
    const ImVec2 ksz = Mono()->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, key);
    const ImVec2 kMax(p.x + ksz.x + 12, p.y + ksz.y + 6);
    dl->AddRectFilled(p, kMax, U32A(Theme::SurfaceHi, alpha), 3.0f);
    dl->AddRect(p, kMax, U32A(Theme::Border, alpha), 3.0f);
    dl->AddLine(ImVec2(p.x + 2, kMax.y), ImVec2(kMax.x - 2, kMax.y), U32A(Theme::Border, alpha), 1.5f);
    dl->AddText(Mono(), 11.0f, ImVec2(p.x + 6, p.y + 3), U32A(Theme::TextDim, alpha), key);
}

static bool HeaderButton(const char* id, ImVec2 pos, ImVec2 size, float* hov) {
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size);
    *hov = Approach(*hov, ImGui::IsItemHovered() ? 1.0f : 0.0f, 16.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, q, Mix(Glass(Theme::Surface, 0.6f), Theme::SurfaceHi, *hov), 6.0f);
    dl->AddRect(pos, q, Mix(Theme::Border, Theme::Accent, *hov * 0.6f), 6.0f);
    dl->AddLine(ImVec2(pos.x + 6, pos.y + 0.5f), ImVec2(q.x - 6, pos.y + 0.5f), IM_COL32(255, 255, 255, 14));
    return clicked;
}

static DWORD WINAPI ExplorerSelectThread(LPVOID p) {
    char* path = (char*)p;
    char args[MAX_PATH + 16];
    snprintf(args, sizeof(args), "/select,\"%s\"", path);
    ShellExecuteA(nullptr, "open", "explorer.exe", args, nullptr, SW_SHOWNORMAL);
    free(path);
    return 0;
}

static void ShowInExplorer(const char* path) {
    char* copy = _strdup(path);
    if (!copy) return;
    HANDLE t = CreateThread(nullptr, 0, ExplorerSelectThread, copy, 0, nullptr);
    if (t) CloseHandle(t); else free(copy);
}

static void ImportAndReport(const char* text, const char* nameHint) {
    const ImportResult r = Profiles_Import(text, nameHint, g_UI.shareLook);
    if (r.ok) {
        char detail[64];
        snprintf(detail, sizeof(detail), "imported, %d settings", r.applied);
        UISound_Play(Sfx::ToggleOn);
        UI_Toast(ICON_LC_IMPORT, r.name, detail, Theme::Success);
    } else {
        UISound_Play(Sfx::ToggleOff);
        UI_Toast(ICON_LC_CIRCLE_ALERT, "Import failed", r.error, Theme::Danger);
    }
}

static void ShareCopyCode() {
    const std::string code = Profiles_ShareCode(g_UI.shareLook);
    ImGui::SetClipboardText(code.c_str());
    char detail[64];
    snprintf(detail, sizeof(detail), "share code copied (%d chars)", (int)code.size());
    UISound_Play(Sfx::ToggleOn);
    UI_Toast(ICON_LC_CLIPBOARD_COPY, Profiles_Active(), detail, Theme::Accent);
}

static void ShareImportClipboard() {
    const char* clip = ImGui::GetClipboardText();
    if (!clip || !*clip) {
        UISound_Play(Sfx::ToggleOff);
        UI_Toast(ICON_LC_CIRCLE_ALERT, "Import failed", "clipboard is empty", Theme::Danger);
        return;
    }
    ImportAndReport(clip, "Shared");
}

static void ShareExportFile() {
    char path[MAX_PATH];
    if (!Profiles_ExportFile(g_UI.shareLook, path, sizeof(path))) {
        UI_Toast(ICON_LC_CIRCLE_ALERT, "Export failed", "could not write the file", Theme::Danger);
        return;
    }
    UISound_Play(Sfx::ToggleOn);
    UI_Toast(ICON_LC_FILE_OUTPUT, Profiles_Active(), "exported, opening folder", Theme::Accent);
    ShowInExplorer(path);
}

static volatile LONG s_PickState = 0;
static char          s_PickPath[MAX_PATH];

static DWORD WINAPI PickFileThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    s_PickPath[0] = 0;
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Configs (*.cfg; *.txt)\0*.cfg;*.txt\0All files\0*.*\0";
    ofn.lpstrFile   = s_PickPath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = "Import config";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    const BOOL ok = GetOpenFileNameA(&ofn);
    CoUninitialize();
    InterlockedExchange(&s_PickState, ok ? 2 : 3);
    return 0;
}

static void ShareImportFile() {
    if (InterlockedCompareExchange(&s_PickState, 1, 0) != 0) return;
    HANDLE t = CreateThread(nullptr, 0, PickFileThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t); else InterlockedExchange(&s_PickState, 0);
}

static void SharePoll() {
    const LONG st = s_PickState;
    if (st < 2) return;
    InterlockedExchange(&s_PickState, 0);
    if (st != 2) return;
    FILE* f = nullptr;
    if (fopen_s(&f, s_PickPath, "rb") != 0 || !f) { UI_Toast(ICON_LC_CIRCLE_ALERT, "Import failed", "could not open the file", Theme::Danger); return; }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < 256 * 1024) text.append(buf, n);
    fclose(f);
    const char* base = strrchr(s_PickPath, '\\');
    char hint[32];
    snprintf(hint, sizeof(hint), "%s", base ? base + 1 : s_PickPath);
    if (char* dot = strrchr(hint, '.')) *dot = 0;
    ImportAndReport(text.c_str(), hint);
}

static void DrawProfilePopup() {
    UI::DropPushStyle();
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f), ImVec2(260.0f, FLT_MAX));
    if (ImGui::BeginPopup("##profiles", ImGuiWindowFlags_NoMove)) {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        UI::DropState d = UI::DropBegin();
        UI::DropCaption(d, "PROFILES");
        for (int i = 0; i < Profiles_Count(); i++) {
            const char* n = Profiles_Name(i);
            const bool  cur = !strcmp(n, Profiles_Active());
            ImGui::PushID(i);
            if (UI::DropRow(d, "##p", ICON_LC_USER, n, cur) && !cur) {
                Profiles_Load(n);
                UISound_Play(Sfx::ToggleOn);
            }
            ImGui::PopID();
        }
        UI::DropSeparator(d);
        if (UI::DropRow(d, "##save", ICON_LC_SAVE, "Save now", false))                        Profiles_Save();
        if (UI::DropRow(d, "##copy", ICON_LC_CLIPBOARD_COPY, "Copy share code", false))        ShareCopyCode();
        if (UI::DropRow(d, "##paste", ICON_LC_CLIPBOARD_PASTE, "Import from clipboard", false)) ShareImportClipboard();
        UI::DropSeparator(d);

        {
            static char s_NewName[32] = "";
            const float bw = 34.0f, h = 30.0f;
            const float fw = ImGui::GetContentRegionAvail().x - bw - 6.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, (h - ImGui::GetFontSize()) * 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, V4(Glass(Theme::SecondaryBg, 0.8f)));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, V4(Theme::SurfaceHi));
            ImGui::PushStyleColor(ImGuiCol_Border, V4(Theme::Border));
            ImGui::SetNextItemWidth(fw);
            const bool enter = ImGui::InputTextWithHint("##newprof", "New profile name", s_NewName, sizeof(s_NewName),
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsItemActive())
                win->DrawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Theme::Accent, 6.0f);
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
            ImGui::SameLine(0.0f, 6.0f);
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            const bool add = ImGui::InvisibleButton("##add", ImVec2(bw, h));
            const bool bh  = ImGui::IsItemHovered();
            const bool can = s_NewName[0] != 0;
            win->DrawList->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + h),
                                         can ? (bh ? Theme::AccentHover : Theme::Accent) : U32A(Theme::SurfaceHi, 0.9f), 6.0f);
            const ImVec2 is = ImGui::CalcTextSize(ICON_LC_PLUS);
            win->DrawList->AddText(ImVec2(bp.x + (bw - is.x) * 0.5f, bp.y + (h - is.y) * 0.5f - 1.0f),
                                   can ? Theme::OnAccent : Theme::TextFaint, ICON_LC_PLUS);
            if ((add || enter) && can && Profiles_Create(s_NewName)) { s_NewName[0] = 0; ImGui::CloseCurrentPopup(); }
            ImGui::Dummy(ImVec2(0, 2));
        }

        if (Profiles_Count() > 1) {
            UI::DropSeparator(d);
            char del[64];
            snprintf(del, sizeof(del), "Delete \"%s\"", Profiles_Active());
            if (UI::DropRow(d, "##del", ICON_LC_TRASH_2, del, false, Theme::Danger)) {
                char name[32];
                snprintf(name, sizeof(name), "%s", Profiles_Active());
                Profiles_Delete(name);
            }
        }
        UI::DropEnd(d);
        ImGui::EndPopup();
        UI_FadeWindowTree(win, UI::EaseOut(d.t));
    }
    UI::DropPopStyle();
}

static void DrawContentPanel() {
    if (g_ActiveTab < 0 || g_ActiveTab >= s_NavCount) g_ActiveTab = 0;
    static int s_LastTab = -1;
    if (s_LastTab != g_ActiveTab) {
        s_LastTab = g_ActiveTab;
        s_TabTime = 0.0f;
    }
    s_TabTime += UI::AnimOff() ? 10.0f : ImGui::GetIO().DeltaTime * UI::g_AnimScale;
    if (s_FocusTime >= 0.0f) s_FocusTime += ImGui::GetIO().DeltaTime;
    s_CardIndex = 0;

    const UI_NavItem* nav = FindNav(g_ActiveTab);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 wsz = ImGui::GetWindowSize();
    const float  ww = wsz.x;
    dl->AddRectFilled(wp, ImVec2(wp.x + wsz.x, wp.y + wsz.y), Glass(Theme::SecondaryBg, 0.20f), Theme::RadiusWindow,
                      ImDrawFlags_RoundCornersRight);

    {
        const float e  = EaseOut(s_TabTime / 0.28f);
        const float ts = g_FontTitle ? g_FontTitle->FontSize : 26.0f;
        const float dx = (1.0f - e) * 14.0f * s_TabDir;
        UI::TextTracked(dl, g_FontTitle, ts, ImVec2(wp.x + 28 + dx, wp.y + 20), U32A(Theme::Text, e), nav->label, 0.6f);
        dl->AddText(ImVec2(wp.x + 29 + dx * 0.5f, wp.y + 52), U32A(Theme::TextDim, e), nav->subtitle);
    }

    {
        static float s_SearchHov = 0.0f, s_ProfHov = 0.0f;
        const float  h = 32.0f, y = wp.y + 24.0f;

        char prof[48];
        snprintf(prof, sizeof(prof), "%s", Profiles_Active());
        const float pw = ImMax(120.0f, ImGui::CalcTextSize(prof).x + 64.0f);
        const ImVec2 pp(wp.x + ww - 28 - pw, y);
        if (HeaderButton("##profbtn", pp, ImVec2(pw, h), &s_ProfHov)) ImGui::OpenPopup("##profiles");
        dl->AddText(ImVec2(pp.x + 12, y + 7), Theme::Accent, ICON_LC_USER);
        dl->AddText(ImVec2(pp.x + 34, y + (h - ImGui::GetFontSize()) * 0.5f), Theme::Text, prof);
        dl->AddText(ImVec2(pp.x + pw - 24, y + 7), Theme::TextFaint, ICON_LC_CHEVRON_DOWN);
        if (ImGui::IsPopupOpen("##profiles")) {
            ImGui::SetNextWindowPos(ImVec2(pp.x + pw, pp.y + h + 6), ImGuiCond_Always, ImVec2(1, 0));
            DrawProfilePopup();
        }

        const float sw = 210.0f;
        const ImVec2 sp(pp.x - 10 - sw, y);
        if (HeaderButton("##searchbtn", sp, ImVec2(sw, h), &s_SearchHov)) UI_Palette_Open();
        dl->AddText(ImVec2(sp.x + 12, y + 7), Mix(Theme::TextFaint, Theme::Accent, s_SearchHov), ICON_LC_SEARCH);
        dl->AddText(ImVec2(sp.x + 34, y + (h - ImGui::GetFontSize()) * 0.5f), Theme::TextFaint, "Search settings");
        DrawKbd(dl, ImVec2(sp.x + sw - 58, y + 7), "CTRL K");
    }

    dl->AddLine(ImVec2(wp.x + 28, wp.y + 84), ImVec2(wp.x + ww - 28, wp.y + 84), Theme::Separator);

    ImGui::SetCursorPos(ImVec2(0, 96));
    ImGui::BeginChild("##content_scroll", ImVec2(0, -10.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    static float s_ScrollTarget = 0.0f;
    static int   s_ScrollTab    = -1;
    if (s_ScrollTab != g_ActiveTab) { s_ScrollTab = g_ActiveTab; s_ScrollTarget = 0.0f; ImGui::SetScrollY(0.0f); }
    ImGui::Indent(28.0f);

    if (nav->render) nav->render();

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::Unindent(28.0f);

    {
        ImGuiWindow* sw = ImGui::GetCurrentWindow();
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.0f && !UI_Palette_IsOpen())
            s_ScrollTarget -= io.MouseWheel * 120.0f;
        if (s_FocusScrollY != 0.0f) {
            s_ScrollTarget = ImGui::GetScrollY() + (s_FocusScrollY - sw->Pos.y) - sw->Size.y * 0.35f;
            s_FocusScrollY = 0.0f;
        }
        s_ScrollTarget = ImClamp(s_ScrollTarget, 0.0f, ImGui::GetScrollMaxY());
        if (ImGui::GetActiveID() == ImGui::GetWindowScrollbarID(sw, ImGuiAxis_Y)) {
            s_ScrollTarget = ImGui::GetScrollY();
        } else {
            const float cur  = ImGui::GetScrollY();
            float       next = Approach(cur, s_ScrollTarget, 14.0f);
            if (fabsf(next - s_ScrollTarget) < 0.5f) next = s_ScrollTarget;
            if (next != cur) ImGui::SetScrollY(next);
        }
    }

    {
        const float remain = ImGui::GetScrollMaxY() - ImGui::GetScrollY();
        if (remain > 1.0f) {
            ImDrawList* sdl = ImGui::GetWindowDrawList();
            const ImVec2 cp = ImGui::GetWindowPos(), cs = ImGui::GetWindowSize();
            const float  a  = ImMin(remain / 40.0f, 1.0f);
            const ImU32  base = Theme::GlassOn ? Theme::PrimaryBg : Theme::SecondaryBg;
            sdl->PushClipRect(cp, ImVec2(cp.x + cs.x, cp.y + cs.y), false);
            sdl->AddRectFilledMultiColor(ImVec2(cp.x, cp.y + cs.y - 42), ImVec2(cp.x + cs.x, cp.y + cs.y),
                U32A(base, 0.0f), U32A(base, 0.0f), U32A(base, a * 0.85f), U32A(base, a * 0.85f));
            sdl->PopClipRect();
        }
    }

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

static void DrawBackdrop(const ImVec2& display, float alpha) {
    alpha *= g_UI.dim;
    if (alpha < 0.005f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 edge   = IM_COL32(4, 4, 6, (int)(150 * alpha));
    const ImU32 center = IM_COL32(8, 8, 10, (int)(60 * alpha));
    const ImVec2 mid(display.x * 0.5f, display.y * 0.5f);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(mid.x, mid.y), edge, center, center, center);
    dl->AddRectFilledMultiColor(ImVec2(mid.x, 0), ImVec2(display.x, mid.y), center, edge, center, center);
    dl->AddRectFilledMultiColor(ImVec2(mid.x, mid.y), display, center, center, edge, center);
    dl->AddRectFilledMultiColor(ImVec2(0, mid.y), ImVec2(mid.x, display.y), center, center, center, edge);
}

void UI_RenderMenu() {
    UISound_NewFrame();
    UIConfig_Tick();
    SharePoll();
    MenuAnim_Update();

    if (!MenuAnim_IsVisible()) return;

    ImGuiIO& io = ImGui::GetIO();
    const float alpha = g_MenuAnim.alpha;
    DrawBackdrop(io.DisplaySize, alpha);

    const ImVec2 ds = io.DisplaySize;
    const ImVec2 menu_size(g_UI.menuW >= 760.0f ? ImMin(g_UI.menuW, ds.x) : 980.0f,
                           g_UI.menuH >= 500.0f ? ImMin(g_UI.menuH, ds.y) : 620.0f);
    static ImVec2 s_BasePos(-1.0f, -1.0f);
    if (s_BasePos.x < 0.0f) {
        const bool saved = g_UI.menuX >= 0.0f && g_UI.menuY >= 0.0f;
        s_BasePos = saved ? ImVec2(ImClamp(g_UI.menuX, 0.0f, ImMax(ds.x - menu_size.x, 0.0f)),
                                   ImClamp(g_UI.menuY, 0.0f, ImMax(ds.y - menu_size.y, 0.0f)))
                          : ImVec2((ds.x - menu_size.x) * 0.5f, (ds.y - menu_size.y) * 0.5f);
    }

    const bool animating = g_MenuAnim.state == MenuAnimState::Opening ||
                           g_MenuAnim.state == MenuAnimState::Closing;
    if (animating)
        ImGui::SetNextWindowPos(ImVec2(s_BasePos.x, s_BasePos.y + 16.0f * (1.0f - alpha)), ImGuiCond_Always);
    else
        ImGui::SetNextWindowPos(s_BasePos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(menu_size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760, 500), ImVec2(FLT_MAX, FLT_MAX));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Theme::Border));
    ImGui::Begin("##glass_menu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGuiWindow* menuWindow = ImGui::GetCurrentWindow();

    if (!animating) {
        s_BasePos = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        const bool moved = fabsf(s_BasePos.x - g_UI.menuX) > 0.5f || fabsf(s_BasePos.y - g_UI.menuY) > 0.5f ||
                           fabsf(ws.x - g_UI.menuW) > 0.5f || fabsf(ws.y - g_UI.menuH) > 0.5f;
        if (moved && !ImGui::IsMouseDown(0)) {
            g_UI.menuX = s_BasePos.x; g_UI.menuY = s_BasePos.y;
            g_UI.menuW = ws.x;        g_UI.menuH = ws.y;
            UIConfig_MarkDirty();
        }
    }
    {
        const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        const ImVec2 we(wp.x + ws.x, wp.y + ws.y);
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        UIGfx_DrawShadow(bg, wp, we, Theme::RadiusWindow, alpha);
        UIGfx_DrawGlass(bg, wp, we, Theme::RadiusWindow, alpha);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(wp.x + Theme::RadiusWindow, wp.y + 1), ImVec2(we.x - Theme::RadiusWindow, wp.y + 1),
                                            IM_COL32(255, 255, 255, 22));
    }

    const float sideW = Spring(ImGui::GetID("##sidew"), g_UI.sidebarCollapsed ? kSideClosed : kSideOpen, 260.0f, 26.0f);
    DrawSidebar(sideW);
    ImGui::SameLine(0.0f, 0.0f);
    DrawContentPanel();

    const bool menuHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    UI_FadeWindowTree(menuWindow, alpha);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) UI_Palette_Open();
    UI_RenderPalette();

    if (menuHovered && ImGui::IsMouseReleased(0) && ImGui::IsAnyItemHovered() && !UISound_PlayedThisFrame())
        UISound_Play(Sfx::Click);
    {
        static ImGuiID s_LastHovered = 0;
        const ImGuiID hov = GImGui->HoveredId;
        if (hov != 0 && hov != s_LastHovered && menuHovered && !ImGui::IsMouseDown(0)) UISound_Play(Sfx::Hover);
        s_LastHovered = hov;
    }
    UI::RenderTooltip();
}

void UI_ApplyAppearance() {
    if (g_UI.accent >= Theme::kAccentCount) SetAccentCustom(g_UI.accentCustom);
    else                                    SetAccent(g_UI.accent);
    SetRoundness(g_UI.roundness);
    SetGlass(g_UI.glass, g_UI.solidity);
    static const float kAnim[4] = { 1000.0f, 1.6f, 1.0f, 0.7f };
    UI::g_AnimScale = kAnim[ImClamp(g_UI.animSpeed, 0, 3)];
}

void UI_RenderTab_Settings() {
    if (!UI::BeginColumns("##settings_cols")) return;

    UI::BeginCard("##card_look", "APPEARANCE");
    {
        const char* names[Theme::kAccentCount + 1];
        for (int i = 0; i < Theme::kAccentCount; i++) names[i] = Theme::kAccents[i].name;
        names[Theme::kAccentCount] = "Custom";
        if (UI::Segmented("Accent", &g_UI.accent, names, Theme::kAccentCount + 1)) { UI_ApplyAppearance(); UIConfig_MarkDirty(); }
        if (g_UI.accent >= Theme::kAccentCount && UI::ColorRow("Custom accent", g_UI.accentCustom)) {
            g_UI.accentCustom[3] = 1.0f;
            UI_ApplyAppearance();
            UIConfig_MarkDirty();
        }
        const char* corners[] = { "Sharp", "Soft", "Round" };
        if (UI::Segmented("Corners", &g_UI.roundness, corners, 3)) { UI_ApplyAppearance(); UIConfig_MarkDirty(); }
        const char* speeds[] = { "Off", "Fast", "Normal", "Relaxed" };
        if (UI::Segmented("Animations", &g_UI.animSpeed, speeds, 4)) { UI_ApplyAppearance(); UIConfig_MarkDirty(); }
        if (UI::Toggle("Compact sidebar", &g_UI.sidebarCollapsed)) UIConfig_MarkDirty();
    }
    UI::EndCard();

    UI::BeginCard("##card_surface", "SURFACES");
    {
        if (Theme::GlassAvailable) {
            if (UI::Toggle("Glass blur", &g_UI.glass)) { UI_ApplyAppearance(); UIConfig_MarkDirty(); }
            UI::Hint("Blurs the scene behind the menu. Off saves a little GPU time.");
        } else {
            UI::Hint("Glass backdrop unavailable on this device, using solid surfaces.");
        }
        if (Theme::GlassOn && UI::SliderPercent("Panel opacity", &g_UI.solidity)) { UI_ApplyAppearance(); UIConfig_MarkDirty(); }
        if (UI::SliderPercent("Background dim", &g_UI.dim)) UIConfig_MarkDirty();
    }
    UI::EndCard();

    UI::BeginCard("##card_sound", "SOUND");
    if (UI::Toggle("Interface sounds", &g_UI.sounds)) UIConfig_MarkDirty();
    if (g_UI.sounds) {
        if (UI::SliderPercent("Volume", &g_UI.volume)) UIConfig_MarkDirty();
        if (UI::Toggle("Hover ticks", &g_UI.hoverSounds)) UIConfig_MarkDirty();
        UI::Hint("Quiet tick when the cursor moves onto a control.");
    }
    UI::EndCard();

    UI::NextColumn();

    UI::BeginCard("##card_share", "PROFILES & SHARING");
    {
        ImGui::TextColored(V4(Theme::TextDim), "Active profile");
        ImGui::SameLine();
        ImGui::TextColored(V4(Theme::Accent), "%s", Profiles_Active());
        UI::Hint("Every setting, hotkey and Hold/Toggle choice saves to the active profile automatically.");
        ImGui::Dummy(ImVec2(0, 2));

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
        if (ImGui::BeginTable("##share_btns", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn(); if (UI::Button("Copy code", ICON_LC_CLIPBOARD_COPY))   ShareCopyCode();
            ImGui::TableNextColumn(); if (UI::Button("Paste code", ICON_LC_CLIPBOARD_PASTE)) ShareImportClipboard();
            ImGui::TableNextColumn(); if (UI::Button("Export file", ICON_LC_FILE_OUTPUT))    ShareExportFile();
            ImGui::TableNextColumn(); if (UI::Button(s_PickState == 1 ? "Choosing..." : "Import file", ICON_LC_FILE_INPUT)) ShareImportFile();
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        if (UI::Toggle("Include menu look", &g_UI.shareLook)) UIConfig_MarkDirty();
        UI::Hint("Codes carry only changed values and fit in a chat message. Imports become a new profile; "
                 "the current one stays as it is.");
    }
    UI::EndCard();

    UI::BeginCard("##card_input", "INPUT");
    if (UI::Toggle("Block input while menu open", &g_BlockInput)) UIConfig_MarkDirty();
    UI::Hint("When off, mouse and keyboard pass through to the application. INSERT toggles the menu either way.");
    UI::EndCard();

    UI::BeginCard("##card_tips", "TIPS");
    {
        static const char* const kTips[] = {
            "Hover a switch and click the keyboard chip to bind a key. Esc clears a binding.",
            "Next to a bound key, slide HOLD / TOGGLE: hold keeps the option on only while the key is down.",
            "Right-click a changed row to reset it to its default.",
            "Ctrl+K opens the command palette from anywhere in the menu.",
        };
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Theme::TextDim));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        for (const char* t : kTips) {
            ImGui::TextUnformatted(t);
            ImGui::Dummy(ImVec2(0, 2));
        }
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    UI::EndCard();

    UI::BeginCard("##card_about", "ABOUT");
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        UI::DrawLogo(dl, ImVec2(p.x + 18, p.y + 20), 34.0f, Theme::Accent);
        const float bs = g_FontBrand ? g_FontBrand->FontSize : 19.0f;
        UI::TextTracked(dl, g_FontBrand, bs, ImVec2(p.x + 46, p.y + 4), Theme::Text, s_BrandTitle, 2.0f);
        dl->AddText(ImVec2(p.x + 47, p.y + 26), Theme::TextDim, "Glass Menu for Dear ImGui");
        ImGui::Dummy(ImVec2(0, 48));
        ImGui::PushFont(Mono());
        ImGui::TextColored(V4(Theme::TextFaint), "%%APPDATA%%\\%s\\", UIConfig_Folder());
        if (ImGui::IsItemHovered())
            UI::Tooltip(ImGui::GetID("##cfgpath"), UIConfig_Path(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), UI::TipSide::Below);
        ImGui::PopFont();
    }
    UI::EndCard();

    UI::EndColumns();
}
