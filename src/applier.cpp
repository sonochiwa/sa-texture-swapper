#include "applier.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "game.h"
#include "log.h"
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

bool WithinEditDistance(const std::string& a, const std::string& b, size_t limit) {
    if (a.size() > b.size() + limit || b.size() > a.size() + limit)
        return false;

    std::vector<size_t> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j)
        previous[j] = j;

    for (size_t i = 1; i <= a.size(); ++i) {
        current[0]  = i;
        size_t best = current[0];
        for (size_t j = 1; j <= b.size(); ++j) {
            const size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            current[j] = (std::min)({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});
            best       = (std::min)(best, current[j]);
        }
        if (best > limit)
            return false;
        previous.swap(current);
    }
    return previous[b.size()] <= limit;
}

// A missing texture is nearly always a typo in the file name, so the log points at
// the names that do exist and look close.
std::string SimilarNames(RwTexDictionary* dict, const std::string& wanted) {
    const std::string want = ToLower(wanted);

    std::string joined;
    size_t      count = 0;
    for (const std::string& name : game::TextureNames(dict)) {
        const std::string lower = ToLower(name);
        const bool        close = lower.find(want) != std::string::npos ||
                           want.find(lower) != std::string::npos ||
                           WithinEditDistance(lower, want, 2);
        if (!close)
            continue;

        if (!joined.empty())
            joined += ", ";
        joined += name;
        if (++count == 3)
            break;
    }
    return joined;
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

    if (!overrides) {
        WarnAboutNearMiss(slot);
        return;
    }

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

// Nothing matched this dictionary. If a folder is nearly one of its names, say so:
// silence is the worst outcome, because there is nothing to search the log for.
void Applier::WarnAboutNearMiss(int slot) {
    if (warned_.count(slot))
        return;

    const auto        slotIt   = slotNames_.find(slot);
    const auto        fileIt   = fileNames_.find(slot);
    const std::string slotName = slotIt != slotNames_.end() ? slotIt->second : std::string();
    const std::string fileName = fileIt != fileNames_.end() ? fileIt->second : std::string();
    if (slotName.empty() && fileName.empty())
        return;

    std::string accepted = slotName.empty() ? fileName : slotName;
    if (!fileName.empty() && !slotName.empty() && fileName != slotName)
        accepted += "' or '" + fileName;

    for (const auto& entry : registry_->all()) {
        const TxdOverride& txd = entry.second;
        const bool         isNear =
            (!slotName.empty() && WithinEditDistance(txd.txdName, slotName, 2)) ||
            (!fileName.empty() && WithinEditDistance(txd.txdName, fileName, 2));
        if (!isNear)
            continue;

        warned_.insert(slot);
        LOG_ERROR("your folder '%s' matches no txd; the dictionary that just loaded takes "
                  "'%s' - rename the folder to that",
                  txd.txdName.c_str(), accepted.c_str());
        return;
    }
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
        LOG_INFO("restoring original '%s' (slot %d)", it->name.c_str(), slot);
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
            LOG_INFO("reloaded %s.txd/%s", overrides->txdName.c_str(), source.name.c_str());
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
            LOG_INFO("replaced %s.txd/%s", overrides->txdName.c_str(), source.name.c_str());
        } else {
            RwTexture* created = game::RwTextureCreate(raster);
            if (!created) {
                game::RwRasterDestroy(raster);
                LOG_ERROR("RwTextureCreate failed for %s.txd/%s", overrides->txdName.c_str(),
                          source.name.c_str());
                continue;
            }
            game::RwTextureSetName(created, source.name.c_str());
            created->mask[0]          = '\0';
            created->filterAddressing = kDefaultFilterAddressing;
            game::RwTexDictionaryAddTexture(dict, created);

            record.texture     = created;
            record.ownsTexture = true;

            const std::string similar = SimilarNames(dict, source.name);
            if (similar.empty()) {
                LOG_INFO("added %s.txd/%s (no such texture in the original txd)",
                         overrides->txdName.c_str(), source.name.c_str());
            } else {
                LOG_ERROR("%s.txd has no texture called '%s', so it was added as a new one "
                          "and nothing in the game draws it. Did you mean: %s?",
                          overrides->txdName.c_str(), source.name.c_str(), similar.c_str());
            }
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
