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

#if __has_include (<linux/stdarg.h>)
#	include <linux/stdarg.h>
#else
#	include <stdarg.h>
#endif

#include "st25r391x_i2c.h"
#include "st25r391x_commands.h"
#include "st25r391x_registers.h"

s32 st25r391x_read_register_byte(struct i2c_client *i2c, u8 reg)
{
	s32 result;
	do {
		result = i2c_smbus_read_byte_data(
			i2c, reg | ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev, "Could not read register %.02hhXh: %d",
				reg, result);
			break;
		}
	} while (0);
	return result;
}

s32 st25r391x_read_registers_u16(struct i2c_client *i2c, u8 first_reg)
{
	s32 result;
	do {
		u8 buffer[2];
		result = i2c_smbus_read_i2c_block_data(
			i2c, first_reg | ST25R391X_REGISTER_READ_MODE, 2,
			buffer);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"Could not read registers %.02hhXh and next (%d)",
				first_reg, result);
			break;
		}
		result = ((u16)buffer[0]) << 8 | buffer[1];
	} while (0);
	return result;
}

s32 st25r391x_write_register_byte_check(struct i2c_client *i2c, u8 reg,
					u8 value)
{
	int result;
	do {
		result = i2c_smbus_write_byte_data(
			i2c, reg | ST25R391X_REGISTER_WRITE_MODE, value);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_register_byte_check: failed to write %#2hhx to register %.02hhXh (%d)",
				value, reg, result);
			break;
		}

		result = i2c_smbus_read_byte_data(
			i2c, reg | ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_register_byte_check: failed to read register %.02hhXh back (%d)",
				reg, result);
			break;
		}

		if (((u8)result) != value) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_register_byte_check: value mismatch for register %.02hhXh, wrote %#2hhx, read %#2hhx",
				reg, value, (u8)result);
			result = -1;
		} else {
			result = 0;
		}
	} while (0);
	return result;
}

s32 st25r391x_write_registers_check(struct i2c_client *i2c, u8 first_reg,
				    u8 count, ...)
{
	s32 result;
	va_list ap;
	u8 ix;
	u8 buffer[count];
	u8 check_buffer[count];
	va_start(ap, count);
	for (ix = 0; ix < count; ix++) {
		buffer[ix] = (u8)va_arg(ap, int);
	}
	va_end(ap);
	do {
		result = i2c_smbus_write_i2c_block_data(
			i2c, first_reg | ST25R391X_REGISTER_WRITE_MODE, count,
			buffer);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_registers_check: failed to write %d registers starting with %.02hhXh (%d)",
				count, first_reg, result);
			break;
		}

		result = i2c_smbus_read_i2c_block_data(
			i2c, first_reg | ST25R391X_REGISTER_READ_MODE, count,
			check_buffer);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_registers_check: failed to read %d registers back starting with %.02hhXh %d",
				count, first_reg, result);
			break;
		}

		result = 0;
		for (ix = 0; ix < count; ix++) {
			if (buffer[ix] != check_buffer[ix]) {
				struct device *dev = &i2c->dev;
				dev_err(dev,
					"st25r391x_write_registers_check: value mismatch for register %.02hhXh, wrote %#2hhx, read %#2hhx",
					first_reg + ix, buffer[ix],
					check_buffer[ix]);
				result = -1;
			}
		}
	} while (0);
	return result;
}

s32 st25r391x_write_bank_b_registers(struct i2c_client *i2c, u8 first_reg,
				     u8 count, ...)
{
	s32 result;
	va_list ap;
	u8 ix;
	u8 buffer[count + 1];
	va_start(ap, count);
	for (ix = 0; ix < count; ix++) {
		buffer[ix + 1] = (u8)va_arg(ap, int);
	}
	va_end(ap);
	buffer[0] = first_reg;
	do {
		result = i2c_smbus_write_i2c_block_data(
			i2c, ST25R391X_REGISTER_SPACE_B_ACCESS_COMMAND_CODE,
			count + 1, buffer);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_write_bank_b_registers: failed to write %d registers starting with %.02hhXh (%d)",
				count, first_reg, result);
			return result;
		}
	} while (0);
	return result;
}

