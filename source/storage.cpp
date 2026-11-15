// WalkerBox storage, milestone S1 (read only).
//
// Finds Gen 1/2 Virtual Console saves on the SD card and shows their boxes and
// party, using PKSM-Core (lib/PKSM-Core, GPL-3.0) to read them. Nothing is
// written to any game save in this milestone.
//
// Save access follows PKSM's method for Virtual Console titles (docs/RESEARCH.md
// section 5): the title's save archive ARCHIVE_USER_SAVEDATA {SD, low id, high id},
// file "/sav.dat". Needs Luma3DS (homebrew may open other titles' saves there).
// GBA Virtual Console titles (injects, Gen 3) are read through PxiFS0 from the
// title's "save data and content" archive (source/gbavc.cpp); moves are saved the
// way the game and the 3DS save (gameMatchFinish3, gbavc::buildNext / write).

#include "storage.h"
#include "app_paths.h"
#include "routes.h"
#include "dbg.h"
#include "gamematch.hpp"
#include "gbavc.hpp"
#include "walk_poke.hpp"

extern "C" void ui_legacy_present(void); // include/ui.h (v0.32: screen output only)

#include "enums/Language.hpp"
#include "enums/Species.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "sav/Item.hpp"
#include "utils/i18n.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <ctime>
#include <memory>
#include <span>
#include <string>
#include <sys/stat.h>
#include <vector>

#define LOG_PATH APP_DIR "/link_log.txt"

namespace
{
    struct FoundSave
    {
        u64 titleId;
        u32 length;
        std::shared_ptr<u8[]> data;
        std::unique_ptr<pksm::Sav> sav;
        std::string label;
        std::string name;          // the title's name (HOME menu)
        std::vector<u8> original;  // the save as read, for the backup before the first write
        bool backedUp = false;
        int choice    = -1;          // index into kVersions chosen by hand, or -1
        // GBA Virtual Console (inject): `original` is the whole two-slot container
        // file; the save shown is the slot below (include/gbavc.hpp).
        FS_MediaType media = MEDIATYPE_SD;  // or MEDIATYPE_GAME_CARD (Gen 6/7 cartridge)
        const char* path   = "/sav.dat";    // Game Boy VC; "/main" for Gen 6/7
        bool gba = false;
        std::vector<u8> gbaFile;       // the container as it is on the card now
        walkerbox::gbavc::Pick gbaPick; // its current slot
    };

    PrintConsole *g_top, *g_bottom;
    bool g_dataReady; // romfs mounted and PKSM-Core's English strings loaded

    void refresh()
    {
        ui_legacy_present(); // v0.32: the consoles write into RAM; shown on the GPU screens (include/ui.h)
    }

    u32 waitKeys(u32 mask)
    {
        while (aptMainLoop())
        {
            hidScanInput();
            u32 down = hidKeysDown();
            if (down & mask)
                return down;
            gspWaitForVBlank();
        }
        return KEY_B;
    }

    // romfs holds PKSM-Core's species tables and text; load them once.
    bool prepareData()
    {
        if (g_dataReady)
            return true;
        Result r = romfsInit();
        dbg_printf("storage: romfsInit -> %08lx", (unsigned long)r);
        if (R_FAILED(r))
            return false;
        try
        {
            i18n::init(pksm::Language::ENG);
        }
        catch (...)
        {
            dbg_printf("storage: loading PKSM-Core's strings failed");
            return false;
        }
        g_dataReady = true;
        return true;
    }

    // Size range of the save file a Pokemon game keeps in its save archive:
    // Game Boy VC "/sav.dat" 32 KiB (+ a little), 3DS games "/main" (X/Y 0x65600,
    // OR/AS 0x76000, S/M 0x6BE00, US/UM 0x6CC00).
    bool plausibleSize(const char* path, u64 size)
    {
        if (!strcmp(path, "/sav.dat"))
            return size >= 0x8000 && size <= 0x10000;
        return size >= 0x60000 && size <= 0x80000;
    }

