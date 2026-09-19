#include "overrides.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "game.h"

namespace fs = std::filesystem;

namespace {

std::string ToNarrow(const std::wstring& text) {
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return text;
}

bool IsPng(const fs::path& path) {
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return ext == L".png";
}

bool EndsWith(const std::string& text, const char* suffix) {
    const size_t length = strlen(suffix);
    return text.size() > length && text.compare(text.size() - length, length, suffix) == 0;
}

uint64_t StampOf(const fs::directory_entry& entry) {
    std::error_code ec;
    const auto time = entry.last_write_time(ec).time_since_epoch().count();
    const auto size = entry.file_size(ec);
    return static_cast<uint64_t>(time) * 31u + static_cast<uint64_t>(size);
}

} // namespace

bool Registry::Rescan(const std::wstring& root) {
    txds_.clear();
    textureCount_ = 0;

    std::error_code ec;
    const fs::path rootPath(root);
    if (!fs::is_directory(rootPath, ec)) {
        return false;
    }

    fs::recursive_directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        return false;
    }

    // The folder holding the file names the txd; everything above it is free-form,
    // so people can mirror the game's own layout (models/hud/fist.png).
    auto consider = [&](const fs::directory_entry& entry) {
        if (!entry.is_regular_file(ec) || !IsPng(entry.path()))
            return;

        const fs::path parent = entry.path().parent_path();
        if (fs::equivalent(parent, rootPath, ec)) {
            return;
        }

        std::string       txdName     = ToLower(ToNarrow(parent.filename().wstring()));
        const std::string textureName = ToNarrow(entry.path().stem().wstring());
        if (txdName.empty() || textureName.empty())
            return;

        // "hud.txd" and "hud" both name the same dictionary. The explicit form is
        // preferred because it cannot be mistaken for a grouping folder.
        if (EndsWith(txdName, ".txd"))
            txdName.erase(txdName.size() - 4);

        // CTxdStore is a flat namespace keyed by name, so an archive cannot narrow a
        // match down. A PNG sitting straight inside an .img folder means someone
        // expected otherwise.
        if (EndsWith(txdName, ".img")) {
            return;
        }

        if (textureName.size() >= kRwTextureBaseNameLength) {
            return;
        }

        const uint32_t hash = game::GetUppercaseKey(txdName.c_str());
        TxdOverride&   txd  = txds_[hash];
        if (txd.txdName.empty()) {
            txd.txdName = txdName;
            txd.hash    = hash;
        }

        const std::string key = ToLower(textureName);
        auto              dup = std::find_if(txd.textures.begin(), txd.textures.end(),
                                             [&](const TextureSource& t) { return ToLower(t.name) == key; });
        if (dup != txd.textures.end()) {
            return;
        }

        txd.textures.push_back({textureName, entry.path().wstring(), StampOf(entry)});
        ++textureCount_;
    };

    try {
        for (const fs::directory_entry& entry : it)
            consider(entry);
    } catch (const std::exception&) {
        // A folder vanishing mid-scan (hot reload) must not take the game down.
    }

    return true;
}

const TxdOverride* Registry::Find(uint32_t hash) const {
    const auto it = txds_.find(hash);
    return it == txds_.end() ? nullptr : &it->second;
}
