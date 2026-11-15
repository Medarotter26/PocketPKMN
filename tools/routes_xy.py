#!/usr/bin/env python3
"""Builds the Pokemon X / Y route file from the user's course workbook.

usage: python3 -I tools/routes_xy.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/XY Pokewalker Courses.xlsx (env), else ../walker courses/XY Pokewalker Courses.xlsx next to the project folder (read only)
  out       default: romfs/routes/xy.txt

Reads the "Courses" and "Items" sheets, resolves every name against the
project's name lists (romfs/i18n/eng/species.txt, moves.txt, items.txt),
gives each route Pokemon the last 4 moves it learns by level-up in X/Y
(art_src/pokeapi/pokemon_moves.csv + version_groups.csv) and writes one
deterministic text file. Stops with a list if any name does not resolve.
"""
import csv
import os
import sys
import unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pwimg  # noqa: E402

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")), "XY Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "xy.txt")
I18N = os.path.join(ROOT, "romfs", "i18n", "eng")
POKEAPI = os.path.join(ROOT, "art_src", "pokeapi")

# The user's decisions -------------------------------------------------------
COURSE_RENAMES = {
    "Abandoned Factory": "Rusted Works",
    "Ancient Battlefield": "Old Battlefield",
    "Champion's Bazaar": "Elite Bazaar",
}
# One picture per course (user 2026-11-15: "do those route pictures actually match our routes though?"):
# art/routes/<slug>.txt, drawn by tools/route_art.py. slug = lower case, apostrophes dropped, spaces -> "_".
COURSE_NAMES = ["Quiet Outskirts", "Sunlit Woods", "Cafe Boulevard", "Shopping District", "Palace Riverside",
                "Dojo Grounds", "Seashell Coast", "Crystal Cliffs", "Rusted Works", "Shadowed Thicket", "Frost Ridge",
                "Grand Estate", "Starter's Lane", "Fossil Dig Site", "Old Battlefield", "Trader's Crossing",
                "Wyrm Hollow", "Elite Bazaar"]
PICTURES = {n: n.lower().replace("'", "").replace("’", "").replace(" ", "_") for n in COURSE_NAMES}
# (course, workbook Pokemon) -> (new Pokemon, new level), for legality (user 2026-11-15): a Lv 23 Politoed cannot
# exist (Poliwag evolves at 25) -> "Slowpoke"; Ditto is Friend-Safari-only at Lv 30 -> "Kecleon at Lv 11".
POKE_SWAPS = {
    ("Palace Riverside", "Politoed"): ("Slowpoke", 23),
    ("Shopping District", "Ditto"): ("Kecleon", 11),
}
# workbook item text -> list of (name, version)
ITEM_CHOICES = {
    "Power Bracer (or another Power item)": [("Power Bracer", "all")],
    "Choice Band or Choice Specs": [("Choice Band", "all")],
    "Charizardite X or Y": [("Charizardite X", "x"), ("Charizardite Y", "y")],
}
# old / workbook spelling -> name in the project's lists (after these, the
# match ignores case, accents, spaces, hyphens and apostrophes)
ALIASES = {
    "items": {
        "Parlyz Heal": "Paralyze Heal",
        "X Defend": "X Defense",
        "X Special": "X Sp. Atk",
    },
    "species": {},
    "moves": {},
}
# group -> (walker slot, chance %), copied from HeartGold/SoulSilver's pattern
GROUP_SLOT = {"C": (0, 50), "B": (1, 75), "A": (2, 100)}
ITEM_CHANCES = [20, 20, 30, 30, 30, 40, 50, 50, 50, 100]
WIDTH_LIMIT = {"course": 80, "item": 96, "poke": 80}
MAX_SPECIES = 721
GEN6_MAX_ITEM = 775  # last Gen 6 (ORAS) item id; later games add repeat names above it


def norm(s):
    s = unicodedata.normalize("NFKD", s)
    s = "".join(c for c in s if not unicodedata.combining(c))
    return "".join(c for c in s.lower() if c not in " -'’‘.")


def display(s):
    """The name as the walker font draws it (ASCII apostrophe)."""
    return s.replace("’", "'").replace("‘", "'")


def name_width(s):
    return pwimg.text_width(display(s)) + 2


class NameList:
    def __init__(self, kind):
        self.kind = kind
        with open(os.path.join(I18N, kind + ".txt"), encoding="utf-8") as f:
            self.names = f.read().split("\n")
        self.index = {}
        self.dupes = {}
        for i, n in enumerate(self.names):
            k = norm(n)
            if not k:
                continue
            if k in self.index:
                self.dupes.setdefault(k, [self.index[k]]).append(i)
            else:
                self.index[k] = i  # lowest id wins

    def find(self, name):
        name = ALIASES[self.kind].get(name, name)
        return self.index.get(norm(name))


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


