#!/usr/bin/env python3
"""Check whether each Pokemon on the X/Y walker courses could be a legal
Pokemon obtained in Pokemon X or Pokemon Y.

Reads  romfs/routes/xy.txt  and the PokeAPI CSV tables in  art_src/pokeapi/ ,
prints a summary and writes  docs/ROUTE_LEGALITY_XY.md .

Run:  python3 -I tools/check_route_legality.py

Rules (agreed with the user; course species are never swapped):
* The met level is free: a Pokemon may have been met at any legal level at or
  below its course level and then grown. WILD_OK when some repeatable
  encounter range in that version has min level <= course level; the met level
  used is the highest legal level <= course level.
* EVO_FROM_WILD: the species is not wild itself, but an earlier member of its
  line is (as above), and every evolution step up to the course species is
  non-trade.  EVO_NEEDS_TRADE: a step on the way is a trade evolution (legal
  only with a traded-once history).  EVO_LEVEL: course level is below the
  evolution level.
* WILD_LEVEL: wild (it or a pre-evolution), but every range starts above the
  course level.  STATIC: only gift / static (one-time) encounters.
* Only when no line member is wild: EGG_OK (base form breedable, no trade
  step; met as an egg at Lv 1), EGG_LEVEL (course level below evolution
  level), TRADE_EVO, NOT_OBTAINABLE.
* User 2026-11-15 "Yes, use eggs": when no wild catch works at the course
  level (WILD_LEVEL, STATIC, EVO_LEVEL), the Pokemon is generated as hatched
  instead whenever the egg route is legal (EGG_OK), or EGG_NEEDS_TRADE when
  the line then needs a trade evolution (traded-once history), e.g. Timburr
  hatched -> Gurdurr -> Conkeldurr.
"""
import csv
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "art_src", "pokeapi")
ROUTES = os.path.join(ROOT, "romfs", "routes", "xy.txt")
OUT = os.path.join(ROOT, "docs", "ROUTE_LEGALITY_XY.md")

ALL_ST = ("WILD_OK", "EVO_FROM_WILD", "EGG_OK", "EVO_NEEDS_TRADE", "EGG_NEEDS_TRADE", "EVO_LEVEL",
          "WILD_LEVEL", "STATIC", "EGG_LEVEL", "TRADE_EVO", "NOT_OBTAINABLE")
GOOD = {"WILD_OK", "EVO_FROM_WILD", "EGG_OK"}
ONE_TIME = {"gift", "gift-egg", "static"}


def rows(name):
    with open(os.path.join(DATA, name + ".csv"), encoding="utf-8") as f:
        return list(csv.DictReader(f))


# ---------------------------------------------------------------- tables
versions = {r["identifier"]: r for r in rows("versions")}
VER = {"X": versions["x"]["id"], "Y": versions["y"]["id"]}
VG_XY = int(versions["x"]["version_group_id"])

methods = {r["id"]: r["identifier"] for r in rows("encounter_methods")}
slots = {r["id"]: r for r in rows("encounter_slots")}
areas = {r["id"]: r for r in rows("location_areas")}
loc_ident = {r["id"]: r["identifier"] for r in rows("locations")}
loc_en = {r["location_id"]: r["name"] for r in rows("location_names")
          if r["local_language_id"] == "9"}
cond_vals = {r["id"]: r["identifier"] for r in rows("encounter_condition_values")}
enc_conds = defaultdict(list)
for r in rows("encounter_condition_value_map"):
    enc_conds[r["encounter_id"]].append(cond_vals.get(r["encounter_condition_value_id"], "?"))
poke_species = {r["id"]: r["species_id"] for r in rows("pokemon")}
species = {r["id"]: r for r in rows("pokemon_species")}
egg_group_names = {r["id"]: r["identifier"] for r in rows("egg_groups")}
egg_groups = defaultdict(set)
for r in rows("pokemon_egg_groups"):
    egg_groups[r["species_id"]].add(egg_group_names[r["egg_group_id"]])
triggers = {r["id"]: r["identifier"] for r in rows("evolution_triggers")}
evos = defaultdict(list)          # evolved species id -> evolution rows
for r in rows("pokemon_evolution"):
    evos[r["evolved_species_id"]].append(r)


def sp_name(sid):
    return species[sid]["identifier"].replace("-", " ").title()


