// PC test of Gen 3 moves on a real GBA Virtual Console save (the whole container
// file WalkerBox backs up, *.gbavc), with PKSM-Core, source/gamematch.cpp and the
// portable part of source/gbavc.cpp:
//   A. a copy into a box  -> saved as the game saves (new copy in the other slot,
//      counter + 1, sectors rotated by one); the current copy, the extra sectors
//      and everything but the box data are untouched; re-read: copy present,
//      party / other boxes / trainer unchanged, checksums valid
//   B. taking it out again -> the next copy holds exactly the original data
//   C. an edit outside the boxes is refused and changes nothing
//   D. the GBA container: only the other slot (header + save) changes, the
//      header's counter goes up by one and that slot becomes the current one
// Usage: test_gen3 <file.gbavc>
#include "gamematch.hpp"
#include "gbavc.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"

#include <algorithm>
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

struct View
{
    std::vector<std::vector<Raw>> boxes; // per box: (slot, raw) with empties as {}
    std::vector<Raw> party;
    std::string ot;
    u16 tid = 0;
};

static View view(const pksm::Sav& s)
{
    View v;
    int n = s.maxBoxes(), per = s.maxSlot() / n;
    v.boxes.resize((size_t)n);
    for (int b = 0; b < n; b++)
        for (int i = 0; i < per; i++)
            v.boxes[(size_t)b].push_back(rawOf(s.pkm((u8)b, (u8)i).get()));
    for (int i = 0; i < s.partyCount(); i++)
        v.party.push_back(rawOf(s.pkm((u8)i).get()));
    v.ot  = s.otName();
    v.tid = s.TID();
    return v;
}

// Sector data (0..0xFF4) by id for the current slot.
static std::vector<Raw> sectorsById(const Raw& save)
{
    uint32_t c;
    int rot;
    int slot = walkerbox::gen3CurrentSlot(save.data(), c, rot);
    std::vector<Raw> out(14);
    if (slot < 0)
        return out;
    for (int p = 0; p < 14; p++)
    {
        const u8* sec = save.data() + slot * 0xE000 + p * 0x1000;
        int id        = sec[0xFF4] | (sec[0xFF5] << 8);
        out[(size_t)id].assign(sec, sec + 0xFF4);
    }
    return out;
}

