// Texture Swapper replaces single textures inside GTA San Andreas texture
// dictionaries from loose PNG files while the game runs. Everything hangs off
// CTxdStore: the game, Mod Loader and anything else feeding the stream build
// the dictionary first, then the plugin patches its textures into the
// finished RwTexDictionary, which is what makes it additive rather than
// competing. Hooks are installed in DllMain because the first dictionary is
// loaded before any thread of ours could run; the folder scan happens later,
// outside the loader lock.

#include "game_check.h"
#include "hooks.h"

#include <windows.h>

namespace {

DWORD WINAPI StartupThread(LPVOID) {
    StartInitialisation();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        const char* mismatch = nullptr;
        if (CheckGameVersion(&mismatch) && InstallHooks(module))
            MarkHooksInstalled();

        if (HANDLE thread = CreateThread(nullptr, 0, &StartupThread, nullptr, 0, nullptr))
            CloseHandle(thread);
    }
    return TRUE;
}
