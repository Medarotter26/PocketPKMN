// test_walk_poke.cpp - every Pokemon in the user's saves (hw/) -> walk data (source/walk_poke.cpp).
// Usage: test_walk_poke <save>...  (.sav Gen 1/2, .gbavc Gen 3 container, .main Gen 6)
// Checks per Pokemon: summary species / level / moves / item match the PKX, nickname text decodes to the
// walker nickname, name image fits, eggs are refused; the result composes with the test route.
// v0.81 (names step 1, owner: "Go 12"): --names <romfs/i18n/> <names_eng_today.txt> also renders every
// species name of eng/fre/ger/spa/ita (species.txt, species2.txt, species4.txt) as the walker name picture: no
// character of those languages may be missing, every picture fits 80x16, the stored text stays the ASCII rule,
// and every English name whose picture text did not change gives the same picture bytes as before v0.81.
#include "gbavc.hpp"
#include "pw_proto.h"
#include "sav/Sav.hpp"
#include "utils/i18n.hpp"
#include "walk_compose.h"
#include "walk_poke.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "pw_image.h"

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
        // v0.81 (names step 1, owner: "Go 12"): the picture has its own text (accents, signs)
        std::string pic = walkerbox::walkerNicknamePicture(*pk);
        CHECK(!pic.empty());
        CHECK(wbx_label_image(pic.c_str(), 80, 16, img) == 0);
        CHECK(memcmp(img, wp.name_img, sizeof img) == 0);
        if (pic != want)
            printf("  picture \"%s\" / stored text \"%s\"\n", pic.c_str(), want.c_str());
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

// ---- v0.81 (names step 1, owner: "Go 12") ---------------------------------------------------------
static std::vector<std::string> readLines(const std::string& path)
{
    std::vector<std::string> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return v;
    char buf[512];
    while (fgets(buf, sizeof buf, f)) {
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        v.push_back(s);
    }
    fclose(f);
    return v;
}

static u32 fnv1a(const u8* p, size_t n)
{
    u32 h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x01000193u;
    return h;
}

// name without trailing spaces, cut to 12 characters (code points; v0.81, owner: "Go 12" - was 10) - what the
// picture must show
static std::string expectedPicture(const std::string& name)
{
    std::string s = name;
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    size_t chars = 0;
    for (size_t i = 0; i < s.size(); i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80 && chars++ == 12)
            return s.substr(0, i);
    return s;
}

