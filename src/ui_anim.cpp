#include "ui_anim.h"
#include "ui_sound.h"
#include "ui_kit.h"

#include <cmath>

MenuAnim g_MenuAnim = { MenuAnimState::Opening, 0.0f, 0.0f, 0.22f, 0.20f };
bool     g_ShowMenu   = true;
bool     g_BlockInput = true;

static inline float EaseOutCubic(float x) {
    float u = 1.0f - x;
    return 1.0f - u * u * u;
}
static inline float SmoothStep(float x) { return x * x * (3.0f - 2.0f * x); }

static float InvEaseOutCubic(float a) { return 1.0f - cbrtf(1.0f - a); }
static float InvSmoothStep(float y) {
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 20; i++) { const float m = (lo + hi) * 0.5f; (SmoothStep(m) < y ? lo : hi) = m; }
    return (lo + hi) * 0.5f;
}

bool MenuAnim_WantsInput() {
    return g_MenuAnim.state == MenuAnimState::Open ||
           g_MenuAnim.state == MenuAnimState::Opening;
}

bool MenuAnim_IsVisible() {
    return g_MenuAnim.state != MenuAnimState::Closed || g_MenuAnim.alpha > 0.001f;
}

void MenuAnim_Toggle() {
    switch (g_MenuAnim.state) {
        case MenuAnimState::Closed:
        case MenuAnimState::Closing:
            g_MenuAnim.t = InvEaseOutCubic(g_MenuAnim.alpha);
            g_MenuAnim.state = MenuAnimState::Opening;
            g_ShowMenu = true;
            UISound_Play(Sfx::Open);
            break;
        case MenuAnimState::Open:
        case MenuAnimState::Opening:
            g_MenuAnim.t = InvSmoothStep(1.0f - g_MenuAnim.alpha);
            g_MenuAnim.state = MenuAnimState::Closing;
            UISound_Play(Sfx::Close);
            break;
    }
}

void MenuAnim_Update() {
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;
    if (dt < 0.0f)         dt = 0.0f;
    dt *= UI::g_AnimScale;
    switch (g_MenuAnim.state) {
        case MenuAnimState::Closed:
            g_MenuAnim.alpha = 0.0f;
            return;
        case MenuAnimState::Opening:
            g_MenuAnim.t += dt / g_MenuAnim.openDuration;
            if (g_MenuAnim.t >= 1.0f) {
                g_MenuAnim.t = 1.0f;
                g_MenuAnim.alpha = 1.0f;
                g_MenuAnim.state = MenuAnimState::Open;
            } else {
                g_MenuAnim.alpha = EaseOutCubic(g_MenuAnim.t);
            }
            break;
        case MenuAnimState::Open:
            g_MenuAnim.alpha = 1.0f;
            break;
        case MenuAnimState::Closing:
            g_MenuAnim.t += dt / g_MenuAnim.closeDuration;
            if (g_MenuAnim.t >= 1.0f) {
                g_MenuAnim.t = 0.0f;
                g_MenuAnim.alpha = 0.0f;
                g_MenuAnim.state = MenuAnimState::Closed;
                g_ShowMenu = false;
            } else {
                g_MenuAnim.alpha = 1.0f - SmoothStep(g_MenuAnim.t);
            }
            break;
    }
}
