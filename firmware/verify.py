#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""板卡配置自检：config.h / keymap.h / VIA json / SignalRGB 与框架枚举的一致性。

板名、USB 身份、目录、文件名与产物名统一取自 boards.json。
用法：python verify.py [--board <id>]
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from board_config import load, parse_board_arg, out_stem, board_dir, product_chars, vpid

ROOT = Path(__file__).resolve().parent
MANIFEST, BOARD = load(parse_board_arg())
KB = board_dir(BOARD)
SRC = ROOT / "src" / "qmk" / "quantum"
NAME = BOARD["name"]
VID = MANIFEST["vendorId"]
PID = BOARD["productId"]
ROWS, COLS = BOARD["matrix"]["rows"], BOARD["matrix"]["cols"]
LEDS, KEYS = BOARD["leds"], BOARD["keys"]
EMPTY = {tuple(p) for p in BOARD["emptyPositions"]}
LAYER_TOTAL = BOARD["layers"]["total"]
LAYER_DYN = BOARD["layers"]["dynamic"]
ok = True
print("板卡：%s  VID/PID %s/%s  vpid %d" % (NAME, VID, PID, vpid(BOARD, VID)))


def chk(cond: bool, msg: str) -> None:
    global ok
    print(("  OK   " if cond else "  FAIL ") + msg)
    if not cond:
        ok = False


def macro(text: str, name: str):
    m = re.search(r"^#define\s+" + name + r"\s+(.+?)\s*(?://.*)?$", text, re.M)
    if not m:
        return None
    # 行尾可能是 /* ... */ 块注释，正则里的 (?://.*)? 只吃掉 // 注释
    return re.sub(r"/\*.*?\*/", "", m.group(1)).strip()


def layer(text: str, name: str):
    m = re.search(r"#define " + name + r" \{(.*?)\n\}", text, re.S)
    return [[x.strip() for x in r.split(",")]
            for r in re.findall(r"\{([^{}]*)\}", m.group(1))]


def on(name: str) -> bool:
    return bool(re.search(r"^#define\s+" + name + r"\s*$", cfg, re.M))


print("=== config.h ===")
cfg = (KB / "config.h").read_text(encoding="utf-8")
chk(macro(cfg, "MATRIX_ROWS") == str(ROWS) and macro(cfg, "MATRIX_COLS") == str(COLS),
    "矩阵 %dx%d" % (ROWS, COLS))
chk(macro(cfg, "DIODE_DIRECTION") == "COL2ROW", "DIODE_DIRECTION = COL2ROW")
chk(macro(cfg, "MATRIX_ROW_PINS") == "{ 33, 34, 35, 13, 12 }", "ROW = P3.3/3.4/3.5/1.3/1.2")
chk(macro(cfg, "MATRIX_COL_PINS") == "{ 32, 14, 15, 16, 17 }", "COL = P3.2/1.4/1.5/1.6/1.7")
chk(macro(cfg, "WS2812_DI_PIN") == "10", "WS2812 DIN = P1.0")
chk(macro(cfg, "RGB_MATRIX_LED_COUNT") == str(LEDS), "%d 颗灯" % LEDS)

chk(on("TWO_LAYER_ENABLE"), "TWO_LAYER_ENABLE 开（VIA 上报 %d 层）" % LAYER_DYN)
chk(macro(cfg, "TOTAL_LAYER_COUNT") == str(LAYER_TOTAL),
    "TOTAL_LAYER_COUNT = %d（%d 动态 + %d 静态）"
    % (LAYER_TOTAL, LAYER_DYN, LAYER_TOTAL - LAYER_DYN))
