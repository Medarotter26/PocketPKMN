// See include/gamematch.hpp. Layouts: pret's disassemblies (pokered / pokegold /
// pokecrystal ram/sram.asm and layout.link) and PKSM-Core's Sav1/Sav2.
//
// What the games do on a PC deposit / withdraw:
//
// Gen 1 (Red/Blue/Yellow, international): the open box is part of the main data
//   (0x30C0) and covered by the main checksum (0x3523). A deposit/withdraw into
//   the open box changes the open box and the main checksum; the box banks
//   (0x4000-0x5FFF boxes 1-6, 0x6000-0x7FFF boxes 7-12, each with a bank
//   checksum and per-box checksums) are only written on a box switch.
//   An edit of a box that is not open (only possible in the game by switching
//   to it) is reduced to: that box's storage, its per-box checksum and its
//   bank's checksum - the open box's storage copy is left as it was.
// Gen 2 (Gold/Silver/Crystal, international): the open box has a working copy in
//   its own SRAM area ("Active Box": 0x2D6C Gold/Silver, 0x2D10 Crystal, 0x450
//   bytes) and its real copy in its storage slot (0x4000 + n*0x450 boxes 1-7,
//   0x6000 + n*0x450 boxes 8-14); neither is covered by a checksum. The game
//   reloads the Active Box from the storage slot when it starts (seen on the
//   user's Crystal, test S4), so a change to the open box must be in both.
//   The main data, its backup copy and both checksums are not touched by a box
//   change (PKSM-Core's extra checksum write at OFS_CHECKSUM_TWO is put back).

#include "gamematch.hpp"

#include "enums/Language.hpp"
#include "enums/Species.hpp"
#include "pkx/PK6.hpp"
#include "utils/crypto.hpp"
#include "sav/Item.hpp"

#include <map>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

namespace walkerbox
{
    namespace
    {
        constexpr uint32_t G1_BOX       = 0x462;
        constexpr uint32_t G1_BOXES     = 12;
        constexpr uint32_t G1_PER_BANK  = 6;
        constexpr uint32_t G1_BANK[2]   = {0x4000, 0x6000};
        constexpr uint32_t G1_SUMS[2]   = {0x5A4C, 0x7A4C}; // bank checksum, then one per box

        constexpr uint32_t G2_BOX       = 0x450; // slot size: 0x44E of box data + 2 filler bytes
        constexpr uint32_t G2_BOX_DATA  = 0x44E; // count, species list, 20 Pokemon, OT names, nicknames
        constexpr uint32_t G2_PER_BANK  = 7;
        constexpr uint32_t G2_ACTIVE_GS = 0x2D6C;
        constexpr uint32_t G2_ACTIVE_C  = 0x2D10;

        uint32_t restoreOutside(u8* d, const std::vector<u8>& pre, uint32_t length,
            std::initializer_list<std::pair<uint32_t, uint32_t>> keep)
        {
            uint32_t n = 0;
            for (uint32_t i = 0; i < length; i++)
            {
                bool kept = false;
                for (auto [s, e] : keep)
                    if (i >= s && i < e)
                        kept = true;
                if (!kept && d[i] != pre[i])
                {
                    d[i] = pre[i];
                    n++;
                }
            }
            return n;
        }
    }

    bool gameMatchSupported(const pksm::Sav& sav, std::string& why)
    {
        if (sav.generation() == pksm::Generation::THREE)
            return true; // gameMatchFinish3: the game's own save rule, any language
        if (sav.generation() == pksm::Generation::SIX)
            return true; // gameMatchFinish6
        if (sav.generation() == pksm::Generation::SEVEN)
            return true; // gameMatchFinish7 (v0.56: checked against the user's Moon, game-made before/after pair)
        if (sav.generation() != pksm::Generation::ONE && sav.generation() != pksm::Generation::TWO)
        {
            why = "only Gen 1 to 3 and 6/7 saves are handled so far";
            return false;
        }
        if (sav.language() == pksm::Language::JPN || sav.language() == pksm::Language::KOR)
        {
            why = "Japanese and Korean saves are not supported yet";
            return false;
        }
        return true;
    }

