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



#ifndef _VDEC_VDC_DRV_H_
#define _VDEC_VDC_DRV_H_


#include "vdc_auq.h"
#include "../vds/disp_q.h"
#include "../vdec_dpb.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	VDC_NUM_OF_CHANNEL			3



typedef enum
{
	VDC_CODEC_H264_HP = 0,
	VDC_CODEC_H264_MVC,
	VDC_CODEC_VC1_RCV_V1,		// File Play
	VDC_CODEC_VC1_RCV_V2,		// File Play
	VDC_CODEC_VC1_AP,
	VDC_CODEC_MPEG2_HP,
	VDC_CODEC_H263,
	VDC_CODEC_XVID,
	VDC_CODEC_DIVX3,		// File Play
	VDC_CODEC_DIVX4,
	VDC_CODEC_DIVX5,
	VDC_CODEC_RVX,			// File Play
	VDC_CODEC_AVS,
	VDC_CODEC_VP8,

	VDC_CODEC_MIN = VDC_CODEC_H264_HP,
	VDC_CODEC_MAX = VDC_CODEC_VP8,
	VDC_CODEC_INVALID
} E_VDC_CODEC_T;


typedef struct
{
	enum
	{
		VDC_CMD_HDR_NONE = 0,
		VDC_CMD_HDR_SEQINFO,
		VDC_CMD_HDR_PICINFO,
		VDC_CMD_HDR_RESET,
		VDC_CMD_HDR_32bits = 0x20110714
	} eCmdHdr;

	union
	{
		struct
		{
			UINT32	ui32IsSuccess;
			BOOLEAN	bOverSpec;
			UINT32	ui32PicWidth;
			UINT32	ui32PicHeight;
			UINT32	ui32AspectRatio;
			UINT32	ui32uID;
			UINT32	ui32Profile;
			UINT32	ui32Level;

			UINT32	ui32FrameRateRes;
			UINT32	ui32FrameRateDiv;

			UINT32	ui32RefFrameCount;
			UINT32	ui32ProgSeq;
		} seqInfo;

		struct
		{
			UINT32	ui32IsSuccess;
			UINT32	ui32PicWidth;
			UINT32	ui32PicHeight;
			UINT32	ui32AspectRatio;

			UINT32	ui32NumOfErrMBs;
			UINT32	ui32PicType;
			UINT32	ui32LowDelay;
			UINT32	ui32ActiveFMT;
			UINT32	ui32ProgSeq;
			UINT32	ui32ProgFrame;

			struct
			{
				UINT32		ui32Residual;
				UINT32		ui32Divider;
			} sFrameRate_Parser;
			struct
			{
				UINT32		ui32Residual;
				UINT32		ui32Divider;
			} sFrameRate_Decoder;

			struct
			{
				UINT32 ui32DTS;
				UINT64 ui64TimeStamp;
				UINT32 ui32uID;
			} sDTS;
			struct
			{
				UINT32 ui32PTS;
				UINT64 ui64TimeStamp;
				UINT32 ui32uID;
			} sPTS;

			UINT32	ui32Stride;
			UINT32	ui32YFrameAddr;
			UINT32	ui32CFrameAddr;
			UINT32	ui32InputGSTC;
			UINT32	ui32FeedGSTC;
			UINT32	ui32FlushAge;
			UINT32	ui32CropLeftOffset;
			UINT32	ui32CropRightOffset;
			UINT32	ui32CropTopOffset;
			UINT32	ui32CropBottomOffset;
			UINT32	ui32PictureStructure;
			BOOLEAN	bVariableFrameRate;
			E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
			UINT32	ui32DisplayPeriod;
			SINT32	si32FrmPackArrSei;
			E_DISPQ_3D_LR_ORDER_T eLR_Order;
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
} VDC_REPORT_INFO_T;

typedef struct
{
	UINT32	ui32Ch;
	BOOLEAN	bRet;
	UINT32	ui32AuType;
	UINT32	ui32AuStartAddr;
	UINT8 	*pui8VirStartPtr;
	UINT32	ui32AuEndAddr;
	UINT32	ui32AuSize;
	UINT32	ui32DTS;
	UINT32	ui32PTS;
	UINT64	ui64TimeStamp;
	UINT32	ui32uID;

	UINT32	ui32Width;
	UINT32	ui32Height;
	UINT32	ui32FrameRateRes_Parser;
	UINT32	ui32FrameRateDiv_Parser;

	UINT32	ui32AuqOccupancy;

	BOOLEAN	bFlushed;
} S_VDC_CALLBACK_BODY_ESINFO_T;

typedef struct
{
	UINT32	ui8Vch;
	UINT32	u32CodecType_Config;

	enum
	{
		VDC_DECINFO_UPDATE_HDR_NONE = 0,
		VDC_DECINFO_UPDATE_HDR_SEQINFO,
		VDC_DECINFO_UPDATE_HDR_PICINFO,
		VDC_DECINFO_UPDATE_HDR_32bits = 0x20110714
	} eCmdHdr;

	union
	{
		struct
		{
			UINT32	u32IsSuccess;
			UINT32	u32Profile;
			UINT32	u32Level;
			UINT32	u32Aspectratio;
			UINT32	ui32uID;
			UINT32	u32PicWidth;
			UINT32	u32PicHeight;
			UINT32	ui32FrameRateRes;
			UINT32	ui32FrameRateDiv;
			UINT32	ui32RefFrameCount;
			UINT32	ui32ProgSeq;
		} seqInfo;

		struct
		{
			UINT32	u32DecodingSuccess;
			UINT32	ui32YFrameAddr;
			UINT32	ui32CFrameAddr;
			UINT32	u32BufStride;
			UINT32	u32Aspectratio;
			UINT32	u32PicWidth;
			UINT32	u32PicHeight;
			UINT32	u32CropLeftOffset;
			UINT32	u32CropRightOffset;
			UINT32	u32CropTopOffset;
			UINT32	u32CropBottomOffset;
			E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
			BOOLEAN	bFieldPicture;
			UINT32	ui32DisplayPeriod;
			SINT32	si32FrmPackArrSei;
			E_DISPQ_3D_LR_ORDER_T eLR_Order;
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

			struct
			{
				UINT32 ui32DTS;
				UINT64 ui64TimeStamp;
				UINT32 ui32uID;
			} sDTS;
			struct
			{
				UINT32 ui32PTS;
				UINT64 ui64TimeStamp;
				UINT32 ui32uID;
			} sPTS;

			UINT32	ui32InputGSTC;
			UINT32	ui32FeedGSTC;
			UINT32	ui32FlushAge;

			UINT32	u32NumOfErrMBs;
			UINT32	u32PicType;
			UINT32	u32ActiveFMT;
			UINT32	u32ProgSeq;
			UINT32	u32ProgFrame;
			UINT32	u32LowDelay;
			BOOLEAN	bVariableFrameRate;

			struct
			{
				UINT32		ui32Residual;
				UINT32		ui32Divider;
			} sFrameRate_Parser;
			struct
			{
				UINT32		ui32Residual;
				UINT32		ui32Divider;
			} sFrameRate_Decoder;

			UINT32	ui32DqOccupancy;

		} picInfo;
	} u;
} S_VDC_CALLBACK_BODY_DECINFO_T;

typedef struct
{
	UINT8	ui8Vch;
	UINT8	bReset;
	UINT32	ui32Addr;
	UINT32	ui32Size;
} S_VDC_CALLBACK_BODY_REQUESTRESET_T;


void VDC_Init( void (*_fpVDC_FeedInfo)(S_VDC_CALLBACK_BODY_ESINFO_T *pFeedInfo),
				void (*_fpVDC_DecInfo)(S_VDC_CALLBACK_BODY_DECINFO_T *pDecInfo),
				void (*_fpVDC_RequestReset)(S_VDC_CALLBACK_BODY_REQUESTRESET_T *pRequestReset) );
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
					);
void VDC_Close(UINT8 ui8VdcCh);
BOOLEAN VDC_UpdateAuQ(UINT8 ui8VdcCh, S_VDC_AU_T *psVesSeqAu);
void VDC_Reset(UINT8 ui8VdcCh);
BOOLEAN VDC_Flush(UINT8 ui8VdcCh, UINT32 ui32FlushAge);
BOOLEAN VDC_IsSeqInit(UINT8 ui8VdcCh);
UINT32 VDC_AU_Q_Occupancy(UINT8 ui8VdcCh);
UINT32 VDC_DQ_Occupancy(UINT8 ui8VdcCh);
BOOLEAN VDC_FeedKick(void);
BOOLEAN VDC_SetPicScanMode(UINT8 ui8VdcCh, UINT8 ui8ScanMode);
BOOLEAN VDC_SetSrcScanType(UINT8 ui8VdcCh, UINT8 ui8ScanType);
BOOLEAN VDC_SetUserDataEn(UINT8 ui8VdcCh, UINT32 u32Enable);
BOOLEAN VDC_GetUserDataInfo(UINT8 ui8VdcCh, UINT32 *pui32Address, UINT32 *pui32Size);
UINT32 VDC_GetAllocatedDPB(UINT8 ui8VdcCh);
BOOLEAN VDC_ReconfigDPB(UINT8 ui8VdcCh);
BOOLEAN VDC_RegisterDPB(UINT8 ui8VdcCh, S_VDEC_DPB_INFO_T *psVdecDpb, UINT32 ui32BufStride);
BOOLEAN VDC_DEC_PutUsedFrame(UINT8 ui8VdcCh, UINT32 ui32ClearFrameAddr);

UINT32 VDC_MSVC_ISR(UINT32 ui32ISRMask);
UINT32 VDC_VP8_ISR(UINT32 ui32ISRMask);



#ifdef __cplusplus
}
#endif
#endif /* _VDEC_VDC_DRV_H_ */
