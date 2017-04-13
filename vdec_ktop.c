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
 * Video Decoder Channel Management.
 *
 * author     youngki.lyu@lge.com
 * version    1.0
 * date       2011.04.01
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/
#define USE_ION_ALLOC

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "vdec_cfg.h"
#include "vdec_drv.h"

#include "mcu/mcu_config.h"

#include "vdec_cpb.h"
#include "vdec_dpb.h"
#ifdef USE_ION_ALLOC
#include "vdec_ion.h"
#endif
#include "vdec_ktop.h"
#include "ves/ves_drv.h"
#include "ves/ves_auib.h"
#include "ves/ves_cpb.h"
#include "vdc/vdc_drv.h"
#include "vdc/vdc_auq.h"
#include "vdc/msvc_drv.h"
#include "vds/sync_drv.h"

#include "hal/lg1152/top_hal_api.h"
#include "hal/lg1152/pdec_hal_api.h"
#include "hal/lg1152/lq_hal_api.h"
#include "hal/lg1152/vsync_hal_api.h"
#include "hal/lg1152/de_ipc_hal_api.h"
#include "hal/lg1152/ipc_reg_api.h"
#include "hal/lg1152/mcu_hal_api.h"
#include "hal/lg1152/av_lipsync_hal_api.h"

#include "mcu/ipc_cmd.h"
#include "mcu/ipc_req.h"
#include "mcu/ipc_dbg.h"
#include "mcu/vdec_shm.h"

#include "mcu/vdec_print.h"

#include "mcu/ram_log.h"

#include "vdec_noti.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		VDEC_KTOP_NUM_OF_CALLBACK_NODE				0x400

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT8	ui8Ch;
	enum
	{
		VDEC_KDRV_CB_WQ_ID_NONE = 0,
		VDEC_KDRV_CB_WQ_ID_INPUT,
		VDEC_KDRV_CB_WQ_ID_FEED,
		VDEC_KDRV_CB_WQ_ID_DEC,
		VDEC_KDRV_CB_WQ_ID_DISP,
		VDEC_KDRV_CB_WQ_ID_FLUSH,
		VDEC_KDRV_CB_WQ_ID_RESET,
		VDEC_KDRV_CB_WQ_ID_UPDATE_ES,
		VDEC_KDRV_CB_WQ_ID_UINT32 = 0x20130308,
	} eNotiHdr;
	union
	{
		S_VDEC_NOTI_CALLBACK_ESINFO_T		sInputInfo;
		S_VDEC_NOTI_CALLBACK_ESINFO_T		sFeedInfo;
		S_VDEC_NOTI_CALLBACK_DECINFO_T	sDecInfo;
		S_VDEC_NOTI_CALLBACK_DISPINFO_T	sDispInfo;
		struct
		{
			UINT32	ui32Addr;
			UINT32	ui32Size;
		} sEsInfo;
	} u;
} S_VDEC_CALLBACK_WQ_NODE_T;

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
static void _VDEC_KTOP_CallBack_workfunc(struct work_struct *data);
DECLARE_WORK( _VDEC_KTOP_CallBack_work, _VDEC_KTOP_CallBack_workfunc );

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static struct
{
	VDEC_KTOP_State State;

	struct
	{
		UINT8 ui8Vcodec;
		VDEC_SRC_T eInSrc;
		VDEC_DST_T eOutDst;
		// KTOP Param
		BOOLEAN bNoAdpStr;
		BOOLEAN bForThumbnail;
		// VES Param
		UINT32 ui32DecodeOffset_bytes;
		BOOLEAN b512bytesAligned;
		// VDC Param
		UINT32 ui32ErrorMBFilter;
		UINT32 ui32Width;
		UINT32 ui32Height;
		// VDC & VDS Param
		BOOLEAN bDualDecoding;
		BOOLEAN bLowLatency;
		// VDS Param
		BOOLEAN bUseGstc;
		UINT32 ui32DisplayOffset_ms;
		UINT8 ui8ClkID;
	} Param;

	struct
	{
		UINT8	ui8VesCh;
		UINT8	ui8VdcCh;
		UINT8	ui8SyncCh;
	} ChNum;

	struct
	{
		BOOLEAN 	bSeqInit;
		BOOLEAN 	bEndOfDPB;
		UINT32		ui32PicWidth;
		UINT32		ui32Height;
	} Status;

	UINT32		ui32CpbBufAddr;

	S_VDEC_DPB_INFO_T	sVdecDpb;
#ifdef USE_ION_ALLOC
	struct s_ion_handle *psIonHandle;
#endif

	struct
	{
		UINT32		ui32LogTick;
	} Debug;

} gsVdecTop[VDEC_NUM_OF_CHANNEL];

static UINT32	gui32ActiveSrc;
static UINT32	gui32ActiveDst;

static UINT32	gui32GenId = 0x64116077;

static struct mutex	stVdecKtopLock;

struct workqueue_struct *_VDEC_KTOP_CallBack_workqueue;
struct
{
	S_VDEC_CALLBACK_WQ_NODE_T	sWqNodeInfo[VDEC_KTOP_NUM_OF_CALLBACK_NODE];
	spinlock_t	stWrlock;
	UINT32		ui32WrIndex;
	UINT32		ui32RdIndex;
} gsVdecKtopCallbackQ;


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
static SINT32 _VDEC_KTOP_FindFrameFd(UINT8 ui8Vch, UINT32 ui32FrameAddr)
{
	UINT32	ui32NumOfFb;
	UINT32	ui32MatchedCnt = 0;
	UINT32	ui32MatchedIdx = gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb;

	for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
	{
		if( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY == ui32FrameAddr )
		{
			ui32MatchedCnt++;
			ui32MatchedIdx = ui32NumOfFb;
		}
	}
	if( ui32MatchedCnt != 1 )
	{
		VDEC_KDRV_Message(ERROR,"[VDEC_K%d] No Matched Frame Addr, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
		return 0;
	}

	return gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32MatchedIdx].si32FrameFd;
}

