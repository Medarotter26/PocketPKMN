// test_evwalk.cpp - 2026-11-17 (owner: "I want you to thoroughly check the evs"). Walk EV training (applyWalkGains,
// +1 EV per 20 steps on the chosen stat, EXP = steps, at most one level per walk) on every box Pokemon of the saves
// given. Writes each Pokemon before the walk and after each walk variant, for PKHeX (tests/run_evwalk.sh) to judge
// which Pokemon were legal before and are not after.
// v0.95: applyWalkGains itself now limits Gen 3/4 EVs (walkEvsFitExp), so every variant is the app's real walk; the
// "cap" variants keep this file's own, separately written copy of the rule as a cross-check (both must give 0).
//   test_evwalk <out dir> <save>...   (.sav / .main / .gbavc)
// Files: <out>/s<save>_p<n>_base.pk<gen>, s<save>_p<n>_<stat>_<steps>.pk<gen> (one walk), s<save>_p<n>_<stat>_x<k>.pk<gen>
// (k walks of 5,000 steps in a row).
#include "gbavc.hpp"
#include "sav/Sav.hpp"
#include "walk_poke.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

static std::vector<u8> readAll(const char* path)
{
    std::vector<u8> d;
    FILE* f = fopen(path, "rb");
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

static std::unique_ptr<pksm::Sav> load(const char* path)
{
    std::vector<u8> file = readAll(path);
    size_t ofs = 0, size = file.size();
    if (strstr(path, ".gbavc"))
    {
        using namespace walkerbox::gbavc;
        u32 h1, h2;
        bool has1, has2;
        std::string why;
        if (!findHeaders(file, h1, h2, has1, has2, why))
            return nullptr;
        Pick p = choose(file, h1, h2, has1, has2, true, true);
        ofs  = p.saveOfs;
        size = p.saveSize;
    }
    if (size == 0x8010)
        size = 0x8000;
    std::shared_ptr<u8[]> d(new u8[size]);
    memcpy(d.get(), file.data() + ofs, size);
    return pksm::Sav::getSave(d, (u32)size);
}

// PROTOTYPE of a possible rule (test only, not in the app): PKHeX's Gen 3/4 EV check (EffortValueVerifier /
// EffortExpLegality): per stat, the EVs above the vitamin part (multiples of 10, up to 100) must come from battles;
// the least EXP for exactly n such EVs over the battle options (EV, EXP) - Gen 4: (1,1) (5,1); Gen 3: HP/Atk/Spe
// (1,1) (2,1), Def (1,4) (2,3) (4,3), SpA (1,3) (2,3), SpD (1,2) (2,2) - the Pokerus options left out (stricter);
// stat indexes here are the app's (HP Atk Def SpA SpD Spe), PKHeX's are HP Atk Def Spe SpA SpD;
// summed over the stats it must be <= the EXP gained since the met level (at least level 1).
static u32 leastExp(int gen, int stat, int n)
{
    static const int opt4[][2] = {{1, 1}, {5, 1}};
    static const int optFast[][2] = {{1, 1}, {2, 1}};
    static const int optDef[][2] = {{1, 4}, {2, 3}, {4, 3}}; // (8, 3) only with Pokerus
    static const int optSpA[][2] = {{1, 3}, {2, 3}};         // (4, 3) only with Pokerus
    static const int optSpD[][2] = {{1, 2}, {2, 2}};         // (4, 2) only with Pokerus
    const int(*opt)[2];
    int nopt;
    if (gen == 4) { opt = opt4; nopt = 2; }
    else if (stat == 2) { opt = optDef; nopt = 3; }
    else if (stat == 3) { opt = optSpA; nopt = 2; }
    else if (stat == 4) { opt = optSpD; nopt = 2; }
    else { opt = optFast; nopt = 2; }
    std::vector<u32> best(n + 1, 0xFFFFFFFFu);
    best[0] = 0;
    for (int e = 1; e <= n; e++)
        for (int o = 0; o < nopt; o++)
            if (opt[o][0] <= e && best[e - opt[o][0]] != 0xFFFFFFFFu)
                best[e] = std::min(best[e], best[e - opt[o][0]] + (u32)opt[o][1]);
    return best[n];
}
static const pksm::Stat kStat[6] = {pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPATK,
    pksm::Stat::SPDEF, pksm::Stat::SPD};
static bool evsFit(const pksm::PKX& pk, int gen)
{
    u32 need = 0;
    for (int s = 0; s < 6; s++)
    {
        int ev = pk.ev(kStat[s]), vit = std::min(ev / 10 * 10, 100);
        need += leastExp(gen, s, ev - vit);
    }
    auto base = pk.clone();
    base->level((u8)std::max<int>(1, pk.metLevel()));
    u32 gained = pk.experience() > base->experience() ? pk.experience() - base->experience() : 0;
    return need <= gained;
}
// a walk with the prototype rule: as applyWalkGains, then EVs taken back one by one until they fit
static void cappedWalk(pksm::PKX& pk, int gen, u32 steps, int st)
{
    u16 before = pk.ev(kStat[st]);
    walkerbox::WalkGains g;
    walkerbox::applyWalkGains(pk, gen, steps, st, g);
    while (pk.ev(kStat[st]) > before && !evsFit(pk, gen))
        pk.ev(kStat[st], (u16)(pk.ev(kStat[st]) - 1));
    pk.refreshChecksum();
}

static void put(const std::string& name, const pksm::PKX& pk)
{
    auto raw = pk.rawData();
    FILE* f  = fopen(name.c_str(), "wb");
    if (f)
    {
        fwrite(raw.data(), 1, raw.size(), f);
        fclose(f);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("usage: test_evwalk <out dir> <save>...\n");
        return 2;
    }
    static const char* const statName[6] = {"hp", "atk", "def", "spa", "spd", "spe"};
    int written = 0;
    long evCapped[8] = {}, evUncapped[8] = {}, evCapped6[8] = {};
    for (int a = 2; a < argc; a++)
    {
        auto sav = load(argv[a]);
        if (!sav)
        {
            printf("FAIL: cannot read %s\n", argv[a]);
            return 1;
        }
        int gen = sav->generation() == pksm::Generation::THREE  ? 3
                : sav->generation() == pksm::Generation::FOUR ? 4
                : sav->generation() == pksm::Generation::SIX  ? 6
                : sav->generation() == pksm::Generation::SEVEN ? 7 : 0;
        if (!gen || walkerbox::walkEvCap(gen) == 0)
            continue;
        int n = 0;
        for (int b = 0; b < sav->maxBoxes() && n < 40; b++)
            for (int s = 0; s < sav->maxSlot() / sav->maxBoxes() && n < 40; s++)
            {
                auto pk = sav->pkm((u8)b, (u8)s);
                if (!pk || pk->species() == pksm::Species::None || pk->egg())
                    continue;
                char pre[512];
                snprintf(pre, sizeof(pre), "%s/s%d_p%02d", argv[1], a - 2, n);
                std::string ext = ".pk" + std::to_string(gen);
                put(std::string(pre) + "_base" + ext, *pk);
                for (int st = 0; st < 6; st++)
                {
                    for (u32 steps : {500u, 2000u, 5000u, 20000u})
                    {
                        auto w = pk->clone();
                        walkerbox::WalkGains g;
                        walkerbox::applyWalkGains(*w, gen, steps, st, g);
                        put(std::string(pre) + "_" + statName[st] + "_" + std::to_string(steps) + ext, *w);
                        written++;
                    }
                    if (gen <= 4) // the prototype rule (Gen 3/4 only: PKHeX has no EXP check from Gen 5 on)
                    {
                        for (u32 steps : {5000u, 20000u})
                        {
                            auto c = pk->clone();
                            cappedWalk(*c, gen, steps, st);
                            put(std::string(pre) + "_" + statName[st] + "_cap" + std::to_string(steps) + ext, *c);
                            evCapped[gen] += c->ev(kStat[st]) - pk->ev(kStat[st]);
                            auto u = pk->clone();
                            walkerbox::WalkGains g;
                            walkerbox::applyWalkGains(*u, gen, steps, st, g);
                            evUncapped[gen] += u->ev(kStat[st]) - pk->ev(kStat[st]);
                        }
                        auto c = pk->clone();
                        for (int k = 1; k <= 6; k++)
                        {
                            cappedWalk(*c, gen, 5000, st);
                            if (k == 1 || k == 2 || k == 6)
                                put(std::string(pre) + "_" + statName[st] + "_capx" + std::to_string(k) + ext, *c);
                        }
                        evCapped6[gen] += c->ev(kStat[st]) - pk->ev(kStat[st]);
                    }
                    auto w = pk->clone(); // walks in a row (5,000 steps each) until the stat is full
                    for (int k = 1; k <= 6; k++)
                    {
                        walkerbox::WalkGains g;
                        walkerbox::applyWalkGains(*w, gen, 5000, st, g);
                        if (k == 1 || k == 2 || k == 6)
                        {
                            put(std::string(pre) + "_" + statName[st] + "_x" + std::to_string(k) + ext, *w);
                            written++;
                        }
                    }
                }
                n++;
            }
        printf("%s: Gen %d, %d Pokemon\n", argv[a], gen, n);
    }
    printf("%d walked Pokemon written\n", written);
    for (int g = 3; g <= 4; g++)
        printf("Gen %d, one walk of 5,000 / 20,000 steps: EVs given %ld without the rule, %ld with it; six walks "
               "with it: %ld\n", g, evUncapped[g], evCapped[g], evCapped6[g]);
    return 0;
}
