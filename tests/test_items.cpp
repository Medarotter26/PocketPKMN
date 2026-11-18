// test_items.cpp - v0.53: walk items into real saves (Gen 2: PC item storage, X/Y: bag), the way the game would
// store them (gameMatchItems). For each save: add a course item and a gift item, check that only one item list and
// its checksum changed, read the result back with PKSM-Core (checksum valid, items there) and write it out for
// PKHeX (tests/run_catches.sh opens it with ~/pkhex-check items).
//   test_items <out dir> <save>...
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamematch.hpp"
#include "sav/Item.hpp"
#include "sav/Sav.hpp"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static std::vector<u8> readFile(const char* p)
{
    std::vector<u8> d;
    FILE* f = fopen(p, "rb");
    if (!f)
        return d;
    fseek(f, 0, SEEK_END);
    d.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(d.data(), 1, d.size(), f) != d.size())
        d.clear();
    fclose(f);
    return d;
}

static int countOf(pksm::Sav& sav, u16 nationalId, bool pc)
{
    using P = pksm::Sav::Pouch;
    int total = 0;
    for (auto& pr : sav.pouches())
    {
        if (pc != (pr.first == P::PCItem))
            continue;
        for (int s = 0; s < pr.second; s++)
        {
            auto it = sav.item(pr.first, (u16)s);
            if (it->id() == nationalId)
                total += it->count();
        }
    }
    return total;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("usage: test_items <out dir> <save>...\n");
        return 2;
    }
    for (int a = 2; a < argc; a++)
    {
        std::vector<u8> d = readFile(argv[a]);
        size_t len = d.size() == 0x8010 ? 0x8000 : d.size();
        std::shared_ptr<u8[]> buf(new u8[len]);
        memcpy(buf.get(), d.data(), len);
        auto sav = pksm::Sav::getSave(buf, (u32)len);
        CHECK(sav, "%s: cannot read", argv[a]);
        if (!sav)
            continue;
        bool g2 = sav->generation() == pksm::Generation::TWO;
        bool pc = g2 || sav->generation() == pksm::Generation::THREE; // v0.94: Gen 3 (FRLG) has an item PC too
        std::vector<u8> before(sav->rawData().get(), sav->rawData().get() + len);
        // a course item (Gen 2: Everstone = Gen 2 id 112; X/Y: Everstone national 229) and a gift (national Potion 17)
        u16 courseId = g2 ? 112 : 229;
        int everBefore = countOf(*sav, 229, pc), potionBefore = countOf(*sav, 17, pc);
        std::string where;
        CHECK(walkerbox::addItemToSave(*sav, courseId, g2, where), "%s: course item: %s", argv[a], where.c_str());
        CHECK(walkerbox::addItemToSave(*sav, 17, false, where), "%s: gift item: %s", argv[a], where.c_str());
        // one item list at a time, as the game stores them one by one: commit after each would be the same rule;
        // here both land in the same list (Gen 2 PC) or two pockets (X/Y) - check them one at a time
        std::string note;
        if (pc) // both items in the one PC list (Gen 2; v0.94 Gen 3)
            CHECK(walkerbox::gameMatchItems(*sav, before.data(), (u32)len, note), "%s: %s", argv[a], note.c_str());
        else
        {
            // X/Y: Everstone (Items pocket) and Potion (Medicine pocket) are different pockets: match each separately
            std::vector<u8> after(sav->rawData().get(), sav->rawData().get() + len);
            memcpy(sav->rawData().get(), before.data(), len);
            sav->beginEditing();
            CHECK(walkerbox::addItemToSave(*sav, courseId, false, where), "re-add");
            CHECK(walkerbox::gameMatchItems(*sav, before.data(), (u32)len, note), "%s: %s", argv[a], note.c_str());
            std::vector<u8> mid(sav->rawData().get(), sav->rawData().get() + len);
            if (sav->generation() == pksm::Generation::FOUR)
            {
                // v0.67: a Gen 4 item goes into a new trainer-data copy in the other half; the app reads the save
                // again after every such write, and so does this test
                std::shared_ptr<u8[]> re(new u8[len]);
                memcpy(re.get(), mid.data(), len);
                sav = pksm::Sav::getSave(re, (u32)len);
                CHECK(sav && sav->checksumsValid(), "%s: not readable after the first item", argv[a]);
                if (!sav)
                    continue;
            }
            CHECK(walkerbox::addItemToSave(*sav, 17, false, where), "re-add 2");
            CHECK(walkerbox::gameMatchItems(*sav, mid.data(), (u32)len, note), "%s: %s", argv[a], note.c_str());
        }
        printf("%s: %s\n", argv[a], note.c_str());
        // read back
        std::shared_ptr<u8[]> back(new u8[len]);
        memcpy(back.get(), sav->rawData().get(), len);
        auto again = pksm::Sav::getSave(back, (u32)len);
        CHECK(again && again->checksumsValid(), "%s: checksums not valid after the items", argv[a]);
        if (again)
        {
            CHECK(countOf(*again, 229, pc) == everBefore + 1, "%s: Everstone %d -> %d", argv[a], everBefore,
                  countOf(*again, 229, pc));
            CHECK(countOf(*again, 17, pc) == potionBefore + 1, "%s: Potion %d -> %d", argv[a], potionBefore,
                  countOf(*again, 17, pc));
        }
        char out[512];
        const char* base = strrchr(argv[a], '/');
        snprintf(out, sizeof(out), "%s/%s", argv[1], base ? base + 1 : argv[a]);
        FILE* f = fopen(out, "wb");
        if (f)
        {
            fwrite(sav->rawData().get(), 1, len, f);
            fclose(f);
        }
    }
    printf(fails ? "ITEM TESTS: %d FAILED\n" : "item tests passed (%d failures)\n", fails);
    return fails != 0;
}
