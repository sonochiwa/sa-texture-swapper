#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "overrides.h"

struct RwRaster;
struct RwTexture;
struct RwTexDictionary;
struct TxdDef;

// Owns the "what did we swap into which slot" bookkeeping, so replacements can be
// re-applied when a file changes and undone when the TXD is unloaded.
class Applier {
public:
    void Init(const Registry* registry) { registry_ = registry; }

    // A TXD slot just finished loading (whatever produced its data, Mod Loader included).
    void OnTxdLoaded(int slot);

    // A TXD slot is about to be unloaded; release everything we still own for it.
    void OnTxdRemoved(int slot);

    // A TXD slot was registered under this name.
    void OnTxdSlotAdded(int slot, const char* name);

    // A TXD slot is about to be loaded from this file. The name of the file is not
    // always the name of the slot - models\fronten_pc.txd lands in a slot called
    // "frontend_pc" - and a folder may be named after either.
    void OnTxdFileLoad(int slot, const char* path);

    // Reconcile every currently loaded slot against the registry (used after a rescan).
    void ResyncAll();

private:
    struct AppliedTexture {
        std::string name;
        RwTexture*  texture        = nullptr; // texture inside the dictionary
        RwRaster*   originalRaster = nullptr; // the game's raster, kept for restoring
        bool        ownsTexture    = false;   // we added this texture; it wasn't in the txd
        uint64_t    stamp          = 0;
    };

    const TxdOverride* FindOverrides(int slot, const TxdDef* def) const;
    void               WarnAboutNearMiss(int slot);
    void               Sync(int slot, RwTexDictionary* dict, const TxdOverride* overrides);
    void               Revert(AppliedTexture& applied);
    void               Forget(int slot);

    const Registry* registry_ = nullptr;

    std::unordered_map<int, std::vector<AppliedTexture>> applied_;

    // Slot index to the two names a folder may legitimately carry.
    std::unordered_map<int, std::string> slotNames_;
    std::unordered_map<int, std::string> fileNames_;
    std::unordered_set<int>              warned_;
};
