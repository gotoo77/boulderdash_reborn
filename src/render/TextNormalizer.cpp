#include "TextNormalizer.h"

#include <string>

namespace {

bool foldCodePoint(unsigned int codePoint, std::string& out) {
    auto push = [&](char c) { out.push_back(c); };
    switch (codePoint) {
    case 0x00C0:
    case 0x00C1:
    case 0x00C2:
    case 0x00C3:
    case 0x00C4:
    case 0x00C5:
    case 0x00E0:
    case 0x00E1:
    case 0x00E2:
    case 0x00E3:
    case 0x00E4:
    case 0x00E5:
        push('A');
        return true;
    case 0x00C7:
    case 0x00E7:
        push('C');
        return true;
    case 0x00C8:
    case 0x00C9:
    case 0x00CA:
    case 0x00CB:
    case 0x00E8:
    case 0x00E9:
    case 0x00EA:
    case 0x00EB:
        push('E');
        return true;
    case 0x00CC:
    case 0x00CD:
    case 0x00CE:
    case 0x00CF:
    case 0x00EC:
    case 0x00ED:
    case 0x00EE:
    case 0x00EF:
        push('I');
        return true;
    case 0x00D1:
    case 0x00F1:
        push('N');
        return true;
    case 0x00D2:
    case 0x00D3:
    case 0x00D4:
    case 0x00D5:
    case 0x00D6:
    case 0x00F2:
    case 0x00F3:
    case 0x00F4:
    case 0x00F5:
    case 0x00F6:
        push('O');
        return true;
    case 0x00D9:
    case 0x00DA:
    case 0x00DB:
    case 0x00DC:
    case 0x00F9:
    case 0x00FA:
    case 0x00FB:
    case 0x00FC:
        push('U');
        return true;
    case 0x00DD:
    case 0x00FD:
    case 0x00FF:
    case 0x0178:
        push('Y');
        return true;
    case 0x0152:
    case 0x0153:
        push('O');
        push('E');
        return true;
    case 0x00E6:
    case 0x00C6:
        push('A');
        push('E');
        return true;
    case 0x00DF:
        push('S');
        push('S');
        return true;
    default:
        break;
    }
    return false;
}

} // namespace

std::string TextNormalizer::normalize(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            char ascii = static_cast<char>(c);
            if (ascii >= 'a' && ascii <= 'z') {
                ascii = static_cast<char>(ascii - 'a' + 'A');
            }
            normalized.push_back(ascii);
            ++i;
            continue;
        }

        unsigned int codePoint = 0;
        if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
            if ((c2 & 0xC0) != 0x80) {
                ++i;
                continue;
            }
            codePoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(text[i + 2]);
            if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) {
                ++i;
                continue;
            }
            codePoint = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            i += 3;
        } else {
            ++i;
            continue;
        }

        if (!foldCodePoint(codePoint, normalized)) {
            // Unknown character: ignore silently to avoid garbling layout.
        }
    }
    return normalized;
}
