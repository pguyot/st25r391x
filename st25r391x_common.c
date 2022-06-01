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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/types.h>

#include "st25r391x.h"
#include "st25r391x_common.h"
#include "st25r391x_registers.h"
#include "st25r391x_i2c.h"
#include "st25r391x_commands.h"

s32 st25r391x_enable_tx_and_rx(struct i2c_client *i2c)
{
	return st25r391x_set_register_bits(
		i2c, ST25R391X_OPERATION_CONTROL_REGISTER,
		ST25R391X_OPERATION_CONTROL_REGISTER_rx_en |
			ST25R391X_OPERATION_CONTROL_REGISTER_tx_en);
}

static s32
st25r391x_perform_collision_avoidance(struct i2c_client *i2c,
				      struct st25r391x_interrupts *ints)
{
	s32 result;

	st25r391x_clear_interrupts(
		ints, 0,
		ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cac |
			ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cat,
		0, 0);
	result = st25r391x_direct_command(
		i2c, ST25R391X_NFC_INITIAL_FIELD_ON_COMMAND_CODE);
	if (result < 0) {
		return result;
	}
	result = st25r391x_polling_wait_for_interrupt_bit(
		i2c, ints, 0,
		ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cac |
			ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cat,
		0, 0, 20000); // TODO: check timeout
	if (result < 0) {
		dev_err(&i2c->dev,
			"st25r391x_perform_collision_avoidance: time out waiting for interrupt bits");
		return result;
	}
	if (result & ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cac) {
		dev_err(&i2c->dev,
			"st25r391x_perform_collision_avoidance: collision was detected");
		return -1;
	}

	return 0;
}

static s32 st25r391x_turn_oscillator_on(struct i2c_client *i2c,
					struct st25r391x_interrupts *ints)
{
	s32 result;

	// Enable oscillator
	do {
		st25r391x_clear_interrupts(
			ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_osc, 0, 0, 0);
		result = st25r391x_write_register_byte_check(
			i2c, ST25R391X_OPERATION_CONTROL_REGISTER,
			ST25R391X_OPERATION_CONTROL_REGISTER_en |
				ST25R391X_OPERATION_CONTROL_REGISTER_en_fd_c1 |
				ST25R391X_OPERATION_CONTROL_REGISTER_en_fd_c0);
		if (result < 0)
			break;
		// "Since the start-up time varies with crystal type, temperature and other parameters, the oscillator amplitude is observed and an interrupt is generated when stable oscillator operation is reached."
		// page 17/157
		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_osc, 0,
			0, 0, 5000);
		if (result < 0)
			break;
		result = st25r391x_read_register_byte(
			i2c, ST25R391X_AUXILIARY_DISPLAY_REGISTER);
		if (result < 0)
			break;
		if ((result & ST25R391X_AUXILIARY_DISPLAY_REGISTER_osc_ok) ==
		    0) {
			dev_err(&i2c->dev,
				"st25r391x_turn_oscillator_on: Auxiliary display register says oscillator is not ok: %d",
				result);
			return -1;
		}
	} while (0);
	return result;
}

static s32 st25r391x_turn_oscillator_off(struct i2c_client *i2c)
{
	s32 result;

	// Disable oscillator
	result = st25r391x_write_register_byte_check(
		i2c, ST25R391X_OPERATION_CONTROL_REGISTER, 0);
	if (result < 0) {
		dev_err(&i2c->dev,
			"st25r391x_turn_oscillator_off: Failed to write operation control register %d",
			result);
		return result;
	}

	return result;
}

/**
 * Turn field on and set it up.
 */
s32 st25r391x_turn_field_on(struct st25r391x_i2c_data *priv)
{
	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;
	s32 result;

	// Set this bit on now to always try to turn it off when leaving.
	priv->field_on = 1;

	result = st25r391x_turn_oscillator_on(i2c, ints);
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Failed to turn oscillator on: %d",
			result);
		return result;
	}

	// Adjust regulators
	st25r391x_clear_interrupts(
		ints, 0, ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_dct, 0,
		0);
	result = st25r391x_direct_command(
		i2c, ST25R391X_ADJUST_REGULATORS_COMMAND_CODE);
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Failed to send adjust regulators command code %d",
			result);
		return result;
	}
	result = st25r391x_polling_wait_for_interrupt_bit(
		i2c, ints, 0, ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_dct,
		0, 0, 10000); // TODO: check timeout
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Time out waiting for interrupt bit (adjust regulators command)");
		return result;
	}
	// STOP & Reset RX Gain
	result = st25r391x_direct_command(i2c, ST25R391X_STOP_ALL_COMMAND_CODE);
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Failed to send stop command code %d",
			result);
		return result;
	}
	result = st25r391x_direct_command(i2c,
					  ST25R391X_RESET_RX_GAIN_COMMAND_CODE);
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Failed to send reset rx gain command code %d",
			result);
		return result;
	}
	// Perform collision avoidance and turn field on
	result = st25r391x_perform_collision_avoidance(i2c, ints);
	if (result < 0) {
		dev_err(priv->device,
			"st25r391x_turn_field_on: Failed to perform collision avoidance: %d (will not abort)",
			result);
	}
	return result;
}