    // Read one file of a title's save archive (ARCHIVE_USER_SAVEDATA, as PKSM:
    // SD titles and game cards alike). Returns false if the title has no such
    // file or it is not the size of a Pokemon save of that kind.
    bool readUserSave(u64 tid, FS_MediaType media, const char* path, std::shared_ptr<u8[]>& out, u32& length,
        Result& why)
    {
        u32 lowPath[3] = {(u32)media, (u32)tid, (u32)(tid >> 32)};
        FS_Path archPath = {PATH_BINARY, sizeof(lowPath), lowPath};
        FS_Archive arch;
        why = FSUSER_OpenArchive(&arch, ARCHIVE_USER_SAVEDATA, archPath);
        if (R_FAILED(why))
            return false;
        bool ok = false;
        Handle file;
        why = FSUSER_OpenFile(&file, arch, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
        if (R_SUCCEEDED(why))
        {
            u64 size = 0;
            FSFILE_GetSize(file, &size);
            if (plausibleSize(path, size))
            {
                out = std::shared_ptr<u8[]>(new u8[size]);
                u32 got = 0;
                why = FSFILE_Read(file, &got, 0, out.get(), (u32)size);
                if (R_SUCCEEDED(why) && got == size)
                {
                    length = (u32)size;
                    ok     = true;
                }
            }
            else if (!strcmp(path, "/sav.dat"))
            {
                dbg_printf("storage: %016llx has /sav.dat of %llu bytes - not a Game Boy save",
                    (unsigned long long)tid, (unsigned long long)size);
            }
            FSFILE_Close(file);
        }
        FSUSER_CloseArchive(arch);
        return ok;
    }

    // The title's name as the HOME menu shows it: the English short title from
    // the icon data (SMDH) in the title's ExeFS. Empty if it cannot be read.
    std::string titleName(u64 tid, FS_MediaType media)
    {
        u32 archPath[4]        = {(u32)tid, (u32)(tid >> 32), (u32)media, 0};
        static const u32 icon[5] = {0, 0, 2, 0x6E6F6369 /* "icon" */, 0}; // ExeFS file "icon"
        FS_Path aPath          = {PATH_BINARY, sizeof(archPath), archPath};
        FS_Path fPath          = {PATH_BINARY, sizeof(icon), icon};
        Handle h;
        Result ro = FSUSER_OpenFileDirectly(&h, ARCHIVE_SAVEDATA_AND_CONTENT, aPath, fPath, FS_OPEN_READ, 0);
        if (R_FAILED(ro))
        {
            dbg_printf("storage: %016llx: title name: opening the icon failed %08lx", (unsigned long long)tid,
                (unsigned long)ro);
            return "";
        }
        // SMDH: "SMDH", u16 version, u16 reserved, then 16 title blocks of 0x200 bytes
        // (u16 short title[0x40], long title[0x80], publisher[0x40]); index 1 = English.
        // Test S2b: reading only the first 0x410 bytes failed with e0c046f9 - an
        // ExeFS file has to be read whole, so read the full SMDH (0x36C0 bytes).
        std::vector<u16> smdh(0x36C0 / 2);
        u32 got = 0;
        Result r = FSFILE_Read(h, &got, 0, smdh.data(), 0x36C0);
        FSFILE_Close(h);
        if (R_FAILED(r) || got < 0x36C0 || memcmp(smdh.data(), "SMDH", 4) != 0)
        {
            dbg_printf("storage: %016llx: title name: reading the icon failed %08lx (%lu bytes)",
                (unsigned long long)tid, (unsigned long)r, (unsigned long)got);
            return "";
        }
        std::string name;
        const u16* shortTitle = smdh.data() + (0x8 + 0x200) / 2;
        for (int i = 0; i < 0x40 && shortTitle[i]; i++)
        {
            u16 c = shortTitle[i];
            name += (c >= 0x20 && c < 0x7F) ? (char)c : (c == 0xE9 ? 'e' : '?');
        }
        return name;
    }

    bool contains(const std::string& s, const char* word)
    {
        return s.find(word) != std::string::npos;
    }

    // Red/Blue (and Gold/Silver) saves are identical inside, so PKSM-Core cannot
    // tell them apart from the save (test S1: Blue showed as Red). Take the
    // version from the installed title's name instead.
    void fixVersion(pksm::Sav& sav, const std::string& name)
    {
        using pksm::GameVersion;
        bool jpn = sav.language() == pksm::Language::JPN;
        GameVersion v = GameVersion::INVALID;
        if (sav.generation() != pksm::Generation::ONE && sav.generation() != pksm::Generation::TWO)
            return; // Gen 3: PKSM-Core tells the games apart from the save itself
        if (sav.generation() == pksm::Generation::ONE)
        {
            if (contains(name, "Red"))
                v = GameVersion::RD;
            else if (contains(name, "Blue"))
                v = jpn ? GameVersion::BU : GameVersion::GN; // international Blue = GN in PKSM-Core
            else if (contains(name, "Green"))
                v = GameVersion::GN;
            else if (contains(name, "Yellow"))
                v = GameVersion::YW;
        }
        else
        {
            if (contains(name, "Gold"))
                v = GameVersion::GD;
            else if (contains(name, "Silver"))
                v = GameVersion::SV;
            else if (contains(name, "Crystal"))
                v = GameVersion::C;
        }
        if (v != GameVersion::INVALID && v != sav.version())
        {
            dbg_printf("storage: version set from the title name \"%s\" (save alone said %s)", name.c_str(),
                i18n::game(pksm::Language::ENG, sav.version()).c_str());
            sav.version(v);
        }
    }

    // --- version chosen by hand (SELECT in the save list) ----------------------
    //
    // The automatic title-name lookup can fail (test S2: no name was read, so Blue
    // stayed "Red"). The user can set the version per game; it is remembered in
    // sdmc:/3ds/walkerbox/versions.txt as "<title id> <code>" lines.

    struct VersionChoice
    {
        const char* code;
        pksm::GameVersion version;
        const char* label;
        int gen;
    };

    const VersionChoice kVersions[] = {
        {"RD", pksm::GameVersion::RD, "Red", 1},
        {"GN", pksm::GameVersion::GN, "Blue", 1}, // international Blue (Green in Japan)
        {"BU", pksm::GameVersion::BU, "Blue (JP)", 1},
        {"YW", pksm::GameVersion::YW, "Yellow", 1},
        {"GD", pksm::GameVersion::GD, "Gold", 2},
        {"SV", pksm::GameVersion::SV, "Silver", 2},
        {"C", pksm::GameVersion::C, "Crystal", 2},
    };
    const char* const VERSIONS_PATH = APP_DIR "/versions.txt";

    // Index into kVersions remembered for this title, or -1.
    int loadVersionChoice(u64 tid)
    {
        FILE* f = fopen(VERSIONS_PATH, "r");
        if (!f)
            return -1;
        char line[64];
        int found = -1;
        while (fgets(line, sizeof(line), f))
        {
            unsigned long long id = 0;
            char code[8] = {0};
            if (sscanf(line, "%llx %7s", &id, code) != 2 || id != tid)
                continue;
            for (int i = 0; i < (int)(sizeof(kVersions) / sizeof(kVersions[0])); i++)
                if (!strcmp(code, kVersions[i].code))
                    found = i; // the last line for a title wins
        }
        fclose(f);
        return found;
    }

    void saveVersionChoice(u64 tid, int index)
    {
        mkdir(APP_DIR, 0777);
        FILE* f = fopen(VERSIONS_PATH, "a");
        if (!f)
            return;
        fprintf(f, "%016llx %s\n", (unsigned long long)tid, kVersions[index].code);
        fclose(f);
    }

    // Set the version chosen by hand (if it fits the save's generation) and
    // rebuild the label: chosen version, else the title's name, else PKSM's guess.
    void applyVersionChoice(FoundSave& f, int index)
    {
        pksm::Sav& sav = *f.sav;
        int gen        = sav.generation() == pksm::Generation::ONE   ? 1
                         : sav.generation() == pksm::Generation::TWO ? 2
                                                                     : 3;
        if (index >= 0 && kVersions[index].gen == gen)
        {
            f.choice = index;
            sav.version(kVersions[index].version);
        }
        std::string base = f.choice >= 0 ? kVersions[f.choice].label
                           : !f.name.empty() ? f.name
                                             : i18n::game(pksm::Language::ENG, sav.version());
        f.label = base + " - " + sav.otName();
    }

    // Look through every installed SD title for a Gen 1/2 save PKSM-Core knows.
    std::vector<FoundSave> scanVcSaves()
    {
        std::vector<FoundSave> found;
        Result r = amInit();
        if (R_FAILED(r))
        {
            dbg_printf("storage: amInit -> %08lx", (unsigned long)r);
            return found;
        }
        // Installed titles, then a 3DS game card if one is in (Gen 6/7 cartridges).
        std::vector<std::pair<u64, FS_MediaType>> titles;
        for (FS_MediaType media : {MEDIATYPE_SD, MEDIATYPE_GAME_CARD})
        {
            if (media == MEDIATYPE_GAME_CARD)
            {
                FS_CardType type;
                bool inserted = false;
                if (R_FAILED(FSUSER_CardSlotIsInserted(&inserted)) || !inserted ||
                    R_FAILED(FSUSER_GetCardType(&type)) || type != CARD_CTR)
                {
                    dbg_printf("storage: no 3DS game card in the slot");
                    continue;
                }
            }
            u32 count = 0;
            AM_GetTitleCount(media, &count);
            std::vector<u64> ids(count);
            u32 read = 0;
            if (count)
                AM_GetTitleList(&read, media, count, ids.data());
            dbg_printf("storage: %lu %s title(s)", (unsigned long)read, media == MEDIATYPE_SD ? "SD" : "game card");
            for (u32 i = 0; i < read; i++)
                titles.push_back({ids[i], media});
        }
        amExit();

        for (auto [tid, media] : titles)
        {
            if ((tid >> 32) != 0x00040000) // applications only
                continue;
            std::shared_ptr<u8[]> data;
            u32 length = 0;
            Result why = 0;
            FoundSave f;
            f.media = media;
            if (media == MEDIATYPE_SD && readUserSave(tid, media, "/sav.dat", data, length, why))
            {
                f.original.assign(data.get(), data.get() + length);
            }
            else if (readUserSave(tid, media, "/main", data, length, why))
            {
                // A 3DS game's save of the size of a Gen 6/7 one.
                f.path = "/main";
                f.original.assign(data.get(), data.get() + length);
            }
            else if (media != MEDIATYPE_SD)
            {
                continue;
            }
            else
            {
                // No Game Boy save: a GBA Virtual Console title (inject)?
                std::vector<u8> file;
                walkerbox::gbavc::Pick pick;
                std::string gwhy;
                if (!walkerbox::gbavc::read(tid, file, pick, gwhy))
                {
                    if (gwhy.rfind("open ", 0) != 0) // titles without a GBA save fail at "open ..."; stay quiet
                        dbg_printf("storage: %016llx: GBA save not usable: %s", (unsigned long long)tid, gwhy.c_str());
                    continue;
                }
                dbg_printf("storage: %016llx: GBA save, %lu-byte file, using slot %d (%s), %lu-byte save",
                    (unsigned long long)tid, (unsigned long)file.size(), pick.slot, gwhy.c_str(),
                    (unsigned long)pick.saveSize);
                length = pick.saveSize;
                data   = std::shared_ptr<u8[]>(new u8[length]);
                memcpy(data.get(), file.data() + pick.saveOfs, length);
                f.gba      = true;
                f.gbaPick  = pick;
                f.original = file;
                f.gbaFile  = std::move(file);
            }
            std::unique_ptr<pksm::Sav> sav;
            try
            {
                sav = pksm::Sav::getSave(data, length);
            }
            catch (...)
            {
                sav = nullptr;
            }
            using G     = pksm::Generation;
            bool main   = !strcmp(f.path, "/main");
            bool wanted = sav && (f.gba ? sav->generation() == G::THREE
                                  : main ? (sav->generation() == G::SIX || sav->generation() == G::SEVEN)
                                         : (sav->generation() == G::ONE || sav->generation() == G::TWO));
            if (!wanted)
            {
                if (!main) // other 3DS games have a /main too; only Game Boy / GBA misses are worth a line
                    dbg_printf("storage: %016llx: %lu-byte %s is not a Gen %s Pokemon save", (unsigned long long)tid,
                        (unsigned long)length, f.gba ? "GBA save" : "/sav.dat", f.gba ? "3" : "1/2");
                continue;
            }
            std::string name = titleName(tid, media);
            fixVersion(*sav, name);
            f.titleId = tid;
            f.length  = length;
            f.data    = data;
            f.name    = name;
            f.sav     = std::move(sav);
            applyVersionChoice(f, loadVersionChoice(tid));
            int boxes = f.sav->maxBoxes();
            dbg_printf("storage: %016llx%s: %s, %lu bytes, %d boxes of %d", (unsigned long long)tid,
                media == MEDIATYPE_SD ? "" : " (game card)", f.label.c_str(), (unsigned long)length, boxes,
                boxes ? f.sav->maxSlot() / boxes : 0);
            found.push_back(std::move(f));
        }
        return found;
    }

    // "Species     L12 *" with the name cut to nameWidth (the bottom screen is
    // only 40 characters wide, so it uses shorter names).
    std::string describe(const pksm::PKX* pk, int nameWidth = 11)
    {
        if (!pk || pk->species() == pksm::Species::None)
            return "-";
        std::string name = i18n::species(pksm::Language::ENG, pk->species());
        // The console font has no gender signs: Nidoran (male / female) -> "Nidoran M" / "Nidoran F".
        for (const char* sign : {"\xE2\x99\x82", "\xE2\x99\x80"})
        {
            size_t at = name.find(sign);
            if (at != std::string::npos)
                name.replace(at, 3, sign[2] == '\x82' ? " M" : " F");
        }
        if ((int)name.size() > nameWidth)
            name.resize((size_t)nameWidth);
        char buf[40];
        snprintf(buf, sizeof(buf), "%-*s L%-3u%s", nameWidth, name.c_str(), pk->level(), pk->shiny() ? "*" : "");
        return buf;
    }

    // --- WalkerBox bank ---------------------------------------------------------
    //
    // sdmc:/3ds/walkerbox/bank.wbk: 16-byte header ("WBXBANK1", u32 boxes, u32
    // slots per box, little-endian), then boxes x slots entries of 0x200 bytes:
    // u8 generation (1-9, 0 = empty), u8 reserved, u16 length, u32 reserved, then
    // the Pokemon exactly as its game stores it (PKSM's idea: keep it native,
    // convert only when it goes somewhere else).

    constexpr int BANK_BOXES   = 30;
    constexpr int BANK_SLOTS   = 30;
    constexpr u32 BANK_HEADER  = 16;
    constexpr u32 BANK_ENTRY   = 0x200;
    constexpr u32 BANK_DATA    = BANK_ENTRY - 8;
    constexpr u32 BANK_SIZE    = BANK_HEADER + BANK_BOXES * BANK_SLOTS * BANK_ENTRY;
    const char* const BANK_PATH = APP_DIR "/bank.wbk";
    const char* const BANK_TMP  = APP_DIR "/bank.tmp";
    const char* const BANK_BAK  = APP_DIR "/bank.bak";

    std::vector<u8> g_bank;
    std::string g_bankNote; // v0.50: a message for the user (bank recovered), taken by storage_take_bank_note
    // v0.50 (audit S3): slots whose copy is also in a game (the bank save after a put-back failed): no send, no
    // put back until the app starts again
    std::vector<int> g_blockedSlots;

    // Gen 2 writes: on since the game-matching rule (source/gamematch.cpp) passed the
    // PC tests on the user's real Crystal / Silver saves (2026-11-14, tests/run_storage.sh).
    constexpr bool kGen2Moves = true;

    int genNumber(pksm::Generation g)
    {
        using G = pksm::Generation;
        if (g == G::ONE)
            return 1;
        if (g == G::TWO)
            return 2;
        if (g == G::THREE)
            return 3;
        if (g == G::FOUR)
            return 4;
        if (g == G::FIVE)
            return 5;
        if (g == G::SIX)
            return 6;
        if (g == G::SEVEN)
            return 7;
        return 0; // not stored by this build
    }

    pksm::Generation genFromNumber(int n)
    {
        using G = pksm::Generation;
        switch (n)
        {
            case 1: return G::ONE;
            case 2: return G::TWO;
            case 3: return G::THREE;
            case 4: return G::FOUR;
            case 5: return G::FIVE;
            case 6: return G::SIX;
            default: return G::SEVEN;
        }
    }

    u8* bankEntry(int box, int slot)
    {
        return g_bank.data() + BANK_HEADER + (u32)(box * BANK_SLOTS + slot) * BANK_ENTRY;
    }

    bool bankSave();

    bool bankLoad()
    {
        if (!g_bank.empty())
            return true;
        g_bank.assign(BANK_SIZE, 0);
        memcpy(g_bank.data(), "WBXBANK1", 8);
        u32 dims[2] = {BANK_BOXES, BANK_SLOTS};
        memcpy(g_bank.data() + 8, dims, 8);
        FILE* f = fopen(BANK_PATH, "rb");
        if (!f)
        {
            // v0.50 (audit S2): power lost between bankSave's two renames leaves no bank.wbk but a complete
            // bank.tmp (newest) or bank.bak (previous). Use it instead of starting empty (an empty bank saved
            // next would delete both).
            for (const char* alt : {BANK_TMP, BANK_BAK})
            {
                FILE* a = fopen(alt, "rb");
                if (!a)
                    continue;
                std::vector<u8> file(BANK_SIZE);
                size_t got = fread(file.data(), 1, BANK_SIZE, a);
                bool more  = fgetc(a) != EOF;
                fclose(a);
                if (got == BANK_SIZE && !more && memcmp(file.data(), "WBXBANK1", 8) == 0 &&
                    memcmp(file.data() + 8, dims, 8) == 0)
                {
                    g_bank = std::move(file);
                    dbg_printf("storage: bank.wbk missing - RECOVERED from %s", alt);
                    g_bankNote = std::string("Your PocketPKMN box was recovered\nfrom ") +
                                 (alt == BANK_TMP ? "bank.tmp" : "bank.bak") +
                                 " (the last save of it\nwas cut off).";
                    if (!bankSave())
                        dbg_printf("storage: saving the recovered bank failed (kept in memory)");
                    return true;
                }
                dbg_printf("storage: %s is not a complete bank - not used", alt);
            }
            dbg_printf("storage: no bank yet, starting an empty one");
            return true;
        }
        std::vector<u8> file(BANK_SIZE);
        size_t got = fread(file.data(), 1, BANK_SIZE, f);
        fclose(f);
        if (got != BANK_SIZE || memcmp(file.data(), "WBXBANK1", 8) != 0 || memcmp(file.data() + 8, dims, 8) != 0)
        {
            dbg_printf("storage: bank.wbk is not a PocketPKMN bank (%lu bytes) - not used", (unsigned long)got);
            g_bank.clear();
            return false;
        }
        g_bank = std::move(file);
        return true;
    }

    // Write via bank.tmp, keep the previous file as bank.bak.
    bool bankSave()
    {
        mkdir("sdmc:/3ds", 0777);
        mkdir(APP_DIR, 0777);
        FILE* f = fopen(BANK_TMP, "wb");
        if (!f)
            return false;
        bool ok = fwrite(g_bank.data(), 1, g_bank.size(), f) == g_bank.size();
        if (fclose(f) != 0)
            ok = false;
        if (!ok)
        {
            remove(BANK_TMP);
            return false;
        }
        remove(BANK_BAK);
        rename(BANK_PATH, BANK_BAK); // fails harmlessly the first time
        if (rename(BANK_TMP, BANK_PATH) != 0)
        {
            rename(BANK_BAK, BANK_PATH);
            return false;
        }
        return true;
    }

    int bankGen(int box, int slot)
    {
        return bankEntry(box, slot)[0];
    }

    std::unique_ptr<pksm::PKX> bankPkm(int box, int slot)
    {
        u8* e = bankEntry(box, slot);
        if (!e[0])
            return nullptr;
        u16 len = (u16)(e[2] | (e[3] << 8));
        return pksm::PKX::getPKM(genFromNumber(e[0]), e + 8, len);
    }

    void bankPut(int box, int slot, int gen, std::span<const u8> raw)
    {
        u8* e = bankEntry(box, slot);
        memset(e, 0, BANK_ENTRY);
        e[0] = (u8)gen;
        e[2] = (u8)raw.size();
        e[3] = (u8)(raw.size() >> 8);
        memcpy(e + 8, raw.data(), raw.size());
    }

    // Origin record (2026-11-15): which game and trainer a Pokemon was deposited from, so that what it
    // catches on a walk can get that trainer. At entry +0x1C0 (the Pokemon data ends far below: the
    // largest format, PK8/PK9, is 0x158 bytes; entry data starts at +8):
    //   +0x00 "WBO1", +0x04 u64 title id, +0x0C u8 generation, +0x0D u8 language, +0x0E u8 game version,
    //   +0x0F 0, +0x10 u16 TID, +0x12 u16 SID (0 for Gen 1/2), +0x14 trainer name UTF-8, 0-padded (24 bytes).
    // Entries from before have zeros there (no record).
    constexpr u32 BANK_ORIGIN = 0x1C0;

    void bankSetOrigin(int box, int slot, const FoundSave& f, int gen)
    {
        u8* o = bankEntry(box, slot) + BANK_ORIGIN;
        memset(o, 0, BANK_ENTRY - BANK_ORIGIN);
        memcpy(o, "WBO1", 4);
        for (int i = 0; i < 8; i++)
            o[4 + i] = (u8)(f.titleId >> (8 * i));
        o[0x0C]          = (u8)gen;
        o[0x0D]          = (u8)f.sav->language();
        o[0x0E]          = (u8)f.sav->version();
        o[0x0F]          = (u8)(f.media + 1); // v0.50 (audit S7): FS media + 1 (0 = not recorded, older entries)
        u16 tid          = f.sav->TID();
        u16 sid          = gen <= 2 ? 0 : f.sav->SID();
        o[0x10]          = (u8)tid;
        o[0x11]          = (u8)(tid >> 8);
        o[0x12]          = (u8)sid;
        o[0x13]          = (u8)(sid >> 8);
        std::string name = f.sav->otName();
        memcpy(o + 0x14, name.data(), std::min<size_t>(name.size(), 23));
    }

    void bankClear(int box, int slot)
    {
        memset(bankEntry(box, slot), 0, BANK_ENTRY);
    }

    // v0.50 (audit S1/S3): a slot whose Pokemon is also in a game (a save failed half way) is blocked for this run
    void blockSlot(int box, int slot)
    {
        g_blockedSlots.push_back(box * BANK_SLOTS + slot);
        dbg_printf("storage: bank %d/%d BLOCKED for this run (copy also in a game)", box + 1, slot + 1);
    }

    bool slotBlocked(int box, int slot)
    {
        for (int k : g_blockedSlots)
            if (k == box * BANK_SLOTS + slot)
                return true;
        return false;
    }

    // --- writing a Virtual Console save ---------------------------------------

    std::string timestamp()
    {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
        return buf;
    }

    // The untouched save, once per session, before the first write.
    bool backupGameSave(FoundSave& f)
    {
        if (f.backedUp)
            return true;
        mkdir(APP_DIR "/game_backups", 0777);
        char path[160];
        // GBA: the whole container file as the 3DS keeps it (both slots, headers).
        snprintf(path, sizeof(path), APP_DIR "/game_backups/%016llx-%s.%s",
            (unsigned long long)f.titleId, timestamp().c_str(), f.gba ? "gbavc" : !strcmp(f.path, "/main") ? "main" : "sav");
        FILE* out = fopen(path, "wb");
        if (!out)
            return false;
        bool ok = fwrite(f.original.data(), 1, f.original.size(), out) == f.original.size();
        if (fclose(out) != 0)
            ok = false;
        dbg_printf("storage: game save backup %s: %s", path, ok ? "ok" : "FAILED");
        f.backedUp = ok;
        return ok;
    }

    // Write the save file, commit, read it back and compare, then (SD titles)
    // drop the title's secure value as PKSM does, so the game accepts the save.
    bool writeUserSave(u64 tid, FS_MediaType media, const char* path, const u8* data, u32 length)
    {
        u32 lowPath[3]   = {(u32)media, (u32)tid, (u32)(tid >> 32)};
        FS_Path archPath = {PATH_BINARY, sizeof(lowPath), lowPath};
        FS_Archive arch;
        Result r = FSUSER_OpenArchive(&arch, ARCHIVE_USER_SAVEDATA, archPath);
        if (R_FAILED(r))
        {
            dbg_printf("storage: write: open archive %08lx", (unsigned long)r);
            return false;
        }
        bool ok = false;
        Handle file;
        r = FSUSER_OpenFile(&file, arch, fsMakePath(PATH_ASCII, path), FS_OPEN_WRITE, 0);
        if (R_SUCCEEDED(r))
        {
            u32 written = 0;
            r = FSFILE_Write(file, &written, 0, data, length, FS_WRITE_FLUSH);
            FSFILE_Close(file);
            ok = R_SUCCEEDED(r) && written == length;
            if (!ok)
                dbg_printf("storage: write: %08lx, %lu of %lu bytes", (unsigned long)r, (unsigned long)written,
                    (unsigned long)length);
        }
        else
        {
            dbg_printf("storage: write: open file %08lx", (unsigned long)r);
        }
        if (ok)
        {
            r = FSUSER_ControlArchive(arch, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr, 0, nullptr, 0);
            ok = R_SUCCEEDED(r);
            dbg_printf("storage: write: commit %08lx", (unsigned long)r);
        }
        FSUSER_CloseArchive(arch);
        if (!ok)
            return false;

        std::shared_ptr<u8[]> back;
        u32 backLen = 0;
        Result why  = 0;
        if (!readUserSave(tid, media, path, back, backLen, why) || backLen != length ||
            memcmp(back.get(), data, length) != 0)
        {
            dbg_printf("storage: write: read-back did NOT match");
            return false;
        }
        if (media != MEDIATYPE_SD)
            return true;

        u64 in = ((u64)SECUREVALUE_SLOT_SD << 32) | (((tid >> 8) & 0xFFFFF) << 8);
        u8 existed = 0;
        r = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &in, sizeof(in), &existed, 1);
        dbg_printf("storage: write: secure value delete %08lx (existed %u)", (unsigned long)r, existed);
        return true;
    }

