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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/circ_buf.h>
#include <linux/stdarg.h>

#include <linux/version.h>

#include "st25r391x.h"

#include "st25r391x_commands.h"
#include "st25r391x_common.h"
#include "st25r391x_dev.h"
#include "st25r391x_i2c.h"
#include "st25r391x_interrupts.h"
#include "st25r391x_nfca.h"
#include "st25r391x_nfcb.h"
#include "st25r391x_nfcf.h"
#include "st25r391x_registers.h"
#include "st25r391x_st25tb.h"

#include "nfc.h"

// ========================================================================== //
// PROTOCOL
// ========================================================================== //

#define CHIP_MODEL_IDENTITY "ST25R3916/7"

// ========================================================================== //
// Definitions and data structures
// ========================================================================== //

// Definitions

#define DRV_NAME "st25r391x"
#define DEVICE_NAME "nfc"

// Polling interval
#define POLLING_TIMEOUT_SECS_DIV 100

// Prototypes

static void st25r391x_polling_timer_cb(struct timer_list *t);
static void stop_polling_timer(struct st25r391x_i2c_data *priv);
static void restart_polling_timer(struct st25r391x_i2c_data *priv);

static int st25r391x_open(struct inode *inode, struct file *file);
static int st25r391x_release(struct inode *inode, struct file *file);
static ssize_t st25r391x_read(struct file *file, char __user *buffer,
			      size_t len, loff_t *offset);
static unsigned int st25r391x_poll(struct file *file, poll_table *wait);
static long st25r391x_unlocked_ioctl(struct file *file, unsigned int,
				     unsigned long);

static int st25r391x_i2c_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0)
static int st25r391x_i2c_remove(struct i2c_client *client);
#else
static void st25r391x_i2c_remove(struct i2c_client *client);
#endif

// ========================================================================== //
// Polling code
// ========================================================================== //

static void st25r391x_transition_to_idle(struct st25r391x_i2c_data *priv)
{
	struct nfc_message_header idle_response_header;

	if (priv->field_on) {
		(void)st25r391x_turn_field_off(priv);
	}

	priv->mode = mode_idle;
	idle_response_header.message_type =
		NFC_IDLE_MODE_ACKNOWLEDGE_MESSAGE_TYPE;
	idle_response_header.payload_length = 0;
	st25r391x_write_to_device(priv, (const u8 *)&idle_response_header,
				  sizeof(idle_response_header));
	stop_polling_timer(priv);
}

void st25r391x_process_selected_tag(
	struct st25r391x_i2c_data *priv,
	const struct nfc_detected_tag_message_payload *tag_payload, u8 cid)
{
	u16 payload_len;
	const u8 *uid;
	u8 uid_len;
	struct nfc_message_header detected_tag_message_header;
	int select_tag = priv->mode == mode_select ||
			 priv->mode_params.discover.flags &
				 NFC_DISCOVER_FLAGS_SELECT;

	switch (tag_payload->tag_type) {
	case NFC_TAG_TYPE_ISO14443A:
	case NFC_TAG_TYPE_ISO14443A_T2T:
	case NFC_TAG_TYPE_MIFARE_CLASSIC:
	case NFC_TAG_TYPE_ISO14443A_NFCDEP:
		uid = tag_payload->tag_info.iso14443a.uid;
		uid_len = tag_payload->tag_info.iso14443a.uid_len;
		payload_len = sizeof(tag_payload->tag_type) +
			      sizeof(tag_payload->tag_info.iso14443a) -
			      sizeof(tag_payload->tag_info.iso14443a.uid) +
			      tag_payload->tag_info.iso14443a.uid_len;
		break;

	case NFC_TAG_TYPE_ISO14443A_T4T:
	case NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP:
		uid = tag_payload->tag_info.iso14443a4.uid;
		uid_len = tag_payload->tag_info.iso14443a4.uid_len;
		payload_len = sizeof(tag_payload->tag_type) +
			      sizeof(tag_payload->tag_info.iso14443a4) -
			      sizeof(tag_payload->tag_info.iso14443a4.ats) +
			      tag_payload->tag_info.iso14443a4.ats_len;
		break;

	case NFC_TAG_TYPE_ISO14443B:
		uid = tag_payload->tag_info.iso14443b.pupi;
		uid_len = sizeof(tag_payload->tag_info.iso14443b.pupi);
		payload_len = sizeof(tag_payload->tag_type) +
			      sizeof(tag_payload->tag_info.iso14443b);
		break;

	case NFC_TAG_TYPE_ST25TB:
		uid = tag_payload->tag_info.st25tb.uid;
		uid_len = sizeof(tag_payload->tag_info.st25tb.uid);
		payload_len = sizeof(tag_payload->tag_type) +
			      sizeof(tag_payload->tag_info.st25tb);
		break;
	}

	detected_tag_message_header.message_type =
		select_tag ? NFC_SELECTED_TAG_MESSAGE_TYPE :
				   NFC_DETECTED_TAG_MESSAGE_TYPE;
	detected_tag_message_header.payload_length = payload_len;
	st25r391x_write_to_device(priv,
				  (const u8 *)&detected_tag_message_header,
				  sizeof(detected_tag_message_header));
	st25r391x_write_to_device(priv, (const u8 *)tag_payload, payload_len);

	if (select_tag) {
		priv->mode = mode_selected;
		priv->mode_params.selected.tag_id.tag_type =
			tag_payload->tag_type;
		priv->mode_params.selected.tag_id.cid = cid;
		priv->mode_params.selected.tag_id.uid_len = uid_len;
		memcpy((void *)priv->mode_params.selected.tag_id.uid, uid,
		       uid_len);
		memset(priv->mode_params.selected.tag_id.uid + uid_len, 0,
		       sizeof(priv->mode_params.selected.tag_id.uid) - uid_len);
		stop_polling_timer(priv);
	} else {
		if (priv->mode_params.discover.device_count > 0) {
			priv->mode_params.discover.device_count--;
			if (priv->mode_params.discover.device_count == 0) {
				st25r391x_transition_to_idle(priv);
			}
		}
	}
}

