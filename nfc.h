/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * I/O Control Device driver for NFC chips.
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

#ifndef _NFC_H_
#define _NFC_H_

#include <linux/types.h>

// The device's interface consists of:
// An ioctl to get the protocol version.
// A blocking I/O interface with messages with the chip.

/* ioctl definitions */

#define NFC_RD_GET_PROTOCOL_VERSION                 _IOR ('N', 0, uint64_t)

// This version
#define NFC_PROTOCOL_VERSION_1                      0x004E464300000001ULL

/* messages */

// A single client can open the device at a time.
//
// Each message between client and driver is composed of a header and a payload.
// Header is four bytes: message type and payload length (in bytes).
// Payload is up to 65535 bytes.

struct nfc_message_header {
    uint8_t message_type;
    uint16_t payload_length;
} __attribute__((packed));

// ---- Identify request ----
// Client => Driver
// Get the name of the chipset.
// Can be processed in any mode.
#define NFC_IDENTIFY_REQUEST_MESSAGE_TYPE 0

// ---- Identify response ----
// Driver => Client
// The name of the chipset.
#define NFC_IDENTIFY_RESPONSE_MESSAGE_TYPE 1
// Payload length is variable

// ---- Transition to idle mode ----
// Client => Driver : transition request
// Transition the device to idle mode. Can be sent when the device is in any
// mode.
#define NFC_IDLE_MODE_REQUEST_MESSAGE_TYPE 2

// ---- Transition to idle mode ----
// Driver => Client : acknowledge of idle mode transition
// Device was transitioned to idle mode.
//
// Message is always sent when the device transitions to idle mode.
// It is sent when it stops discovering (after set period/number of
// discovered tags).
// It is also sent to acknowledge a transition in response to idle mode request
// message, unless the device was already in idle mode.
#define NFC_IDLE_MODE_ACKNOWLEDGE_MESSAGE_TYPE 3

// ---- Discover message ----
// Client => Driver
// Define discover parameters.
// Transition the device to discovery mode. The message can also be sent if the
// device was already in discover mode, thus updating the parameters.
struct nfc_discover_mode_request_message_payload {
    uint64_t protocols;         // protocols to poll for (NFC_TAG_PROTOCOL_*)
    uint32_t polling_period;    // polling period in ms
    uint8_t device_count;       // number of devices to find before transitionning to idle, 0 means infinite
    uint8_t max_bitrate;        // maximum bit rate for communications (NFC_BITRATE_*)
    uint8_t flags;              // Discover flags
};
#define NFC_DISCOVER_MODE_REQUEST_MESSAGE_TYPE 4

// Select tag and exits discover mode.
// Device will reply with a NFC_SELECTED_TAG_MESSAGE_TYPE instead of
// NFC_DETECTED_TAG_MESSAGE_TYPE message.
#define NFC_DISCOVER_FLAGS_SELECT       1

// ---- Detected tag message ----
// Driver => Client
#define NFC_DETECTED_TAG_MESSAGE_TYPE   5
/// Payload length is variable

#define NFC_TAG_TYPE_ISO14443A          1
#define NFC_TAG_TYPE_ISO14443A_T2T      2
#define NFC_TAG_TYPE_MIFARE_CLASSIC     3
#define NFC_TAG_TYPE_ISO14443A_NFCDEP   4
// Every array of bytes (atqa, uid, etc.) is stored in protocol order.
struct nfc_tag_info_iso14443a {
    uint8_t atqa[2];
    uint8_t sak;
    uint8_t uid_len;
    uint8_t uid[10];   // up to 10.
} __attribute__((packed));

#define NFC_TAG_TYPE_ISO14443A_T4T          6
#define NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP   7
struct nfc_tag_info_iso14443a4 {
    uint8_t atqa[2];
    uint8_t sak;
    uint8_t uid_len;
    uint8_t uid[10];
    uint8_t ats_len;    // length of ATS, excluding length byte
    uint8_t ats[254];   // up to 254, excluding length byte (TL)
} __attribute__((packed));

