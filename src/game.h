// Minimal RenderWare / GTA SA (1.0 US) definitions used by the plugin.
// Addresses are cross-checked against gta_sa.exe 1.0 US (SizeOfImage 0x1177000).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------- RenderWare

struct RwLLLink {
    RwLLLink* next;
    RwLLLink* prev;
};

struct RwLinkList {
    RwLLLink link;
};

struct RwObject {
    uint8_t type;
    uint8_t subType;
    uint8_t flags;
    uint8_t privateFlags;
    void*   parent;
};

struct RwRaster {
    RwRaster* parent;
    uint8_t*  cpPixels;
    uint8_t*  palette;
    int32_t   width, height, depth;
    int32_t   stride;
    int16_t   nOffsetX, nOffsetY;
    uint8_t   cType;
    uint8_t   cFlags;
    uint8_t   privateFlags;
    uint8_t   cFormat;
    uint8_t*  originalPixels;
    int32_t   originalWidth;
    int32_t   originalHeight;
    int32_t   originalStride;
};

struct RwTexDictionary {
    RwObject   object;
    RwLinkList texturesInDict;
    RwLLLink   lInInstance;
};

constexpr size_t kRwTextureBaseNameLength = 32;

struct RwTexture {
    RwRaster*        raster;
    RwTexDictionary* dict;
    RwLLLink         lInDictionary;
    char             name[kRwTextureBaseNameLength];
    char             mask[kRwTextureBaseNameLength];
    uint32_t         filterAddressing;
    int32_t          refCount;
};

enum : int32_t {
    rwRASTERLOCKWRITE        = 0x01,
    rwRASTERLOCKNOFETCH      = 0x04,
    rwRASTERTYPETEXTURE      = 0x04,
    rwRASTERFORMAT8888       = 0x0500,
};

enum : uint32_t {
    rwFILTERLINEAR          = 2,
    rwTEXTUREFILTERMODEMASK = 0x000000FF,
};

// ------------------------------------------------------------- game internals

// A slot of CTxdStore::ms_pTxdPool.
struct TxdDef {
    RwTexDictionary* dict;
    uint16_t         refsCount;
    int16_t          parentIndex;
    uint32_t         hash; // CKeyGen::GetUppercaseKey(txd name)
};
static_assert(sizeof(TxdDef) == 0xC, "TxdDef layout");

// CPool<TxdDef> header. Only the fields we read are named.
struct CPoolRaw {
    void*    objects;
    uint8_t* byteMap; // bit 7 set => slot is empty
    int32_t  size;
    int32_t  firstFree;
    bool     ownsAllocations;
    bool     locked;
};

namespace game {

// Hook targets.
constexpr uintptr_t kAddrLoadTxd       = 0x731DD0; // CTxdStore::LoadTxd(int, RwStream*)
constexpr uintptr_t kAddrFinishLoadTxd = 0x731E40; // CTxdStore::FinishLoadTxd(int, RwStream*)
constexpr uintptr_t kAddrRemoveTxd     = 0x731E90; // CTxdStore::RemoveTxd(int)
constexpr uintptr_t kAddrAddTxdSlot    = 0x731C80; // CTxdStore::AddTxdSlot(const char*)
constexpr uintptr_t kAddrLoadTxdFile   = 0x7320B0; // CTxdStore::LoadTxd(int, const char*)
constexpr uintptr_t kAddrTimerUpdate   = 0x561B10; // CTimer::Update()

// Called functions.
constexpr uintptr_t kAddrTxdPoolPtr        = 0xC8800C; // CPool<TxdDef>**
constexpr uintptr_t kAddrGetUppercaseKey   = 0x53CF30;
constexpr uintptr_t kAddrRwRasterCreate    = 0x7FB230;
constexpr uintptr_t kAddrRwRasterDestroy   = 0x7FB020;
constexpr uintptr_t kAddrRwRasterLock      = 0x7FB2D0;
constexpr uintptr_t kAddrRwRasterUnlock    = 0x7FAEC0;
constexpr uintptr_t kAddrRwTextureCreate   = 0x7F37C0;
constexpr uintptr_t kAddrRwTextureDestroy  = 0x7F3820;
constexpr uintptr_t kAddrRwTextureSetName  = 0x7F38A0;
constexpr uintptr_t kAddrRwTexDictAddTex   = 0x7F3980;

inline uint32_t GetUppercaseKey(const char* str) {
    return reinterpret_cast<uint32_t(__cdecl*)(const char*)>(kAddrGetUppercaseKey)(str);
}

inline RwRaster* RwRasterCreate(int32_t w, int32_t h, int32_t depth, int32_t flags) {
    return reinterpret_cast<RwRaster*(__cdecl*)(int32_t, int32_t, int32_t, int32_t)>(
        kAddrRwRasterCreate)(w, h, depth, flags);
}

inline int32_t RwRasterDestroy(RwRaster* raster) {
    return reinterpret_cast<int32_t(__cdecl*)(RwRaster*)>(kAddrRwRasterDestroy)(raster);
}

inline uint8_t* RwRasterLock(RwRaster* raster, uint8_t level, int32_t lockMode) {
    return reinterpret_cast<uint8_t*(__cdecl*)(RwRaster*, uint8_t, int32_t)>(
        kAddrRwRasterLock)(raster, level, lockMode);
}

inline RwRaster* RwRasterUnlock(RwRaster* raster) {
    return reinterpret_cast<RwRaster*(__cdecl*)(RwRaster*)>(kAddrRwRasterUnlock)(raster);
}

inline RwTexture* RwTextureCreate(RwRaster* raster) {
    return reinterpret_cast<RwTexture*(__cdecl*)(RwRaster*)>(kAddrRwTextureCreate)(raster);
}

inline int32_t RwTextureDestroy(RwTexture* texture) {
    return reinterpret_cast<int32_t(__cdecl*)(RwTexture*)>(kAddrRwTextureDestroy)(texture);
}

inline RwTexture* RwTextureSetName(RwTexture* texture, const char* name) {
    return reinterpret_cast<RwTexture*(__cdecl*)(RwTexture*, const char*)>(
        kAddrRwTextureSetName)(texture, name);
}

inline RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture) {
    return reinterpret_cast<RwTexture*(__cdecl*)(RwTexDictionary*, RwTexture*)>(
        kAddrRwTexDictAddTex)(dict, texture);
}

inline CPoolRaw* TxdPool() {
    return *reinterpret_cast<CPoolRaw**>(kAddrTxdPoolPtr);
}

// Returns the slot descriptor, or nullptr if the index is out of range or free.
inline TxdDef* GetTxdDef(int index) {
    CPoolRaw* pool = TxdPool();
    if (!pool || !pool->objects || !pool->byteMap)
        return nullptr;
    if (index < 0 || index >= pool->size)
        return nullptr;
    if (pool->byteMap[index] & 0x80) // bEmpty
        return nullptr;
    return reinterpret_cast<TxdDef*>(static_cast<uint8_t*>(pool->objects) + index * sizeof(TxdDef));
}

// Walks texturesInDict; RwTexDictionaryFindNamedTexture is case sensitive and we are not.
RwTexture* FindTextureNoCase(RwTexDictionary* dict, const char* name);

// Names of every texture in the dictionary, in dictionary order.
std::vector<std::string> TextureNames(RwTexDictionary* dict);

} // namespace game
