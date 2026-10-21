#include "ui_layout.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include "ui_overlay.h"
#include "ui_gfx.h"
#include "ui_registry.h"
#include "ui_palette.h"
#include "ui_toast.h"
#include "ui_config.h"
#include "ui_hud.h"
#include "ui_fields.h"
#include "IconsLucide.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace demo {

struct State {
    bool  enabled       = true;
    bool  notifications = true;
    bool  autoUpdate    = false;
    bool  telemetry     = false;
    float intensity     = 65.0f;
    int   mode          = 1;
    int   quality       = 2;
    int   samples       = 8;
    float opacity