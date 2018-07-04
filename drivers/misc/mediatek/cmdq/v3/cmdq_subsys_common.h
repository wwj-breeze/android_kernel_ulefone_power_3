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

/* GCE subsys */
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_G3D_CONFIG_BASE, 0, MFG, g3d_config_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MMSYS_CONFIG, 1, MMSYS, mmsys_config_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_DISP_DITHER, 2, MMSYS, disp_dither_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_NA, 3, MMSYS, mm_na_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_IMGSYS, 4, CAM, imgsys_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VDEC_GCON, 5, VDEC, vdec_gcon_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VENC_GCON, 6, VENC, venc_gcon_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_CONN_PERIPHERALS, 7, PERISYS, conn_peri_base)

DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_TOPCKGEN, 8, TOP_AO_3, topckgen_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_KP, 9, INFRA_AO, kp_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_SCP_SRAM, 10, INFRA_AO, scp_sram_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_INFRA_NA3, 11, NA, infra_na3_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_INFRA_NA4, 12, NA, infra_na4_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_SCP, 13, SCP, scp_base)

DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MCUCFG, 14, INFRASYS, mcucfg_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_GCPU, 15, INFRASYS, gcpu_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_USB0, 16, PERISYS, usb0_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_USB_SIF, 17, PERISYS, usb_sif_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_AUDIO, 18, PERISYS, audio_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MSDC0, 19, PERISYS, msdc0_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MSDC1, 20, PERISYS, msdc1_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MSDC2, 21, PERISYS, msdc2_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MSDC3, 22, PERISYS, msdc3_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_AP_DMA, 23, INFRASYS, ap_dma_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_GCE, 24, GCE, gce_base)

DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VDEC, 25, VDEC, vdec_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VDEC1, 26, VDEC, vdec1_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VDEC2, 27, VDEC, vdec2_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VDEC3, 28, VDEC, vdec3_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_CAMSYS, 29, CAMSYS, camsys_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_CAMSYS1, 30, CAMSYS, camsys1_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_CAMSYS2, 31, CAMSYS, camsys2_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_CAMSYS3, 32, CAMSYS, camsys3_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_IMGSYS1, 33, IMGSYS, imgsys1_base)

DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_SMI_LAB1, 34, SMI_LAB1, smi_larb1_base)

/* Special subsys */
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_PWM_SW, 35, SPECIAL, pwm_sw_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_PWM1_SW, 36, SPECIAL, pwm1_sw_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_DIP_A0_SW, 37, SPECIAL, dip_a0_sw_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MIPITX0, 38, SPECIAL, mipitx0_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_MIPITX1, 39, SPECIAL, mipitx1_base)
DECLARE_CMDQ_SUBSYS(CMDQ_SUBSYS_VENC, 40, SPECIAL, venc_base)
