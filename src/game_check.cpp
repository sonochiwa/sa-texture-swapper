#include "game_check.h"

#include "addresses.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

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
    // jmp rel32, jmp rel8, push imm32; ret, jmp [addr]
    return code[0] == 0xE9 || code[0] == 0xEB || code[0] == 0x68 ||
           (code[0] == 0xFF && code[1] == 0x25);
}

} // namespace

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
            // Hooked by someone else; MinHook chains onto it.
            continue;

        static char message[128];
        _snprintf_s(message, _TRUNCATE, "%s does not look like GTA San Andreas 1.0 US",
                    target.name);
        *what = message;
        return false;
    }
    return true;
}