static size_t diffOutside(const Raw& a, const Raw& b, size_t from, size_t to)
{
    size_t n = 0;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i] != b[i] && (i < from || i >= to))
            n++;
    return n;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    FILE* fp = fopen(argv[1], "rb");
    if (!fp)
        return 1;
    Raw file;
    int ch;
    while ((ch = fgetc(fp)) != EOF)
        file.push_back((u8)ch);
    fclose(fp);
    printf("== %s (%zu bytes)\n", argv[1], file.size());

    using namespace walkerbox::gbavc;
    u32 h1, h2;
    bool has1, has2;
    std::string why;
    CHECK(findHeaders(file, h1, h2, has1, has2, why));
    Pick cur = choose(file, h1, h2, has1, has2, true, true); // CMACs: valid on the console (log)
    CHECK(cur.slot != 0);
    Raw original(file.begin() + cur.saveOfs, file.begin() + cur.saveOfs + cur.saveSize);
    uint32_t counter;
    int rot;
    int gslot = walkerbox::gen3CurrentSlot(original.data(), counter, rot);
    auto sav  = fresh(original);
    CHECK(sav != nullptr);
    if (!sav)
        return 1;
    printf("  GBA slot %d; Gen 3 save: %s, current copy %c, counter %u, rotation %d, %d boxes, party %d, checksums %s\n",
        cur.slot, sav->otName().c_str(), gslot ? 'B' : 'A', counter, rot, sav->maxBoxes(), sav->partyCount(),
        sav->checksumsValid() ? "valid" : "INVALID");
    CHECK(gslot >= 0);
    CHECK(walkerbox::gameMatchSupported(*sav, why));
    View v0 = view(*sav);

    // The Pokemon to use: the first one in a box, else the first party member.
    Raw moved;
    for (auto& b : v0.boxes)
        for (auto& r : b)
            if (moved.empty() && !r.empty())
                moved = r;
    if (moved.empty() && sav->partyCount() > 0)
    {
        auto p = sav->pkm((u8)0);
        moved  = rawOf(p.get());
        moved.resize(80); // party -> box format (the first 80 bytes)
        printf("  (no Pokemon in the boxes; using party member 1 in box form)\n");
    }
    CHECK(!moved.empty());
    // A free slot: the last box's last slot unless taken.
    int box = sav->maxBoxes() - 1, slot = -1;
    for (int i = (int)v0.boxes[(size_t)box].size() - 1; i >= 0 && slot < 0; i--)
        if (v0.boxes[(size_t)box][(size_t)i].empty())
            slot = i;
    CHECK(slot >= 0);

    // A. Copy into the box.
    auto s = fresh(original);
    {
        Raw m  = moved;
        auto pk = pksm::PKX::getPKM(pksm::Generation::THREE, m.data(), m.size());
        s->pkm(*pk, (u8)box, (u8)slot, false);
    }
    std::string note;
    bool ok = walkerbox::gameMatchFinish3(*s, original.data(), (uint32_t)original.size(), note);
    printf("  A. copy into box %d slot %d: %s\n", box + 1, slot + 1, note.c_str());
    CHECK(ok);
    Raw a(s->rawData().get(), s->rawData().get() + original.size());
    uint32_t c2;
    int r2;
    int g2 = walkerbox::gen3CurrentSlot(a.data(), c2, r2);
    CHECK(g2 == 1 - gslot && c2 == counter + 1 && r2 == (rot + 1) % 14);
    size_t otherFrom = (size_t)(1 - gslot) * 0xE000;
    size_t bad       = diffOutside(original, a, otherFrom, otherFrom + 0xE000);
    printf("     bytes changed outside the other copy: %zu\n", bad);
    CHECK(bad == 0);
    auto ids0 = sectorsById(original), idsA = sectorsById(a);
    int changedIds = 0;
    for (int id = 0; id < 14; id++)
        if (ids0[(size_t)id] != idsA[(size_t)id])
        {
            changedIds++;
            CHECK(id >= 5); // only box sectors
        }
    printf("     sectors with new data: %d (all box sectors)\n", changedIds);
    auto sa = fresh(a);
    CHECK(sa && sa->checksumsValid());
    if (sa)
    {
        View va = view(*sa);
        CHECK(va.boxes[(size_t)box][(size_t)slot] == moved);
        CHECK(va.party == v0.party && va.ot == v0.ot && va.tid == v0.tid);
        for (size_t b = 0; b < va.boxes.size(); b++)
            if ((int)b != box)
                CHECK(va.boxes[b] == v0.boxes[b]);
        for (size_t i = 0; i < va.boxes[(size_t)box].size(); i++)
            if ((int)i != slot)
                CHECK(va.boxes[(size_t)box][i] == v0.boxes[(size_t)box][i]);
    }

    // B. Take it out again: the copy after that holds the original data.
    if (sa)
    {
        sa->pkm(*sa->emptyPkm(), (u8)box, (u8)slot, false);
        std::string n2;
        bool ok2 = walkerbox::gameMatchFinish3(*sa, a.data(), (uint32_t)a.size(), n2);
        printf("  B. take it out again: %s\n", n2.c_str());
        CHECK(ok2);
        Raw b(sa->rawData().get(), sa->rawData().get() + a.size());
        uint32_t c3;
        int r3;
        CHECK(walkerbox::gen3CurrentSlot(b.data(), c3, r3) == gslot && c3 == counter + 2);
        auto idsB = sectorsById(b);
        int same = 0;
        for (int id = 0; id < 14; id++)
            same += idsB[(size_t)id] == ids0[(size_t)id];
        printf("     sectors identical to the original by id: %d of 14\n", same);
        CHECK(same == 14);
    }

    // C. An edit outside the boxes is refused and changes nothing.
    {
        auto sc = fresh(original);
        sc->TID((u16)(sc->TID() ^ 1));
        Raw edited(sc->rawData().get(), sc->rawData().get() + original.size());
        std::string n3;
        bool ok3 = walkerbox::gameMatchFinish3(*sc, original.data(), (uint32_t)original.size(), n3);
        printf("  C. trainer ID edited: %s (%s)\n", ok3 ? "ACCEPTED" : "refused", n3.c_str());
        CHECK(!ok3);
        CHECK(memcmp(sc->rawData().get(), edited.data(), edited.size()) == 0);
    }

    // D. The GBA container.
    {
        Raw next;
        u32 hdr = 0;
        CHECK(buildNext(file, cur, a.data(), (u32)a.size(), next, hdr));
        CHECK(hdr == (cur.slot == 1 ? 0x200 + cur.saveSize : 0u));
        size_t outside = diffOutside(file, next, hdr, hdr + 0x200 + cur.saveSize);
        Header oldH, newH;
        memcpy(&oldH, file.data() + cur.headerOfs, sizeof(Header));
        memcpy(&newH, next.data() + hdr, sizeof(Header));
        u32 n1, n2;
        bool x1, x2;
        findHeaders(next, n1, n2, x1, x2, why);
        Pick np = choose(next, n1, n2, x1, x2, true, true);
        printf("  D. container: new slot %d, counter %u -> %u, %zu byte(s) changed outside it\n", np.slot,
            oldH.savesMade, newH.savesMade, outside);
        CHECK(outside == 0);
        CHECK(newH.savesMade == oldH.savesMade + 1);
        CHECK(np.headerOfs == hdr && memcmp(next.data() + np.saveOfs, a.data(), a.size()) == 0);
        CHECK(memcmp(newH.arm7Registers, oldH.arm7Registers, 8) == 0 && newH.titleId == oldH.titleId);
    }

    printf(failures ? "gen3 tests FAILED (%d)\n" : "gen3 tests passed\n", failures);
    return failures ? 1 : 0;
}
