/* Copyright 2016-2018 Erez Zukerman, Jack Humbert, Yiancar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "../../../keyboards/config_common.h"
#include "../platforms/wait.h"
#include "matrix.h"
#include "keyboard.h"
#include "send_string/send_string.h"
#include "keymap_common.h"
#include "quantum_keycodes.h"
#include "../platforms/timer.h"
#include "../platforms/gpio.h"
#include "../tmk_core/protocol/host.h"
#include "led.h"
#include "action_util.h"
#include <stddef.h>
#include <stdlib.h>
#include <Arduino.h>
#include "send_string/send_string.h"
#include "dynamic_keymap.h"
#include "via.h"

#ifdef ENCODER_ENABLE
#    include "encoder.h"
#endif

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix/rgb_matrix.h"
#endif

enum keycodes_user {
    BASE_CODE = QK_KB_0 - 1,

    DIAL_CCW,
    DIAL_BUTTON,
    DIAL_CW,

    KC_TO_0,
    KC_TO_1,
    KC_MO_0,
    KC_MO_1,

    KC_TD_0,
    KC_TD_1,
    KC_TD_2,
    KC_TD_3,
    KC_TD_4,
    
    RGB_MODE_U,
    RGB_HUI_U,
    RGB_HUD_U,
    RGB_SAI_U,
    RGB_SAD_U,
    RGB_VAI_U,
    RGB_VAD_U,
    RGB_SPI_U,
    RGB_SPD_U,

    /* 静态层入口。注意：只能追加在枚举末尾！
     * VIA json 的 customKeycodes 下标 = QK_KB_0 + i，在中间插入会让后面
     * 所有自定义键码错位；新增项必须同步追加到 DS22Lite_via.json 末尾。 */
    KC_TO_2,
    KC_TO_3,
    KC_MO_2,
    KC_MO_3,
};

extern __data uint8_t current_layer, layer_store;

void usb_send_for_debug(uint8_t v1, uint8_t v2);
