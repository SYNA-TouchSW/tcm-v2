// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Synaptics TouchComm C library
 *
 * Copyright (C) 2017-2025 Synaptics Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

/*
 * This file implements the TouchComm version 2 command-response protocol
 */

#include "synaptics_touchcom_core_dev.h"

#ifdef TOUCHCOMM_VERSION_2

#define HOST_PRIMARY (0)

#define EXTRA_PACKET_BYTES (MESSAGE_HEADER_SIZE + TCM_MSG_CRC_LENGTH)
#define CHECK_PACKET_CRC

#define COMMAND_V2_RETRY_TIMES (3)
#define RESPONSE_V2_RETRY_TIMES (10)

#define PACKET_CRC_FAILURE (0xFF)
#define PACKET_CORRUPTED (0xFE)
#define PACKET_MISMATCHED_CRC_SETUP (0xFD)
#define PACKET_INVALID_PREDICT (0xFC)
#define PACKET_RETRY_REQUEST (0xF8)

#define WR_RD_RETRY_TIMES (3)
#define WR_RD_RETRY_DELAY (5)

#define RESP_RETRY_DELAY (100)

/* Header of TouchComm v2 Message Packet */
struct tcm_v2_message_header {
	union {
		struct {
			unsigned char code;
			unsigned char length[2];
			unsigned char byte3;
		};
		unsigned char data[MESSAGE_HEADER_SIZE];
	};
};


/* Internal helpers */
static int syna_tcm_v2_process_command(struct tcm_dev *, unsigned char, unsigned char *, unsigned int, unsigned int, bool);
static int syna_tcm_v2_process_command_write_then_read(struct tcm_dev*, unsigned char, unsigned char*, unsigned int, unsigned int, bool);


/*
 *  Helper to determine whether the fw mode switching could be triggered
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *    [ in] command: command to process
 *
 * return
 *    void.
 */
static bool syna_tcm_v2_is_mode_switching(struct tcm_dev *tcm_dev, unsigned char command)
{
	switch (command) {
	case CMD_RUN_BOOTLOADER_FIRMWARE:
	case CMD_RUN_APPLICATION_FIRMWARE:
	case CMD_ENTER_PRODUCTION_TEST_MODE:
#ifdef TOUCHCOMM_TDDI
	case CMD_ROMBOOT_RUN_APP_FIRMWARE:
	case CMD_REBOOT_TO_ROM_BOOTLOADER:
#endif
#ifdef TOUCHCOMM_SMART_BRIDGE
	case CMD_REBOOT_TO_DISPLAY_ROM_BOOTLOADER:
#endif
	case CMD_RESET:
		return true;
	default:
		return false;
	}

	return false;
}

/*
 *  Terminate the command processing
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *
 * return
 *    void.
 */
static void syna_tcm_v2_terminate(struct tcm_dev *tcm_dev)
{
	struct tcm_message_data_blob *tcm_msg = NULL;
	syna_pal_completion_t *cmd_completion = NULL;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return;
	}

	tcm_msg = &tcm_dev->msg_data;
	cmd_completion = &tcm_msg->cmd_completion;

	if (ATOMIC_GET(tcm_msg->command_status) != CMD_STATE_BUSY)
		return;

	LOGI("Terminate the processing of command %02X\n\n", tcm_msg->command);

	ATOMIC_SET(tcm_msg->command_status, CMD_STATE_TERMINATED);
	syna_pal_completion_complete(cmd_completion);
}

/*
 *  Set up the capability of message reading and writing.
 *  The given size to read/write must be equal or less than the max read/write size defined in the identify report
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *    [ in] wr_size: the max. size for each write operation requested by caller
 *    [ in] rd_size: the max. size for each read operation requested by caller
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_set_up_max_rw_size(struct tcm_dev *tcm_dev,
	unsigned int wr_size, unsigned int rd_size)
{
	int retval;
	struct tcm_identification_info *id_info;
	unsigned int max_write_size = 0, current_write_size = 0;
	unsigned int max_read_size = 0, current_read_size = 0;
	unsigned char data[2] = { 0 };

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	id_info = &tcm_dev->id_info;
	if (id_info->version < 2) {
		LOGE("Invalid identify report stored\n");
		return -ERR_INVAL;
	}

	/* set up the write size */
	max_write_size = syna_pal_le2_to_uint(id_info->max_write_size);
	current_write_size = syna_pal_le2_to_uint(id_info->current_write_size);
	if (current_write_size == 0) {
		wr_size = (tcm_dev->platform_wr_size == 0) ?
			MIN(max_write_size, wr_size) :
			MIN(max_write_size, MIN(wr_size, tcm_dev->platform_wr_size));
	} else {
		wr_size = (tcm_dev->platform_wr_size == 0) ?
			MIN(current_write_size, wr_size) :
			MIN(current_write_size, MIN(wr_size, tcm_dev->platform_wr_size));
		if (wr_size < current_write_size) {
			data[0] = (unsigned char)wr_size;
			data[1] = (unsigned char)(wr_size >> 8);
			if (tcm_dev->msg_data.write_then_read_support)
				retval = syna_tcm_v2_process_command_write_then_read(tcm_dev, CMD_SET_MAX_WRITE_LENGTH, data, sizeof(data), sizeof(data), false);
			else
				retval = syna_tcm_v2_process_command(tcm_dev, CMD_SET_MAX_WRITE_LENGTH, data, sizeof(data), sizeof(data), false);

			if (retval < 0) {
				LOGE("Fail to set current fw max wr length to %d\n", wr_size);
				return retval;
			}
			LOGD("Update the current fw max wr length to %d\n", tcm_dev->max_wr_size);
		}
	}

	if (tcm_dev->max_wr_size != wr_size) {
		tcm_dev->max_wr_size = wr_size;
		LOGD("Set the sw max wr length to %d bytes\n", tcm_dev->max_wr_size);
	}

	/* set up the read size */
	max_read_size = syna_pal_le2_to_uint(id_info->max_read_size);
	current_read_size = syna_pal_le2_to_uint(id_info->current_read_size);

	rd_size = (tcm_dev->platform_rd_size == 0) ?
		MIN(current_read_size, rd_size) :
		MIN(current_read_size, MIN(rd_size, tcm_dev->platform_rd_size));
	if (rd_size < current_read_size) {
		data[0] = (unsigned char)rd_size;
		data[1] = (unsigned char)(rd_size >> 8);
		if (tcm_dev->msg_data.write_then_read_support)
			retval = syna_tcm_v2_process_command_write_then_read(tcm_dev, CMD_SET_MAX_READ_LENGTH, data, sizeof(data), sizeof(data), false);
		else
			retval = syna_tcm_v2_process_command(tcm_dev, CMD_SET_MAX_READ_LENGTH, data, sizeof(data), sizeof(data), false);

		if (retval < 0) {
			LOGE("Fail to set current fw max rd length to %d\n", rd_size);
			return retval;
		}
		LOGD("Update the current fw max rd length to %d\n", tcm_dev->max_wr_size);
	}
	if (tcm_dev->max_rd_size != rd_size) {
		tcm_dev->max_rd_size = rd_size;
		LOGD("Set the sw max rd length to %d bytes\n", tcm_dev->max_rd_size);
	}

	if (id_info->version >= 3) {
		LOGD("Fw capability: write length:%d (%d / %d)\n", tcm_dev->max_wr_size, wr_size, max_write_size);
		LOGD("Fw capability: read length:%d (%d / %d)\n", tcm_dev->max_rd_size, rd_size, max_read_size);
	} else {
		LOGD("Fw capability: write length:%d (%d)\n", tcm_dev->max_wr_size, max_write_size);
		LOGD("Fw capability: read length:%d (%d / %d)\n", tcm_dev->max_rd_size, rd_size, max_read_size);
	}


	return 0;
}
/*
 *  Check and sync values, the size of message reading and writing, between
 *  the values from the identify report and the recorded values
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *    [ in] resync:  flag to resync the read/write size
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_check_max_rw_size(struct tcm_dev *tcm_dev, bool resync)
{
	unsigned int write_size, read_size;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	if (resync) {
		if (tcm_dev->max_rd_size != tcm_dev->platform_rd_size)
			tcm_dev->max_rd_size = tcm_dev->platform_rd_size;
		if (tcm_dev->max_wr_size != tcm_dev->platform_wr_size)
			tcm_dev->max_wr_size = tcm_dev->platform_wr_size;
	}

	write_size = tcm_dev->max_wr_size;
	read_size = tcm_dev->max_rd_size;

	if (tcm_dev->hw->alignment_enabled) {
		write_size = syna_pal_int_alignment(write_size, tcm_dev->hw->alignment_base, false);
		read_size = syna_pal_int_alignment(read_size, tcm_dev->hw->alignment_base, false);
	}

	return syna_tcm_v2_set_up_max_rw_size(tcm_dev, write_size, read_size);
}

/*
 *  Parse the identification info packet and record the essential info
 *
 * param
 *    [ in] tcm_dev:  pointer to TouchComm device
 *    [ in] data:     buffer containing the identification info packet
 *    [ in] size:     size of data buffer
 *    [ in] data_len: length of actual data
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_parse_idinfo(struct tcm_dev *tcm_dev,
	unsigned char *data, unsigned int size, unsigned int data_len)
{
	int retval;
	unsigned int build_id = 0;
	struct tcm_identification_info *id_info;
	struct tcm_message_data_blob *tcm_msg = NULL;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	if ((!data) || (data_len == 0)) {
		LOGE("Invalid given data buffer\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;
	id_info = &tcm_dev->id_info;

	retval = syna_pal_mem_cpy((unsigned char *)id_info,
			sizeof(struct tcm_identification_info),
			data,
			size,
			MIN(sizeof(*id_info), data_len));
	if (retval < 0) {
		LOGE("Fail to copy identification info\n");
		return retval;
	}

	build_id = syna_pal_le4_to_uint(id_info->build_id);

	if (build_id == 0)
		return 0;

	if (tcm_dev->packrat_number != build_id)
		tcm_dev->packrat_number = build_id;

	LOGD("Fw mode:0x%02X, build id:%d\n", id_info->mode, build_id);

	tcm_dev->dev_mode = id_info->mode;
	/* update the read/write size */
	syna_tcm_v2_check_max_rw_size(tcm_dev, (tcm_msg->status_report_code == REPORT_IDENTIFY));

	tcm_msg->has_crc = (GET_BIT(id_info->v2_ext_features, 1) == 0);
	LOGD("Fw feature: support of CRC and Seq-bit:%s\n", (tcm_msg->has_crc) ? "yes" : "no");

	tcm_msg->write_then_read_support = false;
	if (tcm_dev->hw->ops_write_then_read_data) {
		tcm_msg->write_then_read_support = (GET_BIT(id_info->v2_ext_features, 0) == 1);
		tcm_msg->write_then_read_turnaround_bytes = id_info->v2_ext_turnaround_bytes;

		if (tcm_msg->write_then_read_support)
			LOGD("Fw feature: write-then-read support:yes, (turnaround bytes:%d)\n",
				tcm_msg->write_then_read_turnaround_bytes);
	}

	return 0;
}

