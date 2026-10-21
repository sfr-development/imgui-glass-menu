<div align="center">

# Glass Menu

A modern glass-blur interface for Dear ImGui.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-1f1f1f?style=for-the-badge)](#build)
[![DirectX 11](https://img.shields.io/badge/DirectX-11-1f1f1f?style=for-the-badge)](#requirements)
[![Dear ImGui](https://img.shields.io/badge/Dear%20ImGui-1.91-1f1f1f?style=for-the-badge)](https://github.com/ocornut/imgui)
[![License](https://img.shields.io/badge/License-MIT-F5A623?style=for-the-badge)](LICENSE)

[Highlights](#highlights) · [Preview](#preview) · [Build](#build) · [Integrate](#integrate) · [API](#api)

</div>

---

## Highlights

A ready-to-use overlay interface, built on vanilla Dear ImGui, that goes beyond the default theme.

- Glass-blur sidebar and content panel, drawn with real D3D11 shaders
- Live-swappable accents, corner radii and animation speeds
- Hotkey chips on every toggle, with hold or toggle modes
- Profiles with autosave, share codes that fit in a chat message and file import or export
- Command palette on `Ctrl+K` that searches every page and every setting
- Draggable HUD elements with a dashed frame while the menu is open
- An always-visible active-features panel with a separate section for inactive items that carry a keybind
- Crosshair, info bar and input overlay out of the box
- One-call branding: title, status text, and an optional link button in the sidebar

## Preview

A standalone demo window ships in `demo/`. It wires the library to a Dear ImGui context and three sample pages (`Dashboard`, `Motion`, `Preferences`) plus the built-in `HUD` and `Settings` pages.

```bash
demo\build.bat
demo\bin\glass_menu_demo.exe
```

Press `Insert` to open or close the menu.

## Requirements

- Windows 10 2004 or newer
- DirectX 11 capable GPU
- Visual Studio 2022 or newer with the v143 toolset (C++17)
- Dear ImGui 1.91 or compatible (vendored under `imgui/`)

## Build

The library is a set of translation units in `src/` plus the vendored Dear ImGui files. Add them to any Dear ImGui host application that already has a D3D11 backend. The demo project in `demo/build.bat` shows the exact list.

```bash
git clone https://github.com/sfr-development/imgui-glass-menu.git
cd imgui-glass-menu
demo\build.bat
```

## Integrate

Set the branding, the pages and the status line once at startup; the library owns the sidebar and the content panel from then on.

```cpp
#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_registry.h"
#include "ui_fields.h"
#include "IconsLucide.h"

static void RenderDashboard() {
    UI::BeginCard("##card_main", "GENERAL");
    static bool enable = true;
    UI::Toggle("Enable", &enable);
    UI::Slider("Intensity", &g_State.intensity, 0.0f, 1.0f, "%.0f%%");
    UI::EndCard();
}

static const UI_NavItem kNav[] = {
    { ICON_LC_LAYOUT_DASHBOARD, "Dashboard", "Overview and quick toggles", "WORKSPACE", RenderDashboard },
    { ICON_LC_CROSSHAIR,        "HUD",       "Crosshair, info bar, overlays", "INTERFACE", UI_RenderTab_Hud },
    { ICON_LC_SETTINGS,         "Settings",  "Appearance, sound, profiles",   "INTERFACE", UI_RenderTab_Settings },
};

void Init(HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* ctx, IDXGISwapChain* swap) {
    ApplyTheme();
    UIConfig_SetFolder("MyApp");
    UI_SetBrand("MY", "APP");
    UI_SetStatus("ONLINE");
    UI_SetLink(ICON_LC_EXTERNAL_LINK, "Documentation", "https://example.com/docs");
    UI_SetNavigation(kNav, IM_ARRAYSIZE(kNav));
    UI_LoadFonts();

    const int tabSettings = UI_TabOf(UI_RenderTab_Settings);
    const int tabHud      = UI_TabOf(UI_RenderTab_Hud);
    static const Field kFields[] = { UI_PROFILE_FIELDS(tabSettings, tabHud) };
    Registry_Init(kFields, IM_ARRAYSIZE(kFields));

    UIGfx_Init(dev, ctx);
}

void Draw(ID3D11DeviceContext* ctx, IDXGISwapChain* swap, ID3D11RenderTargetView* rtv) {
    UI_RenderOverlay(nullptr, 0);
    UI_RenderMenu();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    UIGfx_Process(swap);
}
```

Pages are plain functions. The library calls them inside a scroll area and leaves everything else to Dear ImGui and the helpers in `UI::`.

## API

### Branding

```cpp
void UI_SetBrand(const char* title, const char* subtitle);
void UI_SetStatus(const char* text);
void UI_SetLink(const char* icon, const char* label, const char* url);
```

### Pages

```cpp
struct UI_NavItem {
    const char* icon;
    const char* label;
    const char* subtitle;
    const char* group;
    void      (*render)();
};
void UI_SetNavigation(const UI_NavItem* items, int count);
int  UI_TabOf(void (*render)());
```

Items with the same `group` string are listed under one caption. The order of the array is the sidebar order.

### Controls

```cpp
UI::BeginCard(id, title);
UI::Toggle(label, &value);
UI::Slider(label, &value, min, max, format);
UI::SliderPercent(label, &value01);
UI::Segmented(label, &index, items, count);
UI::ColorRow(label, color);
UI::Keybind(label, &vkCode, &mode);
UI::Hint("Short help text shown under the label.");
UI::EndCard();
```

### Profiles and hotkeys

Register every setting that should be saved, searched or bindable:

```cpp
static const Field kFields[] = {
    { "demo.enable",    "Enable",    tabDashboard, FieldType::Bool,  &enable },
    { "demo.intensity", "Intensity", tabDashboard, FieldType::Float, &intensity },
};
Registry_Init(kFields, IM_ARRAYSIZE(kFields));
```

Profiles are plain `.cfg` files under `%APPDATA%\<folder>\profiles\`. Share codes contain only values that differ from the default and fit in a chat message.

### Active-features overlay

An always-visible panel that lists what is on, with a separate section for inactive entries that carry a keybind, so viewers of the overlay know how to activate them.

```cpp
const OverlayItem active[] = {
    { "Highlight", "WORKSPACE", highlight.on, &highlight.on },
    { "Marker",    "WORKSPACE", marker.on,    &marker.on },
};
UI_RenderOverlay(active, IM_ARRAYSIZE(active));
```

## Project layout

```
├─ src/              The library (ui_layout, ui_theme, ui_hud, ui_overlay, ui_registry, ...)
├─ imgui/            Dear ImGui 1.91 (vendored)
├─ demo/             Standalone preview window with three sample pages
├─ LICENSE           MIT
└─ README.md
```

## License

MIT. See [LICENSE](LICENSE). Dear ImGui is MIT, Lucide icons are ISC.
