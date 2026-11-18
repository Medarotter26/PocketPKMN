// test_catches.cpp - v0.53: builds every walk catch from the PKHeX-made templates (romfs/routes/catch_*.bin) for the
// trainers of real saves, several times each (random PID/IVs, female, shiny), and writes them as files for PKHeX's
// legality check (tests/run_catches.sh runs both).
//   test_catches <catch_*.bin> <out dir> <save for version 0> [<save for version 1> ...]
// Gen 2 catches start from the save's first boxed Pokemon (its list layout and language), as in the app.
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "sav/Sav.hpp"
#include "walk_poke.hpp"

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
    if (argc < 4)
    {
        printf("usage: test_catches <catch.bin> <out dir> <save per version...>\n");
        return 2;
    }
    // v0.97: the Gen 3 event pattern check against PKHeX's verdicts on two real test catches (DPPt Wooper eggs,
    // trainer 31012 / 34015): PID 525CAACC = BACD_ES ("PID+ correlation"), PID 9B8D6670 = no pattern
    {
        const u8 bad[6] = { 29, 19, 3, 5, 18, 13 }, good[6] = { 22, 28, 7, 13, 25, 26 }; // HP Atk Def SpA SpD Spe
        if (!walkerbox::pidIvLooksLikeEvent3(0x525CAACCu, bad, 31012 ^ 34015) ||
            walkerbox::pidIvLooksLikeEvent3(0x9B8D6670u, good, 31012 ^ 34015))
        {
            printf("FAIL: Gen 3 event pattern check disagrees with PKHeX\n");
            return 1;
        }
        // made the way PKHeX's CommonEvent3 makes them (X, A, B calls, then the two IV calls): each must be found
        int missed = 0;
        for (u32 k = 1; k <= 300; k++)
        {
            auto next = [](u32 s) { return s * 0x41C64E6Du + 0x6073u; };
            const u32 sX = k * 2654435761u, sA = next(sX), sB = next(sA), s1 = next(sB), s2 = next(s1);
            const u32 x16 = sX >> 16, a16 = sA >> 16, b16 = sB >> 16, v1 = (s1 >> 16) & 0x7FFF, v2 = (s2 >> 16) & 0x7FFF;
            const u8 iv[6] = { (u8)(v1 & 31), (u8)((v1 >> 5) & 31), (u8)((v1 >> 10) & 31), (u8)((v2 >> 5) & 31),
                               (u8)((v2 >> 10) & 31), (u8)(v2 & 31) };
            const u16 idXor = (u16)(k * 40503u);
            const u32 regular = (a16 << 16) | b16;
            const u32 shinyHere = (x16 << 16) | (((idXor ^ x16) & 0xFFF8) | (b16 & 7));                // BACD_S
            const u32 shinyElse = (x16 << 16) | (((idXor ^ 0x5A58u ^ x16) & 0xFFF8) | (b16 & 7));     // BACD_ES
            if (!walkerbox::pidIvLooksLikeEvent3(regular, iv, idXor) || !walkerbox::pidIvLooksLikeEvent3(shinyHere, iv, idXor) ||
                !walkerbox::pidIvLooksLikeEvent3(shinyElse, iv, idXor))
                missed++;
        }
        if (missed)
        {
            printf("FAIL: Gen 3 event pattern check missed %d of 300 made cases\n", missed);
            return 1;
        }
    }
    std::vector<u8> bin = readFile(argv[1]);
    if (bin.size() < 6 || memcmp(bin.data(), "PPCT", 4))
    {
        printf("FAIL: %s is not a catch template file\n", argv[1]);
        return 1;
    }
    int nver = bin[4] | (bin[5] << 8);
    std::vector<std::unique_ptr<pksm::Sav>> savs;
    std::vector<std::shared_ptr<u8[]>> keep;
    for (int v = 0; v < nver; v++)
    {
        const char* path = argv[3 + (v < argc - 3 ? v : argc - 4)];
        std::vector<u8> d = readFile(path);
        size_t len = d.size();
        if (len == 0x8010) // Virtual Console .sav as copied from the 3DS: 0x10 trailer
            len = 0x8000;
        std::shared_ptr<u8[]> buf(new u8[len]);
        memcpy(buf.get(), d.data(), len);
        keep.push_back(buf);
        savs.push_back(pksm::Sav::getSave(buf, (u32)len));
        if (!savs.back())
        {
            printf("FAIL: cannot read save %s\n", path);
            return 1;
        }
    }
    int built = 0, fails = 0;
    size_t at = 6;
    while (at + 8 <= bin.size())
    {
        int vi = bin[at], course = bin[at + 1], poke = bin[at + 2];
        walkerbox::CatchTemplate t;
        t.gen = bin[at + 3];
        t.flags = bin[at + 4];
        t.fixedIVs = bin[at + 5];
        t.len = bin[at + 6] | (bin[at + 7] << 8);
        t.data = bin.data() + at + 8;
        at += 8 + t.len;
        pksm::Sav& sav = *savs[vi];
        walkerbox::CatchTrainer tr;
        tr.name = sav.otName();
        if (const char* ot = getenv("CATCH_OT")) // v0.96: a trainer name of another length (a short name: Gen 3 trash bytes)
            tr.name = ot;
        tr.tid = sav.TID();
        tr.sid = t.gen == 2 ? 0 : sav.SID();
        tr.female = sav.gender() == pksm::Gender::Female;
        tr.language = sav.language();
        if (t.gen == 6 || t.gen == 7)
        {
            tr.country = sav.country();
            tr.subRegion = sav.subRegion();
            tr.consoleRegion = sav.consoleRegion();
        }
        std::unique_ptr<pksm::PKX> shell;
        if (t.gen == 2)
            for (int b = 0; b < sav.maxBoxes() && !shell; b++)
                for (int s = 0; s < 20 && !shell; s++)
                {
                    auto p = sav.pkm((u8)b, (u8)s);
                    if (p && p->species() != pksm::Species::None)
                        shell = std::move(p);
                }
        for (int k = 0; k < 4; k++)
        {
            bool shiny = k == 3, female = (k & 1) != 0;
            std::unique_ptr<pksm::PKX> pk;
            std::string why;
            u32 seed = (u32)(vi * 1000003u + course * 7919u + poke * 104729u + k * 2654435761u);
            if (!walkerbox::buildCatch(t, tr, shell.get(), shiny, female, seed, 15, 11, 2026, pk, why))
            {
                printf("FAIL: v%d course %d poke %d: %s\n", vi, course, poke, why.c_str());
                fails++;
                continue;
            }
            char name[256];
            snprintf(name, sizeof(name), "%s/v%d_c%02d_p%d_%d.pk%d", argv[2], vi, course, poke, k, t.gen);
            auto raw = pk->rawData();
            FILE* f = fopen(name, "wb");
            if (!f || fwrite(raw.data(), 1, raw.size(), f) != raw.size())
            {
                printf("FAIL: write %s\n", name);
                fails++;
            }
            if (f)
                fclose(f);
            built++;
        }
    }
    printf("%d catch(es) built, %d failure(s)\n", built, fails);
    return fails != 0;
}
