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
 * main driver implementation for de device.
 * de device will teach you how to make device driver with new platform.
 *
 * author     youngki.lyu@lge.com
 * version    1.0
 * date       2011.11.08
 * note       Additional information.
 *
 * @addtogroup lg115x_vdec
 * @{
 */


/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "vsync_drv.h"
#include "sync_drv.h"
#include "pts_drv.h"
#include "de_if_drv.h"
#include "../mcu/ipc_req.h"

#ifdef __XTENSA__
#include <stdio.h>
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#endif

#include "../mcu/os_adap.h"
#include "disp_q.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../hal/lg1152/vsync_hal_api.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/lq_hal_api.h"

#include "../mcu/vdec_shm.h"

#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		VSYNC_NUM_OF_WORK_Q				0x80

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT32		ui32GSTC;
	struct
	{
		UINT32		ui32STC;
		BOOLEAN		bIsNeededNewFrame;
	} Ch[VSYNC_MAX_NUM_OF_MULTI_CHANNEL];
} S_VSYNC_WORK_T;

typedef struct
{
	struct
	{
		UINT32		ui32FrameRateResidual;
		UINT32		ui32FrameRateDivider;
		UINT32		ui32FrameDuration90K;
		BOOLEAN		bInterlaced;
		UINT32		ui32VsyncDuration90K;
	} Rate;

	struct
	{
		BOOLEAN		bFixedVSync;
		struct
		{
			BOOLEAN			bUse;
			UINT8 			ui8SyncCh;
			E_DE_IF_DST_T 	eDeIfDstCh;
		} Ch[VSYNC_MAX_NUM_OF_MULTI_CHANNEL];
	} Config;

	struct
	{
		S_VSYNC_WORK_T	sWork[VSYNC_NUM_OF_WORK_Q];
		volatile UINT32		ui32WrIndex;
		volatile UINT32		ui32RdIndex;
	} WorkQ;

	struct
	{
		UINT32		ui32SyncField;
		volatile UINT32		ui32DualUseChMask;
		volatile UINT32		ui32ActiveDeIfChMask;
		struct
		{
			UINT32			ui32STC_Prev;
			UINT32			ui32STC_Duration;
		} Ch[VSYNC_MAX_NUM_OF_MULTI_CHANNEL];
	} Status;

	struct
	{
		UINT32		ui32LogTick_ISR;
		UINT32		ui32LogTick_Dual;
		UINT32		ui32GSTC_Prev;
		UINT32		ui32DurationTick;
		UINT32		ui32DualTickOffset;
//		UINT32		ui32VsyncShare;
//		UINT32		ui32VsyncSum;
	} Debug;

}S_VDEC_SYNC_DB_T;



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
volatile S_VDEC_SYNC_DB_T *gsVSync = NULL;

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
void VSync_Init(void)
{
	UINT32	gsVSyncPhy, gsVSyncSize;
	UINT8	ui8VSyncCh, ui8UseCh;

	gsVSyncSize = sizeof(S_VDEC_SYNC_DB_T) * VSYNC_NUM_OF_CHANNEL;
#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__))
	gsVSyncPhy = IPC_REG_DE_SYNC_GetDb();
#else
	gsVSyncPhy = VDEC_SHM_Malloc(gsVSyncSize);
	IPC_REG_DE_SYNC_SetDb(gsVSyncPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[VSync][DBG] gsVSyncPhy: 0x%X, %s", gsVSyncPhy, __FUNCTION__ );

	gsVSync = (S_VDEC_SYNC_DB_T *)VDEC_TranselateVirualAddr(gsVSyncPhy, gsVSyncSize);

	for( ui8VSyncCh = 0; ui8VSyncCh < VSYNC_NUM_OF_CHANNEL; ui8VSyncCh++ )
	{
		gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual = 0x0;
		gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider = 0x1;
		gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K = 0x0FFFFFFF;
		gsVSync[ui8VSyncCh].Rate.bInterlaced = TRUE;
		gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K = 0x0FFFFFFF;

		gsVSync[ui8VSyncCh].Config.bFixedVSync = FALSE;
		for( ui8UseCh = 0; ui8UseCh < VSYNC_MAX_NUM_OF_MULTI_CHANNEL; ui8UseCh++ )
		{
			gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].bUse = FALSE;
			gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].ui8SyncCh = SYNC_INVALID_CHANNEL;
			gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].eDeIfDstCh = SYNC_INVALID_CHANNEL;
		}

		gsVSync[ui8VSyncCh].WorkQ.ui32WrIndex = 0;
		gsVSync[ui8VSyncCh].WorkQ.ui32RdIndex = 0;

		gsVSync[ui8VSyncCh].Status.ui32SyncField = 0;
		gsVSync[ui8VSyncCh].Status.ui32DualUseChMask = 0x0;
		gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask = 0x0;
		for( ui8UseCh = 0; ui8UseCh < VSYNC_MAX_NUM_OF_MULTI_CHANNEL; ui8UseCh++ )
		{
			gsVSync[ui8VSyncCh].Status.Ch[ui8UseCh].ui32STC_Prev = 0x80000000;
			gsVSync[ui8VSyncCh].Status.Ch[ui8UseCh].ui32STC_Duration = 0x80000000;
		}
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
UINT8 VSync_Open(E_DE_IF_DST_T eDeIfDstCh, UINT8 ui8SyncCh, BOOLEAN bDualDecoding, BOOLEAN bFixedVSync, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bInterlaced)
{
	UINT8		ui8VSyncCh = SYNC_INVALID_CHANNEL;
	UINT8		ui8UseCh;
	UINT8		ui8EmptyCh;
	BOOLEAN		bIsIntrEnable;

	switch( eDeIfDstCh )
	{
	case DE_IF_DST_DE_IF0 :
	case DE_IF_DST_DE_IF1 :
		if( bDualDecoding == TRUE )
		{
			ui8VSyncCh = 0;
			VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] Dual - Sync:%d, DE_IF:%d, %s(%d)", ui8VSyncCh, ui8SyncCh, eDeIfDstCh, __FUNCTION__, __LINE__ );
		}
		else
		{
			ui8VSyncCh = (UINT8)eDeIfDstCh;
		}
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[DE_IF%d][Err] %s", eDeIfDstCh, __FUNCTION__ );
		return SYNC_INVALID_CHANNEL;
	}

	if( gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask & (1 << eDeIfDstCh) )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Already Set - ActiveChMask: 0x%X(%d), %s(%d)", ui8VSyncCh, gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask, eDeIfDstCh, __FUNCTION__, __LINE__ );
		for( ui8UseCh = 0; ui8UseCh < VSYNC_MAX_NUM_OF_MULTI_CHANNEL; ui8UseCh++ )
			VDEC_KDRV_Message(ERROR, "[VSync%d:%d][Err] Use:%d, Sync:%d, DE_IF:%d, %s(%d)", ui8VSyncCh, ui8UseCh,
										gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].bUse,
										gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].ui8SyncCh,
										gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].eDeIfDstCh,
										__FUNCTION__, __LINE__ );

		return SYNC_INVALID_CHANNEL;
	}

	VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] DualDecoding:%d, Fixed:%d, ActiveChMask: 0x%X, DE_IF:%d, Sync:%d, %s ", ui8VSyncCh, bDualDecoding, bFixedVSync, gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask, eDeIfDstCh, ui8SyncCh, __FUNCTION__ );

	ui8EmptyCh = SYNC_INVALID_CHANNEL;
	for( ui8UseCh = 0; ui8UseCh < VSYNC_MAX_NUM_OF_MULTI_CHANNEL; ui8UseCh++ )
	{
		if( gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].bUse == TRUE )
		{
			if( (gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].ui8SyncCh == ui8SyncCh) &&
				(gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].eDeIfDstCh == eDeIfDstCh) )
			{
				VDEC_KDRV_Message(ERROR, "[VSync%d:%d][Err] Already Set - Sync:%d, DE_IF:%d, %s(%d)", ui8VSyncCh, ui8UseCh, ui8SyncCh, eDeIfDstCh, __FUNCTION__, __LINE__ );
				return SYNC_INVALID_CHANNEL;
			}
		}
		else if( ui8EmptyCh == SYNC_INVALID_CHANNEL )
		{
			ui8EmptyCh = ui8UseCh;
		}
	}
	if( ui8EmptyCh == SYNC_INVALID_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Not Enough Mux Channel, %s(%d)", ui8VSyncCh, __FUNCTION__, __LINE__ );
		return SYNC_INVALID_CHANNEL;
	}

	VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] DE_IF:%d, Sync:%d, %s ", ui8VSyncCh, eDeIfDstCh, ui8SyncCh, __FUNCTION__ );

	if( bDualDecoding == TRUE )
	{
		if( gsVSync[ui8VSyncCh].Status.ui32DualUseChMask & (1 << (UINT32)ui8EmptyCh) )
			VDEC_KDRV_Message(ERROR, "[VSync%d][Err] IsDualDecoding - Sync:%d, DE_IF:%d, %s(%d)", ui8VSyncCh, ui8SyncCh, eDeIfDstCh, __FUNCTION__, __LINE__ );
		else
			gsVSync[ui8VSyncCh].Status.ui32DualUseChMask |= (1 << (UINT32)ui8EmptyCh);
	}

	gsVSync[ui8VSyncCh].Config.bFixedVSync = bFixedVSync;
	gsVSync[ui8VSyncCh].Config.Ch[ui8EmptyCh].bUse = TRUE;
	gsVSync[ui8VSyncCh].Config.Ch[ui8EmptyCh].ui8SyncCh = ui8SyncCh;
	gsVSync[ui8VSyncCh].Config.Ch[ui8EmptyCh].eDeIfDstCh = eDeIfDstCh;
	gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask |= (1 << (UINT32)eDeIfDstCh);
	gsVSync[ui8VSyncCh].Status.Ch[ui8EmptyCh].ui32STC_Prev = 0x80000000;
	gsVSync[ui8VSyncCh].Status.Ch[ui8EmptyCh].ui32STC_Duration = 0x80000000;

	if( gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask == (1 << (UINT32)eDeIfDstCh) )
	{
		gsVSync[ui8VSyncCh].Status.ui32SyncField = 0;

		gsVSync[ui8VSyncCh].Debug.ui32LogTick_ISR = 0;
		gsVSync[ui8VSyncCh].Debug.ui32LogTick_Dual = 0;
		gsVSync[ui8VSyncCh].Debug.ui32GSTC_Prev = 0xFFFFFFFF;
		gsVSync[ui8VSyncCh].Debug.ui32DurationTick = 0;
		gsVSync[ui8VSyncCh].Debug.ui32DualTickOffset = 0;
//		gsVSync[ui8VSyncCh].Debug.ui32VsyncShare = 0;
//		gsVSync[ui8VSyncCh].Debug.ui32VsyncSum = 0;

		gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual = ui32FrameRateRes;
		gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider = ui32FrameRateDiv;
		gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
		gsVSync[ui8VSyncCh].Rate.bInterlaced = bInterlaced;
		gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K = (bInterlaced == TRUE) ? gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K / 2 : gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K;

		VSync_HAL_SetVsyncField(ui8VSyncCh, ui32FrameRateRes, ui32FrameRateDiv, bInterlaced);

		TOP_HAL_SetLQSyncMode(ui8SyncCh, ui8VSyncCh);

#if defined(USE_MCU_FOR_VDEC_VDS)
		TOP_HAL_ClearInterIntr(VSYNC0+ui8VSyncCh);
		TOP_HAL_EnableInterIntr(VSYNC0+ui8VSyncCh);
#else
		TOP_HAL_ClearExtIntr(VSYNC0+ui8VSyncCh);
		TOP_HAL_EnableExtIntr(VSYNC0+ui8VSyncCh);
#endif

		VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] Enable Interrupt, %s ", ui8VSyncCh, __FUNCTION__ );
	}