static s32 st25r391x_set_or_clear_register_bits(struct i2c_client *i2c, u8 reg,
						u8 value, int set)
{
	s32 result;
	do {
		result = i2c_smbus_read_byte_data(
			i2c, reg | ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_set_or_clear_register_bits: failed to read register %.02hhXh (%d)",
				reg, result);
			break;
		}

		result = i2c_smbus_write_byte_data(
			i2c, reg | ST25R391X_REGISTER_WRITE_MODE,
			set ? result | value : result & ~value);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_set_or_clear_register_bits: failed to write register %.02hhXh (%d)",
				reg, result);
			break;
		}
	} while (0);
	return result;
}

s32 st25r391x_set_register_bits(struct i2c_client *i2c, u8 reg, u8 value)
{
	return st25r391x_set_or_clear_register_bits(i2c, reg, value, 1);
}

s32 st25r391x_clear_register_bits(struct i2c_client *i2c, u8 reg, u8 value)
{
	return st25r391x_set_or_clear_register_bits(i2c, reg, value, 0);
}

s32 st25r391x_direct_command(struct i2c_client *i2c, u8 cmd)
{
	s32 result;
	do {
		result = i2c_smbus_read_byte_data(
			i2c, cmd | ST25R391X_DIRECT_COMMAND_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_direct_command: could not send direct command %.02hhXh (%d)",
				cmd, result);
			break;
		}
	} while (0);
	return result;
}

s32 st25r391x_load_fifo(struct i2c_client *i2c, u16 len, const u8 *data)
{
	s32 result;
	do {
		result = i2c_smbus_read_word_data(
			i2c, ST25R391X_FIFO_STATUS_1_REGISTER |
				     ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_load_fifo: failed to read FIFO status registers 1 & 2 (%d)",
				result);
			break;
		}

		if ((result & 0xF0FF) != 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_load_fifo: read FIFO status registers, but value mismatch. Got %d, expected 0",
				result);
			break;
		}

		result = i2c_smbus_write_i2c_block_data(
			i2c, ST25R391X_FIFO_LOAD_MODE, len, data);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_load_fifo: could not load FIFO: %d",
				result);
			break;
		}

		result = i2c_smbus_read_word_data(
			i2c, ST25R391X_FIFO_STATUS_1_REGISTER |
				     ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_load_fifo: failed to read FIFO status registers 1 & 2 (%d)",
				result);
			break;
		}

		if (((result & 0xFF) | ((result & 0xC000) >> 6)) != len) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_load_fifo: read FIFO status registers, but value mismatch. Got %d, expected len = %d",
				result, len);
			break;
		}
	} while (0);
	return result;
}

s32 st25r391x_read_fifo(struct i2c_client *i2c, u16 max_len, u8 *data,
			u8 *status_2_flags)
{
	s32 result;
	s32 count;
	do {
		result = i2c_smbus_read_word_data(
			i2c, ST25R391X_FIFO_STATUS_1_REGISTER |
				     ST25R391X_REGISTER_READ_MODE);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_read_fifo: failed to read FIFO status registers 1 & 2 %d",
				result);
			break;
		}

		if (result == 0) {
			break;
		}

		if (status_2_flags) {
			*status_2_flags = (result & 0x3f00) >> 8;
		}
		count = (result & 0xff) | ((result & 0xC000) >> 5);

		if (count > max_len) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_read_fifo: could not read FIFO, got %d bytes but buffer is %d bytes",
				count, max_len);
			result = -1;
			break;
		}

		result = i2c_smbus_read_i2c_block_data(
			i2c, ST25R391X_FIFO_READ_MODE, count, data);
		if (result < 0) {
			struct device *dev = &i2c->dev;
			dev_err(dev,
				"st25r391x_read_fifo: could not read FIFO: %d",
				result);
			break;
		}

		result = count;
	} while (0);
	return result;
}