chk(not on("DYNAMIC_MACRO_ENABLE"), "动态宏未启用")
chk(not on("TAP_DANCE_ENABLE"), "Tap Dance 未启用")
chk(not on("FCODE_TO_MOUSE_ENABLE"), "FCODE_TO_MOUSE 未启用")
chk(on("RGB_KEY_CONTROL") and on("RGB_EFFECTS_PLUS"), "RGB 按键控制与灯效启用")
chk(on("EXTRAKEY_ENABLE") and on("MOUSE_ENABLE"), "媒体键 / 鼠标键启用")
chk(not on("ENCODER_ENABLE"), "编码器未启用")
chk(not on("DIAL_ENABLE"), "DIAL 未启用")

# DataFlash 128B 分区实算：magic 1 + macro_loop 0 + RGB 5 = 6；动态层按矩阵尺寸算
loop_b = 5 if on("DYNAMIC_MACRO_ENABLE") else 0
rgb_b = 5 if on("RGB_EFFECTS_PLUS") and on("RGB_MATRIX_ENABLE") else 4
cfg_end = 1 + loop_b + rgb_b
keymap_b = LAYER_DYN * ROWS * COLS * 2
tap_b = 2 * 2 * 3 if on("TAP_DANCE_ENABLE") else 0
macro_b = 0 if not on("DYNAMIC_MACRO_ENABLE") else 128 - (cfg_end + keymap_b + tap_b)
used_b = cfg_end + keymap_b + tap_b + macro_b
chk(used_b == cfg_end + keymap_b + tap_b + macro_b and used_b <= 128,
    "DataFlash：cfg %dB + keymap %dB + tapdance %dB + macro %dB = %dB / 128B"
    % (cfg_end, keymap_b, tap_b, macro_b, used_b))

via_h = (SRC / "via.h").read_text(encoding="utf-8")
magic = re.search(r"^#define\s+VIA_MAGIC_CODE\s+(0x[0-9A-Fa-f]+)", via_h, re.M)
chk(magic is not None and magic.group(1).upper() == "0X5C",
    "VIA_MAGIC_CODE = 0x5C（配置变更时重建 EEPROM）")
chk(macro(cfg, "VENDOR_ID") == VID and macro(cfg, "PRODUCT_ID") == PID,
    "VID/PID = %s/%s" % (VID, PID))
chk(macro(cfg, "PRODUCT").replace(" ", "") == product_chars(NAME),
    "USB 产品串 = %s" % NAME)
chk(macro(cfg, "VENDOR_ID").upper() != "0XFEED", "VIA 3.0 拒收 0xFEED（已避开）")

print("=== boards.json ===")
chk(vpid(BOARD, VID) == BOARD["vpid"],
    "vpid = %d（= VID * 65536 + PID）" % BOARD["vpid"])
chk((KB / BOARD["viaJson"]).exists(), "VIA 定义文件 %s 存在" % BOARD["viaJson"])
chk((KB / BOARD["signalRgb"]).exists(), "SignalRGB 插件 %s 存在" % BOARD["signalRgb"])
chk(BOARD["viaDefinition"].endswith(BOARD["viaJson"]), "VIA 发布路径指向本板定义")
chk(NAME in BOARD["hex"], "产物名 %s 与板名对应" % BOARD["hex"])

print("=== USB ID ===")
chk(VID.upper() != "0XFEED" and PID.upper() != "0X0000",
    "USB VID/PID = %s/%s" % (VID, PID))

print("=== 框架：静态层 / 功能开关 ===")
dk_h = (SRC / "dynamic_keymap.h").read_text(encoding="utf-8")
dk_c = (SRC / "dynamic_keymap.c").read_text(encoding="utf-8")
qh = (SRC / "quantum.h").read_text(encoding="utf-8")
qm_c = (SRC / "quantum.c").read_text(encoding="utf-8")
via_c = (SRC / "via.c").read_text(encoding="utf-8")
action_c = (SRC / "action.c").read_text(encoding="utf-8")
dm_c = (SRC / "dynamic_macro.c").read_text(encoding="utf-8")
ss_c = (SRC / "send_string" / "send_string.c").read_text(encoding="utf-8")

