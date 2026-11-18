// v0.89 (docs/MOVE_RULES_PLAN.md V1; owner: "Games using the box within same generation is fine as long as we have
// this guard", "Yes, build part 1"): may this Pokemon go into this game version at all?
//
// The check is PKSM-Core's own Sav::invalidTransferReason (what PKSM uses for its bank): species, form, the four
// moves and relearn moves, ability, held item and Poke Ball against the target version's lists (VersionTables) -
// they tell Sun/Moon from Ultra Sun/Ultra Moon, Diamond/Pearl from Platinum, Ruby/Sapphire from Emerald, etc.
// One exception: a Gen 1 game has no held items - a Gen 1 Pokemon's "item" byte is its catch rate, so the item
// check is not used for a Gen 1 target.
// Not covered (a save's progress, not a list): FireRed/LeafGreen take Hoenn Pokemon only after the National Dex.
//
// Portable C++ (no 3DS headers) so the PC test (tests/run_movecheck.sh) runs the same code as the app. Read only.
#pragma once

#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"

#include <string>

namespace walkerbox
{
    // "" = it may go in. Otherwise what is missing there, for the message, e.g. "Poipole" or "the move Clangorous
    // Soulblaze" ("<that> does not exist in <game>").
    std::string moveCheck(const pksm::Sav& target, const pksm::PKX& pk);
}
