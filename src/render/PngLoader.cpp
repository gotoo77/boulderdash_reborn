#include "PngLoader.h"

#include <png.h>

#include <cstdio>

#include "../util/Logger.h"

namespace gfx {
namespace {

struct PngCloser {
    FILE* file = nullptr;
    ~PngCloser() {
        if (file) {
            std::fclose(file);
        }
    }
};

} // namespace

bool loadPngRGBA(
    const std::filesystem::path& path,
    std::vector<unsigned char>& outPixels,
    int& width,
    int& height,
    int& pitch) {
    LOG_T("loadPngRGBA %s", path.string().c_str());
    PngCloser closer;
    closer.file = std::fopen(path.string().c_str(), "rb");
    if (!closer.file) {
        Logger::warn("Unable to open PNG: " + path.string(), __func__);
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        Logger::warn("png_create_read_struct failed for " + path.string(), __func__);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        Logger::warn("png_create_info_struct failed for " + path.string(), __func__);
        png_destroy_read_struct(&png, nullptr, nullptr);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        Logger::warn("PNG decoding failed for " + path.string(), __func__);
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }

    png_init_io(png, closer.file);
    png_read_info(png, info);

    png_uint_32 pngWidth = png_get_image_width(png, info);
    png_uint_32 pngHeight = png_get_image_height(png, info);
    png_byte colorType = png_get_color_type(png, info);
    png_byte bitDepth = png_get_bit_depth(png, info);

    if (bitDepth == 16) {
        png_set_strip_16(png);
    }
    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    png_read_update_info(png, info);
    const png_size_t rowBytes = png_get_rowbytes(png, info);
    outPixels.resize(rowBytes * pngHeight);
    std::vector<png_bytep> rows(pngHeight);
    for (png_uint_32 y = 0; y < pngHeight; ++y) {
        rows[y] = outPixels.data() + y * rowBytes;
    }
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);

    width = static_cast<int>(pngWidth);
    height = static_cast<int>(pngHeight);
    pitch = static_cast<int>(rowBytes);
    return true;
}

} // namespace gfx
