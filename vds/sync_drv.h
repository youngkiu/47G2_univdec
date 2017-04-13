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
 * date       2011.05.03
 * note       Additional information.
 *
 */

#ifndef _VDEC_SYNC_DRV_H_
#define _VDEC_SYNC_DRV_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "de_if_drv.h"
#include "pts_drv.h"
#include "disp_q.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	SYNC_NUM_OF_CHANNEL		DE_IF_NUM_OF_CHANNEL



typedef	E_DE_IF_DST_T			E_SYNC_DST_T;

typedef struct{

	UINT32 ui32YFrameAddr;
	UINT32 ui32CFrameAddr;
	UINT32 ui32Stride;

	UINT32 ui32FlushAge;
	UINT32 ui32AspectRatio;
	UINT32 ui32PicWidth;
	UINT32 ui32PicHeight;
	UINT32 ui32CropLeftOffset;
	UINT32 ui32CropRightOffset;
	UINT32 ui32CropTopOffset;
	UINT32 ui32CropBottomOffset;
	E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
	UINT32 ui32DisplayPeriod;

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

	SINT32 si32FrmPackArrSei;		// 3D SEI
	E_DISPQ_3D_LR_ORDER_T eLR_Order;
	UINT16 ui16ParW;
	UINT16 ui16ParH;

	struct
	{
		UINT32 	ui32PicType;
		UINT32	ui32Rpt_ff;
		UINT32	ui32Top_ff;
		UINT32	ui32BuffAddr;
		UINT32	ui32Size;
	} sUsrData;

	UINT32 ui32InputGSTC;

}S_FRAME_BUF_T;

typedef enum
{
	SYNC_PLAY_NORMAL		= 0x0,
	SYNC_PLAY_STEP			= 0x1,
	SYNC_PLAY_FREEZE		= 0x2,
	SYNC_PLAY_32bits		= 0x20120516
} E_SYNC_PLAY_OPT_T;



#ifdef __XTENSA__
void SYNC_Init( void );
#else
void SYNC_Init( void (*_fpVDS_DispInfo)(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo),
				void (*_fpVDS_DqStatus)(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus) );
#endif
UINT8 SYNC_Open(UINT8 ui8Vch, E_SYNC_DST_T eSyncDstCh, UINT8 ui8Vcodec, UINT8 ui8SrcBufCh, UINT32 ui32DisplayOffset_ms, UINT32 ui32NumOfDq, UINT8 ui8ClkID, BOOLEAN bDualDecoding, BOOLEAN bFixedVSync, BOOLEAN bLipSyncOn, UINT32 ui32FlushAge);
void SYNC_Close(UINT8 ui8SyncCh);
void SYNC_Debug_Set_DisplayOffset(UINT8 ui8SyncCh, UINT32 ui32DisplayOffset_ms);
void SYNC_Reset(UINT8 ui8SyncCh);
void SYNC_Start(UINT8 ui8SyncCh, E_SYNC_PLAY_OPT_T ePlayOpt);
void SYNC_Stop(UINT8 ui8SyncCh);
void SYNC_Flush(UINT8 ui8SyncCh, UINT32 ui32FlushAge);

void SYNC_Set_MatchMode(UINT8 ui8SyncCh, E_PTS_MATCH_MODE_T eMatchMode);
BOOLEAN SYNC_IsDTSMode( UINT8 ui8SyncCh );

void SYNC_Set_Speed( UINT8 ui8SyncCh, SINT32 i32Speed );
SINT32 SYNC_Get_Speed( UINT8 ui8SyncCh );

void SYNC_Set_BaseTime( UINT8 ui8SyncCh, UINT8 ui8ClkID, UINT32 ui32BaseTime, UINT32 ui32BasePTS );
void SYNC_Get_BaseTime( UINT8 ui8SyncCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS );

UINT32 SYNC_GetFrameRateResDiv(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
UINT32 SYNC_GetFrameRateDuration(UINT8 ui8SyncCh);
UINT32 SYNC_GetDecodingRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
UINT32 SYNC_GetDisplayRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
UINT32 SYNC_GetDropRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
BOOLEAN SYNC_GetRunningFb(UINT8 ui8SyncCh, UINT32 *pui32PicWidth, UINT32 *pui32PicHeight, UINT32 *pui32uID, UINT64 *pui64TimeStamp, UINT32 *pui32PTS, UINT32 *pui32FrameAddr);

BOOLEAN SYNC_UpdateFrame(UINT8 ui8SyncCh, S_FRAME_BUF_T *psFrameBuf);
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
								UINT32 ui32FlushAge);
UINT32 SYNC_GetFrameBufOccupancy(UINT8 ui8SyncCh);



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_SYNC_DRV_H_ */

