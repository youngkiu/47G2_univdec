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
 * date       2011.04.22
 * note       Additional information.
 *
 */

#ifndef _VES_CPB_H_
#define _VES_CPB_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"


#if defined(USE_MCU_FOR_TOP) || defined(USE_MCU_FOR_VDEC_VES) || defined(USE_MCU_FOR_VDEC_VDC)
	#if !(defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VES) && defined(USE_MCU_FOR_VDEC_VDC))
		#define CPB_USE_IPC_CMD
	#endif
#endif


#define		VES_CPB_CEILING_512BYTES( _addr )			(((_addr) + 0x1FF) & (~0x1FF))
#define		VES_CPB_BOTTOM_512BYTES( _addr )			(((_addr) + 0x000) & (~0x1FF))


#ifdef __cplusplus
extern "C" {
#endif


typedef 	int (*fpCpbCopyfunc)(void *, void *, int);


typedef struct
{
	UINT8	ui8Vch;
	UINT8	ui8VesCh;
	enum
	{
		CPB_STATUS_ALMOST_EMPTH = 0,
		CPB_STATUS_NORMAL,
		CPB_STATUS_ALMOST_FULL,
		CPB_STATUS_32bits = 0x20110921
	} eBufStatus;
} S_VES_CALLBACK_BODY_CPBSTATUS_T;


void VES_CPB_Init(void (*_fpVES_CpbStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus));
void VES_CPB_Reset(UINT8 ui8VesCh);
void VES_CPB_Flush(UINT8 ui8VesCh);
BOOLEAN VES_CPB_Open(UINT8 ui8VesCh, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned, BOOLEAN bIsHwPath, BOOLEAN bFromTVP);
void VES_CPB_Close(UINT8 ui8VesCh);
UINT32 VES_CPB_GetPhyWrPtr(UINT8 ui8VesCh);
UINT8 *VES_CPB_TranslatePhyWrPtr(UINT8 ui8VesCh, UINT32 ui32WrPhyAddr);
BOOLEAN VES_CPB_UpdateWrPtr(UINT8 ui8VesCh, UINT32 ui32WrPhyAddr);
BOOLEAN VES_CPB_UpdateRdPtr(UINT8 ui8VesCh, UINT32 ui32RdPhyAddr);
UINT32 VES_CPB_Write(	UINT8 ui8VesCh,
							UINT32 ui32AuStartAddr,
							UINT32 ui32AuSize,
							fpCpbCopyfunc fpCopyfunc,
							UINT32 *pui32CpbWrPhyAddr_End);
UINT32 VES_CPB_GetUsedBuffer(UINT8 ui8VesCh);
UINT32 VES_CPB_GetBufferBaseAddr(UINT8 ui8VesCh);
UINT32 VES_CPB_GetBufferSize(UINT8 ui8VesCh);
void VES_CPB_PrintUsedBuffer(UINT8 ui8VesCh);

#if ( (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__)) )
void VDEC_ISR_CPB_AlmostFull(UINT8 ui8VesCh);
void VDEC_ISR_CPB_AlmostEmpty(UINT8 ui8VesCh);
#endif


#ifdef __cplusplus
}
#endif

#endif /* _VES_CPB_H_ */

