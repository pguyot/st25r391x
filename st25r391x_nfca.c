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
#include "st25r391x_nfca.h"
#include "st25r391x_registers.h"
#include "st25r391x.h"

static s32 st25r391x_set_iso14443a_mode(struct i2c_client *i2c)
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
			ST25R391X_MODE_DEFINITION_REGISTER_iso14443a_i, 0);
		if (result < 0)
			break;
		result = st25r391x_write_register_byte_check(
			i2c, ST25R391X_TX_DRIVER_REGISTER,
			ST25R391X_TX_DRIVER_REGISTER_am_12pct);
		if (result < 0)
			break;
		result = st25r391x_write_register_byte_check(
			i2c,
			ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
			0);
		if (result < 0)
			break;
		result = st25r391x_write_registers_check(
			i2c, ST25R391X_RECEIVER_CONFIGURATION_1_REGISTER, 4,
			0x08, 0x2D, 0x00, 0x00);
		if (result < 0)
			break;
		result = st25r391x_write_bank_b_registers(
			i2c, ST25R391X_CORRELATOR_CONFIGURATION_1_B_REGISTER, 2,
			0x51, 0x00);
	} while (0);
	return result;
}

static s32 st25r391x_nfca_transceive_anticollision_frame(
	struct i2c_client *i2c, struct st25r391x_interrupts *ints,
	const u8 *tx_buf, u8 bits_count, u8 *rx_buf)
{
	s32 result;
	u8 bytes_in_fifo;
	u8 fifo_flags;
	u8 received_bits;

	do {
		result = st25r391x_direct_command(
			i2c, ST25R391X_CLEAR_FIFO_COMMAND_CODE);
		if (result < 0)
			break;

		bytes_in_fifo = bits_count / 8;
		if (bits_count & 0x7)
			bytes_in_fifo++;

		result = st25r391x_load_fifo(i2c, bytes_in_fifo, tx_buf);
		if (result < 0) {
			dev_err(&i2c->dev,
				"st25r391x_nfca_transceive_anticollision_frame: failed to load FIFO %d",
				result);
			break;
		}

		result = st25r391x_write_registers_check(
			i2c, ST25R391X_NUMBER_OF_TRANSMITTED_BYTES_1_REGISTER,
			2, 0, bits_count);
		if (result < 0)
			break;

		st25r391x_clear_interrupts(
			ints,
			ST25R391X_MAIN_INTERRUPT_REGISTER_l_txe |
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs |
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe,
			0, 0, 0);

		result = st25r391x_direct_command(
			i2c, ST25R391X_TRANSMIT_WITHOUT_CRC_COMMAND_CODE);
		if (result < 0)
			break;

		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_txe, 0,
			0, 0, 5000); // TODO: fix timeout
		if (result < 0)
			break;
		// Receive data
		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs, 0,
			0, 0, 5000); // TODO: fix timeout
		if (result < 0)
			break;
		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe, 0,
			0, 0, 5000); // TODO: fix timeout
		if (result < 0)
			break;
		result = st25r391x_read_register_byte(
			i2c, ST25R391X_COLLISION_DISPLAY_REGISTER);
		if (result < 0)
			break;
		if (result & ST25R391X_COLLISION_DISPLAY_REGISTER_c_pb) {
			dev_err(&i2c->dev,
				"Collision in parity bit (unimplemeted) => %#2x",
				result);
			result = -1; // Collision in parity bit
			break;
		}
		received_bits = (result >> 1);
		if (received_bits < bits_count) {
			dev_err(&i2c->dev,
				"st25r391x_nfca_transceive_anticollision_frame: collision happened after %d bits, expected at least %d (what we sent)",
				received_bits, bits_count);
			result = -1;
			break;
		}
		received_bits -=
			bits_count; // received_bits from collision display register
		result = st25r391x_read_fifo(i2c, 5, rx_buf, &fifo_flags);
		if (result < 0) {
			dev_err(&i2c->dev,
				"st25r391x_nfca_transceive_anticollision_frame: read FIFO failed");
			break;
		}
		result = result << 3;
		if (fifo_flags & 0x0E) {
			result = result - 8 + ((fifo_flags & 0x0E) >> 1);
		}
		if (received_bits != result) {
			dev_err(&i2c->dev,
				"st25r391x_nfca_transceive_anticollision_frame: read %d bits from FIFO, expected %d",
				result, received_bits);
			result = -1;
			break;
		}
	} while (0);

	return result;
}