    // --- the safety check before every write -----------------------------------
    //
    // User requirement (2026-11-14): a save must not change in any way except the
    // one Pokemon going in or out. Before writing, the edited save is compared
    // with a snapshot taken before the edit: trainer data, the whole party and
    // every other box must be byte-identical, and the edited box must hold the
    // same Pokemon in the same order apart from the moved one, which must be a
    // byte-for-byte copy. If anything else differs, nothing is written.

    using Raw = std::vector<u8>;

    Raw rawOf(const pksm::PKX* pk)
    {
        if (!pk || pk->species() == pksm::Species::None)
            return {};
        auto r = pk->rawData();
        return Raw(r.begin(), r.end());
    }

    struct Snapshot
    {
        std::vector<std::vector<Raw>> boxes; // per box: the Pokemon in slot order, empty slots left out
        std::vector<Raw> party;
        std::string ot;
        u16 tid = 0, sid = 0, hours = 0;
        u32 money = 0;
        u8 badges = 0, minutes = 0;
    };

    Snapshot takeSnapshot(const pksm::Sav& sav)
    {
        Snapshot s;
        int boxes  = sav.maxBoxes();
        int perBox = boxes ? sav.maxSlot() / boxes : 0;
        s.boxes.resize((size_t)boxes);
        for (int b = 0; b < boxes; b++)
            for (int i = 0; i < perBox; i++)
            {
                Raw r = rawOf(sav.pkm((u8)b, (u8)i).get());
                if (!r.empty())
                    s.boxes[(size_t)b].push_back(std::move(r));
            }
        for (int i = 0; i < sav.partyCount() && i < 6; i++)
            s.party.push_back(rawOf(sav.pkm((u8)i).get()));
        s.ot      = sav.otName();
        s.tid     = sav.TID();
        s.sid     = sav.SID();
        s.money   = sav.money();
        s.badges  = sav.badges();
        s.hours   = sav.playedHours();
        s.minutes = sav.playedMinutes();
        return s;
    }

    // deposit: `moved` must leave box `box`; withdraw: it must arrive there.
    bool onlyThisChanged(const Snapshot& before, const Snapshot& after, int box, bool deposit, const Raw& moved,
        std::string& why)
    {
        if (before.ot != after.ot || before.tid != after.tid || before.sid != after.sid ||
            before.money != after.money || before.badges != after.badges || before.hours != after.hours ||
            before.minutes != after.minutes)
        {
            why = "trainer data changed";
            return false;
        }
        if (before.party != after.party)
        {
            why = "the party changed";
            return false;
        }
        if (before.boxes.size() != after.boxes.size())
        {
            why = "the number of boxes changed";
            return false;
        }
        for (size_t b = 0; b < before.boxes.size(); b++)
            if ((int)b != box && before.boxes[b] != after.boxes[b])
            {
                why = "box " + std::to_string(b + 1) + " changed";
                return false;
            }
        // The edited box: take the moved Pokemon out of the side that has it, and
        // the rest must be identical, in the same order.
        std::vector<Raw> more = deposit ? before.boxes[(size_t)box] : after.boxes[(size_t)box];
        const std::vector<Raw>& less = deposit ? after.boxes[(size_t)box] : before.boxes[(size_t)box];
        auto it = std::find(more.begin(), more.end(), moved);
        if (it == more.end())
        {
            why = deposit ? "the Pokemon was not found in the box before"
                          : "the Pokemon in the game is not an exact copy of the stored one";
            return false;
        }
        more.erase(it);
        if (more != less)
        {
            why = "other Pokemon in box " + std::to_string(box + 1) + " changed";
            return false;
        }
        return true;
    }

    // Read the bank entry back from the card and compare it with what was meant
    // to be stored there.
    bool bankEntryOnCard(int box, int slot, const Raw& expect)
    {
        FILE* f = fopen(BANK_PATH, "rb");
        if (!f)
            return false;
        std::vector<u8> e(BANK_ENTRY);
        long at = (long)(BANK_HEADER + (u32)(box * BANK_SLOTS + slot) * BANK_ENTRY);
        bool ok = fseek(f, at, SEEK_SET) == 0 && fread(e.data(), 1, BANK_ENTRY, f) == BANK_ENTRY;
        fclose(f);
        if (!ok)
            return false;
        if (expect.empty())
            return e[0] == 0;
        u16 len = (u16)(e[2] | (e[3] << 8));
        return len == expect.size() && memcmp(e.data() + 8, expect.data(), len) == 0;
    }

    // v0.50 (audit S3): save the bank and read the entry back, up to 3 tries
    bool bankSaveChecked(int box, int slot, const Raw& expect)
    {
        for (int i = 0; i < 3; i++)
        {
            if (bankSave() && bankEntryOnCard(box, slot, expect))
                return true;
            dbg_printf("storage: bank save / read-back try %d failed", i + 1);
        }
        return false;
    }

    // Finish the save the way the game would (see source/gamematch.cpp: only
    // the edited box - and for Gen 1 the checksums that cover it - may change;
    // everything PKSM-Core's finishEditing touches beyond that is put back),
    // write it, and carry on editing.
    // Gen 3 (GBA inject): the edit is saved the way the game saves (a new copy in
    // the other Gen 3 slot, gamematch.cpp), then written the way the 3DS writes a
    // GBA save (the other container slot, gbavc.cpp); afterwards the save is read
    // again from the card. `before` is the save as it was before the edit.
    void reloadSave(FoundSave& f);

    // v0.50 (audit S1): *refused = true when it gave up before writing anything (the game save is unchanged).
    bool commitGame(FoundSave& f, int editedBox, const std::vector<u8>& before, bool* refused = nullptr)
    {
        pksm::Sav& sav = *f.sav;
        std::string note;
        if (refused)
            *refused = false;
        if (f.gba)
        {
            if (before.size() != f.length || !walkerbox::gameMatchFinish3(sav, before.data(), f.length, note))
            {
                dbg_printf("storage: Gen 3 save REFUSED: %s", note.c_str());
                if (refused)
                    *refused = true;
                return false;
            }
            dbg_printf("storage: %s", note.c_str());
            std::vector<u8> next;
            u32 headerOfs = 0;
            if (!walkerbox::gbavc::buildNext(f.gbaFile, f.gbaPick, sav.rawData().get(), f.length, next, headerOfs))
            {
                dbg_printf("storage: GBA write: the save file has no room for a second slot");
                if (refused)
                    *refused = true;
                return false;
            }
            std::string why;
            bool ok = walkerbox::gbavc::write(f.titleId, next, headerOfs, sav.rawData().get(), f.length, why);
            dbg_printf("storage: GBA write: %s", why.c_str());
            // The save object still points at the old copy; take what is on the card now.
            reloadSave(f);
            return ok;
        }
        if (sav.generation() == pksm::Generation::SIX)
        {
            if (before.size() != f.length ||
                !walkerbox::gameMatchFinish6(sav, before.data(), f.length, editedBox, note))
            {
                dbg_printf("storage: Gen 6 save REFUSED: %s", note.c_str());
                if (refused)
                    *refused = true;
                return false;
            }
            dbg_printf("storage: %s", note.c_str());
            return writeUserSave(f.titleId, f.media, f.path, sav.rawData().get(), f.length);
        }
        u32 restored = walkerbox::gameMatchFinish(sav, f.length, editedBox, note);
        dbg_printf("storage: %s (%lu byte(s) put back)", note.c_str(), (unsigned long)restored);
        if (sav.rawData().get() != f.data.get())
            dbg_printf("storage: note: the save object uses its own buffer");
        bool ok = writeUserSave(f.titleId, f.media, f.path, sav.rawData().get(), f.length);
        sav.beginEditing();
        return ok;
    }

    // After a failed write (and after every Gen 3 write): forget the edits and read
    // the save again from the card, so what is shown is what is really there.
    void reloadSave(FoundSave& f)
    {
        std::shared_ptr<u8[]> data;
        u32 length = 0;
        Result why = 0;
        std::unique_ptr<pksm::Sav> sav;
        bool got = false;
        if (f.gba)
        {
            std::vector<u8> file;
            walkerbox::gbavc::Pick pick;
            std::string gwhy;
            if (walkerbox::gbavc::read(f.titleId, file, pick, gwhy))
            {
                length = pick.saveSize;
                data   = std::shared_ptr<u8[]>(new u8[length]);
                memcpy(data.get(), file.data() + pick.saveOfs, length);
                f.gbaFile = std::move(file);
                f.gbaPick = pick;
                got       = true;
            }
            else
                dbg_printf("storage: GBA re-read failed: %s", gwhy.c_str());
        }
        else
            got = readUserSave(f.titleId, f.media, f.path, data, length, why);
        if (got)
        {
            try
            {
                sav = pksm::Sav::getSave(data, length);
            }
            catch (...)
            {
                sav = nullptr;
            }
        }
        if (!sav)
        {
            dbg_printf("storage: reload from the card FAILED");
            return;
        }
        fixVersion(*sav, f.name);
        f.data   = data;
        f.length = length;
        f.sav    = std::move(sav);
        applyVersionChoice(f, f.choice); // keep the version chosen by hand
        dbg_printf("storage: save reloaded from the card");
    }

    bool askOnBottom(const std::string& question);

