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
#include "st25r391x_st25tb.h"
#include "st25r391x.h"

// ST25TB commands
#define ST25TB_COMMAND_INITIATE_H 0x06
#define ST25TB_COMMAND_INITIATE_L 0x00
#define ST25TB_COMMAND_PCALL16_H 0x06
#define ST25TB_COMMAND_PCALL16_L 0x04
#define ST25TB_COMMAND_READ_BLOCK_H 0x08
#define ST25TB_COMMAND_WRITE_BLOCK_H 0x09
#define ST25TB_COMMAND_GET_UID 0x0B
#define ST25TB_COMMAND_RESET_TO_INVENTORY 0x0C
#define ST25TB_COMMAND_SELECT_H 0x0E
#define ST25TB_COMMAND_COMPLETION 0x0F

static s32 st25r391x_st25tb_select_and_get_uid(struct st25r391x_i2c_data *priv, struct nfc_tag_info_st25tb *tag_info, u8 chip_id) {
    s32 result;
    u8 buffer[10];
    struct i2c_client *i2c = priv->i2c;
    struct st25r391x_interrupts *ints = &priv->ints;

    do {
        buffer[0] = ST25TB_COMMAND_SELECT_H;
        buffer[1] = chip_id;
        result = st25r391x_transceive_frame(i2c, ints, buffer, 2, buffer, sizeof(buffer), 0, 5000); // TODO: check rx timeout
        if (result < 0) break;

        if (result != 3 || buffer[0] != chip_id) {
            dev_err(priv->device, "st25r391x_st25tb_select_and_get_uid: unexpected answer to select, result = %d, buffer[0] = %d, chip_id = %d", result, buffer[0], chip_id);
            result = -1;
            break;
        }

        buffer[0] = ST25TB_COMMAND_GET_UID;
        result = st25r391x_transceive_frame(i2c, ints, buffer, 1, buffer, sizeof(buffer), 0, 5000); // TODO: check rx timeout
        if (result < 0) break;

        if (result != 10) {
            dev_err(priv->device, "st25r391x_st25tb_select_and_get_uid: unexpected answer to get_uid, result = %d", result);
            result = -1;
            break;
        }

        memcpy((void*) tag_info->uid, (const void*) buffer, sizeof(tag_info->uid));
    } while (0);
    return result;
}

static s32 st25r391x_st25tb_initiate(struct st25r391x_i2c_data *priv, struct nfc_tag_info_st25tb *tag_info, u8 *cid) {
    s32 result;
    u8 buffer[3];
    struct i2c_client *i2c = priv->i2c;
    struct st25r391x_interrupts *ints = &priv->ints;

    do {
        result = st25r391x_set_iso14443b_mode(i2c);
        if (result < 0) {
            dev_err(priv->device, "st25r391x_st25tb_initiate: Failed to set iso14443b mode: %d", result);
            break;
        }

        // Enable Tx & Rx
        result = st25r391x_enable_tx_and_rx(i2c);
        if (result < 0) {
            dev_err(priv->device, "st25r391x_st25tb_initiate: : Failed to enable tx and rx: %d", result);
            break;
        }

        buffer[0] = ST25TB_COMMAND_INITIATE_H;
        buffer[1] = ST25TB_COMMAND_INITIATE_L;
        result = st25r391x_transceive_frame(i2c, ints, buffer, 2, buffer, sizeof(buffer), 0, 5000); // TODO: check rx timeout
        if (result < 0) break;

        if (buffer[0] == 255) {
            dev_err(priv->device, "st25r391x_st25tb_initiate: got a collision, unimplemented");
            result = -1;
            break;
        }
        
        result = st25r391x_st25tb_select_and_get_uid(priv, tag_info, buffer[0]);
        if (result >= 0) *cid = buffer[0];
    } while (0);
    return result;
}

void st25r391x_st25tb_discover(struct st25r391x_i2c_data *priv) {
    // Passive poll discover ST25TB
    struct nfc_detected_tag_message_payload tag_payload;
    s32 result;
    u8 cid;

    memset(&tag_payload, 0, sizeof(tag_payload));
    result = st25r391x_st25tb_initiate(priv, &tag_payload.tag_info.st25tb, &cid);
    if (result >= 0 && priv->mode_params.discover.protocols & NFC_TAG_PROTOCOL_ST25TB) {
        tag_payload.tag_type = NFC_TAG_TYPE_ST25TB;
        st25r391x_process_selected_tag(priv, &tag_payload, cid);
    }
}

void st25r391x_st25tb_select(struct st25r391x_i2c_data *priv) {
    // Passive poll select ST25TB
    struct nfc_detected_tag_message_payload tag_payload;
    s32 result;
    u8 cid;

    memset(&tag_payload, 0, sizeof(tag_payload));
    result = st25r391x_st25tb_initiate(priv, &tag_payload.tag_info.st25tb, &cid);
    if (result >= 0 && memcmp(priv->mode_params.select.tag_id.uid, tag_payload.tag_info.st25tb.uid, sizeof(tag_payload.tag_info.st25tb.uid)) == 0) {
        tag_payload.tag_type = NFC_TAG_TYPE_ST25TB;
        st25r391x_process_selected_tag(priv, &tag_payload, cid);
    }
}
