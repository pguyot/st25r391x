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

#include <linux/types.h>
#include <linux/i2c.h>

#include "nfc.h"

#include "st25r391x_commands.h"
#include "st25r391x_common.h"
#include "st25r391x_dev.h"
#include "st25r391x_i2c.h"
#include "st25r391x_interrupts.h"
#include "st25r391x_nfcf.h"
#include "st25r391x_registers.h"
#include "st25r391x.h"

// NFC-F commands
#define NFCF_COMMAND_SENSF_REQ 0x00

static s32 st25r391x_set_nfcf_mode(struct i2c_client *i2c)
{
	s32 result;

	do {
		// Disable wake up mode, if set
		result = st25r391x_clear_register_bits(
			i2c, ST25R391X_OPERATION_CONTROL_REGISTER,
			ST25R391X_OPERATION_CONTROL_REGISTER_wu);
		if (result < 0)
			break;
		result = st25r391x_write_registers_check(
			i2c, ST25R391X_MODE_DEFINITION_REGISTER, 2,
			ST25R391X_MODE_DEFINITION_REGISTER_felica_i, 0);
		if (result < 0)
			break;
		result = st25r391x_write_register_byte_check(
			i2c, ST25R391X_TX_DRIVER_REGISTER,
			ST25R391X_TX_DRIVER_REGISTER_am_12pct);
		if (result < 0)
			break;
		result = st25r391x_write_registers_check(
			i2c, ST25R391X_ISO14443B_SETTINGS_1_REGISTER, 2, 0, 0);
		if (result < 0)
			break;
		result = st25r391x_write_registers_check(
			i2c, ST25R391X_RECEIVER_CONFIGURATION_1_REGISTER, 4,
			0x13, 0x3D, 0x00, 0x00);
		if (result < 0)
			break;
		result = st25r391x_write_bank_b_registers(
			i2c, ST25R391X_CORRELATOR_CONFIGURATION_1_B_REGISTER, 2,
			0x54, 0x00);
		if (result < 0)
			break;
	} while (0);
	return result;
}

static s32 st25r391x_nfcf_poll(struct st25r391x_i2c_data *priv,
			       struct nfc_tag_info_nfcf *tag_info)
{
	s32 result;
	u8 buffer[21];
	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;

	do {
		result = st25r391x_set_nfcf_mode(i2c);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfcf_poll: Failed to set nfcf mode: %d",
				result);
			break;
		}

		// Enable Tx & Rx
		result = st25r391x_enable_tx_and_rx(i2c);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfcf_poll: : Failed to enable tx and rx: %d",
				result);
			break;
		}

		buffer[0] = NFCF_COMMAND_SENSF_REQ;
		buffer[1] = 0xFF;
		buffer[2] = 0xFF;
		buffer[3] = 0x00;
		result = st25r391x_transceive_frame(
			i2c, ints, buffer, 4, buffer, sizeof(buffer), 0,
			5000); // TODO: check rx timeout
		if (result < 0)
			break;

		dev_err(priv->device, "st25r391x_nfcf_poll, got %d bytes",
			result);
		result = -1;
	} while (0);
	return result;
}

void st25r391x_nfcf_discover(struct st25r391x_i2c_data *priv)
{
	// Passive poll NFC-F
	struct nfc_detected_tag_message_payload tag_payload;
	s32 result;

	memset(&tag_payload, 0, sizeof(tag_payload));
	result = st25r391x_nfcf_poll(priv, &tag_payload.tag_info.nfcf);
	if (result >= 0 &&
	    priv->mode_params.discover.protocols &
		    (NFC_TAG_PROTOCOL_NFCF | NFC_TAG_PROTOCOL_NFCF_NFCDEP)) {
		tag_payload.tag_type = NFC_TAG_TYPE_NFCF;
		// TODO
	}
}
