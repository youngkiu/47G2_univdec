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
 * date       2011.11.26
 * note       Additional information.
 *
 */

#ifndef _VDEC_PTS_H_
#define _VDEC_PTS_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#if (defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__))
#include "disp_q.h"
#endif



typedef enum
{
	STC_PTS_NONE = 0,
	STC_PTS_DISCARD,
	STC_PTS_SMOOTH_DISCARD,
	STC_PTS_MATCHED,
	STC_PTS_SMOOTH_WAITING,
	STC_PTS_WAITING,
	STC_PTS_FREERUN,
	STC_PTS_UINT32 = 0x20110716
} E_STC_PTS_MATCHED_T;


typedef enum
{
	PTS_MATCH_ENABLE = 0,
	PTS_MATCH_FREERUN_BASED_SYNC,
	PTS_MATCH_FREERUN_IGNORE_SYNC,
	PTS_MATCH_MODE_UINT32 = 0x20120330
} E_PTS_MATCH_MODE_T;


#define		VDEC_UNKNOWN_PTS			0xFFFFFFFE
#define		PTS_INVALID_FRAME			0x0

typedef struct
{
	UINT32	ui32Ch;
	UINT32	ui32FrameAddr;
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

} S_VDS_CALLBACK_BODY_DISPINFO_T;

typedef struct
{
	UINT8	ui8Vch;
	enum
	{
		DQ_STATUS_WRAPAROUND = 0,
		DQ_STATUS_UNDERRUN,
		DQ_STATUS_32bits = 0x20110922
	} eBufStatus;
} S_VDS_CALLBACK_BODY_DQSTATUS_T;




#ifdef __cplusplus
extern "C" {
#endif


#ifdef __XTENSA__
void PTS_Init( void );
#else
void PTS_Init( void (*_fpVDS_DispInfo)(S_VDS_CALLBACK_BODY_DISPINFO_T *pDispInfo),
				void (*_fpVDS_DqStatus)(S_VDS_CALLBACK_BODY_DQSTATUS_T *pDqStatus) );
#endif
UINT8 PTS_Open(UINT8 ui8SyncCh, UINT8 ui8SrcBufCh, UINT8 ui8Vch, UINT32 ui32DisplayOffset_ms, UINT32 ui32NumOfDq, UINT8 ui8Vcodec, BOOLEAN bDualDecoding, E_PTS_MATCH_MODE_T eMatchMode, UINT32 ui32FlushAge);
void PTS_Close(UINT8 ui8SyncCh);
void PTS_Debug_Set_DisplayOffset(UINT8 ui8SyncCh, UINT32 ui32DisplayOffset_ms);
void PTS_Start(UINT8 ui8SyncCh, BOOLEAN bStep);
void PTS_Stop(UINT8 ui8SyncCh);

void PTS_Set_MatchMode(UINT8 ui8SyncCh, E_PTS_MATCH_MODE_T eMatchMode);

void PTS_Set_BaseTime(UINT8 ui8SyncCh, UINT8 ui8ClkID, UINT32 ui32BaseTime, UINT32 ui32BasePTS);
void PTS_Get_BaseTime(UINT8 ui8SyncCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS);
void PTS_Clear_BaseTime(UINT8 ui8SyncCh);
BOOLEAN PTS_IsDTSMode(UINT8 ui8SyncCh);



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_PTS_H_ */

