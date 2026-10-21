#pragma once
#include "imgui/imgui.h"

namespace Theme {
constexpr ImU32 PrimaryBg   = IM_COL32( 13,  14,  16, 255);
constexpr ImU32 SecondaryBg = IM_COL32( 18,  19,  22, 255);
constexpr ImU32 Surface     = IM_COL32( 24,  25,  29, 255);
constexpr ImU32 SurfaceHi   = IM_COL32( 34,  36,  41, 255);
constexpr ImU32 Border      = IM_COL32( 46,  48,  55, 255);
constexpr ImU32 BorderSoft  = IM_COL32( 46,  48,  55, 170);
constexpr ImU32 Separator   = IM_COL32( 36,  38,  44, 255);
constexpr ImU32 Text        = IM_COL32(236, 233, 226, 255);
constexpr ImU32 TextDim     = IM_COL32(164, 160, 151, 255);
constexpr ImU32 TextFaint   = IM_COL32(130, 126, 118, 255);
constexpr ImU32 OnAccent    = IM_COL32( 14,  14,  16, 255);
constexpr ImU32 Success     = IM_COL32(132, 180, 106, 255);
constexpr ImU32 Warning     = IM_COL32(232, 163,  61, 255);
constexpr ImU32 Danger      = IM_COL32(222,  92,  86, 255);

inline bool  GlassOn        = false;
inline bool  GlassAvailable = false;
inline bool  GlassWanted    = true;
inline float Solidity       = 0.0f;

inline ImU32 Accent      = IM_COL32(232, 163,  61, 255);
inline ImU32 AccentHover = IM_COL32(244, 186,  96, 255);
inline ImU32 AccentDeep  = IM_COL32(176, 118,  36, 255);

inline float RadiusWindow = 10.0f;
inline float RadiusCard   = 6.0f;
inline float RadiusFrame  = 4.0f;

struct AccentPreset { const char* name; ImU32 base, hover, deep; };
inline const AccentPreset kAccents[] = {
    { "Amber",   IM_COL32(232, 163,  61, 255), IM_COL32(244, 186,  96, 255), IM_COL32(176, 118,  36, 255) },
    { "Olive",   IM_COL32(158, 184,  96, 255), IM_COL32(180, 204, 118, 255), IM_COL32(112, 136,  62, 255) },
    { "Steel",   IM_COL32(112, 164, 218, 255), IM_COL32(142, 186, 232, 255), IM_COL32( 76, 122, 172, 255) },
    { "Crimson", IM_COL32(224,  92,  92, 255), IM_COL32(238, 122, 122, 255), IM_COL32(170,  62,  62, 255) },
};
constexpr int kAccentCount = 4;
}

static inline ImVec4 V4(ImU32 c)               { return ImGui::ColorConvertU32ToFloat4(c); }
static inline ImVec4 V4A(ImU32 c, float a)     { ImVec4 v = ImGui::ColorConvertU32ToFloat4(c); v.w = a; return v; }
static inline ImU32  U32A(ImU32 c, float a)    { return (c & ~IM_COL32_A_MASK) | ((ImU32)((a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a) * 255.0f) << IM_COL32_A_SHIFT); }

static inline ImU32  Glass(ImU32 c, float glassAlpha) {
    return Theme::GlassOn ? U32A(c, glassAlpha + (1.0f - glassAlpha) * Theme::Solidity) : c;
}

