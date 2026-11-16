# -*- coding: utf-8 -*-
"""VIA 通道探针 + 矩阵实时监视（零第三方依赖，ctypes 走 Windows HID API）。

用法（键盘插在本机上）：
    python via_probe.py            # 握手体检：逐条 VIA 命令发出去、打印回包
    python via_probe.py matrix     # 循环轮询矩阵状态，按键实时打印（测 col0 用）

原理：VIA 走 usage page 0xFF60 的 Raw HID 集合（本板 = 接口 1 / EP2），
Windows 给每个顶层集合一个独立设备接口，直接打开它读写 32 字节报告，
不依赖 VIA 应用——固件回包对不对，一眼看穿。
"""
import ctypes
import ctypes.wintypes as wt
import struct
import sys

Vid = 0x4A17
Pid = 0x4A51
Payload = 32                      # EP2_SIZE，与 config.h 一致

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid_dll = ctypes.WinDLL("hid", use_last_error=True)

INVALID = wt.HANDLE(-1).value

# 64 位下必须声明返回/参数为 HANDLE，否则指针被截断成 int
kernel32.CreateFileW.restype = wt.HANDLE
kernel32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID,
                                 wt.DWORD, wt.DWORD, wt.HANDLE]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.SetCommTimeouts.argtypes = [wt.HANDLE, wt.LPVOID]
kernel32.WriteFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD,
                               wt.LPDWORD, wt.LPVOID]
kernel32.ReadFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD,
                              wt.LPDWORD, wt.LPVOID]
setupapi.SetupDiGetClassDevsW.restype = wt.HANDLE
setupapi.SetupDiDestroyDeviceInfoList.argtypes = [wt.HANDLE]
setupapi.SetupDiEnumDeviceInterfaces.argtypes = [wt.HANDLE, wt.LPVOID,
                                                 wt.LPVOID, wt.DWORD, wt.LPVOID]
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [wt.HANDLE, wt.LPVOID,
                                                      wt.LPVOID, wt.DWORD,
                                                      wt.LPDWORD, wt.LPVOID]
hid_dll.HidD_GetHidGuid.argtypes = [wt.LPVOID]
hid_dll.HidD_GetAttributes.argtypes = [wt.HANDLE, wt.LPVOID]
hid_dll.HidD_GetPreparsedData.argtypes = [wt.HANDLE, wt.LPVOID]
hid_dll.HidD_FreePreparsedData.argtypes = [wt.LPVOID]
hid_dll.HidP_GetCaps.argtypes = [wt.LPVOID, wt.LPVOID]


class Guid(ctypes.Structure):
    _fields_ = [("Data1", wt.DWORD), ("Data2", wt.WORD), ("Data3", wt.WORD),
                ("Data4", ctypes.c_ubyte * 8)]


class SpDeviceInterfaceData(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("InterfaceClassGuid", Guid),
                ("Flags", wt.DWORD), ("ReferenceCount", ctypes.POINTER(wt.DWORD))]


class HidCaps(ctypes.Structure):
    # HIDP_CAPS：共 32 个 USHORT（含 Reserved[17] 和尾部计数）；尺寸必须给够，
    # 否则 HidP_GetCaps 写越界；只用得上前两个字段
    _fields_ = [("UsagePage", wt.USHORT), ("Usage", wt.USHORT),
                ("InputReportByteLength", wt.USHORT),
                ("OutputReportByteLength", wt.USHORT),
                ("FeatureReportByteLength", wt.USHORT),
                ("Rest", wt.USHORT * 27)]


class HidAttributes(ctypes.Structure):
    _fields_ = [("Size", wt.ULONG), ("VendorID", wt.USHORT),
                ("ProductID", wt.USHORT), ("VersionNumber", wt.USHORT)]


class CommTimeouts(ctypes.Structure):
    _fields_ = [("ReadIntervalTimeout", wt.DWORD),
                ("ReadTotalTimeoutMultiplier", wt.DWORD),
                ("ReadTotalTimeoutConstant", wt.DWORD),
                ("WriteTotalTimeoutMultiplier", wt.DWORD),
                ("WriteTotalTimeoutConstant", wt.DWORD)]


def hid_guid():
    g = Guid()
    hid_dll.HidD_GetHidGuid(ctypes.byref(g))
    return g


