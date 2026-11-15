"""render_3ds.py - renders the 3DS model (and later the walker model) for the UI mockups / the app.

Run: blender -b -P tools/render_3ds.py -- <model.dae> <out.png> [mode] [yaw_deg] [pitch_deg] [size]
  mode "views": six small views (front/back/left/right/top/3-4) side by side, to find the model's orientation
  mode "hero":  one render from (yaw, pitch), transparent background, soft studio light (the connect screen)
Model credit: "Nintendo 3DS", 3D Warehouse
(https://3dwarehouse.sketchup.com/model/fc5f72380c92667d78b5af3aa27f5807/Nintendo-3DS) - creator to be named.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
model, out = argv[0], argv[1]
mode = argv[2] if len(argv) > 2 else "views"
yaw = float(argv[3]) if len(argv) > 3 else 205.0
pitch = float(argv[4]) if len(argv) > 4 else 22.0
size = int(argv[5]) if len(argv) > 5 else 800

bpy.ops.wm.read_factory_settings(use_empty=True)
if model.lower().endswith(".obj"):
    bpy.ops.wm.obj_import(filepath=model, up_axis="Z", forward_axis="Y")  # converted by tools/dae2obj.py
elif model.lower().endswith((".gltf", ".glb")):
    bpy.ops.import_scene.gltf(filepath=model)
else:
    bpy.ops.wm.collada_import(filepath=model)
objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]

# Looks (argv 6, optional): "hide=ID1839;shell=ID96,ID343,ID551;color=0.55,0.04,0.04"
#  hide  = materials whose faces are removed (the 3DS model's flat backdrop quad at z = 0)
#  shell = materials repainted as glossy coloured plastic (the user's reference render: a red 3DS)
looks = dict(kv.split("=", 1) for kv in (argv[6].split(";") if len(argv) > 6 and argv[6] else []))
hide = set(filter(None, looks.get("hide", "").split(",")))
shell = set(filter(None, looks.get("shell", "").split(",")))
color = tuple(float(v) for v in looks.get("color", "0.55,0.04,0.04").split(",")) + (1.0,)
if hide:
    import bmesh
    for o in objs:
        names = [s.material.name if s.material else "" for s in o.material_slots]
        bm = bmesh.new()
        bm.from_mesh(o.data)
        kill = [f for f in bm.faces if f.material_index < len(names) and names[f.material_index].split(".")[0] in hide]
        bmesh.ops.delete(bm, geom=kill, context="FACES")
        bm.to_mesh(o.data)
        bm.free()
        print("HIDDEN faces", len(kill))
# Colour fix (user: "the colors of the 3DS model seem a little off compared to what I saw on the download page"):
# the OBJ/MTL colours are the model's sRGB screen colours, but Blender's Principled "Base Color" is linear ->
# convert sRGB -> linear for every untextured OBJ material; and use the "Standard" view transform (Blender 4's
# default AgX desaturates strong colours like the teal shell).
def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


if model.lower().endswith(".obj") and looks.get("linear", "1") == "1":
    for m in bpy.data.materials:
        if not m.use_nodes:
            continue
        b = m.node_tree.nodes.get("Principled BSDF")
        if b is None or b.inputs["Base Color"].is_linked:
            continue
        r_, g_, b_, a_ = b.inputs["Base Color"].default_value
        b.inputs["Base Color"].default_value = (srgb_to_linear(r_), srgb_to_linear(g_), srgb_to_linear(b_), a_)
bpy.context.scene.view_settings.view_transform = looks.get("view", "Standard")

#  gloss = materials made glossy with their own colour kept (user: "glossy blue ... the model I downloaded was blue")
#  paint = "ID:r,g,b|ID:r,g,b" - SketchUp's default back-face colour (ID96, lilac-grey) painted like the shell
gloss = set(filter(None, looks.get("gloss", "").split(",")))
paint = {}
for item in filter(None, looks.get("paint", "").split("|")):
    k, v = item.split(":")
    paint[k] = tuple(float(x) for x in v.split(",")) + (1.0,)
for m in bpy.data.materials:
    base = m.name.split(".")[0]
    if not m.use_nodes:
        continue
    b = m.node_tree.nodes.get("Principled BSDF")
    if not b:
        continue
    if base in shell:
        b.inputs["Base Color"].default_value = color
    if base in paint:
        b.inputs["Base Color"].default_value = paint[base]
    if base in shell or base in gloss or base in paint:
        b.inputs["Roughness"].default_value = 0.3
        if "Coat Weight" in b.inputs:  # glossy, but not a mirror (the walker reflected in the lid as a smudge)
            b.inputs["Coat Weight"].default_value = 0.35
            b.inputs["Coat Roughness"].default_value = 0.25
        print("SHELL", m.name)
mins = Vector((1e9, 1e9, 1e9))
maxs = Vector((-1e9, -1e9, -1e9))
for o in objs:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        mins = Vector((min(mins[i], w[i]) for i in range(3)))
        maxs = Vector((max(maxs[i], w[i]) for i in range(3)))
center = (mins + maxs) / 2
extent = max(maxs - mins)
print("MESHES", len(objs), "BOUNDS", tuple(round(v, 3) for v in mins), tuple(round(v, 3) for v in maxs))

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 48 if mode == "views" else 384
scene.cycles.use_denoising = False  # Ubuntu's Blender has no OpenImageDenoise: more samples instead
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
# User 2026-11-15: "the 3ds buttons and the pokewalker buttons the the rest of the 3ds top half should have outlines
# too" -> optional Freestyle line art (looks "lines=<px>"): silhouettes, sharp creases, borders and material
# boundaries in a dark green-grey, drawn in the render itself (inner edges included).
if float(looks.get("lines", "0")) > 0:
    import math as _math
    scene.render.use_freestyle = True
    scene.render.line_thickness_mode = "ABSOLUTE"
    scene.render.line_thickness = float(looks["lines"])
    vl = scene.view_layers[0]
    vl.use_freestyle = True
    fs = vl.freestyle_settings
    fs.crease_angle = _math.radians(float(looks.get("crease", "140")))
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new("lines")
    ls.select_by_visibility = True
    ls.visibility = "VISIBLE"
    ls.select_by_edge_types = True
    ls.select_silhouette = True
    ls.select_border = True
    ls.select_crease = True
    ls.select_material_boundary = True
    if ls.linestyle is None:
        ls.linestyle = bpy.data.linestyles.new("pocketpkmn_lines")
    ls.linestyle.color = (0.016, 0.026, 0.021)  # linear ~ (34, 44, 40) sRGB, as the 2D outline
    ls.linestyle.thickness = float(looks["lines"])
world = bpy.data.worlds.new("w")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.9, 0.92, 0.9, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6


def light(name, loc, energy, size_, shadow=True):
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = energy
    ld.size = size_
    if not shadow:  # the fill light sits on the camera's side: its shadow of the walker fell on the lid
        if hasattr(ld, "cycles") and hasattr(ld.cycles, "cast_shadow"):
            ld.cycles.cast_shadow = False
        ld.use_shadow = False
    lo = bpy.data.objects.new(name, ld)
    scene.collection.objects.link(lo)
    lo.location = loc
    d = center - Vector(loc)
    lo.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


r = extent * 2.2
light("key", center + Vector((-r, r, r * 1.2)), 160 * extent * extent, extent * 1.5)   # behind-left, high
light("fill", center + Vector((r, r * 0.4, r * 0.5)), 95 * extent * extent, extent * 2, shadow=False)
light("rim", center + Vector((-r * 0.8, -r * 1.2, r * 1.4)), 70 * extent * extent, extent * 1.5)  # off the walker's line

cam_data = bpy.data.cameras.new("cam")
cam_data.lens = 60
cam_data.clip_start = extent * 0.01  # the 3DS model is in inches (~500 units across)
cam_data.clip_end = extent * 20
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam


def aim(yaw_deg, pitch_deg, dist):
    y, p = math.radians(yaw_deg), math.radians(pitch_deg)
    cam.location = center + Vector((dist * math.cos(p) * math.sin(y), -dist * math.cos(p) * math.cos(y),
                                    dist * math.sin(p)))
    cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()


dist = extent * 2.6

if mode == "scene":
    # Walker model (argv 7): "Pokemon Heart Gold Soul Silver Pokewalker X512" by Callum Lax, CC BY 4.0
    # (https://sketchfab.com/3d-models/pokemon-heart-gold-soul-silver-pokewalker-x512-3ceece9eb4c3405ba28948529c24aea4)
    # Its screen texture (a real walker screen) is replaced by our LCD image (argv 8).
    from mathutils import Matrix
    walker_path, lcd_png = argv[7], argv[8]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=walker_path)
    wobjs = [o for o in bpy.data.objects if o not in before]
    # one parent for the whole walker
    pivot = bpy.data.objects.new("walker", None)
    scene.collection.objects.link(pivot)
    for o in wobjs:
        if o.parent is None:
            o.parent = pivot
    # 3DS: width 134 mm = 514 units -> 3.84 units/mm; walker ~46 mm across = 0.506 model units
    sc_ = 46 * (514 / 134.0) / 0.506
    ir = Vector((141.0, 322.0, 28.0))           # the 3DS's IR window: recess x 135-147, z 20-36 on the back edge y 318
    # Placement (user, round 8): "bring them further apart" (their edited picture: walker well to the right);
    # "do not have the walker directly face towards the screen. have the walker face completely upwards" ->
    # lying flat, screen to the sky, its top edge (IR window) pointing at the 3DS's IR window.
    cam_dir = Vector((math.sin(math.radians(yaw)), -math.cos(math.radians(yaw)), 0.0)).normalized()
    screen_right = Vector((-cam_dir.y, cam_dir.x, 0))  # the picture's "right" direction on the ground plane
    local_top = Vector((-0.658, 0.0, 0.253))
    sep = float(looks.get("walker_sep", "330"))    # along the picture's right, model units (3.84 per mm)
    lift = float(looks.get("walker_lift", "0"))    # height of the walker's top above the IR window
    top_world = ir + screen_right * sep + Vector((0, 0, lift))
    tdir = Vector((ir.x - top_world.x, ir.y - top_world.y, 0)).normalized()  # top edge -> the IR window
    # user: "rotate the walker clockwise just a little" (as seen from above: a negative turn about the up axis)
    rot = float(looks.get("walker_rot", "-18"))
    tdir = Matrix.Rotation(math.radians(rot), 3, "Z") @ tdir
    ndir = Vector((0, 0, 1.0))                                                # screen up
    side = tdir.cross(ndir)
    # local axes: -X = screen normal, +Z = top, Y = the rest (right-handed)
    R = Matrix((-ndir, -side, tdir)).transposed()
    T = top_world - (R @ (local_top * sc_))
    pivot.matrix_world = Matrix.Translation(T) @ R.to_4x4() @ Matrix.Scale(sc_, 4)
    import bmesh
    for o in wobjs:  # normals outward, no stored custom normals (they rendered the body black)
        if o.type != "MESH":
            continue
        if o.data.has_custom_normals:
            bpy.context.view_layer.objects.active = o
            o.data.free_normals_split() if hasattr(o.data, "free_normals_split") else None
            with bpy.context.temp_override(object=o, active_object=o):
                try:
                    bpy.ops.mesh.customdata_custom_splitnormals_clear()
                except Exception as e:  # noqa: BLE001
                    print("NORMALS", o.name, e)
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(o.data)
        bm.free()
    for m in bpy.data.materials:
        if not m.use_nodes:
            continue
        b = m.node_tree.nodes.get("Principled BSDF")
        if b is None:
            continue
        if m.name.startswith(("Base", "Buttons", "Strap", "Screen")):
            b.inputs["Metallic"].default_value = 0.0
            for l in list(m.node_tree.links):  # the metal/rough map would put metal back
                if l.to_socket == b.inputs["Metallic"]:
                    m.node_tree.links.remove(l)
        # flat look (user: the walker "looks too detailed compared to the 3ds model"): flattened colour textures
        # from tools/walker_flat_textures.py, no bump map, one even roughness
        flat_dir = looks.get("walker_flat", "art_src/render/walker_flat")
        if m.name.startswith(("Base", "Buttons", "Strap")) and os.path.isdir(flat_dir):
            for n in list(m.node_tree.nodes):
                if n.type == "TEX_IMAGE" and n.image and "baseColor" in n.image.name:
                    p = os.path.join(flat_dir, os.path.basename(n.image.filepath))
                    if os.path.exists(p):
                        n.image = bpy.data.images.load(os.path.abspath(p))
                        n.interpolation = "Closest"
                if n.type == "NORMAL_MAP":
                    for l in list(m.node_tree.links):
                        if l.from_node == n:
                            m.node_tree.links.remove(l)
            for l in list(m.node_tree.links):
                if l.to_socket == b.inputs["Roughness"]:
                    m.node_tree.links.remove(l)
            b.inputs["Roughness"].default_value = 0.45
        if m.name.startswith("Screen"):
            img = bpy.data.images.load(lcd_png)
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE":
                    n.image = img
            b.inputs["Roughness"].default_value = 0.15
    # frame both objects (matrix_world of the walker's parts is only recomputed after a view-layer update)
    bpy.context.view_layer.update()
    pts = []
    for o in bpy.context.scene.objects:  # real vertices (bound_box still counted the hidden backdrop plane)
        if o.type == "MESH" and len(o.data.vertices):
            step = max(1, len(o.data.vertices) // 4000)
            vs_ = o.data.vertices
            pts += [o.matrix_world @ vs_[i].co for i in range(0, len(vs_), step)]
    mins = Vector((min(p[i] for p in pts) for i in range(3)))
    maxs = Vector((max(p[i] for p in pts) for i in range(3)))
    center = (mins + maxs) / 2
    extent = max(maxs - mins)
    ratio = float(argv[9]) if len(argv) > 9 else 0.6  # height / width (the mockup panel's shape)
    scene.render.resolution_x = size
    scene.render.resolution_y = int(size * ratio)
    from bpy_extras.object_utils import world_to_camera_view
    # fit: move the camera until every corner of both models is inside the picture with a margin
    d_ = extent * 2.0
    for _ in range(6):
        aim(yaw, pitch, d_)
        bpy.context.view_layer.update()
        vs = [world_to_camera_view(scene, cam, p) for p in pts]
        worst = max(max(abs(v.x - 0.5), abs(v.y - 0.5)) for v in vs)
        d_ *= worst / 0.46
    aim(yaw, pitch, d_)
    # User 2026-11-15: "The signal picture should be flat, facing the ceiling, like the pokewalker and the 3ds" ->
    # looks "arcs=1": the IR signal as flat red arc bands lying in the walker's plane (facing up), from the walker's
    # IR window toward the 3DS's, built after the camera fit (they do not change the framing), no shadow, no lines.
    if looks.get("arcs", "0") == "1":
        coll = bpy.data.collections.new("ir_arcs")
        scene.collection.children.link(coll)
        to3 = Vector((ir.x - top_world.x, ir.y - top_world.y, 0))
        dist = to3.length
        to3.normalize()
        base_ang = math.atan2(to3.y, to3.x)
        # user: "Rotate the arcs clockwise" (seen from above: negative, as walker_rot)
        base_ang += math.radians(float(looks.get("arc_rot", "0")))
        z = top_world.z
        width = float(looks.get("arc_width", "9"))  # model units (3.84 per mm)
        red = tuple(srgb_to_linear(c / 255) for c in (226, 72, 40))
        for k, f in enumerate((0.16, 0.29, 0.42, 0.55)):
            r = dist * f
            span = math.radians(20 + k * 3)
            verts, faces = [], []
            n = 24
            for i in range(n + 1):
                a = base_ang - span + 2 * span * i / n
                d = Vector((math.cos(a), math.sin(a), 0))
                verts.append(top_world.xy.to_3d() + d * (r - width / 2) + Vector((0, 0, z)))
                verts.append(top_world.xy.to_3d() + d * (r + width / 2) + Vector((0, 0, z)))
                if i:
                    faces.append((2 * i - 2, 2 * i - 1, 2 * i + 1, 2 * i))
            me = bpy.data.meshes.new("ir_arc%d" % k)
            me.from_pydata(verts, [], faces)
            ob = bpy.data.objects.new("ir_arc%d" % k, me)
            coll.objects.link(ob)
            ob.visible_shadow = False
            mat = bpy.data.materials.new("ir_arc_mat%d" % k)
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            bsdf.inputs["Base Color"].default_value = tuple(red) + (1.0,)
            emis = bsdf.inputs.get("Emission Color") or bsdf.inputs.get("Emission")
            emis.default_value = tuple(red) + (1.0,)
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 1.0
            bsdf.inputs["Alpha"].default_value = 1.0 - k * 0.1
            ob.data.materials.append(mat)
        if scene.render.use_freestyle:  # no line art on the arcs
            for ls_ in scene.view_layers[0].freestyle_settings.linesets:
                ls_.select_by_collection = True
                ls_.collection = coll
                ls_.collection_negation = "EXCLUSIVE"
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    for name, p in (("IR3DS", ir), ("IRWALKER", top_world)):
        v = world_to_camera_view(scene, cam, p)
        print(name, round(v.x * scene.render.resolution_x), round((1 - v.y) * scene.render.resolution_y))
    print("SCENE", out)
elif mode == "views":
    import os
    scene.render.resolution_x = scene.render.resolution_y = 320
    shots = [("front", 0, 10), ("back", 180, 10), ("left", 90, 10), ("right", 270, 10), ("top", 0, 85),
             ("three-quarter", 205, 22)]
    paths = []
    for name, yv, pv in shots:
        aim(yv, pv, dist)
        p = f"{out}.{name}.png"
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        paths.append(p)
    print("VIEWS", paths)
else:
    scene.render.resolution_x = size
    scene.render.resolution_y = int(size * 0.6)
    aim(yaw, pitch, dist)
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("HERO", out)