def place(area_id):
    a = areas[area_id]
    loc = loc_en.get(a["location_id"]) or loc_ident.get(a["location_id"], "?")
    if a["identifier"]:
        loc += " (" + a["identifier"].replace("-", " ") + ")"
    return loc


enc = {v: defaultdict(list) for v in VER}
vid_to_v = {vid: v for v, vid in VER.items()}
for r in rows("encounters"):
    v = vid_to_v.get(r["version_id"])
    if not v:
        continue
    sid = poke_species.get(r["pokemon_id"], r["pokemon_id"])
    where_ = place(r["location_area_id"])
    meth_ = methods[slots[r["encounter_slot_id"]]["encounter_method_id"]]
    if where_.startswith("Friend Safari"):
        meth_ = "friend-safari"           # PokeAPI files it as walk
    enc[v][sid].append({
        "method": meth_,
        "place": where_,
        "min": int(r["min_level"]), "max": int(r["max_level"]),
        "conds": sorted(set(enc_conds.get(r["id"], []))),
    })


def merged(encs):
    """Collapse to (place, method, conds) -> (min, max)."""
    m = {}
    for e in encs:
        k = (e["place"], e["method"], ",".join(e["conds"]))
        lo, hi = m.get(k, (99, 0))
        m[k] = (min(lo, e["min"]), max(hi, e["max"]))
    return m


def fmt(k, lo, hi):
    place_, meth, conds = k
    lv = "Lv %d" % lo if lo == hi else "Lv %d-%d" % (lo, hi)
    c = " [%s]" % conds if conds else ""
    return "%s, %s, %s%s" % (place_, meth, lv, c)


# ---------------------------------------------------------------- checks
def wild_check(v, sid, level):
    """(status, level, where) for the species itself; status None if not wild."""
    encs = enc[v].get(sid, [])
    rep = merged([e for e in encs if e["method"] not in ONE_TIME])
    once = merged([e for e in encs if e["method"] in ONE_TIME])
    if rep:
        ok = [(min(hi, level), k, lo, hi) for k, (lo, hi) in rep.items() if lo <= level]
        if ok:
            met = max(t[0] for t in ok)
            where = "; ".join(fmt(k, lo, hi) for m_, k, lo, hi in sorted(ok, key=lambda t: t[1])
                              if m_ == met)
            return "WILD_OK", met, where
        lo0 = min(lo for lo, hi in rep.values())
        where = "; ".join(fmt(k, lo, hi) for k, (lo, hi) in sorted(rep.items()) if lo == lo0)
        return "WILD_LEVEL", lo0, where
    if once:
        where = "; ".join(fmt(k, lo, hi) for k, (lo, hi) in sorted(once.items()))
        return "STATIC", min(lo for lo, hi in once.values()), where
    return None, None, ""


def evo_ok_rows(sid):
    """Evolution rows into sid that exist by Gen VI (rows tied to later games dropped)."""
    return [r for r in evos.get(sid, [])
            if not (r["version_group_id"] and int(r["version_group_id"]) > VG_XY)]


def line_of(sid):
    chain = [sid]
    while species[chain[-1]]["evolves_from_species_id"]:
        chain.append(species[chain[-1]]["evolves_from_species_id"])
    chain.reverse()                       # base ... sid
    return chain


def path_steps(chain):
    """Steps chain[0] -> chain[-1]: (need_level, trade_steps, texts) or None."""
    need, trade, steps = 1, [], []
    for prev, child in zip(chain, chain[1:]):
        rs = evo_ok_rows(child)
        if not rs:
            return None
        non_trade = [r for r in rs if triggers[r["evolution_trigger_id"]] != "trade"]
        if not non_trade:
            trade.append("%s -> %s" % (sp_name(prev), sp_name(child)))
            steps.append("%s by trade" % sp_name(child))
            continue
        best = None
        for r in non_trade:
            t = triggers[r["evolution_trigger_id"]]
            lv = int(r["minimum_level"]) if r["minimum_level"] else (2 if t == "level-up" else 1)
            if best is None or lv < best[0]:
                best = (lv, t, r)
        need = max(need, best[0])
        how = best[1]
        if best[2]["minimum_happiness"]:
            how = "happiness level-up"
        elif best[2]["trigger_item_id"]:
            how = "use-item"
        steps.append("%s by %s%s" % (sp_name(child), how,
                                     " Lv %d" % best[0] if best[2]["minimum_level"] else ""))
    return need, trade, steps


