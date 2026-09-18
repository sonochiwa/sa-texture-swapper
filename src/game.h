#pragma once

#include "addresses.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// RenderWare structures the plugin reads and writes.

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

// Game internals.

// A slot of CTxdStore::ms_pTxdPool.
struct TxdDef {
    RwTexDictionary* dict;
    uint16_t         refsCount;
    int16_t          parentIndex;
    // CKeyGen::GetUppercaseKey(txd name)
    uint32_t         hash;
};
static_assert(sizeof(TxdDef) == 0xC, "TxdDef layout");

// CPool<TxdDef> header. Only the fields we read are named.
struct CPoolRaw {
    void*    objects;
    // Bit 7 set: the slot is empty.
    uint8_t* byteMap;
    int32_t  size;
    int32_t  firstFree;
    bool     ownsAllocations;
    bool     locked;
};

namespace game {

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
    if (pool->byteMap[index] & 0x80)
        return nullptr;
    return reinterpret_cast<TxdDef*>(static_cast<uint8_t*>(pool->objects) + index * sizeof(TxdDef));
}

// Walks texturesInDict; RwTexDictionaryFindNamedTexture is case sensitive and we are not.
RwTexture* FindTextureNoCase(RwTexDictionary* dict, const char* name);

// Names of every texture in the dictionary, in dictionary order.
std::vector<std::string> TextureNames(RwTexDictionary* dict);

} // namespace game
