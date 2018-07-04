/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#ifdef BUILD_LK
#define GPIO2_LCM_1V8_EN		(GPIO2 | 0x80000000)
#define GPIO25_LCM_PO_EN		(GPIO25 | 0x80000000)
#define GPIO26_LCM_NO_EN		(GPIO26 | 0x80000000)
#define GPIO45_LCM_RESET		(GPIO45 | 0x80000000)
#endif

#define LCM_ID_NT36672_ID0 (0x00)
#define LCM_ID_NT36672_ID1 (0x80)
#define LCM_ID_NT36672_ID2 (0x00)


static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(2160)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH									(74520)
#define LCM_PHYSICAL_HEIGHT									(132480)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 30, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table init_setting_cmd[] = {
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x53} },

	{0x06, 1, {0x94} },
	{0x07, 1, {0x94} },
	{0x0E, 1, {0x2B} },
	{0x0F, 1, {0x24} },
	{0x2B, 1, {0x21} },
	{0x68, 1, {0x03} },
	{0x69, 1, {0x99} },
	{0x6D, 1, {0x66} },
	{0x78, 1, {0x00} },
	{0x89, 1, {0x1D} },
	{0x94, 1, {0x00} },
	{0x95, 1, {0xEB} },
	{0x96, 1, {0xEB} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x00} },
	{0x01, 1, {0x24} },
	{0x02, 1, {0x01} },
	{0x03, 1, {0x0C} },
	{0x04, 1, {0x0B} },
	{0x05, 1, {0x10} },
	{0x06, 1, {0x03} },
	{0x07, 1, {0x04} },
	{0x08, 1, {0x0F} },
	{0x09, 1, {0x00} },
	{0x0A, 1, {0x00} },
	{0x0B, 1, {0x00} },
	{0x0C, 1, {0x18} },
	{0x0D, 1, {0x16} },
	{0x0E, 1, {0x14} },

	{0x0F, 1, {0x17} },
	{0x10, 1, {0x15} },
	{0x11, 1, {0x13} },
	{0x12, 1, {0x00} },
	{0x13, 1, {0x24} },
	{0x14, 1, {0x01} },
	{0x15, 1, {0x0C} },
	{0x16, 1, {0x0B} },
	{0x17, 1, {0x10} },
	{0x18, 1, {0x03} },
	{0x19, 1, {0x04} },
	{0x1A, 1, {0x0F} },
	{0x1B, 1, {0x00} },
	{0x1C, 1, {0x00} },
	{0x1D, 1, {0x00} },
	{0x1E, 1, {0x18} },
	{0x1F, 1, {0x16} },
	{0x20, 1, {0x14} },
	{0x21, 1, {0x17} },
	{0x22, 1, {0x15} },
	{0x23, 1, {0x13} },
	{0x2F, 1, {0x02} },
	{0x30, 1, {0x03} },
	{0x31, 1, {0x02} },
	{0x32, 1, {0x03} },
	{0x34, 1, {0x01} },
	{0x35, 1, {0x49} },
	{0x37, 1, {0x02} },
	{0x38, 1, {0x68} },
	{0x39, 1, {0x68} },
	{0x3B, 1, {0x41} },
	{0x3D, 1, {0x20} },
	{0x3F, 1, {0x68} },
	{0x40, 1, {0x04} },
	{0x60, 1, {0x10} },
	{0x61, 1, {0x00} },
	{0x68, 1, {0xC3} },

	{0x78, 1, {0x80} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0x04} },
	{0x7B, 1, {0x9C} },
	{0x7D, 1, {0x04} },
	{0x7E, 1, {0x02} },
	{0x80, 1, {0x42} },
	{0x81, 1, {0x03} },
	{0x8E, 1, {0xF0} },
	{0x90, 1, {0x00} },
	{0x92, 1, {0x77} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0x04} },
	{0xB5, 1, {0x04} },

	{0xDC, 1, {0x09} },
	{0xDD, 1, {0x01} },
	{0xDE, 1, {0x00} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x68} },


	{0xE9, 1, {0x04} },
	{0xED, 1, {0x40} },
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x0A, 1, {0x81} },
	{0x0B, 1, {0xCD} },


	{0x0C, 1, {0x01} },
	{0x17, 1, {0x82} },
	{0x21, 1, {0x1F} },
	{0x22, 1, {0x1F} },
	{0x4B, 1, {0x31} },
	{0x4C, 1, {0x6B} },
	{0x4D, 1, {0x55} },
	{0x65, 1, {0x00} },
	{0x6B, 1, {0x00} },
	{0x80, 1, {0x04} },
	{0x81, 1, {0x2C} },
	{0x84, 1, {0x65} },
	{0x85, 1, {0x05} },
	{0xD7, 1, {0x80} },
	{0xD8, 1, {0x85} },
	{0xD9, 1, {0x00} },
	{0xDA, 1, {0x00} },

	{0xDB, 1, {0x05} },
	{0xDC, 1, {0x00} },
	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0x06, 1, {0xC8} },
	{0x12, 1, {0x5A} },

	{0x19, 1, {0x0B} },
	{0x1A, 1, {0x68} },
	{0x1C, 1, {0xFA} },
	{0x1D, 1, {0x0A} },
	{0x1E, 1, {0x22} },
	{0x33, 1, {0x1B} },
	{0x34, 1, {0x77} },
	{0x36, 1, {0xAF} },
	{0x37, 1, {0x1A} },
	{0x38, 1, {0x27} },
	{0x4F, 1, {0xAF} },
	{0x68, 1, {0xAF} },
	{0x99, 1, {0x20} },
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },

	{0x13, 1, {0x0E} },
	{0x16, 1, {0xB0} },
	{0x17, 1, {0xD0} },
	{0xFF, 1, {0x10} },
	{0x35, 1, {0x00} },
	{0x11, 1, {0x00} },