#ifdef USE_ION_ALLOC
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
static BOOLEAN _VDEC_KTOP_FindFramePhy(UINT8 ui8Vch, SINT32 si32FrameFd, UINT32 *pui32BufAddr, UINT32 *pui32BufSize)
{
	UINT32	ui32NumOfFb;
	UINT32	ui32MatchedCnt = 0;
	UINT32	ui32MatchedIdx = gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb;

	for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
	{
		if( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].si32FrameFd == si32FrameFd )
		{
			ui32MatchedCnt++;
			ui32MatchedIdx = ui32NumOfFb;
		}
	}
	if( ui32MatchedCnt != 1 )
	{
		VDEC_KDRV_Message(ERROR,"[VDEC_K%d] No Matched ION Descriptor, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
		return FALSE;
	}

	*pui32BufAddr = gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32MatchedIdx].ui32AddrY;
	*pui32BufSize = gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY + gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb + gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr;

	return TRUE;
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
static BOOLEAN _VDEC_KTOP_GetWqNode(S_VDEC_CALLBACK_WQ_NODE_T *psWqNodeInfo)
{
	UINT32		ui32WrIndex;
	UINT32		ui32RdIndex, ui32RdIndex_Next;

	ui32WrIndex = gsVdecKtopCallbackQ.ui32WrIndex;
	ui32RdIndex = gsVdecKtopCallbackQ.ui32RdIndex;

	if( ui32WrIndex == ui32RdIndex )
		return FALSE;

	memcpy( psWqNodeInfo, &gsVdecKtopCallbackQ.sWqNodeInfo[ui32RdIndex], sizeof(S_VDEC_CALLBACK_WQ_NODE_T) );

	ui32RdIndex_Next = ui32RdIndex + 1;
	if( ui32RdIndex_Next == VDEC_KTOP_NUM_OF_CALLBACK_NODE )
		ui32RdIndex_Next = 0;

	gsVdecKtopCallbackQ.ui32RdIndex = ui32RdIndex_Next;

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
static BOOLEAN _VDEC_KTOP_PutWqNode(S_VDEC_CALLBACK_WQ_NODE_T *psWqNodeInfo)
{
	UINT32		ui32WrIndex, ui32WrIndex_Next;
	UINT32		ui32RdIndex;
	unsigned long		irq_flag;

	spin_lock_irqsave(&gsVdecKtopCallbackQ.stWrlock, irq_flag);

	ui32WrIndex = gsVdecKtopCallbackQ.ui32WrIndex;
	ui32RdIndex = gsVdecKtopCallbackQ.ui32RdIndex;

	ui32WrIndex_Next = ui32WrIndex + 1;
	if( ui32WrIndex_Next == VDEC_KTOP_NUM_OF_CALLBACK_NODE )
		ui32WrIndex_Next = 0;

	if( ui32WrIndex_Next == ui32RdIndex )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K][Err] Noti Callback Q Overflow");
		spin_unlock_irqrestore(&gsVdecKtopCallbackQ.stWrlock, irq_flag);
		return FALSE;
	}

	memcpy( &gsVdecKtopCallbackQ.sWqNodeInfo[ui32WrIndex], psWqNodeInfo, sizeof(S_VDEC_CALLBACK_WQ_NODE_T) );

	gsVdecKtopCallbackQ.ui32WrIndex = ui32WrIndex_Next;

	spin_unlock_irqrestore(&gsVdecKtopCallbackQ.stWrlock, irq_flag);

	queue_work(_VDEC_KTOP_CallBack_workqueue,  &_VDEC_KTOP_CallBack_work);

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
static void _VDEC_KTOP_Wq_UpdateInputInfo(UINT8 ui8Vch, S_VDEC_NOTI_CALLBACK_ESINFO_T *psInputInfo)
{
	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	VDEC_NOTI_UpdateInputInfo(ui8Vch, psInputInfo);

	mutex_unlock(&stVdecKtopLock);
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
static void _VDEC_KTOP_Wq_UpdateFeedInfo(UINT8 ui8Vch, S_VDEC_NOTI_CALLBACK_ESINFO_T *psFeedInfo)
{
	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	VDEC_NOTI_UpdateFeedInfo(ui8Vch, psFeedInfo);

	mutex_unlock(&stVdecKtopLock);
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
static void _VDEC_KTOP_Wq_UpdateDecodeInfo(UINT8 ui8Vch, S_VDEC_NOTI_CALLBACK_DECINFO_T *psDecInfo)
{
	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	switch( psDecInfo->eCmdHdr )
	{
	case KDRV_NOTI_DECINFO_UPDATE_HDR_SEQINFO :
		if( psDecInfo->u.seqInfo.u32IsSuccess & 0x1 )
		{
			UINT32	ui32BufWidth;
			UINT32	ui32BufHeight;
			UINT32	ui32NumOfFb;

			gsVdecTop[ui8Vch].Status.bSeqInit = TRUE;

			gsVdecTop[ui8Vch].Status.ui32PicWidth = psDecInfo->u.seqInfo.u32PicWidth;
			gsVdecTop[ui8Vch].Status.ui32Height = psDecInfo->u.seqInfo.u32PicHeight;

			if( (gsVdecTop[ui8Vch].Param.eOutDst == VDEC_DST_GRP_BUFF0) || (gsVdecTop[ui8Vch].Param.eOutDst == VDEC_DST_GRP_BUFF1) )
			{
				if( gsVdecTop[ui8Vch].Status.bEndOfDPB == FALSE )
				{
					VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Wrn] Not Complete to Register DPB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
				}
				else if( gsVdecTop[ui8Vch].Param.ui8Vcodec != VDC_CODEC_VP8 )
				{
					if( gsVdecTop[ui8Vch].Param.bNoAdpStr == TRUE )
					{
						ui32BufWidth = (psDecInfo->u.seqInfo.u32PicWidth + 0xF) & (~0xF);
						ui32BufHeight = (psDecInfo->u.seqInfo.u32PicHeight + 0x1F) & (~0x1F);
					}
					else
					{
						ui32BufWidth = 2048;
						ui32BufHeight = 1088;
					}

					if( VDC_RegisterDPB(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, &gsVdecTop[ui8Vch].sVdecDpb, ui32BufWidth) == FALSE )
					{
						VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to Register DPB, %s", ui8Vch, __FUNCTION__);

						psDecInfo->u.seqInfo.u32IsSuccess = 0;
					}
				}
			}
			else if( gsVdecTop[ui8Vch].Param.ui8Vcodec != VDC_CODEC_VP8 )
			{
				UINT32	ui32NFrm;
				UINT32	ui32FrameAddr;
				UINT32	ui32BufWidth;
				UINT32	ui32BufHeight;

				UINT32	ui32FrameSize;
				UINT32	ui32YFrameSize;		// Luma
				UINT32	ui32CFrameSize;		// Chroma

				if( gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb )
					VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Already Exist DPB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );

				if( gsVdecTop[ui8Vch].Param.bNoAdpStr == TRUE )
				{
					ui32BufWidth = (psDecInfo->u.seqInfo.u32PicWidth + 0xF) & (~0xF);
					ui32BufHeight = (psDecInfo->u.seqInfo.u32PicHeight + 0x1F) & (~0x1F);
				}
				else
				{
					ui32BufWidth = 2048;
					ui32BufHeight = 1088;
				}

				if( gsVdecTop[ui8Vch].Param.bForThumbnail == TRUE )
				{
					ui32BufWidth = 2048;
					ui32BufHeight = 1088;

					ui32NFrm = 1;
				}
				else
				{
					UINT32	ui32FrameRate = 0;

					if( psDecInfo->u.seqInfo.ui32FrameRateDiv )
						ui32FrameRate = psDecInfo->u.seqInfo.ui32FrameRateRes / psDecInfo->u.seqInfo.ui32FrameRateDiv;

					if( ui32FrameRate > 45 )
						ui32NFrm = psDecInfo->u.seqInfo.ui32RefFrameCount + 8;
					else if( ui32FrameRate > 30 )
						ui32NFrm = psDecInfo->u.seqInfo.ui32RefFrameCount + 7;
					else
						ui32NFrm = psDecInfo->u.seqInfo.ui32RefFrameCount + 6;

#ifdef USE_ION_ALLOC
					if( gsVdecTop[ui8Vch].Param.bDualDecoding == TRUE )
						ui32NFrm++;
#endif
				}

				ui32FrameSize = ui32BufWidth * ui32BufHeight;
				ui32YFrameSize = (ui32FrameSize + 0x3FFF) & ~0x3FFF;
				ui32CFrameSize = ((ui32FrameSize + 1) /2 + 0x3FFF) & ~0x3FFF;

				gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY = ui32YFrameSize;
				gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb = ui32CFrameSize;
				gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr = 0;

				gsVdecTop[ui8Vch].sVdecDpb.ui32BaseAddr = 0x0;
				gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

				for( ui32NumOfFb = 0; ui32NumOfFb < ui32NFrm; ui32NumOfFb++ )
				{
#ifdef USE_ION_ALLOC
					ui32FrameAddr = ION_Alloc(gsVdecTop[ui8Vch].psIonHandle, ui32YFrameSize + ui32CFrameSize,
											((gsVdecTop[ui8Vch].Param.bDualDecoding == TRUE) && (gsVdecTop[ui8Vch].Param.ui8Vcodec == VDC_CODEC_MPEG2_HP)) ? TRUE : FALSE);
#else
					ui32FrameAddr = VDEC_DPB_Alloc(ui32YFrameSize + ui32CFrameSize);
#endif
					VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] SeqInit: %d, Addr: 0x%X  %s", ui8Vch, gsVdecTop[ui8Vch].State,
													gsVdecTop[ui8Vch].Status.bSeqInit, ui32FrameAddr, __FUNCTION__);

					if( ui32FrameAddr == 0x0 )
					{
						VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to kmalloc DPB, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
						break;
					}

					gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY = ui32FrameAddr;
					gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrCb = ui32FrameAddr + ui32YFrameSize;
					gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrCr = 0x0;
					gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = ui32NumOfFb + 1;
				}

				if( ui32NumOfFb != ui32NFrm )
				{
					VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to kmalloc DPB, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);

#ifdef USE_ION_ALLOC
					ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
					gsVdecTop[ui8Vch].psIonHandle = ION_Init();
					if( gsVdecTop[ui8Vch].psIonHandle == NULL )
					{
						VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
						mutex_unlock(&stVdecKtopLock);
						return;
					}
#else
					for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
					{
						VDEC_DPB_Free( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY );
					}
#endif
					gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

					psDecInfo->u.seqInfo.u32IsSuccess = 0;
				}

				if( VDC_RegisterDPB(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, &gsVdecTop[ui8Vch].sVdecDpb, ui32BufWidth) == FALSE )
				{
					VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to Register DPB, %s", ui8Vch, __FUNCTION__);

#ifdef USE_ION_ALLOC
					ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
					gsVdecTop[ui8Vch].psIonHandle = ION_Init();
					if( gsVdecTop[ui8Vch].psIonHandle == NULL )
					{
						VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
						mutex_unlock(&stVdecKtopLock);
						return;
					}
#else
					for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
					{
						VDEC_DPB_Free( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY );
					}
#endif
					gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

					psDecInfo->u.seqInfo.u32IsSuccess = 0;
				}

				gsVdecTop[ui8Vch].Status.bEndOfDPB = (gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb) ? TRUE : FALSE;
			}

			VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG] Seq Success:0x%x, FrmCnt:%d/%d, Base:0x%X, Size:0x%X+0x%X+0x%X, FrameRate:%d/%d", ui8Vch,
										psDecInfo->u.seqInfo.u32IsSuccess,
										psDecInfo->u.seqInfo.ui32RefFrameCount, gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb,
										gsVdecTop[ui8Vch].sVdecDpb.ui32BaseAddr,
										gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY, gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb, gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr,
										psDecInfo->u.seqInfo.ui32FrameRateRes, psDecInfo->u.seqInfo.ui32FrameRateDiv);
		}

		break;

	case KDRV_NOTI_DECINFO_UPDATE_HDR_PICINFO :

		psDecInfo->u.picInfo.si32FrameFd = 0;

		switch( gsVdecTop[ui8Vch].Param.eOutDst )
		{
		case VDEC_DST_DE0 :
		case VDEC_DST_DE1 :
			break;
		case VDEC_DST_GRP_BUFF0 :
		case VDEC_DST_GRP_BUFF1 :
			psDecInfo->u.picInfo.si32FrameFd = _VDEC_KTOP_FindFrameFd(ui8Vch, psDecInfo->u.picInfo.ui32YFrameAddr);

			if( gui32GenId != psDecInfo->u.picInfo.ui32FlushAge )
			{
				VDC_DEC_PutUsedFrame(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, psDecInfo->u.picInfo.ui32YFrameAddr);
				VDC_FeedKick();

				VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG] Flush Transition: %d --> %d, UID:%d, PTS:0x%X, %s", ui8Vch,
											psDecInfo->u.picInfo.ui32FlushAge, gui32GenId,
											psDecInfo->u.picInfo.sPTS.ui32uID, psDecInfo->u.picInfo.sPTS.ui32PTS, __FUNCTION__ );
				mutex_unlock(&stVdecKtopLock);
				return;
			}
			break;
		case VDEC_DST_BUFF :
			VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG] Dst: Buffer, YFrameAddr:0x%X, %s", ui8Vch, psDecInfo->u.picInfo.ui32YFrameAddr, __FUNCTION__ );
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eOutDst:%d, %s", ui8Vch, gsVdecTop[ui8Vch].Param.eOutDst, __FUNCTION__ );
			break;
		}

		break;

	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eCmdHdr:%d, %s", ui8Vch, psDecInfo->eCmdHdr, __FUNCTION__ );
		break;
	}

	VDEC_NOTI_UpdateDecodeInfo(ui8Vch, psDecInfo);

	mutex_unlock(&stVdecKtopLock);
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
static void _VDEC_KTOP_Wq_UpdateDisplayInfo(UINT8 ui8Vch, S_VDEC_NOTI_CALLBACK_DISPINFO_T *psDispInfo)
{
	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	switch( gsVdecTop[ui8Vch].Param.eInSrc )
	{
	case VDEC_SRC_SDEC0 :
	case VDEC_SRC_SDEC1 :
	case VDEC_SRC_SDEC2 :
	case VDEC_SRC_TVP :
	case VDEC_SRC_BUFF0 :
	case VDEC_SRC_BUFF1 :
		psDispInfo->si32FrameFd = 0;
		break;
	case VDEC_SRC_GRP_BUFF0 :
	case VDEC_SRC_GRP_BUFF1 :
		psDispInfo->si32FrameFd = _VDEC_KTOP_FindFrameFd(ui8Vch, psDispInfo->ui32FrameAddr);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eInSrc:%d, %s", ui8Vch, gsVdecTop[ui8Vch].Param.eInSrc, __FUNCTION__ );
		break;
	}

	VDEC_NOTI_UpdateDisplayInfo(ui8Vch, psDispInfo);

	mutex_unlock(&stVdecKtopLock);
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
static void _VDEC_KTOP_CallBack_workfunc(struct work_struct *data)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;
	UINT8	ui8Vch;

	while( _VDEC_KTOP_GetWqNode(&sWqNodeInfo) == TRUE )
	{
		ui8Vch = sWqNodeInfo.ui8Ch;

		if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
			continue;
		}

		switch( sWqNodeInfo.eNotiHdr )
		{
		case VDEC_KDRV_CB_WQ_ID_INPUT :
			_VDEC_KTOP_Wq_UpdateInputInfo(ui8Vch, &sWqNodeInfo.u.sInputInfo);
			break;
		case VDEC_KDRV_CB_WQ_ID_FEED :
			_VDEC_KTOP_Wq_UpdateFeedInfo(ui8Vch, &sWqNodeInfo.u.sFeedInfo);
			break;
		case VDEC_KDRV_CB_WQ_ID_DEC :
			_VDEC_KTOP_Wq_UpdateDecodeInfo(ui8Vch, &sWqNodeInfo.u.sDecInfo);
			break;
		case VDEC_KDRV_CB_WQ_ID_DISP :
			_VDEC_KTOP_Wq_UpdateDisplayInfo(ui8Vch, &sWqNodeInfo.u.sDispInfo);
			break;
		case VDEC_KDRV_CB_WQ_ID_FLUSH :
			VES_Flush(gsVdecTop[ui8Vch].ChNum.ui8VesCh);
			break;
		case VDEC_KDRV_CB_WQ_ID_RESET :
			VDEC_KTOP_Reset(ui8Vch);
			break;
		case VDEC_KDRV_CB_WQ_ID_UPDATE_ES :
			if( VDEC_KTOP_UpdateCompressedBuffer(ui8Vch,
									VDEC_KTOP_VAU_SEQH,
									sWqNodeInfo.u.sEsInfo.ui32Addr,
									sWqNodeInfo.u.sEsInfo.ui32Size,
									(fpCpbCopyfunc)memcpy,
									0 /*ioUpdateBuffer.ui32uID*/ ,
									0 /*ioUpdateBuffer.ui64TimeStamp*/,
									0 /*ui32TimeStamp_90kHzTick*/,
									0 /*ioUpdateBuffer.frRes*/,
									0 /*ioUpdateBuffer.frDiv*/,
									0 /*ioUpdateBuffer.width*/,
									0 /*ioUpdateBuffer.height*/) == FALSE )
			{
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Seq Init Update Fail, %s", ui8Vch, __FUNCTION__ );
			}
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Callback Noti Hdr, %s", ui8Vch, __FUNCTION__ );
			break;
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
static void _VDEC_KTOP_Cb_UpdateInputInfo(S_VES_CALLBACK_BODY_ESINFO_T *pInputInfo)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;
	S_VDC_AU_T	sVesSeqAu;
	UINT8		ui8VdcCh;
	UINT8		ui8SyncCh;

	sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_INPUT;
	sWqNodeInfo.ui8Ch = pInputInfo->ui32Ch;
	sWqNodeInfo.u.sInputInfo.ui32AuType = pInputInfo->ui32AuType;
	sWqNodeInfo.u.sInputInfo.ui32AuStartAddr = pInputInfo->ui32AuStartAddr;
	sWqNodeInfo.u.sInputInfo.pui8VirStartPtr = pInputInfo->pui8VirStartPtr;
	sWqNodeInfo.u.sInputInfo.ui32AuEndAddr = pInputInfo->ui32AuEndAddr;
	sWqNodeInfo.u.sInputInfo.ui32AuSize = pInputInfo->ui32AuSize;
	sWqNodeInfo.u.sInputInfo.ui32DTS = pInputInfo->ui32DTS;
	sWqNodeInfo.u.sInputInfo.ui32PTS = pInputInfo->ui32PTS;
	sWqNodeInfo.u.sInputInfo.ui64TimeStamp = pInputInfo->ui64TimeStamp;
	sWqNodeInfo.u.sInputInfo.ui32uID = pInputInfo->ui32uID;
	sWqNodeInfo.u.sInputInfo.ui32InputGSTC = pInputInfo->ui32InputGSTC;
	sWqNodeInfo.u.sInputInfo.ui32Width = pInputInfo->ui32Width;
	sWqNodeInfo.u.sInputInfo.ui32Height = pInputInfo->ui32Height;
	sWqNodeInfo.u.sInputInfo.ui32FrameRateRes_Parser = pInputInfo->ui32FrameRateRes_Parser;
	sWqNodeInfo.u.sInputInfo.ui32FrameRateDiv_Parser = pInputInfo->ui32FrameRateDiv_Parser;
	sWqNodeInfo.u.sInputInfo.ui32CpbOccupancy = pInputInfo->ui32CpbOccupancy;
	sWqNodeInfo.u.sInputInfo.ui32CpbSize = pInputInfo->ui32CpbSize;

	ui8VdcCh = gsVdecTop[pInputInfo->ui32Ch].ChNum.ui8VdcCh;
	ui8SyncCh = gsVdecTop[pInputInfo->ui32Ch].ChNum.ui8SyncCh;

	if( (gsVdecTop[pInputInfo->ui32Ch].Param.bUseGstc == TRUE) && (pInputInfo->ui32AuType >= VES_AU_PICTURE_I) )
	{
		UINT32		ui32BaseTime, ui32BasePTS;

		SYNC_Get_BaseTime(ui8SyncCh, &ui32BaseTime, &ui32BasePTS);
		if( ui32BaseTime == 0xFFFFFFFF )
		{
			ui32BaseTime = pInputInfo->ui32InputGSTC & 0x0FFFFFFF;
			ui32BasePTS = pInputInfo->ui32PTS & 0x0FFFFFFF;

			SYNC_Set_BaseTime(ui8SyncCh, 0xFF, ui32BaseTime, ui32BasePTS);
		}
	}

	sVesSeqAu.eAuType = pInputInfo->ui32AuType;
	sVesSeqAu.ui32AuStartAddr = pInputInfo->ui32AuStartAddr;
	sVesSeqAu.pui8VirStartPtr = pInputInfo->pui8VirStartPtr;
	sVesSeqAu.ui32AuEndAddr = pInputInfo->ui32AuEndAddr;
	sVesSeqAu.ui32AuSize = pInputInfo->ui32AuSize;
	sVesSeqAu.ui32DTS = pInputInfo->ui32DTS;
	sVesSeqAu.ui32PTS = pInputInfo->ui32PTS;
	sVesSeqAu.ui64TimeStamp = pInputInfo->ui64TimeStamp;
	sVesSeqAu.ui32uID = pInputInfo->ui32uID;
	sVesSeqAu.ui32InputGSTC = pInputInfo->ui32InputGSTC;
	sVesSeqAu.ui32FrameRateRes_Parser = pInputInfo->ui32FrameRateRes_Parser;
	sVesSeqAu.ui32FrameRateDiv_Parser = pInputInfo->ui32FrameRateDiv_Parser;
	sVesSeqAu.ui32Width = pInputInfo->ui32Width;
	sVesSeqAu.ui32Height = pInputInfo->ui32Height;

	sWqNodeInfo.u.sInputInfo.bRet = VDC_UpdateAuQ(ui8VdcCh, &sVesSeqAu);
	sWqNodeInfo.u.sInputInfo.ui32AuqOccupancy = VDC_AU_Q_Occupancy(ui8VdcCh);

	_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
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
static void _VDEC_KTOP_Cb_UpdateFeedInfo(S_VDC_CALLBACK_BODY_ESINFO_T *pFeedInfo)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;

	VES_UpdateRdPtr(gsVdecTop[pFeedInfo->ui32Ch].ChNum.ui8VesCh, pFeedInfo->ui32AuEndAddr);

	if( pFeedInfo->bFlushed == TRUE )
		return;

	sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_FEED;
	sWqNodeInfo.ui8Ch = pFeedInfo->ui32Ch;
	sWqNodeInfo.u.sFeedInfo.bRet = pFeedInfo->bRet;
	sWqNodeInfo.u.sFeedInfo.ui32AuType = pFeedInfo->ui32AuType;
	sWqNodeInfo.u.sFeedInfo.ui32AuStartAddr = pFeedInfo->ui32AuStartAddr;
	sWqNodeInfo.u.sFeedInfo.pui8VirStartPtr = pFeedInfo->pui8VirStartPtr;
	sWqNodeInfo.u.sFeedInfo.ui32AuEndAddr = pFeedInfo->ui32AuEndAddr;
	sWqNodeInfo.u.sFeedInfo.ui32AuSize = pFeedInfo->ui32AuSize;
	sWqNodeInfo.u.sFeedInfo.ui32DTS = pFeedInfo->ui32DTS;
	sWqNodeInfo.u.sFeedInfo.ui32PTS = pFeedInfo->ui32PTS;
	sWqNodeInfo.u.sFeedInfo.ui64TimeStamp = pFeedInfo->ui64TimeStamp;
	sWqNodeInfo.u.sFeedInfo.ui32uID = pFeedInfo->ui32uID;
	sWqNodeInfo.u.sFeedInfo.ui32Width = pFeedInfo->ui32Width;
	sWqNodeInfo.u.sFeedInfo.ui32Height = pFeedInfo->ui32Height;
	sWqNodeInfo.u.sFeedInfo.ui32FrameRateRes_Parser = pFeedInfo->ui32FrameRateRes_Parser;
	sWqNodeInfo.u.sFeedInfo.ui32FrameRateDiv_Parser = pFeedInfo->ui32FrameRateDiv_Parser;
	sWqNodeInfo.u.sFeedInfo.ui32AuqOccupancy = pFeedInfo->ui32AuqOccupancy;
	sWqNodeInfo.u.sFeedInfo.ui32CpbOccupancy = VES_GetUsedBuffer(gsVdecTop[pFeedInfo->ui32Ch].ChNum.ui8VesCh);
	sWqNodeInfo.u.sFeedInfo.ui32CpbSize = VES_GetBufferSize(gsVdecTop[pFeedInfo->ui32Ch].ChNum.ui8VesCh);

	_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
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
static void _VDEC_KTOP_Cb_UpdateDecodeInfo(S_VDC_CALLBACK_BODY_DECINFO_T *pDecInfo)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;

	sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_DEC;
	sWqNodeInfo.ui8Ch = pDecInfo->ui8Vch;
	sWqNodeInfo.u.sDecInfo.u32CodecType_Config = pDecInfo->u32CodecType_Config;

	switch( pDecInfo->eCmdHdr )
	{
	case VDC_DECINFO_UPDATE_HDR_SEQINFO :
		sWqNodeInfo.u.sDecInfo.eCmdHdr = KDRV_NOTI_DECINFO_UPDATE_HDR_SEQINFO;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32IsSuccess = pDecInfo->u.seqInfo.u32IsSuccess;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32Profile = pDecInfo->u.seqInfo.u32Profile;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32Level = pDecInfo->u.seqInfo.u32Level;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32Aspectratio = pDecInfo->u.seqInfo.u32Aspectratio;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.ui32uID = pDecInfo->u.seqInfo.ui32uID;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32PicWidth = pDecInfo->u.seqInfo.u32PicWidth;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.u32PicHeight = pDecInfo->u.seqInfo.u32PicHeight;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.ui32FrameRateRes = pDecInfo->u.seqInfo.ui32FrameRateRes;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.ui32FrameRateDiv = pDecInfo->u.seqInfo.ui32FrameRateDiv;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.ui32RefFrameCount = pDecInfo->u.seqInfo.ui32RefFrameCount;
		sWqNodeInfo.u.sDecInfo.u.seqInfo.ui32ProgSeq = pDecInfo->u.seqInfo.ui32ProgSeq;
		break;

	case VDC_DECINFO_UPDATE_HDR_PICINFO :
		sWqNodeInfo.u.sDecInfo.eCmdHdr = KDRV_NOTI_DECINFO_UPDATE_HDR_PICINFO;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32DecodingSuccess = pDecInfo->u.picInfo.u32DecodingSuccess;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32YFrameAddr = pDecInfo->u.picInfo.ui32YFrameAddr;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32CFrameAddr = pDecInfo->u.picInfo.ui32CFrameAddr;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32BufStride = pDecInfo->u.picInfo.u32BufStride;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32Aspectratio = pDecInfo->u.picInfo.u32Aspectratio;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32PicWidth = pDecInfo->u.picInfo.u32PicWidth;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32PicHeight = pDecInfo->u.picInfo.u32PicHeight;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32CropLeftOffset = pDecInfo->u.picInfo.u32CropLeftOffset;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32CropRightOffset = pDecInfo->u.picInfo.u32CropRightOffset;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32CropTopOffset = pDecInfo->u.picInfo.u32CropTopOffset;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32CropBottomOffset = pDecInfo->u.picInfo.u32CropBottomOffset;
		sWqNodeInfo.u.sDecInfo.u.picInfo.eDisplayInfo = pDecInfo->u.picInfo.eDisplayInfo;
		sWqNodeInfo.u.sDecInfo.u.picInfo.bFieldPicture = pDecInfo->u.picInfo.bFieldPicture;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32DisplayPeriod = pDecInfo->u.picInfo.ui32DisplayPeriod;
		sWqNodeInfo.u.sDecInfo.u.picInfo.si32FrmPackArrSei = pDecInfo->u.picInfo.si32FrmPackArrSei;
		sWqNodeInfo.u.sDecInfo.u.picInfo.eLR_Order = pDecInfo->u.picInfo.eLR_Order;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui16ParW = pDecInfo->u.picInfo.ui16ParW;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui16ParH = pDecInfo->u.picInfo.ui16ParH;

		sWqNodeInfo.u.sDecInfo.u.picInfo.sUsrData.ui32PicType = pDecInfo->u.picInfo.sUsrData.ui32PicType;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sUsrData.ui32Rpt_ff = pDecInfo->u.picInfo.sUsrData.ui32Rpt_ff;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sUsrData.ui32Top_ff = pDecInfo->u.picInfo.sUsrData.ui32Top_ff;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sUsrData.ui32BuffAddr = pDecInfo->u.picInfo.sUsrData.ui32BuffAddr;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sUsrData.ui32Size = pDecInfo->u.picInfo.sUsrData.ui32Size;

		sWqNodeInfo.u.sDecInfo.u.picInfo.sDTS.ui32DTS = pDecInfo->u.picInfo.sDTS.ui32DTS;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sDTS.ui64TimeStamp = pDecInfo->u.picInfo.sDTS.ui64TimeStamp;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sDTS.ui32uID = pDecInfo->u.picInfo.sDTS.ui32uID;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sPTS.ui32PTS = pDecInfo->u.picInfo.sPTS.ui32PTS;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sPTS.ui64TimeStamp = pDecInfo->u.picInfo.sPTS.ui64TimeStamp;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sPTS.ui32uID = pDecInfo->u.picInfo.sPTS.ui32uID;

		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32InputGSTC = pDecInfo->u.picInfo.ui32InputGSTC;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32FeedGSTC = pDecInfo->u.picInfo.ui32FeedGSTC;
		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32FlushAge = pDecInfo->u.picInfo.ui32FlushAge;

		sWqNodeInfo.u.sDecInfo.u.picInfo.u32NumOfErrMBs = pDecInfo->u.picInfo.u32NumOfErrMBs;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32PicType = pDecInfo->u.picInfo.u32PicType;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32ActiveFMT = pDecInfo->u.picInfo.u32ActiveFMT;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32ProgSeq = pDecInfo->u.picInfo.u32ProgSeq;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32ProgFrame = pDecInfo->u.picInfo.u32ProgFrame;
		sWqNodeInfo.u.sDecInfo.u.picInfo.u32LowDelay = pDecInfo->u.picInfo.u32LowDelay;
		sWqNodeInfo.u.sDecInfo.u.picInfo.bVariableFrameRate = pDecInfo->u.picInfo.bVariableFrameRate;

		sWqNodeInfo.u.sDecInfo.u.picInfo.sFrameRate_Parser.ui32Residual = pDecInfo->u.picInfo.sFrameRate_Parser.ui32Residual;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sFrameRate_Parser.ui32Divider = pDecInfo->u.picInfo.sFrameRate_Parser.ui32Divider;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sFrameRate_Decoder.ui32Residual = pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Residual;
		sWqNodeInfo.u.sDecInfo.u.picInfo.sFrameRate_Decoder.ui32Divider = pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Divider;

		sWqNodeInfo.u.sDecInfo.u.picInfo.ui32DqOccupancy = pDecInfo->u.picInfo.ui32DqOccupancy;

		switch( gsVdecTop[pDecInfo->ui8Vch].Param.eOutDst )
		{
		case VDEC_DST_DE0 :
		case VDEC_DST_DE1 :
		{
			S_FRAME_BUF_T sFrameBuf;
			BOOLEAN	bInterlaced = TRUE;
			UINT8	ui8SyncCh = gsVdecTop[pDecInfo->ui8Vch].ChNum.ui8SyncCh;

			switch( pDecInfo->u.picInfo.eDisplayInfo )
			{
			case DISPQ_DISPLAY_INFO_TOP_FIELD_FIRST :
			case DISPQ_DISPLAY_INFO_BOTTOM_FIELD_FIRST :
				if( (pDecInfo->u.picInfo.ui32DisplayPeriod == 1) || (pDecInfo->u.picInfo.ui32DisplayPeriod >= 4) )
					VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Display Period: %d of Field Picture", pDecInfo->ui8Vch, pDecInfo->u.picInfo.ui32DisplayPeriod );

				bInterlaced = TRUE;
				break;
			case DISPQ_DISPLAY_INFO_PROGRESSIVE_FRAME :
				bInterlaced = FALSE;
				break;
			default :
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Display Info:%d, %s", pDecInfo->ui8Vch, pDecInfo->u.picInfo.eDisplayInfo, __FUNCTION__ );
			}

			SYNC_UpdateRate(ui8SyncCh,
								pDecInfo->u.picInfo.sFrameRate_Parser.ui32Residual, pDecInfo->u.picInfo.sFrameRate_Parser.ui32Divider,
								pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Residual, pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Divider,
								pDecInfo->u.picInfo.sDTS.ui32DTS,
								pDecInfo->u.picInfo.sPTS.ui32PTS,
								pDecInfo->u.picInfo.ui32InputGSTC,
								pDecInfo->u.picInfo.ui32FeedGSTC,
								pDecInfo->u.picInfo.bFieldPicture,
								bInterlaced,
								pDecInfo->u.picInfo.bVariableFrameRate,
								pDecInfo->u.picInfo.ui32FlushAge);

			if( SYNC_GetFrameRateResDiv(ui8SyncCh, &pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Residual, &pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Divider) == 0 )
			{
				SYNC_GetDecodingRate(ui8SyncCh, &pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Residual, &pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Divider);
			}

			sFrameBuf.ui32YFrameAddr = pDecInfo->u.picInfo.ui32YFrameAddr;
			sFrameBuf.ui32CFrameAddr = pDecInfo->u.picInfo.ui32CFrameAddr;
			sFrameBuf.ui32Stride = pDecInfo->u.picInfo.u32BufStride;

			sFrameBuf.sDTS.ui32DTS = pDecInfo->u.picInfo.sDTS.ui32DTS;
			sFrameBuf.sDTS.ui64TimeStamp = pDecInfo->u.picInfo.sDTS.ui64TimeStamp;
			sFrameBuf.sDTS.ui32uID = pDecInfo->u.picInfo.sDTS.ui32uID;
			sFrameBuf.sPTS.ui32PTS = pDecInfo->u.picInfo.sPTS.ui32PTS;
			sFrameBuf.sPTS.ui64TimeStamp = pDecInfo->u.picInfo.sPTS.ui64TimeStamp;
			sFrameBuf.sPTS.ui32uID = pDecInfo->u.picInfo.sPTS.ui32uID;

			sFrameBuf.ui32InputGSTC = pDecInfo->u.picInfo.ui32InputGSTC;
			sFrameBuf.ui32FlushAge = pDecInfo->u.picInfo.ui32FlushAge;

			sFrameBuf.ui32AspectRatio = pDecInfo->u.picInfo.u32Aspectratio;
			sFrameBuf.ui32PicWidth = pDecInfo->u.picInfo.u32PicWidth;
			sFrameBuf.ui32PicHeight = pDecInfo->u.picInfo.u32PicHeight;
			sFrameBuf.ui32CropLeftOffset = pDecInfo->u.picInfo.u32CropLeftOffset;
			sFrameBuf.ui32CropRightOffset = pDecInfo->u.picInfo.u32CropRightOffset;
			sFrameBuf.ui32CropTopOffset = pDecInfo->u.picInfo.u32CropTopOffset;
			sFrameBuf.ui32CropBottomOffset = pDecInfo->u.picInfo.u32CropBottomOffset;
			sFrameBuf.eDisplayInfo = pDecInfo->u.picInfo.eDisplayInfo;
			sFrameBuf.ui32DisplayPeriod = pDecInfo->u.picInfo.ui32DisplayPeriod;
			sFrameBuf.si32FrmPackArrSei = pDecInfo->u.picInfo.si32FrmPackArrSei;
			sFrameBuf.eLR_Order = pDecInfo->u.picInfo.eLR_Order;
			sFrameBuf.ui16ParW = pDecInfo->u.picInfo.ui16ParW;
			sFrameBuf.ui16ParH = pDecInfo->u.picInfo.ui16ParH;

			sFrameBuf.sUsrData.ui32Size = pDecInfo->u.picInfo.sUsrData.ui32Size;
			if( pDecInfo->u.picInfo.sUsrData.ui32Size )
			{
				sFrameBuf.sUsrData.ui32PicType = pDecInfo->u.picInfo.sUsrData.ui32PicType;
				sFrameBuf.sUsrData.ui32Rpt_ff = pDecInfo->u.picInfo.sUsrData.ui32Rpt_ff;
				sFrameBuf.sUsrData.ui32Top_ff = pDecInfo->u.picInfo.sUsrData.ui32Top_ff;
				sFrameBuf.sUsrData.ui32BuffAddr =pDecInfo->u.picInfo.sUsrData.ui32BuffAddr;
			}
#if 0
			ui32FirstData = *(UINT32 *)VDEC_DPB_TranslatePhyToVirtual(pDecInfo->u.picInfo.ui32YFrameAddr);
			if( (ui32FirstData == 0x11111111) || (ui32FirstData != gsVdecTop[pDecInfo->ui8Vch].Status.ui32FirstData) )
			{
				VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG] Y Data: 0x%X, DTS: 0x%X, PTS: 0x%X, %s", pDecInfo->ui8Vch, ui32FirstData, pDecInfo->u.picInfo.sDTS.ui32DTS, pDecInfo->u.picInfo.sPTS.ui32PTS, __FUNCTION__ );
			}
			gsVdecTop[pDecInfo->ui8Vch].Status.ui32FirstData = ui32FirstData;
#endif
			SYNC_UpdateFrame(ui8SyncCh, &sFrameBuf);

			pDecInfo->u.picInfo.sUsrData.ui32Size = 0;
		}
			break;
		case VDEC_DST_GRP_BUFF0 :
		case VDEC_DST_GRP_BUFF1 :
			break;
		case VDEC_DST_BUFF :
			VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG] Dst: Buffer, YFrameAddr:0x%X, %s", pDecInfo->ui8Vch, pDecInfo->u.picInfo.ui32YFrameAddr, __FUNCTION__ );
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eOutDst:%d, %s", pDecInfo->ui8Vch, gsVdecTop[pDecInfo->ui8Vch].Param.eOutDst, __FUNCTION__ );
			break;
		}

//		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][DBG] YFrameAddr:0x%X, FrameFd:%d, %s", pDecInfo->ui8Vch, pDecInfo->u.picInfo.ui32YFrameAddr, pDecInfo->u.picInfo.si32FrameFd, __FUNCTION__ );

		gsVdecTop[pDecInfo->ui8Vch].Debug.ui32LogTick++;
		if( gsVdecTop[pDecInfo->ui8Vch].Debug.ui32LogTick >= 0x100 )
		{
			UINT32			ui32ElapseTime = 0;
			UINT32			ui32DecodeGSTC = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;

			if( pDecInfo->u.picInfo.ui32InputGSTC != 0x80000000 )
				ui32ElapseTime = (ui32DecodeGSTC >= pDecInfo->u.picInfo.ui32InputGSTC) ? ui32DecodeGSTC - pDecInfo->u.picInfo.ui32InputGSTC : ui32DecodeGSTC + 0x80000000 - pDecInfo->u.picInfo.ui32InputGSTC;

			VDEC_KDRV_Message(MONITOR, "[VDEC_K%d][DBG] Decode Latency: %dms",	pDecInfo->ui8Vch, ui32ElapseTime/90);

			gsVdecTop[pDecInfo->ui8Vch].Debug.ui32LogTick = 0;
		}
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eCmdHdr:%d , %s", pDecInfo->ui8Vch, pDecInfo->eCmdHdr, __FUNCTION__ );
		break;
	}

	_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
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
static void _VDEC_KTOP_Cb_UpdateDisplayInfo(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;

	sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_DISP;
	sWqNodeInfo.ui8Ch = pDispInfo->ui32Ch;
	sWqNodeInfo.u.sDispInfo.ui32NumDisplayed = pDispInfo->ui32NumDisplayed;
	sWqNodeInfo.u.sDispInfo.ui32DqOccupancy = pDispInfo->ui32DqOccupancy;
	sWqNodeInfo.u.sDispInfo.bPtsMatched = pDispInfo->bPtsMatched;
	sWqNodeInfo.u.sDispInfo.bDropped = pDispInfo->bDropped;
	sWqNodeInfo.u.sDispInfo.ui32PicWidth = pDispInfo->ui32PicWidth;
	sWqNodeInfo.u.sDispInfo.ui32PicHeight = pDispInfo->ui32PicHeight;
	sWqNodeInfo.u.sDispInfo.ui32uID = pDispInfo->ui32uID;
	sWqNodeInfo.u.sDispInfo.ui64TimeStamp = pDispInfo->ui64TimeStamp;
	sWqNodeInfo.u.sDispInfo.ui32DisplayedPTS = pDispInfo->ui32DisplayedPTS;
	sWqNodeInfo.u.sDispInfo.sUsrData.ui32Size = pDispInfo->sUsrData.ui32Size;
	sWqNodeInfo.u.sDispInfo.sUsrData.ui32PicType = pDispInfo->sUsrData.ui32PicType;
	sWqNodeInfo.u.sDispInfo.sUsrData.ui32Rpt_ff = pDispInfo->sUsrData.ui32Rpt_ff;
	sWqNodeInfo.u.sDispInfo.sUsrData.ui32Top_ff = pDispInfo->sUsrData.ui32Top_ff;
	sWqNodeInfo.u.sDispInfo.sUsrData.ui32BuffAddr = pDispInfo->sUsrData.ui32BuffAddr;
	sWqNodeInfo.u.sDispInfo.ui32FrameAddr = pDispInfo->ui32FrameAddr;

	switch( gsVdecTop[pDispInfo->ui32Ch].Param.eInSrc )
	{
	case VDEC_SRC_SDEC0 :
	case VDEC_SRC_SDEC1 :
	case VDEC_SRC_SDEC2 :
	case VDEC_SRC_TVP :
	case VDEC_SRC_BUFF0 :
	case VDEC_SRC_BUFF1 :
		VDC_DEC_PutUsedFrame(gsVdecTop[pDispInfo->ui32Ch].ChNum.ui8VdcCh, pDispInfo->ui32FrameAddr);
		VDC_FeedKick();
		break;
	case VDEC_SRC_GRP_BUFF0 :
	case VDEC_SRC_GRP_BUFF1 :
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] eInSrc:%d, %s", pDispInfo->ui32Ch, gsVdecTop[pDispInfo->ui32Ch].Param.eInSrc, __FUNCTION__ );
		break;
	}

	_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
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
static void _VDEC_KTOP_Cb_UpdateCpbStatus(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;

	VDEC_KDRV_Message(ERROR, "[VDEC_K%d][DBG] CPB Status: %d, %s", pCpbStatus->ui8Vch, pCpbStatus->eBufStatus, __FUNCTION__ );

	switch( pCpbStatus->eBufStatus )
	{
	case CPB_STATUS_ALMOST_FULL :
		sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_FLUSH;
		sWqNodeInfo.ui8Ch = pCpbStatus->ui8Vch;
		_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
		break;
	case CPB_STATUS_NORMAL :
	case CPB_STATUS_ALMOST_EMPTH :
	default :
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
void _VDEC_KTOP_Cb_UpdateDqStatus(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus)
{
	VDEC_KDRV_Message(ERROR, "[VDEC_K%d][DBG] DQ Status: %d, %s", pDqStatus->ui8Vch, pDqStatus->eBufStatus, __FUNCTION__ );
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
static void _VDEC_KTOP_Cb_RequestReset(S_VDC_CALLBACK_BODY_REQUESTRESET_T *pReqCmd)
{
	S_VDEC_CALLBACK_WQ_NODE_T sWqNodeInfo;

	VDEC_KDRV_Message(ERROR, "[VDEC_K%d][DBG] Request Reset Command(%d): 0x%X++0x%0X %s"
							, pReqCmd->ui8Vch
							, pReqCmd->bReset
							, pReqCmd->ui32Addr
							, pReqCmd->ui32Size
							, __FUNCTION__ );

	if( pReqCmd->bReset == TRUE )
	{
		sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_RESET;
		sWqNodeInfo.ui8Ch = pReqCmd->ui8Vch;
		_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
	}

	if( pReqCmd->ui32Addr && pReqCmd->ui32Size )
	{
		sWqNodeInfo.eNotiHdr = VDEC_KDRV_CB_WQ_ID_UPDATE_ES;
		sWqNodeInfo.ui8Ch = pReqCmd->ui8Vch;
		sWqNodeInfo.u.sEsInfo.ui32Addr = pReqCmd->ui32Addr;
		sWqNodeInfo.u.sEsInfo.ui32Size = pReqCmd->ui32Size;
		_VDEC_KTOP_PutWqNode(&sWqNodeInfo);
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
void VDEC_KTOP_Init(void)
{
	int ui8Vch;

	VDEC_KDRV_Message(_TRACE, "[VDEC_K][DBG] %s", __FUNCTION__ );

	mutex_init(&stVdecKtopLock);
	_VDEC_KTOP_CallBack_workqueue = create_workqueue("VDEC_NOTI_CB");
	spin_lock_init(&gsVdecKtopCallbackQ.stWrlock);
	gsVdecKtopCallbackQ.ui32WrIndex = 0;
	gsVdecKtopCallbackQ.ui32RdIndex = 0;

	// HAL_Init
	TOP_HAL_Init();
	MCU_HAL_Init();
	PDEC_HAL_Init();
	LQ_HAL_Init();
	VSync_HAL_Init();
	DE_IPC_HAL_Init();
	IPC_REG_Init();
	AV_LipSync_HAL_Init();
	VDEC_SHM_Init(VDEC_SHM_BASE, VDEC_SHM_SIZE);

	IPC_DBG_Init();
	IPC_CMD_Init();
	IPC_REQ_Init();
	RAM_LOG_Init();

	for ( ui8Vch=0; ui8Vch<VDEC_NUM_OF_CHANNEL; ui8Vch++ )
	{
		gsVdecTop[ui8Vch].State = VDEC_KTOP_NULL;

		gsVdecTop[ui8Vch].Param.ui8Vcodec = 0xFF;

		gsVdecTop[ui8Vch].Param.bNoAdpStr = FALSE;
		gsVdecTop[ui8Vch].Param.bForThumbnail = FALSE;

		gsVdecTop[ui8Vch].Param.ui32DecodeOffset_bytes = 0;
		gsVdecTop[ui8Vch].Param.b512bytesAligned = FALSE;

		gsVdecTop[ui8Vch].Param.ui32ErrorMBFilter = 0xFFFFFFFF;
		gsVdecTop[ui8Vch].Param.ui32Width = VDEC_KDRV_WIDTH_INVALID;
		gsVdecTop[ui8Vch].Param.ui32Height = VDEC_KDRV_HEIGHT_INVALID;

		gsVdecTop[ui8Vch].Param.bDualDecoding = FALSE;
		gsVdecTop[ui8Vch].Param.bLowLatency = FALSE;

		gsVdecTop[ui8Vch].Param.bUseGstc = FALSE;
		gsVdecTop[ui8Vch].Param.ui32DisplayOffset_ms = 0;
		gsVdecTop[ui8Vch].Param.ui8ClkID = 0xFF;

		gsVdecTop[ui8Vch].ChNum.ui8VesCh = 0xFF;
		gsVdecTop[ui8Vch].ChNum.ui8VdcCh = 0xFF;
		gsVdecTop[ui8Vch].ChNum.ui8SyncCh = 0xFF;

		gsVdecTop[ui8Vch].Status.ui32PicWidth = 0;
		gsVdecTop[ui8Vch].Status.ui32Height = 0;

		gsVdecTop[ui8Vch].Status.bSeqInit = FALSE;
		gsVdecTop[ui8Vch].Status.bEndOfDPB = FALSE;
		gsVdecTop[ui8Vch].ui32CpbBufAddr = 0x0;
		gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;
		gsVdecTop[ui8Vch].sVdecDpb.ui32BaseAddr = 0x0;
#ifdef USE_ION_ALLOC
		gsVdecTop[ui8Vch].psIonHandle = NULL;
#endif
	}
	gui32ActiveSrc = 0x0;
	gui32ActiveDst = 0x0;

	VES_Init(_VDEC_KTOP_Cb_UpdateInputInfo);
	VES_AUIB_Init(_VDEC_KTOP_Cb_UpdateCpbStatus);
	VES_CPB_Init(_VDEC_KTOP_Cb_UpdateCpbStatus);

	AU_Q_Init();
	VDC_Init(_VDEC_KTOP_Cb_UpdateFeedInfo, _VDEC_KTOP_Cb_UpdateDecodeInfo, _VDEC_KTOP_Cb_RequestReset);

	VDEC_CPB_Init(VDEC_CPB_BASE, VDEC_CPB_SIZE);
#if (KDRV_PLATFORM != KDRV_COSMO_PLATFORM)
	if(gMemCfgVdec[0].dpb.mem_base != 0x00000000)
	{
		VDEC_DPB_Init(0, gMemCfgVdec[0].dpb.mem_base, VDEC_DPB0_SIZE);
		VDEC_KDRV_Message(ERROR, "[VDEC_K][DDR0 MEM] DPB0:0x%X/0x%X, %s",
							gMemCfgVdec[0].dpb.mem_base, VDEC_DPB0_SIZE, __FUNCTION__ );
	}
#endif

#ifndef USE_ION_ALLOC
	VDEC_DPB_Init(1, VDEC_DPB_BASE, VDEC_DPB_SIZE);
#endif

	DISP_Q_Init();
	SYNC_Init(_VDEC_KTOP_Cb_UpdateDisplayInfo, _VDEC_KTOP_Cb_UpdateDqStatus);

	VDEC_KDRV_Message(ERROR, "[VDEC_K][MEM] SHM:0x%X/0x%X, CPB:0x%X/0x%X, DPB:0x%X/0x%X, %s",
						VDEC_SHM_BASE, VDEC_SHM_SIZE, VDEC_CPB_BASE, VDEC_CPB_SIZE, VDEC_DPB_BASE, VDEC_DPB_SIZE, __FUNCTION__ );
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
UINT8 VDEC_KTOP_Restart(UINT8 ui8Vch)
{
	UINT8 ui8Vcodec;
	VDEC_SRC_T eInSrc;
	VDEC_DST_T eOutDst;
	// KTOP Param
	BOOLEAN bNoAdpStr;
	BOOLEAN bForThumbnail;
	// VES Param
	UINT32 ui32DecodeOffset_bytes;
	BOOLEAN b512bytesAligned;
	// VDC Param
	UINT32 ui32ErrorMBFilter;
	UINT32 ui32Width;
	UINT32 ui32Height;
	// VDC & VDS Param
	BOOLEAN bDualDecoding;
	BOOLEAN bLowLatency;
	// VDS Param
	BOOLEAN bUseGstc;
	UINT32 ui32DisplayOffset_ms;
	UINT8 ui8ClkID;

	UINT8	ui8VesCh;
	UINT8	ui8VdcCh;
	UINT8	ui8VdsCh;

	VDEC_KTOP_State State;

	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Already Closed, Don't need Restart Channel : %s", ui8Vch, __FUNCTION__);
		mutex_unlock(&stVdecKtopLock);
		return 0;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] Restart Channel : %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);

	ui8Vcodec = gsVdecTop[ui8Vch].Param.ui8Vcodec;
	eInSrc = gsVdecTop[ui8Vch].Param.eInSrc;
	eOutDst = gsVdecTop[ui8Vch].Param.eOutDst;

	bNoAdpStr = gsVdecTop[ui8Vch].Param.bNoAdpStr;
	bForThumbnail = gsVdecTop[ui8Vch].Param.bForThumbnail;

	ui32DecodeOffset_bytes = gsVdecTop[ui8Vch].Param.ui32DecodeOffset_bytes;
	b512bytesAligned = gsVdecTop[ui8Vch].Param.b512bytesAligned;

	ui32ErrorMBFilter = gsVdecTop[ui8Vch].Param.ui32ErrorMBFilter;
	ui32Width = gsVdecTop[ui8Vch].Param.ui32Width;
	ui32Height = gsVdecTop[ui8Vch].Param.ui32Height;

	bDualDecoding = gsVdecTop[ui8Vch].Param.bDualDecoding;
	bLowLatency = gsVdecTop[ui8Vch].Param.bLowLatency;

	bUseGstc = gsVdecTop[ui8Vch].Param.bUseGstc;
	ui32DisplayOffset_ms = gsVdecTop[ui8Vch].Param.ui32DisplayOffset_ms;
	ui8ClkID = gsVdecTop[ui8Vch].Param.ui8ClkID;

	State = gsVdecTop[ui8Vch].State;

	gsVdecTop[ui8Vch].Status.bSeqInit = FALSE;
	gsVdecTop[ui8Vch].Status.ui32PicWidth = 0;
	gsVdecTop[ui8Vch].Status.ui32Height = 0;

	mutex_unlock(&stVdecKtopLock);

	if( State != VDEC_KTOP_NULL )
	{
		VDEC_KTOP_Close(ui8Vch);
		VDEC_KTOP_Open(ui8Vch, ui8Vcodec, eInSrc, eOutDst,
							bNoAdpStr, bForThumbnail,
							ui32DecodeOffset_bytes, b512bytesAligned,
							ui32ErrorMBFilter, ui32Width, ui32Height,
							bDualDecoding, bLowLatency,
							bUseGstc, ui32DisplayOffset_ms, ui8ClkID,
							&ui8VesCh, &ui8VdcCh, &ui8VdsCh);
		VDEC_KTOP_Flush(ui8Vch);
	}

	switch( State )
	{
	case VDEC_KTOP_READY :
		break;
	case VDEC_KTOP_PLAY :
		VDEC_KTOP_Play(ui8Vch, 1000);
		break;
	case VDEC_KTOP_PLAY_STEP :
		VDEC_KTOP_Step(ui8Vch);
		break;
	case VDEC_KTOP_PLAY_FREEZE :
		VDEC_KTOP_Freeze(ui8Vch);
		break;
	case VDEC_KTOP_PAUSE :
		VDEC_KTOP_Pause(ui8Vch);
		break;
	case VDEC_KTOP_NULL :
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		break;
	}

	return 0;
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
UINT8 VDEC_KTOP_Open(	UINT8 ui8Vch,
							UINT8 ui8Vcodec,
							VDEC_SRC_T eInSrc,
							VDEC_DST_T eOutDst,
							// KTOP Param
							BOOLEAN bNoAdpStr,
							BOOLEAN bForThumbnail,
							// VES Param
							UINT32 ui32DecodeOffset_bytes,
							BOOLEAN b512bytesAligned,
							// VDC Param
							UINT32 ui32ErrorMBFilter,
							UINT32 ui32Width, UINT32 ui32Height,
							// VDC & VDS Param
							BOOLEAN bDualDecoding,
							BOOLEAN bLowLatency,
							// VDS Param
							BOOLEAN bUseGstc,
							UINT32 ui32DisplayOffset_ms,
							UINT8 ui8ClkID,
							UINT8 *pui8VesCh, UINT8 *pui8VdcCh, UINT8 *pui8VdsCh)
{
	UINT8 ui8VesCh= 0xFF, ui8VdcCh= 0xFF, ui8SyncCh= 0xFF;
	UINT8 ui8SrcBufCh = 0xFF;
	UINT32 ui32CpbBufAddr = 0x0, ui32CpbBufSize = 0x0;
	UINT8 ui8ret = 0;
	BOOLEAN bFixedVSync = FALSE;
	BOOLEAN bSrcBufMode = FALSE;
	E_PTS_MATCH_MODE_T eMatchMode = PTS_MATCH_ENABLE;
	E_SYNC_DST_T eSyncDstCh = DE_IF_DST_INVALID;
	E_VES_SRC_T eVesSrcCh = VES_SRC_INVALID;
	BOOLEAN bRingBufferMode = TRUE;


	mutex_lock(&stVdecKtopLock);

	gui32GenId++;

	if( (gui32ActiveSrc & (1 << eInSrc)) || (gui32ActiveDst & (1 << eOutDst)) )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err][SDEC%d][DE%d] Active Src:0x%x, Dst:0x%x, %s",
									ui8Vch, eInSrc, eOutDst, gui32ActiveSrc, gui32ActiveDst, __FUNCTION__ );
		ui8ret = 2;
		goto error;
	}

	if( gsVdecTop[ui8Vch].State != VDEC_KTOP_NULL )
	{
		ui8ret = 3;
		goto error;
	}

	gsVdecTop[ui8Vch].ui32CpbBufAddr = 0x0;

	switch(eOutDst)
	{
	case VDEC_DST_DE0:
		eSyncDstCh = DE_IF_DST_DE_IF0;
		break;
	case VDEC_DST_DE1:
		eSyncDstCh = DE_IF_DST_DE_IF1;
		break;
	case VDEC_DST_BUFF:
	case VDEC_DST_GRP_BUFF0:
	case VDEC_DST_GRP_BUFF1:
		eSyncDstCh = DE_IF_DST_INVALID;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][ERR] VDEC Output Destination Param %d", ui8Vch, eOutDst);
		break;
	}

	switch(eInSrc)
	{
	case VDEC_SRC_SDEC0:
		eVesSrcCh = VES_SRC_SDEC0;
		break;
	case VDEC_SRC_SDEC1:
		eVesSrcCh = VES_SRC_SDEC1;
		break;
	case VDEC_SRC_SDEC2:
		eVesSrcCh = VES_SRC_SDEC2;
		break;
	case VDEC_SRC_TVP:
		eVesSrcCh = VES_SRC_TVP;
		break;
	case VDEC_SRC_BUFF0:
		eVesSrcCh = VES_SRC_BUFF0;
		break;
	case VDEC_SRC_BUFF1:
		eVesSrcCh = VES_SRC_BUFF1;
		break;
	case VDEC_SRC_GRP_BUFF0:
	case VDEC_SRC_GRP_BUFF1:
		eVesSrcCh = VES_SRC_INVALID;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][ERR] VDEC Input Source Param %d", ui8Vch, eInSrc);
		break;
	}

	// Renderer Open
	if( eSyncDstCh != DE_IF_DST_INVALID )
	{
		if( bUseGstc == TRUE )
		{
			switch( eInSrc )
			{
			case VDEC_SRC_SDEC0 :
			case VDEC_SRC_SDEC1 :
			case VDEC_SRC_SDEC2 :
				ui8SrcBufCh = 0;
				VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] H/W Ves Path, But Use the GSTC, SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, ui8SrcBufCh);
				break;
			case VDEC_SRC_TVP :
				ui8SrcBufCh = 0;
				break;
			case VDEC_SRC_BUFF0 :
			case VDEC_SRC_BUFF1 :
				ui8SrcBufCh = eInSrc - VDEC_SRC_BUFF0;
				break;
			case VDEC_SRC_GRP_BUFF0 :
			case VDEC_SRC_GRP_BUFF1 :
				ui8SrcBufCh = eInSrc - VDEC_SRC_GRP_BUFF0;
				VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] Source FrameBuf, SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, ui8SrcBufCh);
				break;
			default :
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][DBG] SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, eInSrc);
			}
		}

		if( bLowLatency == TRUE )
		{
			eMatchMode = PTS_MATCH_FREERUN_IGNORE_SYNC;
			bFixedVSync = TRUE;
		}

		ui8SyncCh = SYNC_Open(ui8Vch, eSyncDstCh, ui8Vcodec, ui8SrcBufCh, ui32DisplayOffset_ms, 0x20, ui8ClkID, bDualDecoding, bFixedVSync, eMatchMode, gui32GenId);
		if(ui8SyncCh == 0xFF)
		{
			ui8ret = 4;
			goto error;
		}
	}

	// Decoder Open
	if( eVesSrcCh != VES_SRC_INVALID )
	{
		// CPB Alloc
		if( VDEC_CPB_Alloc( &ui32CpbBufAddr, &ui32CpbBufSize, (eInSrc == VDEC_SRC_TVP) ? TRUE : FALSE ) == FALSE )
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Failed to Alloc CPB, %s", ui8Vch, __FUNCTION__ );
			ui8ret = 5;
			goto error;
		}
		gsVdecTop[ui8Vch].ui32CpbBufAddr = ui32CpbBufAddr;

		if( (ui8Vcodec == VDC_CODEC_VC1_RCV_V1) || (ui8Vcodec == VDC_CODEC_VC1_RCV_V2) ||
			(ui8Vcodec == VDC_CODEC_DIVX3) ||
//			(ui8Vcodec == VDC_CODEC_H263) || (ui8Vcodec == VDC_CODEC_XVID) || (ui8Vcodec == VDC_CODEC_DIVX4) || (ui8Vcodec == VDC_CODEC_DIVX5) ||
			(ui8Vcodec == VDC_CODEC_RVX) ||
			(ui8Vcodec == VDC_CODEC_VP8) )
		{
			bRingBufferMode = FALSE;
		}

		if((eInSrc > VDEC_SRC_TVP) && (eInSrc <= VDEC_SRC_BUFF1))
			bSrcBufMode = TRUE;
		else
			bSrcBufMode = FALSE;

		ui8VdcCh = VDC_Open(ui8Vch, ui8Vcodec, ui32CpbBufAddr, ui32CpbBufSize, bRingBufferMode, bSrcBufMode, bForThumbnail, bLowLatency, bDualDecoding, gui32GenId, ui32ErrorMBFilter, ui32Width, ui32Height);
		if( ui8VdcCh == 0xFF )
		{
			ui8ret = 6;
			goto error;
		}

		ui8VesCh = VES_Open(ui8Vch, eVesSrcCh, ui8Vcodec, ui32CpbBufAddr, ui32CpbBufSize, bUseGstc, ui32DecodeOffset_bytes, bRingBufferMode, b512bytesAligned);
		if(ui8VesCh == 0xFF)
		{
			ui8ret = 7;
			goto error;
		}
	}

	if( eInSrc <= VDEC_SRC_SDEC2 )
	{
		TOP_HAL_SetLQInputSelection(ui8SyncCh, ui8VesCh);
		TOP_HAL_EnableLQInput(ui8SyncCh);
	}
	else
	{
		TOP_HAL_DisableLQInput(ui8SyncCh);
	}

