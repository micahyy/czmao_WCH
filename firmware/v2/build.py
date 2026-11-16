#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DS22Lite v2 —— 不依赖 QMK 的轻量 CH552 固件构建脚本。

工具链复用 ../toolchain（ch55xduino core + SDCC + WS2812 库），
编译产物 build/DS22Lite_v2.hex，可直接用 WCHISPTool 烧录。

用法： python build.py [--clean]
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TC = ROOT.parent / "toolchain"                 # 与旧固件共享工具链
SDCC_BIN = TC / "sdcc" / "bin"
SDCC = SDCC_BIN / "sdcc.exe"
SDAR = SDCC_BIN / "sdar.exe"
CORE_DIR = TC / "ch55x" / "cores" / "ch55xduino"
VAR_DIR = TC / "ch55x" / "variants" / "ch552"
WS2812_DIR = TC / "ch55x" / "libraries" / "WS2812" / "src"
WS2812_TPL = WS2812_DIR / "template"
INC_SDCC = TC / "sdcc" / "include"
LIB_SDCC = TC / "sdcc" / "lib" / "large_int_calc_stack_auto"

NO_PERSIST = "--no-persist" in sys.argv
ROLLBACK = "--rollback" in sys.argv
DIAG = "--diag" in sys.argv
if ROLLBACK:
    OUT = "DS22Lite_v2_rollback"
elif NO_PERSIST:
    OUT = "DS22Lite_v2_nopersist"
elif DIAG:
    OUT = "DS22Lite_v2_diag"
else:
    OUT = "DS22Lite_v2"
BUILD = ROOT / "build"
OBJ = BUILD / "obj"

INCS = [ROOT, ROOT / "src", ROOT / "src" / "usb", CORE_DIR,
        VAR_DIR, VAR_DIR / "include", WS2812_DIR, WS2812_TPL, INC_SDCC]
INCS += sorted({p for p in (ROOT / "src").rglob("*") if p.is_dir()})

CFLAGS = [
    "-MMD", "-c", "-Ddouble=float", "-DUSE_STDINT", "-D__PROG_TYPES_COMPAT__",
    "--model-large", "--int-long-reent", "--less-pedantic",
    "-mmcs51", "-DCH552", "-DF_CPU=24000000L", "-DF_EXT_OSC=0L",
    "-DARDUINO=10819", "-DARDUINO_ch55x", "-DARDUINO_ARCH_MCS51",
    "-DUSER_USB_RAM=266",
    "--include", "Arduino.h",
] + ["-I" + str(p) for p in INCS]


def run(cmd: list) -> subprocess.CompletedProcess:
    return subprocess.run([str(x) for x in cmd], capture_output=True,
                          text=True, errors="replace")


def fix_rel(rel: Path) -> None:
    try:
        text = rel.read_text(encoding="latin-1")
    except OSError:
        return
    for tag in ("CSEG", "GSINIT"):
        m = re.search(r"^A " + tag + r" size ([0-9A-F]+)", text, re.M)
        if m and int(m.group(1), 16) % 2:
            text = text.replace(m.group(0), "A %s size %X" % (tag, int(m.group(1), 16) + 1), 1)
    if rel.name.endswith("__main.rel"):
        text = text.replace("A GSFINAL size 3", "A GSFINAL size 4")
    rel.write_text(text, encoding="latin-1")


def hex_fill_holes(src: Path, dst: Path, rom_end: int) -> int:
    """把 Intel HEX 里被跳过的 0x00 空洞补齐。

    SDCC 的 ihx 写文件时会跳过值为 0x00 的字节（fix_rel 给奇数大小的
    CSEG/GSINIT 补的那 1 个字节就是 0x00）。烧录工具遇到地址空洞通常
    填 0xFF（擦除态），那就不是 NOP 了，会把指令流改坏。这里按链接
    器的真实镜像把空洞补成 0x00，输出一个完整无空洞的 hex。
    返回补上的字节数。
    """
    data: dict[int, int] = {}
    base = 0
    for line in src.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        n, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if rtype == 0x04:
            base = int.from_bytes(raw[4:4 + n], "big") << 16
        elif rtype == 0x02:
            base = int.from_bytes(raw[4:4 + n], "big") << 12
        elif rtype == 0x00:
            for i, b in enumerate(raw[4:4 + n]):
                data[base + addr + i] = b
    filled = 0
    for a in range(rom_end):
        if a not in data:
            data[a] = 0x00
            filled += 1
    out: list[str] = []
    for start in range(0, rom_end, 16):
        chunk = bytes(data.get(start + i, 0x00) for i in range(16))
        rec = bytearray([16, (start >> 8) & 0xFF, start & 0xFF, 0x00]) + bytearray(chunk)
        rec.append((-sum(rec)) & 0xFF)
        out.append(":" + rec.hex().upper())
    out.append(":00000001FF")
    dst.write_text("\n".join(out) + "\n", encoding="ascii")
    return filled


