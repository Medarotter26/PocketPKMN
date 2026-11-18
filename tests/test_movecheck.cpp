// PC test of the version guard (v0.89, docs/MOVE_RULES_PLAN.md V1; owner: "Yes, build part 1"): source/movecheck.cpp,
// the same code the app runs in withdrawOne. Uses the owner's own saves (hw/, not public).
//  1. planted: things the target game does not have are refused, with the right reason
//     (Poipole / Clangorous Soulblaze / an Ultra Sun-only item -> Moon; Origin Giratina / a Platinum-only item ->
//     Diamond); the same Pokemon is accepted by the game that has it;
//  2. no false refusals: every Pokemon in every box of each save may go back into that same game;
//  3. across versions of one generation (Moon <-> Ultra Sun, Diamond <-> Platinum, Crystal <-> Silver, X <-> Alpha
//     Sapphire): every refusal is listed with its reason, for a person to look at (not a failure by itself).
// Usage: bash tests/run_movecheck.sh
#include "gbavc.hpp"
#include "movecheck.hpp"
#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "utils/i18n.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using Raw = std::vector<u8>;
static int failures = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static Raw readFile(const std::string& path)
{
    Raw out;
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp)
        return out;
    int ch;
    while ((ch = fgetc(fp)) != EOF)
        out.push_back((u8)ch);
    fclose(fp);
    return out;
}

static std::unique_ptr<pksm::Sav> load(const std::string& path)
{
    Raw bytes = readFile(path);
    if (path.size() > 6 && path.substr(path.size() - 6) == ".gbavc") // the 3DS GBA container: its current slot
    {
        using namespace walkerbox::gbavc;
        u32 h1, h2;
        bool has1, has2;
        std::string why;
        if (!findHeaders(bytes, h1, h2, has1, has2, why))
            return nullptr;
        Pick p = choose(bytes, h1, h2, has1, has2, true, true);
        if (!p.slot)
            return nullptr;
        bytes = Raw(bytes.begin() + p.saveOfs, bytes.begin() + p.saveOfs + p.saveSize);
    }
    if (bytes.empty())
        return nullptr;
    std::shared_ptr<u8[]> d(new u8[bytes.size()]);
    memcpy(d.get(), bytes.data(), bytes.size());
    try
    {
        return pksm::Sav::getSave(d, bytes.size());
    }
    catch (...)
    {
        return nullptr;
    }
}

static std::unique_ptr<pksm::PKX> firstMon(const pksm::Sav& s)
{
    for (int b = 0; b < s.maxBoxes(); b++)
        for (int i = 0; i < s.maxSlot() / s.maxBoxes(); i++)
        {
            auto pk = s.pkm((u8)b, (u8)i);
            if (pk && pk->species() != pksm::Species::None && !pk->egg())
                return pk;
        }
    return nullptr;
}