/**
 * Perform discovery polling.
 */
static void st25r391x_do_discover(struct st25r391x_i2c_data *priv)
{
	if (st25r391x_turn_field_on(priv) < 0)
		return;

	// Technology depends on the current mode.
	if (priv->mode == mode_discover &&
	    priv->mode_params.discover.protocols &
		    (NFC_TAG_PROTOCOL_ISO14443A |
		     NFC_TAG_PROTOCOL_ISO14443A_T2T |
		     NFC_TAG_PROTOCOL_MIFARE_CLASSIC |
		     NFC_TAG_PROTOCOL_ISO14443A_NFCDEP |
		     NFC_TAG_PROTOCOL_ISO14443A4 |
		     NFC_TAG_PROTOCOL_ISO14443A_T4T |
		     NFC_TAG_PROTOCOL_ISO14443A_T4T_NFCDEP)) {
		st25r391x_nfca_discover(priv);
	}

	// retest mode as discover may transition to idle/selected
	if (priv->mode == mode_discover &&
	    priv->mode_params.discover.protocols &
		    (NFC_TAG_PROTOCOL_ISO14443B)) {
		// Passive poll NFC-B
		st25r391x_nfcb_discover(priv);
	}

	if (priv->mode == mode_discover &&
	    priv->mode_params.discover.protocols & (NFC_TAG_PROTOCOL_ST25TB)) {
		// Passive poll ST25TB
		st25r391x_st25tb_discover(priv);
	}

	if (priv->mode == mode_discover &&
	    priv->mode_params.discover.protocols &
		    (NFC_TAG_PROTOCOL_NFCF | NFC_TAG_PROTOCOL_NFCF_NFCDEP)) {
		// Passive poll ST25TB
		st25r391x_nfcf_discover(priv);
	}
}

/**
 * Perform select polling.
 */
static void st25r391x_do_select(struct st25r391x_i2c_data *priv)
{
	if (st25r391x_turn_field_on(priv) < 0)
		return;

	// Technology depends on the current mode.
	if (priv->mode_params.select.tag_id.tag_type >=
		    NFC_TAG_TYPE_ISO14443A &&
	    priv->mode_params.select.tag_id.tag_type <=
		    NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP) {
		st25r391x_nfca_select(priv);
	} else if (priv->mode_params.select.tag_id.tag_type ==
		   NFC_TAG_TYPE_ISO14443B) {
		st25r391x_nfcb_select(priv);
	} else if (priv->mode_params.select.tag_id.tag_type ==
		   NFC_TAG_TYPE_ST25TB) {
		st25r391x_st25tb_select(priv);
	}
}

/**
 * Perform transceive polling.
 */
