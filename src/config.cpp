#include "config.h"

#include <windows.h>

#include "generated/DefaultConfigIni.h"

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

// Writes the canonical configuration verbatim, so a generated INI is byte-for-byte
// identical to Config\TextureSwapper.ini.
void WriteDefaultIni(const std::wstring& ini) {
    HANDLE file = CreateFileW(ini.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(file, kDefaultConfigIni, static_cast<DWORD>(sizeof(kDefaultConfigIni)), &written,
              nullptr);
    CloseHandle(file);
}

} // namespace

Config LoadConfig(const std::wstring& dllPath) {
    Config cfg;

    const std::wstring dllDir = DirOf(dllPath);
    const std::wstring ini    = JoinPath(dllDir, L"TextureSwapper.ini");
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        WriteDefaultIni(ini);

    cfg.isEnabled      = ReadInt(ini, L"general", L"isEnabled", 1) != 0;
    cfg.loggingEnabled = ReadInt(ini, L"general", L"loggingEnabled", 0) != 0;
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
