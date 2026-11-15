// test_walk_poke.cpp - every Pokemon in the user's saves (hw/) -> walk data (source/walk_poke.cpp).
// Usage: test_walk_poke <save>...  (.sav Gen 1/2, .gbavc Gen 3 container, .main Gen 6)
// Checks per Pokemon: summary species / level / moves / item match the PKX, nickname text decodes to the
// walker nickname, name image fits, eggs are refused; the result composes with the test route.
#include "gbavc.hpp"
#include "pw_proto.h"
#include "sav/Sav.hpp"
#include "utils/i18n.hpp"
#include "walk_compose.h"
#include "walk_poke.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static std::vector<u8> readAll(const char* path)
{
    std::vector<u8> d;
    FILE* f = fopen(path, "rb");
    if (!f)
        return d;
    fseek(f, 0, SEEK_END);
    d.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(d.data(), 1, d.size(), f) != d.size())
        d.clear();
    fclose(f);
    return d;
}

static std::unique_ptr<pksm::Sav> load(const char* path)
{
    std::vector<u8> file = readAll(path);
    size_t ofs = 0, size = file.size();
    if (strstr(path, ".gbavc")) {
        using namespace walkerbox::gbavc;
        u32 h1, h2;
        bool has1, has2;
        std::string why;
        if (!findHeaders(file, h1, h2, has1, has2, why))
            return nullptr;
        Pick p = choose(file, h1, h2, has1, has2, true, true);
        ofs = p.saveOfs;
        size = p.saveSize;
    }
    std::shared_ptr<u8[]> d(new u8[size]);
    memcpy(d.get(), file.data() + ofs, size);
    return pksm::Sav::getSave(d, size);
}

static wbx_route g_route;
static int g_gainChecks = 0;

// W4 gains on a copy of pk: several step counts and EV choices; checks the rules and that the bytes re-read.
static void testGains(const pksm::PKX& orig, int gen)
{
    using namespace walkerbox;
    const u32 stepsList[] = {0, 19, 137, 4999, 5000, 250000};
    for (u32 steps : stepsList)
        for (int ev : {(int)EV_OFF, (int)EV_SPE, (int)EV_HP})
        {
            auto pk = orig.clone();
            WalkGains g;
            CHECK(applyWalkGains(*pk, gen, steps, ev, g));
            g_gainChecks++;
            // EXP / level: never more than one level; a big walk lands exactly at the next level's start
            CHECK(g.levelAfter == g.levelBefore || g.levelAfter == g.levelBefore + 1);
            CHECK(g.expAfter >= g.expBefore && g.expAfter - g.expBefore <= steps);
            if (g.levelBefore >= 100)
                CHECK(g.expAfter == g.expBefore);
            if (g.levelAfter == g.levelBefore + 1)
            {
                auto start = orig.clone();
                start->level(g.levelAfter);
                CHECK(g.expAfter == start->experience());
            }
            else
                CHECK(g.expAfter == g.expBefore + (g.levelBefore >= 100 ? 0 : steps));
            // friendship
            if (gen >= 2)
            {
                int f = g.friendBefore, want = f;
                if (g.levelAfter > g.levelBefore)
                {
                    int dl = f < 100 ? 5 : f < 200 ? 3 : 2;
                    if (gen >= 3 && pk->ball() == pksm::Ball::Luxury)
                        dl++;
                    if (pk->heldItem() == 218)
                        dl = dl * 150 / 100;
                    want = std::min(255, f + dl);
                }
                if (steps >= 5000 && want < 255)
                    want++;
                CHECK(g.friendAfter == want);
                CHECK(pk->currentFriendship() == want);
            }
            // EVs
            u32 total = 0;
            for (auto s : {pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPD, pksm::Stat::SPATK,
                     pksm::Stat::SPDEF})
            {
                total += pk->ev(s);
                if (gen <= 2)
                    CHECK(pk->ev(s) == orig.ev(s));
            }
            if (gen >= 3)
            {
                CHECK(total <= 510);
                if (ev >= 0)
                {
                    CHECK(g.evAfter <= walkEvCap(gen));
                    CHECK(g.evAfter - g.evBefore <= steps / 20);
                }
            }
            // the changed bytes read back as the same Pokemon with the same values
            auto back = pksm::PKX::getPKM(orig.generation(), pk->rawData().data(), pk->getLength());
            CHECK(back && back->species() == orig.species() && back->level() == g.levelAfter &&
                  back->experience() == g.expAfter);
            if (gen >= 2)
                CHECK(back->currentFriendship() == g.friendAfter);
            if (back && orig.PID() != back->PID())
                CHECK(false);
        }
}

