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
#include "sync_drv.h"
#include "de_if_drv.h"
#include "vsync_drv.h"
#include "pts_drv.h"
#include "vdec_rate.h"

#include "../mcu/ipc_req.h"
#include "../mcu/mutex_ipc.h"

#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <asm/barrier.h>
#include <linux/delay.h>

#include "../mcu/os_adap.h"

#include "disp_q.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../hal/lg1152/lq_hal_api.h"
#include "../hal/lg1152/av_lipsync_hal_api.h"

#include "../mcu/vdec_shm.h"

#include "../mcu/ram_log.h"
#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		SYNC_NUM_OF_CMD_Q				0x20

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef enum
{
	SYNC_CMD_NONE = 0,
	SYNC_CMD_PLAY,
	SYNC_CMD_STEP,
	SYNC_CMD_FREEZE,
	SYNC_CMD_PAUSE,
	SYNC_CMD_CLOSE,
	SYNC_CMD_RESET,
	SYNC_CMD_FLUSH,
	SYNC_CMD_UINT32 = 0x20110915
} E_SYNC_CMD_T;

typedef enum
{
	SYNC_OPENED = 0,
	SYNC_CLOSED = 10,
	SYNC_CLOSING,
	SYNC_PLAY_READY = 20,
	SYNC_PLAYING,
	SYNC_PLAYING_FREEZE,
	SYNC_STEP,
	SYNC_PAUSED = 30,
	SYNC_STATE_UINT32 = 0x20110716
} E_SYNC_STATE_T;

typedef struct
{
	E_SYNC_STATE_T	State_Prev;
	E_SYNC_STATE_T	State;
	struct
	{
		E_DE_IF_DST_T	eDeIfDstCh;
		UINT8 			ui8VSyncCh;
		UINT8			ui8ClkID;
		UINT32			ui32FlushAge;
	} Config;
	struct
	{
		E_SYNC_CMD_T	eCmd[SYNC_NUM_OF_CMD_Q];
		volatile UINT32		ui32WrIndex;
		volatile UINT32		ui32RdIndex;
	} CmdQ;
	struct
	{
		UINT32			ui32FbId;
		S_DISPQ_BUF_T	sRunningDisBuf;
	} Status;
	struct
	{
		UINT32		ui32LogTick_UpdateDeIf;
		UINT32		ui32LogTick_UpdateDispQ;
	} Debug;
}S_VDEC_LIPSYNC_DB_T;



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
volatile S_VDEC_LIPSYNC_DB_T *gsSync = NULL;


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
static BOOLEAN _SYNC_PutCmdQ(UINT8 ui8SyncCh, E_SYNC_CMD_T eCmd)
{
	UINT32		ui32WrIndex, ui32WrIndex_Next;
	UINT32		ui32RdIndex;

	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	ui32WrIndex = gsSync[ui8SyncCh].CmdQ.ui32WrIndex;
	ui32RdIndex = gsSync[ui8SyncCh].CmdQ.ui32RdIndex;

	ui32WrIndex_Next = ui32WrIndex + 1;
	if( ui32WrIndex_Next == SYNC_NUM_OF_CMD_Q )
		ui32WrIndex_Next = 0;

	if( ui32WrIndex_Next == ui32RdIndex )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Cmd Q Overflow", ui8SyncCh );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] Cmd: %d, %s", ui8SyncCh, gsSync[ui8SyncCh].State, eCmd, __FUNCTION__ );

	gsSync[ui8SyncCh].CmdQ.eCmd[ui32WrIndex] = eCmd;
#ifndef __XTENSA__
	wmb();
