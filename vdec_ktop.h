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
 * date       2011.04.01
 * note       Additional information.
 *
 */

#ifndef _VDEC_KTOP_H_
#define _VDEC_KTOP_H_


#include "mcu/vdec_type_defs.h"
#include "ves/ves_drv.h"
#include <linux/kernel.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
	VDEC_SRC_SDEC0		= 0,
	VDEC_SRC_SDEC1		= 1,
	VDEC_SRC_SDEC2		= 2,
	VDEC_SRC_TVP		= 3,
	VDEC_SRC_BUFF0		= 4,
	VDEC_SRC_BUFF1		= 5,
	VDEC_SRC_GRP_BUFF0	= 6,
	VDEC_SRC_GRP_BUFF1	= 7,
	VDEC_SRC_UINT32 = 0x20120418
} VDEC_SRC_T;

typedef enum
{
	VDEC_DST_DE0			= 0,
	VDEC_DST_DE1			= 1,
	VDEC_DST_BUFF			= 2,
	VDEC_DST_GRP_BUFF0		= 3,
	VDEC_DST_GRP_BUFF1		= 4,
	VDEC_DST_UINT32 = 0x20120418
} VDEC_DST_T;

typedef enum
{
	VDEC_PIC_SCAN_ALL,			///< decode IPB frame.
	VDEC_PIC_SCAN_I,			///< I picture only (PB skip)
	VDEC_PIC_SCAN_IP,			///< IP picture only (B skip only)
	VDEC_PIC_SCAN_INVALID,
	VDEC_PIC_SCAN_UINT32 = 0x20120418
} VDEC_PIC_SCAN_T;

typedef enum
{
	VDEC_KTOP_NULL,
	VDEC_KTOP_READY,
	VDEC_KTOP_PLAY,
	VDEC_KTOP_PLAY_STEP,
	VDEC_KTOP_PLAY_FREEZE,
	VDEC_KTOP_PAUSE,
	VDEC_KTOP_UINT32 = 0x20120418
} VDEC_KTOP_State;

typedef enum
{
	VDEC_KTOP_VAU_SEQH = 1,
	VDEC_KTOP_VAU_SEQE,
	VDEC_KTOP_VAU_DATA,
	VDEC_KTOP_VAU_EOS,
	VDEC_KTOP_VAU_UNKNOWN,
	VDEC_KTOP_VAU_UINT32 = 0x20130206,
} VDEC_KTOP_VAU_T;

#define	VDEC_KTOP_INVALID					0xFF

#define	VDEC_SPEED_RATE_NORMAL		1000
#define	VDEC_SPEED_RATE_HALF		(VDEC_SPEED_RATE_NORMAL/2)
#define	VDEC_SPEED_RATE_x2			(VDEC_SPEED_RATE_NORMAL*2)
#define	VDEC_SPEED_RATE_x4			(VDEC_SPEED_RATE_NORMAL*4)
#define	VDEC_SPEED_RATE_x8			(VDEC_SPEED_RATE_NORMAL*8)
#define	VDEC_SPEED_RATE_x16			(VDEC_SPEED_RATE_NORMAL*16)
#define	VDEC_SPEED_RATE_x32			(VDEC_SPEED_RATE_NORMAL*32)
#define	VDEC_SPEED_RATE_INVALID		0x7FFFFFFF


// type 1 : pdec, 2 : lq, 3: msvc, 4: dq, 5: de_if
UINT8 VDEC_KTOP_GetChannel(UINT8 ui8VdecCh, UINT8 type)	;

