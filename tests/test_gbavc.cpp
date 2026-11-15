// PC test of the GBA Virtual Console slot choice (source/gbavc.cpp, portable part):
// synthetic two-slot container files, the CMAC results given by hand.
#include "gbavc.hpp"

#include <cstdio>
#include <cstring>

using namespace walkerbox::gbavc;

static int fails = 0;
#define CHECK(c)                                                    \
    do                                                              \
    {                                                               \
        if (!(c))                                                   \
        {                                                           \
            printf("FAIL line %d: %s\n", __LINE__, #c);             \
            fails++;                                                \
        }                                                           \
    } while (0)

static void putHeader(std::vector<u8>& f, u32 at, u32 counter, u32 size)
{
    Header h;
    memset(&h, 0xFF, sizeof(h));
    memcpy(h.magic, ".SAV", 4);
    h.contentId  = 1;
    h.savesMade  = counter;
    h.saveOffset = 0x200;
    h.saveSize   = size;
    memcpy(f.data() + at, &h, sizeof(h));
}

int main()
{
    const u32 S = 0x20000; // Gen 3: 128 KiB flash
    std::vector<u8> f(0x400 + 2 * S, 0xFF);
    u32 h1, h2;
    bool has1, has2;
    std::string why;

    // Both slots, slot 2 newer (counter + 1).
    putHeader(f, 0, 7, S);
    putHeader(f, 0x200 + S, 8, S);
    CHECK(findHeaders(f, h1, h2, has1, has2, why) && has1 && has2 && h1 == 0 && h2 == 0x200 + S);
    Pick p = choose(f, h1, h2, has1, has2, true, true);
    CHECK(p.slot == 2 && p.saveOfs == 0x400 + S && p.saveSize == S);
    // Slot 1 newer.
    putHeader(f, 0x200 + S, 6, S);
    CHECK(choose(f, h1, h2, has1, has2, true, true).slot == 1);
    // Counter wrap: slot 1 0xFFFFFFFF, slot 2 0 -> slot 2 newer.
    putHeader(f, 0, 0xFFFFFFFF, S);
    putHeader(f, 0x200 + S, 0, S);
    CHECK(choose(f, h1, h2, has1, has2, true, true).slot == 2);
    // Only one valid CMAC wins whatever the counters say.
    CHECK(choose(f, h1, h2, has1, has2, true, false).slot == 1);
    CHECK(choose(f, h1, h2, has1, has2, false, true).slot == 2);
    CHECK(choose(f, h1, h2, has1, has2, false, false).slot == 0);

    // Slot 1 never written (all FF): header 2 found after a 128 KiB slot.
    memset(f.data(), 0xFF, 0x200);
    CHECK(findHeaders(f, h1, h2, has1, has2, why) && !has1 && has2 && h2 == 0x200 + S);
    CHECK(choose(f, h1, h2, has1, has2, false, true).slot == 2);

    // Never saved (zeros).
    std::vector<u8> z(0x400 + 2 * S, 0);
    CHECK(!findHeaders(z, h1, h2, has1, has2, why));

    // Hash covers header 0x30.. + the save: changing a save byte changes it, the CMAC bytes do not.
    putHeader(f, 0, 1, S);
    auto a = slotHash(f, 0, S);
    f[0x10] ^= 0xFF; // inside the CMAC field
    CHECK(slotHash(f, 0, S) == a);
    f[0x200 + 5] ^= 0xFF;
    CHECK(slotHash(f, 0, S) != a);

    printf(fails ? "gbavc tests FAILED (%d)\n" : "gbavc tests passed\n", fails);
    return fails ? 1 : 0;
}
