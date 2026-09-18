#pragma once

#include <cstdint>

// GTA San Andreas 1.0 US. Every address is an absolute virtual address in the
// default 0x400000 image.
namespace game {

// Hook targets.
// CTxdStore::LoadTxd(int, RwStream*)
constexpr uintptr_t kAddrLoadTxd = 0x731DD0;
// CTxdStore::FinishLoadTxd(int, RwStream*)
constexpr uintptr_t kAddrFinishLoadTxd = 0x731E40;
// CTxdStore::RemoveTxd(int)
constexpr uintptr_t kAddrRemoveTxd = 0x731E90;
// CTxdStore::AddTxdSlot(const char*)
constexpr uintptr_t kAddrAddTxdSlot = 0x731C80;
// CTxdStore::LoadTxd(int, const char*)
constexpr uintptr_t kAddrLoadTxdFile = 0x7320B0;
// CTimer::Update()
constexpr uintptr_t kAddrTimerUpdate = 0x561B10;

// Called functions.
// CPool<TxdDef>**
constexpr uintptr_t kAddrTxdPoolPtr = 0xC8800C;
constexpr uintptr_t kAddrGetUppercaseKey   = 0x53CF30;
constexpr uintptr_t kAddrRwRasterCreate    = 0x7FB230;
constexpr uintptr_t kAddrRwRasterDestroy   = 0x7FB020;
constexpr uintptr_t kAddrRwRasterLock      = 0x7FB2D0;
constexpr uintptr_t kAddrRwRasterUnlock    = 0x7FAEC0;
constexpr uintptr_t kAddrRwTextureCreate   = 0x7F37C0;
constexpr uintptr_t kAddrRwTextureDestroy  = 0x7F3820;
constexpr uintptr_t kAddrRwTextureSetName  = 0x7F38A0;
constexpr uintptr_t kAddrRwTexDictAddTex   = 0x7F3980;

} // namespace game
