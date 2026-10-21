#include "ui_palette.h"
#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_config.h"
#include "ui_registry.h"
#include "ui_sound.h"
#include "ui_gfx.h"
#include "ui_toast.h"
#include "IconsLucide.h"
#include "imgui/imgui_internal.h"

#include <cctype>
#include <cstdio>
#include <cstring>

enum class Kind { Tab, Field, Action };
enum ActionId { ACT_SAVE, ACT_OVERLAY, ACT_SIDEBAR, ACT_SOUNDS, ACT_LOCK, ACT_COUNT_ };

struct Entry { Kind kind; int idx; int score; };

static bool  s_Open      = false;
static bool  s_JustOpen  = false;
static char  s_Query[64] = "";
static int   s_Sel       = 0;
static Entry s_Results[320];
static int   s_ResultCount = 0;

static const char* ActionLabel(int a) {
    switch (a) {
        case ACT_SAVE:    return "Save profile now";
        case ACT_OVERLAY: return "Toggle active features overlay";
        case ACT_SIDEBAR: return "Toggle compact sidebar";
        case ACT_SOUNDS:  return "Toggle interface sounds";
        case ACT_LOCK:    return "Lock / unlock overlay position";
    }
    return "";
}

static const char* ActionIcon(int a) {
    switch (a) {
        case ACT_SAVE:    return ICON_LC_SAVE;
        case ACT_OVERLAY: return ICON_LC_LAYERS;
        case ACT_SIDEBAR: return ICON_LC_PANEL_LEFT;
        case ACT_SOUNDS:  return ICON_LC_VOLUME_2;
        case ACT_LOCK:    return ICON_LC_LOCK;
    }
    return ICON_LC_ZAP;
}

void UI_Palette_Open() {
    if (s_Open) return;
    s_Open = true; s_JustOpen = true;
    s_Query[0] = 0; s_Sel = 0;
    UISound_Play(Sfx::Open);
}

bool UI_Palette_IsOpen() { return s_Open; }
void UI_Palette_Close()  { s_Open = false; }

static int Score(const char* text, const char* q) {
    if (!*q) return 1;
    char t[96], qq[64];
    int n = 0;
    for (; text[n] && n < 95; n++) t[n] = (char)tolower((unsigned char)text[n]);
    t[n] = 0;
    int m = 0;
    for (; q[m] && m < 63; m++) qq[m] = (char)tolower((unsigned char)q[m]);
    qq[m] = 0;
    if (const char* hit = strstr(t, qq)) {
        const int pos = (int)(hit - t);
        return 1000 - pos * 4 - (n - m) + ((pos == 0 || t[pos - 1] == ' ') ? 200 : 0);
    }
    int ti = 0, gaps = 0;
    for (int qi = 0; qq[qi]; qi++) {
        while (t[ti] && t[ti] != qq[qi]) { ti++; gaps++; }
        if (!t[ti]) return -1;
        ti++;
    }
    return 300 - gaps;
}

static void Rebuild() {
    s_ResultCount = 0;
    auto add = [](Kind k, int i, int sc) {
        if (sc < 0 || s_ResultCount >= IM_ARRAYSIZE(s_Results)) return;
        s_Results[s_ResultCount++] = { k, i, sc };
    };
    for (int t = 0; t < UI_TabCount(); t++)
        if (UI_TabVisible(t)) add(Kind::Tab, t, Score(UI_TabLabel(t), s_Query) + 50);
    for (int i = 0; i < Registry_Count(); i++) {
        const Field* f = Registry_Get(i);
        int sc = Score(f->label, s_Query);
        if (sc < 0) { const int st = Score(UI_TabLabel(f->tab), s_Query); sc = st >= 0 ? st / 4 : -1; }
        add(Kind::Field, i, sc);
    }
    for (int a = 0; a < ACT_COUNT_; a++) add(Kind::Action, a, Score(ActionLabel(a), s_Query));

    for (int i = 1; i < s_ResultCount; i++) {
        Entry e = s_Results[i];
        int j = i - 1;
        while (j >= 0 && s_Results[j].score < e.score) { s_Results[j + 1] = s_Results[j]; j--; }
        s_Results[j + 1] = e;
    }
    if (s_Sel >= s_ResultCount) s_Sel = s_ResultCount - 1;
    if (s_Sel < 0) s_Sel = 0;
}

