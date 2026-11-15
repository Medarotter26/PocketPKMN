// PC test of Gen 7 box changes (source/gamematch.cpp gameMatchFinish7) against what the GAME did, on the user's
// Moon pair made with Checkpoint (hw/gen7): before = moon_before_160900.main, after = moon_after_161029.main, where
// the game moved a Pichu from box 4 slot 5 to box 3 slot 6 and saved.
//   1. PocketPKMN's way, one box per commit: put the Pichu into box 3 slot 6, then empty box 4 slot 5
//   2. each commit changes only its box, the box block's checksum and the signature (else refused)
//   3. the whole box block afterwards is byte-identical to the game's own after-save
//   4. writes the result (out file) for PKHeX: checksums valid, and PKHeX's own re-signing changes nothing
// Usage: test_gen7 <before.main> <after.main> <out.main>
#include "gamematch.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using Raw = std::vector<u8>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static Raw readFile(const char* p)
{
    Raw d;
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

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        printf("usage: test_gen7 <before.main> <after.main> <out.main>\n");
        return 2;
    }
    Raw before = readFile(argv[1]), after = readFile(argv[2]);
    CHECK(before.size() == 0x6BE00 && after.size() == before.size());
    if (failures)
        return 1;
    std::shared_ptr<u8[]> buf(new u8[before.size()]);
    memcpy(buf.get(), before.data(), before.size());
    auto sav = pksm::Sav::getSave(buf, before.size());
    CHECK(sav && sav->generation() == pksm::Generation::SEVEN && sav->checksumsValid());
    if (!sav)
        return 1;
    const u32 len = (u32)before.size();
    {
        // 0. PKSM-Core's checksums + signature on the UNCHANGED save must give the game's own bytes back
        std::shared_ptr<u8[]> c(new u8[len]);
        memcpy(c.get(), before.data(), len);
        auto same = pksm::Sav::getSave(c, len);
        same->finishEditing();
        size_t n = 0, first = 0;
        for (u32 i = 0; i < len; i++)
            if (same->rawData().get()[i] != before[i] && !n++)
                first = i;
        printf("0. re-signing the untouched save with PKSM-Core: %zu byte(s) differ%s\n", n,
               n ? (" (first at " + std::to_string(first) + ")").c_str() : " - the game's own signature");
        // since the v0.56 change to PKSM-Core's resign (hash + zeros, as the game): byte-identical to the game
        CHECK(n == 0);
        std::string p = std::string(argv[3]) + ".resigned";
        FILE* rf = fopen(p.c_str(), "wb");
        if (rf)
        {
            fwrite(same->rawData().get(), 1, len, rf);
            fclose(rf);
        }
    }
    auto pichu = sav->pkm(3, 4);
    CHECK(pichu && pichu->species() == pksm::Species::Pichu);
    CHECK(sav->pkm(2, 5)->species() == pksm::Species::None);
    std::string note;

    // commit 1: the Pichu into box 3 slot 6
    Raw snap(sav->rawData().get(), sav->rawData().get() + len);
    sav->pkm(*pichu, 2, 5, false);
    bool ok1 = walkerbox::gameMatchFinish7(*sav, snap.data(), len, 2, note);
    printf("commit 1 (box 3): %s - %s\n", ok1 ? "ok" : "REFUSED", note.c_str());
    CHECK(ok1);

    // commit 2: box 4 slot 5 emptied
    snap.assign(sav->rawData().get(), sav->rawData().get() + len);
    sav->pkm(*sav->emptyPkm(), 3, 4, false);
    bool ok2 = walkerbox::gameMatchFinish7(*sav, snap.data(), len, 3, note);
    printf("commit 2 (box 4): %s - %s\n", ok2 ? "ok" : "REFUSED", note.c_str());
    CHECK(ok2);

    // the box block = the game's own
    const u8* d   = sav->rawData().get();
    const u32 box = 0x4E00, boxLen = 0x36600;
    size_t diff = 0;
    for (u32 i = box; i < box + boxLen; i++)
        diff += d[i] != after[i];
    printf("box block vs the game's after-save: %zu byte(s) differ\n", diff);
    CHECK(diff == 0);
    // outside box block + table + signature: still the before-save
    size_t outside = 0;
    for (u32 i = 0; i < len; i++)
        if ((i < box || i >= box + boxLen) && i < len - 0x300 && d[i] != before[i])
            outside++;
    printf("outside the box block / table / signature: %zu byte(s) changed\n", outside);
    CHECK(outside == 0);
    {
        std::shared_ptr<u8[]> chk(new u8[len]);
        memcpy(chk.get(), d, len);
        auto re = pksm::Sav::getSave(chk, len);
        CHECK(re && re->checksumsValid());
        CHECK(re && re->pkm(2, 5)->species() == pksm::Species::Pichu && re->pkm(3, 4)->species() == pksm::Species::None);
    }
    FILE* f = fopen(argv[3], "wb");
    CHECK(f && fwrite(d, 1, len, f) == len);
    if (f)
        fclose(f);
    printf(failures ? "GEN 7 TESTS: %d FAILED\n" : "gen7 box tests passed (%d failures)\n", failures);
    return failures != 0;
}
