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
 * date       2012.06.09
 * note       Additional information.
 *
 */

#ifndef _VDC_AU_Q_H_
#define _VDC_AU_Q_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "../ves/ves_drv.h"



#define 	AU_Q_NUM_OF_CHANNEL			3
#define	AU_Q_NUM_OF_NODE				0x800



#ifdef __cplusplus
extern "C" {
#endif



typedef struct
{
	E_VES_AU_T		eAuType;
	UINT32			ui32AuStartAddr;
	UINT8 			*pui8VirStartPtr;
	UINT32			ui32AuEndAddr;
	UINT32			ui32AuSize;

	UINT32			ui32DTS;
	UINT32			ui32PTS;
	UINT64			ui64TimeStamp;
	UINT32			ui32uID;

	UINT32			ui32InputGSTC;

	UINT32 			ui32FrameRateRes_Parser;
	UINT32 			ui32FrameRateDiv_Parser;

	UINT32			ui32Width;
	UINT32			ui32Height;
	UINT32			ui32FlushAge;

}S_VDC_AU_T;


void AU_Q_Init(void);
void AU_Q_Reset(UINT8 ch, UINT32 ui32CpbBufAddr);
BOOLEAN AU_Q_Open(UINT8 ch, UINT32 ui32NumOfAuQ, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize);
void AU_Q_Close(UINT8 ch);
BOOLEAN AU_Q_Push(UINT8 ch, S_VDC_AU_T *psAuQNode);
#if ( (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__)) )
BOOLEAN AU_Q_Pop(UINT8 ch, S_VDC_AU_T *psAuQNode);
BOOLEAN AU_Q_Peep(UINT8 ch, S_VDC_AU_T *psAuQNode);
UINT32 AU_Q_GetWrAddr(UINT8 ch);
#endif
UINT32 AU_Q_NumActive(UINT8 ch);
UINT32 AU_Q_NumFree(UINT8 ch);



#ifdef __cplusplus
}
#endif



#endif /* _VDC_AU_Q_H_ */

