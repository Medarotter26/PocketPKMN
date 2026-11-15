// walk_poke.hpp - turns a stored Pokemon (PKSM-Core PKX, any generation in the bank) into the walking
// Pokemon's walk data: summary, nickname text + image, friendship, sprite from the pack.
#pragma once

#include <string>

#include "pkx/PKX.hpp"
#include "walk_compose.h"

namespace walkerbox
{
    // gen = the bank entry's generation (1-7). Returns false (and why) for eggs and species without a walker
    // sprite number (> 807). spriteFromPack = whether the sprite came from the pack (else the placeholder).
    bool walkPokeFromPkx(const pksm::PKX& pk, int gen, wbx_walk_poke& out, bool& spriteFromPack, std::string& why);

    // Nickname as plain text for the walker: the PKX nickname with characters the walker text and the
    // WalkerBox font cannot show (gender signs, other scripts) left out; the species' English name if
    // nothing is left.
    std::string walkerNickname(const pksm::PKX& pk);

    // --- W4: what a walk gives the Pokemon when it comes back ------------------------------------------------
    // Rules (pret/pokeheartgold overlay 112, return handler; RESUME "W4 research"):
    //  - EXP: 1 per step, at most up to the start of the next level (no move learning, no evolution); nothing
    //    at level 100.
    //  - Friendship (Gen 2+): if a level was gained, the game's level-up rule: +5 (below 100) / +3 (100-199) /
    //    +2 (200+), +1 Luxury Ball, the sum x1.5 with a Soothe Bell; then +1 if the walk had >= 5000 steps.
    //    (The game's "met location 8" +1 is a HeartGold/SoulSilver map rule and is left out.)
    //  - EV walking (the user's rule, 2026-11-14): +1 EV every 20 steps in the stat chosen at send, capped at
    //    the game's own limits (255 per stat in Gen 3-5, 252 in Gen 6-7, 510 in all); off for Gen 1/2.
    enum WalkEvStat { EV_OFF = -1, EV_HP = 0, EV_ATK, EV_DEF, EV_SPA, EV_SPD, EV_SPE };

    struct WalkGains
    {
        u32 steps = 0;
        u32 expBefore = 0, expAfter = 0;
        u8 levelBefore = 0, levelAfter = 0;
        bool friendship = false; // false for Gen 1 (no friendship)
        u8 friendBefore = 0, friendAfter = 0;
        int evStat = EV_OFF;
        u16 evBefore = 0, evAfter = 0;
    };

    // Changes pk in place (and its party data for Gen 1/2). Returns false if nothing could be applied.
    bool applyWalkGains(pksm::PKX& pk, int gen, u32 steps, int evStat, WalkGains& out);

    // The game's own EV cap per stat for a generation (0 = no EVs: Gen 1/2).
    u16 walkEvCap(int gen);
    const char* walkEvName(int evStat); // "Off", "HP", "Atk", "Def", "SpA", "SpD", "Spe"
}
