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

    // v0.81 (names step 1, owner: "Go 12"): the text for the name PICTURE. Keeps every character
    // the WalkerBox font draws (printable ASCII, accented Latin letters, oe/OE, sharp s, gender signs), cut
    // to 10 characters like walkerNickname. If walkerNickname would fall back to the English species name
    // (nothing ASCII in the nickname, e.g. Japanese / Korean / Chinese), it returns exactly walkerNickname.
    // The stored Gen 4 text (wbx_text_pack) keeps walkerNickname.
    std::string walkerNicknamePicture(const pksm::PKX& pk);

    // The same two rules on strings (PC tests): nickname as PKSM-Core gives it (UTF-8) + the species'
    // English name.
    std::string walkerNicknameText(const std::string& nickname, const std::string& englishSpecies);
    std::string walkerNicknamePictureText(const std::string& nickname, const std::string& englishSpecies);

    // --- W4: what a walk gives the Pokemon when it comes back ------------------------------------------------
    // Rules (pret/pokeheartgold overlay 112, return handler; RESUME "W4 research"):
    //  - EXP: 1 per step, at most up to the start of the next level (no move learning, no evolution); nothing
    //    at level 100.
    //  - Friendship (Gen 2+): if a level was gained, the game's level-up rule: +5 (below 100) / +3 (100-199) /
    //    +2 (200+), +1 Luxury Ball, the sum x1.5 with a Soothe Bell; then +1 if the walk had >= 5000 steps.
    //    (The game's "met location 8" +1 is a HeartGold/SoulSilver map rule and is left out.)
    //  - EV walking (the user's rule, 2026-11-14): +1 EV every 20 steps in the stat chosen at send, capped at
    //    the game's own limits (255 per stat in Gen 3-5, 252 in Gen 6-7, 510 in all); off for Gen 1/2.
    //    v0.95 (owner, 2026-11-17: "let's go ahead and do"): Gen 3/4 only as far as the Pokemon's EXP since it was met
    //    can explain them (PKHeX EffortExpLegality: EVs above the vitamin part need battle EXP; tests/run_evwalk.sh),
    //    and none at level 100 (Gen 3/4 give no battle EVs at level 100); evLimited says the walk gave fewer.
    //    Never takes EVs away.
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
        bool evLimited = false; // v0.95: Gen 3/4 - fewer EVs than the walk's steps would give (EXP / level 100)
    };

    // v0.95: do a Gen 3/4 Pokemon's EVs fit its EXP since it was met (PKHeX's rule; Pokerus not counted, so a little
    // stricter)? Always true for other generations.
    bool walkEvsFitExp(const pksm::PKX& pk, int gen);

    // Changes pk in place (and its party data for Gen 1/2). Returns false if nothing could be applied.
    bool applyWalkGains(pksm::PKX& pk, int gen, u32 steps, int evStat, WalkGains& out);

    // The game's own EV cap per stat for a generation (0 = no EVs: Gen 1/2).
    u16 walkEvCap(int gen);

    // --- v0.53: legal catches (user: "Legality is very important. Please make sure any pokemon generated is legal") --
    // A catch is made from a template that PKHeX built and checked on the PC (tools/pkhex_check, romfs
    // routes/catch_*.bin): the course Pokemon at the course level with the course moves, from a legal origin in that
    // game (hidden ability where one is legal). Here only what is free in that origin is changed: the trainer, new
    // random PID / encryption constant / IVs (the template's guaranteed perfect IVs kept) / nature (Gen 6) or DVs
    // (Gen 2), shiny and gender as the walker says, and today's dates.
    struct CatchTemplate
    {
        int gen = 0;          // 2 or 6
        u8 flags = 0;         // 1 hidden ability, 2 egg, 4 traded once
        u8 fixedIVs = 0;      // IVs the origin guarantees at 31 (Gen 6)
        const u8* data = nullptr; // PK6: 232-byte decrypted box data; PK2: 32-byte box struct
        size_t len = 0;
    };
    struct CatchTrainer // the origin save's trainer
    {
        std::string name;
        u16 tid = 0, sid = 0;
        bool female = false;
        pksm::Language language = pksm::Language::ENG;
        u8 country = 0, subRegion = 0, consoleRegion = 0; // Gen 6
    };
    // shell = a Pokemon of the target game's format to start a Gen 2 catch from (the walked Pokemon: right list
    // format and language); unused for Gen 6. Returns false (why) if the template cannot be used.
    bool buildCatch(const CatchTemplate& t, const CatchTrainer& tr, const pksm::PKX* shell, bool shiny, bool female,
        u32 seed, int day, int month, int year, std::unique_ptr<pksm::PKX>& out, std::string& why);
    // v0.97: does this PID + IVs (HP, Atk, Def, SpA, SpD, Spe) fit a Gen 3 event's B-A-C-D pattern (PKHeX MethodFinder
    // GetBACDMatch, any variant)? A hatched Gen 4 egg that fits is read as that event: "PID+ correlation".
    bool pidIvLooksLikeEvent3(u32 pid, const u8 iv[6], u16 idXor);
    const char* walkEvName(int evStat); // "Off", "HP", "Atk", "Def", "SpA", "SpD", "Spe"
}
