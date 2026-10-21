#include "glass_menu.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include <d3d11.h>
#include <windows.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device*            g_Device  = nullptr;
static ID3D11DeviceContext*     g_Context = nullptr;
static IDXGISwapChain*          g_Swap    = nullptr;
static ID3D11RenderTargetView*  g_RTV     = nullptr;
static UINT g_ResizeW = 0, g_ResizeH = 0;

static void CreateRTV() {
    ID3D11Texture2D* bb = nullptr;
    g_Swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) { g_Device->CreateRenderTargetView(bb, nullptr, &g_RTV); bb->Release(); }
}
static void CleanupRTV() { if (g_RTV) { g_RTV->Release(); g_RTV = nullptr; } }

static bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    D3D_FEATURE_LEVEL lvl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &lvl, 1, D3D11_SDK_VERSION, &sd, &g_Swap, &g_Device, nullptr, &g_Context))) return false;
    CreateRTV();
    return true;
}

namespace demo {
    struct { bool on = true; bool outline = true; bool labels = true; bool shadow = true;
             float thickness = 2.0f; float radius = 8.0f; float opacity = 0.9f;
             float color[4] = {0.98f, 0.62f, 0.22f, 1.0f}; } highlight;

    struct { bool on = false; int mode = 0; int target = 0;
             bool animations = true; float speed = 1.0f; } motion;

    struct { bool on = false; float x = 0.5f, y = 0.5f;
             float color[4] = {0.45f, 1.00f, 0.60f, 1.00f}; } marker;

    struct { bool on = false; bool quiet = false; bool nightMode = false;
             int preset = 1; float intensity = 0.6f; } preferences;
}

static void RenderDashboard() {
    if (!UI::BeginColumns("##dash_cols")) return;

    UI::BeginCard("##card_hl", "HIGHLIGHT");
    UI::Toggle("Enable highlight", &demo::highlight.on);
    UI::Hint("Draws an outline and label around the focused element.");
    UI::Toggle("Draw outline", &demo::highlight.outline);
    UI::Toggle("Show labels", &demo::highlight.labels);
    UI::Toggle("Drop shadow", &demo::highlight.shadow);
    UI::Slider("Thickness", &demo::highlight.thickness, 0.5f, 6.0f, "%.1f px");
    UI::Slider("Corner radius", &demo::highlight.radius, 0.0f, 24.0f, "%.0f px");
    UI::SliderPercent("Opacity", &demo::highlight.opacity);
    UI::ColorRow("Highlight color", demo::highlight.color);
    UI::EndCard();

    UI::NextColumn();

    UI::BeginCard("##card_marker", "MARKER");
    UI::Toggle("Pin a marker", &demo::marker.on);
    UI::Hint("A draggable dot in the viewport. Position is a profile field.");
    UI::Slider("Marker X", &demo::marker.x, 0.0f, 1.0f, "%.2f");
    UI::Slider("Marker Y", &demo::marker.y, 0.0f, 1.0f, "%.2f");
    UI::ColorRow("Marker color", demo::marker.color);
    UI::EndCard();

    UI::EndColumns();
}

static void RenderMotion() {
    UI::BeginCard("##card_motion", "MOTION");
    UI::Toggle("Enable motion", &demo::motion.on);
    const char* modes[] = { "Follow", "Orbit", "Linear", "Random" };
    UI::Segmented("Mode", &demo::motion.mode, modes, 4);
    const char* targets[] = { "Camera", "Marker", "Cursor" };
    UI::Segmented("Target", &demo::motion.target, targets, 3);
    UI::Slider("Speed", &demo::motion.speed, 0.1f, 4.0f, "%.1fx");
    UI::Toggle("Animations", &demo::motion.animations);
    UI::Hint("A sample page to show toggles, segmented switches and sliders.");
    UI::EndCard();
}

static void RenderPreferences() {
    UI::BeginCard("##card_pref", "PREFERENCES");
    UI::Toggle("Reduced interface", &demo::preferences.on);
    UI::Toggle("Quiet mode", &demo::preferences.quiet);
    UI::Toggle("Night mode", &demo::preferences.nightMode);
    const char* presets[] = { "Default", "Compact", "Comfortable", "Playful" };
    UI::Segmented("Preset", &demo::preferences.preset, presets, 4);
    UI::SliderPercent("Intensity", &demo::preferences.intensity);
    UI::EndCard();
}

