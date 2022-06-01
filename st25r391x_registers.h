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

#ifndef ST25R391X_REGISTERS_H
#define ST25R391X_REGISTERS_H

// ST25R3916/7 datasheet, DS12484 Rev 4, pages 68-70/157
#define ST25R391X_IO_CONFIGURATION_1_REGISTER 0x00
#define ST25R391X_IO_CONFIGURATION_2_REGISTER 0x01
#define ST25R391X_OPERATION_CONTROL_REGISTER 0x02
#define ST25R391X_MODE_DEFINITION_REGISTER 0x03
#define ST25R391X_BIT_RATE_DEFINITION_REGISTER 0x04
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER 0x05
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER 0x06
#define ST25R391X_ISO14443B_AND_FELICA_SETTINGS_REGISTER 0x07
#define ST25R391X_NFCIP_1_PASSIVE_TARGET_DEFINITION_REGISTER 0x08
#define ST25R391X_STREAM_MODE_DEFINITION_REGISTER 0x09
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER 0x0A
#define ST25R391X_RECEIVER_CONFIGURATION_1_REGISTER 0x0B
#define ST25R391X_RECEIVER_CONFIGURATION_2_REGISTER 0x0C
#define ST25R391X_RECEIVER_CONFIGURATION_3_REGISTER 0x0D
#define ST25R391X_RECEIVER_CONFIGURATION_4_REGISTER 0x0E
#define ST25R391X_MASK_RECEIVE_TIMER_REGISTER 0x0F
#define ST25R391X_NO_RESPONSE_TIMER_1_REGISTER 0x10
#define ST25R391X_NO_RESPONSE_TIMER_2_REGISTER 0x11
#define ST25R391X_TIMER_AND_EMV_CONTROL_REGISTER 0x12
#define ST25R391X_GENERAL_PURPOSE_TIMER_1_REGISTER 0x13
#define ST25R391X_GENERAL_PURPOSE_TIMER_2_REGISTER 0x14
#define ST25R391X_PPON2_FIELD_WAITING_REGISTER 0x15
#define ST25R391X_MASK_MAIN_INTERRUPT_REGISTER 0x16
#define ST25R391X_MASK_TIMER_AND_NFC_INTERRUPT_REGISTER 0x17
#define ST25R391X_MASK_ERROR_AND_WAKEUP_INTERRUPT_REGISTER 0x18
#define ST25R391X_MASK_PASSIVE_TARGET_INTERRUPT_REGISTER 0x19
#define ST25R391X_MAIN_INTERRUPT_REGISTER 0x1A
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER 0x1B
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER 0x1C
#define ST25R391X_PASSIVE_TARGET_INTERRUPT_REGISTER 0x1D
#define ST25R391X_FIFO_STATUS_1_REGISTER 0x1E
#define ST25R391X_FIFO_STATUS_2_REGISTER 0x1F
#define ST25R391X_COLLISION_DISPLAY_REGISTER 0x20
#define ST25R391X_PASSIVE_TARGET_DISPLAY_REGISTER 0x21
#define ST25R391X_NUMBER_OF_TRANSMITTED_BYTES_1_REGISTER 0x22
#define ST25R391X_NUMBER_OF_TRANSMITTED_BYTES_2_REGISTER 0x23
#define ST25R391X_BIT_RATE_DETECTION_DISPLAY_REGISTER 0x24
#define ST25R391X_AD_CONVERTER_OUTPUT_REGISTER 0x25
#define ST25R391X_ANTENNA_TUNING_CONTROL_1_REGISTER 0x26
#define ST25R391X_ANTENNA_TUNING_CONTROL_2_REGISTER 0x27
#define ST25R391X_TX_DRIVER_REGISTER 0x28
#define ST25R391X_PASSIVE_TARGET_MODULATION_REGISTER 0x29
#define ST25R391X_EXTERNAL_FIELD_DETECTOR_ACTIVATION_THRESHOLD_REGISTER 0x2A
#define ST25R391X_EXTERNAL_FIELD_DETECTOR_DEACTIVATION_THRESHOLD_REGISTER 0x2B
#define ST25R391X_REGULATOR_VOLTAGE_CONTROL_REGISTER 0x2C
#define ST25R391X_RSSI_DISPLAY_REGISTER 0x2D
#define ST25R391X_GAIN_REDUCTION_STATE_REGISTER 0x2E
#define ST25R391X_CAPACITIVE_SENSOR_CONTROL_REGISTER 0x2F
#define ST25R391X_CAPACITIVE_SENSOR_DISPLAY_REGISTER 0x30
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER 0x31
#define ST25R391X_WAKEUP_TIMER_CONTROL_REGISTER 0x32
#define ST25R391X_AMPLITUDE_MEASUREMENT_CONFIGURATION_REGISTER 0x33
#define ST25R391X_AMPLITUDE_MEASUREMENT_REFERENCE_REGISTER 0x34
#define ST25R391X_AMPLITUDE_MEASUREMENT_AUTO_AVERAGING_DISPLAY_REGISTER 0x35
#define ST25R391X_AMPLITUDE_MEASUREMENT_DISPLAY_REGISTER 0x36
#define ST25R391X_PHASE_MEASUREMENT_CONFIGURATION_REGISTER 0x37
#define ST25R391X_PHASE_MEASUREMENT_REFERENCE_REGISTER 0x38
#define ST25R391X_PHASE_MEASUREMENT_AUTO_AVERAGING_DISPLAY_REGISTER 0x39
#define ST25R391X_PHASE_MEASUREMENT_DISPLAY_REGISTER 0x3A
#define ST25R391X_CAPACITANCE_MEASUREMENT_CONFIGURATION_REGISTER 0x3B
#define ST25R391X_CAPACITANCE_MEASUREMENT_REFERENCE_REGISTER 0x3C
#define ST25R391X_CAPACITANCE_MEASUREMENT_AUTO_AVERAGING_DISPLAY_REGISTER 0x3D
#define ST25R391X_CAPACITANCE_MEASUREMENT_DISPLAY_REGISTER 0x3E
#define ST25R391X_IC_IDENTITY_REGISTER 0x3F