    uint32_t gameMatchFinish(pksm::Sav& sav, uint32_t length, int editedBox, std::string& note)
    {
        u8* d = sav.rawData().get();
        // The save right after the edit, before PKSM-Core's finishing touches.
        std::vector<u8> pre(d, d + length);
        const int open = sav.currentBox();
        sav.finishEditing();
        d = sav.rawData().get();

        if (sav.generation() == pksm::Generation::ONE)
        {
            if (editedBox == open)
            {
                // Open box (in the main data) + main checksum are kept; box banks as before.
                note = "Gen 1, open box: box banks kept as the game leaves them";
                return restoreOutside(d, pre, length, {{0, G1_BANK[0]}});
            }
            // Another box: only its storage, its per-box checksum and its bank checksum.
            uint32_t bank  = (uint32_t)editedBox / G1_PER_BANK;
            uint32_t idx   = (uint32_t)editedBox % G1_PER_BANK;
            uint32_t start = G1_BANK[bank] + idx * G1_BOX;
            uint32_t sum   = G1_SUMS[bank];
            uint32_t n = restoreOutside(d, pre, length,
                {{start, start + G1_BOX}, {sum, sum + 1}, {sum + 1 + idx, sum + 2 + idx}});
            // The bank checksum must match the bank as it now is (the open box's
            // storage copy, if it is in this bank, is the old one again).
            u8 bankSum = pksm::crypto::diff8({d + G1_BANK[bank], G1_PER_BANK * G1_BOX});
            if (d[sum] != bankSum)
                d[sum] = bankSum;
            note = "Gen 1, box " + std::to_string(editedBox + 1) +
                   " (not open): only that box, its checksum and its bank checksum";
            return n;
        }

        // Gen 2. The box's storage slot is what counts: when the game starts it reloads
        // the open box from its storage slot into the Active Box area (test S4, Crystal:
        // a deposit written only to the Active Box was gone after the game had run). So
        // an open-box edit must change BOTH the Active Box area and that box's storage
        // slot - the two copies the game keeps - and nothing else.
        uint32_t bank    = (uint32_t)editedBox / G2_PER_BANK;
        uint32_t storage = (bank ? 0x6000 : 0x4000) + ((uint32_t)editedBox % G2_PER_BANK) * G2_BOX;
        if (editedBox == open)
        {
            uint32_t active = sav.version() == pksm::GameVersion::C ? G2_ACTIVE_C : G2_ACTIVE_GS;
            note = "Gen 2, open box: its Active Box area and its storage slot";
            // The 2 filler bytes at the end of each slot are never used by the game; keep them.
            return restoreOutside(d, pre, length, {{active, active + G2_BOX_DATA}, {storage, storage + G2_BOX_DATA}});
        }
        note = "Gen 2, box " + std::to_string(editedBox + 1) + " (not open): only that box's storage";
        return restoreOutside(d, pre, length, {{storage, storage + G2_BOX_DATA}});
    }

    // --- Gen 6 -------------------------------------------------------------------
    namespace
    {
        constexpr uint32_t G6_SLOT  = 0xE8;
        constexpr uint32_t G6_PER   = 30;
        constexpr uint32_t G6_XY    = 0x65600;
        constexpr uint32_t G6_ORAS  = 0x76000;

        uint32_t g6BoxStart(uint32_t length) { return length == G6_XY ? 0x22600 : 0x33000; }

        // The game's name for an egg, which it also gives its blank Pokemon.
        const char* eggName(pksm::Language l)
        {
            using L = pksm::Language;
            switch (l)
            {
                case L::JPN: return "タマゴ"; // タマゴ
                case L::FRE: return "Œuf";
                case L::ITA: return "Uovo";
                case L::GER: return "Ei";
                case L::SPA: return "Huevo";
                case L::KOR: return "알"; // 알
                default: return "Egg";
            }
        }
    }

