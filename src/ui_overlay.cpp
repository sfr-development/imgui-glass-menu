#include "ui_overlay.h"
#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include "ui_config.h"
#include "ui_sound.h"
#include "ui_gfx.h"
#include "ui_registry.h"
#include "ui_toast.h"
#include "ui_hud.h"
#include "IconsLucide.h"
#include "imgui/imgui_internal.h"

#include <cstdio>
#include <cstring>
#include <cmath>

using UI::Approach;
using UI::EaseOut;

static const int   kMaxItems  = 64;
static const int   kMaxSlots  = 640;
static const float kSubH      = 26.0f;
static const float kMargin    = 14.0f;
static const float kSnapDist  = 28.0f;
static const float kWidth     = 240.0f;
static const float kHeadH     = 34.0f;
static const float kRowH      = 24.0f;

static ImU32 GroupColor(const char* g) {
    if (!g) return Theme::TextDim;
    unsigned h = 2166136261u;
    for (const char* p = g; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
    const ImU32 tones[4] = { Theme::Accent, Theme::Success, Theme::Warning, Theme::Danger };
    return tones[h % 4];
}

static const char* GroupOfTab(int tab) { return UI_TabGroup(tab); }

struct PanelRow {
    const char* name  = nullptr;
    const char* group = nullptr;
    int         vk    = 0;
    int         mode  = -1;
    bool        inActive = false;
    bool        inKeys   = false;
};

static void FieldKey(int f, int* vk, int* mode) {
    *vk   = f >= 0 ? Hotkey_Get(f) : 0;
    *mode = *vk ? (Hotkey_GetMode(f) == HotkeyMode::Hold ? 0 : 1) : -1;
}

static void DashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float phase) {
    const float dash = 8.0f, gap = 6.0f, period = dash + gap;
    auto edge = [&](ImVec2 p0, ImVec2 p1) {
        const ImVec2 d(p1.x - p0.x, p1.y - p0.y);
        const float  len = sqrtf(d.x * d.x + d.y * d.y);
        const ImVec2 u(d.x / len, d.y / len);
        for (float s = -fmodf(phase, period); s < len; s += period) {
            const float s0 = ImMax(s, 0.0f), s1 = ImMin(s + dash, len);
            if (s1 > s0) dl->AddLine(ImVec2(p0.x + u.x * s0, p0.y + u.y * s0), ImVec2(p0.x + u.x * s1, p0.y + u.y * s1), col, 1.0f);
        }
    };
    edge(a, ImVec2(b.x, a.y)); edge(ImVec2(b.x, a.y), b); edge(b, ImVec2(a.x, b.y)); edge(ImVec2(a.x, b.y), a);
}

static void RenderOverlayPanel(const OverlayItem* items, int count);

ImVec2 UI_HudPos(float nx, float ny, ImVec2 size) {
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const float rx = ImMax(ds.x - 2.0f * kMargin - size.x, 1.0f);
    const float ry = ImMax(ds.y - 2.0f * kMargin - size.y, 1.0f);
    return ImVec2(kMargin + ImClamp(nx, 0.0f, 1.0f) * rx, kMargin + ImClamp(ny, 0.0f, 1.0f) * ry);
}