chk("#ifndef TOTAL_LAYER_COUNT" in dk_h, "TOTAL_LAYER_COUNT 有默认值（上游其它键盘不受影响）")
chk("keymaps[TOTAL_LAYER_COUNT]" in dk_c, "keymaps[] 维度 = 4（静态层进 Flash）")
chk(dk_c.count("if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT)") == 2,
    "get / set 都对 layer>=2 分流静态层")
chk("command_data[0] = DYNAMIC_KEYMAP_LAYER_COUNT;" in via_c,
    "VIA 仍上报 2 层（协议不动，网页不会越界读 EEPROM）")
chk("case KC_TO_2:" in qm_c and "case KC_MO_3:" in qm_c, "layer_switch 有 TO(2)/TO(3)/MO(2)/MO(3)")
chk("keycode >= KC_TO_2 && keycode <= KC_MO_3" in qm_c,
    "process_record_quantum 有第二段层切换区间（枚举末尾不连续）")
chk("#ifdef DYNAMIC_MACRO_ENABLE" in dm_c and "dynamic_macro_reset(void) {}" in dm_c,
    "dynamic_macro.c 整包编出去且留空实现兜住调用点")
chk("#ifdef DYNAMIC_MACRO_ENABLE" in ss_c, "send_string.c 整包编出去")
chk("#ifdef DYNAMIC_MACRO_ENABLE" in via_c and "#ifdef DYNAMIC_MACRO_ENABLE" in action_c,
    "via.c / action.c 的宏调用点已条件编译")
chk(re.search(r"VIA_EEPROM_CUSTOM_MACRO_LOOP_SIZE\s+0", via_h) is not None,
    "via.h 的 macro_loop 区归零（RGB 配置前移 5B）")
chk(re.search(r"DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE\s+0", dk_h) is not None,
    "dynamic_keymap.h 宏区归零（VIA 上报 0 字节缓冲）")

print("=== keymap.h ===")
km = (KB / "keymap.h").read_text(encoding="utf-8")
LAYERS = [layer(km, "LAYER_%d" % i) for i in range(LAYER_TOTAL)]
chk(all(len(L) == ROWS and all(len(r) == COLS for r in L) for L in LAYERS),
    "%d 层都是 %dx%d" % (LAYER_TOTAL, ROWS, COLS))
for i, L in enumerate(LAYERS):
    no = {(r, c) for r in range(ROWS) for c in range(COLS) if L[r][c] == "KC_NO"}
    chk(no == EMPTY, "LAYER_%d KC_NO = %d 个物理空位" % (i, len(EMPTY)))
chk(LAYERS[0][4][0] == "KC_TO_1", "LAYER_0 FN(4,0) = TO(1) 切到层 1")
chk(LAYERS[1][0][0] == "KC_TO_0", "LAYER_1 (0,0) = TO(0) 切回层 0")
chk(LAYERS[2][4][0] == "KC_TO_0" and LAYERS[3][0][0] == "KC_TO_0"
    and LAYERS[3][4][0] == "KC_TO_0", "静态层 2/3 都有 TO(0) 回层键，切过去出得来")
chk(not any("KC_MO_" in x for L in LAYERS for r in L for x in r),
    "%d 层都没用按住式 MO(n)（避免松手粘层）" % LAYER_TOTAL)
kmaps = re.search(r"#define KEY_MAPS \{(.*?)\};", km, re.S)
chk(kmaps is not None and len(re.findall(r"LAYER_\d", kmaps.group(1))) == LAYER_TOTAL,
    "KEY_MAPS 正好 %d 层，与 TOTAL_LAYER_COUNT 一致" % LAYER_TOTAL)

m = re.search(r"#define RGB_CONFIG \{ \{(.*?)\n\}, \{", km, re.S)
rgb = [[x.strip() for x in r.split(",")]
       for r in re.findall(r"\{([^{}]*)\}", m.group(1))[:ROWS]]
