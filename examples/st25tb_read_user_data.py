#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

import nfcdev


def process_st25tb_tag(nfc, tag_info):
    # uid is in little endian
    uid_be = bytearray(tag_info.uid)
    uid_be.reverse()
    uid_str = ":".join("{:02x}".format(c) for c in uid_be)
    print(f"UID: {uid_str}")
    if uid_be[0] != 0xD0:
        print(f"Unexpected MSB, got {uid_be[0]}")
    if uid_be[1] != 0x02:
        print(f"Not a STMicroelectronics chip, will read block 255 anyway")
    nfc.write_message(
        nfcdev.NFCTransceiveFrameRequestMessage(
            2, 0, bytearray([nfcdev.ST25TB_COMMAND_READ_BLOCK, 255])
        )
    )
    header, payload = nfc.read_message()

    if header.message_type == nfcdev.NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE:
        if payload.flags & nfcdev.NFC_TRANSCEIVE_FLAGS_ERROR:
            print(f"Read error (tag removed?)")
            return
        elif payload.rx_count != 6:
            print(f"Unexpected response length, rx_count={payload.rx_count} != 6")
        data = bytearray(payload.data[0:4])
        data.reverse()  # First byte is less significant
        data_str = ":".join("{:02x}".format(c) for c in data)
        print(f"System block (255): {data_str}")
    else:
        print(f"Unexpected message (type={header.message_type})")
        return

    for block in range(7, 16):
        nfc.write_message(
            nfcdev.NFCTransceiveFrameRequestMessage(
                2, 0, bytearray([nfcdev.ST25TB_COMMAND_READ_BLOCK, block])
            )
        )
        header, payload = nfc.read_message()

        if header.message_type == nfcdev.NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE:
            if payload.flags & nfcdev.NFC_TRANSCEIVE_FLAGS_ERROR:
                print(f"Read error (tag removed?)")
                return
            elif payload.rx_count != 6:
                print(f"Unexpected response length, rx_count={payload.rx_count} != 6")
            data = bytearray(payload.data[0:4])
            data.reverse()  # First byte is less significant
            data_str = ":".join("{:02x}".format(c) for c in data)
            print(f"User data block {block}: {data_str}")
        else:
            print(f"Unexpected message (type={header.message_type})")
            return


with nfcdev.NFCDev("/dev/nfc0") as nfc:
    print("Version check: {nfc.check_version()}")
    print("Chip model: {nfc.get_identify_chip_model()}")
    print("Selecting ST25TB tags (exit with control-C)\n")
    nfc.write_message(
        nfcdev.NFCDiscoverModeRequestMessage(
            nfcdev.NFC_TAG_PROTOCOL_ST25TB, 0, 0, 0, nfcdev.NFC_DISCOVER_FLAGS_SELECT
        )
    )

    while True:
        try:
            header, payload = nfc.read_message()

            if header.message_type == nfcdev.NFC_SELECTED_TAG_MESSAGE_TYPE:
                tag_info = payload
                if tag_info.tag_type == nfcdev.NFC_TAG_TYPE_ST25TB:
                    process_st25tb_tag(nfc, tag_info.tag_info)
                else:
                    print("Unexpected tag type")
                # Transition to idle
                nfc.write_message(nfcdev.NFCIdleModeRequestMessage())
            elif header.message_type == nfcdev.NFC_IDLE_MODE_ACKNOWLEDGE_MESSAGE_TYPE:
                print("Selecting another tag (exit with control-C)\n")
                nfc.write_message(
                    nfcdev.NFCDiscoverModeRequestMessage(
                        nfcdev.NFC_TAG_PROTOCOL_ST25TB,
                        0,
                        0,
                        0,
                        nfcdev.NFC_DISCOVER_FLAGS_SELECT,
                    )
                )
            else:
                print(f"Unexpected message (type={header.message_type})")
        except KeyboardInterrupt:
            break
