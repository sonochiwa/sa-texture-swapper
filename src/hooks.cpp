#include "hooks.h"

#include "addresses.h"
#include "applier.h"
#include "config.h"
#include "log.h"
#include "overrides.h"
#include "version.h"
#include "watcher.h"

#include "MinHook.h"

#include <windows.h>

#include <filesystem>
#include <mutex>
#include <system_error>

namespace {

Config   g_config;
Registry g_registry;
Applier  g_applier;
// Never freed: a thread is never joined under the loader lock.
Watcher* g_watcher = nullptr;

std::once_flag g_initOnce;
HMODULE        g_module         = nullptr;
bool           g_hooksInstalled = false;
const char*    g_inactiveReason = nullptr;
bool           g_active         = false;

using LoadTxd_t       = bool(__cdecl*)(int, void*);
using FinishLoadTxd_t = bool(__cdecl*)(int, void*);
using RemoveTxd_t     = void(__cdecl*)(int);
using AddTxdSlot_t    = int(__cdecl*)(const char*);
using LoadTxdFile_t   = bool(__cdecl*)(int, const char*);
using TimerUpdate_t   = void(__cdecl*)();

LoadTxd_t       g_origLoadTxd       = nullptr;
FinishLoadTxd_t g_origFinishLoadTxd = nullptr;
RemoveTxd_t     g_origRemoveTxd     = nullptr;
AddTxdSlot_t    g_origAddTxdSlot    = nullptr;
LoadTxdFile_t   g_origLoadTxdFile   = nullptr;
TimerUpdate_t   g_origTimerUpdate   = nullptr;

// Runs on the first hook call or on the startup thread, never under the loader lock.
void Initialise() {
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_module, dllPath, MAX_PATH);

    g_config = LoadConfig(g_module, dllPath);
    // No log file exists at all unless the INI asks for one; every LOG_ call below
    // then becomes a no-op.
    if (g_config.log)
        logging::Open(g_config.logPath);
    LOG_INFO("%s v%s", PLUGIN_NAME, PLUGIN_VERSION);

    if (!g_hooksInstalled) {
        LOG_ERROR("%s; the plugin stays inactive",
                  g_inactiveReason ? g_inactiveReason : "startup failed");
        return;
    }
    if (!g_config.isEnabled) {
        LOG_INFO("isEnabled=0, the plugin stays inactive");
        return;
    }

    // Create the folder up front: it shows the user where the PNGs go, and the hot
    // reload watcher has nothing to open until it exists.
    std::error_code ec;
    if (std::filesystem::create_directories(g_config.rootDir, ec))
        LOG_INFO("created the textures folder");

    g_registry.Rescan(g_config.rootDir);
    g_applier.Init(&g_registry);
    g_active = true;
    LOG_INFO("ready");
}

void EnsureInitialised() {
    std::call_once(g_initOnce, &Initialise);
}

void Tick() {
    if (!g_config.hotReload)
        return;

    if (!g_watcher) {
        g_watcher = new Watcher();
        g_watcher->Start(g_config.rootDir);
    }

    if (g_watcher->ConsumePending()) {
        LOG_INFO("change detected, rescanning");
        g_registry.Rescan(g_config.rootDir);
        g_applier.ResyncAll();
    }
}

bool __cdecl LoadTxd_Hook(int index, void* stream) {
    EnsureInitialised();
    const bool loaded = g_origLoadTxd(index, stream);
    if (loaded && g_active)
        g_applier.OnTxdLoaded(index);
    return loaded;
}

bool __cdecl FinishLoadTxd_Hook(int index, void* stream) {
    EnsureInitialised();
    const bool loaded = g_origFinishLoadTxd(index, stream);
    if (loaded && g_active)
        g_applier.OnTxdLoaded(index);
    return loaded;
}

void __cdecl RemoveTxd_Hook(int index) {
    EnsureInitialised();
    if (g_active)
        g_applier.OnTxdRemoved(index);
    g_origRemoveTxd(index);
}

int __cdecl AddTxdSlot_Hook(const char* name) {
    EnsureInitialised();
    const int slot = g_origAddTxdSlot(name);
    if (g_active)
        g_applier.OnTxdSlotAdded(slot, name);
    return slot;
}

// The file name is recorded before the original runs, because loading the file is what
// ends up calling LoadTxd_Hook, and the name has to be known by then.
bool __cdecl LoadTxdFile_Hook(int index, const char* filename) {
    EnsureInitialised();
    if (g_active)
        g_applier.OnTxdFileLoad(index, filename);
    return g_origLoadTxdFile(index, filename);
}

void __cdecl TimerUpdate_Hook() {
    EnsureInitialised();
    g_origTimerUpdate();
    if (g_active)
        Tick();
}

bool Hook(uintptr_t target, void* detour, void** original) {
    return MH_CreateHook(reinterpret_cast<void*>(target), detour, original) == MH_OK;
}

} // namespace

bool InstallHooks(HMODULE module) {
    g_module = module;
    if (MH_Initialize() != MH_OK)
        return false;

    const bool created =
        Hook(game::kAddrLoadTxd, &LoadTxd_Hook, reinterpret_cast<void**>(&g_origLoadTxd)) &&
        Hook(game::kAddrFinishLoadTxd, &FinishLoadTxd_Hook,
             reinterpret_cast<void**>(&g_origFinishLoadTxd)) &&
        Hook(game::kAddrRemoveTxd, &RemoveTxd_Hook, reinterpret_cast<void**>(&g_origRemoveTxd)) &&
        Hook(game::kAddrAddTxdSlot, &AddTxdSlot_Hook,
             reinterpret_cast<void**>(&g_origAddTxdSlot)) &&
        Hook(game::kAddrLoadTxdFile, &LoadTxdFile_Hook,
             reinterpret_cast<void**>(&g_origLoadTxdFile)) &&
        Hook(game::kAddrTimerUpdate, &TimerUpdate_Hook,
             reinterpret_cast<void**>(&g_origTimerUpdate));

    if (!created || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MH_Uninitialize();
        return false;
    }
    return true;
}

void SetInactive(const char* reason) {
    g_inactiveReason = reason;
}

void MarkHooksInstalled() {
    g_hooksInstalled = true;
}

void StartInitialisation() {
    EnsureInitialised();
}
