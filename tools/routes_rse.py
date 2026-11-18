#!/usr/bin/env python3
"""Builds the Pokemon Ruby / Sapphire / Emerald route file from the user's RSE course workbook.

usage: python3 -I tools/routes_rse.py [workbook] [out]
  workbook  default: $POCKETPKMN_COURSES/RSE Pokewalker Courses.xlsx (env), else ../walker courses/... (read only)
  out       default: romfs/routes/rse.txt

v0.97 (2026-11-17, owner: "we now have a spreadsheet for rse courses. check and add them"). Same format as
tools/routes_frlg.py: Pokemon lines carry form=0, origin=<games> (from the sheet's "Origin game to set": R,S,E for
the walker's own game, Ruby / Sapphire / Emerald subsets, FR / LG for Kanto and Johto Pokemon, CXD for Colosseum /
XD) and ha=0 (Gen 3 has no Hidden Abilities). Moves: the last 4 level-up moves at that level in the ORIGIN game's
learnset (Emerald / Ruby-Sapphire / FireRed-LeafGreen / Colosseum / XD; PokeAPI), as PKHeX finds the encounter in
that game. Unlocks: badges=N, hof, hof watts=N. The workbook is only read.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import routes_xy as rx  # noqa: E402  (NameList, name_width, wild_moves, limits)
import routes_sm as rs  # noqa: E402  (load_learnsets)
import routes_frlg as rf  # noqa: E402  (parse_unlock, ITEM_NAMES, SPECIES_NAMES)

DEFAULT_BOOK = os.path.join(os.environ.get("POCKETPKMN_COURSES", os.path.join(os.path.dirname(ROOT), "walker courses")),
                            "RSE Pokewalker Courses.xlsx")
DEFAULT_OUT = os.path.join(ROOT, "romfs", "routes", "rse.txt")

ORIGINS = [  # sheet text (start) -> games; the first game's learnset gives the moves
    ("Walker's own game", "R,S,E"), ("Sapphire or Emerald", "S,E"), ("Ruby or Sapphire", "R,S"),
    ("Ruby or Emerald", "R,E"), ("Emerald", "E"), ("FireRed or LeafGreen", "FR,LG"), ("LeafGreen", "LG"),
    ("FireRed", "FR"), ("Colosseum", "CXD"), ("Sapphire", "S"), ("Ruby", "R"),
]
LEARNSET = {"R": "ruby-sapphire", "S": "ruby-sapphire", "E": "emerald", "FR": "firered-leafgreen",
            "LG": "firered-leafgreen", "CXD": "colosseum"}
# "Wanderer's Prairie" is 81 px (limit 80) -> "Roaming Prairie" (default until the owner picks a name); its picture
# keeps the slug of the workbook name
COURSE_RENAMES = {"Wanderer's Prairie": "Roaming Prairie"}
PICTURE_OF = {"Roaming Prairie": "wanderers_prairie"}
# owner 2026-11-17 on Colosseum / XD origins: "if the trainer name and id and secret id and all that match then im fine
# with it" - a Colosseum / XD Pokemon cannot carry the player's IDs (those games make their own; their player is male),
# so Hitmontop comes as a Tyrogue bred and evolved in the player's own game instead, Espeon / Umbreon as a hatched
# Eevee evolved there.
# (owner: "could you just have them come from an egg??" - yes: Ruby / Sapphire / Emerald have a clock, so a hatched
# Eevee can evolve into them there; PKHeX: legal as an egg in R / S / E.)
# owner 2026-11-17: "Delete rolling meadow, buzzing grove, old home path, thorn thicket, vine garden, tangled grove,
# rumbling caave, ember ranch, mind garden, tide pools, champion's road, lily pond, deep current, snowdrift pass, wild
# plains", then "keep champion's road".
# Both are now in the owner's RSE workbook itself ("update the spreadsheets too": the 14 courses removed, the three
# origins "Walker's own game"), so nothing is overridden or left out here any more.
ORIGIN_OVERRIDES = {}
DROP_COURSES = set()
GEN3_LAST_ITEM = 376
MAX_SPECIES = 386


def slug(name):
    return name.lower().replace("'", "").replace("’", "").replace(" ", "_")


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
    learnsets = {g: rs.load_learnsets(g) for g in set(LEARNSET.values())}

    courses, by_name = [], {}
    for r in wb["Courses"].iter_rows(min_row=2, values_only=True):
        if not r or r[0] is None:
            continue
        if str(r[0]).strip() in DROP_COURSES:
            continue
        cname = COURSE_RENAMES.get(str(r[0]).strip(), str(r[0]).strip())
        if cname not in by_name:
            fav = ",".join(t.strip() for t in str(r[9]).split("/"))
            by_name[cname] = [cname, rf.parse_unlock(r[1]), fav, [], []]
            courses.append(by_name[cname])
        pname = str(r[4]).strip()
        if not isinstance(r[5], (int, float)):
            raise SystemExit("%s %s: no level" % (cname, pname))
        origin = ORIGIN_OVERRIDES.get((cname, pname), parse_origin(r[10]))
        by_name[cname][3].append((str(r[3]).strip(), pname, int(r[5]), int(r[6]), origin))

    irows = list(wb["Items"].iter_rows(values_only=True))
    hdr = next(i for i, r in enumerate(irows) if r and r[0] == "Course")
    for r in irows[hdr + 1:]:
        if not r or r[0] is None:
            continue
        if str(r[0]).strip() in DROP_COURSES:
            continue
        cname = COURSE_RENAMES.get(str(r[0]).strip(), str(r[0]).strip())
        if cname not in by_name:
            raise SystemExit("Items sheet course not in Courses sheet: %r" % r[0])
        by_name[cname][4].append((int(r[2]), str(r[3]).strip()))

    unresolved, problems = [], []
    lines = [
        "; PocketPKMN routes for Pokemon Ruby / Sapphire / Emerald - generated by tools/routes_rse.py from the user's "
        "workbook (do not edit by hand)",
        "game r s e",
    ]
    for n, (cname, unlock, fav, pokes, its) in enumerate(courses):
        if rx.name_width(cname) > rx.WIDTH_LIMIT["course"]:
            problems.append("course name too wide: %s (%d px)" % (cname, rx.name_width(cname)))
        pic = PICTURE_OF.get(cname, slug(cname))
        if not os.path.exists(os.path.join(ROOT, "art", "routes", pic + ".txt")):
            problems.append("picture not drawn yet: " + pic)
        lines.append("")
        lines.append("course %d | %s | unlock=%s | picture=%s | favoured=%s" % (n, cname, unlock, pic, fav))
        for grp in ("A", "B", "C"):
            members = [p for p in pokes if p[0] == grp]
            if len(members) != 2:
                raise SystemExit("%s group %s has %d Pokemon" % (cname, grp, len(members)))
            slot, chance = rx.GROUP_SLOT[grp]
            for _, sheet_name, level, steps, origin in members:
                if grp == "A":  # user 2026-11-15: group A's minimum is always 0
                    steps = 0
                sid = species.find(rf.SPECIES_NAMES.get(sheet_name, sheet_name))
                if sid is None or sid > MAX_SPECIES:
                    unresolved.append("species: " + sheet_name)
                    continue
                sname = species.names[sid]
                if rx.name_width(sname) > rx.WIDTH_LIMIT["poke"]:
                    problems.append("Pokemon name too wide: %s" % sname)
                mv = rx.wild_moves(learnsets[LEARNSET[origin.split(",")[0]]].get(sid, []), level)
                if mv.count(0):
                    problems.append("%s Lv%d has only %d level-up moves (%s)" % (sheet_name, level, 4 - mv.count(0), cname))
                mnames = ", ".join(moves.names[m] for m in mv if m)
                lines.append("poke %s %d %d %d %d %d %d %d %d %d form=0 origin=%s ha=0 ; %s: %s"
                             % (grp, slot, chance, sid, level, steps, *mv, origin, sname, mnames))
        if len(its) != 10:
            raise SystemExit("%s has %d items" % (cname, len(its)))
        ordered = sorted(enumerate(its), key=lambda e: (-e[1][0], e[0]))
        for rank, (_, (steps, iname)) in enumerate(ordered):
            real = rf.ITEM_NAMES.get(iname, iname)
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