    std::vector<uint8_t> gen6Blank(const pksm::Sav& sav, const uint8_t* save, bool& found)
    {
        uint32_t length = (uint32_t)sav.getLength();
        uint32_t box    = g6BoxStart(length);
        // Copy it from the save if the game has written one (encryption constant 0,
        // checksum not 0 - an all-zero blank has checksum 0); the most common wins.
        std::map<std::vector<uint8_t>, int> seen;
        for (uint32_t i = 0; i < (uint32_t)sav.maxSlot(); i++)
        {
            const uint8_t* s = save + box + i * G6_SLOT;
            bool ecZero      = !s[0] && !s[1] && !s[2] && !s[3];
            if (ecZero && (s[6] || s[7]))
                seen[std::vector<uint8_t>(s, s + G6_SLOT)]++;
        }
        if (!seen.empty())
        {
            found = true;
            return std::max_element(seen.begin(), seen.end(),
                [](auto& a, auto& b) { return a.second < b.second; })->first;
        }
        // Else build it: blank, nickname "Egg" (in the save's language), the
        // console's country / region / 3DS region / language.
        found   = false;
        auto pk = sav.emptyPkm();
        if (!pk || pk->generation() != pksm::Generation::SIX)
            return {};
        auto* p6 = static_cast<pksm::PK6*>(pk.get()); // a Gen 6 save's emptyPkm is a PK6 (no RTTI on the 3DS)
        p6->nickname(eggName(sav.language()));
        p6->country(sav.country());
        p6->region(sav.subRegion());
        p6->consoleRegion(sav.consoleRegion());
        p6->language(sav.language());
        p6->encrypt();
        auto r = p6->rawData();
        return std::vector<uint8_t>(r.begin(), r.begin() + G6_SLOT);
    }

    bool gameMatchFinish6(pksm::Sav& sav, const uint8_t* before, uint32_t length, int editedBox, std::string& note)
    {
        if (sav.generation() != pksm::Generation::SIX || (length != G6_XY && length != G6_ORAS) || !before ||
            editedBox < 0 || editedBox >= sav.maxBoxes())
        {
            note = "not an X/Y or OR/AS save";
            return false;
        }
        {
            // The game's own checksums must be valid, or recomputing them would change more.
            std::shared_ptr<u8[]> b(new u8[length]);
            memcpy(b.get(), before, length);
            auto orig = pksm::Sav::getSave(b, length);
            if (!orig || !orig->checksumsValid())
            {
                note = "the save's checksums were not valid before the edit";
                return false;
            }
        }
        uint32_t from = g6BoxStart(length) + (uint32_t)editedBox * G6_PER * G6_SLOT;
        uint32_t to   = from + G6_PER * G6_SLOT;
        bool found    = false;
        std::vector<uint8_t> blank = gen6Blank(sav, before, found);
        if (blank.size() != G6_SLOT)
        {
            note = "could not make the game's blank Pokemon";
            return false;
        }
        u8* d = sav.rawData().get();
        int blanked = 0;
        for (uint32_t s = 0; s < G6_PER; s++)
        {
            uint32_t o = from + s * G6_SLOT;
            if (memcmp(d + o, before + o, G6_SLOT) != 0 &&
                sav.pkm((u8)editedBox, (u8)s)->species() == pksm::Species::None)
            {
                memcpy(d + o, blank.data(), G6_SLOT);
                blanked++;
            }
        }
        // Everything but that box as it was; then the block checksums.
        for (uint32_t i = 0; i < length; i++)
            if ((i < from || i >= to) && d[i] != before[i])
                d[i] = before[i];
        sav.finishEditing();
        d = sav.rawData().get();
        uint32_t table = 0, other = 0;
        for (uint32_t i = 0; i < length; i++)
            if ((i < from || i >= to) && d[i] != before[i])
                (i >= length - 0x200 ? table : other)++;
        if (other || table > 2)
        {
            note = "the checksums changed more than the box's (" + std::to_string(other) + "/" +
                   std::to_string(table) + " bytes)";
            memcpy(d, before, length);
            sav.beginEditing();
            return false;
        }
        sav.beginEditing();
        note = "Gen 6, box " + std::to_string(editedBox + 1) + ": only that box and its checksum" +
               (blanked ? std::string("; emptied slot = the game's blank Pokemon (") +
                              (found ? "copied from the save" : "built from the trainer data") + ")"
                        : std::string(""));
        return true;
    }

