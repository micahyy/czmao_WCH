#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "include/ch5xx.h"
#include "include/ch5xx_usb.h"
#include "USBconstant.h"
#include "USBhandler.h"
#include "USBHID.h"
#include <Arduino.h>

volatile __data uint8_t UpPoint1_Busy = 0;
volatile __data uint8_t UpPoint2_Busy = 0;
volatile __data uint8_t raw_hid_receive_flag = 0;

/* USBhandler.c 定义：SET_CONFIGURATION 时置为配置值，总线复位/挂起时清 0 */
extern volatile __xdata uint8_t UsbConfig;

__idata uint8_t raw_hid_buffer[EP2_SIZE];
static __xdata uint8_t statusLED = 0;

/* v2 回调：由 via.c 实现，处理 32B raw HID 包 */
extern void v2_raw_hid_receive(__idata uint8_t *data, uint8_t length);

void USB_EP1_IN(void) {
    UEP1_T_LEN = 0;
    UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;
    UpPoint1_Busy = 0;
}

void USB_EP1_OUT(void) {
    if (U_TOG_OK) {
        statusLED = Ep1Buffer[1];
    }
}

uint8_t USB_EP1_send(const __idata uint8_t *data, __data uint8_t data_len) {
    uint16_t wait = 0;
    while (UpPoint1_Busy) {
        if (++wait >= 20000) return 0;
        delayMicroseconds(5);
    }
    memcpy(&Ep1Buffer[64], data, data_len);
    UEP1_T_LEN = data_len;
    UpPoint1_Busy = 1;
    UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
    return 1;
}

void USB_EP2_IN(void) {
    UEP2_T_LEN = 0;
    UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;
    UpPoint2_Busy = 0;
}

void USB_EP2_OUT(void) {
    if (U_TOG_OK) raw_hid_receive_flag = 1;
}

void raw_hid_task(void) {
    if (raw_hid_receive_flag) {
        memcpy(raw_hid_buffer, Ep2Buffer, EP2_SIZE);
        v2_raw_hid_receive(raw_hid_buffer, EP2_SIZE);
        raw_hid_receive_flag = 0;
    }
}

uint8_t USB_EP2_send(const __idata uint8_t *data, __data uint8_t data_len) {
    uint16_t wait = 0;
    while (UpPoint2_Busy) {
        if (++wait >= 50000) return 0;
        delayMicroseconds(5);
    }
    memcpy(&Ep2Buffer[64], data, data_len);
    UEP2_T_LEN = data_len;
    UpPoint2_Busy = 1;
    UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK;
    return 1;
}

uint8_t keyboard_leds(void) {
    return statusLED;
}

/* 枚举是否已完成（主机已下发 SET_CONFIGURATION）。
 * WS2812 刷新一帧要关全局中断约 700µs，必须等枚举结束再点灯，
 * 否则会把枚举期间的 SETUP/IN 事务打断，主机完全看不到设备。 */
uint8_t usb_configured(void) {
    return UsbConfig != 0;
}

void USBInit(void) {
    USBDeviceCfg();
    USBDeviceEndPointCfg();
    USBDeviceIntCfg();
    UEP0_T_LEN = 0;
    UEP1_T_LEN = 0;
    UEP2_T_LEN = 0;
}
