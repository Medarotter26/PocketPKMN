// GBA Virtual Console ("inject") saves.
//
// The 3DS keeps a GBA title's save inside the title's "save data and content"
// archive, not in the normal save archive, as one file with two slots:
//   0x000            header 1 (0x200 bytes, magic ".SAV", save counter, CMAC)
//   0x200            save 1 (saveSize bytes, the GBA cartridge's save memory)
//   0x200 + size     header 2
//   0x400 + size     save 2
// The newest valid slot is the current save. Each header carries an AES-CMAC
// that only the 3DS can compute (FSPXI_CalcSavegameMAC), over SHA-256 of the
// header from 0x30 plus the save. Method and layout from PKSM (GPL-3.0,
// 3ds/source/titles/loader.cpp). The GBA side writes to this file only when
// the console goes back to the HOME menu after playing (AGB_FIRM -> reboot).
#pragma once

#ifdef __3DS__
#include <3ds/types.h>
#else
#include "utils/coretypes.h" // PC tests: PKSM-Core's u8/u32/u64
#endif

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace walkerbox::gbavc
{
    struct Header
    {
        u8 magic[4];      // ".SAV"
        u8 pad1[12];      // 0xFF
        u8 cmac[0x10];
        u8 pad2[0x10];    // 0xFF
        u32 contentId;    // 1
        u32 savesMade;    // save counter: the newer slot has the other's + 1
        u64 titleId;
        u8 sdCid[0x10];
        u32 saveOffset;   // 0x200
        u32 saveSize;
        u8 pad3[8];
        u8 arm7Registers[8];
        u8 pad4[0x198];
    };
    static_assert(sizeof(Header) == 0x200, "GBA VC save header is 0x200 bytes");

    // Which slot is the current save (portable; the CMAC results are passed in).
    struct Pick
    {
        int slot = 0;       // 1 or 2; 0 = none usable
        u32 headerOfs = 0;  // where that slot's header is
        u32 saveOfs = 0;    // where its save starts
        u32 saveSize = 0;
        std::string why;    // when slot == 0, or a note
    };

    // Offsets of both headers in a container file (header 2 may be missing).
    // Returns false if the file is not an initialised GBA VC save.
    bool findHeaders(const std::vector<u8>& file, u32& h1, u32& h2, bool& has1, bool& has2, std::string& why);

    // PKSM's rule: only valid slots count; if both are valid, slot 2 is newer
    // exactly when its counter is slot 1's + 1 (also across the wrap at 0xFFFFFFFF).
    Pick choose(const std::vector<u8>& file, u32 h1, u32 h2, bool has1, bool has2, bool valid1, bool valid2);

    // SHA-256 the CMAC is made from: header bytes 0x30..0x1FF, then the save.
    std::array<u8, 32> slotHash(const std::vector<u8>& file, u32 headerOfs, u32 saveSize);

    // What the 3DS itself does when a GBA game has saved: the new save goes into the
    // OTHER slot, its header a copy of the current one with the counter + 1 (the
    // CMAC is filled in on the console). The current slot is not touched, so an
    // interrupted write leaves the old save in use. Returns the new file and where
    // the new header is; false if the file has no room for the other slot.
    bool buildNext(const std::vector<u8>& file, const Pick& cur, const u8* save, u32 size, std::vector<u8>& out,
        u32& newHeaderOfs);

#ifdef __3DS__
    // Write `next` (from buildNext) to the title's save: the new slot, then its
    // CMAC; then read back and check the 3DS now picks that slot with `save` in it.
    bool write(u64 tid, const std::vector<u8>& next, u32 newHeaderOfs, const u8* save, u32 size, std::string& why);

    // Put a whole container file back (a backup made by WalkerBox, same size), then
    // read it back: it must be identical and have a valid current slot.
    bool restore(u64 tid, const std::vector<u8>& file, std::string& why);

    // Open the PxiFS0 session (Luma3DS svcControlService). Safe to call again.
    bool init(std::string& why);
    void exit();
    // Read a title's whole GBA VC save container and pick the current slot,
    // checking both CMACs. false = no GBA VC save (why says what failed).
    bool read(u64 tid, std::vector<u8>& file, Pick& pick, std::string& why);
#endif
}