static void testSave(const char* path)
{
    auto sav = load(path);
    printf("%s\n", path);
    CHECK(sav != nullptr);
    if (!sav)
        return;
    int gen = sav->generation() == pksm::Generation::ONE     ? 1
            : sav->generation() == pksm::Generation::TWO     ? 2
            : sav->generation() == pksm::Generation::THREE   ? 3
            : sav->generation() == pksm::Generation::SIX     ? 6
            : 7;
    int ok = 0, eggs = 0, shown = 0, placeholders = 0;
    std::vector<std::unique_ptr<pksm::PKX>> all;
    for (int s = 0; s < sav->partyCount(); s++)
        all.push_back(sav->pkm(s));
    for (int b = 0; b < sav->maxBoxes(); b++)
        for (int s = 0; s < sav->maxSlot() / sav->maxBoxes(); s++)
            all.push_back(sav->pkm(b, s));
    for (auto& pk : all) {
        if (!pk || u16(pk->species()) == 0)
            continue;
        wbx_walk_poke wp;
        bool fromPack = false;
        std::string why;
        bool r = walkerbox::walkPokeFromPkx(*pk, gen, wp, fromPack, why);
        if (pk->egg()) {
            CHECK(!r);
            eggs++;
            continue;
        }
        CHECK(r);
        if (!r) {
            printf("  refused: %s\n", why.c_str());
            continue;
        }
        pw_poke_summary s;
        pw_poke_summary_parse(wp.summary, &s);
        CHECK(s.species == u16(pk->species()));
        CHECK(s.level == pk->level());
        for (int i = 0; i < 4; i++)
            CHECK(s.moves[i] == u16(pk->move(i)));
        CHECK(s.held_item == (gen == 1 ? 0 : pk->heldItem()));
        char nick[16];
        pw_text_to_ascii(wp.nickname, 11, nick, sizeof nick);
        std::string want = walkerbox::walkerNickname(*pk);
        CHECK(want == nick);
        CHECK(!want.empty());
        CHECK(wp.nickname[10] == 0xFFFF);
        u8 img[WBX_NAME_IMG_SIZE];
        CHECK(wbx_label_image(want.c_str(), 80, 16, img) == 0);
        CHECK(memcmp(img, wp.name_img, sizeof img) == 0);
        placeholders += !fromPack;
        testGains(*pk, gen);
        u8 walk[WBX_WALK_SIZE];
        wbx_walk_compose(nullptr, &wp, &g_route, walk);
        CHECK(memcmp(walk, wp.summary, 16) == 0 && walk[0x26] == wp.friendship);
        if (shown < 4) {
            printf("  #%u Lv %u \"%s\" (save: \"%s\") item %u moves %u %u %u %u variant %02x flags %02x friendship %u\n",
                   s.species, s.level, nick, pk->nickname().c_str(), s.held_item, s.moves[0], s.moves[1],
                   s.moves[2], s.moves[3], s.variant, s.flags, wp.friendship);
            shown++;
        }
        ok++;
    }
    printf("  %d Pokemon -> walk data, %d egg(s) refused, %d placeholder sprite(s)\n", ok, eggs, placeholders);
    CHECK(placeholders == 0);
}

int main(int argc, char** argv)
{
    i18n::init(pksm::Language::ENG);
    wbx_route_test(&g_route);
    for (int i = 1; i < argc; i++)
        testSave(argv[i]);
    printf("W4 gains: %d walks applied and checked\n", g_gainChecks);
    printf("%s (%d failure(s))\n", failures ? "FAILED" : "ALL WALK POKEMON CHECKS PASSED", failures);
    return failures != 0;
}
