#!/usr/bin/env python3
"""Builds the Pokemon FireRed / LeafGreen route file from the user's FRLG course workbook.

usage: python3 -I tools/routes_frlg.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/FRLG Pokewalker Courses.xlsx (env), else ../walker courses/... (read only)
  out       default: romfs/routes/frlg.txt

v0.94 (2026-11-17, owner: "can you add the frlg courses?", "could you make one and test it all on your end?"; plan
docs/FRLG_COURSES_PLAN.md). The first Gen 3 courses. Same file format as tools/routes_oras.py: Pokemon lines carry
form=0, origin=FR,LG (FR or LG for the version exclusives, from the sheet's "Origin game to set") and ha=0 (Gen 3
has no Hidden Abilities). Unlocks: badges=N, hof, hof watts=N. Moves: the last 4 level-up moves at that level in
FireRed / LeafGreen (PokeAPI). Items: the walker's (Gen 4) item numbers, as every route file; the app turns them
into Gen 3 item numbers when it puts them in the bag. The workbook is only read.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import routes_xy as rx  # noqa: E402  (NameList, name_width, wild_moves, limits)
import routes_sm as rs  # noqa: E402  (load_learnsets)

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")),
                            "FRLG Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "frlg.txt")

# "Champion's Bazaar" is 81 px (limit 80) -> "Elite Bazaar", as X/Y and Gen 7 did (plan question 3, default)
# "Underground Subway" is 91 px -> "Underground" (default, the owner can pick another name)
COURSE_RENAMES = {"Champion's Bazaar": "Elite Bazaar", "Underground Subway": "Underground"}
ORIGINS = [("Walker's own game", "FR,LG"), ("FireRed or LeafGreen", "FR,LG"), ("FireRed", "FR"), ("LeafGreen", "LG")]
# v0.97 (owner: "add wyrm peak to frlg too"): (workbook in the same folder, course names) taken over as they are
# 2026-11-17: the owner's FRLG workbook now has Wyrm Peak itself ("update the spreadsheets too") - nothing extra
EXTRA_COURSES = []
# Gen 3 spellings in the workbook -> the app's (Gen 4+) item names
ITEM_NAMES = {
    "BlackGlasses": "Black Glasses", "BrightPowder": "Bright Powder", "Parlyz Heal": "Paralyze Heal",
    "SilverPowder": "Silver Powder", "Thunderstone": "Thunder Stone", "TinyMushroom": "Tiny Mushroom",
    "TwistedSpoon": "Twisted Spoon", "Never-Melt Ice": "Never-Melt Ice", "Poke Ball": "Poké Ball",
}
SPECIES_NAMES = {"Nidoran F": "Nidoran♀", "Nidoran M": "Nidoran♂"}
# Ditto (Shopping District, Lv 8 in the workbook) has no legal FireRed / LeafGreen origin below Lv 23 (PKHeX: the
# lowest wild Ditto is Lv 23 on Route 14) -> Lv 23 until the owner decides (question in the report)
LEVEL_OVERRIDES = {("Shopping District", "Ditto"): 23}
# Course pictures: STAND-INS (the closest existing course picture); drawn FRLG pictures later
# v0.97 (owner: "you need to create sprites for the frlg courses"): each course has its own picture (tools/
# route_art_frlg.py); the five that share a name with an X / Y / Gen 7 course share its picture.
PICTURES = {
    "Field": "field", "Shopping District": "shopping_district", "Forest": "forest",
    "Rocky Foothills": "rocky_foothills", "Riverside": "riverside", "Clear Lake": "clear_lake",
    "Underground": "underground", "Residential Zone": "residential_zone", "Hobby Town": "hobby_town",
    "Beach": "beach", "Freight Yard": "freight_yard", "Volcano Peak": "volcano_peak",
    "Safari Grounds": "safari_grounds", "Mountains": "mountains", "Starter's Lane": "starters_lane",
    "Island Shores": "island_shores", "Twilight Grove": "twilight_grove", "Trader's Crossing": "traders_crossing",
    "Elite Bazaar": "elite_bazaar", "Wyrm Peak": "wyrm_peak",
}
GEN3_LAST_ITEM = 376  # items up to here exist in Gen 3 (national item numbers; the app checks each one too)
MAX_SPECIES = 386


def parse_unlock(text):
    t = str(text).strip()
    if t == "Start":
        return "start"
    if t.startswith("Badge "):
        return "badges=%d" % int(t[6:])
    if t == "Hall of Fame":
        return "hof"
    if t.startswith("Hall of Fame + ") and t.endswith(" W total"):
        return "hof watts=%d" % int(t[15:-8].replace(",", ""))
    raise SystemExit("unknown unlock: %r" % t)


def parse_origin(text):
    t = str(text).strip()
    for start, games in ORIGINS:
        if t.startswith(start):
            return games
    raise SystemExit("unknown origin game: %r" % t)


def main():
    book = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BOOK
    out_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    import openpyxl
    wb = openpyxl.load_workbook(book, read_only=True, data_only=True)

    species, moves, items = rx.NameList("species"), rx.NameList("moves"), rx.NameList("items")
    learnsets = rs.load_learnsets("firered-leafgreen")

    courses, by_name = [], {}
    # v0.97 (owner: "add wyrm peak to frlg too"): courses taken from another workbook, after the FRLG ones (read only)
    books = [(wb, None)]
    for other, names in EXTRA_COURSES:
        books.append((openpyxl.load_workbook(os.path.join(os.path.dirname(book), other), read_only=True,
                                             data_only=True), set(names)))
    for bk, only in books:
        read_book(bk, only, courses, by_name)
    _read_rest(courses, by_name, species, moves, items, learnsets, out_path)


def read_book(wb, only, courses, by_name):
    """Adds a workbook's courses (only the names in `only`, if given) with their Pokemon and items."""
    for r in wb["Courses"].iter_rows(min_row=2, values_only=True):
        if not r or r[0] is None or (only is not None and str(r[0]).strip() not in only):
            continue
        cname = COURSE_RENAMES.get(str(r[0]).strip(), str(r[0]).strip())
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        grp = str(r[3]).strip()
        pname = str(r[4]).strip()
        if not isinstance(r[5], (int, float)):
            raise SystemExit("%s %s: no level" % (cname, pname))
        level = LEVEL_OVERRIDES.get((cname, pname), int(r[5]))
        by_name[cname][3].append((grp, pname, level, int(r[6]), parse_origin(r[10])))

    irows = list(wb["Items"].iter_rows(values_only=True))
    hdr = next(i for i, r in enumerate(irows) if r and r[0] == "Course")
    for r in irows[hdr + 1:]:
        if not r or r[0] is None or (only is not None and str(r[0]).strip() not in only):
            continue
        cname = COURSE_RENAMES.get(str(r[0]).strip(), str(r[0]).strip())
        if cname not in by_name:
            raise SystemExit("Items sheet course not in Courses sheet: %r" % r[0])
        by_name[cname][4].append((int(r[2]), str(r[3]).strip()))