#ifdef USE_ION_ALLOC
	gsVdecTop[ui8Vch].psIonHandle = ION_Init();
	if( gsVdecTop[ui8Vch].psIonHandle == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		ui8ret = 1;
		goto error;
	}
#endif

	if( gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb )
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Already Exist DPB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );

	gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;
	gsVdecTop[ui8Vch].sVdecDpb.ui32BaseAddr = 0x0;

	gsVdecTop[ui8Vch].Param.ui8Vcodec = ui8Vcodec;
	gsVdecTop[ui8Vch].Param.eInSrc = eInSrc;
	gsVdecTop[ui8Vch].Param.eOutDst = eOutDst;

	gsVdecTop[ui8Vch].Param.bNoAdpStr = bNoAdpStr;
	gsVdecTop[ui8Vch].Param.bForThumbnail = bForThumbnail;

	gsVdecTop[ui8Vch].Param.ui32DecodeOffset_bytes = ui32DecodeOffset_bytes;
	gsVdecTop[ui8Vch].Param.b512bytesAligned = b512bytesAligned;

	gsVdecTop[ui8Vch].Param.ui32ErrorMBFilter = ui32ErrorMBFilter;
	gsVdecTop[ui8Vch].Param.ui32Width = ui32Width;
	gsVdecTop[ui8Vch].Param.ui32Height = ui32Height;

	gsVdecTop[ui8Vch].Param.bDualDecoding = bDualDecoding;
	gsVdecTop[ui8Vch].Param.bLowLatency = bLowLatency;

	gsVdecTop[ui8Vch].Param.bUseGstc = bUseGstc;
	gsVdecTop[ui8Vch].Param.ui32DisplayOffset_ms = ui32DisplayOffset_ms;
	gsVdecTop[ui8Vch].Param.ui8ClkID = ui8ClkID;

	gsVdecTop[ui8Vch].ChNum.ui8VesCh = ui8VesCh;
	gsVdecTop[ui8Vch].ChNum.ui8VdcCh = ui8VdcCh;
	gsVdecTop[ui8Vch].ChNum.ui8SyncCh = ui8SyncCh;

	gsVdecTop[ui8Vch].Status.ui32PicWidth = 0;
	gsVdecTop[ui8Vch].Status.ui32Height = 0;

	gsVdecTop[ui8Vch].Status.bSeqInit = FALSE;
	gsVdecTop[ui8Vch].Status.bEndOfDPB = FALSE;

	gsVdecTop[ui8Vch].Debug.ui32LogTick = 0;

	gsVdecTop[ui8Vch].State = VDEC_KTOP_READY;

	gui32ActiveSrc |= (1 << eInSrc);
	gui32ActiveDst |= (1 << eOutDst);

	*pui8VesCh = ui8VesCh;
	*pui8VdcCh = ui8VdcCh;
	*pui8VdsCh = ui8SyncCh;

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG][SDEC%d][VES%d][VDC%d][SYNC%d][DE%d] codec type:%d, UseGstc:%d, ClkID:%d, NoAdpStr:%d, %s"
		, ui8Vch, eInSrc, ui8VesCh, ui8VdcCh, ui8SyncCh, eOutDst, ui8Vcodec, bUseGstc, ui8ClkID, bNoAdpStr, __FUNCTION__ );

