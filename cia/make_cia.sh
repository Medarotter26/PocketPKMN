#!/bin/bash
# make_cia.sh - packs pocketpkmn.cia from the already built pocketpkmn.elf + romfs/ (run after "make" / build.ps1).
# Runs where makerom + bannertool run: WSL Ubuntu-24.04 itself (the downloaded Linux builds need glibc 2.38+, the
# devkitarm Docker image has 2.36). build.ps1 cia calls it. Usage (from anywhere):
#   bash cia/make_cia.sh            tools from $CIA_TOOLS (default ~/3ds-cia-tools)
# Title name, product code, unique ID are read from the Makefile (APP_TITLE, APP_DESCRIPTION, APP_AUTHOR,
# APP_PRODUCT_CODE, APP_UNIQUE_ID, ROMFS, TARGET).
set -e
cd "$(dirname "$0")/.."
TOOLS=${CIA_TOOLS:-$HOME/3ds-cia-tools}
MAKEROM=${MAKEROM:-$TOOLS/makerom}
BANNERTOOL=${BANNERTOOL:-$TOOLS/bannertool}
for t in "$MAKEROM" "$BANNERTOOL"; do
    [ -x "$t" ] || { echo "missing $t (docs/SESSION_CIA.md: how to get makerom + bannertool)"; exit 1; }
done
mk() { sed -n "s/^$1[[:space:]]*:=[[:space:]]*//p" Makefile | head -1; }
TARGET=$(mk TARGET); TITLE=$(mk APP_TITLE); DESC=$(mk APP_DESCRIPTION); AUTHOR=$(mk APP_AUTHOR)
CODE=$(mk APP_PRODUCT_CODE); UID_=$(mk APP_UNIQUE_ID); ROMFS=$(mk ROMFS)
[ -f "$TARGET.elf" ] || { echo "$TARGET.elf not built yet - run build.ps1 / make first"; exit 1; }
OUT=cia/out  # banner.bnr + icon.icn, made on each run
mkdir -p "$OUT"
"$BANNERTOOL" makebanner -i cia/banner.png -a cia/jingle.wav -o "$OUT/banner.bnr"
"$BANNERTOOL" makesmdh -s "$TITLE" -l "$DESC" -p "$AUTHOR" -i cia/icon.png -o "$OUT/icon.icn"
"$MAKEROM" -f cia -o "$TARGET.cia" -target t -exefslogo -elf "$TARGET.elf" -rsf cia/pocketpkmn.rsf \
    -banner "$OUT/banner.bnr" -icon "$OUT/icon.icn" \
    -DAPP_TITLE="$TITLE" -DAPP_PRODUCT_CODE="$CODE" -DAPP_UNIQUE_ID="$UID_" -DAPP_ROMFS="$ROMFS"
ls -l "$TARGET.cia"
echo "built ... $TARGET.cia (title ID 00040000$(printf '%06X' "$UID_")00)"
