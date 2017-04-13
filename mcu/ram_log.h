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
 * main driver implementation for de device.
 * de device will teach you how to make device driver with new platform.
 *
 * author     youngki.lyu@lge.com
 * version    1.0
 * date       2011.05.08
 * note       Additional information.
 *
 */

#ifndef _RAM_LOG_H_
#define _RAM_LOG_H_


#include "vdec_type_defs.h"


#ifdef __cplusplus
extern "C" {
#endif



enum
{
	RAM_LOG_MOD_VES	= 0,
	RAM_LOG_MOD_VDC,
	RAM_LOG_MOD_MSVC_CMD,
	RAM_LOG_MOD_MSVC_RSP,
	RAM_LOG_MOD_DQ,
	RAM_LOG_MOD_NUM
};

typedef struct
{
	UINT64 ui64TimeStamp;
	UINT32 ui32GSTC;
	UINT32 ui32Size;
	UINT32 ui32PTS;
	UINT32 ui32DTS;
	UINT32 ui32WrPtr;
	UINT32 wParam0;
} gVESLogMem;

typedef struct
{
	UINT64 ui64TimeStamp;
	UINT32 ui32GSTC;
	UINT8 ui8VDCch;
	UINT8 ui8bRingBuf;
	UINT8 ui8AuType;
	UINT8 ui8TryCnt;
	UINT32 ui32DTS;
	UINT32 ui32STC;
	UINT32 ui32RdPtr;
	UINT32 ui32WrPtr;
} gVDCLogMem;

typedef struct
{
	UINT64 ui64TimeStamp;
	UINT32 ui32GSTC;
	UINT32 ui32ClearIdx;
	UINT32 wDispMark;
	UINT16 ui16AuType;
	UINT16 ui16resv;
	UINT32 ui32AuAddr;
	UINT32 ui32MSVCRdPtr;
} gMSVCCmdLogMem;

typedef struct
{
//	UINT64 ui64TimeStamp;
	UINT32 ui32StartGSTC;
	UINT32 resv;
	UINT32 ui32GSTC;
	UINT32 ui32Size;
	UINT16 ui16DispIdx;
	UINT16 ui16DecodedIdx;
	UINT32 wDispMark;
	UINT32 ui32RdPtr;
	UINT32 ui32WrPtr;
} gMSVCLogMem;

typedef struct
{
	UINT64 ui64TimeStamp;
	UINT32 ui32GSTC;
	UINT32 ui32Size;
	UINT32 ui32PTS;
	UINT32 ui32STC;
	UINT8 ui8DispqUseNum;
	UINT8 ui8PtsContinuity;
	UINT8 ui8StcPtsMatched;
	UINT8 ui8FrameIdx;
	UINT16 ui16FrameIdxBits;
	UINT16 ui32ClearFrameIdxBits;
} gDQLogMem;

void RAM_LOG_Init(void);
BOOLEAN RAM_LOG_Write(UINT32 ui32ModID, void *pLogBody);
UINT32 RAM_LOG_Read(UINT32 ui32ModID, UINT32 *pui32Body, UINT32 ui32DstSize);

#ifdef __cplusplus
}
#endif

#endif /* _IPC_CMD_H_ */

