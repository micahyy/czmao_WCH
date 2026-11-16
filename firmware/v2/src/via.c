#include <stdint.h>
#include <string.h>
#include "config.h"
#include "keymap.h"
#include "action.h"
#include "rgb.h"
#include "storage.h"
#include "matrix.h"
#include "usb/USBHID.h"
#include "usb/USBconstant.h"
#include <Arduino.h>

extern __idata uint8_t raw_hid_buffer[EP2_SIZE];
extern __xdata uint8_t macro_io[MACRO_LEN];
extern void action_macro_set_from_io(uint8_t slot, uint8_t len);
extern uint8_t action_macro_get(uint8_t slot);
/* action_td_set 已随 TD 整链路移除 */

extern void bootloader_jump(void);
extern void soft_reset(void);

/* 恢复出厂走「先回包、后执行」：raw_hid 回调里只挂待办，主循环 via_task() 等
 * EP2 回包发出、延时 120ms 后才重建键表/灯光 + bSW_RESET 热复位。
 * 否则 keymap_flush 的几十次 DataFlash 写会拖住 VIA 命令响应；且不复位时
 * VIA 不重枚举就不会重拉默认键表，界面停在旧值 => 用户感知「不生效」。 */
static __idata uint8_t  via_pending_action;
static __idata uint16_t via_pending_ms;

/* json 灯光通道 3 的 value_id -> v2 rgb 通道：1=Bri->3, 2=Eff->0,
 * 3=Spd->4, 4=Color(h)->1（s 另取 d[4]->2）。表驱动省 ROM */
static const __code uint8_t via_rgb_ch[5] = { 0, 3, 0, 4, 1 };

/* VIA 宏缓冲：必须用 VIA app 的 V1.1 编码（协议 >=11 走 MacroAPIV11），否则
 * readRawKeycodeSequences 解析 0x0E 抛异常 -> VIA 的 loadMacros catch 吞掉后
 * 置 isFeatureSupported=false，界面显示「当前固件不支持宏」（与 macroCount 无关）。
 * V1.1 格式：每个键动作 = 0x01(KeyActionPrefix) 0x01(Tap) kc，每宏结尾恰好一个
 * 0x00(MacroTerminator)，terminator 个数必须 == macroCount。各槽流首尾相接、
 * 尾部补 0，总长 224B（每槽上限 9 动作：9*3+1=28，8 槽正好 224）。
 * XRAM 存不下原始缓冲：0x0E 按键码实时算流（统一导出成 Tap），0x0F 跨包状态机
 * 增量解码（Tap/Down 收键码、Up 只消费不落盘、Delay 吃尾巴，v2 回放自动松键）。 */
#define MACRO_SS_SIZE   (28 * MACRO_COUNT)   /* 224 = VIA 侧总缓冲长度上限 */

/* 0x0F 跨包解码状态（VIA 全量回写期间有意义，offset=0 时复位） */
static __idata uint8_t mi_slot, mi_len, mi_st;

/* 0x0E：紧凑流第 g 字节的内容（逐槽算宽度 3*min(len,9)+1） */
static uint8_t macro_stream_byte(uint16_t g) {
    uint8_t slot, mlen, b;
    for (slot = 0; slot < MACRO_COUNT; slot++) {
        mlen = action_macro_get(slot);
        if (mlen > 9) mlen = 9;
        if (g < (uint16_t)(mlen * 3 + 1)) {
            b = (uint8_t)g;
            if (b == (uint8_t)(mlen * 3)) return 0x00;      /* 本槽分隔符 */
            /* VIA V1.1：每动作 3B = 0x01 前缀 + 0x01 Tap + 键码。
             * 键序号 = 槽内偏移 / 3（旧代码误用全局 g/3，>3 键的宏会读到越界/错键）*/
            if ((b % 3) < 2) return 0x01;
            return macro_io[b / 3];
        }
        g -= (uint16_t)(mlen * 3 + 1);
    }
    return 0;                                               /* 8 槽用尽：补 0 */
}