static void st25r391x_do_transceive_frame(struct st25r391x_i2c_data *priv)
{
	struct nfc_message_header response_message_header;
	struct nfc_message_transceive_frame_response_payload payload;
	struct i2c_client *i2c = priv->i2c;
	struct st25r391x_interrupts *ints = &priv->ints;
	u8 result_flags = priv->mode_params.transceive_frame.flags &
			  (NFC_TRANSCEIVE_FLAGS_NOCRC_RX |
			   NFC_TRANSCEIVE_RESPONSE_FLAGS_NOPAR_RX |
			   NFC_TRANSCEIVE_FLAGS_BITS);
	u16 rx_data_count = 0;
	u16 payload_len;
	s32 result;

	memset(&payload, 0, sizeof(payload));
	result = st25r391x_transceive_frame(
		i2c, ints, priv->mode_params.transceive_frame.tx_data,
		priv->mode_params.transceive_frame.tx_count, payload.rx_data,
		sizeof(payload.rx_data),
		priv->mode_params.transceive_frame.flags,
		priv->mode_params.transceive_frame.rx_timeout);
	if (result == 0 && priv->mode_params.transceive_frame.flags &
				   NFC_TRANSCEIVE_FLAGS_TIMEOUT) {
		result_flags |= NFC_TRANSCEIVE_RESPONSE_FLAGS_TIMEOUT;
	} else if (result > 0) {
		if (result_flags & NFC_TRANSCEIVE_RESPONSE_FLAGS_BITS) {
			rx_data_count = result >> 3;
			if (result & 0x07) {
				rx_data_count++;
			}
		} else {
			rx_data_count = result;
		}
	}

	payload_len =
		rx_data_count +
		offsetof(struct nfc_message_transceive_frame_response_payload,
			 rx_data);
	response_message_header.message_type =
		NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE;
	response_message_header.payload_length = payload_len;
	st25r391x_write_to_device(priv, (const u8 *)&response_message_header,
				  sizeof(response_message_header));

	if (result >= 0) {
		payload.flags = result_flags;
		payload.rx_count = result;
		st25r391x_write_to_device(priv, (const u8 *)&payload,
					  payload_len);

		// tag_id is common between selected and transceive_frame params.
		priv->mode = mode_selected;
	} else {
		payload.flags = NFC_TRANSCEIVE_RESPONSE_FLAGS_ERROR;
		st25r391x_write_to_device(priv, (const u8 *)&payload,
					  payload_len);

		st25r391x_transition_to_idle(priv);
	}
}

/**
 * Perform polling. Common with discovery and select modes.
 */
static void st25r391x_do_poll(struct work_struct *work)
{
	struct st25r391x_i2c_data *priv =
		container_of(work, struct st25r391x_i2c_data, polling_work);

	mutex_lock(&priv->command_lock);
	if (priv->mode == mode_idle) {
		mutex_unlock(&priv->command_lock);
		return;
	}

	if (priv->mode == mode_discover) {
		st25r391x_do_discover(priv);
	} else if (priv->mode == mode_select) {
		st25r391x_do_select(priv);
	} else if (priv->mode == mode_transceive_frame) {
		st25r391x_do_transceive_frame(priv);
	}

	priv->running_command = 0; // unlock mode & params
	wake_up_interruptible(&priv->write_wq);
	mutex_unlock(&priv->command_lock);

	if (priv->field_on &&
	    (priv->mode == mode_idle || priv->mode == mode_select ||
	     priv->mode == mode_discover)) {
		(void)st25r391x_turn_field_off(priv);
	}

	if (priv->mode == mode_discover || priv->mode == mode_select) {
		restart_polling_timer(priv);
	}
}

static void st25r391x_polling_timer_cb(struct timer_list *t)
{
	struct st25r391x_i2c_data *priv = from_timer(priv, t, polling_timer);
	if (priv->opened) {
		schedule_work(&priv->polling_work);
	}
}

static void restart_polling_timer(struct st25r391x_i2c_data *priv)
{
	del_timer_sync(&priv->polling_timer);
	mod_timer(&priv->polling_timer,
		  jiffies + HZ / POLLING_TIMEOUT_SECS_DIV);
}

static void stop_polling_timer(struct st25r391x_i2c_data *priv)
{
	del_timer_sync(&priv->polling_timer);
}