#define NFC_TAG_TYPE_ISO14443A_T1T   8

#define NFC_TAG_TYPE_ISO14443B  16
struct nfc_tag_info_iso14443b {
    uint8_t pupi[4];
    uint8_t application_data[4];
    uint8_t protocol_info[3];
} __attribute__((packed));

#define NFC_TAG_TYPE_ST25TB      17
struct nfc_tag_info_st25tb {
    uint8_t uid[8];
} __attribute__((packed));

#define NFC_TAG_TYPE_NFCF           24
#define NFC_TAG_TYPE_NFCF_NFCDEP    25
struct nfc_tag_info_nfcf {
} __attribute__((packed));

#define NFC_TAG_TYPE_ISO15693           32
#define NFC_TAG_TYPE_ISO15693_ST25XV    33
struct nfc_tag_info_iso15693 {
} __attribute__((packed));

struct nfc_detected_tag_message_payload {
    uint8_t tag_type;
    union {
        struct nfc_tag_info_iso14443a iso14443a;
        struct nfc_tag_info_iso14443a4 iso14443a4;
        struct nfc_tag_info_iso14443b iso14443b;
        struct nfc_tag_info_st25tb st25tb;
        struct nfc_tag_info_nfcf nfcf;
    } tag_info;
} __attribute__((packed));

#define NFC_TAG_PROTOCOL_ISO14443A            1ULL << NFC_TAG_TYPE_ISO14443A            // Generic ISO-14443-A, including the following.
#define NFC_TAG_PROTOCOL_ISO14443A_T2T        1ULL << NFC_TAG_TYPE_ISO14443A_T2T        // ISO-14443-A subtype NFC Forum Type 2 Tag (Mifare Ultralight)
#define NFC_TAG_PROTOCOL_MIFARE_CLASSIC       1ULL << NFC_TAG_TYPE_MIFARE_CLASSIC       // ISO-14443-A subtype MIFARE CLASSIC with encryption
#define NFC_TAG_PROTOCOL_ISO14443A_NFCDEP     1ULL << NFC_TAG_TYPE_ISO14443A_NFCDEP     // ISO-14443-A subtype NFCDEP (NFCA Passive P2P)
#define NFC_TAG_PROTOCOL_ISO14443A4           1ULL << 5                                 // ISO-14443-A subtype supporting ISO-14443-4 protocol (ATS), including the following:
#define NFC_TAG_PROTOCOL_ISO14443A_T4T        1ULL << NFC_TAG_TYPE_ISO14443A_T4T        // ISO-14443-A-4 subtype NFC Forum Type 4 Tag (NFCA Passive ISO-DEP)
#define NFC_TAG_PROTOCOL_ISO14443A_T4T_NFCDEP 1ULL << NFC_TAG_TYPE_ISO14443A_T4T_NFCDEP // ISO-14443-A-4 subtype NFC Forum Type 4 Tag with NFCDEP

#define NFC_TAG_PROTOCOL_ISO14443A_T1T        1ULL << NFC_TAG_TYPE_ISO14443A_T1T        // ISO-14443-A subtype NFC Forum Type 1 Tag (Topaz Jewel)

#define NFC_TAG_PROTOCOL_ISO14443B            1ULL << NFC_TAG_TYPE_ISO14443B        // ISO-14443-B
#define NFC_TAG_PROTOCOL_ST25TB               1ULL << NFC_TAG_TYPE_ST25TB           // ISO-14443-B ST variant
#define NFC_TAG_PROTOCOL_ISO14443BI           1ULL << 18                            // ISO-14443-B'
#define NFC_TAG_PROTOCOL_ISO14443BICLASS      1ULL << 19                            // HID iClass 14443B mode
#define NFC_TAG_PROTOCOL_ISO14443B2CT         1ULL << 20                            // ISO-14443-2B ASK CTx

