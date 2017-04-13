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
 * date       2011.05.03
 * note       Additional information.
 *
 */

#ifndef _VDEC_DE_IF_H_
#define _VDEC_DE_IF_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "disp_q.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	DE_IF_NUM_OF_CHANNEL			2



typedef enum
{
	DE_IF_DST_DE_IF0		= 0x0,
	DE_IF_DST_DE_IF1		= 0x1,
	DE_IF_DST_INVALID,
	DE_IF_DST_32bits		= 0x20120321,
} E_DE_IF_DST_T;


typedef struct{
	UINT32 ui32YFrameAddr;
	UINT32 ui32CFrameAddr;
	UINT32 ui32Stride;

	E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
	UINT32 ui32DisplayPeriod;
	UINT32 ui32AspectRatio;
	UINT32 ui32PicWidth;
	UINT32 ui32PicHeight;
	UINT32 ui32CropLeftOffset;
	UINT32 ui32CropRightOffset;
	UINT32 ui32CropTopOffset;
	UINT32 ui32CropBottomOffset;
	SINT32 i32FramePackArrange;
	E_DISPQ_3D_LR_ORDER_T eLR_Order;

	UINT32 ui32FrameDuration90K;
	UINT32 ui32FrameRateRes;
	UINT32 ui32FrameRateDiv;
	UINT32 ui16ParW;
	UINT32 ui16ParH;

	UINT32 ui32InputGSTC;
	UINT32 ui32PTS;
}S_DE_IF_T;



void DE_IF_Init(void);
BOOLEAN DE_IF_Open(E_DE_IF_DST_T eDeIfDstCh);
void DE_IF_Close(E_DE_IF_DST_T eDeIfDstCh);



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_DE_IF_H_ */

