#!/bin/bash
# sync_gen5_sprites.sh - brings the Gen 5 walker sprites from the sprite session (art_src/gen5two/out, 0x780-byte
# walker files, docs/PROMPT_APP_SPRITES_2026-11-15.md) into the app's romfs. Files only, no reconversion.
#   - NNN.bin (#494-649) and the male gender files 521_m / 592_m / 593_m -> romfs/sprites/pack/NNN.bin
#     (2026-11-15: 107 files; 2026-11-16: 153 + 3 = 156, of which the 52 of docs/PROMPT_APP_SPRITES_2026-11-16.md
#     were new - the others were already equal)
#   - Gen 5 forms -> romfs/sprites/forms/NNN_fK.bin with the games' form numbers (PKSM-Core i18n_form.cpp):
#     550B Basculin Blue-Striped f1, 555Z Darmanitan Zen f1, 585/586 S/A/W Summer/Autumn/Winter f1/f2/f3,
#     641T/642T/645T Therian f1, 646W White Kyurem f1, 646B Black Kyurem f2, 647R Keldeo Resolute f1,
#     (2026-11-16) 648P Meloetta Pirouette f1, Genesect 649B Douse f1 / 649Y Shock f2 / 649R Burn f3 / 649W Chill f4
#     (drive by the colour of art_src/gen5keys/anim/649X.png: B blue, Y yellow, R red, W white) - 19 files
#   - the female files 521_f / 592_f / 593_f are NOT copied: wbx_sprite_load_for cannot choose by gender.
# Delete pocketpkmn.3dsx / .cia before building afterwards (make does not notice romfs changes).
# Usage: bash tools/sync_gen5_sprites.sh          (copy)
#        bash tools/sync_gen5_sprites.sh --check  (only compare; exit 1 if any differs)
set -e
cd "$(dirname "$0")/.."
SRC=art_src/gen5two/out
PACK=romfs/sprites/pack
FORMS=romfs/sprites/forms
bad=0
n=0
one() { # one <source file> <destination file>
    local size
    size=$(stat -c %s "$1")
    if [ "$size" != 1920 ]; then echo "wrong size ($size): $1"; exit 1; fi
    if [ "$MODE" = "--check" ]; then
        cmp -s "$1" "$2" || { echo "differs: $2"; bad=1; }
    else
        cp "$1" "$2"
    fi
    n=$((n + 1))
}
MODE="$1"
for f in "$SRC"/[0-9][0-9][0-9].bin; do one "$f" "$PACK/$(basename "$f")"; done
for g in 521 592 593; do one "$SRC/${g}_m.bin" "$PACK/$g.bin"; done
one "$SRC/550B.bin" "$FORMS/550_f1.bin"
one "$SRC/555Z.bin" "$FORMS/555_f1.bin"
for d in 585 586; do
    one "$SRC/${d}S.bin" "$FORMS/${d}_f1.bin"
    one "$SRC/${d}A.bin" "$FORMS/${d}_f2.bin"
    one "$SRC/${d}W.bin" "$FORMS/${d}_f3.bin"
done
for d in 641 642 645; do one "$SRC/${d}T.bin" "$FORMS/${d}_f1.bin"; done
one "$SRC/646W.bin" "$FORMS/646_f1.bin"
one "$SRC/646B.bin" "$FORMS/646_f2.bin"
one "$SRC/647R.bin" "$FORMS/647_f1.bin"
one "$SRC/648P.bin" "$FORMS/648_f1.bin"
one "$SRC/649B.bin" "$FORMS/649_f1.bin"
one "$SRC/649Y.bin" "$FORMS/649_f2.bin"
one "$SRC/649R.bin" "$FORMS/649_f3.bin"
one "$SRC/649W.bin" "$FORMS/649_f4.bin"
if [ "$MODE" = "--check" ]; then
    [ $bad = 0 ] && echo "romfs Gen 5 sprites = $SRC ($n files)"
    exit $bad
fi
echo "copied $n Gen 5 sprite files (pack + 19 forms)"