    // --- Gen 7 (v0.56) --------------------------------------------------------------
    // Learned from the user's Moon (cartridge) with Checkpoint: before / after the GAME moved a Pichu from box 4
    // slot 5 to box 3 slot 6 and saved (hw/gen7/moon_before_160900.main, moon_after_161029.main). For a box change
    // the game rewrites: the two slots in the box block, the block-checksum table after the last block, and the
    // 128-byte signature 0x100 before that table (plus play time etc., which PocketPKMN leaves alone). The emptied
    // slot became the all-zero Pokemon (encrypted, EC 0, checksum 0) - the form PKSM-Core writes for an empty slot.
    namespace
    {
        constexpr uint32_t G7_SLOT = 0xE8;
        constexpr uint32_t G7_PER  = 30;
        constexpr uint32_t G7_SM   = 0x6BE00;
        constexpr uint32_t G7_USUM = 0x6CC00;

        uint32_t g7BoxStart(uint32_t length) { return length == G7_SM ? 0x4E00 : 0x5200; }
    }

    bool gameMatchFinish7(pksm::Sav& sav, const uint8_t* before, uint32_t length, int editedBox, std::string& note)
    {
        if (sav.generation() != pksm::Generation::SEVEN || (length != G7_SM && length != G7_USUM) || !before ||
            editedBox < 0 || editedBox >= sav.maxBoxes())
        {
            note = "not a Sun/Moon or Ultra Sun/Ultra Moon save";
            return false;
        }
        {
            // the game's own checksums must be valid, or recomputing them would change more
            std::shared_ptr<u8[]> b(new u8[length]);
            memcpy(b.get(), before, length);
            auto orig = pksm::Sav::getSave(b, length);
            if (!orig || !orig->checksumsValid())
            {
                note = "the save's checksums were not valid before the edit";
                return false;
            }
        }
        uint32_t from  = g7BoxStart(length) + (uint32_t)editedBox * G7_PER * G7_SLOT;
        uint32_t to    = from + G7_PER * G7_SLOT;
        uint32_t table = length - 0x200;       // block-checksum table (to the end of the save)
        uint32_t sig   = table - 0x100;        // the signature over that table, 0x80 bytes
        u8* d = sav.rawData().get();
        // everything but that box as it was; then PKSM-Core's checksums + signature
        for (uint32_t i = 0; i < length; i++)
            if ((i < from || i >= to) && d[i] != before[i])
                d[i] = before[i];
        sav.finishEditing();
        d = sav.rawData().get();
        uint32_t inTable = 0, inSig = 0, other = 0;
        for (uint32_t i = 0; i < length; i++)
            if ((i < from || i >= to) && d[i] != before[i])
                (i >= table ? inTable : (i >= sig && i < sig + 0x80) ? inSig : other)++;
        if (other || inTable > 2)
        {
            note = "the save changed more than the box, its checksum and the signature (" + std::to_string(other) +
                   "/" + std::to_string(inTable) + " bytes)";
            memcpy(d, before, length);
            sav.beginEditing();
            return false;
        }
        sav.beginEditing();
        note = "Gen 7, box " + std::to_string(editedBox + 1) + ": only that box, its checksum and the signature (" +
               std::to_string(inSig) + " signature bytes)";
        return true;
    }

