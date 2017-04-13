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
 *  Brief description.
 *  Detailed description starts here.
 *
 *  author     seokjoo.lee
 *  version    1.0
 *  date       2011-04-07
 *
 *  @addtogroup lg1152_vdec
 *	@{
 */

#ifndef	_VDEC_NOTI_H_
#define	_VDEC_NOTI_H_

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/
#include "vdec_kapi.h"
#include "vds/disp_q.h"


#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/
/**
 * checks notify mask and set trigger to process of whcich channel should be waken-up.
 * @param _ch		[IN] channel to be checked.
 * @param _msk		[IN] currently happened notification mask.
 * @param _bTrig	[OUT] trigger mask for each channel.
 */
#define VDEC_CHECK_NOTIFY( _ch, _msk, _bTrig)													\
											if( (_msk) & VDEC_NOTI_GetMaskToCheck(_ch) )		\
											{													\
												_bTrig |= 1<<(_ch);								\
											}

/*----------------------------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------------------------*/
typedef struct
{
	UINT32	ui32AuType;
	UINT32	ui32AuStartAddr;
	UINT8 	*pui8VirStartPtr;
	UINT32	ui32AuEndAddr;
	UINT32	ui32AuSize;
	UINT32	ui32DTS;
	UINT32	ui32PTS;
	UINT64	ui64TimeStamp;
	UINT32	ui32uID;
	UINT32	ui32InputGSTC;

	UINT32	ui32Width;
	UINT32	ui32Height;
	UINT32	ui32FrameRateRes_Parser;
	UINT32	ui32FrameRateDiv_Parser;

	BOOLEAN	bRet;
	UINT32	ui32AuqOccupancy;
	UINT32	ui32CpbOccupancy;
	UINT32	ui32CpbSize;

} S_VDEC_NOTI_CALLBACK_ESINFO_T;

typedef struct
{
	UINT32	u32CodecType_Config;

	enum
	{
		KDRV_NOTI_DECINFO_UPDATE_HDR_NONE = 0,
		KDRV_NOTI_DECINFO_UPDATE_HDR_SEQINFO,
		KDRV_NOTI_DECINFO_UPDATE_HDR_PICINFO,
		KDRV_NOTI_DECINFO_UPDATE_HDR_32bits = 0x20110714
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
			UINT32	ui32RefFrameCount;
			UINT32	ui32ProgSeq;
			UINT32	ui32FrameRateRes;
			UINT32	ui32FrameRateDiv;
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

			SINT32	si32FrameFd;

		} picInfo;
	} u;
} S_VDEC_NOTI_CALLBACK_DECINFO_T;

typedef struct
{
	UINT32	ui32NumDisplayed;
	UINT32	ui32DqOccupancy;
	BOOLEAN	bPtsMatched;
	BOOLEAN	bDropped;
	UINT32	ui32PicWidth;
	UINT32	ui32PicHeight;

	UINT32	ui32uID;
	UINT64	ui64TimeStamp;
	UINT32	ui32DisplayedPTS;

	struct
	{
		UINT32 	ui32PicType;
		UINT32	ui32Rpt_ff;
		UINT32	ui32Top_ff;
		UINT32	ui32BuffAddr;
		UINT32	ui32Size;
	} sUsrData;

	UINT32	ui32FrameAddr;
	SINT32	si32FrameFd;

} S_VDEC_NOTI_CALLBACK_DISPINFO_T;


/*----------------------------------------------------------------------------------------
	Extern Function Prototype Declaration
----------------------------------------------------------------------------------------*/
void VDEC_NOTI_EnableCallback( UINT8 ui8Ch, BOOLEAN bInput, BOOLEAN bFeed, BOOLEAN bDec, BOOLEAN bDisp );
BOOLEAN VDEC_NOTI_GetCbInfo( UINT8 ui8Ch, VDEC_KDRV_NOTI_INFO_T *pInfo );
BOOLEAN VDEC_NOTI_GetInput( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_ES_T *pInput );
BOOLEAN VDEC_NOTI_GetFeed( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_ES_T *pFeed );
BOOLEAN VDEC_NOTI_GetSeqh( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_SEQH_T *pSeqh );
BOOLEAN VDEC_NOTI_GetPicd( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_PICD_T *pPicd );
BOOLEAN VDEC_NOTI_GetDisp( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_DISP_T *pDisp );
BOOLEAN VDEC_NOTI_GetLatestDecUID( UINT8 ui8Ch, UINT32 *pui32UID );
BOOLEAN VDEC_NOTI_GetLatestDispUID( UINT8 ui8Ch, UINT32 *pui32UID );

extern void VDEC_NOTI_UpdateInputInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_ESINFO_T *pInputInfo);
extern void VDEC_NOTI_UpdateFeedInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_ESINFO_T *pFeedInfo);
extern void VDEC_NOTI_UpdateDecodeInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_DECINFO_T *pDecInfo);
extern void VDEC_NOTI_UpdateDisplayInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_DISPINFO_T *pDispInfo);

extern void VDEC_NOTI_Init(void);
extern void VDEC_NOTI_InitOutputInfo(UINT8 ch);

extern int  VDEC_NOTI_Alloc(UINT8 ch, UINT32 length);
extern void VDEC_NOTI_Free(UINT8 ch );
extern void VDEC_NOTI_GarbageCollect(UINT8 ch);

extern void   VDEC_NOTI_SetMask(UINT8 ch, UINT32 mskNotify);
extern UINT32 VDEC_NOTI_GetMask(UINT8 ch);
extern UINT32 VDEC_NOTI_GetMaskToCheck(UINT8 ch);

extern void VDEC_NOTI_SaveSEQH    (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_SEQH_T	*pSeqh);
extern void VDEC_NOTI_SavePICD    (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_PICD_T	*pOut);
extern void VDEC_NOTI_SaveUSRD    (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_USRD_T	*pUsrd);
extern void VDEC_NOTI_SaveDISP    (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_DISP_T	*pDisp);
extern void VDEC_NOTI_SaveERR     (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_ERR_T		*pErr);
extern void VDEC_NOTI_SaveAFULL   (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_AFULL_T	*pAFull);
extern void VDEC_NOTI_SaveAEMPTY  (UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_AEMPTY_T	*pAEmpty);
extern void VDEC_NOTI_SaveRTIMEOUT(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_RTIMEOUT_T*pTimeout);

extern signed long VDEC_NOTI_WaitTimeout(UINT8 ch, signed long timeout);

extern void VDEC_NOTI_Wakeup(UINT8 ch);

extern int  VDEC_NOTI_CopyToUser(UINT8 ch, char __user *buffer, size_t buffer_size);

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _VDEC_NOTI_H_ */

/** @} */
