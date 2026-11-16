#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v2 固件静态校验：编解码往返 / 默认键表容量 / 资源占用 / 命令号一致性。"""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ok = fail = 0


def chk(cond: bool, msg: str) -> None:
    global ok, fail
    if cond:
        ok += 1
        print("  [OK] " + msg)
    else:
        fail += 1
        print(" [FAIL] " + msg)


def enc(kc: int) -> list[int]:
    hi, lo = kc >> 8, kc & 0xFF
    if kc <= 0xE7:
        return [lo]
    if hi in (0x01, 0x7E, 0x52, 0x78, 0x7C, 0x40, 0x20, 0x70):
        return [0xE8 + [0x01, 0x7E, 0x52, 0x78, 0x7C, 0x40, 0x20, 0x70].index(hi), lo]
    if hi == 0x7E:
        return [0xE9, lo]
    if hi == 0x78:
        return [0xEB, lo]
    if hi == 0x77:                     # 宏 KC_M0..M7 走 3B
        return [0xFF, lo, hi]
    return [0xFF, lo, hi]


def dec(b: list[int]) -> tuple[int, int]:
    t = b[0]
    if t <= 0xE7:
        return t, 1
    if t == 0xFF:
        return (b[2] << 8) | b[1], 3
    table = [0x01, 0x7E, 0x52, 0x78, 0x7C, 0x40, 0x20, 0x70]
    if 0xE8 <= t <= 0xEF:
        return (table[t - 0xE8] << 8) | b[1], 2
    return 0, 1


EMPTY = {(1, 4), (3, 4), (4, 3)}   # 与 config.h IS_EMPTY_CELL 一致


def delta_roundtrip(mods: dict, cap: int = 48):
    """模拟 keymap.c 的差量编码/解码（默认值一律取 0），返回 (是否无损, blob 长度或 None)。"""
    blob = [0xA5]
    for l in range(4):                                   # LAYER_COUNT
        cnt_pos = len(blob); blob.append(0); cnt = 0
        for r in range(5):
            for c in range(5):
                if (r, c) in EMPTY:
                    continue
                kc = mods.get((l, r, c), 0)
                if kc == 0:
                    continue                             # == 出厂默认，不存
                if len(blob) + 4 > cap:
                    return False, None                   # 溢出：放弃持久化（不越界写宏区）
                blob.append(r * 5 + c); blob += enc(kc); cnt += 1
        blob[cnt_pos] = cnt
    i = 1; got = {}
    if blob[0] != 0xA5:
        return False, len(blob)
    for l in range(4):
        if i >= cap:
            return False, len(blob)
        cnt = blob[i]; i += 1
        for _ in range(cnt):
            if i >= cap:
                return False, len(blob)
            pos = blob[i]; i += 1
            if pos >= 25 or i >= cap:
                return False, len(blob)
            kc, used = dec(blob[i:i + 3]); i += used
            if i > cap:
                return False, len(blob)
            r, c = pos // 5, pos % 5
            if (r, c) in EMPTY:
                continue
            got[(l, r, c)] = kc
    exp = {k: v for k, v in mods.items() if v != 0 and (k[1], k[2]) not in EMPTY}
    return got == exp, len(blob)


def hex_holes(path: Path, rom_end: int) -> list:
    """返回 Intel HEX 覆盖 0..rom_end-1 时缺失的地址列表（应为空）。"""
    cov = set()
    base = 0
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        n, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if rtype == 0x04:
            base = int.from_bytes(raw[4:4 + n], "big") << 16
        elif rtype == 0x00:
            cov.update(base + addr + i for i in range(n))
    return [a for a in range(rom_end) if a not in cov]


def hex_head(path: Path, n: int = 0x40) -> bytearray:
    """取 Intel HEX 镜像开头 n 字节（向量区）。"""
    mem = bytearray(n)
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        cnt, addr, rt = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if rt == 0 and addr < n:
            mem[addr:addr + cnt] = raw[4:4 + cnt]
        if rt == 1:
            break
    return mem


