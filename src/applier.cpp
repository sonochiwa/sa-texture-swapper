#include "applier.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "game.h"
#include "texture.h"

namespace {

bool EqualsNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (::tolower(static_cast<unsigned char>(a[i])) !=
            ::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

// rwFILTERLINEAR + wrap addressing on both axes, matching what the game's own textures use.
constexpr uint32_t kDefaultFilterAddressing = rwFILTERLINEAR | (1u << 8) | (1u << 12);

std::string ToLower(std::string text) {
    for (char& c : text)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return text;
}

} // namespace

// A folder may be named after the slot the game uses internally, or after the .txd
// file on disk. Those differ for the frontend dictionaries, and only the file name is
// something a person can see, so both are accepted.
const TxdOverride* Applier::FindOverrides(int slot, const TxdDef* def) const {
    if (const TxdOverride* bySlot = registry_->Find(def->hash))
        return bySlot;

    const auto file = fileNames_.find(slot);
    if (file != fileNames_.end())
        return registry_->Find(game::GetUppercaseKey(file->second.c_str()));

    return nullptr;
}

void Applier::OnTxdLoaded(int slot) {
    if (!registry_)
        return;

    TxdDef* def = game::GetTxdDef(slot);
    if (!def || !def->dict)
        return;

    const TxdOverride* overrides = FindOverrides(slot, def);

    // A fresh dictionary replaced whatever was in this slot, so any raster we were
    // holding on to for it belongs to a dictionary that no longer exists.
    Forget(slot);

    if (!overrides)
        return;

    Sync(slot, def->dict, overrides);
}

void Applier::OnTxdRemoved(int slot) {
    Forget(slot);
}

void Applier::OnTxdSlotAdded(int slot, const char* name) {
    if (name && *name)
        slotNames_[slot] = ToLower(name);
}

void Applier::OnTxdFileLoad(int slot, const char* path) {
    if (!path || !*path)
        return;

    std::string name = ToLower(path);
    if (const size_t slash = name.find_last_of("\\/"); slash != std::string::npos)
        name.erase(0, slash + 1);
    if (const size_t dot = name.find_last_of('.'); dot != std::string::npos)
        name.erase(dot);

    if (!name.empty())
        fileNames_[slot] = name;
}

void Applier::ResyncAll() {
    if (!registry_)
        return;

    CPoolRaw* pool = game::TxdPool();
    if (!pool)
        return;

    for (int slot = 0; slot < pool->size; ++slot) {
        TxdDef* def = game::GetTxdDef(slot);
        if (!def || !def->dict) {
            if (applied_.count(slot))
                Forget(slot); // slot went away without us noticing
            continue;
        }

        const TxdOverride* overrides = FindOverrides(slot, def);
        if (!overrides && !applied_.count(slot))
            continue;

        Sync(slot, def->dict, overrides);
    }
}

void Applier::Sync(int slot, RwTexDictionary* dict, const TxdOverride* overrides) {
    std::vector<AppliedTexture>& applied = applied_[slot];

    // Drop replacements whose file disappeared, putting the original texture back.
    for (auto it = applied.begin(); it != applied.end();) {
        const bool stillWanted =
            overrides && std::any_of(overrides->textures.begin(), overrides->textures.end(),
                                     [&](const TextureSource& s) { return EqualsNoCase(s.name, it->name); });
        if (stillWanted) {
            ++it;
            continue;
        }
        Revert(*it);
        it = applied.erase(it);
    }

    if (!overrides) {
        if (applied.empty())
            applied_.erase(slot);
        return;
    }

    for (const TextureSource& source : overrides->textures) {
        auto known = std::find_if(applied.begin(), applied.end(), [&](const AppliedTexture& a) {
            return EqualsNoCase(a.name, source.name);
        });

        if (known != applied.end() && known->stamp == source.stamp)
            continue; // unchanged

        RwRaster* raster = CreateRasterFromPng(source.path);
        if (!raster)
            continue;

        if (known != applied.end()) {
            // Re-apply: swap in the new raster and drop the one we made last time.
            RwRaster* previous = known->texture->raster;
            known->texture->raster = raster;
            known->stamp = source.stamp;
            if (previous && previous != known->originalRaster)
                game::RwRasterDestroy(previous);
            continue;
        }

        AppliedTexture record;
        record.name  = source.name;
        record.stamp = source.stamp;

        if (RwTexture* existing = game::FindTextureNoCase(dict, source.name.c_str())) {
            // Swapping the raster keeps the RwTexture pointer valid for anything
            // that already grabbed it (sprites, materials of models loaded later).
            record.texture        = existing;
            record.originalRaster = existing->raster;
            existing->raster      = raster;
        } else {
            RwTexture* created = game::RwTextureCreate(raster);
            if (!created) {
                game::RwRasterDestroy(raster);
                continue;
            }
            game::RwTextureSetName(created, source.name.c_str());
            created->mask[0]          = '\0';
            created->filterAddressing = kDefaultFilterAddressing;
            game::RwTexDictionaryAddTexture(dict, created);

            record.texture     = created;
            record.ownsTexture = true;
        }

        applied.push_back(record);
    }

    if (applied.empty())
        applied_.erase(slot);
}

void Applier::Revert(AppliedTexture& applied) {
    if (!applied.texture)
        return;

    if (applied.ownsTexture) {
        // RwTextureDestroy unlinks it from the dictionary and frees our raster.
        game::RwTextureDestroy(applied.texture);
    } else {
        RwRaster* ours = applied.texture->raster;
        applied.texture->raster = applied.originalRaster;
        if (ours && ours != applied.originalRaster)
            game::RwRasterDestroy(ours);
    }

    applied.texture        = nullptr;
    applied.originalRaster = nullptr;
}

void Applier::Forget(int slot) {
    auto it = applied_.find(slot);
    if (it == applied_.end())
        return;

    // The dictionary these textures lived in is gone (or is being torn down), so the
    // textures and their rasters are freed by RenderWare. The originals we saved are
    // not referenced by anything anymore, so they are ours to free.
    for (AppliedTexture& applied : it->second) {
        if (!applied.ownsTexture && applied.originalRaster)
            game::RwRasterDestroy(applied.originalRaster);
    }
    applied_.erase(it);
}