/**
 * Turn field off.
 */
s32 st25r391x_turn_field_off(struct st25r391x_i2c_data *priv)
{
	s32 result = st25r391x_turn_oscillator_off(priv->i2c);
	priv->field_on = 0;
	return result;
}

s32 st25r391x_transceive_frame(struct i2c_client *i2c,
			       struct st25r391x_interrupts *ints,
			       const u8 *tx_buf, u16 tx_count, u8 *rx_buf,
			       u16 rx_buf_len, int flags, u16 rx_timeout_usec)
{
	s32 result;
	u16 tx_bits_count;
	u16 tx_bytes_count;
	u8 fifo_flags;
	if (flags & transceive_frame_bits) {
		tx_bits_count = tx_count;
		tx_bytes_count = tx_bits_count >> 3;
		if (tx_bits_count & 0x7) {
			tx_bytes_count++;
		}
	} else {
		tx_bytes_count = tx_count;
		tx_bits_count = tx_count << 3;
	}
	do {
		result = st25r391x_direct_command(
			i2c, ST25R391X_CLEAR_FIFO_COMMAND_CODE);
		if (result < 0)
			break;

		if (tx_count > 0) {
			result = st25r391x_load_fifo(i2c, tx_bytes_count,
						     tx_buf);
			if (result < 0) {
				struct device *dev = &i2c->dev;
				dev_err(dev,
					"st25r391x_transceive_frame: failed to load FIFO %d",
					result);
				break;
			}

			result = st25r391x_write_registers_check(
				i2c,
				ST25R391X_NUMBER_OF_TRANSMITTED_BYTES_1_REGISTER,
				2, tx_bits_count >> 8, tx_bits_count & 0xFF);
			if (result < 0)
				break;

			if (flags & transceive_frame_no_crc_rx) {
				result = st25r391x_set_register_bits(
					i2c,
					ST25R391X_AUXILIARY_DEFINITION_REGISTER,
					ST25R391X_AUXILIARY_DEFINITION_REGISTER_no_crc_rx);
				if (result < 0)
					break;
			} else {
				result = st25r391x_clear_register_bits(
					i2c,
					ST25R391X_AUXILIARY_DEFINITION_REGISTER,
					ST25R391X_AUXILIARY_DEFINITION_REGISTER_no_crc_rx);
				if (result < 0)
					break;
			}

			if (flags & (transceive_frame_no_par_tx |
				     transceive_frame_no_par_rx)) {
				u8 settings = 0;
				if (flags & transceive_frame_no_par_tx) {
					settings |=
						ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_no_tx_par;
				}
				if (flags & transceive_frame_no_par_rx) {
					settings |=
						ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_no_rx_par;
				}
				result = st25r391x_write_register_byte_check(
					i2c,
					ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
					settings);
				if (result < 0)
					break;
			}

			st25r391x_clear_interrupts(
				ints,
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_txe |
					ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs |
					ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe,
				0, 0, 0);

			result = st25r391x_direct_command(
				i2c,
				flags & transceive_frame_no_crc_tx ?
					      ST25R391X_TRANSMIT_WITHOUT_CRC_COMMAND_CODE :
					      ST25R391X_TRANSMIT_WITH_CRC_COMMAND_CODE);
			if (result < 0)
				break;

			result = st25r391x_polling_wait_for_interrupt_bit(
				i2c, ints,
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_txe, 0, 0,
				0, 5000); // TODO: check timeout
			if (result < 0)
				break;
		}

		if (flags & transceive_frame_tx_only) {
			if (rx_timeout_usec) {
				usleep_range(rx_timeout_usec,
					     rx_timeout_usec + 200);
			}
		} else {
			// Receive data
			result = st25r391x_polling_wait_for_interrupt_bit(
				i2c, ints,
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs, 0, 0,
				0, rx_timeout_usec);
			if (result < 0) {
				if (flags &
				    transceive_frame_timeout) { // timeout not an error
					result = 0;
				}
				break;
			}
			result = st25r391x_polling_wait_for_interrupt_bit(
				i2c, ints,
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe, 0, 0,
				0, 5000); // TODO: check timeout
			if (result < 0)
				break;
			result = st25r391x_read_fifo(i2c, rx_buf_len, rx_buf,
						     &fifo_flags);
			if (result < 0)
				break;
			if (flags & transceive_frame_bits) {
				result = result << 3;
				if (fifo_flags & 0x0E) {
					result = result - 8 +
						 ((fifo_flags & 0x0E) >> 1);
				}
			}
		}
	} while (0);

	return result;
}
