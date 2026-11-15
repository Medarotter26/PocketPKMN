// PC test of Gen 6 moves on real X/Y and OR/AS saves (/main backups, *.main) with
// PKSM-Core and source/gamematch.cpp:
//   0. the game's blank Pokemon built from the trainer data equals the one the
//      game wrote into the save (if the save has one)
//   A. deposit (a Pokemon leaves its box slot) -> only that box and the box
//      block's checksum change; the slot holds the game's blank Pokemon; the save
//      re-reads with valid checksums, everything else unchanged
//   B. withdraw it back into the same slot -> the save is byte-identical again
//   C. a copy into an empty slot of another box -> only that box + its checksum
// Usage: test_gen6 <file.main> [...]
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

static std::vector<Raw> allSlots(const pksm::Sav& s)
{
    std::vector<Raw> v;
    int n = s.maxBoxes(), per = s.maxSlot() / n;
    for (int b = 0; b < n; b++)
        for (int i = 0; i < per; i++)
            v.push_back(rawOf(s.pkm((u8)b, (u8)i).get()));
    for (int i = 0; i < s.partyCount(); i++)
        v.push_back(rawOf(s.pkm((u8)i).get()));
    return v;
}

static size_t diffOutside(const Raw& a, const u8* b, size_t from, size_t to, size_t& table)
{
    size_t n = 0;
    table    = 0;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i] != b[i] && (i < from || i >= to))
            (i >= a.size() - 0x200 ? table : n)++;
    return n;
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
    CHECK(sav && sav->generation() == pksm::Generation::SIX);
    if (!sav)
        return;
    const uint32_t len = (uint32_t)orig.size(), boxStart = len == 0x65600 ? 0x22600 : 0x33000;
    printf("== %s\n  %s, OT %s, %d boxes, checksums %s\n", path, len == 0x65600 ? "X/Y" : "OR/AS",
        sav->otName().c_str(), sav->maxBoxes(), sav->checksumsValid() ? "valid" : "INVALID");
    CHECK(sav->checksumsValid());

    // 0. Blank Pokemon: built vs found.
    bool found = false;
    Raw blank  = walkerbox::gen6Blank(*sav, orig.data(), found);
    if (found)
    {
        Raw hidden = orig; // the same save with the game's blanks zeroed -> forces the built one
        for (uint32_t i = 0; i < (uint32_t)sav->maxSlot(); i++)
        {
            u8* s = hidden.data() + boxStart + i * 0xE8;
            if (!s[0] && !s[1] && !s[2] && !s[3] && (s[6] || s[7]))
                memset(s, 0, 0xE8);
        }
        bool f2  = true;
        Raw made = walkerbox::gen6Blank(*sav, hidden.data(), f2);
        printf("  0. blank Pokemon in the save; built from the trainer data: %s\n",
            !f2 && made == blank ? "IDENTICAL" : "DIFFERENT");
        CHECK(!f2 && made == blank);
    }
    else
        printf("  0. the save has no blank Pokemon written by the game; it would be built\n");

    // A Pokemon to move: the first one in a box.
    auto before = allSlots(*sav);
    int per = 30, box = -1, slot = -1;
    for (int b = 0; b < sav->maxBoxes() && box < 0; b++)
        for (int i = 0; i < per && box < 0; i++)
            if (!rawOf(sav->pkm((u8)b, (u8)i).get()).empty())
            {
                box  = b;
                slot = i;
            }
    CHECK(box >= 0);
    if (box < 0)
        return;
    Raw moved = rawOf(sav->pkm((u8)box, (u8)slot).get());

    // A. Deposit.
    auto s = fresh(orig);
    s->pkm(*s->emptyPkm(), (u8)box, (u8)slot, false);
    std::string note;
    CHECK(walkerbox::gameMatchFinish6(*s, orig.data(), len, box, note));
    size_t table = 0;
    uint32_t from = boxStart + (uint32_t)box * 30 * 0xE8;
    size_t bad = diffOutside(orig, s->rawData().get(), from, from + 30 * 0xE8, table);
    printf("  A. deposit box %d slot %d: %s; %zu byte(s) changed outside the box, %zu in the checksum table\n", box + 1,
        slot + 1, note.c_str(), bad, table);
    CHECK(bad == 0 && table <= 2);
    CHECK(memcmp(s->rawData().get() + from + slot * 0xE8, blank.data(), 0xE8) == 0);
    Raw a(s->rawData().get(), s->rawData().get() + len);
    auto sa = fresh(a);
    CHECK(sa && sa->checksumsValid());
    if (sa)
    {
        auto now = allSlots(*sa);
        size_t idx = (size_t)box * 30 + (size_t)slot, others = 0;
        for (size_t i = 0; i < now.size(); i++)
            if (i != idx && now[i] != before[i])
                others++;
        CHECK(now[idx].empty() && others == 0);
    }

    // B. Withdraw back into the same slot.
    if (sa)
    {
        Raw m   = moved;
        auto pk = pksm::PKX::getPKM(pksm::Generation::SIX, m.data(), m.size());
        sa->pkm(*pk, (u8)box, (u8)slot, false);
        std::string n2;
        CHECK(walkerbox::gameMatchFinish6(*sa, a.data(), len, box, n2));
        size_t total = 0;
        for (uint32_t i = 0; i < len; i++)
            total += sa->rawData()[i] != orig[i];
        printf("  B. withdraw back: %zu byte(s) differ from the original\n", total);
        CHECK(total == 0);
    }

    // C. A copy into an empty slot of another box.
    int cbox = -1, cslot = -1;
    for (int b = sav->maxBoxes() - 1; b >= 0 && cbox < 0; b--)
        for (int i = per - 1; i >= 0 && cbox < 0; i--)
            if (b != box && rawOf(sav->pkm((u8)b, (u8)i).get()).empty())
            {
                cbox  = b;
                cslot = i;
            }
    if (cbox >= 0)
    {
        auto sc = fresh(orig);
        Raw m   = moved;
        auto pk = pksm::PKX::getPKM(pksm::Generation::SIX, m.data(), m.size());
        sc->pkm(*pk, (u8)cbox, (u8)cslot, false);
        std::string n3;
        CHECK(walkerbox::gameMatchFinish6(*sc, orig.data(), len, cbox, n3));
        uint32_t f3 = boxStart + (uint32_t)cbox * 30 * 0xE8;
        size_t bad3 = diffOutside(orig, sc->rawData().get(), f3, f3 + 30 * 0xE8, table);
        Raw c(sc->rawData().get(), sc->rawData().get() + len);
        auto s3 = fresh(c);
        printf("  C. copy into box %d slot %d: %zu outside the box, %zu in the table, checksums %s\n", cbox + 1,
            cslot + 1, bad3, table, s3 && s3->checksumsValid() ? "valid" : "INVALID");
        CHECK(bad3 == 0 && table <= 2 && s3 && s3->checksumsValid());
        CHECK(s3 && rawOf(s3->pkm((u8)cbox, (u8)cslot).get()) == moved);
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        testSave(argv[i]);
    printf(failures ? "gen6 tests FAILED (%d)\n" : "gen6 tests passed\n", failures);
    return failures ? 1 : 0;
}