static void Execute(const Entry& e, bool toggle) {
    switch (e.kind) {
        case Kind::Tab:
            UI_FocusSetting(e.idx, "");
            UISound_Play(Sfx::Click);
            break;
        case Kind::Field: {
            const Field* f = Registry_Get(e.idx);
            if (toggle && f->type == FieldType::Bool) {
                bool* b = (bool*)f->ptr;
                *b = !*b;
                UISound_Play(*b ? Sfx::ToggleOn : Sfx::ToggleOff);
                UI_Toast(ICON_ON, f->label, *b ? "ON" : "OFF", *b ? Theme::Accent : Theme::TextFaint);
                return;
            }
            UI_FocusSetting(f->tab, f->label);
            UISound_Play(Sfx::Click);
            break;
        }
        case Kind::Action:
            switch (e.idx) {
                case ACT_SAVE:    Profiles_Save(); break;
                case ACT_OVERLAY: g_UI.overlay = !g_UI.overlay; UIConfig_MarkDirty(); break;
                case ACT_SIDEBAR: UI_ToggleSidebar(); break;
                case ACT_SOUNDS:  g_UI.sounds = !g_UI.sounds; UIConfig_MarkDirty(); break;
                case ACT_LOCK:    g_UI.overlayLocked = !g_UI.overlayLocked; UIConfig_MarkDirty(); break;
            }
            UISound_Play(Sfx::ToggleOn);
            break;
    }
    s_Open = false;
}

