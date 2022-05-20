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

#ifndef ST25R391X_COMMON_H
#define ST25R391X_COMMON_H

#include <linux/i2c.h>
#include <linux/types.h>

struct st25r391x_interrupts;
struct st25r391x_i2c_data;

// Common functions to interact with the ST25R391x chip.

s32 st25r391x_enable_tx_and_rx(struct i2c_client* i2c);
s32 st25r391x_turn_field_on(struct st25r391x_i2c_data *priv);
s32 st25r391x_turn_field_off(struct st25r391x_i2c_data *priv);
s32 st25r391x_transceive_frame(struct i2c_client* i2c, struct st25r391x_interrupts* ints, const u8* tx_buf, u16 tx_count, u8* rx_buf, u16 rx_buf_len, int crc, int receive);

#endif
