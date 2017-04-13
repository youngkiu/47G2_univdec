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
 * date       2011.02.21
 * note       Additional information.
 *
 * @addtogroup lg115x_ves
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/
#define		L9_A0_WORKAROUND

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "ves_drv.h"

#ifdef __XTENSA__
#include <stdio.h>
#include <string.h>
#else
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h> // copy_from_user
#include <asm/div64.h> // do_div
#include <linux/delay.h>
#endif

#include "../mcu/os_adap.h"

#include "ves_cpb.h"
#include "ves_auib.h"
#include "../hal/lg1152/pdec_hal_api.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/lq_hal_api.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../mcu/vdec_shm.h"

#include "../mcu/ram_log.h"
#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	enum
	{
		VES_NULL = 0,
		VES_READY = 10,
		VES_PLAY = 20,
		VES_PAUSE,
	} State;
	struct
	{
		E_VES_SRC_T	eVesSrcCh;
		UINT8		ui8Vch;
		UINT8		ui8CodecType_Config;
		BOOLEAN		bUseGstc;

		BOOLEAN		bFromTVP;

		UINT32		ui32CpbBufAddr;
		UINT32		ui32CpbBufSize;
	} Config;
	struct
	{
		S_VES_CALLBACK_BODY_ESINFO_T	sVesSeqAu;
		S_PDEC_AUI_T	sPdecAui_Prev;	// for DTV Live
		UINT32			ui32PushedAui;

		volatile BOOLEAN		bFlushing;
	} Status;
	struct
	{
		UINT32		ui32ReceivedAuCnt;
		UINT32		ui32LogTick;
		UINT32		ui32GSTC_Prev;

		UINT32		ui32AuStartAddr_Prev;
		UINT32		ui32AuEndAddr_Prev;
	} Debug;
#ifdef L9_A0_WORKAROUND
	struct
	{
		UINT32		ui32SeqEndCpbOffset;
		BOOLEAN		bEOS_Found;
		UINT32		ui32AuStartAddr_HW;
		UINT32		ui32AddrMismatchingCnt;
	} A0;
#endif

} S_VDEC_VES_DB_T;

typedef struct
{
	UINT8	ui8VesCh;
	UINT32	ui32au_type;
	UINT32	ui32UserBuf;
	UINT32	ui32UserBufSize;
	BOOLEAN	bIsDTS;
	UINT32	ui32uID;
	UINT64	ui64TimeStamp;
	UINT32	ui32TimeStamp_90kHzTick;
	UINT32	ui32FrameRateRes;
	UINT32	ui32FrameRateDiv;
} S_IPC_MCUWRITE_T;


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
#if (!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
UINT32	ui32WorkfuncParam = 0x0;
static void _VDEC_ISR_PIC_Detected_workfunc(struct work_struct *data);

DECLARE_WORK( _VDEC_VES_work, _VDEC_ISR_PIC_Detected_workfunc );
#endif

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static S_VDEC_VES_DB_T *gsVesDrv = NULL;

#if (!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
struct workqueue_struct *_VDEC_VES_workqueue;
#endif
#ifndef __XTENSA__
static spinlock_t	stVdecVesSpinlock;