#ifndef BUILD_LK
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1, {0x00} },
#endif
/* ///////////////////CABC SETTING///////// */
	{0x51, 1, {0x00} },
	{0x5E, 1, {0x00} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },

};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x53} },

	{0x06, 1, {0x94} },
	{0x07, 1, {0x94} },
	{0x0E, 1, {0x2B} },
	{0x0F, 1, {0x24} },
	{0x2B, 1, {0x21} },
	{0x68, 1, {0x03} },
	{0x69, 1, {0x99} },
	{0x6D, 1, {0x66} },
	{0x78, 1, {0x00} },
	{0x89, 1, {0x1D} },
	{0x94, 1, {0x00} },
	{0x95, 1, {0xEB} },
	{0x96, 1, {0xEB} },
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x00, 1, {0x00} },
	{0x01, 1, {0x24} },
	{0x02, 1, {0x01} },
	{0x03, 1, {0x0C} },
	{0x04, 1, {0x0B} },
	{0x05, 1, {0x10} },
	{0x06, 1, {0x03} },
	{0x07, 1, {0x04} },
	{0x08, 1, {0x0F} },
	{0x09, 1, {0x00} },
	{0x0A, 1, {0x00} },
	{0x0B, 1, {0x00} },
	{0x0C, 1, {0x18} },
	{0x0D, 1, {0x16} },
	{0x0E, 1, {0x14} },

	{0x0F, 1, {0x17} },
	{0x10, 1, {0x15} },
	{0x11, 1, {0x13} },
	{0x12, 1, {0x00} },
	{0x13, 1, {0x24} },
	{0x14, 1, {0x01} },
	{0x15, 1, {0x0C} },
	{0x16, 1, {0x0B} },
	{0x17, 1, {0x10} },
	{0x18, 1, {0x03} },
	{0x19, 1, {0x04} },
	{0x1A, 1, {0x0F} },
	{0x1B, 1, {0x00} },
	{0x1C, 1, {0x00} },
	{0x1D, 1, {0x00} },
	{0x1E, 1, {0x18} },
	{0x1F, 1, {0x16} },
	{0x20, 1, {0x14} },
	{0x21, 1, {0x17} },
	{0x22, 1, {0x15} },
	{0x23, 1, {0x13} },
	{0x2F, 1, {0x02} },
	{0x30, 1, {0x03} },
	{0x31, 1, {0x02} },
	{0x32, 1, {0x03} },
	{0x34, 1, {0x01} },
	{0x35, 1, {0x49} },
	{0x37, 1, {0x02} },
	{0x38, 1, {0x68} },
	{0x39, 1, {0x68} },
	{0x3B, 1, {0x41} },
	{0x3D, 1, {0x20} },
	{0x3F, 1, {0x68} },
	{0x40, 1, {0x04} },
	{0x60, 1, {0x10} },
	{0x61, 1, {0x00} },
	{0x68, 1, {0xC3} },

	{0x78, 1, {0x80} },
	{0x79, 1, {0x00} },
	{0x7A, 1, {0x04} },
	{0x7B, 1, {0x9C} },
	{0x7D, 1, {0x04} },
	{0x7E, 1, {0x02} },
	{0x80, 1, {0x42} },
	{0x81, 1, {0x03} },
	{0x8E, 1, {0xF0} },
	{0x90, 1, {0x00} },
	{0x92, 1, {0x77} },
	{0xB3, 1, {0x00} },
	{0xB4, 1, {0x04} },
	{0xB5, 1, {0x04} },

	{0xDC, 1, {0x09} },
	{0xDD, 1, {0x01} },
	{0xDE, 1, {0x00} },
	{0xDF, 1, {0x03} },
	{0xE0, 1, {0x68} },


	{0xE9, 1, {0x04} },
	{0xED, 1, {0x40} },
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x0A, 1, {0x81} },
	{0x0B, 1, {0xCD} },


	{0x0C, 1, {0x01} },
	{0x17, 1, {0x82} },
	{0x21, 1, {0x1F} },
	{0x22, 1, {0x1F} },
	{0x4B, 1, {0x31} },
	{0x4C, 1, {0x6B} },
	{0x4D, 1, {0x55} },
	{0x65, 1, {0x00} },
	{0x6B, 1, {0x00} },
	{0x80, 1, {0x04} },
	{0x81, 1, {0x2C} },
	{0x84, 1, {0x65} },
	{0x85, 1, {0x05} },
	{0xD7, 1, {0x80} },
	{0xD8, 1, {0x85} },
	{0xD9, 1, {0x00} },
	{0xDA, 1, {0x00} },

	{0xDB, 1, {0x05} },
	{0xDC, 1, {0x00} },
	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0x06, 1, {0xC8} },
	{0x12, 1, {0x5A} },

	{0x19, 1, {0x0B} },
	{0x1A, 1, {0x68} },
	{0x1C, 1, {0xFA} },
	{0x1D, 1, {0x0A} },
	{0x1E, 1, {0x22} },
	{0x33, 1, {0x1B} },
	{0x34, 1, {0x77} },
	{0x36, 1, {0xAF} },
	{0x37, 1, {0x1A} },
	{0x38, 1, {0x27} },
	{0x4F, 1, {0xAF} },
	{0x68, 1, {0xAF} },
	{0x99, 1, {0x20} },
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },

	{0x13, 1, {0x0E} },
	{0x16, 1, {0xB0} },
	{0x17, 1, {0xD0} },
	{0xFF, 1, {0x10} },
	{0x35, 1, {0x00} },
	{0x11, 1, {0x00} },

