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

#ifndef _MCU_REG_XC_h
#define _MCU_REG_XC_h


#ifndef __XTENSA__
#include <linux/kernel.h>
#endif

#include "../../mcu/vdec_type_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	MCU_ISR_IPC = 0,
	MCU_ISR_DMA
} MCU_ISR_MODE;

// VDEC Command
#define VDEC_CMD_INIT								0x00000001
#define VDEC_CMD_START								0x00000002
#define VDEC_CMD_PDEC_INPUT_MODE_SET				0x00000004
#define VDEC_CMD_CHANNEL_CLOSE						0x00000008
#define VDEC_CMD_SET_USER_DATA 						0x00000010
#define VDEC_CMD_SET_ENDIAN							0x00000020
#define VDEC_CMD_SET_FRAME							0x00000040
#define VDEC_CMD_SOFT_RESET							0x00000080
#define VDEC_CMD_LIPSYNC_MODE						0x00000100
#define VDEC_CMD_SET_DISPLAY						0x00000200
#define VDEC_CMD_SET_BUFF_LEVEL						0x00000400
#define VDEC_CMD_LIPSYNC							0x00000800
#define VDEC_CMD_LOG								0x00001000
#define VDEC_CMD_CHB_RUN							0x00002000
#define VDEC_CMD_STOP                               0x00004000
#define VDEC_CMD_AU_DETECT0                         0x01000000
#define VDEC_CMD_MCU_REQ_DONE						0x80000000

// MCU Request
#define MCU_REQ_BOOT_CODE_DOWN						0x00000001
#define MCU_REQ_PRINT_REQUEST						0x00000002
#define MCU_REQ_USER_DATA_BUF_BASE					0x00000004
#define MCU_REQ_ES_DUMP_WPTR						0x00000008
#define MCU_REQ_DISPLAY_POP 						0x00000010
#define MCU_REQ_DISPLAY_FRAME_INIT					0x00000020
#define MCU_REQ_CHB_FRAME_INFO						0x00000040
#define MCU_REQ_SEQ_HEADER							0x00000080
#define MCU_REQ_PIC_HEADER							0x00000100
#define MCU_REQ_PIC_DECODED							0x00000200
#define MCU_REQ_ERROR								0x00000400
#define MCU_REQ_ALMOST_FULL							0x00000800
#define MCU_REQ_ALMOST_EMPTY						0x00001000
#define MCU_REQ_VDEC_CMD_DONE						0x80000000
#define MCU_REQ_DBG_PRINT							0x40000000

typedef enum
{
	VDEC_RESET_ALL= 0,
	MSVC_RESET,
	CH0_FLUSH,
	CH1_FLUSH,
	CH2_FLUSH,
	WATCHDOG_RESET
} VDEC_RESET_MODE_T;

void MCU_HAL_Init(void);
void MCU_HAL_SegRunStall(UINT8 bStall);
void MCU_HAL_Reset(void);
UINT32 MCU_HAL_GetStatus(void);
void MCU_HAL_InitSystemMemory(UINT32 addrSrom, UINT32 addrSram0, UINT32 addrSram1, UINT32 addrSram2);
void MCU_HAL_SetExtIntrEvent(void);
void MCU_HAL_EnableExtIntr(int mode);
void MCU_HAL_DisableExtIntr(int mode);
void MCU_HAL_ClearExtIntr(int mode);
void MCU_HAL_DisableExtIntrAll(void);
UINT32 MCU_HAL_IsExtIntrEnable(int mode);
UINT32 MCU_HAL_GetExtIntrStatus(int mode);
void MCU_HAL_SetExtIntrStatus(int mode, int value);
void MCU_HAL_SetIntrEvent(void);
void MCU_HAL_EnableInterIntr(int mode);
void MCU_HAL_DisableInterIntr(int mode);
void MCU_HAL_ClearInterIntr(int mode);
void MCU_HAL_DisableInterIntrAll(void);
UINT32 MCU_HAL_IsInterIntrEnable(int mode);
UINT32 MCU_HAL_GetInterIntrStatus(int mode);
void MCU_HAL_SetInterIntrStatus(int mode, int value);
UINT32 MCU_HAL_GetVersion(void);

#ifdef __XTENSA__
void MCU_DMA_MemoryCopy(void *DstAddr, void *SrcAddr, int ui32Datalen);
UINT32 *MCU_HAL_TranslateAddrforMCU(UINT32 u32BusAddr, UINT32 ui32Size);
#else
void MCU_HAL_CodeDown(UINT32 SROMAddr, UINT32 SROMSize, UINT8 *mcu_code, UINT32 mcu_code_size);
#endif

#ifdef __cplusplus
}
#endif

#endif // endif of _MCU_REG_XC_h