/*
 *  Process the TouchComm report read in by the read_message(),
 *  and copy the data from internal buffer.in to internal buffer.report.
 *
 *  If dispatch function is registered, call the corresponding callback functions then.
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *
 * return
 *    void.
 */
static void syna_tcm_v2_dispatch_report(struct tcm_dev *tcm_dev)
{
	int retval;
	struct tcm_message_data_blob *tcm_msg = NULL;
	syna_pal_completion_t *cmd_completion = NULL;
	unsigned char report_code;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return;
	}

	tcm_msg = &tcm_dev->msg_data;
	cmd_completion = &tcm_msg->cmd_completion;

	report_code = tcm_msg->status_report_code;

	if (tcm_msg->payload_length == 0) {
		tcm_dev->report_buf.data_length = 0;
		goto exit;
	}

	/* store the received report into the internal buffer.report */
	syna_tcm_buf_lock(&tcm_dev->report_buf);

	retval = syna_tcm_buf_alloc(&tcm_dev->report_buf, tcm_msg->payload_length);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.report\n");
		syna_tcm_buf_unlock(&tcm_dev->report_buf);
		goto exit;
	}

	syna_tcm_buf_lock(&tcm_msg->in);

	retval = syna_pal_mem_cpy(tcm_dev->report_buf.buf,
			tcm_dev->report_buf.buf_size,
			&tcm_msg->in.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->in.buf_size - MESSAGE_HEADER_SIZE,
			tcm_msg->payload_length);
	if (retval < 0) {
		LOGE("Fail to copy payload to buf_report\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_tcm_buf_unlock(&tcm_dev->report_buf);
		goto exit;
	}

	tcm_dev->report_buf.data_length = tcm_msg->payload_length;

	syna_tcm_buf_unlock(&tcm_msg->in);
	syna_tcm_buf_unlock(&tcm_dev->report_buf);

	if (report_code == REPORT_IDENTIFY) {
		/* parse the identify report */
		retval = syna_tcm_v2_parse_idinfo(tcm_dev,
				&tcm_msg->in.buf[MESSAGE_HEADER_SIZE],
				tcm_msg->in.buf_size - MESSAGE_HEADER_SIZE,
				tcm_msg->payload_length);
		if (retval < 0) {
			LOGE("Fail to parse identification data\n");
			return;
		}
		/* recover the status and report code because it may be overridden by setting the rw size */
		if (tcm_msg->status_report_code != REPORT_IDENTIFY)
			tcm_msg->status_report_code = REPORT_IDENTIFY;

		/* in case, the identify info packet is resulted from the command */
		if (ATOMIC_GET(tcm_msg->command_status) == CMD_STATE_BUSY) {
			switch (tcm_msg->command) {
			case CMD_RUN_BOOTLOADER_FIRMWARE:
			case CMD_RUN_APPLICATION_FIRMWARE:
			case CMD_ENTER_PRODUCTION_TEST_MODE:
#ifdef TOUCHCOMM_TDDI
			case CMD_ROMBOOT_RUN_APP_FIRMWARE:
			case CMD_REBOOT_TO_ROM_BOOTLOADER:
#endif
#ifdef TOUCHCOMM_SMART_BRIDGE
			case CMD_REBOOT_TO_DISPLAY_ROM_BOOTLOADER:
#endif
			case CMD_GET_REPORT:
				ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);
				syna_pal_completion_complete(cmd_completion);
				goto exit;
			case CMD_RESET:
				LOGD("Reset by command 0x%02X\n", tcm_msg->command);
				ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);
				syna_pal_completion_complete(cmd_completion);
				goto exit;
			default:
				if (tcm_dev->testing_purpose) {
					ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);
					syna_pal_completion_complete(cmd_completion);
				} else {
					LOGI("Unexpected 0x%02X report received\n", REPORT_IDENTIFY);
					ATOMIC_SET(tcm_msg->command_status, CMD_STATE_ERROR);
					syna_pal_completion_complete(cmd_completion);
				}
				break;
			}
		}
	}

	/* dispatch the report to the proper callbacks if registered */
	if (tcm_dev->cb_report_dispatcher[report_code].cb) {
		syna_tcm_buf_lock(&tcm_dev->report_buf);
		tcm_dev->cb_report_dispatcher[report_code].cb(
				report_code,
				tcm_dev->report_buf.buf,
				tcm_dev->report_buf.data_length,
				tcm_dev->cb_report_dispatcher[report_code].private_data);
		syna_tcm_buf_unlock(&tcm_dev->report_buf);
	}
exit:
	return;
}

/*
 *  Process the response packet read in by the read_message(),
 *  and copy the data from internal buffer.in to internal buffer.resp.
 *
 *  Complete the command processing at the end of function.
 *
 * param
 *    [ in] tcm_dev: pointer to TouchComm device
 *    [ in] store_resp: flag indicating to store the resp packet
 *
 * return
 *    void.
 */