void UI_RenderPalette() {
    static float s_Anim = 0.0f;
    s_Anim = UI::Approach(s_Anim, s_Open ? 1.0f : 0.0f, 18.0f);
    if (s_Anim < 0.01f) return;
    const float a = s_Anim;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##palette_dim", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                 (s_Open ? 0 : ImGuiWindowFlags_NoInputs));
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, (int)(110 * a)));
    if (s_Open && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) { s_Open = false; UISound_Play(Sfx::Close); }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    const float W = 600.0f;
    const ImVec2 pos((io.DisplaySize.x - W) * 0.5f, io.DisplaySize.y * 0.18f - (1.0f - a) * 16.0f);
    const int   visibleRows = 9;
    const float rowH = 42.0f, headH = 54.0f, footH = 34.0f;
    const float H = headH + visibleRows * rowH + footH;

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(W, H));
    if (s_Open) ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##palette", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoMove | (s_Open ? 0 : ImGuiWindowFlags_NoInputs));
    ImGuiWindow* paletteWindow = ImGui::GetCurrentWindow();
    ImGui::BringWindowToDisplayFront(paletteWindow);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos(), q(p.x + W, p.y + H);
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    UIGfx_DrawShadow(bg, p, q, 10.0f, a);
    UIGfx_DrawGlass(dl, p, q, 10.0f, 1.0f);
    dl->AddRectFilled(p, q, Glass(Theme::PrimaryBg, 0.72f), 10.0f);
    dl->AddRect(p, q, Theme::Border, 10.0f);
    dl->AddLine(ImVec2(p.x + 10, p.y + 0.5f), ImVec2(q.x - 10, p.y + 0.5f), IM_COL32(255, 255, 255, 22));

    dl->AddText(ImVec2(p.x + 20, p.y + 17), Theme::Accent, ICON_LC_SEARCH);
    ImGui::SetCursorPos(ImVec2(46, 12));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
    ImGui::SetNextItemWidth(W - 70);
    if (s_JustOpen) { ImGui::SetKeyboardFocusHere(); s_JustOpen = false; }
    const bool enter = ImGui::InputTextWithHint("##q", "Search settings, tabs and actions...", s_Query, sizeof(s_Query),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemEdited()) s_Sel = 0;
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    dl->AddLine(ImVec2(p.x, p.y + headH), ImVec2(q.x, p.y + headH), Theme::Separator);

    Rebuild();

    if (s_Open) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { s_Sel = ImMin(s_Sel + 1, s_ResultCount - 1); UISound_Play(Sfx::Hover); }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   { s_Sel = ImMax(s_Sel - 1, 0); UISound_Play(Sfx::Hover); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))    { s_Open = false; UISound_Play(Sfx::Close); }
    }

    ImGui::SetCursorPos(ImVec2(0, headH));
    ImGui::BeginChild("##results", ImVec2(W, visibleRows * rowH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImDrawList* rl = ImGui::GetWindowDrawList();
    ImFont* mono = g_FontMono ? g_FontMono : ImGui::GetFont();
    static int s_LastSel = -1;
    for (int i = 0; i < s_ResultCount; i++) {
        const Entry& e = s_Results[i];
        ImGui::PushID(i);
        const ImVec2 r0 = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("##row", ImVec2(W, rowH));
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.x + ImGui::GetIO().MouseDelta.y * ImGui::GetIO().MouseDelta.y > 0.0f)
            s_Sel = i;
        if (i == s_Sel && s_LastSel != s_Sel) ImGui::SetScrollHereY(0.5f);
        const ImVec2 r1(r0.x + W, r0.y + rowH);
        const bool sel = i == s_Sel;
        if (sel) {
            rl->AddRectFilled(ImVec2(r0.x + 8, r0.y + 2), ImVec2(r1.x - 8, r1.y - 2), U32A(Theme::Accent, 0.12f), 6.0f);
            rl->AddRectFilled(ImVec2(r0.x + 8, r0.y + 10), ImVec2(r0.x + 10, r1.y - 10), Theme::Accent);
        }

        const char* icon = ICON_LC_ZAP;
        const char* title = "";
        char sub[64] = "";
        char right[32] = "";
        ImU32 rightCol = Theme::TextFaint;
        switch (e.kind) {
            case Kind::Tab:
                icon = UI_TabIcon(e.idx); title = UI_TabLabel(e.idx);
                snprintf(sub, sizeof(sub), "Go to tab");
                break;
            case Kind::Field: {
                const Field* f = Registry_Get(e.idx);
                icon = UI_TabIcon(f->tab); title = f->label;
                snprintf(sub, sizeof(sub), "%s", UI_TabLabel(f->tab));
                switch (f->type) {
                    case FieldType::Bool:
                        snprintf(right, sizeof(right), "%s", *(bool*)f->ptr ? "ON" : "OFF");
                        rightCol = *(bool*)f->ptr ? Theme::Accent : Theme::TextFaint;
                        break;
                    case FieldType::Float: snprintf(right, sizeof(right), "%.2f", *(float*)f->ptr); break;
                    case FieldType::Int:   snprintf(right, sizeof(right), "%d", *(int*)f->ptr); break;
                    case FieldType::Color: break;
                }
                if (const int vk = Hotkey_Get(e.idx)) {
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%s  %s", Hotkey_Name(vk), right);
                    snprintf(right, sizeof(right), "%s", tmp);
                }
                if (f->type == FieldType::Color) {
                    const float* c = (const float*)f->ptr;
                    rl->AddRectFilled(ImVec2(r1.x - 52, r0.y + 13), ImVec2(r1.x - 24, r1.y - 13),
                                      ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], 1.0f)), 3.0f);
                }
                break;
            }
            case Kind::Action:
                icon = ActionIcon(e.idx); title = ActionLabel(e.idx);
                snprintf(sub, sizeof(sub), "Action");
                break;
        }
        const float fh = ImGui::GetFontSize();
        rl->AddText(ImVec2(r0.x + 24, r0.y + (rowH - fh) * 0.5f - 1), sel ? Theme::Accent : Theme::TextFaint, icon);
        rl->AddText(ImVec2(r0.x + 54, r0.y + 5), sel ? Theme::Text : Theme::TextDim, title);
        rl->AddText(mono, 11.0f, ImVec2(r0.x + 54, r0.y + 24), Theme::TextFaint, sub);
        if (right[0]) {
            const ImVec2 rs = mono->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, right);
            rl->AddText(mono, 12.0f, ImVec2(r1.x - 24 - rs.x, r0.y + (rowH - rs.y) * 0.5f), rightCol, right);
        }
        if (clicked) { Execute(e, false); }
        ImGui::PopID();
    }
    if (s_ResultCount == 0) {
        ImGui::SetCursorPos(ImVec2(24, 16));
        ImGui::TextColored(V4(Theme::TextFaint), "No matches for \"%s\"", s_Query);
    }
    s_LastSel = s_Sel;
    ImGui::EndChild();

    {
        const float fy = q.y - footH;
        dl->AddLine(ImVec2(p.x, fy), ImVec2(q.x, fy), Theme::Separator);
        const char* hints = "UP/DOWN navigate    ENTER open    CTRL+ENTER toggle    ESC close";
        dl->AddText(mono, 11.0f, ImVec2(p.x + 20, fy + 11), Theme::TextFaint, hints);
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d", s_ResultCount);
        const ImVec2 cs = mono->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, cnt);
        dl->AddText(mono, 11.0f, ImVec2(q.x - 20 - cs.x, fy + 11), Theme::Accent, cnt);
    }

    if (s_Open && enter && s_ResultCount > 0) {
        Execute(s_Results[s_Sel], io.KeyCtrl);
        if (s_Open) s_JustOpen = true;
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    UI_FadeWindowTree(paletteWindow, a);
}
