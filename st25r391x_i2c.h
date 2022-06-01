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

#ifndef ST25R391X_I2C_H
#define ST25R391X_I2C_H

#include <linux/i2c.h>

s32 st25r391x_read_register_byte(struct i2c_client *i2c, u8 reg);
s32 st25r391x_read_registers_u16(struct i2c_client *i2c,
				 u8 first_reg); // first register is msb
s32 st25r391x_write_register_byte_check(struct i2c_client *i2c, u8 reg,
					u8 value);
s32 st25r391x_write_registers_check(struct i2c_client *i2c, u8 first_reg,
				    u8 count, ...);
s32 st25r391x_write_bank_b_registers(struct i2c_client *i2c, u8 first_reg,
				     u8 count, ...);
s32 st25r391x_set_register_bits(struct i2c_client *i2c, u8 reg, u8 value);
s32 st25r391x_clear_register_bits(struct i2c_client *i2c, u8 reg, u8 value);
s32 st25r391x_direct_command(struct i2c_client *i2c, u8 cmd);
s32 st25r391x_load_fifo(struct i2c_client *i2c, u16 len, const u8 *data);
s32 st25r391x_read_fifo(struct i2c_client *i2c, u16 max_len, u8 *data,
			u8 *status_2_flags);

#endif
