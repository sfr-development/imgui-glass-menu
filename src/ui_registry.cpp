#include "ui_registry.h"
#include "ui_config.h"
#include "ui_sound.h"
#include "ui_toast.h"
#include "ui_theme.h"
#include "imgui/imgui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

static const int kMaxFields   = 256;
static const int kMaxProfiles = 32;

static const Field* s_Fields = nullptr;
static int          s_Count  = 0;
static int          s_Hotkey[kMaxFields] = {};
static int          s_HotkeyMode[kMaxFields] = {};
static bool         s_KeyPrev[kMaxFields] = {};
static bool         s_HoldOrig[kMaxFields] = {};
static unsigned char s_Default[kMaxFields][16];

static char s_Profiles[kMaxProfiles][32];
static int  s_ProfileCount = 0;

static unsigned s_SavedHash  = 0;
static unsigned s_LastHash   = 0;
static DWORD    s_ChangeTick = 0;
static DWORD    s_CheckTick  = 0;

int          Registry_Count()     { return s_Count; }
const Field* Registry_Get(int i)  { return (i >= 0 && i < s_Count) ? &s_Fields[i] : nullptr; }

int Registry_IndexOf(const void* ptr) {
    for (int i = 0; i < s_Count; i++)
        if (s_Fields[i].ptr == ptr) return i;
    return -1;
}

static size_t FieldSize(FieldType t) {
    switch (t) {
        case FieldType::Bool:  return sizeof(bool);
        case FieldType::Float: return sizeof(float);
        case FieldType::Int:   return sizeof(int);
        case FieldType::Color: return sizeof(float) * 4;
    }
    return 0;
}

static unsigned StateHash() {
    unsigned h = 2166136261u;
    auto mix = [&](const void* p, size_t n) {
        const unsigned char* b = (const unsigned char*)p;
        for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    };
    for (int i = 0; i < s_Count; i++) mix(s_Fields[i].ptr, FieldSize(s_Fields[i].type));
    mix(s_Hotkey, sizeof(int) * s_Count);
    mix(s_HotkeyMode, sizeof(int) * s_Count);
    return h;
}

bool Registry_IsModified(int i) {
    if (i < 0 || i >= s_Count) return false;
    const Field& f = s_Fields[i];
    if (f.type == FieldType::Float) return fabsf(*(const float*)f.ptr - *(const float*)s_Default[i]) > 1e-4f;
    if (f.type == FieldType::Color) {
        const float* a = (const float*)f.ptr;
        const float* b = (const float*)s_Default[i];
        for (int k = 0; k < 4; k++) if (fabsf(a[k] - b[k]) > 1e-3f) return true;
        return false;
    }
    return memcmp(f.ptr, s_Default[i], FieldSize(f.type)) != 0;
}

void Registry_Reset(int i) {
    if (i < 0 || i >= s_Count) return;
    memcpy(s_Fields[i].ptr, s_Default[i], FieldSize(s_Fields[i].type));
}

static void ProfilesDir(char* out, size_t n) {
    char base[MAX_PATH];
    snprintf(base, sizeof(base), "%s", UIConfig_Path());
    char* slash = strrchr(base, '\\');
    if (slash) *slash = 0;
    snprintf(out, n, "%s\\profiles", base);
    CreateDirectoryA(out, nullptr);
}

static void ProfilePath(const char* name, char* out, size_t n) {
    char dir[MAX_PATH];
    ProfilesDir(dir, sizeof(dir));
    snprintf(out, n, "%s\\%s.cfg", dir, name);
}

const char* Profiles_Folder() {
    static char dir[MAX_PATH];
    ProfilesDir(dir, sizeof(dir));
    return dir;
}