norgb = {(r, c) for r in range(ROWS) for c in range(COLS) if rgb[r][c] == "NO_LED"}
chk(norgb == EMPTY, "RGB NO_LED 落在 %d 个物理空位（与 KC_NO 无关）" % len(EMPTY))
leds = sorted(int(x) for r in rgb for x in r if x.isdigit())
chk(leds == list(range(LEDS)), "LED 编号 0..%d 各一次，共 %d 颗" % (LEDS - 1, LEDS))

print("=== %s ===" % BOARD["viaJson"])
via_text = (KB / BOARD["viaJson"]).read_text(encoding="utf-8")
via = json.loads(via_text)
chk(via["name"] == NAME, "VIA 显示名 = %s" % NAME)
chk(all(ord(c) < 128 for c in via_text),
    "VIA json 全 ASCII（中文被 VIA 按 cp1252 解码会乱码）")
chk(via["matrix"] == {"rows": ROWS, "cols": COLS}, "matrix %d/%d" % (ROWS, COLS))
coords = {tuple(map(int, it.split(",")))
          for row in via["layouts"]["keymap"] for it in row if isinstance(it, str)}
chk(len(coords) == KEYS and coords == {(r, c) for r in range(ROWS) for c in range(COLS)} - EMPTY,
    "%d 个键位 == %d 位减去 %d 个物理空位" % (KEYS, ROWS * COLS, len(EMPTY)))
chk(all(t in coords for t in [(2, 4), (4, 4), (4, 2)]),
    "+ (2,4) / Enter (4,4) / . (4,2) 电气坐标")
chk(via["vendorId"] == VID and via["productId"] == PID
    and via["vendorId"].lower() == macro(cfg, "VENDOR_ID").lower()
    and via["productId"].lower() == macro(cfg, "PRODUCT_ID").lower(),
    "VID/PID 与 config.h 一致且非 0xFEED")

# VIA customKeycodes 的下标就是 QK_KB_0 + i，必须与 quantum.h 枚举顺序严格一致
enum = re.search(r"enum keycodes_user \{(.*?)\};", qh, re.S).group(1)
names = [x.split(",")[0].strip() for x in
         re.sub(r"//.*", "", enum).replace("BASE_CODE = QK_KB_0 - 1,", "").splitlines()
         if x.strip()]
names = [n for n in names if n and re.fullmatch(r"[A-Z0-9_]+", n)]
custom = via["customKeycodes"]
chk(len(names) == len(custom) == 25,
    "自定义键码 25 项，与 quantum.h 枚举等长（%d / %d）" % (len(names), len(custom)))
expect_head = ["DIAL_CCW", "DIAL_BUTTON", "DIAL_CW", "KC_TO_0", "KC_TO_1",
               "KC_MO_0", "KC_MO_1", "KC_TD_0", "KC_TD_1", "KC_TD_2",
               "KC_TD_3", "KC_TD_4", "RGB_MODE_U"]
chk(names[:13] == expect_head, "枚举前 13 项：3 Dial + 4 层切换 + 5 TD + RGB")
chk(names[-4:] == ["KC_TO_2", "KC_TO_3", "KC_MO_2", "KC_MO_3"],
    "静态层 4 项追加在枚举末尾（插中间会让后面全部错位）")
chk([k["name"] for k in custom][-4:] == ["TO(2)", "TO(3)", "FN(2)", "FN(3)"],
    "VIA json 末尾 4 个占位与枚举逐项对应")
chk("static" in custom[-4]["title"] and "static" in custom[-3]["title"],
    "TO(2)/TO(3) 标注静态层不可 VIA 改键")
chk("Switch to layer 1" == next((k["title"] for k in custom if k["name"] == "TO(1)"), ""),
    "TO(1) 在双层下是有效键")
chk(all("DISABLED" in k["title"] for k in custom if k["name"].startswith("TD(")),
    "5 个 TD 占位已标 DISABLED（枚举占位仍不能删）")
