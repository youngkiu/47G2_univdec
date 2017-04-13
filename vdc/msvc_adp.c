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
 * date       2012.05.24
 * note
 *
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "msvc_adp.h"

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#endif

#include "../vds/pts_drv.h"
#include "vdc_drv.h"

#include "../mcu/os_adap.h"
#include "../mcu/vdec_print.h"

#include "../mcu/ram_log.h"
#include "../hal/lg1152/lg1152_vdec_base.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/msvc_hal_api.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define	MSVC_MAX_FRAME_BUFF_NUM			64
#define	MSVC_MAX_SLICE_NUM				100
#define	MSVC_INTERNAL_BUFFER_SIZE		512
#define	MSVC_PREFETCH_SIZE				(MSVC_INTERNAL_BUFFER_SIZE * 2)	// double buffering
#define	MSVC_VCORE_SHARE_RANGE			16


/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	BOOLEAN	bUnmatchedMeta;
	UINT32	ui32FlushAge;
	UINT32	ui32DTS;
	UINT64 	ui64TimeStamp;
	UINT32 	ui32uID;
	UINT32	ui32InputGSTC;
	UINT32	ui32FeedGSTC;
	struct
	{
		UINT32		ui32Residual;
		UINT32		ui32Divider;
	} sFrameRate_Parser;
} S_MSVC_DEC_INFO_T;

typedef struct
{
	BOOLEAN	bUnmatchedMeta;
	UINT32	ui32FlushAge;
	UINT32	ui32DecodingSuccess;
	UINT32	ui32NumOfErrMBs;
	UINT32	ui32PicType;
	UINT32	ui32PTS;
	UINT64	ui64TimeStamp;
	UINT32	ui32uID;

	struct
	{
		UINT32		ui32Residual;
		UINT32		ui32Divider;
	} sFrameRate_Decoder;
	BOOLEAN	bVariableFrameRate;
	UINT32	ui32AspectRatio;
	UINT32	ui32PicWidth;
	UINT32	ui32PicHeight;
	UINT32	ui32CropLeftOffset;
	UINT32	ui32CropRightOffset;
	UINT32	ui32CropTopOffset;
	UINT32	ui32CropBottomOffset;
	E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
	UINT32	ui32DisplayPeriod;
	SINT32	i32FramePackArrange;		// 3D SEI
	E_DISPQ_3D_LR_ORDER_T eLR_Order;
	UINT16	ui16ParW;
	UINT16	ui16ParH;
	UINT32	ui32PictureStructure;

	struct
	{
		UINT32 	ui32PicType;
		UINT32	ui32Rpt_ff;
		UINT32	ui32Top_ff;
		UINT32	ui32BuffAddr;
		UINT32	ui32Size;
	} sUsrData;

	UINT32	ui32ActiveFMT;
	UINT32	ui32ProgSeq;
	UINT32	ui32ProgFrame;
	SINT32	si32FrmPackArrSei;
	UINT32	ui32LowDelay;
} S_MSVC_DISP_INFO_T;

typedef struct
{
	struct
	{
		E_MSVC_CODEC_T	eCodecType;
		BOOLEAN		bNoReordering;
		UINT8		ui8VdcCh;
		BOOLEAN		bDual3D;
		UINT32		ui32CPBAddr;
		UINT32		ui32CPBSize;
		BOOLEAN		bRingBufferMode;
		BOOLEAN		bLowLatency;

		// for PicOption
		E_DEC_PARAM_SKIP_FRAME_MODE_T	eSkipFrameMode;
		BOOLEAN		bUserDataEnable;
		BOOLEAN		bSrcSkipB;

		// for MSVC_CODEC_DIVX3
		UINT32		ui32Width;
		UINT32		ui32Height;
	} Config;
	struct
	{
		BOOLEAN		bFlushCmd;
		BOOLEAN		bEosCmd;
		UINT32		ui32EosFrmCnt;
		UINT32		ui32PictureStructure;
		UINT32		ui32PackedMode;
		BOOLEAN		bFieldDone;
		BOOLEAN		bResetting;
	} Status;
	struct
	{
		UINT32			ui32CmdCnt;
		S_MSVC_CMD_T	sReceiveCmd[MSVC_MAX_SLICE_NUM];

		UINT32			ui32UsedCnt;
		S_MSVC_CMD_T	sSelectCmd;

		UINT32			ui32CmdDoneRatio;

		UINT32			ui32GSTC_Prev;
	} Cmd;
	struct
	{
		UINT32		ui32FrameSizeY;
		UINT32		ui32FrameSizeCb;
		UINT32		ui32FrameSizeCr;
		UINT32		ui32Stride;

		UINT32		ui32FrameBufDelay;
		UINT32		ui32MinFrmCnt;
		UINT32		ui32TotalFrmCnt;

		FRAME_BUF_INFO_T stFrameBufInfo;
		UINT32		ui32MsvcMvColBufUsed;
	} SeqInit;
	struct
	{
		struct
		{
			UINT32		ui32WrIndex;
			UINT32		ui32RdIndex;
			S_MSVC_DEC_INFO_T	DecInfo[MSVC_MAX_FRAME_BUFF_NUM];
		} DecOrder;
		struct
		{
			UINT32		ui32BufferFrame;
			S_MSVC_DISP_INFO_T	DispInfo[MSVC_MAX_FRAME_BUFF_NUM];
		} DispOrder;
	} PicDoneQ;
	struct
	{
		UINT32		ui32SendCmdCnt;
		UINT32		ui32RunDoneCnt;
		UINT32		ui32LogTick_SendCmd;
		UINT32		ui32LogTick_RunDone;

		struct
		{
			S_MSVC_CMD_T	sSelectCmd_Prev;
			UINT32			ui32CmdCnt_Prev;
			UINT32			ui32UsedCnt_Prev;
			UINT32			ui32CurWrPtr;
			UINT32			ui32CurRdPtr;
		} Done;

		struct
		{
			UINT32		ui32AuAddr;
			UINT32		ui32AuSize;
			UINT32		ui32AuEnd;
			UINT32		ui32CurWrPtr;
			UINT32		ui32CurRdPtr;
		} Send;

		UINT32		ui32AliveFrame;
	} Debug;
}S_MSVC_ADP_DB_T;

typedef struct
{
	UINT8 bitStreamFormat;
	UINT8 seqAspClass;
	UINT8 u32AuxStd;
	UINT8 u32SeqOption;
} MSVC_STD_INFO_T;

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
static S_MSVC_ADP_DB_T gsMsvcAdp[NUM_OF_MSVC_CHANNEL];
static UINT32 (*gfpMSVC_GetUsedFrame)(UINT8 ui8VdcCh);
static void (*gfpMSVC_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param);
static UINT32 (*gfpMSVC_Get_CPBPhyWrAddr)(UINT8 ui8VdcCh);
static struct
{
	struct
	{
		UINT32	ui32OpenedBitMask;
		UINT32 	ui32RunningBitMask;
		UINT32 	ui32ClosingBitMask;
		volatile BOOLEAN	bResetting;
	} sStatus;

	struct
	{
		UINT32	ui32PicRunCmdTime;
		UINT32	ui32PicRunDoneTime;

		UINT32	ui32EventTime;
		UINT32	ui32RunTime;
		UINT32	ui32TotalTime;

		UINT32	ui32MeasureCnt;
		BOOLEAN	bWantToSeePerformance;
		UINT32	ui32VCoreShare;
	} sDebug;

} gsMsvcCore[MSVC_NUM_OF_CORE];

