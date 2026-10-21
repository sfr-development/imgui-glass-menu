#pragma once
#include <cstddef>
#include <string>

enum class FieldType { Bool, Float, Int, Color };

struct Field {
    const char* key;
    const char* label;
    int         tab;
    FieldType   type;
    void*       ptr;
    const void* def = nullptr;
};

void         Registry_Init(const Field* fields, int count);
int          Registry_Count();
const Field* Registry_Get(int i);
int          Registry_IndexOf(const void* ptr);

bool         Registry_IsModified(int i);
void         Registry_Reset(int i);

enum class HotkeyMode { Toggle = 0, Hold = 1 };
int         Hotkey_Get(int fieldIdx);
void        Hotkey_Set(int fieldIdx, int vk);
HotkeyMode  Hotkey_GetMode(int fieldIdx);
void        Hotkey_SetMode(int fieldIdx, HotkeyMode mode);
void        Hotkey_BeginCapture(int fieldIdx);
int         Hotkey_CapturingIndex();
const char* Hotkey_Name(int vk);
void        Hotkeys_Poll();

void        KeyCapture_Begin(int* target);
void        KeyCapture_Cancel();
bool        KeyCapture_Active(const int* target);

int         Profiles_Count();
const char* Profiles_Name(int i);
const char* Profiles_Active();
bool        Profiles_Load(const char* name);
bool        Profiles_Save();
bool        Profiles_Create(const char* name);
bool        Profiles_Delete(const char* name);
void        Profiles_Tick();
void        Profiles_Rescan();
const char* Profiles_Folder();

std::string Profiles_ShareCode(bool includeUi);
bool        Profiles_ExportFile(bool includeUi, char* outPath, size_t cap);
struct ImportResult { bool ok = false; int applied = 0; char name[32] = ""; char error[64] = ""; };
ImportResult Profiles_Import(const char* text, const char* nameHint, bool includeUi);
