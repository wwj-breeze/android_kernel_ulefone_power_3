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
 *   mt_soc_spkprotect_common.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

#include <linux/compat.h>
#include <linux/wakelock.h>
#include "mtk-auddrv-scp-spkprotect-common.h"

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
#include "audio_ipi_client_spkprotect.h"
#include <audio_task_manager.h>
#include <audio_ipi_client_spkprotect.h>
#include <audio_dma_buf_control.h>
#endif

static Aud_Spk_Message_t mAud_Spk_Message;

void spkprocservice_ipicmd_received(ipi_msg_t *ipi_msg)
{

	switch (ipi_msg->msg_id) {
	case SPK_PROTECT_IRQDL:
		mAud_Spk_Message.msg_id = ipi_msg->msg_id;
		mAud_Spk_Message.param1 = ipi_msg->param1;
		mAud_Spk_Message.param2 = ipi_msg->param2;
		mAud_Spk_Message.payload = ipi_msg->payload;
		AudDrv_DSP_IRQ_handler((void *)&mAud_Spk_Message);
		break;
	case SPK_PROTECT_NEEDDATA:
		break;
	default:
		break;
	}
}

void spkprocservice_ipicmd_send(audio_ipi_msg_data_t data_type,
						audio_ipi_msg_ack_t ack_type, uint16_t msg_id, uint32_t param1,
						uint32_t param2, char *payload)
{
	ipi_msg_t ipi_msg;
	int send_result = 0;
	int retry_count;
	const int k_max_try_count = 200;  /* maximum wait 20ms */

	memset((void *)&ipi_msg, 0, sizeof(ipi_msg_t));
	for (retry_count = 0; retry_count < k_max_try_count ; retry_count++) {
		send_result = audio_send_ipi_msg(&ipi_msg, TASK_SCENE_SPEAKER_PROTECTION,
						 AUDIO_IPI_LAYER_KERNEL_TO_SCP,
						 data_type, ack_type, msg_id, param1, param2, (char *)payload);
		if (send_result == 0)
			break;
		udelay(100);
	}

	if (send_result < 0)
		pr_err("%s(), scp_ipi send fail\n", __func__);

}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SpkPotect");
