/* ****************************************************************************************
 * DTV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * COPYRIGHT(c) 1998-2010 by LG Electronics Inc.
 *
 * All rights reserved. No part of this work covered by this copyright hereon
 * may be reproduced, stored in a retrieval system, in any form
 * or by any means, electronic, mechanical, photocopying, recording
 * or otherwise, without the prior written  permission of LG Electronics.
 * *****************************************************************************************/

/** @file
 *
 *  DE IPC HAL.
 *  (Register Access Hardware Abstraction Layer )
 *
 *  author	youngki.lyu@lge.com
 *  version	1.0
 *  date		2011-05-03
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "lg1152_vdec_base.h"
#include "../../mcu/os_adap.h"
#include "../../mcu/vdec_print.h"

#include "de_ipc_hal_api.h"
#include "deipc_reg.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#endif

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define 		L9_VDEC_DE_IPC_BASE(ch)				(LG1152_VDEC_BASE + 0x1E00 + (ch * 0x40))

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   External Function Prototype Declarations
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   External Variables
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   global Functions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   global Variables
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Static Function Prototypes Declarations
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static volatile DEIPC_REG_T		*stpDE_IPC_Reg[DE_IPC_NUM_OF_CHANNEL];


/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
void DE_IPC_HAL_Init(void)
{
	UINT8	ui8DeIpcCh;

	for( ui8DeIpcCh = 0; ui8DeIpcCh < DE_IPC_NUM_OF_CHANNEL; ui8DeIpcCh++ )
	{
		stpDE_IPC_Reg[ui8DeIpcCh] = (volatile DEIPC_REG_T *)VDEC_TranselateVirualAddr(L9_VDEC_DE_IPC_BASE(ui8DeIpcCh), 0x40);
	}
}

/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
BOOLEAN DE_IPC_HAL_Update(UINT8 ui8DeIpcCh, S_DE_IPC_T *psDeIpc)
{
	const UINT32		ui32DownScaler = 0x10;
	UINT32			ui32DisplayMode;
	UINT32 			ui16FrameRateRes;
	UINT32 			ui16FrameRateDiv;

	if( ui8DeIpcCh >= DE_IPC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DE_IPC][Err] Channel Number(%d), %s", ui8DeIpcCh, __FUNCTION__ );
		return FALSE;
	}

	ui32DisplayMode = stpDE_IPC_Reg[ui8DeIpcCh]->display_info & 0x3;
	switch( psDeIpc->eDisplayMode )
	{
	case DE_IPC_DISPLAY_MODE_TOP_FIELD :
	case DE_IPC_DISPLAY_MODE_BOTTOM_FIELD :
		if( ui32DisplayMode == psDeIpc->eDisplayMode )
		{
			UINT32		ui32DisplayInfo;

			VDEC_KDRV_Message(WARNING, "[DE_IPC%d][Warning] Field Repeat (Reuse Previsou Frame) - Frame Index:0x%X -> 0x%X, Display Info:0x%X -> 0x%X, %s", ui8DeIpcCh,
										stpDE_IPC_Reg[ui8DeIpcCh]->frame_idx, psDeIpc->ui32FrameIdx,
										stpDE_IPC_Reg[ui8DeIpcCh]->display_info, psDeIpc->eDisplayMode,
										__FUNCTION__ );

			ui32DisplayInfo = stpDE_IPC_Reg[ui8DeIpcCh]->display_info;
			ui32DisplayInfo &= ~(0x3);
			if( ui32DisplayMode == 0x1 )
				ui32DisplayInfo |= 0x2;
			else
				ui32DisplayInfo |= 0x1;

			stpDE_IPC_Reg[ui8DeIpcCh]->display_info = ui32DisplayInfo;
			return FALSE;
		}
		break;
	case DE_IPC_DISPLAY_MODE_FRAME_PICTURE :
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[DE_IPC%d][Err] Display Info:%d", ui8DeIpcCh, psDeIpc->eDisplayMode);
	}
	stpDE_IPC_Reg[ui8DeIpcCh]->display_info = ((psDeIpc->ui32VdecPause & 0x1) << 3) | (psDeIpc->eDisplayMode & 0x3);

	stpDE_IPC_Reg[ui8DeIpcCh]->frame_idx = (psDeIpc->ui32FrameIdx & 0xFF);

	ui16FrameRateRes = psDeIpc->ui16FrameRateRes;
	ui16FrameRateDiv = psDeIpc->ui16FrameRateDiv;
	while( (ui16FrameRateRes > 0xFFFF) || (ui16FrameRateDiv > 0xFFFF) )
	{
		ui16FrameRateRes /= ui32DownScaler;
		ui16FrameRateDiv /= ui32DownScaler;
	}

	stpDE_IPC_Reg[ui8DeIpcCh]->y_frame_base_address = psDeIpc->ui32Y_FrameBaseAddr;
	stpDE_IPC_Reg[ui8DeIpcCh]->c_frame_base_address = psDeIpc->ui32C_FrameBaseAddr;
	stpDE_IPC_Reg[ui8DeIpcCh]->y_frame_offset = psDeIpc->ui32Y_FrameOffset;
	stpDE_IPC_Reg[ui8DeIpcCh]->c_frame_offset = psDeIpc->ui32C_FrameOffset;
	stpDE_IPC_Reg[ui8DeIpcCh]->stride = psDeIpc->ui32Stride;

	stpDE_IPC_Reg[ui8DeIpcCh]->frame_rate = ((UINT32)ui16FrameRateDiv << 16) | ((UINT32)ui16FrameRateRes & 0xFFFF);
	stpDE_IPC_Reg[ui8DeIpcCh]->aspect_ratio = psDeIpc->ui32AspectRatio;
	stpDE_IPC_Reg[ui8DeIpcCh]->picture_size = ((UINT32)(psDeIpc->ui16PicWidth) << 16) | ((UINT32)(psDeIpc->ui16PicHeight) & 0xFFFF);
	stpDE_IPC_Reg[ui8DeIpcCh]->h_offset = (psDeIpc->ui32CropLeftOffset << 16) |(psDeIpc->ui32CropRightOffset & 0xFFFF);
	stpDE_IPC_Reg[ui8DeIpcCh]->v_offset = (psDeIpc->ui32CropTopOffset <<16) |(psDeIpc->ui32CropBottomOffset & 0xFFFF);
	stpDE_IPC_Reg[ui8DeIpcCh]->update = psDeIpc->ui32UpdateInfo;
	stpDE_IPC_Reg[ui8DeIpcCh]->frame_pack_arrange = (psDeIpc->ui32FramePackArrange & 0xFF) | ((psDeIpc->eLR_Order & 0x3) << 8);
	stpDE_IPC_Reg[ui8DeIpcCh]->pixel_aspect_ratio = ((UINT32)(psDeIpc->ui16ParW) << 16) | ((UINT32)(psDeIpc->ui16ParH) & 0xFFFF);
	stpDE_IPC_Reg[ui8DeIpcCh]->pts_info = psDeIpc->ui32PTS;

	return TRUE;
}

/** @} */

