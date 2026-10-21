#include "ui_sound.h"
#include "ui_config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstring>
#include <vector>

#pragma comment(lib, "winmm.lib")

static const int   kRate = 44100;
static const float kPi   = 3.14159265f;

static std::vector<unsigned char> s_Wav[(int)Sfx::Count_];
static float s_BuiltVolume  = -1.0f;
static bool  s_PlayedFrame  = false;
static unsigned s_Noise     = 0x9E3779B9u;

static float Noise() {
    s_Noise ^= s_Noise << 13; s_Noise ^= s_Noise >> 17; s_Noise ^= s_Noise << 5;
    return (s_Noise & 0xFFFF) / 32767.5f - 1.0f;
}

template <typename F>
static std::vector<unsigned char> Synth(float seconds, float gain, F f) {
    const int n = (int)(seconds * kRate);
    std::vector<unsigned char> wav(44 + n * 2);
    unsigned char* p = wav.data();
    auto u32 = [&](unsigned v) { memcpy(p, &v, 4); p += 4; };
    auto u16 = [&](unsigned short v) { memcpy(p, &v, 2); p += 2; };
    memcpy(p, "RIFF", 4); p += 4; u32(36 + n * 2);
    memcpy(p, "WAVE", 4); p += 4;
    memcpy(p, "fmt ", 4); p += 4; u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    memcpy(p, "data", 4); p += 4; u32(n * 2);
    for (int i = 0; i < n; i++) {
        const float t = (float)i / kRate;
        float s = f(t) * gain;
        const float tail = (seconds - t) / 0.002f;
        if (tail < 1.0f) s *= tail;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        u16((unsigned short)(short)(s * 32767.0f));
    }
    return wav;
}

static void Build(float vol) {
    const float g = 0.55f * vol;
    float phase = 0.0f;

    s_Wav[(int)Sfx::Click] = Synth(0.030f, g, [](float t) {
        return sinf(2 * kPi * 2100.0f * t) * expf(-t / 0.005f) * 0.7f
             + Noise() * expf(-t / 0.0012f) * 0.35f;
    });

    s_Wav[(int)Sfx::ToggleOn] = Synth(0.075f, g, [](float t) {
        const float f   = t < 0.028f ? 1250.0f : 1870.0f;
        const float t0  = t < 0.028f ? t : t - 0.028f;
        return sinf(2 * kPi * f * t) * expf(-t0 / 0.014f) * 0.75f;
    });

    s_Wav[(int)Sfx::ToggleOff] = Synth(0.075f, g, [](float t) {
        const float f   = t < 0.028f ? 1600.0f : 1050.0f;
        const float t0  = t < 0.028f ? t : t - 0.028f;
        return sinf(2 * kPi * f * t) * expf(-t0 / 0.014f) * 0.65f;
    });

    phase = 0.0f;
    s_Wav[(int)Sfx::Open] = Synth(0.17f, g, [&phase](float t) {
        const float d = 0.17f, f = 380.0f + (950.0f - 380.0f) * (t / d);
        phase += 2 * kPi * f / kRate;
        const float env = sinf(kPi * t / d);
        return (sinf(phase) * 0.8f + sinf(phase * 2.0f) * 0.15f) * env * env * 0.5f;
    });

    phase = 0.0f;
    s_Wav[(int)Sfx::Close] = Synth(0.14f, g, [&phase](float t) {
        const float d = 0.14f, f = 900.0f + (340.0f - 900.0f) * (t / d);
        phase += 2 * kPi * f / kRate;
        const float env = sinf(kPi * t / d);
        return (sinf(phase) * 0.8f + sinf(phase * 2.0f) * 0.15f) * env * env * 0.45f;
    });

    s_Wav[(int)Sfx::Hover] = Synth(0.012f, g, [](float t) {
        return sinf(2 * kPi * 3300.0f * t) * expf(-t / 0.0025f) * 0.22f;
    });

    s_BuiltVolume = vol;
}

static HANDLE        s_Event   = nullptr;
static volatile LONG s_Pending = -1;
static volatile LONG s_Volume  = 0;

static DWORD WINAPI SoundThread(LPVOID) {
    for (;;) {
        WaitForSingleObject(s_Event, INFINITE);
        const LONG s = InterlockedExchange(&s_Pending, -1);
        if (s < 0) continue;
        const float vol = s_Volume / 1000.0f;
        if (fabsf(s_BuiltVolume - vol) > 0.001f) {
            PlaySoundW(nullptr, nullptr, 0);
            Build(vol);
        }
        PlaySoundW((LPCWSTR)s_Wav[s].data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
}

void UISound_Play(Sfx s) {
    s_PlayedFrame = true;
    if (!g_UI.sounds || g_UI.volume <= 0.001f) return;
    if (s == Sfx::Hover && !g_UI.hoverSounds) return;
    if (!s_Event) {
        s_Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!s_Event) return;
        HANDLE t = CreateThread(nullptr, 0, SoundThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    InterlockedExchange(&s_Volume, (LONG)(g_UI.volume * 1000.0f));
    InterlockedExchange(&s_Pending, (LONG)s);
    SetEvent(s_Event);
}

void UISound_NewFrame()        { s_PlayedFrame = false; }
bool UISound_PlayedThisFrame() { return s_PlayedFrame; }
