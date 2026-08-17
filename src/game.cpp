#include "game.h"

#include <cctype>
#include <cstring>

namespace game {
namespace {

// Iterates texturesInDict, calling fn(RwTexture*) until it returns false.
template <typename Fn>
void ForEachTexture(RwTexDictionary* dict, Fn fn) {
    if (!dict)
        return;

    const RwLLLink* head = &dict->texturesInDict.link;
    for (RwLLLink* link = head->next; link && link != head; link = link->next) {
        RwTexture* texture = reinterpret_cast<RwTexture*>(
            reinterpret_cast<uint8_t*>(link) - offsetof(RwTexture, lInDictionary));
        if (!fn(texture))
            return;
    }
}

bool EqualsNoCase(const char* a, const char* b) {
    for (size_t i = 0; i < kRwTextureBaseNameLength; ++i) {
        const int ca = ::tolower(static_cast<unsigned char>(a[i]));
        const int cb = ::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb)
            return false;
        if (ca == '\0')
            return true;
    }
    return true;
}

} // namespace

RwTexture* FindTextureNoCase(RwTexDictionary* dict, const char* name) {
    if (!name)
        return nullptr;

    RwTexture* found = nullptr;
    ForEachTexture(dict, [&](RwTexture* texture) {
        if (!EqualsNoCase(texture->name, name))
            return true;
        found = texture;
        return false;
    });
    return found;
}

std::vector<std::string> TextureNames(RwTexDictionary* dict) {
    std::vector<std::string> names;
    ForEachTexture(dict, [&](RwTexture* texture) {
        names.emplace_back(texture->name,
                           strnlen(texture->name, kRwTextureBaseNameLength));
        return true;
    });
    return names;
}

} // namespace game