def load_learnsets():
    vg = None
    with open(os.path.join(POKEAPI, "version_groups.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r["identifier"] == "x-y":
                vg = r["id"]
    if vg is None:
        raise SystemExit("version group x-y not in version_groups.csv")
    sets = {}
    with open(os.path.join(POKEAPI, "pokemon_moves.csv"), newline="") as f:
        for row_no, r in enumerate(csv.DictReader(f)):
            if r["version_group_id"] != vg or r["pokemon_move_method_id"] != "1":
                continue
            pid = int(r["pokemon_id"])
            if pid > 10000:
                continue
            sets.setdefault(pid, []).append((int(r["level"]), row_no, int(r["move_id"])))
    for v in sets.values():
        v.sort()
    return int(vg), sets


def wild_moves(learnset, level):
    """Last 4 moves learned at or below level; a repeated move keeps its latest place."""
    seq = [m for (lv, _, m) in learnset if lv <= level]
    out = []
    for m in seq:
        if m in out:
            out.remove(m)
        out.append(m)
    out = out[-4:]
    return out + [0] * (4 - len(out))


def main():
    book = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BOOK
    out_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    import openpyxl
    wb = openpyxl.load_workbook(book, read_only=True, data_only=True)

    species, moves, items = NameList("species"), NameList("moves"), NameList("items")
    _, learnsets = load_learnsets()

    courses = []  # [name, unlock, favoured, [pokes]]
    by_name = {}
    rows = list(wb["Courses"].iter_rows(min_row=2, values_only=True))
    for r in rows:
        if not r or r[0] is None:
            continue
        cname = COURSE_RENAMES.get(r[0], r[0])
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        pname, level = str(r[4]).strip(), int(r[5])
        pname, level = POKE_SWAPS.get((cname, pname), (pname, level))
        by_name[cname][3].append((str(r[3]).strip(), pname, level, int(r[6])))

    irows = list(wb["Items"].iter_rows(values_only=True))
    hdr = next(i for i, r in enumerate(irows) if r and r[0] == "Course")
    for r in irows[hdr + 1:]:
        if not r or r[0] is None:
            continue
        cname = COURSE_RENAMES.get(r[0], r[0])
        if cname not in by_name:
            raise SystemExit("Items sheet course not in Courses sheet: %r" % r[0])
        by_name[cname][4].append((int(r[2]), str(r[3]).strip()))

    unresolved = []
    problems = []
    lines = [
        "; PocketPKMN routes for Pokemon X / Y - generated by tools/routes_xy.py "
        "from the user's workbook (do not edit by hand)",
        "game x y",
    ]
    for n, (cname, unlock, fav, pokes, its) in enumerate(courses):
        if cname not in PICTURES:
            raise SystemExit("no picture kind for course %r" % cname)
        if name_width(cname) > WIDTH_LIMIT["course"]:
            problems.append("course name too wide: %s (%d px)" % (cname, name_width(cname)))
        lines.append("")
        lines.append("course %d | %s | unlock=%s | picture=%s | favoured=%s"
                     % (n, cname, unlock, PICTURES[cname], fav))
        for grp in ("A", "B", "C"):
            members = [p for p in pokes if p[0] == grp]
            if len(members) != 2:
                raise SystemExit("%s group %s has %d Pokemon" % (cname, grp, len(members)))
            slot, chance = GROUP_SLOT[grp]
            for _, pname, level, steps in members:
                # user 2026-11-15 ("Set A to 0 for all of our games' courses so far"): the walker falls back to group
                # A whenever nothing else has met its step minimum, so A's minimum never applied -> always 0
                if grp == "A":
                    steps = 0
                sid = species.find(pname)
                if sid is None:
                    unresolved.append("species: " + pname)
                    continue
                sname = species.names[sid]
                if name_width(sname) > WIDTH_LIMIT["poke"]:
                    problems.append("Pokemon name too wide: %s" % sname)
                ls = learnsets.get(sid, [])
                mv = wild_moves(ls, level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)"
                                    % (sname, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                lines.append("poke %s %d %d %d %d %d %d %d %d %d ; %s: %s"
                             % (grp, slot, chance, sid, level, steps, *mv, sname, mnames))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            for real, ver in ITEM_CHOICES.get(iname, [(iname, "all")]):
                iid = items.find(real)
                if iid is None:
                    unresolved.append("item: " + real)
                    continue
                k = norm(items.names[iid])
                if k in items.dupes and sum(1 for d in items.dupes[k] if d <= GEN6_MAX_ITEM) > 1:
                    problems.append("item name %s has several ids %s (used %d)"
                                    % (items.names[iid], items.dupes[k], iid))
                if name_width(items.names[iid]) > WIDTH_LIMIT["item"]:
                    problems.append("item name too wide: %s (%d px)"
                                    % (items.names[iid], name_width(items.names[iid])))
                lines.append("item %d %d %d %d %s ; %s"
                             % (rank, ITEM_CHANCES[rank], iid, steps, ver, items.names[iid]))

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