error:

	if( ui8ret > 0 )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][ERR][SDEC%d][VES%d][VDC%d][SYNC%d][DE%d] codec type:%d, UseGstc:%d, ClkID:%d, NoAdpStr:%d, %s"
			, ui8Vch, eInSrc, ui8VesCh, ui8VdcCh, ui8SyncCh, eOutDst, ui8Vcodec, bUseGstc, ui8ClkID, bNoAdpStr, __FUNCTION__ );

		if( ui32CpbBufAddr )
		{
			VDEC_CPB_Free( ui32CpbBufAddr );
			gsVdecTop[ui8Vch].ui32CpbBufAddr = 0x0;
		}

		for( ui8Vch = 0; ui8Vch < VDEC_NUM_OF_CHANNEL; ui8Vch++ )
		{
			if( gsVdecTop[ui8Vch].State != VDEC_KTOP_NULL )
			{
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][ERR][SDEC%d][VES%d][VDC%d][SYNC%d][DE%d] codec type:%d, UseGstc:%d, ClkID:%d, NoAdpStr:%d, %s"
					, ui8Vch, gsVdecTop[ui8Vch].State, gsVdecTop[ui8Vch].Param.eInSrc, gsVdecTop[ui8Vch].ChNum.ui8VesCh, gsVdecTop[ui8Vch].ChNum.ui8VdcCh, gsVdecTop[ui8Vch].ChNum.ui8SyncCh, gsVdecTop[ui8Vch].Param.eOutDst, gsVdecTop[ui8Vch].Param.ui8Vcodec, gsVdecTop[ui8Vch].Param.bUseGstc, gsVdecTop[ui8Vch].Param.ui8ClkID, gsVdecTop[ui8Vch].Param.bNoAdpStr, __FUNCTION__ );
			}
		}

		if( ui8VesCh != 0xFF )
			VES_Close(ui8VesCh);
		if( ui8VdcCh != 0xFF )
			VDC_Close(ui8VdcCh);
		if( ui8SyncCh != 0xFF )
			SYNC_Close(ui8SyncCh);

		*pui8VesCh = 0xFF;
		*pui8VdcCh = 0xFF;
		*pui8VdsCh = 0xFF;
	}

	mutex_unlock(&stVdecKtopLock);

	return ui8ret;
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
void VDEC_KTOP_Close(UINT8 ui8Vch)
{
	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][ERR] Already Closed Channel, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	if(gsVdecTop[ui8Vch].ChNum.ui8VesCh!=0xFF)
		VES_Close(gsVdecTop[ui8Vch].ChNum.ui8VesCh);
	if(gsVdecTop[ui8Vch].ChNum.ui8VdcCh!=0xFF)
		VDC_Close(gsVdecTop[ui8Vch].ChNum.ui8VdcCh);
	if(gsVdecTop[ui8Vch].ChNum.ui8SyncCh!=0xFF)
		SYNC_Close(gsVdecTop[ui8Vch].ChNum.ui8SyncCh);

	gui32ActiveSrc &= ~(1 << gsVdecTop[ui8Vch].Param.eInSrc);
	gui32ActiveDst &= ~(1 << gsVdecTop[ui8Vch].Param.eOutDst);

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d][DBG][SDEC%d][VES%d][VDC%d][SYNC%d][DE%d] Active Src:0x%x, Dst:0x%x, %s", ui8Vch, gsVdecTop[ui8Vch].Param.eInSrc, gsVdecTop[ui8Vch].ChNum.ui8VesCh, gsVdecTop[ui8Vch].ChNum.ui8VdcCh, gsVdecTop[ui8Vch].ChNum.ui8SyncCh, gsVdecTop[ui8Vch].Param.eOutDst, gui32ActiveSrc, gui32ActiveDst, __FUNCTION__ );

	// CPB Free
	if( gsVdecTop[ui8Vch].ui32CpbBufAddr )
		VDEC_CPB_Free(gsVdecTop[ui8Vch].ui32CpbBufAddr);
	gsVdecTop[ui8Vch].ui32CpbBufAddr = 0x0;

	// DPB Free