static const UI_NavItem kNav[] = {
    { ICON_LC_LAYOUT_DASHBOARD, "Dashboard",   "Toggles, sliders and color pickers",     "WORKSPACE", RenderDashboard },
    { ICON_LC_SPARKLES,         "Motion",      "Segmented switches and continuous input", "WORKSPACE", RenderMotion    },
    { ICON_LC_SLIDERS,          "Preferences", "Grouped bool and preset controls",       "WORKSPACE", RenderPreferences },
    { ICON_LC_CROSSHAIR,        "HUD",         "Crosshair, info bar and overlays",       "INTERFACE", UI_RenderTab_Hud },
    { ICON_LC_SETTINGS,         "Settings",    "Appearance, sound, profiles, input",     "INTERFACE", UI_RenderTab_Settings },
};

static void RegisterDemoSettings() {
    const int tabSettings = UI_TabOf(UI_RenderTab_Settings);
    const int tabHud      = UI_TabOf(UI_RenderTab_Hud);
    const int tabDash     = UI_TabOf(RenderDashboard);
    const int tabMotion   = UI_TabOf(RenderMotion);
    const int tabPref     = UI_TabOf(RenderPreferences);
    static const Field kFields[] = {
        { "hl.on",         "Enable highlight", tabDash,   FieldType::Bool,  &demo::highlight.on },
        { "hl.outline",    "Draw outline",     tabDash,   FieldType::Bool,  &demo::highlight.outline },
        { "hl.labels",     "Show labels",      tabDash,   FieldType::Bool,  &demo::highlight.labels },
        { "hl.shadow",     "Drop shadow",      tabDash,   FieldType::Bool,  &demo::highlight.shadow },
        { "hl.thickness",  "Thickness",        tabDash,   FieldType::Float, &demo::highlight.thickness },
        { "hl.radius",     "Corner radius",    tabDash,   FieldType::Float, &demo::highlight.radius },
        { "hl.opacity",    "Opacity",          tabDash,   FieldType::Float, &demo::highlight.opacity },
        { "hl.color",      "Highlight color",  tabDash,   FieldType::Color, demo::highlight.color },
        { "marker.on",     "Pin a marker",     tabDash,   FieldType::Bool,  &demo::marker.on },
        { "marker.x",      "Marker X",         tabDash,   FieldType::Float, &demo::marker.x },
        { "marker.y",      "Marker Y",         tabDash,   FieldType::Float, &demo::marker.y },
        { "marker.color",  "Marker color",     tabDash,   FieldType::Color, demo::marker.color },
        { "motion.on",     "Enable motion",    tabMotion, FieldType::Bool,  &demo::motion.on },
        { "motion.mode",   "Mode",             tabMotion, FieldType::Int,   &demo::motion.mode },
        { "motion.target", "Target",           tabMotion, FieldType::Int,   &demo::motion.target },
        { "motion.speed",  "Speed",            tabMotion, FieldType::Float, &demo::motion.speed },
        { "motion.anim",   "Animations",       tabMotion, FieldType::Bool,  &demo::motion.animations },
        { "pref.on",       "Reduced interface",tabPref,   FieldType::Bool,  &demo::preferences.on },
        { "pref.quiet",    "Quiet mode",       tabPref,   FieldType::Bool,  &demo::preferences.quiet },
        { "pref.night",    "Night mode",       tabPref,   FieldType::Bool,  &demo::preferences.nightMode },
        { "pref.preset",   "Preset",           tabPref,   FieldType::Int,   &demo::preferences.preset },
        { "pref.intensity","Intensity",        tabPref,   FieldType::Float, &demo::preferences.intensity },
        UI_PROFILE_FIELDS(tabSettings, tabHud),
    };
    Registry_Init(kFields, IM_ARRAYSIZE(kFields));
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 0;
    switch (msg) {
    case WM_SIZE:
        if (g_Device && wParam != SIZE_MINIMIZED) {
            g_ResizeW = (UINT)LOWORD(lParam);
            g_ResizeH = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_INSERT) MenuAnim_Toggle();
        break;
    case WM_MOUSEWHEEL:
        Hud_InputWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void SaveBackbufferBmp(const wchar_t* path) {
    ID3D11Texture2D* back = nullptr;
    g_Swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (!back) return;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    g_Device->CreateTexture2D(&d, nullptr, &staging);
    if (!staging) { back->Release(); return; }
    g_Context->CopyResource(staging, back);
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(g_Context->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"wb") == 0 && f) {
            BITMAPFILEHEADER fh = {}; BITMAPINFOHEADER ih = {};
            ih.biSize = sizeof(ih); ih.biWidth = (LONG)d.Width; ih.biHeight = -(LONG)d.Height;
            ih.biPlanes = 1; ih.biBitCount = 32; ih.biCompression = BI_RGB;
            fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih);
            fh.bfSize = fh.bfOffBits + d.Width * d.Height * 4;
            fwrite(&fh, sizeof(fh), 1, f); fwrite(&ih, sizeof(ih), 1, f);
            for (UINT y = 0; y < d.Height; y++) {
                const BYTE* row = (const BYTE*)m.pData + y * m.RowPitch;
                for (UINT x = 0; x < d.Width; x++) {
                    const BYTE px[4] = { row[x * 4 + 2], row[x * 4 + 1], row[x * 4 + 0], 255 };
                    fwrite(px, 4, 1, f);
                }
            }
            fclose(f);
        }
        g_Context->Unmap(staging, 0);
    }
    staging->Release();
    back->Release();
}