def main() -> int:
    print("=== 编解码往返（含 3B 宏键码） ===")
    samples = [0x00, 0x04, 0x1B, 0xE7, 0x0100 | 0x1B, 0x7E00, 0x7E03,
               0x7E15, 0x7800 | 0x0C, 0x7701, 0x7737, 0x1234, 0xABCD]
    bad = [s for s in samples if dec(enc(s))[0] != s]
    chk(not bad, "%d 种键码往返无损（坏值: %s）" % (len(samples), [hex(x) for x in bad]))

    print("=== 键表差量存储编解码往返 ===")
    ok0, n0 = delta_roundtrip({})
    chk(ok0 and n0 == 5, "出厂默认差量为空，仅 %sB（KM_MARK+4个0）=> 腾出宏区" % n0)
    m1 = {(0, 0, 0): 0x04, (0, 4, 0): 0x7E05, (1, 3, 0): 0x011B, (2, 0, 0): 0x7701}
    ok1, _ = delta_roundtrip(m1)
    chk(ok1, "混合 1B/2B/3B 键码的改动无损往返（含宏键 0x77xx）")
    ok2, _ = delta_roundtrip({(0, 1, 4): 0x1C})           # 落在物理空位：应被丢弃
    chk(ok2, "改动落在物理空位不破坏往返")
    many = {(l, r, c): 0x1234 for l in range(4) for r in range(5) for c in range(5)}
    okm, nm = delta_roundtrip(many)
    chk(nm is None or okm, "改动超出 48B 上限时安全放弃（返回不持久化，绝不越界）")

    print("=== 源码一致性 ===")
    km = (ROOT / "src/keymap.c").read_text(encoding="utf-8")
    cfg = (ROOT / "config.h").read_text(encoding="utf-8")
    board = (ROOT / "board.h").read_text(encoding="utf-8")
    chk("((uint16_t)in[2] << 8) | in[1]" in km, "3B 解码高低字节顺序正确")
    chk("#define DF_KEYMAP_CAP" in cfg and "DF_OFF_KEYMAP" in cfg, "DataFlash 分区宏齐全")
    chk(all(("#define LAYER_%d " % i) in board for i in range(4)), "board.h 有 4 层默认键表")
    chk("RGB_LED_COUNT           22" in cfg, "22 颗灯配置")
    rgb = (ROOT / "src/rgb.c").read_text(encoding="utf-8")
    hid = (ROOT / "src/usb/USBHID.c").read_text(encoding="utf-8")
    chk("if (!usb_configured()) return;" in rgb and "UsbConfig != 0" in hid,
        "RGB 枚举门控在位（WS2812 刷新关中断约 700us，枚举完成前禁止点灯）")

    print("=== DataFlash 持久化分区 ===")
    per = (ROOT / "src/persist.c").read_text(encoding="utf-8")
    km_off = int(re.search(r"#define DF_OFF_KEYMAP\s+(\d+)", cfg).group(1))
    km_cap = int(re.search(r"#define DF_KEYMAP_CAP\s+(\d+)", cfg).group(1))
    mac_off = km_off + km_cap
    mac_cap = 128 - mac_off
    chk("#define DF_OFF_MACRO" in cfg, "config.h 定义宏分区 DF_OFF_MACRO")
    chk(km_off + km_cap + mac_cap == 128,
        "DataFlash 分区填满 128B：magic/rgb@0..%d, keymap %d@%d, macro %d@%d"
        % (km_off - 1, km_cap, km_off, mac_cap, mac_off))
    mlen = int(re.search(r"#define MACRO_LEN\s+(\d+)", cfg).group(1))
    mcnt = int(re.search(r"#define MACRO_COUNT\s+(\d+)", cfg).group(1))
    chk(1 + mcnt * (1 + mlen) <= mac_cap,
        "宏 %d 条×(len+%d)=%dB + 标记1B 落在宏区 %dB 内（反复改写不越界）"
        % (mcnt, mlen, mcnt * (1 + mlen), mac_cap))
    chk("storage_read" in per and "storage_write" in per
        and "__asm" not in per and "SAFE_MOD" not in per
        and "GLOBAL_CFG" not in per and "#include <ch5xx.h>" not in per,
        "persist.c 走 DataFlash storage_read/write，已无 CodeFlash IAP 寄存器操作/MOVC"
        "（应用内不能擦 CodeFlash，是宏断电不保存的真凶）")
    bpy = (ROOT / "build.py").read_text(encoding="utf-8")
    chk('"14336"' in bpy and '"14200"' not in bpy,
        "链接 code-size 统一 14336（宏迁 DataFlash 后不再保留 CodeFlash 顶端区）")

    print("=== 产物 ===")
    for name in ("DS22Lite_v2", "DS22Lite_v2_rollback", "DS22Lite_v2_diag"):
        hexf = ROOT / ("build/%s.hex" % name)
        memf = ROOT / ("build/%s.mem" % name)
        if not memf.exists():
            print("  [--] %s 未构建，跳过" % name)
            continue
        mem = memf.read_text(encoding="latin-1", errors="replace")
        m = re.search(r"ROM/EPROM/FLASH\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)\s+(\d+)", mem, re.I)
        x = re.search(r"EXTERNAL RAM\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)", mem, re.I)
        if m:
            used, cap = int(m.group(1)), int(m.group(2))
            chk(used <= cap, "%s：ROM %d/%d B（余 %d）" % (name, used, cap, cap - used))
            chk(cap <= 0x3800, "%s：链接上限 %d <= 0x3800，bootloader 区不被程序占用"
                % (name, cap))
        if x:
            chk(int(x.group(1)) <= 758, "%s：XRAM %d/758 B" % (name, int(x.group(1))))
        if hexf.exists() and m:
            holes = hex_holes(hexf, int(m.group(1)))
            chk(not holes, "%s：hex 无地址空洞（烧录工具不会把空洞填成 0xFF 破坏指令流）"
                % name)
        # 入口 + 向量表守卫：SDCC 只把中断向量表生成在定义了 main 的
        # 编译单元里。历史上 build.py 裁掉 core main 后 IVT 消失，
        # Timer0/USB 向量是垃圾：设备不枚举、主机完全无反应。
        mapf = ROOT / ("build/%s.map" % name)
        if mapf.exists():
            has_main = re.search(r"^C:\s+[0-9A-Fa-f]+\s+_main\b",
                                 mapf.read_text(encoding="latin-1", errors="replace"), re.M)
            chk(bool(has_main), "%s：链接镜像含 _main 入口（setup/USBInit 可达）" % name)
        if hexf.exists():
            v = hex_head(hexf, 0x50)
            # CH552: INT_NO_USB=8 → 0x0B+8*8=0x43（0x3B 是 TKEY，不是 USB！）
            chk(v[0x00] == 0x02 and v[0x0B] == 0x02 and v[0x43] == 0x02,
                "%s：向量表在位（reset@0x0000/Timer0@0x000B/USB@0x0043 均为 LJMP）" % name)

    print("\nRESULT: %s（%d pass / %d fail）" % ("ALL PASS" if fail == 0 else "FAIL", ok, fail))
    return 1 if fail else 0


