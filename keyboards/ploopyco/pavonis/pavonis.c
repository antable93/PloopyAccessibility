// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#ifdef VIA_ENABLE
#    include "via.h"
#    include "bootloader.h"

// Channel
#define PAVONIS_CUSTOM_CHANNEL 0

// Values
#define PAVONIS_CUSTOM_VALUE_BOOT 1
#define PAVONIS_CUSTOM_VALUE_DPI  2

// 🎯 Persistent DPI (EEPROM-backed)
#include "eeconfig.h"
static uint16_t custom_cpi = 800;

// EEPROM helpers
static void pavonis_load_cpi(void) {
    uint16_t val = eeprom_read_word((uint16_t*)4); // Use offset 4 (avoid QMK config)
    if (val >= 500 && val <= 4000) {
        custom_cpi = val;
    } else {
        custom_cpi = 2000;
    }
}
static void pavonis_save_cpi(void) {
    eeprom_update_word((uint16_t*)4, custom_cpi);
}

// 🔐 Bootloader state
static bool boot_armed = false;
static uint32_t arm_time = 0;

static uint32_t last_tap_time = 0;
static uint8_t tap_count = 0;

// VIA handler
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id = &(data[0]);
    uint8_t  channel_id = data[1];
    uint8_t  value_id   = data[2];
    uint8_t *value_data = &(data[3]);

    if (channel_id != PAVONIS_CUSTOM_CHANNEL) {
        *command_id = id_unhandled;
        return;
    }

    if (*command_id == id_custom_save) {
        return;
    }

    // 🎯 DPI control
    if (value_id == PAVONIS_CUSTOM_VALUE_DPI) {
        switch (*command_id) {
            case id_custom_get_value: {
                value_data[0] = (custom_cpi >> 0) & 0xFF;
                value_data[1] = (custom_cpi >> 8) & 0xFF;
                break;
            }
            case id_custom_set_value: {
                uint16_t new_cpi = ((uint16_t)value_data[1] << 8) | value_data[0];
                // Enforce range and round to nearest 100
                if (new_cpi < 500) new_cpi = 500;
                if (new_cpi > 4000) new_cpi = 4000;
                new_cpi = ((new_cpi + 50) / 100) * 100;
                custom_cpi = new_cpi;
                pavonis_save_cpi();
                break;
            }
            default:
                *command_id = id_unhandled;
                break;
        }
        return;
    }

    // 🔐 Bootloader arm (VIA toggle)
    if (value_id == PAVONIS_CUSTOM_VALUE_BOOT) {
        switch (*command_id) {

            case id_custom_set_value:
                if (value_data[0]) {
                    boot_armed = true;
                    arm_time = timer_read32();
                    tap_count = 0;
                }
                return;

            case id_custom_get_value:
                value_data[0] = boot_armed;
                return;

            default:
                *command_id = id_unhandled;
                return;
        }
    }

    *command_id = id_unhandled;
}

// 🚫 Disable unsafe VIA-triggered bootloader
bool via_command_kb(uint8_t *data, uint8_t length) {
    return false;
}

#endif // VIA_ENABLE

// 🖱️ Apply DPI at startup
void pointing_device_init_user(void) {
    pavonis_load_cpi();
    pointing_device_set_cpi(custom_cpi);
}

// 🖱️ Main pointing device loop
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
#ifdef VIA_ENABLE
    // 🔥 ALWAYS enforce DPI (fixes your issue)
    pointing_device_set_cpi(custom_cpi);

    // ⏱️ Timeout (5s)
    if (boot_armed && timer_elapsed32(arm_time) > 5000) {
        boot_armed = false;
        tap_count = 0;
    }

    // 🖱️ Detect click = tap
    if (mouse_report.buttons) {
        uint32_t now = timer_read32();
        if (timer_elapsed32(last_tap_time) < 300) {
            tap_count++;
        } else {
            tap_count = 1;
        }
        last_tap_time = now;
        // 🚀 Double tap → bootloader
        if (boot_armed && tap_count >= 2) {
            boot_armed = false;
            tap_count = 0;
            bootloader_jump();
        }
    }
#endif
   
}
