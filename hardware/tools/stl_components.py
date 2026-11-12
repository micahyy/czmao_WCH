"""检查 STL(binary/ASCII) 的连通体数量与各连通体包围盒。
用法: 在 DS22Lite/ 目录下运行 python tools/stl_components.py case/output/xxx.stl
"""
import struct
import sys
import re
import collections

b = open(sys.argv[1], "rb").read()
tris = []
if b[:5] == b"solid" and b"facet" in b[:4000]:
    txt = b.decode("ascii", errors="replace")
    vs = [tuple(round(float(v), 3) for v in m.group(1).split())
          for m in re.finditer(r"vertex\s+([-\d.eE+\s]+?)\s*\n", txt)]
    tris = [vs[i:i + 3] for i in range(0, len(vs) - 2, 3)]
else:
    n = struct.unpack_from("<I", b, 80)[0]
    for i in range(n):
        vals = struct.unpack_from("<12fH", b, 84 + 50 * i)
        tris.append([tuple(round(v, 3) for v in vals[k:k + 3]) for k in (3, 6, 9)])

parent = {}


def find(x):
    while parent[x] != x:
        parent[x] = parent[parent[x]]
        x = parent[x]
    return x


def uni(a, c):
    ra, rc = find(a), find(c)
    if ra != rc:
        parent[ra] = rc


for vs in tris:
    for v in vs:
        if v not in parent:
            parent[v] = v
    uni(vs[0], vs[1])
    uni(vs[1], vs[2])

groups = collections.defaultdict(list)
for v in parent:
    groups[find(v)].append(v)
print(sys.argv[1])
print("triangles:", len(tris), " components:", len(groups))
for g in sorted(groups.values(), key=len, reverse=True):
    bounds = [(min(v[k] for v in g), max(v[k] for v in g)) for k in range(3)]
    print("  verts=%5d  bbox=%s" % (len(g), " ".join("[%.1f,%.1f]" % p for p in bounds)))
