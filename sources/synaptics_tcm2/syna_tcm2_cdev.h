/* SPDX-License-Identifier: GPL-2.0-or-later
 *
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
 * The header file defines the structure being used in the ioctl interface
 */

#ifndef _SYNAPTICS_TCM2_CDEV_H_
#define _SYNAPTICS_TCM2_CDEV_H_



/* Definitions of the IOCTLs supported */
#define IOCTL_MAGIC 's'

/* Legacy IOCTLs */
#define OLD_RESET_ID		(0x00)
#define OLD_SET_IRQ_MODE_ID	(0x01)
#define OLD_SET_RAW_MODE_ID	(0x02)
#define OLD_CONCURRENT_ID	(0x03)

#define IOCTL_OLD_RESET         _IO(IOCTL_MAGIC, OLD_RESET_ID)
#define IOCTL_OLD_SET_IRQ_MODE  _IOW(IOCTL_MAGIC, OLD_SET_IRQ_MODE_ID, int)
#define IOCTL_OLD_SET_RAW_MODE  _IOW(IOCTL_MAGIC, OLD_SET_RAW_MODE_ID, int)
#define IOCTL_OLD_CONCURRENT    _IOW(IOCTL_MAGIC, OLD_CONCURRENT_ID, int)

/* Standard IOCTLs */
#define STD_IOCTL_BEGIN             (0x10)
#define STD_SET_PID_ID              (0x11)
#define STD_ENABLE_IRQ_ID           (0x12)
#define STD_RAW_READ_ID             (0x13)
#define STD_RAW_WRITE_ID            (0x14)
#define STD_GET_FRAME_ID            (0x15)
#define STD_SEND_MESSAGE_ID         (0x16)
#define STD_SET_REPORTS_ID          (0x17)
#define STD_CHECK_FRAMES_ID         (0x18)
#define STD_CLEAN_OUT_FRAMES_ID     (0x19)
#define STD_APPLICATION_INFO_ID     (0x1A)
#define STD_DO_HW_RESET_ID          (0x1B)

#define STD_DRIVER_CONFIG_ID        (0x21)
#define STD_DRIVER_GET_CONFIG_ID    (0x22)


#define IOCTL_STD_IOCTL_BEGIN       _IO(IOCTL_MAGIC, STD_IOCTL_BEGIN)
#define IOCTL_STD_SET_PID           _IOW(IOCTL_MAGIC, STD_SET_PID_ID, struct syna_ioctl_data *)
#define IOCTL_STD_ENABLE_IRQ        _IOW(IOCTL_MAGIC, STD_ENABLE_IRQ_ID, struct syna_ioctl_data *)
#define IOCTL_STD_RAW_READ          _IOR(IOCTL_MAGIC, STD_RAW_READ_ID, struct syna_ioctl_data *)
#define IOCTL_STD_RAW_WRITE         _IOW(IOCTL_MAGIC, STD_RAW_WRITE_ID, struct syna_ioctl_data *)
#define IOCTL_STD_GET_FRAME         _IOWR(IOCTL_MAGIC, STD_GET_FRAME_ID, struct syna_ioctl_data *)
#define IOCTL_STD_SEND_MESSAGE      _IOWR(IOCTL_MAGIC, STD_SEND_MESSAGE_ID, struct syna_ioctl_data *)
#define IOCTL_STD_SET_REPORT_TYPES  _IOW(IOCTL_MAGIC, STD_SET_REPORTS_ID, struct syna_ioctl_data *)
#define IOCTL_STD_CHECK_FRAMES      _IOWR(IOCTL_MAGIC, STD_CHECK_FRAMES_ID, struct syna_ioctl_data *)
#define IOCTL_STD_CLEAN_OUT_FRAMES  _IOWR(IOCTL_MAGIC, STD_CLEAN_OUT_FRAMES_ID, struct syna_ioctl_data *)
#define IOCTL_STD_APPLICATION_INFO  _IOWR(IOCTL_MAGIC, STD_APPLICATION_INFO_ID, struct syna_ioctl_data *)
#define IOCTL_STD_DO_HW_RESET       _IOWR(IOCTL_MAGIC, STD_DO_HW_RESET_ID, struct syna_ioctl_data *)

#define IOCTL_DRIVER_CONFIG         _IOW(IOCTL_MAGIC, STD_DRIVER_CONFIG_ID, struct syna_ioctl_data *)
#define IOCTL_DRIVER_GET_CONFIG     _IOR(IOCTL_MAGIC, STD_DRIVER_GET_CONFIG_ID, struct syna_ioctl_data *)


