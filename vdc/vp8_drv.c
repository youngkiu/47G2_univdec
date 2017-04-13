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
 * author     sooya.joo@lge.com
 * version    1.0
 * date       2011.11.09
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

#include "../mcu/vdec_type_defs.h"
#include "../hal/lg1152/lg1152_vdec_base.h"

#ifdef __XTENSA__
#include <stdio.h>
#else
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#include <asm/uaccess.h> // copy_from_user
#include <asm/cacheflush.h>
#endif

#include "vp8_drv.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/mcu_hal_api.h"
#include "../hal/lg1152/vp8/inc/vp8decapi.h"
#include "../hal/lg1152/vp8/inc/dwl.h"
#include "../hal/lg1152/vp8/vp8/vp8hwd_container.h"

#include "../ves/ves_cpb.h"

#include "../vdec_dpb.h"

#include "../mcu/vdec_print.h"
#include "../mcu/ram_log.h"


#include "../mcu/os_adap.h"

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
extern void DWLResetDPB(void);
extern void DWLRegisterDPB(S_VDEC_DPB_INFO_T *psVdecDpb);

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
typedef struct
{
	UINT8		ui8VdcCh;

	// feeder Requirement
	VP8DecInput		decInput;
	VP8DecOutput	decOutput;
	VP8DecInst 		Instance;

	BOOLEAN		bReady;
} VDEC_VP8_DB_T;