#ifdef USE_ION_ALLOC
	ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
	gsVdecTop[ui8Vch].psIonHandle = NULL;
#else
{
	UINT32	ui32NumOfFb;

	for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
	{
		VDEC_DPB_Free( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY );
	}
}
#endif
	gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

	gsVdecTop[ui8Vch].Param.ui8Vcodec = 0xFF;
	gsVdecTop[ui8Vch].Param.bNoAdpStr = FALSE;
	gsVdecTop[ui8Vch].Param.bForThumbnail = FALSE;
	gsVdecTop[ui8Vch].Param.ui32DecodeOffset_bytes = 0;
	gsVdecTop[ui8Vch].Param.b512bytesAligned = FALSE;
	gsVdecTop[ui8Vch].Param.ui32ErrorMBFilter = 0xFFFFFFFF;
	gsVdecTop[ui8Vch].Param.ui32Width = VDEC_KDRV_WIDTH_INVALID;
	gsVdecTop[ui8Vch].Param.ui32Height = VDEC_KDRV_HEIGHT_INVALID;
	gsVdecTop[ui8Vch].Param.bDualDecoding = FALSE;
	gsVdecTop[ui8Vch].Param.bLowLatency = FALSE;
	gsVdecTop[ui8Vch].Param.bUseGstc = FALSE;
	gsVdecTop[ui8Vch].Param.ui32DisplayOffset_ms = 0;
	gsVdecTop[ui8Vch].Param.ui8ClkID = 0xFF;

	gsVdecTop[ui8Vch].ChNum.ui8VesCh = 0xFF;
	gsVdecTop[ui8Vch].ChNum.ui8VdcCh = 0xFF;
	gsVdecTop[ui8Vch].ChNum.ui8SyncCh = 0xFF;

	gsVdecTop[ui8Vch].Status.ui32PicWidth = 0;
	gsVdecTop[ui8Vch].Status.ui32Height = 0;

	gsVdecTop[ui8Vch].Status.bSeqInit = FALSE;
	gsVdecTop[ui8Vch].Status.bEndOfDPB = FALSE;

	gsVdecTop[ui8Vch].State = VDEC_KTOP_NULL;

	mutex_unlock(&stVdecKtopLock);
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
void VDEC_KTOP_CloseAll(void)
{
	UINT8	ui8Vch;

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K][DBG] %s", __FUNCTION__);

	for( ui8Vch = 0; ui8Vch < VDEC_NUM_OF_CHANNEL; ui8Vch++ )
	{
		if( gsVdecTop[ui8Vch].State != VDEC_KTOP_NULL )
		{
			VDEC_KTOP_Close( ui8Vch );
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
int VDEC_KTOP_Play(UINT8 ui8Vch, SINT32 speed)
{
	int iRet = 0;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] speed: %d, %s", ui8Vch, gsVdecTop[ui8Vch].State, speed, __FUNCTION__);

	switch( gsVdecTop[ui8Vch].State )
	{
	case VDEC_KTOP_NULL :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] VDEC is Not Open, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		iRet = -1;
		break;
	case VDEC_KTOP_PLAY :
		if(speed == 0)
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] speed = %d, %s", ui8Vch, gsVdecTop[ui8Vch].State, speed, __FUNCTION__ );
			mutex_unlock(&stVdecKtopLock);
			return -1;
		}
		if( speed != VDEC_SPEED_RATE_INVALID )
			SYNC_Set_Speed(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, speed);

		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_NORMAL);
		break;
	case VDEC_KTOP_PLAY_STEP :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_NORMAL);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY;
		break;
	case VDEC_KTOP_PLAY_FREEZE :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_NORMAL);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY;
		break;
	case VDEC_KTOP_READY :
	case VDEC_KTOP_PAUSE :
		if(speed == 0)
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] speed = %d, %s", ui8Vch, gsVdecTop[ui8Vch].State, speed, __FUNCTION__ );
			mutex_unlock(&stVdecKtopLock);
			return -1;
		}
		if( speed != VDEC_SPEED_RATE_INVALID )
			SYNC_Set_Speed(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, speed);

		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_NORMAL);
		VES_Start(gsVdecTop[ui8Vch].ChNum.ui8VesCh);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
	}

	VDC_FeedKick();

	mutex_unlock(&stVdecKtopLock);

	return iRet;
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
int VDEC_KTOP_Step(UINT8 ui8Vch)
{
	int iRet = 0;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);

	switch( gsVdecTop[ui8Vch].State )
	{
	case VDEC_KTOP_NULL :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] VDEC is Not Open, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		iRet = -1;
		break;
	case VDEC_KTOP_PLAY :
	case VDEC_KTOP_PLAY_STEP :
	case VDEC_KTOP_PLAY_FREEZE :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_STEP);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY_STEP;
		break;
	case VDEC_KTOP_READY :
	case VDEC_KTOP_PAUSE :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_STEP);
		VES_Start(gsVdecTop[ui8Vch].ChNum.ui8VesCh);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY_STEP;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
	}

	VDC_FeedKick();

	mutex_unlock(&stVdecKtopLock);

	return iRet;
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
int VDEC_KTOP_Freeze(UINT8 ui8Vch)
{
	int iRet = 0;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);

	switch( gsVdecTop[ui8Vch].State )
	{
	case VDEC_KTOP_NULL :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] VDEC is Not Open, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		iRet = -1;
		break;
	case VDEC_KTOP_PLAY_FREEZE :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Already Freeze State, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		break;
	case VDEC_KTOP_PLAY :
	case VDEC_KTOP_PLAY_STEP :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_FREEZE);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY_FREEZE;
		break;
	case VDEC_KTOP_READY :
	case VDEC_KTOP_PAUSE :
		SYNC_Start(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, SYNC_PLAY_FREEZE);
		VES_Start(gsVdecTop[ui8Vch].ChNum.ui8VesCh);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PLAY_FREEZE;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
	}

	mutex_unlock(&stVdecKtopLock);

	return iRet;
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
int VDEC_KTOP_Pause(UINT8 ui8Vch)
{
	int iRet = 0;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);

	switch( gsVdecTop[ui8Vch].State )
	{
	case VDEC_KTOP_NULL :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] VDEC is Not Open, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		iRet = -1;
		break;
	case VDEC_KTOP_PAUSE :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Already Pause State, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		break;
	case VDEC_KTOP_READY :
	case VDEC_KTOP_PLAY :
	case VDEC_KTOP_PLAY_STEP :
	case VDEC_KTOP_PLAY_FREEZE :
		SYNC_Stop(gsVdecTop[ui8Vch].ChNum.ui8SyncCh);

		gsVdecTop[ui8Vch].State = VDEC_KTOP_PAUSE;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
	}

	mutex_unlock(&stVdecKtopLock);

	return iRet;
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
void VDEC_KTOP_Reset(UINT8 ui8Vch)
{
	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		return;
	}

	mutex_lock(&stVdecKtopLock);

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	SYNC_Reset(gsVdecTop[ui8Vch].ChNum.ui8SyncCh);
	VDC_Reset(gsVdecTop[ui8Vch].ChNum.ui8VdcCh);
	VES_Reset(gsVdecTop[ui8Vch].ChNum.ui8VesCh);