static void syna_tcm_v2_dispatch_response(struct tcm_dev *tcm_dev, bool store_resp)
{
	int retval;
	struct tcm_message_data_blob *tcm_msg = NULL;
	syna_pal_completion_t *cmd_completion = NULL;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return;
	}

	tcm_msg = &tcm_dev->msg_data;
	cmd_completion = &tcm_msg->cmd_completion;

	tcm_msg->response_code = tcm_msg->status_report_code;

	if (tcm_msg->status_report_code == STATUS_ACK)
		return;

	if (tcm_msg->payload_length == 0) {
		if (store_resp)
			tcm_dev->resp_buf.data_length = 0;
		goto exit;
	}

	if (!store_resp)
		goto dispatch;

	/* copy the received resp data into the internal buffer.resp */
	syna_tcm_buf_lock(&tcm_dev->resp_buf);

	retval = syna_tcm_buf_alloc(&tcm_dev->resp_buf, tcm_msg->payload_length);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.resp\n");
		syna_tcm_buf_unlock(&tcm_dev->resp_buf);
		ATOMIC_SET(tcm_msg->command_status, CMD_STATE_ERROR);
		goto exit;
	}

	syna_tcm_buf_lock(&tcm_msg->in);

	retval = syna_pal_mem_cpy(tcm_dev->resp_buf.buf,
			tcm_dev->resp_buf.buf_size,
			&tcm_msg->in.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->in.buf_size - MESSAGE_HEADER_SIZE,
			tcm_msg->payload_length);
	if (retval < 0) {
		LOGE("Fail to copy payload to internal resp_buf\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_tcm_buf_unlock(&tcm_dev->resp_buf);
		ATOMIC_SET(tcm_msg->command_status, CMD_STATE_ERROR);
		goto exit;
	}

	syna_tcm_buf_unlock(&tcm_msg->in);

	tcm_dev->resp_buf.data_length = tcm_msg->payload_length;

dispatch:
	if (tcm_msg->command == CMD_IDENTIFY) {
		retval = syna_tcm_v2_parse_idinfo(tcm_dev,
			tcm_dev->resp_buf.buf,
			tcm_dev->resp_buf.buf_size,
			tcm_dev->resp_buf.data_length);
		if (retval < 0) {
			LOGE("Fail to parse identify packet from resp_buf\n");
			syna_tcm_buf_unlock(&tcm_dev->resp_buf);
			goto exit;
		}
	}

	syna_tcm_buf_unlock(&tcm_dev->resp_buf);

exit:
	switch (tcm_msg->response_code) {
	case STATUS_IDLE:
	case STATUS_NO_REPORT_AVAILABLE:
		break;
	case STATUS_OK:
		ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);
		syna_pal_completion_complete(cmd_completion);
		break;
	case STATUS_CONTINUED_READ:
		LOGE("Out-of-sync continued read\n");
		break;
	default:
		LOGE("Incorrect Status code, 0x%02X, for command %02X\n", tcm_msg->response_code, tcm_msg->command);
		ATOMIC_SET(tcm_msg->command_status, CMD_STATE_ERROR);
		syna_pal_completion_complete(cmd_completion);
		break;
	}
}

/*
 *  Verify the TouchComm v2 packet.
 *
 * param
 *    [ in] tcm_dev:       pointer to TouchComm device
 *    [ in] buf:           buffer being filled in the generated packet
 *    [ in] buf_size:      size of buffer
 *    [ in] packet_size:   size of message packet
 *    [ in] ignore_corrupt_read:   flag to ignore the corrupt read when the fw mode switched
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_check_packet(struct tcm_dev *tcm_dev, unsigned char *buf,
	unsigned int buf_size, unsigned int packet_size, bool ignore_corrupt_read)
{
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int length;
	unsigned char seq;
	unsigned char crc6 = 0;
	unsigned short crc16 = 0xFFFF;
	bool valid_crc;

	tcm_msg = &tcm_dev->msg_data;

	if (!buf || (buf_size < MESSAGE_HEADER_SIZE)) {
		LOGE("Invalid buffer\n");
		return -ERR_INVAL;
	}

	header = (struct tcm_v2_message_header *)buf;
	length = syna_pal_le2_to_uint(header->length);

	if (header->byte3 == (unsigned char)0x5A) {
		if (tcm_msg->has_crc) {
			LOGW("Packet CRC may be disabled, please retry\n");
			return -PACKET_MISMATCHED_CRC_SETUP;
		}
		/* disabling the checking of crc */
		return 0;
	}

	seq = (header->byte3 & 0x40) >> 6;
	crc6 = (header->byte3 & 0x3f);

	valid_crc = (syna_tcm_crc6(header->data, MESSAGE_HEADER_SIZE << 3) == 0);
	/* check header crc always */
	if (!valid_crc) {
		if (ignore_corrupt_read) {
			LOGW("Read corrupted, assuming ACK because of %02X command\n", tcm_msg->command);
			header->code = CMD_ACK;
			return 0;
		}
		LOGW("Incorrect header crc6: 0x%02x\n", crc6);
		return -PACKET_CRC_FAILURE;
	}

	if (header->code == STATUS_RETRY_REQUESTED) {
		LOGD("Catch the RETRY_REQUEST\n");
		if ((valid_crc) && (!tcm_msg->has_crc)) {
			LOGW("Packet CRC may be enabled, please retry\n");
			return -PACKET_MISMATCHED_CRC_SETUP;
		}

		return -PACKET_RETRY_REQUEST;
	}

	if (tcm_msg->seq_toggle != seq) {
		if ((tcm_msg->command == CMD_RESET) || (tcm_msg->command == CMD_RUN_BOOTLOADER_FIRMWARE)) {
			LOGW("Sequence bit mismatched %d (expected:%d) assuming ACK because of %02X command\n",
				seq, tcm_msg->seq_toggle, tcm_msg->command);
			header->code = CMD_ACK;
			return 0;
		}

		LOGW("Incorrect sequence bit %d, expected:%d\n", seq, tcm_msg->seq_toggle);
		return -PACKET_CORRUPTED;
	}

	/* check the size of packet if predict reads enabled */
	if ((length > 0) && (packet_size > MESSAGE_HEADER_SIZE) && (tcm_msg->predict_reads)) {
		if ((length > (packet_size - MESSAGE_HEADER_SIZE)) && (length < tcm_dev->max_rd_size - EXTRA_PACKET_BYTES)) {
			LOGD("Incorrect size from predict reads, read again\n");
			return -PACKET_INVALID_PREDICT;
		}
	}

#ifdef CHECK_PACKET_CRC
	/* check payload crc */
	if ((length > 0) && (packet_size > MESSAGE_HEADER_SIZE)) {
		crc16 = (unsigned short)syna_pal_le2_to_uint(&buf[packet_size - TCM_MSG_CRC_LENGTH]);
		if (syna_tcm_crc16(buf, packet_size, 0xFFFF) != 0) {
			LOGW("Incorrect payload crc16: 0x%02x\n", crc16);
			return -PACKET_CRC_FAILURE;
		}
		tcm_msg->crc_bytes = crc16;
	}
#endif
	return 0;
}

/*
 *  Assemble the TouchComm v2 packet.
 *
 * param
 *    [ in] tcm_dev:       pointer to TouchComm device
 *    [ in] command:       command code
 *    [ in] payload:       data payload if any
 *    [ in] payload_size:  size of given payload
 *    [ in] header_length: length being filled into the header
 *    [ in] resend:        flag determining whether resend the packet
 *    [out] buf:           buffer being filled in the generated packet
 *    [out] buf_size:      size of buffer
 *    [out] packet_size:   size of message packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_create_packet(struct tcm_dev *tcm_dev, unsigned char command,
	unsigned char *payload, unsigned int payload_size, unsigned int header_length,
	bool resend, unsigned char *buf, unsigned int buf_size, unsigned int *packet_size)
{
	int retval = 0;
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int expected_size = MESSAGE_HEADER_SIZE + payload_size;
	unsigned char crc6;
	unsigned short crc16;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (payload_size > 0)
		expected_size += TCM_MSG_CRC_LENGTH;

	if (!buf || (buf_size < expected_size)) {
		LOGE("Invalid buffer\n");
		return -ERR_INVAL;
	}

	if (payload_size > header_length) {
		LOGE("Invalid payload length, given:%d (header:%d)\n", payload_size, header_length);
		return -ERR_INVAL;
	}

	header = (struct tcm_v2_message_header *)buf;

	if (!resend)
		tcm_msg->seq_toggle = (tcm_msg->seq_toggle + 1) & 0x01;

	header->code = command;
	header->length[0] = (unsigned char)header_length;
	header->length[1] = (unsigned char)(header_length >> 8);
	if (tcm_msg->has_crc) {
		header->byte3 = ((HOST_PRIMARY & 0x01) << 7);
		header->byte3 |= (tcm_msg->seq_toggle << 6);
		crc6 = syna_tcm_crc6(header->data, (MESSAGE_HEADER_SIZE << 3) - 6);
		header->byte3 |= crc6;
		LOGD("Command packet: %02X %02X %02X %02X, payload length:%d (seq:%d, crc6:%02X)\n",
			header->data[0], header->data[1], header->data[2], header->data[3],
			payload_size, tcm_msg->seq_toggle, crc6);
	} else {
		header->byte3 = 0x5A;
		LOGD("Command packet: %02X %02X %02X %02X, payload length:%d\n",
			header->data[0], header->data[1], header->data[2], header->data[3], payload_size);
	}

	if (payload_size != header_length)
		LOGD("payload length in header:%d, actual to write:%d\n", header_length, payload_size);

	*packet_size = MESSAGE_HEADER_SIZE;

	if (payload_size > 0) {
		retval = syna_pal_mem_cpy(&buf[MESSAGE_HEADER_SIZE], buf_size - MESSAGE_HEADER_SIZE,
			payload, payload_size, payload_size);
		if (retval < 0) {
			LOGE("Fail to copy payload data\n");
			return retval;
		}

		/* append payload crc */
		*packet_size += payload_size;
		crc16 = (tcm_msg->has_crc) ? syna_tcm_crc16(buf, *packet_size, 0xFFFF) : 0xA5A5;
		buf[*packet_size] = (unsigned char)((crc16 >> 8) & 0xFF);
		buf[*packet_size + 1] = (unsigned char)(crc16 & 0xFF);
		*packet_size += TCM_MSG_CRC_LENGTH;
	}

	return retval;
}