static void ScanProfiles() {
    s_ProfileCount = 0;
    char dir[MAX_PATH], pattern[MAX_PATH];
    ProfilesDir(dir, sizeof(dir));
    snprintf(pattern, sizeof(pattern), "%s\\*.cfg", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (s_ProfileCount >= kMaxProfiles) break;
        char name[64];
        snprintf(name, sizeof(name), "%s", fd.cFileName);
        char* dot = strrchr(name, '.');
        if (dot) *dot = 0;
        snprintf(s_Profiles[s_ProfileCount++], 32, "%s", name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void Profiles_Rescan() { ScanProfiles(); }

static bool IsUiKey(const char* key) { return strncmp(key, "ui.", 3) == 0; }

static std::string ProfileText(bool onlyChanged, bool includeUi) {
    std::string s;
    char line[256];
    for (int i = 0; i < s_Count; i++) {
        const Field& fd = s_Fields[i];
        if (!includeUi && IsUiKey(fd.key)) continue;
        if (!onlyChanged || Registry_IsModified(i)) {
            switch (fd.type) {
                case FieldType::Bool:  snprintf(line, sizeof(line), "%s=%d\n", fd.key, *(bool*)fd.ptr ? 1 : 0); break;
                case FieldType::Float: snprintf(line, sizeof(line), "%s=%.6g\n", fd.key, *(float*)fd.ptr); break;
                case FieldType::Int:   snprintf(line, sizeof(line), "%s=%d\n", fd.key, *(int*)fd.ptr); break;
                case FieldType::Color: {
                    const float* c = (const float*)fd.ptr;
                    snprintf(line, sizeof(line), "%s=%.3g,%.3g,%.3g,%.3g\n", fd.key, c[0], c[1], c[2], c[3]);
                    break;
                }
            }
            s += line;
        }
        if (s_Hotkey[i]) { snprintf(line, sizeof(line), "hotkey.%s=%d\n", fd.key, s_Hotkey[i]); s += line; }
        if (s_Hotkey[i] && s_HotkeyMode[i]) { snprintf(line, sizeof(line), "hotkeymode.%s=%d\n", fd.key, s_HotkeyMode[i]); s += line; }
    }
    return s;
}

static bool WriteProfile(const char* name) {
    char path[MAX_PATH];
    ProfilePath(name, path, sizeof(path));
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;
    const std::string text = ProfileText(false, true);
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    s_SavedHash = s_LastHash = StateHash();
    return true;
}

static bool ApplyLine(char* line, bool includeUi) {
    char* eq = strchr(line, '=');
    if (!eq) return false;
    *eq = 0;
    char* v = eq + 1;
    v[strcspn(v, "\r\n")] = 0;
    const bool isMode   = strncmp(line, "hotkeymode.", 11) == 0;
    const bool isHotkey = !isMode && strncmp(line, "hotkey.", 7) == 0;
    const char* key = isMode ? line + 11 : isHotkey ? line + 7 : line;
    if (!includeUi && IsUiKey(key)) return false;
    for (int i = 0; i < s_Count; i++) {
        const Field& fd = s_Fields[i];
        if (strcmp(fd.key, key) != 0) continue;
        if (isHotkey) { const int vk = atoi(v); s_Hotkey[i] = (vk > 0 && vk < 256) ? vk : 0; return true; }
        if (isMode)   { s_HotkeyMode[i] = atoi(v) == 1 ? 1 : 0; return true; }
        switch (fd.type) {
            case FieldType::Bool:  *(bool*)fd.ptr = atoi(v) != 0; break;
            case FieldType::Int:   *(int*)fd.ptr  = atoi(v); break;
            case FieldType::Float: {
                const float x = (float)atof(v);
                if (std::isfinite(x)) *(float*)fd.ptr = x;
                break;
            }
            case FieldType::Color: {
                float c[4] = { 1, 1, 1, 1 };
                if (sscanf_s(v, "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]) >= 3) {
                    float* out = (float*)fd.ptr;
                    for (int k = 0; k < 4; k++) out[k] = std::isfinite(c[k]) ? (c[k] < 0 ? 0 : c[k] > 1 ? 1 : c[k]) : 1.0f;
                }
                break;
            }
        }
        return true;
    }
    return false;
}

static bool ReadProfile(const char* name) {
    char path[MAX_PATH];
    ProfilePath(name, path, sizeof(path));
    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") != 0 || !f) return false;
    memset(s_Hotkey, 0, sizeof(s_Hotkey));
    memset(s_HotkeyMode, 0, sizeof(s_HotkeyMode));
    char line[256];
    while (fgets(line, sizeof(line), f)) ApplyLine(line, true);
    fclose(f);
    s_SavedHash = s_LastHash = StateHash();
    return true;
}

int         Profiles_Count()      { return s_ProfileCount; }
const char* Profiles_Name(int i)  { return (i >= 0 && i < s_ProfileCount) ? s_Profiles[i] : ""; }
const char* Profiles_Active()     { return g_UI.profile; }

bool Profiles_Load(const char* name) {
    if (!ReadProfile(name)) return false;
    snprintf(g_UI.profile, sizeof(g_UI.profile), "%s", name);
    UIConfig_MarkDirty();
    UI_Toast(ICON_PROFILE, name, "profile loaded", Theme::Accent);
    return true;
}

bool Profiles_Save() {
    const bool ok = WriteProfile(g_UI.profile);
    if (ok) UI_Toast(ICON_SAVED, g_UI.profile, "saved", Theme::Success);
    return ok;
}

static bool ValidName(const char* name) {
    if (!name || !*name) return false;
    for (const char* p = name; *p; p++)
        if (strchr("\\/:*?\"<>|", *p) || (unsigned char)*p < 32) return false;
    return true;
}

bool Profiles_Create(const char* name) {
    if (!ValidName(name)) return false;
    snprintf(g_UI.profile, sizeof(g_UI.profile), "%s", name);
    UIConfig_MarkDirty();
    const bool ok = WriteProfile(name);
    ScanProfiles();
    if (ok) UI_Toast(ICON_PROFILE, name, "profile created", Theme::Accent);
    return ok;
}

bool Profiles_Delete(const char* name) {
    if (s_ProfileCount <= 1) return false;
    char path[MAX_PATH];
    ProfilePath(name, path, sizeof(path));
    if (!DeleteFileA(path)) return false;
    ScanProfiles();
    if (!strcmp(name, g_UI.profile)) Profiles_Load(s_Profiles[0]);
    UI_Toast(ICON_DELETED, name, "profile deleted", Theme::Danger);
    return true;
}

void Profiles_Tick() {
    const DWORD now = GetTickCount();
    if (now - s_CheckTick < 250) return;
    s_CheckTick = now;
    const unsigned h = StateHash();
    if (h != s_LastHash) { s_LastHash = h; s_ChangeTick = now; }
    if (h != s_SavedHash && now - s_ChangeTick > 1000) WriteProfile(g_UI.profile);
}

static const char  kCodePrefix[] = "GLMK1:";
static const char  kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::string& in) {
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        const unsigned n = ((unsigned char)in[i] << 16) |
                           (i + 1 < in.size() ? (unsigned char)in[i + 1] << 8 : 0) |
                           (i + 2 < in.size() ? (unsigned char)in[i + 2] : 0);
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += i + 1 < in.size() ? kB64[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? kB64[n & 63] : '=';
    }
    return out;
}

static bool Base64Decode(const char* in, std::string& out) {
    unsigned acc = 0;
    int bits = 0;
    for (const char* p = in; *p; p++) {
        const char c = *p;
        if (c == '=' || c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        const char* pos = strchr(kB64, c);
        if (!pos || !c) return false;
        acc = (acc << 6) | (unsigned)(pos - kB64);
        bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((acc >> bits) & 0xFF); }
    }
    return true;
}

std::string Profiles_ShareCode(bool includeUi) {
    std::string text = "name=";
    text += g_UI.profile;
    text += "\n";
    text += ProfileText(true, includeUi);
    return std::string(kCodePrefix) + Base64Encode(text);
}

bool Profiles_ExportFile(bool includeUi, char* outPath, size_t cap) {
    char dir[MAX_PATH], base[MAX_PATH];
    snprintf(base, sizeof(base), "%s", UIConfig_Path());
    char* slash = strrchr(base, '\\');
    if (slash) *slash = 0;
    snprintf(dir, sizeof(dir), "%s\\exports", base);
    CreateDirectoryA(dir, nullptr);
    snprintf(outPath, cap, "%s\\%s.cfg", dir, g_UI.profile);
    FILE* f = nullptr;
    if (fopen_s(&f, outPath, "w") != 0 || !f) return false;
    std::string text = "# Glass Menu config - drop into the profiles folder or use Import file\n";
    text += "name=";
    text += g_UI.profile;
    text += "\n";
    text += ProfileText(false, includeUi);
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return true;
}

ImportResult Profiles_Import(const char* input, const char* nameHint, bool includeUi) {
    ImportResult r;
    if (!input) return r;
    while (*input == ' ' || *input == '\r' || *input == '\n' || *input == '\t') input++;

    std::string text;
    if (strncmp(input, kCodePrefix, sizeof(kCodePrefix) - 1) == 0) {
        if (!Base64Decode(input + sizeof(kCodePrefix) - 1, text)) { snprintf(r.error, sizeof(r.error), "broken share code"); return r; }
    } else {
        text = input;
    }
    if (text.size() > 256 * 1024) { snprintf(r.error, sizeof(r.error), "text too large"); return r; }

    char name[32] = "";
    int known = 0;
    {
        size_t pos = 0;
        while (pos < text.size()) {
            const size_t end = text.find('\n', pos);
            std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? text.size() : end + 1;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            if (key == "name") {
                std::string v = line.substr(eq + 1);
                while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
                snprintf(name, sizeof(name), "%s", v.c_str());
                continue;
            }
            if (key.rfind("hotkeymode.", 0) == 0) key = key.substr(11);
            else if (key.rfind("hotkey.", 0) == 0) key = key.substr(7);
            for (int i = 0; i < s_Count; i++)
                if (key == s_Fields[i].key) { known++; break; }
        }
    }
    if (known == 0) { snprintf(r.error, sizeof(r.error), "no settings found"); return r; }

    if (StateHash() != s_SavedHash) WriteProfile(g_UI.profile);

    for (int i = 0; i < s_Count; i++) {
        if (!includeUi && IsUiKey(s_Fields[i].key)) continue;
        Registry_Reset(i);
        s_Hotkey[i] = 0;
        s_HotkeyMode[i] = 0;
    }
    {
        size_t pos = 0;
        char line[256];
        while (pos < text.size()) {
            const size_t end = text.find('\n', pos);
            const size_t len = (end == std::string::npos ? text.size() : end) - pos;
            snprintf(line, sizeof(line), "%.*s", (int)(len < sizeof(line) - 1 ? len : sizeof(line) - 1), text.c_str() + pos);
            pos = end == std::string::npos ? text.size() : end + 1;
            if (strncmp(line, "name=", 5) == 0) continue;
            if (ApplyLine(line, includeUi)) r.applied++;
        }
    }

    char base[32];
    snprintf(base, sizeof(base), "%s", ValidName(name) ? name : (ValidName(nameHint) ? nameHint : "Imported"));
    {
        char* sp = strrchr(base, ' ');
        if (sp && sp != base && sp[1]) {
            bool digits = true;
            for (const char* d = sp + 1; *d; d++) if (*d < '0' || *d > '9') { digits = false; break; }
            if (digits) *sp = 0;
        }
    }
    snprintf(r.name, sizeof(r.name), "%s", base);
    for (int n = 2; n < 100; n++) {
        bool taken = false;
        for (int i = 0; i < s_ProfileCount; i++)
            if (_stricmp(s_Profiles[i], r.name) == 0) { taken = true; break; }
        if (!taken) break;
        snprintf(r.name, sizeof(r.name), "%.24s %d", base, n);
    }
    snprintf(g_UI.profile, sizeof(g_UI.profile), "%s", r.name);
    UIConfig_MarkDirty();
    r.ok = WriteProfile(r.name);
    ScanProfiles();
    if (!r.ok) snprintf(r.error, sizeof(r.error), "could not write the profile");
    return r;
}

void Registry_Init(const Field* fields, int count) {
    s_Fields = fields;
    s_Count  = count > kMaxFields ? kMaxFields : count;
    for (int i = 0; i < s_Count; i++)
        memcpy(s_Default[i], s_Fields[i].def ? s_Fields[i].def : s_Fields[i].ptr, FieldSize(s_Fields[i].type));
    if (!g_UI.profile[0]) snprintf(g_UI.profile, sizeof(g_UI.profile), "Default");
    ScanProfiles();
    if (!ReadProfile(g_UI.profile)) {
        WriteProfile(g_UI.profile);
        ScanProfiles();
    }
}

static int  s_Capture        = -1;
static int* s_CaptureTarget  = nullptr;
static bool s_CaptureArmed   = false;

int  Hotkey_Get(int i)            { return (i >= 0 && i < s_Count) ? s_Hotkey[i] : 0; }
void Hotkey_Set(int i, int vk)    { if (i >= 0 && i < s_Count) s_Hotkey[i] = vk; }
int  Hotkey_CapturingIndex()      { return s_Capture; }
void Hotkey_BeginCapture(int i)   { s_Capture = i; s_CaptureTarget = nullptr; s_CaptureArmed = false; }

HotkeyMode Hotkey_GetMode(int i)  { return (i >= 0 && i < s_Count && s_HotkeyMode[i] == 1) ? HotkeyMode::Hold : HotkeyMode::Toggle; }
void Hotkey_SetMode(int i, HotkeyMode m) {
    if (i < 0 || i >= s_Count) return;
    if (s_HotkeyMode[i] == 1 && m == HotkeyMode::Toggle && s_KeyPrev[i] && s_Fields[i].type == FieldType::Bool)
        *(bool*)s_Fields[i].ptr = s_HoldOrig[i];
    s_HotkeyMode[i] = (int)m;
}

void KeyCapture_Begin(int* target)  { s_CaptureTarget = target; s_Capture = -1; s_CaptureArmed = false; }
void KeyCapture_Cancel()            { s_CaptureTarget = nullptr; }
bool KeyCapture_Active(const int* target) {
    return target ? s_CaptureTarget == target : (s_CaptureTarget != nullptr || s_Capture >= 0);
}

const char* Hotkey_Name(int vk) {    static char buf[4][24];
    static int  slot = 0;
    char* out = buf[slot = (slot + 1) & 3];
    switch (vk) {
        case 0:            return "";
        case VK_LBUTTON:   return "LMB";
        case VK_RBUTTON:   return "RMB";
        case VK_MBUTTON:   return "MMB";
        case VK_XBUTTON1:  return "MB4";
        case VK_XBUTTON2:  return "MB5";
        case VK_LSHIFT:    return "LSHIFT";
        case VK_RSHIFT:    return "RSHIFT";
        case VK_LCONTROL:  return "LCTRL";
        case VK_RCONTROL:  return "RCTRL";
        case VK_LMENU:     return "LALT";
        case VK_RMENU:     return "RALT";
        case VK_CAPITAL:   return "CAPS";
        case VK_TAB:       return "TAB";
        case VK_SPACE:     return "SPACE";
        case VK_HOME:      return "HOME";
        case VK_END:       return "END";
        case VK_PRIOR:     return "PGUP";
        case VK_NEXT:      return "PGDN";
        case VK_DELETE:    return "DEL";
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) { snprintf(out, 24, "%c", vk); return out; }
    if (vk >= VK_F1 && vk <= VK_F24)       { snprintf(out, 24, "F%d", vk - VK_F1 + 1); return out; }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { snprintf(out, 24, "NUM%d", vk - VK_NUMPAD0); return out; }
    const UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (sc && GetKeyNameTextA((LONG)(sc << 16), out, 24) > 0) {
        for (char* p = out; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        return out;
    }
    snprintf(out, 24, "VK%02X", vk);
    return out;
}

static bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

void Hotkeys_Poll() {
    if (s_Capture >= 0 || s_CaptureTarget) {
        bool any = false;
        for (int vk = 1; vk < 255; vk++) {
            if (vk == VK_LBUTTON || vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
            if (!KeyDown(vk)) continue;
            any = true;
            if (!s_CaptureArmed) break;
            if (s_CaptureTarget) {
                if (vk != VK_ESCAPE && vk != VK_INSERT) *s_CaptureTarget = vk;
                s_CaptureTarget = nullptr;
            } else {
                if (vk == VK_ESCAPE || vk == VK_BACK) s_Hotkey[s_Capture] = 0;
                else if (vk != VK_INSERT)             s_Hotkey[s_Capture] = vk;
                s_KeyPrev[s_Capture] = true;
                s_Capture = -1;
            }
            UISound_Play(vk == VK_ESCAPE ? Sfx::ToggleOff : Sfx::ToggleOn);
            break;
        }
        if (!any && !KeyDown(VK_LBUTTON)) s_CaptureArmed = true;
        return;
    }

    const bool typing = ImGui::GetIO().WantTextInput;
    for (int i = 0; i < s_Count; i++) {
        const int vk = s_Hotkey[i];
        if (!vk || s_Fields[i].type != FieldType::Bool) continue;
        bool* b = (bool*)s_Fields[i].ptr;
        const bool down = KeyDown(vk) && !typing;
        if (s_HotkeyMode[i] == 1) {
            if (down && !s_KeyPrev[i]) {
                s_HoldOrig[i] = *b;
                *b = !*b;
                UISound_Play(*b ? Sfx::ToggleOn : Sfx::ToggleOff);
            } else if (!down && s_KeyPrev[i]) {
                *b = s_HoldOrig[i];
            }
        } else if (down && !s_KeyPrev[i]) {
            *b = !*b;
            UISound_Play(*b ? Sfx::ToggleOn : Sfx::ToggleOff);
            UI_Toast(*b ? ICON_ON : ICON_OFF, s_Fields[i].label, *b ? "ON" : "OFF",
                     *b ? Theme::Accent : Theme::TextFaint);
        }
        s_KeyPrev[i] = down;
    }
}
