/*
	SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
	Copyright(c) 2010 by LG Electronics Inc.

	This program is free software; you can redistribute it and/or 
	modify it under the terms of the GNU General Public License
	version 2 as published by the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
	GNU General Public License for more details.
*/ 


/** @file
 *
 * IO Control : Main Entry.
 *
 *
 * author     sooya.joo@lge.com
 * version    0.1
 * date       2010.03.11
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <linux/mman.h>
#include <linux/seq_file.h>		// for seq_file

#include <mach/platform.h>

#include <linux/delay.h>		// for seq_file

#include "base_types.h"
#include "os_util.h"

#include "vdec_kapi.h"
#include "vdec_io.h"
#include "vdec_noti.h"

#include "vdec_ktop.h"
#include "hal/lg1152/lg1152_vdec_base.h"
#include "hal/lg1152/pdec_hal_api.h"
#include "hal/lg1152/mcu_hal_api.h"
#include "hal/lg1152/top_hal_api.h"
#include "hal/lg1152/ipc_reg_api.h"
#include "vdc/vdc_drv.h"
#include "ves/ves_drv.h"
#include "vds/pts_drv.h"

#include "mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   External Function Prototype Declarations
 *---------------------------------------------------------------------------------------*/
extern VDEC_KDRV_OUTPUT_T *VDEC_NOTI_GetOutput( UINT32 ch );

#ifdef USE_MCU
extern void VDEC_MCU_Reset(void);
#endif

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


/**
********************************************************************************
* @brief
*   vdec IO init.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
* @param
* @return
********************************************************************************
*/
void VDEC_IO_Init(void)
{
	VDEC_KTOP_Init();
	VDEC_NOTI_Init();
}

UINT8 VDEC_GetVdecStdType(UINT8 ui8CodecType)
{
	static UINT8 ui8aVidStdSetInfo[15][5]= {//[0]video standard [1]asp class [2]aux std [3]seq option broad cast [4]seq option file play
#if 1
	{0x0,0x0,0x0,0x2,0xE}, {0x0,0x0,0x0,0x2,0xE}, {0x1,0x0,0x0,0x0,0xE}, {0x1,0x1,0x0,0x0,0xE},
	{0x1,0x2,0x0,0x0,0xE}, {0x2,0x0,0x0,0x0,0xC}, {0x3,0x0,0x0,0x0,0xC},
	{0x3,0x2,0x0,0x0,0xC}, {0x3,0x0,0x1,0x0,0xC}, {0x3,0x5,0x0,0x0,0xC},
	{0x3,0x1,0x0,0x0,0xC}, {0x4,0x0,0x0,0x0,0xC}, {0x5,0x0,0x0,0x2,0xE},
	{0xFF,0xFF,0xFF,0xFF,0xFF}/*VP8*/, {0xFF,0xFF,0xFF,0xFF,0xFF}/*SW*/,
	/*{0x8,0x0,0x0,0x0,0xC} for MJPEG*/};
#else
	{0x0,0x0,0x0,0x42,0xE}, {0x1,0x0,0x0,0x40,0xE}, {0x1,0x1,0x0,0x40,0xE},
	{0x1,0x2,0x0,0x40,0xE}, {0x2,0x0,0x0,0x40,0xC}, {0x3,0x0,0x0,0x40,0xC},
	{0x3,0x2,0x0,0x40,0xC}, {0x3,0x0,0x1,0x40,0xC}, {0x3,0x5,0x0,0x40,0xC},
	{0x3,0x1,0x0,0x40,0xC}, {0x4,0x0,0x0,0x40,0xC}, {0x5,0x0,0x0,0x42,0xE},
	{0x8,0x0,0x0,0x40,0xC}};
#endif
	return ui8aVidStdSetInfo[ui8CodecType][0];
}

