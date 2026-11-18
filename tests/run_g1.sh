#!/bin/bash
# PC check of G1 (v0.89, docs/SCAN_SPEED_PLAN.md; owner: "Yes, start with G1"): every place in source/storage.cpp that
# writes a game save (writeUserSave, writeAndKeep, gbavc::write, gbavc::restore) must call stillOnCard( earlier in the
# same function - the save on the card is read again and compared before anything is written. storage.cpp needs the
# 3DS libraries, so this reads the source (as a code review would) instead of running it.
# Not checked here (they are the write helpers themselves): writeUserSave's own definition, dsWrite (called by
# writeUserSave; a DS game card is checked inside it), writeAndKeep's body. Usage: bash tests/run_g1.sh
set -e
cd "$(dirname "$0")/.."
awk '
  # a function starts at a line with no indent beyond 4 spaces that opens a definition: "    type name(...)" or
  # extern "C" ... - remember where, and whether stillOnCard( was called since
  /^(    )?[A-Za-z_][A-Za-z0-9_:<>*& ]* [*&]?[A-Za-z_][A-Za-z0-9_:]*\(.*[^;]$/ || /^extern "C"/ {
      fn = $0; fnline = NR; checked = 0
  }
  /stillOnCard\(/ && !/bool stillOnCard\(/ { checked = 1 }
  /writeUserSave\(|writeAndKeep\(|gbavc::write\(|gbavc::restore\(/ {
      if ($0 ~ /bool writeUserSave\(|bool writeAndKeep\(/) next          # the definitions
      if (fn ~ /bool writeAndKeep\(/) next                                 # the helper body (callers are checked)
      if ($0 ~ /^ *\/\//) next                                             # comments
      writes++
      if (!checked) { printf("  FAIL: line %d writes a game save without stillOnCard first (function at line %d)\n", NR, fnline); bad++ }
  }
  END {
      printf("G1: %d game-save write(s) found, %d without the check before them\n", writes, bad)
      if (writes < 6) { printf("  FAIL: expected at least 6 writes - the pattern no longer finds them\n"); bad++ }
      printf(bad ? "G1 TESTS: %d FAILED\n" : "G1 tests passed (%d failures)\n", bad)
      exit bad != 0
  }' source/storage.cpp

# v0.91 (review of v0.89, findings 1-3; owner: "Yes, fix 1-3"): source checks, as above (storage.cpp cannot run on a PC).
S=source/storage.cpp
bad=0
need() { # need <count> <fixed string> <what>
    local n
    n=$(grep -cF -- "$2" "$S" || true)
    if [ "$n" != "$1" ]; then echo "  FAIL: $3 (found $n, expected $1)"; bad=$((bad + 1)); fi
}
# 1: dsWrite's two refusals (another card / its save changed) say so; commitGame's Gen 4 write and the item write
#    clear the flag first; commitGame turns it into a refused write
need 2 "g_dsRefused = true; // v0.91: nothing written" "dsWrite marks both of its refusals"
need 3 "g_dsRefused = false;" "the flag: its declaration + cleared before the Gen 4 write and the item write"
need 1 "if (!ok && g_dsRefused && refused)" "commitGame reports a DS refusal as refused"
# 2/3: after an edit that was not written, the save in memory is never left edited: the only bare reloadSave(f) calls
#    left are inside commitGame (Gen 3, Gen 4: after the write, the callers then decide), Restore (no edit in memory)
#    and after a successful Gen 4 item write
n=$(grep -cE '^\s*reloadSave\(f\);' "$S" || true)
if [ "$n" != 5 ]; then echo "  FAIL: expected 5 bare reloadSave(f) calls, found $n"; grep -nE '^\s*reloadSave\(f\);' "$S"; bad=$((bad + 1)); fi
need 6 "afterRefused(f, beforeRaw)" "deposit + withdraw: safety check, box not saved, refused write"
need 2 "afterFailed(f, beforeRaw)" "deposit + withdraw: failed write"
need 2 "afterRefused(f, before);" "items: card changed / DS refused"
need 1 "afterFailed(f, before);" "items: failed write"
need 1 "else if (readAgain && countInSave(f, pk.get()) == countBefore)" "deposit: no count from a save not read again"
awk '/if \(f.unsure\)/ { u = NR } /int r = settleMove\(bbox, cur, f\);/ { if (!u || NR - u > 10) { print "  FAIL: settleMove in withdraw without the unsure check before it"; exit 1 } }' "$S" || bad=$((bad + 1))
need 1 "f.unsure = false;" "reloadSave clears unsure"
need 1 "f.unsure = true;" "afterFailed sets unsure"
if [ $bad = 0 ]; then echo "v0.91 fix checks passed (0 failures)"; else echo "v0.91 FIX CHECKS: $bad FAILED"; exit 1; fi
