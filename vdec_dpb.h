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
 * date       2011.06.09
 * note       Additional information.
 *
 */

#ifndef _VDEC_DPB_H_
#define _VDEC_DPB_H_


#include "mcu/vdec_type_defs.h"


#define VDEC_MAX_DPB_NUM		32


#ifdef __cplusplus
extern "C" {
#endif



typedef struct
{
	UINT32			ui32DpbPoolIndex;

	UINT32			ui32NumOfFb;

	UINT32			ui32BaseAddr;					// Frame buffer base address

	UINT32			ui32FrameSizeY;
	UINT32			ui32FrameSizeCb;
	UINT32			ui32FrameSizeCr;

	struct
	{
		SINT32			si32FrameFd;
		UINT32			ui32AddrY;
		UINT32			ui32AddrCb;
		UINT32			ui32AddrCr;
 	} aBuf[VDEC_MAX_DPB_NUM];

} S_VDEC_DPB_INFO_T;



#if (!defined(USE_MCU_FOR_TOP) && defined(__XTENSA__))
void VDEC_DPB_Init(UINT32 ui32DpbPoolIndex);
#else
void VDEC_DPB_Init(UINT32 ui32DpbPoolIndex, UINT32 ui32PoolAddr, UINT32 ui32PoolSize);
#endif
UINT32 VDEC_DPB_TranslatePhyToVirtual(UINT32 ui32PhyAddr);

BOOLEAN VDEC_DPB_AllocAll(UINT32 ui32FbWidth, UINT32 ui32FbHeigth, UINT32 ui32NumOfFb, S_VDEC_DPB_INFO_T *psVdecDpb, BOOLEAN bDualDecoding);
void VDEC_DPB_FreeAll(S_VDEC_DPB_INFO_T *psVdecDpb);

UINT32 VDEC_DPB_Alloc(UINT32 ui32DpbSize);
void VDEC_DPB_Free(UINT32 ui32FbAddr);


#ifdef __cplusplus
}
#endif

#endif /* _VDEC_DPB_H_ */

