// test_frlg.cpp - v0.94 (FRLG courses; the owner has no FireRed / LeafGreen save: "could you make one and test it all
// on your end?"). On PKHeX-made FireRed / LeafGreen saves (pkhex-check blank3, tests/run_catches.sh):
//   - PKSM-Core reads them as FireRed / LeafGreen (Gen 3, version FR - the two look alike inside), badges as made;
//   - the Hall of Fame flag the app reads (game clear: FRLG 0x82C, RS 0x804, E 0x864; hallOfFameOf) is set only where made so;
//   - a box move saved the game's way (gameMatchFinish3: a new copy in the other slot) reads back with the Pokemon
//     moved and valid checksums; the result is written out for PKHeX (run_catches.sh: pkhex-check items).
//   - the box Pokemon after a walk (EXP, friendship, EVs) is written out for PKHeX (pkhex-check pk 3).
//   test_frlg <out dir> <save> <badges> <hof 0/1> [<save> <badges> <hof>]...
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamematch.hpp"
#include "walk_poke.hpp"
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

int main(int argc, char** argv)
{
    if (argc < 5 || (argc - 2) % 3)
    {
        printf("usage: test_frlg <out dir> <save> <badges> <hof 0/1> ...\n");
        return 2;
    }
    for (int a = 2; a + 2 < argc; a += 3)
    {
        const char* path = argv[a];
        int wantBadges = atoi(argv[a + 1]), wantHof = atoi(argv[a + 2]);
        std::vector<u8> d = readFile(path);
        CHECK(d.size() == 0x20000, "%s: %zu bytes, not a 128 KiB Gen 3 save", path, d.size());
        if (d.size() != 0x20000)
            continue;
        std::shared_ptr<u8[]> buf(new u8[d.size()]);
        memcpy(buf.get(), d.data(), d.size());
        auto sav = pksm::Sav::getSave(buf, (u32)d.size());
        // v0.97: every Gen 3 game (RS / E / FRLG) - its flags place, game clear flag and first badge flag, as the app uses
        CHECK(sav && sav->generation() == pksm::Generation::THREE, "%s: not read as a Gen 3 save", path);
        const bool isE = sav && sav->version() == pksm::GameVersion::E, isFrlg = sav && sav->version() == pksm::GameVersion::FR;
        const u32 flagsAt = isFrlg ? 0xEE0 : isE ? 0x1270 : 0x1220;
        const int clearFlag = isFrlg ? 0x82C : isE ? 0x864 : 0x804, badge0 = isFrlg ? 0x820 : isE ? 0x867 : 0x807;
        if (!sav)
            continue;
        CHECK(sav->badges() == wantBadges, "%s: %d badges, expected %d", path, sav->badges(), wantBadges);
        bool hof = walkerbox::gen3Flag(d.data(), (u32)d.size(), flagsAt, clearFlag) == 1; // what storage.cpp reads
        int flagBadges = 0; // the same reader on the badge flags must agree with PKSM-Core's badges()
        for (int b = 0; b < 8; b++)
            flagBadges += walkerbox::gen3Flag(d.data(), (u32)d.size(), flagsAt, badge0 + b) == 1;
        CHECK(flagBadges == sav->badges(), "%s: flag reader says %d badges, PKSM-Core %d", path, flagBadges, sav->badges());
        CHECK(hof == (wantHof != 0), "%s: Hall of Fame flag %d, expected %d", path, hof, wantHof);
        uint32_t counter = 0;
        int rotation     = 0;
        int slot         = walkerbox::gen3CurrentSlot(d.data(), counter, rotation);
        CHECK(slot == 0, "%s: current slot %d (counter %u)", path, slot, counter);

        // a box move: box 1 slot 1 -> box 1 slot 6, as Move Pokemon does it, saved the game's way
        auto pk = sav->pkm(0, 0);
        CHECK(pk && pk->species() != pksm::Species::None, "%s: no Pokemon in box 1 slot 1", path);
        if (!pk || pk->species() == pksm::Species::None)
            continue;
        u16 species = u16(pk->species());
        u32 pid     = pk->PID();
        std::vector<u8> before(sav->rawData().get(), sav->rawData().get() + d.size());
        sav->pkm(*pk, 0, 5, false);
        sav->pkm(*sav->emptyPkm(), 0, 0, false);
        std::string note;
        CHECK(walkerbox::gameMatchFinish3(*sav, before.data(), (u32)d.size(), note), "%s: %s", path, note.c_str());
        printf("%s: %d badge(s), Hall of Fame %s; %s\n", path, sav->badges(), hof ? "yes" : "no", note.c_str());
        std::shared_ptr<u8[]> back(new u8[d.size()]);
        memcpy(back.get(), sav->rawData().get(), d.size());
        auto again = pksm::Sav::getSave(back, (u32)d.size());
        CHECK(again && again->checksumsValid(), "%s: not valid after the move", path);
        if (again)
        {
            auto moved = again->pkm(0, 5), gone = again->pkm(0, 0);
            CHECK(moved && u16(moved->species()) == species && moved->PID() == pid, "%s: Pokemon not in slot 6", path);
            CHECK(gone && gone->species() == pksm::Species::None, "%s: slot 1 not empty", path);
            CHECK(again->badges() == wantBadges &&
                      (walkerbox::gen3Flag(back.get(), (u32)d.size(), flagsAt, clearFlag) == 1) == (wantHof != 0),
                "%s: badges / Hall of Fame changed by the move", path);
            uint32_t c2 = 0;
            int r2      = 0;
            CHECK(walkerbox::gen3CurrentSlot(back.get(), c2, r2) == 1 && c2 == counter + 1, "%s: new copy not in slot B",
                path);
        }
        // a walk: the Pokemon comes back with the walk's EXP / friendship (/ EVs where allowed), as storage_walk_return
        // makes it; written out for PKHeX (run_catches.sh: pkhex-check pk 3)
        for (int ev : {(int)walkerbox::EV_OFF, (int)walkerbox::EV_HP})
        for (u32 steps : {137u, 5000u, 250000u})
        {
            auto walked = pk->clone();
            walkerbox::WalkGains g;
            CHECK(walkerbox::applyWalkGains(*walked, 3, steps, ev, g), "%s: walk gains", path);
            char wname[512];
            const char* wb = strrchr(path, '/');
            snprintf(wname, sizeof(wname), "%s/%s_%u_%s.pk3", argv[1], ev < 0 ? "walked" : "evwalk", steps, wb ? wb + 1 : path);
            auto raw = walked->rawData();
            FILE* wf = fopen(wname, "wb");
            if (wf)
            {
                fwrite(raw.data(), 1, raw.size(), wf);
                fclose(wf);
            }
        }
        char out[512];
        const char* base = strrchr(path, '/');
        snprintf(out, sizeof(out), "%s/moved_%s", argv[1], base ? base + 1 : path);
        FILE* f = fopen(out, "wb");
        if (f)
        {
            fwrite(sav->rawData().get(), 1, d.size(), f);
            fclose(f);
        }
    }
    printf(fails ? "FRLG TESTS: %d FAILED\n" : "FRLG save tests passed (%d failures)\n", fails);
    return fails != 0;
}