#endif
	gsSync[ui8SyncCh].CmdQ.ui32WrIndex = ui32WrIndex_Next;

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
static void _SYNC_Close(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );

	VSync_Close(gsSync[ui8SyncCh].Config.ui8VSyncCh, gsSync[ui8SyncCh].Config.eDeIfDstCh, ui8SyncCh);
	DE_IF_Close(gsSync[ui8SyncCh].Config.eDeIfDstCh);
	PTS_Close(ui8SyncCh);

	gsSync[ui8SyncCh].Config.eDeIfDstCh = 0xFF;
	gsSync[ui8SyncCh].Config.ui8VSyncCh = 0xFF;
	gsSync[ui8SyncCh].Config.ui8ClkID = 0xFF;

	gsSync[ui8SyncCh].State = SYNC_CLOSED;
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
void SYNC_Init( void )
#else
void SYNC_Init( void (*_fpVDS_DispInfo)(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo),
				void (*_fpVDS_DqStatus)(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus) )
#endif
{
	UINT32	gsLsPhy, gsLsSize;
	UINT8	ui8SyncCh;

	gsLsSize = sizeof(S_VDEC_LIPSYNC_DB_T) * SYNC_NUM_OF_CHANNEL;
#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__))
	gsLsPhy = IPC_REG_LIPSYNC_GetDb();
#else
	gsLsPhy = VDEC_SHM_Malloc(gsLsSize);
	IPC_REG_LIPSYNC_SetDb(gsLsPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[Sync][DBG] gsLsPhy: 0x%X, %s", gsLsPhy, __FUNCTION__ );

	gsSync = (S_VDEC_LIPSYNC_DB_T *)VDEC_TranselateVirualAddr(gsLsPhy, gsLsSize);

	for( ui8SyncCh = 0; ui8SyncCh < SYNC_NUM_OF_CHANNEL; ui8SyncCh++ )
	{
		gsSync[ui8SyncCh].State_Prev = SYNC_CLOSED;
		gsSync[ui8SyncCh].State = SYNC_CLOSED;
		gsSync[ui8SyncCh].Config.eDeIfDstCh = 0xFF;
		gsSync[ui8SyncCh].Config.ui8VSyncCh = 0xFF;
		gsSync[ui8SyncCh].Config.ui8ClkID = 0xFF;

		gsSync[ui8SyncCh].CmdQ.ui32WrIndex = 0;
		gsSync[ui8SyncCh].CmdQ.ui32RdIndex = 0;

		gsSync[ui8SyncCh].Status.ui32FbId = 0;
		gsSync[ui8SyncCh].Status.sRunningDisBuf.ui32YFrameAddr = 0x0;

		gsSync[ui8SyncCh].Debug.ui32LogTick_UpdateDeIf = 0;
		gsSync[ui8SyncCh].Debug.ui32LogTick_UpdateDispQ = 0;
	}

	VDEC_RATE_Init();
#ifdef __XTENSA__
	PTS_Init();
#else
	PTS_Init(_fpVDS_DispInfo, _fpVDS_DqStatus);
#endif
	VSync_Init();
	DE_IF_Init();
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
UINT8 SYNC_Open(UINT8 ui8Vch, E_SYNC_DST_T eSyncDstCh, UINT8 ui8Vcodec, UINT8 ui8SrcBufCh, UINT32 ui32DisplayOffset_ms, UINT32 ui32NumOfDq, UINT8 ui8ClkID, BOOLEAN bDualDecoding, BOOLEAN bFixedVSync, E_PTS_MATCH_MODE_T eMatchMode, UINT32 ui32FlushAge)
{
	UINT8	ui8SyncCh = (UINT8)eSyncDstCh;
	UINT8	ui8VSyncCh = 0xFF;
	UINT32	ui32WaintCount = 0;

	Mutex_ARM_Lock();

	while( gsSync[ui8SyncCh].State == SYNC_CLOSING )
	{
		if( ui32WaintCount > 0x100 )
			break;

		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Wrn] Closing, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
#ifndef __XTENSA__
		mdelay(10);
#endif
		ui32WaintCount++;
	}

	if( gsSync[ui8SyncCh].State != SYNC_CLOSED )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] State: %d, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
		_SYNC_Close(ui8SyncCh);
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );

	if( DISP_Q_Open(ui8SyncCh, ui32NumOfDq) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Err] Failed to Alloc Display Queue Memory, %s", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );
		ui8SyncCh = 0xFF;
		goto _SYNC_Open_Exit;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] Display Offset: %d, SrcBufCh: %d", ui8SyncCh, gsSync[ui8SyncCh].State, ui32DisplayOffset_ms, ui8SrcBufCh );

	LQC_HAL_Init(ui8SyncCh);
	LQ_HAL_Enable(ui8SyncCh);

	if( DE_IF_Open((E_DE_IF_DST_T)eSyncDstCh) == FALSE )
	{
		DISP_Q_Close(ui8SyncCh);
		ui8SyncCh = 0xFF;
		goto _SYNC_Open_Exit;
	}

	ui8VSyncCh = VSync_Open((E_DE_IF_DST_T)eSyncDstCh, ui8SyncCh, bDualDecoding, bFixedVSync, VDEC_FRAMERATE_MAX, 1, FALSE);
	if( ui8SyncCh == SYNC_INVALID_CHANNEL )
	{
		DISP_Q_Close(ui8SyncCh);
		DE_IF_Close((E_DE_IF_DST_T)eSyncDstCh);
		ui8SyncCh = 0xFF;
		goto _SYNC_Open_Exit;
	}

	PTS_Open(ui8SyncCh, ui8SrcBufCh, ui8Vch, ui32DisplayOffset_ms, ui32NumOfDq, ui8Vcodec, bDualDecoding, eMatchMode, ui32FlushAge);

	VDEC_RATE_Reset(ui8SyncCh);
	VDEC_RATE_SetCodecType(ui8SyncCh, ui8Vcodec);

	gsSync[ui8SyncCh].State_Prev = gsSync[ui8SyncCh].State;
	gsSync[ui8SyncCh].State = SYNC_OPENED;
	gsSync[ui8SyncCh].Config.eDeIfDstCh = (E_DE_IF_DST_T)eSyncDstCh;
	gsSync[ui8SyncCh].Config.ui8VSyncCh = ui8VSyncCh;
	gsSync[ui8SyncCh].Config.ui8ClkID = ui8ClkID;
	gsSync[ui8SyncCh].Config.ui32FlushAge = ui32FlushAge;

	gsSync[ui8SyncCh].Status.ui32FbId = 0;
	gsSync[ui8SyncCh].Status.sRunningDisBuf.ui32YFrameAddr = 0x0;

