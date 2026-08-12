#pragma once

#include <string>
#include <string_view>

// Converts UTF-8 user strings into the limited bitmap alphabet.
class TextNormalizer {
public:
    static std::string normalize(std::string_view text);
};