def _read_rest(courses, by_name, species, moves, items, learnsets, out_path):
    unresolved, problems = [], []
    lines = [
        "; PocketPKMN routes for Pokemon FireRed / LeafGreen - generated by tools/routes_frlg.py from the user's "
        "workbook (do not edit by hand)",
        "game fr lg",
    ]
    for n, (cname, unlock, fav, pokes, its) in enumerate(courses):
        if rx.name_width(cname) > rx.WIDTH_LIMIT["course"]:
            problems.append("course name too wide: %s (%d px)" % (cname, rx.name_width(cname)))
        lines.append("")
        if cname not in PICTURES:
            raise SystemExit("no picture for course %r" % cname)
        if not os.path.exists(os.path.join(ROOT, "romfs", "routes", "pictures", PICTURES[cname] + ".txt")):
            raise SystemExit("picture file missing: %s" % PICTURES[cname])
        lines.append("course %d | %s | unlock=%s | picture=%s | favoured=%s" % (n, cname, unlock, PICTURES[cname], fav))
        for grp in ("A", "B", "C"):
            members = [p for p in pokes if p[0] == grp]
            if len(members) != 2:
                raise SystemExit("%s group %s has %d Pokemon" % (cname, grp, len(members)))
            slot, chance = rx.GROUP_SLOT[grp]
            for _, sheet_name, level, steps, origin in members:
                if grp == "A":  # user 2026-11-15: group A's minimum is always 0
                    steps = 0
                sid = species.find(SPECIES_NAMES.get(sheet_name, sheet_name))
                if sid is None or sid > MAX_SPECIES:
                    unresolved.append("species: " + sheet_name)
                    continue
                sname = species.names[sid]
                if rx.name_width(sname) > rx.WIDTH_LIMIT["poke"]:
                    problems.append("Pokemon name too wide: %s" % sname)
                mv = rx.wild_moves(learnsets.get(sid, []), level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)" % (sheet_name, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                lines.append("poke %s %d %d %d %d %d %d %d %d %d form=0 origin=%s ha=0 ; %s: %s"
                             % (grp, slot, chance, sid, level, steps, *mv, origin, sname, mnames))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            real = ITEM_NAMES.get(iname, iname)
            iid = items.find(real)
            if iid is None or iid > GEN3_LAST_ITEM:
                unresolved.append("item: %s (%s)" % (real, iid))
                continue
            if rx.name_width(items.names[iid]) > rx.WIDTH_LIMIT["item"]:
                problems.append("item name too wide: %s (%d px)" % (items.names[iid], rx.name_width(items.names[iid])))
            lines.append("item %d %d %d %d all ; %s" % (rank, rx.ITEM_CHANCES[rank], iid, steps, items.names[iid]))

    if unresolved:
        print("UNRESOLVED NAMES (nothing written):")
        for u in sorted(set(unresolved)):
            print("  " + u)
        sys.exit(1)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s: %d courses" % (out_path, len(courses)))
    for p in problems:
        print("note: " + p)


if __name__ == "__main__":
    main()