_SYNC_Open_Exit :
	Mutex_ARM_Unlock();

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
void SYNC_Close(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	Mutex_ARM_Lock();

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );

	if( gsSync[ui8SyncCh].State == SYNC_CLOSED )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Err] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );
		goto _SYNC_Close_Exit;
	}

	DISP_Q_Close(ui8SyncCh);

	gsSync[ui8SyncCh].State_Prev = gsSync[ui8SyncCh].State;
	gsSync[ui8SyncCh].State = SYNC_CLOSING;

	_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_CLOSE );

_SYNC_Close_Exit :
	Mutex_ARM_Unlock();
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
void SYNC_Debug_Set_DisplayOffset(UINT8 ui8SyncCh, UINT32 ui32DisplayOffset_ms)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(ERROR, "[Sync%d][DBG] DisplayOffset: %dms, %s", ui8SyncCh, ui32DisplayOffset_ms, __FUNCTION__);

	PTS_Debug_Set_DisplayOffset(ui8SyncCh, ui32DisplayOffset_ms);
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
void SYNC_Reset(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );

	_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_RESET );
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
void SYNC_Start(UINT8 ui8SyncCh, E_SYNC_PLAY_OPT_T ePlayOpt)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(WARNING, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	Mutex_ARM_Lock();

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] Play Option:%d, %s ", ui8SyncCh, gsSync[ui8SyncCh].State, ePlayOpt, __FUNCTION__ );

	gsSync[ui8SyncCh].State_Prev = gsSync[ui8SyncCh].State;
	gsSync[ui8SyncCh].State = SYNC_PLAY_READY;
	switch( ePlayOpt )
	{
	case SYNC_PLAY_NORMAL :
		_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_PLAY );
		break;
	case SYNC_PLAY_STEP :
		_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_STEP );
		break;
	case SYNC_PLAY_FREEZE :
		_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_FREEZE );
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Play Option:%d, %s", ui8SyncCh, ePlayOpt, __FUNCTION__ );
		break;
	}

	Mutex_ARM_Unlock();
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
void SYNC_Stop(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] %s ", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__ );

	_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_PAUSE );
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
void SYNC_Flush(UINT8 ui8SyncCh, UINT32 ui32FlushAge)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(WARNING, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	Mutex_ARM_Lock();

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] DPB ID:%d, %s ", ui8SyncCh, gsSync[ui8SyncCh].State, ui32FlushAge, __FUNCTION__ );

	gsSync[ui8SyncCh].Config.ui32FlushAge = ui32FlushAge;

	if( gsSync[ui8SyncCh].Config.ui8ClkID == 0xFF )
		PTS_Clear_BaseTime( ui8SyncCh );

	if( gsSync[ui8SyncCh].State == SYNC_PAUSED )
	{
		gsSync[ui8SyncCh].State_Prev = gsSync[ui8SyncCh].State;
		gsSync[ui8SyncCh].State = SYNC_OPENED;
	}

	_SYNC_PutCmdQ( ui8SyncCh, SYNC_CMD_FLUSH );

	VDEC_RATE_Flush( ui8SyncCh );

	Mutex_ARM_Unlock();
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
void SYNC_Set_MatchMode(UINT8 ui8SyncCh, E_PTS_MATCH_MODE_T eMatchMode)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(WARNING, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d][DBG] Matched Mode:%d, %s", ui8SyncCh, eMatchMode, __FUNCTION__);

	PTS_Set_MatchMode(ui8SyncCh, eMatchMode);
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
BOOLEAN SYNC_IsDTSMode( UINT8 ui8SyncCh )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return PTS_IsDTSMode(ui8SyncCh);
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
void SYNC_Set_Speed( UINT8 ui8SyncCh, SINT32 i32Speed )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(WARNING, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( i32Speed == 0 )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Err] speed = %d, %s", ui8SyncCh, gsSync[ui8SyncCh].State, i32Speed, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[Sync%d-%d][DBG] Speed: %d", ui8SyncCh, gsSync[ui8SyncCh].State, i32Speed);

	VDEC_RATE_Set_Speed(ui8SyncCh, i32Speed);
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
SINT32 SYNC_Get_Speed( UINT8 ui8SyncCh )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_Get_Speed(ui8SyncCh);
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
void SYNC_Set_BaseTime( UINT8 ui8SyncCh, UINT8 ui8ClkID, UINT32 ui32BaseTime, UINT32 ui32BasePTS )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( (ui8ClkID == 0xFF) &&
		((gsSync[ui8SyncCh].State != SYNC_PLAY_READY) && (gsSync[ui8SyncCh].State != SYNC_PLAYING) && (gsSync[ui8SyncCh].State != SYNC_PLAYING_FREEZE)) )
	{
		VDEC_KDRV_Message(WARNING, "[Sync%d-%d][Wrn] Not Play State, Clock ID: %d, %s ", ui8SyncCh, gsSync[ui8SyncCh].State, ui8ClkID, __FUNCTION__ );
		return;
	}

	if( gsSync[ui8SyncCh].Config.ui8ClkID != ui8ClkID )
	{
		if( ui8ClkID != 0xFF )
			VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Clock ID: %d/%d, %s", ui8SyncCh, ui8ClkID, gsSync[ui8SyncCh].Config.ui8ClkID, __FUNCTION__ );

		return;
	}

	PTS_Set_BaseTime(ui8SyncCh, ui8ClkID, ui32BaseTime, ui32BasePTS);
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
void SYNC_Get_BaseTime( UINT8 ui8SyncCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS )
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	PTS_Get_BaseTime(ui8SyncCh, pui32BaseTime, pui32BasePTS);
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
UINT32 SYNC_GetFrameRateResDiv(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_GetFrameRateResDiv(gsSync[ui8SyncCh].Config.ui8VSyncCh, pui32FrameRateRes, pui32FrameRateDiv);
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
UINT32 SYNC_GetFrameRateDuration(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_GetFrameRateDuration(gsSync[ui8SyncCh].Config.ui8VSyncCh);
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
UINT32 SYNC_GetDecodingRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_GetDecodingRate(ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv);
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
UINT32 SYNC_GetDisplayRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_GetDisplayRate(ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv);
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
UINT32 SYNC_GetDropRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return VDEC_RATE_GetDropRate(ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv);
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
BOOLEAN SYNC_GetRunningFb(UINT8 ui8SyncCh, UINT32 *pui32PicWidth, UINT32 *pui32PicHeight, UINT32 *pui32uID, UINT64 *pui64TimeStamp, UINT32 *pui32PTS, UINT32 *pui32FrameAddr)
{
	S_DISPQ_BUF_T	*psDisBuf;

	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	psDisBuf = (S_DISPQ_BUF_T *)&gsSync[ui8SyncCh].Status.sRunningDisBuf;

	if( psDisBuf->ui32YFrameAddr == 0x0 )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] No Running FB, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	*pui32PicWidth = psDisBuf->ui32PicWidth;
	*pui32PicHeight = psDisBuf->ui32PicHeight;
	*pui32FrameAddr = psDisBuf->ui32YFrameAddr;
	if( PTS_IsDTSMode(ui8SyncCh) == TRUE )
	{
		*pui32uID = psDisBuf->sDTS.ui32uID;
		*pui64TimeStamp = psDisBuf->sDTS.ui64TimeStamp;
		*pui32PTS = psDisBuf->sDTS.ui32DTS;
	}
	else
	{
		*pui32uID = psDisBuf->sPTS.ui32uID;
		*pui64TimeStamp = psDisBuf->sPTS.ui64TimeStamp;
		*pui32PTS = psDisBuf->sPTS.ui32PTS;
	}

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
BOOLEAN SYNC_UpdateFrame(UINT8 ui8SyncCh, S_FRAME_BUF_T *psFrameBuf)
{
	S_DISPQ_BUF_T sDisBuf;
	BOOLEAN		bRet = FALSE;

	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	switch( gsSync[ui8SyncCh].State )
	{
	case SYNC_OPENED :
	case SYNC_PAUSED :
		VDEC_KDRV_Message(WARNING, "[Sync%d-%d][Wrn] Current State, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
	case SYNC_PLAY_READY :
	case SYNC_PLAYING :
	case SYNC_PLAYING_FREEZE :
	case SYNC_STEP :
		gsSync[ui8SyncCh].Status.ui32FbId++;
		sDisBuf.ui32FbId 				= gsSync[ui8SyncCh].Status.ui32FbId;
		sDisBuf.ui32YFrameAddr 			= psFrameBuf->ui32YFrameAddr;
		sDisBuf.ui32CFrameAddr 			= psFrameBuf->ui32CFrameAddr;
		sDisBuf.ui32Stride 				= psFrameBuf->ui32Stride;
		sDisBuf.ui32FlushAge 			= psFrameBuf->ui32FlushAge;
		sDisBuf.ui32AspectRatio 		= psFrameBuf->ui32AspectRatio;
		sDisBuf.ui32PicWidth 			= psFrameBuf->ui32PicWidth;
		sDisBuf.ui32PicHeight 			= psFrameBuf->ui32PicHeight;
		sDisBuf.ui32CropLeftOffset 		= psFrameBuf->ui32CropLeftOffset;
		sDisBuf.ui32CropRightOffset 	= psFrameBuf->ui32CropRightOffset;
		sDisBuf.ui32CropTopOffset 		= psFrameBuf->ui32CropTopOffset;
		sDisBuf.ui32CropBottomOffset 	= psFrameBuf->ui32CropBottomOffset;
		sDisBuf.eDisplayInfo 			= psFrameBuf->eDisplayInfo;
		sDisBuf.sDTS.ui32DTS 			= psFrameBuf->sDTS.ui32DTS;
		sDisBuf.sDTS.ui64TimeStamp 		= psFrameBuf->sDTS.ui64TimeStamp;
		sDisBuf.sDTS.ui32uID 			= psFrameBuf->sDTS.ui32uID;
		sDisBuf.sPTS.ui32PTS 			= psFrameBuf->sPTS.ui32PTS;
		sDisBuf.sPTS.ui64TimeStamp 		= psFrameBuf->sPTS.ui64TimeStamp;
		sDisBuf.sPTS.ui32uID 			= psFrameBuf->sPTS.ui32uID;
		sDisBuf.ui32InputGSTC 			= psFrameBuf->ui32InputGSTC;
		sDisBuf.ui32DisplayPeriod 		= psFrameBuf->ui32DisplayPeriod;
		sDisBuf.i32FramePackArrange 	= psFrameBuf->si32FrmPackArrSei;
		sDisBuf.eLR_Order 				= psFrameBuf->eLR_Order;
		sDisBuf.ui16ParW 				= psFrameBuf->ui16ParW;
		sDisBuf.ui16ParH 				= psFrameBuf->ui16ParH;

		sDisBuf.sUsrData.ui32PicType 	= psFrameBuf->sUsrData.ui32PicType;
		sDisBuf.sUsrData.ui32Rpt_ff 	= psFrameBuf->sUsrData.ui32Rpt_ff;
		sDisBuf.sUsrData.ui32Top_ff 	= psFrameBuf->sUsrData.ui32Top_ff;
		sDisBuf.sUsrData.ui32BuffAddr 	= psFrameBuf->sUsrData.ui32BuffAddr;
		sDisBuf.sUsrData.ui32Size 		= psFrameBuf->sUsrData.ui32Size;

		bRet = DISP_Q_Push(ui8SyncCh, &sDisBuf);
 		break;
	case SYNC_CLOSING :
	case SYNC_CLOSED :
	default :
		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Err] Current State, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
	}

	return bRet;
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
BOOLEAN SYNC_UpdateRate(	UINT8 ui8SyncCh,
								UINT32 ui32FrameRateRes_Parser,
								UINT32 ui32FrameRateDiv_Parser,
								UINT32 ui32FrameRateRes_Decoder,
								UINT32 ui32FrameRateDiv_Decoder,
								UINT32 ui32DTS,
								UINT32 ui32PTS,
								UINT32 ui32InputGSTC,
								UINT32 ui32FeedGSTC,
								BOOLEAN	bFieldPicture,
								BOOLEAN	bInterlaced,
								BOOLEAN	bVariableFrameRate,
								UINT32 ui32FlushAge)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;
	UINT32		ui32FrameRate_Parser = 0, ui32FrameRate_Dec = 0, ui32FrameRate_Feed = 0, ui32FrameRate_DTS = 0, ui32FrameRate_PTS = 0, ui32FrameRate_Input = 0;

	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	switch( gsSync[ui8SyncCh].State )
	{
	case SYNC_OPENED :
	case SYNC_PAUSED :
		VDEC_KDRV_Message(WARNING, "[Sync%d-%d][Wrn] Current State, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
	case SYNC_PLAY_READY :
	case SYNC_PLAYING :
	case SYNC_PLAYING_FREEZE :
	case SYNC_STEP :
		VDEC_RATE_UpdateFrameRate_Parser(ui8SyncCh, ui32FrameRateRes_Parser, ui32FrameRateDiv_Parser);
		if( ui32FrameRateDiv_Parser )
			ui32FrameRate_Parser = ui32FrameRateRes_Parser / ui32FrameRateDiv_Parser;
		VDEC_RATE_UpdateFrameRate_Decoder(ui8SyncCh, ui32FrameRateRes_Decoder, ui32FrameRateDiv_Decoder,
												bFieldPicture, bInterlaced, bVariableFrameRate, PTS_IsDTSMode( ui8SyncCh ));
		if( ui32FrameRateDiv_Decoder )
			ui32FrameRate_Dec = ui32FrameRateRes_Decoder / ui32FrameRateDiv_Decoder;
		if( gsSync[ui8SyncCh].Config.ui32FlushAge == ui32FlushAge )
		{
			VDEC_RATE_UpdateFrameRate_byDTS(ui8SyncCh, ui32DTS, &ui32FrameRateRes, &ui32FrameRateDiv);
			if( ui32FrameRateDiv )
				ui32FrameRate_DTS = ui32FrameRateRes / ui32FrameRateDiv;
			VDEC_RATE_UpdateFrameRate_byPTS(ui8SyncCh, ui32PTS, &ui32FrameRateRes, &ui32FrameRateDiv);
			if( ui32FrameRateDiv )
				ui32FrameRate_PTS = ui32FrameRateRes / ui32FrameRateDiv;
		}
		VDEC_RATE_UpdateFrameRate_byFeed(ui8SyncCh, ui32FeedGSTC, &ui32FrameRateRes, &ui32FrameRateDiv);
		if( ui32FrameRateDiv )
			ui32FrameRate_Feed = ui32FrameRateRes / ui32FrameRateDiv;
		VDEC_RATE_UpdateFrameRate_byInput(ui8SyncCh, ui32InputGSTC, &ui32FrameRateRes, &ui32FrameRateDiv);
		if( ui32FrameRateDiv )
			ui32FrameRate_Input = ui32FrameRateRes / ui32FrameRateDiv;

		if( bVariableFrameRate == TRUE )
		{
			ui32FrameRateDiv = 1;
			if( bInterlaced == TRUE )
				ui32FrameRateRes = VDEC_FRAMERATE_MAX / 2;
			else
				ui32FrameRateRes = VDEC_FRAMERATE_MAX;
		}
		else
		{
			VDEC_RATE_GetFrameRateResDiv(ui8SyncCh, &ui32FrameRateRes, &ui32FrameRateDiv);
		}
		VSync_SetVsyncField(gsSync[ui8SyncCh].Config.ui8VSyncCh, gsSync[ui8SyncCh].Config.eDeIfDstCh, ui32FrameRateRes, ui32FrameRateDiv, bInterlaced);

		VDEC_RATE_UpdateFrameRate_byDecDone(ui8SyncCh, TOP_HAL_GetGSTCC(), &ui32FrameRateRes, &ui32FrameRateDiv);

		gsSync[ui8SyncCh].Debug.ui32LogTick_UpdateDispQ++;
		if( gsSync[ui8SyncCh].Debug.ui32LogTick_UpdateDispQ == 0x100 )
		{
			VDEC_KDRV_Message(MONITOR, "[Sync%d][DBG] Occupancy:%d, Rate - Parser:%d, Decoder:%d, DTS:%d, PTS:%d, Feed:%d, Input:%d",
										ui8SyncCh,
										DISP_Q_NumActive(ui8SyncCh),
										ui32FrameRate_Parser, ui32FrameRate_Dec, ui32FrameRate_DTS, ui32FrameRate_PTS, ui32FrameRate_Feed, ui32FrameRate_Input);

			gsSync[ui8SyncCh].Debug.ui32LogTick_UpdateDispQ = 0;
		}
 		break;
	case SYNC_CLOSING :
	case SYNC_CLOSED :
	default :
		VDEC_KDRV_Message(ERROR, "[Sync%d-%d][Err] Current State, %s(%d)", ui8SyncCh, gsSync[ui8SyncCh].State, __FUNCTION__, __LINE__ );
	}

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
UINT32 SYNC_GetFrameBufOccupancy(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= SYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[Sync%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	return DISP_Q_NumActive(ui8SyncCh);
}

