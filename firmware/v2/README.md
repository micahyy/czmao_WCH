# DS22Lite v2（不依赖 QMK 的轻量固件）

在 CH552T 上从零写的键盘固件，只复用芯片 SDK（ch55xduino core + SDCC + WS2812 汇编库），**不再依赖 QMK 框架**。目标是把 v1 中被资源砍掉的功能尽量捡回来。

## 构建

```bash
cd firmware/v2
python build.py --clean              # 完整版：build/DS22Lite_v2.hex（含宏掉电保持）
python build.py --clean --diag       # 诊断档：build/DS22Lite_v2_diag.hex（RGB 三色定位）
python build.py --clean --rollback   # 回退基线：build/DS22Lite_v2_rollback.hex
python build.py --clean --no-persist # 对照档：build/DS22Lite_v2_nopersist.hex
python verify.py                     # 静态校验（编解码往返 + 向量表守卫 + 回退基线回归）
```

工具链复用 `../toolchain`。hex 用 WCHISPTool 烧录。

> **烧录 `build/*.hex`（补洞版）**。SDCC 的 ihx 写文件会跳过值为 0x00 的字节，
> 在 hex 里留下地址空洞；烧录工具遇到空洞通常填 0xFF（擦除态），那就不是 NOP 了。
> `build.py` 会按链接器真实镜像把空洞补成 0x00 再输出，原始输出保留为
> `build/*_sdcc.hex`（只用于对比，不要烧）。

### 四档构建

| 档 | 命令 | 产物 | 说明 |
|---|---|---|---|
| 完整版 | （无参数） | `DS22Lite_v2.hex` | 全功能 + 宏掉电保持（DataFlash） |
| 诊断档 | `--diag` | `DS22Lite_v2_diag.hex` | 完整版 + RGB 三色定位 + 按住键跳过持久化；写路径置空 |
| 回退基线 | `--rollback` | `DS22Lite_v2_rollback.hex` | 不编译 `persist.c` / 诊断代码的骨架基线 |
| 对照档 | `--no-persist` | `DS22Lite_v2_nopersist.hex` | 不编译 `persist.c`，其余同完整版 |

> **`--rollback` 档是回归基线**：不编译 `persist.c`、不编译诊断代码（`#ifdef DIAG`
> 全部裁掉）。`verify.py` 锁死这个规格，并断言每档含 `_main` 入口与完整中断向量表
> （reset@0x0000 / Timer0@0x000B / USB@0x003B 均为 LJMP）。

### 诊断档：一次烧录出结论

诊断档用 RGB 灯的颜色把启动进度钉死，**烧一次就能定位挂在哪一步**：

| 灯色 | 含义 |
|---|---|
| **完全不亮** | 挂在 `matrix_init`/`host_init`/`keymap_init`/`rgb_init` 之前或之中 |
| **蓝常亮** | 走到 `rgb_init` 之后；若同时不枚举 => 挂在 `action_init`/`USBInit` |
| **绿常亮** | `USBInit()` 已返回，USB 子系统就绪；此时不枚举 => USB 层问题 |
| **白常亮** | `persist_load()` 正常走完，一切正常 |

**上电按住任意键 = 跳过持久化加载**，同一颗固件可做「按 / 不按」两组对比实验。

## 资源占用

| | 用量 | 上限 |
|---|---:|---:|
| ROM（CodeFlash 用户区） | 14318 | 14336（0x0000..0x37FF；`0x3800` 起是芯片 bootloader，绝不覆盖） |
| XRAM | 589 | 758 |
| 内部 RAM（direct/idata） | 顶满 | 0x00..0x7F 无碎片 |

> 新增 `__data` 变量或函数局部变量都可能报 `Could not get ... OSEG`，
> 新加代码请优先用 `__idata` / `__xdata`。ROM 也仅剩约 18B，加功能前先裁
> core 里用不到的 ISR。

## 持久化（DataFlash，非 CodeFlash）