bool UI_HudDrag(const char* id, ImVec2 pos, ImVec2 size, float* nx, float* ny, bool round) {
    const bool interactive = MenuAnim_IsVisible() && g_MenuAnim.alpha > 0.5f && !g_UI.overlayLocked;
    if (!interactive) return false;

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 ds = io.DisplaySize;
    const ImVec2 bMin(kMargin, kMargin), bMax(ds.x - kMargin, ds.y - kMargin);
    const float rx = ImMax(bMax.x - bMin.x - size.x, 1.0f);
    const float ry = ImMax(bMax.y - bMin.y - size.y, 1.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    if (GImGui->OpenPopupStack.Size == 0) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##drag", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    ImGuiStorage* st   = ImGui::GetStateStorage();
    const ImGuiID kHov = ImGui::GetID("hv"), kFrame = ImGui::GetID("fr");
    float hv = UI::Approach(st->GetFloat(kHov, 0.0f), (hovered || active) ? 1.0f : 0.0f, 14.0f);
    st->SetFloat(kHov, hv);

    bool dragging = false;
    if (active && ImGui::IsMouseDragging(0, 0.0f)) {
        dragging = true;
        const float px = ImClamp(pos.x + io.MouseDelta.x, bMin.x, bMax.x - size.x);
        const float py = ImClamp(pos.y + io.MouseDelta.y, bMin.y, bMax.y - size.y);
        *nx = (px - bMin.x) / rx;
        *ny = (py - bMin.y) / ry;
    }
    if (ImGui::IsItemDeactivated()) {
        if (*nx * rx < kSnapDist)          *nx = 0.0f;
        if ((1.0f - *nx) * rx < kSnapDist) *nx = 1.0f;
        if (*ny * ry < kSnapDist)          *ny = 0.0f;
        if ((1.0f - *ny) * ry < kSnapDist) *ny = 1.0f;
        UISound_Play(Sfx::Click);
    }
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    if (hv > 0.01f) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 c = U32A(Theme::Accent, 0.8f * hv);
        if (round) dl->AddCircle(ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), size.x * 0.5f + 4.0f, c, 0, 1.5f);
        else       dl->AddRect(ImVec2(pos.x - 3, pos.y - 3), ImVec2(pos.x + size.x + 3, pos.y + size.y + 3), c, 6.0f, 0, 1.5f);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    float fr = UI::Approach(ImGui::GetStateStorage()->GetFloat(kFrame, 0.0f), dragging || active ? 1.0f : (hovered ? 0.55f : 0.0f), 12.0f);
    ImGui::GetStateStorage()->SetFloat(kFrame, fr);
    if (fr > 0.01f) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        DashedRect(fg, bMin, bMax, U32A(Theme::Accent, 0.6f * fr), (float)ImGui::GetTime() * 14.0f);
        UI::TextTracked(fg, g_FontSection, g_FontSection ? g_FontSection->FontSize : 12.0f,
                        ImVec2(bMin.x + 10, bMin.y + 8), U32A(Theme::Accent, 0.85f * fr), "OVERLAY AREA", 1.8f);
        if (dragging) {
            const ImVec2 c(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            fg->AddLine(ImVec2(c.x, bMin.y), ImVec2(c.x, bMax.y), U32A(Theme::Accent, 0.18f * fr));
            fg->AddLine(ImVec2(bMin.x, c.y), ImVec2(bMax.x, c.y), U32A(Theme::Accent, 0.18f * fr));
        }
    }
    return dragging;
}

static void SyncAppearance() {
    unsigned h = 2166136261u;
    auto mix = [&](const void* p, size_t n) {
        const unsigned char* b = (const unsigned char*)p;
        for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    };
    mix(&g_UI.accent, sizeof(g_UI.accent));
    mix(g_UI.accentCustom, sizeof(float) * 3);
    mix(&g_UI.roundness, sizeof(g_UI.roundness));
    mix(&g_UI.animSpeed, sizeof(g_UI.animSpeed));
    mix(&g_UI.glass, sizeof(g_UI.glass));
    mix(&g_UI.solidity, sizeof(g_UI.solidity));
    static unsigned s_Applied = 0;
    if (h != s_Applied) { s_Applied = h; UI_ApplyAppearance(); }
}

void UI_RenderOverlay(const OverlayItem* items, int count) {
    Hotkeys_Poll();
    Profiles_Tick();
    UIConfig_Tick();
    SyncAppearance();
    Hud_RenderCrosshair();
    int active = 0;
    for (int i = 0; i < count; i++) active += items[i].active ? 1 : 0;
    Hud_RenderInfoBar(active);
    Hud_RenderInputOverlay();
    RenderOverlayPanel(items, count);
    UI_RenderToasts();
}

