import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
for typ in [
    "NEXT_ASSEMBLY_USAGE_OCCURRENCE",
    "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
    "ITEM_DEFINED_TRANSFORMATION",
    "PRODUCT_DEFINITION_SHAPE",
    "SHAPE_DEFINITION_REPRESENTATION",
    "MANIFOLD_SOLID_BREP",
    "ADVANCED_FACE",
]:
    ms = list(re.finditer(r"(?m)^\s*#\d+\s*=\s*" + typ + r"\s*\(", text))
    print("==", typ, len(ms))
    for m in ms[:2]:
        print(text[m.start():m.start() + 320].replace("\n", " "))

prods = re.findall(r"=\s*PRODUCT\(\s*'([^']*)'\s*,\s*'([^']*)'", text)
names = [b or a for a, b in prods]
print("\n== 所有 kailh / switch 相关产品名 ==")
for i, n in enumerate(names):
    if any(k in n.lower() for k in ("kailh", "switch", "sw", "mx", "usb", "type-c", "hole")):
        print(i, n.encode("ascii", "backslashreplace").decode())