static s32 st25r391x_nfca_rats(struct st25r391x_i2c_data *priv,
			       struct nfc_tag_info_iso14443a4 *tag_info)
{
	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;
	u8 buffer[254];
	s32 result;

	// Perform RATS
	buffer[0] = 0xE0;
	buffer[1] = 0x80;
	result = st25r391x_transceive_frame(i2c, ints, buffer, 2, buffer,
					    sizeof(buffer), 0,
					    5000); // TODO: check rx timeout
	if (result >= 0) {
		if (result == buffer[0] + 2) {
			tag_info->ats_len = buffer[0] - 1;
			memcpy(tag_info->ats, (const void *)buffer + 1,
			       tag_info->ats_len);
		} else {
			dev_err(priv->device,
				"st25r391x_nfca_rats: Incorrect TL byte for ATS, expected %d got %d",
				result, buffer[0]);
			result = -1;
		}
	}
	return result;
}

static s32 st25r391x_nfca_do_select(struct st25r391x_i2c_data *priv,
				    struct nfc_tag_info_iso14443a4 *tag_info)
{
	s32 result;
	u8 buffer[8];

	u8 cascade_level = 1;
	u8 known_bits = 0;
	u8 uid[15];

	u8 index_bits;

	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;

	do {
		// Receive without CRC is done automatically when setting antcl bit
		// ST25R3916/7 datasheet, DS12484 Rev 4, page 81/157
		result = st25r391x_write_register_byte_check(
			i2c,
			ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
			ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_antcl);
		if (result < 0)
			break;

		do {
			buffer[0] = 0x91 + (cascade_level * 2);
			buffer[1] = (2 + (known_bits / 8)) << 4 |
				    (known_bits & 0x7); // Test bits
			for (index_bits = 0; index_bits < known_bits;
			     index_bits += 8) {
				u8 index_byte = index_bits / 8;
				buffer[2 + index_byte] =
					uid[(cascade_level - 1) * 5 +
					    index_byte];
			}
			result = st25r391x_nfca_transceive_anticollision_frame(
				i2c, ints, buffer, 16 + known_bits, buffer);
			if (result < 0)
				break;

			for (index_bits = 0; index_bits < result;
			     index_bits++) {
				u8 index_byte_buffer = index_bits / 8;
				u8 index_within_byte_buffer = index_bits & 0x07;
				u8 index_byte_uid =
					(cascade_level - 1) * 5 +
					(known_bits + index_bits) / 8;
				u8 index_within_byte_uid =
					(known_bits + index_bits) & 0x07;
				if (buffer[index_byte_buffer] &
				    (1 << index_within_byte_buffer)) {
					uid[index_byte_uid] |=
						(1 << index_within_byte_uid);
				} else {
					uid[index_byte_uid] &=
						~(1 << index_within_byte_uid);
				}
			}
			known_bits += result;
			if (known_bits < 40) {
				continue; // Fetch more bits.
			}

			// Reset antcl bit for SAK
			result = st25r391x_write_register_byte_check(
				i2c,
				ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
				0);
			if (result < 0)
				break;

			buffer[0] = 0x91 + (cascade_level * 2);
			buffer[1] = 0x70; // 5 known bytes + 2 header bytes
			buffer[2] = uid[(cascade_level - 1) * 5];
			buffer[3] = uid[(cascade_level - 1) * 5 + 1];
			buffer[4] = uid[(cascade_level - 1) * 5 + 2];
			buffer[5] = uid[(cascade_level - 1) * 5 + 3];
			buffer[6] = uid[(cascade_level - 1) * 5 + 4];
			result = st25r391x_transceive_frame(
				i2c, ints, buffer, 7, buffer, sizeof(buffer), 0,
				5000); // TODO: check rx timeout
			if (result < 0)
				break;

			if (buffer[0] & 0x04) {
				cascade_level++;
				known_bits = 0;

				// Set antcl bit for next cascade level.
				result = st25r391x_write_register_byte_check(
					i2c,
					ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
					ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_antcl);
				if (result < 0)
					break;
				continue; // Fetch next level.
			}

			// We have a full uid.
			if (cascade_level == 1) {
				if ((uid[0] ^ uid[1] ^ uid[2] ^ uid[3]) !=
				    uid[4]) {
					dev_err(priv->device,
						"st25r391x_do_select_nfca - UID BCC error - SAK = %#2hhx, uid = %.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx)",
						buffer[0], uid[0], uid[1],
						uid[2], uid[3], uid[4],
						uid[0] ^ uid[1] ^ uid[2] ^
							uid[3]);
					result = -1;
				} else {
					tag_info->sak = buffer[0];
					tag_info->uid[0] = uid[0];
					tag_info->uid[1] = uid[1];
					tag_info->uid[2] = uid[2];
					tag_info->uid[3] = uid[3];
					tag_info->uid_len = 4;
					result = 0;
				}
			} else if (cascade_level == 2) {
				if (uid[0] != 0x88 ||
				    uid[4] != (uid[0] ^ uid[1] ^ uid[2] ^
					       uid[3]) ||
				    uid[9] != (uid[5] ^ uid[6] ^ uid[7] ^
					       uid[8])) {
					dev_err(priv->device,
						"st25r391x_do_select_nfca - UID CT/BCC error SAK = %#2hhx, uid = CT=%.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx) %.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx)",
						buffer[0], uid[0], uid[1],
						uid[2], uid[3], uid[4],
						uid[0] ^ uid[1] ^ uid[2] ^
							uid[3],
						uid[5], uid[6], uid[7], uid[8],
						uid[9],
						uid[5] ^ uid[6] ^ uid[7] ^
							uid[8]);
					result = -1;
				} else {
					tag_info->sak = buffer[0];
					tag_info->uid[0] = uid[1];
					tag_info->uid[1] = uid[2];
					tag_info->uid[2] = uid[3];
					tag_info->uid[3] = uid[5];
					tag_info->uid[4] = uid[6];
					tag_info->uid[5] = uid[7];
					tag_info->uid[6] = uid[8];
					tag_info->uid_len = 7;
					result = 0;
				}
			} else /* if (cascade_level == 3) */ {
				if (uid[0] != 0x88 ||
				    uid[4] != (uid[0] ^ uid[1] ^ uid[2] ^
					       uid[3]) ||
				    uid[5] != 0x88 ||
				    uid[9] != (uid[5] ^ uid[6] ^ uid[7] ^
					       uid[8]) ||
				    uid[14] != (uid[10] ^ uid[11] ^ uid[12] ^
						uid[13])) {
					dev_err(priv->device,
						"st25r391x_do_select_nfca - UID CT/BCC error SAK = %#2hhx, uid = CT=%.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx) CT=%.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx) %.02hhx %.02hhx %.02hhx %.02hhx BCC=%.02hhx (=%.02hhx)",
						buffer[0], uid[0], uid[1],
						uid[2], uid[3], uid[4],
						uid[0] ^ uid[1] ^ uid[2] ^
							uid[3],
						uid[5], uid[6], uid[7], uid[8],
						uid[9],
						uid[5] ^ uid[6] ^ uid[7] ^
							uid[8],
						uid[10], uid[11], uid[12],
						uid[13], uid[14],
						uid[10] ^ uid[11] ^ uid[12] ^
							uid[13]);
					result = -1;
				} else {
					tag_info->sak = buffer[0];
					tag_info->uid[0] = uid[1];
					tag_info->uid[1] = uid[2];
					tag_info->uid[2] = uid[3];
					tag_info->uid[3] = uid[6];
					tag_info->uid[4] = uid[7];
					tag_info->uid[5] = uid[8];
					tag_info->uid[6] = uid[10];
					tag_info->uid[7] = uid[11];
					tag_info->uid[8] = uid[12];
					tag_info->uid[9] = uid[13];
					tag_info->uid_len = 7;
					result = 0;
				}
			}
		} while (known_bits < 40);

		// Reset antcl bit
		(void)st25r391x_write_register_byte_check(
			i2c,
			ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER,
			0);
	} while (0);
	return result;
}

