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
 * date       2011.02.23
 * note       Additional information.
 *
 */

#ifndef _TOP_HAL_API_h
#define _TOP_HAL_API_h


#include "../../mcu/vdec_type_defs.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	MACH0							= 0,
	MACH1							= 1,
	NOT_ALLOWED_MEMACCESS		= 2,
	SIMPLE_DE0						= 3,
	VSYNC0							= 4,
	VSYNC1							= 5,
	VSYNC2							= 6,
	VSYNC3							= 7,
	LQ0_DONE						= 8,
	LQ1_DONE						= 9,
	LQ2_DONE						= 10,
	LQ3_DONE						= 11,
	PDEC0_AU_DETECT				= 12,
	PDEC1_AU_DETECT				= 13,
	PDEC2_AU_DETECT				= 14,
	BUFFER_STATUS					= 16,
	SIMPLE_DE1						= 17,
	MACH0_SRST_DONE				= 18,
	MACH1_SRST_DONE				= 19,
	PDEC_SRST_DONE				= 20,
	G1DEC_SRST_DONE				= 21,
	VSYNC4							= 22,
	G1DEC							= 23
}VDEC_INTR_T;

typedef enum{
	PDEC0_CPB_ALMOST_FULL							= 0,
	PDEC0_CPB_ALMOST_EMPTY							= 1,
	PDEC0_AUB_ALMOST_FULL							= 2,
	PDEC0_AUB_ALMOST_EMPTY							= 3,
	PDEC0_BDRC_ALMOST_FULL							= 4,
	PDEC0_BDRC_ALMOST_EMPTY							= 5,
	PDEC0_IES_ALMOST_FULL							= 6,
	PDEC1_CPB_ALMOST_FULL							= 8,
	PDEC1_CPB_ALMOST_EMPTY							= 9,
	PDEC1_AUB_ALMOST_FULL							= 10,
	PDEC1_AUB_ALMOST_EMPTY							= 11,
	PDEC1_BDRC_ALMOST_FULL							= 12,
	PDEC1_BDRC_ALMOST_EMPTY							= 13,
	PDEC1_IES_ALMOST_FULL							= 14,
	PDEC2_CPB_ALMOST_FULL							= 16,
	PDEC2_CPB_ALMOST_EMPTY							= 17,
	PDEC2_AUB_ALMOST_FULL							= 18,
	PDEC2_AUB_ALMOST_EMPTY							= 19,
	PDEC2_BDRC_ALMOST_FULL							= 20,
	PDEC2_BDRC_ALMOST_EMPTY							= 21,
	PDEC2_IES_ALMOST_FULL							= 22,
	LQC0_ALMOST_EMPTY								= 24,
	LQC1_ALMOST_EMPTY								= 25,
	LQC2_ALMOST_EMPTY								= 26,
	LQC3_ALMOST_EMPTY								= 27,
	LQC0_ALMOST_FULL								= 28,
	LQC1_ALMOST_FULL								= 29,
	LQC2_ALMOST_FULL								= 30,
	LQC3_ALMOST_FULL								= 31,
}BUFFER_INTR_T;

void TOP_HAL_Init(void);
UINT32 TOP_HAL_GetVersion(void);
void TOP_HAL_ResetMach(UINT8 vcore_num);
void TOP_HAL_ResetPDECAll(void);
void TOP_HAL_SetPdecInputSelection(UINT8 ui8PdecCh, UINT8 ui8TECh);
UINT8 TOP_HAL_GetPdecInputSelection(UINT8 ui8PdecCh);
void TOP_HAL_EnablePdecInput(UINT8 ui8PdecCh);
void TOP_HAL_DisablePdecInput(UINT8 ui8PdecCh);
UINT8 TOP_HAL_IsEnablePdecInput(UINT8 ui8PdecCh);
void TOP_HAL_SetLQSyncMode(UINT8 ui8LQCh, UINT8 ui8Vsync);
UINT8 TOP_HAL_GetLQSyncMode(UINT8 ui8LQCh);
void TOP_HAL_SetLQInputSelection(UINT8 ui8LQCh, UINT8 ui8PdecCh);
UINT8 TOP_HAL_GetLQInputSelection(UINT8 ui8LQCh);
void TOP_HAL_EnableLQInput(UINT8 ui8LQCh);
void TOP_HAL_DisableLQInput(UINT8 ui8LQCh);
UINT8 TOP_HAL_IsEnableLQInput(UINT8 ui8LQCh);
void TOP_HAL_SetMachIntrMode(UINT8 vcore_num, UINT8 IntrMode);
UINT8 TOP_HAL_GetMachIntrMode(UINT8 vcore_num);
UINT8 TOP_HAL_GetMachIdleStatus(UINT8 vcore_num);
UINT8 TOP_HAL_GetMachUnderRunStatus(UINT8 vcore_num);
void TOP_HAL_EnableExtIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_DisableExtIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_ClearExtIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_ClearExtIntrMsk(UINT32 ui32IntrMsk);
void TOP_HAL_DisableExtIntrAll(void);
UINT8 TOP_HAL_IsExtIntrEnable(VDEC_INTR_T eVdecIntrSrc);
UINT32 TOP_HAL_GetExtIntrStatus(void);
void TOP_HAL_EnableInterIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_DisableInterIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_ClearInterIntr(VDEC_INTR_T ui32IntrSrc);
void TOP_HAL_ClearInterIntrMsk(UINT32 ui32IntrMsk);
void TOP_HAL_DisableInterIntrAll(void);
UINT8 TOP_HAL_IsInterIntrEnable(VDEC_INTR_T ui32IntrSrc);
UINT32 TOP_HAL_GetInterIntrStatus(void);
void TOP_HAL_EnableBufIntr(BUFFER_INTR_T ui32IntrSrc);
void TOP_HAL_DisableBufIntr(BUFFER_INTR_T ui32IntrSrc);
void TOP_HAL_ClearBufIntr(BUFFER_INTR_T ui32IntrSrc);
void TOP_HAL_ClearBufIntrMsk(UINT32 ui32IntrMsk);
void TOP_HAL_DisableBufIntrAll(void);
UINT8 TOP_HAL_IsBufIntrEnable(BUFFER_INTR_T ui32IntrSrc);
UINT32 TOP_HAL_GetBufIntrStatus(void);
UINT32 TOP_HAL_GetGSTCC(void);
void TOP_HAL_PrintReg(void);

#ifdef __cplusplus
}
#endif

#endif // endif of _TOP_HAL_API_h

