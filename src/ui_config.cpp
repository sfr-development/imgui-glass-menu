#include "ui_config.h"
#include "ui_anim.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

UISettings g_UI;

static char  s_Path[MAX_PATH] = {};
static char  s_Folder[64]     = "GlassMenu";

const char* UIConfig_Folder() { return s_Folder; }

void UIConfig_SetFolder(const char* folderName) {
    snprintf(s_Folder, sizeof(s_Folder), "%s", folderName);
    s_Path[0] = 0;
}
static bool  s_Dirty          = false;
static DWORD s_DirtyTick      = 0;

const char* UIConfig_Path() {
    if (!s_Path[0]) {
        char appdata[MAX_PATH] = {};
        if (!GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH))
            GetTempPathA(MAX_PATH, appdata);
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\%s", appdata, s_Folder);
        CreateDirectoryA(dir, nullptr);
        snprintf(s_Path, sizeof(s_Path), "%s\\ui.cfg", dir);
    }
    return s_Path;
}

static bool ParseBool(const char* v) { return v[0] == '1' || v[0] == 't' || v[0] == 'T'; }

void UIConfig_Load() {
    FILE* f = nullptr;
    if (fopen_s(&f, UIConfig_Path(), "r") != 0 || !f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* k = line;
        char* v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if      (!strcmp(k, "sounds"))        g_UI.sounds        = ParseBool(v);
        else if (!strcmp(k, "volume"))        g_UI.volume        = (float)atof(v);
        else if (!strcmp(k, "hoverSounds"))   g_UI.hoverSounds   = ParseBool(v);
        else if (!strcmp(k, "accent"))        g_UI.accent        = atoi(v);
        else if (!strcmp(k, "accentCustom"))  sscanf_s(v, "%f,%f,%f", &g_UI.accentCustom[0], &g_UI.accentCustom[1], &g_UI.accentCustom[2]);
        else if (!strcmp(k, "roundness"))     g_UI.roundness     = atoi(v);
        else if (!strcmp(k, "glass"))         g_UI.glass         = ParseBool(v);
        else if (!strcmp(k, "solidity"))      g_UI.solidity      = (float)atof(v);
        else if (!strcmp(k, "dim"))           g_UI.dim           = (float)atof(v);
        else if (!strcmp(k, "animSpeed"))     g_UI.animSpeed     = atoi(v);
        else if (!strcmp(k, "overlay"))       g_UI.overlay       = ParseBool(v);
        else if (!strcmp(k, "overlayLocked")) g_UI.overlayLocked = ParseBool(v);
        else if (!strcmp(k, "overlayX"))      g_UI.overlayX      = (float)atof(v);
        else if (!strcmp(k, "overlayY"))      g_UI.overlayY      = (float)atof(v);
        else if (!strcmp(k, "blockInput"))    g_BlockInput       = ParseBool(v);
        else if (!strcmp(k, "sidebarCollapsed")) g_UI.sidebarCollapsed = ParseBool(v);
        else if (!strcmp(k, "hideMiscNotice"))   g_UI.hideMiscNotice   = ParseBool(v);
        else if (!strcmp(k, "shareLook"))        g_UI.shareLook        = ParseBool(v);
        else if (!strcmp(k, "menuRect"))         sscanf_s(v, "%f,%f,%f,%f", &g_UI.menuX, &g_UI.menuY, &g_UI.menuW, &g_UI.menuH);
        else if (!strcmp(k, "profile") && v[0])  snprintf(g_UI.profile, sizeof(g_UI.profile), "%s", v);
    }
    fclose(f);

    if (g_UI.volume < 0.0f) g_UI.volume = 0.0f;
    if (g_UI.volume > 1.0f) g_UI.volume = 1.0f;
    if (g_UI.accent < 0 || g_UI.accent > 4) g_UI.accent = 0;
    if (g_UI.roundness < 0 || g_UI.roundness > 2) g_UI.roundness = 1;
    if (g_UI.animSpeed < 0 || g_UI.animSpeed > 3) g_UI.animSpeed = 2;
    for (float& c : g_UI.accentCustom) c = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
    g_UI.accentCustom[3] = 1.0f;
    g_UI.solidity = g_UI.solidity < 0.0f ? 0.0f : g_UI.solidity > 1.0f ? 1.0f : g_UI.solidity;
    g_UI.dim      = g_UI.dim < 0.0f ? 0.0f : g_UI.dim > 1.0f ? 1.0f : g_UI.dim;
}

void UIConfig_Save() {
    FILE* f = nullptr;
    if (fopen_s(&f, UIConfig_Path(), "w") != 0 || !f) return;
    fprintf(f, "sounds=%d\n",        g_UI.sounds ? 1 : 0);
    fprintf(f, "volume=%.3f\n",      g_UI.volume);
    fprintf(f, "hoverSounds=%d\n",   g_UI.hoverSounds ? 1 : 0);
    fprintf(f, "accent=%d\n",        g_UI.accent);
    fprintf(f, "accentCustom=%.3f,%.3f,%.3f\n", g_UI.accentCustom[0], g_UI.accentCustom[1], g_UI.accentCustom[2]);
    fprintf(f, "roundness=%d\n",     g_UI.roundness);
    fprintf(f, "glass=%d\n",         g_UI.glass ? 1 : 0);
    fprintf(f, "solidity=%.3f\n",    g_UI.solidity);
    fprintf(f, "dim=%.3f\n",         g_UI.dim);
    fprintf(f, "animSpeed=%d\n",     g_UI.animSpeed);
    fprintf(f, "overlay=%d\n",       g_UI.overlay ? 1 : 0);
    fprintf(f, "overlayLocked=%d\n", g_UI.overlayLocked ? 1 : 0);
    fprintf(f, "overlayX=%.4f\n",    g_UI.overlayX);
    fprintf(f, "overlayY=%.4f\n",    g_UI.overlayY);
    fprintf(f, "blockInput=%d\n",    g_BlockInput ? 1 : 0);
    fprintf(f, "sidebarCollapsed=%d\n", g_UI.sidebarCollapsed ? 1 : 0);
    fprintf(f, "hideMiscNotice=%d\n",   g_UI.hideMiscNotice ? 1 : 0);
    fprintf(f, "shareLook=%d\n",        g_UI.shareLook ? 1 : 0);
    fprintf(f, "menuRect=%.0f,%.0f,%.0f,%.0f\n", g_UI.menuX, g_UI.menuY, g_UI.menuW, g_UI.menuH);
    fprintf(f, "profile=%s\n",       g_UI.profile);
    fclose(f);
    s_Dirty = false;
}

void UIConfig_MarkDirty() {
    s_Dirty     = true;
    s_DirtyTick = GetTickCount();
}

void UIConfig_Tick() {
    if (s_Dirty && GetTickCount() - s_DirtyTick > 600) UIConfig_Save();
}