static void trigger_polling_work(struct st25r391x_i2c_data *priv)
{
	del_timer_sync(&priv->polling_timer);
	if (priv->opened) {
		schedule_work(&priv->polling_work);
	}
}

// ========================================================================== //
// File operations & commands
// ========================================================================== //

static int st25r391x_open(struct inode *inode, struct file *file)
{
	struct st25r391x_i2c_data *priv;
	priv = container_of(inode->i_cdev, struct st25r391x_i2c_data, cdev);
	file->private_data = priv;

	if (priv->opened) {
		return -EBUSY;
	}
	priv->opened = 1;
	priv->running_command = 0;
	priv->write_offset = 0;
	priv->read_buffer_head = 0;
	priv->read_buffer_tail = 0;
	priv->mode = mode_idle;

	return 0;
}

static int st25r391x_release(struct inode *inode, struct file *file)
{
	struct st25r391x_i2c_data *priv;
	priv = container_of(inode->i_cdev, struct st25r391x_i2c_data, cdev);
	priv->opened = 0;

	cancel_work_sync(&priv->polling_work);
	stop_polling_timer(priv);

	return 0;
}

static ssize_t st25r391x_read(struct file *file, char __user *buffer,
			      size_t len, loff_t *ppos)
{
	struct st25r391x_i2c_data *priv =
		(struct st25r391x_i2c_data *)file->private_data;
	int read_count = 0;
	spin_lock(&priv->consumer_lock);
	if (wait_event_interruptible(priv->read_wq,
				     priv->read_buffer_head !=
					     priv->read_buffer_tail)) {
		spin_unlock(&priv->consumer_lock);
		return -ERESTARTSYS;
	}
	/* Read index before reading contents at that index. */
	while (len > 0) {
		unsigned long head = smp_load_acquire(&priv->read_buffer_head);
		unsigned long tail = priv->read_buffer_tail;
		if (CIRC_CNT(head, tail, CIRCULAR_BUFFER_SIZE) >= 1) {
			if (copy_to_user(buffer, &priv->read_buffer[tail], 1)) {
				read_count = -EFAULT;
				break;
			}
			buffer++;
			read_count++;
			len--;
			/* Finish reading descriptor before incrementing tail. */
			smp_store_release(&priv->read_buffer_tail,
					  (tail + 1) &
						  (CIRCULAR_BUFFER_SIZE - 1));
		} else {
			// Len was exhausted, exit
			break;
		}
	}
	spin_unlock(&priv->consumer_lock);
	if (read_count > 0) {
		*ppos += read_count;
	}
	return read_count;
}