static UINT32 gui32MsvcMvColBufPool = 0xFFFFFFFF;	// 0x40000 * 32


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
void MSVC_ADP_Init(	UINT32 (*_fpVDC_GetUsedFrame)(UINT8 ui8VdcCh),
						void (*_fpVDC_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param),
						UINT32 (*_fpVDC_Get_CPBPhyWrAddr)(UINT8 ui8VdcCh) )
{
	UINT8	ui8CoreNum;
	UINT8	ui8MsvcCh;

	gfpMSVC_GetUsedFrame = _fpVDC_GetUsedFrame;
	gfpMSVC_ReportPicInfo = _fpVDC_ReportPicInfo;
	gfpMSVC_Get_CPBPhyWrAddr = _fpVDC_Get_CPBPhyWrAddr;

	for( ui8CoreNum = 0; ui8CoreNum < MSVC_NUM_OF_CORE; ui8CoreNum++ )
	{
		gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask = 0x0;
		gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask = 0x0;
		gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask = 0x0;
		gsMsvcCore[ui8CoreNum].sStatus.bResetting = FALSE;

		gsMsvcCore[ui8CoreNum].sDebug.ui32PicRunCmdTime = 0x80000000;
		gsMsvcCore[ui8CoreNum].sDebug.ui32PicRunDoneTime = 0x80000000;
		gsMsvcCore[ui8CoreNum].sDebug.ui32EventTime = 0x80000000;
		gsMsvcCore[ui8CoreNum].sDebug.ui32RunTime = 0x0;
		gsMsvcCore[ui8CoreNum].sDebug.ui32TotalTime = 0x0;
		gsMsvcCore[ui8CoreNum].sDebug.ui32MeasureCnt = 0x0;
		gsMsvcCore[ui8CoreNum].sDebug.bWantToSeePerformance = FALSE;
		gsMsvcCore[ui8CoreNum].sDebug.ui32VCoreShare = 0;
	}

	for( ui8MsvcCh = 0; ui8MsvcCh < NUM_OF_MSVC_CHANNEL; ui8MsvcCh++ )
	{
		gsMsvcAdp[ui8MsvcCh].Config.eCodecType = MSVC_CODEC_INVALID;
	}

	gui32MsvcMvColBufPool = 0xFFFFFFFF;
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
UINT8 MSVC_ADP_Open(	UINT8 ui8VdcCh,
							E_MSVC_CODEC_T eCodecType,
							UINT32 ui32CPBAddr,
							UINT32 ui32CPBSize,
							BOOLEAN bRingBufferMode,
							BOOLEAN bLowLatency,
							BOOLEAN bDual3D,
							BOOLEAN bNoReordering,
							UINT32 ui32Width,
							UINT32 ui32Height)
{
	UINT8	ui8MsvcCh = 0xFF;
	UINT8	ui8CoreNum;
	UINT8	ui8InstanceNum;
	UINT32	ui32CoreActiveNum[MSVC_NUM_OF_CORE];
	UINT8	ui8CoreNum_Fewest = MSVC_NUM_OF_CORE;
	UINT32	ui32UseNum_Fewest = MSVC_NUM_OF_CHANNEL;

	VDEC_KDRV_Message(_TRACE, "[MSVC_ADP][DBG] VdcCh:%d, CodecType:%d, bNoReordering:%d, %s", ui8VdcCh, eCodecType, bNoReordering, __FUNCTION__ );

	for( ui8CoreNum = 0; ui8CoreNum < MSVC_NUM_OF_CORE; ui8CoreNum++ )
	{
		UINT32	ui32OpenedBitMask;

		ui32CoreActiveNum[ui8CoreNum] = 0;
		ui32OpenedBitMask = gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask & ~gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask;

		for( ui8InstanceNum = 0; ui8InstanceNum < MSVC_NUM_OF_CHANNEL; ui8InstanceNum++ )
		{
			if( ui32OpenedBitMask & (1 << ui8InstanceNum) )
			{
				ui32CoreActiveNum[ui8CoreNum]++;
			}
		}

		if( ui32CoreActiveNum[ui8CoreNum] <= ui32UseNum_Fewest )
		{
			ui8CoreNum_Fewest = ui8CoreNum;
			ui32UseNum_Fewest = ui32CoreActiveNum[ui8CoreNum];
		}
	}
	if( ui8CoreNum_Fewest == MSVC_NUM_OF_CORE )
	{
		for( ui8CoreNum = 0; ui8CoreNum < MSVC_NUM_OF_CORE; ui8CoreNum++ )
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP][Err] Not Enough MSVC Channel - Used Instance:0x%X, Closing:0x%X, %s(%d)",
										gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask, gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask,
										__FUNCTION__, __LINE__ );

		return 0xFF;
	}

	switch( eCodecType )
	{
	case MSVC_CODEC_H264_HP :
	case MSVC_CODEC_VC1_RCV_V1 :
	case MSVC_CODEC_VC1_RCV_V2 :
	case MSVC_CODEC_VC1_AP :
	case MSVC_CODEC_MPEG2_HP :
	case MSVC_CODEC_AVS :
		break;
	case MSVC_CODEC_H264_MVC :
	case MSVC_CODEC_H263 :
	case MSVC_CODEC_XVID :
	case MSVC_CODEC_DIVX3 :
	case MSVC_CODEC_DIVX4 :
	case MSVC_CODEC_DIVX5 :
	case MSVC_CODEC_RVX :
		if( ui8CoreNum_Fewest == 1 )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP][DBG] Change VCore - CodecType:%d, bNoReordering: %d, %s", eCodecType, bNoReordering, __FUNCTION__ );
			ui8CoreNum_Fewest = 0;
		}
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP][Err] Not Support Codec Type : %d %s", eCodecType, __FUNCTION__ );
		break;
	}

	if( ui32CoreActiveNum[ui8CoreNum_Fewest] > 1 )
	{
		for( ui8CoreNum = 0; ui8CoreNum < MSVC_NUM_OF_CORE; ui8CoreNum++ )
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP][Err] Not Enough MSVC Channel - Used Instance:0x%X, Closing:0x%X, %s(%d)",
										gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask, gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask,
										__FUNCTION__, __LINE__ );

		return 0xFF;
	}

	for( ui8InstanceNum = 0; ui8InstanceNum < MSVC_NUM_OF_CHANNEL; ui8InstanceNum++ )
	{
		if( !(gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32OpenedBitMask & (1 << ui8InstanceNum)) )
			break;
	}
	if( ui8InstanceNum == MSVC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP][Err] Not Enough MSVC Channel - Used Instance:0x%X, Closing:0x%X, %s(%d)",
									gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32OpenedBitMask, gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32ClosingBitMask,
									__FUNCTION__, __LINE__ );
		return 0xFF;
	}

	ui8MsvcCh = ui8CoreNum_Fewest * MSVC_NUM_OF_CHANNEL + ui8InstanceNum;

	if( gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32ClosingBitMask && (ui32CoreActiveNum[ui8CoreNum_Fewest] == 0) )
		MSVC_ADP_Reset(ui8MsvcCh);

	gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32OpenedBitMask |= (1 << ui8InstanceNum);
	gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32RunningBitMask &= ~(1 << ui8InstanceNum);

	gsMsvcAdp[ui8MsvcCh].Config.eCodecType = eCodecType;
	gsMsvcAdp[ui8MsvcCh].Config.bNoReordering = bNoReordering;
	gsMsvcAdp[ui8MsvcCh].Config.bDual3D = bDual3D;
	gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh = ui8VdcCh;
	gsMsvcAdp[ui8MsvcCh].Config.ui32CPBAddr = ui32CPBAddr;
	gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize = ui32CPBSize;
	gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode = bRingBufferMode;
	gsMsvcAdp[ui8MsvcCh].Config.bLowLatency = bLowLatency;
	gsMsvcAdp[ui8MsvcCh].Config.eSkipFrameMode = DEC_PARAM_SKIP_MODE_NO;
	gsMsvcAdp[ui8MsvcCh].Config.bUserDataEnable = FALSE;
	gsMsvcAdp[ui8MsvcCh].Config.bSrcSkipB = FALSE;
	gsMsvcAdp[ui8MsvcCh].Config.ui32Width = ui32Width;
	gsMsvcAdp[ui8MsvcCh].Config.ui32Height = ui32Height;

	gsMsvcAdp[ui8MsvcCh].Status.bFlushCmd = FALSE;
	gsMsvcAdp[ui8MsvcCh].Status.bEosCmd = FALSE;
	gsMsvcAdp[ui8MsvcCh].Status.ui32EosFrmCnt = 0;
	gsMsvcAdp[ui8MsvcCh].Status.ui32PictureStructure = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Status.ui32PackedMode = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Status.bFieldDone = FALSE;
	gsMsvcAdp[ui8MsvcCh].Status.bResetting = FALSE;

	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex = 0;
	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex = 0;
	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame = 0x0;
	gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame = 0x0;

	gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt = 0xFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio = 1;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32GSTC_Prev = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.eAuType = MSVC_AU_UNKNOWN;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.eAuType = MSVC_AU_UNKNOWN;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuAddr = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuSize = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuEnd = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS = VDEC_UNKNOWN_PTS;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS = VDEC_UNKNOWN_PTS;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui64TimeStamp = 0xFFFFFFFFFFFFFFFFLL;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32InputGSTC = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FeedGSTC = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateRes_Parser = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateDiv_Parser = 0xFFFFFFFF;

	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY = 0x0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCb = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCr = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32Stride = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt = 0;

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC_ADP%d][Err] Not Free MV_Col Buffer, Pool:0x%08X, Alloc:0%08X, %s", ui8MsvcCh,
									gui32MsvcMvColBufPool, gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed, __FUNCTION__);
		gui32MsvcMvColBufPool |= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed = 0x0;
	}

	gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt = 0;
	gsMsvcAdp[ui8MsvcCh].Debug.ui32RunDoneCnt = 0;
	gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_SendCmd = 0x40;
	gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_RunDone = 0xC0;

	if( gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32OpenedBitMask == (1 << ui8InstanceNum) )
	{
#ifdef USE_MCU_FOR_VDEC_VDC
		TOP_HAL_EnableInterIntr(MACH0+ui8CoreNum_Fewest);
#else
		TOP_HAL_EnableExtIntr(MACH0+ui8CoreNum_Fewest);
#endif

		VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] Enable Interrupt of Core:%d, %s", ui8MsvcCh, ui8CoreNum_Fewest, __FUNCTION__ );
	}

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] VdcCh:%d, CodecType:%d, OpenedMask:0x%X, RunningBitMask:0x%X, %s", ui8MsvcCh, ui8VdcCh,
								eCodecType, gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32OpenedBitMask, gsMsvcCore[ui8CoreNum_Fewest].sStatus.ui32RunningBitMask, __FUNCTION__ );

	return ui8MsvcCh;
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
static void _MSVC_ADP_Close(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask &= ~(1 << ui8InstanceNum);
	gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask &= ~(1 << ui8InstanceNum);

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] Used Instance:0x%X, Closing:0x%X, %s(%d)", ui8MsvcCh,
								gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask, gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask,
								__FUNCTION__, __LINE__ );

	gsMsvcAdp[ui8MsvcCh].Config.eCodecType = MSVC_CODEC_INVALID;
	gsMsvcAdp[ui8MsvcCh].Config.bNoReordering = FALSE;
	gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh = 0xFF;
	gsMsvcAdp[ui8MsvcCh].Config.bDual3D = FALSE;
	gsMsvcAdp[ui8MsvcCh].Config.ui32CPBAddr = 0;
	gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize = 0;
	gsMsvcAdp[ui8MsvcCh].Config.ui32Width = 0;
	gsMsvcAdp[ui8MsvcCh].Config.ui32Height = 0;

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed )
	{
		gui32MsvcMvColBufPool |= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed = 0x0;
	}

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask == 0x0 )
	{
#ifdef USE_MCU_FOR_VDEC_VDC
		TOP_HAL_ClearInterIntr(MACH0+ui8CoreNum);
		TOP_HAL_DisableInterIntr(MACH0+ui8CoreNum);
#else
		TOP_HAL_ClearExtIntr(MACH0+ui8CoreNum);
		TOP_HAL_DisableExtIntr(MACH0+ui8CoreNum);
#endif

		VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] Disable Interrupt of Core:%d, %s", ui8MsvcCh, ui8CoreNum, __FUNCTION__ );
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
void MSVC_ADP_Close(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] %s", ui8MsvcCh, __FUNCTION__ );

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask & (1 << ui8InstanceNum) )
	{
		gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask |= (1 << ui8InstanceNum);

		MSVC_HAL_SetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum, gfpMSVC_Get_CPBPhyWrAddr( gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh ));

		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn] Running, so Wait RunDone, %s", ui8MsvcCh, __FUNCTION__ );
	}
	else
	{
		_MSVC_ADP_Close(ui8MsvcCh);
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
static void _MSVC_ADP_Reset_Done(UINT8 ui8CoreNum)
{
	UINT8		ui8MsvcCh;

	if( ui8CoreNum >= MSVC_NUM_OF_CORE)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP_C%d][Err] Core Number, %s", ui8CoreNum, __FUNCTION__ );
		return;
	}

	if( gsMsvcCore[ui8CoreNum].sStatus.bResetting == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP_C%d][DBG] Already Processed, %s", ui8CoreNum, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP_C%d][DBG] %s", ui8CoreNum, __FUNCTION__ );

	TOP_HAL_ClearExtIntrMsk(1 << (MACH0_SRST_DONE+ui8CoreNum));
	TOP_HAL_DisableExtIntr(MACH0_SRST_DONE+ui8CoreNum);

	MSVC_HAL_SetRun(ui8CoreNum, 0);
	MSVC_BootCodeDown(ui8CoreNum);
	MSVC_HAL_SetRun(ui8CoreNum, 1);

	gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask = 0x0;
	gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask &= ~gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask;
	gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask = 0x0;

	for( ui8MsvcCh = (ui8CoreNum * MSVC_NUM_OF_CHANNEL); ui8MsvcCh < ((ui8CoreNum + 1) * MSVC_NUM_OF_CHANNEL); ui8MsvcCh++ )
	{
		MSVC_HAL_SetFrameDisplayFlag(ui8CoreNum, ui8MsvcCh % MSVC_NUM_OF_CHANNEL, 0);

		gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex = 0;
		gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex = 0;
		gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame = 0x0;
		gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame = 0x0;

		gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt = 0xFFFF;
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio = 1;
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32GSTC_Prev = 0xFFFFFFFF;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.eAuType = MSVC_AU_UNKNOWN;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.eAuType = MSVC_AU_UNKNOWN;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuAddr = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuSize = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuEnd = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS = VDEC_UNKNOWN_PTS;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS = VDEC_UNKNOWN_PTS;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui64TimeStamp = 0xFFFFFFFFFFFFFFFFLL;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID = 0xFFFFFFFF;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32InputGSTC = 0xFFFFFFFF;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FeedGSTC = 0xFFFFFFFF;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateRes_Parser = 0x0;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateDiv_Parser = 0xFFFFFFFF;

		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY = 0x0;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCb = 0;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCr = 0;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32Stride = 0;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt = 0;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt = 0;

		if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed )
		{
			gui32MsvcMvColBufPool |= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed;
			gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed = 0x0;
		}

		gsMsvcAdp[ui8MsvcCh].Status.bResetting = FALSE;
	}

	gsMsvcCore[ui8CoreNum].sStatus.bResetting = FALSE;
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
BOOLEAN MSVC_ADP_Reset(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] %s", ui8MsvcCh, __FUNCTION__ );

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d] BZ %d Instance %d Instruction %d RD %08X WR %08X",
			ui8MsvcCh,
			MSVC_HAL_IsBusy(ui8CoreNum),
			MSVC_HAL_GetCurInstance(ui8CoreNum),
			MSVC_HAL_GetCurCommand(ui8CoreNum),
			MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum),
			MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum) );

	if( gsMsvcCore[ui8CoreNum].sStatus.bResetting == TRUE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][DBG] Resetting, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	gsMsvcCore[ui8CoreNum].sStatus.bResetting = TRUE;

	TOP_HAL_EnableExtIntr(MACH0_SRST_DONE+ui8CoreNum);
	TOP_HAL_ResetMach(ui8CoreNum);

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
BOOLEAN MSVC_ADP_IsResetting(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;

	return gsMsvcCore[ui8CoreNum].sStatus.bResetting;
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
void MSVC_ADP_ResetDone(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;

	_MSVC_ADP_Reset_Done(ui8CoreNum);
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
BOOLEAN MSVC_ADP_Flush(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] Busy:%d RD:0x%X WR:0x%X, %s",
			ui8MsvcCh,
			MSVC_HAL_IsBusy(ui8CoreNum),
			MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum),
			MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum),
			__FUNCTION__ );

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt != 0 )
		gsMsvcAdp[ui8MsvcCh].Status.bFlushCmd = TRUE;

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
BOOLEAN MSVC_ADP_SetPicSkipMode(UINT32 ui8MsvcCh, E_MSVC_SKIP_TYPE_T ePicSkip)
{
	E_DEC_PARAM_SKIP_FRAME_MODE_T eSkipFrameMode;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] Pic SkipMode:%d, %s", ui8MsvcCh, ePicSkip, __FUNCTION__ );

	switch( ePicSkip )
	{
	case MSVC_SKIP_NO :
		eSkipFrameMode = DEC_PARAM_SKIP_MODE_NO;
		break;
	case MSVC_SKIP_PB :
		eSkipFrameMode = DEC_PARAM_SKIP_MODE_PB;
		break;
	case MSVC_SKIP_B :
		eSkipFrameMode = DEC_PARAM_SKIP_MODE_B;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Not Support Pic SkipMode:%d, %s", ui8MsvcCh, ePicSkip, __FUNCTION__ );
		return FALSE;
	}

	gsMsvcAdp[ui8MsvcCh].Config.eSkipFrameMode = eSkipFrameMode;

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
BOOLEAN MSVC_ADP_SetSrcSkipType(UINT32 ui8MsvcCh, E_MSVC_SKIP_TYPE_T eSrcSkip)
{
	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] Src SkipMode:%d, %s", ui8MsvcCh, eSrcSkip, __FUNCTION__ );

	switch( eSrcSkip )
	{
	case MSVC_SKIP_B :
		gsMsvcAdp[ui8MsvcCh].Config.bSrcSkipB = TRUE;
		break;
	case MSVC_SKIP_NO :
		gsMsvcAdp[ui8MsvcCh].Config.bSrcSkipB = FALSE;
		break;
	case MSVC_SKIP_PB :
	default :
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Not Support Src SkipMode:%d, %s", ui8MsvcCh, eSrcSkip, __FUNCTION__ );
		return FALSE;
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
BOOLEAN MSVC_ADP_SetUserDataEn(UINT32 ui8MsvcCh, BOOLEAN bUserDataEnable)
{
	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] UserDataEnable:%d, %s", ui8MsvcCh, bUserDataEnable, __FUNCTION__ );

	gsMsvcAdp[ui8MsvcCh].Config.bUserDataEnable = bUserDataEnable;

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
BOOLEAN MSVC_ADP_ReconfigDPB(UINT32 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] %s", ui8MsvcCh, __FUNCTION__ );

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed )
	{
		gui32MsvcMvColBufPool |= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed = 0x0;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask )
	{
		if( (gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask & (1 << ui8InstanceNum)) == gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask )
		{
			MSVC_ADP_Reset(ui8MsvcCh);
			gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask |= (1 << ui8InstanceNum);
		}
	}

	gsMsvcAdp[ui8MsvcCh].Status.bFlushCmd = FALSE;
	gsMsvcAdp[ui8MsvcCh].Status.bEosCmd = FALSE;
	gsMsvcAdp[ui8MsvcCh].Status.ui32EosFrmCnt = 0;
	gsMsvcAdp[ui8MsvcCh].Status.ui32PictureStructure = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Status.ui32PackedMode = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Status.bFieldDone = FALSE;

	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex = 0;
	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex = 0;
	gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame = 0x0;
	gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame = 0x0;

	gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt = 0xFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio = 1;
	gsMsvcAdp[ui8MsvcCh].Cmd.ui32GSTC_Prev = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.eAuType = MSVC_AU_UNKNOWN;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.eAuType = MSVC_AU_UNKNOWN;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuAddr = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuSize = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuEnd = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS = VDEC_UNKNOWN_PTS;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS = VDEC_UNKNOWN_PTS;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui64TimeStamp = 0xFFFFFFFFFFFFFFFFLL;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32InputGSTC = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FeedGSTC = 0xFFFFFFFF;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateRes_Parser = 0x0;
	gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateDiv_Parser = 0xFFFFFFFF;

	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY = 0x0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCb = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCr = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32Stride = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt = 0;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt = 0;

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
BOOLEAN MSVC_ADP_RegisterDPB(UINT32 ui8MsvcCh, S_VDEC_DPB_INFO_T *psVdecDpb, UINT32 ui32BufStride)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;
	UINT32		ui32BufIdx;
	UINT32		ui32MsvcMvColBuf = 0x0;
	UINT32		ui32BaseAddrMvCol;
	FRAME_BUF_INFO_T *pstFrameBufInfo;
	UINT32		ui32FrameBufDelay;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	// Alloc MV_Col Buffer
	for( ui32BufIdx = 0; ui32BufIdx < psVdecDpb->ui32NumOfFb; ui32BufIdx++ )
	{
		ui32MsvcMvColBuf >>= 1;
		ui32MsvcMvColBuf |= 0x80000000;
	}
	for( ui32BufIdx = 0; ui32BufIdx <= (32 - psVdecDpb->ui32NumOfFb); ui32BufIdx++ )
	{
		if( (gui32MsvcMvColBufPool & ui32MsvcMvColBuf) == ui32MsvcMvColBuf )
			break;

		ui32MsvcMvColBuf >>= 1;
	}
	if( ui32BufIdx > (32 - psVdecDpb->ui32NumOfFb) )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC_ADP%d][Err] Not Enough MV_Col Buffer, Pool:0x%08X, Alloc:0%08X(%d), %s", ui8MsvcCh,
									gui32MsvcMvColBufPool, ui32MsvcMvColBuf, psVdecDpb->ui32NumOfFb, __FUNCTION__);
		return FALSE;
	}

	gui32MsvcMvColBufPool &= ~ui32MsvcMvColBuf;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MsvcMvColBufUsed = ui32MsvcMvColBuf;
	ui32BaseAddrMvCol = VDEC_MSVC_MVCOL_BUF_BASE + (VDEC_MSVC_MVCOL_SIZE * ui32BufIdx);

	pstFrameBufInfo = &gsMsvcAdp[ui8MsvcCh].SeqInit.stFrameBufInfo;
	pstFrameBufInfo->u32FrameNum = psVdecDpb->ui32NumOfFb;

	for (ui32BufIdx=0; ui32BufIdx < psVdecDpb->ui32NumOfFb; ui32BufIdx++)
	{
		pstFrameBufInfo->astFrameBuf[ui32BufIdx].u32AddrY = psVdecDpb->aBuf[ui32BufIdx].ui32AddrY;
		pstFrameBufInfo->astFrameBuf[ui32BufIdx].u32AddrCb = psVdecDpb->aBuf[ui32BufIdx].ui32AddrCb;
		pstFrameBufInfo->astFrameBuf[ui32BufIdx].u32AddrCr = psVdecDpb->aBuf[ui32BufIdx].ui32AddrCr;
		pstFrameBufInfo->astFrameBuf[ui32BufIdx].u32MvColBuf = ui32BaseAddrMvCol + (VDEC_MSVC_MVCOL_SIZE * ui32BufIdx);
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	MSVC_HAL_RegisterFrameBuffer(ui8CoreNum, ui8InstanceNum, pstFrameBufInfo);

	ui32FrameBufDelay = gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameBufDelay;
	if( gsMsvcAdp[ui8MsvcCh].Config.bLowLatency == TRUE )
	{
//		if( ui32FrameBufDelay > 2 )
//			ui32FrameBufDelay -= 2;
		ui32FrameBufDelay = 0;
	}

	MSVC_HAL_SetFrameBuffer(ui8CoreNum, ui8InstanceNum, psVdecDpb->ui32NumOfFb, ui32FrameBufDelay, ui32BufStride);

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d] nFrm: %d, Stride: %d, Delay: %d, %s", ui8MsvcCh, psVdecDpb->ui32NumOfFb, ui32BufStride, ui32FrameBufDelay, __FUNCTION__);

	MSVC_HAL_SetFrameDisplayFlag(ui8CoreNum, ui8InstanceNum, 0);

	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY = psVdecDpb->ui32FrameSizeY;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCb = psVdecDpb->ui32FrameSizeCb;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCr = psVdecDpb->ui32FrameSizeCr;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32Stride = ui32BufStride;
	gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt = psVdecDpb->ui32NumOfFb;
	if( psVdecDpb->ui32NumOfFb >= MSVC_MAX_FRAME_BUFF_NUM )
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Frame Buf Num: %d, %s", ui8MsvcCh, psVdecDpb->ui32NumOfFb, __FUNCTION__ );

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
FRAME_BUF_INFO_T *MSVC_ADP_DBG_GetDPB(UINT32 ui8MsvcCh)
{
	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return NULL;
	}

	return &gsMsvcAdp[ui8MsvcCh].SeqInit.stFrameBufInfo;
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
static BOOLEAN _MSVC_ADP_IsWithinRange(UINT32 ui32StartAddr, UINT32 ui32EndAddr, UINT32 ui32TargetAddr)
{
	if( ui32StartAddr == ui32EndAddr)
	{
		if( ui32TargetAddr == ui32EndAddr )
			return TRUE;
		else
			return FALSE;
	}
	else if( ui32StartAddr <= ui32EndAddr )
	{
		if( (ui32TargetAddr > ui32StartAddr) &&
			(ui32TargetAddr <= ui32EndAddr) )
			return TRUE;
	}
	else
	{
		if( (ui32TargetAddr > ui32StartAddr) ||
			(ui32TargetAddr <= ui32EndAddr) )
			return TRUE;
	}

	return FALSE;
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
static BOOLEAN _MSVC_ADP_UpdateWrPtr(UINT8 ui8MsvcCh, UINT32 ui32AuAddr, UINT32 ui32AuSize, UINT32 ui32AuEnd)
{
	BOOLEAN		bPrefetch = TRUE;
	UINT32		ui32WrAddr_CPB;
	UINT32		ui32WrAddr_Prefetch;

	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( ui32AuEnd == 0x0 )
	{
		VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][Err] AuAddr:0x%08X+0x%X=0x%08X, %s", ui8MsvcCh,
									ui32AuAddr, ui32AuSize, ui32AuEnd, __FUNCTION__ );
		return FALSE;
	}

	ui32WrAddr_CPB = gfpMSVC_Get_CPBPhyWrAddr( gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh );

	ui32WrAddr_Prefetch = ui32AuEnd + MSVC_PREFETCH_SIZE;	// for Boda Prefetch
	if( ui32WrAddr_Prefetch >= (gsMsvcAdp[ui8MsvcCh].Config.ui32CPBAddr + gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize) )
		ui32WrAddr_Prefetch -= gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize;

	if( _MSVC_ADP_IsWithinRange( ui32AuEnd, ui32WrAddr_CPB, ui32WrAddr_Prefetch ) == FALSE )
	{
		VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn] Shortage CPB for Boda Prefetch - AU End:0x%08X, CPB Wr:0x%08X, %s", ui8MsvcCh,
									ui32AuEnd, ui32WrAddr_CPB, __FUNCTION__);

		ui32WrAddr_Prefetch = ui32AuEnd;
		bPrefetch = FALSE;
	}

	gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuAddr = ui32AuAddr;
	gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuSize = ui32AuSize;
	gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuEnd = ui32AuEnd;
	gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurWrPtr = MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum);
	gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum);

	MSVC_HAL_SetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum, ui32WrAddr_Prefetch);

	return bPrefetch;
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
static BOOLEAN _MSVC_ADP_SeqInit(UINT8 ui8MsvcCh, S_MSVC_CMD_T *psMsvcCmd)
{
	SEQ_PARAM_T		stSeqParam;

	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;


	stSeqParam.u32BitStreamBuffer = gsMsvcAdp[ui8MsvcCh].Config.ui32CPBAddr;
	stSeqParam.u32BitStreamBufferSize = gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize;
	stSeqParam.u32PicWidth = 0;
	stSeqParam.u32PicHeight = 0;

	switch( gsMsvcAdp[ui8MsvcCh].Config.eCodecType )
	{
	case MSVC_CODEC_H264_HP :
		stSeqParam.u32BitStreamFormat = AVC_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_H264_MVC :
		stSeqParam.u32BitStreamFormat = AVC_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 1;
		break;
	case MSVC_CODEC_VC1_RCV_V1 :
		stSeqParam.u32BitStreamFormat = VC1_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_VC1_RCV_V2 :
		stSeqParam.u32BitStreamFormat = VC1_DEC;
		stSeqParam.u32SeqAspClass = 1;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_VC1_AP :
		stSeqParam.u32BitStreamFormat = VC1_DEC;
		stSeqParam.u32SeqAspClass = 2;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_MPEG2_HP :
		stSeqParam.u32BitStreamFormat = MP2_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_H263 :
		stSeqParam.u32BitStreamFormat = MP4_DIVX3_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_XVID :
		stSeqParam.u32BitStreamFormat = MP4_DIVX3_DEC;
		stSeqParam.u32SeqAspClass = 2;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_DIVX3 :
		stSeqParam.u32BitStreamFormat = MP4_DIVX3_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 1;
		stSeqParam.u32PicWidth = gsMsvcAdp[ui8MsvcCh].Config.ui32Width;
		stSeqParam.u32PicHeight = gsMsvcAdp[ui8MsvcCh].Config.ui32Height;
		break;
	case MSVC_CODEC_DIVX4 :
		stSeqParam.u32BitStreamFormat = MP4_DIVX3_DEC;
		stSeqParam.u32SeqAspClass = 5;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_DIVX5 :
		stSeqParam.u32BitStreamFormat = MP4_DIVX3_DEC;
		stSeqParam.u32SeqAspClass = 1;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_RVX :
		stSeqParam.u32BitStreamFormat = RV_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	case MSVC_CODEC_AVS :
		stSeqParam.u32BitStreamFormat = AVS_DEC;
		stSeqParam.u32SeqAspClass = 0;
		stSeqParam.u32AuxStd = 0;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Not Support MSVC Codec Type:%d, %s", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].Config.eCodecType, __FUNCTION__ );
		return FALSE;
	}

	stSeqParam.sSeqOption.bDeblockEnable = FALSE;
	stSeqParam.sSeqOption.bReorderEnable = TRUE;
	if( gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode == TRUE )
	{
		stSeqParam.sSeqOption.bFilePlayEnable = FALSE;
		stSeqParam.sSeqOption.bDynamicBufEnable = FALSE;
	}
	else
	{
		stSeqParam.sSeqOption.bFilePlayEnable = TRUE;
		stSeqParam.sSeqOption.bDynamicBufEnable = TRUE;
	}

	_MSVC_ADP_UpdateWrPtr(ui8MsvcCh, psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd);
	MSVC_HAL_SeqInit(ui8CoreNum, ui8InstanceNum, &stSeqParam);

	VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] CodecType:%d, AuType:%d, AuAddr:0x%08X+0x%X=0x%08X, %s", ui8MsvcCh,
								gsMsvcAdp[ui8MsvcCh].Config.eCodecType, psMsvcCmd->sCurr.eAuType,
								psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd, __FUNCTION__ );

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
static BOOLEAN _MSVC_ADP_PicRun(UINT8 ui8MsvcCh, S_MSVC_CMD_T *psMsvcCmd)
{
	DEC_PARAM_T 	stDecParam;

	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == TRUE )
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] au type:%d, au addr:0x%X", ui8MsvcCh, psMsvcCmd->sCurr.eAuType, psMsvcCmd->sCurr.ui32AuAddr);

	if( psMsvcCmd->sCurr.eAuType == MSVC_AU_EOS )
	{
		if( gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == FALSE )
		{
			gsMsvcAdp[ui8MsvcCh].Status.bEosCmd = TRUE;

			_MSVC_ADP_UpdateWrPtr(ui8MsvcCh, psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd);

			MSVC_HAL_SetEoS(ui8CoreNum, ui8InstanceNum);
		}

		MSVC_HAL_PicRunPack(ui8CoreNum, ui8InstanceNum);
	}
	else
	{
		if( gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode == TRUE )
			stDecParam.u32ChunkSize = 0;
		else
			stDecParam.u32ChunkSize = psMsvcCmd->sCurr.ui32AuSize;
		stDecParam.bPackedMode = FALSE;

//		stDecParam.i32PicStartByteOffset = 0;

		stDecParam.sPicOption.bIframeSearchEn = FALSE;
		stDecParam.sPicOption.eSkipFrameMode = gsMsvcAdp[ui8MsvcCh].Config.eSkipFrameMode;
		stDecParam.sPicOption.bPicUserDataEn = gsMsvcAdp[ui8MsvcCh].Config.bUserDataEnable;
		stDecParam.sPicOption.bPicUserMode = FALSE;
		stDecParam.sPicOption.bSrcSkipB = gsMsvcAdp[ui8MsvcCh].Config.bSrcSkipB;
//		stDecParam.sPicOption.stUserDataParam = ;

		stDecParam.u32AuAddr = psMsvcCmd->sCurr.ui32AuAddr;
		stDecParam.bEOF = FALSE;

		_MSVC_ADP_UpdateWrPtr(ui8MsvcCh, psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd);

		if( gsMsvcAdp[ui8MsvcCh].Status.bFlushCmd == TRUE )
		{
			MSVC_HAL_Flush(ui8CoreNum, ui8InstanceNum, 1, psMsvcCmd->sCurr.ui32AuAddr);
			gsMsvcAdp[ui8MsvcCh].Status.bFlushCmd = FALSE;
		}

		MSVC_HAL_PicRun(ui8CoreNum, ui8InstanceNum, &stDecParam);
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
static UINT32 _MSVC_ADP_ClearUsedFrame(UINT8 ui8MsvcCh)
{
	UINT32	ui32DisplayMark, ui32ClearMark = 0;
	UINT32	ui32ClearFrameAddr;

	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	ui32DisplayMark = MSVC_HAL_GetFrameDisplayFlag(ui8CoreNum, ui8InstanceNum);
	while( (ui32ClearFrameAddr = gfpMSVC_GetUsedFrame(gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh)) != 0x0 )
	{
		UINT32	ui32ClearFrameIdx;

		if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt == 0 )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Not Initialised SeqHdr", ui8MsvcCh);
			break;
		}

		for( ui32ClearFrameIdx = 0; ui32ClearFrameIdx < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt; ui32ClearFrameIdx++ )
		{
			if( ui32ClearFrameAddr == gsMsvcAdp[ui8MsvcCh].SeqInit.stFrameBufInfo.astFrameBuf[ui32ClearFrameIdx].u32AddrY )
				break;
		}
		if( ui32ClearFrameIdx == gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Wrong Clear Frame Address: 0x%X, %s", ui8MsvcCh, ui32ClearFrameAddr, __FUNCTION__ );
			continue;
		}

//		VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] Clear Index:%d, Addr:0x%X", ui8MsvcCh, ui32ClearFrameIdx, ui32ClearFrameAddr);

		if( ui32ClearFrameIdx >= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Too Big ClearFrameIndex %u >= %u", ui8MsvcCh,
										ui32ClearFrameIdx, gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt );
		}
		else if( ui32DisplayMark & (1 << ui32ClearFrameIdx) )
		{
			ui32ClearMark |= (1 << ui32ClearFrameIdx);
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] DispMark:0x%X, ClearMark:0x%X, ClearFrameIndex:%d, AliveFrame:0x%X, BufferFrame:0x%X, %s", ui8MsvcCh,
										ui32DisplayMark, ui32ClearMark, ui32ClearFrameIdx,
										gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
										__FUNCTION__);
		}

		if( gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame & (1 << ui32ClearFrameIdx) )
			gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame &= ~(1 << ui32ClearFrameIdx);
		else
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] AliveFrame:0x%X, ClearFrameIndex:%d, %s", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, ui32ClearFrameIdx, __FUNCTION__ );
	}

	ui32DisplayMark &= (~ui32ClearMark);

	MSVC_HAL_SetFrameDisplayFlag(ui8CoreNum, ui8InstanceNum, ui32DisplayMark);

	return ui32ClearMark;
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
static void _MSVC_ADP_UpdateRunTime(UINT8 ui8CoreNum, BOOLEAN bRunning)
{
	UINT32		ui32ElapseTime;
	UINT32		ui32EventTime_Prev;
	UINT32		ui32EventTime_Curr;

	ui32EventTime_Curr = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;
	ui32EventTime_Prev = gsMsvcCore[ui8CoreNum].sDebug.ui32EventTime;
	if( ui32EventTime_Prev != 0x80000000 )
	{
		ui32ElapseTime = (ui32EventTime_Curr >= ui32EventTime_Prev) ? (ui32EventTime_Curr - ui32EventTime_Prev) : (ui32EventTime_Curr + 0x80000000 - ui32EventTime_Prev);

		if( bRunning == TRUE )
			gsMsvcCore[ui8CoreNum].sDebug.ui32RunTime += ui32ElapseTime;
		gsMsvcCore[ui8CoreNum].sDebug.ui32TotalTime += ui32ElapseTime;
	}
	gsMsvcCore[ui8CoreNum].sDebug.ui32EventTime = ui32EventTime_Curr;

	gsMsvcCore[ui8CoreNum].sDebug.ui32MeasureCnt++;
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
static BOOLEAN _MSVC_ADP_SendCmd(UINT8 ui8MsvcCh, S_MSVC_CMD_T *psMsvcCmd, UINT32 ui32GSTC)
{
	BOOLEAN		bSent = FALSE;
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt == 0 )
	{
		if( (psMsvcCmd->sCurr.eAuType == MSVC_AU_SEQUENCE_HEADER) || (psMsvcCmd->sCurr.eAuType == MSVC_AU_UNKNOWN) )
		{
			_MSVC_ADP_SeqInit(ui8MsvcCh, psMsvcCmd);
			bSent = TRUE;

			gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask |= (1 << ui8InstanceNum);
		}
		else
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn] SEQ Init:%d, AU Type:%d, Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh,
										gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt, psMsvcCmd->sCurr.eAuType, psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd, __FUNCTION__, __LINE__);
		}
	}
	else
	{
		UINT32		ui32CurRdPtr;
		UINT32		ui32CurWrPtr;

		ui32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum);
		ui32CurWrPtr = MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum);

		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt )
		{
			UINT32	ui32CmdIndex;

			for( ui32CmdIndex = 0; ui32CmdIndex < gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt; ui32CmdIndex++ )
			{
				VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] CmdIndex:%d,   Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC, ui32CmdIndex,
											ui32CurRdPtr, ui32CurWrPtr,
											gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuEnd,
											gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].ui32DTS, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].ui32PTS,
											__FUNCTION__, __LINE__);
			}
		}
		else
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] Num of Cmd:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC, gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
										ui32CurRdPtr, ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd,
										gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS,
										__FUNCTION__, __LINE__);
		}

		if( psMsvcCmd->sCurr.eAuType == MSVC_AU_UNKNOWN )
			psMsvcCmd->sCurr.eAuType = MSVC_AU_PICTURE_X;

		_MSVC_ADP_ClearUsedFrame(ui8MsvcCh);

		_MSVC_ADP_PicRun(ui8MsvcCh, psMsvcCmd);
		bSent = TRUE;

		gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask |= (1 << ui8InstanceNum);

		_MSVC_ADP_UpdateRunTime(ui8CoreNum, FALSE);
	}
