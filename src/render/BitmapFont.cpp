#include "BitmapFont.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

#include "../util/Logger.h"
#include "TextNormalizer.h"

namespace {

const std::unordered_map<char, BitmapFont::Glyph> kGlyphs = {
    {'A', {5, {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}}},
    {'B', {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}}},
    {'C', {5, {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}}},
    {'D', {5, {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}}},
    {'E', {5, {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}}},
    {'F', {5, {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}}},
    {'G', {5, {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}}},
    {'H', {5, {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}}},
    {'I', {3, {0b111, 0b010, 0b010, 0b010, 0b010, 0b010, 0b111}}},
    {'J', {5, {0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100}}},
    {'K', {5, {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}}},
    {'L', {4, {0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b1111}}},
    {'M', {5, {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}}},
    {'N', {5, {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}}},
    {'O', {5, {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}},
    {'P', {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}}},
    {'Q', {5, {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}}},
    {'R', {5, {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}}},
    {'S', {5, {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}}},
    {'T', {5, {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}}},
    {'U', {5, {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}},
    {'V', {5, {0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b01010, 0b00100}}},
    {'W', {5, {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}}},
    {'X', {5, {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}}},
    {'Y', {5, {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}}},
    {'Z', {5, {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}}},
    {'\'', {1, {0b1, 0b1, 0b0, 0b0, 0b0, 0b0, 0b0}}},
    {' ', {3, {0, 0, 0, 0, 0, 0, 0}}},
    {'/', {3, {0b001, 0b001, 0b010, 0b010, 0b100, 0b100, 0b000}}},
    {'0', {5, {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}}},
    {'1', {3, {0b010, 0b110, 0b010, 0b010, 0b010, 0b010, 0b111}}},
    {'2', {5, {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}}},
    {'3', {5, {0b11110, 0b00001, 0b00001, 0b00110, 0b00001, 0b00001, 0b11110}}},
    {'4', {5, {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}}},
    {'5', {5, {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}}},
    {'6', {5, {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}}},
    {'7', {5, {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}}},
    {'8', {5, {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}}},
    {'9', {5, {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}}},
};

} // namespace

BitmapFont::BitmapFont(SDL_Renderer* renderer)
    : m_renderer(renderer) {
    LOG_T("BitmapFont ctor renderer=%p", static_cast<void*>(renderer));
}

void BitmapFont::drawText(int x, int y, std::string_view text, SDL_Color color, int scale) const {
    const std::string normalized = TextNormalizer::normalize(text);
    LOG_T("BitmapFont::drawText (%d,%d) scale=%d text='%s'", x, y, scale, normalized.c_str());
    if (!m_renderer || scale <= 0) {
        return;
    }
    int cursorX = x;
    for (char c : normalized) {
        const auto* glyph = glyphFor(c);
        if (!glyph) {
            cursorX += scale * 4;
            continue;
        }
        drawGlyph(cursorX, y, *glyph, color, scale);
        cursorX += (glyph->width + 1) * scale;
    }
}

int BitmapFont::textWidth(std::string_view text, int scale) const {
    const std::string normalized = TextNormalizer::normalize(text);
    LOG_T("BitmapFont::textWidth scale=%d text='%s'", scale, normalized.c_str());
    if (scale <= 0) {
        return 0;
    }
    int width = 0;
    for (char c : normalized) {
        const auto* glyph = glyphFor(c);
        if (!glyph) {
            width += scale * 4;
            continue;
        }
        width += (glyph->width + 1) * scale;
    }
    return std::max(0, width - scale);
}

int BitmapFont::lineHeight(int scale) const {
    LOG_T("BitmapFont::lineHeight scale=%d", scale);
    if (scale <= 0) {
        return 0;
    }
    return 7 * scale + scale;
}

void BitmapFont::drawGlyph(int x, int y, const Glyph& glyph, SDL_Color color, int scale) const {
    LOG_T("BitmapFont::drawGlyph (%d,%d) width=%d scale=%d", x, y, glyph.width, scale);
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    for (int row = 0; row < static_cast<int>(glyph.rows.size()); ++row) {
        const auto bits = glyph.rows[row];
        for (int col = 0; col < glyph.width; ++col) {
            if ((bits >> (glyph.width - col - 1)) & 1U) {
                SDL_Rect pixel{ x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(m_renderer, &pixel);
            }
        }
    }
}

const BitmapFont::Glyph* BitmapFont::glyphFor(char c) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const auto it = kGlyphs.find(upper);
    if (it != kGlyphs.end()) {
        return &it->second;
    }
    return nullptr;
}
