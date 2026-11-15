#!/usr/bin/env python3
"""Checks romfs/routes/gsc.txt (made by tools/routes_gsc.py) - tests/check_routes_xy.py adapted for Gen 2:
species 1-251, moves from the Gold/Silver level-up lists, item ids = Gen 2 ids (items2.txt), no version tags.
Course pictures that are not drawn yet are listed (not an error).

usage: python3 -I tests/check_routes_gsc.py [routes file]
Prints a short summary and PASS or FAIL (exit code 0 / 1).
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pwimg  # noqa: E402

I18N = os.path.join(ROOT, "romfs", "i18n", "eng")
POKEAPI = os.path.join(ROOT, "art_src", "pokeapi")
GROUP_SLOT = {"C": (0, 50), "B": (1, 75), "A": (2, 100)}
ITEM_CHANCES = [20, 20, 30, 30, 30, 40, 50, 50, 50, 100]
import os as _os
# one picture per course: every art/routes/<slug>.txt that exists (tools/route_art.py)
PICTURE_KINDS = {f[:-4] for f in _os.listdir(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..",
                                                           "art", "routes")) if f.endswith(".txt")}
LIMIT = {"course": 80, "item": 96, "poke": 80}


def read_list(kind):
    with open(os.path.join(I18N, kind + ".txt"), encoding="utf-8") as f:
        return f.read().split("\n")


def width(s):
    return pwimg.text_width(s.replace("’", "'").replace("‘", "'")) + 2


def learnsets():
    with open(os.path.join(POKEAPI, "version_groups.csv"), newline="") as f:
        vg = next(r["id"] for r in csv.DictReader(f) if r["identifier"] == "gold-silver")
    sets = {}
    with open(os.path.join(POKEAPI, "pokemon_moves.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r["version_group_id"] == vg and r["pokemon_move_method_id"] == "1":
                sets.setdefault(int(r["pokemon_id"]), []).append((int(r["level"]), int(r["move_id"])))
    return sets


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "romfs", "routes", "gsc.txt")
    species, moves, items = read_list("species"), read_list("moves"), read_list("items2")
    sets = learnsets()
    errors = []
    courses = []
    game = None
    item_ids = None
    pending = []
    with open(path, encoding="utf-8") as f:
        for no, raw in enumerate(f, 1):
            line = raw.split(";", 1)[0].strip()
            if not line:
                continue
            word = line.split()[0]
            if word == "game":
                game = line.split()[1:]
            elif word == "items":
                item_ids = line.split()[1:]
            elif word == "course":
                parts = [p.strip() for p in line.split("|")]
                head = parts[0].split()
                fields = dict(p.split("=", 1) for p in parts[2:])
                courses.append({"n": int(head[1]), "name": parts[1], "f": fields,
                                "pokes": [], "items": [], "line": no})
            elif word == "poke":
                t = line.split()
                courses[-1]["pokes"].append((t[1],) + tuple(int(x) for x in t[2:]) + (no,))
            elif word == "item":
                t = line.split()
                courses[-1]["items"].append(tuple(int(x) for x in t[1:5]) + (t[5], no))
            else:
                errors.append("line %d: unknown line %r" % (no, word))

    if game != ["gold", "silver", "crystal"]:
        errors.append("game line is %r" % game)
    if item_ids != ["gen2"]:
        errors.append("items line is %r (want gen2)" % item_ids)
    if len(courses) != 19:
        errors.append("%d courses, want 19" % len(courses))
    widest = {"course": 0, "item": 0, "poke": 0}
    padded = []
    n_pokes = n_items = 0
    for ci, c in enumerate(courses):
        where = "course %d %s" % (c["n"], c["name"])
        if c["n"] != ci:
            errors.append("%s: number out of order" % where)
        w = width(c["name"])
        widest["course"] = max(widest["course"], w)
        if w > LIMIT["course"]:
            errors.append("%s: name %d px wide" % (where, w))
        f = c["f"]
        if not f.get("picture"):
            errors.append("%s: no picture" % where)
        elif f.get("picture") not in PICTURE_KINDS:
            pending.append(f.get("picture"))
        u = f.get("unlock", "")
        if not (u in ("start", "hof") or u.startswith("badges=") and u[7:].isdigit()
                or u.startswith("hof watts=") and u[10:].isdigit()):
            errors.append("%s: unlock %r" % (where, u))
        if len(f.get("favoured", "").split(",")) != 3:
            errors.append("%s: favoured %r" % (where, f.get("favoured")))
        for grp, (slot, chance) in GROUP_SLOT.items():
            g = [p for p in c["pokes"] if p[0] == grp]
            if len(g) != 2:
                errors.append("%s: group %s has %d pokes" % (where, grp, len(g)))
            for p in g:
                _, pslot, pchance, sid, level, steps, m1, m2, m3, m4, no = p
                n_pokes += 1
                at = "line %d" % no
                if (pslot, pchance) != (slot, chance):
                    errors.append("%s: group %s slot/chance %d/%d" % (at, grp, pslot, pchance))
                if not 1 <= sid <= 251:
                    errors.append("%s: species %d" % (at, sid))
                    continue
                if not 1 <= level <= 100:
                    errors.append("%s: level %d" % (at, level))
                if steps < 0:
                    errors.append("%s: steps %d" % (at, steps))
                w = width(species[sid])
                widest["poke"] = max(widest["poke"], w)
                if w > LIMIT["poke"]:
                    errors.append("%s: %s is %d px wide" % (at, species[sid], w))
                mv = [m1, m2, m3, m4]
                real = [m for m in mv if m]
                if mv[:len(real)] != real:
                    errors.append("%s: a 0 move before a real one" % at)
                if len(set(real)) != len(real):
                    errors.append("%s: repeated move" % at)
                if not real:
                    errors.append("%s: no moves" % at)
                if len(real) < 4:
                    padded.append("%s Lv%d (%d)" % (species[sid], level, len(real)))
                allowed = {m for (lv, m) in sets.get(sid, []) if lv <= level}
                for m in real:
                    if not 1 <= m < len(moves) or not moves[m].strip():
                        errors.append("%s: move id %d not valid" % (at, m))
                    elif m not in allowed:
                        errors.append("%s: %s not in %s's Gold/Silver level-up list at Lv%d"
                                      % (at, moves[m], species[sid], level))
        if len(c["pokes"]) != 6:
            errors.append("%s: %d pokes" % (where, len(c["pokes"])))
        ranks = sorted({it[0] for it in c["items"]})
        if ranks != list(range(10)):
            errors.append("%s: item ranks %s" % (where, ranks))
        last_steps = None
        for rank in range(10):
            lines = [it for it in c["items"] if it[0] == rank]
            vers = sorted(it[4] for it in lines)
            if vers != ["all"]:
                errors.append("%s: rank %d versions %s" % (where, rank, vers))
            for r, chance, iid, steps, ver, no in lines:
                n_items += 1
                if chance != ITEM_CHANCES[rank]:
                    errors.append("line %d: chance %d, want %d" % (no, chance, ITEM_CHANCES[rank]))
                if not 1 <= iid < len(items) or not items[iid].strip():
                    errors.append("line %d: item id %d not valid" % (no, iid))
                    continue
                w = width(items[iid])
                widest["item"] = max(widest["item"], w)
                if w > LIMIT["item"]:
                    errors.append("line %d: %s is %d px wide" % (no, items[iid], w))
            st = {it[3] for it in lines}
            if len(st) > 1:
                errors.append("%s: rank %d steps differ by version" % (where, rank))
            if st:
                s = st.pop()
                if last_steps is not None and s > last_steps:
                    errors.append("%s: rank %d steps %d > rank %d" % (where, rank, s, rank - 1))
                last_steps = s
        if "items" in c and c["items"] and min(it[3] for it in c["items"]) < 0:
            errors.append("%s: negative item steps" % where)

    print("routes file: %s" % path)
    print("courses %d, poke lines %d, item lines %d" % (len(courses), n_pokes, n_items))
    print("widest names: course %d/%d px, Pokemon %d/%d px, item %d/%d px"
          % (widest["course"], LIMIT["course"], widest["poke"], LIMIT["poke"],
             widest["item"], LIMIT["item"]))
    print("species in two courses: %d" % (n_pokes - len({p[3] for c in courses for p in c["pokes"]})))
    print("pictures still to draw: %d %s" % (len(pending), " ".join(sorted(pending))))
    print("Pokemon with fewer than 4 moves (padded with 0): %d" % len(padded))
    for p in padded:
        print("  " + p)
    for e in errors:
        print("ERROR " + e)
    print("PASS" if not errors else "FAIL (%d errors)" % len(errors))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