static void st25r391x_write_process_packet(struct st25r391x_i2c_data *priv,
					   u16 payload_len)
{
	uint8_t message_type =
		((struct nfc_message_header *)priv->write_buffer)->message_type;
	switch (message_type) {
	case NFC_IDENTIFY_REQUEST_MESSAGE_TYPE: {
		size_t identity_payload_len = sizeof(CHIP_MODEL_IDENTITY) - 1;
		struct nfc_message_header identity_response_header;
		identity_response_header.message_type =
			NFC_IDENTIFY_RESPONSE_MESSAGE_TYPE;
		identity_response_header.payload_length = identity_payload_len;
		st25r391x_write_to_device(priv,
					  (const u8 *)&identity_response_header,
					  sizeof(identity_response_header));
		st25r391x_write_to_device(priv, CHIP_MODEL_IDENTITY,
					  identity_payload_len);
		break;
	}

	case NFC_IDLE_MODE_REQUEST_MESSAGE_TYPE: {
		if (priv->mode != mode_idle) {
			st25r391x_transition_to_idle(priv);
		}
		break;
	}

	case NFC_DISCOVER_MODE_REQUEST_MESSAGE_TYPE: {
		const struct nfc_discover_mode_request_message_payload *payload =
			(const struct nfc_discover_mode_request_message_payload
				 *)(priv->write_buffer +
				    sizeof(struct nfc_message_header));
		priv->mode_params.discover.protocols = payload->protocols;
		priv->mode_params.discover.polling_period =
			payload->polling_period;
		priv->mode_params.discover.device_count = payload->device_count;
		priv->mode_params.discover.max_bitrate = payload->max_bitrate;
		priv->mode_params.discover.flags = payload->flags;
		if (priv->mode != mode_discover) {
			priv->mode = mode_discover;
			trigger_polling_work(priv);
		}
		break;
	}

	case NFC_SELECT_TAG_MESSAGE_TYPE: {
		const struct nfc_select_tag_message_payload *payload =
			(const struct nfc_select_tag_message_payload
				 *)(priv->write_buffer +
				    sizeof(struct nfc_message_header));
		memset(&priv->mode_params.select, 0,
		       sizeof(priv->mode_params.select));
		priv->mode_params.select.tag_id.tag_type = payload->tag_type;
		if (payload->tag_type >= NFC_TAG_TYPE_ISO14443A &&
		    payload->tag_type <= NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP) {
			priv->mode_params.select.tag_id.uid_len =
				payload->tag_id.iso14443a.uid_len;
			memcpy(priv->mode_params.select.tag_id.uid,
			       (const void *)payload->tag_id.iso14443a.uid,
			       payload->tag_id.iso14443a.uid_len);
		} else if (payload->tag_type == NFC_TAG_TYPE_ISO14443B) {
			priv->mode_params.select.tag_id.uid_len =
				sizeof(payload->tag_id.iso14443b.pupi);
			memcpy(priv->mode_params.select.tag_id.uid,
			       (const void *)payload->tag_id.iso14443b.pupi,
			       sizeof(payload->tag_id.iso14443b.pupi));
		} else if (payload->tag_type == NFC_TAG_TYPE_ST25TB) {
			priv->mode_params.select.tag_id.uid_len =
				sizeof(payload->tag_id.st25tb.uid);
			memcpy(priv->mode_params.select.tag_id.uid,
			       (const void *)payload->tag_id.st25tb.uid,
			       sizeof(payload->tag_id.st25tb.uid));
		} else {
			dev_err(priv->device,
				"NFC_SELECT_TAG_MESSAGE_TYPE: unexpected tag type %d",
				payload->tag_type);
		}
		if (priv->mode != mode_select) {
			priv->mode = mode_select;
			trigger_polling_work(priv);
		}
		break;
	}

	case NFC_TRANSCEIVE_FRAME_REQUEST_MESSAGE_TYPE: {
		const struct nfc_transceive_frame_request_message_payload
			*payload;
		if (priv->mode != mode_selected) {
			struct nfc_message_header response_message_header;
			struct nfc_message_transceive_frame_response_payload
				payload;

			dev_err(priv->device,
				"NFC_TRANSCEIVE_FRAME_REQUEST_MESSAGE_TYPE: unexpected message, tag must be selected first (mode=%d)",
				priv->mode);
			payload_len = offsetof(
				struct nfc_message_transceive_frame_response_payload,
				rx_data);
			response_message_header.message_type =
				NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE;
			response_message_header.payload_length = payload_len;
			st25r391x_write_to_device(
				priv, (const u8 *)&response_message_header,
				sizeof(response_message_header));
			payload.flags = NFC_TRANSCEIVE_RESPONSE_FLAGS_ERROR;
			st25r391x_write_to_device(priv, (const u8 *)&payload,
						  payload_len);

			if (priv->mode != mode_idle) {
				st25r391x_transition_to_idle(priv);
			}
			break;
		}
		payload =
			(const struct nfc_transceive_frame_request_message_payload
				 *)(priv->write_buffer +
				    sizeof(struct nfc_message_header));
		// tag_id is common between selected and transceive_frame params
		priv->mode_params.transceive_frame.tx_count = payload->tx_count;
		priv->mode_params.transceive_frame.flags = payload->flags;
		priv->mode_params.transceive_frame.rx_timeout =
			payload->rx_timeout;
		memcpy((void *)priv->mode_params.transceive_frame.tx_data,
		       priv->write_buffer + sizeof(struct nfc_message_header) +
			       offsetof(
				       struct nfc_transceive_frame_request_message_payload,
				       tx_data),
		       payload_len -
			       offsetof(
				       struct nfc_transceive_frame_request_message_payload,
				       tx_data));
		priv->mode = mode_transceive_frame;
		trigger_polling_work(priv);
		priv->running_command =
			1; // Block further commands until this one is executed.
		break;
	}
	}
}

static int st25r391x_write_bytes(struct st25r391x_i2c_data *priv,
				 const char __user *buffer, size_t buffer_len,
				 size_t count)
{
	size_t actual = count > buffer_len ? buffer_len : count;
	if (copy_from_user(priv->write_buffer + priv->write_offset, buffer,
			   actual))
		return -EFAULT;
	priv->write_offset += actual;
	return actual;
}