def evo_from_wild(v, sid, level):
    """Best earlier line member that is wild in v (None if none is)."""
    chain = line_of(sid)
    best = None
    for i in range(len(chain) - 2, -1, -1):
        m = chain[i]
        st, met, where = wild_check(v, m, level)
        if st not in ("WILD_OK", "WILD_LEVEL"):
            continue
        ps = path_steps(chain[i:])
        if ps is None:
            continue
        need, trade, steps = ps
        cand = {"member": m, "st": st, "met": met, "where": where,
                "need": need, "trade": trade, "steps": steps}
        rank = (st == "WILD_OK", level >= need, not trade,
                met if st == "WILD_OK" else -met)
        if best is None or rank > best[0]:
            best = (rank, cand)
    return best[1] if best else None


def egg_check(sid, level):
    """(status, min level, note) for hatching in X/Y."""
    chain = line_of(sid)
    base = chain[0]
    # babies (Pichu etc.) sit in the no-eggs group but hatch from their
    # evolved parents: judge breeding on the first non-baby form.
    breeder = base
    if species[base]["is_baby"] == "1":
        if len(chain) > 1:
            breeder = chain[1]
        else:
            kids = [s for s, r in species.items() if r["evolves_from_species_id"] == base]
            breeder = kids[0] if kids else base
    groups = egg_groups.get(breeder, set())
    if "ditto" in groups:
        return "NOT_OBTAINABLE", None, "Ditto cannot be bred; wild only"
    if "no-eggs" in groups or not groups:
        sp = species[breeder]
        why = "legendary" if sp["is_legendary"] == "1" else (
            "mythical" if sp["is_mythical"] == "1" else "Undiscovered egg group")
        return "NOT_OBTAINABLE", None, "cannot be bred (%s)" % why
    ps = path_steps(chain)
    if ps is None:
        return "NOT_OBTAINABLE", None, "no usable evolution path in Gen VI"
    need, trade, steps = ps
    note = "hatch %s as an egg (met Lv 1)" % sp_name(chain[0])
    if steps:
        note += ", then " + ", ".join(steps)
    if species[breeder]["gender_rate"] == "-1":
        note += " (genderless line: breed with Ditto)"
    if species[base]["is_baby"] == "1" and base != sid:
        note += " (baby may need an incense; parent can also hatch stage 1)"
    if level < need:
        return "EGG_LEVEL", need, note + "; needs Lv %d+" % need
    if trade:
        return "EGG_NEEDS_TRADE", need, note + " (needs a traded-once history for the trade step %s)"             % ", ".join(trade)
    return "EGG_OK", need, note


def judge(v, sid, level):
    st, lv, where = wild_check(v, sid, level)
    if st == "WILD_OK":
        o = "caught wild as %s, met Lv %d: %s" % (sp_name(sid), lv, where)
        return {"st": st, "lv": lv, "txt": o}
    ew = evo_from_wild(v, sid, level)
    if ew and ew["st"] == "WILD_OK" and level >= ew["need"]:
        o = "caught wild as %s, met Lv %d: %s; then %s" % (
            sp_name(ew["member"]), ew["met"], ew["where"], ", ".join(ew["steps"]))
        if ew["trade"]:
            return {"st": "EVO_NEEDS_TRADE", "lv": ew["met"],
                    "txt": o + " (needs a traded-once history for the trade step %s)"
                    % ", ".join(ew["trade"])}
        return {"st": "EVO_FROM_WILD", "lv": ew["met"], "txt": o}
    if st in ("WILD_LEVEL", "STATIC") or ew:
        parts = []
        if st == "WILD_LEVEL":
            parts.append("wild only from Lv %d: %s" % (lv, where))
        elif st == "STATIC":
            parts.append("one-time only: %s" % where)
        if ew and ew["st"] == "WILD_OK":
            parts.append("pre-evo %s wild (met Lv %d: %s) but evolving needs Lv %d+%s"
                         % (sp_name(ew["member"]), ew["met"], ew["where"], ew["need"],
                            " and a trade step (%s)" % ", ".join(ew["trade"])
                            if ew["trade"] else ""))
            ev_st, ev_lv = "EVO_LEVEL", ew["need"]
        elif ew:
            parts.append("pre-evo %s wild only from Lv %d: %s"
                         % (sp_name(ew["member"]), ew["met"], ew["where"]))
            ev_st, ev_lv = "WILD_LEVEL", max(ew["met"], ew["need"])
        else:
            ev_st, ev_lv = None, None
        if st == "WILD_LEVEL":
            out_st, out_lv = st, lv if ev_lv is None else min(lv, ev_lv)
        elif ev_st:
            out_st, out_lv = ev_st, ev_lv
        else:
            out_st, out_lv = st, lv
        est, elv, enote = egg_check(sid, level)
        if est in ("EGG_OK", "EGG_NEEDS_TRADE"):      # user: "Yes, use eggs"
            return {"st": est, "lv": elv, "txt": "%s (hatched instead, user rule; not usable wild: %s)"
                    % (enote, ". ".join(parts))}
        parts.append("egg route: %s - %s" % (est, enote))
        return {"st": out_st, "lv": out_lv, "txt": ". ".join(parts), "egg": est}
    est, elv, enote = egg_check(sid, level)
    return {"st": est, "lv": elv, "txt": "no line member wild in %s; %s" % (v, enote)}


