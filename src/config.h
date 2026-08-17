#pragma once

#include <string>

struct Config {
    bool isEnabled      = true;
    bool loggingEnabled = false;
    bool hotReload      = true;

    // Absolute paths, resolved at load time. rootDir is always <game>\swapper.
    std::wstring gameDir;
    std::wstring rootDir;
    std::wstring logPath;
};

// Reads <dll dir>\TextureSwapper.ini, creating it from the canonical template when
// it is missing.
Config LoadConfig(const std::wstring& dllPath);
