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

#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>

#include "st25r391x.h"
#include "st25r391x_dev.h"

void st25r391x_write_to_device(struct st25r391x_i2c_data *priv, const u8* data, int count) {
    int ix;
    spin_lock(&priv->producer_lock);
    for (ix = 0; ix < count; ix++) {
        unsigned long head = priv->read_buffer_head;
        /* The spin_unlock() and next spin_lock() provide needed ordering. */
        unsigned long tail = READ_ONCE(priv->read_buffer_tail);
        if (CIRC_SPACE(head, tail, CIRCULAR_BUFFER_SIZE) >= count - ix) {
            priv->read_buffer[head] = data[ix];
            smp_store_release(&priv->read_buffer_head, (head + 1) & (CIRCULAR_BUFFER_SIZE - 1));
        } else {
            dev_err(&priv->i2c->dev, "Not writing to device as circular buffer would overflow");
            break;
        }
    }
    wake_up_interruptible(&priv->read_wq);
    spin_unlock(&priv->producer_lock);
}