/*
 *  Assemble the TouchComm v2 packet and send the packet to device.
 *
 * param
 *    [ in] tcm_dev:       pointer to TouchComm device
 *    [ in] command:       command code
 *    [ in] payload:       data payload if any
 *    [ in] size:          size of given payload
 *    [ in] header_length: length being filled into the header
 *    [ in] resend:        flag for re-sending the packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_write(struct tcm_dev *tcm_dev, unsigned char command,
	unsigned char *payload, unsigned int size, unsigned int header_length, bool resend)
{
	int retval;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int max_wr_size;
	unsigned int total_xfer_size;
	int retry = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	max_wr_size = tcm_dev->max_wr_size;

	/* size of each write transaction shall not larger than the max write size */
	if ((max_wr_size != 0) && (size > max_wr_size)) {
		LOGE("Invalid xfer length, len: %d, max_wr_size: %d\n",
			size, max_wr_size);
		tcm_msg->status_report_code = STATUS_INVALID;
		return -ERR_INVAL;
	}

	total_xfer_size = size + sizeof(struct tcm_v2_message_header);
	if (size > 0)
		total_xfer_size += TCM_MSG_CRC_LENGTH;

	syna_tcm_buf_lock(&tcm_msg->out);

	/* allocate the internal out buffer */
	retval = syna_tcm_buf_alloc(&tcm_msg->out, total_xfer_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.out\n");
		goto exit;
	}
	retval = syna_tcm_v2_create_packet(tcm_dev, command, payload, size, header_length,
		resend, tcm_msg->out.buf, tcm_msg->out.buf_size, &tcm_msg->out.data_length);
	if (retval < 0) {
		LOGE("Fail to create command packet for command 0x%02X\n", command);
		goto exit;
	}

	/* write command packet to the bus */
	do {
		retval = syna_tcm_write(tcm_dev, tcm_msg->out.buf, tcm_msg->out.data_length);
		if (retval < 0) {
			LOGE("Fail to write command 0x%02X to device, do retry %d\n",
				command, ++retry);

			syna_pal_sleep_ms(WR_RD_RETRY_DELAY);
		}
	} while ((retval < 0) && (retry < WR_RD_RETRY_TIMES));

exit:
	syna_tcm_buf_unlock(&tcm_msg->out);
#ifndef OS_WIN
	/* delay for the bus turnaround time */
	syna_pal_sleep_us(tcm_msg->turnaround_time);
#endif
	return retval;
}
/*
 *  Read in a TouchComm packet from device.
 *
 * param
 *    [ in] tcm_dev:    pointer to TouchComm device
 *    [ in] rd_length:  number of reading bytes;
 *                      '0' means to read the message header only
 *    [ in] ignore_corrupt_read:   flag to ignore the corrupt read when the fw mode switched
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_read(struct tcm_dev *tcm_dev, unsigned int rd_length, bool ignore_corrupt_read)
{
	int retval;
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int max_rd_size;
	unsigned int size = MESSAGE_HEADER_SIZE;
	unsigned int xfer_len = (unsigned int)sizeof(struct tcm_v2_message_header);
	int retry = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	max_rd_size = tcm_dev->max_rd_size;

	if (rd_length > 0)
		xfer_len += (rd_length + TCM_MSG_CRC_LENGTH);

	/* size of each read transaction shall not larger than the max read size */
	if ((max_rd_size != 0) && (rd_length > max_rd_size)) {
		LOGE("Invalid xfer length:%d (rd_length:%d), max_rd_size:%d\n",
			xfer_len, rd_length, max_rd_size);
		return -ERR_INVAL;
	}

	syna_tcm_buf_lock(&tcm_msg->temp);

	/* allocate the internal temp buffer */
	retval = syna_tcm_buf_alloc(&tcm_msg->temp, xfer_len);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.temp\n");
		goto exit;
	}
	/* read data from the bus */
	do {
		retval = syna_tcm_read(tcm_dev, tcm_msg->temp.buf, xfer_len);
		if (retval < 0) {
			LOGE("Fail to read %d bytes from device, do retry %d\n", xfer_len, ++retry);
			syna_pal_sleep_ms(WR_RD_RETRY_DELAY);
			continue;
		}

		tcm_msg->temp.data_length = xfer_len;

		header = (struct tcm_v2_message_header *)tcm_msg->temp.buf;

		if (tcm_msg->has_crc) {
			LOGD("Data %02X %02X %02X %02X (seq:%d, crc6:%02X) (rd_length:%d)\n",
				header->data[0], header->data[1], header->data[2], header->data[3],
				((header->byte3 & 0x40) >> 6), (header->byte3 & 0x3f), rd_length);
		} else {
			LOGD("Data %02X %02X %02X %02X (rd_length:%d)\n",
				header->data[0], header->data[1], header->data[2], header->data[3], rd_length);
		}

		size += syna_pal_le2_to_uint(header->length);
		if (size > MESSAGE_HEADER_SIZE)
			size += TCM_MSG_CRC_LENGTH;
		if (size > tcm_msg->temp.data_length)
			size = tcm_msg->temp.data_length;

		retval = syna_tcm_v2_check_packet(tcm_dev, tcm_msg->temp.buf, tcm_msg->temp.buf_size, size, ignore_corrupt_read);
		if (retval < 0) {
			switch (retval) {
			case -PACKET_MISMATCHED_CRC_SETUP:
			case -PACKET_INVALID_PREDICT:
			case -PACKET_CORRUPTED:
			case -PACKET_RETRY_REQUEST:
				goto exit;
			default:
				LOGW("Invalid packet retrieved, do retry %d\n", ++retry);
				syna_pal_sleep_ms(WR_RD_RETRY_DELAY);
				break;
			}
		}
	} while ((retval < 0) && (retry < WR_RD_RETRY_TIMES));

exit:
	syna_tcm_buf_unlock(&tcm_msg->temp);
#ifndef OS_WIN
	/* delay for the bus turnaround time */
	syna_pal_sleep_us(tcm_msg->turnaround_time);
#endif
	return retval;
}

/*
 *  Assemble the TouchComm v2 packet and then perform write-then-read operation
 *
 * param
 *    [ in] tcm_dev:       pointer to TouchComm device
 *    [ in] command:       command code
 *    [ in] payload:       data payload if any
 *    [ in] size:          size of given payload
 *    [ in] header_length: length being filled into the header
 *    [ in] rd_length:     number of reading bytes;
 *                         '0' means to read the message header only
 *    [ in] resend:        flag for re-sending the packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_write_then_read(struct tcm_dev *tcm_dev, unsigned char command,
	unsigned char *payload, unsigned int size, unsigned int header_length,
	unsigned int rd_length, bool resend)
{
	int retval;
	struct tcm_hw_platform *hw;
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int wr_size;
	unsigned int valid_length;
	int retry = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	hw = tcm_dev->hw;
	if (!hw->ops_write_then_read_data) {
		LOGE("Invalid write then read operation\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	wr_size = size + sizeof(struct tcm_v2_message_header);
	if (size > 0)
		wr_size += TCM_MSG_CRC_LENGTH;

	syna_tcm_buf_lock(&tcm_msg->temp);

	/* allocate the internal temp buffer */
	retval = syna_tcm_buf_alloc(&tcm_msg->temp, rd_length);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.temp\n");
		syna_tcm_buf_unlock(&tcm_msg->temp);
		return retval;
	}

	syna_tcm_buf_lock(&tcm_msg->out);

	/* allocate the internal out buffer */
	retval = syna_tcm_buf_alloc(&tcm_msg->out, wr_size);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf.out\n");
		goto exit;
	}
	retval = syna_tcm_v2_create_packet(tcm_dev, command, payload, size, header_length,
		resend, tcm_msg->out.buf, tcm_msg->out.buf_size, &tcm_msg->out.data_length);
	if (retval < 0) {
		LOGE("Fail to create command packet for command 0x%02X\n", command);
		goto exit;
	}

	do {
		retval = hw->ops_write_then_read_data(hw, tcm_msg->out.buf,
			tcm_msg->out.data_length, tcm_msg->temp.buf, rd_length,
			tcm_msg->write_then_read_turnaround_bytes);
		if (retval < 0) {
			LOGE("Fail to do write and read for command 0x%02X, do retry %d\n",
				command, ++retry);
			syna_pal_sleep_ms(WR_RD_RETRY_DELAY);
			continue;
		}

		tcm_msg->temp.data_length = rd_length;

		header = (struct tcm_v2_message_header *)tcm_msg->temp.buf;
		valid_length = syna_pal_le2_to_uint(header->length);

		if (tcm_msg->has_crc) {
			LOGD("Data %02X %02X %02X %02X (seq:%d, crc6:%02X) (length:%d)\n",
				header->data[0], header->data[1], header->data[2], header->data[3],
				((header->byte3 & 0x40) >> 6), (header->byte3 & 0x3f), valid_length);
		} else {
			LOGD("Data %02X %02X %02X %02X (length:%d)\n",
				header->data[0], header->data[1], header->data[2], header->data[3], valid_length);
		}

		if (valid_length > MESSAGE_HEADER_SIZE)
			valid_length += TCM_MSG_CRC_LENGTH;

		retval = syna_tcm_v2_check_packet(tcm_dev, tcm_msg->temp.buf,
				tcm_msg->temp.buf_size, valid_length + MESSAGE_HEADER_SIZE, false);
		if (retval < 0) {
			switch (retval) {
			case -PACKET_MISMATCHED_CRC_SETUP:
			case -PACKET_INVALID_PREDICT:
			case -PACKET_CORRUPTED:
			case -PACKET_RETRY_REQUEST:
				goto exit;
			default:
				LOGW("Invalid packet retrieved, do retry %d\n", ++retry);
				syna_pal_sleep_ms(WR_RD_RETRY_DELAY);
				break;
			}
		}

	} while ((retval < 0) && (retry < (int)tcm_msg->retry_cmd_cnt));

