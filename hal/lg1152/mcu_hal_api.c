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
 * author     sooya.joo@lge.com
 * version    0.1
 * date       2010.03.11
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "../../mcu/vdec_type_defs.h"
#ifdef __XTENSA__
#include <stdio.h>
#include <xtensa/xtruntime.h>
#elif !defined(DEFINE_EVALUATION)
#include "l9/base_addr_sw_l9.h"
#endif

#include "lg1152_vdec_base.h"
#include "../../mcu/os_adap.h"

#include "mcu_hal_api.h"

// L9_VDEC
#include "mcu_reg.h"

#include "../../mcu/vdec_print.h"

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
 *   External Variables
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   global Functions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   global Variables
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Static Function Prototypes Declarations
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
//static UINT32 ui32VdecCMDVirAddr;
//static UINT32 ui32VdecREQVirAddr;
//static UINT32 ui32VdecDBGVirAddr;

static volatile reg_mcu_top_t *reg_mcu_top;

void MCU_HAL_Init(void)
{
// L9_VDEC
#ifndef __XTENSA__
#ifndef DEFINE_EVALUATION
	LX_ADDR_SW_CFG_T	addr_sw_cfg_vdec_mcu;
#endif
	UINT32 de_sav = 0x34000110;		// default value : CASE 4
	UINT32 cpu_gpu = 0x000200D0;
	UINT32 cpu_shadow = 0x0C010100;
#endif
	reg_mcu_top = (reg_mcu_top_t *)VDEC_TranselateVirualAddr(VDEC_MCU_REG_BASE, 0x100);
#ifndef __XTENSA__
#ifndef DEFINE_EVALUATION
	BASE_L9_GetAddrSwCfg(ADDR_SW_CFG_VDEC_MCU, &addr_sw_cfg_vdec_mcu);
	de_sav = addr_sw_cfg_vdec_mcu.de_sav;		// default value : CASE 4
	cpu_gpu = addr_sw_cfg_vdec_mcu.cpu_gpu;
	cpu_shadow = addr_sw_cfg_vdec_mcu.cpu_shadow;
#endif
	reg_mcu_top->addr_sw_de_sav = de_sav;		// 0x24
	reg_mcu_top->addr_sw_cpu_gpu = cpu_gpu;		// 0x28
	reg_mcu_top->addr_sw_cpu_shadow = cpu_shadow;		// 0x2C
#endif
}

void MCU_HAL_SegRunStall(UINT8 bStall)
{
	reg_mcu_top->sys_ctrl.f.runstall = bStall;
}

void MCU_HAL_Reset(void)
{
	reg_mcu_top->sys_ctrl.f.runstall = 1;
	reg_mcu_top->sys_ctrl.f.sw_reset = 1;
	while(reg_mcu_top->sys_ctrl.f.sw_reset == 1) {}

	reg_mcu_top->sys_ctrl.f.runstall = 1;
	reg_mcu_top->sys_ctrl.f.sw_reset = 1;
	while(reg_mcu_top->sys_ctrl.f.sw_reset == 1) {}

	reg_mcu_top->sys_ctrl.f.runstall = 0;
}

UINT32 MCU_HAL_GetStatus(void)
{
	UINT32 mcuStatus=0;
	reg_mcu_top->sys_ctrl.f.pdbg_en = 1;

	mcuStatus = reg_mcu_top->sys_ctrl.f.pdbg_st;
	reg_mcu_top->sys_ctrl.f.pdbg_en = 0;

	return mcuStatus;
}

void MCU_HAL_InitSystemMemory(UINT32 addrSrom, UINT32 addrSram0, UINT32 addrSram1, UINT32 addrSram2)
{
	// srom offset 0xc0003710 0xaa60000
	reg_mcu_top->sys_ctrl.f.srom_offset = (addrSrom>>12);
	// sram offset 0xc0003714 0xaae0000
	reg_mcu_top->sys_ctrl.f.sram_offset_0 = (addrSram0>>12);
	reg_mcu_top->sys_ctrl.f.sram_offset_1 = (addrSram1>>12);
	reg_mcu_top->sys_ctrl.f.sram_offset_2 = (addrSram2>>12);
}