    // v0.50 (audit S4): what restoring `data` (a backup of f) would do to Pokemon that moved since: those in the game
    // now but not in the backup are lost (e.g. one put back from PocketPKMN), those in the backup that left the game
    // and are now in PocketPKMN's boxes come back as a second copy. Fills both lists (names); false = backup unreadable.
    bool restoreImpact(FoundSave& f, const std::vector<u8>& data, std::vector<std::string>& lost,
        std::vector<std::string>& doubled)
    {
        u32 ofs = 0, len = (u32)data.size();
        if (f.gba)
        {
            u32 h1 = 0, h2 = 0;
            bool has1 = false, has2 = false;
            std::string why;
            if (!walkerbox::gbavc::findHeaders(data, h1, h2, has1, has2, why))
                return false;
            walkerbox::gbavc::Pick p = walkerbox::gbavc::choose(data, h1, h2, has1, has2, has1, has2);
            if (!p.slot || (u64)p.saveOfs + p.saveSize > data.size())
                return false;
            ofs = p.saveOfs;
            len = p.saveSize;
        }
        std::shared_ptr<u8[]> buf(new u8[len]);
        memcpy(buf.get(), data.data() + ofs, len);
        std::unique_ptr<pksm::Sav> old;
        try
        {
            old = pksm::Sav::getSave(buf, len);
        }
        catch (...)
        {
            old = nullptr;
        }
        if (!old || old->generation() != f.sav->generation())
            return false;
        auto all = [](pksm::Sav& s, std::vector<std::unique_ptr<pksm::PKX>>& out) {
            int boxes = s.maxBoxes(), per = boxes ? s.maxSlot() / boxes : 0;
            for (int b = 0; b < boxes; b++)
                for (int i = 0; i < per; i++)
                    out.push_back(s.pkm((u8)b, (u8)i));
            for (int i = 0; i < s.partyCount() && i < 6; i++)
                out.push_back(s.pkm((u8)i));
        };
        std::vector<std::unique_ptr<pksm::PKX>> now, then;
        all(*f.sav, now);
        all(*old, then);
        std::vector<Raw> nowRaw, thenRaw;
        for (auto& p : now)
            nowRaw.push_back(rawOf(p.get()));
        for (auto& p : then)
            thenRaw.push_back(rawOf(p.get()));
        auto has = [](const std::vector<Raw>& v, const Raw& r) {
            for (const Raw& x : v)
                if (x == r)
                    return true;
            return false;
        };
        for (size_t i = 0; i < now.size(); i++)
            if (!nowRaw[i].empty() && !has(thenRaw, nowRaw[i]))
                lost.push_back(describe(now[i].get()));
        for (size_t i = 0; i < then.size(); i++)
        {
            if (thenRaw[i].empty() || has(nowRaw, thenRaw[i]))
                continue;
            bool inBank = false;
            for (int b = 0; b < BANK_BOXES && !inBank; b++)
                for (int s = 0; s < BANK_SLOTS && !inBank; s++)
                {
                    const u8* e = bankEntry(b, s);
                    u16 l       = (u16)(e[2] | (e[3] << 8));
                    inBank = e[0] && l == thenRaw[i].size() && memcmp(e + 8, thenRaw[i].data(), l) == 0;
                }
            if (inBank)
                doubled.push_back(describe(then[i].get()));
        }
        return true;
    }

    std::string nameList(const std::vector<std::string>& v)
    {
        std::string s;
        for (size_t i = 0; i < v.size() && i < 4; i++)
            s += "  " + v[i] + "\n";
        if (v.size() > 4)
            s += "  +" + std::to_string(v.size() - 4) + " more\n";
        return s;
    }

    // Put the newest backup of this title back (X in the save list).
    void restoreBackup(FoundSave& f)
    {
        char prefix[24];
        snprintf(prefix, sizeof(prefix), "%016llx-", (unsigned long long)f.titleId);
        std::string newest;
        DIR* d = opendir(APP_DIR "/game_backups");
        if (d)
        {
            while (struct dirent* e = readdir(d))
                if (!strncmp(e->d_name, prefix, strlen(prefix)) && std::string(e->d_name) > newest)
                    newest = e->d_name; // names end in YYYYMMDD-HHMMSS, so the largest is the newest
            closedir(d);
        }
        consoleSelect(g_bottom);
        consoleClear();
        if (f.sav->generation() == pksm::Generation::SEVEN)
        {
            // Comes with Gen 7 moves; WalkerBox has not written to these saves.
            printf("Restoring is not available for\nGen 7 saves yet. (PocketPKMN has\nnot written to this save.)\n\nA/B: back\n");
            refresh();
            waitKeys(KEY_A | KEY_B);
            return;
        }
        if (newest.empty())
        {
            printf("No backup of this game yet.\n(One is made before the first\nmove.)\n\nA/B: back\n");
            refresh();
            waitKeys(KEY_A | KEY_B);
            return;
        }
        std::string path = APP_DIR "/game_backups/" + newest;
        std::vector<u8> data;
        // GBA backups are the whole container file (both slots).
        size_t want = f.gba ? f.gbaFile.size() : f.length;
        FILE* in = fopen(path.c_str(), "rb");
        if (in)
        {
            data.resize(want);
            if (fread(data.data(), 1, want, in) != want || fgetc(in) != EOF)
                data.clear();
            fclose(in);
        }
        if (data.empty())
        {
            printf("The backup\n%s\ncannot be used (wrong size).\n\nA/B: back\n", newest.c_str());
            refresh();
            waitKeys(KEY_A | KEY_B);
            return;
        }
        printf("RESTORE GAME SAVE\n\n%s\n\nfrom backup\n%s\n\n", f.label.c_str(), newest.c_str());
        printf("Everything moved since then is\nundone in the game (PocketPKMN's\nboxes are not changed).\n");
        std::vector<std::string> lost, doubled;
        if (!restoreImpact(f, data, lost, doubled))
            dbg_printf("storage: restore check: the backup could not be read as a save (no list)");
        if (!lost.empty() || !doubled.empty())
        {
            // v0.50 (audit S4): say exactly which Pokemon this loses or doubles; L+R needed then
            consoleSelect(g_top);
            consoleClear();
            printf("PLEASE CHECK\n\n");
            if (!lost.empty())
                printf("Not in the backup, so LOST\nfrom the game:\n%s\n", nameList(lost).c_str());
            if (!doubled.empty())
                printf("Back in the game AND still in\nPocketPKMN (a second copy):\n%s\n", nameList(doubled).c_str());
            dbg_printf("storage: restore check: %u lost, %u doubled", (unsigned)lost.size(), (unsigned)doubled.size());
            consoleSelect(g_bottom);
            printf("\nRestore anyway?\nHold L+R and press A = yes, B = no\n");
            refresh();
            for (;;)
            {
                u32 k = waitKeys(KEY_A | KEY_B);
                if (k & KEY_B || !k)
                    return;
                if ((hidKeysHeld() & KEY_L) && (hidKeysHeld() & KEY_R))
                    break;
            }
        }
        else if (!askOnBottom("Restore it?"))
            return;
        bool ok;
        if (f.gba)
        {
            std::string why;
            ok = walkerbox::gbavc::restore(f.titleId, data, why);
            dbg_printf("storage: GBA %s", why.c_str());
        }
        else
            ok = writeUserSave(f.titleId, f.media, f.path, data.data(), f.length);
        dbg_printf("storage: restore %s from %s: %s", f.label.c_str(), newest.c_str(), ok ? "ok" : "FAILED");
        reloadSave(f);
        dbg_flush(LOG_PATH);
        consoleSelect(g_bottom);
        printf(ok ? "\nRestored.\n\nA/B: back\n" : "\nRestore FAILED (see the log).\n\nA/B: back\n");
        refresh();
        waitKeys(KEY_A | KEY_B);
    }

    // --- the move screen -------------------------------------------------------

