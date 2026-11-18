// v0.89 (docs/MOVE_RULES_PLAN.md V1): see include/movecheck.hpp.
#include "movecheck.hpp"

#include "utils/i18n.hpp"

namespace walkerbox
{
    namespace
    {
        const pksm::Language L = pksm::Language::ENG;

        std::string firstBadMove(const pksm::Sav& target, const pksm::PKX& pk)
        {
            for (int i = 0; i < 4; i++)
            {
                pksm::Move m = pk.move((u8)i);
                if (!target.availableMoves().count(m))
                    return "the move " + i18n::move(L, m);
            }
            for (int i = 0; i < 4; i++)
            {
                pksm::Move m = pk.relearnMove((u8)i);
                if (!target.availableMoves().count(m))
                    return "the move " + i18n::move(L, m) + " (one it can relearn)";
            }
            return "one of its moves";
        }
    }

    std::string moveCheck(const pksm::Sav& target, const pksm::PKX& pk)
    {
        using R = pksm::Sav::BadTransferReason;
        // PKSM-Core's Sun/Moon move list ends at Mind Blown, one too far: Sun/Moon's last move is 10,000,000 Volt
        // Thunderbolt (PKHeX MaxMoveID_7 = 719). Mind Blown, Plasma Fists and Photon Geyser came with Ultra Sun/Ultra
        // Moon (found 2026-11-17 checking the owner's list: Poipole's line, Zeraora, the other Ultra Beasts,
        // Ultra Necrozma). Every other range was checked against PKHeX's maximums and matches.
        if (target.version() == pksm::GameVersion::SN || target.version() == pksm::GameVersion::MN)
            for (int i = 0; i < 8; i++)
            {
                pksm::Move m = i < 4 ? pk.move((u8)i) : pk.relearnMove((u8)(i - 4));
                if (m > pksm::Move::TenMillionVoltThunderbolt)
                    return "the move " + i18n::move(L, m) + (i < 4 ? "" : " (one it can relearn)");
            }
        // Held items PKSM-Core's lists allow (their numbers are inside the older game's range) but the older game does
        // not have (owner: "a Pokemon holding the gitrious orb being sent to Diamond"). Found by comparing PKHeX's
        // held-item lists of every game with its same-generation games (2026-11-17): only these three; every other
        // newer item is above the older game's last item and already refused.
        {
            pksm::GameVersion v = target.version();
            u16 item            = pk.heldItem();
            bool dp = v == pksm::GameVersion::D || v == pksm::GameVersion::P;
            bool xy = v == pksm::GameVersion::X || v == pksm::GameVersion::Y;
            if ((dp && item == 112) ||              // Griseous Orb (Platinum on)
                (xy && (item == 534 || item == 535))) // Red Orb, Blue Orb (Omega Ruby / Alpha Sapphire)
                return "its held item (" + i18n::item(L, item) + ")";
        }
        R r = target.invalidTransferReason(pk);
        if (r == R::ITEM && target.generation() == pksm::Generation::ONE)
            r = R::OKAY; // Gen 1 has no held items (the byte is the catch rate)
        switch (r)
        {
            case R::OKAY:
                return "";
            case R::MOVE:
                return firstBadMove(target, pk);
            case R::SPECIES:
                return i18n::species(L, pk.species());
            case R::FORM:
                return "this form of " + i18n::species(L, pk.species());
            case R::ABILITY:
                return "the ability " + i18n::ability(L, pk.ability());
            case R::ITEM:
                return "its held item (" + i18n::item(L, pk.heldItem()) + ")";
            case R::BALL:
                return "its ball (" + i18n::ball(L, pk.ball()) + ")";
        }
        return "something about it";
    }
}
