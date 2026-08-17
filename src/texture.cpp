#include "texture.h"

#include <windows.h>

#include <vector>

#include "game.h"
#include "log.h"
#include "stb_image.h"

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

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > (64 << 20)) {
        CloseHandle(file);
        return false;
    }

    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
    CloseHandle(file);
    return ok && read == out.size();
}

bool IsPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

} // namespace

RwRaster* CreateRasterFromPng(const std::wstring& path) {
    const std::string narrowPath = ToNarrow(path);

    std::vector<uint8_t> file;
    if (!ReadWholeFile(path, file)) {
        LOG_ERROR("cannot read '%s'", narrowPath.c_str());
        return nullptr;
    }

    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &width,
                                            &height, &channels, 4);
    if (!pixels) {
        LOG_ERROR("cannot decode '%s': %s", narrowPath.c_str(), stbi_failure_reason());
        return nullptr;
    }

    if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
        LOG_INFO("'%s' is %dx%d; non power-of-two sizes may misbehave on some drivers",
                 narrowPath.c_str(), width, height);
    }

    const int32_t flags  = rwRASTERTYPETEXTURE | rwRASTERFORMAT8888;
    RwRaster*     raster = game::RwRasterCreate(width, height, 32, flags);
    if (!raster) {
        stbi_image_free(pixels);
        LOG_ERROR("RwRasterCreate failed for '%s' (%dx%d)", narrowPath.c_str(), width, height);
        return nullptr;
    }

    uint8_t* dest = game::RwRasterLock(raster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    if (!dest) {
        game::RwRasterDestroy(raster);
        stbi_image_free(pixels);
        LOG_ERROR("RwRasterLock failed for '%s'", narrowPath.c_str());
        return nullptr;
    }

    // stb gives RGBA; RenderWare's 8888 raster is BGRA on the D3D9 driver.
    const int stride = raster->stride;
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = pixels + static_cast<size_t>(y) * width * 4;
        uint8_t*       dst = dest + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            src += 4;
            dst += 4;
        }
    }

    game::RwRasterUnlock(raster);
    stbi_image_free(pixels);
    return raster;
}