exit:
	syna_tcm_buf_unlock(&tcm_msg->out);
	syna_tcm_buf_unlock(&tcm_msg->temp);
#ifndef OS_WIN
	/* delay for the bus turnaround time */
	syna_pal_sleep_us(tcm_msg->turnaround_time);
#endif
	return retval;
}

/*
 *  Continuously read in the remaining payload of data from device.
 *
 * param
 *    [ in] tcm_dev:  pointer to TouchComm device
 *    [ in] payload_length: requested length of remaining payload
 *    [ in] has_first_chunk: flag to determine whether the first chunk is required to read
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_continued_read(struct tcm_dev *tcm_dev, unsigned int payload_length,
	bool has_first_chunk)
{
	int retval = 0;
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned int iterations = 0, offset = 0;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int total_length;
	unsigned int remaining_length;
	unsigned int xfer_len;
	unsigned int valid_len;
	bool resend = false;
	int retry = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	if ((payload_length == 0) || (tcm_msg->payload_length == 0))
		return 0;

	if ((payload_length & 0xffff) == 0xffff)
		return -ERR_INVAL;

	/* continued read packet contains the header, payload, and crc16 */
	total_length = tcm_msg->payload_length + EXTRA_PACKET_BYTES;
	/* length to read, remember a padding at the end */
	remaining_length = payload_length;

	syna_tcm_buf_lock(&tcm_msg->in);

	/* extend the internal buf_in if needed */
	retval = syna_tcm_buf_realloc(&tcm_msg->in, total_length);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf_in\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		return -ERR_NOMEM;
	}

	/* available chunk space for payload =
	 *     total chunk size - (header + crc16)
	 */
	chunk_space = tcm_dev->max_rd_size;
	if (tcm_dev->max_rd_size == 0) {
		chunk_space = remaining_length;
		if ((tcm_dev->hw->alignment_enabled) && (remaining_length > tcm_dev->hw->alignment_boundary))
			chunk_space = syna_pal_int_alignment(remaining_length, tcm_dev->hw->alignment_base, false);
	}
	chunk_space -= EXTRA_PACKET_BYTES;

	chunks = syna_pal_int_division(remaining_length, chunk_space, true);
	chunks = chunks == 0 ? 1 : chunks;

	offset = MESSAGE_HEADER_SIZE + (tcm_msg->payload_length - payload_length);

	if (has_first_chunk) {
		xfer_len = (remaining_length > chunk_space) ? chunk_space : remaining_length;
		valid_len = xfer_len;

		/* ensure the remaining length to transfer is still aligned */
		if (tcm_dev->hw->alignment_enabled) {
			if ((xfer_len > tcm_dev->hw->alignment_boundary) && (xfer_len == remaining_length)) {
				xfer_len = syna_pal_int_alignment(xfer_len + EXTRA_PACKET_BYTES, tcm_dev->hw->alignment_base, true);
				xfer_len -= EXTRA_PACKET_BYTES;
			}
		}

		/* the first continued read operation can be retrieved directly */
		retval = syna_tcm_v2_read(tcm_dev, xfer_len, false);
		if (retval < 0) {
			LOGE("Fail to continued read %d bytes from device\n", xfer_len);
			retval = -ERR_TCMMSG;
			goto exit;
		}

		/* copy data from internal buffer.temp to buffer.in */
		syna_tcm_buf_lock(&tcm_msg->temp);
		retval = syna_pal_mem_cpy(&tcm_msg->in.buf[offset],
			tcm_msg->in.buf_size - offset,
			&tcm_msg->temp.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->temp.buf_size - MESSAGE_HEADER_SIZE,
			valid_len);
		if (retval < 0) {
			LOGE("Fail to copy payload\n");
			syna_tcm_buf_unlock(&tcm_msg->temp);
			goto exit;
		}

		syna_tcm_buf_unlock(&tcm_msg->temp);

		offset += valid_len;
		remaining_length -= valid_len;
	}

	if (!has_first_chunk)
		chunks += 1;

	/* if still having remaining data, read in through ACK command */
	for (iterations = 1; iterations < chunks; iterations++) {
		retry = 0;
		resend = false;
		do {
			xfer_len = (remaining_length > chunk_space) ? chunk_space : remaining_length;
			valid_len = xfer_len;

			if (tcm_dev->hw->alignment_enabled) {
				if ((xfer_len > tcm_dev->hw->alignment_boundary) && (xfer_len == remaining_length)) {
					xfer_len = syna_pal_int_alignment(xfer_len + EXTRA_PACKET_BYTES, tcm_dev->hw->alignment_base, true);
					xfer_len -= EXTRA_PACKET_BYTES;
				}
			}

			if (tcm_msg->write_then_read_support) {
				retval = syna_tcm_v2_write_then_read(tcm_dev, CMD_ACK, NULL, 0, 0,
						xfer_len + EXTRA_PACKET_BYTES, resend);
			} else {
				retval = syna_tcm_v2_write(tcm_dev, CMD_ACK, NULL, 0, 0, resend);
				if (retval < 0) {
					LOGE("Fail to send ACK for continued read\n");
					syna_pal_sleep_ms(tcm_msg->command_retry_time);
					continue;
				}

				retval = syna_tcm_v2_read(tcm_dev, xfer_len, false);
			}

			if (retval < 0) {
				if (retval == -PACKET_CORRUPTED) {
					LOGW("Read corrupted at chunk %d/%d, retry %d\n", iterations, chunks, ++retry);
					resend = true;

					syna_pal_sleep_ms(tcm_msg->command_retry_time);
					continue;
				}
				LOGE("Fail to continued read %d bytes from device at chunk %d/%d\n",
					xfer_len, iterations, chunks);
				retval = -ERR_TCMMSG;
				goto exit;
			}
		} while ((retval < 0) && (retry < (int)tcm_msg->retry_cmd_cnt));

		/* append data from temporary buffer to in_buf */
		syna_tcm_buf_lock(&tcm_msg->temp);

		/* copy data from internal buffer.temp to buffer.in */
		retval = syna_pal_mem_cpy(&tcm_msg->in.buf[offset],
			tcm_msg->in.buf_size - offset,
			&tcm_msg->temp.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->temp.buf_size - MESSAGE_HEADER_SIZE,
			valid_len);
		if (retval < 0) {
			LOGE("Fail to copy payload to internal buf_in\n");
			syna_tcm_buf_unlock(&tcm_msg->temp);
			goto exit;
		}

		syna_tcm_buf_unlock(&tcm_msg->temp);

		offset += valid_len;
		remaining_length -= valid_len;
	}

	tcm_msg->in.data_length = offset;

exit:
	syna_tcm_buf_unlock(&tcm_msg->in);

	return retval;
}

