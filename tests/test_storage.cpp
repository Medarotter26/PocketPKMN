// PC test of WalkerBox storage on real Gen 1 / Gen 2 (international) saves, with
// PKSM-Core and the app's own game-matching code (source/gamematch.cpp):
//   1. deposit + withdraw back in the OPEN box  -> the save must be byte-identical
//   2. put a copy into a box that is NOT open    -> only that box (Gen 1: + its sums) changes
//   3. put a copy into the OPEN box              -> only the open box (Gen 1: + main sum) changes
// After 2 and 3 the save is read again: the copy is where it should be, the other
// boxes are unchanged, and the moved Pokemon is a byte-exact copy.
// Usage: test_storage <save.sav> [...]   (run from tests/run_storage.sh)

#include "gamematch.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"
#include "utils/crypto.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

using Raw = std::vector<unsigned char>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static Raw rawOf(const pksm::PKX* pk)
{
    if (!pk || pk->species() == pksm::Species::None)
        return {};
    auto r = pk->rawData();
    return Raw(r.begin(), r.end());
}

static std::vector<std::vector<Raw>> boxes(const pksm::Sav& sav)
{
    int n = sav.maxBoxes(), per = n ? sav.maxSlot() / n : 0;
    std::vector<std::vector<Raw>> out((size_t)n);
    for (int b = 0; b < n; b++)
        for (int i = 0; i < per; i++)
        {
            Raw r = rawOf(sav.pkm((u8)b, (u8)i).get());
            if (!r.empty())
                out[(size_t)b].push_back(r);
        }
    return out;
}

static int firstFree(const pksm::Sav& sav, int box)
{
    int per = sav.maxSlot() / sav.maxBoxes();
    for (int i = 0; i < per; i++)
        if (rawOf(sav.pkm((u8)box, (u8)i).get()).empty())
            return i;
    return -1;
}

static std::unique_ptr<pksm::Sav> fresh(const std::vector<u8>& bytes)
{
    std::shared_ptr<u8[]> d(new u8[bytes.size()]);
    memcpy(d.get(), bytes.data(), bytes.size());
    return pksm::Sav::getSave(d, bytes.size());
}

using Ranges = std::vector<std::pair<size_t, size_t>>; // [start, end)

// What the game changes when one Pokemon goes into / out of `box` (see gamematch.cpp).
static Ranges allowed(const pksm::Sav& sav, int box, int open)
{
    bool gen1 = sav.generation() == pksm::Generation::ONE;
    if (gen1)
    {
        if (box == open)
            return {{0x30C0, 0x3522}, {0x3523, 0x3524}};
        size_t bank = (size_t)box / 6, idx = (size_t)box % 6, base = bank ? 0x6000 : 0x4000;
        size_t sum = bank ? 0x7A4C : 0x5A4C;
        return {{base + idx * 0x462, base + (idx + 1) * 0x462}, {sum, sum + 1}, {sum + 1 + idx, sum + 2 + idx}};
    }
    size_t base = box < 7 ? 0x4000 : 0x6000;
    size_t st   = base + (size_t)(box % 7) * 0x450;
    if (box == open)
    {
        size_t a = sav.version() == pksm::GameVersion::C ? 0x2D10 : 0x2D6C;
        return {{a, a + 0x44E}, {st, st + 0x44E}};
    }
    return {{st, st + 0x44E}};
}

// What the game does when it starts (test S4, Crystal): a Gen 2 game reloads the
// open box from its storage slot into the Active Box area. Re-reading the save
// after this shows what the player will really see.
static void gameBoot(std::vector<u8>& bytes)
{
    auto s = fresh(bytes);
    if (!s || s->generation() != pksm::Generation::TWO)
        return;
    int open      = s->currentBox();
    size_t active = s->version() == pksm::GameVersion::C ? 0x2D10 : 0x2D6C;
    size_t st     = (open < 7 ? 0x4000 : 0x6000) + (size_t)(open % 7) * 0x450;
    // Box data only (0x44E); the 2 filler bytes are left out so the test measures the
    // app's changes, not the simulation's.
    std::copy(bytes.begin() + (long)st, bytes.begin() + (long)st + 0x44E, bytes.begin() + (long)active);
}

static size_t outside(const std::vector<u8>& a, const u8* b, const Ranges& keep, size_t& total)
{
    size_t bad = 0;
    total      = 0;
    for (size_t i = 0; i < a.size(); i++)
    {
        if (a[i] == b[i])
            continue;
        total++;
        bool ok = false;
        for (auto [s, e] : keep)
            if (i >= s && i < e)
                ok = true;
        if (!ok && ++bad <= 8)
            printf("    byte 0x%04zx changed outside the allowed area: %02x -> %02x\n", i, a[i], b[i]);
    }
    return bad;
}