CH552 唯一能像 EEPROM 反复改写（不需先擦）的非易失存储是 **128B DataFlash**；
应用侧 IAP 写 Code 区只能把位 1->0、不能在固件内擦回 1，改一次宏就写坏。因此键表 +
灯效 + 宏全部落在 DataFlash，键表用「差量存储」（只存与出厂默认不同的键）给宏腾空间。
分区见 `config.h`，宏读写见 `src/persist.c`（走 `storage_write` → `ROM_CMD_WRITE`）。

## 功能对照

| 功能 | v1 | v2 |
|---|---|---|
| 4 层键表、VIA 全部可改 | 有（压缩） | **有**（同算法，修了高低字节 bug） |
| 22 颗 RGB | 有 | **有**（6 种灯效，按层变色） |
| 动态宏 8 条 × 8 步 | 砍掉 | **有**，VIA 命令 `0x0C~0x10`，DataFlash 掉电保持 |
| Tap Dance | 砍掉 | **整链路移除**（TD0~TD4 键码保留占位但无动作，与 v1 同） |
| 鼠标键 / F13~F21→鼠标 | 砍掉 | **有** |
| 拨盘（Dial）HID 报告 | 砍掉 | **有** |
| System / Consumer 媒体键 | 部分 | **完整**（表驱动映射，见 `src/host.c`） |
| 掉电保存键位 + 灯效 + 宏 | 部分 | **有**（DataFlash 128B） |

## 目录

- `build.py` / `verify.py` —— 构建与校验
- `config.h` —— 引脚、USB VID/PID（v2 用 **0x4A51**）、功能开关、DataFlash 分区
- `board.h` —— 4 层默认键表 + 灯位
- `src/v2_keycodes.h` —— 自定义键码（数值与 QMK 一致，旧 VIA 键码不错位）
- `src/keymap.c` —— 变长键码编解码 + XRAM 展开缓存 + VIA 线性偏移
- `src/action.c` —— 层切换 / 修饰键解码 / 宏回放 / 鼠标 / FCODE / 拨盘
- `src/persist.c` —— 宏的 DataFlash 持久化
- `src/rgb.c` —— WS2812 灯效
- `src/via.c` —— VIA 协议
- `src/usb/` —— USB 描述符与端点收发（不依赖 QMK）
- `DS22Lite_v2_via.json` —— VIA 网页定义（导入后可在 VIA 里改键）

## VIA / 服务器协议（Raw HID 32B，command 在 data[0]）

标准 VIA 命令 `0x01`~`0x15` 兼容，特别地：

- `0x11` 返回层数 = 4
- `0x0C~0x10` 动态宏读写
- `0x12/0x13` 整包键表读写（返回/写入线性展开）
- `0x0B` 软件跳转进 CH552 片内 ROM USB ISP（`lcall #0x3800`）供 WCHISPTool 烧录

## 已知限制

- ROM 余约 18B、XRAM 余约 169B、内部 RAM 已顶满；加功能前先裁 core 里用不到的 ISR。
- **不识别根因（已修）**：SDCC 只把中断向量表生成在**定义了 `main` 的编译单元**里。
  早期重构裁掉 core `main()` 后没人接管 IVT，USB 向量变垃圾 → 完全不枚举。现在入口
  沿用 core `main()`，`build.py` 绝不裁它，`verify.py` 每档验向量表守卫。
- **WS2812 刷新期间关全局中断（一帧约 700us）**：`rgb_tick()` 用 `usb_configured()`
  门控，枚举完成前不点灯；副作用是枚举失败时灯全灭，排查请用诊断档。
- **在线升级**：CH552 无 BLE / 双分区，做不到 CH582 那种 WebHID/拖文件静默自刷；
  只能经上面向导跳到片内 ROM ISP，由 WCHISPTool 完成实际烧写。
- **未经实机验证的项**以实测为准：USB 枚举、VIA 往返、灯链、宏回放、DataFlash 读写。