    std::string slotText(const pksm::PKX* pk, int index, bool cursor, int nameWidth = 11, bool walking = false)
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "%c%2d%c%s", cursor ? '>' : ' ', index + 1, walking ? '~' : ' ',
            describe(pk, nameWidth).c_str());
        return buf;
    }

    bool askOnBottom(const std::string& question)
    {
        consoleSelect(g_bottom);
        printf("\n%s\nA = yes, B = no\n", question.c_str());
        refresh();
        return (waitKeys(KEY_A | KEY_B) & KEY_A) != 0;
    }

    // Game box on the top screen, WalkerBox box on the bottom. X switches between
    // them, the D-pad moves, L/R change the box on the active side, A moves the
    // Pokemon under the cursor to the first free slot on the other side, Y shows
    // the party (read only), B goes back.
    // Keep one untouched copy of every save that is opened, even if nothing is
    // moved: a safety copy, and real data to check new save rules against on the PC.
    bool hasAnyBackup(u64 tid)
    {
        char prefix[24];
        snprintf(prefix, sizeof(prefix), "%016llx-", (unsigned long long)tid);
        bool found = false;
        DIR* d     = opendir(APP_DIR "/game_backups");
        if (d)
        {
            while (struct dirent* e = readdir(d))
                if (!strncmp(e->d_name, prefix, strlen(prefix)))
                    found = true;
            closedir(d);
        }
        return found;
    }

    // Deposit: bank first (with it), then the game (without it). A failure in between leaves a copy in both
    // places, never none. bslot = -1: the first free slot of bank box bbox. Returns true when it moved.
    // (Code moved unchanged out of editSave; `continue` became `return false`.)
    bool depositOne(FoundSave& f, int gbox, int cur, int bbox, int bslot, std::string& msg, int* placed = nullptr)
    {
        pksm::Sav& sav = *f.sav;
        int gameGen    = genNumber(sav.generation());
        auto pk = sav.pkm((u8)gbox, (u8)cur);
        if (!pk || pk->species() == pksm::Species::None)
        {
            msg = "That slot is empty.";
            return false;
        }
        int free = -1;
        if (bslot >= 0)
            free = bankGen(bbox, bslot) ? -1 : bslot;
        else
            for (int s = 0; s < BANK_SLOTS && free < 0; s++)
                if (!bankGen(bbox, s))
                    free = s;
        if (free < 0)
        {
            msg = bslot >= 0 ? "The walker slot is not free." : "This PocketPKMN box is full.";
            return false;
        }
        auto raw = pk->rawData();
        if (!gameGen || raw.size() > BANK_DATA)
        {
            msg = "This Pokemon cannot be stored yet.";
            return false;
        }
        std::string name = describe(pk.get());
        Raw moved(raw.begin(), raw.end());

        // 1. Edit the save in memory and check that nothing but this Pokemon changed.
        std::vector<u8> beforeRaw(sav.rawData().get(), sav.rawData().get() + f.length);
        Snapshot before = takeSnapshot(sav);
        sav.pkm(*sav.emptyPkm(), (u8)gbox, (u8)cur, false);
        std::string why;
        if (!onlyThisChanged(before, takeSnapshot(*f.sav), gbox, true, moved, why))
        {
            reloadSave(f);
            msg = "Safety check failed (" + why + ").\nNothing was written.";
            dbg_printf("storage: deposit of %s REFUSED by the safety check: %s", name.c_str(), why.c_str());
            return false;
        }
        // 2. Store it in WalkerBox and confirm what reached the card.
        bankPut(bbox, free, gameGen, moved);
        bankSetOrigin(bbox, free, f, gameGen);
        if (!bankSave() || !bankEntryOnCard(bbox, free, moved))
        {
            bankClear(bbox, free);
            bankSave();
            reloadSave(f);
            msg = "Could not save the PocketPKMN box.\nNothing was moved.";
            dbg_printf("storage: deposit of %s: bank save / read-back failed", name.c_str());
            return false;
        }
        if (placed)
            *placed = free;
        // 3. Write the game without it.
        bool refused = false;
        if (!commitGame(f, gbox, beforeRaw, &refused))
        {
            reloadSave(f);
            if (refused) // v0.50 (audit S1): nothing reached the game, so take the PocketPKMN copy out again
            {
                std::vector<u8> keep(bankEntry(bbox, free), bankEntry(bbox, free) + BANK_ENTRY);
                bankClear(bbox, free);
                if (bankSave() && bankEntryOnCard(bbox, free, {}))
                {
                    msg = "The game save could not be changed\n(its safety check refused).\nNothing was moved.";
                    dbg_printf("storage: deposit of %s: game write refused, bank copy taken out again", name.c_str());
                    return false;
                }
                memcpy(bankEntry(bbox, free), keep.data(), BANK_ENTRY); // as on the card
                blockSlot(bbox, free);
            }
            msg = "Writing the game save FAILED.\nThe Pokemon is now in both places.";
            dbg_printf("storage: deposit of %s: game write failed (copy kept in bank %d/%d)", name.c_str(),
                bbox + 1, free + 1);
            return false;
        }
        msg = "Moved " + name + " to PocketPKMN.";
        dbg_printf("storage: deposit %s from %s box %d slot %d -> bank %d/%d", name.c_str(),
            f.label.c_str(), gbox + 1, cur + 1, bbox + 1, free + 1);
        return true;
    }

    // Withdraw: game first (with it), then the bank (without it). Into the first free slot of game box gbox, or
    // with anyBox of any box from gbox on. Returns true when it moved; *outBox/*outSlot = where it went.
    // (Code moved unchanged out of editSave; `continue` became `return false`; anyBox is new.)
    bool withdrawOne(FoundSave& f, int bbox, int cur, int gbox, bool anyBox, std::string& msg, int* outBox,
        int* outSlot)
    {
        pksm::Sav& sav = *f.sav;
        int gameGen    = genNumber(sav.generation());
        int boxes      = sav.maxBoxes();
        int perBox     = boxes ? sav.maxSlot() / boxes : 0;
        int gen = bankGen(bbox, cur);
        if (!gen)
        {
            msg = "That slot is empty.";
            return false;
        }
        if (bankEntry(bbox, cur)[1] & 0x01) // walking mark (see storage_walk_mark)
        {
            msg = "This Pokemon is on a walk.\nEnd the walk first (End walk).";
            return false;
        }
        if (slotBlocked(bbox, cur)) // v0.50 (audit S3)
        {
            msg = "This PocketPKMN copy is also in a game\n(a save failed). Restart PocketPKMN\nbefore moving it.";
            return false;
        }
        if (gen != gameGen)
        {
            msg = "A Gen " + std::to_string(gen) + " Pokemon cannot go into\na Gen " +
                  std::to_string(gameGen) + " game yet.";
            return false;
        }
        int free = -1;
        for (int b = 0; b < (anyBox ? boxes : 1) && free < 0; b++)
        {
            int tryBox = (gbox + b) % (boxes ? boxes : 1);
            for (int s = 0; s < perBox && free < 0; s++)
            {
                auto g = sav.pkm((u8)tryBox, (u8)s);
                if (!g || g->species() == pksm::Species::None)
                {
                    free = s;
                    gbox = tryBox;
                }
            }
        }
        if (free < 0)
        {
            msg = anyBox ? "Every box of this game is full." : "This game box is full.";
            return false;
        }
        auto pk = bankPkm(bbox, cur);
        if (!pk)
        {
            msg = "This PocketPKMN slot cannot be read.";
            return false;
        }
        std::string name = describe(pk.get());
        Raw moved = rawOf(pk.get());

        // v0.50 (audit S3): the very same Pokemon (byte for byte) already in this game = an earlier put-back reached
        // the game but the bank kept its copy. Drop the bank copy instead of making a second one.
        for (int b = 0; b < boxes; b++)
            for (int s = 0; s < perBox; s++)
                if (rawOf(sav.pkm((u8)b, (u8)s).get()) == moved)
                {
                    bankClear(bbox, cur);
                    if (!bankSaveChecked(bbox, cur, {}))
                    {
                        msg = "It is already in the game, but the\nPocketPKMN box could not be saved.";
                        return false;
                    }
                    msg = name + " was already back in the game\n(box " + std::to_string(b + 1) + ", slot " +
                          std::to_string(s + 1) + "). The extra copy\nin PocketPKMN was removed.";
                    dbg_printf("storage: withdraw of %s: already in %s box %d slot %d - bank copy removed",
                        name.c_str(), f.label.c_str(), b + 1, s + 1);
                    if (outBox)
                        *outBox = b;
                    if (outSlot)
                        *outSlot = s;
                    return true;
                }

        // 1. Edit the save in memory and check that nothing but this Pokemon
        //    changed and that it arrives as an exact copy.
        std::vector<u8> beforeRaw(sav.rawData().get(), sav.rawData().get() + f.length);
        Snapshot before = takeSnapshot(sav);
        sav.pkm(*pk, (u8)gbox, (u8)free, false);
        std::string why;
        if (!onlyThisChanged(before, takeSnapshot(*f.sav), gbox, false, moved, why))
        {
            reloadSave(f);
            msg = "Safety check failed (" + why + ").\nNothing was written.";
            dbg_printf("storage: withdraw of %s REFUSED by the safety check: %s", name.c_str(),
                why.c_str());
            return false;
        }
        // 2. Write the game with it.
        if (!commitGame(f, gbox, beforeRaw))
        {
            reloadSave(f);
            msg = "Writing the game save FAILED.\nThe Pokemon stays in PocketPKMN.";
            dbg_printf("storage: withdraw of %s: game write failed", name.c_str());
            return false;
        }
        if (outBox)
            *outBox = gbox;
        if (outSlot)
            *outSlot = free;
        // 3. Then let WalkerBox drop it.
        std::vector<u8> keep(bankEntry(bbox, cur), bankEntry(bbox, cur) + BANK_ENTRY);
        bankClear(bbox, cur);
        if (!bankSaveChecked(bbox, cur, {})) // v0.50 (audit S3): 3 tries, then the slot is blocked
        {
            memcpy(bankEntry(bbox, cur), keep.data(), BANK_ENTRY); // as on the card
            blockSlot(bbox, cur);
            msg = "Moved, but the PocketPKMN box could\nnot be saved: the copy left in\nPocketPKMN is locked. Restart\nPocketPKMN and put it back again:\nthe extra copy is then removed.";
            dbg_printf("storage: withdraw of %s: bank save failed (copy in both, slot blocked)", name.c_str());
            return true;
        }
        msg = "Moved " + name + " to the game.";
        dbg_printf("storage: withdraw %s from bank %d/%d -> %s box %d slot %d", name.c_str(), bbox + 1,
            cur + 1, f.label.c_str(), gbox + 1, free + 1);
        return true;
    }

    void editSave(FoundSave& f)
    {
        if (!hasAnyBackup(f.titleId))
        {
            backupGameSave(f);
            dbg_flush(LOG_PATH);
        }
        int boxes      = f.sav->maxBoxes();
        int perBox     = boxes ? f.sav->maxSlot() / boxes : 0;
        int gbox = 0, bbox = 0, side = 0, cur = 0;
        bool party = false, asked = false;
        std::string msg;
        for (;;)
        {
            // Bound afresh each turn: a failed write replaces f.sav (reloadSave).
            pksm::Sav& sav = *f.sav;
            int n    = side == 0 ? perBox : BANK_SLOTS;
            int rows = (n + 1) / 2;
            if (cur >= n)
                cur = n - 1;

            consoleSelect(g_top);
            consoleClear();
            if (party)
            {
                printf("%s\nParty (%u) - read only\n\n", f.label.c_str(), sav.partyCount());
                for (int s = 0; s < sav.partyCount() && s < 6; s++)
                    printf("%2d %s\n", s + 1, describe(sav.pkm((u8)s).get()).c_str());
            }
            else
            {
                printf("%s%s\nBox %d/%d: %s\n", side == 0 ? "> " : "  ", f.label.c_str(), gbox + 1, boxes,
                    sav.boxName((u8)gbox).c_str());
                int grows = (perBox + 1) / 2;
                for (int row = 0; row < grows; row++)
                {
                    int a = row, b = row + grows;
                    printf("%-25s", slotText(sav.pkm((u8)gbox, (u8)a).get(), a, side == 0 && cur == a).c_str());
                    if (b < perBox)
                        printf("%s", slotText(sav.pkm((u8)gbox, (u8)b).get(), b, side == 0 && cur == b).c_str());
                    printf("\n");
                }
            }

            consoleSelect(g_bottom);
            consoleClear();
            printf("%sPocketPKMN box %d/%d\n", side == 1 ? "> " : "  ", bbox + 1, BANK_BOXES);
            int brows = (BANK_SLOTS + 1) / 2;
            for (int row = 0; row < brows; row++)
            {
                int a = row, b = row + brows;
                // 40 columns: two slots of up to 18 characters (8-letter names).
                // '~' after the slot number = on a walk
                printf("%-20s", slotText(bankPkm(bbox, a).get(), a, side == 1 && cur == a, 8, bankEntry(bbox, a)[1] & 1).c_str());
                printf("%s\n", slotText(bankPkm(bbox, b).get(), b, side == 1 && cur == b, 8, bankEntry(bbox, b)[1] & 1).c_str());
            }
            printf("X: switch side  L/R: box  A: move\nY: party  B: back\n%s", msg.c_str());
            refresh();
            if (!msg.empty())
            {
                // Every outcome of A goes to the log, so a move that did not happen
                // is never silent (test S2 had no record of the screen messages).
                dbg_printf("storage: shown: %s", msg.c_str());
                dbg_flush(LOG_PATH);
            }
            msg.clear();

            u32 k = waitKeys(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_L | KEY_R | KEY_A | KEY_X | KEY_Y |
                             KEY_B);
            if (k & KEY_B)
                return;
            if (k & KEY_Y)
                party = !party;
            if (party)
                continue;
            if (k & KEY_X)
                side = 1 - side;
            if (k & KEY_DOWN)
                cur = (cur % rows + 1 < rows && cur + 1 < n) ? cur + 1 : cur - cur % rows;
            if (k & KEY_UP)
                cur = (cur % rows > 0) ? cur - 1 : std::min(n - 1, cur - cur % rows + rows - 1);
            if (k & (KEY_LEFT | KEY_RIGHT))
                cur = cur < rows ? std::min(n - 1, cur + rows) : cur - rows;
            if (k & KEY_R)
            {
                if (side == 0 && boxes)
                    gbox = (gbox + 1) % boxes;
                else
                    bbox = (bbox + 1) % BANK_BOXES;
            }
            if (k & KEY_L)
            {
                if (side == 0 && boxes)
                    gbox = (gbox + boxes - 1) % boxes;
                else
                    bbox = (bbox + BANK_BOXES - 1) % BANK_BOXES;
            }
            if (!(k & KEY_A))
                continue;

            // --- A: move one Pokemon to the other side ---
            {
                std::string why;
                if (!walkerbox::gameMatchSupported(sav, why))
                {
                    msg = "Moving is not possible here:\n" + why + ".";
                    continue;
                }
                if (!kGen2Moves && sav.generation() == pksm::Generation::TWO)
                {
                    // Gen 2 moves stay off until the game-matching rule has been checked
                    // on the user's real Crystal / Silver data on the PC.
                    msg = "Gen 2 moves are switched off\nuntil they are checked. (Opening\nthis save made a backup copy.)";
                    continue;
                }
            }
            if (!asked)
            {
                if (!askOnBottom("Moving Pokemon writes to the game\nsave. A backup of it is made\nfirst. Go on?"))
                    continue;
                asked = true;
            }
            if (!backupGameSave(f))
            {
                msg = "Could not back up the game save.\nNothing was moved.";
                continue;
            }
            // The moves themselves live in depositOne / withdrawOne (unchanged code, moved out 2026-11-15 so
            // that "Send to walker" and "End walk" use exactly the same safety steps).
            if (side == 0)
                depositOne(f, gbox, cur, bbox, -1, msg);
            else
                withdrawOne(f, bbox, cur, gbox, false, msg, nullptr, nullptr);
            dbg_flush(LOG_PATH);
        }
    }
}

extern "C" void storage_browse(PrintConsole* top, PrintConsole* bottom)
{
    g_top    = top;
    g_bottom = bottom;
    consoleSelect(g_top);
    consoleClear();
    printf("MOVE POKEMON\n\nLooking for Pokemon saves\n(Game Boy, GBA, 3DS)...\n");
    refresh();
    dbg_printf("==== storage ====");
    // The PxiFS0 session for GBA saves is opened on first use; close it on the way out.
    struct GbaSession
    {
        ~GbaSession() { walkerbox::gbavc::exit(); }
    } gbaSession;

    if (!prepareData() || !bankLoad())
    {
        printf("\nCould not load the Pokemon data\n(romfs) or the PocketPKMN bank.\nSee the log.\n");
        dbg_flush(LOG_PATH);
        refresh();
        waitKeys(KEY_A | KEY_B);
        return;
    }
    std::vector<FoundSave> saves = scanVcSaves();
    dbg_flush(LOG_PATH);
    if (saves.empty())
    {
        printf("\nNo Pokemon save found.\n(Has the game been saved at least\nonce? GBA: back to HOME after.)\n\nA/B: back\n");
        refresh();
        waitKeys(KEY_A | KEY_B);
        return;
    }
    int sel = 0, n = (int)saves.size();
    for (;;)
    {
        consoleSelect(g_top);
        consoleClear();
        printf("MOVE POKEMON\n\nSaves found:\n\n");
        for (int i = 0; i < n; i++)
            printf(" %c %s\n", i == sel ? '>' : ' ', saves[i].label.c_str());
        consoleSelect(g_bottom);
        consoleClear();
        printf("Up/Down: choose  A: open  B: back\nX: restore the newest backup of\n   the chosen game\n");
        printf("SELECT: set the game's version\n   (if the name is wrong)\n");
        refresh();
        u32 k = waitKeys(KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X | KEY_SELECT);
        if (k & KEY_SELECT)
        {
            // Next version of this generation; remembered for this title.
            FoundSave& f = saves[sel];
            if (f.sav->generation() != pksm::Generation::ONE && f.sav->generation() != pksm::Generation::TWO)
                continue; // Gen 3 saves name their game themselves
            int gen = f.sav->generation() == pksm::Generation::ONE ? 1 : 2;
            int count    = (int)(sizeof(kVersions) / sizeof(kVersions[0]));
            int next     = f.choice;
            do
                next = (next + 1) % count;
            while (kVersions[next].gen != gen);
            applyVersionChoice(f, next);
            saveVersionChoice(f.titleId, next);
            dbg_printf("storage: %016llx: version set by hand to %s", (unsigned long long)f.titleId,
                kVersions[next].label);
            dbg_flush(LOG_PATH);
        }
        if (k & KEY_B)
            return;
        if (k & KEY_DOWN)
            sel = (sel + 1) % n;
        if (k & KEY_UP)
            sel = (sel + n - 1) % n;
        if (k & KEY_A)
            editSave(saves[sel]);
        if (k & KEY_X)
            restoreBackup(saves[sel]);
    }
}

// --- walks (Send to walker, see include/storage.h) -----------------------------------------------------

namespace
{
    constexpr u8 BANK_WALKING = 0x01; // entry byte +1

    bool bankInRange(int box, int slot) { return box >= 0 && box < BANK_BOXES && slot >= 0 && slot < BANK_SLOTS; }

    void setWhy(char* why, int size, const std::string& s)
    {
        if (why && size > 0)
            snprintf(why, (size_t)size, "%s", s.c_str());
    }
}

extern "C" int storage_walk_list(storage_walk_item* items, int max)
{
    if (!prepareData() || !bankLoad())
        return -1;
    int n = 0;
    for (int b = 0; b < BANK_BOXES; b++)
        for (int s = 0; s < BANK_SLOTS && n < max; s++)
        {
            auto pk = bankPkm(b, s);
            if (!pk || pk->species() == pksm::Species::None || pk->egg())
                continue;
            storage_walk_item& it = items[n++];
            it.box     = b;
            it.slot    = s;
            it.gen     = bankGen(b, s);
            it.walking = (bankEntry(b, s)[1] & BANK_WALKING) != 0;
            snprintf(it.label, sizeof(it.label), "%s", describe(pk.get(), 11).c_str());
        }
    return n;
}

extern "C" int storage_walk_prepare(int box, int slot, wbx_walk_poke* out, int* sprite_from_pack, char* why,
    int why_size)
{
    if (!bankInRange(box, slot) || !prepareData() || !bankLoad())
    {
        setWhy(why, why_size, "The PocketPKMN bank cannot be read.");
        return -1;
    }
    if (slotBlocked(box, slot)) // v0.50 (audit S3): never send a copy that is also in a game
    {
        setWhy(why, why_size, "This PocketPKMN copy is also in a game\n(a save failed). Restart PocketPKMN first.");
        return -1;
    }
    auto pk = bankPkm(box, slot);
    if (!pk)
    {
        setWhy(why, why_size, "That PocketPKMN slot is empty.");
        return -1;
    }
    bool fromPack = false;
    std::string reason;
    if (!walkerbox::walkPokeFromPkx(*pk, bankGen(box, slot), *out, fromPack, reason))
    {
        setWhy(why, why_size, reason);
        return -1;
    }
    if (sprite_from_pack)
        *sprite_from_pack = fromPack;
    return 0;
}

