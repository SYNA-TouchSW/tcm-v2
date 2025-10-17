// SPDX-License-Identifier: GPL-2.0-or-later
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
 * This file implements the testing of firmware/device ID checking.
 *
 *        Limits:
 *            1. build ID / firmware ID to compare with
 *            2. string of device ID to compare with
 *
 *        Result data returned (Optional):
 *            1. build ID / firmware ID
 *            2. string of device ID
 *
 */


#include "synaptics_touchcom_testing.h"

#ifdef _TESTING_ID_0001

#define VERSION_TESTING_0001 (1)


/* private functions to check the data
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
static bool syna_tcm_testing_0001_check_device_id(char *dev_id, int size_id,
	char *limit, int size_limit)
{
	bool result = false;
	int i;

	if (!dev_id || !limit)
		return false;

	for (i = 0; i <= (int)(size_id - size_limit); i++) {
		if (dev_id[i] == (unsigned char)limit[0]) {
			if (syna_pal_str_cmp((const char *)&dev_id[i], (const char *)limit, size_limit) == 0) {
				result = true;
				goto exit;
			}
		}
		result = false;
	}
	if (!result)
		TEST_ERROR("Device ID mismatched, FW: %s (limit: %s)\n", dev_id, limit);
exit:
	return result;
}

/* private function to check the build id
 *
 * param
 *    [ in] build_id: id to be validated
 *    [ in] limit:    data for testing
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
static bool syna_tcm_testing_0001_check_build_id(unsigned int build_id, unsigned int limit)
{
	bool result;

	result = (limit == build_id);
	if (!result)
		TEST_ERROR("Firmware ID mismatched, FW: %d (limit: %d)\n", build_id, limit);

	return result;
}

/* test to check the correctness of device ID, build ID, or other identification information
 *
 * param
 *    [ in] tcm_dev:          pointer to TouchComm device
 *    [ in] testing_data:     metadata of testing item
 *    [ in] skip_comparison:  flag to skip data comparison if needed
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_testing_build_id(struct tcm_dev *tcm_dev,
	struct testing_item *testing_data, bool skip_comparison)
{
	int retval = -TEST_NO_START;
	bool ret_1 = true, ret_2 = true;
	struct tcm_identification_info info;
	char *p_dev_id = NULL;
	unsigned int build_id;

	if (!tcm_dev || (!testing_data))
		return -TEST_INVALID_PARAMETERS;

	LOGD("Start testing\n");

	retval = syna_tcm_identify(tcm_dev, &info, 0);
	if (retval < 0) {
		LOGE("Fail to get identification\n");
		testing_data->result = false;
		retval = -TEST_ERROR_ON_COMMAND_PROCESSING;
		goto exit;
	}

	build_id = syna_pal_le4_to_uint(info.build_id);
	p_dev_id = (char *)info.part_number;

	/* copy the result data to caller */
	if (testing_data->result_data[0]) {
		if (syna_tcm_buf_alloc(testing_data->result_data[0], sizeof(unsigned int)) >= 0) {
			syna_pal_mem_cpy(testing_data->result_data[0]->buf,
				testing_data->result_data[0]->buf_size,
				&build_id,
				sizeof(unsigned int),
				sizeof(unsigned int));
			testing_data->result_data[0]->data_length = sizeof(unsigned int);
		}
	}
	if (testing_data->result_data[1]) {
		if (syna_tcm_buf_alloc(testing_data->result_data[1], sizeof(info.part_number)) >= 0) {
			syna_pal_mem_cpy(testing_data->result_data[1]->buf,
				testing_data->result_data[1]->buf_size,
				info.part_number,
				sizeof(info.part_number),
				sizeof(info.part_number));
			testing_data->result_data[1]->data_length = sizeof(info.part_number);
		}
	}

	if (skip_comparison) {
		testing_data->result = true;
		goto exit;
	}

	/* data comparison */
	if (testing_data->limit[0]) {
		if (testing_data->limit[0]->size >= sizeof(unsigned int))
			ret_1 = syna_tcm_testing_0001_check_build_id(build_id,
					*(unsigned int *)testing_data->limit[0]->value);
	}

	if (testing_data->limit[1]) {
		if (testing_data->limit[1]->size > 0)
			ret_2 = syna_tcm_testing_0001_check_device_id(p_dev_id,
				(int)sizeof(info.part_number), (char *)testing_data->limit[1]->value,
				testing_data->limit[1]->size);
	}

	/* generate result */
	testing_data->result = ret_1 && ret_2;
	if (!testing_data->result)
		retval = -TEST_ERROR_ON_DATA_VERIFYING;

exit:
	LOGI("Result = %s\n", (testing_data->result) ? "pass" : "fail");

	return retval;
}

/*
 * Declare the testing item
 */
static struct testing_item test_0001 = {
	.version = VERSION_TESTING_0001,
	.test_id = TEST_ID_0001,
	.title = (const char *)"Firmware/Device ID Test",
	.do_test = syna_tcm_testing_build_id,
};


/* helper to get the declaration of testing item
 *
 * param
 *   void
 *
 * return
 *    the structure of testing item
 */
struct testing_item *syna_tcm_get_testing_0001(void)
{
	return &test_0001;
}


#endif  /* end of _TESTING_ID_0001 */
