#!/usr/bin/env python3
"""Builds the Pokemon Gold / Silver / Crystal route file from the user's course workbook.

usage: python3 -I tools/routes_gsc.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/GSC Pokewalker Courses.xlsx (env), else ../walker courses/GSC Pokewalker Courses.xlsx next to the project folder (read only)
  out       default: romfs/routes/gsc.txt

Same file format as romfs/routes/xy.txt (tools/routes_xy.py), with two differences for Gen 2:
  - item ids are GEN 2 item ids (romfs/i18n/eng/items2.txt, line n = id n), so a found item goes back into a
    Gold / Silver / Crystal save as itself (many GSC items - Berry, PsnCureBerry, Pink Bow ... - have no later id);
  - moves = the last 4 moves learned by level-up in Gold / Silver (PokeAPI version group "gold-silver") at the
    course level (valid in Crystal too).
Course and Pokemon rows are taken from the workbook as they are (the user's lists; Gold / Silver exclusives are
offered in every game). Stops with a list if any name does not resolve.
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import routes_xy as xy  # noqa: E402  (norm, name widths, unlock parsing, move choice)

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")), "GSC Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "gsc.txt")
LEARNSET_VG = "gold-silver"

# "Champion's Bazaar" is 81 px wide in the walker font (limit 80) -> the same short name as on the X/Y courses.
COURSE_RENAMES = {"Champion's Bazaar": "Elite Bazaar", "Whispering Stones": "Whispering Ruins"}  # 81 px -> 74; a ruins area (user)
COURSE_NAMES = ["Refreshing Field", "Noisy Forest", "Rugged Road", "Moonlit Grove", "Shopping District", "Ranch",
                "Nursery Meadow", "Beautiful Beach", "Suburban Area", "Whispering Ruins", "Dim Cave", "Rocky Pass",
                "Blue Lake", "Harbor Docks", "Town Outskirts", "Frozen Cavern", "Starter's Lane", "Trader's Crossing",
                "Elite Bazaar"]
# One picture per course, art/routes/<slug>.txt. Courses with the same name as an X/Y course share its picture
# (Shopping District, Starter's Lane, Trader's Crossing, Elite Bazaar). User 2026-11-15: "Any courses that are themed
# similarly to the XY courses can reuse their course drawings, but new ones are good too" -> REUSE below; the other
# eight get new drawings (tools/route_art.py).
PICTURES = {n: n.lower().replace("'", "").replace(" ", "_") for n in COURSE_NAMES}
REUSE = {
    "Noisy Forest": "sunlit_woods",
    "Moonlit Grove": "shadowed_thicket",
    "Beautiful Beach": "seashell_coast",
    "Town Outskirts": "quiet_outskirts",
    "Rocky Pass": "crystal_cliffs",
    "Blue Lake": "wyrm_hollow",        # the cave lake (Dragon's Den feel)
    "Frozen Cavern": "frost_ridge",
}
PICTURES.update(REUSE)
SPECIES_ALIASES = {"Nidoran M": "Nidoran♂", "Nidoran F": "Nidoran♀"}
ITEM_ALIASES = {"Elixir": "Elixer", "Max Elixir": "Max Elixer"}  # Gen 2 spelling (items2.txt, as the games)
GROUP_SLOT = xy.GROUP_SLOT
ITEM_CHANCES = xy.ITEM_CHANCES
MAX_SPECIES = 251


def load_learnsets():
    vg = None
    with open(os.path.join(xy.POKEAPI, "version_groups.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r["identifier"] == LEARNSET_VG:
                vg = r["id"]
    if vg is None:
        raise SystemExit("version group %s not in version_groups.csv" % LEARNSET_VG)
    sets = {}
    with open(os.path.join(xy.POKEAPI, "pokemon_moves.csv"), newline="") as f:
        for row_no, r in enumerate(csv.DictReader(f)):
            if r["version_group_id"] != vg or r["pokemon_move_method_id"] != "1":
                continue
            pid = int(r["pokemon_id"])
            if pid > MAX_SPECIES:
                continue
            sets.setdefault(pid, []).append((int(r["level"]), row_no, int(r["move_id"])))
    for v in sets.values():
        v.sort()
    return sets


class Gen2Items:
    """romfs/i18n/eng/items2.txt: line n (from 0) = Gen 2 item id n."""

    def __init__(self):
        with open(os.path.join(xy.I18N, "items2.txt"), encoding="utf-8") as f:
            self.names = f.read().split("\n")
        self.index = {}
        for i, n in enumerate(self.names):
            k = xy.norm(n)
            if k and k not in self.index:
                self.index[k] = i

    def find(self, name):
        return self.index.get(xy.norm(ITEM_ALIASES.get(name, name)))


LEVEL_FIXES = {
    # user 2026-11-15 "just make ditto level 10": GSC has no Ditto below Lv 10 (Route 34 / 35 from Lv 10; Ditto
    # cannot be bred), so a Lv 9 catch would not be legal (PKHeX, tools/pkhex_check templates)
    ("Shopping District", "Ditto"): 10,
}


def main():
    book = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BOOK
    out_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    import openpyxl
    wb = openpyxl.load_workbook(book, read_only=True, data_only=True)

    species, moves = xy.NameList("species"), xy.NameList("moves")
    items = Gen2Items()
    learnsets = load_learnsets()

    courses, by_name = [], {}
    for r in wb["Courses"].iter_rows(min_row=2, values_only=True):
        if not r or r[0] is None:
            continue
        cname = COURSE_RENAMES.get(r[0], r[0])
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, xy.parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        by_name[cname][3].append((str(r[3]).strip(), str(r[4]).strip(), int(r[5]), int(r[6])))

    irows = list(wb["Items"].iter_rows(values_only=True))
    hdr = next(i for i, r in enumerate(irows) if r and r[0] == "Course")
    for r in irows[hdr + 1:]:
        if not r or r[0] is None:
            continue
        cname = COURSE_RENAMES.get(r[0], r[0])
        if cname not in by_name:
            raise SystemExit("Items sheet course not in Courses sheet: %r" % r[0])
        by_name[cname][4].append((int(r[2]), str(r[3]).strip()))

    unresolved, problems = [], []
    lines = ["; PocketPKMN routes for Pokemon Gold / Silver / Crystal - generated by tools/routes_gsc.py from the "
             "user's workbook (do not edit by hand). ITEM IDS ARE GEN 2 ITEM IDS (items2.txt).",
             "game gold silver crystal",
             "items gen2"]
    seen = {}
    for n, (cname, unlock, fav, pokes, its) in enumerate(courses):
        if cname not in PICTURES:
            raise SystemExit("no picture for course %r (add it to COURSE_NAMES)" % cname)
        if xy.name_width(cname) > xy.WIDTH_LIMIT["course"]:
            problems.append("course name too wide: %s (%d px)" % (cname, xy.name_width(cname)))
        lines += ["", "course %d | %s | unlock=%s | picture=%s | favoured=%s" % (n, cname, unlock, PICTURES[cname], fav)]
        for grp in ("A", "B", "C"):
            members = [p for p in pokes if p[0] == grp]
            if len(members) != 2:
                raise SystemExit("%s group %s has %d Pokemon" % (cname, grp, len(members)))
            slot, chance = GROUP_SLOT[grp]
            for _, pname, level, steps in members:
                # legality fixes the user chose (the workbook is not edited; a rebuild keeps them)
                level = LEVEL_FIXES.get((cname, pname), level)
                sid = species.find(SPECIES_ALIASES.get(pname, pname))
                if sid is None or sid > MAX_SPECIES:
                    unresolved.append("species: " + pname)
                    continue
                sname = species.names[sid]
                if sid in seen:
                    problems.append("%s is in two courses (%s, %s)" % (sname, seen[sid], cname))
                seen[sid] = cname
                if xy.name_width(sname) > xy.WIDTH_LIMIT["poke"]:
                    problems.append("Pokemon name too wide: %s" % sname)
                mv = xy.wild_moves(learnsets.get(sid, []), level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)" % (sname, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                lines.append("poke %s %d %d %d %d %d %d %d %d %d ; %s: %s"
                             % (grp, slot, chance, sid, level, steps, *mv, sname, mnames))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            iid = items.find(iname)
            if iid is None:
                unresolved.append("item: " + iname)
                continue
            if xy.name_width(items.names[iid]) > xy.WIDTH_LIMIT["item"]:
                problems.append("item name too wide: %s (%d px)" % (items.names[iid], xy.name_width(items.names[iid])))
            lines.append("item %d %d %d %d all ; %s" % (rank, ITEM_CHANCES[rank], iid, steps, items.names[iid]))

    if unresolved:
        print("UNRESOLVED NAMES (nothing written):")
        for u in sorted(set(unresolved)):
            print("  " + u)
        sys.exit(1)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s: %d courses" % (out_path, len(courses)))
    missing = sorted({p for p in PICTURES.values()
                      if not os.path.exists(os.path.join(ROOT, "art", "routes", p + ".txt"))})
    if missing:
        print("pictures still to draw (%d): %s" % (len(missing), ", ".join(missing)))
    for p in problems:
        print("note: " + p)


if __name__ == "__main__":
    main()