extern "C" unsigned long storage_walk_fingerprint(int box, int slot)
{
    if (!bankInRange(box, slot) || !bankLoad())
        return 0;
    const u8* e = bankEntry(box, slot);
    if (!e[0])
        return 0;
    u16 len = (u16)(e[2] | (e[3] << 8));
    u32 h   = 2166136261u;
    h       = (h ^ e[0]) * 16777619u;
    for (u16 i = 0; i < len && i < BANK_DATA; i++)
        h = (h ^ e[8 + i]) * 16777619u;
    return h ? h : 1;
}

extern "C" int storage_walk_mark(int box, int slot, int walking, unsigned long fingerprint, char* why, int why_size)
{
    if (!bankInRange(box, slot) || !bankLoad())
    {
        setWhy(why, why_size, "The PocketPKMN bank cannot be read.");
        return -1;
    }
    if (storage_walk_fingerprint(box, slot) != fingerprint)
    {
        setWhy(why, why_size, "PocketPKMN box " + std::to_string(box + 1) + " slot " + std::to_string(slot + 1) +
                                  " no longer holds that Pokemon.");
        return -1;
    }
    u8* e     = bankEntry(box, slot);
    u8 before = e[1];
    e[1]      = walking ? (u8)(e[1] | BANK_WALKING) : (u8)(e[1] & ~BANK_WALKING);
    if (!bankSave())
    {
        e[1] = before;
        setWhy(why, why_size, "Saving the PocketPKMN bank failed.");
        return -1;
    }
    // read back from the card
    FILE* f   = fopen(BANK_PATH, "rb");
    long at   = (long)(BANK_HEADER + (u32)(box * BANK_SLOTS + slot) * BANK_ENTRY);
    u8 back[BANK_ENTRY];
    bool ok   = f && fseek(f, at, SEEK_SET) == 0 && fread(back, 1, BANK_ENTRY, f) == BANK_ENTRY &&
              memcmp(back, e, BANK_ENTRY) == 0;
    if (f)
        fclose(f);
    if (!ok)
    {
        setWhy(why, why_size, "The PocketPKMN bank did not read back the same.");
        return -1;
    }
    dbg_printf("storage: bank %d/%d walking mark %s", box + 1, slot + 1, walking ? "set" : "cleared");
    return 0;
}

extern "C" int storage_walk_origin(int box, int slot, char* trainer_name, int name_size, unsigned* tid,
    unsigned* sid, unsigned long long* title_id)
{
    if (!bankInRange(box, slot) || !bankLoad())
        return 0;
    const u8* o = bankEntry(box, slot) + BANK_ORIGIN;
    if (!bankEntry(box, slot)[0] || memcmp(o, "WBO1", 4) != 0)
        return 0;
    if (trainer_name && name_size > 0)
        snprintf(trainer_name, (size_t)name_size, "%.23s", (const char*)o + 0x14);
    if (tid)
        *tid = (unsigned)(o[0x10] | (o[0x11] << 8));
    if (sid)
        *sid = (unsigned)(o[0x12] | (o[0x13] << 8));
    if (title_id)
    {
        *title_id = 0;
        for (int i = 0; i < 8; i++)
            *title_id |= (unsigned long long)o[4 + i] << (8 * i);
    }
    return 1;
}

extern "C" int storage_walk_ev_cap(int box, int slot)
{
    if (!bankInRange(box, slot) || !bankLoad())
        return 0;
    return walkerbox::walkEvCap(bankGen(box, slot));
}

extern "C" int storage_walk_return(int box, int slot, unsigned long fingerprint, unsigned steps, int ev_stat,
    storage_walk_result* res, unsigned long* new_fp, char* why, int why_size)
{
    if (!bankInRange(box, slot) || !prepareData() || !bankLoad())
    {
        setWhy(why, why_size, "The PocketPKMN bank cannot be read.");
        return -1;
    }
    if (storage_walk_fingerprint(box, slot) != fingerprint)
    {
        setWhy(why, why_size, "PocketPKMN box " + std::to_string(box + 1) + " slot " + std::to_string(slot + 1) +
                                  " no longer holds that Pokemon.");
        return -1;
    }
    u8* e    = bankEntry(box, slot);
    auto pk  = bankPkm(box, slot);
    int gen  = bankGen(box, slot);
    if (!pk)
    {
        setWhy(why, why_size, "That PocketPKMN slot cannot be read.");
        return -1;
    }
    walkerbox::WalkGains g;
    if (!walkerbox::applyWalkGains(*pk, gen, steps, ev_stat, g))
    {
        setWhy(why, why_size, "No gains could be applied.");
        return -1;
    }
    auto raw = pk->rawData();
    u16 len  = (u16)(e[2] | (e[3] << 8));
    if (raw.size() != len)
    {
        setWhy(why, why_size, "The changed Pokemon has a different size; nothing was written.");
        return -1;
    }
    std::vector<u8> before(e, e + BANK_ENTRY);
    memcpy(e + 8, raw.data(), len); // origin record and the rest of the entry stay as they are
    e[1] = (u8)(e[1] & ~BANK_WALKING);
    if (!bankSave())
    {
        memcpy(e, before.data(), BANK_ENTRY);
        setWhy(why, why_size, "Saving the PocketPKMN bank failed; nothing changed.");
        return -1;
    }
    FILE* f = fopen(BANK_PATH, "rb");
    long at = (long)(BANK_HEADER + (u32)(box * BANK_SLOTS + slot) * BANK_ENTRY);
    u8 back[BANK_ENTRY];
    bool ok = f && fseek(f, at, SEEK_SET) == 0 && fread(back, 1, BANK_ENTRY, f) == BANK_ENTRY &&
              memcmp(back, e, BANK_ENTRY) == 0;
    if (f)
        fclose(f);
    if (!ok)
    {
        setWhy(why, why_size, "The PocketPKMN bank did not read back the same.");
        return -1;
    }
    if (res)
    {
        res->steps         = g.steps;
        res->level_before  = g.levelBefore;
        res->level_after   = g.levelAfter;
        res->exp_before    = g.expBefore;
        res->exp_after     = g.expAfter;
        res->friendship    = g.friendship;
        res->friend_before = g.friendBefore;
        res->friend_after  = g.friendAfter;
        res->ev_stat       = g.evStat;
        res->ev_before     = g.evBefore;
        res->ev_after      = g.evAfter;
    }
    if (new_fp)
        *new_fp = storage_walk_fingerprint(box, slot);
    dbg_printf("storage: walk return bank %d/%d: %u steps, Lv %u -> %u, EXP %u -> %u, friendship %u -> %u, EV %s "
               "%u -> %u", box + 1, slot + 1, (unsigned)g.steps, g.levelBefore, g.levelAfter, (unsigned)g.expBefore,
        (unsigned)g.expAfter,
        g.friendBefore, g.friendAfter, walkerbox::walkEvName(g.evStat), g.evBefore, g.evAfter);
    return 0;
}

// --- one-slot flow (see include/storage.h) ------------------------------------------------------------------

namespace
{
    // Test Path is offered for every game until real routes exist (XY routes: the user's workbook, questions
    // open). With this off, games without routes get "Not yet compatible".
    constexpr bool kTestPathForAllGames = true;

    bool gameHasRoutes(const FoundSave& f)
    {
        (void)f;
        return kTestPathForAllGames;
    }

    // The checks every move does first (as in editSave): the save layout is supported, Gen 2 moves are on,
    // and the untouched save is backed up.
    bool readyToMove(FoundSave& f, std::string& msg)
    {
        std::string why;
        if (!walkerbox::gameMatchSupported(*f.sav, why))
        {
            msg = "Moving is not possible here:\n" + why + ".";
            return false;
        }
        if (!kGen2Moves && f.sav->generation() == pksm::Generation::TWO)
        {
            msg = "Gen 2 moves are switched off.";
            return false;
        }
        if (!backupGameSave(f))
        {
            msg = "Could not back up the game save.\nNothing was moved.";
            return false;
        }
        return true;
    }

    void showMessage(const std::string& title, const std::string& text)
    {
        consoleSelect(g_bottom);
        consoleClear();
        printf("%s\n\n%s\n\nA: OK\n", title.c_str(), text.c_str());
        refresh();
        dbg_printf("storage: shown: %s / %s", title.c_str(), text.c_str());
        dbg_flush(LOG_PATH);
        waitKeys(KEY_A | KEY_B);
    }

    struct GbaSessionGuard
    {
        ~GbaSessionGuard() { walkerbox::gbavc::exit(); }
    };

    // Box browser for choosing the Pokemon to walk. Returns true when it moved into the walker slot.
    bool pickInGame(FoundSave& f)
    {
        if (!hasAnyBackup(f.titleId))
        {
            backupGameSave(f);
            dbg_flush(LOG_PATH);
        }
        int boxes  = f.sav->maxBoxes();
        int perBox = boxes ? f.sav->maxSlot() / boxes : 0;
        int gbox = 0, cur = 0;
        std::string msg;
        for (;;)
        {
            pksm::Sav& sav = *f.sav;
            int rows = (perBox + 1) / 2;
            consoleSelect(g_top);
            consoleClear();
            printf("%s\nBox %d/%d: %s\n", f.label.c_str(), gbox + 1, boxes, sav.boxName((u8)gbox).c_str());
            for (int row = 0; row < rows; row++)
            {
                int a = row, b = row + rows;
                printf("%-25s", slotText(sav.pkm((u8)gbox, (u8)a).get(), a, cur == a).c_str());
                if (b < perBox)
                    printf("%s", slotText(sav.pkm((u8)gbox, (u8)b).get(), b, cur == b).c_str());
                printf("\n");
            }
            consoleSelect(g_bottom);
            consoleClear();
            printf("SEND TO WALKER\nChoose the Pokemon for the walk.\n\nD-pad: move  L/R: box\nA: choose  B: back\n\n%s",
                msg.c_str());
            refresh();
            msg.clear();
            u32 k = waitKeys(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_L | KEY_R | KEY_A | KEY_B);
            if (k & KEY_B)
                return false;
            if (k & KEY_DOWN)
                cur = (cur % rows + 1 < rows && cur + 1 < perBox) ? cur + 1 : cur - cur % rows;
            if (k & KEY_UP)
                cur = (cur % rows > 0) ? cur - 1 : std::min(perBox - 1, cur - cur % rows + rows - 1);
            if (k & (KEY_LEFT | KEY_RIGHT))
                cur = cur < rows ? std::min(perBox - 1, cur + rows) : cur - rows;
            if ((k & KEY_R) && boxes)
                gbox = (gbox + 1) % boxes;
            if ((k & KEY_L) && boxes)
                gbox = (gbox + boxes - 1) % boxes;
            if (!(k & KEY_A))
                continue;
            auto pk = sav.pkm((u8)gbox, (u8)cur);
            if (!pk || pk->species() == pksm::Species::None)
            {
                msg = "That slot is empty.";
                continue;
            }
            if (pk->egg())
            {
                msg = "An egg cannot go for a walk.";
                continue;
            }
            std::string name = describe(pk.get());
            if (!askOnBottom("Send " + name + "\nfor a walk? It moves out of the\ngame into the walker slot and\n"
                             "comes back after the walk."))
                continue;
            if (!readyToMove(f, msg))
                continue;
            bool ok = depositOne(f, gbox, cur, STORAGE_WALK_BOX, STORAGE_WALK_SLOT, msg);
            dbg_flush(LOG_PATH);
            if (ok)
                return true;
        }
    }
}

extern "C" int storage_return_to_game(char* msg, int msg_size)
{
    if (!prepareData() || !bankLoad())
    {
        setWhy(msg, msg_size, "The Pokemon data or the bank cannot be loaded.");
        return -1;
    }
    const int bb = STORAGE_WALK_BOX, bs = STORAGE_WALK_SLOT;
    if (!bankGen(bb, bs))
    {
        setWhy(msg, msg_size, "The walker slot is empty.");
        return 1;
    }
    if (bankEntry(bb, bs)[1] & BANK_WALKING)
    {
        setWhy(msg, msg_size, "It is still on a walk.");
        return 0;
    }
    char ot[24];
    unsigned long long tid = 0;
    if (!storage_walk_origin(bb, bs, ot, sizeof(ot), nullptr, nullptr, &tid))
    {
        setWhy(msg, msg_size, "It was put in before v0.28 and has no\nrecord of its game. Use Walker tools ->\nMove Pokemon to move it back.");
        return 0;
    }
    GbaSessionGuard gba;
    std::vector<FoundSave> saves = scanVcSaves();
    // v0.50 (audit S7): the copy it came from (game card or SD), if recorded. Only that copy, or with the user's
    // OK another copy of the same game when that one is not here.
    int recMedia = bankEntry(bb, bs)[BANK_ORIGIN + 0x0F] - 1; // -1 = not recorded
    bool sameHere = false;
    for (FoundSave& f : saves)
        if (f.titleId == tid && (recMedia < 0 || (int)f.media == recMedia))
            sameHere = true;
    for (FoundSave& f : saves)
    {
        if (f.titleId != tid)
            continue;
        if (recMedia >= 0 && (int)f.media != recMedia)
        {
            if (sameHere)
                continue;
            if (!askOnBottom(std::string("It came from the ") + (recMedia == MEDIATYPE_GAME_CARD ? "game card" : "SD card") +
                             " copy\nof this game, which is not here.\nPut it in the " +
                             (f.media == MEDIATYPE_GAME_CARD ? "game card" : "SD card (eShop)") + " copy instead?"))
            {
                setWhy(msg, msg_size, "Not moved. It waits in the walker slot.");
                return 0;
            }
        }
        std::string m;
        if (!readyToMove(f, m))
        {
            setWhy(msg, msg_size, m);
            return -1;
        }
        int gb = -1, gs = -1;
        bool ok = withdrawOne(f, bb, bs, 0, true, m, &gb, &gs);
        dbg_flush(LOG_PATH);
        if (ok)
        {
            setWhy(msg, msg_size, "Back in " + f.name + "\n(box " + std::to_string(gb + 1) + ", slot " +
                                      std::to_string(gs + 1) + ").");
            return 1;
        }
        setWhy(msg, msg_size, m + "\nIt waits in the walker slot.");
        return 0;
    }
    char t[64];
    snprintf(t, sizeof(t), "%016llx", tid);
    setWhy(msg, msg_size, std::string("Its game (") + t + ")\nis not on this 3DS (cartridge out?).\nIt waits in the "
                                                          "walker slot.");
    return 0;
}