/*
 *  Process the message and retrieve the response through the write-then-read operation.
 *
 * param
 *    [ in] tcm_dev:        pointer to TouchComm device
 *    [ in] command:        command code
 *    [ in] payload:        data payload if any
 *    [ in] payload_length: size of payload data
 *    [ in] header_length:  total size of payload data being filled into the header
 *    [ in] store_resp:     flag indicating to store the resp packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_process_command_write_then_read(struct tcm_dev *tcm_dev, unsigned char command,
	unsigned char *payload, unsigned int payload_length, unsigned int header_length, bool store_resp)
{
	int retval = 0;
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	bool resend = false;
	bool do_predict = false;
	unsigned int len = 0;
	int len_copy = 0;
	int remaining_len = 0;
	int retry = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	/* predict reading is applied only when processing GET_REPORT command */
	do_predict = (tcm_msg->predict_reads) && (command == CMD_GET_REPORT);

	retry = 0;
	resend = false;
	do {
		if (do_predict && (tcm_msg->predict_length > 0))
			len = tcm_msg->predict_length;

		retval = syna_tcm_v2_write_then_read(tcm_dev, command, payload, payload_length,
				header_length, MESSAGE_HEADER_SIZE + len, resend);
		if (retval < 0) {
			if ((retval == -PACKET_INVALID_PREDICT) && (tcm_msg->predict_reads)) {
				len = syna_pal_le2_to_uint(&tcm_msg->temp.buf[1]);
				resend = true;
				continue;
			}

			if ((retval == -PACKET_CORRUPTED) || (retval == -PACKET_RETRY_REQUEST))
				resend = true;

			syna_pal_sleep_ms(tcm_msg->command_retry_time);
			LOGD("Retry the command processing of %02X command, retry %d\n", command, retry);
		}
	} while ((retval < 0) && (++retry < (int)tcm_msg->retry_cmd_cnt));

	if (retry >= (int)tcm_msg->retry_cmd_cnt) {
		LOGE("Fail to process a command 0x%02X status code 0x%02X\n",
			command, tcm_msg->status_report_code);
		retval = -ERR_TCMMSG;
		goto exit;
	}

	syna_tcm_buf_lock(&tcm_msg->temp);

	header = (struct tcm_v2_message_header *)tcm_msg->temp.buf;

	tcm_msg->status_report_code = header->code;
	tcm_msg->payload_length = syna_pal_le2_to_uint(header->length);

	LOGD("Status code: 0x%02X, payload length: %d\n",
		tcm_msg->status_report_code, tcm_msg->payload_length);

	syna_tcm_buf_lock(&tcm_msg->in);

	/* extend the internal buffer.in if needed */
	retval = syna_tcm_buf_realloc(&tcm_msg->in, tcm_msg->payload_length + MESSAGE_HEADER_SIZE);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf_in\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_tcm_buf_unlock(&tcm_msg->temp);
		goto exit;
	}

	/* copy data from internal buffer.temp to buffer.in */
	len_copy = 0;
	if (tcm_msg->temp.data_length > MESSAGE_HEADER_SIZE) {
		len_copy = (tcm_msg->payload_length > tcm_msg->temp.data_length) ?
			tcm_msg->temp.data_length - TCM_MSG_CRC_LENGTH : tcm_msg->payload_length;

		retval = syna_pal_mem_cpy(tcm_msg->in.buf, tcm_msg->in.buf_size,
				tcm_msg->temp.buf, tcm_msg->temp.buf_size, len_copy);
		if (retval < 0) {
			LOGE("Fail to copy payload to internal buf_in\n");
			syna_tcm_buf_unlock(&tcm_msg->in);
			syna_tcm_buf_unlock(&tcm_msg->temp);
			goto exit;
		}
		tcm_msg->in.data_length = len_copy;
	} else {
		retval = syna_pal_mem_cpy(tcm_msg->in.buf, tcm_msg->in.buf_size,
				tcm_msg->temp.buf, tcm_msg->temp.buf_size, tcm_msg->temp.buf_size);
		if (retval < 0) {
			LOGE("Fail to copy payload to internal buf_in\n");
			syna_tcm_buf_unlock(&tcm_msg->in);
			syna_tcm_buf_unlock(&tcm_msg->temp);
			goto exit;
		}
		tcm_msg->in.data_length = tcm_msg->temp.buf_size;
	}

	syna_tcm_buf_unlock(&tcm_msg->in);
	syna_tcm_buf_unlock(&tcm_msg->temp);


	/* read in remaining payload */
	remaining_len = tcm_msg->payload_length - len;
	if (remaining_len > 0) {
		LOGD("Prepare to read in remaining payload, remaining size: %d\n", remaining_len);
		retval = syna_tcm_v2_continued_read(tcm_dev, remaining_len, (len == 0));
		if (retval < 0) {
			LOGE("Fail to read in remaining payload, remaining size: %d\n", remaining_len);
			goto exit;
		}
	}

	/* duplicate the data to the external buffer */
	if (tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].cb) {
		syna_tcm_buf_lock(&tcm_msg->in);
		tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].cb(
			tcm_msg->status_report_code,
			&tcm_msg->in.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->payload_length,
			tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].private_data);
		syna_tcm_buf_unlock(&tcm_msg->in);
	}

	/* process the retrieved packet */
	if (tcm_msg->status_report_code >= REPORT_IDENTIFY)
		syna_tcm_v2_dispatch_report(tcm_dev);
	else
		syna_tcm_v2_dispatch_response(tcm_dev, store_resp);

	/* update the length for the predict reading */
	if (do_predict) {
		if (tcm_dev->max_rd_size == 0)
			tcm_msg->predict_length = tcm_msg->payload_length;
		else
			tcm_msg->predict_length = MIN(tcm_msg->payload_length,
				tcm_dev->max_rd_size - EXTRA_PACKET_BYTES);

		if (tcm_msg->status_report_code <= REPORT_TOUCH)
			tcm_msg->predict_length = 0;
	}
exit:
	return retval;
}