static s32 st25r391x_nfca_reqa(struct st25r391x_i2c_data *priv, u8 atqa[])
{
	s32 result;
	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;

	do {
		result = st25r391x_set_iso14443a_mode(i2c);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfca_reqa: failed to set iso14443a mode: %d",
				result);
			break;
		}

		// Enable Tx & Rx
		result = st25r391x_enable_tx_and_rx(i2c);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfca_reqa: failed to enable tx and rx: %d",
				result);
			break;
		}

		st25r391x_clear_interrupts(
			ints,
			ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs |
				ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe,
			0, 0, 0);

		// Write Transmit REQA command
		result = st25r391x_direct_command(
			i2c, ST25R391X_TRANSMIT_REQA_COMMAND_CODE);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfca_reqa: failed to send Transmit REQA command %d",
				result);
			break;
		}
		// Receive data (ATQA)
		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs, 0,
			0, 0, 5000); // TODO: fix timeout
		if (result < 0) {
			break;
		}

		result = st25r391x_polling_wait_for_interrupt_bit(
			i2c, ints, ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe, 0,
			0, 0, 5000); // TODO: fix timeout
		if (result < 0) {
			break;
		}

		result = st25r391x_read_fifo(i2c, 2, atqa, NULL);
		if (result < 0) {
			dev_err(priv->device,
				"st25r391x_nfca_reqa: Read FIFO failed");
			break;
		}
	} while (0);
	return result;
}