    // --- items (v0.53) -------------------------------------------------------------
    bool addItemToSave(pksm::Sav& sav, uint16_t id, bool gen2Id, std::string& where)
    {
        using P = pksm::Sav::Pouch;
        if (sav.generation() == pksm::Generation::TWO)
        {
            pksm::Item2 probe;
            if (gen2Id)
                probe.id2(id);
            else
                probe.id(id); // national -> Gen 2 (0 if the item does not exist in Gen 2)
            u16 g2 = probe.id2();
            if (!g2)
            {
                where = "does not exist in this game";
                return false;
            }
            int n = 0;
            for (auto& pr : sav.pouches())
                if (pr.first == P::PCItem)
                    n = pr.second;
            int empty = -1;
            for (int s = 0; s < n; s++)
            {
                auto it = sav.item(P::PCItem, (u16)s);
                auto& i2 = static_cast<pksm::Item2&>(*it);
                if (i2.id2() == g2 && i2.count() < 99)
                {
                    i2.count(i2.count() + 1);
                    sav.item(i2, P::PCItem, (u16)s);
                    where = "PC";
                    return true;
                }
                if (i2.id2() == 0 && empty < 0)
                    empty = s;
            }
            if (empty < 0)
            {
                where = "the PC item storage is full";
                return false;
            }
            auto it = sav.item(P::PCItem, (u16)empty);
            auto& i2 = static_cast<pksm::Item2&>(*it);
            i2.id2(g2);
            i2.count(1);
            sav.item(i2, P::PCItem, (u16)empty);
            where = "PC";
            return true;
        }
        if (sav.generation() == pksm::Generation::SIX || sav.generation() == pksm::Generation::SEVEN) // v0.56: Gen 7 too
        {
            for (auto& [pouch, valid] : sav.validItems())
            {
                if (std::find(valid.begin(), valid.end(), (int)id) == valid.end())
                    continue;
                int n = 0;
                for (auto& pr : sav.pouches())
                    if (pr.first == pouch)
                        n = pr.second;
                int empty = -1;
                for (int s = 0; s < n; s++)
                {
                    auto it = sav.item(pouch, (u16)s);
                    if (it->id() == id && it->count() < it->maxCount())
                    {
                        it->count(it->count() + 1);
                        sav.item(*it, pouch, (u16)s);
                        where = "bag";
                        return true;
                    }
                    if (it->id() == 0 && empty < 0)
                        empty = s;
                }
                if (empty < 0)
                {
                    where = "that bag pocket is full";
                    return false;
                }
                auto it = sav.item(pouch, (u16)empty);
                it->id(id);
                it->count(1);
                sav.item(*it, pouch, (u16)empty);
                where = "bag";
                return true;
            }
            where = "does not exist in this game";
            return false;
        }
        where = "items: not supported for this game";
        return false;
    }

    bool gameMatchItems(pksm::Sav& sav, const uint8_t* before, uint32_t length, std::string& note)
    {
        const bool g2 = sav.generation() == pksm::Generation::TWO;
        const bool g6 = sav.generation() == pksm::Generation::SIX && (length == G6_XY || length == G6_ORAS);
        // v0.56: Gen 7 like Gen 6 (the bag, the block table), plus the 0x80-byte signature 0x100 before the table
        const bool g7 = sav.generation() == pksm::Generation::SEVEN && (length == G7_SM || length == G7_USUM);
        if (!g2 && !g6 && !g7)
        {
            note = "items: only Gen 2, 6 and 7 saves";
            return false;
        }
        if (g6 || g7)
        {
            std::shared_ptr<u8[]> b(new u8[length]);
            memcpy(b.get(), before, length);
            auto orig = pksm::Sav::getSave(b, length);
            if (!orig || !orig->checksumsValid())
            {
                note = "the save's checksums were not valid before the edit";
                return false;
            }
        }
        u8* d = sav.rawData().get();
        // what the item edit itself changed: one item list (Gen 2 PC list 0x6A bytes; an X/Y pouch)
        uint32_t lo = length, hi = 0;
        for (uint32_t i = 0; i < length; i++)
            if (d[i] != before[i])
            {
                lo = std::min(lo, i);
                hi = i + 1;
            }
        if (lo >= hi)
        {
            note = "items: nothing changed";
            return true;
        }
        const uint32_t window = g2 ? 0x80 : 0x1000;
        if (hi - lo > window)
        {
            note = "items: the edit spans " + std::to_string(hi - lo) + " bytes (more than one item list)";
            memcpy(d, before, length);
            sav.beginEditing();
            return false;
        }
        sav.finishEditing();
        d = sav.rawData().get();
        uint32_t sums = 0, other = 0;
        std::string where;
        for (uint32_t i = 0; i < length; i++)
        {
            // the item list itself: Gen 2's PC list is 0x6A bytes from its count byte (the end marker 0xFF after the
            // last entry moves when an entry is added); an X/Y pocket lies inside the window
            if ((i >= lo && i < (g2 ? lo + 0x6A : hi)) || d[i] == before[i])
                continue;
            if (g2 && i >= 0x4000)
                d[i] = before[i]; // box storage (PKSM-Core recopies the open box) and the backup copy's checksum: the
                                  // game changes neither when it stores an item
            else if (g7 && i >= length - 0x300 && i < length - 0x280)
                continue; // Gen 7 signature over the block table (re-made by PKSM-Core like the game's)
            else if ((g2 && i >= 0x2000 && i < 0x4000) || ((g6 || g7) && i >= length - 0x200))
                sums++; // Gen 2 main checksum / X/Y / Gen 7 block table checksum
            else
                other++;
            if (where.size() < 80)
            {
                char b[16];
                snprintf(b, sizeof(b), " %x", i);
                where += b;
            }
        }
        if (other || sums > 2)
        {
            note = "items: the save changed more than the item list and its checksum (" + std::to_string(other) + "/" +
                   std::to_string(sums) + " bytes; list " + std::to_string(lo) + "-" + std::to_string(hi) + ";" +
                   where + ")";
            memcpy(d, before, length);
            sav.beginEditing();
            return false;
        }
        // Crystal keeps a backup copy of its game data (file 0x1209, the same 0xB7A bytes as 0x2009, checksum at
        // 0x1F0D; identical in the user's own Crystal saves) and the game writes both copies whenever it saves - so
        // the backup gets the new item list too (PKHeX checks it). Gold / Silver's backup is not one copy and is left
        // as it is (as PKSM-Core / PKHeX do).
        bool backup = false;
        if (g2 && sav.version() == pksm::GameVersion::C && length >= 0x8000)
        {
            memcpy(d + 0x1209, d + 0x2009, 0x2B83 - 0x2009);
            d[0x1F0D] = d[0x2D0D];
            d[0x1F0E] = d[0x2D0E];
            backup = true;
        }
        sav.beginEditing();
        note = std::string(g2 ? "Gen 2" : g7 ? "Gen 7" : "Gen 6") + (backup ? " (+ Crystal's backup copy)" : "") +
               " items: only the item list (" + std::to_string(hi - lo) +
               " bytes) and its checksum";
        return true;
    }

