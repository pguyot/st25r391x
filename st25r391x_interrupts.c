/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * ST25R3916/7 NFC Reader Driver
 *
 * Copyright (C) 2020-2022 Paul Guyot <pguyot@kallisys.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
 */

#include "st25r391x_interrupts.h"

#include <linux/delay.h>
#include <linux/timekeeping.h>

#include "st25r391x_commands.h"
#include "st25r391x_registers.h"

void st25r391x_clear_interrupts(struct st25r391x_interrupts* ints, u8 main_mask, u8 timer_and_nfc_mask, u8 error_and_wakeup_mask, u8 passive_target_mask) {
    ints->flags[ST25R391X_MAIN_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] &= ~ main_mask;
    ints->flags[ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] &= ~ timer_and_nfc_mask;
    ints->flags[ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] &= ~ error_and_wakeup_mask;
    ints->flags[ST25R391X_PASSIVE_TARGET_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] &= ~ passive_target_mask;
}

int st25r391x_polling_wait_for_interrupt_bit(struct i2c_client *i2c, struct st25r391x_interrupts* ints, u8 main_mask, u8 timer_and_nfc_mask, u8 error_and_wakeup_mask, u8 passive_target_mask, u16 timeout_usec) {
    int sleep_min = timeout_usec >= 2000 ? 1000 : timeout_usec / 2;
    u64 timeout_ktime_ns = ktime_get_ns() + (timeout_usec * 1000);
    u8 masks[4];
    u8 base_addr = ST25R391X_MAIN_INTERRUPT_REGISTER;
    u8 count = 4;
    u8 start_index = 0;
    int result;
    u8 ix;
    masks[ST25R391X_MAIN_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] = main_mask;
    masks[ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] = timer_and_nfc_mask;
    masks[ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] = error_and_wakeup_mask;
    masks[ST25R391X_PASSIVE_TARGET_INTERRUPT_REGISTER - ST25R391X_MAIN_INTERRUPT_REGISTER] = passive_target_mask;
    for (ix = 0; ix < count; ix++) {
        if (masks[ix] == 0) {
            start_index++;
        } else {
            break;
        }
    }
    if (start_index == 4) {
        return -1;  // BAD ARG
    }
    for (ix = count - 1; ix >= 0; ix--) {
        if (masks[ix] == 0) {
            count--;
        } else {
            break;
        }
    }
    count -= start_index;

    do {
        for (ix = start_index; ix < start_index + count; ix++) {
            if (masks[ix] & ints->flags[ix]) {
                return 0;
            }
        }
        usleep_range(sleep_min, sleep_min * 2);
        do {
            // Busy loop on bus
            result = i2c_smbus_read_i2c_block_data(i2c, (base_addr + start_index) | ST25R391X_REGISTER_READ_MODE, count, ints->flags + start_index);
        } while (result < 0 && ktime_get_ns() < timeout_ktime_ns);
    } while (ktime_get_ns() < timeout_ktime_ns);

    return -1;
}
