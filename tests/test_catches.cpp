// test_catches.cpp - v0.53: builds every walk catch from the PKHeX-made templates (romfs/routes/catch_*.bin) for the
// trainers of real saves, several times each (random PID/IVs, female, shiny), and writes them as files for PKHeX's
// legality check (tests/run_catches.sh runs both).
//   test_catches <catch_*.bin> <out dir> <save for version 0> [<save for version 1> ...]
// Gen 2 catches start from the save's first boxed Pokemon (its list layout and language), as in the app.
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
        tr.tid = sav.TID();
        tr.sid = t.gen == 2 ? 0 : sav.SID();
        tr.female = sav.gender() == pksm::Gender::Female;
        tr.language = sav.language();
        if (t.gen == 6)
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
