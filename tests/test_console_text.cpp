// test_console_text.cpp - consolePlain (include/console_text.hpp, v0.92): what the console screens print for a name.
#include <cstdio>
#include "console_text.hpp"

static int fails;
static void eq(const std::string& in, const char* want)
{
    std::string got = consolePlain(in);
    if (got != want)
    {
        fails++;
        printf("FAIL: \"%s\" -> \"%s\", expected \"%s\"\n", in.c_str(), got.c_str(), want);
    }
}

int main()
{
    eq("Pok\xC3\xA9mon Diamond", "Pokemon Diamond");                // the owner's photo
    eq("POK\xC3\x89MON", "POKEMON");                                // capital E acute
    eq("Pokemon Crystal", "Pokemon Crystal");                       // ASCII unchanged
    eq("Flab\xC3\xA9" "b\xC3\xA9", "Flabebe");
    eq("\xC3\x80\xC3\xBF\xC3\x97\xC3\xB7", "Ayx/");                  // A grave, y diaeresis, times, divide
    eq("Nidoran\xE2\x99\x80", "Nidoran?");                           // 3-byte character -> one '?'
    eq("\xE3\x83\x9D\xE3\x82\xB1\xE3\x83\xA2\xE3\x83\xB3", "????"); // Japanese, 4 characters
    eq("a\xF0\x9F\x98\x80" "b", "a?b");                              // 4-byte character
    eq("a\xA9" "b", "a?b");                                          // a stray byte
    eq("a\xC3", "a?");                                               // cut at the end
    eq("", "");
    printf(fails ? "CONSOLE TEXT TESTS: %d FAILED\n" : "console text tests passed (%d failures)\n", fails);
    return fails != 0;
}