#ifndef BUILD_LK
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1, {0x00} },
#endif
/* ///////////////////CABC SETTING///////// */
	{0x51, 1, {0x00} },
	{0x5E, 1, {0x00} },
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },

};


static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;/*20*/
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	/* 420->460 */
	params->dsi.PLL_CLOCK = 480;	/* this value must be in MTK suggested table */
#else
	/* 440->460 */
	params->dsi.PLL_CLOCK = 490;	/* this value must be in MTK suggested table */
#endif
	params->dsi.PLL_CK_CMD = 420;
	params->dsi.PLL_CK_VDO = 440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_width = 1080;
	params->corner_pattern_height = 32;
#endif
}

static void lcm_init_power(void)
{
	/*LCM_LOGD("lcm_init_power\n");*/

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO2_LCM_1V8_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO2_LCM_1V8_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO2_LCM_1V8_EN, GPIO_OUT_ONE);
	MDELAY(2);

	mt_set_gpio_mode(GPIO25_LCM_PO_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO25_LCM_PO_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO25_LCM_PO_EN, GPIO_OUT_ONE);
	MDELAY(2);

	mt_set_gpio_mode(GPIO26_LCM_NO_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO26_LCM_NO_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO26_LCM_NO_EN, GPIO_OUT_ONE);

	MDELAY(20);