# ---------------------------------------------------------------- input
def parse_routes():
    courses = []
    for line in open(ROUTES, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("course "):
            parts = [p.strip() for p in line.split("|")]
            courses.append({"n": int(parts[0].split()[1]), "name": parts[1], "pokes": []})
        elif line.startswith("poke "):
            data, _, rest = line.partition(";")
            f = data.split()
            courses[-1]["pokes"].append({
                "group": f[1], "sid": f[4], "level": int(f[5]),
                "name": rest.split(":")[0].strip()})
    return courses


# ---------------------------------------------------------------- main
def main():
    courses = parse_routes()
    total = sum(len(c["pokes"]) for c in courses)
    counts = {v: defaultdict(int) for v in VER}
    results = []
    for c in courses:
        for p in c["pokes"]:
            res = {v: judge(v, p["sid"], p["level"]) for v in VER}
            for v in VER:
                counts[v][res[v]["st"]] += 1
            results.append((c, p, res))

    md = []
    md.append("# Walker courses for X / Y: are the Pokemon legal?\n")
    md.append("Generated by `tools/check_route_legality.py` from `romfs/routes/xy.txt` "
              "(%d course Pokemon on %d courses) and PokeAPI's CSV tables in "
              "`art_src/pokeapi/` (github.com/PokeAPI/pokeapi, data/v2/csv). "
              "Re-run the script after changing the courses.\n" % (total, len(courses)))
    md.append("Each Pokemon is checked twice, once as if it goes into an **X** save and "
              "once into a **Y** save, because the walker gives every course to both. "
              "The course species are kept as they are.\n")
    md.append("The met level is free: a Pokemon may have been met at any legal level at or "
              "below its course level and then grown. An evolved species can be generated as "
              "if its pre-evolution was caught wild and then evolved.\n")
    md.append("**Categories** (checked in this order)\n")
    md.append("- **WILD_OK** - wild in that game with a repeatable method (grass, horde, "
              "surf, rods, rock smash, flowers, rough terrain, ambushes, berry trees) "
              "starting at or below the course level. Shown: met level to use and where.")
    md.append("- **EVO_FROM_WILD** - not wild itself, but an earlier member of its line is "
              "(met at or below the course level), and every evolution step up to the "
              "course species is non-trade (level-up, item, happiness).")
    md.append("- **EVO_NEEDS_TRADE** - as EVO_FROM_WILD, but a step is a trade evolution: "
              "legal only with a traded-once history (another trainer as past handler).")
    md.append("- **EVO_LEVEL** - a pre-evolution is wild, but the course level is below the "
              "evolution level.")
    md.append("- **WILD_LEVEL** - wild (itself or a pre-evolution), but every encounter "
              "starts above the course level. The lowest wild level and place are shown.")
    md.append("- **STATIC** - only a one-time gift or static encounter in that game.")
    md.append("- **EGG_OK** - hatched from a breedable parent and evolved without trading; met "
              "as an egg at Lv 1. Used when no line member is wild, and (user 2026-11-15: \"Yes, "
              "use eggs\") whenever the wild / gift origin does not fit the course level.")
    md.append("- **EGG_NEEDS_TRADE** - as EGG_OK, but a step is a trade evolution: legal only "
              "with a traded-once history.")
    md.append("- **EGG_LEVEL** - as EGG_OK, but the course level is below the evolution level.")
    md.append("- **TRADE_EVO** - no line member wild and only reachable by a trade evolution.")
    md.append("- **NOT_OBTAINABLE** - cannot be caught or hatched in that game (legendary, "
              "Undiscovered egg group, Ditto, ...).\n")
    md.append("For WILD_LEVEL, EVO_LEVEL and STATIC the egg route is shown too. Moves were "
              "not checked.\n")
    md.append("| Status | X | Y |\n|---|---|---|")
    for st in ALL_ST:
        md.append("| %s | %d | %d |" % (st, counts["X"][st], counts["Y"][st]))
    md.append("")

    def cell(r):
        return ("**%s** - %s" % (r["st"], r["txt"])).replace("|", "/")

    for c in courses:
        md.append("## Course %d - %s\n" % (c["n"], c["name"]))
        md.append("| Pokemon | Lv | X | Y |\n|---|---|---|---|")
        for cc, p, res in results:
            if cc is c:
                md.append("| %s | %d | %s | %s |" % (p["name"], p["level"],
                                                    cell(res["X"]), cell(res["Y"])))
        md.append("")

    md.append("## Needs a decision\n")
    md.append("Every entry that is not WILD_OK, EVO_FROM_WILD or EGG_OK in both X and Y, "
              "grouped by its worst status. The species stay as they are; each entry says "
              "which legal origin applies per game (met species, level, place).\n")
    order = ["NOT_OBTAINABLE", "TRADE_EVO", "STATIC", "WILD_LEVEL", "EVO_LEVEL",
             "EGG_LEVEL", "EVO_NEEDS_TRADE", "EGG_NEEDS_TRADE"]
    groups = defaultdict(list)
    for c, p, res in results:
        bad = [res[v]["st"] for v in VER if res[v]["st"] not in GOOD]
        if bad:
            groups[min(bad, key=order.index)].append((c, p, res))
    flagged = []
    for st in order:
        if not groups[st]:
            continue
        md.append("### %s\n" % st)
        for c, p, res in groups[st]:
            flagged.append((c, p, res))
            md.append("- **Course %d %s: %s Lv %d**" % (c["n"], c["name"], p["name"], p["level"]))
            if res["X"]["txt"] == res["Y"]["txt"] and res["X"]["st"] == res["Y"]["st"]:
                md.append("  - X and Y: **%s** - %s" % (res["X"]["st"], res["X"]["txt"]))
            else:
                for v in VER:
                    md.append("  - %s: **%s** - %s" % (v, res[v]["st"], res[v]["txt"]))
            hint = []
            lv = [res[v]["lv"] for v in VER
                  if res[v]["st"] in ("WILD_LEVEL", "EVO_LEVEL", "EGG_LEVEL") and res[v]["lv"]]
            if lv:
                hint.append("lowest course level that would be legal: Lv %d" % max(lv))
            if any(res[v].get("egg") == "EGG_OK" for v in VER):
                hint.append("or generate it as hatched (the egg route is legal at this level)")
            if hint:
                md.append("  - Option: " + "; ".join(hint) + ".")
        md.append("")

    md.append("## Data caveats\n")
    md.append("- PokeAPI's X/Y tables contain grass, horde, flowers, rough terrain, surf, "
              "rods, rock smash, ambush, berry-tree, gift and static encounters, and the "
              "Friend Safari (filed as walk at location Friend Safari, Lv 30, with a slot "
              "condition; shown here as method friend-safari and counted as a repeatable "
              "wild catch - it needs a friend's safari type and post-game access).")
    md.append("- Location area names are PokeAPI's; some X/Y areas only have a number "
              "(\"unknown area NNN\").")
    md.append("- PokeAPI's X/Y gift/static list is short (10 rows per game); a one-time "
              "encounter missing there shows as not wild.")
    md.append("- Evolution rows come from PokeAPI; rows tied to later games are ignored. "
              "Happiness and item evolutions are taken as possible from Lv 2 / any level.")
    md.append("- Moves, abilities, forms and Poke Ball were not checked.\n")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(md) + "\n")

    print("entries: %d" % total)
    for v in VER:
        print("%s: %s" % (v, ", ".join("%s=%d" % (s, counts[v][s]) for s in ALL_ST if counts[v][s])))
    print("needs a decision: %d" % len(flagged))
    for c, p, res in flagged:
        print("  course %d %s: %s Lv %d -> X %s, Y %s" % (
            c["n"], c["name"], p["name"], p["level"], res["X"]["st"], res["Y"]["st"]))
    print("wrote", OUT)


if __name__ == "__main__":
    sys.exit(main())
