#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""比较两个 Intel HEX 的代码布局差异。

用途：判断新固件是在基线之上「尾部追加」，还是把基线整体位移了。
- 若首个差异地址很小（几百字节内）→ 布局被整体位移，任何绝对地址敏感的
  代码（中断向量、跳转表、USB 描述符常量区）都可能被打乱。
- 若首个差异地址很大、且基线区间逐字节相同 → 只是尾部追加，布局完好。
"""
from __future__ import annotations

import sys
from pathlib import Path


def load(path: Path) -> dict[int, int]:
    data: dict[int, int] = {}
    base = 0
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        count, addr_hi, addr_lo, rtype = raw[0], raw[1], raw[2], raw[3]
        addr = (addr_hi << 8) | addr_lo
        payload = raw[4:4 + count]
        if rtype == 0x04:
            base = int.from_bytes(payload, "big") << 16
        elif rtype == 0x02:
            base = int.from_bytes(payload, "big") << 12
        elif rtype == 0x00:
            for i, b in enumerate(payload):
                data[base + addr + i] = b
    return data


def main() -> int:
    if len(sys.argv) < 3:
        print("用法: python hexdiff.py <基线.hex> <新.hex>")
        return 2
    a = load(Path(sys.argv[1]))
    b = load(Path(sys.argv[2]))
    ka, kb = sorted(a), sorted(b)
    print("基线 %s: %d 字节, 0x%04X..0x%04X" % (Path(sys.argv[1]).name, len(a), ka[0], ka[-1]))
    print("新   %s: %d 字节, 0x%04X..0x%04X" % (Path(sys.argv[2]).name, len(b), kb[0], kb[-1]))

    common = [k for k in ka if k in b]
    diff = [k for k in common if a[k] != b[k]]
    only_a = [k for k in ka if k not in b]
    only_b = [k for k in kb if k not in a]

    print("共同地址 %d, 其中内容不同 %d" % (len(common), len(diff)))
    if diff:
        print("  首个不同地址 0x%04X (基线=%02X 新=%02X)" % (diff[0], a[diff[0]], b[diff[0]]))
        print("  最后不同地址 0x%04X" % diff[-1])
        # 找「完全一致的前缀长度」
        run = 0
        for k in common:
            if a[k] == b[k]:
                run += 1
            else:
                break
        print("  从 0x%04X 起连续一致 %d 字节" % (ka[0], run))
    if only_a:
        print("  仅基线有: %d 字节 (首个 0x%04X)" % (len(only_a), only_a[0]))
    if only_b:
        print("  仅新有  : %d 字节 (首个 0x%04X, 末个 0x%04X)" % (len(only_b), only_b[0], only_b[-1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
