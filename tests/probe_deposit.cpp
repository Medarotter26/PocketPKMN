// Probe: replay a deposit exactly as the app does it (slot given on the command
// line) and report what gameMatchFinish puts back and whether the Pokemon left.
// Usage: probe_deposit <save> <box> <slot>   (box/slot 1-based)
#include "gamematch.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "enums/Species.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 4)
        return 2;
    FILE* f = fopen(argv[1], "rb");
    if (!f)
        return 1;
    std::vector<u8> orig;
    int c;
    while ((c = fgetc(f)) != EOF)
        orig.push_back((u8)c);
    fclose(f);
    int box = atoi(argv[2]) - 1, slot = atoi(argv[3]) - 1;
    std::shared_ptr<u8[]> d(new u8[orig.size()]);
    memcpy(d.get(), orig.data(), orig.size());
    auto sav = pksm::Sav::getSave(d, orig.size());
    printf("version %u, open box %d, editing box %d slot %d: species %u\n", (unsigned)sav->version(),
        sav->currentBox() + 1, box + 1, slot + 1, (unsigned)sav->pkm((u8)box, (u8)slot)->species());
    sav->pkm(*sav->emptyPkm(), (u8)box, (u8)slot, false);
    std::vector<u8> pre(sav->rawData().get(), sav->rawData().get() + orig.size());
    std::string note;
    unsigned put = walkerbox::gameMatchFinish(*sav, (uint32_t)orig.size(), box, note);
    printf("%s: %u byte(s) put back\n", note.c_str(), put);
    // Which bytes were put back (differ between the pre-finish buffer and the result)?
    const u8* now = sav->rawData().get();
    size_t s = 0;
    bool in = false;
    for (size_t i = 0; i <= orig.size(); i++)
    {
        bool dif = i < orig.size() && now[i] != pre[i];
        if (dif && !in) { s = i; in = true; }
        if (!dif && in) { printf("  result differs from the edit at 0x%04zx-0x%04zx (%zu)\n", s, i - 1, i - s); in = false; }
    }
    std::vector<u8> out(now, now + orig.size());
    std::shared_ptr<u8[]> d2(new u8[out.size()]);
    memcpy(d2.get(), out.data(), out.size());
    auto again = pksm::Sav::getSave(d2, out.size());
    printf("after: box %d slot %d species %u (was it removed? %s)\n", box + 1, slot + 1,
        (unsigned)again->pkm((u8)box, (u8)slot)->species(),
        again->pkm((u8)box, (u8)slot)->species() != sav->pkm((u8)box, (u8)slot)->species() ? "?" : "see list");
    int per = again->maxSlot() / again->maxBoxes(), n = 0;
    for (int i = 0; i < per; i++)
        if (again->pkm((u8)box, (u8)i)->species() != pksm::Species::None)
            n++;
    printf("box %d now holds %d Pokemon\n", box + 1, n);
    return 0;
}
