// Texture Swapper - replaces individual textures inside GTA San Andreas texture
// dictionaries.
//
// Everything hangs off CTxdStore: the game (and Mod Loader, and anything else feeding
// the stream) builds the dictionary first, then we patch our textures into the finished
// RwTexDictionary. That is what makes the plugin additive rather than competing.

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

#include "MinHook.h"
#include "applier.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "overrides.h"
#include "version.h"
#include "watcher.h"

namespace {

Config   g_config;
Registry g_registry;
Applier  g_applier;
Watcher* g_watcher = nullptr; // intentionally leaked: never join a thread under the loader lock

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

// ------------------------------------------------------------------ version check

bool IsReadable(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0)
        return false;
    if (info.State != MEM_COMMIT)
        return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(info.Protect & readable))
        return false;
    const uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return address + size <= end;
}

bool MatchesSignature(uintptr_t address, const uint8_t* bytes, size_t size) {
    return IsReadable(address, size) &&
           memcmp(reinterpret_cast<void*>(address), bytes, size) == 0;
}

// PE identity of gta_sa.exe 1.0 US. Nothing patches the headers, so this survives
// other plugins, unlike the code the plugin hooks.
constexpr uint32_t kTimeDateStamp1_0_US = 0x427101CA;
constexpr uint32_t kSizeOfImage1_0_US   = 0x1177000;

bool MatchesPeIdentity() {
    const auto base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    if (!base || !IsReadable(reinterpret_cast<uintptr_t>(base), 0x40))
        return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    if (!IsReadable(reinterpret_cast<uintptr_t>(base + dos->e_lfanew), sizeof(IMAGE_NT_HEADERS32)))
        return false;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    return nt->FileHeader.TimeDateStamp == kTimeDateStamp1_0_US &&
           nt->OptionalHeader.SizeOfImage == kSizeOfImage1_0_US;
}

// A function whose first byte is a jump has already been detoured by another plugin,
// which says nothing about the game version.
bool LooksDetoured(uintptr_t address) {
    if (!IsReadable(address, 2))
        return false;
    const auto* code = reinterpret_cast<const uint8_t*>(address);
    return code[0] == 0xE9 ||                     // jmp rel32
           code[0] == 0xEB ||                     // jmp rel8
           code[0] == 0x68 ||                     // push imm32; ret
           (code[0] == 0xFF && code[1] == 0x25);  // jmp [addr]
}

// Prologues taken from gta_sa.exe 1.0 US. Each one dereferences a store global, so a
// mismatch means we are not looking at the function we think we are. Only memory
// comparisons happen here, which keeps DllMain free of anything heavier.
bool CheckGameVersion(const char** what) {
    static const uint8_t kLoadTxd[]     = {0xA1, 0x0C, 0x80, 0xC8, 0x00, 0x8B, 0x48, 0x04};
    static const uint8_t kFinishLoad[]  = {0xA1, 0x0C, 0x80, 0xC8, 0x00, 0x8B, 0x48, 0x04,
                                           0x56, 0x57};
    static const uint8_t kRemoveTxd[]   = {0x8B, 0x0D, 0x0C, 0x80, 0xC8, 0x00, 0x8B, 0x51, 0x04};
    static const uint8_t kAddTxdSlot[]  = {0x8B, 0x0D, 0x0C, 0x80, 0xC8, 0x00, 0x56, 0xE8};
    static const uint8_t kLoadTxdFile[] = {0x8B, 0x44, 0x24, 0x08, 0x90, 0xE9};
    static const uint8_t kTimerUpdate[] = {0x8B, 0x0D, 0x28, 0xCB, 0xB7, 0x00, 0x83, 0xEC, 0x0C};

    struct Target {
        uintptr_t      address;
        const uint8_t* bytes;
        size_t         size;
        const char*    name;
    };
    const Target targets[] = {
        {game::kAddrLoadTxd, kLoadTxd, sizeof(kLoadTxd), "CTxdStore::LoadTxd"},
        {game::kAddrFinishLoadTxd, kFinishLoad, sizeof(kFinishLoad), "CTxdStore::FinishLoadTxd"},
        {game::kAddrRemoveTxd, kRemoveTxd, sizeof(kRemoveTxd), "CTxdStore::RemoveTxd"},
        {game::kAddrAddTxdSlot, kAddTxdSlot, sizeof(kAddTxdSlot), "CTxdStore::AddTxdSlot"},
        {game::kAddrLoadTxdFile, kLoadTxdFile, sizeof(kLoadTxdFile),
         "CTxdStore::LoadTxd(int, const char*)"},
        {game::kAddrTimerUpdate, kTimerUpdate, sizeof(kTimerUpdate), "CTimer::Update"},
    };

    if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != 0x400000) {
        *what = "the executable is not based at 0x400000";
        return false;
    }

    // The PE headers alone identify the build. When they match there is no reason to
    // look at code another plugin may already have hooked - and loading from scripts\
    // means the plugin often does load after those plugins.
    if (MatchesPeIdentity())
        return true;

    for (const Target& target : targets) {
        if (MatchesSignature(target.address, target.bytes, target.size))
            continue;
        if (LooksDetoured(target.address))
            continue; // hooked by someone else; MinHook chains onto it

        static char message[128];
        _snprintf_s(message, _TRUNCATE, "%s does not look like GTA San Andreas 1.0 US",
                    target.name);
        *what = message;
        return false;
    }
    return true;
}

// ------------------------------------------------------------ deferred initialisation

// Runs on the first hook call or on the startup thread, never under the loader lock.
void Initialise() {
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_module, dllPath, MAX_PATH);

    g_config = LoadConfig(dllPath);
    // No log file exists at all unless the INI asks for one; every LOG_ call below
    // then becomes a no-op.
    if (g_config.loggingEnabled)
        logging::Open(g_config.logPath);
    LOG_INFO("%s v%s", TEXTURE_SWAPPER_NAME, TEXTURE_SWAPPER_VERSION);

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

DWORD WINAPI StartupThread(LPVOID) {
    EnsureInitialised();
    return 0;
}

// ------------------------------------------------------------------------- hooks

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

bool InstallHooks() {
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

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        g_module = module;

        // Hooks have to exist before the game loads its first texture dictionary, so
        // they are installed here. Everything else - configuration, logging, scanning
        // the textures folder - happens in Initialise(), outside the loader lock.
        const char* mismatch = nullptr;
        if (!CheckGameVersion(&mismatch)) {
            g_inactiveReason = mismatch ? mismatch : "this is not GTA San Andreas 1.0 US";
        } else if (!InstallHooks()) {
            g_inactiveReason = "the hooks could not be installed";
        } else {
            g_hooksInstalled = true;
        }

        if (HANDLE thread = CreateThread(nullptr, 0, &StartupThread, nullptr, 0, nullptr))
            CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        logging::Close();
    }
    return TRUE;
}