static ssize_t st25r391x_write(struct file *file, const char __user *buffer,
			       size_t len, loff_t *ppos)
{
	struct st25r391x_i2c_data *priv =
		(struct st25r391x_i2c_data *)file->private_data;
	int written_count = 0;
	if (len == 0) {
		return 0;
	}
	mutex_lock(&priv->command_lock);
	if (wait_event_interruptible(priv->write_wq,
				     priv->running_command == 0)) {
		mutex_unlock(&priv->command_lock);
		return -ERESTARTSYS;
	}
	do {
		u16 payload_len = 0;
		if (priv->write_offset < sizeof(struct nfc_message_header)) {
			written_count = st25r391x_write_bytes(
				priv, buffer, len,
				sizeof(struct nfc_message_header) -
					priv->write_offset);
			if (written_count < 0)
				break;
			len -= written_count;
			buffer += written_count;
		}
		payload_len = ((struct nfc_message_header *)priv->write_buffer)
				      ->payload_length;
		if (priv->write_offset <
		    payload_len + sizeof(struct nfc_message_header)) {
			int payload_written_count = st25r391x_write_bytes(
				priv, buffer, len,
				sizeof(struct nfc_message_header) +
					payload_len - priv->write_offset);
			if (payload_written_count < 0)
				break;
			written_count += payload_written_count;
			len -= payload_written_count;
			buffer += payload_written_count;
		}
		if (priv->write_offset ==
		    payload_len + sizeof(struct nfc_message_header)) {
			st25r391x_write_process_packet(priv, payload_len);
			priv->write_offset = 0;
		}
	} while (0);
	mutex_unlock(&priv->command_lock);
	if (written_count > 0) {
		*ppos += written_count;
	}
	return written_count;
}

static unsigned int st25r391x_poll(struct file *file, poll_table *wait)
{
	struct st25r391x_i2c_data *priv =
		(struct st25r391x_i2c_data *)file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &priv->read_wq, wait);
	poll_wait(file, &priv->write_wq, wait);
	spin_lock(&priv->consumer_lock);
	if (priv->read_buffer_head != priv->read_buffer_tail) {
		mask |= POLLIN | POLLRDNORM;
	}
	spin_unlock(&priv->consumer_lock);
	if (priv->running_command == 0) {
		mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}

static long st25r391x_unlocked_ioctl(struct file *file, unsigned int cmd,
				     unsigned long arg)
{
	// struct st25r391x_i2c_data *priv = (struct st25r391x_i2c_data *) file->private_data;
	// Fixed size commands.
	switch (cmd) {
	case NFC_RD_GET_PROTOCOL_VERSION: {
		uint64_t version = NFC_PROTOCOL_VERSION_1;
		return copy_to_user((uint64_t *)arg, &version,
				    sizeof(version)) ?
				     -EFAULT :
				     0;
	}
	}

	return -ENOIOCTLCMD;
}

