/*
	SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
	Copyright(c) 2010 by LG Electronics Inc.

	This program is free software; you can redistribute it and/or 
	modify it under the terms of the GNU General Public License
	version 2 as published by the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
	GNU General Public License for more details.
*/ 

/** @file
 *
 * main driver implementation for de device.
 * de device will teach you how to make device driver with new platform.
 *
 * author     youngki.lyu@lge.com
 * version    0.1
 * date       2010.03.11
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

#ifndef _VDEC_IO_H_
#define _VDEC_IO_H_

#include <linux/module.h>
#include <linux/cdev.h>
#include "debug_util.h"

#include "vdec_noti.h"
#include "vdec_dbg.h"

#ifdef __cplusplus
extern "C" {
#endif


#define REG_WRITE32( addr, value )	( *( volatile UINT32 * )( addr ) ) = ( volatile UINT32 )( value )
#define REG_READ32( addr )			( *( volatile UINT32 * )( addr ) )

#define MAX_NUM_INSTANCE				8		// Maximum number of decoder instances

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   External Function Prototype Declarations
 *---------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------
   enum
----------------------------------------------------------------------------------------*/
/**
 *	main control block for vdec device.
 *	each minor device has unique control block
 *
 */

/*----------------------------------------------------------------------------------------
   API
----------------------------------------------------------------------------------------*/
void VDEC_IO_Init(void);

UINT8 VDEC_GetVdecStdType(UINT8 ui8CodecType);

int VDEC_IO_OpenChannel(UINT32 ulArg);
int VDEC_IO_CloseChannel(UINT32 ulArg);
int VDEC_IO_PlayChannel(UINT32 ulArg);
int VDEC_IO_FlushChannel(UINT32 ulArg);
int VDEC_IO_EnableCallback(UINT32 ulArg);
int VDEC_IO_CallbackNoti(UINT32 ulArg);
int VDEC_IO_GetSeqh(UINT32 ulArg);
int VDEC_IO_GetPicd(UINT32 ulArg);
int VDEC_IO_GetDisp(UINT32 ulArg);
int VDEC_IO_GetRunningFb(UINT32 ulArg);
int VDEC_IO_GetLatestDecUID(UINT32 ulArg);
int VDEC_IO_GetLatestDispUID(UINT32 ulArg);
int VDEC_IO_GetDisplayFps(UINT32 ulArg);
int VDEC_IO_GetDropFps(UINT32 ulArg);
int VDEC_IO_SetBaseTime(UINT32 ulArg);
int VDEC_IO_GetBaseTime(UINT32 ulArg);
int VDEC_IO_CtrlUsrd(UINT32 ulArg);
int VDEC_IO_SetFreeDPB(UINT32 ulArg);
int VDEC_IO_ReconfigDPB(UINT32 ulArg);
int VDEC_IO_GetBufferStatus(UINT32 ulArg);

int VDEC_InitChannel(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_CloseChannel(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_StartChannel(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_StopChannel(UINT8 ui8Ch, UINT32 option);
int VDEC_FlushChannel(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_IO_Restart(void);

int VDEC_IO_SetPLAYOption(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_IO_GetPLAYOption(UINT8 ui8Ch, UINT32 ulArg);
int VDEC_IO_SetBufferLVL(UINT8 ui8Ch, UINT32 ulArg);

//misc
int VDEC_IO_GetFiemwareVersion(UINT32 ulArg);

//for buffered/file play only
int VDEC_IO_UpdateBuffer(UINT32 ulArg);
int VDEC_IO_UpdateFrameBuffer(UINT32 ulArg);
int VDEC_IO_UpdateGraphicBuffer(UINT32 ulArg);

//aux data gathering
int VDEC_IO_GetOutput(UINT8 ui8Ch, UINT32 ulArg);

//vdec debugging
int VDEC_IO_SetRegister(UINT32 ulArg);
int VDEC_IO_GetRegister(UINT32 ulArg);
int VDEC_IO_EnableLog(UINT32 ulArg);
int VDEC_IO_DbgCmd(UINT8 ui8Ch, UINT32 ulArg);

int VDEC_DBG_PrintISRDelay(char * buffer);
void VDEC_IO_DBG_DumpFPQTS(struct seq_file *m);
/*----------------------------------------------------------------------------------------
 *   External Variables
 *---------------------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif

#endif	/* _VDEC_IO_H_ */

/** @} */
