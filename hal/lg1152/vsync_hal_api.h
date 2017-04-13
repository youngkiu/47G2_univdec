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
 * date       2011.05.28
 * note       Additional information.
 *
 */

#ifndef _SYNC_HAL_API_h
#define _SYNC_HAL_API_h

#include "../../mcu/vdec_type_defs.h"
#include "lg1152_vdec_base.h"
#include "de_ipc_hal_api.h"


#ifdef __cplusplus
extern "C" {
#endif



#define	VSYNC_NUM_OF_CHANNEL			DE_IPC_NUM_OF_CHANNEL

#define	VSYNC_FIELDRATE_MAX				60



void VSync_HAL_Init(void);

UINT32 VSync_HAL_GetVersion(void);
void VSync_HAL_Reset(void);
UINT32 VSync_HAL_UTIL_GCD(UINT32 a, UINT32 b);
BOOLEAN VSync_HAL_SetVsyncField(UINT8 ui8VSyncCh, UINT32 u32FrameRateRes, UINT32 u32FrameRateDiv, BOOLEAN bInterlaced);
void VSync_HAL_RestartAllVsync(void);
void VSync_HAL_PrintReg(void);



#ifdef __cplusplus
}
#endif


#endif // endif of _VDEC_HAL_API_h

