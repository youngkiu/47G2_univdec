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
 * date       2011.05.10
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

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#include <linux/spinlock.h>
#endif

#include "../mcu/os_adap.h"

#include "hal/lg1152/top_hal_api.h"
#include "hal/lg1152/pdec_hal_api.h"
#include "hal/lg1152/ipc_reg_api.h"
#include "hal/lg1152/lq_hal_api.h"
#include "msvc_adp.h"
#include "msvc_drv.h"
#include "vp8_drv.h"
#include "../mcu/vdec_shm.h"

#include "vdc_drv.h"
#include "vdc_feed.h"
#include "vdc_auq.h"
#include "vp8_drv.h"

#include "vdec_stc_timer.h"

#include "../mcu/vdec_print.h"

#include "../mcu/ram_log.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define	VDC_NUM_OF_CMD_Q				0x40
#define	VDEC_MAX_SEQHDR_SIZE			2048
#define	VDC_PB_SKIP_THRESHOLD_BUFF		256
#define	VDC_PB_SKIP_THRESHOLD_LIVE		32

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
typedef enum
{
	VDC_CLOSED = 0,
	VDC_OPENED,
	VDC_INVALID = 0x20120609
} VDC_STATE_T;

typedef enum
{
	VDC_CMD_NONE = 0,
	VDC_CMD_PLAY,
	VDC_CMD_STEP,
	VDC_CMD_PAUSE,
	VDC_CMD_CLOSE,
	VDC_CMD_RESET,
	VDC_CMD_FLUSH,
	VDC_CMD_UINT32 = 0x20110915
} E_VDC_CMD_T;

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT8 ui8VDCCh;
	E_VDC_CMD_T eCmd;
} VDC_CMD_T;