    // --- Gen 3 -------------------------------------------------------------------
    //
    // Layout and rules from pret's pokeemerald / pokeruby / pokefirered save.c:
    // sector footer at 0xFF4: u16 id, u16 checksum, u32 signature 0x08012025, u32
    // save counter. Checksum: u32 sum of the sector's data, then (hi + lo) & 0xFFFF.
    // The data sizes are at most 0xF80 and the game zero-fills the rest before
    // writing, so the sum over 0xF80 bytes is the game's checksum (checked on the
    // user's Emerald save: all 28 sectors). Box storage is sectors 5-13.
    namespace
    {
        constexpr uint32_t G3_SECTOR  = 0x1000;
        constexpr int G3_SECTORS      = 14;
        constexpr uint32_t G3_SLOT    = G3_SECTOR * G3_SECTORS; // 0xE000
        constexpr uint32_t G3_SIG     = 0x08012025;
        constexpr uint32_t G3_SUMLEN  = 0xF80;
        constexpr uint32_t G3_SIZE    = 0x20000;
        constexpr int G3_FIRST_BOX_ID = 5;

        uint16_t rd16(const u8* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
        uint32_t rd32(const u8* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
        void wr16(u8* p, uint16_t v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
        void wr32(u8* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (u8)(v >> (8 * i)); }

        uint16_t g3Sum(const u8* sector)
        {
            uint32_t s = 0;
            for (uint32_t i = 0; i < G3_SUMLEN; i += 4)
                s += rd32(sector + i);
            return (uint16_t)((s >> 16) + (s & 0xFFFF));
        }

        struct G3Slot
        {
            bool ok          = false;
            uint32_t counter = 0;
            int phys[G3_SECTORS] = {};
        };

        // A slot counts if all 14 ids are there once, each signed and with a valid checksum.
        G3Slot g3Slot(const u8* d, int slot)
        {
            G3Slot s;
            unsigned seen = 0;
            for (int p = 0; p < G3_SECTORS; p++)
            {
                const u8* sec = d + slot * G3_SLOT + p * G3_SECTOR;
                uint16_t id   = rd16(sec + 0xFF4);
                if (rd32(sec + 0xFF8) != G3_SIG || id >= G3_SECTORS || (seen & (1u << id)) ||
                    g3Sum(sec) != rd16(sec + 0xFF6))
                    return s;
                seen |= 1u << id;
                s.phys[id] = p;
                s.counter  = rd32(sec + 0xFFC);
            }
            s.ok = seen == (1u << G3_SECTORS) - 1;
            return s;
        }
    }

    int gen3CurrentSlot(const uint8_t* save, uint32_t& counter, int& rotation)
    {
        G3Slot a = g3Slot(save, 0), b = g3Slot(save, 1);
        int pick = -1;
        if (a.ok && b.ok)
        {
            // pret GetSaveValidStatus: the higher counter wins; 0 is newer than 0xFFFFFFFF.
            if ((a.counter == 0xFFFFFFFF && b.counter == 0) || (a.counter == 0 && b.counter == 0xFFFFFFFF))
                pick = (a.counter + 1) < (b.counter + 1) ? 1 : 0;
            else
                pick = a.counter < b.counter ? 1 : 0;
        }
        else if (a.ok)
            pick = 0;
        else if (b.ok)
            pick = 1;
        if (pick < 0)
            return -1;
        const G3Slot& s = pick ? b : a;
        counter         = s.counter;
        rotation        = s.phys[0];
        return pick;
    }

    bool gameMatchFinish3(pksm::Sav& sav, const uint8_t* before, uint32_t length, std::string& note)
    {
        if (sav.generation() != pksm::Generation::THREE || length != G3_SIZE || !before)
        {
            note = "not a 128 KiB Gen 3 save";
            return false;
        }
        uint32_t counter = 0;
        int rotation     = 0;
        int cur          = gen3CurrentSlot(before, counter, rotation);
        if (cur < 0)
        {
            note = "the save has no valid copy";
            return false;
        }
        G3Slot slot = g3Slot(before, cur);
        u8* d       = sav.rawData().get();

        // The edit must lie in the current slot's box sectors (ids 5-13), data part only.
        uint32_t changed = 0;
        for (uint32_t i = 0; i < length; i++)
        {
            if (d[i] == before[i])
                continue;
            changed++;
            uint32_t base = cur * G3_SLOT;
            if (i < base || i >= base + G3_SLOT)
            {
                note = "the edit is outside the current save copy";
                return false;
            }
            uint32_t sec = (i - base) / G3_SECTOR, off = (i - base) % G3_SECTOR;
            uint16_t id  = rd16(before + base + sec * G3_SECTOR + 0xFF4);
            if (off >= G3_SUMLEN || id < G3_FIRST_BOX_ID)
            {
                note = "the edit touched more than the boxes (sector " + std::to_string(id) + ")";
                return false;
            }
        }

        uint32_t newCounter = counter + 1;
        int target          = (int)(newCounter % 2); // the game writes slot gSaveCounter % 2
        if (target == cur)
        {
            note = "the save counter does not match its slot";
            return false;
        }
        int newRotation = (rotation + 1) % G3_SECTORS;

        std::vector<u8> out(before, before + length);
        for (int id = 0; id < G3_SECTORS; id++)
        {
            const u8* src = d + cur * G3_SLOT + slot.phys[id] * G3_SECTOR;
            u8* dst       = out.data() + target * G3_SLOT + ((id + newRotation) % G3_SECTORS) * G3_SECTOR;
            memcpy(dst, src, 0xFF4);
            wr16(dst + 0xFF4, (uint16_t)id);
            wr16(dst + 0xFF6, g3Sum(dst));
            wr32(dst + 0xFF8, G3_SIG);
            wr32(dst + 0xFFC, newCounter);
        }
        uint32_t c2 = 0;
        int r2      = 0;
        if (gen3CurrentSlot(out.data(), c2, r2) != target || c2 != newCounter || r2 != newRotation)
        {
            note = "the new save copy did not check out";
            return false;
        }
        memcpy(d, out.data(), length);
        note = std::string("Gen 3: saved as the game does - new copy in slot ") + (target ? "B" : "A") +
               ", counter " + std::to_string(newCounter) + " (" + std::to_string(changed) + " byte(s) of box data)";
        return true;
    }
}