void MCU_HAL_SetExtIntrEvent(void)
{
	reg_mcu_top->intr_e_ev.f.ipc = 1;
}

void MCU_HAL_EnableExtIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_e_en.f.ipc = 1;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_e_en.f.dma = 1;
		break;
	}
}

void MCU_HAL_DisableExtIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_e_en.f.ipc = 0;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_e_en.f.dma = 0;
		break;
	}
}

void MCU_HAL_ClearExtIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_e_cl.f.ipc = 1;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_e_cl.f.dma = 1;
		break;
	}
}

void MCU_HAL_DisableExtIntrAll(void)
{
	reg_mcu_top->intr_e_en.w = 0;
	reg_mcu_top->intr_e_cl.w = 0xFFFFFFFF;
}

UINT32 MCU_HAL_IsExtIntrEnable(int mode)
{
	UINT32 benabledIntr = 0;
	switch(mode)
	{
	case MCU_ISR_IPC:
		if(reg_mcu_top->intr_e_en.f.ipc == 1)
		{
			benabledIntr = 1;
		}
		break;
	case MCU_ISR_DMA:
		if(reg_mcu_top->intr_e_en.f.dma == 1)
		{
			benabledIntr = 1;
		}
		break;
	}

	return benabledIntr;
}

UINT32 MCU_HAL_GetExtIntrStatus(int mode)
{
	UINT32 beIntrStatus = 0;
	switch(mode)
	{
	case MCU_ISR_IPC:
		if(reg_mcu_top->intr_e_st.f.ipc == 1)
		{
			beIntrStatus = 1;
		}
		break;
	case MCU_ISR_DMA:
		if(reg_mcu_top->intr_e_st.f.dma == 1)
		{
			beIntrStatus = 1;
		}
		break;
	}

	return beIntrStatus;
}

void MCU_HAL_SetExtIntrStatus(int mode, int value)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_e_st.f.ipc = value;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_e_st.f.dma = value;
		break;
	}
}

void MCU_HAL_SetIntrEvent(void)
{
	reg_mcu_top->intr_ev.f.ipc = 1;
}

void MCU_HAL_EnableInterIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_en.f.ipc = 1;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_en.f.dma = 1;
		break;
	}
}

void MCU_HAL_DisableInterIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_en.f.ipc = 0;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_en.f.dma = 0;
		break;
	}
}

void MCU_HAL_ClearInterIntr(int mode)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_cl.f.ipc = 1;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_cl.f.dma = 1;
		break;
	}
}

void MCU_HAL_DisableInterIntrAll(void)
{
	reg_mcu_top->intr_en.w = 0;
	reg_mcu_top->intr_cl.w = 0xFFFFFFFF;
}

UINT32 MCU_HAL_IsInterIntrEnable(int mode)
{
	UINT32 benabledIntr = 0;
	switch(mode)
	{
	case MCU_ISR_IPC:
		if(reg_mcu_top->intr_en.f.ipc == 1)
		{
			benabledIntr = 1;
		}
		break;
	case MCU_ISR_DMA:
		if(reg_mcu_top->intr_en.f.dma == 1)
		{
			benabledIntr = 1;
		}
		break;
	}

	return benabledIntr;
}

UINT32 MCU_HAL_GetInterIntrStatus(int mode)
{
	UINT32 beIntrStatus = 0;
	switch(mode)
	{
	case MCU_ISR_IPC:
		if(reg_mcu_top->intr_st.f.ipc == 1)
		{
			beIntrStatus = 1;
		}
		break;
	case MCU_ISR_DMA:
		if(reg_mcu_top->intr_st.f.dma == 1)
		{
			beIntrStatus = 1;
		}
		break;
	}

	return beIntrStatus;
}