/**
********************************************************************************
* @brief
*   initialize vdec.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_OpenChannel(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = 0;
	UINT8 retOpen = 3;
	VDEC_KDRV_IO_OPEN_PARAM_T initCh;
	UINT8 vcodec = 0x0;
	VDEC_SRC_T eInSrc;
	VDEC_DST_T eOutDst;

	if (copy_from_user(&initCh, (void*)ulArg, sizeof(VDEC_KDRV_IO_OPEN_PARAM_T))) return -EFAULT;

	ui8Ch = initCh.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	vcodec = initCh.vcodec;
	switch(initCh.src)
	{
	case VDEC_KDRV_SRC_SDEC0:
		eInSrc = VDEC_SRC_SDEC0;
		break;
	case VDEC_KDRV_SRC_SDEC1:
		eInSrc = VDEC_SRC_SDEC1;
		break;
	case VDEC_KDRV_SRC_SDEC2:
		eInSrc = VDEC_SRC_SDEC2;
		break;
	case VDEC_KDRV_SRC_TVP:
		eInSrc = VDEC_SRC_TVP;
		break;
	case VDEC_KDRV_SRC_BUFF0:
		eInSrc = VDEC_SRC_BUFF0;
		break;
	case VDEC_KDRV_SRC_BUFF1:
		eInSrc = VDEC_SRC_BUFF1;
		break;
	case VDEC_KDRV_SRC_GRP_BUFF0:
		eInSrc = VDEC_SRC_GRP_BUFF0;
		break;
	case VDEC_KDRV_SRC_GRP_BUFF1:
		eInSrc = VDEC_SRC_GRP_BUFF1;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] VDEC Input Source Param %d", ui8Ch, initCh.src);
		eRet = -1;
		goto error;
	}

	switch(initCh.dst)
	{
	case VDEC_KDRV_DST_DE0:
		eOutDst = VDEC_DST_DE0;
		break;
	case VDEC_KDRV_DST_DE1:
		eOutDst = VDEC_DST_DE1;
		break;
	case VDEC_KDRV_DST_BUFF:
		eOutDst = VDEC_DST_BUFF;
		break;
	case VDEC_KDRV_DST_GRP_BUFF0:
		eOutDst = VDEC_DST_GRP_BUFF0;
		break;
	case VDEC_KDRV_DST_GRP_BUFF1:
		eOutDst = VDEC_DST_GRP_BUFF1;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] VDEC Output Destination Param %d", ui8Ch, initCh.dst);
		eRet = -1;
		goto error;
	}

	retOpen = VDEC_KTOP_Open(ui8Ch, vcodec, eInSrc, eOutDst,
							initCh.bNoAdpStr, initCh.bForThumbnail,
							initCh.ui32DecodeOffset_bytes, initCh.bAlignedBuf,
							initCh.ui32ErrorMBFilter, initCh.ui32Width, initCh.ui32Height,
							initCh.bDualDecoding, initCh.bLowLatency,
							initCh.bUseGstc, initCh.ui32DisplayOffset_ms, initCh.ui8ClkID,
							&initCh.ui8VesCh, &initCh.ui8VdcCh, &initCh.ui8VdsCh);

	if(retOpen)
	{
		eRet = -1;
		goto error;
	}
	else
	{
		eRet = ui8Ch;
	}

error:

	if (copy_to_user((void*)ulArg, &initCh, sizeof(VDEC_KDRV_IO_OPEN_PARAM_T))) return -EFAULT;

	return (eRet);
}

/**
********************************************************************************
* @brief
*   ch close.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_CloseChannel(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = 0;
	VDEC_KDRV_IO_NULL_PARAM_T ioClose;

	if (copy_from_user(&ioClose, (void*)ulArg, sizeof(VDEC_KDRV_IO_NULL_PARAM_T))) return -EFAULT;

	ui8Ch = ioClose.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_NOTI_EnableCallback(ui8Ch, FALSE, FALSE, FALSE, FALSE);
	VDEC_KTOP_Close(ui8Ch);
	eRet = ui8Ch;

	return (eRet);
}

/**
********************************************************************************
* @brief
*   standard set.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_PlayChannel(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = 0;
	VDEC_KDRV_IO_PLAY_PARAM_T ioPlay;

	if ( copy_from_user(&ioPlay, (void*)ulArg, sizeof(VDEC_KDRV_IO_PLAY_PARAM_T)) )	{ return -EFAULT; }

	ui8Ch = ioPlay.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if(ioPlay.picScanMode != VDEC_KDRV_PIC_SCAN_INVALID)
	{
		eRet = VDEC_KTOP_SetPicScanMode(ui8Ch, (UINT8)ioPlay.picScanMode);
	}
	if(ioPlay.srcScanType != VDEC_KDRV_PIC_SCAN_INVALID)
	{
		eRet = VDEC_KTOP_SetSrcScanType(ui8Ch, (UINT8)ioPlay.srcScanType);
	}

	if(ioPlay.syncOn != VDEC_KDRV_SYNC_INVALID)
	{
		eRet = VDEC_KTOP_SetLipSync(ui8Ch, ioPlay.syncOn);
	}

	switch( ioPlay.cmd )
	{
	case VDEC_KDRV_CMD_PLAY :
		eRet = VDEC_KTOP_Play(ui8Ch, ioPlay.speed);
		break;
	case VDEC_KDRV_CMD_PAUSE :
		eRet = VDEC_KTOP_Pause(ui8Ch);
		break;
	case VDEC_KDRV_CMD_STEP :
		eRet = VDEC_KTOP_Step(ui8Ch);
		break;
	case VDEC_KDRV_CMD_FREEZE :
		eRet = VDEC_KTOP_Freeze(ui8Ch);
		break;
	default :
		break;
	}

	return (eRet);
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
int VDEC_IO_FlushChannel(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_NULL_PARAM_T ioFlush;

	if ( copy_from_user(&ioFlush, (void*)ulArg, sizeof(VDEC_KDRV_IO_NULL_PARAM_T)) )	{ return -EFAULT; }

	ui8Ch = ioFlush.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_KTOP_Flush(ui8Ch);

	return (eRet);
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
int VDEC_IO_EnableCallback(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_ENABLE_CALLBACK_T enableCallback;

	if ( copy_from_user(&enableCallback, (void*)ulArg, sizeof(VDEC_KDRV_ENABLE_CALLBACK_T)) )	{ return -EFAULT; }

	ui8Ch = enableCallback.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_NOTI_EnableCallback(ui8Ch, enableCallback.bInputNoti, enableCallback.bFeedNoti, enableCallback.bDecodeNoti, enableCallback.bDisplayNoti);

	return (eRet);
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
int VDEC_IO_CallbackNoti(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_CALLBACK_INFO_T cbInfo;

	if ( copy_from_user(&cbInfo, (void*)ulArg, sizeof(VDEC_KDRV_IO_CALLBACK_INFO_T)) )	{ return -EFAULT; }

	ui8Ch = cbInfo.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetCbInfo(ui8Ch, &cbInfo.info) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &cbInfo, sizeof(VDEC_KDRV_IO_CALLBACK_INFO_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetSeqh(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_DEC_SEQH_OUTPUT_T ioDecSeqhOutput;

	if ( copy_from_user(&ioDecSeqhOutput, (void*)ulArg, sizeof(VDEC_KDRV_IO_DEC_SEQH_OUTPUT_T)) )	{ return -EFAULT; }

	ui8Ch = ioDecSeqhOutput.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetSeqh(ui8Ch, &ioDecSeqhOutput.seqh) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioDecSeqhOutput, sizeof(VDEC_KDRV_IO_DEC_SEQH_OUTPUT_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetPicd(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_DEC_PICD_OUTPUT_T ioDecPicdOutput;

	if ( copy_from_user(&ioDecPicdOutput, (void*)ulArg, sizeof(VDEC_KDRV_IO_DEC_PICD_OUTPUT_T)) )	{ return -EFAULT; }

	ui8Ch = ioDecPicdOutput.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetPicd(ui8Ch, &ioDecPicdOutput.picd) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioDecPicdOutput, sizeof(VDEC_KDRV_IO_DEC_PICD_OUTPUT_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetRunningFb(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_FB_INFO_T ioFbInfo;

	if ( copy_from_user(&ioFbInfo, (void*)ulArg, sizeof(VDEC_KDRV_IO_FB_INFO_T)) )	{ return -EFAULT; }

	ui8Ch = ioFbInfo.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_KTOP_GetRunningFb(ui8Ch, &ioFbInfo.ui32PicWidth, &ioFbInfo.ui32PicHeight, &ioFbInfo.ui32uID, &ioFbInfo.ui64TimeStamp, &ioFbInfo.ui32PTS, &ioFbInfo.si32FrameFd) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioFbInfo, sizeof(VDEC_KDRV_IO_FB_INFO_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetDisp(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_DISP_OUTPUT_T ioDispOutput;

	if ( copy_from_user(&ioDispOutput, (void*)ulArg, sizeof(VDEC_KDRV_IO_DISP_OUTPUT_T)) )	{ return -EFAULT; }

	ui8Ch = ioDispOutput.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetDisp(ui8Ch, &ioDispOutput.disp) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioDispOutput, sizeof(VDEC_KDRV_IO_DISP_OUTPUT_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetLatestDecUID(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_GET_LATEST_UID_T ioLatestUID;

	if ( copy_from_user(&ioLatestUID, (void*)ulArg, sizeof(VDEC_KDRV_IO_GET_LATEST_UID_T)) )	{ return -EFAULT; }

	ui8Ch = ioLatestUID.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetLatestDecUID(ui8Ch, &ioLatestUID.uID) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioLatestUID, sizeof(VDEC_KDRV_IO_GET_LATEST_UID_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetLatestDispUID(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_GET_LATEST_UID_T ioLatestUID;

	if ( copy_from_user(&ioLatestUID, (void*)ulArg, sizeof(VDEC_KDRV_IO_GET_LATEST_UID_T)) )	{ return -EFAULT; }

	ui8Ch = ioLatestUID.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( VDEC_NOTI_GetLatestDispUID(ui8Ch, &ioLatestUID.uID) == FALSE )
		eRet = NOT_OK;

	if ( copy_to_user( (void*)ulArg, &ioLatestUID, sizeof(VDEC_KDRV_IO_GET_LATEST_UID_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetDisplayFps(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_FRAMERATE_T ioFrameRate;

	if ( copy_from_user(&ioFrameRate, (void*)ulArg, sizeof(VDEC_KDRV_IO_FRAMERATE_T)) )	{ return -EFAULT; }

	ui8Ch = ioFrameRate.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_GetDisplayFps(ui8Ch, &ioFrameRate.ui32FrameRateRes, &ioFrameRate.ui32FrameRateDiv);

	if ( copy_to_user( (void*)ulArg, &ioFrameRate, sizeof(VDEC_KDRV_IO_FRAMERATE_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_GetDropFps(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_FRAMERATE_T ioFrameRate;

	if ( copy_from_user(&ioFrameRate, (void*)ulArg, sizeof(VDEC_KDRV_IO_FRAMERATE_T)) )	{ return -EFAULT; }

	ui8Ch = ioFrameRate.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_GetDropFps(ui8Ch, &ioFrameRate.ui32FrameRateRes, &ioFrameRate.ui32FrameRateDiv);

	if ( copy_to_user( (void*)ulArg, &ioFrameRate, sizeof(VDEC_KDRV_IO_FRAMERATE_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_SetBaseTime(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_BASETIME_T ioBaseTime;

	if ( copy_from_user(&ioBaseTime, (void*)ulArg, sizeof(VDEC_KDRV_IO_BASETIME_T)) )	{ return -EFAULT; }

	ui8Ch = ioBaseTime.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( ioBaseTime.clkID != 0xFF )
	{
		eRet = VDEC_KTOP_SetBaseTime(ui8Ch, ioBaseTime.clkID, ioBaseTime.stcBaseTime, ioBaseTime.ptsBaseTime);
	}

	return (eRet);
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
int VDEC_IO_GetBaseTime(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_BASETIME_T ioBaseTime;

	if ( copy_from_user(&ioBaseTime, (void*)ulArg, sizeof(VDEC_KDRV_IO_BASETIME_T)) )	{ return -EFAULT; }

	ui8Ch = ioBaseTime.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if( ioBaseTime.clkID != 0xFF )
	{
		eRet = VDEC_KTOP_GetBaseTime(ui8Ch, ioBaseTime.clkID, &ioBaseTime.stcBaseTime, &ioBaseTime.ptsBaseTime);
	}

	if ( copy_to_user( (void*)ulArg, &ioBaseTime, sizeof(VDEC_KDRV_IO_BASETIME_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_CtrlUsrd(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_CTRL_USRD_T ioCtrlUsrd;

	if ( copy_from_user(&ioCtrlUsrd, (void*)ulArg, sizeof(VDEC_KDRV_IO_CTRL_USRD_T)) )	{ return -EFAULT; }

	ui8Ch = ioCtrlUsrd.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_SetUserDataEn(ui8Ch, ioCtrlUsrd.bEnable);
	if(eRet == -1)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] get User Data Address Error\n", ui8Ch);
	}

	if( ioCtrlUsrd.bEnable == TRUE )
	{
		eRet = VDEC_KTOP_GetUserDataInfo(ui8Ch, (UINT32 *)&ioCtrlUsrd.usrPA, (UINT32 *)&ioCtrlUsrd.usrSz);
		if(eRet == -1)
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] get User Data Address Error\n", ui8Ch);
		}
	}
	else
	{
		ioCtrlUsrd.usrPA = 0x0;
		ioCtrlUsrd.usrSz = 0x0;
	}

	if ( copy_to_user( (void*)ulArg, &ioCtrlUsrd, sizeof(VDEC_KDRV_IO_CTRL_USRD_T)))	{ return -EFAULT; }

	return (eRet);
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
int VDEC_IO_SetFreeDPB(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_FREE_DPB_T ioFreeDpb;

	if ( copy_from_user(&ioFreeDpb, (void*)ulArg, sizeof(VDEC_KDRV_IO_FREE_DPB_T)) )	{ return -EFAULT; }

	ui8Ch = ioFreeDpb.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_SetFreeDPB(ui8Ch, ioFreeDpb.si32FreeFrameFd);

	return (eRet);
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
int VDEC_IO_ReconfigDPB(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_NULL_PARAM_T ioReconfigDpb;

	if ( copy_from_user(&ioReconfigDpb, (void*)ulArg, sizeof(VDEC_KDRV_IO_NULL_PARAM_T)) )	{ return -EFAULT; }

	ui8Ch = ioReconfigDpb.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_ReconfigDPB(ui8Ch);

	return (eRet);
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
int VDEC_IO_GetBufferStatus(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_BUFFER_STATUS_T bufferStatus;

	if ( copy_from_user(&bufferStatus, (void*)ulArg, sizeof(VDEC_KDRV_IO_BUFFER_STATUS_T)) )	{ return -EFAULT; }

	ui8Ch = bufferStatus.ch;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	eRet = VDEC_KTOP_GetBufferStatus(ui8Ch,
									&bufferStatus.cpb_depth, &bufferStatus.cpb_size,
									&bufferStatus.auq_depth, &bufferStatus.dq_depth );

	if ( copy_to_user( (void*)ulArg, &bufferStatus, sizeof(VDEC_KDRV_IO_BUFFER_STATUS_T)))	{ return -EFAULT; }

	return (eRet);
}

/**
********************************************************************************
* @brief
*   initialize vdec.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_InitChannel(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = 0;
	UINT8 retOpen = 3;
	VDEC_KDRV_IO_OPEN_CH_T pInitCh[1];
	UINT8 vcodec = 0x0;
	VDEC_SRC_T eInSrc;
	VDEC_DST_T eOutDst;
	BOOLEAN bForThumbnail = FALSE;
	BOOLEAN bDualDecoding = FALSE;

	UINT32 ui32UDAddr=0;
	UINT32 ui32UDSize=0;

	UINT8	ui8VesCh;
	UINT8	ui8VdcCh;
	UINT8	ui8VdsCh;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }
	if (copy_from_user(pInitCh, (void*)ulArg, sizeof(*pInitCh))) return -EFAULT;

	vcodec = pInitCh->vcodec;
	switch(pInitCh->src)
	{
	case VDEC_KDRV_SRC_SDEC0:
		eInSrc = VDEC_SRC_SDEC0;
		break;
	case VDEC_KDRV_SRC_SDEC1:
		eInSrc = VDEC_SRC_SDEC1;
		break;
	case VDEC_KDRV_SRC_SDEC2:
		eInSrc = VDEC_SRC_SDEC2;
		break;
	case VDEC_KDRV_SRC_TVP:
		eInSrc = VDEC_SRC_TVP;
		break;
	case VDEC_KDRV_SRC_BUFF0:
		eInSrc = VDEC_SRC_BUFF0;
		break;
	case VDEC_KDRV_SRC_BUFF1:
		eInSrc = VDEC_SRC_BUFF1;
		break;
	case VDEC_KDRV_SRC_GRP_BUFF0:
		eInSrc = VDEC_SRC_GRP_BUFF0;
		break;
	case VDEC_KDRV_SRC_GRP_BUFF1:
		eInSrc = VDEC_SRC_GRP_BUFF1;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] VDEC Input Source Param %d", ui8Ch, pInitCh->src);
		eRet = -1;
		goto error;
	}

	switch(pInitCh->dst)
	{
	case VDEC_KDRV_DST_DE0:
		eOutDst = VDEC_DST_DE0;
		break;
	case VDEC_KDRV_DST_DE1:
		eOutDst = VDEC_DST_DE1;
		break;
	case VDEC_KDRV_DST_BUFF:
		eOutDst = VDEC_DST_BUFF;
		break;
	case VDEC_KDRV_DST_GRP_BUFF0:
		eOutDst = VDEC_DST_GRP_BUFF0;
		break;
	case VDEC_KDRV_DST_GRP_BUFF1:
		eOutDst = VDEC_DST_GRP_BUFF1;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] VDEC Output Destination Param %d", ui8Ch, pInitCh->dst);
		eRet = -1;
		goto error;
	}

	switch(pInitCh->opmode)
	{
	case VDEC_KDRV_OPMOD_NORMAL:
		break;
	case VDEC_KDRV_OPMOD_ONE_FRAME:
		bForThumbnail = TRUE;
		break;
	case VDEC_KDRV_OPMOD_DUAL:
		bDualDecoding = TRUE;
		break;
	case VDEC_KDRV_OPMOD_GRAPHIC_BUFF:
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] VDEC Operation Mode Param %d", ui8Ch, pInitCh->opmode);
		eRet = -1;
		goto error;
	}

	retOpen = VDEC_KTOP_Open(ui8Ch, vcodec, eInSrc, eOutDst,
							pInitCh->noAdpStr, bForThumbnail,
							pInitCh->bufDelaySize, TRUE,
							0xFFFFFFFF, VDEC_KDRV_WIDTH_INVALID, VDEC_KDRV_HEIGHT_INVALID,
							bDualDecoding, FALSE,
							pInitCh->useGstc, pInitCh->disDelayOffset, 0xFF,
							&ui8VesCh, &ui8VdcCh, &ui8VdsCh);

	if(retOpen==1)
	{
		VDEC_KTOP_Close(ui8Ch);
		retOpen = VDEC_KTOP_Open(ui8Ch, vcodec, eInSrc, eOutDst,
								pInitCh->noAdpStr, bForThumbnail,
								pInitCh->bufDelaySize, TRUE,
								0xFFFFFFFF, VDEC_KDRV_WIDTH_INVALID, VDEC_KDRV_HEIGHT_INVALID,
								bDualDecoding, FALSE,
								pInitCh->useGstc, pInitCh->disDelayOffset, 0xFF,
								&ui8VesCh, &ui8VdcCh, &ui8VdsCh);
	}

	if(retOpen>1)
	{
		eRet = -1;
		VDEC_KDRV_Message(ERROR, "VDEC_OPEN FAIL : %d\n", retOpen);
		goto error;
	}

	VDEC_NOTI_InitOutputInfo(ui8Ch);

	VDEC_KDRV_Message(_TRACE, "[IO] Ch: %d, ... Codec :: %d, OPMode :: %d", ui8Ch, pInitCh->vcodec, pInitCh->opmode);

	eRet = VDEC_KTOP_GetUserDataInfo(ui8Ch, (UINT32 *)&ui32UDAddr, (UINT32 *)&ui32UDSize);
	if(eRet == -1)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] get User Data Address Error\n", ui8Ch);
		VDEC_KTOP_Close(ui8Ch);
		goto error;
	}

	pInitCh->usrPA = ui32UDAddr;
	pInitCh->usrSz = ui32UDSize;

error:

	if (copy_to_user((void*)ulArg, pInitCh, sizeof(*pInitCh))) return -EFAULT;

	return (eRet);
}

/**
********************************************************************************
* @brief
*   ch close.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_CloseChannel(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = 0;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_KTOP_Close(ui8Ch);
	VDEC_NOTI_SetMask(ui8Ch, (UINT32)0x0);
	VDEC_NOTI_InitOutputInfo(ui8Ch);

	return (eRet);
}

/**
********************************************************************************
* @brief
*   standard set.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_StartChannel(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = 0;
	VDEC_KDRV_IO_PLAY_T pioStart[1];

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if ( copy_from_user(pioStart, (void*)ulArg, sizeof(*pioStart)) )	{ return -EFAULT; }

	if( pioStart->speed == 0 )
		pioStart->cmd = VDEC_KDRV_CMD_PAUSE;

	switch( pioStart->cmd )
	{
	case VDEC_KDRV_CMD_PLAY :
		eRet = VDEC_KTOP_Play(ui8Ch, pioStart->speed);
		break;
	case VDEC_KDRV_CMD_PAUSE :
		eRet = VDEC_KTOP_Pause(ui8Ch);
		break;
	case VDEC_KDRV_CMD_STEP :
		eRet = VDEC_KTOP_Step(ui8Ch);
		break;
	case VDEC_KDRV_CMD_FREEZE :
		eRet = VDEC_KTOP_Freeze(ui8Ch);
		break;
	default :
		break;
	}

	return (eRet);
}

/**
********************************************************************************
* @brief
*   ch stop.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_StopChannel(UINT8 ui8Ch, UINT32 option)
{
	int eRet = 0;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_KTOP_Close(ui8Ch);

	return (eRet);
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
int VDEC_FlushChannel(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = OK;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	VDEC_KTOP_Flush(ui8Ch);

	return (eRet);
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
int VDEC_IO_Restart(void)
{
	int eRet = 0;
	UINT8 ui8Vch;

	for(ui8Vch=0; ui8Vch<VDEC_NUM_OF_CHANNEL; ui8Vch++)
		VDEC_KTOP_Restart(ui8Vch);

	return (eRet);
}

/**
********************************************************************************
* @brief
*   set play option.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
* @return
*  int
********************************************************************************
*/
int VDEC_IO_SetPLAYOption(UINT8 ui8Ch, UINT32 ulArg)
{
	VDEC_KDRV_IO_PLAY_SET_T playSetOp;
	int eRet = 0;
	int errLine = 0;
	UINT32 u32Enable;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ return INVALID_PARAMS; }

	if (copy_from_user(&playSetOp, (void*)ulArg, sizeof(VDEC_KDRV_IO_PLAY_SET_T)) ) return -EFAULT;

	if(playSetOp.picScanMode < VDEC_KDRV_PIC_SCAN_MAXN)
	{
		eRet = VDEC_KTOP_SetPicScanMode(ui8Ch, (UINT8)playSetOp.picScanMode);
		if(eRet !=0 )
		{
			errLine=__LINE__;
			goto error;
		}
	}

	if(playSetOp.srcScanType < VDEC_KDRV_PIC_SCAN_MAXN)
	{
		eRet = VDEC_KTOP_SetSrcScanType(ui8Ch, (UINT8)playSetOp.srcScanType);
		if(eRet !=0 )
		{
			errLine=__LINE__;
			goto error;
		}
	}

	if(playSetOp.notifyMask < VDEC_KDRV_NOTIFY_MSK_ENDMARK)
	{
		u32Enable = (playSetOp.notifyMask & VDEC_KDRV_NOTIFY_MSK_USRD) ? 1 : 0;
		eRet = VDEC_KTOP_SetUserDataEn(ui8Ch, u32Enable);
		if(eRet !=0 )
		{
			errLine=__LINE__;
			goto error;
		}

		if(VDEC_NOTI_Alloc(ui8Ch, 128)<0)
		{
			eRet = -ENOMEM;
			errLine=__LINE__;
			goto error;
		}
		VDEC_NOTI_SetMask(ui8Ch, (UINT32)playSetOp.notifyMask);
	}

	if(playSetOp.syncOn < VDEC_KDRV_SYNC_MAXN)
	{
		eRet = VDEC_KTOP_SetLipSync(ui8Ch, playSetOp.syncOn);
		if(eRet !=0 )
		{
			errLine=__LINE__;
			goto error;
		}
	}

	if(playSetOp.freeze < VDEC_KDRV_FREEZE_MAXN)
	{
		if( playSetOp.freeze == VDEC_KDRV_FREEZE_SET )
			eRet = VDEC_KTOP_Freeze(ui8Ch);
		else
			eRet = VDEC_KTOP_Play(ui8Ch, 1000);

		if(eRet !=0 )
		{
			errLine=__LINE__;
			goto error;
		}
	}