static VDEC_VP8_DB_T gsVP8db[NUM_OF_VP8_CHANNEL];
static UINT32 (*gfpVP8_GetUsedFrame)(UINT8 ui8VdcCh);
static void (*gfpVP8_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param);


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
void VP8_Init(	UINT32 (*_fpVDC_GetUsedFrame)(UINT8 ui8VdcCh),
				void (*_fpVDC_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param))
{
	UINT8 ui8ch;
	VP8DecApiVersion decApi;
	VP8DecBuild decBuild;
	DWLHwConfig_t hwConfig;

	VDEC_KDRV_Message(_TRACE, "[VP8][DBG] %s", __FUNCTION__ );

	gfpVP8_GetUsedFrame = _fpVDC_GetUsedFrame;
	gfpVP8_ReportPicInfo = _fpVDC_ReportPicInfo;

	for(ui8ch=0; ui8ch<NUM_OF_VP8_CHANNEL; ui8ch++)
	{
		gsVP8db[ui8ch].Instance = NULL;
		DWLmemset(&gsVP8db[ui8ch].decInput, 0, sizeof(VP8DecInput));
		DWLmemset(&gsVP8db[ui8ch].decOutput, 0, sizeof(VP8DecOutput));
	}

	DWLInitRegister();

	/* Print API version number */
	decApi = VP8DecGetAPIVersion();
	decBuild = VP8DecGetBuild();
	DWLReadAsicConfig(&hwConfig);
	VDEC_KDRV_Message(_TRACE, "[VP8][DBG] 8170 VP8 Decoder API v%d.%d - SW build: %d - HW build: %x",
			decApi.major, decApi.minor, decBuild.swBuild, decBuild.hwBuild);

	VDEC_KDRV_Message(_TRACE, "[VP8][DBG] HW Supports video decoding up to %d pixels,",
			hwConfig.maxDecPicWidth);

	VDEC_KDRV_Message(_TRACE, "[VP8][DBG] supported codecs: %s%s",
			hwConfig.vp7Support ? "VP-7 " : "",
			hwConfig.vp8Support ? "VP-8" : "");

	if(hwConfig.ppSupport)
		VDEC_KDRV_Message(_TRACE, "[VP8][DBG] Maximum Post-processor output size %d pixels\n",
				hwConfig.maxPpOutPicWidth);
	else
		VDEC_KDRV_Message(_TRACE, "[VP8][DBG] Post-Processor not supported\n");
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
UINT8 VP8_Open(UINT8 ui8VdcCh)
{
	UINT8	ui8VP8ch = 0xFF;

	for( ui8VP8ch = 0; ui8VP8ch < NUM_OF_VP8_CHANNEL; ui8VP8ch++ )
	{
		if( gsVP8db[ui8VP8ch].Instance == NULL )
			break;
	}
	if( ui8VP8ch == NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8][Err] Not Enough Channel %s", __FUNCTION__ );
		return 0xFF;
	}

	DWLmemset(&gsVP8db[ui8VP8ch].decInput, 0, sizeof(VP8DecInput));
	DWLmemset(&gsVP8db[ui8VP8ch].decOutput, 0, sizeof(VP8DecOutput));
	DWLResetDPB();

	VDEC_KDRV_Message(_TRACE, "[VP8%d][DBG] %s", ui8VP8ch, __FUNCTION__ );

	gsVP8db[ui8VP8ch].ui8VdcCh = ui8VdcCh;
	gsVP8db[ui8VP8ch].bReady = TRUE;

	return ui8VP8ch;
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
void VP8_Close(UINT8 ui8VP8ch)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return;
	}

	if( gsVP8db[ui8VP8ch].Instance == NULL )
		VDEC_KDRV_Message(ERROR, "[VP8%d][Wrn] not exist release instance %s", ui8VP8ch, __FUNCTION__ );

	VP8DecRelease(gsVP8db[ui8VP8ch].Instance);
	gsVP8db[ui8VP8ch].Instance = NULL;

	DWLmemset(&gsVP8db[ui8VP8ch].decInput, 0, sizeof(VP8DecInput));
	DWLmemset(&gsVP8db[ui8VP8ch].decOutput, 0, sizeof(VP8DecOutput));
	gsVP8db[ui8VP8ch].ui8VdcCh = 0xFF;
	gsVP8db[ui8VP8ch].bReady = FALSE;

	VDEC_KDRV_Message(_TRACE, "[VP8%d][DBG] %s", ui8VP8ch, __FUNCTION__ );
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
BOOLEAN VP8_Flush(UINT8 ui8VP8ch)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[VP8%d][DBG] %s", ui8VP8ch, __FUNCTION__ );

	gsVP8db[ui8VP8ch].decOutput.findKeyFrame = 0;

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
BOOLEAN VP8_Reset(UINT8 ui8VP8ch)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	gsVP8db[ui8VP8ch].decOutput.findKeyFrame = 0;

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
BOOLEAN VP8_IsReady(UINT8 ui8VP8ch)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] Channel Number, %s", ui8VP8ch, __FUNCTION__ );
		return TRUE;
	}

	return gsVP8db[ui8VP8ch].bReady;
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
BOOLEAN VP8_SetScanMode(UINT8 ui8VP8ch, UINT8 ui8ScanMode)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	if((gsVP8db[ui8VP8ch].decOutput.picScanMode!=0) && (ui8ScanMode==0))
		gsVP8db[ui8VP8ch].decOutput.findKeyFrame = 0;
	gsVP8db[ui8VP8ch].decOutput.picScanMode = ui8ScanMode;

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
BOOLEAN VP8_RegisterDPB(UINT8 ui8VP8ch, S_VDEC_DPB_INFO_T *psVdecDpb)
{
	VP8DecFormat decFormat;
	u32 tiledOutput = 0;
	VP8DecRet ret;

	decFormat = VP8DEC_VP8;

	ret = VP8DecInit(ui8VP8ch, &gsVP8db[ui8VP8ch].Instance, decFormat, 0,
			        psVdecDpb->ui32NumOfFb, tiledOutput );

	if (ret != VP8DEC_OK)
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][DBG] DECODER INITIALIZATION FAILED, %s", ui8VP8ch, __FUNCTION__ );
		return 0xFF;
	}

	DWLRegisterDPB(psVdecDpb);

	VDEC_KDRV_Message(DBG_SYS, "[VP8%d] nFrm: %d, %s", ui8VP8ch, psVdecDpb->ui32NumOfFb, __FUNCTION__);

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
BOOLEAN VP8_ReconfigDPB(UINT8 ui8VP8ch)
{
	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	if( gsVP8db[ui8VP8ch].Instance == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Wrn] not exist release instance %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	VP8DecRelease(gsVP8db[ui8VP8ch].Instance);
	gsVP8db[ui8VP8ch].Instance = NULL;

	DWLmemset(&gsVP8db[ui8VP8ch].decInput, 0, sizeof(VP8DecInput));
	DWLmemset(&gsVP8db[ui8VP8ch].decOutput, 0, sizeof(VP8DecOutput));
	DWLResetDPB();

	VDEC_KDRV_Message(DBG_SYS, "[VP8%d] %s", ui8VP8ch, __FUNCTION__);

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
BOOLEAN VP8_SendCmd(UINT8 ui8VP8ch, VP8_CmdQ_T *pstVP8Aui)
{
	VP8DecRet ret;
	VP8DecInst decInst;
	VP8DecInfo info;
	VP8DecPicture decPicture;

	VP8DecInput *pDecInput = NULL;
	VP8DecOutput *pDecOutput = NULL;

//	u32 ui32BufStride = 0;
//	u32 ui32BufHeight;
//	u32 ui32YFrameSize;
//	DecAsicBuffers_t *pAsicBuff;
	UINT32		ui32ClearFrameAddr;

	BOOLEAN	bSeqInit = FALSE;

	if( ui8VP8ch >= NUM_OF_VP8_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][Err] %s", ui8VP8ch, __FUNCTION__ );
		return FALSE;
	}

	decInst = gsVP8db[ui8VP8ch].Instance;
	pDecInput = &gsVP8db[ui8VP8ch].decInput;
	pDecOutput = &gsVP8db[ui8VP8ch].decOutput;

	pDecInput->pStream = (u8*)pstVP8Aui->pui8VirStartPtr;
	pDecInput->dataLen = pstVP8Aui->ui32AuSize;
	pDecInput->streamBusAddress = (u32)pstVP8Aui->ui32AuPhyAddr;

	VDEC_KDRV_Message(DBG_VDC, "[VP8%d][DBG] %x, %x, %d %s"
		, ui8VP8ch
		, (UINT32)pDecInput->pStream
		, pDecInput->streamBusAddress
		, pDecInput->dataLen
		, __FUNCTION__ );

	while( (ui32ClearFrameAddr = gfpVP8_GetUsedFrame(gsVP8db[ui8VP8ch].ui8VdcCh)) != 0x0 )
	{
		;
	}

	gsVP8db[ui8VP8ch].bReady = FALSE;

	ret = VP8DecDecode(decInst, pDecInput, pDecOutput);

	if (ret == VP8DEC_HDRS_RDY)
	{
		VDC_REPORT_INFO_T vp8_seqinfo_param = {0, };

		ret = VP8DecGetInfo(decInst, &info);

		vp8_seqinfo_param.eCmdHdr = VDC_CMD_HDR_SEQINFO;
		vp8_seqinfo_param.u.seqInfo.ui32IsSuccess = 1;
		vp8_seqinfo_param.u.seqInfo.ui32PicWidth = info.frameWidth;
		vp8_seqinfo_param.u.seqInfo.ui32PicHeight = info.frameHeight;
		vp8_seqinfo_param.u.seqInfo.ui32AspectRatio = 3;	// ??
		vp8_seqinfo_param.u.seqInfo.ui32Profile = info.vpProfile;
		vp8_seqinfo_param.u.seqInfo.ui32Level = 0;
		vp8_seqinfo_param.u.seqInfo.ui32uID = pstVP8Aui->ui32uID;
		vp8_seqinfo_param.u.seqInfo.ui32RefFrameCount = 0;

		VDEC_KDRV_Message(DBG_SYS, "[VP8%d][SEQ] Profile: %d, Width: %d, Height: %d, AspectRatio: %d",
				ui8VP8ch,
				vp8_seqinfo_param.u.seqInfo.ui32Profile,
				vp8_seqinfo_param.u.seqInfo.ui32PicWidth,
				vp8_seqinfo_param.u.seqInfo.ui32PicHeight,
				vp8_seqinfo_param.u.seqInfo.ui32AspectRatio);

		gfpVP8_ReportPicInfo(gsVP8db[ui8VP8ch].ui8VdcCh, &vp8_seqinfo_param);

		bSeqInit = TRUE;
	}
	else if (ret != VP8DEC_PIC_DECODED && ret != VP8DEC_SLICE_RDY)
	{
		VDEC_KDRV_Message(ERROR, "[VP8%d][DBG] Frame Decoding Fail = %d %s", ui8VP8ch, ret, __FUNCTION__ );
		//moreFrames = 0; //Sudhaher - Temp

		gsVP8db[ui8VP8ch].bReady = TRUE;

		return FALSE;
	}

	if(ret == VP8DEC_SLICE_RDY)
		VDEC_KDRV_Message(_TRACE, "[VP8%d][DBG] Slice Ready", ui8VP8ch);

	VDEC_KDRV_Message(DBG_VDC, "[VP8%d][DBG] Picture Ready %d", ui8VP8ch, ret);

	while (VP8DecNextPicture(decInst, &decPicture, 0) == VP8DEC_PIC_RDY)
	{
		VDC_REPORT_INFO_T vp8_picinfo_param = {0, };

//		ret = VP8DecGetInfo(decInst, &info);

//		pAsicBuff = ((VP8DecContainer_t *) decInst)->asicBuff;

		vp8_picinfo_param.eCmdHdr = VDC_CMD_HDR_PICINFO;
		vp8_picinfo_param.u.picInfo.ui32IsSuccess = 1;
		vp8_picinfo_param.u.picInfo.ui32NumOfErrMBs = decPicture.nbrOfErrMBs;
		vp8_picinfo_param.u.picInfo.ui32PicType = (decPicture.isIntraFrame?0:1);	// I(0) , P(1)
		vp8_picinfo_param.u.picInfo.ui32AspectRatio = 3;
		vp8_picinfo_param.u.picInfo.ui32PicWidth = decPicture.codedWidth;
		vp8_picinfo_param.u.picInfo.ui32PicHeight = decPicture.codedHeight;
		vp8_picinfo_param.u.picInfo.ui32CropLeftOffset = 0;
		vp8_picinfo_param.u.picInfo.ui32CropRightOffset =0;
		vp8_picinfo_param.u.picInfo.ui32CropTopOffset = 0;
		vp8_picinfo_param.u.picInfo.ui32CropBottomOffset = 0;
		vp8_picinfo_param.u.picInfo.ui32ProgSeq = 1;
		vp8_picinfo_param.u.picInfo.ui32ProgFrame = 1;
		vp8_picinfo_param.u.picInfo.si32FrmPackArrSei = -1;		// 3d info for H.264
		vp8_picinfo_param.u.picInfo.sDTS.ui32uID = pstVP8Aui->ui32uID;
		vp8_picinfo_param.u.picInfo.sDTS.ui64TimeStamp = pstVP8Aui->ui64TimeStamp;
		vp8_picinfo_param.u.picInfo.sDTS.ui32DTS = pstVP8Aui->ui32DTS;
		vp8_picinfo_param.u.picInfo.sPTS.ui32uID = pstVP8Aui->ui32uID;
		vp8_picinfo_param.u.picInfo.sPTS.ui64TimeStamp = pstVP8Aui->ui64TimeStamp;
		vp8_picinfo_param.u.picInfo.sPTS.ui32PTS = pstVP8Aui->ui32PTS;

		vp8_picinfo_param.u.picInfo.sFrameRate_Parser.ui32Residual = pstVP8Aui->ui32FrameRateRes_Parser;
		vp8_picinfo_param.u.picInfo.sFrameRate_Parser.ui32Divider = pstVP8Aui->ui32FrameRateDiv_Parser;
		vp8_picinfo_param.u.picInfo.sFrameRate_Decoder.ui32Residual = 0x0;
		vp8_picinfo_param.u.picInfo.sFrameRate_Decoder.ui32Divider = 0xFFFFFFFF;

//		ui32BufStride = (info.frameWidth + 0xF) & (~0xF);
//		ui32BufHeight = (info.frameHeight + 0xF) & (~0xF);
//		ui32YFrameSize = (ui32BufStride*ui32BufHeight+0x3FFF)&(~0x3FFF);

		vp8_picinfo_param.u.picInfo.ui32Stride = decPicture.frameWidth;
		vp8_picinfo_param.u.picInfo.ui32YFrameAddr = decPicture.outputFrameBusAddress;
		vp8_picinfo_param.u.picInfo.ui32CFrameAddr = decPicture.outputFrameBusAddressC;

		vp8_picinfo_param.u.picInfo.ui32InputGSTC = pstVP8Aui->ui32InputGSTC;

		vp8_picinfo_param.u.picInfo.ui32FlushAge = pstVP8Aui->ui32FlushAge;

		vp8_picinfo_param.u.picInfo.eDisplayInfo = DISPQ_DISPLAY_INFO_PROGRESSIVE_FRAME;
		vp8_picinfo_param.u.picInfo.ui32DisplayPeriod = 1;
		vp8_picinfo_param.u.picInfo.eLR_Order = DISPQ_3D_FRAME_NONE;
		vp8_picinfo_param.u.picInfo.ui16ParW = 1;
		vp8_picinfo_param.u.picInfo.ui16ParH = 1;

		vp8_picinfo_param.u.picInfo.sUsrData.ui32Size = 0;

		if(vp8_picinfo_param.u.picInfo.ui32IsSuccess != 1)
			VDEC_KDRV_Message(_TRACE, "[VP8][PIC] ui32IsSuccess;(%d)", vp8_picinfo_param.u.picInfo.ui32IsSuccess);
		VDEC_KDRV_Message(DBG_VDC, "[VP8][PIC] ui32IsSuccess;(%d)", vp8_picinfo_param.u.picInfo.ui32IsSuccess);
	//	VDEC_KDRV_Message(DBG_VDC, "[VP8][PIC] Index Decoded (%d) Display(%d)", vp8_picinfo_param.u.picInfo.u32IndexFrameDecoded, vp8_picinfo_param.u.picInfo.u32IndexFrameDisplay);
		VDEC_KDRV_Message(DBG_VDC, "[VP8][PIC] Width * Height : %d * %d", vp8_picinfo_param.u.picInfo.ui32PicWidth, vp8_picinfo_param.u.picInfo.ui32PicHeight);
		VDEC_KDRV_Message(DBG_VDC, "[VP8][PIC] type : %x ", vp8_picinfo_param.u.picInfo.ui32PicType);
		VDEC_KDRV_Message(DBG_VDC, "[VP8][PIC] Addr : 0x%x, 0x%x", vp8_picinfo_param.u.picInfo.ui32YFrameAddr, vp8_picinfo_param.u.picInfo.ui32CFrameAddr);
		VDEC_KDRV_Message(DBG_VDC, "[VP8][DBG] TimeStmp  %d %llu", vp8_picinfo_param.u.picInfo.sPTS.ui32PTS, vp8_picinfo_param.u.picInfo.sPTS.ui64TimeStamp);

		gfpVP8_ReportPicInfo(gsVP8db[ui8VP8ch].ui8VdcCh, &vp8_picinfo_param);
	}

	gsVP8db[ui8VP8ch].bReady = TRUE;

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
UINT32 VP8_DECODER_ISR(UINT32 ui32ISRMask)
{

#if (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__))
	if (ui32ISRMask&(1<<G1DEC)) {
		extern void DWLReceiveIntr(void);
		DWLReceiveIntr();
#ifdef __XTENSA__
		TOP_HAL_ClearInterIntrMsk(1<<G1DEC);
#else //!__XTENSA__
		TOP_HAL_ClearExtIntrMsk(1<<G1DEC);
#endif //~__XTENSA__
		ui32ISRMask = ui32ISRMask&~(1<<G1DEC);
//		printk("\n\nG1DEC_ISR\n\n");
	}
#endif

#ifndef __XTENSA__
	if (ui32ISRMask&(1<<G1DEC_SRST_DONE)) {
		VDEC_KDRV_Message(_TRACE, "Intr G1DEC_SRST_DONE\n");
		TOP_HAL_DisableExtIntr(G1DEC_SRST_DONE);
	}
#endif

	return ui32ISRMask;
}

