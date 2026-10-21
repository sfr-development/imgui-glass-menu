#pragma once
#include "imgui/imgui.h"

struct HudConfig {
    bool  crosshair      = false;
    int   chStyle        = 0;
    float chSize         = 7.0f;
    float chGap          = 4.0f;
    float chThick        = 1.5f;
    bool  chOutline      = true;
    bool  chDot          = false;
    float chColor[4]     = { 0.45f, 1.00f, 0.60f, 1.00f };

    bool  info           = false;
    bool  infoFps        = true;
    bool  infoClock      = true;
    bool  infoProfile    = true;
    bool  infoActive     = true;
    float infoX          = 0.0f;
    float infoY          = 0.0f;

    bool  input          = false;
    bool  inputKeys      = true;
    bool  inputMouse     = true;
    bool  inputStats     = true;
    float inputScale     = 1.0f;
    float inputX         = 1.0f;
    float inputY         = 1.0f;

    float overlayShadow  = 0.0f;
    float infoShadow     = 0.0f;
    float inputShadow    = 0.0f;
};
extern HudConfig g_Hud;

void Hud_RenderCrosshair();
void Hud_RenderInfoBar(int activeCount);
void Hud_RenderInputOverlay();
void Hud_InputWheel(float delta);
void Hud_InputDemo(bool on);
void Hud_InputCard();