#if 0
	if( bSent == TRUE )
	{
		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt )
		{
			gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 1];
			gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt = 0;

			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] Num of Cmd:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC, gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
										ui32CurRdPtr, ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32AuEnd,
										gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS,
										__FUNCTION__, __LINE__);
		}
	}
#endif

	return bSent;
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
static BOOLEAN _MSVC_ADP_CheckRemainingBuffer(UINT8 ui8MsvcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ)
{
	BOOLEAN			bNeedMore = TRUE;
	S_MSVC_CMD_T	*psMsvcCmd;

	UINT8			ui8CoreNum;
	UINT8			ui8InstanceNum;
	UINT32			ui32CurRdPtr;
	UINT32			ui32CurWrPtr;

	UINT32			ui32AuEnd_Curr;

	if( gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt == 0 )
		return TRUE;

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	ui32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum);
	ui32CurWrPtr = MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum);

	psMsvcCmd = &gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd;
	ui32AuEnd_Curr = psMsvcCmd->sCurr.ui32AuAddr + VES_CPB_CEILING_512BYTES(psMsvcCmd->sCurr.ui32AuSize);
	if( ui32AuEnd_Curr >= (gsMsvcAdp[ui8MsvcCh].Config.ui32CPBAddr + gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize) )
		ui32AuEnd_Curr -= gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize;

	if( (ui32CurRdPtr != ui32CurWrPtr) && (psMsvcCmd->sCurr.ui32AuAddr) )
	{
		if( _MSVC_ADP_IsWithinRange(psMsvcCmd->sCurr.ui32AuAddr, ui32CurWrPtr, ui32CurRdPtr) == FALSE )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Err:0x%08X] Send,                Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurRdPtr, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuEnd, __FUNCTION__, __LINE__);
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Err:0x%08X] Cmd Used Cnt:%d, Prev Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32UsedCnt_Prev,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurRdPtr, gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuEnd, __FUNCTION__, __LINE__);
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Err:0x%08X] Cmd Used Cnt:%d, Curr Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
										ui32CurRdPtr, ui32CurWrPtr,
										psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd, __FUNCTION__, __LINE__);

			if( gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode == TRUE )
			{
				if( ui32NumOfFreeDQ && (gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == FALSE) )
					_MSVC_ADP_SendCmd(ui8MsvcCh, psMsvcCmd, ui32GSTC);
				else
					VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] DQ Almost Overflow, %s(%d)", ui8MsvcCh, ui32GSTC, __FUNCTION__, __LINE__);

				bNeedMore = FALSE;
			}
		}
		else if( _MSVC_ADP_IsWithinRange(ui32CurRdPtr, ui32CurWrPtr, psMsvcCmd->sNext.ui32AuEnd) == TRUE )
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Rd:0x%08X, Wr:0x%08X, Next AU Addr:0x%08X+0x%X=0x%08X, AU Type:%d, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
										ui32CurRdPtr, ui32CurWrPtr, psMsvcCmd->sNext.ui32AuAddr, psMsvcCmd->sNext.ui32AuSize, psMsvcCmd->sNext.ui32AuEnd, psMsvcCmd->sNext.eAuType, __FUNCTION__, __LINE__);
		}
		else if( _MSVC_ADP_IsWithinRange(ui32CurRdPtr, ui32CurWrPtr, ui32AuEnd_Curr) == TRUE )
		{
			if( gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode == TRUE )
			{
				if( ui32NumOfFreeDQ && (gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == FALSE) )
				{
					UINT32		ui32AuSize;

					VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC,
												gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
												ui32CurRdPtr, ui32CurWrPtr,
												psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd,
												psMsvcCmd->ui32DTS, psMsvcCmd->ui32PTS,
												__FUNCTION__, __LINE__);

					ui32AuSize = ( ui32CurRdPtr >= psMsvcCmd->sCurr.ui32AuAddr ) ? ui32CurRdPtr - psMsvcCmd->sCurr.ui32AuAddr : ui32CurRdPtr + gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize - psMsvcCmd->sCurr.ui32AuAddr;;

					psMsvcCmd->sCurr.ui32AuAddr = ui32CurRdPtr;
					if( psMsvcCmd->sCurr.ui32AuSize < ui32AuSize )
					{
						VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC,
													gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
													ui32CurRdPtr, ui32CurWrPtr,
													psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd,
													psMsvcCmd->ui32DTS, psMsvcCmd->ui32PTS,
													__FUNCTION__, __LINE__);

						psMsvcCmd->sCurr.ui32AuSize = 0;
					}
					else
					{
						psMsvcCmd->sCurr.ui32AuSize -= ui32AuSize;
					}

					_MSVC_ADP_SendCmd(ui8MsvcCh, psMsvcCmd, ui32GSTC);
				}
				else
				{
					VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] DQ Almost Overflow, %s(%d)", ui8MsvcCh, ui32GSTC, __FUNCTION__, __LINE__);
				}

				bNeedMore = FALSE;
			}
		}
	}

	return bNeedMore;
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
UINT32 MSVC_ADP_IsBusy(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
//	UINT8		ui8InstanceNum;
	UINT32		ui32Busy;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return TRUE;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
//	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	ui32Busy = gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask;
	if( MSVC_HAL_IsBusy(ui8CoreNum) )
		ui32Busy |= 0x80000000;

	return ui32Busy;
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
BOOLEAN MSVC_ADP_IsReady(UINT8 ui8MsvcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;
	BOOLEAN		bReady = FALSE;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return TRUE;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask & (1 << ui8InstanceNum) )
	{
		bReady = FALSE;

		if( (gsMsvcAdp[ui8MsvcCh].Config.bRingBufferMode == TRUE) &&
			(MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum) == MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum)) )
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] Not Enough CPB(Empty), %s(%d)", ui8MsvcCh, ui32GSTC, __FUNCTION__, __LINE__);
			bReady = TRUE;
		}
	}
	else if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask )
	{
		BOOLEAN		bIsBusyVCore;

		bReady = FALSE;

		bIsBusyVCore = MSVC_HAL_IsBusy(ui8CoreNum);
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn:0x%x] Running Other Instance, RunningBitMask:0x%X, VCore Busy:%d, %s", ui8MsvcCh, ui32GSTC, gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask, bIsBusyVCore, __FUNCTION__ );

		if( (gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask & gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask) && (bIsBusyVCore == FALSE) )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn:0x%x] Force to Close the Running Other Instance, VCore Busy:%d, %s", ui8MsvcCh, ui32GSTC, bIsBusyVCore, __FUNCTION__ );
			gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask &= ~gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask;
			gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask &= ~gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask;
			gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask = 0x0;
			bReady = TRUE;
		}
	}
	else
	{
		bReady = _MSVC_ADP_CheckRemainingBuffer(ui8MsvcCh, ui32GSTC, ui32NumOfFreeDQ);
	}

	return bReady;
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
BOOLEAN MSVC_ADP_UpdateBuffer(	UINT8 ui8MsvcCh,
										S_MSVC_CMD_T *psMsvcCmd,
										BOOLEAN *pbNeedMore,
										UINT32 ui32GSTC,
										UINT32 ui32NumOfActiveAuQ,
										UINT32 ui32NumOfActiveDQ)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	UINT32		ui32CurRdPtr;
	UINT32		ui32CurWrPtr;

	BOOLEAN		bConsume = FALSE;

	UINT32		ui32VCoreShare = 0;

	BOOLEAN		bLogPrint = FALSE;


	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( !(gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask & (1 << ui8InstanceNum)) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Closed Channel, %s", ui8MsvcCh, __FUNCTION__ );
		return FALSE;
	}

	ui32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum);
	ui32CurWrPtr = MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum);

	if( (gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio >= 2) &&
		(gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt == 0) )
	{
		if( gsMsvcAdp[ui8MsvcCh].Status.bFieldDone == FALSE )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Prev Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32UsedCnt_Prev,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurRdPtr, gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuEnd, __FUNCTION__, __LINE__);
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Curr Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
										ui32CurRdPtr, ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd, __FUNCTION__, __LINE__);
		}

		*pbNeedMore = TRUE;
	}
	else
	{
		*pbNeedMore = FALSE;
	}

	VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] Num of Cmd:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC,
								gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
								ui32CurRdPtr, ui32CurWrPtr,
								psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd,
								psMsvcCmd->ui32DTS, psMsvcCmd->ui32PTS,
								__FUNCTION__, __LINE__);

	if( (gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt != 0) &&
		((psMsvcCmd->sCurr.eAuType == MSVC_AU_SEQUENCE_HEADER) || (psMsvcCmd->sCurr.eAuType == MSVC_AU_SEQUENCE_END)) )
	{
		*pbNeedMore = TRUE;
		VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Wrn:0x%08X] Num of Cmd:%d, Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, Skip SeqHdr, %s(%d)", ui8MsvcCh, ui32GSTC,
									gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
									ui32CurRdPtr, ui32CurWrPtr,
									psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd,
									__FUNCTION__, __LINE__);

		bConsume = TRUE;
	}
	else
	{
		gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt] = *psMsvcCmd;

		gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt++;
		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt == MSVC_MAX_SLICE_NUM )
			gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt = MSVC_MAX_SLICE_NUM - 1;
	}

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask & (1 << ui8InstanceNum) )
	{
		_MSVC_ADP_UpdateWrPtr(ui8MsvcCh, psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd);
		bConsume = TRUE;

		if( (gsMsvcAdp[ui8MsvcCh].Config.bDual3D == FALSE) && ((gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_SendCmd % 0x40) == 0) )
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] Num of Cmd:%d,  UpdWr Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
										ui32CurRdPtr, ui32CurWrPtr,
										psMsvcCmd->sCurr.ui32AuAddr, psMsvcCmd->sCurr.ui32AuSize, psMsvcCmd->sCurr.ui32AuEnd,
										__FUNCTION__, __LINE__);

		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32GSTC_Prev != ui32GSTC )
			_MSVC_ADP_UpdateRunTime(ui8CoreNum, FALSE);
	}
	else if( gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask == 0x0 )
	{
		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt == 0 )
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][Err] AU Type:%d, %s", ui8MsvcCh, psMsvcCmd->sCurr.eAuType, __FUNCTION__ );
		}
		else
		{
			_MSVC_ADP_SendCmd(ui8MsvcCh, psMsvcCmd, ui32GSTC);
			bConsume = TRUE;
		}
	}
	else
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Running Other Instance, RunningBitMask:0x%X, VCore Busy:%d, Rd:0x%08X, Wr:0x%08X, %s", ui8MsvcCh,
									gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask, MSVC_HAL_IsBusy(ui8CoreNum), ui32CurRdPtr, ui32CurWrPtr,__FUNCTION__ );
	}

	if( gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt == 0 )
		VDEC_KDRV_Message(_TRACE, "[MSVC_ADP%d][DBG] Start UpdateBuffer - Time Stamp: %llu, DTS: 0x%08X",
									ui8MsvcCh, psMsvcCmd->ui64TimeStamp, psMsvcCmd->ui32DTS);
	gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt++;

	if( gsMsvcCore[ui8CoreNum].sDebug.ui32TotalTime )
		ui32VCoreShare = gsMsvcCore[ui8CoreNum].sDebug.ui32RunTime * 100 / gsMsvcCore[ui8CoreNum].sDebug.ui32TotalTime;