#if defined(USE_MCU_FOR_VDEC_VDS)
	bIsIntrEnable = TOP_HAL_IsInterIntrEnable(VSYNC0+ui8VSyncCh);
#else
	bIsIntrEnable = TOP_HAL_IsExtIntrEnable(VSYNC0+ui8VSyncCh);
#endif
	if( bIsIntrEnable == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] VSync Channel Interrupt Disabled, %s ", ui8VSyncCh, __FUNCTION__ );
	}

	return ui8VSyncCh;
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
void VSync_Close(UINT8 ui8VSyncCh, E_DE_IF_DST_T eDeIfDstCh, UINT8 ui8SyncCh)
{
	UINT8	ui8UseCh;
	UINT8	ui8DeleteCh;

	if( ui8VSyncCh >= VSYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] %s", ui8VSyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] %s", ui8VSyncCh, __FUNCTION__ );

	ui8DeleteCh = SYNC_INVALID_CHANNEL;
	for( ui8UseCh = 0; ui8UseCh < VSYNC_MAX_NUM_OF_MULTI_CHANNEL; ui8UseCh++ )
	{
		if( gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].bUse == TRUE )
		{
			if( (gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].ui8SyncCh == ui8SyncCh) &&
				(gsVSync[ui8VSyncCh].Config.Ch[ui8UseCh].eDeIfDstCh == eDeIfDstCh) )
			{
				if( ui8DeleteCh != SYNC_INVALID_CHANNEL )
					VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Not Matched Channel - DE_IF:%d, Sync:%d, %s(%d)", ui8VSyncCh, eDeIfDstCh, ui8SyncCh, __FUNCTION__, __LINE__ );

				ui8DeleteCh = ui8UseCh;
			}
		}
	}
	if( ui8DeleteCh == SYNC_INVALID_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Not Matched Channel - DE_IF:%d, Sync:%d, %s(%d)", ui8VSyncCh, eDeIfDstCh, ui8SyncCh, __FUNCTION__, __LINE__ );
		return;
	}

	gsVSync[ui8VSyncCh].Config.Ch[ui8DeleteCh].ui8SyncCh = SYNC_INVALID_CHANNEL;
	gsVSync[ui8VSyncCh].Config.Ch[ui8DeleteCh].eDeIfDstCh = SYNC_INVALID_CHANNEL;
	gsVSync[ui8VSyncCh].Config.Ch[ui8DeleteCh].bUse = FALSE;
	gsVSync[ui8VSyncCh].Status.ui32DualUseChMask &= ~(1 << (UINT32)ui8DeleteCh);
	gsVSync[ui8VSyncCh].Status.Ch[ui8DeleteCh].ui32STC_Prev = 0x80000000;
	gsVSync[ui8VSyncCh].Status.Ch[ui8DeleteCh].ui32STC_Duration = 0x80000000;

	VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] DE_IF:%d, Sync:%d, %s ", ui8VSyncCh, eDeIfDstCh, ui8SyncCh, __FUNCTION__ );

	gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask &= ~(1 << (UINT32)eDeIfDstCh);
	if( gsVSync[ui8VSyncCh].Status.ui32ActiveDeIfChMask == 0x0 )
	{
		BOOLEAN			bIsIntrEnable;

		gsVSync[ui8VSyncCh].Status.ui32SyncField = 0;

		gsVSync[ui8VSyncCh].Debug.ui32LogTick_ISR = 0;
		gsVSync[ui8VSyncCh].Debug.ui32LogTick_Dual = 0;
		gsVSync[ui8VSyncCh].Debug.ui32GSTC_Prev = 0xFFFFFFFF;
		gsVSync[ui8VSyncCh].Debug.ui32DurationTick = 0;
		gsVSync[ui8VSyncCh].Debug.ui32DualTickOffset = 0;
//		gsVSync[ui8VSyncCh].Debug.ui32VsyncShare = 0;
//		gsVSync[ui8VSyncCh].Debug.ui32VsyncSum = 0;

		gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual = 0x0;
		gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider = 0x1;
		gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K = 0x0FFFFFFF;
		gsVSync[ui8VSyncCh].Rate.bInterlaced = TRUE;
		gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K = 0x0FFFFFFF;

#if defined(USE_MCU_FOR_VDEC_VDS)
		bIsIntrEnable = TOP_HAL_IsInterIntrEnable(VSYNC0+ui8VSyncCh);
#else
		bIsIntrEnable = TOP_HAL_IsExtIntrEnable(VSYNC0+ui8VSyncCh);
#endif
		if( bIsIntrEnable == FALSE )
		{
			VDEC_KDRV_Message(ERROR, "[VSync%d][Err] VSync Channel Interrupt Disabled, %s ", ui8VSyncCh, __FUNCTION__ );
		}

#if defined(USE_MCU_FOR_VDEC_VDS)
		TOP_HAL_DisableInterIntr(VSYNC0+ui8VSyncCh);
		TOP_HAL_ClearInterIntr(VSYNC0+ui8VSyncCh);
#else
		TOP_HAL_DisableExtIntr(VSYNC0+ui8VSyncCh);
		TOP_HAL_ClearExtIntr(VSYNC0+ui8VSyncCh);
#endif

		VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] Disable Interrupt, %s ", ui8VSyncCh, __FUNCTION__ );
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
UINT32 VSync_CalculateFrameDuration(UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv)
{
	UINT32		ui32FrameDuration90K = 0;

	UINT32		ui32FrameRateRes_Scaled;
	UINT32		ui32_90kHz_Scaled = 90000;
	UINT32		ui32Gcd;
	UINT32		ui320xFFFFFFFF_Boundary;
	UINT32		ui32FrameRateDiv_Temp;

	const UINT32	ui32ScaleUnit = 10;	// must > 1

	if( ui32FrameRateRes == 0 )
		return 0;

	ui32Gcd = VSync_HAL_UTIL_GCD( ui32_90kHz_Scaled, ui32FrameRateRes );
	ui32_90kHz_Scaled /= ui32Gcd;
	ui32FrameRateRes_Scaled = ui32FrameRateRes / ui32Gcd;

	ui320xFFFFFFFF_Boundary = 0xFFFFFFFF / ui32_90kHz_Scaled;

	ui32FrameRateDiv_Temp = ui32FrameRateDiv;
	while( ui32_90kHz_Scaled )
	{
		if( ui32FrameRateDiv_Temp <= ui320xFFFFFFFF_Boundary )
			break;

		ui32FrameRateDiv_Temp /= ui32ScaleUnit;
		ui32_90kHz_Scaled /= ui32ScaleUnit;
		ui32FrameRateRes_Scaled /= ui32ScaleUnit;
	}

	// FrameDuration90K = 90KHz / (FrameRateRes / FrameRateDiv)
	ui32FrameDuration90K = ui32_90kHz_Scaled * ui32FrameRateDiv / ui32FrameRateRes_Scaled; // = 90kHz / FrameRate

	return ui32FrameDuration90K;
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
UINT32 VSync_GetFrameRateResDiv(UINT8 ui8VSyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8VSyncCh >= VSYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Channel Number, %s", ui8VSyncCh, __FUNCTION__ );
		return 0;
	}

	*pui32FrameRateRes = gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual;
	*pui32FrameRateDiv = gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider;

	if( *pui32FrameRateDiv ==  0 )
		return 0;

	return *pui32FrameRateRes / *pui32FrameRateDiv;
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
UINT32 VSync_GetFrameRateDuration(UINT8 ui8VSyncCh)
{
	if( ui8VSyncCh >= VSYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Channel Number, %s", ui8VSyncCh, __FUNCTION__ );
		return 0;
	}

	return gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K;
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
BOOLEAN VSync_SetVsyncField(UINT8 ui8VSyncCh, E_DE_IF_DST_T eDeIfDstCh, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bInterlaced)
{
	BOOLEAN		bUpdated = FALSE;

	if( ui8VSyncCh >= VSYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Channel Number, %s", ui8VSyncCh, __FUNCTION__ );
		return FALSE;
	}

	if( gsVSync[ui8VSyncCh].Config.bFixedVSync == TRUE )
	{
		return FALSE;
	}

	if( (ui32FrameRateRes == 0) || (ui32FrameRateDiv == 0) )
	{
//		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] FrameRateRes:%d, FrameRateDiv:%d, %s", ui8VSyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);
		return FALSE;
	}

	switch( eDeIfDstCh )
	{
	case DE_IF_DST_DE_IF0 :
	case DE_IF_DST_DE_IF1 :
		if( (gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual != ui32FrameRateRes) ||
			(gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider != ui32FrameRateDiv) ||
			(gsVSync[ui8VSyncCh].Rate.bInterlaced != bInterlaced) )
		{
			UINT32		ui32FrameDuration90K;
			UINT32		ui32VsyncDuration90K;

			ui32FrameDuration90K = VSync_CalculateFrameDuration( ui32FrameRateRes, ui32FrameRateDiv );
			ui32VsyncDuration90K = (bInterlaced == TRUE) ? ui32FrameDuration90K / 2 : ui32FrameDuration90K;

			if( ui32VsyncDuration90K < 1499 ) // 60Hz +- 500ppm: 59.97Hz ~ 60.03Hz: 1500.75 ~ 1499.25
			{
				VDEC_KDRV_Message(_TRACE, "[VSync%d][Err] Over Sync Capability - Field Duration: %d, %c", ui8VSyncCh,
											ui32VsyncDuration90K, (gsVSync[ui8VSyncCh].Rate.bInterlaced == TRUE) ? 'i' : 'p');

				ui32FrameRateDiv *= 2;
				ui32FrameDuration90K *= 2;
				ui32VsyncDuration90K *= 2;
			}

			gsVSync[ui8VSyncCh].Rate.ui32FrameRateResidual = ui32FrameRateRes;
			gsVSync[ui8VSyncCh].Rate.ui32FrameRateDivider = ui32FrameRateDiv;
			gsVSync[ui8VSyncCh].Rate.bInterlaced = bInterlaced;

			if( gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K != ui32VsyncDuration90K )
			{
				VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] FieldDuration90k: %d --> %d", ui8VSyncCh, gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K, ui32VsyncDuration90K);
				gsVSync[ui8VSyncCh].Rate.ui32FrameDuration90K = ui32FrameDuration90K;
				gsVSync[ui8VSyncCh].Rate.ui32VsyncDuration90K = ui32VsyncDuration90K;
				VSync_HAL_SetVsyncField(ui8VSyncCh, ui32FrameRateRes, ui32FrameRateDiv, bInterlaced);
				bUpdated = TRUE;
			}
		}
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[DE_IF%d][Err] %s", eDeIfDstCh, __FUNCTION__ );
		return SYNC_INVALID_CHANNEL;
	}

	return bUpdated;
}

