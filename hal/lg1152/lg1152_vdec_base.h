/****************************************************************************************
 *   DTV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   COPYRIGHT(c) 1998-2010 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work covered by this copyright hereon
 *   may be reproduced, stored in a retrieval system, in any form
 *   or by any means, electronic, mechanical, photocopying, recording
 *   or otherwise, without the prior written  permission of LG Electronics.
 ***************************************************************************************/

/** @file
 *
 *  Brief description.
 *  Detailed description starts here.
 *
 *  @author     sooya.joo@lge.com
 *  @version    1.0
 *  @date       2011-04-05
 *  @note       Additional information.
 */

#ifndef	_LG1152_VDEC_REG_BASE_H_
#define	_LG1152_VDEC_REG_BASE_H_

#ifdef LG1152_VDEC_BASE
#undef LG1152_VDEC_BASE
#endif


#define MCU_ROM_BASE				0x50000000
#define MCU_RAM0_BASE				0x60000000
#define MCU_RAM1_BASE				0x80000000
#define MCU_RAM2_BASE				0xa0000000
#define MCU_REG_BASE				0xf0000000

#define MCU_ROM_MAX_SIZE			0x10000000
#define MCU_RAM_MAX_SIZE			0x20000000

#define VDEC_REG_OFFSET				0x00003000

#define LG1152_VDEC_BASE			(0xC0000000 + VDEC_REG_OFFSET)

/////////////////////////////////////////////////////////////////
//	VDEC DEFINE
/////////////////////////////////////////////////////////////////
#define	VDEC_MAIN_ISR_HANDLE		80
#define	VDEC_MCU_ISR_HANDLE			81

#define	PDEC_NUM_OF_CHANNEL			3
#define	SRCBUF_NUM_OF_CHANNEL		2
#define	AUIB_NUM_OF_CHANNEL			(PDEC_NUM_OF_CHANNEL + 0)	// <= (PDEC_NUM_OF_HW_CHANNEL + SRCBUF_NUM_OF_CHANNEL)
#define	CPB_NUM_OF_CHANNEL			AUIB_NUM_OF_CHANNEL
#define	LQC_NUM_OF_CHANNEL			4

#define	MSVC_NUM_OF_CORE			2
#define	MSVC_NUM_OF_CHANNEL			4

#define	NUM_OF_MSVC_CHANNEL			(MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL)
#define	NUM_OF_VP8_CHANNEL			1
#define	NUM_OF_SWDEC_CHANNEL		2

#define	VDEC_BASE_CHANNEL_VP8		NUM_OF_MSVC_CHANNEL
#define	VDEC_BASE_CHANNEL_SWDEC		(NUM_OF_MSVC_CHANNEL+NUM_OF_VP8_CHANNEL)

#define	VDEC_NUM_OF_CHANNEL			(NUM_OF_MSVC_CHANNEL+NUM_OF_VP8_CHANNEL+NUM_OF_SWDEC_CHANNEL)

/////////////////////////////////////////////////////////////////
//	VDEC DRAM1 Memory Map (VDEC Main Memory Map)
/////////////////////////////////////////////////////////////////
#define	VDEC_MCU_SROM_BASE		0x70000000
#define	VDEC_MCU_SROM_SIZE		0x00080000

#define	VDEC_MCU_SRAM_BASE		(VDEC_MCU_SROM_BASE+VDEC_MCU_SROM_SIZE)		//0x70080000
#define	VDEC_MCU_SRAM_SIZE		0x00080000

#define	VDEC_MSVC_CORE0_BASE	(VDEC_MCU_SRAM_BASE+VDEC_MCU_SRAM_SIZE)		//0x70100000
#define	VDEC_MSVC_SIZE			0x001E3000
#define	VDEC_MSVC_CORE1_BASE	(VDEC_MSVC_CORE0_BASE+VDEC_MSVC_SIZE)		//0x702E3000

#define	VDEC_MSVC_MVCOL_BUF_BASE	(VDEC_MSVC_CORE1_BASE+VDEC_MSVC_SIZE)		//0x704C6000
#define	VDEC_MSVC_MVCOL_SIZE		0x00040000
#define	VDEC_MSVC_MVCOL_BUF_SIZE	(VDEC_MSVC_MVCOL_SIZE*32)						//0x00800000

#define	VDEC_CPB_BASE			(VDEC_MSVC_MVCOL_BUF_BASE+VDEC_MSVC_MVCOL_BUF_SIZE)	//0x70CC6000
#define	VDEC_CPB_SIZE			0x01000000

#define	VDEC_SECURE_CPB_BASE	0x34000000
#define	VDEC_SECURE_CPB_SIZE	0x00700000

#define	VDEC_SHM_BASE			(VDEC_CPB_BASE+VDEC_CPB_SIZE)					//0x71CC6000
#define	VDEC_SHM_SIZE			0x00100000

#define	VDEC_RAM_LOG_BUF_BASE	(VDEC_SHM_BASE+VDEC_SHM_SIZE)				//0x71DC6000
#define	VDEC_RAM_LOG_BUF_SIZE	0xA000		// 40KByte
#define	VDEC_DBG_BUF_SIZE		0x120000

#if (KDRV_PLATFORM == KDRV_COSMO_PLATFORM)
#define	VDEC_DPB_BASE			0x72000000
#define	VDEC_DPB_SIZE			0x06000000										//0x05FA0000 = (0x330000 * 30)
#else
#define	VDEC_DPB_BASE			(VDEC_RAM_LOG_BUF_BASE+VDEC_DBG_BUF_SIZE)	//0x71EE6000
#define	VDEC_DPB_SIZE			(0x330000 * 22)									//0x04620000
#endif

/////////////////////////////////////////////////////////////////
//	VDEC DRAM0 Memory Map (VDEC Memory Map for Korea Dual 3D)
/////////////////////////////////////////////////////////////////
#define	VDEC_DPB0_BASE			0x20000000
#define	VDEC_DPB0_SIZE			0x02260000

/////////////////////////////////////////////////////////////////
//	VDEC DRAM0 Memory Map (VDEC Memory Map for GCD TVP)
/////////////////////////////////////////////////////////////////
#define	VDEC_TVP_CPB_BASE		0x34000000
#define	VDEC_TVP_CPB_SIZE		0x00700000

#define	VDEC_TVP_TZ_BASE		(VDEC_TVP_CPB_BASE+VDEC_TVP_CPB_SIZE)
#define	VDEC_TVP_TZ_SIZE		0x00700000

#define	VDEC_TVP_REF_BASE		(VDEC_TVP_TZ_BASE+VDEC_TVP_TZ_SIZE)
#define	VDEC_TVP_REF_SIZE		0x00400000

/////////////////////////////////////////////////////////////////
//	VDEC MCU Memory Map
/////////////////////////////////////////////////////////////////
#define	VDEC_MCU_SRAM0_BASE		VDEC_MCU_SRAM_BASE
#define	VDEC_MCU_SRAM1_BASE		0x0
#define	VDEC_MCU_SRAM2_BASE		0x0

#endif /* _LG1152_VDEC_REG_BASE_H_ */

