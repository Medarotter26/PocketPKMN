// walk_catch.cpp - v0.53: a walk's catch as a legal Pokemon (see include/walk_poke.hpp, buildCatch). PC-tested with
// PKHeX's own legality check (tests/run_catches.sh).
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "walk_poke.hpp"

#include "enums/Gender.hpp"
#include "enums/Language.hpp"
#include "enums/Move.hpp"
#include "enums/Nature.hpp"
#include "enums/Stat.hpp"
#include "personal/personal.hpp"
#include "pkx/PK2.hpp"
#include "pkx/PK3.hpp" // v0.94: Gen 3 catches
#include "pkx/PK4.hpp"
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

        // v0.64 (review 2026-11, finding 1): the species name as the GAME writes it for a Pokemon without a nickname,
        // in the game's own language - before, every catch got the English name, which PKHeX calls illegal on
        // Japanese / French / German / Korean / Chinese games ("Nickname does not match species name").
        // Tables made by PKHeX (pkhex-check names): romfs i18n/<lang>/species.txt (Gen 5+), species4.txt (Gen 3/4:
        // capitals), species2.txt (Gen 1/2); one name per line, line n = species n. "" if there is no such table or
        // line - the caller then uses the English name as before.
        const char* langFolder(pksm::Language l)
        {
            using L = pksm::Language;
            switch (l)
            {
                case L::JPN: return "jpn";
                case L::ENG: return "eng";
                case L::FRE: return "fre";
                case L::ITA: return "ita";
                case L::GER: return "ger";
                case L::SPA: return "spa";
                case L::KOR: return "kor";
                case L::CHS: return "chs";
                case L::CHT: return "cht";
                default: return nullptr;
            }
        }

        std::string gameSpeciesName(pksm::Language lang, int gen, u16 species)
        {
            static std::vector<std::string> table;
            static int tableLang = -1, tableGen = -1;
            const char* folder = langFolder(lang);
            if (!folder)
                return "";
            int kind = gen <= 2 ? 2 : gen <= 4 ? 4 : 0;
            if (tableLang != (int)lang || tableGen != kind)
            {
                table.clear();
                tableLang = (int)lang;
                tableGen  = kind;
                std::string path = std::string(_PKSMCORE_LANG_FOLDER) + folder +
                                   (kind == 2 ? "/species2.txt" : kind == 4 ? "/species4.txt" : "/species.txt");
                if (FILE* f = fopen(path.c_str(), "rb"))
                {
                    char line[96];
                    while (fgets(line, sizeof(line), f))
                    {
                        line[strcspn(line, "\r\n")] = 0;
                        table.emplace_back(line);
                    }
                    fclose(f);
                }
            }
            return species < table.size() ? table[species] : std::string();
        }

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
            // v0.93: also Gen 6 - an ORAS catch from the other ORAS game or from X / Y (tools/pkhex_check_Templates.cs)
            bool htIsPlayer = (p7 && p7->htName() == "PLAYER") || (p6 && p6->htName() == "PLAYER");
            // the origin save's trainer (the template was made for "PLAYER" 12345/54321)
            pk->otName(tr.name);
            pk->TID(tr.tid);
            pk->SID(tr.sid);
            pk->otGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
            // v0.64: a Pokemon of X / Y origin (the Elite Bazaar Ditto in Gen 7) cannot be Chinese - X / Y have no
            // Chinese; on a Chinese game it keeps the template's English (PKHeX: "Language ID should be <= KOR")
            pksm::Language lang = tr.language;
            if (gen7 && (lang == pksm::Language::CHS || lang == pksm::Language::CHT) &&
                (pk->version() == pksm::GameVersion::X || pk->version() == pksm::GameVersion::Y))
                lang = pksm::Language::ENG;
            pk->language(lang);
            if (p6)
            {
                p6->country(tr.country);
                p6->region(tr.subRegion);
                p6->consoleRegion(tr.consoleRegion);
                if (htIsPlayer)
                {
                    p6->htName(tr.name);
                    p6->htGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
                }
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
            {
                std::string own = gameSpeciesName(lang, 6, u16(pk->species())); // v0.64: the game's language
                pk->nickname(own.empty() ? i18n::species(pksm::Language::ENG, pk->species()) : own);
            }
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

        // v0.65: a shiny PID for a hatched Gen 4 egg, built instead of drawn: low half random, high half = the
        // trainer's shiny value ^ low half ^ 2..7, with the nature (PID % 25), ability slot (bit 0) and gender (low
        // byte against the ratio; ratio 0 / 254 / 255 = no choice) asked for. PKSM-Core's own random shiny PID was
        // now and then one PKHeX reads as a Gen 5 gift's ("G5MGShiny": low half below 0x100, shiny value 0 or 1) and
        // calls illegal for an egg - about 1 in 300 shiny catches; both parts of that pattern are avoided here.
        u32 shinyPid4(Rng& rng, u32 tsv, u32 nature, u32 abilityBit, u8 ratio, bool wantFemale)
        {
            const bool free = ratio == 0 || ratio == 254 || ratio == 255;
            for (;;)
            {
                u32 low = rng.next() & 0xFFFF;
                if ((low & 0xFF00) == 0 || (low & 1) != abilityBit)
                    continue;
                if (!free && ((low & 0xFF) < ratio) != wantFemale)
                    continue;
                u32 x   = 2 + rng.next() % 6;
                u32 pid = (((tsv ^ low ^ x) & 0xFFFF) << 16) | low;
                if (pid % 25 == nature)
                    return pid;
            }
        }

        // v0.61 Gen 4 (Diamond / Pearl / Platinum). A hatched-egg template (flags & 2): a new PID through PKSM-Core's
        // setters (each re-rolls the PID keeping the ability slot and the other choices) for nature, gender and
        // shininess, random IVs. A wild template: PID and IVs stay exactly as PKHeX made them (Gen 4 wild PIDs and
        // IVs come from one RNG frame; any change would be illegal) - only the trainer changes.
        bool buildGen4(const CatchTemplate& t, const CatchTrainer& tr, bool shiny, bool female, Rng& rng,
            const Date& today, std::unique_ptr<pksm::PKX>& out, std::string& why)
        {
            if (t.len != pksm::PK4::BOX_LENGTH)
            {
                why = "template is not a Gen 4 Pokemon";
                return false;
            }
            std::vector<u8> buf(t.data, t.data + t.len);
            auto pk = pksm::PKX::getPKM(pksm::Generation::FOUR, buf.data(), buf.size());
            if (!pk || pk->generation() != pksm::Generation::FOUR)
            {
                why = "template cannot be read";
                return false;
            }
            pk->otName(tr.name);
            pk->TID(tr.tid);
            pk->SID(tr.sid);
            pk->otGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
            pk->language(tr.language);
            // Gen 4 shows species names in capitals (as PKHeX's template has it in English)
            {
                std::string own = gameSpeciesName(tr.language, 4, u16(pk->species())); // v0.64: the game's language
                pk->nickname(own.empty() ? upper(i18n::species(pksm::Language::ENG, pk->species())) : own);
            }
            pk->nicknamed(false);
            if (t.flags & 2) // hatched egg: PID and IVs are free
            {
                pk->nature(pksm::Nature{(u8)(rng.next() % 25)});
                u8 ratio = pksm::PersonalDPPtHGSS::gender(pk->formSpecies());
                pk->gender(ratio == 255 ? pksm::Gender::Genderless : ratio == 254 ? pksm::Gender::Female
                           : ratio == 0 ? pksm::Gender::Male : female ? pksm::Gender::Female : pksm::Gender::Male);
                pk->shiny(shiny);
                if (shiny) // v0.65: same nature, ability slot and gender, a PID PKHeX accepts for an egg
                    pk->PID(shinyPid4(rng, (u32)(pk->TID() ^ pk->SID()), pk->PID() % 25, pk->PID() & 1, ratio,
                        pk->gender() == pksm::Gender::Female));
                // A one-gender evolution of a mixed-gender Pokemon keeps the gender its PID gave the pre-evolution
                // (Gen 4 reads gender from the PID): the PID's low byte must fit the PRE-evolution's ratio (PKHeX:
                // "PID-Gender mismatch" otherwise). Same nature, ability slot and shininess are kept.
                struct OneGender
                {
                    u16 species;
                    u8 preRatio;
                    bool female;
                };
                static const OneGender kOneGender[] = {
                    {478, 127, true},  // Froslass  <- female Snorunt
                    {416, 31, true},   // Vespiquen <- female Combee
                    {413, 127, true},  // Wormadam  <- female Burmy
                    {475, 127, false}, // Gallade   <- male Kirlia
                    {414, 127, false}, // Mothim    <- male Burmy
                };
                for (const OneGender& g : kOneGender)
                    if (u16(pk->species()) == g.species)
                    {
                        u32 old = pk->PID();
                        u32 tsv = (u32)(pk->TID() ^ pk->SID());
                        if (shiny) // v0.65: built, not searched (a search could run out of tries)
                        {
                            pk->PID(shinyPid4(rng, tsv, old % 25, old & 1, g.preRatio, g.female));
                            break;
                        }
                        for (int tries = 0; tries < 2000000; tries++)
                        {
                            u32 pid = rng.next();
                            bool sh = ((tsv ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8);
                            bool fem = (pid & 0xFF) < g.preRatio;
                            if (pid % 25 == old % 25 && (pid & 1) == (old & 1) && sh == shiny && fem == g.female)
                            {
                                pk->PID(pid);
                                break;
                            }
                        }
                    }
                // v0.97: new IVs (same PID: nature, gender, ability slot, shininess kept) while PID + IVs happen to fit
                // a Gen 3 event pattern - PKHeX would read the egg as that event ("PID+ correlation"; about 1 in
                // 250,000, seen in tests/run_catches.sh: a DPPt Wooper, PID 525CAACC, as BACD_ES)
                for (int tries = 0; tries < 64; tries++)
                {
                    for (pksm::Stat s : kStats)
                        pk->iv(s, (u8)(rng.next() % 32));
                    const u8 iv[6] = { pk->iv(pksm::Stat::HP), pk->iv(pksm::Stat::ATK), pk->iv(pksm::Stat::DEF),
                        pk->iv(pksm::Stat::SPATK), pk->iv(pksm::Stat::SPDEF), pk->iv(pksm::Stat::SPD) };
                    if (!pidIvLooksLikeEvent3(pk->PID(), iv, (u16)(pk->TID() ^ pk->SID())))
                        break;
                }
                pk->eggDate(today);
            }
            pk->metDate(today);
            pk->refreshChecksum();
            out = std::move(pk);
            return true;
        }

        // v0.94 (FRLG courses; plan docs/FRLG_COURSES_PLAN.md): Gen 3 - FireRed / LeafGreen. The template is PKHeX's
        // legal Pokemon (PK3, 80-byte decrypted box data). Gen 3 PIDs and IVs come from the game's random numbers and
        // PKHeX checks that, so a new pair is made the way the game makes it - Method 1 (GBA RNG
        // seed * 0x41C64E6D + 0x6073; PID = second call << 16 | first call, then two calls for the IVs) - for wild,
        // static and gift templates; a hatched egg keeps PKHeX's own PID and IVs (below). Checked: tests/run_catches.sh.
        // Never shiny (owner default for Gen 3: a shiny Method 1 PID would need a long search). Nature, gender and
        // the ability slot follow from the PID (the ability bit = PID & 1, as the game sets it).
        bool buildGen3(const CatchTemplate& t, const CatchTrainer& tr, Rng& rng, std::unique_ptr<pksm::PKX>& out,
            std::string& why)
        {
            if (t.len != pksm::PK3::BOX_LENGTH)
            {
                why = "template is not a Gen 3 Pokemon";
                return false;
            }
            std::vector<u8> buf(t.data, t.data + t.len);
            auto pk = pksm::PKX::getPKM(pksm::Generation::THREE, buf.data(), buf.size());
            if (!pk || pk->generation() != pksm::Generation::THREE)
            {
                why = "template cannot be read";
                return false;
            }
            // v0.96 (owner's first FireRed catch, a 3-letter OT: PKHeX "Final terminator missing"): the 7-byte OT field is
            // erased to 0xFF first, as the game has it - writing a shorter name over the template's "PLAYER" left
            // "ER" after the end mark. 0x14 = PK3 OT name (PKHeX PK3, PKSM-Core PK3::otName).
            {
                u8* raw = pk->rawData().data();
                memset(raw + 0x14, 0xFF, 7);
            }
            // v0.97: a Colosseum / XD origin (RSE courses: Espeon, Umbreon, Hitmontop) comes as from the player's own
            // Colosseum / XD game: those games make the trainer ID (and from it the PID) with their own RNG and the
            // player is always male (PKHeX: "Trainer ID is not obtainable from any RNG seed", "OT from Colosseum/XD
            // cannot be female") - so it keeps the template's TID, SID and gender; only the name is the player's
            const bool cxd = pk->version() == pksm::GameVersion::CXD;
            pk->otName(tr.name);
            if (!cxd)
            {
                pk->TID(tr.tid);
                pk->SID(tr.sid);
                pk->otGender(tr.female ? pksm::Gender::Female : pksm::Gender::Male);
            }
            pk->language(tr.language);
            {
                std::string own = gameSpeciesName(tr.language, 4, u16(pk->species())); // Gen 3 names = Gen 4 capitals
                pk->nickname(own.empty() ? upper(i18n::species(pksm::Language::ENG, pk->species())) : own);
            }
            pk->nicknamed(false);
            // a hatched egg keeps PKHeX's own legal PID and IVs (a Method 1 PID makes PKHeX read it as a different
            // encounter: "PID+ correlation" / "Fateful Encounter flag missing"); v0.97: so do a Colosseum / XD origin
            // (its own PID method) and Unown (its letter comes from the PID and must fit its Tanoby chamber)
            if ((t.flags & 2) || cxd || u16(pk->species()) == 201)
            {
                pk->refreshChecksum();
                out = std::move(pk);
                return true;
            }
            const u32 tsv = (u32)(tr.tid ^ tr.sid);
            for (int tries = 0; tries < 64; tries++)
            {
                u32 seed = rng.next();
                auto call = [&seed]() {
                    seed = seed * 0x41C64E6Du + 0x6073u;
                    return (u16)(seed >> 16);
                };
                u16 lo = call(), hi = call(), iv1 = call(), iv2 = call();
                u32 pid = ((u32)hi << 16) | lo;
                if ((tsv ^ hi ^ lo) < 8) // shiny: take another seed
                    continue;
                pk->PID(pid);
                // the ability bit = PID & 1 only where the species has two abilities (PKHeX: "Ability does not match
                // ability number" for a one-ability species with the bit set)
                bool two = pk->abilities(1) != pksm::Ability::None && pk->abilities(1) != pk->abilities(0);
                static_cast<pksm::PK3*>(pk.get())->abilityBit(two && (pid & 1) != 0);
                pk->iv(pksm::Stat::HP, (u8)(iv1 & 31));
                pk->iv(pksm::Stat::ATK, (u8)((iv1 >> 5) & 31));
                pk->iv(pksm::Stat::DEF, (u8)((iv1 >> 10) & 31));
                pk->iv(pksm::Stat::SPD, (u8)(iv2 & 31));
                pk->iv(pksm::Stat::SPATK, (u8)((iv2 >> 5) & 31));
                pk->iv(pksm::Stat::SPDEF, (u8)((iv2 >> 10) & 31));
                pk->refreshChecksum();
                out = std::move(pk);
                return true;
            }
            why = "no PID found";
            return false;
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
            std::string nick = gameSpeciesName(tr.language, 2, u16(pk->species())); // v0.64: the game's language
            if (nick.empty())
            {
                nick = upper(i18n::species(pksm::Language::ENG, pk->species()));
                if (nick.size() > 10)
                    nick.resize(10);
            }
            pk->nickname(nick);
            pk->updatePartyData();
            out = std::move(pk);
            return true;
        }
    }

    // v0.97: PKHeX MethodFinder.GetBACDMatch + CommonEvent3 (read from PKHeX master 2026-11-18): every seed whose
    // two 15-bit calls give the IVs (brute force over the 16 low bits instead of PKHeX's lattice reversal - same
    // seeds); the two calls before them are B (low PID half) and A (high), one more before = X.
    bool pidIvLooksLikeEvent3(u32 pid, const u8 iv[6], u16 idXor)
    {
        auto next = [](u32 s) { return s * 0x41C64E6Du + 0x6073u; };
        auto prev = [](u32 s) { return s * 0xEEB9EB65u + 0x0A3561A1u; };
        const u32 iv1 = iv[0] | (iv[1] << 5) | (iv[2] << 10); // HP, Atk, Def
        const u32 iv2 = iv[5] | (iv[3] << 5) | (iv[4] << 10); // Spe, SpA, SpD
        const u32 x = idXor;
        const bool shiny = ((x ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8);
        auto antishiny = [x](u32 a, u32 b) { return (((a ^ (x ^ b)) << 16) | b); };
        for (u32 low = 0; low < 0x10000; low++)
        {
            const u32 seed = (iv1 << 16) | low;
            if ((next(seed) & 0x7FFF0000u) != (iv2 << 16))
                continue;
            const u32 s0 = prev(seed);
            for (u32 s : { s0, s0 ^ 0x80000000u })
            {
                const u32 b16 = s >> 16;
                const u32 p = prev(s);
                const u32 a16 = p >> 16;
                const u32 x16 = prev(p) >> 16;
                const u32 expect = (a16 << 16) | b16;
                if (expect == pid)
                    return true; // BACD
                if (!shiny)
                {
                    if (((expect + 8) & 0xFFFFFFF8u) == pid)
                        return true; // BACD_A
                    if ((a16 & ~7u) != 0 && antishiny(a16, b16) == pid)
                        return true; // BACD_AX
                    if (x16 == pid >> 16 && (pid & 7) == (b16 & 7) && ((x ^ x16) & 0xFFF8) != (pid & 0xFFF8))
                        return true; // BACD_ES
                }
                else
                {
                    const u32 delta = pid - expect;
                    if ((pid & 7) == 0 && delta != 0 && delta < 8)
                        return true; // BACD_EA
                    if ((a16 & ~7u) != 0 && (pid & 0xFFFF) == b16 && (antishiny(a16, b16) >> 19) != (pid >> 19))
                        return true; // BACD_EAX
                    if (x16 == pid >> 16 && ((x16 << 16) | (((x ^ x16) & 0xFFF8) | (b16 & 7))) == pid)
                        return true; // BACD_S
                }
            }
        }
        return false;
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
        if (t.gen == 4)
            return buildGen4(t, tr, shiny, female, rng, Date{(u8)day, (u8)month, (u32)year}, out, why);
        if (t.gen == 3) // v0.94: never shiny, gender from the PID (Gen 3)
            return buildGen3(t, tr, rng, out, why);
        why = "no catches for this generation yet";
        return false;
    }
}