extern "C" int storage_pick_for_walk(PrintConsole* top, PrintConsole* bottom)
{
    g_top    = top;
    g_bottom = bottom;
    consoleSelect(g_top);
    consoleClear();
    printf("SEND TO WALKER\n\nLooking for Pokemon saves\n(Game Boy, GBA, 3DS)...\n");
    refresh();
    dbg_printf("==== send: pick ====");
    GbaSessionGuard gba;
    if (!prepareData() || !bankLoad())
    {
        showMessage("SEND TO WALKER", "Could not load the Pokemon data\n(romfs) or the bank. See the log.");
        return 0;
    }
    const int bb = STORAGE_WALK_BOX, bs = STORAGE_WALK_SLOT;
    if (bankGen(bb, bs))
    {
        if (bankEntry(bb, bs)[1] & BANK_WALKING)
        {
            showMessage("SEND TO WALKER", "A Pokemon is already on a walk.\nEnd that walk first.");
            return 0;
        }
        // A Pokemon waits in the slot (its game was not here at the last walk end): offer it again.
        auto pk = bankPkm(bb, bs);
        consoleSelect(g_top);
        consoleClear();
        printf("The walker slot holds\n%s\n(back from a walk; its game was not\nfound then).\n",
            describe(pk.get()).c_str());
        consoleSelect(g_bottom);
        consoleClear();
        printf("A: send it for another walk\nX: try to put it back in its game\nB: back\n");
        refresh();
        u32 k = waitKeys(KEY_A | KEY_X | KEY_B);
        if (k & KEY_A)
            return 1;
        if (k & KEY_X)
        {
            char m[160];
            storage_return_to_game(m, sizeof(m));
            showMessage("PUT IT BACK", m);
        }
        return 0;
    }
    std::vector<FoundSave> saves = scanVcSaves();
    dbg_flush(LOG_PATH);
    if (saves.empty())
    {
        showMessage("SEND TO WALKER", "No Pokemon save found.\n(Has the game been saved at least\nonce?)");
        return 0;
    }
    int sel = 0, n = (int)saves.size();
    for (;;)
    {
        consoleSelect(g_top);
        consoleClear();
        printf("SEND TO WALKER\n\nChoose the game whose Pokemon\ngoes for a walk:\n\n");
        for (int i = 0; i < n; i++)
            printf(" %c %s\n", i == sel ? '>' : ' ', saves[i].label.c_str());
        consoleSelect(g_bottom);
        consoleClear();
        printf("Up/Down: choose  A: use this game\nB: back\n");
        refresh();
        u32 k = waitKeys(KEY_UP | KEY_DOWN | KEY_A | KEY_B);
        if (k & KEY_B)
            return 0;
        if (k & KEY_DOWN)
            sel = (sel + 1) % n;
        if (k & KEY_UP)
            sel = (sel + n - 1) % n;
        if (!(k & KEY_A))
            continue;
        if (!gameHasRoutes(saves[sel]))
        {
            showMessage("NOT YET COMPATIBLE", saves[sel].name + " has no walker\nroutes yet, so its Pokemon\n"
                                                                "cannot go for a walk.");
            continue;
        }
        if (pickInGame(saves[sel]))
            return bankGen(bb, bs) ? 1 : 0;
    }
}

// --- v0.37: the one-slot pick as data for the GPU screens (include/storage.h) -------------------------------------
// New functions only; they call the same scan, readyToMove and depositOne as storage_pick_for_walk / pickInGame.

namespace
{
    std::vector<FoundSave> g_games;
    std::vector<bool> g_gameEntered;
    std::unique_ptr<GbaSessionGuard> g_gbaGuard;

    int genOf(const pksm::Sav& sav)
    {
        using G = pksm::Generation;
        switch (sav.generation())
        {
            case G::ONE: return 1;
            case G::TWO: return 2;
            case G::THREE: return 3;
            case G::FOUR: return 4;
            case G::FIVE: return 5;
            case G::SIX: return 6;
            case G::SEVEN: return 7;
            default: return 0;
        }
    }

    void copyStr(char* out, size_t n, const std::string& s) { snprintf(out, n, "%s", s.c_str()); }
}

extern "C" void storage_games_release(void)
{
    g_games.clear();
    g_gameEntered.clear();
    g_gbaGuard.reset(); // ends the GBA session (as GbaSessionGuard at the end of storage_pick_for_walk)
}

// v0.52 (locked courses): has the save entered the Hall of Fame? Read only.
//   Gen 2: the first Hall of Fame record (newest first) in SRAM bank 1 - Gold/Silver at file 0x321A, Crystal at
//   0x32C0 (found on the user's own saves, pokecrystal "SRAM Hall of Fame"): win count > 0, a real first species,
//   the record's end byte 0xFF. Record layout: win count, 6 x 16-byte mons, end byte (0x62 bytes).
//   X / Y: Hall of Fame block at 0x19400 (PKHeX SAV6XY "HoF"), entry 0 = the first clear, its u32 index at +0x1B0
//   has "has data" in bit 31 (PKHeX HallFame6Index) -> byte 0x195B3 bit 7.
//   Other games: 0 (no courses use it yet).
namespace
{
    int hallOfFameOf(const FoundSave& f)
    {
        const u8* d = f.sav->rawData().get();
        int gen     = genOf(*f.sav);
        if (gen == 2)
        {
            u32 base = f.sav->version() == pksm::GameVersion::C ? 0x32C0 : 0x321A;
            if (f.length < base + 0x62)
                return 0;
            return d[base] != 0 && d[base + 1] >= 1 && d[base + 1] <= 251 && d[base + 0x61] == 0xFF;
        }
        if (gen == 6 && (f.sav->version() == pksm::GameVersion::X || f.sav->version() == pksm::GameVersion::Y))
            return f.length > 0x195B3 && (d[0x195B3] & 0x80) != 0;
        return 0;
    }

    struct Progress
    {
        u64 tid;
        int badges, hof;
    };
    std::vector<Progress> g_progress; // kept after storage_games_release: the send screen asks for it later
}

extern "C" int storage_game_progress(unsigned long long title_id, int* badges, int* hof)
{
    for (const Progress& p : g_progress)
        if (p.tid == title_id)
        {
            *badges = p.badges;
            *hof    = p.hof;
            return 1;
        }
    return 0;
}

extern "C" int storage_games_scan(storage_game_info* out, int max, char* why, int why_size)
{
    storage_games_release();
    dbg_printf("==== send: pick (GPU screens) ====");
    if (!prepareData() || !bankLoad())
    {
        setWhy(why, why_size, "Could not load the Pokemon data (romfs) or the bank. See the log.");
        return -1;
    }
    g_gbaGuard = std::make_unique<GbaSessionGuard>();
    g_games    = scanVcSaves();
    g_gameEntered.assign(g_games.size(), false);
    dbg_flush(LOG_PATH);
    int n = 0;
    for (FoundSave& f : g_games)
    {
        if (n >= max)
            break;
        storage_game_info& g = out[n++];
        memset(&g, 0, sizeof(g));
        g.title_id = f.titleId;
        copyStr(g.name, sizeof(g.name), f.choice >= 0 ? std::string(kVersions[f.choice].label)
                                        : !f.name.empty() ? f.name
                                                          : i18n::game(pksm::Language::ENG, f.sav->version()));
        copyStr(g.trainer, sizeof(g.trainer), f.sav->otName());
        g.tid        = f.sav->displayTID();
        g.gen        = genOf(*f.sav);
        g.kind       = f.gba ? 2 : f.media == MEDIATYPE_GAME_CARD ? 1 : 0;
        g.has_routes = routes_file_for(f.titleId, g.gen, nullptr) != nullptr; // v0.41: a route file for this game
        g.boxes      = f.sav->maxBoxes();
        g.per_box    = g.boxes ? f.sav->maxSlot() / g.boxes : 0;
        // v0.52: progress for the course unlocks (badges: PKSM-Core; Gen 2 counts Johto + Kanto badges)
        g.badges = f.sav->badges();
        g.hof    = hallOfFameOf(f);
        bool known = false;
        for (Progress& p : g_progress)
            if (p.tid == f.titleId)
            {
                p.badges = g.badges;
                p.hof    = g.hof;
                known    = true;
            }
        if (!known)
            g_progress.push_back({f.titleId, g.badges, g.hof});
        dbg_printf("storage: %s: %d badge(s), Hall of Fame %s", g.name, g.badges, g.hof ? "yes" : "no");
    }
    return n;
}

extern "C" int storage_game_box(int game, int box, storage_box_mon* out, int max, char* box_name, int name_size)
{
    if (game < 0 || game >= (int)g_games.size())
        return -1;
    FoundSave& f = g_games[game];
    if (!g_gameEntered[game]) // as pickInGame on entering a game: back it up if it never was
    {
        g_gameEntered[game] = true;
        if (!hasAnyBackup(f.titleId))
        {
            backupGameSave(f);
            dbg_flush(LOG_PATH);
        }
    }
    pksm::Sav& sav = *f.sav;
    int boxes = sav.maxBoxes(), perBox = boxes ? sav.maxSlot() / boxes : 0;
    if (box < 0 || box >= boxes)
        return -1;
    if (box_name && name_size > 0)
        copyStr(box_name, (size_t)name_size, sav.boxName((u8)box));
    int gen = genOf(sav);
    for (int s = 0; s < perBox && s < max; s++)
    {
        storage_box_mon& m = out[s];
        memset(&m, 0, sizeof(m));
        auto pk = sav.pkm((u8)box, (u8)s);
        if (!pk || pk->species() == pksm::Species::None)
            continue;
        m.present = 1;
        m.species = (int)u16(pk->species());
        m.form    = pk->alternativeForm();
        m.level   = pk->level();
        m.gender  = (int)u8(pk->gender());
        m.egg     = pk->egg();
        m.shiny   = pk->shiny();
        copyStr(m.nickname, sizeof(m.nickname), pk->nickname());
        copyStr(m.species_name, sizeof(m.species_name), i18n::species(pksm::Language::ENG, pk->species()));
        if (gen >= 3)
        {
            copyStr(m.nature, sizeof(m.nature), i18n::nature(pksm::Language::ENG, pk->nature()));
            copyStr(m.ability, sizeof(m.ability), i18n::ability(pksm::Language::ENG, pk->ability()));
            m.has_evs = 1;
            const pksm::Stat st[6] = {pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPATK,
                pksm::Stat::SPDEF, pksm::Stat::SPD};
            for (int i = 0; i < 6; i++)
                m.evs[i] = pk->ev(st[i]);
        }
        m.friendship = gen >= 2 ? pk->currentFriendship() : 0;
        if (m.level < 100 && !m.egg)
        {
            u32 exp = pk->experience();
            auto a = pk->clone(), b = pk->clone();
            a->level((u8)m.level);
            b->level((u8)(m.level + 1));
            u32 start = a->experience(), next = b->experience();
            m.exp_level_span = next > start ? next - start : 0;
            m.exp_into_level = exp > start ? exp - start : 0;
            m.exp_to_next    = next > exp ? next - exp : 0;
        }
        copyStr(m.ot, sizeof(m.ot), pk->otName());
        m.ot_tid = pk->TID();
    }
    return boxes;
}

extern "C" int storage_pick_commit(int game, int box, int slot, char* msg, int msg_size)
{
    if (game < 0 || game >= (int)g_games.size())
    {
        setWhy(msg, msg_size, "No game chosen.");
        return 0;
    }
    FoundSave& f = g_games[game];
    if (!routes_file_for(f.titleId, genOf(*f.sav), nullptr)) // v0.41: same rule as the game list
    {
        setWhy(msg, msg_size, f.name + " has no walker routes yet.");
        return 0;
    }
    // From here exactly pickInGame's A branch.
    std::string m;
    auto pk = f.sav->pkm((u8)box, (u8)slot);
    if (!pk || pk->species() == pksm::Species::None)
    {
        setWhy(msg, msg_size, "That slot is empty.");
        return 0;
    }
    if (pk->egg())
    {
        setWhy(msg, msg_size, "An egg cannot go for a walk.");
        return 0;
    }
    if (!readyToMove(f, m))
    {
        setWhy(msg, msg_size, m);
        return 0;
    }
    bool ok = depositOne(f, box, slot, STORAGE_WALK_BOX, STORAGE_WALK_SLOT, m);
    dbg_flush(LOG_PATH);
    if (!ok)
    {
        setWhy(msg, msg_size, m);
        return 0;
    }
    return bankGen(STORAGE_WALK_BOX, STORAGE_WALK_SLOT) ? 1 : 0;
}

// v0.50 (audit S2): a message about the bank for the user (recovered from bank.tmp / bank.bak), once; NULL if none.
extern "C" const char* storage_take_bank_note(void)
{
    static std::string shown;
    if (g_bankNote.empty())
        return nullptr;
    shown = g_bankNote;
    g_bankNote.clear();
    return shown.c_str();
}

