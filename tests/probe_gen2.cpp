// Probe: where does PKSM-Core write when a Pokemon leaves the open box of a Gen 2
// save? Deposit the last Pokemon of the open box, run finishEditing (no
// game-matching), and list the changed byte ranges.
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

int main(int argc, char** argv)
{
    for (int a = 1; a < argc; a++)
    {
        FILE* f = fopen(argv[a], "rb");
        if (!f)
            continue;
        std::vector<u8> orig;
        int c;
        while ((c = fgetc(f)) != EOF)
            orig.push_back((u8)c);
        fclose(f);
        std::shared_ptr<u8[]> d(new u8[orig.size()]);
        memcpy(d.get(), orig.data(), orig.size());
        auto sav = pksm::Sav::getSave(d, orig.size());
        int open = sav->currentBox(), per = sav->maxSlot() / sav->maxBoxes(), last = -1;
        for (int i = 0; i < per; i++)
            if (sav->pkm((u8)open, (u8)i)->species() != pksm::Species::None)
                last = i;
        printf("== %s: version %u, open box %d, last filled slot %d\n", argv[a], (unsigned)sav->version(), open + 1,
            last + 1);
        sav->pkm(*sav->emptyPkm(), (u8)open, (u8)last, false);
        // before finishEditing
        std::vector<u8> mid(sav->rawData().get(), sav->rawData().get() + orig.size());
        sav->finishEditing();
        const u8* now = sav->rawData().get();
        auto ranges = [&](const u8* x, const char* label)
        {
            printf("  %s:\n", label);
            size_t s = 0;
            bool in  = false;
            for (size_t i = 0; i <= orig.size(); i++)
            {
                bool dif = i < orig.size() && x[i] != orig[i];
                if (dif && !in)
                {
                    s  = i;
                    in = true;
                }
                if (!dif && in)
                {
                    printf("    0x%04zx-0x%04zx (%zu)\n", s, i - 1, i - s);
                    in = false;
                }
            }
        };
        ranges(mid.data(), "changed by the edit (before finishEditing)");
        ranges(now, "changed after finishEditing");
    }
    return 0;
}
