#pragma once
#include "imgui/imgui.h"
#include "IconsLucide.h"

#define ICON_ON      ICON_LC_TOGGLE_RIGHT
#define ICON_OFF     ICON_LC_TOGGLE_RIGHT
#define ICON_PROFILE ICON_LC_USER
#define ICON_SAVED   ICON_LC_CHECK
#define ICON_DELETED ICON_LC_TRASH_2

void UI_Toast(const char* icon, const char* title, const char* detail, ImU32 color);
void UI_RenderToasts();