def check_baseline() -> None:
    """回退基线：--rollback 档必须与「已知能识别」的固件规格完全一致。
    这一档不编译 persist.c、不编译诊断代码（#ifdef DIAG 全部裁掉），
    用于验证固件骨架没被改坏。"""
    global ok, fail
    print("=== 回退基线（--rollback）===")
    memf = ROOT / "build/DS22Lite_v2_rollback.mem"
    if not memf.exists():
        print("  [--] 未构建 rollback 档，跳过")
        return
    mem = memf.read_text(encoding="latin-1", errors="replace")
    m = re.search(r"ROM/EPROM/FLASH\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)\s+(\d+)", mem, re.I)
    x = re.search(r"EXTERNAL RAM\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)", mem, re.I)
    if m:
        used, cap = int(m.group(1)), int(m.group(2))
        chk(used == 13781 and cap == 14336,
            "回退基线 ROM %d/%d B（须为 13781/14336，含 core main + IVT，已裁 "
            "TKEY/TMR2/GPIO 向量、TD 整链路移除、宏紧凑流状态机、键表差量存储、7 档物理灯效、恢复出厂延迟热复位）" % (used, cap))
        rom_end = used
    if x:
        chk(int(x.group(1)) == 583, "回退基线 XRAM %d B（须为 583，键表差量+MACRO_LEN=8 使宏区缩小）" % int(x.group(1)))
    rawf = ROOT / "build/DS22Lite_v2_rollback_sdcc.hex"
    if rawf.exists():
        chk(rawf.stat().st_size == 33453,
            "回退基线 SDCC 原始输出 %d B（须为 33453）" % rawf.stat().st_size)
    hexf = ROOT / "build/DS22Lite_v2_rollback.hex"
    if hexf.exists() and m:
        holes = hex_holes(hexf, int(m.group(1)))
        chk(not holes, "回退基线补洞版 hex 无空洞（%d B）" % hexf.stat().st_size)


if __name__ == "__main__":
    rc = main()
    check_baseline()
    print("\nTOTAL: %s（%d pass / %d fail）" % ("ALL PASS" if fail == 0 else "FAIL", ok, fail))
    raise SystemExit(1 if (fail or rc) else 0)