static void testNames(const std::string& dir, const char* goldenPath)
{
    std::vector<std::string> eng = readLines(dir + "eng/species.txt");
    CHECK(eng.size() > 800);
    std::map<int, u32> golden;
    for (const std::string& l : readLines(goldenPath)) {
        int n;
        unsigned h;
        if (!l.empty() && l[0] != ';' && sscanf(l.c_str(), "%d %x", &n, &h) == 2)
            golden[n] = h;
    }
    CHECK(golden.size() == eng.size() - 1);
    const char* langs[] = {"eng", "fre", "ger", "spa", "ita"};
    const char* files[] = {"species.txt", "species2.txt", "species4.txt"};
    int total = 0, missing = 0, wide = 0, cut = 0, same = 0, changed = 0, widest = 0;
    std::string widestName;
    for (const char* lang : langs)
        for (const char* file : files) {
            std::vector<std::string> v = readLines(dir + lang + "/" + file);
            if (v.empty() && strcmp(lang, "eng") == 0 && strcmp(file, "species.txt") != 0)
                continue; // eng has species.txt only
            CHECK(v.size() > 250);
            for (size_t s = 1; s < v.size(); s++) {
                const std::string& name = v[s];
                if (name.empty())
                    continue;
                const std::string& engName = s < eng.size() ? eng[s] : name;
                std::string pic  = walkerbox::walkerNicknamePictureText(name, engName);
                std::string text = walkerbox::walkerNicknameText(name, engName);
                total++;
                // no character of these languages may be left out of the picture
                if (pic != expectedPicture(name)) {
                    missing++;
                    printf("  %s/%s #%zu \"%s\": picture \"%s\" (character missing)\n", lang, file, s, name.c_str(),
                           pic.c_str());
                }
                std::string trimmed = name;
                while (!trimmed.empty() && trimmed.back() == ' ')
                    trimmed.pop_back();
                if (expectedPicture(name) != trimmed)
                    cut++; // longer than 12 characters: cut
                // the stored Gen 4 text: the ASCII rule, nothing else
                uint16_t t[11];
                wbx_text_pack(text.c_str(), t, 11, 1);
                char back[16];
                pw_text_to_ascii(t, 11, back, sizeof back);
                CHECK(text == back);
                // the picture: 80x16 with the shadow, nothing on the top row (the accent zone stays inside)
                u8 img[WBX_NAME_IMG_SIZE], sh[80 * 16];
                if (wbx_label_image(pic.c_str(), 80, 16, img) != 0) {
                    wide++;
                    printf("  %s/%s #%zu \"%s\": wider than 80 px (%d)\n", lang, file, s, pic.c_str(),
                           pw_text_width(pic.c_str()));
                }
                if (pw_text_width(pic.c_str()) > widest) {
                    widest = pw_text_width(pic.c_str());
                    widestName = pic;
                }
                pw_img_decode(img, 80, 16, sh);
                for (int x = 0; x < 80; x++)
                    CHECK(sh[x] == 0);
                // English: unchanged picture text -> the exact bytes of before v0.81
                if (strcmp(lang, "eng") == 0 && strcmp(file, "species.txt") == 0) {
                    if (pic == text) {
                        same++;
                        CHECK(golden.count((int)s) && golden[(int)s] == fnv1a(img, sizeof img));
                    } else {
                        changed++;
                        printf("  English #%zu: picture \"%s\" (stored text \"%s\")\n", s, pic.c_str(), text.c_str());
                    }
                }
            }
        }
    printf("names: %d species names rendered, %d with a missing character, %d wider than 80 px, %d cut at 12 "
           "characters; widest %d px (\"%s\"); English: %d pictures byte-identical to before, %d changed (signs, "
           "punctuation, accents, 11-12 characters)\n", total, missing, wide, cut, widest, widestName.c_str(), same,
           changed);
    CHECK(missing == 0);
    CHECK(wide == 0);
    // Japanese / other scripts: the fallback is exactly the stored text (the English name, ASCII)
    CHECK(walkerbox::walkerNicknamePictureText("\xE3\x83\x94\xE3\x82\xAB\xE3\x83\x81\xE3\x83\xA5\xE3\x82\xA6",
                                               "Pikachu") == "Pikachu");
    CHECK(walkerbox::walkerNicknamePictureText("\xEB\x8B\x88\xEB\x93\x9C\xEB\x9F\xB0\xE2\x99\x82", "Nidoran\xE2\x99\x82")
          == "Nidoran");
    CHECK(walkerbox::walkerNicknamePictureText("\xE2\x99\x82\xE2\x99\x80", "Nidoran\xE2\x99\x82") == "Nidoran");
    // Gen 5+ signs as PKSM-Core gives them (U+E08E / U+E08F) are kept and drawn as the signs
    CHECK(walkerbox::walkerNicknamePictureText("Nidoran\xEE\x82\x8E", "Nidoran\xE2\x99\x82") == "Nidoran\xEE\x82\x8E");
    u8 a[WBX_NAME_IMG_SIZE], b[WBX_NAME_IMG_SIZE];
    wbx_label_image("Nidoran\xEE\x82\x8E", 80, 16, a);
    wbx_label_image("Nidoran\xE2\x99\x82", 80, 16, b);
    CHECK(memcmp(a, b, sizeof a) == 0);
}

int main(int argc, char** argv)
{
    i18n::init(pksm::Language::ENG);
    wbx_route_test(&g_route);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--names") && i + 2 < argc) {
            testNames(argv[i + 1], argv[i + 2]);
            i += 2;
            continue;
        }
        testSave(argv[i]);
    }
    printf("W4 gains: %d walks applied and checked\n", g_gainChecks);
    printf("%s (%d failure(s))\n", failures ? "FAILED" : "ALL WALK POKEMON CHECKS PASSED", failures);
    return failures != 0;
}
