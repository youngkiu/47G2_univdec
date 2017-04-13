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
 * date       2011.06.09
 * note       Additional information.
 *
 * @addtogroup lg115x_dpb
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "vdec_dpb.h"

#include "hal/lg1152//mcu_hal_api.h"
#include "hal/lg1152//ipc_reg_api.h"
#include "mcu/vdec_shm.h"
#include "mcu/os_adap.h"

#include "mcu/vdec_print.h"

#include "hal/lg1152/lg1152_vdec_base.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define DPB_NUM_OF_POOL			(2)

#define DPB_FRAGMENT_SIZE		(0x4000 * 0x4)

#define NUM_OF_MAX_FRAGMENT	(VDEC_DPB_SIZE/DPB_FRAGMENT_SIZE)
#define DPB_MEM_MAX_NUM_OF_DIVIDE ((NUM_OF_MAX_FRAGMENT+0xFF)&~0xFF)

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT32	ui32PoolAddr;
	UINT32	ui32PoolSize;

	UINT32	ui32TotalNumOfFragment;
	UINT32	ui32UsedFragment[DPB_MEM_MAX_NUM_OF_DIVIDE/32];	// Bit Flag

	UINT32	ui32_AllocedFragmentSize[DPB_MEM_MAX_NUM_OF_DIVIDE];
	UINT32	ui32RemainingNumOfFragment;
} S_VDEC_DPB_DB_T;

static UINT32	ui32VirPoolAddr[DPB_NUM_OF_POOL];

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
volatile S_VDEC_DPB_DB_T *gsDpbPool[2] = {NULL, NULL};


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
#if (!defined(USE_MCU_FOR_TOP) && defined(__XTENSA__))
void VDEC_DPB_Init(UINT32 ui32DpbPoolIndex)
#else
void VDEC_DPB_Init(UINT32 ui32DpbPoolIndex, UINT32 ui32PoolAddr, UINT32 ui32PoolSize)
#endif
{
	UINT32	gsDpbPhy, gsDpbSize;

	gsDpbSize = sizeof(S_VDEC_DPB_DB_T);
#if (!defined(USE_MCU_FOR_TOP) && defined(__XTENSA__))
	gsDpbPhy = IPC_REG_DPB_GetDb();
#else
	if( ui32DpbPoolIndex >= DPB_NUM_OF_POOL )
		return;

	gsDpbPhy = VDEC_SHM_Malloc(gsDpbSize);
	IPC_REG_DPB_SetDb(gsDpbPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] gsDpbPhy: 0x%X, %s", gsDpbPhy, __FUNCTION__ );

	gsDpbPool[ui32DpbPoolIndex] = (S_VDEC_DPB_DB_T *)VDEC_TranselateVirualAddr(gsDpbPhy, gsDpbSize);

#if (!(!defined(USE_MCU_FOR_TOP) && defined(__XTENSA__)))
{
	UINT32	i;

	// DE constraint : buffer start address should be multiful of 16KB
	// Alignment of 0x4000 bytes is needed for DE
	gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr = ui32PoolAddr;
	gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr = (gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr + (0x4000-1)) & 0xFFFFC000;
	gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize = ui32PoolSize;
	gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize = (gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize) & 0xFFFFC000;

	ui32VirPoolAddr[ui32DpbPoolIndex] = (UINT32)VDEC_TranselateVirualAddr(gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr, gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize);

	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Addr:0x%X, Size:0x%X, %s", gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr, gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize, __FUNCTION__);

	// 1920 * 1080 = almost 0x4000 * 0x80
	// 1920 * 1080 * 0.5 = almost 0x4000 * 0x40
	// 0x40000 = 0x4000 * 0x10
	// 2048 * 1088 = 0x4000 * 0x88
	// 2048 * 1088 * 0.5 = 0x4000 * 0x44

	// 0x4BA0000 / (0x4000 * 0x4) = 1210
	// 0x4BA0000 / (2048 * 1088 * 1.5 + 0x40000) = 22
	gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment = gsDpbPool[ui32DpbPoolIndex]->ui32PoolSize / DPB_FRAGMENT_SIZE;
	gsDpbPool[ui32DpbPoolIndex]->ui32RemainingNumOfFragment = gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment;
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Num of Fragments: %d, MAX: %d", gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment, DPB_MEM_MAX_NUM_OF_DIVIDE);
	if( gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment > DPB_MEM_MAX_NUM_OF_DIVIDE )
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Num of Fragments: %d, MAX: %d", gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment, DPB_MEM_MAX_NUM_OF_DIVIDE);
	}

	for( i = 0; i < (DPB_MEM_MAX_NUM_OF_DIVIDE/32); i++ )
		gsDpbPool[ui32DpbPoolIndex]->ui32UsedFragment[i] = 0;

	for( i = 0; i < DPB_MEM_MAX_NUM_OF_DIVIDE; i++ )
		gsDpbPool[ui32DpbPoolIndex]->ui32_AllocedFragmentSize[i] = 0;
}
#endif
}

