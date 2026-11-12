import re
import sys
import collections
from pathlib import Path

sys.stdout.reconfigure(encoding="ascii", errors="backslashreplace")
text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")


def show(typ, n=2, width=420):
    ms = list(re.finditer(r"#\d+\s*=\s*" + typ + r"\s*\(", text))
    print("\n==", typ, len(ms))
    for m in ms[:n]:
        s = text[m.start():m.start() + width].replace("\n", " ")
        print(s.encode("ascii", "backslashreplace").decode())


for t in ["EDGE_CURVE", "ORIENTED_EDGE", "EDGE_LOOP", "FACE_OUTER_BOUND",
          "ADVANCED_FACE", "VERTEX_POINT", "CIRCLE", "LINE",
          "B_SPLINE_CURVE_WITH_KNOTS", "PLANE"]:
    show(t)