error :

	if(eRet != 0)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][ERR] Err Line : %d, %s", ui8Ch, errLine, __FUNCTION__);
	}

	return eRet;
}

/**
********************************************************************************
* @brief
*   set play option.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
* @return
*  int
********************************************************************************
*/
int VDEC_IO_GetPLAYOption(UINT8 ui8Ch, UINT32 ulArg)
{
	VDEC_KDRV_IO_PLAY_GET_T playGetOp;
	int eRet = 0;

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL )	{ eRet = INVALID_PARAMS;	goto error; }

	if (copy_from_user(&playGetOp, (void*)ulArg, sizeof(VDEC_KDRV_IO_PLAY_GET_T)) ) return -EFAULT;

	if(playGetOp.clkID != 0xFF)
	{
		eRet = VDEC_KTOP_GetBaseTime(ui8Ch, playGetOp.clkID, &playGetOp.stcBaseTime, &playGetOp.ptsBaseTime);
	}

	eRet = copy_to_user((void*)ulArg, &playGetOp, sizeof(VDEC_KDRV_IO_PLAY_GET_T));

	if(eRet)	{ eRet = -EFAULT; goto error;}

error :

	return eRet;
}

/**
********************************************************************************
* @brief
*   set buffer level.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_SetBufferLVL	(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_IO_BUFFER_LVL_T stpLXVdecIOBufferLvl[1];

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)	{ eRet = INVALID_PARAMS;	goto error; }
	if ( ui8Ch >  1 )					{ eRet = NOT_SUPPORTED;		goto error; }

	if ( copy_from_user(stpLXVdecIOBufferLvl, (void*)ulArg, sizeof(*stpLXVdecIOBufferLvl)) )
										{ eRet = -EFAULT;			goto error; }

error:
	return (eRet);
}

/**
********************************************************************************
* @brief
*   get buffer level.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_GetFiemwareVersion(UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_IO_VERSION_T *stpLXIOVersion;

	stpLXIOVersion = (VDEC_KDRV_IO_VERSION_T *)ulArg;

	stpLXIOVersion->ui32RTLVersion = TOP_HAL_GetVersion();
	stpLXIOVersion->ui32FirmwareVersion = MCU_HAL_GetVersion();

	VDEC_KDRV_Message(DBG_SYS, "RTLVersion[0x%08x]", stpLXIOVersion->ui32RTLVersion);
	VDEC_KDRV_Message(DBG_SYS, "FirmwareVersion[0x%08x]", stpLXIOVersion->ui32FirmwareVersion);

	return (eRet);
}

/**
********************************************************************************
* @brief
*   set register.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_SetRegister(UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_SET_REG_T *stpLXVdecSetReg;

	stpLXVdecSetReg = (VDEC_KDRV_SET_REG_T *) ulArg;

	REG_WRITE32(stpLXVdecSetReg ->addr, stpLXVdecSetReg ->value);
	VDEC_KDRV_Message(DBG_SYS, "REG_WRITE[0x%08x] [0x%08x]", stpLXVdecSetReg->addr, stpLXVdecSetReg ->value);

	return (eRet);
}

/**
********************************************************************************
* @brief
*   get register.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_GetRegister(UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_GET_REG_T *stpLXVdecGetReg;
	UINT32 ui32RetValue = 0x0;

	stpLXVdecGetReg = (VDEC_KDRV_GET_REG_T *) ulArg;

	ui32RetValue = REG_READ32(stpLXVdecGetReg->addr);
	VDEC_KDRV_Message(DBG_SYS, "REG_READ[0x%08x]", ui32RetValue);
	stpLXVdecGetReg->value = ui32RetValue;

	return (eRet);
}

/**
********************************************************************************
* @brief
*   enable log.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_EnableLog(UINT32 ulArg)
{
	int eRet = OK;
	int idx;

	if(g_vdec_debug_fd<0)
		return (eRet);

	for ( idx = 0; idx < LX_MAX_MODULE_DEBUG_NUM; idx++)
	{
		if ( ulArg & (1<<idx) ) OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, idx, DBG_COLOR_NONE );
		else					OS_DEBUG_DisableModuleByIndex( g_vdec_debug_fd, idx);
	}

	if (ulArg)	OS_DEBUG_EnableModule(g_vdec_debug_fd);
	else		OS_DEBUG_DisableModule(g_vdec_debug_fd);

	IPC_REG_Set_McuDebugMask(ulArg);

	return (eRet);
}

/**
********************************************************************************
* @brief
*   command for debugging.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_DbgCmd(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_DBG_CMD_T stDbgCmd[1];

	if ( ui8Ch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO][Err] Chnum : %d, %s", ui8Ch,  __FUNCTION__ );
		 eRet = -EFAULT;
		 goto error;
	}

	if ( copy_from_user(stDbgCmd, (void*)ulArg, sizeof(VDEC_KDRV_DBG_CMD_T)) )
	{
		eRet = -EFAULT;
		goto error;
	}

	switch( stDbgCmd->ui32Module )
	{
		case VDEC_KDRV_DBGCMD_MOD_TOP:
		{
			if(stDbgCmd->ui32CmdType == VDEC_KDRV_DBGCMD_RESET)
			{
				VDEC_KTOP_Reset(ui8Ch);
			}
#ifdef USE_MCU
			else if(stDbgCmd->ui32CmdType == VDEC_KDRV_DBGCMD_MCU_RESET)
			{
				VDEC_MCU_Reset();
			}
#endif
		}
		break;
		case VDEC_KDRV_DBGCMD_MOD_PDEC:
			break;
		case VDEC_KDRV_DBGCMD_MOD_FEED:
			break;
		case VDEC_KDRV_DBGCMD_MOD_MSVC:
			break;
		case VDEC_KDRV_DBGCMD_MOD_SYNC:
			if(stDbgCmd->ui32CmdType == VDEC_KDRV_DBGCMD_SYNC_DISPLAY_OFFSET)
			{
				if(stDbgCmd->ui32nSize==1)
				{
					VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Dbg] Display Offset: %d, %s", ui8Ch, stDbgCmd->ui32Data[0],  __FUNCTION__ );
					VDEC_KTOP_SetDBGDisplayOffset(ui8Ch, stDbgCmd->ui32Data[0]);
				}
			}
			break;
		default:
			VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] not defined Module : %d, %s", ui8Ch, stDbgCmd->ui32Module,  __FUNCTION__ );
		break;
	}

error:

	return (eRet);
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
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_UpdateBuffer(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_UPDATE_BUFFER_T	ioUpdateBuffer;
	UINT64		ui64TimeStamp_90kHzTick;
	UINT32		ui32TimeStamp_90kHzTick;
	VDEC_KTOP_VAU_T	eVAU_Type;

	eRet = copy_from_user(&ioUpdateBuffer, (void*)ulArg, sizeof(VDEC_KDRV_IO_UPDATE_BUFFER_T));

	if ( eRet ) return -EFAULT;

	ui8Ch = ioUpdateBuffer.ch;

//	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Time Stamp: %llu, %s", ui8VesCh, ioUpdateBuffer.ui64TimeStamp, __FUNCTION__ );

	ui64TimeStamp_90kHzTick = ioUpdateBuffer.ui64TimeStamp;
	if( ui64TimeStamp_90kHzTick == VDEC_UNKNOWN_TIMESTAMP )
	{
		ui32TimeStamp_90kHzTick = VDEC_UNKNOWN_PTS;
	}
	else if( ioUpdateBuffer.is90kHzTick == TRUE )
	{
		ui32TimeStamp_90kHzTick = (UINT32)(ui64TimeStamp_90kHzTick);
		ui32TimeStamp_90kHzTick = ui32TimeStamp_90kHzTick & 0x0FFFFFFF;
	}
	else
	{
		ui64TimeStamp_90kHzTick *= 9;
		do_div(ui64TimeStamp_90kHzTick, 100000);		// = Xns * 90,000 / 1,000,000,000
		ui32TimeStamp_90kHzTick = (UINT32)(ui64TimeStamp_90kHzTick);
		ui32TimeStamp_90kHzTick = ui32TimeStamp_90kHzTick & 0x0FFFFFFF;
	}

	switch( ioUpdateBuffer.au_type )
	{
	case VDEC_KDRV_VAU_SEQH :
		eVAU_Type = VDEC_KTOP_VAU_SEQH;
		break;
	case VDEC_KDRV_VAU_SEQE :
		eVAU_Type = VDEC_KTOP_VAU_SEQE;
		break;
	case VDEC_KDRV_VAU_DATA :
		eVAU_Type = VDEC_KTOP_VAU_DATA;
		break;
	case VDEC_KDRV_VAU_EOS :
		eVAU_Type = VDEC_KTOP_VAU_EOS;
		break;
	case VDEC_KDRV_VAU_UNKNOWN :
		eVAU_Type = VDEC_KTOP_VAU_UNKNOWN;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] AU Type: %d, %s", ui8Ch, ioUpdateBuffer.au_type, __FUNCTION__ );
		return -EFAULT;
	}

	if( VDEC_KTOP_UpdateCompressedBuffer(ui8Ch,
							eVAU_Type,
							ioUpdateBuffer.au_ptr,
							ioUpdateBuffer.au_size,
							(fpCpbCopyfunc)copy_from_user,
							ioUpdateBuffer.ui32uID,
							ioUpdateBuffer.ui64TimeStamp,
							ui32TimeStamp_90kHzTick,
							ioUpdateBuffer.frRes,
							ioUpdateBuffer.frDiv,
							ioUpdateBuffer.width,
							ioUpdateBuffer.height) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] AU Ptr:0x%X, AU Size:0x%X - %s", ui8Ch, (UINT32)ioUpdateBuffer.au_ptr, ioUpdateBuffer.au_size, __FUNCTION__ );
		eRet = NOT_ENOUGH_RESOURCE;
	}

	return (eRet);
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
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_UpdateGraphicBuffer(UINT32 ulArg)
{
	UINT8 ui8Ch;
	int eRet = OK;
	VDEC_KDRV_IO_UPDATE_GRAPHIC_BUFFER_T	ioGrahpicBuffer;
	UINT64		ui64TimeStamp_90kHzTick;
	UINT32		ui32TimeStamp_90kHzTick;

	eRet = copy_from_user(&ioGrahpicBuffer, (void*)ulArg, sizeof(VDEC_KDRV_IO_UPDATE_GRAPHIC_BUFFER_T));

	if ( eRet ) return -EFAULT;

	ui8Ch = ioGrahpicBuffer.ch;

//	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Time Stamp: %llu, %s", ui8VesCh, ioUpdateBuffer.ui64TimeStamp, __FUNCTION__ );

	ui64TimeStamp_90kHzTick = ioGrahpicBuffer.ui64TimeStamp;
	if( ui64TimeStamp_90kHzTick == VDEC_UNKNOWN_TIMESTAMP )
	{
		ui32TimeStamp_90kHzTick = VDEC_UNKNOWN_PTS;
	}
	else if( ioGrahpicBuffer.is90kHzTick == TRUE )
	{
		ui32TimeStamp_90kHzTick = (UINT32)(ui64TimeStamp_90kHzTick);
		ui32TimeStamp_90kHzTick = ui32TimeStamp_90kHzTick & 0x0FFFFFFF;
	}
	else
	{
		ui64TimeStamp_90kHzTick *= 9;
		do_div(ui64TimeStamp_90kHzTick, 100000);		// = Xns * 90,000 / 1,000,000,000
		ui32TimeStamp_90kHzTick = (UINT32)(ui64TimeStamp_90kHzTick);
		ui32TimeStamp_90kHzTick = ui32TimeStamp_90kHzTick & 0x0FFFFFFF;
	}

	if( VDEC_KTOP_UpdateGraphicBuffer(ui8Ch,
							ioGrahpicBuffer.si32FrameFd,
							ioGrahpicBuffer.ui32BufStride,
							ioGrahpicBuffer.ui32PicWidth,
							ioGrahpicBuffer.ui32PicHeight,
							ioGrahpicBuffer.ui64TimeStamp,
							ui32TimeStamp_90kHzTick,
							ioGrahpicBuffer.ui32FrameRateRes,
							ioGrahpicBuffer.ui32FrameRateDiv,
							ioGrahpicBuffer.ui32uID) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_IO%d][Err] Frame FD: 0x%X - %s", ui8Ch, ioGrahpicBuffer.si32FrameFd, __FUNCTION__ );
		eRet = NOT_ENOUGH_RESOURCE;
	}

	return (eRet);
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
void VDEC_IO_DBG_DumpFPQTS(struct seq_file *m)
{
#if 0
	int i=0;
	UINT32	ts_up, ts_tr, ts_wr;
#endif
	if (!m) return;

#if 0
	VDEC_CQ_DBG_DumpStartFPQTS(0);
	while( VDEC_CQ_DBG_DumpNextFPQTS(0, &ts_up, &ts_tr, &ts_wr) == TRUE )
	{
		seq_printf(m, "FPQ, %d, %u, %u, %u\n", i, ts_up, ts_tr, ts_wr);
		i++;
	}
#endif

	return;
}

/**
********************************************************************************
* @brief
*   enable log.
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*   ui32Arg :arguments from userspace
* @return
*  int
********************************************************************************
*/
int VDEC_IO_GetOutput(UINT8 ui8Ch, UINT32 ulArg)
{
	int eRet = OK;
	VDEC_KDRV_OUTPUT_T stpLXVdecOutput[1], *pOut;

	pOut = VDEC_NOTI_GetOutput(ui8Ch);

	memcpy( stpLXVdecOutput, pOut, sizeof(*stpLXVdecOutput));

	eRet = copy_to_user((void*)ulArg, stpLXVdecOutput, sizeof(*stpLXVdecOutput));

	if(eRet)	{ eRet = -EFAULT; goto error;}

error:

	return (eRet);
}

/** @} */
