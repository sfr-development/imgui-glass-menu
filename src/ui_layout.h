#pragma once
#include "imgui/imgui.h"
#include "ui_kit.h"

extern ImFont* g_FontBody;
extern ImFont* g_FontSidebar;
extern ImFont* g_FontSection;
extern ImFont* g_FontTitle;
extern ImFont* g_FontMono;
extern ImFont* g_FontBrand;

struct UI_NavItem {
    const char* icon;
    const char* label;
    const char* subtitle;
    const char* group;
    void      (*render)();
};

void UI_SetNavigation(const UI_NavItem* items, int count);
int  UI_TabCount();
int  UI_TabOf(void (*render)());
extern int g_ActiveTab;

void UI_SetBrand(const char* title, const char* subtitle);
const char* UI_BrandTitle();
const char* UI_BrandSubtitle();
void UI_SetLink(const char* icon, const char* label, const char* url);
void UI_SetStatus(const char* text);

void UI_LoadFonts();
void UI_RenderMenu();
void UI_RenderTab_Settings();
void UI_RenderTab_Hud();
void UI_ApplyAppearance();

struct ImGuiWindow;
void        UI_FadeWindowTree(ImGuiWindow* root, float alpha);

bool        UI_TabVisible(int tab);
const char* UI_TabLabel(int tab);
const char* UI_TabIcon(int tab);
const char* UI_TabGroup(int tab);
void        UI_FocusSetting(int tab, const char* label);
void        UI_ToggleSidebar();

namespace UI {
void  SectionHeader(const char* label);
bool  BeginCard(const char* id, const char* title = nullptr);
void  EndCard();
enum ToggleFlags_ { ToggleFlags_None = 0, ToggleFlags_NoHoldMode = 1 << 0 };
bool  Toggle(const char* label, bool* v, int flags = ToggleFlags_None);
bool  Keybind(const char* label, int* vk, int* mode = nullptr, bool enabled = true);
void  Hint(const char* text);
bool  Button(const char* label, const char* icon = nullptr);
bool  Slider(const char* label, float* v, float min, float max, const char* fmt = "%.1f");
bool  SliderInt(const char* label, int* v, int min, int max, const char* fmt = "%d");
bool  SliderPercent(const char* label, float* v01);
bool  ColorRow(const char* label, float col[4]);
bool  Combo(const char* label, int* v, const char* const items[], int count);
bool  Segmented(const char* label, int* v, const char* const items[], int count);
bool  BeginColumns(const char* id, int count = 2);
void  NextColumn();
void  EndColumns();

bool  Notice(const char* id, const char* icon, ImU32 tone, const char* title, const char* body,
             const char* const chips[], int chipCount, bool* dontShowAgain);

enum class TipSide { Right, Below };
void  Tooltip(ImGuiID id, const char* text, ImVec2 anchorMin, ImVec2 anchorMax, TipSide side, float delay = 0.35f);

}
