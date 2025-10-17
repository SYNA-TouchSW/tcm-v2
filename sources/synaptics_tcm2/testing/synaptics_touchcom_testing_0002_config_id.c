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
 * This file implements the testing of configuration ID checking.
 *
 *        Limits:
 *            1. configuration ID to compare with
 *
 *        Result data returned (Optional):
 *            1. configuration ID in device
 *
 */


#include "synaptics_touchcom_testing.h"

#ifdef _TESTING_ID_0002

#define VERSION_TESTING_0002 (2)


/* private function to check the config id
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
static bool syna_tcm_testing_0002_check_config_id(char *config_id, int size_id,
	char *limit, int size_limit)
{
	bool result = false;
	int i;

	if (!config_id || !limit)
		return false;

	if (size_limit == 0)
		return false;

	result = true;
	for (i = 0; i <= size_limit; i++) {
		if (config_id[i] != limit[i]) {
			LOGD("Data mismatched at the byte %d, data:%02X (limit:%02X)\n", i, config_id[i], limit[i]);
			result = false;
			break;
		}
	}
	if (!result)
		TEST_ERROR("Config ID mismatched, FW: %s (limit: %s)\n", config_id, limit);

	return result;
}

/* test to check the correctness of config ID
 *
 * param
 *    [ in] tcm_dev:          pointer to TouchComm device
 *    [ in] testing_data:     metadata of testing item
 *    [ in] skip_comparison:  flag to skip data comparison if needed
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_testing_config_id(struct tcm_dev *tcm_dev,
	struct testing_item *testing_data, bool skip_comparison)
{
	int retval = -TEST_NO_START;
	struct tcm_application_info info;
	char *p_config_id = NULL;
	int size_of_config_id;

	if (!tcm_dev || (!testing_data))
		return -TEST_INVALID_PARAMETERS;

	LOGD("Start testing\n");

	retval = syna_tcm_get_app_info(tcm_dev, &info, 0);
	if (retval < 0) {
		LOGE("Fail to get application info\n");
		testing_data->result = false;
		retval = -TEST_ERROR_ON_COMMAND_PROCESSING;
		goto exit;
	}

	p_config_id = (char *)info.customer_config_id;
	size_of_config_id = sizeof(info.customer_config_id);

	/* copy the result data to caller */
	if (testing_data->result_data[0]) {
		if (syna_tcm_buf_alloc(testing_data->result_data[0], size_of_config_id) >= 0) {
			syna_pal_mem_cpy(testing_data->result_data[0]->buf,
				testing_data->result_data[0]->buf_size,
				info.customer_config_id, size_of_config_id, size_of_config_id);
			testing_data->result_data[0]->data_length = size_of_config_id;
		}
	}

	if (skip_comparison) {
		testing_data->result = true;
		goto exit;
	}

	/* data comparison */
	if (testing_data->limit[0]) {
		if (testing_data->limit[0]->size > 0)
			testing_data->result = syna_tcm_testing_0002_check_config_id(p_config_id,
				size_of_config_id, (char *)testing_data->limit[0]->value, testing_data->limit[0]->size);
	}

	/* generate result */
	if (!testing_data->result)
		retval = -TEST_ERROR_ON_DATA_VERIFYING;

exit:
	LOGI("Result = %s\n", (testing_data->result) ? "pass" : "fail");

	return retval;
}

/*
 * Declare the testing item
 */
static struct testing_item test_0002 = {
	.version = VERSION_TESTING_0002,
	.test_id = TEST_ID_0002,
	.title = (const char *)"Configuration ID Test",
	.do_test = syna_tcm_testing_config_id,
};


/* helper to get the declaration of testing item
 *
 * param
 *   void
 *
 * return
 *    the structure of testing item
 */
struct testing_item *syna_tcm_get_testing_0002(void)
{
	return &test_0002;
}


#endif  /* end of _TESTING_ID_0002 */