def find_raw_device():
    """返回 (handle, path)，取 usage page 0xFF60 且 VID/PID 匹配的集合。"""
    g = hid_guid()
    hdev = setupapi.SetupDiGetClassDevsW(ctypes.byref(g), None, None, 0x12)
    if hdev == INVALID:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        idx = 0
        while True:
            ifd = SpDeviceInterfaceData()
            ifd.cbSize = ctypes.sizeof(ifd)
            r = setupapi.SetupDiEnumDeviceInterfaces(hdev, None, ctypes.byref(g),
                                                     idx, ctypes.byref(ifd))
            if not r:
                break
            idx += 1
            detail = (ctypes.c_ubyte * 512)()
            # SP_DEVICE_INTERFACE_DETAIL_DATA_W: DWORD cbSize + WCHAR Path[]
            # 64 位下 cbSize 必须在调用前置为 8（DWORD+对齐），否则 1784 错误
            ctypes.cast(detail, ctypes.POINTER(wt.DWORD))[0] = 8
            dsize = wt.DWORD()
            if not setupapi.SetupDiGetDeviceInterfaceDetailW(
                    hdev, ctypes.byref(ifd), detail, ctypes.sizeof(detail),
                    ctypes.byref(dsize), None):
                continue
            path = ctypes.wstring_at(ctypes.addressof(detail) + ctypes.sizeof(wt.DWORD))
            if ("vid_%04x" % Vid) not in path.lower() or \
               ("pid_%04x" % Pid) not in path.lower():
                continue
            h = kernel32.CreateFileW(path, 0xC0000000, 0x03, None, 3, 0, None)
            if h == INVALID:
                continue
            attr = HidAttributes()
            attr.Size = ctypes.sizeof(attr)
            if not hid_dll.HidD_GetAttributes(h, ctypes.byref(attr)):
                kernel32.CloseHandle(h)
                continue
            prep = ctypes.c_void_p()
            caps = HidCaps()
            if hid_dll.HidD_GetPreparsedData(h, ctypes.byref(prep)) and \
               hid_dll.HidP_GetCaps(prep, ctypes.byref(caps)) >= 0:
                page, usage = caps.UsagePage, caps.Usage
                hid_dll.HidD_FreePreparsedData(prep)
            else:
                page = usage = 0
            if page == 0xFF60 or usage == 0xFF60:
                # 实测：Windows 把本板 Raw 集合报成 page=0x0061/usage=0xFF60，
                # 两个字段任一命中 0xFF60 即认
                return h, path
            kernel32.CloseHandle(h)
        return None, None
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(hdev)


def set_timeouts(h, read_ms):
    t = CommTimeouts(0, 0, read_ms, 0, 1000)
    kernel32.SetCommTimeouts(h, ctypes.byref(t))


def xfer(h, cmd, read_ms=400):
    buf = (ctypes.c_ubyte * (Payload + 1))()
    buf[0] = 0                                # 报告 ID 前缀（描述符无 ID -> 0）
    for i, b in enumerate(cmd):
        buf[1 + i] = b
    set_timeouts(h, 0)
    written = wt.DWORD()
    if not kernel32.WriteFile(h, buf, Payload + 1, ctypes.byref(written), None):
        raise ctypes.WinError(ctypes.get_last_error())
    set_timeouts(h, read_ms)
    rbuf = (ctypes.c_ubyte * (Payload + 1))()
    read = wt.DWORD()
    if not kernel32.ReadFile(h, rbuf, Payload + 1, ctypes.byref(read), None):
        raise ctypes.WinError(ctypes.get_last_error())
    if read.value == 0:
        return None                            # 超时
    return bytes(rbuf[1:read.value])


def hexs(b):
    return " ".join("%02X" % x for x in b) if b else "<无回包/超时>"


def probe():
    h, path = find_raw_device()
    if h is None:
        print("!! 没找到 %04X:%04X 的 Raw HID 集合（usage page 0xFF60）" % (Vid, Pid))
        print("   => Windows 根本没暴露 raw 接口：枚举/驱动层问题，不是 via.c 逻辑问题")
        return 1
    print("设备:", path)
    print("接口存在 => Windows 已把 0xFF60 集合挂出来，下面测固件回包\n")

    tests = [
        ("01 协议版本",      [0x01],                       "应回 01 00 0C"),
        ("02/01 uptime",     [0x02, 0x01],                 "4 字节递增毫秒数"),
        ("02/03 矩阵状态",   [0x02, 0x03],                 "25bit 按键位图"),
        ("0C 宏数量",        [0x0C],                       "应回 08（8 槽已对 VIA 开放）"),
        ("0D 宏缓冲区大小",  [0x0D],                       "应回 00 E0（224B）"),
        ("0E 宏缓冲区前28B", [0x0E, 0, 0, 28],             "紧凑 SS 流：0x01 t/d/u kc 令牌，0x00 分隔"),
        ("11 层数",          [0x11],                       "应回 04"),
        ("04 键码(0,0,0)",   [0x04, 0, 0, 0],              "L0(0,0)=0x004A(HOME)"),
        ("04 键码(0,4,0)",   [0x04, 0, 4, 0],              "L0(4,0)=MO1 自定义码"),
        ("12 键表前16B",     [0x12, 0, 0, 16],             "L0 前 8 个键码"),
        ("08/3/2 灯光Eff",   [0x08, 3, 2],                 "d[3]=mode（VIA 灯光页 GET，卡死元凶）"),
        ("08/3/4 灯光Color", [0x08, 3, 4],                 "d[3]=hue d[4]=sat"),
        ("08/6/1 System",    [0x08, 6, 1],                 "d[3] 应回 00（动作控件 GET）"),
        ("7F 未知命令",      [0x7F],                       "应回首字节 FF（unhandled）"),
    ]
    for name, cmd, note in tests:
        r = xfer(h, cmd)
        print("%-18s -> %s   # %s" % (name, hexs(r), note))
    kernel32.CloseHandle(h)
    return 0


