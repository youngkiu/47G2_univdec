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
 * date       2011.11.08
 * note       Additional information.
 *
 */

#ifndef _VDEC_VSYNC_DRV_H_
#define _VDEC_VSYNC_DRV_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "de_if_drv.h"


#define		VSYNC_MAX_NUM_OF_MULTI_CHANNEL		DE_IF_NUM_OF_CHANNEL
#define		SYNC_INVALID_CHANNEL				0xFF


#ifdef __cplusplus
extern "C" {
#endif



void VSync_Init(void);
UINT8 VSync_Open(E_DE_IF_DST_T eDeIfDstCh, UINT8 ui8SyncCh, BOOLEAN bDualDecoding, BOOLEAN bFixedVSync, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bInterlaced);
void VSync_Close(UINT8 ui8VSyncCh, E_DE_IF_DST_T eDeIfDstCh, UINT8 ui8LipSyncCh);
UINT32 VSync_CalculateFrameDuration(UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv);
UINT32 VSync_GetFrameRateResDiv(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv);
UINT32 VSync_GetFrameRateDuration(UINT8 ui8SyncCh);
BOOLEAN VSync_SetVsyncField(UINT8 ui8VSyncCh, E_DE_IF_DST_T eDeIfDstCh, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bInterlaced);



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_VSYNC_DRV_H_ */