// ST25R3916/7 datasheet, DS12484 Rev 4, page 70/157
#define ST25R391X_EMD_SUPPRESSION_CONFIGURATION_B_REGISTER 0x05
#define ST25R391X_SUBCARRIER_START_TIMER_B_REGISTER 0x06
#define ST25R391X_P2P_RECEIVER_CONFIGURATION_1_B_REGISTER 0x0B
#define ST25R391X_CORRELATOR_CONFIGURATION_1_B_REGISTER 0x0C
#define ST25R391X_CORRELATOR_CONFIGURATION_2_B_REGISTER 0x0D
#define ST25R391X_SQUELCH_TIMER_B_REGISTER 0x0F
#define ST25R391X_NFC_FILED_ON_GUARD_TIMER_B_REGISTER 0x15
#define ST25R391X_AUXILIARY_MODULATION_SETTING_B_REGISTER 0x28
#define ST25R391X_TX_DRIVER_TIMING_B_REGISTER 0x29
#define ST25R391X_RESISTIVE_AM_MODULATION_B_REGISTER 0x2A
#define ST25R391X_TX_DRIVER_TIMING_DISPLAY_B_REGISTER 0x2B
#define ST25R391X_REGULATOR_DISPLAY_B_REGISTER 0x2C
#define ST25R391X_OVERSHOOT_PROTECTION_CONFIGURATION_1_B_REGISTER 0x30
#define ST25R391X_OVERSHOOT_PROTECTION_CONFIGURATION_2_B_REGISTER 0x31
#define ST25R391X_UNDERSHOOT_PROTECTION_CONFIGURATION_1_B_REGISTER 0x32
#define ST25R391X_UNDERSHOOT_PROTECTION_CONFIGURATION_2_B_REGISTER 0x33

// ST25R3916/7 datasheet, DS12484 Rev 4, page 73/157
#define ST25R391X_OPERATION_CONTROL_REGISTER_en 0b10000000
#define ST25R391X_OPERATION_CONTROL_REGISTER_rx_en 0b01000000
#define ST25R391X_OPERATION_CONTROL_REGISTER_rx_chn 0b00100000
#define ST25R391X_OPERATION_CONTROL_REGISTER_rx_man 0b00010000
#define ST25R391X_OPERATION_CONTROL_REGISTER_tx_en 0b00001000
#define ST25R391X_OPERATION_CONTROL_REGISTER_wu 0b00000100
#define ST25R391X_OPERATION_CONTROL_REGISTER_en_fd_c1 0b00000010
#define ST25R391X_OPERATION_CONTROL_REGISTER_en_fd_c0 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, pages 74-75/157
// Initiator operation modes
#define ST25R391X_MODE_DEFINITION_REGISTER_ncfip1_i 0b00000000
#define ST25R391X_MODE_DEFINITION_REGISTER_iso14443a_i 0b00001000
#define ST25R391X_MODE_DEFINITION_REGISTER_iso14443b_i 0b00010000
#define ST25R391X_MODE_DEFINITION_REGISTER_felica_i 0b00011000
#define ST25R391X_MODE_DEFINITION_REGISTER_nfc_ft1_i 0b00100000
#define ST25R391X_MODE_DEFINITION_REGISTER_subcst_i 0b01110000
#define ST25R391X_MODE_DEFINITION_REGISTER_bpskst_i 0b01111000
// Target operation modes
#define ST25R391X_MODE_DEFINITION_REGISTER_iso14443a_p 0b10001000
#define ST25R391X_MODE_DEFINITION_REGISTER_felica_p 0b10100000
#define ST25R391X_MODE_DEFINITION_REGISTER_ncfip1_p 0b10111000
#define ST25R391X_MODE_DEFINITION_REGISTER_bitrate_d_p 0b11000000
#define ST25R391X_MODE_DEFINITION_REGISTER_br_felica 0b00100000
#define ST25R391X_MODE_DEFINITION_REGISTER_br_iso14443a 0b00001000

