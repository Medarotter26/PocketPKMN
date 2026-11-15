// See include/gbavc.hpp.
#include "gbavc.hpp"

#include "utils/crypto.hpp"

#include <algorithm>
#include <cstring>

#ifdef __3DS__
#include <3ds.h>
extern "C" Result svcControlService(int op, ...);
#endif

namespace walkerbox::gbavc
{
    namespace
    {
        constexpr u32 HDR = sizeof(Header);

        bool isHeader(const std::vector<u8>& file, u32 at)
        {
            return at + HDR <= file.size() && memcmp(file.data() + at, ".SAV", 4) == 0;
        }

        Header headerAt(const std::vector<u8>& file, u32 at)
        {
            Header h;
            memcpy(&h, file.data() + at, HDR);
            return h;
        }

        bool allBytes(const std::vector<u8>& file, u8 v, u32 n)
        {
            return file.size() >= n && std::all_of(file.begin(), file.begin() + n, [v](u8 b) { return b == v; });
        }
    }

    bool findHeaders(const std::vector<u8>& file, u32& h1, u32& h2, bool& has1, bool& has2, std::string& why)
    {
        h1 = 0;
        h2 = 0;
        has1 = has2 = false;
        if (file.size() < 2 * HDR)
        {
            why = "file too small";
            return false;
        }
        if (allBytes(file, 0x00, 0x20))
        {
            why = "the game has never been saved";
            return false;
        }
        if (allBytes(file, 0xFF, 0x20))
        {
            // Only slot 2 has been written; it sits after slot 1's (unknown) size.
            static constexpr u32 SIZES[] = {0x400, 0x2000, 0x8000, 0x10000, 0x20000};
            for (u32 s : SIZES)
                if (isHeader(file, HDR + s))
                {
                    h2   = HDR + s;
                    has2 = true;
                    return true;
                }
            why = "no save header found";
            return false;
        }
        if (!isHeader(file, 0))
        {
            why = "not a GBA Virtual Console save";
            return false;
        }
        has1 = true;
        Header a = headerAt(file, 0);
        h2   = HDR + a.saveSize;
        has2 = isHeader(file, h2);
        return true;
    }

    Pick choose(const std::vector<u8>& file, u32 h1, u32 h2, bool has1, bool has2, bool valid1, bool valid2)
    {
        Pick p;
        bool ok1 = has1 && valid1, ok2 = has2 && valid2;
        if (!ok1 && !ok2)
        {
            p.why = (has1 || has2) ? "the save's check value (CMAC) is wrong" : "no save";
            return p;
        }
        int slot = ok1 && !ok2 ? 1 : !ok1 && ok2 ? 2 : 0;
        if (!slot)
        {
            // Both valid: slot 2 is newer exactly when its counter is slot 1's + 1.
            u32 c1 = headerAt(file, h1).savesMade, c2 = headerAt(file, h2).savesMade;
            slot = c2 == c1 + 1 ? 2 : 1;
        }
        p.slot      = slot;
        p.headerOfs = slot == 1 ? h1 : h2;
        p.saveOfs   = p.headerOfs + HDR;
        p.saveSize  = headerAt(file, p.headerOfs).saveSize;
        if ((u64)p.saveOfs + p.saveSize > file.size())
        {
            p.slot = 0;
            p.why  = "the save is cut short";
        }
        return p;
    }

    std::array<u8, 32> slotHash(const std::vector<u8>& file, u32 headerOfs, u32 saveSize)
    {
        u32 from = headerOfs + 0x30, to = headerOfs + HDR + saveSize;
        if (to > file.size())
            to = (u32)file.size();
        return pksm::crypto::sha256({file.data() + from, to - from});
    }

    bool buildNext(const std::vector<u8>& file, const Pick& cur, const u8* save, u32 size, std::vector<u8>& out,
        u32& newHeaderOfs)
    {
        if (!cur.slot || size != cur.saveSize)
            return false;
        newHeaderOfs = cur.slot == 1 ? HDR + size : 0;
        if ((u64)newHeaderOfs + HDR + size > file.size() || (u64)HDR * 2 + size * 2 > file.size())
            return false;
        Header h = headerAt(file, cur.headerOfs);
        h.savesMade++;
        memset(h.cmac, 0, sizeof(h.cmac)); // set on the console
        out = file;
        memcpy(out.data() + newHeaderOfs, &h, HDR);
        memcpy(out.data() + newHeaderOfs + HDR, save, size);
        return true;
    }

#ifdef __3DS__
    namespace
    {
        Handle g_pxi = 0;

        // PKSM asks repeatedly until two answers agree.
        bool cmacOk(FSPXI_File fh, const std::vector<u8>& file, u32 headerOfs)
        {
            Header h = headerAt(file, headerOfs);
            std::array<u8, 32> hash = slotHash(file, headerOfs, h.saveSize);
            std::array<u8, 16> now{}, prev{};
            Result r;
            int tries = 10;
            do
            {
                prev = now;
                r    = FSPXI_CalcSavegameMAC(g_pxi, fh, hash.data(), hash.size(), now.data(), now.size());
            } while (R_SUCCEEDED(r) && now != prev && --tries > 0);
            return R_SUCCEEDED(r) && memcmp(now.data(), h.cmac, 16) == 0;
        }
    }

