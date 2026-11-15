#!/usr/bin/env python3
"""Builds the Pokemon Sun / Moon / Ultra Sun / Ultra Moon route file from the user's Gen 7 course workbook.

usage: python3 -I tools/routes_sm.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/Gen 7 Pokewalker Courses.xlsx (env), else ../walker courses/... (read only)
  out       default: romfs/routes/sm.txt

Same file format as tools/routes_xy.py (shared helpers are imported from it), plus three tokens on Pokemon lines
(before the ';'): form=N (Alolan forms, Blue-Striped Basculin), origin=SN,MN,US,UM (the games the catch may come
from, from the sheet's "Origin game to set"; X,Y for the Imposter Ditto) and ha=0/1 (the sheet's "Give the Hidden
Ability?"). Unlocks: trials=N (N = trials done in story order, read from the trial Z-Crystals), hof, hof watts=N.
Moves: the last 4 level-up moves at that level in Ultra Sun / Ultra Moon (PokeAPI data in art_src/pokeapi).
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import routes_xy as rx  # noqa: E402  (NameList, norm, name_width, wild_moves, limits)

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")),
                            "Gen 7 Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "sm.txt")

# The user's decisions -------------------------------------------------------
# 2026-11-15: "Champion's Bazaar" is 81 px (limit 80) -> "Elite Bazaar" (user picked it, as on X/Y).
COURSE_RENAMES = {"Champion's Bazaar": "Elite Bazaar"}
# 2026-11-15: Type: Null is a one-time gift ("We don't want those in the pokewalker"); "Porygon and Porygon are both
# in that group".
POKE_SWAPS = {("Starter's Lane", "Type: Null"): "Porygon"}
# 2026-11-15: "Can you make imposter ditto as obtained in XY" -> origin X / Y (Friend Safari), Hidden Ability.
ORIGIN_OVERRIDES = {("Elite Bazaar", "Ditto"): ("X,Y", 1)}
# workbook item text -> the item used (same choices as X/Y)
ITEM_CHOICES = {
    "Power Bracer (or another Power item)": "Power Bracer",
    "Choice Band or Choice Specs": "Choice Band",
}
# sheet name -> (species, form, PokeAPI pokemon id for the form's learnset or None)
FORMS = {
    "Grimer (Alolan)": ("Grimer", 1, 10112), "Rattata (Alolan)": ("Rattata", 1, 10091),
    "Golem (Alolan)": ("Golem", 1, 10111), "Vulpix (Alolan)": ("Vulpix", 1, 10103),
    "Sandshrew (Alolan)": ("Sandshrew", 1, 10101),
    "Basculin (Red-Striped)": ("Basculin", 0, None), "Basculin (Blue-Striped)": ("Basculin", 1, 10016),
}
# story order of the island challenge; "trials=N" opens after the N-th (the app counts the trial Z-Crystals held)
TRIALS = ["Trial 1 (Ilima)", "Grand Trial 1 (Hala)", "Trial 2 (Lana)", "Trial 3 (Kiawe)", "Trial 4 (Mallow)",
          "Grand Trial 2 (Olivia)", "Trial 5 (Sophocles)", "Trial 6 (Acerola)", "Grand Trial 3 (Nanu)",
          "Trial 7 (Vast Poni Canyon)", "Grand Trial 4 (Hapu)"]
ORIGINS = [  # sheet text (start) -> games
    ("Walker's own game", "SN,MN,US,UM"), ("Ultra Sun or Ultra Moon", "US,UM"), ("Sun or Ultra Sun", "SN,US"),
    ("Moon or Ultra Moon", "MN,UM"), ("Ultra Sun", "US"), ("Ultra Moon", "UM"),
]
# Course pictures (romfs/routes/pictures/<name>.txt, drawn by tools/route_art.py). Since 2026-11-15 (user: "Yes, draw
# them") the 14 Gen 7-only courses have their own (route_art.py SM1); the four that share an X/Y course's name share
# its picture.
PICTURES = {
    "Boardwalk": "boardwalk", "City Square": "city_square", "Shopping Plaza": "shopping_plaza",
    "Seaside Cliff": "seaside_cliff", "Clear Lake": "clear_lake", "Volcanic Trail": "volcanic_trail",
    "Vanished Woods": "vanished_woods", "Gemstone Cavern": "gemstone_cavern", "Rural Station": "rural_station",
    "Illusionary Manor": "illusionary_manor", "Sunbaked Desert": "sunbaked_desert", "Coral Reef": "coral_reef",
    "Snowy Summit": "snowy_summit", "Starter's Lane": "starters_lane", "Fossil Dig Site": "fossil_dig_site",
    "Trader's Crossing": "traders_crossing", "Dragon Hollow": "dragon_hollow", "Elite Bazaar": "elite_bazaar",
}
GEN7_MAX_ITEM = 959  # last Ultra Sun / Ultra Moon item id
MAX_SPECIES = 807


def slug(name):
    return name.lower().replace("'", "").replace("’", "").replace(" ", "_")


def parse_unlock(text):
    t = str(text).strip()
    if t == "Start":
        return "start"
    if t in TRIALS:
        return "trials=%d" % (TRIALS.index(t) + 1)
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


def load_learnsets(group="ultra-sun-ultra-moon"):
    vg = None
    with open(os.path.join(rx.POKEAPI, "version_groups.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r["identifier"] == group:
                vg = r["id"]
    if vg is None:
        raise SystemExit("version group %s not in version_groups.csv" % group)
    sets = {}
    with open(os.path.join(rx.POKEAPI, "pokemon_moves.csv"), newline="") as f:
        for row_no, r in enumerate(csv.DictReader(f)):
            if r["version_group_id"] != vg or r["pokemon_move_method_id"] != "1":
                continue
            sets.setdefault(int(r["pokemon_id"]), []).append((int(r["level"]), row_no, int(r["move_id"])))
    for v in sets.values():
        v.sort()
    return sets


def levels_from_sheet(wb):
    """Group A/B/C level per unlock, computed like the Levels tab (cached formula values may be missing)."""
    out, best = {"Start": (5, 6, 8)}, 0
    settings = {r[0]: r[1] for r in wb["Settings"].iter_rows(values_only=True) if r and r[0]}
    da = int(settings.get("Group A level vs trial ace", -4))
    db = int(settings.get("Group B level vs trial ace", -2))
    dc = int(settings.get("Group C level vs trial ace", 0))
    for r in list(wb["Levels"].iter_rows(values_only=True))[1:]:
        if not r or r[0] not in TRIALS or not isinstance(r[3], (int, float)):
            continue
        best = max(best, int(r[3]))
        out[r[0]] = (best + da, best + db, best + dc)
    return out


def main():
    book = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BOOK
    out_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    import openpyxl
    wb = openpyxl.load_workbook(book, read_only=True, data_only=True)

    species, moves, items = rx.NameList("species"), rx.NameList("moves"), rx.NameList("items")
    learnsets = load_learnsets()
    levels = levels_from_sheet(wb)

    courses, by_name = [], {}
    for r in wb["Courses"].iter_rows(min_row=2, values_only=True):
        if not r or r[0] is None:
            continue
        cname = COURSE_RENAMES.get(r[0], r[0])
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        grp = str(r[3]).strip()
        pname = POKE_SWAPS.get((cname, str(r[4]).strip()), str(r[4]).strip())
        level = r[5]
        if not isinstance(level, (int, float)):
            level = levels[str(r[1]).strip()]["ABC".index(grp)]
        origin = parse_origin(r[12])
        ha = 1 if str(r[11]).strip().startswith("Yes") else 0
        if (cname, pname) in ORIGIN_OVERRIDES:
            origin, ha = ORIGIN_OVERRIDES[(cname, pname)]
        if pname != str(r[4]).strip():  # a swapped-in Pokemon takes the other slot's settings
            same = [p for p in by_name[cname][3] if p[1] == pname]
            if same:
                origin, ha = same[0][5], same[0][6]
        by_name[cname][3].append((grp, pname, int(level), int(r[6]), None, origin, ha))

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
    lines = [
        "; PocketPKMN routes for Pokemon Sun / Moon / Ultra Sun / Ultra Moon - generated by tools/routes_sm.py "
        "from the user's workbook (do not edit by hand)",
        "game sn mn us um",
    ]
    for n, (cname, unlock, fav, pokes, its) in enumerate(courses):
        if rx.name_width(cname) > rx.WIDTH_LIMIT["course"]:
            problems.append("course name too wide: %s (%d px)" % (cname, rx.name_width(cname)))
        lines.append("")
        if cname not in PICTURES:
            raise SystemExit("no picture for course %r" % cname)
        lines.append("course %d | %s | unlock=%s | picture=%s | favoured=%s" % (n, cname, unlock, PICTURES[cname], fav))
        for grp in ("A", "B", "C"):
            members = [p for p in pokes if p[0] == grp]
            if len(members) != 2:
                raise SystemExit("%s group %s has %d Pokemon" % (cname, grp, len(members)))
            slot, chance = rx.GROUP_SLOT[grp]
            for _, sheet_name, level, steps, _, origin, ha in members:
                # user 2026-11-15 ("Set A to 0 for all of our games' courses so far"): the walker falls back to group
                # A whenever nothing else has met its step minimum, so A's minimum never applied -> always 0
                if grp == "A":
                    steps = 0
                base, form, api_id = FORMS.get(sheet_name, (sheet_name, 0, None))
                sid = species.find(base)
                if sid is None or sid > MAX_SPECIES:
                    unresolved.append("species: " + sheet_name)
                    continue
                sname = species.names[sid]
                if rx.name_width(sname) > rx.WIDTH_LIMIT["poke"]:
                    problems.append("Pokemon name too wide: %s" % sname)
                ls = learnsets.get(api_id if api_id else sid, [])
                mv = rx.wild_moves(ls, level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)" % (sheet_name, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                # the name before ':' is the walker's name label, so it stays the plain species name (the form is
                # in form=N; the sheet's form name follows the moves as a note)
                note = " [%s]" % sheet_name if "(" in sheet_name else ""
                lines.append("poke %s %d %d %d %d %d %d %d %d %d form=%d origin=%s ha=%d ; %s: %s%s"
                             % (grp, slot, chance, sid, level, steps, *mv, form, origin, ha, sname, mnames, note))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            real = ITEM_CHOICES.get(iname, iname)
            iid = items.find(real)
            if iid is None or iid > GEN7_MAX_ITEM:
                unresolved.append("item: " + real)
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