/*
 *  Process the message and retrieve the immediate response to the command.
 *
 * param
 *    [ in] tcm_dev:        pointer to TouchComm device
 *    [ in] command:        command code
 *    [ in] payload:        data payload if any
 *    [ in] payload_length: size of payload data
 *    [ in] header_length:  total size of payload data being filled into the header
 *    [ in] store_resp:     flag indicating to store the resp packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_process_command(struct tcm_dev *tcm_dev, unsigned char command,
	unsigned char *payload, unsigned int payload_length, unsigned int header_length, bool store_resp)
{
	int retval = 0;
	struct tcm_v2_message_header *header;
	struct tcm_message_data_blob *tcm_msg = NULL;
	bool resend = false;
	bool do_predict = false;
	unsigned int len = 0;
	int len_copy;
	int remaining_len = 0;
	int retry = 0;
	int timeout = 0;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;

	if (!tcm_msg) {
		LOGE("Invalid tcm message blob\n");
		return -ERR_INVAL;
	}

	/* predict reading is applied only when processing GET_REPORT command */
	do_predict = (tcm_msg->predict_reads) && (command == CMD_GET_REPORT);

	retry = 0;
	resend = false;
	do {
		if ((retry > 0) && (retval != -PACKET_MISMATCHED_CRC_SETUP)) {
			syna_pal_sleep_ms(tcm_msg->command_retry_time);
			LOGW("Retry the command processing of %02X command, retry %d\n", command, retry);
		}

		/* send command to device */
		retval = syna_tcm_v2_write(tcm_dev, command, payload,
				payload_length, header_length, resend);
		if (retval < 0) {
			LOGE("Fail to process command 0x%02X\n", command);
			goto exit;
		}

		if (do_predict && (tcm_msg->predict_length > 0))
			len = tcm_msg->predict_length;

		/* read in the immediate response */
		timeout = 0;
		do {
			if (timeout++ > (int)tcm_msg->retry_resp_cnt) {
				LOGE("Fail to read in the response to %02X command, timeout !\n", command);
				retval = -ERR_TCMMSG;
				break;
			}

			if ((tcm_dev->hw->alignment_enabled) && (len > tcm_dev->hw->alignment_boundary))
				len = syna_pal_int_alignment(len, tcm_dev->hw->alignment_base, false);

			/* workaround if the resp to bootloader command doesn't return immediately */
			if ((timeout > 1) && (IS_BOOTLOADER_MODE(tcm_dev->dev_mode)))
				syna_pal_sleep_ms(RESP_RETRY_DELAY);

			retval = syna_tcm_v2_read(tcm_dev, len, syna_tcm_v2_is_mode_switching(tcm_dev, command));
			if (retval < 0) {
				if ((retval == -PACKET_INVALID_PREDICT) && (tcm_msg->predict_reads)) {
					len = syna_pal_le2_to_uint(&tcm_msg->temp.buf[1]);
					continue;
				}

				if (retval == -PACKET_MISMATCHED_CRC_SETUP)
					break;

				syna_pal_sleep_ms(RESP_RETRY_DELAY);
				if ((retval != -PACKET_CORRUPTED) && (retval != -PACKET_RETRY_REQUEST))
					LOGW("Attempt to read in the immediate response, retry %d\n", timeout);
			}
		} while ((retval < 0) && (retval != -PACKET_CORRUPTED) && (retval != -PACKET_RETRY_REQUEST) && (tcm_msg->retry_resp_cnt));

		if ((retval < 0) && ((retval == -PACKET_CORRUPTED) || (retval == -PACKET_RETRY_REQUEST)))
			resend = true;

		if (retval == -PACKET_MISMATCHED_CRC_SETUP) {
			tcm_msg->has_crc = !tcm_msg->has_crc;
			continue;
		}

	} while ((retval < 0) && (++retry < (int)tcm_msg->retry_cmd_cnt));

	if (retval < 0) {
		LOGE("Fail to process a command 0x%02X status code 0x%02X\n", command, tcm_msg->status_report_code);
		retval = -ERR_TCMMSG;
		goto exit;
	}

	syna_tcm_buf_lock(&tcm_msg->temp);

	header = (struct tcm_v2_message_header *)tcm_msg->temp.buf;

	tcm_msg->status_report_code = header->code;
	tcm_msg->payload_length = syna_pal_le2_to_uint(header->length);

	LOGD("Status code: 0x%02X, payload length: %d\n",
		tcm_msg->status_report_code, tcm_msg->payload_length);

	syna_tcm_buf_lock(&tcm_msg->in);

	/* extend the internal buffer.in if needed */
	retval = syna_tcm_buf_realloc(&tcm_msg->in, tcm_msg->payload_length + MESSAGE_HEADER_SIZE);
	if (retval < 0) {
		LOGE("Fail to allocate memory for internal buf_in\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_tcm_buf_unlock(&tcm_msg->temp);
		goto exit;
	}

	len_copy = tcm_msg->temp.data_length;
	if (len > 0)
		len_copy -= TCM_MSG_CRC_LENGTH;

	/* copy data from internal buffer.temp to buffer.in */
	retval = syna_pal_mem_cpy(tcm_msg->in.buf, tcm_msg->in.buf_size,
			tcm_msg->temp.buf, tcm_msg->temp.buf_size, len_copy);
	if (retval < 0) {
		LOGE("Fail to copy payload to internal buf_in\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_tcm_buf_unlock(&tcm_msg->temp);
		goto exit;
	}

	tcm_msg->in.data_length = len_copy;

	syna_tcm_buf_unlock(&tcm_msg->in);
	syna_tcm_buf_unlock(&tcm_msg->temp);


	remaining_len = tcm_msg->payload_length - len;

	/* read in remaining payload */
	if (remaining_len > 0) {
		LOGD("Prepare to read in remaining payload, remaining size: %d\n", remaining_len);
		retval = syna_tcm_v2_continued_read(tcm_dev, remaining_len, (len == 0));
		if (retval < 0) {
			LOGE("Fail to read in remaining payload, remaining size: %d\n", remaining_len);
			goto exit;
		}
	}

	/* duplicate the data to the external buffer */
	if (tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].cb) {
		syna_tcm_buf_lock(&tcm_msg->in);
		tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].cb(
			tcm_msg->status_report_code,
			&tcm_msg->in.buf[MESSAGE_HEADER_SIZE],
			tcm_msg->payload_length,
			tcm_dev->cb_data_duplicator[tcm_msg->status_report_code].private_data);
		syna_tcm_buf_unlock(&tcm_msg->in);
	}

	/* process the retrieved packet */
	if (tcm_msg->status_report_code >= REPORT_IDENTIFY)
		syna_tcm_v2_dispatch_report(tcm_dev);
	else
		syna_tcm_v2_dispatch_response(tcm_dev, store_resp);

	/* update the length for the predict reading */
	if (do_predict) {
		if (tcm_dev->max_rd_size == 0)
			tcm_msg->predict_length = tcm_msg->payload_length;
		else
			tcm_msg->predict_length = MIN(tcm_msg->payload_length,
				tcm_dev->max_rd_size - EXTRA_PACKET_BYTES);

		if (tcm_msg->status_report_code <= REPORT_TOUCH)
			tcm_msg->predict_length = 0;
	}
exit:
	return retval;
}

/*
 *  The entry to read in a TouchComm message packet from device.
 *  According the type of data, process and dispatch the message accordingly.
 *
 * param
 *    [ in] tcm_dev:            pointer to TouchComm device
 *    [out] status_report_code: status code or report code in the packet
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_read_message(struct tcm_dev *tcm_dev,
	unsigned char *status_report_code)
{
	int retval;
	struct tcm_message_data_blob *tcm_msg = NULL;
	syna_pal_mutex_t *rw_mutex = NULL;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;
	rw_mutex = &tcm_msg->rw_mutex;

	if (status_report_code)
		*status_report_code = STATUS_INVALID;

	syna_pal_mutex_lock(rw_mutex);

	tcm_msg->status_report_code = STATUS_IDLE;

	tcm_msg->crc_bytes = 0;

	/* request a command */
	if (tcm_msg->write_then_read_support)
		retval = syna_tcm_v2_process_command_write_then_read(tcm_dev, CMD_GET_REPORT, NULL, 0, 0, true);
	else
		retval = syna_tcm_v2_process_command(tcm_dev, CMD_GET_REPORT, NULL, 0, 0, true);
	if (retval < 0) {
		LOGE("Fail to send command CMD_GET_REPORT\n");
		goto exit;
	}

	/* copy the status report code to caller */
	if (status_report_code)
		*status_report_code = tcm_msg->status_report_code;

exit:
	syna_pal_mutex_unlock(rw_mutex);

	return retval;
}
/*
 *  A block function to wait for the ATTN assertion.
 *
 * param
 *    [ in] tcm_dev:       pointer to TouchComm device
 *    [ in] timeout:       timeout time waiting for the assertion
 * return
 *    void.
 */
