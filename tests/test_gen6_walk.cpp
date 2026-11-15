// test_gen6_walk.cpp - the whole walk on real X/Y and OR/AS saves (/main backups in hw/gen6), end to end:
//   take a Pokemon out of its box (as a deposit: the slot gets the game's blank Pokemon), apply a walk's gains with
//   source/walk_poke.cpp (2500 steps, Speed EV training), put it back into the same slot (gameMatchFinish6), then:
//   - the save re-reads with valid checksums
//   - nothing changed outside that box except <= 2 bytes of the checksum table
//   - every other Pokemon in the save is unchanged
//   - the Pokemon is the same one (species, PID, encryption constant, OT, TID/SID, nickname, moves, nature)
//   - its EXP / level / friendship / Speed EVs are exactly what applyWalkGains reported
// Asked by the user 2026-11-15 ("did you also do tests to ensure that gen 6 saves stay correct after sending and
// receiving pokemon"). Usage: test_gen6_walk <file.main> [...]
#include "gamematch.hpp"
#include "walk_poke.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"
#include "enums/Stat.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using Raw = std::vector<u8>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static std::unique_ptr<pksm::Sav> fresh(const Raw& bytes)
{
    std::shared_ptr<u8[]> d(new u8[bytes.size()]);
    memcpy(d.get(), bytes.data(), bytes.size());
    return pksm::Sav::getSave(d, bytes.size());
}

static Raw rawOf(const pksm::PKX* pk)
{
    if (!pk || pk->species() == pksm::Species::None)
        return {};
    auto r = pk->rawData();
    return Raw(r.begin(), r.end());
}

static void walkOne(const Raw& orig, int box, int slot, u32 steps, int ev)
{
    const uint32_t len = (uint32_t)orig.size(), boxStart = len == 0x65600 ? 0x22600 : 0x33000;
    auto sav = fresh(orig);
    auto pk0 = sav->pkm((u8)box, (u8)slot);
    Raw before = rawOf(pk0.get());

    // 1. out of the game (deposit)
    sav->pkm(*sav->emptyPkm(), (u8)box, (u8)slot, false);
    std::string note;
    CHECK(walkerbox::gameMatchFinish6(*sav, orig.data(), len, box, note));
    Raw out(sav->rawData().get(), sav->rawData().get() + len);

    // 2. the walk
    Raw m   = before;
    auto pk = pksm::PKX::getPKM(pksm::Generation::SIX, m.data(), m.size());
    walkerbox::WalkGains g;
    bool applied = walkerbox::applyWalkGains(*pk, 6, steps, ev, g);
    CHECK(applied);

    // 3. back into the same slot
    auto s2 = fresh(out);
    s2->pkm(*pk, (u8)box, (u8)slot, false);
    std::string n2;
    CHECK(walkerbox::gameMatchFinish6(*s2, out.data(), len, box, n2));
    Raw back(s2->rawData().get(), s2->rawData().get() + len);

    // checks against the original save
    auto s3 = fresh(back);
    CHECK(s3 && s3->checksumsValid());
    if (!s3)
        return;
    uint32_t from = boxStart + (uint32_t)box * 30 * 0xE8, to = from + 30 * 0xE8;
    size_t outside = 0, table = 0;
    for (size_t i = 0; i < len; i++)
        if (orig[i] != back[i] && (i < from || i >= to))
            (i >= len - 0x200 ? table : outside)++;
    CHECK(outside == 0 && table <= 2);
    auto o  = fresh(orig);
    int others = 0, n = o->maxBoxes();
    for (int b = 0; b < n; b++)
        for (int i = 0; i < 30; i++)
            if (!(b == box && i == slot) && rawOf(o->pkm((u8)b, (u8)i).get()) != rawOf(s3->pkm((u8)b, (u8)i).get()))
                others++;
    CHECK(others == 0);
    auto a = o->pkm((u8)box, (u8)slot), z = s3->pkm((u8)box, (u8)slot);
    CHECK(a->species() == z->species() && a->PID() == z->PID() && a->encryptionConstant() == z->encryptionConstant());
    CHECK(a->otName() == z->otName() && a->TID() == z->TID() && a->SID() == z->SID());
    CHECK(a->nickname() == z->nickname() && a->nature() == z->nature());
    for (int i = 0; i < 4; i++)
        CHECK(a->move(i) == z->move(i));
    CHECK(z->experience() == g.expAfter && z->level() == g.levelAfter);
    CHECK(z->currentFriendship() == g.friendAfter);
    CHECK(ev < 0 || z->ev(pksm::Stat::SPD) == g.evAfter);
    printf("  box %d slot %d %s: Lv %u->%u, EXP %u->%u, friendship %u->%u, Spe EV %u->%u; %zu outside the box, %zu "
           "table byte(s), %d other Pokemon changed, checksums %s\n",
        box + 1, slot + 1, z->nickname().c_str(), g.levelBefore, g.levelAfter, (unsigned)g.expBefore,
        (unsigned)g.expAfter, g.friendBefore, g.friendAfter, g.evBefore, g.evAfter, outside, table, others,
        s3->checksumsValid() ? "valid" : "INVALID");
}

static void testSave(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp)
    {
        CHECK(fp != nullptr);
        return;
    }
    Raw orig;
    int ch;
    while ((ch = fgetc(fp)) != EOF)
        orig.push_back((u8)ch);
    fclose(fp);
    auto sav = fresh(orig);
    CHECK(sav && sav->generation() == pksm::Generation::SIX && sav->checksumsValid());
    if (!sav)
        return;
    printf("== %s (%s, OT %s)\n", path, orig.size() == 0x65600 ? "X/Y" : "OR/AS", sav->otName().c_str());
    // up to three Pokemon below Lv 100 (so EXP can change), from the first boxes
    int done = 0;
    for (int b = 0; b < sav->maxBoxes() && done < 3; b++)
        for (int i = 0; i < 30 && done < 3; i++)
        {
            auto pk = sav->pkm((u8)b, (u8)i);
            if (pk && pk->species() != pksm::Species::None && !pk->egg() && pk->level() < 100)
            {
                walkOne(orig, b, i, done == 1 ? 6000 : 2500, walkerbox::EV_SPE); // one with >= 5000 steps
                done++;
            }
        }
    CHECK(done > 0);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        testSave(argv[i]);
    printf(failures ? "GEN 6 WALK TESTS FAILED (%d)\n" : "gen6 walk round trip tests passed\n", failures);
    return failures ? 1 : 0;
}