static void st25r391x_nfca_process_tag(
	struct st25r391x_i2c_data *priv,
	const struct nfc_detected_tag_message_payload *tag_payload, int select)
{
	int matching_type;
	u16 tag_type = tag_payload->tag_type;

	if (select) {
		matching_type = tag_type ==
				priv->mode_params.select.tag_id.tag_type;
	} else {
		matching_type = priv->mode_params.discover.protocols &
				NFC_TAG_PROTOCOL_ISO14443A4;
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_ISO14443A_T2T &&
					tag_type == NFC_TAG_TYPE_ISO14443A_T2T;
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_MIFARE_CLASSIC &&
					tag_type == NFC_TAG_TYPE_MIFARE_CLASSIC;
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_ISO14443A_NFCDEP &&
					tag_type ==
						NFC_TAG_TYPE_ISO14443A_NFCDEP;
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_ISO14443A4 &&
					(tag_type ==
						 NFC_TAG_TYPE_ISO14443A_T4T ||
					 tag_type ==
						 NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP);
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_ISO14443A_T4T &&
					tag_type == NFC_TAG_TYPE_ISO14443A_T4T;
		matching_type =
			matching_type ?
				1 :
				      priv->mode_params.discover.protocols &
						NFC_TAG_PROTOCOL_ISO14443A_T4T_NFCDEP &&
					tag_type ==
						NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP;
	}

	if (matching_type) {
		u8 cid; // there is no cid with ISO 14443-A, we'll put sak here.

		if (tag_type == NFC_TAG_TYPE_ISO14443A ||
		    tag_type == NFC_TAG_TYPE_ISO14443A_T2T ||
		    tag_type == NFC_TAG_TYPE_MIFARE_CLASSIC ||
		    tag_type == NFC_TAG_TYPE_ISO14443A_NFCDEP) {
			cid = tag_payload->tag_info.iso14443a.sak;
		} else {
			cid = tag_payload->tag_info.iso14443a4.sak;
		}
		st25r391x_process_selected_tag(priv, tag_payload, cid);
	}
}

