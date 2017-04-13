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
#include "vdec_type_defs.h"
#include "mcu_config.h"

#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/mcu_hal_api.h"

#if (defined(USE_MCU_FOR_VDEC_VES) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
#include "../ves/ves_drv.h"
#endif
#if ( (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__)) )
#include "../ves/ves_cpb.h"
#include "../ves/ves_auib.h"
#endif
#if (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__))
#include "../vdc/vdc_drv.h"
#endif
#if (defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__))
#include "../vds/vsync_drv.h"
#endif

#include "ipc_req.h"
#include "ipc_dbg.h"

#include "vdec_print.h"
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


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
void VDEC_ISR_Handler(void)
{
	UINT32 u32IntReason = 0;
	UINT32 u32BufIntReason = 0;
	UINT32 u32BufClearInt = 0;

#ifdef __XTENSA__
	u32IntReason = TOP_HAL_GetInterIntrStatus();
#else // !__XTENSA__
	u32IntReason = TOP_HAL_GetExtIntrStatus();
#endif // ~__XTENSA__

//	VDEC_KDRV_Message(_TRACE, "VDEC ISR :: %x \n", u32IntReason);

#if (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__))
	u32IntReason = VDC_MSVC_ISR(u32IntReason);
	u32IntReason = VDC_VP8_ISR(u32IntReason);
#endif

	if (u32IntReason&(1<<LQ3_DONE)) {
//		VDEC_KDRV_Message(_TRACE, "LQ Done Interrupt for IPC");
#ifdef __XTENSA__		// Receive IPC in MCU
		TOP_HAL_ClearInterIntrMsk(1<<LQ3_DONE);
#else //!__XTENSA__
//		TOP_HAL_ClearExtIntrMsk(1<<LQ3_DONE);
#endif //~__XTENSA__
		u32IntReason = u32IntReason&~(1<<LQ3_DONE);

#ifdef __XTENSA__		// Receive IPC in MCU
//		IPC_CMD_Receive();
#endif //~__XTENSA__
	}

#if (defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__))
	if (u32IntReason&(1<<VSYNC0)) {
		VDEC_ISR_VSync(0);
	}
	if (u32IntReason&(1<<VSYNC1)) {
		VDEC_ISR_VSync(1);
	}
	if (u32IntReason&(1<<VSYNC2)) {
		VDEC_ISR_VSync(2);
	}
	if (u32IntReason&(1<<VSYNC3)) {
		VDEC_ISR_VSync(3);
	}
	if (u32IntReason&(1<<VSYNC4)) {
	}
#endif

#if (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__))
	if (u32IntReason&(1<<BUFFER_STATUS)) {
		u32BufIntReason = TOP_HAL_GetBufIntrStatus();

		if(u32BufIntReason&(1<<LQC0_ALMOST_EMPTY))
		{
			u32BufClearInt |= (1<<LQC0_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<LQC1_ALMOST_EMPTY))
		{
			u32BufClearInt |= (1<<LQC1_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<LQC2_ALMOST_EMPTY))
		{
			u32BufClearInt |= (1<<LQC2_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<LQC3_ALMOST_EMPTY))
		{
			u32BufClearInt |= (1<<LQC3_ALMOST_EMPTY);
		}

		if(u32BufIntReason&(1<<PDEC0_CPB_ALMOST_FULL))
		{
			VDEC_ISR_CPB_AlmostFull(0);
			u32BufClearInt |= (1<<PDEC0_CPB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC0_CPB_ALMOST_EMPTY))
		{
			VDEC_ISR_CPB_AlmostEmpty(0);
			u32BufClearInt |= (1<<PDEC0_CPB_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<PDEC0_AUB_ALMOST_FULL))
		{
			VDEC_ISR_AUIB_AlmostFull(0);
			u32BufClearInt |= (1<<PDEC0_AUB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC0_AUB_ALMOST_EMPTY))
		{
			VDEC_ISR_AUIB_AlmostEmpty(0);
			u32BufClearInt |= (1<<PDEC0_AUB_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<PDEC1_CPB_ALMOST_FULL))
		{
			VDEC_ISR_CPB_AlmostFull(1);
			u32BufClearInt |= (1<<PDEC1_CPB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC1_CPB_ALMOST_EMPTY))
		{
			VDEC_ISR_CPB_AlmostEmpty(1);
			u32BufClearInt |= (1<<PDEC1_CPB_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<PDEC1_AUB_ALMOST_FULL))
		{
			VDEC_ISR_AUIB_AlmostFull(1);
			u32BufClearInt |= (1<<PDEC1_AUB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC1_AUB_ALMOST_EMPTY))
		{
			VDEC_ISR_AUIB_AlmostEmpty(1);
			u32BufClearInt |= (1<<PDEC1_AUB_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<PDEC2_CPB_ALMOST_FULL))
		{
			VDEC_ISR_CPB_AlmostFull(2);
			u32BufClearInt |= (1<<PDEC2_CPB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC2_CPB_ALMOST_EMPTY))
		{
			VDEC_ISR_CPB_AlmostEmpty(2);
			u32BufClearInt |= (1<<PDEC2_CPB_ALMOST_EMPTY);
		}
		if(u32BufIntReason&(1<<PDEC2_AUB_ALMOST_FULL))
		{
			VDEC_ISR_AUIB_AlmostFull(2);
			u32BufClearInt |= (1<<PDEC2_AUB_ALMOST_FULL);
		}
		if(u32BufIntReason&(1<<PDEC2_AUB_ALMOST_EMPTY))
		{
			VDEC_ISR_AUIB_AlmostEmpty(2);
			u32BufClearInt |= (1<<PDEC2_AUB_ALMOST_EMPTY);
		}

		if( u32BufClearInt )
			TOP_HAL_ClearBufIntrMsk(u32BufClearInt);
	}
#endif

#if (defined(USE_MCU_FOR_VDEC_VES) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VES) && !defined(__XTENSA__))
	if (u32IntReason&(1 << PDEC0_AU_DETECT)) {
		VDEC_ISR_PIC_Detected(0);
	}
	if (u32IntReason&(1 << PDEC1_AU_DETECT)) {
		VDEC_ISR_PIC_Detected(1);
	}
	if (u32IntReason&(1 << PDEC2_AU_DETECT)) {
		VDEC_ISR_PIC_Detected(2);
	}

	if (u32IntReason&(1<<PDEC_SRST_DONE)) {
		VDEC_KDRV_Message(_TRACE, "Intr PDEC_SRST_DONE\n");
	}
#endif

#ifdef __XTENSA__
	TOP_HAL_ClearInterIntrMsk(u32IntReason);
#else //!__XTENSA__
	TOP_HAL_ClearExtIntrMsk(u32IntReason);
#endif //~__XTENSA__
}

void MCU_ISR_Handler(void)
{
	UINT32 ui32Reason;
#ifdef __XTENSA__
	if(MCU_HAL_GetInterIntrStatus(MCU_ISR_DMA))
		MCU_HAL_ClearInterIntr(MCU_ISR_DMA);

	ui32Reason = MCU_HAL_GetInterIntrStatus(MCU_ISR_IPC);
	MCU_HAL_ClearInterIntr(MCU_ISR_IPC);
#else
	if(MCU_HAL_GetExtIntrStatus(MCU_ISR_DMA))
		MCU_HAL_ClearExtIntr(MCU_ISR_DMA);

	ui32Reason = MCU_HAL_GetExtIntrStatus(MCU_ISR_IPC);
	MCU_HAL_ClearExtIntr(MCU_ISR_IPC);
#endif

	if (ui32Reason)
	{
#ifdef __XTENSA__
//		IPC_CMD_Receive();
#else
		IPC_REQ_Receive();
		IPC_DBG_Receive();
#endif
	}
}

