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
 * version    0.1
 * date       2012.05.24
 * note       Additional information.
 *
 */

#ifndef _MSVC_ADP_H_
#define _MSVC_ADP_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "msvc_drv.h"
#include "vdc_drv.h"



#ifdef __cplusplus
extern "C" {
#endif



typedef enum
{
	MSVC_CODEC_H264_HP = 0,
	MSVC_CODEC_H264_MVC,
	MSVC_CODEC_VC1_RCV_V1,
	MSVC_CODEC_VC1_RCV_V2,
	MSVC_CODEC_VC1_AP,
	MSVC_CODEC_MPEG2_HP,
	MSVC_CODEC_H263,
	MSVC_CODEC_XVID,
	MSVC_CODEC_DIVX3,
	MSVC_CODEC_DIVX4,
	MSVC_CODEC_DIVX5,
	MSVC_CODEC_RVX,
	MSVC_CODEC_AVS,

	MSVC_CODEC_32bits = 0x20120726,
	MSVC_CODEC_INVALID = MSVC_CODEC_32bits,
} E_MSVC_CODEC_T;

typedef enum
{
	MSVC_AU_SEQUENCE_HEADER		= 0x1,
	MSVC_AU_SEQUENCE_END		= 0x2,
	MSVC_AU_PICTURE_I			= 0x3,
	MSVC_AU_PICTURE_P			= 0x4,
	MSVC_AU_PICTURE_B_NOSKIP	= 0x5,
	MSVC_AU_PICTURE_B_SKIP		= 0x6,
	MSVC_AU_PICTURE_X,
	MSVC_AU_UNKNOWN,
	MSVC_AU_EOS,
	MSVC_AU_32bits				= 0x20130206,
} E_MSVC_AU_T;

typedef struct
{
	struct
	{
		E_MSVC_AU_T	eAuType;
		UINT32		ui32AuAddr;
		UINT32		ui32AuSize;
		UINT32		ui32AuEnd;
	} sCurr;
	struct
	{
		E_MSVC_AU_T	eAuType;
		UINT32		ui32AuAddr;
		UINT32		ui32AuSize;
		UINT32		ui32AuEnd;
	} sNext;

	UINT32		ui32DTS;
	UINT32		ui32PTS;
	UINT64		ui64TimeStamp;
	UINT32		ui32uID;
	UINT32		ui32InputGSTC;
	UINT32		ui32FeedGSTC;
	UINT32		ui32FrameRateRes_Parser;
	UINT32		ui32FrameRateDiv_Parser;

#define MSVC_WIDTH_INVALID	0x0
#define MSVC_HEIGHT_INVALID	0x0
	// Out of Band Scaling (Picture Size of Adaptive Stream)
	UINT32		ui32Width;
	UINT32		ui32Height;

	UINT32		ui32FlushAge;
} S_MSVC_CMD_T;


typedef struct
{
	enum
	{
		MSVC_DONE_HDR_NONE = 0,
		MSVC_DONE_HDR_SEQINFO,
		MSVC_DONE_HDR_PICINFO,
		MSVC_DONE_HDR_32bits = 0x20120524
	} eMsvcDoneHdr;

	union
	{
		struct
		{
			UINT32	u32IsSuccess;
			BOOLEAN	bOverSpec;
			UINT32	u32PicWidth;
			UINT32	u32PicHeight;
			UINT32	u32AspectRatio;
			UINT32	u32Profile;
			UINT32	u32Level;
			UINT32	u32ProgSeq;

			UINT32	u32FrameRateRes;
			UINT32	u32FrameRateDiv;

 			UINT32	ui32RefFrameCount;
			UINT32	ui32FrameBufDelay;
		} seqInfo;

		struct
		{
			UINT32	u32DecodingSuccess;
			UINT32	u32NumOfErrMBs;
			SINT32	i32IndexFrameDecoded;
			SINT32	i32IndexFrameDisplay;
			UINT32	u32PicType;
			UINT32	u32LowDelay;
			UINT32	u32Aspectratio;
			UINT32	u32PicWidth;
			UINT32	u32PicHeight;
			UINT32	u32CropLeftOffset;
			UINT32	u32CropRightOffset;
			UINT32	u32CropTopOffset;
			UINT32	u32CropBottomOffset;
			UINT32	u32ActiveFMT;
			UINT32	ui32FrameRateRes;
			UINT32	ui32FrameRateDiv;
			UINT32	u32ProgSeq;
			UINT32	u32ProgFrame;
			UINT32	u32PictureStructure;
			SINT32	si32FrmPackArrSei;
			SINT32	u32bPackedMode;
			SINT32	u32bRetryDec;

			UINT32	ui32DisplayInfo;
			BOOLEAN	bVariableFrameRate;
			UINT32	ui32DisplayPeriod;
			SINT32	i32FramePackArrange;		// 3D SEI
			UINT32	ui32LR_Order;
			UINT16	ui16ParW;
			UINT16	ui16ParH;

			struct
			{
				UINT32 	ui32PicType;
				UINT32	ui32Rpt_ff;
				UINT32	ui32Top_ff;
				UINT32	ui32BuffAddr;
				UINT32	ui32Size;
			} sUsrData;
		} picInfo;
	} u;
} S_MSVC_DONE_INFO_T;


typedef enum
{
	MSVC_SKIP_NO = 0,
	MSVC_SKIP_PB,
	MSVC_SKIP_B,
	MSVC_SKIP_TYPE_32bits = 0x20120727,
} E_MSVC_SKIP_TYPE_T;




void MSVC_ADP_Init(	UINT32 (*_fpVDC_GetUsedFrame)(UINT8 ui8VdcCh),
						void (*_fpVDC_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param),
						UINT32 (*_fpVDC_Get_CPBPhyWrAddr)(UINT8 ui8VdcCh) );
UINT8 MSVC_ADP_Open(	UINT8 ui8VdcCh,
							E_MSVC_CODEC_T eCodecType,
							UINT32 ui32CPBAddr,
							UINT32 ui32CPBSize,
							BOOLEAN bRingBufferMode,
							BOOLEAN bLowLatency,
							BOOLEAN bDual3D,
							BOOLEAN bNoReordering,
							UINT32 ui32Width,
							UINT32 ui32Height);
void MSVC_ADP_Close(UINT8 ui8MsvcCh);
BOOLEAN MSVC_ADP_Reset(UINT8 ui8MsvcCh);
BOOLEAN MSVC_ADP_IsResetting(UINT8 ui8MsvcCh);
void MSVC_ADP_ResetDone(UINT8 ui8MsvcCh);
BOOLEAN MSVC_ADP_Flush(UINT8 ui8MsvcCh);
BOOLEAN MSVC_ADP_SetPicSkipMode(UINT32 ui8MsvcCh, E_MSVC_SKIP_TYPE_T ePicSkip);
BOOLEAN MSVC_ADP_SetSrcSkipType(UINT32 ui8MsvcCh, E_MSVC_SKIP_TYPE_T eSrcSkip);
BOOLEAN MSVC_ADP_SetUserDataEn(UINT32 ui8MsvcCh, BOOLEAN bUserDataEnable);
BOOLEAN MSVC_ADP_ReconfigDPB(UINT32 ui8MsvcCh);
BOOLEAN MSVC_ADP_RegisterDPB(UINT32 ui8MsvcCh, S_VDEC_DPB_INFO_T *psVdecDpb, UINT32 ui32BufStride);
FRAME_BUF_INFO_T *MSVC_ADP_DBG_GetDPB(UINT32 ui8MsvcCh);
UINT32 MSVC_ADP_IsBusy(UINT8 ui8MsvcCh);
BOOLEAN MSVC_ADP_IsReady(UINT8 ui8MsvcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ);
BOOLEAN MSVC_ADP_UpdateBuffer(	UINT8 ui8MsvcCh,
										S_MSVC_CMD_T *psMsvcCmd,
										BOOLEAN *pbNeedMore,
										UINT32 ui32GSTC,
										UINT32 ui32NumOfActiveAuQ,
										UINT32 ui32NumOfActiveDQ);
void MSVC_ADP_ISR_RunDone(UINT8 ui8MsvcCh, S_MSVC_DONE_INFO_T *pMsvcDoneInfo);
void MSVC_ADP_ISR_FieldDone(UINT8 ui8MsvcCh);
void MSVC_ADP_ISR_Empty(UINT8 ui8MsvcCh);
void MSVC_ADP_ISR_ResetDone(UINT8 ui8CoreNum);



#ifdef __cplusplus
}
#endif

#endif /* _MSVC_ADP_H_ */

