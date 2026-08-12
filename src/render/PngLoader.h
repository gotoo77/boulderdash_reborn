#pragma once

#include <filesystem>
#include <vector>

namespace gfx {

bool loadPngRGBA(
    const std::filesystem::path& path,
    std::vector<unsigned char>& outPixels,
    int& width,
    int& height,
    int& pitch);

} // namespace gfx