typedef struct
{
	VDC_STATE_T state;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	struct
	{
		UINT8		ui8Vch;
		UINT32		ui32CpbBufAddr;
		UINT32		ui32CpbBufSize;

		UINT32		ui32ErrorMBFilter;

		BOOLEAN		bNoReordering;
		BOOLEAN		bSrcBufMode;
		UINT32		ui32FlushAge;
	} Config;

	struct
	{
		UINT32		ui32MinFrmCnt;
		UINT32		ui32TotalFrmCnt;
		UINT32		ui32DisplayingPicCnt;

		BOOLEAN 	bSeqInit;
		UINT32 		ui32SeqFailCnt;
		BOOLEAN 	bOverSpec;
		UINT32		ui32FlushAge;
		UINT32		ui32NoErrorMbCnt;

		BOOLEAN		bEosCmd;

		struct
		{
			UINT32		ui32AuCpbAddr;
			UINT8 		*pui8VirCpbPtr;
			UINT8		ui8aData[VDEC_MAX_SEQHDR_SIZE];
			UINT32		ui32Size;
		} sSeqHdr;

		struct
		{
			UINT32		ui32WrIndex;
			UINT32		ui32RdIndex;
			UINT32		aui32ClearFrameAddr[VDEC_MAX_DPB_NUM];
		} sUsedFramdAddrQ;

	} Status;

} VDEC_VDC_DB_T;

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
static volatile VDEC_VDC_DB_T *gsVDCdb = NULL;
static UINT32 ui32VdcResetCnt[VDC_CODEC_MAX] = {0, };
static spinlock_t	tVdcLock;
static void (*_gfpVDC_FeedInfo)(S_VDC_CALLBACK_BODY_ESINFO_T *pFeedInfo);
static void (*_gfpVDC_DecInfo)(S_VDC_CALLBACK_BODY_DECINFO_T *pDecInfo);
static void (*_gfpVDC_RequestReset)(S_VDC_CALLBACK_BODY_REQUESTRESET_T *pRequestReset);


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
static BOOLEAN _VDC_DEC_PutUsedFrame(UINT8 ui8VdcCh, UINT32 ui32ClearFrameAddr)
{
	UINT32			ui32WrIndex_Next;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	ui32WrIndex_Next = gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex + 1;
	if( ui32WrIndex_Next >= VDEC_MAX_DPB_NUM )
		ui32WrIndex_Next = 0;

	if( ui32WrIndex_Next == gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Used Frame Address Queue Overflow(%d), %s", ui8VdcCh, gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex, __FUNCTION__ );
		return FALSE;
	}

	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.aui32ClearFrameAddr[gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex] = ui32ClearFrameAddr;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = ui32WrIndex_Next;

//	VDEC_KDRV_Message(DBG_SYS, "[VDC%d][DBG] DisplayingPicCnt:%d, Addr:0x%X, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt, ui32ClearFrameAddr, __FUNCTION__ );

	if( gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt )
		gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt--;
	else
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] No Displaying Picture, Clear Frame Address: 0x%X, %s", ui8VdcCh, ui32ClearFrameAddr, __FUNCTION__ );

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
static UINT32 _VDC_DEC_GetUsedFrame(UINT8 ui8VdcCh)
{
	UINT32			ui32ClearFrameAddr = 0x0;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return ui32ClearFrameAddr;
	}

	if( gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex != gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex )
	{
		ui32ClearFrameAddr = gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.aui32ClearFrameAddr[gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex];
		gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex++;
		if( gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex >= VDEC_MAX_DPB_NUM )
			gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;
	}

	return ui32ClearFrameAddr;
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
static BOOLEAN _VDC_FEED_GetDQStatus(UINT8 ui8VdcCh, UINT32 *pui32NumOfActiveDQ, UINT32 *pui32NumOfFreeDQ)
{
	UINT32			ui32MaxNumOfFreeDQ;

	*pui32NumOfActiveDQ = 0;
	*pui32NumOfFreeDQ = 0;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if( gsVDCdb[ui8VdcCh].Status.bSeqInit == FALSE )
	{
		if( (gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt > 0) && (gsVDCdb[ui8VdcCh].Status.bOverSpec == TRUE) )
			*pui32NumOfFreeDQ = 0;
		else
			*pui32NumOfFreeDQ = 1;
	}
	else if( gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt )
	{
		UINT32		ui32DqMargin;

		*pui32NumOfActiveDQ = gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt;

		if( gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt < gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt )
		{
			ui32MaxNumOfFreeDQ = gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt;
			ui32DqMargin = 0;
		}
		else
		{
			ui32MaxNumOfFreeDQ = gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt - gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt;
			ui32DqMargin = (gsVDCdb[ui8VdcCh].eVcodec == VDC_CODEC_H264_MVC) ? 2 : 1;
		}

		if( (gsVDCdb[ui8VdcCh].eVcodec == VDC_CODEC_VP8) && (ui32MaxNumOfFreeDQ >= 2) )
			ui32MaxNumOfFreeDQ /= 2;
		else if( ui32MaxNumOfFreeDQ > ui32DqMargin )
			ui32MaxNumOfFreeDQ -= ui32DqMargin;

		if( *pui32NumOfActiveDQ > ui32MaxNumOfFreeDQ )
		{
			if( gsVDCdb[ui8VdcCh].Status.bEosCmd == FALSE )
				VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] DQ Overflow - Frame Buffer Count: %d/%d, TotalFrmCnt: %d, %s(%d)", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh,
										*pui32NumOfActiveDQ, ui32MaxNumOfFreeDQ, gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt, __FUNCTION__, __LINE__ );

			*pui32NumOfFreeDQ = 0;
			return FALSE;
		}

		*pui32NumOfFreeDQ = ui32MaxNumOfFreeDQ - *pui32NumOfActiveDQ;
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
static BOOLEAN _VDC_FEED_IsReady(UINT8 ui8VdcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ)
{
	BOOLEAN			bReady= FALSE;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if( gsVDCdb[ui8VdcCh].state == VDC_CLOSED )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	switch( gsVDCdb[ui8VdcCh].eVcodec )
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bReady = MSVC_ADP_IsReady(gsVDCdb[ui8VdcCh].ui8DecCh, ui32GSTC, ui32NumOfFreeDQ);
		break;
	case VDC_CODEC_VP8:
		bReady = VP8_IsReady(gsVDCdb[ui8VdcCh].ui8DecCh);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, gsVDCdb[ui8VdcCh].eVcodec, __FUNCTION__ );
		break;
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
static BOOLEAN _VDC_FEED_RequestReset(UINT8 ui8VdcCh)
{
	S_VDC_CALLBACK_BODY_REQUESTRESET_T sResetCmd;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(ERROR, "[VDC%d-%d][DBG] Busy:0x%x, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, MSVC_ADP_IsBusy(gsVDCdb[ui8VdcCh].ui8DecCh), __FUNCTION__ );

	sResetCmd.ui8Vch = gsVDCdb[ui8VdcCh].Config.ui8Vch;
	sResetCmd.bReset = TRUE;
	sResetCmd.ui32Addr = 0x0;
	sResetCmd.ui32Size = 0;

	if( gsVDCdb[ui8VdcCh].Config.bSrcBufMode == TRUE )
	{
		if( gsVDCdb[ui8VdcCh].Status.bSeqInit == TRUE )
		{
			sResetCmd.ui32Addr = (UINT32)gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui8aData;
			sResetCmd.ui32Size = gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size;
		}
		else if( gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size != 0 )
		{
			sResetCmd.ui32Size = (gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size > VDEC_MAX_SEQHDR_SIZE) ? VDEC_MAX_SEQHDR_SIZE : gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size;
			memcpy(&gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui8aData, gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr, sResetCmd.ui32Size);
			sResetCmd.ui32Addr = (UINT32)gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui8aData;
		}
	}

	_gfpVDC_RequestReset(&sResetCmd);

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
static BOOLEAN _VDC_FEED_SendCmd(UINT8 ui8VdcCh, S_VDC_AU_T *pVdcAu, S_VDC_AU_T *pVdcAu_Next, BOOLEAN *pbNeedMore, UINT32 ui32GSTC, UINT32 ui32NumOfActiveAuQ, UINT32 ui32NumOfActiveDQ)
{
	BOOLEAN		bRet = FALSE;
	S_VDC_CALLBACK_BODY_ESINFO_T		sVdcFeedNoti;

	*pbNeedMore = FALSE;

	if( gsVDCdb[ui8VdcCh].Status.bSeqInit == FALSE )
	{
		if( gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr == 0x0 )
		{
			gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr = pVdcAu->ui32AuStartAddr;
			gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr = pVdcAu->pui8VirStartPtr;
		}

		gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size += pVdcAu->ui32AuSize;
	}

	if( pVdcAu->ui32FlushAge != gsVDCdb[ui8VdcCh].Config.ui32FlushAge )
		VDEC_KDRV_Message(ERROR, "[VDC%d][Wrn] AU Flush Age:%d, Config Flush Age:%d, %s", ui8VdcCh, pVdcAu->ui32FlushAge, gsVDCdb[ui8VdcCh].Config.ui32FlushAge, __FUNCTION__ );

	switch(gsVDCdb[ui8VdcCh].eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		{
		S_MSVC_CMD_T sMsvcCmd;

		switch( pVdcAu->eAuType )
		{
		case VES_AU_SEQUENCE_HEADER :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_SEQUENCE_HEADER;
			break;
		case VES_AU_SEQUENCE_END :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_SEQUENCE_END;
			break;
		case VES_AU_PICTURE_I :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_PICTURE_I;
			break;
		case VES_AU_PICTURE_P :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_PICTURE_P;
			break;
		case VES_AU_PICTURE_B_NOSKIP :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_PICTURE_B_NOSKIP;
			break;
		case VES_AU_PICTURE_B_SKIP :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_PICTURE_B_SKIP;
			break;
		case VES_AU_UNKNOWN :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_UNKNOWN;
			break;
		case VES_AU_PICTURE_X :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_PICTURE_X;
			break;
		case VES_AU_EOS :
			sMsvcCmd.sCurr.eAuType = MSVC_AU_EOS;
			VDEC_KDRV_Message(DBG_SYS, "[VDC%d][DBG] End Of Stream, UID:%d, %s", ui8VdcCh, pVdcAu->ui32uID, __FUNCTION__ );
			gsVDCdb[ui8VdcCh].Status.bEosCmd = TRUE;
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VDC%d][Err] AU Type: %d, %s", ui8VdcCh, pVdcAu->eAuType, __FUNCTION__ );
			return FALSE;
		}
		sMsvcCmd.sCurr.ui32AuAddr = pVdcAu->ui32AuStartAddr;
		sMsvcCmd.sCurr.ui32AuSize = pVdcAu->ui32AuSize;
		sMsvcCmd.sCurr.ui32AuEnd = pVdcAu->ui32AuEndAddr;
		if( pVdcAu_Next == NULL )
		{
			sMsvcCmd.sNext.eAuType = MSVC_AU_UNKNOWN;
			sMsvcCmd.sNext.ui32AuAddr = 0x0;
			sMsvcCmd.sNext.ui32AuSize = 0x0;
			sMsvcCmd.sNext.ui32AuEnd = 0x0;
		}
		else
		{
			switch( pVdcAu_Next->eAuType )
			{
			case VES_AU_SEQUENCE_HEADER :
				sMsvcCmd.sNext.eAuType = MSVC_AU_SEQUENCE_HEADER;
				break;
			case VES_AU_SEQUENCE_END :
				sMsvcCmd.sNext.eAuType = MSVC_AU_SEQUENCE_END;
				break;
			case VES_AU_PICTURE_I :
				sMsvcCmd.sNext.eAuType = MSVC_AU_PICTURE_I;
				break;
			case VES_AU_PICTURE_P :
				sMsvcCmd.sNext.eAuType = MSVC_AU_PICTURE_P;
				break;
			case VES_AU_PICTURE_B_NOSKIP :
				sMsvcCmd.sNext.eAuType = MSVC_AU_PICTURE_B_NOSKIP;
				break;
			case VES_AU_PICTURE_B_SKIP :
				sMsvcCmd.sNext.eAuType = MSVC_AU_PICTURE_B_SKIP;
				break;
			case VES_AU_UNKNOWN :
				sMsvcCmd.sNext.eAuType = MSVC_AU_UNKNOWN;
				break;
			case VES_AU_PICTURE_X :
				sMsvcCmd.sNext.eAuType = MSVC_AU_PICTURE_X;
				break;
			case VES_AU_EOS :
				sMsvcCmd.sNext.eAuType = MSVC_AU_EOS;
				break;
			default :
				VDEC_KDRV_Message(ERROR, "[VDC%d][Err] AU Type: %d, %s", ui8VdcCh, pVdcAu_Next->eAuType, __FUNCTION__ );
				return FALSE;
			}
			sMsvcCmd.sNext.eAuType = pVdcAu_Next->eAuType;
			sMsvcCmd.sNext.ui32AuAddr = pVdcAu_Next->ui32AuStartAddr;
			sMsvcCmd.sNext.ui32AuSize = pVdcAu_Next->ui32AuSize;
			sMsvcCmd.sNext.ui32AuEnd = pVdcAu_Next->ui32AuEndAddr;
		}
		sMsvcCmd.ui32DTS = pVdcAu->ui32DTS;
		sMsvcCmd.ui32PTS = pVdcAu->ui32PTS;
		sMsvcCmd.ui32uID = pVdcAu->ui32uID;
		sMsvcCmd.ui64TimeStamp = pVdcAu->ui64TimeStamp;
		sMsvcCmd.ui32InputGSTC = pVdcAu->ui32InputGSTC;
		sMsvcCmd.ui32FeedGSTC = ui32GSTC & 0x7FFFFFFF;
		sMsvcCmd.ui32FrameRateRes_Parser = pVdcAu->ui32FrameRateRes_Parser;
		sMsvcCmd.ui32FrameRateDiv_Parser = pVdcAu->ui32FrameRateDiv_Parser;
		sMsvcCmd.ui32Width = ( pVdcAu->ui32Width == VES_WIDTH_INVALID ) ? MSVC_WIDTH_INVALID : pVdcAu->ui32Width;
		sMsvcCmd.ui32Height = ( pVdcAu->ui32Height == VES_HEIGHT_INVALID ) ? MSVC_HEIGHT_INVALID : pVdcAu->ui32Height;
		sMsvcCmd.ui32FlushAge = pVdcAu->ui32FlushAge;

		bRet = MSVC_ADP_UpdateBuffer(gsVDCdb[ui8VdcCh].ui8DecCh, &sMsvcCmd, pbNeedMore, ui32GSTC, ui32NumOfActiveAuQ, ui32NumOfActiveDQ);
		}
		break;
	case VDC_CODEC_VP8:
		if( pVdcAu->eAuType != VES_AU_EOS )
		{
			VP8_CmdQ_T stVP8Aui;

			stVP8Aui.ui32AuPhyAddr = pVdcAu->ui32AuStartAddr;
			stVP8Aui.pui8VirStartPtr = pVdcAu->pui8VirStartPtr;
			stVP8Aui.ui32AuSize = pVdcAu->ui32AuSize;
			stVP8Aui.ui32DTS = pVdcAu->ui32DTS;
			stVP8Aui.ui32PTS = pVdcAu->ui32PTS;
			stVP8Aui.ui64TimeStamp = pVdcAu->ui64TimeStamp;
			stVP8Aui.ui32uID = pVdcAu->ui32uID;
			stVP8Aui.ui32InputGSTC = pVdcAu->ui32InputGSTC;
			stVP8Aui.ui32FeedGSTC = ui32GSTC & 0x7FFFFFFF;
			stVP8Aui.ui32FrameRateRes_Parser = pVdcAu->ui32FrameRateRes_Parser;
			stVP8Aui.ui32FrameRateDiv_Parser = pVdcAu->ui32FrameRateDiv_Parser;
			stVP8Aui.ui32FlushAge = pVdcAu->ui32FlushAge;

			bRet = VP8_SendCmd(gsVDCdb[ui8VdcCh].ui8DecCh, &stVP8Aui);
		}
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, gsVDCdb[ui8VdcCh].eVcodec, __FUNCTION__ );
		break;
	}

	if( gsVDCdb[ui8VdcCh].Status.ui32FlushAge != gsVDCdb[ui8VdcCh].Config.ui32FlushAge )
		VDEC_KDRV_Message(WARNING, "[VDC%d-%d][DBG] Flush Transition: %d --> %d, UID:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh,
									gsVDCdb[ui8VdcCh].Status.ui32FlushAge, gsVDCdb[ui8VdcCh].Config.ui32FlushAge, pVdcAu->ui32uID, __FUNCTION__ );

	sVdcFeedNoti.ui32Ch = gsVDCdb[ui8VdcCh].Config.ui8Vch;
	sVdcFeedNoti.bRet = bRet;
	sVdcFeedNoti.ui32AuType = pVdcAu->eAuType;
	sVdcFeedNoti.ui32AuStartAddr = pVdcAu->ui32AuStartAddr;
	sVdcFeedNoti.pui8VirStartPtr = pVdcAu->pui8VirStartPtr;
	sVdcFeedNoti.ui32AuEndAddr = pVdcAu->ui32AuEndAddr;
	sVdcFeedNoti.ui32AuSize = pVdcAu->ui32AuSize;
	sVdcFeedNoti.ui32DTS = pVdcAu->ui32DTS;
	sVdcFeedNoti.ui32PTS = pVdcAu->ui32PTS;
	sVdcFeedNoti.ui64TimeStamp = pVdcAu->ui64TimeStamp;
	sVdcFeedNoti.ui32uID = pVdcAu->ui32uID;
	sVdcFeedNoti.ui32Width = pVdcAu->ui32Width;
	sVdcFeedNoti.ui32Height = pVdcAu->ui32Height;
	sVdcFeedNoti.ui32FrameRateRes_Parser = pVdcAu->ui32FrameRateRes_Parser;
	sVdcFeedNoti.ui32FrameRateDiv_Parser = pVdcAu->ui32FrameRateDiv_Parser;
 	sVdcFeedNoti.ui32AuqOccupancy = AU_Q_NumActive(ui8VdcCh);
 	sVdcFeedNoti.bFlushed = FALSE;

	_gfpVDC_FeedInfo((S_VDC_CALLBACK_BODY_ESINFO_T *)&sVdcFeedNoti);

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
static void _VDC_DEC_ReportPicInfo(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param)
{
	BOOLEAN		bSucces;
	S_VDC_CALLBACK_BODY_DECINFO_T		sVdcDecNoti;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return;
	}

	if( gsVDCdb[ui8VdcCh].state == VDC_CLOSED )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
	}
	else
	{
		sVdcDecNoti.ui8Vch = gsVDCdb[ui8VdcCh].Config.ui8Vch;
		sVdcDecNoti.u32CodecType_Config = gsVDCdb[ui8VdcCh].eVcodec;

		switch( vdc_picinfo_param->eCmdHdr )
		{
		case VDC_CMD_HDR_SEQINFO:
			bSucces = vdc_picinfo_param->u.seqInfo.ui32IsSuccess & 0x1;
			if( bSucces == 1 )
			{
				gsVDCdb[ui8VdcCh].Status.bSeqInit = TRUE;
				gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = vdc_picinfo_param->u.seqInfo.ui32RefFrameCount;

				if( gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size > VDEC_MAX_SEQHDR_SIZE )
				{
					gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr += (gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size - VDEC_MAX_SEQHDR_SIZE);
					gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr += (gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size - VDEC_MAX_SEQHDR_SIZE);
					gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size = VDEC_MAX_SEQHDR_SIZE;
				}

				if( gsVDCdb[ui8VdcCh].Config.bSrcBufMode == TRUE )
				{
					memcpy(&gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui8aData, gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr, gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size);
				}
			}
			else
			{
				gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt++;
				if( (gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt % 16) == 0 )
				{
					_VDC_FEED_RequestReset(ui8VdcCh);
				}

				gsVDCdb[ui8VdcCh].Status.bOverSpec = vdc_picinfo_param->u.seqInfo.bOverSpec;
			}

			sVdcDecNoti.eCmdHdr = VDC_DECINFO_UPDATE_HDR_SEQINFO;
			sVdcDecNoti.u.seqInfo.u32IsSuccess = vdc_picinfo_param->u.seqInfo.ui32IsSuccess;
			sVdcDecNoti.u.seqInfo.u32Profile = vdc_picinfo_param->u.seqInfo.ui32Profile;
			sVdcDecNoti.u.seqInfo.u32Level = vdc_picinfo_param->u.seqInfo.ui32Level;
			sVdcDecNoti.u.seqInfo.u32Aspectratio = vdc_picinfo_param->u.seqInfo.ui32AspectRatio;
			sVdcDecNoti.u.seqInfo.ui32uID  = vdc_picinfo_param->u.seqInfo.ui32uID;
			sVdcDecNoti.u.seqInfo.u32PicWidth = vdc_picinfo_param->u.seqInfo.ui32PicWidth;
			sVdcDecNoti.u.seqInfo.u32PicHeight = vdc_picinfo_param->u.seqInfo.ui32PicHeight;
			sVdcDecNoti.u.seqInfo.ui32FrameRateRes = vdc_picinfo_param->u.seqInfo.ui32FrameRateRes;
			sVdcDecNoti.u.seqInfo.ui32FrameRateDiv = vdc_picinfo_param->u.seqInfo.ui32FrameRateDiv;
			sVdcDecNoti.u.seqInfo.ui32RefFrameCount = vdc_picinfo_param->u.seqInfo.ui32RefFrameCount;
			sVdcDecNoti.u.seqInfo.ui32ProgSeq = vdc_picinfo_param->u.seqInfo.ui32ProgSeq;
			break;

		case VDC_CMD_HDR_PICINFO:
			if( vdc_picinfo_param->u.picInfo.ui32FlushAge != gsVDCdb[ui8VdcCh].Status.ui32FlushAge )
			{
				if( (vdc_picinfo_param->u.picInfo.ui32FlushAge == gsVDCdb[ui8VdcCh].Config.ui32FlushAge) &&
					(vdc_picinfo_param->u.picInfo.ui32PicType == 0) &&
					(vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs == 0) )
				{
					gsVDCdb[ui8VdcCh].Status.ui32FlushAge = vdc_picinfo_param->u.picInfo.ui32FlushAge;
					VDEC_KDRV_Message(WARNING, "[VDC%d][Wrn] Apply New DPB ID:%d, NoErrMbCnt:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].Status.ui32FlushAge, gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt, __FUNCTION__ );
				}
				else
				{
					UINT32	ui32PBSkipThreshold;

					ui32PBSkipThreshold = ( gsVDCdb[ui8VdcCh].Config.bSrcBufMode == TRUE ) ? VDC_PB_SKIP_THRESHOLD_BUFF : VDC_PB_SKIP_THRESHOLD_LIVE;

					if( gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt > ui32PBSkipThreshold )
					{
						gsVDCdb[ui8VdcCh].Status.ui32FlushAge = vdc_picinfo_param->u.picInfo.ui32FlushAge;
						VDEC_KDRV_Message(ERROR, "[VDC%d][Wrn] Give Up I-Pic Search, NoErrMbCnt:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt, __FUNCTION__ );
					}
				}
			}

			if( vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs > gsVDCdb[ui8VdcCh].Config.ui32ErrorMBFilter )
			{
				vdc_picinfo_param->u.picInfo.ui32FlushAge -= 1;
				VDEC_KDRV_Message(ERROR, "[VDC%d][Wrn] Discard Frames with Error MBs:%d, Filter:%d, %s", ui8VdcCh, vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs, gsVDCdb[ui8VdcCh].Config.ui32ErrorMBFilter, __FUNCTION__ );
			}
			else if( vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs )
			{
				VDEC_KDRV_Message(ERROR, "[VDC%d][Wrn] Display Frames with Error MBs:%d, Filter:%d, %s", ui8VdcCh, vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs, gsVDCdb[ui8VdcCh].Config.ui32ErrorMBFilter, __FUNCTION__ );
			}

			if( vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs )
				gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt = 0;
			else
				gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt++;

			if( gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt == 0 )
			{
				VDEC_KDRV_Message(DBG_SYS, "[VDC%d-%d][DBG] PicType:%d, Y:0x%08x, C:0x%08x, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh,
												vdc_picinfo_param->u.picInfo.ui32PicType,
												vdc_picinfo_param->u.picInfo.ui32YFrameAddr,
												vdc_picinfo_param->u.picInfo.ui32CFrameAddr,
												__FUNCTION__ );
			}

			gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt++;

//			VDEC_KDRV_Message(DBG_SYS, "[VDC%d][DBG] DisplayingPicCnt:%d, Addr:0x%X, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt, vdc_picinfo_param->u.picInfo.ui32YFrameAddr, __FUNCTION__ );

			sVdcDecNoti.eCmdHdr = VDC_DECINFO_UPDATE_HDR_PICINFO;
			sVdcDecNoti.u.picInfo.u32DecodingSuccess = vdc_picinfo_param->u.picInfo.ui32IsSuccess;
			sVdcDecNoti.u.picInfo.ui32YFrameAddr = vdc_picinfo_param->u.picInfo.ui32YFrameAddr;
			sVdcDecNoti.u.picInfo.ui32CFrameAddr = vdc_picinfo_param->u.picInfo.ui32CFrameAddr;
			sVdcDecNoti.u.picInfo.u32BufStride = vdc_picinfo_param->u.picInfo.ui32Stride;
			sVdcDecNoti.u.picInfo.u32Aspectratio = vdc_picinfo_param->u.picInfo.ui32AspectRatio;
			sVdcDecNoti.u.picInfo.u32PicWidth = vdc_picinfo_param->u.picInfo.ui32PicWidth;
			sVdcDecNoti.u.picInfo.u32PicHeight = vdc_picinfo_param->u.picInfo.ui32PicHeight;
			sVdcDecNoti.u.picInfo.u32CropLeftOffset = vdc_picinfo_param->u.picInfo.ui32CropLeftOffset;
			sVdcDecNoti.u.picInfo.u32CropRightOffset = vdc_picinfo_param->u.picInfo.ui32CropRightOffset;
			sVdcDecNoti.u.picInfo.u32CropTopOffset = vdc_picinfo_param->u.picInfo.ui32CropTopOffset;
			sVdcDecNoti.u.picInfo.u32CropBottomOffset = vdc_picinfo_param->u.picInfo.ui32CropBottomOffset;
			sVdcDecNoti.u.picInfo.eDisplayInfo = vdc_picinfo_param->u.picInfo.eDisplayInfo;
			sVdcDecNoti.u.picInfo.bFieldPicture = ( vdc_picinfo_param->u.picInfo.ui32PictureStructure == FRAME_PIC ) ? FALSE : TRUE;
			sVdcDecNoti.u.picInfo.ui32DisplayPeriod = vdc_picinfo_param->u.picInfo.ui32DisplayPeriod;
			sVdcDecNoti.u.picInfo.si32FrmPackArrSei = vdc_picinfo_param->u.picInfo.si32FrmPackArrSei;
			sVdcDecNoti.u.picInfo.eLR_Order = vdc_picinfo_param->u.picInfo.eLR_Order;
			sVdcDecNoti.u.picInfo.ui16ParW = vdc_picinfo_param->u.picInfo.ui16ParW;
			sVdcDecNoti.u.picInfo.ui16ParH = vdc_picinfo_param->u.picInfo.ui16ParH;

			sVdcDecNoti.u.picInfo.sUsrData.ui32Size = vdc_picinfo_param->u.picInfo.sUsrData.ui32Size;
			if( vdc_picinfo_param->u.picInfo.sUsrData.ui32Size )
			{
				sVdcDecNoti.u.picInfo.sUsrData.ui32PicType = vdc_picinfo_param->u.picInfo.sUsrData.ui32PicType;
				sVdcDecNoti.u.picInfo.sUsrData.ui32Rpt_ff = vdc_picinfo_param->u.picInfo.sUsrData.ui32Rpt_ff;
				sVdcDecNoti.u.picInfo.sUsrData.ui32Top_ff = vdc_picinfo_param->u.picInfo.sUsrData.ui32Top_ff;
				sVdcDecNoti.u.picInfo.sUsrData.ui32BuffAddr =vdc_picinfo_param->u.picInfo.sUsrData.ui32BuffAddr;
			}

			sVdcDecNoti.u.picInfo.sDTS.ui64TimeStamp = vdc_picinfo_param->u.picInfo.sDTS.ui64TimeStamp;
			sVdcDecNoti.u.picInfo.sDTS.ui32DTS  = vdc_picinfo_param->u.picInfo.sDTS.ui32DTS;
			sVdcDecNoti.u.picInfo.sDTS.ui32uID  = vdc_picinfo_param->u.picInfo.sDTS.ui32uID;
			sVdcDecNoti.u.picInfo.sPTS.ui64TimeStamp = vdc_picinfo_param->u.picInfo.sPTS.ui64TimeStamp;
			sVdcDecNoti.u.picInfo.sPTS.ui32PTS  = vdc_picinfo_param->u.picInfo.sPTS.ui32PTS;
			sVdcDecNoti.u.picInfo.sPTS.ui32uID  = vdc_picinfo_param->u.picInfo.sPTS.ui32uID;

			sVdcDecNoti.u.picInfo.ui32InputGSTC = vdc_picinfo_param->u.picInfo.ui32InputGSTC;
			sVdcDecNoti.u.picInfo.ui32FeedGSTC = vdc_picinfo_param->u.picInfo.ui32FeedGSTC;
			sVdcDecNoti.u.picInfo.ui32FlushAge = gsVDCdb[ui8VdcCh].Status.ui32FlushAge;

			sVdcDecNoti.u.picInfo.u32NumOfErrMBs = vdc_picinfo_param->u.picInfo.ui32NumOfErrMBs;
			sVdcDecNoti.u.picInfo.u32PicType = vdc_picinfo_param->u.picInfo.ui32PicType;
			sVdcDecNoti.u.picInfo.u32ActiveFMT = vdc_picinfo_param->u.picInfo.ui32ActiveFMT;
			sVdcDecNoti.u.picInfo.u32ProgSeq = vdc_picinfo_param->u.picInfo.ui32ProgSeq;
			sVdcDecNoti.u.picInfo.u32ProgFrame = vdc_picinfo_param->u.picInfo.ui32ProgFrame;
			sVdcDecNoti.u.picInfo.u32LowDelay = vdc_picinfo_param->u.picInfo.ui32LowDelay;

			sVdcDecNoti.u.picInfo.sFrameRate_Parser.ui32Residual = vdc_picinfo_param->u.picInfo.sFrameRate_Parser.ui32Residual;
			sVdcDecNoti.u.picInfo.sFrameRate_Parser.ui32Divider = vdc_picinfo_param->u.picInfo.sFrameRate_Parser.ui32Divider;
			sVdcDecNoti.u.picInfo.sFrameRate_Decoder.ui32Residual = vdc_picinfo_param->u.picInfo.sFrameRate_Decoder.ui32Residual;
			sVdcDecNoti.u.picInfo.sFrameRate_Decoder.ui32Divider = vdc_picinfo_param->u.picInfo.sFrameRate_Decoder.ui32Divider;
			sVdcDecNoti.u.picInfo.bVariableFrameRate = vdc_picinfo_param->u.picInfo.bVariableFrameRate;

			sVdcDecNoti.u.picInfo.ui32DqOccupancy  = gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt;
			break;

		case VDC_CMD_HDR_RESET:
			_VDC_FEED_RequestReset(ui8VdcCh);
			break;
		default:
			VDEC_KDRV_Message(WARNING, "[VDC%d-%d][Wrn] eCmdHdr:%d , %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, vdc_picinfo_param->eCmdHdr, __FUNCTION__ );
		}

		if( (vdc_picinfo_param->eCmdHdr == VDC_CMD_HDR_SEQINFO) ||
			(vdc_picinfo_param->eCmdHdr == VDC_CMD_HDR_PICINFO) )
		{
			_gfpVDC_DecInfo((S_VDC_CALLBACK_BODY_DECINFO_T *)&sVdcDecNoti);
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
static void _VDC_DEC_WaitResetComplete(E_VDC_CODEC_T eVcodec, UINT8 ui8DecCh)
{
	UINT32		ui32WaintCount = 0;
	unsigned long		irq_flag;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		while( MSVC_ADP_IsResetting(ui8DecCh) == TRUE )
		{
			if( ui32WaintCount > 500 )
			{
				VDEC_KDRV_Message(ERROR, "[VDC-%d][Err] Missing RST ISR, %s", ui8DecCh, __FUNCTION__);

				spin_lock_irqsave(&tVdcLock, irq_flag);
				MSVC_ADP_ResetDone(ui8DecCh);
				spin_unlock_irqrestore(&tVdcLock, irq_flag);
				break;
			}

			mdelay(10);
			ui32WaintCount++;
		}
		break;
	case VDC_CODEC_VP8:
	default:
		break;
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
static UINT32 _VDC_Get_CPBPhyWrAddr(UINT8 ui8VdcCh)
{
	return AU_Q_GetWrAddr(ui8VdcCh);
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
void VDC_Init( void (*_fpVDC_FeedInfo)(S_VDC_CALLBACK_BODY_ESINFO_T *pFeedInfo),
				void (*_fpVDC_DecInfo)(S_VDC_CALLBACK_BODY_DECINFO_T *pDecInfo),
				void (*_fpVDC_RequestReset)(S_VDC_CALLBACK_BODY_REQUESTRESET_T *pRequestReset) )
{
	UINT32	gsVDCPhy, gsVDCSize, gsVDCCmdPhy;
	UINT8	ui8VdcCh;

#if (!defined (__XTENSA__)&&!defined(USE_MCU_FOR_TOP))	\
		||(defined (__XTENSA__)&&defined(USE_MCU_FOR_TOP))
	VDEC_KDRV_Message(_TRACE, "[VDC][DBG] %s", __FUNCTION__ );
	gsVDCSize = sizeof(VDEC_VDC_DB_T) * VDC_NUM_OF_CHANNEL;
	gsVDCPhy = VDEC_SHM_Malloc(gsVDCSize);
	IPC_REG_VDC_SetDb(gsVDCPhy);
#else
	VDEC_KDRV_Message(_TRACE, "[VDC][DBG] %s", __FUNCTION__ );
	gsVDCPhy = IPC_REG_VDC_GetDb();
#endif

	gsVDCSize = sizeof(VDEC_VDC_DB_T) * VDC_NUM_OF_CHANNEL;
	gsVDCdb = (VDEC_VDC_DB_T *)VDEC_TranselateVirualAddr(gsVDCPhy, gsVDCSize);
	gsVDCCmdPhy = (gsVDCPhy+gsVDCSize);

	spin_lock_init(&tVdcLock);
	_gfpVDC_FeedInfo = _fpVDC_FeedInfo;
	_gfpVDC_DecInfo = _fpVDC_DecInfo;
	_gfpVDC_RequestReset = _fpVDC_RequestReset;

#if (!defined (__XTENSA__)&&!defined(USE_MCU_FOR_TOP))	\
		||(defined (__XTENSA__)&&defined(USE_MCU_FOR_TOP))
	for(ui8VdcCh=0; ui8VdcCh<VDC_NUM_OF_CHANNEL ; ui8VdcCh++)
	{
		gsVDCdb[ui8VdcCh].state = VDC_CLOSED;
		gsVDCdb[ui8VdcCh].eVcodec = VDC_CODEC_INVALID;
		gsVDCdb[ui8VdcCh].ui8DecCh = 0xFF;

		gsVDCdb[ui8VdcCh].Config.ui8Vch = 0xFF;

		gsVDCdb[ui8VdcCh].Config.bNoReordering = FALSE;
		gsVDCdb[ui8VdcCh].Config.bSrcBufMode = FALSE;
		gsVDCdb[ui8VdcCh].Config.ui32FlushAge = 0x0;

		gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = 0;
		gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = 0;

		gsVDCdb[ui8VdcCh].Status.ui32FlushAge = 0x0;

		gsVDCdb[ui8VdcCh].Status.bSeqInit = FALSE;
		gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt = 0;
		gsVDCdb[ui8VdcCh].Status.bOverSpec = FALSE;
		gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr = 0x0;
		gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr = NULL;
		gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size = 0;

		gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = 0;
		gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;
	}

	MSVC_ADP_Init(_VDC_DEC_GetUsedFrame, _VDC_DEC_ReportPicInfo, _VDC_Get_CPBPhyWrAddr);
	MSVC_Init();
	VP8_Init(_VDC_DEC_GetUsedFrame, _VDC_DEC_ReportPicInfo);
	STC_TIMER_Init();
#endif

	FEED_Init(_VDC_FEED_GetDQStatus, _VDC_FEED_IsReady, _VDC_FEED_RequestReset, _VDC_FEED_SendCmd);
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
UINT8 VDC_Open(	UINT8 ui8Vch,
					E_VDC_CODEC_T eVcodec,
					UINT32 ui32CpbBufAddr,
					UINT32 ui32CpbBufSize,
					BOOLEAN bRingBufferMode,
					BOOLEAN bSrcBufMode,
					BOOLEAN bNoReordering,
					BOOLEAN bLowLatency,
					BOOLEAN bDualDecoding,
					UINT32 ui32FlushAge,
					UINT32 ui32ErrorMBFilter,
					UINT32 ui32Width, UINT32 ui32Height // for VDC_CODEC_DIVX3
					)
{
	UINT8	ui8VdcCh;
	UINT8	ui8DecCh = 0xFF;
	unsigned long		irq_flag;

	VDEC_KDRV_Message(_TRACE, "[VDC][DBG] CodecType:%d, SrcBufMode:%d, bNoReordering:%d, %s", eVcodec, bSrcBufMode, bNoReordering, __FUNCTION__ );

	spin_lock_irqsave(&tVdcLock, irq_flag);

	for( ui8VdcCh = 0; ui8VdcCh < VDC_NUM_OF_CHANNEL; ui8VdcCh++ )
	{
		if( gsVDCdb[ui8VdcCh].state == VDC_CLOSED )
			break;
	}
	if( ui8VdcCh == VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC][Err] Not Enough VDC Channel, %s", __FUNCTION__ );
		for( ui8VdcCh = 0; ui8VdcCh < VDC_NUM_OF_CHANNEL; ui8VdcCh++ )
			VDEC_KDRV_Message(ERROR, "[VDC][Err] State:%d, %s", gsVDCdb[ui8VdcCh].state, __FUNCTION__ );

		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return 0xFF;
	}

	if( AU_Q_Open( ui8VdcCh, AU_Q_NUM_OF_NODE, ui32CpbBufAddr, ui32CpbBufSize) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDC][Err] Fail to Open AuQ, %s", __FUNCTION__ );

		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return 0xFF;
	}

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		{
		E_MSVC_CODEC_T		eMsvcCodec;

		switch(eVcodec)
		{
		case VDC_CODEC_H264_HP:
			eMsvcCodec = MSVC_CODEC_H264_HP;
			break;
		case VDC_CODEC_H264_MVC:
			eMsvcCodec = MSVC_CODEC_H264_MVC;
			break;
		case VDC_CODEC_VC1_RCV_V1:
			eMsvcCodec = MSVC_CODEC_VC1_RCV_V1;
			break;
		case VDC_CODEC_VC1_RCV_V2:
			eMsvcCodec = MSVC_CODEC_VC1_RCV_V2;
			break;
		case VDC_CODEC_VC1_AP:
			eMsvcCodec = MSVC_CODEC_VC1_AP;
			break;
		case VDC_CODEC_MPEG2_HP:
			eMsvcCodec = MSVC_CODEC_MPEG2_HP;
			break;
		case VDC_CODEC_AVS:
			eMsvcCodec = MSVC_CODEC_AVS;
			break;
		case VDC_CODEC_H263:
			eMsvcCodec = MSVC_CODEC_H263;
			break;
		case VDC_CODEC_XVID:
			eMsvcCodec = MSVC_CODEC_XVID;
			break;
		case VDC_CODEC_DIVX3:
			eMsvcCodec = MSVC_CODEC_DIVX3;
			break;
		case VDC_CODEC_DIVX4:
			eMsvcCodec = MSVC_CODEC_DIVX4;
			break;
		case VDC_CODEC_DIVX5:
			eMsvcCodec = MSVC_CODEC_DIVX5;
			break;
		case VDC_CODEC_RVX:
			eMsvcCodec = MSVC_CODEC_RVX;
			break;
		default:
			eMsvcCodec = MSVC_CODEC_INVALID;
			VDEC_KDRV_Message(ERROR, "[VDC][Err] Not Support MSVC Codec Type:%d, %s", eVcodec, __FUNCTION__ );
			break;
		}

		ui8DecCh = MSVC_ADP_Open(ui8VdcCh, eMsvcCodec, ui32CpbBufAddr, ui32CpbBufSize, bRingBufferMode, bLowLatency, bDualDecoding, bNoReordering, ui32Width, ui32Height);

		ui8DecCh = MSVC_Open(ui8DecCh, bNoReordering, eVcodec);
		}
		break;
	case VDC_CODEC_VP8:
		ui8DecCh = VP8_Open(ui8VdcCh);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC][Err] Not Support Codec Type:%d, %s", eVcodec, __FUNCTION__ );
		break;
	}

	if( ui8DecCh == 0xFF )
	{
		VDEC_KDRV_Message(ERROR, "[VDC][Err] Not Enough Dec Channel, %s", __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return 0xFF;
	}

	FEED_Open(ui8VdcCh, bDualDecoding);

	// feeder state
	gsVDCdb[ui8VdcCh].state = VDC_OPENED;
	gsVDCdb[ui8VdcCh].eVcodec = eVcodec;
	gsVDCdb[ui8VdcCh].ui8DecCh = ui8DecCh;

	gsVDCdb[ui8VdcCh].Config.ui8Vch = ui8Vch;
	gsVDCdb[ui8VdcCh].Config.ui32CpbBufAddr = ui32CpbBufAddr;
	gsVDCdb[ui8VdcCh].Config.ui32CpbBufSize = ui32CpbBufSize;

	gsVDCdb[ui8VdcCh].Config.bNoReordering = bNoReordering;
	gsVDCdb[ui8VdcCh].Config.bSrcBufMode = bSrcBufMode;
	gsVDCdb[ui8VdcCh].Config.ui32FlushAge = ui32FlushAge;
	gsVDCdb[ui8VdcCh].Config.ui32ErrorMBFilter = ui32ErrorMBFilter;

	gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32FlushAge = ui32FlushAge - 1;
	gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;

	gsVDCdb[ui8VdcCh].Status.bSeqInit = FALSE;
	gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt = 0;
	gsVDCdb[ui8VdcCh].Status.bOverSpec = FALSE;
	gsVDCdb[ui8VdcCh].Status.bEosCmd = FALSE;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr = 0x0;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr = NULL;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size = 0;

	STC_TIMER_Start(STC_TIMER_ID_FEED);

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	_VDC_DEC_WaitResetComplete(eVcodec, ui8DecCh);

	return ui8VdcCh;
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
void VDC_Close(UINT8 ui8VdcCh)
{
	UINT8 			ch;
	unsigned long		irq_flag;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VDC%d-%d][DBG] %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, __FUNCTION__ );

	FEED_Close(ui8VdcCh);

	switch(gsVDCdb[ui8VdcCh].eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		MSVC_Close(gsVDCdb[ui8VdcCh].ui8DecCh);
		MSVC_ADP_Close(gsVDCdb[ui8VdcCh].ui8DecCh);
		break;
	case VDC_CODEC_VP8:
		VP8_Close(gsVDCdb[ui8VdcCh].ui8DecCh);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, gsVDCdb[ui8VdcCh].eVcodec, __FUNCTION__ );
		break;
	}

	AU_Q_Close(ui8VdcCh);

	gsVDCdb[ui8VdcCh].state = VDC_CLOSED;
	gsVDCdb[ui8VdcCh].eVcodec = VDC_CODEC_INVALID;
	gsVDCdb[ui8VdcCh].ui8DecCh = 0xFF;

	gsVDCdb[ui8VdcCh].Config.ui8Vch = 0xFF;
	gsVDCdb[ui8VdcCh].Config.ui32CpbBufAddr = 0x0;
	gsVDCdb[ui8VdcCh].Config.ui32CpbBufSize = 0x0;

	gsVDCdb[ui8VdcCh].Config.bNoReordering = FALSE;
	gsVDCdb[ui8VdcCh].Config.bSrcBufMode = FALSE;

	gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = 0;

	gsVDCdb[ui8VdcCh].Status.bSeqInit = FALSE;
	gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt = 0;
	gsVDCdb[ui8VdcCh].Status.bOverSpec = FALSE;
	gsVDCdb[ui8VdcCh].Status.bEosCmd = FALSE;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr = 0x0;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr = NULL;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size = 0;

	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;

	for( ch = 0; ch < VDC_NUM_OF_CHANNEL; ch++ )
	{
		if(gsVDCdb[ch].state == VDC_OPENED)
			break;
	}
	if( ch >= VDC_NUM_OF_CHANNEL )	// exist no more worked decoding channel
	{
		VDEC_KDRV_Message(_TRACE, "[VDC%d-%d][DBG] All Channel is NULL State - Intr Clear: %d %s", ui8VdcCh, gsVDCdb[ui8VdcCh].ui8DecCh, ch, __FUNCTION__ );
//		STC_TIMER_Stop();
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);
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
BOOLEAN VDC_UpdateAuQ(UINT8 ui8VdcCh, S_VDC_AU_T *psVesSeqAu)
{
	BOOLEAN		bRet = FALSE;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	psVesSeqAu->ui32FlushAge = gsVDCdb[ui8VdcCh].Config.ui32FlushAge;
	if( (bRet = AU_Q_Push(ui8VdcCh, psVesSeqAu)) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] AU Q Overflow - UseNum: %d, %s(%d)", ui8VdcCh, AU_Q_NumActive(ui8VdcCh), __FUNCTION__, __LINE__ );
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
void VDC_Reset(UINT8 ui8VdcCh)
{
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	VDEC_KDRV_Message(ERROR, "[VDC%d-%d][DBG] CodecType:%d - ResetCnt:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, ui32VdcResetCnt[eVcodec], __FUNCTION__ );

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		MSVC_ADP_Reset(ui8DecCh);
		break;
	case VDC_CODEC_VP8:
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return;
	}

	ui32VdcResetCnt[eVcodec]++;

	AU_Q_Reset(ui8VdcCh, gsVDCdb[ui8VdcCh].Config.ui32CpbBufAddr);

	gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt = 0;

	gsVDCdb[ui8VdcCh].Status.bSeqInit = FALSE;
	gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt = 0;
	gsVDCdb[ui8VdcCh].Status.bOverSpec = FALSE;
	gsVDCdb[ui8VdcCh].Status.bEosCmd = FALSE;

	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	_VDC_DEC_WaitResetComplete(eVcodec, ui8DecCh);
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
BOOLEAN VDC_Flush(UINT8 ui8VdcCh, UINT32 ui32FlushAge)
{
	BOOLEAN 			bSeqInit;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return FALSE;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	gsVDCdb[ui8VdcCh].Config.ui32FlushAge = ui32FlushAge;
	gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt = 0;

	VDEC_KDRV_Message(_TRACE, "[VDC%d-%d][DBG] DPB ID:%d, %s", ui8VdcCh, ui8DecCh, ui32FlushAge, __FUNCTION__ );

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		MSVC_ADP_Flush(ui8DecCh);
		break;
	case VDC_CODEC_VP8:
		VP8_Flush(ui8DecCh);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	if( (bSeqInit = gsVDCdb[ui8VdcCh].Status.bSeqInit) == TRUE )
	{
		S_VDC_AU_T			sVdcAu;

		while( AU_Q_Pop( ui8VdcCh, &sVdcAu ) == TRUE )
		{
			S_VDC_CALLBACK_BODY_ESINFO_T		sVdcFeedNoti;

			sVdcFeedNoti.ui32Ch = gsVDCdb[ui8VdcCh].Config.ui8Vch;
			sVdcFeedNoti.bRet = FALSE;
			sVdcFeedNoti.ui32AuType = sVdcAu.eAuType;
			sVdcFeedNoti.ui32AuStartAddr = sVdcAu.ui32AuStartAddr;
			sVdcFeedNoti.pui8VirStartPtr = sVdcAu.pui8VirStartPtr;
			sVdcFeedNoti.ui32AuEndAddr = sVdcAu.ui32AuEndAddr;
			sVdcFeedNoti.ui32AuSize = sVdcAu.ui32AuSize;
			sVdcFeedNoti.ui32DTS = sVdcAu.ui32DTS;
			sVdcFeedNoti.ui32PTS = sVdcAu.ui32PTS;
			sVdcFeedNoti.ui64TimeStamp = sVdcAu.ui64TimeStamp;
			sVdcFeedNoti.ui32uID = sVdcAu.ui32uID;
			sVdcFeedNoti.ui32Width = sVdcAu.ui32Width;
			sVdcFeedNoti.ui32Height = sVdcAu.ui32Height;
			sVdcFeedNoti.ui32FrameRateRes_Parser = sVdcAu.ui32FrameRateRes_Parser;
			sVdcFeedNoti.ui32FrameRateDiv_Parser = sVdcAu.ui32FrameRateDiv_Parser;
		 	sVdcFeedNoti.ui32AuqOccupancy = AU_Q_NumActive(ui8VdcCh);
		 	sVdcFeedNoti.bFlushed = TRUE;

			_gfpVDC_FeedInfo((S_VDC_CALLBACK_BODY_ESINFO_T *)&sVdcFeedNoti);
		}
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return bSeqInit;
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
BOOLEAN VDC_IsSeqInit(UINT8 ui8VdcCh)
{
	BOOLEAN 			bSeqInit;
	unsigned long		irq_flag;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if( gsVDCdb[ui8VdcCh].state == VDC_CLOSED )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return FALSE;
	}

	bSeqInit = gsVDCdb[ui8VdcCh].Status.bSeqInit;

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return bSeqInit;
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
UINT32 VDC_AU_Q_Occupancy(UINT8 ui8VdcCh)
{
	UINT32			ui32UsedBuf;

	ui32UsedBuf = AU_Q_NumActive(ui8VdcCh);

	return ui32UsedBuf;
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
UINT32 VDC_DQ_Occupancy(UINT8 ui8VdcCh)
{
	UINT32			ui32UsedBuf;
	unsigned long		irq_flag;

	spin_lock_irqsave(&tVdcLock, irq_flag);

	ui32UsedBuf = gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt;

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return ui32UsedBuf;
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
BOOLEAN VDC_FeedKick(void)
{
	UINT8				ui8KicCnt = 0;
	unsigned long		irq_flag;

	spin_lock_irqsave(&tVdcLock, irq_flag);

	ui8KicCnt = FEED_Kick();

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return ui8KicCnt ? TRUE : FALSE;
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
BOOLEAN VDC_SetPicScanMode(UINT8 ui8VdcCh, UINT8 ui8ScanMode)
{
	BOOLEAN				bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bRet = MSVC_ADP_SetPicSkipMode(ui8DecCh, (E_MSVC_SKIP_TYPE_T)ui8ScanMode);
		break;
	case VDC_CODEC_VP8:
		bRet = VP8_SetScanMode(ui8DecCh, ui8ScanMode);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
BOOLEAN VDC_SetSrcScanType(UINT8 ui8VdcCh, UINT8 ui8ScanType)
{
	BOOLEAN				bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bRet = MSVC_ADP_SetSrcSkipType(ui8DecCh, (E_MSVC_SKIP_TYPE_T)ui8ScanType);
		break;
	case VDC_CODEC_VP8:
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
BOOLEAN VDC_SetUserDataEn(UINT8 ui8VdcCh, UINT32 u32Enable)
{
	BOOLEAN				bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		MSVC_SetUserDataEn(ui8DecCh, u32Enable);
		bRet = MSVC_ADP_SetUserDataEn(ui8DecCh, u32Enable);
		break;
	case VDC_CODEC_VP8:
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
BOOLEAN VDC_GetUserDataInfo(UINT8 ui8VdcCh, UINT32 *pui32Address, UINT32 *pui32Size)
{
	BOOLEAN bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		*pui32Address = 0;
		*pui32Size = 0;
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		*pui32Address = 0;
		*pui32Size = 0;
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bRet = MSVC_GetUserDataInfo(ui8DecCh, pui32Address, pui32Size);
		break;
	case VDC_CODEC_VP8:
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
UINT32 VDC_GetAllocatedDPB(UINT8 ui8VdcCh)
{
	UINT32 				dpbSize = 0;
	unsigned long		irq_flag;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return dpbSize;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return dpbSize;
	}

	dpbSize = gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt;

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return dpbSize;
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
BOOLEAN VDC_ReconfigDPB(UINT8 ui8VdcCh)
{
	BOOLEAN				bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bRet = MSVC_ADP_ReconfigDPB(ui8DecCh);
		break;
	case VDC_CODEC_VP8:
		bRet = VP8_ReconfigDPB(ui8DecCh);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	gsVDCdb[ui8VdcCh].Status.ui32MinFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32NoErrorMbCnt = 0;
	gsVDCdb[ui8VdcCh].Status.ui32DisplayingPicCnt = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32WrIndex = 0;
	gsVDCdb[ui8VdcCh].Status.sUsedFramdAddrQ.ui32RdIndex = 0;

	gsVDCdb[ui8VdcCh].Status.bSeqInit = FALSE;
	gsVDCdb[ui8VdcCh].Status.ui32SeqFailCnt = 0;
	gsVDCdb[ui8VdcCh].Status.bOverSpec = FALSE;
	gsVDCdb[ui8VdcCh].Status.bEosCmd = FALSE;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32AuCpbAddr = 0x0;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.pui8VirCpbPtr = NULL;
	gsVDCdb[ui8VdcCh].Status.sSeqHdr.ui32Size = 0;

	VDEC_KDRV_Message(DBG_SYS, "[VDC%d-%d] %s", ui8VdcCh, ui8DecCh, __FUNCTION__);

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
BOOLEAN VDC_RegisterDPB(UINT8 ui8VdcCh, S_VDEC_DPB_INFO_T *psVdecDpb, UINT32 ui32BufStride)
{
	BOOLEAN				bRet = FALSE;
	unsigned long		irq_flag;
	E_VDC_CODEC_T eVcodec;
	UINT8		ui8DecCh;

	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return bRet;
	}

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(gsVDCdb[ui8VdcCh].state == VDC_CLOSED)
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Already Closed, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	if( psVdecDpb->ui32NumOfFb == 0 )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] No FB, %s", ui8VdcCh, __FUNCTION__ );
		spin_unlock_irqrestore(&tVdcLock, irq_flag);
		return bRet;
	}

	eVcodec = gsVDCdb[ui8VdcCh].eVcodec;
	ui8DecCh = gsVDCdb[ui8VdcCh].ui8DecCh;

	switch(eVcodec)
	{
	case VDC_CODEC_H264_HP:
	case VDC_CODEC_H264_MVC:
	case VDC_CODEC_VC1_RCV_V1:
	case VDC_CODEC_VC1_RCV_V2:
	case VDC_CODEC_VC1_AP:
	case VDC_CODEC_MPEG2_HP:
	case VDC_CODEC_AVS:
	case VDC_CODEC_H263:
	case VDC_CODEC_XVID:
	case VDC_CODEC_DIVX3:
	case VDC_CODEC_DIVX4:
	case VDC_CODEC_DIVX5:
	case VDC_CODEC_RVX:
		bRet = MSVC_ADP_RegisterDPB(ui8DecCh, psVdecDpb, ui32BufStride);
		break;
	case VDC_CODEC_VP8:
		bRet = VP8_RegisterDPB(ui8DecCh, psVdecDpb);
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDC%d-%d][Err] Not Support Codec Type:%d, %s", ui8VdcCh, ui8DecCh, eVcodec, __FUNCTION__ );
		break;
	}

	if( bRet == TRUE )
	{
		gsVDCdb[ui8VdcCh].Status.ui32TotalFrmCnt = psVdecDpb->ui32NumOfFb;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDC%d-%d] nFrm: %d, %s", ui8VdcCh, ui8DecCh, psVdecDpb->ui32NumOfFb, __FUNCTION__);

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

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
BOOLEAN VDC_DEC_PutUsedFrame(UINT8 ui8VdcCh, UINT32 ui32ClearFrameAddr)
{
	if( ui8VdcCh >= VDC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VDC%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	_VDC_DEC_PutUsedFrame(ui8VdcCh, ui32ClearFrameAddr);

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
UINT32 VDC_LQ_ISR(UINT32 ui32ISRMask)
{
	UINT32		ui32BufClearInt = 0x0;
	unsigned long		irq_flag;

	spin_lock_irqsave(&tVdcLock, irq_flag);

	if(ui32ISRMask&(1<<LQC0_ALMOST_EMPTY))
	{
		ui32BufClearInt |= (1<<LQC0_ALMOST_EMPTY);
	}
	if(ui32ISRMask&(1<<LQC1_ALMOST_EMPTY))
	{
		ui32BufClearInt |= (1<<LQC1_ALMOST_EMPTY);
	}
	if(ui32ISRMask&(1<<LQC2_ALMOST_EMPTY))
	{
		FEED_Kick();
		ui32BufClearInt |= (1<<LQC2_ALMOST_EMPTY);
	}
	if(ui32ISRMask&(1<<LQC3_ALMOST_EMPTY))
	{
		FEED_Kick();
		ui32BufClearInt |= (1<<LQC3_ALMOST_EMPTY);
	}

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return ui32BufClearInt;
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
UINT32 VDC_MSVC_ISR(UINT32 ui32ISRMask)
{
	unsigned long		irq_flag;

	spin_lock_irqsave(&tVdcLock, irq_flag);

	ui32ISRMask = MSVC_DECODER_ISR(ui32ISRMask);

	FEED_Kick();

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return ui32ISRMask;
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
UINT32 VDC_VP8_ISR(UINT32 ui32ISRMask)
{
	unsigned long		irq_flag;

	spin_lock_irqsave(&tVdcLock, irq_flag);

	ui32ISRMask = VP8_DECODER_ISR(ui32ISRMask);

	FEED_Kick();

	spin_unlock_irqrestore(&tVdcLock, irq_flag);

	return ui32ISRMask;
}