/* 0x0F：流式吃 1 字节；遇 0x00 落盘一个槽（每宏至多 1 次 IAP） */
static void macro_stream_feed(uint8_t b) {
    uint8_t kc;
    switch (mi_st) {
        case 0:                                   /* 动作首字节 */
            if (b == 0x00) {                      /* MacroTerminator：落盘一个槽 */
                if (mi_slot < MACRO_COUNT)
                    action_macro_set_from_io(mi_slot++, mi_len);
                mi_len = 0;
            } else if (b == 0x01) mi_st = 1;      /* KeyActionPrefix */
            else {                                /* CharacterStream：ASCII 文本字符
                                                   * 转成 HID 键码存为一次 tap（大写前插左 Shift） */
                if (b >= 'a' && b <= 'z')      kc = (uint8_t)(b - 0x61 + 0x04);
                else if (b >= 'A' && b <= 'Z') {
                    if (mi_len < MACRO_LEN) macro_io[mi_len++] = 0xE1;
                    kc = (uint8_t)(b - 0x41 + 0x04);
                }
                else if (b >= '1' && b <= '9') kc = (uint8_t)(b - 0x31 + 0x1E);
                else if (b == '0')             kc = 0x27;
                else if (b == ' ')             kc = 0x2C;
                else                             kc = 0;
                if (kc && mi_len < MACRO_LEN) macro_io[mi_len++] = kc;
            }
            break;
        case 1:                                   /* 动作类型 1Tap 2Down 3Up 4Delay */
            if (b == 0x04) mi_st = 3;             /* Delay：吃 ASCII 数字尾巴 */
            else mi_st = (b == 0x03) ? 4 : 2;     /* Up 只消费键码不落盘 */
            break;
        case 2:                                   /* 键码字节：存成一次 Tap */
            if (mi_len < MACRO_LEN) macro_io[mi_len++] = b;
            mi_st = 0;
            break;
        case 4:                                   /* Up 的键码：丢弃（回放自动松键） */
            mi_st = 0;
            break;
        default:                                  /* 3: Delay 尾巴直到 '|' */
            if (b == '|') mi_st = 0;
            break;
    }
}

