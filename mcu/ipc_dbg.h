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
 * version    1.0
 * date       2011.05.08
 * note       Additional information.
 *
 */

#ifndef _IPC_DBG_H_
#define _IPC_DBG_H_


#include "vdec_type_defs.h"



#ifdef __cplusplus
extern "C" {
#endif



void IPC_DBG_Init(void);

#ifdef __XTENSA__
BOOLEAN IPC_DBG_Send(UINT32 mode, const char *fmt, ...);
#else
void IPC_DBG_Receive(void);
#endif



#ifdef __cplusplus
}
#endif

#endif /* _IPC_DBG_H_ */

