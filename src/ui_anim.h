#pragma once
#include "imgui/imgui.h"

enum class MenuAnimState { Closed, Opening, Open, Closing };

struct MenuAnim {
    MenuAnimState state = MenuAnimState::Closed;
    float t             = 0.0f;
    float alpha         = 0.0f;
    float openDuration  = 0.18f;
    float closeDuration = 0.16f;
};

extern MenuAnim g_MenuAnim;
extern bool     g_ShowMenu;
extern bool     g_BlockInput;

void MenuAnim_Update();
void MenuAnim_Toggle();
bool MenuAnim_WantsInput();
bool MenuAnim_IsVisible();
