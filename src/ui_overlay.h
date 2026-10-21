#pragma once
#include "imgui/imgui.h"

struct OverlayItem {
    const char* name;
    const char* group;
    bool        active;
    const void* field   = nullptr;
    int         key     = 0;
    int         keyMode = -1;
};

void UI_RenderOverlay(const OverlayItem* items, int count);

ImVec2 UI_HudPos(float nx, float ny, ImVec2 size);
bool   UI_HudDrag(const char* id, ImVec2 pos, ImVec2 size, float* nx, float* ny, bool round = false);