#ifdef USE_ION_ALLOC
	ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
	gsVdecTop[ui8Vch].psIonHandle = ION_Init();
	if( gsVdecTop[ui8Vch].psIonHandle == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}
#else
{
	UINT32	ui32NumOfFb;

	for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
	{
		VDEC_DPB_Free( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY );
	}
}
#endif
	gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

	mutex_unlock(&stVdecKtopLock);
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
void VDEC_KTOP_Flush(UINT8 ui8Vch)
{
	UINT32		ui32GenId;
	UINT8		ui8VesCh;
	UINT8		ui8VdcCh;
	UINT8		ui8SyncCh;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	gui32GenId++;
	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] Gen ID:%d, %s", ui8Vch, gsVdecTop[ui8Vch].State, gui32GenId, __FUNCTION__);

	ui32GenId = gui32GenId;

	ui8VesCh = gsVdecTop[ui8Vch].ChNum.ui8VesCh;
	ui8VdcCh = gsVdecTop[ui8Vch].ChNum.ui8VdcCh;
	ui8SyncCh = gsVdecTop[ui8Vch].ChNum.ui8SyncCh;

	if( VDC_IsSeqInit(ui8VdcCh) == TRUE )
		VES_Flush(ui8VesCh);
	VDC_Flush(ui8VdcCh, ui32GenId);
	SYNC_Flush(ui8SyncCh, ui32GenId);

	mutex_unlock(&stVdecKtopLock);
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
VDEC_KTOP_State VDEC_KTOP_GetState(UINT8 ui8Vch)
{
	VDEC_KTOP_State State = VDEC_KTOP_NULL;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return VDEC_KTOP_NULL;
	}

	State = gsVdecTop[ui8Vch].State;

	mutex_unlock(&stVdecKtopLock);

	return State;
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
UINT8 VDEC_KTOP_GetChannel(UINT8 ui8Vch, UINT8 type)	// type 1 : ves, 2 : lq, 3: msvc, 4: lipsync, 5: de_if
{
	UINT8 retCh = 0xFF;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return retCh;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return retCh;
	}

	switch(type)
	{
	case 1:
		retCh = gsVdecTop[ui8Vch].ChNum.ui8VesCh;
		break;
	case 2:
		retCh = gsVdecTop[ui8Vch].ChNum.ui8VdcCh;
		break;
	case 3:
		retCh = gsVdecTop[ui8Vch].ChNum.ui8SyncCh;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] tyep: %d, %s", ui8Vch, type, __FUNCTION__ );
	}

	mutex_unlock(&stVdecKtopLock);

	return retCh;
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
int VDEC_KTOP_SetLipSync(UINT8 ui8Vch, UINT32 bEnable)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] Enable: %d %s", ui8Vch, gsVdecTop[ui8Vch].State, bEnable, __FUNCTION__);

	if( bEnable )
	{
		SYNC_Set_MatchMode(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, PTS_MATCH_ENABLE);
	}
	else
	{
		SYNC_Set_MatchMode(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, PTS_MATCH_FREERUN_BASED_SYNC);
	}

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_SetPicScanMode(UINT8 ui8Vch, UINT8 ui8Mode)
{
	UINT32 ui32ScanMode = MSVC_NO_SKIP;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	switch(ui8Mode)
	{
	case VDEC_PIC_SCAN_ALL:
		ui32ScanMode = MSVC_NO_SKIP;
		break;
	case VDEC_PIC_SCAN_I:
		ui32ScanMode = MSVC_BP_SKIP;
		break;
	case VDEC_PIC_SCAN_IP:
		ui32ScanMode = MSVC_B_SKIP;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Scan Mode: %d, %s", ui8Vch, ui8Mode, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] ScanMode: %d %s", ui8Vch, gsVdecTop[ui8Vch].State, ui32ScanMode, __FUNCTION__);
	VDC_SetPicScanMode(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, ui32ScanMode);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_SetSrcScanType(UINT8 ui8Vch, UINT8 ui8ScanType)
{
	UINT32 ui32ScanType = MSVC_NO_SKIP;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	switch(ui8ScanType)
	{
	case VDEC_PIC_SCAN_ALL:
		ui32ScanType = MSVC_NO_SKIP;
		break;
	case VDEC_PIC_SCAN_I:
		ui32ScanType = MSVC_BP_SKIP;
		break;
	case VDEC_PIC_SCAN_IP:
		ui32ScanType = MSVC_B_SKIP;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Src Scan Type: %d, %s", ui8Vch, ui8ScanType, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] SrcScanType: %d %s", ui8Vch, gsVdecTop[ui8Vch].State, ui32ScanType, __FUNCTION__);
	VDC_SetSrcScanType(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, ui32ScanType);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_SetUserDataEn(UINT8 ui8Vch, UINT32 u32Enable)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDC_SetUserDataEn(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, u32Enable);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_GetUserDataInfo(UINT8 ui8Vch, UINT32 *pui32Address, UINT32 *pui32Size)
{
	mutex_lock(&stVdecKtopLock);

 	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDC_GetUserDataInfo(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, pui32Address, pui32Size);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
void VDEC_KTOP_SetDBGDisplayOffset(UINT8 ui8Vch, UINT32 ui32DisplayOffset_ms)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return;
	}

	SYNC_Debug_Set_DisplayOffset(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, ui32DisplayOffset_ms);

	mutex_unlock(&stVdecKtopLock);
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
int VDEC_KTOP_SetBaseTime(UINT8 ui8Vch, UINT8 ui8ClkID, UINT32 stcBaseTime, UINT32 ptsBaseTime)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if(gsVdecTop[ui8Vch].Param.ui8ClkID != ui8ClkID)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC %d][Err] Invalid Clk ID %d / %d, %s"
			, ui8Vch
			, gsVdecTop[ui8Vch].Param.ui8ClkID
			, ui8ClkID
			, __FUNCTION__ );

		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d][DBG] Clock ID:%d, Set Base Time:0x%08X, PTS:0x%08X, %s", ui8Vch, gsVdecTop[ui8Vch].State, ui8ClkID, stcBaseTime, ptsBaseTime, __FUNCTION__);

	SYNC_Set_BaseTime(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, ui8ClkID, stcBaseTime, ptsBaseTime);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_GetBaseTime(UINT8 ui8Vch, UINT8 ui8ClkID, UINT32 *pstcBaseTime, UINT32 *pptsBaseTime)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if(gsVdecTop[ui8Vch].Param.ui8ClkID != ui8ClkID)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC %d][Err] Invalid Clk ID %d / %d, %s"
			, ui8Vch
			, gsVdecTop[ui8Vch].Param.ui8ClkID
			, ui8ClkID
			, __FUNCTION__ );

		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	SYNC_Get_BaseTime(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, pstcBaseTime, pptsBaseTime);

	VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] Clock ID:%d, Get Base Time:0x%08X, PTS:0x%08X, %s", ui8Vch, gsVdecTop[ui8Vch].State, ui8ClkID, *pstcBaseTime, *pptsBaseTime, __FUNCTION__);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_GetBufferStatus(UINT8 ui8Vch, UINT32 *pui32CpbDepth, UINT32 *pui32CpbSize, UINT32 *pui32AuqDepth, UINT32 *pui32DqDepth)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	*pui32CpbDepth = VES_GetUsedBuffer(gsVdecTop[ui8Vch].ChNum.ui8VesCh);
	*pui32CpbSize = VES_GetBufferSize(gsVdecTop[ui8Vch].ChNum.ui8VesCh);
	*pui32AuqDepth = VDC_AU_Q_Occupancy(gsVdecTop[ui8Vch].ChNum.ui8VdcCh);
	*pui32DqDepth = VDC_DQ_Occupancy(gsVdecTop[ui8Vch].ChNum.ui8VdcCh);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
