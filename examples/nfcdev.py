# SPDX-License-Identifier: GPL-2.0-only

import os
import fcntl
import array
import struct
import ctypes
from io import FileIO
from ioctl_opt import IOR

NFC_RD_GET_PROTOCOL_VERSION = IOR(ord("N"), 0, ctypes.c_uint64)
NFC_PROTOCOL_VERSION_1 = 0x004E464300000001

NFC_IDENTIFY_REQUEST_MESSAGE_TYPE = 0
NFC_IDENTIFY_RESPONSE_MESSAGE_TYPE = 1
NFC_IDLE_MODE_REQUEST_MESSAGE_TYPE = 2
NFC_IDLE_MODE_ACKNOWLEDGE_MESSAGE_TYPE = 3
NFC_DISCOVER_MODE_REQUEST_MESSAGE_TYPE = 4
NFC_DETECTED_TAG_MESSAGE_TYPE = 5
NFC_SELECT_TAG_MESSAGE_TYPE = 6
NFC_SELECTED_TAG_MESSAGE_TYPE = 7
NFC_TRANSCEIVE_FRAME_REQUEST_MESSAGE_TYPE = 8
NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE = 9

NFC_TAG_TYPE_ISO14443A = 1
NFC_TAG_TYPE_ISO14443A_T2T = 2
NFC_TAG_TYPE_MIFARE_CLASSIC = 3
NFC_TAG_TYPE_ISO14443A_NFCDEP = 4
NFC_TAG_TYPE_ISO14443A_T4T = 6
NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP = 7
NFC_TAG_TYPE_ISO14443B = 16
NFC_TAG_TYPE_ST25TB = 17
NFC_TAG_TYPE_NFCF = 24

NFC_TAG_PROTOCOL_ISO14443A = 1 << NFC_TAG_TYPE_ISO14443A
NFC_TAG_PROTOCOL_ISO14443A_T2T = 1 << NFC_TAG_TYPE_ISO14443A_T2T
NFC_TAG_PROTOCOL_MIFARE_CLASSIC = 1 << NFC_TAG_TYPE_MIFARE_CLASSIC
NFC_TAG_PROTOCOL_ISO14443A_NFCDEP = 1 << NFC_TAG_TYPE_ISO14443A_NFCDEP
NFC_TAG_PROTOCOL_ISO14443A4 = 1 << 5
NFC_TAG_PROTOCOL_ISO14443A_T4T = 1 << NFC_TAG_TYPE_ISO14443A_T4T
NFC_TAG_PROTOCOL_ISO14443A_T4T_NFCDEP = 1 << NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP

NFC_TAG_PROTOCOL_ISO14443B = 1 << NFC_TAG_TYPE_ISO14443B
NFC_TAG_PROTOCOL_ST25TB = 1 << NFC_TAG_TYPE_ST25TB

NFC_TAG_PROTOCOL_NFCF = 1 << NFC_TAG_TYPE_NFCF

NFC_TAG_PROTOCOL_ALL = (
    NFC_TAG_PROTOCOL_ISO14443A
    | NFC_TAG_PROTOCOL_ISO14443A_T2T
    | NFC_TAG_PROTOCOL_MIFARE_CLASSIC
    | NFC_TAG_PROTOCOL_ISO14443A_NFCDEP
    | NFC_TAG_PROTOCOL_ISO14443A4
    | NFC_TAG_PROTOCOL_ISO14443A_T4T
    | NFC_TAG_PROTOCOL_ISO14443A_T4T_NFCDEP
    | NFC_TAG_PROTOCOL_ISO14443B
    | NFC_TAG_PROTOCOL_ST25TB
    | NFC_TAG_PROTOCOL_NFCF
)

NFC_DISCOVER_FLAGS_SELECT = 1

ST25TB_COMMAND_READ_BLOCK = 0x08
ST25TB_COMMAND_WRITE_BLOCK = 0x09


class NFCMessageHeader(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("message_type", ctypes.c_uint8), ("payload_length", ctypes.c_uint16)]

    def __init__(self, message_type=0, payload_length=0):
        self.message_type = message_type
        self.payload_length = payload_length


class NFCIdentityRequestMessage(NFCMessageHeader):
    def __init__(self):
        super().__init__(NFC_IDENTIFY_REQUEST_MESSAGE_TYPE, 0)


