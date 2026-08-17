#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// One .png on disk that replaces a texture inside a TXD.
struct TextureSource {
    std::string  name;  // texture name inside the dictionary (max 31 chars)
    std::wstring path;
    uint64_t     stamp; // write time + size; used to detect edits
};

// All replacements targeting a single TXD slot.
struct TxdOverride {
    std::string                txdName;
    uint32_t                   hash = 0; // CKeyGen::GetUppercaseKey(txdName)
    std::vector<TextureSource> textures;
};

// The set of overrides found on disk, keyed by TXD name hash.
class Registry {
public:
    // Rebuilds from disk. Returns false if the root folder is missing.
    bool Rescan(const std::wstring& root);

    const TxdOverride* Find(uint32_t hash) const;

    const std::unordered_map<uint32_t, TxdOverride>& all() const { return txds_; }
    size_t txdCount() const { return txds_.size(); }
    size_t textureCount() const { return textureCount_; }

private:
    std::unordered_map<uint32_t, TxdOverride> txds_;
    size_t                                    textureCount_ = 0;
};
