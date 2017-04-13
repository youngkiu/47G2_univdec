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

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/
#define _VDEC_MEM_CFG_768MB

/*----------------------------------------------------------------------------------------
	File Inclusions
----------------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include "vdec_cfg.h"
#include "hal/lg1152/lg1152_vdec_base.h"

#include "mcu/vdec_print.h"

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
	External Function Prototype Declarations
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	External Variables
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	global Variables
----------------------------------------------------------------------------------------*/
/**
VDEC Memory configuration.

@code
Example in Verification S/W
---- CPB area(vdec Input)
ch0_stream_base/size : VDEC_BUFFER_BASE + msvc_buffer_size, VDEC_CH0_CPB_SIZE;
ch1_stream_base/size : VDEC_VCS_BASE, VDEC_VCS_CPB_SIZE;
ch2_stream_base/size : VDEC_CHB_BASE, VDEC_CHB_CPB_SIZE;
ch3_stream_base/size : VDEC_JPEG_BUFFER_BASE, VDEC_JPEG_CPB_SIZE;
---- DPB area(vdec Output)
ch3_yy_offset/cc_offset: JPEG_YCBCR_SIZE,JPEG_YCBCR_SIZE*2

// VDEC_FrameBufferInitialize()
FrameBaseAddr = VDEC_BUFFER_BASE + msvc_buffer_size + ch0_stream_size;
coMvFrameSize = {0x40000,0x1E000,0x40000}
coMvTotalSize = frame_num * coMvFrameSize;

StrideY   = { PIC_STRIDE=2048, PIC_CHB_STRIDE=2048, VCS_PIC_STRIDE=1024, JPEG_FRAME_STRIDE=1024 }
PicWidth  = ALIGNUP(16, PicWidth);
PicHeight = ALIGNUP(32, PicHeight{1088,1088,480});
LumaFrameSize = strideY * PicHeight; //
LumaBaseAddr  = FrameBaseAddr + coMvTotalSize;
ChromaBaseAddr= LumaBaseAddr  + LumaTotalSize;

MSVC_BUFFER_BASE:      [0x0a2d0000 0x000B0000 720896] -> 0x0a380000
{
CODE_BUFFER_BASE:      [0x0a2d0000 0x00021000 135168] -> 0x0a2f1000
PARA_BUFFER_BASE:      [0x0a2f1000 0x00001000   4096] -> 0x0a2f2000
PSPS_BUFFER_SIZE:      [0x0a2f2000 0x0000A000  40960] -> 0x0afc0000
SLIC_BUFFER_BASE:      [0x0a2fc000 0x00004000  16384] -> 0x0a300000
WORK_BUFFER_BASE:      [0x0a300000 0x00050000 327680] -> 0x0a350000
USER_BUFFER_BASE:      [0x0a350000 0x00030000 196608] -> 0x0a380000
}

CH0 CPB_BUF_BASE:      [0x0a380000 0x00400000 4MB]    -> 0x0a780000]
Co-Located MV#0 BUFFER  0x0a780000~0x0a980000         -> 0x0a980000
VDEC LUMA#0 Chroma#0	0x0a980000 0x0ba80000 0x00220000 0x00110000
VDEC LUMA#1 Chroma#1	0x0aba0000 0x0bb90000
VDEC LUMA#2 Chroma#2	0x0adc0000 0x0bca0000
VDEC LUMA#3 Chroma#3	0x0afe0000 0x0bdb0000
VDEC LUMA#4 Chroma#4	0x0b200000 0x0bec0000
VDEC LUMA#5 Chroma#5	0x0b420000 0x0bfd0000
VDEC LUMA#6 Chroma#6	0x0b640000 0x0c0e0000
VDEC LUMA#7 Chroma#7	0x0b860000 0x0c1f0000         -> 0x0c300000

-> actually set from VDEC_RegisterFrameBuffer();
@endcode
*/