def matrix_watch():
    h, path = find_raw_device()
    if h is None:
        print("!! 没找到 raw HID 集合")
        return 1
    print("轮询矩阵中（Ctrl+C 退出）。col0 键: (0,0)HOME (1,0)END (2,0)PGUP (3,0)PGDN (4,0)MO1")
    prev = None
    names = []
    for r in range(5):
        for c in range(5):
            names.append("(%d,%d)" % (r, c))
    try:
        while True:
            resp = xfer(h, [0x02, 0x03], read_ms=200)
            if resp is None:
                print("读超时：raw 通道无响应（传输层挂了）")
                continue
            bits = 0
            for i in range(2, min(len(resp), 29)):
                bits |= resp[i] << (8 * (i - 2))
            if bits != prev:
                on = [names[i] for i in range(25) if (bits >> i) & 1]
                print("state=%08X  按下: %s" % (bits, ",".join(on) if on else "（无）"))
                prev = bits
    except KeyboardInterrupt:
        pass
    finally:
        kernel32.CloseHandle(h)
    return 0


def factory_reset():
    """发 VIA 0x0A：DataFlash 键表按新固件的 board.h 默认重建（改过默认表后跑一次）。"""
    h, path = find_raw_device()
    if h is None:
        print("!! 没找到 raw HID 集合")
        return 1
    r = xfer(h, [0x0A])
    print("0A 恢复出厂 ->", hexs(r) if r else "无回包（正常：flash 写入耗时，重插 USB 后生效）")
    kernel32.CloseHandle(h)
    return 0


def list_all():
    """列出总机上所有 4A17:4A51 的 HID 顶层集合（不过滤 usage page）。"""
    g = hid_guid()
    hdev = setupapi.SetupDiGetClassDevsW(ctypes.byref(g), None, None, 0x12)
    ifd = SpDeviceInterfaceData()
    ifd.cbSize = ctypes.sizeof(ifd)
    idx = 0
    found = 0
    while setupapi.SetupDiEnumDeviceInterfaces(hdev, None, ctypes.byref(g),
                                               idx, ctypes.byref(ifd)):
        idx += 1
        detail = (ctypes.c_ubyte * 512)()
        ctypes.cast(detail, ctypes.POINTER(wt.DWORD))[0] = 8   # cbSize 调用前置位
        dsize = wt.DWORD()
        if not setupapi.SetupDiGetDeviceInterfaceDetailW(
                hdev, ctypes.byref(ifd), detail, ctypes.sizeof(detail),
                ctypes.byref(dsize), None):
            continue
        path = ctypes.wstring_at(ctypes.addressof(detail) + ctypes.sizeof(wt.DWORD))
        if ("vid_%04x" % Vid) not in path.lower() and "all" not in sys.argv:
            continue
        found += 1
        h = kernel32.CreateFileW(path, 0xC0000000, 0x03, None, 3, 0, None)
        page = usage = inlen = outlen = 0xFFFF
        if h != INVALID:
            prep = ctypes.c_void_p()
            caps = HidCaps()
            if hid_dll.HidD_GetPreparsedData(h, ctypes.byref(prep)) and \
               hid_dll.HidP_GetCaps(prep, ctypes.byref(caps)) >= 0:
                page, usage = caps.UsagePage, caps.Usage
                inlen = caps.InputReportByteLength
                outlen = caps.OutputReportByteLength
                hid_dll.HidD_FreePreparsedData(prep)
            kernel32.CloseHandle(h)
        print("vid=%04X pid_match=%s page=0x%04X usage=0x%04X in=%d out=%d\n    %s"
              % (Vid, ("pid_%04x" % Pid) in path.lower(), page, usage, inlen, outlen, path))
    if not found:
        print("总机上没有任何 %04X:%04X 的 HID 接口（键盘未插/VID 不匹配）" % (Vid, Pid))
    setupapi.SetupDiDestroyDeviceInfoList(hdev)


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "probe"
    if mode == "list":
        list_all()
        sys.exit(0)
    if mode == "reset":
        sys.exit(factory_reset())
    sys.exit(matrix_watch() if mode == "matrix" else probe())