// every boxed Pokemon of `from` checked against `to`; returns the number refused (listed when `list`)
static int sweep(const pksm::Sav& from, const pksm::Sav& to, bool list)
{
    int n = 0, bad = 0;
    for (int b = 0; b < from.maxBoxes(); b++)
        for (int i = 0; i < from.maxSlot() / from.maxBoxes(); i++)
        {
            auto pk = from.pkm((u8)b, (u8)i);
            if (!pk || pk->species() == pksm::Species::None)
                continue;
            n++;
            std::string why = walkerbox::moveCheck(to, *pk);
            if (!why.empty())
            {
                bad++;
                if (list)
                    printf("    box %d slot %d %s: %s\n", b + 1, i + 1,
                        i18n::species(pksm::Language::ENG, pk->species()).c_str(), why.c_str());
            }
        }
    printf("  %d Pokemon, %d refused\n", n, bad);
    return bad;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string hw = argv[1];
    i18n::init(pksm::Language::ENG);
    struct Named
    {
        const char* label;
        std::string path;
        std::unique_ptr<pksm::Sav> sav;
    } saves[] = {
        {"Moon", hw + "/gen7/moon_before_160900.main", nullptr},
        {"Ultra Sun", hw + "/gen7/usun_20261121.main", nullptr},
        {"Diamond", hw + "/gen4/d_before_190400.sav", nullptr},
        {"Platinum", hw + "/gen4/pt_before_190651.sav", nullptr},
        {"Crystal", hw + "/gen2/0004000000172800-20261118-182625.sav", nullptr},
        {"Silver", hw + "/gen2/0004000000172700-20261118-182627.sav", nullptr},
        {"Blue", hw + "/storage_s2/0004000000171100-20261118-164615.sav", nullptr},
        {"X", hw + "/gen6/0004000000055d00-20261118-203228.main", nullptr},
        {"Alpha Sapphire", hw + "/gen6/000400000011c500-20261118-203146.main", nullptr},
        {"Emerald", hw + "/gen3/0004000003ea7a00-20261118-200606.gbavc", nullptr},
    };
    for (auto& s : saves)
    {
        s.sav = load(s.path);
        CHECK(s.sav != nullptr, "%s: %s not loaded", s.label, s.path.c_str());
        if (!s.sav)
            return 1;
    }
    auto& moon = *saves[0].sav;
    auto& usun = *saves[1].sav;
    auto& dia  = *saves[2].sav;
    auto& plat = *saves[3].sav;
    using pksm::Species;

    // 1. planted ---------------------------------------------------------------------------------------------
    printf("planted:\n");
    {
        auto pk = firstMon(usun);
        CHECK(pk != nullptr, "no Pokemon in Ultra Sun");
        if (!pk)
            return 1;
        CHECK(walkerbox::moveCheck(moon, *pk).empty(), "Ultra Sun's %s refused by Moon: %s",
            i18n::species(pksm::Language::ENG, pk->species()).c_str(), walkerbox::moveCheck(moon, *pk).c_str());
        auto p = pk->clone();
        p->species(Species::Poipole);
        p->alternativeForm(0);
        std::string w = walkerbox::moveCheck(moon, *p);
        printf("  Poipole -> Moon: \"%s\"\n", w.c_str());
        CHECK(w == "Poipole", "Poipole -> Moon gave \"%s\"", w.c_str());
        CHECK(walkerbox::moveCheck(usun, *p).empty(), "Poipole -> Ultra Sun refused: %s",
            walkerbox::moveCheck(usun, *p).c_str());

        // owner: "what about poipole's evolution? Zeorora? The other two ultea beats form? Ultra necrozma?" - every
        // species, form and learnable move new in Ultra Sun / Ultra Moon (not the Z-moves: no Pokemon knows those)
        for (u16 sn : {804, 805, 806, 807}) // Naganadel, Stakataka, Blacephalon, Zeraora
        {
            auto q = pk->clone();
            pksm::Species s{sn};
            q->species(s);
            q->alternativeForm(0);
            std::string name = i18n::species(pksm::Language::ENG, s);
            w = walkerbox::moveCheck(moon, *q);
            printf("  %s -> Moon: \"%s\"\n", name.c_str(), w.c_str());
            CHECK(w == name, "%s -> Moon gave \"%s\"", name.c_str(), w.c_str());
            CHECK(walkerbox::moveCheck(usun, *q).empty(), "%s -> Ultra Sun refused", name.c_str());
        }
        for (int form = 1; form <= 3; form++) // Dusk Mane, Dawn Wings, Ultra (battle only)
        {
            auto q = pk->clone();
            q->species(Species::Necrozma);
            q->alternativeForm((u16)form);
            w = walkerbox::moveCheck(moon, *q);
            printf("  Necrozma form %d -> Moon: \"%s\"; -> Ultra Sun: \"%s\"\n", form, w.c_str(),
                walkerbox::moveCheck(usun, *q).c_str());
            CHECK(w == "this form of Necrozma", "Necrozma form %d -> Moon gave \"%s\"", form, w.c_str());
            if (form <= 2)
                CHECK(walkerbox::moveCheck(usun, *q).empty(), "Necrozma form %d -> Ultra Sun refused", form);
        }
        {
            auto q = pk->clone();
            q->species(Species::Lycanroc);
            q->alternativeForm(2); // Dusk Form
            w = walkerbox::moveCheck(moon, *q);
            printf("  Dusk Lycanroc -> Moon: \"%s\"\n", w.c_str());
            CHECK(w == "this form of Lycanroc", "Dusk Lycanroc -> Moon gave \"%s\"", w.c_str());
            CHECK(walkerbox::moveCheck(usun, *q).empty(), "Dusk Lycanroc -> Ultra Sun refused");
        }
        for (u16 mn : {720, 721, 722}) // Mind Blown, Plasma Fists, Photon Geyser
        {
            auto m = pk->clone();
            pksm::Move mv{mn};
            m->move(1, mv);
            std::string name = "the move " + i18n::move(pksm::Language::ENG, mv);
            w = walkerbox::moveCheck(moon, *m);
            printf("  %s -> Moon: \"%s\"\n", name.c_str(), w.c_str());
            CHECK(w == name, "%s -> Moon gave \"%s\"", name.c_str(), w.c_str());
            CHECK(walkerbox::moveCheck(usun, *m).empty(), "%s -> Ultra Sun refused: %s", name.c_str(),
                walkerbox::moveCheck(usun, *m).c_str());
        }
        {
            auto m = pk->clone();
            m->move(1, pksm::Move::TenMillionVoltThunderbolt); // Sun/Moon's last move: allowed
            CHECK(walkerbox::moveCheck(moon, *m).empty(), "10,000,000 Volt Thunderbolt -> Moon refused: %s",
                walkerbox::moveCheck(moon, *m).c_str());
        }

        auto it = pk->clone();
        it->heldItem((u16)930); // above Sun/Moon's last item (920), inside Ultra Sun/Ultra Moon's (959)
        w = walkerbox::moveCheck(moon, *it);
        printf("  item 930 -> Moon: \"%s\"\n", w.c_str());
        CHECK(w.find("its held item") == 0, "item -> Moon gave \"%s\"", w.c_str());
        CHECK(walkerbox::moveCheck(usun, *it).empty(), "item -> Ultra Sun refused: %s", walkerbox::moveCheck(usun, *it).c_str());
    }
    {
        auto pk = firstMon(plat);
        CHECK(pk != nullptr, "no Pokemon in Platinum");
        if (!pk)
            return 1;
        auto g = pk->clone();
        g->species(Species::Giratina);
        g->alternativeForm(1); // Origin Forme
        std::string w = walkerbox::moveCheck(dia, *g);
        printf("  Origin Giratina -> Diamond: \"%s\"\n", w.c_str());
        CHECK(w == "this form of Giratina", "Origin Giratina -> Diamond gave \"%s\"", w.c_str());
        CHECK(walkerbox::moveCheck(plat, *g).empty(), "Origin Giratina -> Platinum refused: %s",
            walkerbox::moveCheck(plat, *g).c_str());
        // owner: "a Pokemon holding the gitrious orb being sent to Diamond" (item 112: inside Diamond's range)
        auto o = pk->clone();
        o->species(Species::Giratina);
        o->alternativeForm(0);
        o->heldItem((u16)112);
        w = walkerbox::moveCheck(dia, *o);
        printf("  Giratina holding item 112 -> Diamond: \"%s\"\n", w.c_str());
        CHECK(w == "its held item (Griseous Orb)", "Griseous Orb -> Diamond gave \"%s\"", w.c_str());
        CHECK(walkerbox::moveCheck(plat, *o).empty(), "Griseous Orb -> Platinum refused: %s",
            walkerbox::moveCheck(plat, *o).c_str());
        auto it = pk->clone();
        it->heldItem((u16)466); // above Diamond/Pearl's last item (464), inside Platinum's (467)
        w = walkerbox::moveCheck(dia, *it);
        printf("  item 466 -> Diamond: \"%s\"\n", w.c_str());
        CHECK(w.find("its held item") == 0, "item -> Diamond gave \"%s\"", w.c_str());
        CHECK(walkerbox::moveCheck(plat, *it).empty(), "item -> Platinum refused: %s", walkerbox::moveCheck(plat, *it).c_str());
    }

    {
        auto& xs = *saves[7].sav;
        auto& as = *saves[8].sav;
        auto pk  = firstMon(as);
        CHECK(pk != nullptr, "no Pokemon in Alpha Sapphire");
        if (!pk)
            return 1;
        for (u16 item : {534, 535}) // Red Orb, Blue Orb: inside X/Y's item range, but only in Omega Ruby / Alpha Sapphire
        {
            auto q = pk->clone();
            q->heldItem(item);
            std::string w = walkerbox::moveCheck(xs, *q);
            printf("  item %u -> X: \"%s\"\n", item, w.c_str());
            CHECK(w.find("its held item") == 0, "item %u -> X gave \"%s\"", item, w.c_str());
            CHECK(walkerbox::moveCheck(as, *q).empty(), "item %u -> Alpha Sapphire refused", item);
        }
    }

    // 2. no false refusals: every boxed Pokemon back into its own game --------------------------------------
    printf("own game (must be 0 refused):\n");
    for (auto& s : saves)
    {
        printf(" %s:", s.label);
        int bad = sweep(*s.sav, *s.sav, true);
        CHECK(bad == 0, "%s: %d of its own Pokemon refused by its own game", s.label, bad);
    }

    // 3. across versions (for a person to read) -------------------------------------------------------------
    printf("across versions (listed, not failures):\n");
    const int pairs[][2] = {{1, 0}, {0, 1}, {3, 2}, {2, 3}, {4, 5}, {5, 4}, {7, 8}, {8, 7}};
    for (auto& p : pairs)
    {
        printf(" %s -> %s:", saves[p[0]].label, saves[p[1]].label);
        sweep(*saves[p[0]].sav, *saves[p[1]].sav, true);
    }

    printf("version guard: %d checks, %d failure(s)\n", checks, failures);
    printf(failures ? "MOVECHECK TESTS: %d FAILED\n" : "movecheck tests passed (%d failures)\n", failures);
    return failures != 0;
}