static struct file_operations st25r391x_fops = {
	.owner = THIS_MODULE,
	.open = st25r391x_open,
	.read = st25r391x_read,
	.write = st25r391x_write,
	.release = st25r391x_release,
	.poll = st25r391x_poll,
	.unlocked_ioctl = st25r391x_unlocked_ioctl,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

// ========================================================================== //
// Probing, initialization and cleanup
// ========================================================================== //

static int st25r391x_i2c_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct st25r391x_i2c_data *priv;
	int err;
	s32 result;
	u8 buffer[2];

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	// Set default
	result = st25r391x_direct_command(i2c,
					  ST25R391X_SET_DEFAULT_COMMAND_CODE);
	if (result < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to send set default command %d",
			result);
		return result;
	}

	// Prevent the internal overheat protection to trigger below the junction
	// temperature
	buffer[0] = ST25R391X_TEST_SPACE_OVERHEAT_PROTECTION_REGISTER;
	buffer[1] = ST25R391X_TEST_SPACE_OVERHEAT_PROTECTION_VALUE;
	result = i2c_smbus_write_i2c_block_data(
		i2c, ST25R391X_TEST_ACCESS_COMMAND_CODE, 2, buffer);
	if (result < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to write test register %d",
			result);
		return result;
	}

	// Configure IO Configuration Registers
	result = st25r391x_write_registers_check(
		i2c, ST25R391X_IO_CONFIGURATION_1_REGISTER, 2, 0, 0b00100000);
	if (result < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to write IO Configuration Registers %d",
			result);
		return result;
	}

	// Read IC identity register to make sure we have a ST25R
	result = st25r391x_read_register_byte(i2c,
					      ST25R391X_IC_IDENTITY_REGISTER);
	if (result < 0) {
		return result;
	}
	if (result != 0b00101010) {
		dev_err(dev,
			"st25r391x_i2c_probe: Unexpected identity register value %d",
			result);
		return -1;
	}

	// Adjust regulators
	result = st25r391x_write_register_byte_check(
		i2c, ST25R391X_REGULATOR_VOLTAGE_CONTROL_REGISTER, 0b11110000);
	if (result < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to write regulator voltage control register: %d",
			result);
		return result;
	}
	result = st25r391x_write_register_byte_check(
		i2c, ST25R391X_REGULATOR_VOLTAGE_CONTROL_REGISTER, 0b01110000);
	if (result < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to write regulator voltage control register: %d",
			result);
		return result;
	}

	i2c_set_clientdata(i2c, priv);
	priv->i2c = i2c;

	timer_setup(&priv->polling_timer, st25r391x_polling_timer_cb, 0);

	// Register device.
	err = alloc_chrdev_region(&priv->chrdev, 0, 2, DEVICE_NAME);
	if (err < 0) {
		dev_err(dev,
			"st25r391x_i2c_probe: Failed to register character device: %d",
			err);
		st25r391x_i2c_remove(i2c);
		return err;
	}

	// Create device class
	priv->st25r391x_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(priv->st25r391x_class)) {
		err = PTR_ERR(priv->st25r391x_class);
		dev_err(dev, "st25r391x_i2c_probe: class_create failed: %d",
			err);
		st25r391x_i2c_remove(i2c);
		return err;
	}

	cdev_init(&priv->cdev, &st25r391x_fops);

	err = cdev_add(&priv->cdev, priv->chrdev, 1);
	if (err) {
		dev_err(dev, "st25r391x_i2c_probe: Failed to add cdev: %d",
			err);
		st25r391x_i2c_remove(i2c);
		return err;
	}

	priv->device = device_create(priv->st25r391x_class, dev, priv->chrdev,
				     NULL,
				     /* no additional data */ DEVICE_NAME "%d",
				     MINOR(priv->chrdev));
	if (IS_ERR(priv->device)) {
		err = PTR_ERR(priv->device);
		dev_err(dev, "st25r391x_i2c_probe: Failed to create device: %d",
			err);
		st25r391x_i2c_remove(i2c);
		return err;
	}

	spin_lock_init(&priv->producer_lock);
	spin_lock_init(&priv->consumer_lock);
	mutex_init(&priv->command_lock);
	init_waitqueue_head(&priv->read_wq);
	init_waitqueue_head(&priv->write_wq);
	INIT_WORK(&priv->polling_work, st25r391x_do_poll);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0)
static int st25r391x_i2c_remove(struct i2c_client *client)
#else
static void st25r391x_i2c_remove(struct i2c_client *client)
#endif
{
	struct st25r391x_i2c_data *priv;
	priv = i2c_get_clientdata(client);

	if (priv->chrdev) {
		if (priv->st25r391x_class) {
			if (priv->cdev.ops) {
				device_destroy(priv->st25r391x_class,
					       MKDEV(MAJOR(priv->chrdev),
						     MINOR(priv->chrdev)));
				cdev_del(&priv->cdev);
			}
			class_destroy(priv->st25r391x_class);
		}
		unregister_chrdev_region(priv->chrdev, 2);
	}

	del_timer_sync(&priv->polling_timer);
	cancel_work_sync(&priv->polling_work);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0)
	return 0;
#endif
}

#ifdef CONFIG_OF
static const struct of_device_id st25r391x_i2c_ids[] = {
	{
		.compatible = "stm,st25r391x",
	},
	{}
};
MODULE_DEVICE_TABLE(of, st25r391x_i2c_ids);
#endif

static struct i2c_driver st25r391x_i2c_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = of_match_ptr(st25r391x_i2c_ids),
    },
    .probe              = st25r391x_i2c_probe,
    .remove             = st25r391x_i2c_remove,
};

module_i2c_driver(st25r391x_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics ST25R3916/7 Driver");
MODULE_AUTHOR("Paul Guyot <pguyot@kallisys.net>");
MODULE_LICENSE("GPL");
