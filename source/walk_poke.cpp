// walk_poke.cpp - stored Pokemon -> walk data (see include/walk_poke.hpp).
#include "walk_poke.hpp"

#include <algorithm>
#include <cstring>

#include "enums/Ball.hpp"
#include "enums/Stat.hpp"

#include "enums/Gender.hpp"
#include "enums/Language.hpp"
#include "utils/i18n.hpp"
#include "wbx_sprites.h"

namespace walkerbox
{
    namespace
    {
        // Keeps what wbx_text_pack can encode (ASCII letters, digits, space). 'é' / 'É' become 'e' / 'E' so
        // the stored text and the name image say the same thing.
        std::string plainText(const std::string& in)
        {
            std::string out;
            for (size_t i = 0; i < in.size(); i++) {
                unsigned char c = (unsigned char)in[i];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ')
                    out += (char)c;
                else if (c == 0xC3 && i + 1 < in.size() && ((unsigned char)in[i + 1] == 0xA9 ||
                                                            (unsigned char)in[i + 1] == 0x89)) {
                    out += (unsigned char)in[i + 1] == 0xA9 ? 'e' : 'E';
                    i++;
                }
                // anything else (gender signs, punctuation, other scripts) is left out
            }
            while (!out.empty() && out.back() == ' ')
                out.pop_back();
            return out;
        }
    }

    std::string walkerNickname(const pksm::PKX& pk)
    {
        std::string n = plainText(pk.nickname());
        if (n.empty())
            n = plainText(i18n::species(pksm::Language::ENG, pk.species()));
        if (n.size() > 10)
            n.resize(10);
        return n;
    }

    u16 walkEvCap(int gen)
    {
        return gen <= 2 ? 0 : gen <= 5 ? 255 : 252;
    }

    const char* walkEvName(int evStat)
    {
        static const char* const names[] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
        return evStat >= 0 && evStat < 6 ? names[evStat] : "Off";
    }

    namespace
    {
        pksm::Stat pksmStat(int evStat)
        {
            switch (evStat)
            {
                case EV_HP: return pksm::Stat::HP;
                case EV_ATK: return pksm::Stat::ATK;
                case EV_DEF: return pksm::Stat::DEF;
                case EV_SPA: return pksm::Stat::SPATK;
                case EV_SPD: return pksm::Stat::SPDEF;
                default: return pksm::Stat::SPD;
            }
        }
        constexpr u16 SOOTHE_BELL = 218; // national item number
    }

    bool applyWalkGains(pksm::PKX& pk, int gen, u32 steps, int evStat, WalkGains& g)
    {
        g          = WalkGains{};
        g.steps    = steps;
        g.evStat   = evStat;
        if (pk.egg())
            return false;
        g.levelBefore = g.levelAfter = pk.level();
        g.expBefore = g.expAfter = pk.experience();
        bool leveled = false;
        if (g.levelBefore < 100 && steps > 0)
        {
            auto next = pk.clone();
            next->level((u8)(g.levelBefore + 1));
            u32 need = next->experience() > g.expBefore ? next->experience() - g.expBefore : 0;
            if (steps >= need)
            {
                pk.level((u8)(g.levelBefore + 1)); // lands exactly at the start of the next level
                leveled = true;
            }
            else
                pk.experience(g.expBefore + steps);
        }
        g.expAfter   = pk.experience();
        g.levelAfter = pk.level();

        if (gen >= 2)
        {
            g.friendship  = true;
            int f         = pk.currentFriendship();
            g.friendBefore = (u8)f;
            if (leveled)
            {
                int delta = f < 100 ? 5 : f < 200 ? 3 : 2;
                if (gen >= 3 && pk.ball() == pksm::Ball::Luxury)
                    delta += 1;
                if (gen >= 2 && pk.heldItem() == SOOTHE_BELL)
                    delta = delta * 150 / 100;
                f = std::min(255, f + delta);
            }
            if (steps >= 5000 && f < 255)
                f += 1;
            pk.currentFriendship((u8)f);
            g.friendAfter = (u8)f;
        }

        if (evStat >= 0 && evStat < 6 && walkEvCap(gen) > 0)
        {
            pksm::Stat s = pksmStat(evStat);
            u32 total    = 0;
            for (int i = 0; i < 6; i++)
                total += pk.ev(pksmStat(i));
            u16 cur    = pk.ev(s);
            u32 add    = steps / 20;
            u32 room   = std::min<u32>(walkEvCap(gen) > cur ? walkEvCap(gen) - cur : 0, total < 510 ? 510 - total : 0);
            add        = std::min(add, room);
            g.evBefore = cur;
            pk.ev(s, (u16)(cur + add));
            g.evAfter = pk.ev(s);
        }
        if (gen <= 2 || pk.isParty())
            pk.updatePartyData(); // stored party stats follow the new level
        pk.refreshChecksum();     // Gen 3+ data carries a checksum over the changed bytes
        return true;
    }

    bool walkPokeFromPkx(const pksm::PKX& pk, int gen, wbx_walk_poke& out, bool& spriteFromPack, std::string& why)
    {
        std::memset(&out, 0, sizeof out);
        if (pk.egg()) {
            why = "An egg cannot go for a walk.";
            return false;
        }
        u16 species = u16(pk.species());
        if (species < 1 || species > WBX_SPRITE_LAST) {
            why = "No walker sprite number for species " + std::to_string(species) + ".";
            return false;
        }
        u16 moves[4];
        for (int i = 0; i < 4; i++)
            moves[i] = u16(pk.move(i));
        // Gen 1 has no held item (that byte is the catch rate) and no friendship.
        u16 item   = gen == 1 ? 0 : pk.heldItem();
        u16 form   = gen <= 2 ? 0 : pk.alternativeForm();
        u8 variant = (u8)(form & 0x1F) | (pk.gender() == pksm::Gender::Female ? WBX_VARIANT_FEMALE : 0);
        u8 flags   = (form ? WBX_FLAG_HAS_FORM : 0) | (pk.shiny() ? WBX_FLAG_SHINY : 0);
        wbx_summary_pack(out.summary, species, item, moves, pk.level(), variant, flags);
        std::string nick = walkerNickname(pk);
        wbx_text_pack(nick.c_str(), out.nickname, 11, 1);
        wbx_label_image(nick.c_str(), 80, 16, out.name_img);
        out.friendship = gen == 1 ? 70 : pk.currentFriendship();
        // Sprite only (user 2026-11-15: Gen 1/2 Pokemon use the Gen 1/2 set; Unown by its letter). The summary
        // above is unchanged.
        // v0.56: Gen 7 by its form too (Alolan sprites, romfs/sprites/forms)
        spriteFromPack =
            wbx_sprite_load_for(species, (gen == 2 || gen == 7) ? pk.alternativeForm() : 0, gen, out.sprite) == 1;
        return true;
    }
}
