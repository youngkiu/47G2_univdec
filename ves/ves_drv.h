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
 * date       2011.05.04
 * note       Additional information.
 *
 */

#ifndef _VES_DRV_H_
#define _VES_DRV_H_


#include "../mcu/vdec_type_defs.h"
#include "ves_cpb.h"
#include "../hal/lg1152/pdec_hal_api.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	VES_NUM_OF_CHANNEL			3



typedef enum
{
	VES_SRC_SDEC0		= 0x00,
	VES_SRC_SDEC1		= 0x01,
	VES_SRC_SDEC2		= 0x02,
	VES_SRC_TVP			= 0x12,
	VES_SRC_BUFF0		= 0x20,
	VES_SRC_BUFF1		= 0x21,
	VES_SRC_INVALID		= 0x30,
	VES_SRC_32bits		= 0x20120316,
} E_VES_SRC_T;


typedef enum
{
	VES_AU_SEQUENCE_HEADER		= 0x1,
	VES_AU_SEQUENCE_END			= 0x2,
	VES_AU_PICTURE_I			= 0x3,
	VES_AU_PICTURE_P			= 0x4,
	VES_AU_PICTURE_B_NOSKIP		= 0x5,
	VES_AU_PICTURE_B_SKIP		= 0x6,
	VES_AU_PICTURE_X,
	VES_AU_UNKNOWN,
	VES_AU_EOS,
	VES_AU_32bits				= 0x20130207,
} E_VES_AU_T;


typedef struct
{
	UINT32	ui32Ch;
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

	UINT32	ui32CpbOccupancy;
	UINT32	ui32CpbSize;
} S_VES_CALLBACK_BODY_ESINFO_T;


void VES_Init( void (*_fpVES_InputInfo)(S_VES_CALLBACK_BODY_ESINFO_T *pInputInfo) );
UINT8 VES_Open(UINT8 ui8Vch, E_VES_SRC_T eVesSrcCh, UINT8 ui8CodecType_Config, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bUseGstc, UINT32 ui32DecodeOffset_bytes, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned);
UINT8 VES_GetVdecCh(UINT8 ui8PdecCh);
void VES_Close(UINT8 ui8PdecCh);
void VES_Reset(UINT8 ui8PesDecCh);
void VES_Start(UINT8 ui8PdecCh);
void VES_Stop(UINT8 ui8PdecCh);
void VES_Flush(UINT8 ui8PdecCh);
BOOLEAN VES_UpdateRdPtr(UINT8 ui8VesCh, UINT32 ui32RdPhyAddr);
UINT32 VES_GetUsedBuffer(UINT8 ui8VesCh);
BOOLEAN VES_GetBufferSize(UINT8 ui8VesCh);

BOOLEAN VES_UpdateBuffer(UINT8 ui8PdecCh,
									E_VES_AU_T eAuType,
									UINT32 ui32UserBuf,
									UINT32 ui32UserBufSize,
									fpCpbCopyfunc fpCopyfunc,
									UINT32 ui32uID,
									UINT64 ui64TimeStamp,	// ns
									UINT32 ui32TimeStamp_90kHzTick,
									UINT32 ui32FrameRateRes,
									UINT32 ui32FrameRateDiv,
#define VES_WIDTH_INVALID	0x0
#define VES_HEIGHT_INVALID	0x0
									UINT32 ui32Width,
									UINT32 ui32Height,
									BOOLEAN bSeqInit);

#if (defined(USE_MCU_FOR_VDEC_VES) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
void VDEC_ISR_PIC_Detected(UINT8 ui8PdecCh);
#endif


#ifdef __cplusplus
}
#endif

#endif /* _VES_DRV_H_ */