void v2_raw_hid_receive(__idata uint8_t *d, uint8_t len) {
    uint8_t cmd, r, c, bit, i, n;
    uint16_t kc, off;
    (void)len;
    cmd = d[0];
    /* json System 菜单按钮（channel 6）映射成标准命令，复用 0x0A/0x0B，
     * 不另写一份 factory/bootloader 代码（ROM/OSEG 都省） */
    if (cmd == 0x07 && d[1] == 6 && d[3]) {
        if (d[2] == 1) cmd = 0x0A;
        else if (d[2] == 2) cmd = 0x0B;
    }

    /* 所有变量在函数顶部声明：SDCC 对 switch case 内声明会 SIGSEGV */
    switch (cmd) {
        case 0x01:
            d[1] = (VIA_PROTOCOL_VERSION >> 8) & 0xFF;
            d[2] = VIA_PROTOCOL_VERSION & 0xFF;
            break;

        case 0x02:
            if (d[1] == 0x01) {
                /* uptime 毫秒，大端 4 字节：VIA 用它做 keepalive 判连 */
                uint32_t ms = millis();
                d[2] = (uint8_t)(ms >> 24); d[3] = (uint8_t)(ms >> 16);
                d[4] = (uint8_t)(ms >> 8);  d[5] = (uint8_t)ms;
            } else if (d[1] == 0x03) {
                bit = 0;
                memset(&d[2], 0, 29);
                for (r = 0; r < MATRIX_ROWS; r++) {
                    for (c = 0; c < MATRIX_COLS; c++) {
                        if (matrix_is_pressed(r, c)) d[2 + (bit >> 3)] |= (1 << (bit & 7));
                        bit++;
                    }
                }
            } else if (d[1] == 0x04) {
                /* QMK: id_get_current_layer，回真实层号（v2 此前硬编 2，VIA
                 * 可能拿它校验状态，不一致会判异常断开） */
                d[2] = 0;
            }
            break;

        case 0x04:
            kc = keymap_get_keycode(d[1], d[2], d[3]);
            d[4] = (uint8_t)(kc >> 8); d[5] = (uint8_t)kc;
            break;

        case 0x05:
            kc = ((uint16_t)d[4] << 8) | d[5];
            keymap_set_keycode(d[1], d[2], d[3], kc);
            break;

        case 0x06:
            keymap_reset_default();
            break;

        case 0x07:
        case 0x09:
            /* 包格式对齐 v1/QMK：[cmd, channel, value_id, value...] */
            if (d[1] == 3 && d[2] >= 1 && d[2] <= 4) {
                rgb_set_value(via_rgb_ch[d[2]], d[3]);
                if (d[2] == 4) rgb_set_value(2, d[4]);     /* Color 的 sat */
            }
            break;

        case 0x08:
            if (d[1] == 3 && d[2] >= 1 && d[2] <= 4) {
                d[3] = rgb_get_value(via_rgb_ch[d[2]]);
                if (d[2] == 4) d[4] = rgb_get_value(2);
            } else if (d[1] == 6) {
                /* v1 教训：动作型控件菜单加载时也会 GET，必须回 0，
                 * 否则 VIA 把整个菜单响应判错 -> 连接卡死退回选择页 */
                d[3] = 0;
            }
            break;

        case 0x0A:
            via_pending_action = 1;
            via_pending_ms = (uint16_t)millis();
            break;

        case 0x0B:
            USB_EP2_send(d, 0);
            delay(20);
            bootloader_jump();
            break;

        /* 宏：对 VIA 开放（8 槽，紧凑 SS 缓冲，语义与 v1 dynamic_macro 对齐） */
        case 0x0C: d[1] = MACRO_COUNT; break;
        case 0x0D: d[1] = (MACRO_SS_SIZE >> 8) & 0xFF; d[2] = MACRO_SS_SIZE & 0xFF; break;

        case 0x0E:
            n = d[3]; if (n > 28) n = 28;
            for (i = 0; i < n; i++) d[4 + i] = macro_stream_byte((uint16_t)d[2] + i);
            break;

        case 0x0F:
            n = d[3]; if (n > 28) n = 28;
            if (d[2] == 0) { mi_slot = 0; mi_len = 0; mi_st = 0; }  /* 新一轮全量写 */
            for (i = 0; i < n; i++) macro_stream_feed(d[4 + i]);
            /* 半途断流：最后一个未闭合槽保持旧内容，与 v1 尾字节非 0 即弃同语义 */
            break;

        case 0x10:
            for (i = 0; i < MACRO_COUNT; i++) action_macro_set_from_io(i, 0);
            break;

        case 0x11: d[1] = LAYER_COUNT; break;

        case 0x12:
            off = ((uint16_t)d[1] << 8) | d[2];
            n = d[3];
            if (n > 28) n = 28;
            for (i = 0; i < n; i++) d[4 + i] = keymap_buffer_byte(off + i);
            break;

        case 0x13:
            off = ((uint16_t)d[1] << 8) | d[2];
            n = d[3];
            if ((uint16_t)(off + n) > (uint16_t)(LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2)) break;
            for (i = 0; i < n; i++) keymap_set_buffer_byte(off + i, d[4 + i]);
            /* 与 v1/QMK 一致：每包都落盘（尾包常常 <28B，条件 flush 会丢最后一次写） */
            keymap_flush();
            break;

        case 0x14: d[4] = d[5] = 0; break;
        case 0x15: break;

        default:
            /* 未知命令回 id_unhandled(0xFF)，不能原样回显，否则 VIA 把
             * keepalive 轮询当成协议异常判定断开 */
            d[0] = 0xFF;
            break;
    }
    USB_EP2_send(d, EP2_SIZE);
}

/* 由 main loop() 每轮调用：处理延迟下来的恢复出厂（回包已发出才做重活） */
void via_task(void) {
    if (!via_pending_action) return;
    if ((uint16_t)((uint16_t)millis() - via_pending_ms) < 120) return;
    via_pending_action = 0;
    storage_write(DF_OFF_RGB, 0xFF);
    keymap_reset_default();
#if !defined(ROLLBACK) && !defined(NO_PERSIST)
    action_factory_reset();
#endif
    soft_reset();        /* 热复位：USB 重举，VIA 重拉默认键表/灯光 */
}