//	if( ui32NumOfActiveDQ == 0 )
//		gsMsvcCore[ui8CoreNum].sDebug.bWantToSeePerformance = TRUE;

	if( gsMsvcCore[ui8CoreNum].sDebug.bWantToSeePerformance == TRUE )
		bLogPrint = TRUE;
	else if( ui32VCoreShare > 90 )
		bLogPrint = TRUE;

	if( gsMsvcCore[ui8CoreNum].sDebug.ui32MeasureCnt > MSVC_VCORE_SHARE_RANGE )
	{
		if( bLogPrint  == TRUE )
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][DBG] AliveFrame:0x%X, BufferFrame:0x%X, SendCmdCnt:%d, RunDoneCnt:%d, CodecType:%d, Utilization:%d%%, AuQ:%d, DQ:%d, %s(%d)",
										ui8MsvcCh,
										gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame,
										gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
										gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt,
										gsMsvcAdp[ui8MsvcCh].Debug.ui32RunDoneCnt,
										gsMsvcAdp[ui8MsvcCh].Config.eCodecType,
										ui32VCoreShare,
										ui32NumOfActiveAuQ, ui32NumOfActiveDQ,
										__FUNCTION__, __LINE__ );

		gsMsvcCore[ui8CoreNum].sDebug.ui32MeasureCnt = 0;
		gsMsvcCore[ui8CoreNum].sDebug.ui32RunTime = 0;
		gsMsvcCore[ui8CoreNum].sDebug.ui32TotalTime = 0;
		gsMsvcCore[ui8CoreNum].sDebug.bWantToSeePerformance = FALSE;
		gsMsvcCore[ui8CoreNum].sDebug.ui32VCoreShare = ui32VCoreShare;
	}

	gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_SendCmd++;
	if( (gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_SendCmd % 0x100) == 0 )
	{
		VDEC_KDRV_Message(MONITOR, "[MSVC_ADP%d][DBG] AliveFrame:0x%X, BufferFrame:0x%X, SendCmdCnt:%d, RunDoneCnt:%d, CodecType:%d, Utilization:%d%%, AuQ:%d, DQ:%d",
									ui8MsvcCh,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame,
									gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32RunDoneCnt,
									gsMsvcAdp[ui8MsvcCh].Config.eCodecType,
									gsMsvcCore[ui8CoreNum].sDebug.ui32VCoreShare,
									ui32NumOfActiveAuQ, ui32NumOfActiveDQ);
	}

	gsMsvcAdp[ui8MsvcCh].Cmd.ui32GSTC_Prev = ui32GSTC;

	return bConsume;
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
void MSVC_ADP_ISR_RunDone(UINT8 ui8MsvcCh, S_MSVC_DONE_INFO_T *pMsvcDoneInfo)
{
	UINT32		ui32GSTC;
	VDC_REPORT_INFO_T	msvc_doneinfo_param;
	S_MSVC_DEC_INFO_T	*pDecInfoPush = NULL, *pDecInfoPop = NULL;
	S_MSVC_DISP_INFO_T	*pDispInfoPush = NULL, *pDispInfoPop = NULL;
	SINT32		i32IndexFrameDecoded, i32IndexFrameDisplay;
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	UINT32		ui32CurRdPtr;
	UINT32		ui32CurWrPtr;

	UINT32		ui32CmdIndex;
	BOOLEAN		bUnmatchedMeta = FALSE;
	UINT32		ui32DisplayMark;


	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( !(gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask & (1 << ui8InstanceNum)) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Closed Channel, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui32GSTC = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;

	memset(&msvc_doneinfo_param, 0x0, sizeof(VDC_REPORT_INFO_T));
	msvc_doneinfo_param.eCmdHdr = VDC_CMD_HDR_NONE;

	_MSVC_ADP_UpdateRunTime(ui8CoreNum, TRUE);

	ui32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum);
	ui32CurWrPtr = MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum);

	if( gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurRdPtr == ui32CurRdPtr )
	{
		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Prev Prev                          AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
									gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32UsedCnt_Prev,
									gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev.sCurr.ui32AuEnd, __FUNCTION__, __LINE__);
		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Cmd Used Cnt:%d, Prev Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
									gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
									gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurRdPtr, gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurWrPtr,
									gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd, __FUNCTION__, __LINE__);

		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Send,                Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
									gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurRdPtr, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32CurWrPtr,
									gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Debug.Send.ui32AuEnd, __FUNCTION__, __LINE__);
		for( ui32CmdIndex = 0; ui32CmdIndex < gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt; ui32CmdIndex++ )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] CmdIndex:%d,     Curr Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC, ui32CmdIndex,
										ui32CurRdPtr, ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuEnd,
										__FUNCTION__, __LINE__);
		}

		bUnmatchedMeta = TRUE;
	}

	gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurWrPtr = ui32CurWrPtr;
	gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CurRdPtr = ui32CurRdPtr;

	if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt )
	{
		UINT32		ui32CmdDoneRatio;

		for( ui32CmdIndex = 0; ui32CmdIndex < gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt; ui32CmdIndex++ )
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] CmdIndex:%d,   Rd:0x%08X, Wr:0x%08X, AU Addr:0x%08X+0x%X=0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC, ui32CmdIndex,
										ui32CurRdPtr, ui32CurWrPtr,
										gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].sCurr.ui32AuEnd,
										gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].ui32DTS, gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[ui32CmdIndex].ui32PTS,
										__FUNCTION__, __LINE__);
		}

		gsMsvcAdp[ui8MsvcCh].Debug.Done.sSelectCmd_Prev = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[0];
		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt > 1 )
		{
			UINT32		ui32AuAddr, ui32AuEnd;

			if( gsMsvcAdp[ui8MsvcCh].Config.bDual3D == FALSE )
				gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 1];

			ui32AuAddr = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[0].sCurr.ui32AuAddr;
			ui32AuEnd = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 2].sCurr.ui32AuEnd;

			gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize = (ui32AuEnd >= ui32AuAddr) ? (ui32AuEnd - ui32AuAddr) : (ui32AuEnd + gsMsvcAdp[ui8MsvcCh].Config.ui32CPBSize - ui32AuAddr);
			gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize += gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 1].sCurr.ui32AuSize;
		}

		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd = gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 1].sCurr.ui32AuEnd;
		gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext= gsMsvcAdp[ui8MsvcCh].Cmd.sReceiveCmd[gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt - 1].sNext;

		ui32CmdDoneRatio = gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt / gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt;
		if( gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio != ui32CmdDoneRatio )
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn:0x%08X] Send : Done = %d : %d, %s(%d)", ui8MsvcCh, ui32GSTC,
										gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt, gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt, __FUNCTION__, __LINE__);
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio = ui32CmdDoneRatio;

		gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32UsedCnt_Prev = gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt;
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt = 0;

		gsMsvcAdp[ui8MsvcCh].Debug.Done.ui32CmdCnt_Prev = gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt;
		gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt = 0;
	}
	else
	{
		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Err:0x%08X] Num of Cmd:%d, Rd:0x%08X, Wr:0x%08X, DTS:0x%X, PTS:0x%X, %s(%d)", ui8MsvcCh, ui32GSTC, gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdCnt,
									ui32CurRdPtr, ui32CurWrPtr,
									gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS,
									__FUNCTION__, __LINE__);
		VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Err:0x%08X] Curr AU Addr:0x%08X+0x%X=0x%08X, Next AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
									gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd,
									gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sNext.ui32AuEnd,
									__FUNCTION__, __LINE__);
	}

	gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt++;

	VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d][DBG:0x%08X] Cmd Used Cnt:%d, Rd:0x%08X, Wr:0x%08X, Curr AU Addr:0x%08X+0x%X=0x%08X, %s(%d)", ui8MsvcCh, ui32GSTC,
								gsMsvcAdp[ui8MsvcCh].Cmd.ui32UsedCnt,
								ui32CurRdPtr, ui32CurWrPtr,
								gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuAddr, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuSize, gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.sCurr.ui32AuEnd,
								__FUNCTION__, __LINE__);

	gsMsvcAdp[ui8MsvcCh].Debug.ui32RunDoneCnt++;

	switch( pMsvcDoneInfo->eMsvcDoneHdr )
	{
	case MSVC_DONE_HDR_SEQINFO :
		// Report Noti
		msvc_doneinfo_param.eCmdHdr = VDC_CMD_HDR_SEQINFO;
		msvc_doneinfo_param.u.seqInfo.ui32uID = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID;
		msvc_doneinfo_param.u.seqInfo.ui32IsSuccess = pMsvcDoneInfo->u.seqInfo.u32IsSuccess;
		msvc_doneinfo_param.u.seqInfo.bOverSpec = pMsvcDoneInfo->u.seqInfo.bOverSpec;
		msvc_doneinfo_param.u.seqInfo.ui32PicWidth = pMsvcDoneInfo->u.seqInfo.u32PicWidth;
		msvc_doneinfo_param.u.seqInfo.ui32PicHeight = pMsvcDoneInfo->u.seqInfo.u32PicHeight;
		msvc_doneinfo_param.u.seqInfo.ui32AspectRatio = pMsvcDoneInfo->u.seqInfo.u32AspectRatio;
		msvc_doneinfo_param.u.seqInfo.ui32Profile = pMsvcDoneInfo->u.seqInfo.u32Profile;
		msvc_doneinfo_param.u.seqInfo.ui32Level = pMsvcDoneInfo->u.seqInfo.u32Level;

		msvc_doneinfo_param.u.seqInfo.ui32FrameRateRes = pMsvcDoneInfo->u.seqInfo.u32FrameRateRes;
		msvc_doneinfo_param.u.seqInfo.ui32FrameRateDiv = pMsvcDoneInfo->u.seqInfo.u32FrameRateDiv;

		msvc_doneinfo_param.u.seqInfo.ui32RefFrameCount = pMsvcDoneInfo->u.seqInfo.ui32RefFrameCount;
		msvc_doneinfo_param.u.seqInfo.ui32ProgSeq = pMsvcDoneInfo->u.seqInfo.u32ProgSeq;

		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameBufDelay = pMsvcDoneInfo->u.seqInfo.ui32FrameBufDelay;
		gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt = pMsvcDoneInfo->u.seqInfo.ui32RefFrameCount;

		gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask &= ~(1 << ui8InstanceNum);
		break;

	case MSVC_DONE_HDR_PICINFO :
		//i32IndexFrameDecoded
		// -1 (0xFFFF) : insufficient frame buffer
		// -2 (0xFFFE) : no decoded frame(because of  skip option or picture header error)
		i32IndexFrameDecoded = pMsvcDoneInfo->u.picInfo.i32IndexFrameDecoded;
		i32IndexFrameDisplay = pMsvcDoneInfo->u.picInfo.i32IndexFrameDisplay;

		if( gsMsvcAdp[ui8MsvcCh].Config.bNoReordering == TRUE )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][DBG] No Reordering - DecIdx: %d, DispIdx: %d, %s", ui8MsvcCh, i32IndexFrameDecoded, i32IndexFrameDisplay, __FUNCTION__ );

			if( i32IndexFrameDecoded >= 0 && i32IndexFrameDecoded < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
			{
				i32IndexFrameDisplay = i32IndexFrameDecoded;
			}
			else if( i32IndexFrameDisplay >= 0 && i32IndexFrameDisplay < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
			{
				i32IndexFrameDecoded = i32IndexFrameDisplay;
			}
			else
			{
				i32IndexFrameDecoded = 0;
				i32IndexFrameDisplay = 0;
			}
		}

		if( gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == TRUE )
		{
			if( i32IndexFrameDecoded >= 0 && i32IndexFrameDecoded < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
			{
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] EOS - DecodeFrameIndex: %d, BufferFrame: 0x%X, %s(%d)", ui8MsvcCh, i32IndexFrameDecoded, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame, __FUNCTION__, __LINE__ );
				i32IndexFrameDecoded = -9;
			}
		}

		ui32DisplayMark = MSVC_HAL_GetFrameDisplayFlag(ui8CoreNum, ui8InstanceNum);

		// 1. Push Info to Re/Ordering Q
		if( i32IndexFrameDecoded >= 0 && i32IndexFrameDecoded < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
		{
			if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame & (1 << i32IndexFrameDecoded) )
			{
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] DecodeFrameIndex: %d, BufferFrame: 0x%X, %s(%d)", ui8MsvcCh, i32IndexFrameDecoded, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame, __FUNCTION__, __LINE__ );
			}

			pDispInfoPush = &gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.DispInfo[i32IndexFrameDecoded];
			gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame |= (1 << i32IndexFrameDecoded);

			if( gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame & (1 << i32IndexFrameDecoded) )
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] DecodeFrameIndex: %d, AliveFrame:0x%X, %s(%d)", ui8MsvcCh, i32IndexFrameDecoded, gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, __FUNCTION__, __LINE__ );
			else
				gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame |= (1 << i32IndexFrameDecoded);

			pDecInfoPush = &gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.DecInfo[gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex];
			gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex++;
			if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex >= MSVC_MAX_FRAME_BUFF_NUM )
				gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex = 0;
			if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex == gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex )
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Decoding Buffer Overflow - BufferFrame: 0x%X, %s(%d)", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame, __FUNCTION__, __LINE__ );

			pDecInfoPush->ui32InputGSTC = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32InputGSTC;
			pDecInfoPush->ui32FeedGSTC = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FeedGSTC;
			pDecInfoPush->sFrameRate_Parser.ui32Residual = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateRes_Parser;
			pDecInfoPush->sFrameRate_Parser.ui32Divider = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FrameRateDiv_Parser;
			pDecInfoPush->ui32FlushAge = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FlushAge;

			pDecInfoPush->bUnmatchedMeta = bUnmatchedMeta;
			pDecInfoPush->ui32DTS = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32DTS;
			pDecInfoPush->ui64TimeStamp = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui64TimeStamp;
			pDecInfoPush->ui32uID = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID;
			pDispInfoPush->bUnmatchedMeta = bUnmatchedMeta;
			pDispInfoPush->ui32PTS = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32PTS;
			pDispInfoPush->ui64TimeStamp = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui64TimeStamp;
			pDispInfoPush->ui32uID = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32uID;

			pDispInfoPush->ui32FlushAge = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32FlushAge;
			pDispInfoPush->ui32DecodingSuccess = pMsvcDoneInfo->u.picInfo.u32DecodingSuccess;
			pDispInfoPush->ui32NumOfErrMBs = pMsvcDoneInfo->u.picInfo.u32NumOfErrMBs;
			pDispInfoPush->ui32PicType = pMsvcDoneInfo->u.picInfo.u32PicType;
			pDispInfoPush->sFrameRate_Decoder.ui32Residual = pMsvcDoneInfo->u.picInfo.ui32FrameRateRes;
			pDispInfoPush->sFrameRate_Decoder.ui32Divider = pMsvcDoneInfo->u.picInfo.ui32FrameRateDiv;
			pDispInfoPush->bVariableFrameRate = pMsvcDoneInfo->u.picInfo.bVariableFrameRate;
			pDispInfoPush->ui32AspectRatio = pMsvcDoneInfo->u.picInfo.u32Aspectratio;
			pDispInfoPush->ui32ActiveFMT= pMsvcDoneInfo->u.picInfo.u32ActiveFMT;
			pDispInfoPush->ui32ProgSeq= pMsvcDoneInfo->u.picInfo.u32ProgSeq;
			pDispInfoPush->ui32ProgFrame= pMsvcDoneInfo->u.picInfo.u32ProgFrame;
			pDispInfoPush->si32FrmPackArrSei= pMsvcDoneInfo->u.picInfo.si32FrmPackArrSei;
			pDispInfoPush->ui32LowDelay= pMsvcDoneInfo->u.picInfo.u32LowDelay;
			if( gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32Width == MSVC_WIDTH_INVALID )
				pDispInfoPush->ui32PicWidth = pMsvcDoneInfo->u.picInfo.u32PicWidth;
			else
				pDispInfoPush->ui32PicWidth = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32Width;
			if( gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32Height == MSVC_HEIGHT_INVALID )
				pDispInfoPush->ui32PicHeight = pMsvcDoneInfo->u.picInfo.u32PicHeight;
			else
				pDispInfoPush->ui32PicHeight = gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd.ui32Height;
			pDispInfoPush->ui32CropLeftOffset = pMsvcDoneInfo->u.picInfo.u32CropLeftOffset;
			pDispInfoPush->ui32CropRightOffset = pMsvcDoneInfo->u.picInfo.u32CropRightOffset;
			pDispInfoPush->ui32CropTopOffset = pMsvcDoneInfo->u.picInfo.u32CropTopOffset;
			pDispInfoPush->ui32CropBottomOffset = pMsvcDoneInfo->u.picInfo.u32CropBottomOffset;
			pDispInfoPush->eDisplayInfo = pMsvcDoneInfo->u.picInfo.ui32DisplayInfo;
			pDispInfoPush->ui32DisplayPeriod = pMsvcDoneInfo->u.picInfo.ui32DisplayPeriod;
			pDispInfoPush->i32FramePackArrange = pMsvcDoneInfo->u.picInfo.i32FramePackArrange;
			pDispInfoPush->eLR_Order = pMsvcDoneInfo->u.picInfo.ui32LR_Order;
			pDispInfoPush->ui16ParW = pMsvcDoneInfo->u.picInfo.ui16ParW;
			pDispInfoPush->ui16ParH = pMsvcDoneInfo->u.picInfo.ui16ParH;
			pDispInfoPush->ui32PictureStructure = pMsvcDoneInfo->u.picInfo.u32PictureStructure;

			pDispInfoPush->sUsrData.ui32Size = pMsvcDoneInfo->u.picInfo.sUsrData.ui32Size;
			if( pMsvcDoneInfo->u.picInfo.sUsrData.ui32Size )
			{
				pDispInfoPush->sUsrData.ui32PicType = pMsvcDoneInfo->u.picInfo.sUsrData.ui32PicType;
				pDispInfoPush->sUsrData.ui32Rpt_ff = pMsvcDoneInfo->u.picInfo.sUsrData.ui32Rpt_ff;
				pDispInfoPush->sUsrData.ui32Top_ff = pMsvcDoneInfo->u.picInfo.sUsrData.ui32Top_ff;
				pDispInfoPush->sUsrData.ui32BuffAddr =pMsvcDoneInfo->u.picInfo.sUsrData.ui32BuffAddr;
			}
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn] DecodeFrameIndex = %d, DisplayFrmaeIndex = %d, DisplayMark:0x%X, AliveFrame:0x%X, BufferFrame: 0x%X, %s(%d)", ui8MsvcCh,
										i32IndexFrameDecoded, i32IndexFrameDisplay,
										ui32DisplayMark, gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
										__FUNCTION__, __LINE__);
		}

		if( ui32DisplayMark != gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame )
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] DecodeFrameIndex = %d, DisplayFrmaeIndex = %d, DisplayMark:0x%X, AliveFrame:0x%X, BufferFrame:0x%X, %s(%d)", ui8MsvcCh,
										i32IndexFrameDecoded, i32IndexFrameDisplay,
										ui32DisplayMark, gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
										__FUNCTION__, __LINE__);

		// 2. Pop Info from Re/Ordering Q
		if( i32IndexFrameDisplay >= 0 && i32IndexFrameDisplay < gsMsvcAdp[ui8MsvcCh].SeqInit.ui32TotalFrmCnt )
		{
			if( !(gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame & (1 << i32IndexFrameDisplay)) )
			{
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] BufferFrame: 0x%X, DisplayFrmaeIndex = %d, %s(%d)", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame, i32IndexFrameDisplay, __FUNCTION__, __LINE__ );
			}
			else
			{
				UINT32	ui32DecOrderBufCnt = 0;
				UINT32	ui32RdIndex;
				UINT32	ui32DispOrderBufCnt = 0;
				UINT32	ui32Cnt;
//				static UINT32	ui32DTS_prev = 0;

				ui32RdIndex = gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex;
				while( ui32RdIndex != gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32WrIndex )
				{
					ui32DecOrderBufCnt++;

					ui32RdIndex++;
					if( ui32RdIndex >= MSVC_MAX_FRAME_BUFF_NUM )
						ui32RdIndex = 0;
				}

				for( ui32Cnt = 0; ui32Cnt < MSVC_MAX_FRAME_BUFF_NUM; ui32Cnt++ )
				{
					if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame & (1 << ui32Cnt) )
						ui32DispOrderBufCnt++;
				}

				if( ui32DecOrderBufCnt != ui32DispOrderBufCnt )
					VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Buffer Count Mismatching of between Decoding and Display Buffer - DecOrderBufCnt: %d, DispOrderBufCnt: %d, %s", ui8MsvcCh, ui32DecOrderBufCnt, ui32DispOrderBufCnt, __FUNCTION__ );

				while( ui32DecOrderBufCnt > ui32DispOrderBufCnt )
				{
					ui32DecOrderBufCnt--;

					gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex++;
					if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex >= MSVC_MAX_FRAME_BUFF_NUM )
						gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex = 0;
				}

				pDispInfoPop = &gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.DispInfo[i32IndexFrameDisplay];
				gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame &= ~(1 << i32IndexFrameDisplay);

				pDecInfoPop = &gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.DecInfo[gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex];
				if( ui32DecOrderBufCnt == ui32DispOrderBufCnt )
				{
					gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex++;
					if( gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex >= MSVC_MAX_FRAME_BUFF_NUM )
						gsMsvcAdp[ui8MsvcCh].PicDoneQ.DecOrder.ui32RdIndex = 0;
				}

