#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""板卡清单加载。

boards.json 是所有板名、USB 身份与文件名的单一事实来源。
build.py / verify.py 都通过本模块取配置，避免板名散落在多个脚本里。
新增一块板时，只需在 boards.json 增加一条并创建对应的 keyboards/<id>/ 目录。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "boards.json"


def parse_board_arg(argv: list[str] | None = None) -> str | None:
    """从命令行解析 --board <id>（--board=<id> 亦可）。"""
    args = list(sys.argv[1:] if argv is None else argv)
    for i, a in enumerate(args):
        if a == "--board":
            if i + 1 >= len(args):
                raise SystemExit("--board 缺少板卡 id")
            return args[i + 1]
        if a.startswith("--board="):
            return a.split("=", 1)[1]
    return None


def validate(data: dict) -> None:
    """清单自检：id / PID / 产物名唯一，vpid 与计算值一致，目录名可作 C 宏。"""
    import re as _re
    seen_id, seen_pid, seen_hex = set(), set(), set()
    for b in data["boards"]:
        bid = b["id"]
        if bid in seen_id:
            raise SystemExit("boards.json 中板卡 id 重复：%s" % bid)
        seen_id.add(bid)
        pid = b["productId"].lower()
        if pid in seen_pid:
            raise SystemExit("boards.json 中 productId 重复：%s（VIA 靠 VID/PID 认设备）" % pid)
        seen_pid.add(pid)
        if b["hex"] in seen_hex:
            raise SystemExit("boards.json 中产物名重复：%s" % b["hex"])
        seen_hex.add(b["hex"])
        if not _re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", b["directory"]):
            raise SystemExit("板卡 %s 的 directory 不是合法 C 标识符：%s" % (bid, b["directory"]))
        if not _re.fullmatch(r"[A-Za-z0-9_-]+", b["name"]):
            raise SystemExit("板卡 %s 的 name 含 PRODUCT 字符列表不支持的字符" % bid)
        if vpid(b, data["vendorId"]) != b["vpid"]:
            raise SystemExit("板卡 %s 的 vpid 与 VID/PID 不一致" % bid)


def load(board_id: str | None = None):
    """返回 (manifest, board)。未指定时取 manifest['default']。"""
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    validate(data)
    boards = data["boards"]
    key = board_id or data["default"]
    hit = next((b for b in boards if b["id"] == key), None)
    if hit is None:
        known = ", ".join(b["id"] for b in boards)
        raise SystemExit("未知板卡 %r；boards.json 中可用：%s" % (key, known))
    return data, hit


def vpid(board: dict, vendor_id: str) -> int:
    """VIA 定义的 vpid = vendorId * 65536 + productId。"""
    return (int(vendor_id, 16) << 16) | int(board["productId"], 16)


def product_chars(name: str) -> str:
    """把显示名转成 config.h 里 PRODUCT 宏的字符列表。"""
    return ",".join("'%s'" % c for c in name)


def board_dir(board: dict) -> Path:
    return ROOT / "keyboards" / board["directory"]


def out_stem(board: dict) -> str:
    """编译产物名（不含扩展名），如 DS22Lite_CH552T。"""
    return Path(board["hex"]).stem
