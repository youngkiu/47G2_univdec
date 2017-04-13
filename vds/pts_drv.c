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
 * date       2011.06.14
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
#include "pts_drv.h"
#include "sync_drv.h"
#include "de_if_drv.h"
#include "vsync_drv.h"
#include "../mcu/ipc_req.h"

#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#include "base_drv.h"

#include "../mcu/os_adap.h"

#include "disp_q.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../hal/lg1152/lq_hal_api.h"
#include "../hal/lg1152/av_lipsync_hal_api.h"

#include "../mcu/vdec_shm.h"
#include "vdec_rate.h"

#include "../mcu/ram_log.h"
#include "../mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define	PTS_GET_PTS_OFFSET( _prev, _curr )	\
				(((_curr) < (_prev)) ? ((_curr) + 0x10000000 - (_prev)) : (_curr) - (_prev))
#define	PTS_CHECK_PTS_WRAPAROUND( _pts)	((_pts) & 0x0FFFFFFF)

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS))
typedef struct
{
	UINT8	ui8SyncCh;
	UINT32	ui32BaseTime;
	UINT32	ui32BasePTS;
} S_IPC_BASETIME_T;
#endif

typedef enum
{
	PTS_CONTINUITY_NONE = 0,
	PTS_CONTINUITY_LITTLE,
	PTS_CONTINUITY_NORMAL,
	PTS_CONTINUITY_SAME,
	PTS_CONTINUITY_JITTER,
	PTS_CONTINUITY_WRAPAROUND,
	PTS_CONTINUITY_UINT32 = 0x20110809
} E_PTS_CONTINUITY_T;

typedef enum
{
	PTS_DQ_NONE = 0,
	PTS_DQ_UNDERRUN,
	PTS_DQ_NORMAL,
	PTS_DQ_OVERFLOW,
	PTS_DQ_UINT32 = 0x20110716
} E_PTS_FLOW_T;

typedef struct
{
	struct
	{
		UINT8		ui8Vch;
		UINT8		ui8SrcBufCh;
		E_PTS_MATCH_MODE_T		eMatchMode;
		UINT32 		ui32DisplayOffset_ms;
		UINT8		bDualDecoding;
		UINT32		ui32FlushAge;
	} Config;
	struct
	{
		UINT32		ui32DTS_Prev_Org;
		UINT32		ui32PTS_Prev_Org;
		UINT32		ui32TS_Prev_Mdf;
		UINT32		ui32STC_Prev;
		UINT32		ui32STC_Matched;
		E_PTS_CONTINUITY_T		ePtsContinuity;

		S_DISPQ_BUF_T	sClearDisBuf;
		E_STC_PTS_MATCHED_T eClearDisBuf_PtsMatched;

		UINT32		ui32FrameSkipCount;	// When underrun, Count vsync
		UINT32		ui32PtsContinuityCount;
		UINT32		ui32SmoothWaitingCount;
		UINT32		ui32SmoothDiscardCount;
		UINT32		ui32DisplayContinuityCount;

		UINT32		ui32DisplayFrameCount;

		BOOLEAN		bUseDTS;

		BOOLEAN		bHighResolution;

		UINT32		ui32DualTickDiff;

	} Status;
	struct
	{
		UINT32		ui32BaseTime_Prev;
		UINT32		ui32BaseTime;
		UINT32		ui32BasePTS;
		UINT32		ui32PauseTime;
	} Rate;
	struct
	{
		UINT32		ui32LogTick_GetNew;
		UINT32		ui32LogTick_Noti;
		UINT32		ui32LogTick_Match1;
		UINT32		ui32LogTick_Match2;
		UINT32		ui32LogTick_Pause;
		UINT32		ui32LogTick_NoFrameRate;

		UINT32		ui32MatchedFrameCount;
		UINT32		ui32MatchingFrameDuration;
		UINT32		ui32PopFrameCnt;
	} Debug;
	struct
	{
		UINT32		ui32GSTC;
		BOOLEAN		bInput;
		BOOLEAN		bDisplay;
		BOOLEAN		bDrop;
	} Sync;
}S_VDEC_PTS_DB_T;



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
volatile S_VDEC_PTS_DB_T *gsPts = NULL;
#ifndef __XTENSA__
static void (*_gfpVDS_DispInfo)(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo);
static void (*_gfpVDS_DqStatus)(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus);
#endif


