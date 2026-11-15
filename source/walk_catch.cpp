// walk_catch.cpp - v0.53: a walk's catch as a legal Pokemon (see include/walk_poke.hpp, buildCatch). PC-tested with
// PKHeX's own legality check (tests/run_catches.sh).
#include <cctype>
#include <cstring>
#include <vector>

#include "walk_poke.hpp"

#include "enums/Gender.hpp"
#include "enums/Language.hpp"
#include "enums/Move.hpp"
#include "enums/Nature.hpp"
#include "enums/Stat.hpp"
#include "personal/personal.hpp"
#include "pkx/PK2.hpp"
#include "pkx/PK6.hpp"
#include "pkx/PK7.hpp"
#include "utils/DateTime.hpp"
#include "utils/i18n.hpp"

namespace walkerbox
{
    namespace
    {
        struct Rng
        {
            u32 s;
            explicit Rng(u32 seed) : s(seed ? seed : 0x9E3779B9u) {}
            u32 next()
            {
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                return s;
            }
        };

        const pksm::Stat kStats[6] = { pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPD,
            pksm::Stat::SPATK, pksm::Stat::SPDEF };

        // Gen 6 (PK6) and, since v0.55, Gen 7 (PK7: same 232-byte layout for everything set here)
        bool buildGen6(const CatchTemplate& t, const CatchTrainer& tr, bool shiny, bool female, Rng& rng,
            const Date& today, std::unique_ptr<pksm::PKX>& out, std::string& why, bool gen7 = false)
        {
            if (t.len != (gen7 ? pksm::PK7::BOX_LENGTH : pksm::PK6::BOX_LENGTH))
            {
                why = gen7 ? "template is not a Gen 7 Pokemon" : "template is not a Gen 6 Pokemon";
                return false;
            }
            std::vector<u8> buf(t.data, t.data + t.len);
            pksm::Generation g = gen7 ? pksm::Generation::SEVEN : pksm::Generation::SIX;
            auto pk = pksm::PKX::getPKM(g, buf.data(), buf.size());
            auto* p6 = !gen7 && pk && pk->generation() == g ? static_cast<pksm::PK6*>(pk.get()) : nullptr;
            auto* p7 = gen7 && pk && pk->generation() == g ? static_cast<pksm::PK7*>(pk.get()) : nullptr;
            if (!p6 && !p7)
            {
                why = "template cannot be read";
                return false;
            }
            // Gen 7 templates of a Gen 6 origin (the X/Y Ditto) list the Gen 7 player as handler under the template's
            // trainer name "PLAYER": that becomes the player too (read before the OT name changes)
            bool htIsPlayer = p7 && p7->htName() == "PLAYER";
            // the origin save's trainer (the template was made for "PLAYER" 12345/54321)
            pk->otName(tr.name);
            pk->TID(tr.tid);
            pk->SID(tr.sid);
            pk->otGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
            pk->language(tr.language);
            if (p6)
            {
                p6->country(tr.country);
                p6->region(tr.subRegion);
                p6->consoleRegion(tr.consoleRegion);
            }
            else
            {
                p7->country(tr.country);
                p7->region(tr.subRegion);
                p7->consoleRegion(tr.consoleRegion);
                if (htIsPlayer)
                {
                    p7->htName(tr.name);
                    p7->htGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
                }
            }
            pk->nickname(i18n::species(tr.language, pk->species()));
            pk->nicknamed(false);
            // free in every Gen 6 origin: encryption constant, PID, nature, IVs (keeping guaranteed perfect ones)
            pk->encryptionConstant(rng.next());
            pk->PID(rng.next());
            pk->shiny(shiny);
            pk->nature(pksm::Nature{(u8)(rng.next() % 25)});
            for (pksm::Stat s : kStats)
                pk->iv(s, (u8)(rng.next() % 32));
            for (int placed = 0; placed < t.fixedIVs && placed < 6;)
            {
                pksm::Stat s = kStats[rng.next() % 6];
                if (pk->iv(s) != 31)
                {
                    pk->iv(s, 31);
                    placed++;
                }
                else if (placed + 1 > 6)
                    break;
                else
                {
                    // already 31 by chance: counts only if every stat is checked once below
                    int perfect = 0;
                    for (pksm::Stat x : kStats)
                        perfect += pk->iv(x) == 31;
                    placed = perfect;
                }
            }
            // gender (Gen 6 stores it apart from the PID): the walker's, if the species allows it
            u8 ratio = gen7 ? pksm::PersonalSMUSUM::gender(pk->formSpecies()) : pksm::PersonalXYORAS::gender(pk->formSpecies());
            pk->gender(ratio == 255 ? pksm::Gender::Genderless : ratio == 254 ? pksm::Gender::Female
                       : ratio == 0 ? pksm::Gender::Male : female ? pksm::Gender::Female : pksm::Gender::Male);
            pk->metDate(today);
            if (t.flags & 2)
                pk->eggDate(today);
            pk->refreshChecksum();
            out = std::move(pk);
            return true;
        }

