#!/bin/bash
# Renders the connect-screen picture (3DS seen from behind + walker at its IR window) for the UI mockups / app:
#   art_src/render/connect_scene.png  (transparent background, 736 x 250 = the mockup panel at 2x)
#   art_src/render/connect_scene.txt  (pixel positions of the two IR windows, for the beam drawn on top)
# Models (credits): "Nintendo 3DS", 3D Warehouse (creator: ask the user) - 3DS Models/3DS.zip;
#   "Pokemon Heart Gold Soul Silver Pokewalker X512" by Callum Lax, CC BY 4.0 - 3DS Models/walker zip.
# Usage: bash tools/render_connect.sh [dex for the walker screen, default 658]
set -e
cd "$(dirname "$0")/.."
DEX="${1:-658}"
python3 -I - "$DEX" <<'EOF'
import sys
sys.path.insert(0, 'tools')
import ui_mockups as U
U.lcd_texture(int(sys.argv[1]), 8).convert('RGB').save(f'art_src/render/lcd_{sys.argv[1]}.png')
EOF
[ -d art_src/render/walker_flat ] || python3 -I tools/walker_flat_textures.py "3DS Models/walker_model" art_src/render/walker_flat
[ -f "3DS Models/3DS_model/model.obj" ] || python3 -I tools/dae2obj.py "3DS Models/3DS_model/model.dae" "3DS Models/3DS_model/model.obj"
blender -b -P tools/render_3ds.py -- "3DS Models/3DS_model/model.obj" art_src/render/connect_scene.png scene "${YAW:-120}" "${PITCH:-22}" 736 \
  "hide=ID1839,ID96;gloss=ID343,ID551" "3DS Models/walker_model/scene.gltf" "art_src/render/lcd_${DEX}.png" 0.34 2>&1 \
  > art_src/render/connect_scene.log
grep -E '^(IR3DS|IRWALKER) ' art_src/render/connect_scene.log > art_src/render/connect_scene.txt || {
  echo "render FAILED - see art_src/render/connect_scene.log"; grep -iE 'error|Traceback' art_src/render/connect_scene.log; exit 1; }
cat art_src/render/connect_scene.txt
