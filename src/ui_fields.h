#pragma once
#include "ui_anim.h"
#include "ui_config.h"
#include "ui_layout.h"
#include "ui_registry.h"

inline const UISettings kUiDefaults{};
inline const bool       kUiBlockInputDefault = true;

#define UI_PROFILE_FIELDS(TabSettings, TabHud)                                                                                      \
    { "ui.accent",           "Accent",                      (TabSettings), FieldType::Int,   &g_UI.accent,           &kUiDefaults.accent },           \
    { "ui.accentCustom",     "Custom accent",               (TabSettings), FieldType::Color, g_UI.accentCustom,      kUiDefaults.accentCustom },      \
    { "ui.roundness",        "Corners",                     (TabSettings), FieldType::Int,   &g_UI.roundness,        &kUiDefaults.roundness },        \
    { "ui.animSpeed",        "Animations",                  (TabSettings), FieldType::Int,   &g_UI.animSpeed,        &kUiDefaults.animSpeed },        \
    { "ui.sidebarCollapsed", "Compact sidebar",             (TabSettings), FieldType::Bool,  &g_UI.sidebarCollapsed, &kUiDefaults.sidebarCollapsed }, \
    { "ui.glass",            "Glass blur",                  (TabSettings), FieldType::Bool,  &g_UI.glass,            &kUiDefaults.glass },            \
    { "ui.solidity",         "Panel opacity",               (TabSettings), FieldType::Float, &g_UI.solidity,         &kUiDefaults.solidity },         \
    { "ui.dim",              "Background dim",              (TabSettings), FieldType::Float, &g_UI.dim,              &kUiDefaults.dim },              \
    { "ui.sounds",           "Interface sounds",            (TabSettings), FieldType::Bool,  &g_UI.sounds,           &kUiDefaults.sounds },           \
    { "ui.volume",           "Volume",                      (TabSettings), FieldType::Float, &g_UI.volume,           &kUiDefaults.volume },           \
    { "ui.hoverSounds",      "Hover ticks",                 (TabSettings), FieldType::Bool,  &g_UI.hoverSounds,      &kUiDefaults.hoverSounds },      \
    { "ui.blockInput",       "Block input while menu open", (TabSettings), FieldType::Bool,  &g_BlockInput,          &kUiBlockInputDefault },         \
    { "ui.overlay",          "Show overlay",                (TabHud),      FieldType::Bool,  &g_UI.overlay,          &kUiDefaults.overlay },          \
    { "ui.overlayLocked",    "Lock HUD positions",          (TabHud),      FieldType::Bool,  &g_UI.overlayLocked,    &kUiDefaults.overlayLocked },    \
    { "ui.overlayX",         "Overlay position X",          (TabHud),      FieldType::Float, &g_UI.overlayX,         &kUiDefaults.overlayX },         \
    { "ui.overlayY",         "Overlay position Y",          (TabHud),      FieldType::Float, &g_UI.overlayY,         &kUiDefaults.overlayY }
