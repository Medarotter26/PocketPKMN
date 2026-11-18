// console_text.hpp - v0.92 (owner's photo: "Pok├⌐mon Diamond" in Move Pokemon's "Saves found" list; owner: "Yes, fix
// it"). The libctru console draws one glyph per byte, so a UTF-8 letter such as "é" (2 bytes, C3 A9) came out as two
// wrong symbols. consolePlain() is for the console (text) screens only: accented Latin letters become the plain letter
// ("Pokémon" -> "Pokemon"), any other non-ASCII character becomes '?'. The stored names, the GPU menus (which draw
// "é" themselves) and the saves are not changed. Header-only so the PC test (tests/test_console_text.cpp) can use it.
#pragma once
#include <string>

inline std::string consolePlain(const std::string& s)
{
    // U+00C0-U+00FF -> a plain letter ('x' for the multiplication sign, '/' for division)
    static const char kLatin1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
        {
            out += (char)c;
            i++;
            continue;
        }
        size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (len == 2 && i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80)
        {
            unsigned cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
            out += (cp >= 0xC0 && cp <= 0xFF) ? kLatin1[cp - 0xC0] : '?';
        }
        else
            out += '?';
        // skip the whole character (or one stray byte)
        size_t j = i + 1;
        while (j < s.size() && j < i + len && ((unsigned char)s[j] & 0xC0) == 0x80)
            j++;
        i = j;
    }
    return out;
}
