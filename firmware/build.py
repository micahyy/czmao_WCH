#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DS22Lite / CH552T 本地构建 —— 复刻 Arduino IDE + ch55xduino 0.0.18 的编译流程。

用法：  python build.py            # 全量编译
        python build.py --clean    # 先清 build/ 再编译

产物：  build/DS22Lite_CH552T.hex   （可直接给 WCHISPTool / vnproch55x 烧录）
        build/DS22Lite_CH552T.mem   （ROM / XRAM 占用）

工具链在 toolchain/ 里，都是便携包，不需要装 Arduino IDE 或系统级 SDCC：
  sdcc/build.13407_3            SDCC mcs51 编译器
  ch55xduino-core-0.0.18        Arduino core + CH552 variant
  ch55xduino-tools-2023.06.01  wrapper / 烧录工具

编译参数逐条对齐 ch55xduino platform.txt 与 boards.txt 的
  CH552 / 24MHz internal / USER CODE w/ 266B ram / P3.6 bootloader pin
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TC = ROOT / "toolchain"
SDCC_BIN = TC / "sdcc" / "bin"
SDCC = SDCC_BIN / "sdcc.exe"
SDAR = SDCC_BIN / "sdar.exe"
CORE_DIR = TC / "ch55x" / "cores" / "ch55xduino"
VAR_DIR = TC / "ch55x" / "variants" / "ch552"
WS2812_DIR = TC / "ch55x" / "libraries" / "WS2812" / "src"
WS2812_TPL = WS2812_DIR / "template"
INC_SDCC = TC / "sdcc" / "include"
LIB_SDCC = TC / "sdcc" / "lib" / "large_int_calc_stack_auto"

BUILD = ROOT / "build"
OBJ = BUILD / "obj"
OUT = "DS22Lite_CH552T"

# Arduino IDE 会把 sketch/src 下所有目录都加进 include 路径
INCS = [ROOT, ROOT / "src", ROOT / "keyboards", CORE_DIR,
        VAR_DIR, VAR_DIR / "include", WS2812_DIR, WS2812_TPL, INC_SDCC]
INCS += sorted({p for p in (ROOT / "src").rglob("*") if p.is_dir()})

CFLAGS = [
    "-MMD", "-c", "-Ddouble=float", "-DUSE_STDINT", "-D__PROG_TYPES_COMPAT__",
    "--model-large", "--int-long-reent", "--less-pedantic",
    "-mmcs51", "-DCH552",
    "-DF_CPU=24000000L", "-DF_EXT_OSC=0L",
    "-DARDUINO=10819", "-DARDUINO_ch55x", "-DARDUINO_ARCH_MCS51",
    "-DUSER_USB_RAM=266",
    # WS2812.h 先 include template/WS2812_pins_header.h 再 include Arduino.h，
    # 头文件里用了 uint32_t 却没有 stdint 上下文。Arduino IDE 编译时会自动
    # 注入 Arduino.h，手工构建用 --include 复刻这一步。
    "--include", "Arduino.h",
] + ["-I" + str(p) for p in INCS]


def run(cmd: list) -> subprocess.CompletedProcess:
    return subprocess.run([str(x) for x in cmd], capture_output=True,
                          text=True, errors="replace")


def fix_rel(rel: Path) -> None:
    """复刻 ch55xduino tools/wrapper/sdcc.sh：CSEG/GSINIT 按偶数对齐。

    SDCC 的 mcs51 后端在奇数段起始地址上会产生时序问题，官方 wrapper
    编译完每个 .rel 都会做这个修正，手工构建必须照做。
    """
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


# 上游 README 第 8 条：功能开得多、ROM 不够时，可注释掉 main.c 里
# 本项目用不到的中断函数，多出约 1KB。键盘只用 USB 中断 + Timer0，
# 串口 / 外部中断 / GPIO 中断都不需要。这里自动生成 patched 副本，
# 不改动 toolchain/ 里的原文件。
ISR_TO_DROP = ("Uart0_ISR", "Uart1_ISR", "INT0_ISR", "INT1_ISR")


def patch_core_main() -> Path:
    text = (CORE_DIR / "main.c").read_text(encoding="utf-8", errors="replace")
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
            text = (text[:m.start()] + "#if 0  /* DS22Lite: 未使用的中断，省 ROM */\n"
                    + text[m.start():end] + "\n#endif\n" + text[end:])
    dst = BUILD / "patched_core" / "main.c"
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text, encoding="utf-8")
    return dst


def compile_one(src: Path, tag: str, errors: list) -> Path | None:
    stem = tag + "__" + src.stem
    o = OBJ / (stem + ".o")
    rel = OBJ / (stem + ".rel")
    r = run([SDCC] + CFLAGS + [src, "-o", o])
    if r.returncode != 0 or not o.exists():
        errors.append((str(src.relative_to(ROOT)), (r.stderr or r.stdout or "").strip()[:800]))
        return None
    shutil.copyfile(o, rel)
    fix_rel(rel)
    warn = (r.stderr or "").strip()
    if warn:
        print("  ! %s: %s" % (src.name, warn.splitlines()[0][:140]))
    return rel


