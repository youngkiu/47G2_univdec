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
 *  driver interface header for vdec device. ( used only within kdriver )
 *	vdec device will teach you how to make device driver with new platform.
 *
 *  @author		seokjoo.lee (seokjoo.lee@lge.com)
 *  @version	1.0
 *  @date		2009.12.30
 *
 *  @addtogroup lg1152_vdec
 *	@{
 */

#ifndef	_VDEC_DRV_H_
#define	_VDEC_DRV_H_

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/
//#include "debug_util.h"
#include "vdec_cfg.h"
#include "vdec_kapi.h"
//#include "vdec_sdec_com.h"
#include "mcu/vdec_type_defs.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Extern Function Prototype Declaration
----------------------------------------------------------------------------------------*/
extern	int      VDEC_Init(void);
extern	void     VDEC_Cleanup(void);

/*----------------------------------------------------------------------------------------
	Extern Variables
----------------------------------------------------------------------------------------*/
#if 0
extern	int		g_vdec_debug_fd;
#endif

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _VDEC_DRV_H_ */

/** @} */