ISR_TO_DROP = ("Uart0_ISR", "Uart1_ISR", "INT0_ISR", "INT1_ISR", "TOUCHKEY_ISR")

# main.c 顶部的 Timer2/GPIO 前置 __interrupt 声明：函数体在 weakOverride*.c 里，
# 但声明留在 main.c 就会占 IVT 槽并强制链接器拉入那两个 .rel。v2 不用它们。
ISR_DECL_TO_DROP = ("Timer2Interrupt", "GPIOInterrupt")


def patch_core_main() -> Path:
    text = (CORE_DIR / "main.c").read_text(encoding="utf-8", errors="replace")
    # 只裁 UART/INT0/INT1 这几个用不到的 ISR，**绝不能裁 core 的 main()**：
    # SDCC 只把中断向量表（IVT）生成在定义了 main 的编译单元里。
    # 裁掉 core main() 后没人发 IVT（实测二分定位：只裁 main 即丢 IVT），
    # 链接产物 0x0000/0x000B/0x0033 全是垃圾：Timer0 第一次溢出 CPU 就
    # 乱飞、USB 中断永远进不去 —— 设备完全不枚举、主机无反应。
    # 入口沿用 core main()：init(); setup(); for(;;) loop();，
    # USER_USB_RAM 已定义，其循环体里的 USBSerial_flush 不会编入。
    for name in ISR_TO_DROP:
        m = re.search(r"void\s+" + name + r"\s*\(void\)", text)
        if not m:
            continue
        brace = text.find("{", m.end())
        if brace < 0:
            continue
        depth, end = 0, None
        for i in range(brace, len(text)):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    end = i + 1
                    break
        if end:
            text = (text[:m.start()] + "#if 0  /* v2 裁掉 */\n"
                    + text[m.start():end] + "\n#endif\n" + text[end:])
    for name in ISR_DECL_TO_DROP:
        text = re.sub(r"void\s+" + name + r"\s*\(void\)\s*__interrupt\s*\([^)]*\)\s*;",
                      "/* v2 裁掉: " + name + " */", text)
    dst = BUILD / "patched_core" / "main.c"
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text, encoding="utf-8")
    return dst


def compile_one(src: Path, tag: str, errors: list, extra: list | None = None) -> Path | None:
    stem = tag + "__" + src.stem
    o = OBJ / (stem + ".o")
    rel = OBJ / (stem + ".rel")
    r = run([SDCC] + CFLAGS + (extra or []) + [src, "-o", o])
    if r.returncode != 0 or not o.exists():
        errors.append((str(src.relative_to(ROOT)), (r.stderr or r.stdout or "").strip()[:1200]))
        return None
    shutil.copyfile(o, rel)
    fix_rel(rel)
    warn = (r.stderr or "").strip()
    if warn:
        print("  ! %s: %s" % (src.name, warn.splitlines()[0][:160]))
    return rel