LX_VDEC_MEM_CFG_T gMemCfgVdec[] =
{
	{
#ifdef KDRV_GLOBAL_LINK
		// mcu srom / sram
		{
		{"srom", 0x00000000, 0x00000000},
		{"sram", 0x00000000, 0x00000000},
		},
		// msvc 0 / msvc 1
		{
		{"msvc0", 0x00000000, 0x00000000},
		{"msvc1", 0x00000000, 0x00000000},
		},
		// msvc MV_Col
		{"mvcol", 0x00000000, 0x00000000},
		// cpb
		{"cpb", 0x00000000, 0x00000000},
		// inter-pool
		{"shm", 0x00000000, 0x00000000},
		// Ram Log
		{"ramlog", 0x00000000, 0x00000000},
		// dpb
		{"dpb", 0x00000000, VDEC_DPB0_SIZE},
#else
		// mcu srom / sram
		{
		{"srom", 0x00000000, 0x00000000},
		{"sram", 0x00000000, 0x00000000},
		},
		// msvc 0 / msvc 1
		{
		{"msvc0", 0x00000000, 0x00000000},
		{"msvc1", 0x00000000, 0x00000000},
		},
		// msvc MV_Col
		{"mvcol", 0x00000000, 0x00000000},
		// cpb
		{"cpb", 0x00000000, 0x00000000},
		// inter-pool
		{"shm", 0x00000000, 0x00000000},
		// Ram Log
		{"ramlog", 0x00000000, 0x00000000},
		// dpb
		{"dpb", VDEC_DPB0_BASE, VDEC_DPB0_SIZE},
#endif
	},
	{
#ifdef KDRV_GLOBAL_LINK
		// mcu srom / sram
		{
			{"srom", 0x00000000, VDEC_MCU_SROM_SIZE},
			{"sram", 0x00000000, VDEC_MCU_SRAM_SIZE},
		},
		// msvc 0 / msvc 1
		{
			{"msvc0", 0x00000000, VDEC_MSVC_SIZE},
			{"msvc1", 0x00000000, VDEC_MSVC_SIZE},
		},
		// msvc MV_Col
		{"mvcol", 0x00000000, VDEC_MSVC_MVCOL_BUF_SIZE},
		// cpb
		{"cpb", 0x00000000, VDEC_CPB_SIZE},
		// inter-pool
		{"shm", 0x00000000, VDEC_SHM_SIZE},
		// Ram Log
		{"ramlog", 0x00000000, VDEC_DBG_BUF_SIZE},
		// dpb
#if (KDRV_PLATFORM == KDRV_COSMO_PLATFORM)
		{"dpb", 0x00000000, 0x00000000},
#else
		{"dpb", 0x00000000, VDEC_DPB_SIZE},
#endif
#else
		// mcu srom / sram
		{
			{"srom", VDEC_MCU_SROM_BASE, VDEC_MCU_SROM_SIZE},
			{"sram", VDEC_MCU_SRAM_BASE, VDEC_MCU_SRAM_SIZE},
		},
		// msvc 0 / msvc 1
		{
			{"msvc0", VDEC_MSVC_CORE0_BASE, VDEC_MSVC_SIZE},
			{"msvc1", VDEC_MSVC_CORE1_BASE, VDEC_MSVC_SIZE},
		},
		// msvc MV_Col
		{"mvcol", VDEC_MSVC_MVCOL_BUF_BASE, VDEC_MSVC_MVCOL_BUF_SIZE},
		// cpb
		{"cpb", VDEC_CPB_BASE, VDEC_CPB_SIZE},
		// inter-pool
		{"shm", VDEC_SHM_BASE, VDEC_SHM_SIZE},
		// Ram Log
		{"ramlog", VDEC_RAM_LOG_BUF_BASE, VDEC_DBG_BUF_SIZE},
		// dpb
		{"dpb", VDEC_DPB_BASE, VDEC_DPB_SIZE},
#endif
	}
};

