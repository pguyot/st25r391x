# ST25R3916/7 driver for Linux (Raspbian)

## Datasheet and technical documents

- [Official documentation](https://www.st.com/en/nfc/st25r3916.html) from ST
- ISO-14443-2 specification
- ISO-14443-3 specification
- ISO-14443-4 specification
- NFC Type 2 Tags specification
- NXP AN10833 for tag identification (to identify MIFARE classic tags)
- [SRI512](https://www.advanide.de/wp-content/uploads/products/rfid/SRI512.pdf)
for ST25TB protocol

## Electronic interface

This driver is designed for an ST25R3916 or ST25R3917 only connected on I2C bus
(with no interrupt line). It was developed specifically for
([Nabaztag 2022 NFC card](https://tagtagtag.fr/)

It can also be used with ST Microelectronics evaluation board after a small
configuration (a soldering iron is required).

## Installation

Install requirements

    sudo apt-get install raspberrypi-kernel-headers

Clone source code.

    git clone https://github.com/pguyot/st25r391x

Compile and install with

    cd st25r391x
    make
    sudo make install

Makefile will automatically edit /boot/config.txt and add/enable if required
the following params and overlays:

    dtparam=i2c_arm=on
    dtoverlay=cr14

You might want to review changes before rebooting.

Reboot.

## Interface

Driver creates device /dev/nfc0

The interface was developed with companion Python library
[pynfcdev](https://github.com/pguyot/pynfcdev).
