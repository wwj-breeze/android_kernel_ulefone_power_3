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
*
* You should have received a copy of the GNU General Public License
* along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/


/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_analog_type.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

#ifndef _AUDIO_ANALOG_TYPE_H
#define _AUDIO_ANALOG_TYPE_H


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/


typedef enum {
	AUDIO_ANALOG_VOLUME_HSOUTL = 0,
	AUDIO_ANALOG_VOLUME_HSOUTR,
	AUDIO_ANALOG_VOLUME_HPOUTL,
	AUDIO_ANALOG_VOLUME_HPOUTR,
	AUDIO_ANALOG_VOLUME_SPKL,
	AUDIO_ANALOG_VOLUME_SPKR,
	AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_R,
	AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_L,
	AUDIO_ANALOG_VOLUME_IV_BUFFER,
	AUDIO_ANALOG_VOLUME_LINEOUTL,
	AUDIO_ANALOG_VOLUME_LINEOUTR,
	AUDIO_ANALOG_VOLUME_LINEINL,
	AUDIO_ANALOG_VOLUME_LINEINR,
	AUDIO_ANALOG_VOLUME_MICAMP1,
	AUDIO_ANALOG_VOLUME_MICAMP2,
	AUDIO_ANALOG_VOLUME_MICAMP3,
	AUDIO_ANALOG_VOLUME_MICAMP4,
	AUDIO_ANALOG_VOLUME_LEVELSHIFTL,
	AUDIO_ANALOG_VOLUME_LEVELSHIFTR,
	AUDIO_ANALOG_VOLUME_TYPE_MAX
} AUDIO_ANALOG_VOLUME_TYPE;

/* mux seleciotn */
typedef enum {
	AUDIO_ANALOG_MUX_VOICE = 0,
	AUDIO_ANALOG_MUX_AUDIO,
	AUDIO_ANALOG_MUX_IV_BUFFER,
	AUDIO_ANALOG_MUX_LINEIN_STEREO,
	AUDIO_ANALOG_MUX_LINEIN_L,
	AUDIO_ANALOG_MUX_LINEIN_R,
	AUDIO_ANALOG_MUX_LINEIN_AUDIO_MONO,
	AUDIO_ANALOG_MUX_LINEIN_AUDIO_STEREO,
	AUDIO_ANALOG_MUX_IN_MIC1,
	AUDIO_ANALOG_MUX_IN_MIC2,
	AUDIO_ANALOG_MUX_IN_MIC3,
	AUDIO_ANALOG_MUX_IN_MIC4,
	AUDIO_ANALOG_MUX_IN_LINE_IN,
	AUDIO_ANALOG_MUX_IN_PREAMP_1,
	AUDIO_ANALOG_MUX_IN_PREAMP_2,
	AUDIO_ANALOG_MUX_IN_PREAMP_3,
	AUDIO_ANALOG_MUX_IN_PREAMP_4,
	AUDIO_MICSOURCE_MUX_IN_1,
	AUDIO_MICSOURCE_MUX_IN_2,
	AUDIO_MICSOURCE_MUX_IN_3,
	AUDIO_MICSOURCE_MUX_IN_4,
	AUDIO_ANALOG_MUX_IN_LEVEL_SHIFT_BUFFER,
	AUDIO_ANALOG_MUX_MUTE,
	AUDIO_ANALOG_MUX_OPEN,
	AUDIO_ANALOG_MAX_MUX_TYPE
} AUDIO_ANALOG_MUX_TYPE;