void VDEC_KTOP_Init(void);
UINT8 VDEC_KTOP_Restart(UINT8 ui8Vch);
UINT8 VDEC_KTOP_Open(	UINT8 ui8Vch,
							UINT8 ui8Vcodec,
							VDEC_SRC_T eInSrc,
							VDEC_DST_T eOutDst,
							BOOLEAN bNoAdpStr,
							BOOLEAN bForThumbnail,
							UINT32 ui32DecodeOffset_bytes,
							BOOLEAN b512bytesAligned,
							UINT32 ui32ErrorMBFilter,
							UINT32 ui32Width, UINT32 ui32Height,
							BOOLEAN bDualDecoding,
							BOOLEAN bLowLatency,
							BOOLEAN bUseGstc,
							UINT32 ui32DisplayOffset_ms,
							UINT8 ui8ClkID,
							UINT8 *pui8VesCh, UINT8 *pui8VdcCh, UINT8 *pui8VdsCh);
void VDEC_KTOP_Close(UINT8 ui8Vch);
void VDEC_KTOP_CloseAll(void);
int VDEC_KTOP_Play(UINT8 ui8Vch, SINT32 speed);
int VDEC_KTOP_Step(UINT8 ui8Vch);
int VDEC_KTOP_Freeze(UINT8 ui8VdecCh);
int VDEC_KTOP_Pause(UINT8 ui8Vch);
void VDEC_KTOP_Reset(UINT8 ch);
void VDEC_KTOP_Flush(UINT8 ch);
VDEC_KTOP_State VDEC_KTOP_GetState(UINT8 ui8VdecCh);
int VDEC_KTOP_SetLipSync(UINT8 ui8VdecCh, UINT32 bEnable);
int VDEC_KTOP_SetPicScanMode(UINT8 ui8VdecCh, UINT8 ui8Mode);
int VDEC_KTOP_SetSrcScanType(UINT8 ui8VdecCh, UINT8 ui8ScanType);
SINT32 VDEC_KTOP_SetUserDataEn(UINT8 ui8VdecCh, UINT32 u32Enable);
int VDEC_KTOP_GetUserDataInfo(UINT8 ui8VdecCh, UINT32 *pui32Address, UINT32 *pui32Size);
void VDEC_KTOP_SetDBGDisplayOffset(UINT8 ui8VdecCh, UINT32 ui32DisplayOffset_ms);
int VDEC_KTOP_SetBaseTime(UINT8 ui8VdecCh, UINT8 ui8ClkID, UINT32 stcBaseTime, UINT32 ptsBaseTime);
int VDEC_KTOP_GetBaseTime(UINT8 ui8VdecCh, UINT8 ui8ClkID, UINT32 *pstcBaseTime, UINT32 *pptsBaseTime);

int VDEC_KTOP_GetBufferStatus(UINT8 ui8Vch, UINT32 *pui32CpbSize, UINT32 *pui32CpbDepth, UINT32 *pui32AuibSize, UINT32 *pui32AuibDepth);

BOOLEAN VDEC_KTOP_UpdateCompressedBuffer(UINT8 ui8VdecCh,
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
									UINT32 ui32Height);
BOOLEAN VDEC_KTOP_UpdateGraphicBuffer(UINT8 ui8Vch,
									SINT32 si32FreeFrameFd,
									UINT32 ui32BufStride,
									UINT32 ui32PicWidth,
									UINT32 ui32PicHeight,
									UINT64 ui64TimeStamp,	// ns
									UINT32 ui32TimeStamp_90kHzTick,
									UINT32 ui32FrameRateRes,
									UINT32 ui32FrameRateDiv,
									UINT32 ui32uID);

int VDEC_KTOP_GetDisplayFps(UINT8 ui8VdecCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
int VDEC_KTOP_GetDropFps(UINT8 ui8VdecCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);

int VDEC_KTOP_SetFreeDPB(UINT8 ui8Vch, SINT32 si32FreeFrameFd);
int VDEC_KTOP_ReconfigDPB(UINT8 ui8Vch);
int VDEC_KTOP_GetRunningFb(UINT8 ui8Vch, UINT32 *pui32PicWidth, UINT32 *pui32PicHeight, UINT32 *pui32uID, UINT64 *pui64TimeStamp, UINT32 *pui32PTS, SINT32 *psi32FrameFd);


#ifdef __cplusplus
}
#endif

#endif // endif of _VDEC_KTOP_H_

