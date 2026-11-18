#!/usr/bin/env python3
"""Builds the Pokemon Omega Ruby / Alpha Sapphire route file from the user's ORAS course workbook.

usage: python3 -I tools/routes_oras.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/ORAS Pokewalker Courses.xlsx (env), else ../walker courses/... (read only)
  out       default: romfs/routes/oras.txt

v0.93 (2026-11-17, owner: "lets do oras courses", "Yes, build it"; plan docs/ORAS_COURSES_PLAN.md). Same file format
as tools/routes_sm.py (helpers from routes_xy / routes_sm): Pokemon lines carry form=N (Shellos West / East Sea),
origin=OR,AS (the games the catch may come from, from the sheet's "Origin game to set": version exclusives OR or AS,
the Friend Safari Machoke X,Y) and ha=0/1 (the sheet's "Give the Hidden Ability?"). Unlocks: badges=N, hof,
hof watts=N. Moves: the last 4 level-up moves at that level in Omega Ruby / Alpha Sapphire (PokeAPI data in
art_src/pokeapi). The workbook is only read.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import routes_xy as rx  # noqa: E402  (NameList, name_width, wild_moves, limits)
import routes_sm as rs  # noqa: E402  (load_learnsets)

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")),
                            "ORAS Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "oras.txt")

# sheet name -> (species, form, PokeAPI pokemon id for the form's learnset or None = the species' own)
FORMS = {"Shellos (West Sea)": ("Shellos", 0, None), "Shellos (East Sea)": ("Shellos", 1, None)}
ORIGINS = [  # sheet text (start) -> games
    ("Walker's own game", "OR,AS"), ("Omega Ruby", "OR"), ("Alpha Sapphire", "AS"), ("X or Y", "X,Y"),
]
# Course pictures: STAND-INS (plan question 6, default until the owner says otherwise) - the closest existing X/Y,
# Gen 7 or HGSS course picture (romfs/routes/pictures/<name>.txt); drawn ORAS pictures later.
# v0.97 (owner: "add any needed course sprites"): each course its own picture (tools/route_art_hoenn.py); names shared
# with FireRed / LeafGreen (Field, Forest) and X / Y (Fossil Dig Site) share those pictures
PICTURES = {
    "Field": "field", "Forest": "forest", "Dim Tunnel": "dim_tunnel", "Beachside": "beachside",
    "Generator Yard": "generator_yard", "Busy City": "busy_city", "Sunbaked Ruins": "sunbaked_ruins",
    "Underwater": "underwater", "Cliffside": "cliffside", "Misty Mountain": "misty_mountain",
    "Frozen Shoal": "frozen_shoal", "Beginner's Path": "beginners_path", "Scavenger's Trail": "scavengers_trail",
    "Far Shores": "far_shores", "Trader's Hill": "traders_hill", "Fossil Dig Site": "fossil_dig_site",
    "Champion's Road": "champions_road", "Starfall Crater": "starfall_crater",
}
# workbook item text -> the item used: the first one named, as X/Y and Gen 7 did (Power Bracer, Choice Band)
# owner 2026-11-17: "replace machoke with machamp" (Champion's Road; "replace it in oras too") - the Friend Safari
# Machoke's origin and Hidden Ability carry over (a traded-and-evolved Machamp)
# 2026-11-17: the owner's ORAS workbook now says Machamp itself ("update the spreadsheets too")
POKE_SWAPS = {}
ITEM_CHOICES = {
    "A Power item (Power Bracer etc.)": "Power Bracer",
    "Choice Band, Scarf or Specs": "Choice Band",
    "Liechi or Salac Berry": "Liechi Berry",
    "Toxic Orb or Flame Orb": "Toxic Orb",
}
ORAS_MAX_ITEM = 775  # last Omega Ruby / Alpha Sapphire item id
MAX_SPECIES = 721


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
    learnsets = rs.load_learnsets("omega-ruby-alpha-sapphire")
    # a Pokemon from X / Y (the Friend Safari Machoke) knows the X / Y moves at its level - PKHeX finds no X / Y
    # encounter with ORAS-only moves (Machoke: Dual Chop)
    learnsets_xy = rs.load_learnsets("x-y")

    courses, by_name = [], {}
    for r in wb["Courses"].iter_rows(min_row=2, values_only=True):
        if not r or r[0] is None:
            continue
        cname = str(r[0]).strip()
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        grp = str(r[3]).strip()
        pname = POKE_SWAPS.get((cname, str(r[4]).strip()), str(r[4]).strip()) # v0.97
        if not isinstance(r[5], (int, float)):
            raise SystemExit("%s %s: no level" % (cname, pname))
        origin = parse_origin(r[12])
        ha = 1 if str(r[11]).strip().startswith("Yes") else 0
        by_name[cname][3].append((grp, pname, int(r[5]), int(r[6]), origin, ha))

    irows = list(wb["Items"].iter_rows(values_only=True))
    hdr = next(i for i, r in enumerate(irows) if r and r[0] == "Course")
    for r in irows[hdr + 1:]:
        if not r or r[0] is None:
            continue
        cname = str(r[0]).strip()
        if cname not in by_name:
            raise SystemExit("Items sheet course not in Courses sheet: %r" % r[0])
        by_name[cname][4].append((int(r[2]), str(r[3]).strip()))

    unresolved, problems = [], []
    lines = [
        "; PocketPKMN routes for Pokemon Omega Ruby / Alpha Sapphire - generated by tools/routes_oras.py from the "
        "user's workbook (do not edit by hand)",
        "game or as",
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
            for _, sheet_name, level, steps, origin, ha in members:
                # user 2026-11-15 ("Set A to 0 for all of our games' courses so far"): group A's minimum is always 0
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
                ls = (learnsets_xy if origin == "X,Y" else learnsets).get(api_id if api_id else sid, [])
                mv = rx.wild_moves(ls, level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)" % (sheet_name, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                note = " [%s]" % sheet_name if "(" in sheet_name else ""
                lines.append("poke %s %d %d %d %d %d %d %d %d %d form=%d origin=%s ha=%d ; %s: %s%s"
                             % (grp, slot, chance, sid, level, steps, *mv, form, origin, ha, sname, mnames, note))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            real = ITEM_CHOICES.get(iname, iname)
            iid = items.find(real)
            if iid is None or iid > ORAS_MAX_ITEM:
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