BOOLEAN VDEC_KTOP_UpdateCompressedBuffer(UINT8 ui8Vch,
									VDEC_KTOP_VAU_T eVAU_Type,
									UINT32 ui32UserBuf,
									UINT32 ui32UserBufSize,
									fpCpbCopyfunc fpCopyfunc,
									UINT32 ui32uID,
									UINT64 ui64TimeStamp,	// ns
									UINT32 ui32TimeStamp_90kHzTick,
									UINT32 ui32FrameRateRes,
									UINT32 ui32FrameRateDiv,
									UINT32 ui32Width,
									UINT32 ui32Height)
{
	BOOLEAN		bRet = FALSE;
	E_VES_AU_T	eAuType;

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		return FALSE;
	}

	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return FALSE;
	}

	ui32Width = ( ui32Width == VDEC_KDRV_WIDTH_INVALID ) ? VES_WIDTH_INVALID : ui32Width;
	ui32Height = ( ui32Height == VDEC_KDRV_HEIGHT_INVALID ) ? VES_HEIGHT_INVALID : ui32Height;

	switch( eVAU_Type )
	{
	case VDEC_KTOP_VAU_SEQH :
		eAuType = VES_AU_SEQUENCE_HEADER;
		break;
	case VDEC_KTOP_VAU_SEQE :
		eAuType = VES_AU_SEQUENCE_END;
		break;
	case VDEC_KTOP_VAU_DATA :
		eAuType = VES_AU_PICTURE_X;
		break;
	case VDEC_KTOP_VAU_EOS :
		eAuType = VES_AU_EOS;
		break;
	case VDEC_KTOP_VAU_UNKNOWN :
		eAuType = VES_AU_UNKNOWN;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] AU Type:%d, %s", ui8Vch, gsVdecTop[ui8Vch].State, eVAU_Type, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return FALSE;
	}

	if( (gsVdecTop[ui8Vch].Param.ui8Vcodec == VDC_CODEC_VP8) && (gsVdecTop[ui8Vch].Status.bSeqInit == FALSE) )
	{
		UINT32	ui32BufW = 2048, ui32BufH = 1088, ui32NFrm = 8;

		if( (gsVdecTop[ui8Vch].Param.eOutDst == VDEC_DST_GRP_BUFF0) || (gsVdecTop[ui8Vch].Param.eOutDst == VDEC_DST_GRP_BUFF1) )
		{
			if( gsVdecTop[ui8Vch].Status.bEndOfDPB == FALSE )
			{
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Wrn] Not Complete to Register DPB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
				mutex_unlock(&stVdecKtopLock);
				return FALSE;
			}
		}
		else if( gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb )
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Already Exist DPB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		}
		else
		{
			UINT32	ui32NumOfFb;
			UINT32	ui32FrameSize;
			UINT32	ui32YFrameSize; 	// Luma
			UINT32	ui32CFrameSize; 	// Chroma

			ui32FrameSize = ui32BufW * ui32BufH;
			ui32YFrameSize = ui32FrameSize;
			ui32CFrameSize = (ui32FrameSize + 1) /2;

			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY = ui32YFrameSize;
			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb = ui32CFrameSize;
			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr = 0;

			gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;
			gsVdecTop[ui8Vch].sVdecDpb.ui32BaseAddr = 0x0;

			for( ui32NumOfFb = 0; ui32NumOfFb < ui32NFrm; ui32NumOfFb++ )
			{
				UINT32	ui32FrameAddr;
#ifdef USE_ION_ALLOC
				ui32FrameAddr = ION_Alloc(gsVdecTop[ui8Vch].psIonHandle, ui32YFrameSize + ui32CFrameSize, FALSE);
#else
				ui32FrameAddr = VDEC_DPB_Alloc(ui32YFrameSize + ui32CFrameSize);
#endif
				VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] SeqInit: %d, Addr: 0x%X  %s", ui8Vch, gsVdecTop[ui8Vch].State,
												gsVdecTop[ui8Vch].Status.bSeqInit, ui32FrameAddr, __FUNCTION__);

				if( ui32FrameAddr == 0x0 )
				{
					VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to kmalloc DPB, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
					break;
				}

				gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY = ui32FrameAddr;
				gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrCb = ui32FrameAddr + ui32YFrameSize;
				gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrCr = 0x0;
				gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = ui32NumOfFb + 1;
			}

			if( ui32NumOfFb != ui32NFrm )
			{
				VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to kmalloc DPB, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);

#ifdef USE_ION_ALLOC
				ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
				gsVdecTop[ui8Vch].psIonHandle = ION_Init();
				if( gsVdecTop[ui8Vch].psIonHandle == NULL )
				{
					VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
					mutex_unlock(&stVdecKtopLock);
					return FALSE;
				}
#else
				for( ui32NumOfFb = 0; ui32NumOfFb < gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb; ui32NumOfFb++ )
				{
					VDEC_DPB_Free( gsVdecTop[ui8Vch].sVdecDpb.aBuf[ui32NumOfFb].ui32AddrY );
				}