UINT32 VDEC_DPB_TranslatePhyToVirtual(UINT32 ui32PhyAddr)
{
	return ui32VirPoolAddr[1] + (ui32PhyAddr - gsDpbPool[1]->ui32PoolAddr);
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
static UINT32 _VDEC_DPB_Malloc(UINT32 ui32DpbPoolIndex, UINT32 ui32NeedFragments)
{
	UINT32	ui32FragmentsStart, ui32FragmentsCount;
	UINT32	ui32FragmentsStart_Forward, ui32FragmentsEnd_Forward;
	UINT32	ui32FragmentsStart_Backward, ui32FragmentsEnd_Backward;
	UINT32	ui32FbAddr;
	SINT32	i;


	if( gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr == 0 )	// Not Initialised
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Not Initialised, %s", __FUNCTION__ );
		return 0x0;
	}

	// Forward
	ui32FragmentsStart = DPB_MEM_MAX_NUM_OF_DIVIDE;	// invalid value
	ui32FragmentsCount = 0;
	for( i = 0; i < DPB_MEM_MAX_NUM_OF_DIVIDE; i++ )
	{
		if( gsDpbPool[ui32DpbPoolIndex]->ui32UsedFragment[i / 32] & (0x1 << (i % 32)) )
		{
			ui32FragmentsStart = DPB_MEM_MAX_NUM_OF_DIVIDE;	// invalid value
			ui32FragmentsCount = 0;
		}
		else
		{
			if( ui32FragmentsStart == DPB_MEM_MAX_NUM_OF_DIVIDE )
			{
				ui32FragmentsStart = i;
				ui32FragmentsCount = 1;
			}
			else
			{
				ui32FragmentsCount++;
			}
		}

		if( ui32FragmentsCount >= ui32NeedFragments )
			break;
	}
	if( i == DPB_MEM_MAX_NUM_OF_DIVIDE )
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Failed to Forward Alloc FB Memory: %d(%d/%d), %s", ui32NeedFragments, gsDpbPool[ui32DpbPoolIndex]->ui32RemainingNumOfFragment, gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment, __FUNCTION__ );
		return 0x0;
	}

	ui32FragmentsStart_Forward = ui32FragmentsStart;
	ui32FragmentsEnd_Forward = ui32FragmentsStart + (ui32FragmentsCount - 1);
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Alloc Forward (%d-%d/%d)", ui32FragmentsStart_Forward, ui32FragmentsEnd_Forward, gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment);

	// Backward
	ui32FragmentsStart = DPB_MEM_MAX_NUM_OF_DIVIDE;	// invalid value
	ui32FragmentsCount = 0;
	for( i = (gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment - 1); i >= 0; i-- )
	{
		if( gsDpbPool[ui32DpbPoolIndex]->ui32UsedFragment[i / 32] & (0x1 << (i % 32)) )
		{
			ui32FragmentsStart = DPB_MEM_MAX_NUM_OF_DIVIDE;	// invalid value
			ui32FragmentsCount = 0;
		}
		else
		{
			if( ui32FragmentsStart == DPB_MEM_MAX_NUM_OF_DIVIDE )
			{
				ui32FragmentsStart = i;
				ui32FragmentsCount = 1;
			}
			else
			{
				ui32FragmentsCount++;
			}
		}

		if( ui32FragmentsCount >= ui32NeedFragments )
			break;
	}
	if( i == -1 )
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Failed to Backward Alloc FB Memory: %d(%d/%d), %s", ui32NeedFragments, gsDpbPool[ui32DpbPoolIndex]->ui32RemainingNumOfFragment, gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment, __FUNCTION__ );
		return 0x0;
	}
	ui32FragmentsStart_Backward = ui32FragmentsStart - (ui32FragmentsCount - 1);
	ui32FragmentsEnd_Backward = ui32FragmentsStart;
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Alloc Backward (%d-%d/%d)", ui32FragmentsStart_Backward, ui32FragmentsEnd_Backward, gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment);

	// Decide forward or backward
	if( (ui32FragmentsStart_Forward - 0) <= (gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment - ui32FragmentsEnd_Backward) )
		ui32FragmentsStart = ui32FragmentsStart_Forward;
	else
		ui32FragmentsStart = ui32FragmentsStart_Backward;

	// Mark used fragments
	for( i = ui32FragmentsStart; i < (ui32FragmentsStart + ui32FragmentsCount); i++ )
	{
		gsDpbPool[ui32DpbPoolIndex]->ui32UsedFragment[i / 32] |= (0x1 << (i % 32));
	}
	gsDpbPool[ui32DpbPoolIndex]->ui32_AllocedFragmentSize[ui32FragmentsStart] = ui32FragmentsCount;
	gsDpbPool[ui32DpbPoolIndex]->ui32RemainingNumOfFragment -= ui32FragmentsCount;

	ui32FbAddr = gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr + (DPB_FRAGMENT_SIZE * ui32FragmentsStart);
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Alloc Addr: 0x%X, %d ~ # of Fragments: %d", ui32FbAddr, ui32FragmentsStart, ui32NeedFragments);

	return ui32FbAddr;
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
static void _VDEC_DPB_Mfree(UINT32 ui32DpbPoolIndex, UINT32 ui32FbAddr)
{
	UINT32	ui32MemOffset;
	UINT32	ui32FragmentsStart, ui32FragmentsCount;
	UINT32	i;


	if( gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr  == 0 )	// Not Initialised
		return;

	ui32MemOffset = ui32FbAddr - gsDpbPool[ui32DpbPoolIndex]->ui32PoolAddr;

	ui32FragmentsStart = ui32MemOffset / DPB_FRAGMENT_SIZE;
	if( ui32FragmentsStart > gsDpbPool[ui32DpbPoolIndex]->ui32TotalNumOfFragment )
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Failed to Free FB Memory: 0x%X, ui32FragmentsStart: %d, %s", ui32FbAddr, ui32FragmentsStart, __FUNCTION__ );
		return;
	}

	ui32FragmentsCount = gsDpbPool[ui32DpbPoolIndex]->ui32_AllocedFragmentSize[ui32FragmentsStart];
	if( ui32FragmentsCount == 0 )
	{
		VDEC_KDRV_Message(ERROR, "[DPB][Err] Failed to Free FB Memory: 0x%X, %s", ui32FbAddr, __FUNCTION__ );
		return;
	}
	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Free Addr: 0x%X, # of Fragments: %d, %s", ui32FbAddr, ui32FragmentsCount, __FUNCTION__ );

	for( i = ui32FragmentsStart; i < (ui32FragmentsStart + ui32FragmentsCount); i++ )
	{
		gsDpbPool[ui32DpbPoolIndex]->ui32UsedFragment[i / 32] &= ~(0x1 << (i % 32));
	}
	gsDpbPool[ui32DpbPoolIndex]->ui32_AllocedFragmentSize[ui32FragmentsStart] = 0;
	gsDpbPool[ui32DpbPoolIndex]->ui32RemainingNumOfFragment += ui32FragmentsCount;
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
BOOLEAN VDEC_DPB_AllocAll(UINT32 ui32FbWidth, UINT32 ui32FbHeigth, UINT32 ui32NumOfFb, S_VDEC_DPB_INFO_T *psVdecDpb, BOOLEAN bDualDecoding)
{
	UINT32	ui32BufStride;
	UINT32	ui32BufHeight;

	UINT32	ui32FrameSize;
	UINT32	ui32YFrameSize;		// Luma
	UINT32	ui32CFrameSize;		// Chroma
	UINT32	ui32YCFrameSize;

	UINT32	ui32YNeedFragments;
	UINT32	ui32CNeedFragments;
	UINT32	ui32NeedFragments;

	UINT32	ui32FbAddr = 0x0;
	UINT32	ui32DpbPoolIndex;
	UINT32	ui32BufIdx;


	VDEC_KDRV_Message(_TRACE, "[DPB][DBG] Width: %d, Height: %d, Num: %d, %s", ui32FbWidth, ui32FbHeigth, ui32NumOfFb, __FUNCTION__ );

	ui32BufStride = (ui32FbWidth + 0xF) & (~0xF);
	ui32BufHeight = (ui32FbHeigth + 0x1F) & (~0x1F);

	ui32FrameSize = ui32BufStride * ui32BufHeight;
	ui32YFrameSize = ui32FrameSize;
	ui32CFrameSize = (ui32FrameSize + 1) /2;

	ui32YNeedFragments = (ui32YFrameSize + (DPB_FRAGMENT_SIZE - 1)) / DPB_FRAGMENT_SIZE;
	ui32CNeedFragments = (ui32CFrameSize + (DPB_FRAGMENT_SIZE - 1)) / DPB_FRAGMENT_SIZE;

	ui32NeedFragments =  (ui32YNeedFragments + ui32CNeedFragments) * ui32NumOfFb;

	if( (bDualDecoding == TRUE) && (gsDpbPool[0] != NULL) )
	{
		if( (gsDpbPool[0]->ui32RemainingNumOfFragment >= ui32NeedFragments) &&
			(gsDpbPool[1]->ui32RemainingNumOfFragment >= ui32NeedFragments) )
		{
			if( (gsDpbPool[0]->ui32RemainingNumOfFragment - ui32NeedFragments) <= (gsDpbPool[1]->ui32RemainingNumOfFragment - ui32NeedFragments) )
				ui32DpbPoolIndex = 0;
			else
				ui32DpbPoolIndex = 1;
		}
		else if( gsDpbPool[0]->ui32RemainingNumOfFragment >= ui32NeedFragments )
		{
			ui32DpbPoolIndex = 0;
		}
		else if( gsDpbPool[1]->ui32RemainingNumOfFragment >= ui32NeedFragments )
		{
			ui32DpbPoolIndex = 1;
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[DPB][Err] Not Enough DBP Pool - Remaining: %d|%d, %s", gsDpbPool[0]->ui32RemainingNumOfFragment, gsDpbPool[1]->ui32RemainingNumOfFragment, __FUNCTION__);
			return FALSE;
		}
	}
	else
	{
		ui32DpbPoolIndex = 1;
	}

	ui32FbAddr = _VDEC_DPB_Malloc( ui32DpbPoolIndex, ui32NeedFragments );
	if( ui32FbAddr == 0x0 )
		return FALSE;

	psVdecDpb->ui32DpbPoolIndex = ui32DpbPoolIndex;

	psVdecDpb->ui32NumOfFb = ui32NumOfFb;

	psVdecDpb->ui32BaseAddr = ui32FbAddr;

	psVdecDpb->ui32FrameSizeY = ui32YNeedFragments * DPB_FRAGMENT_SIZE;
	psVdecDpb->ui32FrameSizeCb = ui32CNeedFragments * DPB_FRAGMENT_SIZE;
	psVdecDpb->ui32FrameSizeCr = 0;

	ui32YCFrameSize = psVdecDpb->ui32FrameSizeY + psVdecDpb->ui32FrameSizeCb + psVdecDpb->ui32FrameSizeCr;
	for (ui32BufIdx=0; ui32BufIdx < psVdecDpb->ui32NumOfFb; ui32BufIdx++)
	{
		psVdecDpb->aBuf[ui32BufIdx].ui32AddrY = psVdecDpb->ui32BaseAddr + (ui32YCFrameSize * ui32BufIdx);
		psVdecDpb->aBuf[ui32BufIdx].ui32AddrCb = psVdecDpb->ui32BaseAddr + (ui32YCFrameSize * ui32BufIdx) + psVdecDpb->ui32FrameSizeY;
		psVdecDpb->aBuf[ui32BufIdx].ui32AddrCr = psVdecDpb->ui32BaseAddr + (ui32YCFrameSize * ui32BufIdx) + psVdecDpb->ui32FrameSizeY + psVdecDpb->ui32FrameSizeCb;
	}

	VDEC_KDRV_Message(_TRACE, "[DPB%d][DBG] Base: 0x%X, %s", ui32DpbPoolIndex, psVdecDpb->ui32BaseAddr, __FUNCTION__);

	return TRUE;
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
void VDEC_DPB_FreeAll(S_VDEC_DPB_INFO_T *psVdecDpb)
{
	_VDEC_DPB_Mfree(psVdecDpb->ui32DpbPoolIndex, psVdecDpb->ui32BaseAddr);

	psVdecDpb->ui32NumOfFb = 0;
	psVdecDpb->ui32BaseAddr = 0x0;
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
UINT32 VDEC_DPB_Alloc(UINT32 ui32DpbSize)
{
	UINT32	ui32NeedFragments;

	ui32NeedFragments = (ui32DpbSize + (DPB_FRAGMENT_SIZE - 1)) / DPB_FRAGMENT_SIZE;

	return _VDEC_DPB_Malloc( 1, ui32NeedFragments );
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
void VDEC_DPB_Free(UINT32 ui32FbAddr)
{
	_VDEC_DPB_Mfree(1, ui32FbAddr);
}