class NFCDiscoverModeRequestMessage(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("message_type", ctypes.c_uint8),
        ("payload_length", ctypes.c_uint16),
        ("protocols", ctypes.c_uint64),
        ("polling_period", ctypes.c_uint32),
        ("device_count", ctypes.c_uint8),
        ("max_bitrate", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
    ]

    def __init__(self, protocols, polling_period, device_count, max_bitrate, flags):
        self.message_type = NFC_DISCOVER_MODE_REQUEST_MESSAGE_TYPE
        self.payload_length = 15
        self.protocols = protocols
        self.polling_period = polling_period
        self.device_count = device_count
        self.max_bitrate = max_bitrate
        self.flags = flags


class NFCIdleModeRequestMessage(NFCMessageHeader):
    def __init__(self):
        super().__init__(NFC_IDLE_MODE_REQUEST_MESSAGE_TYPE, 0)


class NFCTagInfoISO14443A:
    def __init__(self, packed):
        self.atqa = struct.unpack("BB", packed[0:2])
        self.sak = packed[2]
        uid_len = packed[3]
        self.uid = packed[4 : 4 + uid_len]

    def __str__(self):
        return f"<atqa={self.atqa}, sak={self.sak}, uid={self.uid}>"


class NFCTagInfoISO14443A4(NFCTagInfoISO14443A):
    def __init__(self, packed):
        super().__init__(packed)
        ats_len = packed[14]
        self.ats = packed[15 : 15 + ats_len]

    def __str__(self):
        return f"<atqa={self.atqa}, sak={self.sak}, uid={self.uid}, ats={self.ats}>"


class NFCTagInfoISO14443B:
    def __init__(self, packed):
        self.pupi = packed[0:4]
        self.application_data = packed[4:8]
        self.protocol_info = packed[8:11]

    def __str__(self):
        return f"<pupi={self.pupi}, application_data={self.application_data}, protocol_info={self.protocol_info}>"


class NFCTagInfoST25TB:
    def __init__(self, packed):
        self.uid = packed[0:8]

    def __str__(self):
        return f"<uid={self.uid}>"


class NFCTagInfo:
    def __init__(self, packed):
        self.tag_type = packed[0]
        if (
            packed[0] == NFC_TAG_TYPE_ISO14443A
            or packed[0] == NFC_TAG_TYPE_ISO14443A_T2T
            or packed[0] == NFC_TAG_TYPE_MIFARE_CLASSIC
            or packed[0] == NFC_TAG_TYPE_ISO14443A_NFCDEP
        ):
            self.tag_info = NFCTagInfoISO14443A(packed[1:])
        elif (
            packed[0] == NFC_TAG_TYPE_ISO14443A_T4T
            or packed[0] == NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP
        ):
            self.tag_info = NFCTagInfoISO14443A4(packed[1:])
        elif packed[0] == NFC_TAG_TYPE_ISO14443B:
            self.tag_info = NFCTagInfoISO14443B(packed[1:])
        elif packed[0] == NFC_TAG_TYPE_ST25TB:
            self.tag_info = NFCTagInfoST25TB(packed[1:])

    def __str__(self):
        return f"TagInfo<type={self.tag_type}, info={self.tag_info}>"


NFC_TRANSCEIVE_FLAGS_NOCRC = 1
NFC_TRANSCEIVE_FLAGS_RAW = 3  # No CRC and no Parity
NFC_TRANSCEIVE_FLAGS_BITS = 4  # TX and RX partial bits, tx_count in bits
NFC_TRANSCEIVE_FLAGS_ERROR = (
    128  # Transceive failed, chip is unselected and field is turned off.
)


class NFCTransceiveFrameRequestMessage:
    def __init__(self, tx_count, flags, data):
        self.tx_count = tx_count
        self.flags = flags
        self.data = data

    def __bytes__(self):
        payload = struct.pack("=HB", self.tx_count, self.flags) + self.data
        header = bytes(
            NFCMessageHeader(NFC_TRANSCEIVE_FRAME_REQUEST_MESSAGE_TYPE, len(payload))
        )
        return header + payload


class NFCTransceiveFrameResponseMessage:
    def __init__(self, packed):
        self.rx_count, self.flags = struct.unpack("=HB", packed[0:3])
        self.data = packed[3:]


class NFCDev:
    """
    Class to interact with /dev/nfc0 device.
    """

    def __init__(self, path="/dev/nfc0"):
        self.path = path

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.close()

    def open(self):
        self.fd = os.open("/dev/nfc0", os.O_RDWR)
        self.file = FileIO(self.fd, "rb+")

    def close(self):
        self.file.close()

    def check_version(self):
        b = array.array("Q", [0])
        r = fcntl.ioctl(self.fd, NFC_RD_GET_PROTOCOL_VERSION, b)
        (version,) = struct.unpack("Q", b)
        return version == NFC_PROTOCOL_VERSION_1

    def get_identify_chip_model(self):
        self.write_message(NFCIdentityRequestMessage())
        header, payload = self.read_message()
        if header.message_type != NFC_IDENTIFY_RESPONSE_MESSAGE_TYPE:
            raise ("Unexpected message")
        return payload

    def read_message(self):
        header = NFCMessageHeader()
        self.file.readinto(header)
        if header.payload_length > 0:
            payload = self.file.read(header.payload_length)
        else:
            payload = None
        if header.message_type in (
            NFC_DETECTED_TAG_MESSAGE_TYPE,
            NFC_SELECTED_TAG_MESSAGE_TYPE,
        ):
            payload = NFCTagInfo(payload)
        elif header.message_type == NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE:
            payload = NFCTransceiveFrameResponseMessage(payload)
        return header, payload

    def write_message(self, message):
        self.file.write(bytes(message))