void MCU_HAL_SetInterIntrStatus(int mode, int value)
{
	switch(mode)
	{
	case MCU_ISR_IPC:
		reg_mcu_top->intr_st.f.ipc = value;
		break;
	case MCU_ISR_DMA:
		reg_mcu_top->intr_st.f.dma = value;
		break;
	}
}

UINT32 MCU_HAL_GetVersion(void)
{
	return *(volatile UINT32 *)&reg_mcu_top->mcu_ver;
}

#ifdef __XTENSA__
int MCU_DMA_ddr2dram(UINT32  ui32SrcAddr, UINT32 ui32DataLen)
{
	UINT32 dma_req;

	if(ui32DataLen>0x1000)
		return -1;
#if 1
	//DMA READ
	reg_mcu_top->dma_ctrl.f.tunit = 0x100;			//TUNIT:256 bytes
	reg_mcu_top->dma_ctrl.f.sa = ui32SrcAddr;		//DDR address
	reg_mcu_top->dma_ctrl.f.len = ui32DataLen;		//LEN:4K
	reg_mcu_top->dma_ctrl.f.da = 0x1000;			//Dram Addr :0x1000
	reg_mcu_top->dma_ctrl.f.cmd = 0;				// CMD Type : ddr to dram
	reg_mcu_top->dma_ctrl.f.req = 1;				// CMD Request

	dma_req = reg_mcu_top->dma_ctrl.f.req;
	while(dma_req){
		dma_req = reg_mcu_top->dma_ctrl.f.req;
	}
#else
	//DMA READ
	reg_mcu_top->dma_ctrl.w[3] = 0x100;			//TUNIT:256 bytes
	reg_mcu_top->dma_ctrl.w[2] = ui32SrcAddr;		//DDR address
	reg_mcu_top->dma_ctrl.w[1] = (ui32DataLen&0xFFFF)<<16;		//LEN:4K
	reg_mcu_top->dma_ctrl.w[1] |= 0x1000;		//Dram Addr :0x1000
	reg_mcu_top->dma_ctrl.w[0] = 1;	// CMD Type : ddr to dram & CMD Request

	dma_req = (reg_mcu_top->dma_ctrl.w[0]&0x1);
	while(dma_req){
		dma_req = (reg_mcu_top->dma_ctrl.w[0]&0x1);
	}
#endif
	return 0;
}

int MCU_DMA_dram2ddr(UINT32 ui32DstAddr, UINT32 ui32DataLen)
{
	UINT32 dma_req;

	if(ui32DataLen>0x1000)
		return -1;
#if 1
	//DMA READ
	reg_mcu_top->dma_ctrl.f.tunit = 0x100;			//TUNIT:256 bytes
	reg_mcu_top->dma_ctrl.f.sa = ui32DstAddr;		//DDR address
	reg_mcu_top->dma_ctrl.f.len = ui32DataLen;		//LEN:4K
	reg_mcu_top->dma_ctrl.f.da = 0x1000;			//Dram Addr :0x1000
	reg_mcu_top->dma_ctrl.f.cmd = 1;				// CMD Type : dram to ddr
	reg_mcu_top->dma_ctrl.f.req = 1;				// CMD Request

	dma_req = reg_mcu_top->dma_ctrl.f.req;
	while(dma_req){
		dma_req = reg_mcu_top->dma_ctrl.f.req;
	}
#else
	//DMA READ
	reg_mcu_top->dma_ctrl.w[3] = 0x100;			//TUNIT:256 bytes
	reg_mcu_top->dma_ctrl.w[2] = ui32DstAddr;		//DDR address
	reg_mcu_top->dma_ctrl.w[1] = (ui32DataLen&0xFFFF)<<16;		//LEN:4K
	reg_mcu_top->dma_ctrl.w[1] |= 0x1000;		//Dram Addr :0x1000
	reg_mcu_top->dma_ctrl.w[0] = 3;	// CMD Type : dram to ddr & CMD Request

	dma_req = (reg_mcu_top->dma_ctrl.w[0]&0x1);
	while(dma_req){
		dma_req = (reg_mcu_top->dma_ctrl.w[0]&0x1);
	}
#endif
	return 0;
}

