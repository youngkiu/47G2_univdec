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
 * date       2011.08.25
 * note       Additional information.
 *
 */

#ifndef _VP8_DRV_H_
#define _VP8_DRV_H_


#include "../mcu/vdec_type_defs.h"
#include "vdc_drv.h"



#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	UINT32	ui32AuPhyAddr;
	UINT8 	*pui8VirStartPtr;
	UINT32	ui32AuSize;
	UINT32	ui32DTS;
	UINT32	ui32PTS;
	UINT32	ui32uID;
	UINT64	ui64TimeStamp;
	UINT32	ui32InputGSTC;
	UINT32	ui32FeedGSTC;
	UINT32	ui32FrameRateRes_Parser;
	UINT32	ui32FrameRateDiv_Parser;
	UINT32	ui32FlushAge;
}VP8_CmdQ_T;

void VP8_Init(	UINT32 (*_fpVDC_GetUsedFrame)(UINT8 ui8VdcCh),
				void (*_fpVDC_ReportPicInfo)(UINT8 ui8VdcCh, VDC_REPORT_INFO_T *vdc_picinfo_param));
UINT8 VP8_Open(UINT8 ui8VdcCh);
void VP8_Close(UINT8 ui8VP8ch);
BOOLEAN VP8_Flush(UINT8 ui8VP8ch);
BOOLEAN VP8_Reset(UINT8 ui8VP8ch);
BOOLEAN VP8_IsReady(UINT8 ui8VP8ch);
BOOLEAN VP8_SetScanMode(UINT8 ui8VP8ch, UINT8 ui8ScanMode);
BOOLEAN VP8_SendCmd(UINT8 ui8VP8ch, VP8_CmdQ_T *pstVP8Aui);
BOOLEAN VP8_RegisterDPB(UINT8 ui8VP8ch, S_VDEC_DPB_INFO_T *psVdecDpb);
BOOLEAN VP8_ReconfigDPB(UINT8 ui8VP8ch);
UINT32 VP8_DECODER_ISR(UINT32 ui32ISRMask);

#ifdef __cplusplus
}
#endif

#endif /* _VP8_DRV_H_ */