// v0.50 (audit B2): the first bank slot marked walking. Returns 1 with box/slot/fingerprint, 0 if none.
extern "C" int storage_walking_slot(int* box, int* slot, unsigned long* fingerprint)
{
    if (!bankLoad())
        return 0;
    for (int b = 0; b < BANK_BOXES; b++)
        for (int s = 0; s < BANK_SLOTS; s++)
            if (bankGen(b, s) && (bankEntry(b, s)[1] & BANK_WALKING))
            {
                *box         = b;
                *slot        = s;
                *fingerprint = storage_walk_fingerprint(b, s);
                return 1;
            }
    return 0;
}

// v0.50 (result screen): the English species name, "" if the data cannot be loaded. Not for IR time (romfs).
extern "C" void storage_species_name(int species, char* out, int out_size)
{
    if (out_size <= 0)
        return;
    out[0] = 0;
    if (species <= 0 || !prepareData())
        return;
    snprintf(out, (size_t)out_size, "%s", i18n::species(pksm::Language::ENG, pksm::Species((u16)species)).c_str());
}

// v0.50 (result screen): an item's English name. gen2 = 1: Gen 2 item id (the GSC courses' numbering,
// romfs i18n/eng/items2.txt, line n = id n); else the national (Gen 4+) numbering. "" if unknown. Not for IR time.
extern "C" void storage_item_name(int gen2, int id, char* out, int out_size)
{
    if (out_size <= 0)
        return;
    out[0] = 0;
    if (id <= 0 || !prepareData())
        return;
    if (gen2)
    {
        FILE* f = fopen("romfs:/i18n/eng/items2.txt", "r");
        if (!f)
            return;
        char line[64];
        for (int n = 0; fgets(line, sizeof(line), f); n++)
            if (n == id)
            {
                line[strcspn(line, "\r\n")] = 0;
                snprintf(out, (size_t)out_size, "%s", line);
                break;
            }
        fclose(f);
        return;
    }
    snprintf(out, (size_t)out_size, "%s", i18n::item(pksm::Language::ENG, (u16)id).c_str());
}

// --- v0.53: a walk's catches, made legal (walk_catch.cpp, templates from PKHeX) and put into the walked Pokemon's
// game. Each goes into a free PocketPKMN slot first (with the walked Pokemon's origin record), then into the game by
// withdrawOne (the same checked write as every move). A catch that cannot go into the game waits in PocketPKMN.
namespace
{
    // the template for (version index, course, poke) from romfs routes/catch_*.bin
    bool findTemplate(const char* file, int vi, int course, int poke, std::vector<u8>& data, walkerbox::CatchTemplate& t)
    {
        FILE* f = fopen(file, "rb");
        if (!f)
            return false;
        std::vector<u8> bin;
        u8 buf[4096];
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
            bin.insert(bin.end(), buf, buf + got);
        fclose(f);
        if (bin.size() < 6 || memcmp(bin.data(), "PPCT", 4))
            return false;
        size_t at = 6;
        while (at + 8 <= bin.size())
        {
            size_t len = bin[at + 6] | (bin[at + 7] << 8);
            if (at + 8 + len > bin.size())
                break;
            if (bin[at] == vi && bin[at + 1] == course && bin[at + 2] == poke)
            {
                data.assign(bin.begin() + at + 8, bin.begin() + at + 8 + len);
                t.gen      = bin[at + 3];
                t.flags    = bin[at + 4];
                t.fixedIVs = bin[at + 5];
                t.data     = data.data();
                t.len      = data.size();
                return true;
            }
            at += 8 + len;
        }
        return false;
    }

    // the course Pokemon a catch came from: same species, level and moves on one of the game's courses
    bool findCoursePoke(const char* routesFile, const storage_catch& c, int& course, int& poke)
    {
        static route_file rf;
        if (routes_load(routesFile, &rf) <= 0)
            return false;
        for (int ci = 0; ci < rf.ncourse; ci++)
            for (int pi = 0; pi < rf.course[ci].npoke; pi++)
            {
                const route_poke_def& p = rf.course[ci].poke[pi];
                if (p.species != c.species || p.level != c.level)
                    continue;
                bool same = true;
                for (int m = 0; m < 4; m++)
                    same = same && p.moves[m] == c.moves[m];
                if (same)
                {
                    course = ci;
                    poke   = pi;
                    return true;
                }
            }
        return false;
    }
}

extern "C" int storage_deliver_catches(int from_box, int from_slot, const storage_catch* catches, int n, char* msg,
    int msg_size)
{
    std::string out;
    setWhy(msg, msg_size, "");
    if (n <= 0)
        return 0;
    if (!bankInRange(from_box, from_slot) || !prepareData() || !bankLoad() || !bankGen(from_box, from_slot))
    {
        setWhy(msg, msg_size, "The caught Pokemon could not be made (PocketPKMN box).");
        return -1;
    }
    char ot[24];
    unsigned long long tid = 0;
    if (!storage_walk_origin(from_box, from_slot, ot, sizeof(ot), nullptr, nullptr, &tid))
    {
        setWhy(msg, msg_size, "The caught Pokemon could not be made: the walked\nPokemon has no record of its game.");
        return -1;
    }
    int gen    = bankGen(from_box, from_slot);
    auto shell = bankPkm(from_box, from_slot);
    std::vector<u8> origin(bankEntry(from_box, from_slot) + BANK_ORIGIN, bankEntry(from_box, from_slot) + BANK_ENTRY);
    GbaSessionGuard gba;
    std::vector<FoundSave> saves = scanVcSaves();
    FoundSave* fs = nullptr;
    int recMedia  = origin[0x0F] - 1;
    for (FoundSave& f : saves)
        if (f.titleId == tid && (recMedia < 0 || (int)f.media == recMedia))
            fs = &f;
    if (!fs)
        for (FoundSave& f : saves)
            if (f.titleId == tid)
                fs = &f;
    if (!fs)
    {
        setWhy(msg, msg_size, "The caught Pokemon wait: their game is not here.");
        return -1;
    }
    FoundSave& f           = *fs;
    pksm::GameVersion ver  = f.sav->version();
    const char* tplFile    = nullptr;
    const char* routesFile = routes_file_for(tid, gen, nullptr);
    int vi                 = -1;
    if (gen == 6 && (ver == pksm::GameVersion::X || ver == pksm::GameVersion::Y))
    {
        tplFile = "romfs:/routes/catch_xy.bin";
        vi      = ver == pksm::GameVersion::X ? 0 : 1;
    }
    else if (gen == 2)
    {
        tplFile = "romfs:/routes/catch_gsc.bin";
        vi      = ver == pksm::GameVersion::C ? 2 : ver == pksm::GameVersion::SV ? 1 : 0;
    }
    if (!tplFile || !routesFile)
    {
        setWhy(msg, msg_size, "No catches can be made for this game yet.");
        return -1;
    }
    walkerbox::CatchTrainer tr;
    tr.name     = f.sav->otName();
    tr.tid      = f.sav->TID();
    tr.sid      = gen == 2 ? 0 : f.sav->SID();
    tr.female   = f.sav->gender() == pksm::Gender::Female;
    tr.language = f.sav->language();
    if (gen == 6)
    {
        tr.country       = f.sav->country();
        tr.subRegion     = f.sav->subRegion();
        tr.consoleRegion = f.sav->consoleRegion();
    }
    time_t now    = time(nullptr);
    struct tm* lt = localtime(&now);
    int delivered = 0;
    std::string ready;
    bool canMove = readyToMove(f, ready);
    for (int i = 0; i < n; i++)
    {
        const storage_catch& c = catches[i];
        int course = -1, poke = -1;
        std::vector<u8> tdata;
        walkerbox::CatchTemplate t;
        std::string name = i18n::species(pksm::Language::ENG, pksm::Species{c.species});
        if (!findCoursePoke(routesFile, c, course, poke) || !findTemplate(tplFile, vi, course, poke, tdata, t))
        {
            out += name + ": not one of this game's course Pokemon\n(kept only in the log).\n";
            dbg_printf("catch %s Lv%d: no course / template match", name.c_str(), c.level);
            continue;
        }
        std::unique_ptr<pksm::PKX> pk;
        std::string why;
        u32 seed = (u32)now ^ (u32)(c.species * 2654435761u) ^ (u32)(i * 40503u) ^ (u32)svcGetSystemTick();
        if (!walkerbox::buildCatch(t, tr, shell.get(), c.shiny != 0, c.female != 0, seed, lt ? lt->tm_mday : 1,
                lt ? lt->tm_mon + 1 : 1, lt ? lt->tm_year + 1900 : 2026, pk, why))
        {
            out += name + ": could not be made (" + why + ").\n";
            dbg_printf("catch %s: build failed: %s", name.c_str(), why.c_str());
            continue;
        }
        int bb = -1, bs = -1; // a free PocketPKMN slot, never the walker slot
        for (int b = 0; b < BANK_BOXES && bb < 0; b++)
            for (int s = 0; s < BANK_SLOTS && bb < 0; s++)
                if (!(b == STORAGE_WALK_BOX && s == STORAGE_WALK_SLOT) && !bankGen(b, s))
                {
                    bb = b;
                    bs = s;
                }
        if (bb < 0)
        {
            out += name + ": the PocketPKMN boxes are full.\n";
            continue;
        }
        Raw raw = rawOf(pk.get());
        bankPut(bb, bs, gen, raw);
        memcpy(bankEntry(bb, bs) + BANK_ORIGIN, origin.data(), origin.size()); // the same game and trainer
        if (!bankSaveChecked(bb, bs, raw))
        {
            bankClear(bb, bs);
            bankSave();
            out += name + ": could not be saved.\n";
            continue;
        }
        dbg_printf("catch %s Lv%d (course %d poke %d, template flags %d) -> bank %d/%d", name.c_str(), c.level,
            course, poke, t.flags, bb + 1, bs + 1);
        std::string m;
        int gb = -1, gs = -1;
        if (canMove && withdrawOne(f, bb, bs, 0, true, m, &gb, &gs))
        {
            delivered++;
            out += name + " is now in " + f.name + " (box " + std::to_string(gb + 1) + ").\n";
        }
        else
            out += name + " waits in PocketPKMN: " + (canMove ? m : ready) + "\n";
    }
    dbg_flush(LOG_PATH);
    setWhy(msg, msg_size, out);
    return delivered;
}

// v0.53 (user: "items sent over should go to pc, not item bag. Unless its a generation without an item pc"): the
// walk's items into the walked Pokemon's game - Gen 2: the PC item storage, X/Y (no item PC): the bag. One item at a
// time: added in memory, checked to change only that item list (gameMatchItems), written, and read back on failure.
extern "C" int storage_deliver_items(int from_box, int from_slot, const storage_found_item* items, int n,
    char* where, int where_size, char* msg, int msg_size)
{
    setWhy(msg, msg_size, "");
    setWhy(where, where_size, "");
    if (n <= 0)
        return 0;
    char ot[24];
    unsigned long long tid = 0;
    if (!bankInRange(from_box, from_slot) || !prepareData() || !bankLoad() || !bankGen(from_box, from_slot) ||
        !storage_walk_origin(from_box, from_slot, ot, sizeof(ot), nullptr, nullptr, &tid))
    {
        setWhy(msg, msg_size, "The items could not be sent (no record of the game).");
        return -1;
    }
    int recMedia = bankEntry(from_box, from_slot)[BANK_ORIGIN + 0x0F] - 1;
    GbaSessionGuard gba;
    std::vector<FoundSave> saves = scanVcSaves();
    FoundSave* fs = nullptr;
    for (FoundSave& f : saves)
        if (f.titleId == tid && (recMedia < 0 || (int)f.media == recMedia))
            fs = &f;
    if (!fs)
        for (FoundSave& f : saves)
            if (f.titleId == tid)
                fs = &f;
    if (!fs || fs->gba)
    {
        setWhy(msg, msg_size, fs ? "Items cannot be sent to this game yet." : "The items wait: their game is not here.");
        return -1;
    }
    FoundSave& f = *fs;
    std::string ready;
    if (!readyToMove(f, ready))
    {
        setWhy(msg, msg_size, "The items could not be sent: " + ready);
        return -1;
    }
    std::string out, place;
    int sent = 0;
    for (int i = 0; i < n; i++)
    {
        u8* d = f.sav->rawData().get();
        std::vector<u8> before(d, d + f.length);
        std::string w, note;
        char nm[32];
        storage_item_name(items[i].gen2id, (int)items[i].id, nm, sizeof(nm));
        std::string name = nm[0] ? nm : "Item " + std::to_string(items[i].id);
        if (!walkerbox::addItemToSave(*f.sav, (u16)items[i].id, items[i].gen2id != 0, w))
        {
            memcpy(f.sav->rawData().get(), before.data(), f.length);
            f.sav->beginEditing();
            out += name + ": " + w + ".\n";
            continue;
        }
        if (!walkerbox::gameMatchItems(*f.sav, before.data(), f.length, note))
        {
            dbg_printf("items: %s refused: %s", name.c_str(), note.c_str());
            out += name + ": not stored (the safety check refused).\n";
            continue; // gameMatchItems put the save back
        }
        if (!writeUserSave(f.titleId, f.media, f.path, f.sav->rawData().get(), f.length))
        {
            reloadSave(f);
            out += name + ": writing the game save failed.\n";
            dbg_printf("items: %s: write failed", name.c_str());
            continue;
        }
        dbg_printf("items: %d (%s) -> %s of %s (%s)", items[i].id, items[i].gen2id ? "Gen 2 id" : "national",
            w.c_str(), f.label.c_str(), note.c_str());
        place = w;
        sent++;
    }
    if (sent)
        setWhy(where, where_size, place == "PC" ? "the PC" : "the bag");
    dbg_flush(LOG_PATH);
    setWhy(msg, msg_size, out);
    return sent;
}