static void syna_tcm_v2_wait_for_attn(struct tcm_dev *tcm_dev, int timeout)
{
	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return;
	}

	/* if set, invoke the custom function to wait for the ATTN assertion;
	 * otherwise, wait for the completion event.
	 */
	if (tcm_dev->hw->ops_wait_for_attn)
		tcm_dev->hw->ops_wait_for_attn(tcm_dev->hw, timeout);
	else
		syna_pal_completion_wait_for(&tcm_dev->msg_data.cmd_completion, timeout);
}
/*
 *  The entry of the command processing, that includes sending a command message
 *  to device and then read in the response to the command.
 *
 * param
 *    [ in] tcm_dev:        pointer to TouchComm device
 *    [ in] command:        TouchComm command
 *    [ in] payload:        data payload, if any
 *    [ in] payload_length: length of data payload, if any
 *    [out] resp_code:      response code returned
 *    [ in] resp_reading:   method to read in the response
 *                          a positive value presents the ms time delay for polling;
 *                          or, set '0' or 'RESP_IN_ATTN' for ATTN driven
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_v2_write_message(struct tcm_dev *tcm_dev,
	unsigned char command, unsigned char *payload,
	unsigned int payload_length, unsigned char *resp_code,
	unsigned int resp_reading)
{
	int retval;
	struct tcm_message_data_blob *tcm_msg = NULL;
	syna_pal_mutex_t *cmd_mutex = NULL;
	syna_pal_mutex_t *rw_mutex = NULL;
	syna_pal_completion_t *cmd_completion = NULL;
	bool in_polling = false;
	bool irq_disabled = false;
	unsigned int total_length;
	unsigned int remaining_length;
	unsigned int chunks;
	unsigned int chunk_space;
	unsigned int xfer_length;
	unsigned int iterations = 0, offset = 0;
	unsigned int timeout = 0;
	bool last = false;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_msg = &tcm_dev->msg_data;
	cmd_mutex = &tcm_msg->cmd_mutex;
	rw_mutex = &tcm_msg->rw_mutex;
	cmd_completion = &tcm_msg->cmd_completion;

	if (resp_code)
		*resp_code = STATUS_INVALID;

	/* indicate which mode is used */
	in_polling = (resp_reading != CMD_RESPONSE_IN_ATTN);

	syna_pal_mutex_lock(cmd_mutex);

	syna_pal_mutex_lock(rw_mutex);

	ATOMIC_SET(tcm_dev->command_processing, 1);
	ATOMIC_SET(tcm_msg->command_status, CMD_STATE_BUSY);

	/* reset the command completion */
	syna_pal_completion_reset(cmd_completion);

	tcm_msg->command = command;

	LOGD("Command: 0x%02x, payload size: %d  %s\n",
		command, payload_length, (in_polling) ? "(by polling)" : "");

	/* disable irq in case of polling mode */
	if (in_polling)
		irq_disabled = (syna_tcm_enable_irq(tcm_dev, false) > 0);

	/* include the header and two bytes of CRC */
	total_length = payload_length + MESSAGE_HEADER_SIZE;
	if (payload_length > 0)
		total_length += TCM_MSG_CRC_LENGTH;

	/* available space for payload = total size - header - crc */
	chunk_space = tcm_dev->max_wr_size;
	if (tcm_dev->max_wr_size == 0) {
		chunk_space = total_length;
		if ((tcm_dev->hw->alignment_enabled) && (chunk_space > tcm_dev->hw->alignment_boundary))
			chunk_space = syna_pal_int_alignment(chunk_space, tcm_dev->hw->alignment_base, false);
	}

	chunk_space -= EXTRA_PACKET_BYTES;

	chunks = syna_pal_int_division(payload_length, chunk_space, true);
	chunks = chunks == 0 ? 1 : chunks;

	remaining_length = payload_length;

	/* separate into several sub-transfers if the overall size is over
	 * than the maximum write size.
	 */
	offset = 0;
	for (iterations = 0; iterations < chunks; iterations++) {

		last = ((iterations + 1) == chunks);

		if (remaining_length > chunk_space)
			xfer_length = chunk_space;
		else
			xfer_length = remaining_length;

		if (tcm_dev->hw->alignment_enabled) {
			if (last && (xfer_length > tcm_dev->hw->alignment_boundary)) {
				xfer_length = syna_pal_int_alignment(xfer_length, tcm_dev->hw->alignment_base, false);
				xfer_length -= EXTRA_PACKET_BYTES;
				if (xfer_length != remaining_length) {
					chunks += 1;
					last = false;
				}
			}
		}

		retval = syna_tcm_v2_process_command(tcm_dev,
			(iterations == 0) ? command : CMD_CONTINUE_WRITE,
			&payload[offset], xfer_length, remaining_length, true);
		if (retval < 0) {
			LOGE("Fail to send command 0x%02X to device\n", command);
			syna_pal_mutex_unlock(rw_mutex);
			goto exit;
		}

		offset += xfer_length;
		remaining_length -= xfer_length;
	}

	syna_pal_mutex_unlock(rw_mutex);

	if (tcm_msg->response_code != STATUS_ACK)
		goto exit;

	/* disable write-then-read operation when running bootloader command */
	if (command == CMD_RUN_BOOTLOADER_FIRMWARE) {
		if (tcm_msg->write_then_read_support)
			tcm_msg->write_then_read_support = false;
	}

	/* process the command response either in polling or by ATTN */
	timeout = 0;
	do {
		if (in_polling) {
			timeout += resp_reading;
			syna_pal_sleep_ms(resp_reading);
		} else {
			timeout += tcm_msg->command_timeout_time >> 2;
			syna_tcm_v2_wait_for_attn(tcm_dev, tcm_msg->command_timeout_time);
		}

		/* stop the processing if terminated */
		if (ATOMIC_GET(tcm_msg->command_status) == CMD_STATE_TERMINATED) {
			retval = 0;
			goto exit;
		}

		/* whatever the way of processing, attempt to read in a message if not completed */
		if (ATOMIC_GET(tcm_msg->command_status) == CMD_STATE_BUSY) {
			retval = syna_tcm_v2_read_message(tcm_dev, NULL);
			if (retval < 0)
				continue;
		}

		/* break the loop if the valid response was retrieved */
		if (ATOMIC_GET(tcm_msg->command_status) != CMD_STATE_BUSY)
			break;

	} while (timeout < tcm_msg->command_timeout_time);


	if (ATOMIC_GET(tcm_msg->command_status) != CMD_STATE_IDLE) {
		if (timeout >= tcm_msg->command_timeout_time) {
			LOGE("Timed out wait for response of command 0x%02X (%dms)\n",
				command, tcm_msg->command_timeout_time);
			retval = -ERR_TIMEDOUT;
		} else {
			LOGE("Fail to get valid response 0x%02X of command 0x%02X\n",
				tcm_msg->status_report_code, command);
			retval = -ERR_TCMMSG;
		}
		goto exit;
	}

	tcm_msg->response_code = tcm_msg->status_report_code;
	retval = 0;

exit:
	/* copy response code to the caller */
	if (resp_code)
		*resp_code = tcm_msg->response_code;

	tcm_msg->command = CMD_NONE;

	/* recovery the irq only when running in polling mode
	 * and irq has been disabled previously
	 */
	if (in_polling && irq_disabled)
		syna_tcm_enable_irq(tcm_dev, true);

	ATOMIC_SET(tcm_msg->command_status, CMD_STATE_IDLE);
	ATOMIC_SET(tcm_dev->command_processing, 0);

	syna_pal_mutex_unlock(cmd_mutex);

	return retval;
}
/*
 *  Process the startup packet of TouchComm V2 firmware
 *
 *  For TouchComm v2 protocol, each packet must have a valid crc-6.
 *  If so, send an identify command to identify the device and complete
 *  the pre-initialization.
 *
 * param
 *    [ in] tcm_dev:  pointer to TouchComm device
 *    [ in] bypass:   flag to bypass the detection
 *    [ in] do_reset: flag to issue a reset if falling down to error
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
int syna_tcm_v2_detect(struct tcm_dev *tcm_dev, bool bypass, bool do_reset)
{
	int retval;
	unsigned char *data;
	unsigned int info_size = (unsigned int)sizeof(struct tcm_identification_info);
	struct tcm_message_data_blob *tcm_msg = NULL;
	unsigned char resp_code = 0;
	unsigned char command;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return -ERR_INVAL;
	}

	tcm_dev->msg_data.has_crc = true;
	tcm_dev->msg_data.has_extra_rc = false;

	if (bypass)
		goto set_ops;

	tcm_msg = &tcm_dev->msg_data;

	syna_pal_mutex_lock(&tcm_msg->rw_mutex);
	syna_tcm_buf_lock(&tcm_msg->in);

	retval = syna_tcm_buf_alloc(&tcm_msg->in, info_size + MESSAGE_HEADER_SIZE);
	if (retval < 0) {
		LOGE("Fail to allocate memory for buf_in\n");
		syna_tcm_buf_unlock(&tcm_msg->in);
		syna_pal_mutex_unlock(&tcm_msg->rw_mutex);
		return retval;
	}

	data = tcm_msg->in.buf;

	syna_tcm_buf_unlock(&tcm_msg->in);
	syna_pal_mutex_unlock(&tcm_msg->rw_mutex);

	tcm_dev->msg_data.retry_cmd_cnt = COMMAND_V2_RETRY_TIMES;
	/* at startup, the identify report is supposed to be ready */
	tcm_dev->msg_data.retry_resp_cnt = 0;

	/* the identify report should be the first packet at startup
	 * otherwise, send the command to identify
	 */
	retval = syna_tcm_v2_read_message(tcm_dev, &resp_code);
	if ((retval < 0) || ((resp_code == STATUS_ERROR) && (tcm_msg->in.buf[3] == 0x5A))) {
		/* perhaps, the returned packet is not crc-enabled, resend the command to try */
		tcm_dev->msg_data.has_crc = false;
		retval = syna_tcm_v2_read_message(tcm_dev, &resp_code);
	}
	if (resp_code != REPORT_IDENTIFY) {
		command = (do_reset) ? CMD_RESET : CMD_IDENTIFY;
		retval = syna_tcm_v2_write_message(tcm_dev, command, NULL, 0, &resp_code, tcm_msg->command_polling_time);
		if (retval < 0) {
			LOGE("Fail to identify at startup\n");
			return -ERR_TCMMSG;
		}
	}

	/* parse the identify info packet if needed */
	if (tcm_dev->dev_mode == MODE_UNKNOWN) {
		syna_tcm_buf_lock(&tcm_msg->in);
		retval = syna_tcm_v2_parse_idinfo(tcm_dev,
			(unsigned char *)&data[MESSAGE_HEADER_SIZE],
			tcm_msg->in.buf_size, info_size);
		syna_tcm_buf_unlock(&tcm_msg->in);
		if (retval < 0) {
			LOGE("Fail to parse identify report at startup\n");
			return -ERR_TCMMSG;
		}
	}

	LOGI("TouchComm v2 detected\n");
	LOGI("Support of CRC and Seq-bit (%s)\n", (tcm_msg->has_crc) ? "yes" : "no");

set_ops:
	tcm_dev->read_message = syna_tcm_v2_read_message;
	tcm_dev->write_message = syna_tcm_v2_write_message;
	tcm_dev->terminate = syna_tcm_v2_terminate;

	tcm_dev->msg_data.retry_resp_cnt = RESPONSE_V2_RETRY_TIMES;

	tcm_dev->msg_data.predict_length = 0;
	tcm_dev->protocol = TOUCHCOMM_V2;

	return 0;
}


#endif /* end of TOUCHCOMM_VERSION_2 */
