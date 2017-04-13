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
 * DE IPC HAL.
 * (Register Access Hardware Abstraction Layer)
 *
 * author     youngki.lyu (youngki.lyu@lge.com)
 * version    0.1
 * date       2011.05.03
 * note       Additional information.
 *
 */

#ifndef _VDEC_DE_IF_HAL_API_H_
#define _VDEC_DE_IF_HAL_API_H_

#include "../../mcu/vdec_type_defs.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	DE_IPC_NUM_OF_CHANNEL		2



typedef enum {
	DE_IPC_DISPLAY_MODE_NULL = 0,
	DE_IPC_DISPLAY_MODE_TOP_FIELD = 1,
	DE_IPC_DISPLAY_MODE_BOTTOM_FIELD = 2,
	DE_IPC_DISPLAY_MODE_FRAME_PICTURE = 3,
	DE_IPC_DISPLAY_MODE_32bits = 0x20120430,
} E_DE_IPC_DISPLAY_MODE_T;


typedef enum {
	DE_IPC_3D_FRAME_LEFT = 0,
	DE_IPC_3D_FRAME_RIGHT = 1,
	DE_IPC_3D_FRAME_NONE = 2,
	DE_IPC_3D_FRAME_32bits = 0x20120728,
} E_DE_IPC_3D_LR_ORDER_T;


typedef struct{
	UINT32 ui32Y_FrameBaseAddr;
	UINT32 ui32C_FrameBaseAddr;
	UINT32 ui32Y_FrameOffset;
	UINT32 ui32C_FrameOffset;
	UINT32 ui32Stride;

	UINT32 ui32FrameIdx;
	UINT32 ui32ScalerFreeze;
	E_DE_IPC_DISPLAY_MODE_T eDisplayMode;
	UINT32 ui32FieldRepeat;
	UINT32 ui32VdecPause;
	UINT32 ui32AspectRatio;
	UINT32 ui16PicWidth;
	UINT32 ui16PicHeight;
	UINT32 ui32CropLeftOffset;
	UINT32 ui32CropRightOffset;
	UINT32 ui32CropTopOffset;
	UINT32 ui32CropBottomOffset;
	UINT32 ui32UpdateInfo;
	UINT32 ui32FramePackArrange;
	E_DE_IPC_3D_LR_ORDER_T eLR_Order;

	UINT32 ui16FrameRateRes;
	UINT32 ui16FrameRateDiv;
	UINT32 ui16ParW;
	UINT32 ui16ParH;

	UINT32 ui32PTS;
}S_DE_IPC_T;



void DE_IPC_HAL_Init(void);
BOOLEAN DE_IPC_HAL_Update(UINT8 ui8DeIpcCh, S_DE_IPC_T *psDeIpc);



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_DE_IF_HAL_API_H_ */

