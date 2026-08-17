#pragma once

#include <string>

struct RwRaster;

// Decodes a PNG and uploads it into a fresh 32-bit RenderWare raster with a single
// mip level. Returns nullptr on failure (the reason is logged).
RwRaster* CreateRasterFromPng(const std::wstring& path);