/* device power */
typedef enum {
	AUDIO_ANALOG_DEVICE_OUT_EARPIECER = 0,
	AUDIO_ANALOG_DEVICE_OUT_EARPIECEL = 1,
	AUDIO_ANALOG_DEVICE_OUT_HEADSETR = 2,
	AUDIO_ANALOG_DEVICE_OUT_HEADSETL = 3,
	AUDIO_ANALOG_DEVICE_OUT_SPEAKERR = 4,
	AUDIO_ANALOG_DEVICE_OUT_SPEAKERL = 5,
	AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R = 6,
	AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_L = 7,
	AUDIO_ANALOG_DEVICE_OUT_LINEOUTR = 8,
	AUDIO_ANALOG_DEVICE_OUT_LINEOUTL = 9,
	AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP = 10,
	AUDIO_ANALOG_DEVICE_2IN1_SPK = 11,
	/* DEVICE_IN_LINEINR = 11, */
	/* DEVICE_IN_LINEINL = 12, */
	AUDIO_ANALOG_DEVICE_IN_ADC1 = 13,
	AUDIO_ANALOG_DEVICE_IN_ADC2 = 14,
	AUDIO_ANALOG_DEVICE_IN_ADC3 = 15,
	AUDIO_ANALOG_DEVICE_IN_ADC4 = 16,
	AUDIO_ANALOG_DEVICE_IN_PREAMP_L = 17,
	AUDIO_ANALOG_DEVICE_IN_PREAMP_R = 18,
	AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC = 19,
	AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH = 20,
	AUDIO_ANALOG_DEVICE_MAX
} AUDIO_ANALOG_DEVICE_TYPE;

typedef enum {
	AUDIO_ANALOG_DEVICE_OUT_DAC,
	AUDIO_ANALOG_DEVICE_IN_ADC,
	AUDIO_ANALOG_DEVICE_IN_ADC_2,
	AUDIO_ANALOG_DEVICE_INOUT_MAX
} AUDIO_ANALOG_DEVICE_SAMPLERATE_TYPE;


typedef enum {
	AUDIO_ANALOG_DEVICE_PLATFORM_MACHINE,
	AUDIO_ANALOG_DEVICE_PLATFORM,
	AUDIO_ANALOG_DEVICE_MACHINE,
	AUDIO_ANALOG_DEVICE_TYPE_SETTING_MAX
} AUDIO_ANALOG_DEVICE_TYPE_SETTING;

typedef enum {
	AUDIO_ANALOG_AUDIOANALOG_DEVICE,
	AUDIO_ANALOG_AUDIOANALOG_VOLUME,
	AUDIO_ANALOG_AUDIOANALOG_MUX
} AUDIO_ANALOG_AUDIOANALOG_TYPE;

typedef enum {
	AUDIO_ANALOG_AUDIOANALOG_INPUT_OPEN = 0,
	AUDIO_ANALOG_AUDIOANALOG_INPUT_ADC,
	AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP,
} AUDIO_ANALOG_AUDIOANALOG_INPUT;

typedef enum {
	AUDIO_ANALOG_AUDIOANALOGZCD_HEADPHONE = 1,
	AUDIO_ANALOG_AUDIOANALOGZCD_HANDSET = 2,
	AUDIO_ANALOG_AUDIOANALOGZCD_IVBUFFER = 3,
} AUDIO_ANALOG_AUDIOANALOGZCD_TYPE;

typedef enum {
	AUDIO_ANALOG_CLASS_AB = 0,
	AUDIO_ANALOG_CLASS_D,
} AUDIO_ANALOG_SPEAKER_CLASS;


typedef enum {
	AUDIO_ANALOG_CHANNELS_LEFT1 = 0,
	AUDIO_ANALOG_CHANNELS_RIGHT1,
} AUDIO_ANALOG_CHANNELS;


typedef enum {
	AUDIO_ANALOG_SET_SPEAKER_CLASS = 0,
	AUDIO_ANALOG_GET_SPEAKER_CLASS = 1,
	AUDIO_ANALOG_SET_CURRENT_SENSING = 2,
	AUDIO_ANALOG_SET_CURRENT_SENSING_PEAK_DETECTOR = 3,
} AUDIOANALOG_COMMAND;


typedef enum {
	AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON = 0,
	AUDIO_ANALOG_DAC_LOOP_DAC_HS_OFF,
	AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON,
	AUDIO_ANALOG_DAC_LOOP_DAC_HP_OFF,
	AUDIO_ANALOG_DAC_LOOP_DAC_SPEAKER_ON,
	AUDIO_ANALOG_DAC_LOOP_DAC_SPEAKER_OFF,
} AUDIO_ANALOG_LOOPBACK;

