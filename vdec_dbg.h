
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
 *  VDEC Debugger submodule.
 *  Debugging facility for Video Decoder.
 *
 *  @author     seokjoo.lee(seokjoo.lee@lge.com)
 *  @version    1.0
 *  date		2011.03.11
 *  @note       Additional information.
 */

#ifndef	_VDEC_DBG_H_
#define	_VDEC_DBG_H_

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/sched.h>
#include "base_types.h"
#include "vdec_kapi.h"

#define _PROC_CPB_DUMP			// Temporary usage because of danger for security

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/
/**
 * [debug] ID for Event Counter.
 *
 * Usage example.
 *
 * If PIC_RUN_DONE interrupt happens, call VDEC_DBG_EVCNT(ch, PIC_DETECT ) at the end of handling routine.
 *
 * @see VDEC_DBG_EVCNT
 * @see VDEC_DBG_EVCNT_Clear
 */
enum {
	VDEC_DBG_EVCNT_GSTC_START,		///< start time stamp when starts logging following event.

	VDEC_DBG_EVCNT_PIC_DETECT,		///< for ch#_detect_picture ISR.
	VDEC_DBG_EVCNT_BUF_UPDATE,		///< for LX_VDEC_IO_UPDATE_BUFFER_ called.
	VDEC_DBG_EVCNT_CMD_START,		///< for command started  ISR.
	VDEC_DBG_EVCNT_PIC_DECODED,		///< for PIC_RUN_DONE     ISR.
	VDEC_DBG_EVCNT_DE_VSYNC	,		///< for de_vsync_edge    ISR.
	VDEC_DBG_EVCNT_USRD,			///< for User Data found in PIC Decoded ISR.
	VDEC_DBG_EVCNT_USRD_VALID,		///< for User Data found and it is valid.
	VDEC_DBG_EVCNT_USRD_SKIP,		///< for User Data Skipped due to bad frame index.

	VDEC_DBG_EVCNT_CPB_FLUSH,		///< for KDRV internal flush.
	VDEC_DBG_EVCNT_WATCHDOG	,		///< for S/W watchdog.
	VDEC_DBG_EVCNT_SDEC_WATCHDOG,	///< for SDEC watchdog.
	VDEC_DBG_EVCNT_LIPSYNC_LOST,	///< for lipsync lost.

	VDEC_DBG_EVCNT_DPB_REPEAT,		///< for DPB reapeat.
	VDEC_DBG_EVCNT_DPB_SKIP,		///< for DPB skip.

	VDEC_DBG_EVCNT_H264_PACKED,		///< for H.264 Packed mode detected in PIC_RUN_DONE interrupt.
	VDEC_DBG_EVCNT_FIELD_PIC,		///< for Field Picture detected in PIC_RUN_DONE picture run
	VDEC_DBG_EVCNT_OTHER_14,		///< for CH# OTHER and boda int reason 14 happens : underflow.
	VDEC_DBG_EVCNT_OTHER_UFI,		///< for CH# OTHER and boda int reason UFI(Unidentified Failure Interrupt) happens: T.T
	VDEC_DBG_EVCNT_RERUN,			///< for CH# OTHER and RE-RUN command

	VDEC_DBG_EVCNT_LQ_FULL,			///< for queue_full_status(0x2E8) : Lipsync Q full ( 0x2E8:[ 0] bit)
	VDEC_DBG_EVCNT_CQ_FULL,			///< for queue_full_status(0x2E8) : command Q full ( 0x2E8:[ 4] bit)
	VDEC_DBG_EVCNT_DQ_FULL,			///< for queue_full_status(0x2E8) : Decoded Q full ( 0x2E8:[ 8] bit)
	VDEC_DBG_EVCNT_RQ_FULL,			///< for queue_full_status(0x2E8) : Removed Q full ( 0x2E8:[12] bit)
	VDEC_DBG_EVCNT_AU_FULL,			///< for queue_full_status(0x2E8) : Removed Q full ( 0x2E8:[16] bit)
	VDEC_DBG_EVCNT_ES_FULL,			///< for queue_full_status(0x2E8) : ES Buffer full ( 0x2E8:[20] bit)
	VDEC_DBG_EVCNT_CPB_FULL,		///< for queue_full_status(0x2E8) : CPB       full ( 0x2E8:[24] bit)
	VDEC_DBG_EVCNT_CPB_EMPT,		///< for queue_full_status(0x2E8) : CPB       Empty( 0x2E8:[28] bit)

	VDEC_DBG_EVCNT_MAX,
};

/**
 * use this macro (much shorter!!)
 * instead of VDEC_DBG_EVCNT_Increase directly.
 *
 * @param _ch		[IN] which channel to log.
 * @param _sub_id	[IN] only use Event ID without "VDEC_DBG_EVCNT_"
 *
 * use VDEC_DBG_EVCNT(channel, GSTC_START)
 * -> espanded as VDEC_DBG_EVCNT_Increase(channel, VDEC_DBG_EVCNT_GSTC_START)
 *
 */
#define VDEC_DBG_EVCNT(_ch, _sub_id)			VDEC_DBG_EVCNT_Increase(_ch, VDEC_DBG_EVCNT_##_sub_id)
#define VDEC_DBG_EVCNT_FULL_ID(_ch, _full_id)	VDEC_DBG_EVCNT_Increase(_ch, _full_id)

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Extern Function Prototype Declaration
----------------------------------------------------------------------------------------*/
//#define VDEC_DBG_CHECK_CALLER_PROCESS 1
#ifdef VDEC_DBG_CHECK_CALLER_PROCESS
static inline void VDEC_DBG_CheckCallerProcess(UINT8 ui8Ch)
{
	static pid_t caller_tgid[LX_VDEC_CH_MAXN] = {0,};
	static pid_t caller_pid[LX_VDEC_CH_MAXN] = {0,};
	if ( caller_pid[ui8Ch] != task_pid_nr(current) ) {
		VDEC_KDRV_Message(NORMAL1, "vdec_%d:%s: client pid : %4d %4d %4d", ui8Ch, __FUNCTION__, task_pid_nr(current), task_tgid_nr(current),current->exit_code );
		caller_tgid[ui8Ch]	= task_tgid_nr(current);
		caller_pid[ui8Ch]	= task_pid_nr(current);
	}
}
#else
static inline void VDEC_DBG_CheckCallerProcess(UINT8 ui8Ch)	{}
#endif


void VDEC_DBG_ISRDelayCapture(int isr_position, UINT32 gstc, UINT32 captured_gstc, UINT32 isVsyncIsr);

int  VDEC_DBG_PrintStatus(UINT8	ch, char* buffer, int len, int *pEnabled);

int  VDEC_DBG_EVCNT_Init(UINT8	max_ch);
void VDEC_DBG_EVCNT_UnInit(void);
void VDEC_DBG_EVCNT_Clear(UINT8 ch, UINT32 gstc);
void VDEC_DBG_EVCNT_Increase(UINT8	ch, UINT8 ev_id);
int  VDEC_DBG_EVCNT_Print(UINT8 ch, char *buffer, int length);
int  VDEC_DBG_CHANNEL_Print(UINT8 ch, char *buffer, int length);
int  VDEC_DBG_DumpCPB(char ch, char* buffer, char **ppos, off_t offset, int buffer_length, int* eof, void* data);
int VDEC_DBG_MSVC_Cmd( char vdec_ch, char *zCmd_line);
int VDEC_DBG_MSVC_Info( char ch,  char* buffer, int buffer_length);
/*----------------------------------------------------------------------------------------
	Extern Variables
----------------------------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _VDEC_DBG_H_ */

/** @} */
