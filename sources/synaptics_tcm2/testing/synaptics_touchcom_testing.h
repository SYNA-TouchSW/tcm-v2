/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Synaptics TouchComm Production Testing
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
 * synaptics_touchcom_testing.h
 *
 * This file declares the mapping table for production test.
 * Each entry has an unique ID and represents a test item.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _SYNAPTICS_TOUCHCOM_TESTING_MODULE_H_
#define _SYNAPTICS_TOUCHCOM_TESTING_MODULE_H_

#include "../syna_tcm2_testing_items.h"

#define TEST_ERROR(log, ...) LOGE(log, ##__VA_ARGS__)


#define DEFINE_TESTING_ITEM(ID)                       \
	struct testing_item *syna_tcm_get_testing_##ID(void); \
	enum { TEST_ID_##ID = 0x##ID };


#define MAX_BUFFERS (20)

/*
 * Command codes for testing
 */
enum tcm_test_commands {
	TEST_NOT_IMPLEMENTED = 0x00,

	TEST_PID01_TRX_TRX_SHORTS = 0x01,
	TEST_PID02_TRX_SENSOR_OPENS = 0x02,
	TEST_PID03_TRX_GROUND_SHORTS = 0x03,
	TEST_PID04_GPIO_SHORTS = 0x04,
	TEST_PID05_FULL_RAW_CAP = 0x05,
	TEST_PID07_TDDI_DYNAMIC_RANGE = 0x07,
	TEST_PID08_HIGH_RESISTANCE = 0x08,
	TEST_PID09_TX_GROUP = 0x09,
	TEST_PID10_DELTA_NOISE = 0x0A,
	TEST_PID16_SENSOR_SPEED = 0x10,
	TEST_PID17_ADC_RANGE = 0x11,
	TEST_PID18_HYBRID_ABS_RAW = 0x12,
	TEST_PID19_GPIO_OPEN = 0x13,
	TEST_PID20_SYNC_PIN_SHORT = 0x14,
	TEST_PID22_TRANS_CAP_RAW = 0x16,
	TEST_PID25_TRANS_RX_SHORT = 0x19,
	TEST_PID26_HYBRID_ABS_W_CBC = 0x1A,
	TEST_PID29_HYBRID_ABS_NOISE = 0x1D,
	TEST_PID71_HYBRID_RAW_CAP = 0x47,

	TEST_PID_MAX,
};

/*
 * Base description of each testing item
 */
struct testing_limit {
	void *value;
	unsigned int size;
};

struct testing_item {
	/* revision code */
	unsigned int version;
	/* provide the title for the test */
	unsigned int test_id;
	const char *title;

	/* test result */
	bool result;

	/* helper to trigger the testing
	 *
	 * param
	 *    [ in] tcm_dev:          pointer to TouchComm device
	 *    [ in] testing_data:     metadata of testing item
	 *    [ in] skip_comparison:  flag to skip data comparison if needed
	 *
	 * return
	 *    0 or positive value in case of success, a negative value otherwise.
	 */
	int (*do_test)(struct tcm_dev *tcm_dev, struct testing_item *testing_data,
		bool skip_comparison);

	/* additional info to test */
	unsigned int frame_rows;
	unsigned int frame_cols;
	unsigned int multiple_frames;
	unsigned int delay_ms;
	void *ref_data;

	/* limit for the verification, set 'NULL' if not needed */
	struct testing_limit *limit[MAX_BUFFERS];

	/* buffer to store the result data, set 'NULL' if not needed */
	struct tcm_buffer *result_data[MAX_BUFFERS];
};
/* end of structure testing_item */


/* Definitions of test ID
 *
 *  Typically, test ID is assembled by two bytes.
 *
 *            [ 15: 8 ] [ 7:0 ]
 *             PID/RID
 *
 * Bits [ 15:8 ] shall be the PID code being sent to the touchcomm firmware,
 * and then others are reserved for the developer's implementation that is
 * used to easily mange the different requests or changes for the testing.
 *
 * For example, PID$05 is the generic test for raw cap. which ID is set to 0x0500;
 * while, the other test using PID$05 could be assigned to 0x0501.
 *
 * Additionally, the group of 0x00 is reserved for these items w/o PID,
 * e.g. Firmware ID tes, RESET pin test, etc.
 *
 */

#define TEST_ITEM_NONE (0x0000)
/* NEW ITEM should be added at this line below */

/* firmware/device/config ID test */
#ifdef _TESTING_ID_0001
DEFINE_TESTING_ITEM(0001)
#endif
#ifdef _TESTING_ID_0002
DEFINE_TESTING_ITEM(0002)
#endif
/* attention (interrupt)/reset pin test */
#ifdef _TESTING_ID_0003
DEFINE_TESTING_ITEM(0003)
#endif
#ifdef _TESTING_ID_0004
DEFINE_TESTING_ITEM(0004)
#endif
#ifdef _TESTING_ID_0005
DEFINE_TESTING_ITEM(0005)
#endif
/* lockdown test */
#ifdef _TESTING_ID_0006
DEFINE_TESTING_ITEM(0006)
#endif
/* config crc test */
#ifdef _TESTING_ID_0007
DEFINE_TESTING_ITEM(0007)
#endif
/* PID$01 test */
#ifdef _TESTING_ID_0100
DEFINE_TESTING_ITEM(0100)
#endif
/* PID$02 test */
#ifdef _TESTING_ID_0200
DEFINE_TESTING_ITEM(0200)
#endif
/* PID$03 test */
#ifdef _TESTING_ID_0300
DEFINE_TESTING_ITEM(0300)
#endif
/* PID$04 test */
#ifdef _TESTING_ID_0400
DEFINE_TESTING_ITEM(0400)
#endif
/* PID$05 test */
#ifdef _TESTING_ID_0500
DEFINE_TESTING_ITEM(0500)
#endif
#ifdef _TESTING_ID_0501
DEFINE_TESTING_ITEM(0501)
#endif
#ifdef _TESTING_ID_0502
DEFINE_TESTING_ITEM(0502)
#endif
#ifdef _TESTING_ID_0503
DEFINE_TESTING_ITEM(0503)
#endif
/* PID$07 test */
#ifdef _TESTING_ID_0700
DEFINE_TESTING_ITEM(0700)
#endif
/* PID$08 test */
#ifdef _TESTING_ID_0800
DEFINE_TESTING_ITEM(0800)
#endif
/* PID$09 test */
#ifdef _TESTING_ID_0900
DEFINE_TESTING_ITEM(0900)
#endif
/* PID$0A test */
#ifdef _TESTING_ID_0A00
DEFINE_TESTING_ITEM(0A00)
#endif
#ifdef _TESTING_ID_0A01
DEFINE_TESTING_ITEM(0A01)
#endif
/* PID$10 test */
#ifdef _TESTING_ID_1000
DEFINE_TESTING_ITEM(1000)
#endif
/* PID$11 test */
#ifdef _TESTING_ID_1100
DEFINE_TESTING_ITEM(1100)
#endif
/* PID$12 test */
#ifdef _TESTING_ID_1200
DEFINE_TESTING_ITEM(1200)
#endif
/* PID$13 test */
#ifdef _TESTING_ID_1300
DEFINE_TESTING_ITEM(1300)
#endif
/* PID$14 test */
#ifdef _TESTING_ID_1400
DEFINE_TESTING_ITEM(1400)
#endif
/* PID$16 test */
#ifdef _TESTING_ID_1600
DEFINE_TESTING_ITEM(1600)
#endif
/* PID$19 test */
#ifdef _TESTING_ID_1900
DEFINE_TESTING_ITEM(1900)
#endif
#ifdef _TESTING_ID_1901
DEFINE_TESTING_ITEM(1901)
#endif
/* PID$1A test */
#ifdef _TESTING_ID_1A00
DEFINE_TESTING_ITEM(1A00)
#endif
/* PID$1D test */
#ifdef _TESTING_ID_1D00
DEFINE_TESTING_ITEM(1D00)
#endif
/* PID$1E test */
#ifdef _TESTING_ID_1E01
DEFINE_TESTING_ITEM(1E01)
#endif
/* PID$47 test */
#ifdef _TESTING_ID_4700
DEFINE_TESTING_ITEM(4700)
#endif
#ifdef _TESTING_ID_4701
DEFINE_TESTING_ITEM(4701)
#endif

/* NEW ITEM should be defined at this line above */
#define TEST_ITEM_MAX (0xFFFF)




/* helper to check the result data
 *
 * param
 *    [ in] data:         data buffer to check
 *    [ in] data_size:    size of total data buffer
 *    [ in] element_size: size of each element
 *    [ in] rows:         number of rows
 *    [ in] cols:         number of columns
 *    [ in] cb:           callback function to do data comparison
 *    [ in] limit:        buffer of limit
 *    [ in] limit_size:   size of total limit buffer
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
typedef bool (*test_data_comparison_callback_t) (void *data, void *limit, int pos_info1, int pos_info2);

static inline bool syna_tcm_testing_check_frame_data(void *data, size_t data_size, size_t element_size,
	int rows, int cols, test_data_comparison_callback_t cb, void *limit, size_t limit_size)
{
	bool result;
	unsigned char *data_ptr;
	unsigned char *limit_ptr;
	int i, j, pos;
	unsigned int expected_size = (unsigned int)(element_size * rows * cols);

	if (data == NULL || limit == NULL || cb == NULL) {
		TEST_ERROR("Invalid parameters to check frame data\n");
		return false;
	}

	if (data_size < expected_size) {
		TEST_ERROR("Data size mismatched, input:%d (expected:%d)\n", (int)data_size, expected_size);
		return false;
	}

	data_ptr = (unsigned char *)data;
	limit_ptr = (unsigned char *)limit;

	result = true;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			pos = (int)(i * element_size * cols + (j * element_size));
			if (!cb(&data_ptr[pos], (limit_size >= data_size) ? &limit_ptr[pos] : &limit_ptr[0], j, i))
				result = false;
		}
	}
	return result;
}
static inline bool syna_tcm_testing_check_array_data(void *data, size_t data_size, size_t element_size,
	int rows, int cols, test_data_comparison_callback_t cb, void *limit, size_t limit_size)
{
	bool result;
	unsigned char *data_ptr;
	unsigned char *limit_ptr;
	int i, pos;

	if (data == NULL || limit == NULL || cb == NULL) {
		TEST_ERROR("Invalid parameters to check frame data\n");
		return false;
	}

	data_ptr = (unsigned char *)data;
	limit_ptr = (unsigned char *)limit;

	result = true;
	for (i = 0; i < (int)(data_size / element_size); i++) {
		pos = (int)(i * element_size);
		if (!cb(&data_ptr[pos], (limit_size >= data_size) ? &limit_ptr[pos] : &limit_ptr[0], i, cols))
			result = false;
	}
	return result;
}


/* helper to return the result string associated with the given code
 *
 * param
 *    [ in] code: error code
 *
 * return
 *    string
 */
enum testing_errors {
	TEST_NO_START = 0xA0,
	TEST_INVALID_PARAMETERS,
	TEST_ERROR_ON_DATA_VERIFYING,
	TEST_ERROR_ON_COMMAND_PROCESSING,
	TEST_ERROR_ON_MEMORY,
};

static inline const char *syna_tcm_testing_error_string(int code)
{
	switch (code) {
	case TEST_NO_START:
		return "Test no start";
	case TEST_INVALID_PARAMETERS:
		return "Invalid parameters to test";
	case TEST_ERROR_ON_DATA_VERIFYING:
		return "Fail on data verification";
	case TEST_ERROR_ON_COMMAND_PROCESSING:
		return "Fail on command processing";
	case TEST_ERROR_ON_MEMORY:
		return "Fail on memory allocation or data copying";
	default:
		return "Failure";
	}
	return "";
}

#endif /* end of _SYNAPTICS_TOUCHCOM_TESTING_MODULE_H_ */


#ifdef __cplusplus
}
#endif /* __cplusplus */