    bool init(std::string& why)
    {
        if (g_pxi)
            return true;
        Result r = svcControlService(0 /* SERVICEOP_STEAL_CLIENT_SESSION */, &g_pxi, "PxiFS0");
        if (R_FAILED(r))
        {
            g_pxi = 0;
            char buf[64];
            snprintf(buf, sizeof(buf), "PxiFS0 session %08lx (Luma3DS needed)", (unsigned long)r);
            why = buf;
            return false;
        }
        return true;
    }

    void exit()
    {
        if (g_pxi)
            svcCloseHandle(g_pxi);
        g_pxi = 0;
    }

    bool read(u64 tid, std::vector<u8>& file, Pick& pick, std::string& why)
    {
        pick = Pick{};
        if (!g_pxi && !init(why))
            return false;
        const u32 archPath[4] = {(u32)tid, (u32)(tid >> 32), MEDIATYPE_SD, 0};
        FSPXI_Archive arch;
        Result r = FSPXI_OpenArchive(g_pxi, &arch, ARCHIVE_SAVEDATA_AND_CONTENT,
            {PATH_BINARY, sizeof(archPath), archPath});
        char buf[96];
        if (R_FAILED(r))
        {
            snprintf(buf, sizeof(buf), "open archive %08lx", (unsigned long)r);
            why = buf;
            return false;
        }
        static const u32 filePath[5] = {1 /* save data */, 1 /* TMD content index */, 3 /* save */, 0, 0};
        FSPXI_File fh;
        r = FSPXI_OpenFile(g_pxi, &fh, arch, {PATH_BINARY, sizeof(filePath), filePath}, FS_OPEN_READ, 0);
        if (R_FAILED(r))
        {
            FSPXI_CloseArchive(g_pxi, arch);
            snprintf(buf, sizeof(buf), "open save %08lx", (unsigned long)r);
            why = buf;
            return false;
        }
        bool ok = false;
        u64 size = 0;
        r = FSPXI_GetFileSize(g_pxi, fh, &size);
        if (R_SUCCEEDED(r) && size >= 2 * HDR && size <= 0x80000)
        {
            file.assign((size_t)size, 0);
            u32 total = 0;
            while (total < size)
            {
                u32 got = 0;
                r = FSPXI_ReadFile(g_pxi, fh, &got, total, file.data() + total, (u32)size - total);
                if (R_FAILED(r) || got == 0)
                    break;
                total += got;
            }
            if (total == size)
            {
                u32 h1, h2;
                bool has1, has2;
                if (findHeaders(file, h1, h2, has1, has2, why))
                {
                    bool v1 = has1 && cmacOk(fh, file, h1);
                    bool v2 = has2 && cmacOk(fh, file, h2);
                    pick    = choose(file, h1, h2, has1, has2, v1, v2);
                    snprintf(buf, sizeof(buf), "slot 1 %s, slot 2 %s", has1 ? (v1 ? "valid" : "BAD") : "none",
                        has2 ? (v2 ? "valid" : "BAD") : "none");
                    if (pick.slot)
                    {
                        why = buf;
                        ok  = true;
                    }
                    else
                        why = pick.why + " (" + buf + ")";
                }
            }
            else
            {
                snprintf(buf, sizeof(buf), "read %08lx, %lu of %llu bytes", (unsigned long)r, (unsigned long)total,
                    (unsigned long long)size);
                why = buf;
            }
        }
        else
        {
            snprintf(buf, sizeof(buf), "size %08lx, %llu bytes", (unsigned long)r, (unsigned long long)size);
            why = buf;
        }
        FSPXI_CloseFile(g_pxi, fh);
        FSPXI_CloseArchive(g_pxi, arch);
        return ok;
    }