#define ST25R391X_MODE_DEFINITION_REGISTER_tr_am 0b00000100
#define ST25R391X_MODE_DEFINITION_REGISTER_nfc_off 0b00000000
#define ST25R391X_MODE_DEFINITION_REGISTER_nfc_auto 0b00000001
#define ST25R391X_MODE_DEFINITION_REGISTER_nfc_always 0b00000010

// ST25R3916/7 datasheet, DS12484 Rev 4, page 76/157
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_no_tx_par \
	0b10000000
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_no_rx_par \
	0b01000000
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_nfc_f0 0b00100000
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_p_len3 0b00010000
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_p_len2 0b00001000
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_p_len1 0b00000100
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_p_len0 0b00000010
#define ST25R391X_ISO14443A_AND_NFC_106KBS_SETTINGS_REGISTER_antcl 0b00000001

#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_0 0b00000000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_1 0b00100000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_2 0b01000000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_3 0b01100000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_4 0b10000000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_5 0b10100000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_egt_etu_6 0b11000000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_sof_0_11etu 0b00010000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_sof_1_3etu 0b00001000
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_eof_11etu 0b00000100
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_half 0b00000010
#define ST25R391X_ISO14443B_SETTINGS_1_REGISTER_rx_st_om 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 81/157
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_no_crc_rx 0b10000000
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_nfc_id1 0b00100000
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_nfc_id0 0b00010000
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_mfaz_cl90 0b00001000
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_dis_corr 0b00000100
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_nfc_n1 0b00000010
#define ST25R391X_AUXILIARY_DEFINITION_REGISTER_nfc_n0 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 98/157
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_osc 0b10000000
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_wl 0b01000000
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxs 0b00100000
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_rxe 0b00010000
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_txe 0b00001000
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_col 0b00000100
#define ST25R391X_MAIN_INTERRUPT_REGISTER_l_rx_rest 0b00000010

// ST25R3916/7 datasheet, DS12484 Rev 4, page 99/157
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_dct 0b10000000
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_nre 0b01000000
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_gpe 0b00100000
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_eon 0b00010000
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_eof 0b00001000
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cac 0b00000100
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_cat 0b00000010
#define ST25R391X_TIMER_AND_NFC_INTERRUPT_REGISTER_l_nfct 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 100/157
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_crc 0b10000000
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_par 0b01000000
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_err2 0b00100000
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_err1 0b00010000
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_wt 0b00001000
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_wam 0b00000100
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_wph 0b00000010
#define ST25R391X_ERROR_AND_WAKEUP_INTERRUPT_REGISTER_l_wcap 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 103/157
#define ST25R391X_COLLISION_DISPLAY_REGISTER_c_pb 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 109/157
#define ST25R391X_TX_DRIVER_REGISTER_am_10pct 0b01010000
#define ST25R391X_TX_DRIVER_REGISTER_am_12pct 0b01110000

// ST25R3916/7 datasheet, DS12484 Rev 4, page 125/157
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_a_cha 0b10000000
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_efd_o 0b01000000
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_tx_on 0b00100000
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_osc_ok 0b00010000
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_rx_on 0b00001000
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_rx_act 0b00000100
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_en_peer 0b00000010
#define ST25R391X_AUXILIARY_DISPLAY_REGISTER_en_ac 0b00000001

// ST25R3916/7 datasheet, DS12484 Rev 4, page 23/157
#define ST25R391X_TEST_SPACE_OVERHEAT_PROTECTION_REGISTER 0x04
#define ST25R391X_TEST_SPACE_OVERHEAT_PROTECTION_VALUE 0x10

#endif