#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS))
#ifdef __XTENSA__
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
static void _IPC_REQ_Send_BaseTime(UINT8 ui8SyncCh, UINT32 ui32BaseTime, UINT32 ui32BasePTS)
{
	S_IPC_BASETIME_T sBaseTime;

	VDEC_KDRV_Message(DBG_DEIF, "[PTS%d][DBG] 0x%08X:0x%08X, %s", ui8SyncCh, ui32BaseTime, ui32BasePTS, __FUNCTION__ );

	sBaseTime.ui8SyncCh = ui8SyncCh;
	sBaseTime.ui32BaseTime = ui32BaseTime;
	sBaseTime.ui32BasePTS = ui32BasePTS;
	IPC_REQ_Send(IPC_REQ_ID_SET_BASETIME, sizeof(S_IPC_BASETIME_T) , (void *)&sBaseTime);
}
#else // #ifdef __XTENSA__
static void _PTS_Set_AVLipSyncBase(UINT8 ui8SyncCh, UINT32 ui32BaseTime, UINT32 ui32BasePTS);
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
static void _IPC_REQ_Receive_BaseTime(void *pIpcBody)
{
	S_IPC_BASETIME_T *pIpcBaseTime;

	pIpcBaseTime = (S_IPC_BASETIME_T *)pIpcBody;

	VDEC_KDRV_Message(DBG_DEIF, "[PTS][DBG] 0x%08X:0x%08X, %s", pIpcBaseTime->ui32BaseTime, pIpcBaseTime->ui32BasePTS, __FUNCTION__ );

	_PTS_Set_AVLipSyncBase( pIpcBaseTime->ui8SyncCh, pIpcBaseTime->ui32BaseTime, pIpcBaseTime->ui32BasePTS );
}
#endif // #ifdef __XTENSA__
#endif // #if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS))

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
static void _PTS_Set_AVLipSyncBase(UINT8 ui8SyncCh, UINT32 ui32BaseTime, UINT32 ui32BasePTS)
{
	UINT8	ui8SrcBufCh = gsPts[ui8SyncCh].Config.ui8SrcBufCh;

	if( ui8SrcBufCh == 0xFF )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Not Use GSTC, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( ((ui32BaseTime != 0xFFFFFFFF) && (ui32BasePTS == 0xFFFFFFFF)) &&
		((ui32BaseTime == 0xFFFFFFFF) && (ui32BasePTS != 0xFFFFFFFF)) )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][ERR] Set Base Time:0x%08X, PTS:0x%08X, %s", ui8SyncCh, ui32BaseTime, ui32BasePTS, __FUNCTION__);
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Set Base Time:0x%08X, PTS:0x%08X, %s(%d)", ui8SyncCh, ui32BaseTime, ui32BasePTS, __FUNCTION__, __LINE__);

#if defined(__XTENSA__)
	_IPC_REQ_Send_BaseTime(ui8SyncCh, ui32BaseTime, ui32BasePTS);