inline void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    s.WindowRounding=Theme::RadiusWindow; s.ChildRounding=Theme::RadiusCard; s.FrameRounding=Theme::RadiusFrame;
    s.PopupRounding=Theme::RadiusCard; s.ScrollbarRounding=4.0f; s.GrabRounding=Theme::RadiusFrame; s.TabRounding=4.0f;
    s.WindowBorderSize=1.0f; s.ChildBorderSize=1.0f; s.FrameBorderSize=1.0f; s.PopupBorderSize=1.0f; s.TabBorderSize=0.0f;
    s.WindowPadding=ImVec2(0,0); s.FramePadding=ImVec2(10,6); s.CellPadding=ImVec2(8,6);
    s.ItemSpacing=ImVec2(10,8); s.ItemInnerSpacing=ImVec2(8,6); s.IndentSpacing=14.0f;
    s.ScrollbarSize=6.0f; s.GrabMinSize=10.0f;
    s.WindowTitleAlign=ImVec2(0.0f,0.5f); s.ButtonTextAlign=ImVec2(0.5f,0.5f); s.SelectableTextAlign=ImVec2(0.0f,0.5f);
    s.AntiAliasedLines=true; s.AntiAliasedFill=true;

    c[ImGuiCol_Text]                 = V4(Theme::Text);
    c[ImGuiCol_TextDisabled]         = V4(Theme::TextFaint);
    c[ImGuiCol_WindowBg]             = V4(Glass(Theme::PrimaryBg, 0.55f));
    c[ImGuiCol_ChildBg]              = V4(Theme::SecondaryBg);
    c[ImGuiCol_PopupBg]              = V4A(Theme::Surface, 0.98f);
    c[ImGuiCol_Border]               = V4(Theme::BorderSoft);
    c[ImGuiCol_BorderShadow]         = ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg]              = V4(Theme::SecondaryBg);
    c[ImGuiCol_FrameBgHovered]       = V4(Theme::SurfaceHi);
    c[ImGuiCol_FrameBgActive]        = V4A(Theme::Accent, 0.18f);
    c[ImGuiCol_TitleBg]              = V4(Theme::PrimaryBg);
    c[ImGuiCol_TitleBgActive]        = V4(Theme::PrimaryBg);
    c[ImGuiCol_TitleBgCollapsed]     = V4A(Theme::PrimaryBg, 0.75f);
    c[ImGuiCol_MenuBarBg]            = V4(Theme::SecondaryBg);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]        = V4(Theme::Border);
    c[ImGuiCol_ScrollbarGrabHovered] = V4A(Theme::Accent, 0.50f);
    c[ImGuiCol_ScrollbarGrabActive]  = V4(Theme::Accent);
    c[ImGuiCol_CheckMark]            = V4(Theme::Accent);
    c[ImGuiCol_SliderGrab]           = V4(Theme::Accent);
    c[ImGuiCol_SliderGrabActive]     = V4(Theme::AccentHover);
    c[ImGuiCol_Button]               = V4(Theme::SurfaceHi);
    c[ImGuiCol_ButtonHovered]        = V4(Theme::Border);
    c[ImGuiCol_ButtonActive]         = V4A(Theme::Accent, 0.30f);
    c[ImGuiCol_Header]               = V4A(Theme::Accent, 0.14f);
    c[ImGuiCol_HeaderHovered]        = V4(Theme::SurfaceHi);
    c[ImGuiCol_HeaderActive]         = V4A(Theme::Accent, 0.22f);
    c[ImGuiCol_Separator]            = V4(Theme::Separator);
    c[ImGuiCol_SeparatorHovered]     = V4A(Theme::Accent, 0.40f);
    c[ImGuiCol_SeparatorActive]      = V4(Theme::Accent);
    c[ImGuiCol_Tab]                  = V4(Theme::SecondaryBg);
    c[ImGuiCol_TabHovered]           = V4(Theme::SurfaceHi);
    c[ImGuiCol_TabActive]            = V4(Theme::Surface);
    c[ImGuiCol_TableHeaderBg]        = V4(Theme::Surface);
    c[ImGuiCol_TableBorderStrong]    = V4(Theme::Border);
    c[ImGuiCol_TableBorderLight]     = V4(Theme::Separator);
    c[ImGuiCol_TableRowBg]           = ImVec4(0,0,0,0);
    c[ImGuiCol_TableRowBgAlt]        = V4A(Theme::Surface, 0.30f);
    c[ImGuiCol_TextSelectedBg]       = V4A(Theme::Accent, 0.30f);
    c[ImGuiCol_NavHighlight]         = V4(Theme::Accent);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.55f);
}

inline void SetAccent(int idx) {
    if (idx < 0 || idx >= Theme::kAccentCount) idx = 0;
    Theme::Accent      = Theme::kAccents[idx].base;
    Theme::AccentHover = Theme::kAccents[idx].hover;
    Theme::AccentDeep  = Theme::kAccents[idx].deep;
    ApplyTheme();
}

inline void SetAccentCustom(const float rgb[3]) {
    auto shade = [](const float c[3], float k) {
        float o[3];
        for (int i = 0; i < 3; i++) o[i] = k >= 0.0f ? c[i] + (1.0f - c[i]) * k : c[i] * (1.0f + k);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(o[0], o[1], o[2], 1.0f));
    };
    Theme::Accent      = shade(rgb, 0.0f);
    Theme::AccentHover = shade(rgb, 0.22f);
    Theme::AccentDeep  = shade(rgb, -0.25f);
    ApplyTheme();
}

inline void SetRoundness(int style) {
    const float k = style <= 0 ? 0.35f : style >= 2 ? 1.7f : 1.0f;
    Theme::RadiusWindow = 10.0f * k;
    Theme::RadiusCard   = 6.0f * k;
    Theme::RadiusFrame  = 4.0f * k;
    ApplyTheme();
}

inline void SetGlass(bool wanted, float solidity) {
    Theme::GlassWanted = wanted;
    Theme::Solidity    = solidity < 0.0f ? 0.0f : solidity > 1.0f ? 1.0f : solidity;
    Theme::GlassOn     = Theme::GlassAvailable && Theme::GlassWanted;
    ApplyTheme();
}