static void st25r391x_nfca_poll(struct st25r391x_i2c_data *priv, int select)
{
	// Passive poll NFC-A
	struct nfc_detected_tag_message_payload tag_payload;
	uint8_t tag_type = NFC_TAG_TYPE_ISO14443A;
	int rats_succeeded = 0;
	s32 result;

	memset(&tag_payload, 0, sizeof(tag_payload));
	result =
		st25r391x_nfca_reqa(priv, tag_payload.tag_info.iso14443a4.atqa);
	if (result == 2) {
		result = st25r391x_nfca_do_select(
			priv, &tag_payload.tag_info.iso14443a4);
		if (result >= 0) {
			uint8_t sak = tag_payload.tag_info.iso14443a4.sak;
			if (sak & 0x20) {
				result = st25r391x_nfca_rats(
					priv, &tag_payload.tag_info.iso14443a4);
				if (result >= 0) {
					if ((sak & 0x60) == 0x60) {
						tag_type =
							NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP;
					} else {
						tag_type =
							NFC_TAG_TYPE_ISO14443A_T4T;
					}
					rats_succeeded = 1;
				}
			}

			if (!rats_succeeded) {
				if ((sak & 0x60) == 0) {
					tag_type = NFC_TAG_TYPE_ISO14443A_T2T;
				} else if ((sak & 0x60) == 0x40) {
					tag_type =
						NFC_TAG_TYPE_ISO14443A_NFCDEP;
				}

				// Apply AN10833 logic to catch MIFARE Classic
				if (!(sak & 0x02)) { // bit 2 == 0
					if (sak & 0x08) { // bit 4 == 1
						if (sak & 0x10) { // bit 5 == 1
							if (sak &
							    0x01) { // bit 1 == 1
								// MIFARE Classic 2K
								tag_type =
									NFC_TAG_TYPE_MIFARE_CLASSIC;
							} else { // bit 1 == 0
								// MIFARE Classic 4K
								// SmartMX with MIFARE Classic 4K
								tag_type =
									NFC_TAG_TYPE_MIFARE_CLASSIC;
							}
						} else { // bit 5 == 0
							if (sak &
							    0x01) { // bit 1 == 1
								// MIFARE Mini
								tag_type =
									NFC_TAG_TYPE_MIFARE_CLASSIC;
							} else { // bit 1 == 0
								// MIFARE Classic 1K
								// SmartMX with MIFARE Classic 1K
								tag_type =
									NFC_TAG_TYPE_MIFARE_CLASSIC;
							}
						}
					}
				}
			}

			tag_payload.tag_type = tag_type;
			if (select == 0 ||
			    (priv->mode_params.select.tag_id.uid_len ==
				     tag_payload.tag_info.iso14443a.uid_len &&
			     memcmp(priv->mode_params.select.tag_id.uid,
				    tag_payload.tag_info.iso14443a.uid,
				    tag_payload.tag_info.iso14443a.uid_len) ==
				     0)) {
				st25r391x_nfca_process_tag(priv, &tag_payload,
							   select);
			}
		}
	}
}

void st25r391x_nfca_discover(struct st25r391x_i2c_data *priv)
{
	st25r391x_nfca_poll(priv, 0);
}

void st25r391x_nfca_select(struct st25r391x_i2c_data *priv)
{
	st25r391x_nfca_poll(priv, 1);
}