// DRAM to DRAM Copy
void MCU_DMA_MemoryCopy(void *DstAddr, void *SrcAddr, int ui32Datalen)
{
	UINT32 ui32TransferSize = 0;
	UINT32 ui32DstAddr = (UINT32)DstAddr;
	UINT32 ui32SrcAddr = (UINT32)SrcAddr;

	ui32Datalen = (ui32Datalen+ 3)&(~0x3);

	while(ui32TransferSize<ui32Datalen)
	{
		if((ui32Datalen - ui32TransferSize)>0x1000)
		{
			if(MCU_DMA_ddr2dram(ui32SrcAddr+ui32TransferSize, 0x1000)==-1)
			{
				VDEC_KDRV_Message(ERROR, "DMA_Fail %d", __LINE__);
				break;
			}
			if(MCU_DMA_dram2ddr(ui32DstAddr+ui32TransferSize, 0x1000)==-1)
			{
				VDEC_KDRV_Message(ERROR, "DMA_Fail %d", __LINE__);
				break;
			}
			ui32TransferSize += 0x1000;
		}
		else
		{
			if(MCU_DMA_ddr2dram(ui32SrcAddr+ui32TransferSize, (ui32Datalen - ui32TransferSize))==-1)
			{
				VDEC_KDRV_Message(ERROR, "DMA_Fail %d", __LINE__);
				break;
			}
			if(MCU_DMA_dram2ddr(ui32DstAddr+ui32TransferSize, (ui32Datalen - ui32TransferSize))==-1)
			{
				VDEC_KDRV_Message(ERROR, "DMA_Fail %d", __LINE__);
				break;
			}
			ui32TransferSize += (ui32Datalen - ui32TransferSize);
		}
	}
}