typedef enum {
	AUDIO_ANALOGUL_MODE_ACC = 0,
	AUDIO_ANALOGUL_MODE_DCC,
	AUDIO_ANALOGUL_MODE_DMIC,
	AUDIO_ANALOGUL_MODE_DCCECMDIFF,
	AUDIO_ANALOGUL_MODE_DCCECMSINGLE,
} AUDIO_ANALOGUL_MODE;

typedef enum {
	AUDIO_ANALOGUL_MODE_NORMAL = 0,
	AUDIO_ANALOGUL_MODE_LOWPOWER,
} AUDIO_ANALOGUL_POWER_MODE;

typedef enum {
	AUDIO_SPEAKER_MODE_D = 0,
	AUDIO_SPEAKER_MODE_AB = 1,
	AUDIO_SPEAKER_MODE_RECEIVER,
} AUDIO_SPEAKER_MODE;

typedef enum {
	AUDIO_MIC_BIAS0 = 0,
	AUDIO_MIC_BIAS1,
	AUDIO_MIC_BIAS2,
} AUDIO_MIC_BIAS;

typedef enum {
	AUDIO_SDM_LEVEL_MUTE = 0,
	AUDIO_SDM_LEVEL_NORMAL = 0x1e,
} AUDIO_SDM_LEVEL;

typedef enum {
	AUDIO_OFFSET_TRIM_MUX_OPEN = 0,
	AUDIO_OFFSET_TRIM_MUX_HPL,
	AUDIO_OFFSET_TRIM_MUX_HPR,
	AUDIO_OFFSET_TRIM_MUX_HSP,
	AUDIO_OFFSET_TRIM_MUX_HSN,
	AUDIO_OFFSET_TRIM_MUX_LOLP,
	AUDIO_OFFSET_TRIM_MUX_LOLN,
	AUDIO_OFFSET_TRIM_MUX_LORP,
	AUDIO_OFFSET_TRIM_MUX_LORN,
	AUDIO_OFFSET_TRIM_MUX_GROUND,
} AUDIO_OFFSET_TRIM_MUX;

typedef enum {
	AUDIO_OFFSET_TRIM_0V = 0,
	AUDIO_OFFSET_TRIM_MINUSP5,
	AUDIO_OFFSET_TRIM_MINUS1P5,
	AUDIO_OFFSET_TRIM_MINUS2P5,
	AUDIO_OFFSET_TRIM_MINUS3P5,
	AUDIO_OFFSET_TRIM_MINUS4P5,
	AUDIO_OFFSET_TRIM_MINUS5P5,
	AUDIO_OFFSET_TRIM_MINUS6P5,
	AUDIO_OFFSET_TRIM_P5 = 0x9,
	AUDIO_OFFSET_TRIM_1P5,
	AUDIO_OFFSET_TRIM_2P5,
	AUDIO_OFFSET_TRIM_3P5,
	AUDIO_OFFSET_TRIM_4P5,
	AUDIO_OFFSET_TRIM_5P5,
	AUDIO_OFFSET_TRIM_6P5,
} AUDIO_OFFSET_TRIM_VOLTAGE;

typedef enum {
	AUDIO_OFFSET_FINETUNE_ZREO = 0,
	AUDIO_OFFSET_FINETUNE_MINUSP5 = 1,
	AUDIO_OFFSET_FINETUNE_P5 = 2,
} AUDIO_OFFSET_FINETRIM_VOLTAGE;

typedef enum {
	AUDIO_HP_IMPEDANCE16 = 16,
	AUDIO_HP_IMPEDANCE32 = 32,
	AUDIO_HP_IMPEDANCE64 = 64,
	AUDIO_HP_IMPEDANCE128 = 128,
	AUDIO_HP_IMPEDANCE256 = 256,
} AUDIO_HP_IMPEDANCE;
typedef struct {
	int32 mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_TYPE_MAX];
	int32 mAudio_Ana_Mux[AUDIO_ANALOG_MAX_MUX_TYPE];
	int32 mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_MAX];
	int32 mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_MAX];
} mt6331_Codec_Data_Priv;


#endif
