"""扫描 EasyEDA/OCCT 导出的 STEP，输出实体类型、产品名、坐标包围盒等摘要。
用法: python tools/inspect_step.py pcb/xxx.step
"""
import re
import sys
import json
import collections
from pathlib import Path

src = Path(sys.argv[1])
text = src.read_text(encoding="utf-8", errors="replace")
print("bytes:", src.stat().st_size)

types = collections.Counter(m.group(1) for m in re.finditer(r"#\d+\s*=\s*([A-Z0-9_]+)\s*\(", text))
print("\n-- 实体类型 (前 35) --")
for k, v in types.most_common(35):
    print(f"{k:38s} {v}")

print("\n-- PRODUCT 名称 (前 40) --")
prods = re.findall(r"=\s*PRODUCT\(\s*'([^']*)'\s*,\s*'([^']*)'", text)
names = [b or a for a, b in prods]
for i, n in enumerate(names[:40]):
    print(i, repr(n))
print("product count:", len(names))
print("\n-- 名称词频 (前 30) --")
for k, v in collections.Counter(names).most_common(30):
    print(f"{v:6d}  {k!r}")

pts = re.findall(r"CARTESIAN_POINT\(\s*'[^']*'\s*,\s*\(\s*([-\d.eE+]+)\s*,\s*([-\d.eE+]+)\s*,\s*([-\d.eE+]+)\s*\)\s*\)", text)
xyz = [(float(a), float(b), float(c)) for a, b, c in pts]
print("\n-- CARTESIAN_POINT --")
print("count:", len(xyz))
if xyz:
    for ax, label in enumerate("XYZ"):
        lo = min(p[ax] for p in xyz)
        hi = max(p[ax] for p in xyz)
        print(f"{label}: {lo:.4f} .. {hi:.4f}  (span {hi-lo:.4f})")

radii = [float(x) for x in re.findall(r"=\s*CIRCLE\(\s*'[^']*'\s*,\s*#\d+\s*,\s*([-\d.eE+]+)\s*\)", text)]
print("\n-- CIRCLE 半径词频 (前 25) --")
for k, v in collections.Counter(round(r, 4) for r in radii).most_common(25):
    print(f"{v:6d}  r={k}")
print("circle count:", len(radii))

out = {
    "types": types.most_common(),
    "products": names,
    "point_count": len(xyz),
    "radii": collections.Counter(round(r, 4) for r in radii).most_common(60),
}
out_path = Path(__file__).resolve().parent / "step_summary.json"
out_path.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
print("\nwrote %s" % out_path)