#endif
				gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;
			}

			gsVdecTop[ui8Vch].Status.bEndOfDPB = (gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb) ? TRUE : FALSE;
		}

		if( VDC_RegisterDPB(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, &gsVdecTop[ui8Vch].sVdecDpb, ui32BufW) == FALSE )
		{
			VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to Register DPB, %s", ui8Vch, __FUNCTION__);

			mutex_unlock(&stVdecKtopLock);
			return FALSE;
		}
	}

	bRet = VES_UpdateBuffer(gsVdecTop[ui8Vch].ChNum.ui8VesCh,
							eAuType,
							ui32UserBuf,
							ui32UserBufSize,
							fpCopyfunc,
							ui32uID,
							ui64TimeStamp,
							ui32TimeStamp_90kHzTick,
							ui32FrameRateRes,
							ui32FrameRateDiv,
							ui32Width,
							ui32Height,
							VDC_IsSeqInit(gsVdecTop[ui8Vch].ChNum.ui8VdcCh));

	VDC_FeedKick();

	mutex_unlock(&stVdecKtopLock);

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
BOOLEAN VDEC_KTOP_UpdateGraphicBuffer(UINT8 ui8Vch,
									SINT32 si32FrameFd,
									UINT32 ui32BufStride,
									UINT32 ui32PicWidth,
									UINT32 ui32PicHeight,
									UINT64 ui64TimeStamp,	// ns
									UINT32 ui32TimeStamp_90kHzTick,
									UINT32 ui32FrameRateRes,
									UINT32 ui32FrameRateDiv,
									UINT32 ui32uID)
{
	BOOLEAN		bRet = TRUE;
#ifdef USE_ION_ALLOC
	S_FRAME_BUF_T	sFrameBuf;
	UINT32		ui32BufAddr = 0x0;
	UINT32		ui32BufSize = 0x0;
	UINT32		ui32GSTC;

	ui32GSTC = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;

	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return FALSE;
	}

	if( (gsVdecTop[ui8Vch].Param.eInSrc != VDEC_SRC_GRP_BUFF0) && (gsVdecTop[ui8Vch].Param.eInSrc != VDEC_SRC_GRP_BUFF1) )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] No Graphic Buffer Input, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return FALSE;
	}

	if( _VDEC_KTOP_FindFramePhy(ui8Vch, si32FrameFd, &ui32BufAddr, &ui32BufSize) == FALSE )
	{
		if( ION_GetPhy(gsVdecTop[ui8Vch].psIonHandle, si32FrameFd, &ui32BufAddr, &ui32BufSize) == FALSE )
		{
			VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to translate the ION FD to physical address, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
			mutex_unlock(&stVdecKtopLock);
			return FALSE;
		}

		gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].si32FrameFd = si32FrameFd;
		gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrY = ui32BufAddr;
		gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrCb = ui32BufAddr + (ui32BufSize * 2 / 3);
		gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrCr = 0x0;
		gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb++;

		gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY = (ui32BufSize * 2 / 3);
		gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb = (ui32BufSize * 1 / 3);
		gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr = 0;

		VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] NumOfFb: %d, FD: %d, Addr: 0x%X  %s", ui8Vch, gsVdecTop[ui8Vch].State,
										gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb, si32FrameFd, ui32BufAddr, __FUNCTION__);
	}

	if( (gsVdecTop[ui8Vch].Param.eOutDst != VDEC_DST_DE0) && (gsVdecTop[ui8Vch].Param.eOutDst != VDEC_DST_DE1) )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] No DE Output, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return FALSE;
	}

	SYNC_UpdateRate(gsVdecTop[ui8Vch].ChNum.ui8SyncCh,
					0x0, 0xFFFFFFFF,
					ui32FrameRateRes, ui32FrameRateDiv,
					ui32TimeStamp_90kHzTick,
					ui32TimeStamp_90kHzTick,
					ui32GSTC,
					ui32GSTC,
					FALSE,
					FALSE,
					FALSE,
					gui32GenId);

	sFrameBuf.ui32YFrameAddr = ui32BufAddr;
	sFrameBuf.ui32CFrameAddr = ui32BufAddr + (ui32BufSize * 2 / 3);
	sFrameBuf.ui32Stride = ui32BufStride;

	sFrameBuf.sDTS.ui32DTS = ui32TimeStamp_90kHzTick;
	sFrameBuf.sDTS.ui64TimeStamp = ui64TimeStamp;
	sFrameBuf.sDTS.ui32uID = ui32uID;
	sFrameBuf.sPTS.ui32PTS = ui32TimeStamp_90kHzTick;
	sFrameBuf.sPTS.ui64TimeStamp = ui64TimeStamp;
	sFrameBuf.sPTS.ui32uID = ui32uID;

	sFrameBuf.ui32InputGSTC = ui32GSTC;
	sFrameBuf.ui32FlushAge = gui32GenId;

	sFrameBuf.ui32AspectRatio = 3;
	sFrameBuf.ui32PicWidth = ui32PicWidth;
	sFrameBuf.ui32PicHeight = ui32PicHeight;
	sFrameBuf.ui32CropLeftOffset = 0;
	sFrameBuf.ui32CropRightOffset = 0;
	sFrameBuf.ui32CropTopOffset = 0;
	sFrameBuf.ui32CropBottomOffset = 0;
	sFrameBuf.eDisplayInfo = DISPQ_DISPLAY_INFO_PROGRESSIVE_FRAME;
	sFrameBuf.ui32DisplayPeriod = 1;
	sFrameBuf.si32FrmPackArrSei = -1;
	sFrameBuf.eLR_Order = DISPQ_3D_FRAME_NONE;
	sFrameBuf.ui16ParW = 1;
	sFrameBuf.ui16ParH = 1;

	sFrameBuf.sUsrData.ui32Size = 0;

	if( (bRet = SYNC_UpdateFrame(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, &sFrameBuf)) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] SYNC_UpdateFrame - %s", ui8Vch, __FUNCTION__ );
	}

	mutex_unlock(&stVdecKtopLock);
#endif

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
int VDEC_KTOP_GetDisplayFps(UINT8 ui8Vch, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	SYNC_GetDisplayRate(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_GetDropFps(UINT8 ui8Vch, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	SYNC_GetDropRate(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv);

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_SetFreeDPB(UINT8 ui8Vch, SINT32 si32FreeFrameFd)
{
#ifdef USE_ION_ALLOC
	UINT32	ui32BufAddr = 0x0;
	UINT32	ui32BufSize = 0x0;

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		return -1;
	}

	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( (gsVdecTop[ui8Vch].Param.eOutDst != VDEC_DST_GRP_BUFF0) && (gsVdecTop[ui8Vch].Param.eOutDst != VDEC_DST_GRP_BUFF1) )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] No Graphic Buffer Output, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].Status.bEndOfDPB == FALSE )
	{
		if( si32FreeFrameFd )
		{
			if( ION_GetPhy(gsVdecTop[ui8Vch].psIonHandle, si32FreeFrameFd, &ui32BufAddr, &ui32BufSize) == FALSE )
			{
				VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to translate the ION FD to physical address, %s(%d)", ui8Vch, __FUNCTION__, __LINE__);
				mutex_unlock(&stVdecKtopLock);
				return -1;
			}

			gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].si32FrameFd = si32FreeFrameFd;
			gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrY = ui32BufAddr;
			gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrCb = ui32BufAddr + (ui32BufSize * 2 / 3);
			gsVdecTop[ui8Vch].sVdecDpb.aBuf[gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb].ui32AddrCr = 0x0;
			gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb++;

			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeY = (ui32BufSize * 2 / 3);
			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCb = (ui32BufSize * 1 / 3);
			gsVdecTop[ui8Vch].sVdecDpb.ui32FrameSizeCr = 0;
		}
		else
		{
			UINT32	ui32BufWidth;
			UINT32	ui32BufHeight;

			gsVdecTop[ui8Vch].Status.bEndOfDPB = TRUE;

			if( (gsVdecTop[ui8Vch].Param.ui8Vcodec != VDC_CODEC_VP8) && (gsVdecTop[ui8Vch].Status.bSeqInit == TRUE) )
			{
				if( gsVdecTop[ui8Vch].Param.bNoAdpStr == TRUE )
				{
					ui32BufWidth = (gsVdecTop[ui8Vch].Status.ui32PicWidth + 0xF) & (~0xF);
					ui32BufHeight = (gsVdecTop[ui8Vch].Status.ui32Height + 0x1F) & (~0x1F);
				}
				else
				{
					ui32BufWidth = 2048;
					ui32BufHeight = 1088;
				}

				if( VDC_RegisterDPB(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, &gsVdecTop[ui8Vch].sVdecDpb, ui32BufWidth) == FALSE )
				{
					VDEC_KDRV_Message(ERROR,"[VDEC_K%d] Fail to Register DPB, %s", ui8Vch, __FUNCTION__);
					mutex_unlock(&stVdecKtopLock);
					return -1;
				}
			}
		}

		VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] EndOfDPB: %d, SeqInit: %d, FD: %d, Addr: 0x%X  %s", ui8Vch, gsVdecTop[ui8Vch].State,
										gsVdecTop[ui8Vch].Status.bEndOfDPB, gsVdecTop[ui8Vch].Status.bSeqInit,
										si32FreeFrameFd, ui32BufAddr, __FUNCTION__);
	}
	else
	{
//		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][DBG] YFrameAddr:0x%X, FrameFd:%d, %s", ui8Vch, ui32BufAddr, si32FreeFrameFd, __FUNCTION__ );
		_VDEC_KTOP_FindFramePhy(ui8Vch, si32FreeFrameFd, &ui32BufAddr, &ui32BufSize);

		VDC_DEC_PutUsedFrame(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, ui32BufAddr);
		VDC_FeedKick();
	}

	mutex_unlock(&stVdecKtopLock);
#endif

	return 0;
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
int VDEC_KTOP_ReconfigDPB(UINT8 ui8Vch)
{
	E_SYNC_DST_T eSyncDstCh = DE_IF_DST_INVALID;

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		return -1;
	}

	mutex_lock(&stVdecKtopLock);

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	VDEC_KDRV_Message(DBG_SYS, "[VDEC_K%d-%d] SeqInit: %d, %s", ui8Vch, gsVdecTop[ui8Vch].State, gsVdecTop[ui8Vch].Status.bSeqInit, __FUNCTION__);

	VDC_ReconfigDPB(gsVdecTop[ui8Vch].ChNum.ui8VdcCh);
	VES_Reset(gsVdecTop[ui8Vch].ChNum.ui8VesCh);
	VDC_Flush(gsVdecTop[ui8Vch].ChNum.ui8VdcCh, gui32GenId);

	switch(gsVdecTop[ui8Vch].Param.eOutDst)
	{
	case VDEC_DST_DE0:
		eSyncDstCh = DE_IF_DST_DE_IF0;
		break;
	case VDEC_DST_DE1:
		eSyncDstCh = DE_IF_DST_DE_IF1;
		break;
	case VDEC_DST_BUFF:
	case VDEC_DST_GRP_BUFF0:
	case VDEC_DST_GRP_BUFF1:
		eSyncDstCh = DE_IF_DST_INVALID;
		break;
	default:
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][ERR] VDEC Output Destination Param %d", ui8Vch, gsVdecTop[ui8Vch].Param.eOutDst);
		break;
	}

	if( eSyncDstCh != DE_IF_DST_INVALID )
	{
		UINT8 ui8SrcBufCh = 0xFF;
		BOOLEAN bFixedVSync = FALSE;
		E_PTS_MATCH_MODE_T eMatchMode = PTS_MATCH_ENABLE;

		SYNC_Close(gsVdecTop[ui8Vch].ChNum.ui8SyncCh);

		if( gsVdecTop[ui8Vch].Param.bUseGstc == TRUE )
		{
			switch( gsVdecTop[ui8Vch].Param.eInSrc )
			{
			case VDEC_SRC_SDEC0 :
			case VDEC_SRC_SDEC1 :
			case VDEC_SRC_SDEC2 :
				ui8SrcBufCh = 0;
				VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] H/W Ves Path, But Use the GSTC, SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, ui8SrcBufCh);
				break;
			case VDEC_SRC_TVP :
				ui8SrcBufCh = 0;
				break;
			case VDEC_SRC_BUFF0 :
			case VDEC_SRC_BUFF1 :
				ui8SrcBufCh = gsVdecTop[ui8Vch].Param.eInSrc - VDEC_SRC_BUFF0;
				break;
			case VDEC_SRC_GRP_BUFF0 :
			case VDEC_SRC_GRP_BUFF1 :
				ui8SrcBufCh = gsVdecTop[ui8Vch].Param.eInSrc - VDEC_SRC_GRP_BUFF0;
				VDEC_KDRV_Message(_TRACE, "[VDEC_K%d-%d][DBG] Source FrameBuf, SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, ui8SrcBufCh);
				break;
			default :
				VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][DBG] SrcBufCh:%d", ui8Vch, gsVdecTop[ui8Vch].State, gsVdecTop[ui8Vch].Param.eInSrc);
			}
		}

		if( gsVdecTop[ui8Vch].Param.bLowLatency == TRUE )
		{
			eMatchMode = PTS_MATCH_FREERUN_IGNORE_SYNC;
			bFixedVSync = TRUE;
		}

		gsVdecTop[ui8Vch].ChNum.ui8SyncCh = SYNC_Open(ui8Vch,
													eSyncDstCh,
													gsVdecTop[ui8Vch].Param.ui8Vcodec,
													ui8SrcBufCh,
													gsVdecTop[ui8Vch].Param.ui32DisplayOffset_ms,
													0x20,
													gsVdecTop[ui8Vch].Param.ui8ClkID,
													gsVdecTop[ui8Vch].Param.bDualDecoding,
													bFixedVSync,
													eMatchMode,
													gui32GenId);
		if(gsVdecTop[ui8Vch].ChNum.ui8SyncCh == 0xFF)
		{
			VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Fail to SYNC_Open(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		}
	}

	gsVdecTop[ui8Vch].Status.bSeqInit = FALSE;
	gsVdecTop[ui8Vch].Status.bEndOfDPB = FALSE;
	gsVdecTop[ui8Vch].sVdecDpb.ui32NumOfFb = 0;

#ifdef USE_ION_ALLOC
	ION_Deinit(gsVdecTop[ui8Vch].psIonHandle);
	gsVdecTop[ui8Vch].psIonHandle = ION_Init();
	if( gsVdecTop[ui8Vch].psIonHandle == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] ION_Init(), %s(%d)", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__, __LINE__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}
#endif

	mutex_unlock(&stVdecKtopLock);

	return 0;
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
int VDEC_KTOP_GetRunningFb(UINT8 ui8Vch, UINT32 *pui32PicWidth, UINT32 *pui32PicHeight, UINT32 *pui32uID, UINT64 *pui64TimeStamp, UINT32 *pui32PTS, SINT32 *psi32FrameFd)
{
	UINT32		ui32FrameAddr;

	mutex_lock(&stVdecKtopLock);

	if( ui8Vch >= VDEC_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d][Err] Channel Number, %s", ui8Vch, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( gsVdecTop[ui8Vch].State == VDEC_KTOP_NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] Channel is Not Opened, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( SYNC_GetRunningFb(gsVdecTop[ui8Vch].ChNum.ui8SyncCh, pui32PicWidth, pui32PicHeight, pui32uID, pui64TimeStamp, pui32PTS, &ui32FrameAddr) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[VDEC_K%d-%d][Err] No Running FB, %s", ui8Vch, gsVdecTop[ui8Vch].State, __FUNCTION__ );
		mutex_unlock(&stVdecKtopLock);
		return -1;
	}

	if( (gsVdecTop[ui8Vch].Param.eInSrc == VDEC_SRC_GRP_BUFF0) || (gsVdecTop[ui8Vch].Param.eInSrc == VDEC_SRC_GRP_BUFF1) )
		*psi32FrameFd = _VDEC_KTOP_FindFrameFd(ui8Vch, ui32FrameAddr);
	else
		*psi32FrameFd = 0;

	mutex_unlock(&stVdecKtopLock);

	return 0;
}