#else
	BASE_AVLIPSYNC_LOCK();

	if( (ui32BaseTime != 0xFFFFFFFF) && (ui32BasePTS != 0xFFFFFFFF) )
	{
		UINT32	ui32BaseTime_Adec, ui32BasePTS_Adec;

		AV_LipSync_HAL_Adec_GetBase(ui8SrcBufCh, &ui32BaseTime_Adec, &ui32BasePTS_Adec);
		if( (ui32BaseTime_Adec == 0xFFFFFFFF) || (ui32BasePTS_Adec == 0xFFFFFFFF) )
		{
			VDEC_KDRV_Message(DBG_SYS, "[PTS%d][DBG] Set Base Time:0x%08X, PTS:0x%08X from VDEC 1, (ADEC: Base Time:0x%08X, PTS:0x%08X)", ui8SyncCh, ui32BaseTime, ui32BasePTS, ui32BaseTime_Adec, ui32BasePTS_Adec);
		}
		else if( ui32BaseTime_Adec == gsPts[ui8SyncCh].Rate.ui32BaseTime_Prev )
		{
			VDEC_KDRV_Message(DBG_SYS, "[PTS%d][DBG] Set Base Time:0x%08X, PTS:0x%08X from VDEC 2, (ADEC: Base Time:0x%08X, PTS:0x%08X)", ui8SyncCh, ui32BaseTime, ui32BasePTS, ui32BaseTime_Adec, ui32BasePTS_Adec);
			VDEC_KDRV_Message(ERROR, "[PTS%d][DBG] Set Base Time:0x%08X, PTS:0x%08X from VDEC 2, (ADEC: Base Time:0x%08X, PTS:0x%08X)", ui8SyncCh, ui32BaseTime, ui32BasePTS, ui32BaseTime_Adec, ui32BasePTS_Adec);
		}
		else
		{
			VDEC_KDRV_Message(DBG_SYS, "[PTS%d][DBG] Set Base Time:0x%08X, PTS:0x%08X from ADEC, (VDEC: Base Time:0x%08X, PTS:0x%08X)", ui8SyncCh, ui32BaseTime_Adec, ui32BasePTS_Adec, ui32BaseTime, ui32BasePTS);

			ui32BaseTime = ui32BaseTime_Adec;
			ui32BasePTS = ui32BasePTS_Adec;
		}

		if( gsPts[ui8SyncCh].Rate.ui32BaseTime != 0xFFFFFFFF )
			gsPts[ui8SyncCh].Rate.ui32BaseTime_Prev = gsPts[ui8SyncCh].Rate.ui32BaseTime;
		gsPts[ui8SyncCh].Rate.ui32BaseTime = ui32BaseTime;
		gsPts[ui8SyncCh].Rate.ui32BasePTS = ui32BasePTS;
	}

	AV_LipSync_HAL_Vdec_SetBase(ui8SrcBufCh, ui32BaseTime, ui32BasePTS);

	BASE_AVLIPSYNC_UNLOCK();