static void (*_gfpVES_InputInfo)(S_VES_CALLBACK_BODY_ESINFO_T *pInputInfo);
#endif


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
static void _VES_RamLog(UINT64 ui64TimeStamp, UINT32 ui32GSTC, UINT32 ui32Size, UINT32 ui32PTS, UINT32 ui32DTS, UINT32 ui32WrPtr, UINT32 wParam0)
{
	gVESLogMem	sRamLog = {0, };

	sRamLog.ui64TimeStamp = ui64TimeStamp;
	sRamLog.ui32GSTC = ui32GSTC;
	sRamLog.ui32Size = ui32Size;
	sRamLog.ui32PTS = ui32PTS;
	sRamLog.ui32DTS = ui32DTS;
	sRamLog.ui32WrPtr = ui32WrPtr;
	sRamLog.wParam0 = wParam0;

	RAM_LOG_Write(RAM_LOG_MOD_VES, (void *)&sRamLog);
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
static UINT8 _VES_GetTranslateCodecType(UINT8 ui8CodecType_Config)
{
	static UINT8 ui8aVidStdSetInfo[14][5]= {//[0]video standard [1]asp class [2]aux std [3]seq option broad cast [4]seq option file play
	{0x0,0x0,0x0,0x2,0xE}, {0x0,0x0,0x0,0x2,0xE}, {0x1,0x0,0x0,0x0,0xE}, {0x1,0x1,0x0,0x0,0xE},
	{0x1,0x2,0x0,0x0,0xE}, {0x2,0x0,0x0,0x0,0xC}, {0x3,0x0,0x0,0x0,0xC},
	{0x3,0x2,0x0,0x0,0xC}, {0x3,0x0,0x1,0x0,0xC}, {0x3,0x5,0x0,0x0,0xC},
	{0x3,0x1,0x0,0x0,0xC}, {0x4,0x0,0x0,0x0,0xC}, {0x5,0x0,0x0,0x2,0xE},
	{0x8,0x0,0x0,0x0,0xC}};

	UINT8	ui8CodecType_Decode;

	if( ui8CodecType_Config >= 14 )
	{
		VDEC_KDRV_Message(ERROR, "[RATE][Err] Codec Type(%d), %s", ui8CodecType_Config, __FUNCTION__ );
		return 0xFF;
	}

	ui8CodecType_Decode = ui8aVidStdSetInfo[ui8CodecType_Config][0];

	return ui8CodecType_Decode;
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
static void _PDEC_HW_Reset(UINT8 ui8VesCh, UINT8 ui8CodecType_Config, E_VES_SRC_T eVesSrcCh)
{
	BOOLEAN		bIsIntrEnable;

	if( eVesSrcCh >= VES_SRC_BUFF0 )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

#if defined(USE_MCU_FOR_VDEC_VES)
	bIsIntrEnable = TOP_HAL_IsInterIntrEnable(PDEC0_AU_DETECT+ui8VesCh);
#else
	bIsIntrEnable = TOP_HAL_IsExtIntrEnable(PDEC0_AU_DETECT+ui8VesCh);
#endif
	if( bIsIntrEnable == TRUE )
	{
#if defined(USE_MCU_FOR_VDEC_VES)
		TOP_HAL_DisableInterIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_ClearInterIntr(PDEC0_AU_DETECT+ui8VesCh);
#else
		TOP_HAL_DisableExtIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_ClearExtIntr(PDEC0_AU_DETECT+ui8VesCh);
#endif
	}

	TOP_HAL_DisablePdecInput(ui8VesCh);
	PDEC_HAL_Disable(ui8VesCh);

	PDEC_HAL_Reset(ui8VesCh);

	VES_AUIB_Reset(ui8VesCh);
	VES_CPB_Reset(ui8VesCh);

	switch( gsVesDrv[ui8VesCh].State )
	{
	case VES_PLAY :
	case VES_PAUSE :
		if( bIsIntrEnable == FALSE )
		{
			VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Disabled PIC_Detected Interrupt, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
		}
	case VES_READY :
		PDEC_HAL_SetVideoStandard(ui8VesCh, _VES_GetTranslateCodecType(ui8CodecType_Config));

		TOP_HAL_EnablePdecInput(ui8VesCh);
		TOP_HAL_SetPdecInputSelection(ui8VesCh, eVesSrcCh);

#if defined(USE_MCU_FOR_VDEC_VES)
		TOP_HAL_ClearInterIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_EnableInterIntr(PDEC0_AU_DETECT+ui8VesCh);
#else
		TOP_HAL_ClearExtIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_EnableExtIntr(PDEC0_AU_DETECT+ui8VesCh);
#endif
		break;
	case VES_NULL :
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Current State, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
	}

	switch( gsVesDrv[ui8VesCh].State )
	{
	case VES_READY :
		break;
	case VES_PLAY :
		PDEC_HAL_Enable(ui8VesCh);
		break;
	case VES_PAUSE :
		PDEC_HAL_Disable(ui8VesCh);
		break;
	case VES_NULL :
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Current State, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
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
void VES_Init( void (*_fpVES_InputInfo)(S_VES_CALLBACK_BODY_ESINFO_T *pInputInfo) )
{
	UINT32	gsVesPhy, gsVesSize;

	gsVesSize = sizeof(S_VDEC_VES_DB_T) * VES_NUM_OF_CHANNEL;
#if defined(__XTENSA__)
	gsVesPhy = IPC_REG_VES_GetDb();
#else
	gsVesPhy = VDEC_SHM_Malloc(gsVesSize);
	IPC_REG_VES_SetDb(gsVesPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[VES][DBG] gsVesPhy: 0x%X, %s", gsVesPhy, __FUNCTION__ );

	gsVesDrv = (S_VDEC_VES_DB_T *)VDEC_TranselateVirualAddr(gsVesPhy, gsVesSize);

#if !defined(__XTENSA__)
	_VDEC_VES_workqueue = create_workqueue("VDEC_VES");
	spin_lock_init(&stVdecVesSpinlock);

	_gfpVES_InputInfo = _fpVES_InputInfo;
#endif

#if !defined(__XTENSA__)
{
	UINT8	ui8VesCh;

	for( ui8VesCh = 0; ui8VesCh < VES_NUM_OF_CHANNEL; ui8VesCh++ )
	{
		gsVesDrv[ui8VesCh].State = VES_NULL;

		if( ui8VesCh < PDEC_NUM_OF_CHANNEL )
		{
			TOP_HAL_DisablePdecInput(ui8VesCh);
			PDEC_HAL_Disable(ui8VesCh);
		}
	}
}
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
UINT8 VES_Open(UINT8 ui8Vch, E_VES_SRC_T eVesSrcCh, UINT8 ui8CodecType_Config, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bUseGstc, UINT32 ui32DecodeOffset_bytes, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned)
{
	UINT8		ui8VesCh = 0xFF;
	BOOLEAN		bIsHwPath;
	BOOLEAN		bFromTVP = FALSE;

#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( eVesSrcCh == VES_SRC_TVP )
	{
		ui8VesCh = 2;
		bFromTVP = TRUE;

		if( gsVesDrv[ui8VesCh].State != VES_NULL )
		{
			VDEC_KDRV_Message(ERROR, "[VES][Err] Not Enough Channel - VES Src:%d, %s", eVesSrcCh, __FUNCTION__ );
			for( ui8VesCh = 0; ui8VesCh < VES_NUM_OF_CHANNEL; ui8VesCh++ )
				VDEC_KDRV_Message(ERROR, "[VES%d][Err] State: %d", ui8VesCh, gsVesDrv[ui8VesCh].State );

#ifndef __XTENSA__
			spin_unlock(&stVdecVesSpinlock);
#endif
			return 0xFF;
		}

		VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Trust Video Path, %s ", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
	}
	else
	{
		for( ui8VesCh = 0; ui8VesCh < VES_NUM_OF_CHANNEL; ui8VesCh++ )
		{
			if( gsVesDrv[ui8VesCh].State == VES_NULL )
				break;
		}
		if( ui8VesCh == VES_NUM_OF_CHANNEL )
		{
			VDEC_KDRV_Message(ERROR, "[VES][Err] Not Enough Channel - VES Src:%d, %s", eVesSrcCh, __FUNCTION__ );
			for( ui8VesCh = 0; ui8VesCh < VES_NUM_OF_CHANNEL; ui8VesCh++ )
				VDEC_KDRV_Message(ERROR, "[VES%d][Err] State: %d", ui8VesCh, gsVesDrv[ui8VesCh].State );

#ifndef __XTENSA__
			spin_unlock(&stVdecVesSpinlock);
#endif
			return 0xFF;
		}
	}

	bIsHwPath = ( eVesSrcCh <= VES_SRC_TVP ) ? TRUE : FALSE;

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Decoding Offset: 0x%X, UseGstc: %d, %s ", ui8VesCh, gsVesDrv[ui8VesCh].State, ui32DecodeOffset_bytes, bUseGstc, __FUNCTION__ );

	if( VES_CPB_Open( ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize, bRingBufferMode, b512bytesAligned, bIsHwPath, bFromTVP) == FALSE )
	{
#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return 0xFF;
	}
	if( bIsHwPath == TRUE )
	{
		if( VES_AUIB_Open( ui8VesCh, bFromTVP) == FALSE )
		{
			VES_CPB_Close(ui8VesCh);
#ifndef __XTENSA__
			spin_unlock(&stVdecVesSpinlock);
#endif
			return 0xFF;
		}
	}

	gsVesDrv[ui8VesCh].State = VES_READY;
	gsVesDrv[ui8VesCh].Config.eVesSrcCh = eVesSrcCh;
	gsVesDrv[ui8VesCh].Config.ui8CodecType_Config = ui8CodecType_Config;
	gsVesDrv[ui8VesCh].Config.bUseGstc = bUseGstc;
	gsVesDrv[ui8VesCh].Config.ui8Vch = ui8Vch;
	gsVesDrv[ui8VesCh].Config.bFromTVP = bFromTVP;
	gsVesDrv[ui8VesCh].Config.ui32CpbBufAddr = ui32CpbBufAddr;
	gsVesDrv[ui8VesCh].Config.ui32CpbBufSize = ui32CpbBufSize;

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
	{
		if( (bRingBufferMode != TRUE) ||(b512bytesAligned != TRUE) )
			VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Buffer Mode: %d, 512bytes Aligned: %d, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, bRingBufferMode, b512bytesAligned, __FUNCTION__ );
	}

	gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr = 0x0;
	gsVesDrv[ui8VesCh].Status.ui32PushedAui = 0;
	gsVesDrv[ui8VesCh].Status.sPdecAui_Prev.ui32AuStartAddr = 0x0;

	gsVesDrv[ui8VesCh].Status.bFlushing = FALSE;

	if( (bIsHwPath == TRUE) && (bFromTVP == FALSE) )
	{
		_PDEC_HW_Reset(ui8VesCh, ui8CodecType_Config, eVesSrcCh);
	}

#ifdef L9_A0_WORKAROUND
	gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset = 0x0;
	gsVesDrv[ui8VesCh].A0.bEOS_Found = FALSE;
	gsVesDrv[ui8VesCh].A0.ui32AuStartAddr_HW = 0x0;
	gsVesDrv[ui8VesCh].A0.ui32AddrMismatchingCnt = 0;
#endif

	gsVesDrv[ui8VesCh].Debug.ui32ReceivedAuCnt = 0;
	gsVesDrv[ui8VesCh].Debug.ui32LogTick = 0xC0;

	gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev = 0x0;
	gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev = 0x0;

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
#endif
	return ui8VesCh;
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
UINT8 VES_GetVdecCh(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return 0xFF;
	}

	return gsVesDrv[ui8VesCh].Config.ui8Vch;
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
void VES_Close(UINT8 ui8VesCh)
{
#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );

#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
	{
#if defined(USE_MCU_FOR_VDEC_VES)
		TOP_HAL_DisableInterIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_ClearInterIntr(PDEC0_AU_DETECT+ui8VesCh);
#else
		TOP_HAL_DisableExtIntr(PDEC0_AU_DETECT+ui8VesCh);
		TOP_HAL_ClearExtIntr(PDEC0_AU_DETECT+ui8VesCh);
#endif
		TOP_HAL_DisablePdecInput(ui8VesCh);
		PDEC_HAL_Disable(ui8VesCh);
	}

	gsVesDrv[ui8VesCh].State = VES_NULL;

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_TVP )
	{
		VES_AUIB_Close(ui8VesCh);
	}
	VES_CPB_Close(ui8VesCh);

	gsVesDrv[ui8VesCh].Config.eVesSrcCh = 0xFF;
	gsVesDrv[ui8VesCh].Config.ui8Vch = 0xFF;

//	if( gsVesDrv[ui8VesCh].Config.ui8TeCh <= VDEC_SRC_SDEC2 )
//	{
//		_PDEC_HW_Reset(ui8VesCh, gsVesDrv[ui8VesCh].Config.ui8CodecType_Config, gsVesDrv[ui8VesCh].Config.ui8TeCh);
//	}

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
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
void VES_Reset(UINT8 ui8VesCh)
{
#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );

#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr = 0x0;
	gsVesDrv[ui8VesCh].Status.ui32PushedAui = 0;
	gsVesDrv[ui8VesCh].Status.sPdecAui_Prev.ui32AuStartAddr = 0x0;
	gsVesDrv[ui8VesCh].Status.bFlushing = FALSE;

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
	{
		_PDEC_HW_Reset(ui8VesCh, gsVesDrv[ui8VesCh].Config.ui8CodecType_Config, gsVesDrv[ui8VesCh].Config.eVesSrcCh);
	}

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_TVP )
	{
		VES_AUIB_Reset(ui8VesCh);
	}
	VES_CPB_Reset(ui8VesCh);

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
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
void VES_Start(UINT8 ui8VesCh)
{
#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );

#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	switch( gsVesDrv[ui8VesCh].State )
	{
	case VES_READY :
	case VES_PAUSE :
		gsVesDrv[ui8VesCh].State = VES_PLAY;

		if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
			PDEC_HAL_Enable(ui8VesCh);
		break;
	case VES_PLAY :
		VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Already Played, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
		break;
	case VES_NULL :
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Current State, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
	}

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
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
void VES_Stop(UINT8 ui8VesCh)
{
#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );

#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	switch( gsVesDrv[ui8VesCh].State )
	{
	case VES_PLAY :
		if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
			PDEC_HAL_Disable(ui8VesCh);

		gsVesDrv[ui8VesCh].State = VES_PAUSE;
		break;
	case VES_NULL :
	case VES_READY :
	case VES_PAUSE :
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] Current State, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
	}

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
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
static void _VES_Flush(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if(gsVesDrv[ui8VesCh].State == VES_NULL)
	{
		return;
	}

	VES_CPB_Flush(ui8VesCh);

	gsVesDrv[ui8VesCh].Status.sPdecAui_Prev.ui32AuStartAddr = 0x0;
	gsVesDrv[ui8VesCh].Status.ui32PushedAui = 0;

	gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev = 0x0;
	gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev = 0x0;
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
void VES_Flush(UINT8 ui8VesCh)
{
#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if(gsVesDrv[ui8VesCh].State == VES_NULL)
	{
#ifndef __XTENSA__
		spin_unlock(&stVdecVesSpinlock);
#endif
		return;
	}

	if( gsVesDrv[ui8VesCh].Config.eVesSrcCh <= VES_SRC_SDEC2 )
	{
		gsVesDrv[ui8VesCh].Status.bFlushing = TRUE;
	}
	else
	{
		_VES_Flush(ui8VesCh);
	}

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
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
BOOLEAN VES_UpdateRdPtr(UINT8 ui8VesCh, UINT32 ui32RdPhyAddr)
{
	BOOLEAN		bRet;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if(gsVesDrv[ui8VesCh].State == VES_NULL)
	{
		return FALSE;
	}

	bRet = VES_CPB_UpdateRdPtr(ui8VesCh, ui32RdPhyAddr);

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
UINT32 VES_GetUsedBuffer(UINT8 ui8VesCh)
{
	UINT32		ui32UseBytes;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if(gsVesDrv[ui8VesCh].State == VES_NULL)
	{
		return 0;
	}

	ui32UseBytes = VES_CPB_GetUsedBuffer(ui8VesCh);

	return ui32UseBytes;
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
BOOLEAN VES_GetBufferSize(UINT8 ui8VesCh)
{
	UINT32 		ui32BufSize;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );

	if(gsVesDrv[ui8VesCh].State == VES_NULL)
	{
		return 0;
	}

	ui32BufSize = VES_CPB_GetBufferSize(ui8VesCh);

	return ui32BufSize;
}

#ifndef __XTENSA__
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
BOOLEAN VES_UpdateBuffer(UINT8 ui8VesCh,
									E_VES_AU_T eAuType,
									UINT32 ui32UserBuf,
									UINT32 ui32UserBufSize,
									fpCpbCopyfunc fpCopyfunc,
									UINT32 ui32uID,
									UINT64 ui64TimeStamp,	// ns
									UINT32 ui32TimeStamp_90kHzTick,
									UINT32 ui32FrameRateRes,
									UINT32 ui32FrameRateDiv,
									UINT32 ui32Width,
									UINT32 ui32Height,
									BOOLEAN bSeqInit)
{
	UINT32		ui32CpbWrPhyAddr = 0x0;
	UINT32		ui32CpbWrPhyAddr_End = 0x0;
	UINT32		ui32GSTC;
	S_VES_CALLBACK_BODY_ESINFO_T		sVesAu;

#ifndef __XTENSA__
	spin_lock(&stVdecVesSpinlock);
#endif

	gsVesDrv[ui8VesCh].Debug.ui32ReceivedAuCnt++;

	ui32GSTC = TOP_HAL_GetGSTCC();

	switch( gsVesDrv[ui8VesCh].State  )
	{
	case VES_READY :
	case VES_PAUSE :
		VDEC_KDRV_Message(WARNING, "[VES%d-%d][Wrn] Current State, %s",
									ui8VesCh, gsVesDrv[ui8VesCh].State,
									__FUNCTION__ );
	case VES_PLAY :
		if( gsVesDrv[ui8VesCh].Status.ui32PushedAui == 0 )
		{
			VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Start UpdateBuffer - Time Stamp:%llu, DTS:0x%08X, Frame Rate:%d/%d, Width*Height:%d*%d, GSTC:0x%08X",
										ui8VesCh, gsVesDrv[ui8VesCh].State, ui64TimeStamp, ui32TimeStamp_90kHzTick,
										ui32FrameRateRes, ui32FrameRateDiv, ui32Width, ui32Height, ui32GSTC );
		}

		if( eAuType == VES_AU_SEQUENCE_HEADER )
		{
#if 0
			UINT32		ui32CpbDumpBuf[16];

			if( copy_from_user(ui32CpbDumpBuf, (char *)(ui32UserBuf), 4*16) )
				VDEC_KDRV_Message(ERROR, "[VES][Err][CPB%d] copy_from_user(0x%X) - %s \n", ui8VesCh, (UINT32)ui32UserBuf, __FUNCTION__ );

			VDEC_KDRV_Message(DBG_SYS, "[VES%d-%d][DBG] 0x%X 0x%X 0x%X 0x%X", ui8VesCh, gsVesDrv[ui8VesCh].State,
									ui32CpbDumpBuf[0], ui32CpbDumpBuf[1], ui32CpbDumpBuf[2], ui32CpbDumpBuf[3] );
			VDEC_KDRV_Message(DBG_SYS, "[VES%d-%d][DBG] 0x%X 0x%X 0x%X 0x%X", ui8VesCh, gsVesDrv[ui8VesCh].State,
									ui32CpbDumpBuf[4], ui32CpbDumpBuf[5], ui32CpbDumpBuf[6], ui32CpbDumpBuf[7] );
			VDEC_KDRV_Message(DBG_SYS, "[VES%d-%d][DBG] 0x%X 0x%X 0x%X 0x%X", ui8VesCh, gsVesDrv[ui8VesCh].State,
									ui32CpbDumpBuf[8], ui32CpbDumpBuf[9], ui32CpbDumpBuf[10], ui32CpbDumpBuf[11] );
			VDEC_KDRV_Message(DBG_SYS, "[VES%d-%d][DBG] 0x%X 0x%X 0x%X 0x%X", ui8VesCh, gsVesDrv[ui8VesCh].State,
									ui32CpbDumpBuf[12], ui32CpbDumpBuf[13], ui32CpbDumpBuf[14], ui32CpbDumpBuf[15] );
#endif
			VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Receive Sequence Header(Size:%d)", ui8VesCh, gsVesDrv[ui8VesCh].State, ui32UserBufSize );
		}

		if( eAuType == VES_AU_EOS )
		{
			UINT32		ui32StartCode = 0x01000000;

			VDEC_KDRV_Message(DBG_SYS, "[VES%d][DBG] End Of Stream, UID:%d, %s", ui8VesCh, ui32uID, __FUNCTION__ );

			ui32UserBufSize = 4;
			ui32CpbWrPhyAddr = VES_CPB_Write(ui8VesCh, (UINT32)&ui32StartCode, ui32UserBufSize, (fpCpbCopyfunc)memcpy, &ui32CpbWrPhyAddr_End);
		}
		else
		{
			if( gsVesDrv[ui8VesCh].Config.bFromTVP == TRUE )
			{
				ui32CpbWrPhyAddr = ui32UserBuf;
				ui32CpbWrPhyAddr_End = ui32UserBuf + ui32UserBufSize;
				if( ui32CpbWrPhyAddr_End >= (VES_CPB_GetBufferBaseAddr(ui8VesCh) + VES_CPB_GetBufferSize(ui8VesCh)) )
					ui32CpbWrPhyAddr_End -= VES_CPB_GetBufferSize(ui8VesCh);
			}
			else
			{
				if( ui32UserBufSize == 0 )
				{
					VDEC_KDRV_Message(ERROR, "[VES%d][Err] Au Size is Zero, %s", ui8VesCh, __FUNCTION__ );
#ifndef __XTENSA__
					spin_unlock(&stVdecVesSpinlock);
#endif
					return FALSE;
				}

				ui32CpbWrPhyAddr = VES_CPB_Write(ui8VesCh, ui32UserBuf, ui32UserBufSize, fpCopyfunc, &ui32CpbWrPhyAddr_End);
				if( ui32CpbWrPhyAddr == 0x0 )
				{
					VDEC_KDRV_Message(ERROR, "[VES%d-%d][Err] VES_CPB_Write Failed - CPB:0x%X, %s", ui8VesCh, gsVesDrv[ui8VesCh].State,
												VES_CPB_GetUsedBuffer(ui8VesCh), __FUNCTION__ );
#ifndef __XTENSA__
					spin_unlock(&stVdecVesSpinlock);
#endif
					return FALSE;
				}
			}
		}
		VES_CPB_UpdateWrPtr(ui8VesCh, ui32CpbWrPhyAddr_End);

		if( gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev )
		{
			if( gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev > ui32CpbWrPhyAddr )
				if( (gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev - ui32CpbWrPhyAddr) < (gsVesDrv[ui8VesCh].Config.ui32CpbBufSize * 8 / 10) )
					VDEC_KDRV_Message(ERROR, "[VES%d][Err] Start - Prev:0x%X, Curr:0x%X ~ 0x%X, %s(%d)", ui8VesCh, gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev, ui32CpbWrPhyAddr, ui32CpbWrPhyAddr_End, __FUNCTION__, __LINE__ );
		}
		if( gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev )
		{
			if( gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev > ui32CpbWrPhyAddr_End )
				if( (gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev - ui32CpbWrPhyAddr_End) < (gsVesDrv[ui8VesCh].Config.ui32CpbBufSize * 8 / 10) )
					VDEC_KDRV_Message(ERROR, "[VES%d][Err] End - Prev:0x%X, Curr:0x%X ~ 0x%X, %s(%d)", ui8VesCh, gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev, ui32CpbWrPhyAddr, ui32CpbWrPhyAddr_End, __FUNCTION__, __LINE__ );
		}
		gsVesDrv[ui8VesCh].Debug.ui32AuStartAddr_Prev = ui32CpbWrPhyAddr;
		gsVesDrv[ui8VesCh].Debug.ui32AuEndAddr_Prev = ui32CpbWrPhyAddr_End;

//		VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Time Stamp:%llu, DTS: 0x%X(Rate:%u), GSTC: 0x%X", ui8VesCh, ui64TimeStamp, (UINT32)ui64TimeStamp_90kHzTick, ui32FrameRate_byDTS, ui32GSTC );

//		VDEC_KDRV_Message(ERROR, "[VES%d] ui32TimeStamp_90kHzTick: 0x%X", ui8VesCh, ui32TimeStamp_90kHzTick );

		sVesAu.ui32Ch = gsVesDrv[ui8VesCh].Config.ui8Vch;
		sVesAu.ui32AuType = eAuType;
		sVesAu.ui32AuStartAddr = ui32CpbWrPhyAddr;
		sVesAu.pui8VirStartPtr = ( gsVesDrv[ui8VesCh].Config.bFromTVP == TRUE ) ? 0x0 : VES_CPB_TranslatePhyWrPtr(ui8VesCh, ui32CpbWrPhyAddr);
		sVesAu.ui32AuEndAddr = ui32CpbWrPhyAddr_End;
		sVesAu.ui32AuSize = ui32UserBufSize;
		sVesAu.ui32DTS = ui32TimeStamp_90kHzTick;
		sVesAu.ui32PTS = ui32TimeStamp_90kHzTick;
		sVesAu.ui64TimeStamp = ui64TimeStamp;
		sVesAu.ui32uID = ui32uID;
		sVesAu.ui32InputGSTC = ui32GSTC & 0x7FFFFFFF;
		sVesAu.ui32FrameRateRes_Parser = ui32FrameRateRes;
		sVesAu.ui32FrameRateDiv_Parser = ui32FrameRateDiv;
		sVesAu.ui32Width = ui32Width;
		sVesAu.ui32Height = ui32Height;

		sVesAu.ui32CpbOccupancy = VES_CPB_GetUsedBuffer(ui8VesCh);
		sVesAu.ui32CpbSize = VES_CPB_GetBufferSize(ui8VesCh);

		VDEC_KDRV_Message(DBG_VES, "[VES%d] UV:0x%08X, KP:0x%08X, Size:0x%X, Data:0x%08X, Time Stamp:%llu - %s(%d)",
                ui8VesCh, (UINT32)ui32UserBuf, ui32CpbWrPhyAddr, ui32UserBufSize,
                (gsVesDrv[ui8VesCh].Config.bFromTVP == TRUE) ? 0x0 : *(UINT32 *)sVesAu.pui8VirStartPtr,
                ui64TimeStamp, __FUNCTION__, __LINE__ );

#if 0
		if( (eAuType ==  AU_SEQUENCE_HEADER) && (bSeqInit == FALSE) )
		{
			gsVesDrv[ui8VesCh].Status.sVesSeqAu = sVesAu;
		}
		else
#endif
		{
			if( gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr )
			{
				gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuEndAddr = ui32CpbWrPhyAddr_End;
				gsVesDrv[ui8VesCh].Status.ui32PushedAui++;
				_gfpVES_InputInfo(&gsVesDrv[ui8VesCh].Status.sVesSeqAu);

				gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr = 0x0;
			}

			gsVesDrv[ui8VesCh].Status.ui32PushedAui++;
			_gfpVES_InputInfo(&sVesAu);
		}

		break;
	case VES_NULL :
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Current State: %d, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
		break;
	}

	// for debug log
	gsVesDrv[ui8VesCh].Debug.ui32LogTick++;
	if( gsVesDrv[ui8VesCh].Debug.ui32LogTick == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[VES%d-%d][DBG] BufSize:%d, Time Stamp:%llu, DTS:0x%08X, GSTC:0x%08X, Frame Rate:%d/%d, CPB:0x%X, ReceivedAuCnt:%d",
									ui8VesCh, gsVesDrv[ui8VesCh].State,
									ui32UserBufSize,
									ui64TimeStamp, ui32TimeStamp_90kHzTick, ui32GSTC,
									ui32FrameRateRes, ui32FrameRateDiv,
									VES_CPB_GetUsedBuffer(ui8VesCh), gsVesDrv[ui8VesCh].Debug.ui32ReceivedAuCnt);

		gsVesDrv[ui8VesCh].Debug.ui32LogTick = 0;
	}

	gsVesDrv[ui8VesCh].Debug.ui32GSTC_Prev = ui32GSTC;

	_VES_RamLog(ui64TimeStamp, ui32GSTC, ui32UserBufSize, ui32TimeStamp_90kHzTick, ui32TimeStamp_90kHzTick, ui32CpbWrPhyAddr, 0);

#ifndef __XTENSA__
	spin_unlock(&stVdecVesSpinlock);
#endif

	return TRUE;
}
#endif

#if (defined(USE_MCU_FOR_VDEC_VES) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
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
static UINT32 _VES_ParseAuib2AuQ(UINT8 ui8VesCh)
{
	S_PDEC_AUI_T	sPdecAui, *pPdecAui_Prev;
	UINT32			ui32AuStartAddr = 0x0;
	UINT32			ui32DecodedAuiCount = 0;
	UINT32			ui32GSTC;

	ui32GSTC = TOP_HAL_GetGSTCC();

	switch( gsVesDrv[ui8VesCh].State )
	{
	case VES_READY :
	case VES_PAUSE :
		VDEC_KDRV_Message(ERROR, "[VES%d-%d][Wrn] Current State, %s",
									ui8VesCh, gsVesDrv[ui8VesCh].State,
									__FUNCTION__ );
		break;
	case VES_PLAY :
		if( gsVesDrv[ui8VesCh].Status.ui32PushedAui == 0 )
		{
			VDEC_KDRV_Message(_TRACE, "[VES%d-%d][DBG] Start Decoding, GSTC:0x%X",
										ui8VesCh, gsVesDrv[ui8VesCh].State,
										ui32GSTC );
		}

		while( PDEC_HAL_AUIB_Pop(ui8VesCh, &sPdecAui) == TRUE )
		{
			gsVesDrv[ui8VesCh].Debug.ui32ReceivedAuCnt++;
			ui32DecodedAuiCount++;

			pPdecAui_Prev = (S_PDEC_AUI_T *)&gsVesDrv[ui8VesCh].Status.sPdecAui_Prev;
			if( pPdecAui_Prev->ui32AuStartAddr )
			{
				S_VES_CALLBACK_BODY_ESINFO_T		sVesAu;

#ifdef L9_A0_WORKAROUND
{
				UINT32	ui32AuStartAddr_HW;
				UINT32	ui32CpbSize = gsVesDrv[ui8VesCh].Config.ui32CpbBufSize;
				UINT32	ui32AuEndAddr = gsVesDrv[ui8VesCh].Config.ui32CpbBufAddr + ui32CpbSize;
				UINT32	*pui32WrVirAddr;
				UINT32	ui32CpbStartData;

				ui32AuStartAddr_HW = pPdecAui_Prev->ui32AuStartAddr;
//				VDEC_KDRV_Message(DBG_PDEC, "[PDEC%d][Wrn] ui32AuStartAddr_HW: 0x%08X", ui8PdecCh, ui32AuStartAddr_HW);

				if( gsVesDrv[ui8VesCh].A0.bEOS_Found == TRUE )
				{
					if( gsVesDrv[ui8VesCh].A0.ui32AuStartAddr_HW == ui32AuStartAddr_HW )
					{
						gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset += 0x200;
						while( gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset >= ui32CpbSize )
							gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset -= ui32CpbSize;
					}
					VDEC_KDRV_Message(DBG_VES, "[VES%d][Wrn] L9-A0 SEQUENCE_END, SeqEndCpbOffset: 0x%08X", ui8VesCh, gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset);

					gsVesDrv[ui8VesCh].A0.bEOS_Found = FALSE;
				}

				pPdecAui_Prev->ui32AuStartAddr += gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset;
				while( pPdecAui_Prev->ui32AuStartAddr >= ui32AuEndAddr )
					pPdecAui_Prev->ui32AuStartAddr -= ui32CpbSize;
				ui32AuStartAddr = sPdecAui.ui32AuStartAddr;
				ui32AuStartAddr += gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset;
				while( ui32AuStartAddr >= ui32AuEndAddr )
					ui32AuStartAddr -= ui32CpbSize;

				pui32WrVirAddr = (UINT32 *)VES_CPB_TranslatePhyWrPtr(ui8VesCh, pPdecAui_Prev->ui32AuStartAddr);
				if( pui32WrVirAddr )
				{
					ui32CpbStartData = *pui32WrVirAddr;
					if( (ui32CpbStartData & 0x0000FFFF) == 0x00000100 )
					{
						gsVesDrv[ui8VesCh].A0.bEOS_Found = TRUE;
						gsVesDrv[ui8VesCh].A0.ui32AuStartAddr_HW = ui32AuStartAddr_HW;
					}

					if( (ui32CpbStartData & 0x00FFFFFF) != 0x00010000 )
					{
						VDEC_KDRV_Message(DBG_VES, "[VES%d][Wrn] ui32CpbStartData: 0x%08X, SeqEndCpbOffset: 0x%08X", ui8VesCh, ui32CpbStartData, gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset);
						if( (ui32CpbStartData & 0x0000FFFF) != 0x00000100 )
						{
							if( (gsVesDrv[ui8VesCh].A0.ui32AddrMismatchingCnt % 0x40) == 0 )
								VDEC_KDRV_Message(ERROR, "[VES%d][Err] AU Start Address Mismatching, CPB Start Addr:0x%08X Data:0x%08X, SeqEndCpbOffset: %X", ui8VesCh, pPdecAui_Prev->ui32AuStartAddr, ui32CpbStartData, gsVesDrv[ui8VesCh].A0.ui32SeqEndCpbOffset);

							gsVesDrv[ui8VesCh].A0.ui32AddrMismatchingCnt++;
//							PDEC_HAL_Disable(ui8VesCh);
						}
					}
				}
}
#endif // #ifdef L9_A0_WORKAROUND
				VES_CPB_UpdateWrPtr(ui8VesCh, ui32AuStartAddr);

				switch( pPdecAui_Prev->eAuType )
				{
				case PDEC_AU_SEQUENCE_HEADER :
					sVesAu.ui32AuType = VES_AU_SEQUENCE_HEADER;
					break;
				case PDEC_AU_SEQUENCE_END :
					sVesAu.ui32AuType = VES_AU_SEQUENCE_END;
					break;
				case PDEC_AU_PICTURE_I :
					sVesAu.ui32AuType = VES_AU_PICTURE_I;
					break;
				case PDEC_AU_PICTURE_P :
					sVesAu.ui32AuType = VES_AU_PICTURE_P;
					break;
				case PDEC_AU_PICTURE_B_NOSKIP :
					sVesAu.ui32AuType = VES_AU_PICTURE_B_NOSKIP;
					break;
				case PDEC_AU_PICTURE_B_SKIP :
					sVesAu.ui32AuType = VES_AU_PICTURE_B_SKIP;
					break;
				default :
					sVesAu.ui32AuType = VES_AU_UNKNOWN;
					VDEC_KDRV_Message(ERROR, "[VES%d][Err] AU Type: %d, %s", ui8VesCh, pPdecAui_Prev->eAuType, __FUNCTION__);
					break;
				}

				sVesAu.ui32Ch = gsVesDrv[ui8VesCh].Config.ui8Vch;
				sVesAu.ui32AuStartAddr = pPdecAui_Prev->ui32AuStartAddr;
				sVesAu.pui8VirStartPtr = VES_CPB_TranslatePhyWrPtr(ui8VesCh, pPdecAui_Prev->ui32AuStartAddr);
				sVesAu.ui32AuEndAddr = ui32AuStartAddr;
				sVesAu.ui32AuSize = (ui32AuStartAddr >= pPdecAui_Prev->ui32AuStartAddr) ? (ui32AuStartAddr - pPdecAui_Prev->ui32AuStartAddr) : (ui32AuStartAddr + gsVesDrv[ui8VesCh].Config.ui32CpbBufSize - pPdecAui_Prev->ui32AuStartAddr);
				sVesAu.ui32DTS = pPdecAui_Prev->ui32DTS;
				sVesAu.ui32PTS = pPdecAui_Prev->ui32PTS;
				sVesAu.ui64TimeStamp = 0;
				sVesAu.ui32uID = ui32GSTC;
				sVesAu.ui32InputGSTC = ui32GSTC & 0x7FFFFFFF;
				sVesAu.ui32FrameRateRes_Parser = 0x0;
				sVesAu.ui32FrameRateDiv_Parser = 0xFFFFFFFF;
				sVesAu.ui32Width = VES_WIDTH_INVALID;
				sVesAu.ui32Height = VES_HEIGHT_INVALID;

#if 1
				if( pPdecAui_Prev->eAuType ==  PDEC_AU_SEQUENCE_HEADER )
				{
					gsVesDrv[ui8VesCh].Status.sVesSeqAu = sVesAu;
				}
				else
#endif
				{
					if( gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr )
					{
						gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuEndAddr = ui32AuStartAddr;
						gsVesDrv[ui8VesCh].Status.ui32PushedAui++;
						_gfpVES_InputInfo(&gsVesDrv[ui8VesCh].Status.sVesSeqAu);

						gsVesDrv[ui8VesCh].Status.sVesSeqAu.ui32AuStartAddr = 0x0;
					}

					gsVesDrv[ui8VesCh].Status.ui32PushedAui++;
					_gfpVES_InputInfo(&sVesAu);
				}

				if( pPdecAui_Prev->ui32DTS > sPdecAui.ui32DTS )
				{
					VDEC_KDRV_Message(ERROR, "[VES%d-%d][Wrn] DTS Continuity - Jitter (Prev:0x%X, Curr:0x%X)", ui8VesCh, gsVesDrv[ui8VesCh].State, pPdecAui_Prev->ui32DTS, sPdecAui.ui32DTS );
				}

				gsVesDrv[ui8VesCh].Debug.ui32LogTick++;
				if( gsVesDrv[ui8VesCh].Debug.ui32LogTick == 0x100 )
				{
					VDEC_KDRV_Message(MONITOR, "[VES%d-%d][DBG] DTS:0x%08X, PTS:0x%08X, CPB:0x%X, ReceivedAuCnt:%d",
												ui8VesCh, gsVesDrv[ui8VesCh].State,
												pPdecAui_Prev->ui32DTS, pPdecAui_Prev->ui32PTS,
												VES_CPB_GetUsedBuffer(ui8VesCh), gsVesDrv[ui8VesCh].Debug.ui32ReceivedAuCnt);

					gsVesDrv[ui8VesCh].Debug.ui32LogTick = 0;
				}

				_VES_RamLog(0x0, ui32GSTC, 0x0, pPdecAui_Prev->ui32PTS, pPdecAui_Prev->ui32DTS, pPdecAui_Prev->ui32AuStartAddr, 0);
			}

			memcpy( pPdecAui_Prev, &sPdecAui, sizeof(S_PDEC_AUI_T) );

			gsVesDrv[ui8VesCh].Debug.ui32GSTC_Prev = ui32GSTC;

			ui32GSTC = TOP_HAL_GetGSTCC();
		}

		break;
	case VES_NULL :
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Wrn] Current State: %d, %s", ui8VesCh, gsVesDrv[ui8VesCh].State, __FUNCTION__ );
		break;
	}

	gsVesDrv[ui8VesCh].Debug.ui32GSTC_Prev = ui32GSTC;

	return ui32DecodedAuiCount;
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
static void _VDEC_ISR_PIC_Detected(UINT8 ui8VesCh)
{
	switch( ui8VesCh )
	{
	case 0 :
	case 1 :
	case 2 :
		_VES_ParseAuib2AuQ(ui8VesCh);

		if( gsVesDrv[ui8VesCh].Status.bFlushing == TRUE )
		{
			_VES_Flush(ui8VesCh);

			gsVesDrv[ui8VesCh].Status.bFlushing = FALSE;
		}
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		break;
	}
}

#ifndef __XTENSA__
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
static void _VDEC_ISR_PIC_Detected_workfunc(struct work_struct *data)
{
	UINT8	ui8VesCh = 0;
	UINT32	ui32VesChBitMask = ui32WorkfuncParam;
	ui32WorkfuncParam = 0x0;

	spin_lock(&stVdecVesSpinlock);

	while( ui32VesChBitMask )
	{
		if( ui32VesChBitMask & 0x1 )
			_VDEC_ISR_PIC_Detected(ui8VesCh);

		ui32VesChBitMask >>= 1;
		ui8VesCh++;
	}

	spin_unlock(&stVdecVesSpinlock);
}
#endif

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
void VDEC_ISR_PIC_Detected(UINT8 ui8VesCh)
{
#ifdef __XTENSA__
	_VDEC_ISR_PIC_Detected(ui8VesCh);
#else
	ui32WorkfuncParam |= (1 << ui8VesCh);
	queue_work(_VDEC_VES_workqueue,  &_VDEC_VES_work);
#endif
}

#endif

