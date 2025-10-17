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
 * This file implements the testing of delta noise (PID$0A).
 *
 *        Limits:
 *            1. upper bound of data
 *            2. lower bound of data
 *
 *        Result data returned (Optional):
 *            data of PID$0A
 *
 */


#include "synaptics_touchcom_testing.h"

#ifdef _TESTING_ID_0A00

#define VERSION_TESTING_0A00 (1)


/* private functions to check the data
 *
 * return
 *    'true' in case of success, 'false' otherwise.
 */
static bool syna_tcm_testing_0A00_check_upper_bound(void *data, void *limit, int col, int row)
{
	if (*(short *)data > *(short *)limit) {
		TEST_ERROR("Fail on (rows:%2d,cols:%2d)=%5d, limit(upper bound):%4d\n",
			row, col, *(short *)data, *(short *)limit);
		return false;
	}
	return true;
}
static bool syna_tcm_testing_0A00_check_lower_bound(void *data, void *limit, int col, int row)
{
	if (*(short *)data < *(short *)limit) {
		TEST_ERROR("Fail on (rows:%2d,cols:%2d)=%5d, limit(lower bound):%4d\n",
			row, col, *(short *)data, *(short *)limit);
		return false;
	}
	return true;
}
/* test to perform delta noise testing
 *
 * param
 *    [ in] tcm_dev:          pointer to TouchComm device
 *    [ in] testing_data:     metadata of testing item
 *    [ in] skip_comparison:  flag to skip data comparison if needed
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
static int syna_tcm_testing_noise(struct tcm_dev *tcm_dev,
	struct testing_item *testing_data, bool skip_comparison)
{
	int retval = -TEST_NO_START;
	bool result_1, result_2;
	struct tcm_buffer tdata;
	unsigned int cols, rows;

	if (!tcm_dev || (!testing_data))
		return -TEST_INVALID_PARAMETERS;

	syna_tcm_buf_init(&tdata);

	LOGD("Start testing\n");

	cols = tcm_dev->cols;
	rows = tcm_dev->rows;

	retval = syna_tcm_run_production_test(tcm_dev, TEST_PID10_DELTA_NOISE, &tdata, 0);
	if (retval < 0) {
		LOGE("Fail to run test PID%d\n", TEST_PID10_DELTA_NOISE);
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
	result_1 = (!testing_data->limit[0]) ? false :
		syna_tcm_testing_check_frame_data(tdata.buf, tdata.data_length,
			sizeof(short), rows, cols, syna_tcm_testing_0A00_check_upper_bound,
			testing_data->limit[0]->value, testing_data->limit[0]->size);

	result_2 = (!testing_data->limit[1]) ? false :
		syna_tcm_testing_check_frame_data(tdata.buf, tdata.data_length,
			sizeof(short), rows, cols, syna_tcm_testing_0A00_check_lower_bound,
			testing_data->limit[1]->value, testing_data->limit[1]->size);

	/* generate result */
	testing_data->result = (result_1 && result_2);
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
static struct testing_item test_0A00 = {
	.version = VERSION_TESTING_0A00,
	.test_id = TEST_ID_0A00,
	.title = (const char *)"Noise Test",
	.do_test = syna_tcm_testing_noise,
};


/* helper to get the declaration of testing item
 *
 * param
 *   void
 *
 * return
 *    the structure of testing item
 */
struct testing_item *syna_tcm_get_testing_0A00(void)
{
	return &test_0A00;
}


#endif  /* end of _TESTING_ID_0A00 */
