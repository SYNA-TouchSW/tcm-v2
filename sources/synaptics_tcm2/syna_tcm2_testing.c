// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Synaptics TouchComm touchscreen driver
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
 * This file implements the sample code to perform the production testing.
 */

#include "syna_tcm2.h"
#include "syna_tcm2_testing_items.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_base.h"

#include "testing/synaptics_touchcom_testing.h"

#ifdef HAS_TESTING_FEATURE


/* #define SHOW_TEST_RESULT_DATA */

/*
 * Example to trigger the PT0A (PID10) testing.
 *
 * param
 *    [ in] kobj:  pointer to kernel object
 *    [ in] attr:  pointer to kernel attribute
 *    [out] buf:  string buffer shown on console
 *
 * return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt0a_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int retval = 0;
	unsigned int count = 0;
	struct syna_tcm *tcm;
	struct device *p_dev;
	struct testing_item *item = NULL;
	struct testing_limit limit_max;
	struct testing_limit limit_min;
#ifdef SHOW_TEST_RESULT_DATA
	struct tcm_buffer result_data;
	short *data_ptr = NULL;
	int i, j;
#endif

	p_dev = container_of(kobj->parent->parent, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (!tcm->is_connected)
		return scnprintf(buf + count, PAGE_SIZE, "Device is NOT connected\n");

	count = 0;

	item = syna_tcm_get_testing_0A00();
	if (!item)
		return scnprintf(buf + count, PAGE_SIZE - count,
				"Invalid testing item id:%d\n", TEST_ID_0A00);

	item->frame_cols = tcm->tcm_dev->cols;
	item->frame_rows = tcm->tcm_dev->rows;

#ifdef SHOW_TEST_RESULT_DATA
	syna_tcm_buf_init(&result_data);
	item->result_data[0] = &result_data;
#endif

	limit_max.value = (void *)pt0a_hi_limits;
	limit_max.size = (unsigned int)sizeof(pt0a_hi_limits);
	item->limit[0] = &limit_max;

	limit_min.value = (void *)pt0a_lo_limits;
	limit_min.size = (unsigned int)sizeof(pt0a_lo_limits);
	item->limit[1] = &limit_min;

	retval = item->do_test(tcm->tcm_dev, item, false);
	if (retval < 0)
		LOGE("Fail to run test, %s\n", item->title);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"\n%s (version.%d): %s\n\n", item->title, item->version,
			((retval < 0) || (!item->result)) ? "Fail" : "Pass");

#ifdef SHOW_TEST_RESULT_DATA
	/* print out the result data */
	if (result_data.data_length > 0) {
		data_ptr = (short *)&(result_data.buf[0]);
		for (i = 0; i < item->frame_rows; i++) {
			for (j = 0; j < item->frame_cols; j++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
						data_ptr[i * tcm->tcm_dev->cols + j]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	}

	syna_tcm_buf_release(&result_data);
#endif

	return count;
}

static struct kobj_attribute kobj_attr_pt0a =
	__ATTR(pt0a, 0444, syna_testing_pt0a_show, NULL);

/*
 * Example to trigger the PT05 (PID05) testing.
 *
 * param
 *    [ in] kobj:  pointer to kernel object
 *    [ in] attr:  pointer to kernel attribute
 *    [out] buf:  string buffer shown on console
 *
 * return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt05_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int retval = 0;
	unsigned int count = 0;
	struct syna_tcm *tcm;
	struct device *p_dev;
	struct testing_item *item = NULL;
	struct testing_limit limit_max;
	struct testing_limit limit_min;
#ifdef SHOW_TEST_RESULT_DATA
	struct tcm_buffer result_data;
	unsigned short *data_ptr = NULL;
	int i, j;
#endif

	p_dev = container_of(kobj->parent->parent, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (!tcm->is_connected)
		return scnprintf(buf + count, PAGE_SIZE, "Device is NOT connected\n");

	count = 0;

	item = syna_tcm_get_testing_0500();
	if (!item)
		return scnprintf(buf + count, PAGE_SIZE - count,
				"Invalid testing item id:%d\n", TEST_ID_0500);

	item->frame_cols = tcm->tcm_dev->cols;
	item->frame_rows = tcm->tcm_dev->rows;

#ifdef SHOW_TEST_RESULT_DATA
	syna_tcm_buf_init(&result_data);
	item->result_data[0] = &result_data;
#endif

	limit_max.value = (void *)pt05_hi_limits;
	limit_max.size = (unsigned int)sizeof(pt05_hi_limits);
	item->limit[0] = &limit_max;

	limit_min.value = (void *)pt05_lo_limits;
	limit_min.size = (unsigned int)sizeof(pt05_lo_limits);
	item->limit[1] = &limit_min;

	retval = item->do_test(tcm->tcm_dev, item, false);
	if (retval < 0)
		LOGE("Fail to run test, %s\n", item->title);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"\n%s (version.%d): %s\n\n", item->title, item->version,
			((retval < 0) || (!item->result)) ? "Fail" : "Pass");

#ifdef SHOW_TEST_RESULT_DATA
	/* print out the result data */
	if (result_data.data_length > 0) {
		data_ptr = (unsigned short *)&(result_data.buf[0]);
		for (i = 0; i < item->frame_rows; i++) {
			for (j = 0; j < item->frame_cols; j++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
						data_ptr[i * tcm->tcm_dev->cols + j]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	}

	syna_tcm_buf_release(&result_data);
#endif

	return count;
}

static struct kobj_attribute kobj_attr_pt05 =
	__ATTR(pt05, 0444, syna_testing_pt05_show, NULL);

/*
 * Example to trigger the PT01 (PID01) testing.
 *
 * param
 *    [ in] kobj:  pointer to kernel object
 *    [ in] attr:  pointer to kernel attribute
 *    [out] buf:  string buffer shown on console
 *
 * return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt01_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int retval = 0;
	unsigned int count = 0;
	struct syna_tcm *tcm;
	struct device *p_dev;
	struct testing_item *item = NULL;
	struct testing_limit limit;
#ifdef SHOW_TEST_RESULT_DATA
	struct tcm_buffer result_data;
	int i;
#endif

	p_dev = container_of(kobj->parent->parent, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (!tcm->is_connected)
		return scnprintf(buf + count, PAGE_SIZE, "Device is NOT connected\n");

	count = 0;

	item = syna_tcm_get_testing_0100();
	if (!item)
		return scnprintf(buf + count, PAGE_SIZE - count,
				"Invalid testing item id:%d\n", TEST_ID_0100);

#ifdef SHOW_TEST_RESULT_DATA
	syna_tcm_buf_init(&result_data);
	item->result_data[0] = &result_data;
#endif

	limit.value = (void *)pt01_limits;
	limit.size = (unsigned int)sizeof(pt01_limits);
	item->limit[0] = &limit;

	retval = item->do_test(tcm->tcm_dev, item, false);
	if (retval < 0)
		LOGE("Fail to run test, %s\n", item->title);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"\n%s (version.%d): %s\n\n", item->title, item->version,
			((retval < 0) || (!item->result)) ? "Fail" : "Pass");

#ifdef SHOW_TEST_RESULT_DATA
	/* print out the test data */
	if (result_data.data_length > 0) {
		for (i = 0; i < result_data.data_length; i++) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "x%02X ",
					(unsigned char)result_data.buf[i]);
		}
		count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
	}

	syna_tcm_buf_release(&result_data);
