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

#ifndef ST25R391X_INTERRUPTS_H
#define ST25R391X_INTERRUPTS_H

#include <linux/i2c.h>

struct st25r391x_interrupts {
    u8 flags[4];
};

void st25r391x_clear_interrupts(struct st25r391x_interrupts* ints, u8 main_mask, u8 timer_and_nfc_mask, u8 error_and_wakeup_mask, u8 passive_target_mask);
int st25r391x_polling_wait_for_interrupt_bit(struct i2c_client *i2c, struct st25r391x_interrupts* ints, u8 main_mask, u8 timer_and_nfc_mask, u8 error_and_wakeup_mask, u8 passive_target_mask, int sleep_min, int max_loop);

#endif