#define NFC_TAG_PROTOCOL_NFCF                 1ULL << NFC_TAG_TYPE_NFCF             // NFC-F, also known as FELICA and NFC Forum Type 3
#define NFC_TAG_PROTOCOL_NFCF_NFCDEP          1ULL << NFC_TAG_TYPE_NFCF_NFCDEP      // NFC-F subtype NFCDEP

#define NFC_TAG_PROTOCOL_ISO15693             1ULL << NFC_TAG_TYPE_ISO15693         // NFC-V
#define NFC_TAG_PROTOCOL_ISO15693_ST25XV      1ULL << NFC_TAG_TYPE_ISO15693_ST25XV  // NFC-V ST subprotocol

#define NFC_TAG_PROTOCOL_ISO18092             1ULL << 48

#define NFC_BITRATE_1_66            1
#define NFC_BITRATE_26_48           2
#define NFC_BITRATE_52_97           3
#define NFC_BITRATE_106             4
#define NFC_BITRATE_212             5
#define NFC_BITRATE_424             6
#define NFC_BITRATE_848             7
#define NFC_BITRATE_1695            8
#define NFC_BITRATE_3390            9
#define NFC_BITRATE_6780            10
#define NFC_BITRATE_13560           11

// ---- Select tag ----
// Client => Driver
// Will poll and select matching tag when found.
// If tag type implies ISO-14443-A-4, layer 4 is activated.
// Oscillator is turned off when device transitions to idle or discover modes.
#define NFC_SELECT_TAG_MESSAGE_TYPE 6
/// Payload length is variable

struct nfc_tag_id_iso14443a {
    uint8_t uid_len;
    uint8_t uid[10];   // up to 10.
} __attribute__((packed));

struct nfc_tag_id_iso14443b {
    uint8_t pupi[4];
} __attribute__((packed));

struct nfc_tag_id_st25tb {
    uint8_t uid[8];
} __attribute__((packed));

struct nfc_select_tag_message_payload {
    uint8_t tag_type;
    union {
        struct nfc_tag_id_iso14443a iso14443a;
        struct nfc_tag_id_iso14443b iso14443b;
        struct nfc_tag_id_st25tb st25tb;
    } tag_id;
} __attribute__((packed));

// Driver => Client
#define NFC_SELECTED_TAG_MESSAGE_TYPE 7
// Payload is identitical to nfc_detected_tag_message_payload

// ---- Transceive frame ----
// Low level interface to exchange data with picc.
//
// Client => Driver
// Transceive frame to selected tag
// Only available in selected mode (ignored otherwise)
// Followed by a response message from device in case of success or failure.
// If flag NFC_TRANSCEIVE_FLAGS_TX_ONLY is used, there also is an answer with
// rx_count equal to 0.
// If tx_count is 0, no frame is transmitted.
#define NFC_TRANSCEIVE_FRAME_REQUEST_MESSAGE_TYPE 8
/// Payload length is variable

struct nfc_transceive_frame_request_message_payload {
    uint16_t tx_count;          // in bits or in bytes
    uint8_t flags;
    uint8_t tx_data[512];       // could be less
} __attribute__((packed));

#define NFC_TRANSCEIVE_FLAGS_NOCRC      1
#define NFC_TRANSCEIVE_FLAGS_RAW        3       // No CRC and no Parity
#define NFC_TRANSCEIVE_FLAGS_BITS       4       // TX and RX partial bits, tx_count in bits
#define NFC_TRANSCEIVE_FLAGS_TX_ONLY    8       // Do not receive any answer (only transmit)
#define NFC_TRANSCEIVE_FLAGS_ERROR      128     // Transceive failed, chip is unselected and field is turned off.

// Driver => Client
#define NFC_TRANSCEIVE_FRAME_RESPONSE_MESSAGE_TYPE 9
/// Payload length is variable

struct nfc_message_transceive_frame_response_payload {
    uint16_t rx_count;      // in bits or in bytes
    uint8_t flags;          // Identical to request flags, including NFC_TRANSCEIVE_FLAGS_BITS
    uint8_t rx_data[512];   // could be less
} __attribute__((packed));

#endif