#endif

	return count;
}

static struct kobj_attribute kobj_attr_pt01 =
	__ATTR(pt01, 0444, syna_testing_pt01_show, NULL);

/*
 * Example to trigger the Config ID comparison testing.
 *
 * param
 *    [ in] kobj:  pointer to kernel object
 *    [ in] attr:  pointer to kernel attribute
 *    [out] buf:  string buffer shown on console
 *
 * return
 *    string output in case of success, a negative value otherwise.
 */
static ssize_t syna_testing_check_config_id_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int retval = 0;
	unsigned int count = 0;
	struct syna_tcm *tcm;
	struct device *p_dev;
	struct testing_item *item = NULL;
	struct testing_limit limit;
#ifdef SHOW_TEST_RESULT_DATA
	struct tcm_buffer result_data;
#endif

	p_dev = container_of(kobj->parent->parent, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (!tcm->is_connected)
		return scnprintf(buf + count, PAGE_SIZE, "Device is NOT connected\n");

	count = 0;

	item = syna_tcm_get_testing_0002();
	if (!item)
		return scnprintf(buf + count, PAGE_SIZE - count,
				"Invalid testing item id:%d\n", TEST_ID_0002);

#ifdef SHOW_TEST_RESULT_DATA
	syna_tcm_buf_init(&result_data);
	item->result_data[0] = &result_data;
#endif

	limit.value = (void *)config_id_limit;
	limit.size = (unsigned int)sizeof(config_id_limit);

	item->limit[0] = &limit;

	retval = item->do_test(tcm->tcm_dev, item, false);
	if (retval < 0)
		LOGE("Fail to run test, %s\n", item->title);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"\n%s (version.%d): %s\n\n", item->title, item->version,
			((retval < 0) || (!item->result)) ? "Fail" : "Pass");

