/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 17, 2018
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <cfg/cs_Config.h>
#include <drivers/cs_ADC.h>
#include <protocol/cs_ServiceDataPackets.h>
#include <cstdint>


union __attribute__((packed)) uart_msg_status_user_flags_t {
	struct __attribute__((packed)) {
		bool encryptionRequired : 1;
		bool hasBeenSetUp : 1;
		bool hasInternet : 1;
		bool hasError : 1;
	} flags;
	uint8_t asInt;
};

union __attribute__((packed)) uart_msg_status_reply_flags_t {
	struct __attribute__((packed)) {
		bool encryptionRequired : 1;
		bool hasBeenSetUp : 1;
		bool hubMode : 1;
		bool hasError : 1;
	} flags;
	uint8_t asInt;
};

enum UartHubDataType {
	UART_HUB_DATA_TYPE_NONE              = 0,
	UART_HUB_DATA_TYPE_CROWNSTONE_HUB    = 1,
};

struct __attribute__((__packed__)) uart_msg_status_user_t {
	uint8_t type; // See UartHubDataType.
	uart_msg_status_user_flags_t flags;
	uint8_t data[SERVICE_DATA_HUB_DATA_SIZE]; // Depends on type.
};

struct __attribute__((__packed__)) uart_msg_status_reply_t {
	uart_msg_status_reply_flags_t flags;
};

struct __attribute__((__packed__)) uart_msg_hello_t {
	uint8_t sphereId;
	uart_msg_status_reply_t status;
};

struct __attribute__((__packed__)) uart_msg_heartbeat_t {
	uint16_t timeoutSeconds;
};

struct __attribute__((__packed__)) uart_msg_session_nonce_t {
	uint8_t timeoutMinutes;
	uint8_t sessionNonce[SESSION_NONCE_LENGTH];
};

struct __attribute__((__packed__)) uart_msg_session_nonce_reply_t {
	uint8_t sessionNonce[SESSION_NONCE_LENGTH];
};





struct __attribute__((__packed__)) uart_msg_mesh_result_packet_header_t {
	stone_id_t stoneId;
	result_packet_header_t resultHeader;
};

struct __attribute__((__packed__)) uart_msg_power_t {
	uint32_t timestamp;
	int32_t  currentRmsMA;
	int32_t  currentRmsMedianMA;
	int32_t  filteredCurrentRmsMA;
	int32_t  filteredCurrentRmsMedianMA;
	int32_t  avgZeroVoltage;
	int32_t  avgZeroCurrent;
	int32_t  powerMilliWattApparent;
	int32_t  powerMilliWattReal;
	int32_t  avgPowerMilliWattReal;
};

struct __attribute__((__packed__)) uart_msg_current_t {
	uint32_t timestamp;
	int16_t  samples[CS_ADC_NUM_SAMPLES_PER_CHANNEL];
};

struct __attribute__((__packed__)) uart_msg_voltage_t {
	uint32_t timestamp;
	int16_t  samples[CS_ADC_NUM_SAMPLES_PER_CHANNEL];
};

struct __attribute__((__packed__)) uart_msg_adc_channel_config_t {
	adc_channel_id_t channel;
	adc_channel_config_t config;
};