        std::string upper(std::string s)
        {
            for (char& c : s)
                c = (char)std::toupper((unsigned char)c);
            return s;
        }

        bool buildGen2(const CatchTemplate& t, const CatchTrainer& tr, const pksm::PKX* shell, bool shiny,
            bool female, Rng& rng, std::unique_ptr<pksm::PKX>& out, std::string& why)
        {
            if (t.len != 32 || !shell || shell->generation() != pksm::Generation::TWO)
            {
                why = "Gen 2 template or game Pokemon missing";
                return false;
            }
            auto base = shell->rawData();
            std::vector<u8> raw(base.begin(), base.end());
            if (raw.size() != pksm::PK2::INT_LENGTH_WITH_NAMES && raw.size() != pksm::PK2::JP_LENGTH_WITH_NAMES)
            {
                why = "game Pokemon has an unknown Gen 2 layout";
                return false;
            }
            u8* st = raw.data() + 3; // list format: count, species, 0xFF, then the Pokemon (party layout, 48 bytes)
            u8 tidHi = st[6], tidLo = st[7];
            memcpy(st, t.data, 32);
            memset(st + 32, 0, 16); // party part: recomputed below
            st[6] = (u8)(tr.tid >> 8);
            st[7] = (u8)tr.tid;
            (void)tidHi;
            (void)tidLo;
            raw[1] = st[0]; // the list's species byte
            // DVs: the walker's gender and shiny state
            u8 ratio = pksm::PersonalGSC::gender(st[0]);
            static const u8 kShinyAtk[8] = { 2, 3, 6, 7, 10, 11, 14, 15 };
            u8 atk = 0, def, spe, spc;
            bool wantFemale = ratio == 254 || (ratio != 255 && ratio != 0 && female);
            for (int tries = 0; tries < 64; tries++)
            {
                atk = shiny ? kShinyAtk[rng.next() % 8] : (u8)(rng.next() % 16);
                if (ratio == 255 || ratio == 254 || ratio == 0)
                    break;
                bool isFemale = (u32)(atk << 4) < ratio;
                if (isFemale == wantFemale)
                    break;
            }
            if (shiny)
                def = spe = spc = 10;
            else
            {
                def = (u8)(rng.next() % 16);
                spe = (u8)(rng.next() % 16);
                spc = (u8)(rng.next() % 16);
                if (def == 10 && spe == 10 && spc == 10) // not shiny by chance
                    spc = 11;
            }
            st[0x15] = (u8)(atk << 4 | def);
            st[0x16] = (u8)(spe << 4 | spc);
            // Crystal's catch data: the OT gender bit is the save's trainer's
            if (st[0x1D] || st[0x1E])
                st[0x1E] = (u8)((st[0x1E] & 0x7F) | (tr.female ? 0x80 : 0));
            auto pk = pksm::PKX::getPKM(pksm::Generation::TWO, raw.data(), raw.size());
            if (!pk)
            {
                why = "Gen 2 catch cannot be read back";
                return false;
            }
            pk->otName(tr.name);
            std::string nick = upper(i18n::species(pksm::Language::ENG, pk->species()));
            if (nick.size() > 10)
                nick.resize(10);
            pk->nickname(nick);
            pk->updatePartyData();
            out = std::move(pk);
            return true;
        }
    }

    bool buildCatch(const CatchTemplate& t, const CatchTrainer& tr, const pksm::PKX* shell, bool shiny, bool female,
        u32 seed, int day, int month, int year, std::unique_ptr<pksm::PKX>& out, std::string& why)
    {
        Rng rng(seed);
        if (!t.data)
        {
            why = "no template";
            return false;
        }
        if (t.gen == 6)
            return buildGen6(t, tr, shiny, female, rng, Date{(u8)day, (u8)month, (u32)year}, out, why);
        if (t.gen == 7)
            return buildGen6(t, tr, shiny, female, rng, Date{(u8)day, (u8)month, (u32)year}, out, why, true);
        if (t.gen == 2)
            return buildGen2(t, tr, shell, shiny, female, rng, out, why);
        why = "no catches for this generation yet";
        return false;
    }
}