#endif
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
static void _PTS_Noti_DisplayInfo(S_VDS_CALLBACK_BODY_DISPINFO_T *psNotiDispinfo)
{
#ifdef __XTENSA__
	IPC_REQ_Send(IPC_REQ_ID_NOTI_DISPINFO, sizeof(S_VDS_CALLBACK_BODY_DISPINFO_T), (void *)psNotiDispinfo);
#else
	_gfpVDS_DispInfo( psNotiDispinfo );
#endif
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
static void _PTS_Noti_DqStatus(S_VDS_CALLBACK_BODY_DQSTATUS_T *psNotiDqStatus)
{
#ifdef __XTENSA__
	IPC_REQ_Send(IPC_REQ_ID_REPORT_DQ, sizeof(S_VDS_CALLBACK_BODY_DQSTATUS_T), (void *)psNotiDqStatus);
#else
	_gfpVDS_DqStatus( psNotiDqStatus );
#endif
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
#ifdef __XTENSA__
void PTS_Init( void )
#else
void PTS_Init( void (*_fpVDS_DispInfo)(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo),
				void (*_fpVDS_DqStatus)(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus) )
#endif
{
	UINT32	gsPtsPhy, gsPtsSize;
	UINT8	ui8SyncCh;
	UINT8	ui8SrcBufCh;

	gsPtsSize = sizeof(S_VDEC_PTS_DB_T) * SYNC_NUM_OF_CHANNEL;
#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__))
	gsPtsPhy = IPC_REG_PTS_GetDb();
#else
	gsPtsPhy = VDEC_SHM_Malloc(gsPtsSize);
	IPC_REG_PTS_SetDb(gsPtsPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[PTS][DBG] gsPtsPhy: 0x%X, %s", gsPtsPhy, __FUNCTION__ );

	gsPts = (S_VDEC_PTS_DB_T *)VDEC_TranselateVirualAddr(gsPtsPhy, gsPtsSize);

	for( ui8SyncCh = 0; ui8SyncCh < SYNC_NUM_OF_CHANNEL; ui8SyncCh++ )
	{
		gsPts[ui8SyncCh].Config.ui8Vch = 0xFF;
		gsPts[ui8SyncCh].Config.ui8SrcBufCh = 0xFF;
		gsPts[ui8SyncCh].Config.eMatchMode = PTS_MATCH_ENABLE;
		gsPts[ui8SyncCh].Config.ui32DisplayOffset_ms = 0x0;
		gsPts[ui8SyncCh].Config.bDualDecoding = FALSE;
	}

#ifndef __XTENSA__
	_gfpVDS_DispInfo = _fpVDS_DispInfo;
	_gfpVDS_DqStatus = _fpVDS_DqStatus;
#endif

	for( ui8SrcBufCh = 0; ui8SrcBufCh < SRCBUF_NUM_OF_CHANNEL; ui8SrcBufCh++ )
	{
		AV_LipSync_HAL_Vdec_SetBase(ui8SrcBufCh, 0xFFFFFFFF, 0xFFFFFFFF);
	}

	LQ_HAL_Reset(0x8F);

#if (defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__))
	IPC_REQ_Register_ReceiverCallback(IPC_REQ_ID_NOTI_DISPINFO, (IPC_REQ_RECEIVER_CALLBACK_T)_PTS_Noti_DisplayInfo);
	IPC_REQ_Register_ReceiverCallback(IPC_REQ_ID_SET_BASETIME, (IPC_REQ_RECEIVER_CALLBACK_T)_IPC_REQ_Receive_BaseTime);
	IPC_REQ_Register_ReceiverCallback(IPC_REQ_ID_REPORT_DQ, (IPC_REQ_RECEIVER_CALLBACK_T)_PTS_Noti_DqStatus);
#endif
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
UINT8 PTS_Open(UINT8 ui8SyncCh, UINT8 ui8SrcBufCh, UINT8 ui8Vch, UINT32 ui32DisplayOffset_ms, UINT32 ui32NumOfDq, UINT8 ui8Vcodec, BOOLEAN bDualDecoding, E_PTS_MATCH_MODE_T eMatchMode, UINT32 ui32FlushAge)
{
	VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Display Offset: %d, SrcBufCh: %d, MatchMode: %d", ui8SyncCh, ui32DisplayOffset_ms, ui8SrcBufCh, eMatchMode );

	LQC_HAL_Init(ui8SyncCh);
	LQ_HAL_Enable(ui8SyncCh);

	gsPts[ui8SyncCh].Config.ui8Vch = ui8Vch;
	gsPts[ui8SyncCh].Config.ui8SrcBufCh = ui8SrcBufCh;
	gsPts[ui8SyncCh].Config.ui32DisplayOffset_ms = ui32DisplayOffset_ms;
	gsPts[ui8SyncCh].Config.bDualDecoding = bDualDecoding;
	gsPts[ui8SyncCh].Config.ui32FlushAge = ui32FlushAge;

	switch( eMatchMode )
	{
	case PTS_MATCH_ENABLE :
	case PTS_MATCH_FREERUN_BASED_SYNC :
	case PTS_MATCH_FREERUN_IGNORE_SYNC :
		gsPts[ui8SyncCh].Config.eMatchMode = eMatchMode;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Match Mode:%d, %s", ui8SyncCh, eMatchMode, __FUNCTION__ );
	}

	if( ui8SrcBufCh != 0xFF )
		_PTS_Set_AVLipSyncBase( ui8SyncCh, 0xFFFFFFFF, 0xFFFFFFFF );

	// Pic
	gsPts[ui8SyncCh].Status.ui32DTS_Prev_Org = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Status.ui32PTS_Prev_Org = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Status.ui32TS_Prev_Mdf = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Status.ui32STC_Prev = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Status.ui32STC_Matched = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Status.ePtsContinuity = PTS_CONTINUITY_NORMAL;

	gsPts[ui8SyncCh].Status.sClearDisBuf.ui32YFrameAddr = 0x0;
	gsPts[ui8SyncCh].Status.eClearDisBuf_PtsMatched = STC_PTS_NONE;

	gsPts[ui8SyncCh].Status.ui32FrameSkipCount = 0;
	gsPts[ui8SyncCh].Status.ui32PtsContinuityCount = 0;
	gsPts[ui8SyncCh].Status.ui32SmoothWaitingCount = 0;
	gsPts[ui8SyncCh].Status.ui32SmoothDiscardCount = 0;
	gsPts[ui8SyncCh].Status.ui32DisplayContinuityCount = 0;

	gsPts[ui8SyncCh].Status.ui32DisplayFrameCount = 0;
	gsPts[ui8SyncCh].Status.bUseDTS = FALSE;
	gsPts[ui8SyncCh].Status.bHighResolution = FALSE;
	gsPts[ui8SyncCh].Status.ui32DualTickDiff = 0;

	gsPts[ui8SyncCh].Rate.ui32BaseTime_Prev = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Rate.ui32BaseTime = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Rate.ui32BasePTS = 0xFFFFFFFF;
	gsPts[ui8SyncCh].Rate.ui32PauseTime = 0xFFFFFFFF;

	gsPts[ui8SyncCh].Debug.ui32LogTick_GetNew = 0;
	gsPts[ui8SyncCh].Debug.ui32LogTick_Noti = 0x80;
	gsPts[ui8SyncCh].Debug.ui32LogTick_Match1 = 0x40;
	gsPts[ui8SyncCh].Debug.ui32LogTick_Match2 = 0xC0;
	gsPts[ui8SyncCh].Debug.ui32LogTick_Pause = 0;
	gsPts[ui8SyncCh].Debug.ui32LogTick_NoFrameRate = 0;

	gsPts[ui8SyncCh].Debug.ui32MatchedFrameCount = 0;
	gsPts[ui8SyncCh].Debug.ui32MatchingFrameDuration = 0;

	gsPts[ui8SyncCh].Debug.ui32PopFrameCnt = 0;

	LQ_HAL_Reset(ui8SyncCh);

	return ui8SyncCh;
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
void PTS_Close(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] %s", ui8SyncCh, __FUNCTION__ );

	LQ_HAL_Disable(ui8SyncCh);

	gsPts[ui8SyncCh].Config.ui8Vch = 0xFF;
	if( gsPts[ui8SyncCh].Config.ui8SrcBufCh != 0xFF )
		AV_LipSync_HAL_Vdec_SetBase(gsPts[ui8SyncCh].Config.ui8SrcBufCh, 0xFFFFFFFF, 0xFFFFFFFF);
	gsPts[ui8SyncCh].Config.ui8SrcBufCh = 0xFF;
	gsPts[ui8SyncCh].Config.eMatchMode = PTS_MATCH_ENABLE;
	gsPts[ui8SyncCh].Config.ui32DisplayOffset_ms = 0x0;
	gsPts[ui8SyncCh].Config.bDualDecoding = FALSE;
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
void PTS_Debug_Set_DisplayOffset(UINT8 ui8SyncCh, UINT32 ui32DisplayOffset_ms)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(ERROR, "[PTS%d][DBG] DisplayOffset: %dms, %s", ui8SyncCh, ui32DisplayOffset_ms, __FUNCTION__);

	gsPts[ui8SyncCh].Config.ui32DisplayOffset_ms = ui32DisplayOffset_ms;
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
void PTS_Set_MatchMode(UINT8 ui8SyncCh, E_PTS_MATCH_MODE_T eMatchMode)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Matched Mode:%d, %s", ui8SyncCh, eMatchMode, __FUNCTION__);

	switch( eMatchMode )
	{
	case PTS_MATCH_ENABLE :
	case PTS_MATCH_FREERUN_BASED_SYNC :
	case PTS_MATCH_FREERUN_IGNORE_SYNC :
		gsPts[ui8SyncCh].Config.eMatchMode = eMatchMode;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Match Mode:%d, %s", ui8SyncCh, eMatchMode, __FUNCTION__ );
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
void PTS_Set_BaseTime( UINT8 ui8SyncCh, UINT8 ui8ClkID, UINT32 ui32BaseTime, UINT32 ui32BasePTS)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( gsPts[ui8SyncCh].Config.ui8SrcBufCh == 0xFF )
	{
//		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Not Use GSTC, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( ui32BasePTS == VDEC_UNKNOWN_PTS )
	{
		VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Unknwon TS, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	ui32BaseTime &= 0x0FFFFFFF;
	ui32BasePTS &= 0x0FFFFFFF;

	if( ui8ClkID == 0xFF )
	{
		if( gsPts[ui8SyncCh].Rate.ui32BaseTime == 0xFFFFFFFF )
			VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Set Base Time:0x%08X, TS:0x%08X, %s(%d)", ui8SyncCh, ui32BaseTime, ui32BasePTS, __FUNCTION__, __LINE__);
		else
			VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Set Base Time:0x%08X, TS:0x%08X, %s", ui8SyncCh, ui32BaseTime, ui32BasePTS, __FUNCTION__);

		_PTS_Set_AVLipSyncBase( ui8SyncCh, ui32BaseTime, ui32BasePTS);
	}
	else
	{
		if( gsPts[ui8SyncCh].Rate.ui32BaseTime != 0xFFFFFFFF )
			gsPts[ui8SyncCh].Rate.ui32BaseTime_Prev = gsPts[ui8SyncCh].Rate.ui32BaseTime;
		gsPts[ui8SyncCh].Rate.ui32BaseTime = ui32BaseTime;
		gsPts[ui8SyncCh].Rate.ui32BasePTS = ui32BasePTS;

		VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Clock ID:%d, Set Base Time:0x%08X, TS:0x%08X, %s", ui8SyncCh, ui8ClkID, ui32BaseTime, ui32BasePTS, __FUNCTION__);
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
void PTS_Get_BaseTime( UINT8 ui8SyncCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	*pui32BaseTime = gsPts[ui8SyncCh].Rate.ui32BaseTime;
	*pui32BasePTS = gsPts[ui8SyncCh].Rate.ui32BasePTS;
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
void PTS_Clear_BaseTime( UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[PTS%d][DBG] Clear Base Time, %s", ui8SyncCh, __FUNCTION__);

	if( gsPts[ui8SyncCh].Config.ui8SrcBufCh != 0xFF )
	{
		if( gsPts[ui8SyncCh].Rate.ui32BaseTime != 0xFFFFFFFF )
			gsPts[ui8SyncCh].Rate.ui32BaseTime_Prev = gsPts[ui8SyncCh].Rate.ui32BaseTime;
		gsPts[ui8SyncCh].Rate.ui32BaseTime = 0xFFFFFFFF;
		gsPts[ui8SyncCh].Rate.ui32BasePTS = 0xFFFFFFFF;
		gsPts[ui8SyncCh].Rate.ui32PauseTime = 0xFFFFFFFF;
		_PTS_Set_AVLipSyncBase( ui8SyncCh, 0xFFFFFFFF, 0xFFFFFFFF);
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
BOOLEAN PTS_IsDTSMode(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[PTS%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	return gsPts[ui8SyncCh].Status.bUseDTS;
}