static void RenderOverlayPanel(const OverlayItem* items, int count) {
    if (count > kMaxItems) count = kMaxItems;

    ImGuiIO& io = ImGui::GetIO();
    const bool menuOpen    = MenuAnim_IsVisible() && g_MenuAnim.alpha > 0.5f;
    const bool interactive = menuOpen && !g_UI.overlayLocked;

    static PanelRow s_Rows[kMaxSlots];
    int slots = 0;
    for (int i = 0; i < count && slots < kMaxSlots; i++, slots++) {
        PanelRow& r = s_Rows[slots];
        r.name = items[i].name; r.group = items[i].group;
        int fvk = 0, fmode = -1;
        FieldKey(items[i].field ? Registry_IndexOf(items[i].field) : -1, &fvk, &fmode);
        r.vk   = items[i].key ? items[i].key : fvk;
        r.mode = items[i].key ? items[i].keyMode : fmode;
        r.inActive = items[i].active;
        r.inKeys   = !items[i].active && fvk != 0;
        if (r.inKeys) { r.vk = fvk; r.mode = fmode; }
    }
    const int regCount = Registry_Count();
    for (int f = 0; f < regCount && slots < kMaxSlots; f++) {
        const Field* fd = Registry_Get(f);
        PanelRow& r = s_Rows[slots++];
        r = PanelRow();
        if (!fd || fd->type != FieldType::Bool || !fd->ptr) continue;
        bool fixed = false;
        for (int i = 0; i < count && !fixed; i++) fixed = items[i].field == fd->ptr;
        if (fixed) continue;
        FieldKey(f, &r.vk, &r.mode);
        if (!r.vk) continue;
        r.name = fd->label; r.group = GroupOfTab(fd->tab);
        r.inActive = *(const bool*)fd->ptr;
        r.inKeys   = !r.inActive;
    }

    static float s_TA[kMaxSlots], s_TK[kMaxSlots];
    static bool  s_Init = false;
    if (!s_Init) {
        for (int i = 0; i < slots; i++) { s_TA[i] = s_Rows[i].inActive ? 1.0f : 0.0f; s_TK[i] = s_Rows[i].inKeys ? 1.0f : 0.0f; }
        s_Init = true;
    }

    int activeCount = 0, keyCount = 0;
    float rowsA = 0.0f, rowsK = 0.0f;
    for (int i = 0; i < slots; i++) {
        s_TA[i] = Approach(s_TA[i], s_Rows[i].inActive ? 1.0f : 0.0f, 14.0f);
        s_TK[i] = Approach(s_TK[i], s_Rows[i].inKeys ? 1.0f : 0.0f, 14.0f);
        activeCount += s_Rows[i].inActive ? 1 : 0;
        keyCount    += s_Rows[i].inKeys ? 1 : 0;
        rowsA += EaseOut(s_TA[i]) * kRowH;
        rowsK += EaseOut(s_TK[i]) * kRowH;
    }

    static float s_Vis   = 0.0f;
    static float s_Empty = 0.0f;
    static float s_Keys  = 0.0f;
    s_Vis   = Approach(s_Vis, (g_UI.overlay && (activeCount > 0 || keyCount > 0 || menuOpen)) ? 1.0f : 0.0f, 12.0f);
    s_Empty = Approach(s_Empty, activeCount == 0 ? 1.0f : 0.0f, 14.0f);
    s_Keys  = Approach(s_Keys, keyCount > 0 ? 1.0f : 0.0f, 14.0f);
    if (s_Vis < 0.01f) return;

    const float h = kHeadH + rowsA + s_Empty * kRowH + EaseOut(s_Keys) * kSubH + rowsK + 8.0f;
    const ImVec2 bMin(kMargin, kMargin);
    const ImVec2 bMax(io.DisplaySize.x - kMargin, io.DisplaySize.y - kMargin);
    const float rangeX = ImMax(bMax.x - bMin.x - kWidth, 1.0f);
    const float rangeY = ImMax(bMax.y - bMin.y - h, 1.0f);

    static ImVec2 s_Pos(-1.0f, -1.0f);
    static bool   s_Drag = false;
    static float  s_Frame = 0.0f;
    const ImVec2 target(bMin.x + ImClamp(g_UI.overlayX, 0.0f, 1.0f) * rangeX,
                        bMin.y + ImClamp(g_UI.overlayY, 0.0f, 1.0f) * rangeY);
    if (s_Pos.x < 0.0f) s_Pos = target;
    if (!interactive) s_Drag = false;
    if (!s_Drag) { s_Pos.x = Approach(s_Pos.x, target.x, 18.0f); s_Pos.y = Approach(s_Pos.y, target.y, 18.0f); }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    if (!interactive) flags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowPos(s_Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kWidth, h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, s_Vis);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##active_features", nullptr, flags);
    if (menuOpen && GImGui->OpenPopupStack.Size == 0) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const float  a = s_Vis;

    bool hovered = false;
    if (interactive) {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2(kWidth, h));
        hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.0f)) {
            s_Drag = true;
            s_Pos.x = ImClamp(s_Pos.x + io.MouseDelta.x, bMin.x, bMax.x - kWidth);
            s_Pos.y = ImClamp(s_Pos.y + io.MouseDelta.y, bMin.y, bMax.y - h);
        }
        if (ImGui::IsItemDeactivated() && s_Drag) {
            s_Drag = false;
            ImVec2 q = s_Pos;
            if (q.x - bMin.x < kSnapDist)              q.x = bMin.x;
            if (bMax.x - (q.x + kWidth) < kSnapDist)   q.x = bMax.x - kWidth;
            if (q.y - bMin.y < kSnapDist)              q.y = bMin.y;
            if (bMax.y - (q.y + h) < kSnapDist)        q.y = bMax.y - h;
            g_UI.overlayX = (q.x - bMin.x) / rangeX;
            g_UI.overlayY = (q.y - bMin.y) / rangeY;
            UIConfig_MarkDirty();
            UISound_Play(Sfx::Click);
        }
        if (hovered || s_Drag) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    float* hv = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.0f);
    *hv = Approach(*hv, (hovered || s_Drag) ? 1.0f : 0.0f, 14.0f);
    const ImVec2 pMax(p.x + kWidth, p.y + h);
    if (g_Hud.overlayShadow > 0.005f) UIGfx_DrawShadow(ImGui::GetBackgroundDrawList(), p, pMax, Theme::RadiusCard, a * g_Hud.overlayShadow);
    UIGfx_DrawGlass(dl, p, pMax, Theme::RadiusCard, a);
    dl->AddRectFilled(p, pMax, U32A(Theme::PrimaryBg, (Theme::GlassOn ? 0.62f : 0.9f) * a), Theme::RadiusCard);
    dl->AddLine(ImVec2(p.x + 6, p.y + 0.5f), ImVec2(pMax.x - 6, p.y + 0.5f), IM_COL32(255, 255, 255, (int)(20 * a)));
    dl->AddRect(p, pMax, U32A(Theme::Border, a), Theme::RadiusCard);
    if (*hv > 0.01f) dl->AddRect(p, pMax, U32A(Theme::Accent, 0.7f * *hv * a), Theme::RadiusCard, 0, 1.5f);
    dl->AddRectFilled(ImVec2(p.x + 12, p.y), ImVec2(p.x + 52, p.y + 2), U32A(Theme::Accent, a));

    UI::DrawLogo(dl, ImVec2(p.x + 19, p.y + 18), 15.0f, U32A(Theme::Accent, a));
    const float ss = g_FontSection ? g_FontSection->FontSize : 12.0f;
    UI::TextTracked(dl, g_FontSection, ss, ImVec2(p.x + 34, p.y + 11), U32A(Theme::TextDim, a), "ACTIVE", 1.8f);
    {
        ImFont* mono = g_FontMono ? g_FontMono : ImGui::GetFont();
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d", activeCount);
        const ImVec2 cs = mono->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, cnt);
        dl->AddText(mono, 12.0f, ImVec2(pMax.x - 12 - cs.x, p.y + 11), U32A(Theme::Accent, a), cnt);
        if (interactive) {
            const char* grip = ICON_LC_GRIP_VERTICAL;
            const ImVec2 gs = ImGui::CalcTextSize(grip);
            dl->AddText(ImVec2(pMax.x - 22 - cs.x - gs.x, p.y + 9), U32A(Theme::TextFaint, a * (0.4f + 0.6f * *hv)), grip);
        }
    }
    dl->AddLine(ImVec2(p.x + 12, p.y + kHeadH - 4), ImVec2(pMax.x - 12, p.y + kHeadH - 4), U32A(Theme::Separator, a));

    auto drawRow = [&](const PanelRow& r, float y, float e, bool dim) {
        const float ra = a * e;
        const float x  = p.x + 12 + (1.0f - e) * 16.0f;
        const float cy = y + kRowH * 0.5f;
        dl->AddRectFilled(ImVec2(x, cy - 6), ImVec2(x + 2, cy + 6), U32A(GroupColor(r.group), ra * (dim ? 0.45f : 1.0f)));
        float textRight = pMax.x - 12.0f;
        if (r.vk) {
            ImFont* mono = g_FontMono ? g_FontMono : ImGui::GetFont();
            const char*  key = Hotkey_Name(r.vk);
            const ImVec2 ks  = mono->CalcTextSizeA(10.5f, FLT_MAX, 0.0f, key);
            const ImVec2 c0(pMax.x - 12.0f - ks.x - 10.0f, cy - 8.0f), c1(pMax.x - 12.0f, cy + 8.0f);
            dl->AddRectFilled(c0, c1, U32A(Theme::SurfaceHi, ra), 3.0f);
            dl->AddRect(c0, c1, U32A(dim ? Theme::Accent : Theme::Border, ra * (dim ? 0.55f : 1.0f)), 3.0f);
            dl->AddText(mono, 10.5f, ImVec2(c0.x + 5.0f, cy - ks.y * 0.5f), U32A(dim ? Theme::Text : Theme::TextDim, ra), key);
            textRight = c0.x - 6.0f;
            if (r.mode >= 0) {
                const char* m  = r.mode == 0 ? "HOLD" : "TOGGLE";
                const float mw = UI::TrackedWidth(g_FontSection, 9.0f, m, 0.6f);
                UI::TextTracked(dl, g_FontSection, 9.0f, ImVec2(textRight - mw, cy - 5.5f),
                                U32A(r.mode == 0 ? Theme::Accent : Theme::TextFaint, ra), m, 0.6f);
                textRight -= mw + 6.0f;
            }
        }
        const ImVec2 ts = ImGui::CalcTextSize(r.name);
        dl->PushClipRect(ImVec2(x, y), ImVec2(textRight, y + kRowH), true);
        dl->AddText(ImVec2(x + 10, cy - ts.y * 0.5f), U32A(dim ? Theme::TextDim : Theme::Text, ra), r.name);
        dl->PopClipRect();
    };

    float y = p.y + kHeadH;
    dl->PushClipRect(p, pMax, true);
    for (int i = 0; i < slots; i++) {
        const float e = EaseOut(s_TA[i]);
        if (e < 0.01f || !s_Rows[i].name) continue;
        drawRow(s_Rows[i], y, e, false);
        y += e * kRowH;
    }
    if (s_Empty > 0.01f) {
        const ImVec2 ts = ImGui::CalcTextSize("No active features");
        dl->AddText(ImVec2(p.x + 14, y + (kRowH - ts.y) * 0.5f), U32A(Theme::TextFaint, a * s_Empty), "No active features");
        y += s_Empty * kRowH;
    }

    const float ke = EaseOut(s_Keys);
    if (ke > 0.01f) {
        const float ka = a * ke;
        dl->AddLine(ImVec2(p.x + 12, y + 6), ImVec2(pMax.x - 12, y + 6), U32A(Theme::Separator, ka));
        UI::TextTracked(dl, g_FontSection, 10.0f, ImVec2(p.x + 14, y + 11), U32A(Theme::TextFaint, ka), "KEYBINDS", 1.6f);
        ImFont* mono = g_FontMono ? g_FontMono : ImGui::GetFont();
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d", keyCount);
        const ImVec2 cs = mono->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, cnt);
        dl->AddText(mono, 11.0f, ImVec2(pMax.x - 12 - cs.x, y + 10), U32A(Theme::TextFaint, ka), cnt);
        y += ke * kSubH;
    }
    for (int i = 0; i < slots; i++) {
        const float e = EaseOut(s_TK[i]);
        if (e < 0.01f || !s_Rows[i].name) continue;
        drawRow(s_Rows[i], y, e, true);
        y += e * kRowH;
    }
    dl->PopClipRect();

    ImGui::End();
    ImGui::PopStyleVar(3);

    s_Frame = Approach(s_Frame, !interactive ? 0.0f : s_Drag ? 1.0f : (hovered ? 0.55f : 0.0f), 12.0f);
    if (s_Frame > 0.01f) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        DashedRect(fg, bMin, bMax, U32A(Theme::Accent, 0.6f * s_Frame), (float)ImGui::GetTime() * 14.0f);
        UI::TextTracked(fg, g_FontSection, g_FontSection ? g_FontSection->FontSize : 12.0f,
                        ImVec2(bMin.x + 10, bMin.y + 8), U32A(Theme::Accent, 0.85f * s_Frame), "OVERLAY AREA", 1.8f);
        if (s_Drag) {
            const ImVec2 c(s_Pos.x + kWidth * 0.5f, s_Pos.y + h * 0.5f);
            fg->AddLine(ImVec2(c.x, bMin.y), ImVec2(c.x, bMax.y), U32A(Theme::Accent, 0.18f * s_Frame));
            fg->AddLine(ImVec2(bMin.x, c.y), ImVec2(bMax.x, c.y), U32A(Theme::Accent, 0.18f * s_Frame));
        }
    }
}
