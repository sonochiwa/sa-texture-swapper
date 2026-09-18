#pragma once

#include <windows.h>

#include <string>

struct Config {
    bool isEnabled      = true;
    bool log            = false;
    bool hotReload      = true;

    // Absolute paths, resolved at load time. rootDir is always <game>\swapper.
    std::wstring gameDir;
    std::wstring rootDir;
    std::wstring logPath;
};

// Reads <dll dir>\TextureSwapper.ini, creating it from the canonical template when
// it is missing.
Config LoadConfig(HMODULE module, const std::wstring& dllPath);
