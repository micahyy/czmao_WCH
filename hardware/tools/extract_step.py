"""从 EasyEDA Pro 导出的 STEP 中提取：PCB 板框、轴位、圆孔、元件位置。
输出 ../pcb/board_data.json 与 ../pcb/board_data.scad（供 OpenSCAD include）。
用法: 在 DS22Lite/ 目录下运行 python tools/extract_step.py pcb/xxx.step
     （输出路径按本脚本位置定位，与当前工作目录无关）
"""
import re
import sys
import json
import math
import collections
from pathlib import Path

sys.stdout.reconfigure(encoding="ascii", errors="backslashreplace")
text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")


def split_top(s):
    out, buf, depth, quoted = [], "", 0, False
    for ch in s:
        if ch == "'":
            quoted = not quoted
            buf += ch
        elif quoted:
            buf += ch
        elif ch == "(":
            depth += 1
            buf += ch
        elif ch == ")":
            depth -= 1
            buf += ch
        elif ch == "," and depth == 0:
            out.append(buf.strip())
            buf = ""
        else:
            buf += ch
    if buf.strip():
        out.append(buf.strip())
    return out


def refs(s):
    return [int(x) for x in re.findall(r"#(\d+)", s)]


def nums(s):
    return [float(x) for x in re.findall(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", s)]


pat = re.compile(r"(?<![A-Za-z0-9])#(\d+)[ \t]*=[ \t]*(?=[A-Z0-9_]+\s*\(|\()")
starts = [(m.start(), int(m.group(1))) for m in pat.finditer(text)]
ents = {}
for i, (s, eid) in enumerate(starts):
    e = starts[i + 1][0] if i + 1 < len(starts) else len(text)
    body = text[s:e].split(";", 1)[0].split("=", 1)[1].strip()
    m = re.match(r"([A-Z0-9_]+)\s*\((.*)\)\s*$", body, re.S)
    if m:
        ents[eid] = {"t": m.group(1), "a": split_top(m.group(2)), "raw": body}
    else:
        ents[eid] = {"t": "COMPLEX", "a": [], "raw": body}
print("records:", len(ents), "types:", len(set(e["t"] for e in ents.values())))


def T(i):
    return ents[i]["t"] if i in ents else ""


def A(i):
    return ents[i]["a"] if i in ents else []


def RAW(i):
    return ents[i]["raw"] if i in ents else ""


def ref1(i, idx):
    a = A(i)
    r = refs(a[idx]) if idx < len(a) else []
    return r[0] if r else None


def product_of_pd(pd):
    if T(pd) != "PRODUCT_DEFINITION":
        return None
    formation = ref1(pd, 2)
    if not formation or T(formation) != "PRODUCT_DEFINITION_FORMATION":
        return None
    prod = ref1(formation, 2)
    if not prod or T(prod) != "PRODUCT":
        return None
    return A(prod)[0].strip("'")


def cp_xyz(cp):
    if T(cp) != "CARTESIAN_POINT":
        return None
    v = nums(A(cp)[1])
    return v[:3] if len(v) >= 3 else None


def placement_origin(ap):
    if T(ap) == "AXIS2_PLACEMENT_3D":
        return cp_xyz(ref1(ap, 1))
    return None


# ---- 装配实例：名称 + 平移 ----
occ = {}
for i, e in ents.items():
    if e["t"] == "NEXT_ASSEMBLY_USAGE_OCCURRENCE":
        child = ref1(i, 4)
        occ[i] = {"name": product_of_pd(child), "xyz": None}

pds_occ = {}
for i, e in ents.items():
    if e["t"] == "PRODUCT_DEFINITION_SHAPE":
        r = ref1(i, 2)
        if r in occ:
            pds_occ[i] = r

for i, e in ents.items():
    if e["t"] != "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION":
        continue
    pds, rel = ref1(i, 1), ref1(i, 0)
    if pds not in pds_occ or not rel:
        continue
    m = re.search(r"REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION\s*\(\s*#(\d+)\s*\)", RAW(rel))
    if not m:
        continue
    trans = int(m.group(1))
    if T(trans) == "ITEM_DEFINED_TRANSFORMATION":
        target = ref1(trans, 3)
        occ[pds_occ[pds]]["xyz"] = placement_origin(target)

instances = [{"name": o["name"], "xyz": o["xyz"]} for o in occ.values() if o["name"]]
print("instances:", len(instances))

# ---- Board 实体的几何 ----
board_rep = None
for i, e in ents.items():
    if e["t"] == "SHAPE_DEFINITION_REPRESENTATION":
        pds = ref1(i, 0)
        pd = ref1(pds, 2) if pds else None
        name = product_of_pd(pd) if pd else None
        if name and name.startswith("Board~"):
            board_rep = ref1(i, 1)
            break
print("board representation:", board_rep)

seen, stack = set(), [board_rep]
while stack:
    x = stack.pop()
    if x in seen or x not in ents:
        continue
    seen.add(x)
    for r in refs(RAW(x)):
        if r not in seen:
            stack.append(r)
print("board geometry entities:", len(seen))

vpts = {}
for i in seen:
    if T(i) == "VERTEX_POINT":
        p = cp_xyz(ref1(i, 1))
        if p:
            vpts[i] = p
zs = collections.Counter(round(p[2], 3) for p in vpts.values())
print("board Z levels:", zs.most_common(8))
zmin, zmax = min(zs), max(zs)
thickness = zmax - zmin

edges = []
full_circles = []
for i in seen:
    if T(i) != "EDGE_CURVE":
        continue
    v1, v2, curve = ref1(i, 1), ref1(i, 2), ref1(i, 3)
    if v1 not in vpts or v2 not in vpts:
        continue
    p1, p2 = vpts[v1], vpts[v2]
    if abs(p1[2] - p2[2]) > 0.02 or abs(p1[2] - zmin) > 0.02 and abs(p1[2] - zmax) > 0.02:
        continue
    seq = None
    if T(curve) == "LINE":
        seq = [p1[:2], p2[:2]]
    elif T(curve) == "CIRCLE":
        c = placement_origin(ref1(curve, 1))
        rv = nums(A(curve)[2])
        if not c or not rv:
            continue
        r = rv[0]
        if (p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2 < 1e-8:
            full_circles.append((round(c[0], 4), round(c[1], 4), round(r, 4)))
            continue
        a1 = math.atan2(p1[1] - c[1], p1[0] - c[0])
        a2 = math.atan2(p2[1] - c[1], p2[0] - c[0])
        d = (a2 - a1) % (2 * math.pi)
        if d > math.pi:
            d -= 2 * math.pi
        n = 8
        seq = [p1[:2]] + [
            (c[0] + r * math.cos(a1 + d * k / n), c[1] + r * math.sin(a1 + d * k / n))
            for k in range(1, n)
        ] + [p2[:2]]
    if seq:
        edges.append((i, p1[:2], p2[:2], seq, p1[2]))


def key(p):
    return (round(p[0], 4), round(p[1], 4))


def loops_of(edge_list):
    adj = collections.defaultdict(list)
    for eid, p1, p2, seq, _z in edge_list:
        adj[key(p1)].append((key(p2), eid, seq))
        adj[key(p2)].append((key(p1), eid, list(reversed(seq))))
    used, out = set(), []
    for k in list(adj):
        for nbr, eid, _ in adj[k]:
            if eid in used:
                continue
            cur, ceid, pts = k, eid, []
            while ceid not in used:
                used.add(ceid)
                entry = next((x for x in adj[cur] if x[1] == ceid), None)
                if not entry:
                    break
                pts += entry[2][:-1]
                cur = entry[0]
                nxt = [x for x in adj[cur] if x[1] not in used]
                if not nxt:
                    break
                ceid = nxt[0][1]
            if len(pts) >= 3:
                out.append(pts)
    return out


levels = collections.defaultdict(list)
for e in edges:
    levels[round(e[4], 3)].append(e)
loops = []
for lv, es in levels.items():
    got = loops_of(es)
    print("z=", lv, "edges:", len(es), "loops:", len(got))
    loops += got


def area(poly):
    return abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                   for i in range(len(poly))) / 2)


loops.sort(key=area, reverse=True)
outline = [[round(x, 4), round(y, 4)] for x, y in loops[0]] if loops else []
print("loops:", [round(area(l), 2) for l in loops[:12]])
if outline:
    xs = [p[0] for p in outline]
    ys = [p[1] for p in outline]
    print("board bbox:", min(xs), max(xs), min(ys), max(ys), "thickness:", round(thickness, 4))
    print("outline points:", len(outline))

holes = sorted(set(full_circles))
if not holes:
    for l in loops[1:]:
        a = area(l)
        if 1.0 <= a < 30:
            cx = sum(p[0] for p in l) / len(l)
            cy = sum(p[1] for p in l) / len(l)
            holes.append((round(cx, 3), round(cy, 3), round(math.sqrt(a / math.pi), 3)))
    holes = sorted(set(holes))
print("PCB holes (dedup):", len(holes), holes[:8])

name_re = re.compile(r"^[A-Za-z0-9/+*\-]{1,8}$")
seen_xy, sw = set(), []
for o in instances:
    n = o["name"] or ""
    ref = n.split("~")[0]
    if "kailh" not in n.lower() or not o["xyz"] or not name_re.match(ref):
        continue
    x, y = round(o["xyz"][0], 4), round(o["xyz"][1], 4)
    if (x, y) in seen_xy:
        continue
    seen_xy.add((x, y))
    sw.append({"ref": ref, "x": x, "y": y,
               "z": round(o["xyz"][2], 4), "name": n})
sw.sort(key=lambda s: (-s["y"], s["x"]))
print("switches:", len(sw))
rows = collections.defaultdict(list)
for s in sw:
    rows[round(s["y"], 2)].append(s["ref"])
for y in sorted(rows, reverse=True):
    print("  row y=", y, ":", " ".join(rows[y]).encode("ascii", "backslashreplace").decode())

conn = [{"name": o["name"], "xyz": o["xyz"]} for o in instances
        if o["name"] and any(k in o["name"].lower() for k in ("usb", "type-c", "typec", "conn", "jst", "bat", "sw"))]
print("connectors/others:", len(conn))
usb = [o for o in instances if o["name"] and o["name"].startswith("USB1~") and o["xyz"]]
usb_xy = [round(usb[0]["xyz"][0], 4), round(usb[0]["xyz"][1], 4)] if usb else None
print("USB1:", usb_xy)

bbox = [min(p[0] for p in outline), min(p[1] for p in outline),
        max(p[0] for p in outline), max(p[1] for p in outline)] if outline else [0, 0, 0, 0]
inner = [[[round(x, 3), round(y, 3)] for x, y in l] for l in loops[1:] if area(l) < 1000]
data = {
    "units": "mm",
    "board_thickness": round(thickness, 4),
    "board_bbox": bbox,
    "usb_xy": usb_xy,
    "board_outline": outline,
    "pcb_holes": holes,
    "switches": sw,
    "instances": instances,
    "inner_loops": inner,
}
OUT = Path(__file__).resolve().parent.parent / "pcb"   # DS22Lite/pcb
(OUT / "board_data.json").write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")

scad = ["// 由 tools/extract_step.py 从 STEP 自动提取，勿手工编辑", "",
        "// 坐标 = STEP 原始坐标(mm)，原点在 PCB 左下角附近；+Y 边是 USB-C 侧", ""]
scad.append("board_t = %.4f;" % thickness)
scad.append("board_cx = %.4f; board_cy = %.4f;" % ((bbox[0] + bbox[2]) / 2, (bbox[1] + bbox[3]) / 2))
scad.append("board_w = %.4f; board_h = %.4f;" % (bbox[2] - bbox[0], bbox[3] - bbox[1]))
scad.append("usb_x = %.4f; usb_y = %.4f;" % (usb_xy[0], usb_xy[1]) if usb_xy else "usb_x = 0; usb_y = 0;")
scad.append("board_poly = [%s];" % ",".join("[%.4f,%.4f]" % tuple(p) for p in outline))
scad.append("switch_xy = [%s];" % ",".join("[%.4f,%.4f]" % (s["x"], s["y"]) for s in sw))
scad.append("switch_refs = [%s];" % ",".join('"%s"' % s["ref"] for s in sw))
scad.append("pcb_holes = [%s];" % ",".join("[%.3f,%.3f,%.3f]" % h for h in holes))
scad.append("")
(OUT / "board_data.scad").write_text("\n".join(scad), encoding="utf-8")
print("wrote %s / %s" % (OUT / "board_data.json", OUT / "board_data.scad"))
