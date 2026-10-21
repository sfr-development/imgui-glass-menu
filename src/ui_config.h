#pragma once

struct UISettings {
    bool  sounds        = true;
    float volume        = 0.55f;
    bool  hoverSounds   = false;
    int   accent        = 0;
    float accentCustom[4] = { 0.36f, 0.78f, 0.74f, 1.0f };
    int   roundness     = 1;
    bool  glass         = true;
    float solidity      = 0.0f;
    float dim           = 1.0f;
    int   animSpeed     = 2;
    bool  overlay       = true;
    bool  overlayLocked = false;
    float overlayX      = 0.0f;
    float overlayY      = 0.28f;
    bool  sidebarCollapsed = false;
    bool  hideMiscNotice   = false;
    bool  shareLook        = true;
    float menuX = -1.0f, menuY = -1.0f;
    float menuW = -1.0f, menuH = -1.0f;
    char  profile[32]   = "Default";
};

extern UISettings g_UI;

void        UIConfig_Load();
void        UIConfig_Save();
void        UIConfig_MarkDirty();
void        UIConfig_Tick();
const char* UIConfig_Path();
void        UIConfig_SetFolder(const char* folderName);
const char* UIConfig_Folder();