UINT32 *MCU_HAL_TranslateAddrforMCU(UINT32 u32BusAddr, UINT32 ui32Size)
{
	UINT32 u32ROM_Base;
	UINT32 u32RAM_Base[3];
	UINT32 u32Start_RAM_Addr;
	UINT32 *pu32MCU_Addr = 0x0;
	UINT32 u32MaxSlotAddr;
	UINT32 u32MCUAddrOffset;

	// Register Area
	if(u32BusAddr > LG1152_VDEC_BASE)
	{
		u32MCUAddrOffset = (u32BusAddr-VDEC_REG_OFFSET) & 0x0FFFFFFF;
		pu32MCU_Addr = (UINT32*)(MCU_REG_BASE+u32MCUAddrOffset);
	}
	// SRAM, SROM Area
	else
	{
		u32ROM_Base = reg_mcu_top->sys_ctrl.w[4];
		u32RAM_Base[0] = reg_mcu_top->sys_ctrl.w[5];
		u32RAM_Base[1] = reg_mcu_top->sys_ctrl.w[6];
		u32RAM_Base[2] = reg_mcu_top->sys_ctrl.w[7];
		u32Start_RAM_Addr = 0xFFFFFFFF;

		// SRAM2 Area
		if((u32RAM_Base[2]!=0x0)&&(pu32MCU_Addr==0x0))
		{
			u32MaxSlotAddr = u32RAM_Base[2]+MCU_RAM_MAX_SIZE;
			if((u32BusAddr>=u32RAM_Base[2]) && (u32BusAddr < u32MaxSlotAddr))
			{
				u32MCUAddrOffset = u32BusAddr - u32RAM_Base[2];
				pu32MCU_Addr = (UINT32*)(MCU_RAM2_BASE+u32MCUAddrOffset);
			}
			else
				pu32MCU_Addr = 0x0;
		}

		if((u32RAM_Base[1]!=0x0)&&(pu32MCU_Addr==0x0))
		{
			u32MaxSlotAddr = u32RAM_Base[1]+MCU_RAM_MAX_SIZE;
			if((u32BusAddr>=u32RAM_Base[1]) && (u32BusAddr < u32MaxSlotAddr))
			{
				u32MCUAddrOffset = u32BusAddr - u32RAM_Base[1];
				pu32MCU_Addr = (UINT32*)(MCU_RAM1_BASE+u32MCUAddrOffset);
			}
			else
				pu32MCU_Addr = 0x0;
		}

		if((u32RAM_Base[0]!=0x0)&&(pu32MCU_Addr==0x0))
		{
			u32MaxSlotAddr = u32RAM_Base[0]+MCU_RAM_MAX_SIZE;
			if((u32BusAddr>=u32RAM_Base[0]) && (u32BusAddr < u32MaxSlotAddr))
			{
				u32MCUAddrOffset = u32BusAddr - u32RAM_Base[0];
				pu32MCU_Addr = (UINT32*)(MCU_RAM0_BASE+u32MCUAddrOffset);
			}
			else
				pu32MCU_Addr = 0x0;
		}

		if((u32ROM_Base!=0x0)&&(pu32MCU_Addr==0x0))
		{
			if(u32RAM_Base[0]-u32ROM_Base < MCU_ROM_MAX_SIZE)
				u32MaxSlotAddr = u32RAM_Base[0];
			else
				u32MaxSlotAddr = u32ROM_Base+MCU_ROM_MAX_SIZE;

			if((u32BusAddr>=u32ROM_Base) && (u32BusAddr < u32MaxSlotAddr))
			{
				u32MCUAddrOffset = u32BusAddr - u32ROM_Base;
				pu32MCU_Addr = (UINT32*)(MCU_RAM0_BASE+u32MCUAddrOffset);
			}
			else
				pu32MCU_Addr = 0x0;
		}

		if(pu32MCU_Addr!=0x0)
			xthal_set_region_attribute((void*)pu32MCU_Addr, ui32Size, XCHAL_CA_BYPASS, 0);
	}

	return pu32MCU_Addr;
}
#else
void MCU_HAL_CodeDown(UINT32 SROMAddr, UINT32 SROMSize, UINT8 *mcu_code, UINT32 mcu_code_size )
{
	UINT8 *pu8RegPtr;
	UINT32 ui32FWVirAddr, ui32Count;

	ui32FWVirAddr = (UINT32) VDEC_TranselateVirualAddr(SROMAddr, SROMSize);

	VDEC_KDRV_Message(_TRACE, "Vdec MCU base address-virt[0x%08X]", SROMAddr);

	reg_mcu_top->sys_ctrl.f.runstall = 1;

	//	VDEC MCU FirmwareDown
	pu8RegPtr = (UINT8 *)ui32FWVirAddr;
//	memcpy((void *)pu8RegPtr, (void *)mcu_code, sizeof(mcu_code));
	for (ui32Count = 0; ui32Count < mcu_code_size; ui32Count ++)
	{
		*pu8RegPtr = mcu_code[ui32Count];
		pu8RegPtr++;
	}

	// run 0xc0003700 0x0
	reg_mcu_top->sys_ctrl.f.sw_reset = 1;
	while(reg_mcu_top->sys_ctrl.f.sw_reset == 1) {}

	reg_mcu_top->sys_ctrl.f.runstall = 0;

//	reg_mcu_top->intr_e_en.f.ipc   = 0x1;
	VDEC_ClearVirualAddr((void *)ui32FWVirAddr);
	VDEC_KDRV_Message(_TRACE, "+ VDEC MCU is Loaded");
}

#endif
/** @} */