LX_VDEC_MEM_CFG_T *gpVdecMem = &gMemCfgVdec[1];

#if 0
/**
 * Configuration DB.
 *
 * Configuration List for chip revisions.
 */
static LX_VDEC_CFG_T	_gVdecCfgs[] =
{
//	{ .fEnabled = 0x0f, .nPesDec = 2, .nLipQue = 2, .nBodaIP = 1, .nMCU = 1 },	// L9S  A0
	{ .fEnabled = 0xff, .nChannel = 8, .nPesDec = 3, .nLipQue = 4, .nBodaIP = 2, .irqnum = 80, .nMCU = 1, .nMSVC = 0 },	// L9   A0
};

typedef struct
{
	UINT32	chip_rev;		///< overall chip revision. use LX_CHIP_REV() macro.
	UINT32	rev_offset;		///< register address of VDEC H/W revision.
	UINT32	vdec_rev;		///< expected H/W revision value.
	char*	name;			///< canonical name for debugging.
	LX_VDEC_CFG_T *pCfg;	///< probed Configuration pointer.
} LX_VDEC_REV_PROBE_T;

/**
 * Probe List for supported SoC Revision.
 */
static LX_VDEC_REV_PROBE_T _sVdecProbeDB[] =
{
	{ LX_CHIP_REV(L9,A0), 0xc0004400, 0x20100731, "VDEC L9 A0",   &_gVdecCfgs[0]},
	{ .rev_offset = 0,  .vdec_rev=0, .name = "VDEC Unknown", NULL }			// END Marker.
};

/**
 *
 */
LX_VDEC_CFG_T	*gpVdecCfg = &_gVdecCfgs[0];
/*----------------------------------------------------------------------------------------
	Static Function Prototypes Declarations
----------------------------------------------------------------------------------------*/
/**
 * checks configuration array and probe array dimension.
 * probe_array should bigger than configuration array.
 * generate compile error when mismatch.
 */
#define CHECK_CFG_DB_SIZE(_cfg_array, _probe_array)	switch(0) {case 0: case (NELEMENTS(_probe_array) > NELEMENTS(_cfg_array)): break; }

/*----------------------------------------------------------------------------------------
	Static Variables
----------------------------------------------------------------------------------------*/


/*========================================================================================
	Implementation Group
========================================================================================*/

/**
 * Probe Video Decoder revision.
 *
 * Usage
 * 1. make _gVdecCfgs[], based on SoC H/W specification.
 * 2. make _sVdecProbeDB[], with where to read revision value from H/w register and expected revision value.
 * 3. call this function at the very begining point of VDEC_Init();
 * 4. use returned LX_VDEC_CFG_T or gpVdecCfg.
 *
 * FIXME : initially rev_offset
 *
 * @return current pointer to configureation detected.
 */
void VDEC_INIT_ProbeConfig(void)
{
	LX_VDEC_REV_PROBE_T *pProbe;
	const char *msg_head = "vdec   ][probe revision] ";
	char *detected_name = "default";

	CHECK_CFG_DB_SIZE(_gVdecCfgs, _sVdecProbeDB);

	VDEC_KDRV_Message(DBG_SYS, "%s probing", msg_head);

	pProbe = &_sVdecProbeDB[0];

	while (pProbe->rev_offset)
	{
		if ( *((volatile UINT32 *)pProbe->rev_offset) == pProbe->vdec_rev ) break;
		pProbe++;
	}

	if ( pProbe->pCfg )
	{
		gpVdecCfg	= pProbe->pCfg;
		detected_name = pProbe->name;
	}

	VDEC_KDRV_Message(DBG_SYS, "%s %s : %08x", msg_head, detected_name, pProbe->chip_rev);
	/* else : default value unchanged */

	if (pProbe->chip_rev >= LX_CHIP_REV(L9,A0) )
	{
		gpVdecCfg = &_gVdecCfgs[0];
		gpVdecMem = &gMemCfgVdec[0];
	}
}
#endif
/** @} */
