
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
 *  @author     sooya.joo@lge.com
 *  @version    1.0
 *  date		2011.03.11
 *  @note       Additional information.
 */

#ifndef	_VDEC_PRINT_H_
#define	_VDEC_PRINT_H_

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/
#ifdef __XTENSA__
#include "ipc_dbg.h"
#else
#include "debug_util.h"
#endif

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/
//log level
#define ERROR				0	// 0x0001
#define WARNING				1	// 0x0002
#define DBG_SYS				2	// 0x0004
#define _TRACE				3	// 0x0008
#define MONITOR				4	// 0x0010
#define DBG_IPC				5	// 0x0020
#define DBG_VES				6	// 0x0040
#define DBG_FEED			7	// 0x0080
#define DBG_VDC				8	// 0x0100
#define DBG_DEIF			9	// 0x0200

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/

#define _printf(...)	//do nothing
#define _printf2(...) //do nothing
#define _printf3(...) //do nothing
#define _printf4(...) //do nothing
#ifdef __XTENSA__
#define VDEC_KDRV_Message(_idx, format, args...) IPC_DBG_Send(_idx, "(MCU)"format, ##args)
#else
#define VDEC_KDRV_Message(_idx, format, args...) DBG_PRINT(g_vdec_debug_fd, _idx, format "\n", ##args)
#define VDEC_PRINT(format, args...)		DBG_PRINT(  g_vdec_debug_fd, 0, format, ##args)

extern int g_vdec_debug_fd;
#endif
/*----------------------------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Extern Function Prototype Declaration
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Extern Variables
----------------------------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _VDEC_PRINT_H_ */

/** @} */
