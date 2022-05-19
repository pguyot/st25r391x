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
#include "st25r391x_nfcb.h"
#include "st25r391x_registers.h"
#include "st25r391x.h"

// ISO-14443-B commands
#define ISO14443B_COMMAND_REQB_APF 0x05
#define ISO14443B_COMMAND_REQB_AFI_ALL 0x00
#define ISO14443B_COMMAND_REQB_PARAM_NORMAL_N1 0x00

#define ISO14443B_COMMAND_ATQB_HEADER 0x50

#define ISO14443B_COMMAND_ATTRIB_HEADER 0x1D
#define ISO14443B_COMMAND_ATTRIB_PARAM1_DEFAULT 0x00
#define ISO14443B_COMMAND_ATTRIB_PARAM2_DEFAULT 0x08
#define ISO14443B_COMMAND_ATTRIB_PARAM3 0x01

s32 st25r391x_set_iso14443b_mode(struct i2c_client* i2c) {
    s32 result;

    do {
        // Disable wake up mode, if set
        result = st25r391x_clear_register_bits(i2c, ST25R391X_OPERATION_CONTROL_REGISTER, ST25R391X_OPERATION_CONTROL_REGISTER_wu);
        if (result < 0) break;
        result = st25r391x_write_registers_check(i2c, ST25R391X_MODE_DEFINITION_REGISTER, 2, ST25R391X_MODE_DEFINITION_REGISTER_iso14443b_i | ST25R391X_MODE_DEFINITION_REGISTER_tr_am, 0);
        if (result < 0) break;
        result = st25r391x_write_register_byte_check(i2c, ST25R391X_TX_DRIVER_REGISTER, ST25R391X_TX_DRIVER_REGISTER_am_12pct);
        if (result < 0) break;
        result = st25r391x_write_registers_check(i2c, ST25R391X_ISO14443B_SETTINGS_1_REGISTER, 2, 0, 0);
        if (result < 0) break;
        result = st25r391x_write_registers_check(i2c, ST25R391X_RECEIVER_CONFIGURATION_1_REGISTER, 4, 0x04, 0x3D, 0x00, 0x00);
        if (result < 0) break;
        result = st25r391x_write_bank_b_registers(i2c, ST25R391X_CORRELATOR_CONFIGURATION_1_B_REGISTER, 2, 0x1B, 0x00);
        if (result < 0) break;
    } while (0);
    return result;
}


static s32 st25r391x_nfcb_reqb_cid(struct st25r391x_i2c_data *priv, struct nfc_tag_info_iso14443b *tag_info, u8 cid) {
    s32 result;
    u8 buffer[14];
    struct i2c_client *i2c = priv->i2c;
    struct st25r391x_interrupts *ints = &priv->ints;

    do {
        result = st25r391x_set_iso14443b_mode(i2c);
        if (result < 0) {
            dev_err(priv->device, "st25r391x_nfcb_reqb_cid: Failed to set iso14443b mode: %d", result);
            break;
        }

        // Enable Tx & Rx
        result = st25r391x_enable_tx_and_rx(i2c);
        if (result < 0) {
            dev_err(priv->device, "st25r391x_nfcb_reqb_cid; Failed to enable tx and rx: %d", result);
            break;
        }

        buffer[0] = ISO14443B_COMMAND_REQB_APF;
        buffer[1] = ISO14443B_COMMAND_REQB_AFI_ALL;
        buffer[2] = ISO14443B_COMMAND_REQB_PARAM_NORMAL_N1;
        result = st25r391x_transceive_frame(i2c, ints, buffer, 3, buffer, sizeof(buffer), 1);
        if (result < 0) break;

        if (result != 14 || buffer[0] != ISO14443B_COMMAND_ATQB_HEADER) {
            result = -1;
            break;
        }

        memcpy((void*) tag_info, (const void*) buffer + 1, sizeof(*tag_info));

        buffer[0] = ISO14443B_COMMAND_ATTRIB_HEADER;
        buffer[5] = ISO14443B_COMMAND_ATTRIB_PARAM1_DEFAULT;
        buffer[6] = ISO14443B_COMMAND_ATTRIB_PARAM2_DEFAULT;
        buffer[7] = ISO14443B_COMMAND_ATTRIB_PARAM3;
        buffer[8] = cid;  // CID
        result = st25r391x_transceive_frame(i2c, ints, buffer, 9, buffer, sizeof(buffer), 1);
        if (result < 0) break;

        if (result != 3 || buffer[0] != 0) {
            result = -1;
            break;
        }
    } while (0);
    return result;
}

void st25r391x_nfcb_discover(struct st25r391x_i2c_data *priv) {
    // Passive poll NFC-A
    struct nfc_detected_tag_message_payload tag_payload;
    s32 result;
    u8 cid;

    cid = 0;
    memset(&tag_payload, 0, sizeof(tag_payload));
    result = st25r391x_nfcb_reqb_cid(priv, &tag_payload.tag_info.iso14443b, cid);
    if (result >= 0 && priv->mode_params.discover.protocols & NFC_TAG_PROTOCOL_ISO14443B) {
        tag_payload.tag_type = NFC_TAG_TYPE_ISO14443B;
        st25r391x_process_selected_tag(priv, &tag_payload, cid);
    }
}

void st25r391x_nfcb_select(struct st25r391x_i2c_data *priv) {
    // Passive poll select ST25TB
    struct nfc_detected_tag_message_payload tag_payload;
    s32 result;
    u8 cid;

    cid = 0;
    memset(&tag_payload, 0, sizeof(tag_payload));
    result = st25r391x_nfcb_reqb_cid(priv, &tag_payload.tag_info.iso14443b, cid);
    if (result >= 0 && memcmp(priv->mode_params.select.tag_id.uid, tag_payload.tag_info.iso14443b.pupi, sizeof(tag_payload.tag_info.iso14443b.pupi)) == 0) {
        tag_payload.tag_type = NFC_TAG_TYPE_ISO14443B;
        st25r391x_process_selected_tag(priv, &tag_payload, cid);
    }
}