    bool write(u64 tid, const std::vector<u8>& next, u32 newHeaderOfs, const u8* save, u32 size, std::string& why)
    {
        if (!g_pxi && !init(why))
            return false;
        const u32 archPath[4] = {(u32)tid, (u32)(tid >> 32), MEDIATYPE_SD, 0};
        FSPXI_Archive arch;
        char buf[96];
        Result r = FSPXI_OpenArchive(g_pxi, &arch, ARCHIVE_SAVEDATA_AND_CONTENT,
            {PATH_BINARY, sizeof(archPath), archPath});
        if (R_FAILED(r))
        {
            snprintf(buf, sizeof(buf), "write: open archive %08lx", (unsigned long)r);
            why = buf;
            return false;
        }
        static const u32 filePath[5] = {1, 1, 3, 0, 0};
        FSPXI_File fh;
        r = FSPXI_OpenFile(g_pxi, &fh, arch, {PATH_BINARY, sizeof(filePath), filePath},
            FS_OPEN_READ | FS_OPEN_WRITE, 0);
        if (R_FAILED(r))
        {
            FSPXI_CloseArchive(g_pxi, arch);
            snprintf(buf, sizeof(buf), "write: open save %08lx", (unsigned long)r);
            why = buf;
            return false;
        }
        bool ok = true;
        // 1. Header (CMAC still zero) and save of the new slot, in one write.
        u32 len = HDR + size, done = 0;
        r = FSPXI_WriteFile(g_pxi, fh, &done, newHeaderOfs, next.data() + newHeaderOfs, len, FS_WRITE_FLUSH);
        if (R_FAILED(r) || done != len)
        {
            snprintf(buf, sizeof(buf), "write: slot %08lx, %lu of %lu bytes", (unsigned long)r, (unsigned long)done,
                (unsigned long)len);
            why = buf;
            ok  = false;
        }
        // 2. Its CMAC.
        if (ok)
        {
            std::array<u8, 32> hash = slotHash(next, newHeaderOfs, size);
            std::array<u8, 16> now{}, prev{};
            int tries = 10;
            do
            {
                prev = now;
                r    = FSPXI_CalcSavegameMAC(g_pxi, fh, hash.data(), hash.size(), now.data(), now.size());
            } while (R_SUCCEEDED(r) && now != prev && --tries > 0);
            if (R_FAILED(r))
            {
                snprintf(buf, sizeof(buf), "write: CMAC %08lx", (unsigned long)r);
                why = buf;
                ok  = false;
            }
            else
            {
                r = FSPXI_WriteFile(g_pxi, fh, &done, newHeaderOfs + offsetof(Header, cmac), now.data(), 16,
                    FS_WRITE_FLUSH);
                if (R_FAILED(r) || done != 16)
                {
                    snprintf(buf, sizeof(buf), "write: CMAC write %08lx", (unsigned long)r);
                    why = buf;
                    ok  = false;
                }
            }
        }
        FSPXI_CloseFile(g_pxi, fh);
        FSPXI_CloseArchive(g_pxi, arch);
        if (!ok)
            return false;

        // 3. Read back: the 3DS must now pick the new slot, holding exactly `save`.
        std::vector<u8> back;
        Pick pick;
        std::string rwhy;
        if (!read(tid, back, pick, rwhy))
        {
            why = "write: read-back failed: " + rwhy;
            return false;
        }
        if (pick.headerOfs != newHeaderOfs || pick.saveSize != size ||
            memcmp(back.data() + pick.saveOfs, save, size) != 0)
        {
            snprintf(buf, sizeof(buf), "write: read-back picks slot %d (%s)", pick.slot, rwhy.c_str());
            why = buf;
            return false;
        }
        why = "slot " + std::to_string(pick.slot) + " written and checked (" + rwhy + ")";
        return true;
    }

    bool restore(u64 tid, const std::vector<u8>& file, std::string& why)
    {
        if (!g_pxi && !init(why))
            return false;
        const u32 archPath[4] = {(u32)tid, (u32)(tid >> 32), MEDIATYPE_SD, 0};
        FSPXI_Archive arch;
        char buf[96];
        Result r = FSPXI_OpenArchive(g_pxi, &arch, ARCHIVE_SAVEDATA_AND_CONTENT,
            {PATH_BINARY, sizeof(archPath), archPath});
        if (R_FAILED(r))
        {
            snprintf(buf, sizeof(buf), "restore: open archive %08lx", (unsigned long)r);
            why = buf;
            return false;
        }
        static const u32 filePath[5] = {1, 1, 3, 0, 0};
        FSPXI_File fh;
        r = FSPXI_OpenFile(g_pxi, &fh, arch, {PATH_BINARY, sizeof(filePath), filePath},
            FS_OPEN_READ | FS_OPEN_WRITE, 0);
        bool ok = false;
        if (R_SUCCEEDED(r))
        {
            u64 size = 0;
            FSPXI_GetFileSize(g_pxi, fh, &size);
            u32 done = 0;
            if (size != file.size())
            {
                snprintf(buf, sizeof(buf), "restore: the save is %llu bytes, the backup %lu",
                    (unsigned long long)size, (unsigned long)file.size());
                why = buf;
            }
            else
            {
                r  = FSPXI_WriteFile(g_pxi, fh, &done, 0, file.data(), (u32)file.size(), FS_WRITE_FLUSH);
                ok = R_SUCCEEDED(r) && done == file.size();
                if (!ok)
                {
                    snprintf(buf, sizeof(buf), "restore: write %08lx, %lu bytes", (unsigned long)r, (unsigned long)done);
                    why = buf;
                }
            }
            FSPXI_CloseFile(g_pxi, fh);
        }
        else
        {
            snprintf(buf, sizeof(buf), "restore: open save %08lx", (unsigned long)r);
            why = buf;
        }
        FSPXI_CloseArchive(g_pxi, arch);
        if (!ok)
            return false;
        std::vector<u8> back;
        Pick pick;
        std::string rwhy;
        if (!read(tid, back, pick, rwhy) || back != file)
        {
            why = "restore: read-back did not match (" + rwhy + ")";
            return false;
        }
        why = "restored and checked (" + rwhy + ")";
        return true;
    }
#endif
}
