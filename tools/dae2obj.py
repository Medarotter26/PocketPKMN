#!/usr/bin/env python3
"""dae2obj.py <model.dae> <out.obj> - minimal Collada -> OBJ + MTL converter for SketchUp exports (the 3DS model).

Ubuntu's Blender is built without Collada import, so the mockup renderer reads OBJ instead. Handles: node
transform matrices (nested), instance_geometry with bind_material, <triangles> with VERTEX/NORMAL/TEXCOORD inputs,
materials with a diffuse colour or texture (+ transparency). <lines> (edges) are skipped.
"""
import os
import sys
import xml.etree.ElementTree as ET

import numpy as np

src, out = sys.argv[1], sys.argv[2]
root = ET.parse(src).getroot()
NS = root.tag.split("}")[0][1:]
q = lambda tag: f"{{{NS}}}{tag}"  # noqa: E731

ids = {e.get("id"): e for e in root.iter() if e.get("id")}


def floats(e):
    return np.array([float(v) for v in (e.text or "").split()], dtype=float)


# materials -> effect -> diffuse colour / texture
images = {}
for img in root.iter(q("image")):
    f = img.find(q("init_from"))
    if f is not None:
        images[img.get("id")] = f.text
mats = {}
for m in root.iter(q("material")):
    inst = m.find(q("instance_effect"))
    eff = ids.get(inst.get("url")[1:]) if inst is not None else None
    col, tex, alpha = (0.8, 0.8, 0.8), None, 1.0
    if eff is not None:
        d = eff.find(f".//{q('diffuse')}")
        if d is not None:
            c = d.find(q("color"))
            if c is not None:
                v = floats(c)
                col = tuple(v[:3])
                alpha = float(v[3]) if len(v) > 3 else 1.0
            t = d.find(q("texture"))
            if t is not None:
                sampler = t.get("texture")
                # sampler -> surface -> image
                sp = eff.find(f".//{q('newparam')}[@sid='{sampler}']")
                img_id = None
                if sp is not None:
                    s = sp.find(f".//{q('source')}")
                    surf = eff.find(f".//{q('newparam')}[@sid='{s.text}']") if s is not None else None
                    if surf is not None:
                        img_id = surf.find(f".//{q('init_from')}").text
                tex = images.get(img_id)
        tr = eff.find(f".//{q('transparency')}/{q('float')}")
        if tr is not None:
            alpha = min(alpha, float(tr.text))
    mats[m.get("id")] = (col, tex, alpha)

V, VT, VN, faces = [], [], [], []  # faces: (material, [(v, vt, vn) x3])


def emit_geometry(geo, M, bind):
    mesh = geo.find(q("mesh"))
    if mesh is None:
        return
    sources = {s.get("id"): s for s in mesh.findall(q("source"))}
    verts = mesh.find(q("vertices"))
    pos_src = verts.find(q("input")).get("source")[1:]

    def arr(sid):
        s = sources[sid]
        a = floats(s.find(q("float_array")))
        stride = int(s.find(f".//{q('accessor')}").get("stride"))
        return a.reshape(-1, stride)

    P = arr(pos_src)
    P = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3]
    N3 = M[:3, :3]
    try:
        NI = np.linalg.inv(N3).T
    except np.linalg.LinAlgError:
        NI = N3
    for tri in mesh.findall(q("triangles")):
        inputs = tri.findall(q("input"))
        stride = max(int(i.get("offset")) for i in inputs) + 1
        offs = {}
        for i in inputs:
            offs[i.get("semantic")] = (int(i.get("offset")), i.get("source")[1:])
        p = tri.find(q("p"))
        if p is None:
            continue
        idx = np.array([int(v) for v in p.text.split()]).reshape(-1, stride)
        mat_sym = tri.get("material")
        mat = bind.get(mat_sym, mat_sym)
        base_v, base_t, base_n = len(V), len(VT), len(VN)
        V.extend(P.tolist())
        tex = arr(offs["TEXCOORD"][1]) if "TEXCOORD" in offs else None
        # The file's normals are left out: on SketchUp's double-sided faces many point the wrong way (renders
        # black / faceted). Blender computes them from the geometry instead.
        nor = None
        if tex is not None:
            VT.extend(tex[:, :2].tolist())
        if nor is not None:
            n = nor[:, :3] @ NI.T
            n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
            VN.extend(n.tolist())
        for k in range(0, len(idx), 3):
            f = []
            for row in idx[k:k + 3]:
                vi = base_v + row[offs["VERTEX"][0]] + 1
                ti = base_t + row[offs["TEXCOORD"][0]] + 1 if tex is not None else None
                ni = base_n + row[offs["NORMAL"][0]] + 1 if nor is not None else None
                f.append((vi, ti, ni))
            faces.append((mat, f))


def walk(node, M):
    m = node.find(q("matrix"))
    if m is not None:
        M = M @ floats(m).reshape(4, 4)
    for ig in node.findall(q("instance_geometry")):
        bind = {}
        for im in ig.iter(q("instance_material")):
            bind[im.get("symbol")] = im.get("target")[1:]
        emit_geometry(ids[ig.get("url")[1:]], M, bind)
    for child in node.findall(q("node")):
        walk(child, M)


scene = root.find(f".//{q('visual_scene')}")
for n in scene.findall(q("node")):
    walk(n, np.eye(4))

base = os.path.splitext(out)[0]
mtl = base + ".mtl"
src_dir = os.path.dirname(os.path.abspath(src))
with open(mtl, "w") as f:
    for mid, (col, tex, alpha) in mats.items():
        f.write(f"newmtl {mid}\nKd {col[0]:.4f} {col[1]:.4f} {col[2]:.4f}\nd {alpha:.3f}\n")
        if tex:
            f.write(f"map_Kd {os.path.join(src_dir, tex)}\n")
        f.write("\n")
with open(out, "w") as f:
    f.write(f"mtllib {os.path.basename(mtl)}\n")
    for v in V:
        f.write(f"v {v[0]:.5f} {v[1]:.5f} {v[2]:.5f}\n")
    for t in VT:
        f.write(f"vt {t[0]:.5f} {t[1]:.5f}\n")
    for n in VN:
        f.write(f"vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}\n")
    cur = None
    for mat, tri in faces:
        if mat != cur:
            f.write(f"usemtl {mat}\n")
            cur = mat
        f.write("f " + " ".join(f"{v}/{t or ''}/{n or ''}" for v, t, n in tri) + "\n")
print(f"{len(V)} vertices, {len(faces)} triangles, {len(mats)} materials -> {out}")