//				if( ui32DTS_prev > pDecInfoPop->ui32DTS )
//				{
//					VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn] DTS Continuity - Jitter (Prev:0x%X, Curr:0x%X)", ui8MsvcCh, ui32DTS_prev, pDecInfoPop->ui32DTS );
//				}
//				ui32DTS_prev = pDecInfoPop->ui32DTS;
			}
		}
		else
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][Wrn] DecodeFrameIndex = %d, DisplayFrmaeIndex = %d, BufferFrame: 0x%X, %s(%d)", ui8MsvcCh,
										i32IndexFrameDecoded, i32IndexFrameDisplay, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame, __FUNCTION__, __LINE__);
		}

		if( gsMsvcAdp[ui8MsvcCh].Status.ui32PictureStructure != pMsvcDoneInfo->u.picInfo.u32PictureStructure )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][DBG] Picture Struct: %d --> %d, %s", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].Status.ui32PictureStructure, pMsvcDoneInfo->u.picInfo.u32PictureStructure, __FUNCTION__ );
			gsMsvcAdp[ui8MsvcCh].Status.ui32PictureStructure = pMsvcDoneInfo->u.picInfo.u32PictureStructure;
		}

		// 3. Check Overflow
		if( (i32IndexFrameDecoded == -1) && (i32IndexFrameDisplay == -3) )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Frame Index Full, DisplayMark:0x%X, AliveFrame:0x%X, BufferFrame: 0x%X, %s(%d)", ui8MsvcCh,
										ui32DisplayMark, gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame, gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
										__FUNCTION__, __LINE__);

			if( gsMsvcAdp[ui8MsvcCh].Status.bResetting == FALSE )
			{
				gsMsvcAdp[ui8MsvcCh].Status.bResetting = TRUE;

				msvc_doneinfo_param.eCmdHdr = VDC_CMD_HDR_RESET;
			}
		}
		// 4. Report Noti
		else if( pDispInfoPop != NULL )
		{
			UINT32	ui32FrameSize;
			UINT32	ui32FlushAge;

			msvc_doneinfo_param.eCmdHdr = VDC_CMD_HDR_PICINFO;

			ui32FrameSize = gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY + gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCb + gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeCr;

			msvc_doneinfo_param.u.picInfo.ui32Stride = gsMsvcAdp[ui8MsvcCh].SeqInit.ui32Stride;
			msvc_doneinfo_param.u.picInfo.ui32YFrameAddr = gsMsvcAdp[ui8MsvcCh].SeqInit.stFrameBufInfo.astFrameBuf[i32IndexFrameDisplay].u32AddrY;
			msvc_doneinfo_param.u.picInfo.ui32CFrameAddr = msvc_doneinfo_param.u.picInfo.ui32YFrameAddr + gsMsvcAdp[ui8MsvcCh].SeqInit.ui32FrameSizeY;
//			VDEC_KDRV_Message(DBG_SYS, "[MSVC_ADP%d][DBG] Index:%d, Addr:0x%X, Size:0x%X", ui8MsvcCh, i32IndexFrameDisplay, msvc_doneinfo_param.u.picInfo.ui32YFrameAddr, ui32FrameSize);

			if( pDecInfoPop != NULL )
			{
				msvc_doneinfo_param.u.picInfo.sFrameRate_Parser.ui32Residual = pDecInfoPop->sFrameRate_Parser.ui32Residual;
				msvc_doneinfo_param.u.picInfo.sFrameRate_Parser.ui32Divider = pDecInfoPop->sFrameRate_Parser.ui32Divider;
				msvc_doneinfo_param.u.picInfo.ui32InputGSTC = pDecInfoPop->ui32InputGSTC;
				msvc_doneinfo_param.u.picInfo.ui32FeedGSTC = pDecInfoPop->ui32FeedGSTC;

				if( pDecInfoPop->bUnmatchedMeta == TRUE )
					msvc_doneinfo_param.u.picInfo.sDTS.ui32DTS = VDEC_UNKNOWN_PTS;
				else
					msvc_doneinfo_param.u.picInfo.sDTS.ui32DTS = pDecInfoPop->ui32DTS;
				msvc_doneinfo_param.u.picInfo.sDTS.ui64TimeStamp = pDecInfoPop->ui64TimeStamp;
				msvc_doneinfo_param.u.picInfo.sDTS.ui32uID = pDecInfoPop->ui32uID;
			}
			else
			{
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] pDecInfoPop = NULL, %s", ui8MsvcCh, __FUNCTION__ );
				msvc_doneinfo_param.u.picInfo.sDTS.ui32DTS = VDEC_UNKNOWN_PTS;
				msvc_doneinfo_param.u.picInfo.sFrameRate_Parser.ui32Residual = 0x0;
				msvc_doneinfo_param.u.picInfo.sFrameRate_Parser.ui32Divider = 0xFFFFFFFF;
			}

			if( pDispInfoPop->bUnmatchedMeta == TRUE )
				msvc_doneinfo_param.u.picInfo.sPTS.ui32PTS = VDEC_UNKNOWN_PTS;
			else
				msvc_doneinfo_param.u.picInfo.sPTS.ui32PTS = pDispInfoPop->ui32PTS;
			msvc_doneinfo_param.u.picInfo.sPTS.ui64TimeStamp = pDispInfoPop->ui64TimeStamp;
			msvc_doneinfo_param.u.picInfo.sPTS.ui32uID = pDispInfoPop->ui32uID;

			msvc_doneinfo_param.u.picInfo.sFrameRate_Decoder.ui32Residual = pDispInfoPop->sFrameRate_Decoder.ui32Residual;
			msvc_doneinfo_param.u.picInfo.sFrameRate_Decoder.ui32Divider = pDispInfoPop->sFrameRate_Decoder.ui32Divider;

			msvc_doneinfo_param.u.picInfo.ui32IsSuccess 			= pDispInfoPop->ui32DecodingSuccess;
			msvc_doneinfo_param.u.picInfo.ui32PicWidth 				= pDispInfoPop->ui32PicWidth;
			msvc_doneinfo_param.u.picInfo.ui32PicHeight 			= pDispInfoPop->ui32PicHeight;
			msvc_doneinfo_param.u.picInfo.ui32AspectRatio 			= pDispInfoPop->ui32AspectRatio;

			msvc_doneinfo_param.u.picInfo.ui32CropLeftOffset 		= pDispInfoPop->ui32CropLeftOffset;
			msvc_doneinfo_param.u.picInfo.ui32CropRightOffset 		= pDispInfoPop->ui32CropRightOffset;
			msvc_doneinfo_param.u.picInfo.ui32CropTopOffset 		= pDispInfoPop->ui32CropTopOffset;
			msvc_doneinfo_param.u.picInfo.ui32CropBottomOffset 		= pDispInfoPop->ui32CropBottomOffset;
			msvc_doneinfo_param.u.picInfo.ui32PictureStructure 		= pDispInfoPop->ui32PictureStructure;
			msvc_doneinfo_param.u.picInfo.bVariableFrameRate 		= pDispInfoPop->bVariableFrameRate;
			msvc_doneinfo_param.u.picInfo.eDisplayInfo 				= pDispInfoPop->eDisplayInfo;
			msvc_doneinfo_param.u.picInfo.ui32DisplayPeriod 		= pDispInfoPop->ui32DisplayPeriod;
			msvc_doneinfo_param.u.picInfo.si32FrmPackArrSei 		= pDispInfoPop->i32FramePackArrange;
			msvc_doneinfo_param.u.picInfo.eLR_Order 				= pDispInfoPop->eLR_Order;
			msvc_doneinfo_param.u.picInfo.ui16ParW 					= pDispInfoPop->ui16ParW;
			msvc_doneinfo_param.u.picInfo.ui16ParH 					= pDispInfoPop->ui16ParH;

			if( pDispInfoPop->bUnmatchedMeta == FALSE )
			{
				ui32FlushAge = pDispInfoPop->ui32FlushAge;
			}
			else if( pDecInfoPop->bUnmatchedMeta == FALSE )
			{
				ui32FlushAge = pDecInfoPop->ui32FlushAge;
			}
			else
			{
				ui32FlushAge = pDispInfoPop->ui32FlushAge;
				VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn] UnMatched FlushAge:%d, %s", ui8MsvcCh, ui32FlushAge, __FUNCTION__ );
			}

			msvc_doneinfo_param.u.picInfo.ui32FlushAge 				= ui32FlushAge;
			msvc_doneinfo_param.u.picInfo.ui32PicType 				= pDispInfoPop->ui32PicType;
			msvc_doneinfo_param.u.picInfo.ui32NumOfErrMBs 			= pDispInfoPop->ui32NumOfErrMBs;

			msvc_doneinfo_param.u.picInfo.ui32ActiveFMT 			= pDispInfoPop->ui32ActiveFMT;
			msvc_doneinfo_param.u.picInfo.ui32ProgSeq 				= pDispInfoPop->ui32ProgSeq;
			msvc_doneinfo_param.u.picInfo.ui32ProgFrame 			= pDispInfoPop->ui32ProgFrame;
			msvc_doneinfo_param.u.picInfo.si32FrmPackArrSei 		= pDispInfoPop->si32FrmPackArrSei;
			msvc_doneinfo_param.u.picInfo.ui32LowDelay 				= pDispInfoPop->ui32LowDelay;

			msvc_doneinfo_param.u.picInfo.sUsrData.ui32Size 		= pDispInfoPop->sUsrData.ui32Size;
			if( pDispInfoPop->sUsrData.ui32Size )
			{
				msvc_doneinfo_param.u.picInfo.sUsrData.ui32PicType = pDispInfoPop->sUsrData.ui32PicType;
				msvc_doneinfo_param.u.picInfo.sUsrData.ui32Rpt_ff = pDispInfoPop->sUsrData.ui32Rpt_ff;
				msvc_doneinfo_param.u.picInfo.sUsrData.ui32Top_ff = pDispInfoPop->sUsrData.ui32Top_ff;
				msvc_doneinfo_param.u.picInfo.sUsrData.ui32BuffAddr = pDispInfoPop->sUsrData.ui32BuffAddr;
			}
		}

		if( !(gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask & (1 << ui8InstanceNum)) )
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][DBG] No Running State, %s", ui8MsvcCh, __FUNCTION__ );

		if( pMsvcDoneInfo->u.picInfo.u32bPackedMode == TRUE )
		{
			VDEC_KDRV_Message(WARNING, "[MSVC_ADP%d][DBG] Packed Mode, %s", ui8MsvcCh, __FUNCTION__ );
			gsMsvcAdp[ui8MsvcCh].Status.ui32PackedMode = TRUE;
		}
		else
		{
			gsMsvcCore[ui8CoreNum].sStatus.ui32RunningBitMask &= ~(1 << ui8InstanceNum);
		}

		gsMsvcCore[ui8CoreNum].sDebug.ui32PicRunDoneTime = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;
		break;

	default :
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] eCmdHdr:%d , %s", ui8MsvcCh, pMsvcDoneInfo->eMsvcDoneHdr, __FUNCTION__ );

		msvc_doneinfo_param.eCmdHdr = VDC_CMD_HDR_NONE;
		break;
	}

	gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_RunDone++;
	if( gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_RunDone == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[MSVC_ADP%d][DBG] AliveFrame:0x%X, BufferFrame:0x%X, SendCmdCnt:%d, RunDoneCnt:%d, CmdDoneRatio:%d, %s",
									ui8MsvcCh,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32AliveFrame,
									gsMsvcAdp[ui8MsvcCh].PicDoneQ.DispOrder.ui32BufferFrame,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32SendCmdCnt,
									gsMsvcAdp[ui8MsvcCh].Debug.ui32RunDoneCnt,
									gsMsvcAdp[ui8MsvcCh].Cmd.ui32CmdDoneRatio,
									__FUNCTION__ );

		gsMsvcAdp[ui8MsvcCh].Debug.ui32LogTick_RunDone = 0;
	}

	if( gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask & (1 << ui8InstanceNum) )
	{
		gsMsvcCore[ui8CoreNum].sStatus.ui32ClosingBitMask &= ~(1 << ui8InstanceNum);
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn] Running Complete, %s", ui8MsvcCh, __FUNCTION__ );

		_MSVC_ADP_Close(ui8MsvcCh);
	}
	else
	{
		gfpMSVC_ReportPicInfo(gsMsvcAdp[ui8MsvcCh].Config.ui8VdcCh, &msvc_doneinfo_param);

		if( gsMsvcAdp[ui8MsvcCh].Status.bEosCmd == TRUE )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Wrn] EosFrmCnt/MinFrmCn: %u/%u, %s", ui8MsvcCh, gsMsvcAdp[ui8MsvcCh].Status.ui32EosFrmCnt, gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt, __FUNCTION__ );

			gsMsvcAdp[ui8MsvcCh].Status.ui32EosFrmCnt++;
			if( (gsMsvcAdp[ui8MsvcCh].Status.ui32EosFrmCnt <= gsMsvcAdp[ui8MsvcCh].SeqInit.ui32MinFrmCnt) &&
				(pDispInfoPop != NULL) )
			{
				_MSVC_ADP_PicRun(ui8MsvcCh, &gsMsvcAdp[ui8MsvcCh].Cmd.sSelectCmd);
			}
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
void MSVC_ADP_ISR_FieldDone(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( !(gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask & (1 << ui8InstanceNum)) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Closed Channel, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d] Field Done - Rd:0x%08X, Wr:0x%08X", ui8MsvcCh, MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum), MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum));

	gsMsvcAdp[ui8MsvcCh].Status.bFieldDone = TRUE;

	MSVC_HAL_SetFieldPicFlag(ui8CoreNum, ui8InstanceNum, TRUE);
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
void MSVC_ADP_ISR_Empty(UINT8 ui8MsvcCh)
{
	UINT8		ui8CoreNum;
	UINT8		ui8InstanceNum;

	if( ui8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Channel Number, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	ui8CoreNum = ui8MsvcCh / MSVC_NUM_OF_CHANNEL;
	ui8InstanceNum = ui8MsvcCh % MSVC_NUM_OF_CHANNEL;

	if( !(gsMsvcCore[ui8CoreNum].sStatus.ui32OpenedBitMask & (1 << ui8InstanceNum)) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP%d][Err] Closed Channel, %s", ui8MsvcCh, __FUNCTION__ );
		return;
	}

	_MSVC_ADP_UpdateRunTime(ui8CoreNum, TRUE);

	VDEC_KDRV_Message(DBG_VDC, "[MSVC_ADP%d] Empty - Rd:0x%08X, Wr:0x%08X", ui8MsvcCh, MSVC_HAL_GetBitstreamRdPtr(ui8CoreNum, ui8InstanceNum), MSVC_HAL_GetBitstreamWrPtr(ui8CoreNum, ui8InstanceNum));
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
void MSVC_ADP_ISR_ResetDone(UINT8 ui8CoreNum)
{
	if( ui8CoreNum >= MSVC_NUM_OF_CORE)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_ADP_C%d][Err] Channel Number, %s", ui8CoreNum, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(DBG_SYS, "Intr MACH%d_SRST_DONE", ui8CoreNum);

	_MSVC_ADP_Reset_Done(ui8CoreNum);
}