// Put `moved` into `box` of a fresh copy of `original`, finish it like the app,
// and check bytes and contents.
static void putCopy(const std::vector<u8>& original, const Raw& moved, int box, const char* label)
{
    auto s = fresh(original);
    int open = s->currentBox();
    int slot = firstFree(*s, box);
    if (slot < 0)
    {
        printf("  %s: box %d is full - skipped\n", label, box + 1);
        return;
    }
    auto before = boxes(*s);
    Raw stored  = moved;
    auto pk     = pksm::PKX::getPKM(s->generation(), stored.data(), stored.size());
    s->pkm(*pk, (u8)box, (u8)slot, false);
    std::string note;
    unsigned put = walkerbox::gameMatchFinish(*s, (uint32_t)original.size(), box, note);
    size_t total = 0;
    size_t bad   = outside(original, s->rawData().get(), allowed(*s, box, open), total);
    printf("  %s: box %d slot %d; %s; %u byte(s) put back; %zu byte(s) differ, %zu outside the allowed area\n",
        label, box + 1, slot + 1, note.c_str(), put, total, bad);
    CHECK(bad == 0);
    std::vector<u8> now(s->rawData().get(), s->rawData().get() + original.size());
    gameBoot(now); // what the player sees after the game has started
    auto again = fresh(now);
    CHECK(again != nullptr);
    if (!again)
        return;
    CHECK(again->checksumsValid());
    auto after = boxes(*again);
    CHECK(std::find(after[(size_t)box].begin(), after[(size_t)box].end(), moved) != after[(size_t)box].end());
    for (size_t b = 0; b < after.size(); b++)
        if ((int)b != box)
            CHECK(after[b] == before[b]);
}

static void testSave(const char* path)
{
    printf("== %s\n", path);
    FILE* f = fopen(path, "rb");
    CHECK(f != nullptr);
    if (!f)
        return;
    std::vector<u8> original;
    int c;
    while ((c = fgetc(f)) != EOF)
        original.push_back((u8)c);
    fclose(f);

    auto sav = fresh(original);
    CHECK(sav != nullptr);
    if (!sav)
        return;
    std::string why;
    CHECK(walkerbox::gameMatchSupported(*sav, why));
    int open = sav->currentBox(), nboxes = sav->maxBoxes();
    printf("  %zu bytes, Gen %d, %d boxes of %d, open box %d, checksums %s\n", original.size(),
        sav->generation() == pksm::Generation::ONE ? 1 : 2, nboxes, sav->maxSlot() / nboxes, open + 1,
        sav->checksumsValid() ? "valid" : "INVALID");

    // A Pokemon to move: the LAST one in the open box (boxes are lists, so taking it
    // out and putting it back returns it to the same place - only then must the
    // save come out byte-identical), else the first one anywhere.
    auto all   = boxes(*sav);
    int srcBox = all[(size_t)open].empty() ? -1 : open;
    for (int b = 0; b < nboxes && srcBox < 0; b++)
        if (!all[(size_t)b].empty())
            srcBox = b;
    CHECK(srcBox >= 0);
    if (srcBox < 0)
        return;
    Raw moved = srcBox == open ? all[(size_t)open].back() : all[(size_t)srcBox][0];

    // 1. Deposit + withdraw back, open box.
    if (srcBox == open)
    {
        int per = sav->maxSlot() / nboxes, at = -1;
        for (int i = 0; i < per && at < 0; i++)
            if (rawOf(sav->pkm((u8)open, (u8)i).get()) == moved)
                at = i;
        sav->pkm(*sav->emptyPkm(), (u8)open, (u8)at, false);
        std::string n1;
        walkerbox::gameMatchFinish(*sav, (uint32_t)original.size(), open, n1);
        sav->beginEditing();
        std::vector<u8> mid(sav->rawData().get(), sav->rawData().get() + original.size());
        gameBoot(mid);
        auto s2  = fresh(mid);
        // The deposit must really have happened as the player sees it (test S4: it did not).
        auto gone = boxes(*s2);
        size_t had = all[(size_t)open].size();
        printf("  1a. after the deposit and a game start: open box holds %zu (had %zu)\n",
            gone[(size_t)open].size(), had);
        CHECK(gone[(size_t)open].size() + 1 == had);
        CHECK(std::find(gone[(size_t)open].begin(), gone[(size_t)open].end(), moved) == gone[(size_t)open].end());
        Raw back = moved;
        auto pk  = pksm::PKX::getPKM(s2->generation(), back.data(), back.size());
        int free = firstFree(*s2, open);
        s2->pkm(*pk, (u8)open, (u8)free, false);
        std::string n2;
        walkerbox::gameMatchFinish(*s2, (uint32_t)original.size(), open, n2);
        size_t total = 0;
        outside(original, s2->rawData().get(), {}, total);
        printf("  1. open box deposit + withdraw: %zu byte(s) differ from the original (%s)\n", total, n2.c_str());
        CHECK(total == 0);
    }
    else
    {
        printf("  1. skipped: the open box is empty\n");
    }

    // 2. A box that is not open.
    putCopy(original, moved, (open + 1) % nboxes, "2. copy into a box that is not open");
    // 3. The open box.
    putCopy(original, moved, open, "3. copy into the open box");
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        testSave(argv[i]);
    if (failures)
    {
        printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("storage tests passed\n");
    return 0;
}
