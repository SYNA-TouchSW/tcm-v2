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
 * This file implements the testing of trx-trx short (PID$01).
 *
 *        Limits:
 *            1. limits by channel
 *
 *        Result data returned (Optional):
 *            data of PID$01
 *
 */


#include "synaptics_touchcom_testing.h"

#ifdef _TESTING_ID_0100

#define VERSION_TESTING_0100 (3)



/* private functions to check the data
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
static bool syna_tcm_testing_0100_check_data(void *data, void *limit, int pos_x, int pos_y)
{
	bool result = true;
	unsigned char value = *(char *)data;
	unsigned char v, l;
	int i;

	for (i = 0; i < 8; i++) {
		v = GET_BIT(value, i);
		l = GET_BIT(*(unsigned char *)limit, i);
		if (v != l) {
			TEST_ERROR("Fail on TRX-%03d (data:%X, limit:%X)\n", (pos_x * 8 + i), v, l);
			result = false;
		}
	}
	return result;
}
/* test to perform trx-trx short testing
 *
 * param
 *    [ in] tcm_dev:          pointer to TouchComm device
 *    [ in] testing_data:     metadata of testing item
 *    [ in] skip_comparison:  flag to skip data comparison if needed
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_testing_trx_trx_short(struct tcm_dev *tcm_dev,
	struct testing_item *testing_data, bool skip_comparison)
{
	int retval = -TEST_NO_START;
	struct tcm_buffer tdata;

	if (!tcm_dev || (!testing_data))
		return -TEST_INVALID_PARAMETERS;

	syna_tcm_buf_init(&tdata);

	LOGD("Start testing\n");

	retval = syna_tcm_run_production_test(tcm_dev, TEST_PID01_TRX_TRX_SHORTS, &tdata, 0);
	if (retval < 0) {
		LOGE("Fail to run test PID%d\n", TEST_PID01_TRX_TRX_SHORTS);
		testing_data->result = false;
		retval = -TEST_ERROR_ON_COMMAND_PROCESSING;
		goto exit;
	}

	/* copy the result data to caller */
	if (testing_data->result_data[0])
		syna_tcm_buf_copy(testing_data->result_data[0], &tdata);

	if (skip_comparison) {
		testing_data->result = true;
		goto exit;
	}

	/* data comparison */
	testing_data->result = (!testing_data->limit[0]) ? false :
		syna_tcm_testing_check_array_data(tdata.buf, tdata.data_length,
			sizeof(char), 0, 0, syna_tcm_testing_0100_check_data,
			testing_data->limit[0]->value, testing_data->limit[0]->size);

	/* generate result */
	if (!testing_data->result)
		retval = -TEST_ERROR_ON_DATA_VERIFYING;

exit:
	LOGI("Result = %s\n", (testing_data->result) ? "pass" : "fail");

	syna_tcm_buf_release(&tdata);

	return retval;
}

/*
 * Declare the testing item
 */
static struct testing_item test_0100 = {
	.version = VERSION_TESTING_0100,
	.test_id = TEST_ID_0100,
	.title = (const char *)"TRx-TRx Shorts Test",
	.do_test = syna_tcm_testing_trx_trx_short,
};


/* helper to get the declaration of testing item
 *
 * param
 *   void
 *
 * return
 *    the structure of testing item
 */
struct testing_item *syna_tcm_get_testing_0100(void)
{
	return &test_0100;
}


#endif  /* end of _TESTING_ID_0100 */
