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
 *	common header file for lg1152 driver
 *
 *  @author		sooya.joo@lge.com
 *  @version	1.0
 *  @date		2011.08.03
 *
 *  @addtogroup lg115x_vdec
 */

#ifndef	_MCU_CONFIG_H_
#define	_MCU_CONFIG_H_

//#define USE_MCU_FOR_TOP
//#define USE_MCU_FOR_VDEC_VES
//#define USE_MCU_FOR_VDEC_VDC
#define USE_MCU_FOR_VDEC_VDS

#if defined(USE_MCU_FOR_VDEC_VES)||defined(USE_MCU_FOR_VDEC_VDC)||defined(USE_MCU_FOR_VDEC_VDS)
#define USE_MCU
#endif


#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _MCU_CONFIG_H_ */

