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
 *  main configuration file for vdec device
 *	vdec device will teach you how to make device driver with new platform.
 *
 *  author		sooya.joo@lge.com
 *  version		1.0
 *  date		2009.12.30
 *  note		Additional information.
 *
 *  @addtogroup lg1152_vdec
 *	@{
 */

#ifndef	_VDEC_CFG_H_
#define	_VDEC_CFG_H_

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/
#include "base_types.h"

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/

/* DO NOT USE THIS except for array size; use gpVdecCfg->nChannel*/

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------------------------*/
/**
*/
typedef struct
{
	char*	mem_name;	///< name of dpb
	UINT32	mem_base;	///< base address.
	UINT32	mem_size;	///< total buffer size(total) in bytes.

} LX_VDEC_MEM_T;

typedef struct
{
	LX_VDEC_MEM_T mcu[2];
	LX_VDEC_MEM_T msvc[2];
	LX_VDEC_MEM_T mvcol;
	LX_VDEC_MEM_T cpb;
	LX_VDEC_MEM_T ipc;
	LX_VDEC_MEM_T ramlog;
	LX_VDEC_MEM_T dpb;
} LX_VDEC_MEM_CFG_T;

extern LX_VDEC_MEM_CFG_T gMemCfgVdec[];

#if 0
typedef struct
{
	UINT32	vdec_rev;		///< vdec h/w  revision.
	UINT32	boda_ver;		///< boda f/w  version.
	UINT32	mcu_ver;		///< MCU  f/w  version.

	UINT32	fEnabled:24,	///< enabled vdec channel.
			nChannel:8;		///< number of channels

	UINT32	nPesDec :8,		///< number of PES Decoder.
			nLipQue :8,		///< number of Lip Sync. Queue.
			nBodaIP :8,		///< number of H/W Decoder Core.
			nMCU	:4,		///< number of MCU.
			nMSVC	:4;		///< number of MSVC
	UINT32	irqnum;
} LX_VDEC_CFG_T;

/*----------------------------------------------------------------------------------------
	Extern Function Prototype Declaration
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Extern Variables
----------------------------------------------------------------------------------------*/

extern LX_VDEC_MEM_CFG_T *gpVdecMem, gMemCfgVdec[];
extern LX_VDEC_CFG_T	*gpVdecCfg;

void VDEC_INIT_ProbeConfig(void);
#endif
#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _VDEC_CFG_H_ */

/** @} */