menu_labels = [m.get("label") for m in via.get("menus", [])]
chk("Tap Dance" not in menu_labels, "VIA 的 Tap Dance 菜单已移除")
chk("Macro Loop" not in menu_labels, "VIA 的 Macro Loop 菜单未启用")
chk("Lighting" in menu_labels and "System" in menu_labels, "Lighting / System 菜单保留")

print("=== 恢复出厂设置 ===")
chk("case id_eeprom_reset:" in via_c and "via_factory_reset();" in via_c,
    "标准 0x0A EEPROM Reset 已接通")
chk("case id_bootloader_jump:" in via_c and "via_bootloader_jump();" in via_c,
    "标准 0x0B Bootloader Jump 已接通")
chk("case QK_CLEAR_EEPROM:" in via_c and "case QK_BOOTLOADER:" in via_c,
    "EE_CLR / QK_BOOT 系统键值与 VIA 按钮共用一条路径")
chk("GLOBAL_CFG |= bSW_RESET" in via_c and "void via_task(void)" in via_c
    and "VIA_ACTION_DELAY_MS" in via_c,
    "回包后延迟重建 EEPROM + bSW_RESET 软复位（VIA 重枚举重读默认键位）")
chk("VIA 在 set 之后必补发 save" in via_c and "动作型控件在菜单加载时仍会 GET" in via_c,
    "动作按钮 GET 回 0、SAVE 显式吞掉（不重复触发、不回 unhandled）")
main_c = (ROOT / "src" / "CH55x_QMK_Lite.c").read_text(encoding="utf-8")
chk("via_task();" in main_c, "主循环调用 via_task()（延迟动作有机会执行）")
sys_menu = next((m for m in via.get("menus", []) if m.get("label") == "System"), {})
sys_items = [it for sub in sys_menu.get("content", []) for it in sub.get("content", [])]
chk(len(sys_items) == 2 and all(it.get("type") == "button"
                               and it.get("options") == [1] for it in sys_items),
    "VIA System 两个动作都是 button（toggle 会回弹 / 关回 0 重复触发）")

print("=== %s ===" % BOARD["signalRgb"])
js = (KB / BOARD["signalRgb"]).read_text(encoding="utf-8")
jskeys = re.search(r"const vKeys = \[(.*?)\];", js, re.S).group(1)
jspos = re.search(r"const vKeyPositions = \[(.*?)\];", js, re.S).group(1)
jsleds = [int(x) for x in re.findall(r"\b\d+\b", re.sub(r"//.*", "", jskeys))]
chk(sorted(jsleds) == list(range(LEDS)),
    "SignalRGB %d 个 LED 编号 0..%d 各一次" % (LEDS, LEDS - 1))
chk(len(re.findall(r"\[\s*\d+\s*,\s*\d+\s*\]", jspos)) == LEDS,
    "SignalRGB %d 个位置" % LEDS)
chk('return "%s"' % NAME in js and VID in js and PID in js and "0xFEED" not in js,
    "插件名称与 VID/PID（非 0xFEED）")
chk(not (ROOT / "keyboards" / "DC22").exists(), "历史 keyboards 目录已清理")

print("=== build/%s.mem ===" % out_stem(BOARD))
mem = ROOT / "build" / (out_stem(BOARD) + ".mem")
if mem.exists():
    line = next((l for l in mem.read_text(encoding="latin-1", errors="replace").splitlines()
                 if "ROM/EPROM/FLASH" in l), "")
    m = re.search(r"\s(\d+)\s+(\d+)\s*$", line.strip())
    used, limit = (int(m.group(1)), int(m.group(2))) if m else (-1, -1)
    chk(0 <= used <= 14336 and limit == 14336,
        "ROM %d/14336B（余 %dB，0x3800 以上不能碰）" % (used, 14336 - used))
else:
    chk(False, "未找到 .mem，请先跑 python build.py --clean --board %s" % BOARD["id"])

print("\nRESULT: " + ("ALL PASS" if ok else "HAS FAILURE"))
sys.exit(0 if ok else 1)
