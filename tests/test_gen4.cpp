// PC test of Gen 4 box changes (source/gamematch.cpp gameMatchFinish4) against what the GAME did, on the user's
// Checkpoint pairs (hw/gen4):
//   Platinum: pt_before_190651.sav -> pt_after_190903.sav, the game moved a Ludicolo from box 4 slot 1 to box 3 slot 1
//   Diamond:  d_before_190400.sav  -> d_after_190549.sav,  the game moved a Kadabra from box 1 slot 1 to box 18 slot 1
// v0.67: PocketPKMN saves the game's way - the changed boxes block goes into the half that is NOT current, with the
// block counter + 1; the current copy is not touched (v0.62-v0.66 changed it where it lay, as PKSM does).
//   1. PocketPKMN's way, one box per commit: put the Pokemon into its new slot, then empty the old one; the save is
//      read again after each commit, as the app does
//   2. after commit 1: the new boxes copy is in the half the GAME itself wrote its new copy to, with the game's own
//      block counter; the save number is unchanged (the game raises it only when it saves both blocks at once);
//      nothing outside that copy changed - the copy that was current before is byte for byte as it was
//   3. after commit 2: all 18 boxes are byte-identical to the boxes of the game's own after-save; both copies of both
//      blocks have valid checksums; the trainer-data blocks are untouched
//   4. the result opens with valid checksums and the Pokemon where the game put it; written out for PKHeX
// Usage: test_gen4 <before.sav> <after.sav> <fromBox> <fromSlot> <toBox> <toSlot> <out.sav>   (box/slot from 1)
#include "gamematch.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "sav/Sav4.hpp"
#include "utils/crypto.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
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

static std::unique_ptr<pksm::Sav> openSave(const Raw& r)
{
    std::shared_ptr<u8[]> b(new u8[r.size()]);
    memcpy(b.get(), r.data(), r.size());
    return pksm::Sav::getSave(b, r.size());
}

static bool isPt(const pksm::Sav& s) { return s.version() == pksm::GameVersion::Pt; }
static u32 sbo(const pksm::Sav& s) { return (u32)static_cast<const pksm::Sav4&>(s).getSBO(); }
static u32 gbo(const pksm::Sav& s) { return (u32)static_cast<const pksm::Sav4&>(s).getGBO(); }
static u32 genSize(const pksm::Sav& s) { return isPt(s) ? 0xCF2C : 0xC100; }
static u32 stoSize(const pksm::Sav& s) { return isPt(s) ? 0x121E4 : 0x121E0; }

// where the 18 boxes start in this save's current storage block
static u32 boxesAt(const pksm::Sav& s) { return sbo(s) + genSize(s) + 4; }

static u32 rd32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

// the footer of the storage block in a half: save number, block counter; and is its checksum right?
struct Foot
{
    u32 save, block;
    bool ok;
};
static Foot footOf(const u8* d, u32 start, u32 size)
{
    const u8* f = d + start + size - 0x14;
    u16 crc     = pksm::crypto::ccitt16({d + start, size - 0x14});
    return {rd32(f), rd32(f + 4), crc == (u16)(f[0x12] | (f[0x13] << 8))};
}

