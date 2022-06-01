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

#ifndef ST25R391X_H
#define ST25R391X_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "st25r391x_interrupts.h"

#include "nfc.h"

enum st25r391x_mode {
	mode_idle,
	mode_discover,
	mode_select,
	mode_selected,
	mode_transceive_frame
};

#define MAX_PACKET_SIZE 1285
#define CIRCULAR_BUFFER_SIZE 8192

// Data structures

struct st25r391x_tag_id {
	u8 tag_type;
	u8 cid;
	u8 uid_len;
	u8 uid[10];
};

struct st25r391x_discover_params {
	u64 protocols;
	u32 polling_period;
	u8 device_count;
	u8 max_bitrate;
	u8 flags;
};

struct st25r391x_select_params {
	struct st25r391x_tag_id tag_id;
};

struct st25r391x_selected_params {
	struct st25r391x_tag_id tag_id;
};

struct st25r391x_transceive_frame_params {
	struct st25r391x_tag_id tag_id;
	u16 tx_count;
	u8 flags;
	u16 rx_timeout;
	u8 tx_data[512];
};

union st25r391x_mode_params {
	struct st25r391x_discover_params discover;
	struct st25r391x_select_params select;
	struct st25r391x_selected_params selected;
	struct st25r391x_transceive_frame_params transceive_frame;
};

struct st25r391x_i2c_data {
	struct i2c_client *i2c;
	struct st25r391x_interrupts ints;
	dev_t chrdev;
	struct class *st25r391x_class;
	struct cdev cdev;
	struct device *device;
	struct timer_list polling_timer;
	struct work_struct polling_work;
	spinlock_t producer_lock;
	spinlock_t consumer_lock;
	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	int read_buffer_head;
	int read_buffer_tail;
	char read_buffer[CIRCULAR_BUFFER_SIZE];
	int write_offset; // current offset in write buffer
	char write_buffer[MAX_PACKET_SIZE];
	struct mutex command_lock; // locks mode and params
	unsigned opened : 1; // whether the device is opened
	unsigned field_on : 1; // whether field is on
	unsigned running_command : 1; // whether we're currently running a command
	enum st25r391x_mode mode;
	union st25r391x_mode_params mode_params;
};

void st25r391x_process_selected_tag(
	struct st25r391x_i2c_data *priv,
	const struct nfc_detected_tag_message_payload *tag_payload, u8 cid);

#endif
