#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

import nfcdev

MODELS = {
    0x02: {
        (
            6,
            0b000011,
            "SRIX4K",
        ),  # http://www.orangetags.com/wp-content/downloads/datasheet/STM/srix4k.pdf
        (
            6,
            0b000110,
            "SRI512",
        ),  # http://www.advanide.com/wp-content/uploads/products/rfid/SRI512.pdf
        (
            6,
            0b001100,
            "SRT512",
        ),  # https://www.advanide.de/wp-content/uploads/products/rfid/SRT512.pdf
        (
            6,
            0b000111,
            "SRI4K",
        ),  # https://www.advanide.de/wp-content/uploads/products/rfid/SRI4K.pdf
        (
            6,
            0b001111,
            "SRI2K",
        ),  # https://www.advanide.de/wp-content/uploads/products/rfid/SRI2K.pdf
        (
            8,
            0x1B,
            "ST25TB512-AC",
        ),  # https://www.st.com/resource/en/datasheet/st25tb512-ac.pdf
        (
            8,
            0x1F,
            "ST25TB04K",
        ),  # https://www.st.com/resource/en/datasheet/st25tb04k.pdf
        (
            8,
            0x33,
            "ST25TB512-AT",
        ),  # https://www.st.com/resource/en/datasheet/st25tb512-at.pdf
        (
            8,
            0x3F,
            "ST25TB02K",
        ),  # https://www.st.com/resource/en/datasheet/st25tb02k.pdf
    }
}
MANUFACTURERS = {
    0x01: "Motorola",
    0x02: "ST Microelectronics",
    0x03: "Hitachi",
    0x04: "NXP Semiconductors",
    0x05: "Infineon Technologies",
    0x06: "Cylinc",
    0x07: "Texas Instruments Tag-it",
    0x08: "Fujitsu Limited",
    0x09: "Matsushita Electric Industrial",
    0x0A: "NEC",
    0x0B: "Oki Electric",
    0x0C: "Toshiba",
    0x0D: "Mitsubishi Electric",
    0x0E: "Samsung Electronics",
    0x0F: "Hyundai Electronics",
    0x10: "LG Semiconductors",
    0x16: "EM Microelectronic-Marin",
    0x1F: "Melexis",
    0x2B: "Maxim",
    0x33: "AMIC",
    0x44: "GenTag, Inc (USA)",
    0x45: "Invengo Information Technology Co.Ltd",
}


def little_endian_hex_to_str(data):
    ba = bytearray(data)
    ba.reverse()
    return ":".join("{:02x}".format(c) for c in ba)


def print_iso14443a_tag_info(tag_info):
    print(f"ATQA: {little_endian_hex_to_str(tag_info.atqa)}")
    print(f"SAK: {tag_info.sak:02x}")
    print(f"UID: {little_endian_hex_to_str(tag_info.uid)}")


def print_mifare_classic_tag_info(tag_info):
    print_iso14443a_tag_info(tag_info)
    if tag_info.sak == 0x08:
        print("Product: NXP MIFARE Classic 1k")
    elif tag_info.sak == 0x18:
        print("Product: NXP MIFARE Classic 4k")
    else:
        print("Product: Unknown")


def print_iso14443a4_tag_info(tag_info):
    print_iso14443a_tag_info(tag_info)
    print(f"ATS: {little_endian_hex_to_str(tag_info.ats)}")


def print_iso14443b_tag_info(tag_info):
    print(f"PUPI: {little_endian_hex_to_str(tag_info.pupi)}")
    print(f"Application data: {little_endian_hex_to_str(tag_info.application_data)}")
    print(f"Protocol info: {little_endian_hex_to_str(tag_info.protocol_info)}")


def print_st25tb_tag_info(tag_info):
    # uid is in little endian
    uid = bytearray(tag_info.uid)
    uid.reverse()
    uid_str = ":".join("{:02x}".format(c) for c in uid)
    print(f"UID: {uid_str}")
    if uid[0] != 0xD0:
        print(f"Unexpected MSB, got {uid[0]}")
    if uid[1] in MANUFACTURERS:
        manufacturer = MANUFACTURERS[uid[1]]
        print(f"Manufacturer: {manufacturer}")
    else:
        print(f"Manufacturer: unknown ({int(uid[1])})")
    serial_start = 2
    if uid[1] in MODELS:
        model_unknown = True
        for bits, model_id, model_str in MODELS[uid[1]]:
            model = uid[2]
            if bits < 8:
                model = model >> (8 - bits)
            if model == model_id:
                print(f"Model: {model_str}")
                if bits < 8:
                    uid[2] = uid[2] & ~(model << (8 - bits))
                else:
                    serial_start = 3
                model_unknown = False
                break
        if model_unknown:
            print(f"Model: unknown ({uid[2]})")
    serial = ":".join("{:02x}".format(c) for c in uid[serial_start:])
    print(f"Serial number: {serial}")


with nfcdev.NFCDev("/dev/nfc0") as nfc:
    print("Version check: {nfc.check_version()}")
    print("Chip model: {nfc.get_identify_chip_model()}")
    print("Discovering tags (exit with control-C)\n")
    nfc.write_message(
        nfcdev.NFCDiscoverModeRequestMessage(nfcdev.NFC_TAG_PROTOCOL_ALL, 0, 0, 0, 0)
    )

    while True:
        try:
            header, payload = nfc.read_message()

            if header.message_type == nfcdev.NFC_DETECTED_TAG_MESSAGE_TYPE:
                tag_info = payload
                if tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443A:
                    print("ISO-14443-A generic tag")
                    print_iso14443a_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443A_T2T:
                    print("ISO-14443-A T2T tag")
                    print_iso14443a_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_MIFARE_CLASSIC:
                    print("MIFARE Classic tag")
                    print_mifare_classic_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443A_NFCDEP:
                    print("ISO-14443-A tag with NFCDEP")
                    print_iso14443a_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443A_T4T:
                    print("ISO-14443-A-4 (T4T) tag")
                    print_iso14443a4_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP:
                    print("ISO-14443-A-4 (T4T) tag with NFCDEP")
                    print_iso14443a4_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ISO14443B:
                    print("ISO-14443-B tag")
                    print_iso14443b_tag_info(tag_info.tag_info)
                elif tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ST25TB:
                    print("ST25TB tag")
                    print_st25tb_tag_info(tag_info.tag_info)
                print()
            else:
                print(f"Unexpected message (type={header.message_type})")
        except KeyboardInterrupt:
            break