int main(int argc, char** argv)
{
    if (argc < 8)
    {
        printf("usage: test_gen4 <before.sav> <after.sav> <fromBox> <fromSlot> <toBox> <toSlot> <out.sav>\n");
        return 2;
    }
    Raw before = readFile(argv[1]), after = readFile(argv[2]);
    const int fb = atoi(argv[3]) - 1, fs = atoi(argv[4]) - 1, tb = atoi(argv[5]) - 1, ts = atoi(argv[6]) - 1;
    CHECK(before.size() == 0x80000 && after.size() == before.size());
    if (failures)
        return 1;
    const u32 len = (u32)before.size();
    auto sav = openSave(before);
    auto ref = openSave(after);
    CHECK(sav && sav->generation() == pksm::Generation::FOUR && sav->checksumsValid());
    CHECK(ref && ref->checksumsValid());
    if (!sav || !ref)
        return 1;
    std::string why;
    CHECK(walkerbox::gameMatchSupported(*sav, why));
    {
        // 0. PKSM-Core's checksums on the UNCHANGED save must give the game's own bytes back
        auto same = openSave(before);
        same->finishEditing();
        size_t n = 0;
        for (u32 i = 0; i < len; i++)
            n += same->rawData().get()[i] != before[i];
        printf("0. %s: re-checksumming the untouched save: %zu byte(s) differ\n",
               isPt(*sav) ? "Platinum" : "Diamond/Pearl", n);
        CHECK(n == 0);
    }
    const u32 gs = genSize(*sav), ss = stoSize(*sav);
    const u32 half0 = sbo(*sav), gen0 = gbo(*sav);
    const Foot cur0 = footOf(before.data(), half0 + gs, ss);
    const Foot game = footOf(after.data(), sbo(*ref) + gs, ss);
    auto mon = sav->pkm(fb, fs);
    CHECK(mon && mon->species() != pksm::Species::None);
    CHECK(sav->pkm(tb, ts)->species() == pksm::Species::None);
    printf("   moving species %d from box %d slot %d to box %d slot %d\n", (int)u16(mon->species()), fb + 1, fs + 1,
           tb + 1, ts + 1);
    std::string note;

    // commit 1: into the new slot
    sav->pkm(*mon, tb, ts, false);
    bool ok1 = walkerbox::gameMatchFinish4(*sav, before.data(), len, tb, note);
    printf("commit 1 (box %d): %s - %s\n", tb + 1, ok1 ? "ok" : "REFUSED", note.c_str());
    CHECK(ok1);
    Raw r1(sav->rawData().get(), sav->rawData().get() + len);
    sav = openSave(r1); // the app reads the save again after every Gen 4 write
    CHECK(sav && sav->checksumsValid());
    if (!sav)
        return 1;
    {
        const u32 half1 = sbo(*sav);
        const Foot new1 = footOf(r1.data(), half1 + gs, ss);
        printf("   new boxes copy: half %d (the game wrote its own to half %d); block counter %u -> %u (the game's: %u); "
               "save number %u -> %u (the game's full save: %u)\n",
               half1 ? 2 : 1, sbo(*ref) ? 2 : 1, cur0.block, new1.block, game.block, cur0.save, new1.save, game.save);
        CHECK(half1 != half0);                // not where the current copy lay
        CHECK(half1 == sbo(*ref));            // the half the game itself chose
        CHECK(new1.block == cur0.block + 1);  // the next block counter ...
        CHECK(new1.block == game.block);      // ... which is the game's own
        CHECK(new1.save == cur0.save);        // a one-block save keeps the save number
        CHECK(new1.ok);
        CHECK(gbo(*sav) == gen0);             // the trainer data it pairs with: the same copy as before
        size_t outside = 0;
        for (u32 i = 0; i < len; i++)
            if ((i < half1 + gs || i >= half1 + gs + ss) && r1[i] != before[i])
                outside++;
        printf("   outside the new copy: %zu byte(s) changed (the copy that was current is untouched)\n", outside);
        CHECK(outside == 0);
        CHECK(sav->pkm(tb, ts)->species() == mon->species());
    }

    // commit 2: the old slot emptied
    sav->pkm(*sav->emptyPkm(), fb, fs, false);
    bool ok2 = walkerbox::gameMatchFinish4(*sav, r1.data(), len, fb, note);
    printf("commit 2 (box %d): %s - %s\n", fb + 1, ok2 ? "ok" : "REFUSED", note.c_str());
    CHECK(ok2);
    Raw r2(sav->rawData().get(), sav->rawData().get() + len);
    sav = openSave(r2);
    CHECK(sav && sav->checksumsValid());
    if (!sav)
        return 1;

    // an edit that also touched something outside the box: that byte is put back (as the game would leave it)
    {
        auto bad = openSave(before);
        const u32 stray = gbo(*bad) + 0x20; // a trainer-data byte
        bad->pkm(*mon, tb, ts, false);
        bad->rawData().get()[stray] ^= 1;
        std::string n2;
        bool r = walkerbox::gameMatchFinish4(*bad, before.data(), len, tb, n2);
        printf("an edit that also changed the trainer data: %s, stray byte %s\n", r ? "written" : "refused",
               bad->rawData().get()[stray] == before[stray] ? "put back" : "STILL CHANGED");
        CHECK(bad->rawData().get()[stray] == before[stray]);
    }

    // the 18 boxes = the game's own
    const u8* d    = r2.data();
    const u32 ours = boxesAt(*sav), theirs = boxesAt(*ref), boxLen = 18 * 30 * 136;
    size_t diff = 0, firstDiff = 0;
    for (u32 i = 0; i < boxLen; i++)
        if (d[ours + i] != after[theirs + i] && !diff++)
            firstDiff = i;
    std::string where = diff ? " (first at box " + std::to_string(firstDiff / (30 * 136) + 1) + " slot " +
                                   std::to_string(firstDiff % (30 * 136) / 136 + 1) + " byte " +
                                   std::to_string(firstDiff % 136) + ")"
                             : "";
    printf("18 boxes vs the game's after-save: %zu byte(s) differ%s\n", diff, where.c_str());
    CHECK(diff == 0);
    // both trainer-data copies untouched; both boxes copies valid, the newest two block counters on
    size_t general = 0;
    for (u32 h = 0; h < 2; h++)
        for (u32 i = 0; i < gs; i++)
            general += d[h * 0x40000 + i] != before[h * 0x40000 + i];
    const Foot a = footOf(d, gs, ss), b = footOf(d, 0x40000 + gs, ss);
    printf("trainer-data blocks: %zu byte(s) changed; boxes copies: half 1 counter %u %s, half 2 counter %u %s\n", general,
           a.block, a.ok ? "valid" : "BAD", b.block, b.ok ? "valid" : "BAD");
    CHECK(general == 0);
    CHECK(a.ok && b.ok);
    CHECK(sbo(*sav) == half0); // two saves on: back in the half it started in
    CHECK(footOf(d, sbo(*sav) + gs, ss).block == cur0.block + 2);
    CHECK(sav->pkm(tb, ts)->species() == mon->species() && sav->pkm(fb, fs)->species() == pksm::Species::None);
    FILE* f = fopen(argv[7], "wb");
    CHECK(f && fwrite(d, 1, len, f) == len);
    if (f)
        fclose(f);
    printf(failures ? "GEN 4 TESTS: %d FAILED\n" : "gen4 box tests passed (%d failures)\n", failures);
    return failures != 0;
}
