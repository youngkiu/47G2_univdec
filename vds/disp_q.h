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
 * date       2012.04.14
 * note       Additional information.
 *
 */

#ifndef _VDEC_DISP_Q_H_
#define _VDEC_DISP_Q_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"



#define 	DISP_Q_NUM_OF_CHANNEL			3



#ifdef __cplusplus
extern "C" {
#endif



typedef enum {
	DISPQ_DISPLAY_INFO_NULL = 0,
	DISPQ_DISPLAY_INFO_TOP_FIELD_FIRST = 1,
	DISPQ_DISPLAY_INFO_BOTTOM_FIELD_FIRST = 2,
	DISPQ_DISPLAY_INFO_PROGRESSIVE_FRAME = 3,
	DISPQ_DISPLAY_INFO_32bits = 0x20110531,
} E_DISPQ_DISPLAY_INFO_T;


typedef enum {
	DISPQ_3D_FRAME_LEFT = 0,
	DISPQ_3D_FRAME_RIGHT = 1,
	DISPQ_3D_FRAME_NONE = 2,
	DISPQ_3D_FRAME_32bits = 0x20120114,
} E_DISPQ_3D_LR_ORDER_T;


typedef struct{
	UINT32	ui32FbId;
	UINT32	ui32YFrameAddr;
	UINT32	ui32CFrameAddr;
	UINT32	ui32Stride;

	UINT32	ui32FlushAge;
	UINT32	ui32AspectRatio;
	UINT32	ui32PicWidth;
	UINT32	ui32PicHeight;
	UINT32	ui32CropLeftOffset;
	UINT32	ui32CropRightOffset;
	UINT32	ui32CropTopOffset;
	UINT32	ui32CropBottomOffset;
	E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
	UINT32	ui32DisplayPeriod;

	struct
	{
		UINT32	ui32DTS;
		UINT64	ui64TimeStamp;
		UINT32	ui32uID;
	} sDTS;
	struct
	{
		UINT32	ui32PTS;
		UINT64	ui64TimeStamp;
		UINT32	ui32uID;
	} sPTS;
	UINT32	ui32InputGSTC;

	SINT32	i32FramePackArrange;		// 3D SEI
	E_DISPQ_3D_LR_ORDER_T eLR_Order;
	UINT16	ui16ParW;
	UINT16	ui16ParH;

	struct
	{
		UINT32 	ui32PicType;
		UINT32	ui32Rpt_ff;
		UINT32	ui32Top_ff;
		UINT32	ui32BuffAddr;
		UINT32	ui32Size;
	} sUsrData;

}S_DISPQ_BUF_T;


void DISP_Q_Init(void);
void DISP_Q_Reset(UINT8 ch);
BOOLEAN DISP_Q_Open(UINT8 ch, UINT32 ui32NumOfDq);
void DISP_Q_Close(UINT8 ch);
BOOLEAN DISP_Q_Push(UINT8 ch, S_DISPQ_BUF_T *psDisBuf);
#if ( (defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__)) )
BOOLEAN DISP_Q_PeepLast(UINT8 ch, S_DISPQ_BUF_T *psDisBuf);
BOOLEAN DISP_Q_Pop(UINT8 ch, S_DISPQ_BUF_T *psDisBuf);
#endif
UINT32 DISP_Q_NumActive(UINT8 ch);
UINT32 DISP_Q_NumFree(UINT8 ch);
UINT32 DISP_Q_ValidateFrameAddr(UINT8 ch);


#ifdef __cplusplus
}
#endif

#endif /* _VDEC_DISP_Q_H_ */