#ifdef SHOW_TEST_RESULT_DATA
	/* print out the test data */
	if (result_data.data_length > 0) {
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\nConfig ID: %s\n", result_data.buf);
	}

	syna_tcm_buf_release(&result_data);
#endif

	return count;
}

static struct kobj_attribute kobj_attr_check_config_id =
	__ATTR(check_config_id, 0444, syna_testing_check_config_id_show, NULL);


/*
 * Example to trigger the ID comparison testing.
 *
 * param
 *    [ in] kobj:  pointer to kernel object
 *    [ in] attr:  pointer to kernel attribute
 *    [out] buf:  string buffer shown on console
 *
 * return
 *    string output in case of success, a negative value otherwise.
 */
static ssize_t syna_testing_check_dev_id_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval = 0;
	unsigned int count = 0;
	struct syna_tcm *tcm;
	struct device *p_dev;
	struct testing_item *item = NULL;
	struct testing_limit limit;
#ifdef SHOW_TEST_RESULT_DATA
	struct tcm_buffer result_data[2];
#endif

	p_dev = container_of(kobj->parent->parent, struct device, kobj);
	tcm = dev_get_drvdata(p_dev);

	if (!tcm->is_connected)
		return scnprintf(buf, PAGE_SIZE, "Device is NOT connected\n");

	count = 0;

	item = syna_tcm_get_testing_0001();
	if (!item)
		return scnprintf(buf + count, PAGE_SIZE - count,
				"Invalid testing item id:%d\n", TEST_ID_0001);

#ifdef SHOW_TEST_RESULT_DATA
	syna_tcm_buf_init(&result_data[0]);
	item->result_data[0] = &result_data[0];

	syna_tcm_buf_init(&result_data[1]);
	item->result_data[1] = &result_data[1];
#endif

	limit.value = (void *)device_id_limit;
	limit.size = syna_pal_str_len(device_id_limit);

	item->limit[0] = NULL; /* this is used to do build id comparison */
	item->limit[1] = &limit;

	retval = item->do_test(tcm->tcm_dev, item, false);
	if (retval < 0)
		LOGE("Fail to run test, %s\n", item->title);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"\n%s (version.%d): %s\n\n", item->title, item->version,
			((retval < 0) || (!item->result)) ? "Fail" : "Pass");

#ifdef SHOW_TEST_RESULT_DATA
	/* print out the test data */
	if (result_data[0].data_length > 0) {
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"Build ID: %d\n", syna_pal_le4_to_uint(result_data[0].buf));
	}

	if (result_data[1].data_length > 0) {
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"Device ID: %s\n", result_data[1].buf);
	}

	syna_tcm_buf_release(&result_data[0]);
	syna_tcm_buf_release(&result_data[1]);
#endif

	return count;
}

static struct kobj_attribute kobj_attr_check_dev_id =
	__ATTR(check_dev_id, 0444, syna_testing_check_dev_id_show, NULL);


/* Definitions of sysfs attributes for testing */
static struct attribute *attrs[] = {
	&kobj_attr_check_dev_id.attr,
	&kobj_attr_check_config_id.attr,
	&kobj_attr_pt01.attr,
	&kobj_attr_pt05.attr,
	&kobj_attr_pt0a.attr,
	NULL,
};

static struct attribute_group attr_testing_group = {
	.attrs = attrs,
};

/*
 * Create a directory for the use of sysfs attributes in testing.
 *
 * param
 *    [ in] tcm:  the driver handle
 *    [ in] sysfs_dir: root directory of sysfs nodes
 *
 * return
 *    0 or positive value in case of success, a negative value otherwise.
 */
int syna_testing_create_dir(struct syna_tcm *tcm)
{
	int retval = 0;

	tcm->sysfs_testing_dir = kobject_create_and_add("testing",
			tcm->sysfs_dir);
	if (!tcm->sysfs_testing_dir) {
		LOGE("Fail to create testing directory\n");
		return -EINVAL;
	}

	retval = sysfs_create_group(tcm->sysfs_testing_dir, &attr_testing_group);
	if (retval < 0) {
		LOGE("Fail to create sysfs group\n");

		kobject_put(tcm->sysfs_testing_dir);
		return retval;
	}

	return 0;
}
/*
 * Remove a directory allocated previously.
 *
 * param
 *    [ in] tcm:  the driver handle
 *
 * return
 *    void.
 */
void syna_testing_remove_dir(struct syna_tcm *tcm)
{
	if (tcm->sysfs_testing_dir) {
		sysfs_remove_group(tcm->sysfs_testing_dir, &attr_testing_group);
		kobject_put(tcm->sysfs_testing_dir);
	}
}


#endif /* end of HAS_SYSFS_INTERFACE */