def main() -> int:
    if "--clean" in sys.argv and BUILD.exists():
        # 不整目录删：build/obj 下几百个 .rel/.d 每次编译都会被覆盖重写，
        # 只删最终产物与 .lib 强制重新打包/链接即可。
        # （rmtree 一次删 400+ 文件会触发沙箱的批量删除保护）
        for p in (BUILD / (OUT + ".ihx"), BUILD / (OUT + ".hex"),
                  BUILD / (OUT + ".mem"), BUILD / "core.lib",
                  BUILD / "optionalLink.lib"):
            if p.exists():
                p.unlink()
    OBJ.mkdir(parents=True, exist_ok=True)

    missing = [p for p in (SDCC, SDAR, CORE_DIR / "Arduino.h", LIB_SDCC) if not p.exists()]
    if missing:
        print("[FAIL] 工具链不完整，缺：", *(str(p) for p in missing))
        print("       toolchain/ 下应有 sdcc/、ch55x/、tools/ 三个解压目录。")
        return 2

    errors: list = []

    # 1) core：全部编成 .rel 再打包成 .lib（archive 机制让链接器按需拉入，
    #    这样 sketch 里覆盖的 USBhandler.c / USBconstant.c 不会与 core 重复定义）
    print("[1/5] 编译 ch55xduino core（自动裁掉未用中断）...")
    patched_main = patch_core_main()
    core_rels = []
    for src in sorted(CORE_DIR.glob("*.c")):
        rel = compile_one(patched_main if src.name == "main.c" else src, "core", errors)
        if rel:
            core_rels.append(rel)
    core_lib = BUILD / "core.lib"
    if core_lib.exists():
        core_lib.unlink()
    r = run([SDAR, "rcs", core_lib] + core_rels)
    if r.returncode != 0:
        errors.append(("core.lib", (r.stderr or r.stdout or "").strip()[:800]))
    print("      core %d 个模块 -> core.lib" % len(core_rels))

    # 2) WS2812 库：16 个脚位实现带 optionalLink 前缀，按官方 wrapper 的做法
    #    打包成 optionalLink.lib，链接器只会拉入实际被调用的 neopixel_show_Px_y。
    print("[2/5] 编译 WS2812 库 ...")
    src_rels = []
    ws = compile_one(WS2812_DIR / "WS2812.c", "ws2812", errors)
    if ws:
        src_rels.append(ws)
    opt_rels = []
    for src in sorted(WS2812_TPL.glob("optionalLink_*.c")):
        rel = compile_one(src, "ws2812", errors)
        if rel:
            opt_rels.append(rel)
    opt_lib = BUILD / "optionalLink.lib"
    if opt_lib.exists():
        opt_lib.unlink()
    r = run([SDAR, "rcs", opt_lib] + opt_rels)
    if r.returncode != 0:
        errors.append(("optionalLink.lib", (r.stderr or r.stdout or "").strip()[:800]))
    print("      %d 个脚位实现 -> optionalLink.lib（链接器按需拉入）" % len(opt_rels))

    # 3) sketch src：框架全部 .c 直接参与链接（不进 archive）
    print("[3/5] 编译 CH55x_QMK_Lite 框架与 DS22Lite 配置 ...")
    for src in sorted((ROOT / "src").rglob("*.c")):
        rel = compile_one(src, "sketch", errors)
        if rel:
            src_rels.append(rel)
    main_rel = compile_one(ROOT / "sketch_main.c", "sketch", errors)
    if main_rel:
        src_rels.append(main_rel)

    if errors:
        print("\n[编译错误] %d 个：" % len(errors))
        for name, msg in errors:
            print("  --- %s ---\n%s" % (name, msg))
        return 1
    print("      sketch %d 个模块" % len(src_rels))

    # 3) 链接（参数逐条对齐 platform.txt / boards.txt）
    print("[4/5] 链接 ...")
    ihx = BUILD / (OUT + ".ihx")
    if ihx.exists():
        ihx.unlink()
    link = ([SDCC, "--nostdlib", "-L" + str(BUILD), "-L" + str(LIB_SDCC),
             "--code-size", "14336", "--xram-size", "758", "--xram-loc", "266",
             "-mmcs51", "-DCH552"]
            + src_rels + [core_lib, opt_lib]
            + ["-lmcs51", "-llibsdcc", "-lliblong", "-lliblonglong",
               "-llibint", "-llibfloat", "--out-fmt-ihx", "-o", ihx])
    r = run(link)
    if r.returncode != 0 or not ihx.exists():
        print("[FAIL] 链接失败：\n%s" % ((r.stderr or r.stdout or "")[-4000:]))
        return 1

    # 4) 输出 hex（sdcc --out-fmt-ihx 产出的就是标准 Intel HEX）
    hex_path = BUILD / (OUT + ".hex")
    shutil.copyfile(ihx, hex_path)
    mem = BUILD / (OUT + ".mem")
    print("[5/5] 完成：%s (%d 字节)" % (hex_path, hex_path.stat().st_size))
    if mem.exists():
        for line in mem.read_text(encoding="latin-1", errors="replace").splitlines():
            if re.search(r"ROM/EPROM/FLASH|EXTERNAL RAM|INTERNAL RAM|STACK", line):
                print("      " + line.strip())
    print("      ROM 上限 14336B / XRAM 758B（USB 占 0..265）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