/* Register-like format for the device information
 *
 *       Description       BYTE |    BIT 7    |    BIT 6    |    BIT 5    |    BIT 4    |    BIT 3    |    BIT 2    |    BIT 1    |    BIT 0    |
 * --------------------------------------------------------------------------------------------------------------------------------------------------
 *      DUT Connection     [ 0] |                   reserved                            |Bare connect |          reserved         |   Activate  |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 1] |           current touchcomm version                                                                           |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 2] |           max chunk size for bus write (LSB)                                                                  |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 3] |           max chunk size for bus write (HSB)                                                                  |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 4] |           max chunk size for bus read (LSB)                                                                   |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 5] |           max chunk size for bus read (HSB)                                                                   |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 6] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 7] |                   reserved                                                                                    |
 * --------------------------------------------------------------------------------------------------------------------------------------------------
 */
struct drv_param_dut {
	union {
		struct {
			/* connection : 8 bytes */
			unsigned char activate:1;
			unsigned char reserve_b1__2:2;
			unsigned char bare:1;
			unsigned char reserve_b4__7:4;
			unsigned char touchcomm_version;
			unsigned short max_wr_size;
			unsigned short max_rd_size;
			unsigned char reserve_b48__55;
			unsigned char reserve_b56__63;
		} __packed;
		unsigned char data[8];
	};
};
/* Register-like format for the driver configurations
 *
 *       Description       BYTE |    BIT 7    |    BIT 6    |    BIT 5    |    BIT 4    |    BIT 3    |    BIT 2    |    BIT 1    |    BIT 0    |
 * --------------------------------------------------------------------------------------------------------------------------------------------------
 *      Features           [ 0] |                   reserved                                                                      |Predict Read |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 1] |           Extra bytes to read                                                                                 |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 2] |           Depth of kernel fifo                                                                                |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 3] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 4] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 5] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 6] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 7] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 8] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [ 9] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [10] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 *                         [11] |                   reserved                                                                                    |
 *                              ---------------------------------------------------------------------------------------------------------------------
 */
struct drv_param_feature {
	union {
		struct {
			/* features : 12 bytes */
			unsigned char predict_reads:1;
			unsigned char reserve_b1__7:7;
			unsigned char extra_bytes_to_read:8;
			unsigned char depth_of_fifo:8;
			unsigned char reserve_b24__31;
			unsigned char reserve_b32__39;
			unsigned char reserve_b40__47;
			unsigned char reserve_b48__55;
			unsigned char reserve_b56__63;
			unsigned char reserve_b64__71;
			unsigned char reserve_b72__79;
			unsigned char reserve_b80__87;
			unsigned char reserve_b88__95;
		} __packed;
		unsigned char data[12];
	};
};

/* Structure for the driver configuration */
struct drv_param {
	union {
		struct {
			struct drv_param_dut dut;
			struct drv_param_feature feature;
		} __packed;
		unsigned char parameter[20];
	};
};



/*
 *  Return the string of IOCTL.
 *
 * param
 *    [ in] code: IOCTL code
 *
 * return
 *    A string representing the name of the IOCTL operation
 */
static inline char *syna_cdev_ioctl_get_name(unsigned int code)
{
	switch (code) {
	case OLD_RESET_ID:
		return "IOCTL_OLD_RESET";
	case OLD_SET_IRQ_MODE_ID:
		return "IOCTL_OLD_SET_IRQ_MODE";
	case OLD_SET_RAW_MODE_ID:
		return "IOCTL_OLD_SET_RAW_MODE";
	case OLD_CONCURRENT_ID:
		return "IOCTL_OLD_CONCURRENT";
	case STD_IOCTL_BEGIN:
		return "IOCTL_QUERY_STD_SUPPORT";
	case STD_SET_PID_ID:
		return "IOCTL_STD_SET_PID";
	case STD_ENABLE_IRQ_ID:
		return "IOCTL_STD_CONFIG_IRQ";
	case STD_RAW_READ_ID:
		return "IOCTL_STD_RAW_READ";
	case STD_RAW_WRITE_ID:
		return "IOCTL_STD_RAW_WRITE";
	case STD_GET_FRAME_ID:
		return "IOCTL_STD_WAIT_DATA_FROM_KERNEL_FIFO";
	case STD_SEND_MESSAGE_ID:
		return "IOCTL_STD_SEND_MESSAGE";
	case STD_SET_REPORTS_ID:
		return "IOCTL_STD_CONFIG_DATA_TYPE_TO_KERNEL_FIFO";
	case STD_CHECK_FRAMES_ID:
		return "IOCTL_STD_CHECK_DATA_IN_KERNEL_FIFO";
	case STD_CLEAN_OUT_FRAMES_ID:
		return "IOCTL_STD_CLEAN_KERNEL_FIFO";
	case STD_APPLICATION_INFO_ID:
		return "IOCTL_STD_APPLICATION_INFO";
	case STD_DO_HW_RESET_ID:
		return "IOCTL_STD_DO_HW_RESET";
	case STD_DRIVER_CONFIG_ID:
		return "IOCTL_STD_DRIVER_CONFIG";
	case STD_DRIVER_GET_CONFIG_ID:
		return "IOCTL_STD_DRIVER_GET_CONFIG";
	default:
		return " ";
	}
	return " ";
}


#endif /* end of _SYNAPTICS_TCM2_CDEV_H_ */

