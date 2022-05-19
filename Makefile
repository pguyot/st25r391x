# SPDX-License-Identifier: GPL-2.0
KERNELRELEASE ?= $(shell uname -r)

obj-m += st25r391x.o
st25r391x-objs := st25r391x_main.o st25r391x_common.o st25r391x_dev.o st25r391x_i2c.o st25r391x_interrupts.o st25r391x_nfca.o st25r391x_nfcb.o st25r391x_nfcf.o st25r391x_st25tb.o
dtbo-y += st25r391x.dtbo

targets += $(dtbo-y)

# Gracefully supporting the new always-y without cutting off older target with kernel 4.x
ifeq ($(firstword $(subst ., ,$(KERNELRELEASE))),4)
	always := $(dtbo-y)
else
	always-y := $(dtbo-y)
endif

CFLAGS_st25r391x.o += -std=gnu11 -Wno-vla
CFLAGS_st25r391x_i2c.o += -std=gnu11 -Wno-vla

all:
	make -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) clean

install: st25r391x.ko st25r391x.dtbo
	install -o root -m 755 -d /lib/modules/$(KERNELRELEASE)/kernel/input/misc/
	install -o root -m 644 st25r391x.ko /lib/modules/$(KERNELRELEASE)/kernel/input/misc/
	depmod -a $(KERNELRELEASE)
	install -o root -m 644 st25r391x.dtbo /boot/overlays/
	sed /boot/config.txt -i -e "s/^#dtparam=i2c_arm=on/dtparam=i2c_arm=on/"
	grep -q -E "^dtparam=i2c_arm=on" /boot/config.txt || printf "dtparam=i2c_arm=on\n" >> /boot/config.txt
	sed /boot/config.txt -i -e "s/^#dtoverlay=st25r391x/dtoverlay=st25r391x/"
	grep -q -E "^dtoverlay=st25r391x" /boot/config.txt || printf "dtoverlay=st25r391x\n" >> /boot/config.txt

.PHONY: all clean install