struct Shot { const wchar_t* name; void (*setup)(); void (*late)(); };

static void ShotPage0()   { g_ActiveTab = 0; }
static void ShotPage1()   { g_ActiveTab = 1; }
static void ShotPage2()   { g_ActiveTab = 2; }
static void ShotPage3()   { g_ActiveTab = 3; }
static void ShotPage4()   { g_ActiveTab = 4; }
static void ShotPalette() { g_ActiveTab = 0; UI_Palette_Open(); }
static void TypeQuery()   { ImGui::GetIO().AddInputCharactersUTF8("color"); }
static void ShotCompact() { UI_Palette_Close(); g_UI.sidebarCollapsed = true; g_ActiveTab = 1; }
static void PickAccent(int i) { g_UI.sidebarCollapsed = false; g_ActiveTab = 0; g_UI.accent = i; UI_ApplyAppearance(); }
static void ShotAccent0() { PickAccent(0); }
static void ShotAccent1() { PickAccent(1); }
static void ShotAccent2() { PickAccent(2); }
static void ShotAccent3() { PickAccent(3); }
static void ShotHud() {
    PickAccent(0);
    g_Hud.crosshair = g_Hud.input = true;
    MenuAnim_Toggle();
}

static const Shot kShots[] = {
    { L"page_dashboard",   ShotPage0,   nullptr },
    { L"page_motion",      ShotPage1,   nullptr },
    { L"page_preferences", ShotPage2,   nullptr },
    { L"page_hud",         ShotPage3,   nullptr },
    { L"page_settings",    ShotPage4,   nullptr },
    { L"palette",          ShotPalette, TypeQuery },
    { L"compact",          ShotCompact, nullptr },
    { L"accent_0",         ShotAccent0, nullptr },
    { L"accent_1",         ShotAccent1, nullptr },
    { L"accent_2",         ShotAccent2, nullptr },
    { L"accent_3",         ShotAccent3, nullptr },
    { L"hud_overlays",     ShotHud,     nullptr },
};

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmdLine, int) {
    static wchar_t shotDir[MAX_PATH] = {};
    const bool shooting = cmdLine && wcsncmp(cmdLine, L"--shot ", 7) == 0;
    if (shooting) {
        const wchar_t* s = cmdLine + 7;
        while (*s == L' ' || *s == L'"') s++;
        wcsncpy_s(shotDir, s, _TRUNCATE);
        size_t n = wcslen(shotDir);
        while (n && (shotDir[n - 1] == L' ' || shotDir[n - 1] == L'"' || shotDir[n - 1] == L'\\')) shotDir[--n] = 0;
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr,
                       LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"GlassMenuDemo", nullptr };
    RegisterClassExW(&wc);
    RECT wr = { 0, 0, 1600, 900 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Glass Menu - INSERT to open",
        WS_OVERLAPPEDWINDOW, 60, 40, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, hInst, nullptr);
    if (!CreateDevice(hwnd)) { MessageBoxW(nullptr, L"D3D11 init failed", L"Glass Menu", MB_ICONERROR); return 1; }
    ShowWindow(hwnd, shooting ? SW_SHOWNOACTIVATE : SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ApplyTheme();
    UIConfig_SetFolder("GlassMenuDemo");
    UI_SetBrand("GLASS", "MENU");
    UI_SetStatus("ONLINE");
    UI_SetNavigation(kNav, IM_ARRAYSIZE(kNav));
    UI_LoadFonts();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_Device, g_Context);
    UIGfx_Init(g_Device, g_Context);
    RegisterDemoSettings();

    if (shooting) {
        if (g_UI.menuW < 760.0f) { g_UI.menuW = 1080.0f; g_UI.menuH = 680.0f; }
        g_Hud.info = true;
        g_Hud.chStyle = 0;
        Hud_InputDemo(true);
        const int hl = Registry_IndexOf(&demo::highlight.on);
        Hotkey_Set(hl, VK_F4);
        Hotkey_SetMode(hl, HotkeyMode::Hold);
        Hotkey_Set(Registry_IndexOf(&demo::marker.on), 'M');
        Hotkey_Set(Registry_IndexOf(&demo::preferences.nightMode), 'N');
        demo::motion.on = true;
    }
    int shotFrame = 0;
    int shotIndex = -1;

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (g_ResizeW && g_ResizeH) {
            CleanupRTV();
            g_Swap->ResizeBuffers(0, g_ResizeW, g_ResizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeW = g_ResizeH = 0;
            CreateRTV();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!UIGfx_BlurReady()) {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            const ImVec2 ds = ImGui::GetIO().DisplaySize;
            bg->AddRectFilledMultiColor(ImVec2(0, 0), ds,
                IM_COL32(26, 28, 36, 255), IM_COL32(38, 32, 48, 255),
                IM_COL32(18, 20, 28, 255), IM_COL32(28, 24, 36, 255));
        }

        const OverlayItem active[] = {
            { "Highlight",   "WORKSPACE", demo::highlight.on,   &demo::highlight.on },
            { "Marker",      "WORKSPACE", demo::marker.on,      &demo::marker.on },
            { "Motion",      "WORKSPACE", demo::motion.on,      &demo::motion.on },
            { "Night mode",  "INTERFACE", demo::preferences.nightMode, &demo::preferences.nightMode },
            { "Quiet mode",  "INTERFACE", demo::preferences.quiet,     &demo::preferences.quiet },
        };
        UI_RenderOverlay(active, IM_ARRAYSIZE(active));
        UI_RenderMenu();

        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const float clear[4] = { 0, 0, 0, 1 };
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        g_Context->ClearRenderTargetView(g_RTV, clear);
        UIGfx_DrawTestScene(g_RTV, ds.x, ds.y, (float)ImGui::GetTime());
        UIGfx_Process(g_Swap);
        ImGui::Render();
        g_Context->OMSetRenderTargets(1, &g_RTV, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (shooting) {
            const int perShot = 60;
            if (shotFrame % perShot == 0) {
                if (shotIndex >= 0) {
                    wchar_t path[MAX_PATH];
                    swprintf_s(path, L"%ls\\%ls.bmp", shotDir, kShots[shotIndex].name);
                    SaveBackbufferBmp(path);
                }
                shotIndex++;
                if (shotIndex >= (int)IM_ARRAYSIZE(kShots)) running = false;
                else kShots[shotIndex].setup();
            } else if (shotFrame % perShot == 12 && shotIndex >= 0 && kShots[shotIndex].late) {
                kShots[shotIndex].late();
            }
            shotFrame++;
        }
        g_Swap->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupRTV();
    if (g_Swap)    g_Swap->Release();
    if (g_Context) g_Context->Release();
    if (g_Device)  g_Device->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}
