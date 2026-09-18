#include "config.h"

#include "resource.h"

#include <windows.h>

namespace {

// The textures folder is not configurable: one fixed place next to gta_sa.exe means
// a reader of any guide, log or forum post is looking at the same path.
constexpr wchar_t kTexturesFolder[] = L"swapper";

std::wstring DirOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& tail) {
    if (tail.empty())
        return base;
    if (base.empty())
        return tail;
    return base + L"\\" + tail;
}

int ReadInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int def) {
    return static_cast<int>(GetPrivateProfileIntW(section, key, def, ini.c_str()));
}

// Writes the RCDATA copy of Config\TextureSwapper.ini byte for byte.
void WriteDefaultIni(HMODULE module, const std::wstring& ini) {
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_DEFAULT_INI), RT_RCDATA);
    if (!resource)
        return;
    const HGLOBAL handle = LoadResource(module, resource);
    const DWORD size = SizeofResource(module, resource);
    const void* data = handle ? LockResource(handle) : nullptr;
    if (!data || size == 0)
        return;

    HANDLE file = CreateFileW(ini.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
}

} // namespace

Config LoadConfig(HMODULE module, const std::wstring& dllPath) {
    Config cfg;

    const std::wstring dllDir = DirOf(dllPath);
    const std::wstring ini    = JoinPath(dllDir, L"TextureSwapper.ini");
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        WriteDefaultIni(module, ini);

    cfg.isEnabled      = ReadInt(ini, L"general", L"isEnabled", 1) != 0;
    cfg.log            = ReadInt(ini, L"general", L"log", 0) != 0;
    cfg.hotReload      = ReadInt(ini, L"general", L"hotReload", 1) != 0;

    // The folder follows the game, not the plugin: the .asi may sit in scripts\ while
    // the textures always live beside the executable.
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    cfg.gameDir = DirOf(exe);
    cfg.rootDir = JoinPath(cfg.gameDir, kTexturesFolder);
    cfg.logPath = JoinPath(dllDir, L"TextureSwapper.log");

    return cfg;
}
