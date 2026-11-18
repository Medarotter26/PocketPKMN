// Make a save change exactly the way the game itself would change it.
//
// User requirement (2026-11-14): a save must change only by the Pokemon going
// in or out - "consider what the actual game would do, and make that choice".
// PKSM-Core's finishEditing() also rewrites things the game would not touch on
// a PC deposit/withdraw (it flushes the open box to its storage slot, and for
// Gen 2 writes a second checksum). gameMatchFinish() runs finishEditing() and
// then puts back every byte the game would have left alone.
//
// Portable C++ (no 3DS headers) so the PC test runs the same code as the app.
#pragma once

#include "sav/Sav.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace walkerbox
{
    // True if this save's layout is handled: Gen 1 or Gen 2, international
    // (not Japanese / Korean - their box layouts differ and are not verified).
    bool gameMatchSupported(const pksm::Sav& sav, std::string& why);

    // finishEditing() + put back what the game would not change, given that one
    // Pokemon went into or out of box `editedBox`. `length` is the save size.
    // Returns the number of bytes put back (for the log).
    uint32_t gameMatchFinish(pksm::Sav& sav, uint32_t length, int editedBox, std::string& note);

    // Gen 3 (Ruby/Sapphire/Emerald/FireRed/LeafGreen). A Gen 3 game keeps two copies
    // of its save (slots A and B, 14 sectors of 0x1000 each, every sector with a
    // footer: id, checksum, signature 0x08012025, save counter). A box change only
    // lasts once the player saves, and saving writes ALL 14 sectors into the OTHER
    // slot with counter + 1 and the sector order rotated by one (pret's save.c:
    // gLastWrittenSector++, gSaveCounter++). The newest copy stays as the backup.
    //
    // `before` is the 0x20000-byte save as it was before the edit; `sav` holds the
    // edit (PKSM-Core writes into the current slot). The result, written into
    // sav's buffer: `before` with the other slot replaced by the edited sectors,
    // renumbered as the game would. Refused (false, nothing changed) if anything
    // but box sectors (ids 5-13) was edited, or the save is not as expected.
    bool gameMatchFinish3(pksm::Sav& sav, const uint8_t* before, uint32_t length, std::string& note);
    bool gameMatchItems3(pksm::Sav& sav, const uint8_t* before, uint32_t length, std::string& note); // v0.94

    // Gen 6 (X/Y, Omega Ruby/Alpha Sapphire). One save file, made of blocks, each
    // with a CRC-16 in the block table at the end. When the game takes a Pokemon
    // out of a box slot it does not zero the slot: it writes a blank Pokemon whose
    // only set fields are the nickname "Egg" and the console's country, region,
    // 3DS region and language (seen in the user's X and Alpha Sapphire saves).
    // The result: `before` with only the edited box's slots and the box block's
    // CRC changed; a slot emptied by the edit gets the game's blank Pokemon (copied
    // from the save itself if it has one, else built from the trainer's data).
    // Refused (false) if the save's checksums were not valid to begin with or
    // anything outside that box would change.
    bool gameMatchFinish6(pksm::Sav& sav, const uint8_t* before, uint32_t length, int editedBox, std::string& note);

    // v0.56: Gen 7 (Sun/Moon, Ultra Sun/Ultra Moon). Like Gen 6 plus the 128-byte signature over the block table
    // (memecrypto, recomputed by PKSM-Core). Learned from the user's Moon: an emptied slot is the all-zero Pokemon
    // (what PKSM-Core writes). Result: `before` with only the edited box, the box block's checksum and the
    // signature changed; refused if the checksums were not valid before or anything else would change.
    bool gameMatchFinish7(pksm::Sav& sav, const uint8_t* before, uint32_t length, int editedBox, std::string& note);

    // v0.62: Gen 4 (Diamond / Pearl / Platinum): only the edited box in the storage block + that block's CRC may
    // change (learned from the user's Checkpoint pairs). Refused otherwise.
    // v0.67: saved the game's way - the result is `before` with the edited boxes block written into the OTHER half
    // (block counter + 1, its CRC); the current copy is untouched. `sav` still looks at the old copy afterwards:
    // read the save again before using it (as after gameMatchFinish3). gameMatchItems does the same for the
    // trainer-data block of a Gen 4 save.
    bool gameMatchFinish4(pksm::Sav& sav, const uint8_t* before, uint32_t length, int editedBox, std::string& note);

    // v0.53: items added to the save (Gen 2: the PC item list; X/Y: the bag) - only what the game itself changes when
    // it stores an item: that one item list, plus Gen 2's main checksum / X/Y's block checksum. Everything else is
    // put back. Refused (false, sav as before) if the edit touched more than one item list or anything else changed.
    // v0.94: Gen 3 (FireRed / LeafGreen): the PC item list in the large block, saved as the game does - a new copy in
    // the other slot (as gameMatchFinish3); read the save again afterwards.
    bool gameMatchItems(pksm::Sav& sav, const uint8_t* before, uint32_t length, std::string& note);

    // v0.53 (user: "items sent over should go to pc, not item bag. Unless its a generation without an item pc"):
    // adds one item to the save in memory - Gen 2: the PC item storage; X/Y (no item PC): the bag pocket that holds
    // it. Stacks onto the same item first. gen2Id = the id is a Gen 2 item number (GSC courses), else national.
    // where = "PC" / the pocket's name. False (why) if there is no room or the item does not exist in that game.
    bool addItemToSave(pksm::Sav& sav, uint16_t id, bool gen2Id, std::string& where);

    // The game's blank Pokemon for this Gen 6 save, encrypted as stored (0xE8 bytes):
    // `found` says whether it was copied from the save or built.
    std::vector<uint8_t> gen6Blank(const pksm::Sav& sav, const uint8_t* save, bool& found);

    // The current Gen 3 slot as the game picks it (0 = A, 1 = B, -1 = none), with
    // its counter and the physical position of sector 0. Used by the PC test too.
    int gen3CurrentSlot(const uint8_t* save, uint32_t& counter, int& rotation);
    // v0.94: a FireRed / LeafGreen event flag (0 / 1) from the current copy of a 128 KiB save; -1 = no valid copy.
    // 0x82C = "game clear" (Hall of Fame entered), 0x820-0x827 = the badges.
    int gen3FlagFRLG(const uint8_t* save, uint32_t length, int flag);
    // v0.97: any Gen 3 game; flagsAt = its event flags in the large block (RS 0x1220, Emerald 0x1270, FRLG 0xEE0).
    // Ruby / Sapphire: game clear 0x804, badges 0x807-0x80E; Emerald 0x864 / 0x867-0x86E (pokeruby / pokeemerald).
    int gen3Flag(const uint8_t* save, uint32_t length, uint32_t flagsAt, int flag);
}