def main() -> int:
    print("DS22Lite v2 (no-QMK) -> build/%s.hex" % OUT)
    if "--clean" in sys.argv and BUILD.exists():
        for p in (BUILD / (OUT + ".ihx"), BUILD / (OUT + ".hex"),
                  BUILD / (OUT + "_sdcc.hex"), BUILD / (OUT + ".mem"),
                  BUILD / "core.lib", BUILD / "optionalLink.lib"):
            if p.exists():
                p.unlink()
    OBJ.mkdir(parents=True, exist_ok=True)

    missing = [p for p in (SDCC, SDAR, CORE_DIR / "Arduino.h", LIB_SDCC) if not p.exists()]
    if missing:
        print("[FAIL] 工具链不完整：", *(str(p) for p in missing))
        return 2

    errors: list = []

    # 1) core（裁掉 main + 未用中断）
    print("[1/4] 编译 ch55xduino core ...")
    patched_main = patch_core_main()
    core_rels = []
    # v2 不用 Serial 打印 / USBCDC，且 USB 部分用自己的实现，
    # 排除这些模块省 ~150B XRAM + 大量 ROM
    CORE_SKIP = {
        "Print.c", "Print-float.c", "WMath.c",
        "HardwareSerial0.c", "HardwareSerial1.c",
        "HardwareSerial0ISR.c", "HardwareSerial1ISR.c",
        "genericPrintSerial0.c", "genericPrintSerial1.c",
        "genericPrintSerial0Float.c", "genericPrintSerial1Float.c",
        "genericPrintUSBSerial.c", "genericPrintUSBSerialFloat.c",
        "USBCDC.c", "USBconstant.c", "USBhandler.c", "debug_util.c",
    }
    for src in sorted(CORE_DIR.glob("*.c")):
        if src.name in CORE_SKIP:
            continue
        rel = compile_one(patched_main if src.name == "main.c" else src, "core", errors)
        if rel:
            core_rels.append(rel)
    core_lib = BUILD / "core.lib"
    if core_lib.exists():
        core_lib.unlink()
    r = run([SDAR, "rcs", core_lib] + core_rels)
    if r.returncode != 0:
        errors.append(("core.lib", (r.stderr or r.stdout or "").strip()[:800]))

    # 2) WS2812 库（按需链接 P1.0 汇编实现）
    print("[2/4] 编译 WS2812 库 ...")
    src_rels = []
    ws = compile_one(WS2812_DIR / "WS2812.c", "ws", errors)
    if ws:
        src_rels.append(ws)
    opt_rels = []
    for src in sorted(WS2812_TPL.glob("optionalLink_*.c")):
        rel = compile_one(src, "ws", errors)
        if rel:
            opt_rels.append(rel)
    opt_lib = BUILD / "optionalLink.lib"
    if opt_lib.exists():
        opt_lib.unlink()
    r = run([SDAR, "rcs", opt_lib] + opt_rels)
    if r.returncode != 0:
        errors.append(("optionalLink.lib", (r.stderr or r.stdout or "").strip()[:800]))

    # 3) v2 自有源码
    print("[3/4] 编译 v2 源码 ...")
    # --no-persist：排除持久化模块（对照档）；--rollback：完全回退到加持久化之前
    # --diag：完整版 + 自带诊断指示（蓝/绿/白 + 上电按住键跳过持久化）
    skip_persist = NO_PERSIST or ROLLBACK
    v2_defines = []
    if ROLLBACK:
        v2_defines = ["-DROLLBACK"]
    elif NO_PERSIST:
        v2_defines = ["-DNO_PERSIST"]
    elif DIAG:
        v2_defines = ["-DDIAG"]
    for src in sorted((ROOT / "src").rglob("*.c")):
        if skip_persist and src.name == "persist.c":
            continue
        rel = compile_one(src, "v2", errors, v2_defines or None)
        if rel:
            src_rels.append(rel)
    main_rel = compile_one(ROOT / "main.c", "v2", errors, v2_defines or None)
    if main_rel:
        src_rels.append(main_rel)

    if errors:
        print("\n[编译错误] %d 个：" % len(errors))
        for name, msg in errors:
            print("  --- %s ---\n%s" % (name, msg))
        return 1

    # 4) 链接
    print("[4/4] 链接 ...")
    ihx = BUILD / (OUT + ".ihx")
    if ihx.exists():
        ihx.unlink()
    # 宏持久化已迁到 DataFlash（见 config.h 分区），不再占用 CodeFlash 顶端区，
    # 所有档位都可用满 0x0000..0x37FF（14336B）用户区，0x3800 以上是 bootloader。
    link = ([SDCC, "--nostdlib", "-L" + str(BUILD), "-L" + str(LIB_SDCC),
             "--code-size", "14336",
             "--xram-size", "758", "--xram-loc", "266",
             "-mmcs51", "-DCH552"]
            + src_rels + [core_lib, opt_lib]
            + ["-lmcs51", "-llibsdcc", "-lliblong", "-lliblonglong",
               "-llibint", "-llibfloat", "--out-fmt-ihx", "-o", ihx])
    r = run(link)
    if r.returncode != 0 or not ihx.exists():
        print("[FAIL] 链接失败：\n%s" % ((r.stderr or r.stdout or "")[-5000:]))
        return 1

    hex_path = BUILD / (OUT + ".hex")
    raw_path = BUILD / (OUT + "_sdcc.hex")
    shutil.copyfile(ihx, raw_path)
    mem = BUILD / (OUT + ".mem")
    rom_end = 0
    if mem.exists():
        m = re.search(r"ROM/EPROM/FLASH\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)\s+(\d+)",
                      mem.read_text(encoding="latin-1", errors="replace"), re.I)
        if m:
            rom_end = int(m.group(1))
    if rom_end:
        filled = hex_fill_holes(raw_path, hex_path, rom_end)
        print("[OK] %s（补齐 %d 个 0x00 空洞，%d 字节）"
              % (hex_path, filled, hex_path.stat().st_size))
        print("      SDCC 原始输出: %s（%d 字节）"
              % (raw_path.name, raw_path.stat().st_size))
    else:
        shutil.copyfile(raw_path, hex_path)
        print("[OK] %s (%d 字节)" % (hex_path, hex_path.stat().st_size))
    if mem.exists():
        for line in mem.read_text(encoding="latin-1", errors="replace").splitlines():
            if re.search(r"ROM/EPROM/FLASH|EXTERNAL RAM|INTERNAL RAM|STACK", line):
                print("      " + line.strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