#else
	display_bias_enable();

#endif
}

static void lcm_suspend_power(void)
{
	/*LCM_LOGD("lcm_suspend_power\n");*/

#ifdef BUILD_LK
	mt_set_gpio_out(GPIO2_LCM_1V8_EN, GPIO_OUT_ZERO);
	MDELAY(2);

	mt_set_gpio_out(GPIO26_LCM_NO_EN, GPIO_OUT_ZERO);
	MDELAY(2);

	mt_set_gpio_out(GPIO25_LCM_PO_EN, GPIO_OUT_ZERO);
#else
	display_bias_disable();

#endif
}

static void lcm_resume_power(void)
{
	/*LCM_LOGD("lcm_resume_power\n");*/

	/*lcm_init_power();*/
	SET_RESET_PIN(0);
	display_bias_enable();
}

static void lcm_init(void)
{

	/*LCM_LOGD("lcm_init\n");*/

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO45_LCM_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO45_LCM_RESET, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ONE);
	MDELAY(20);
#else
	SET_RESET_PIN(0);
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	if (lcm_dsi_mode == CMD_MODE) {
		push_table(NULL, init_setting_cmd, sizeof(init_setting_cmd) / sizeof(struct LCM_setting_table), 1);
		LCM_LOGI("nt36672----rt5081----lcm mode = cmd mode :%d----\n", lcm_dsi_mode);
	} else {
		push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
		LCM_LOGI("nt36672----rt5081----lcm mode = vdo mode :%d----\n", lcm_dsi_mode);
	}

#endif



}

static void lcm_suspend(void)
{
	/*LCM_LOGD("lcm_suspend\n");*/

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	/*LCM_LOGD("lcm_resume\n");*/

	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id0 = 0, id1 = 0, id2 = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xDA, buffer, 2);
	id0 = buffer[0];     /* we only need ID */

	read_reg_v2(0xDB, buffer, 1);
	id1 = buffer[0];

	read_reg_v2(0xDC, buffer, 1);
	id2 = buffer[0];

	LCM_LOGI("%s,NT36672_ID0=0x%08x,NT36672_ID1=0x%x,NT36672_ID2=0x%x\n", __func__, id0, id1, id2);

	if (id0 == LCM_ID_NT36672_ID0 && id1 == LCM_ID_NT36672_ID1 && id2 == LCM_ID_NT36672_ID2)
		return 1;
	else
		return 0;
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		LCM_LOGD("%s calc error,assign full roi:y=%d,h=%d\n", __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*LCM_LOGD("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif

#if (LCM_DSI_CMD_MODE)
LCM_DRIVER nt36672_fhdp_dsi_cmd_tianma_rt5081_lcm_drv = {
	.name = "nt36672_fhdp_dsi_cmd_tianma_rt5081_drv",
#else

LCM_DRIVER nt36672_fhdp_dsi_vdo_tianma_rt5081_lcm_drv = {
	.name = "nt36672_fhdp_dsi_vdo_tianma_rt5081_drv",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
