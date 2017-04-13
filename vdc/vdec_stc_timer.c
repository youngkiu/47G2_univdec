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
 * version    1.0
 * date       2011.05.10
 * note
 *
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#endif

#include "../mcu/os_adap.h"

#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/lq_hal_api.h"

#include "vdec_stc_timer.h"

#include "../mcu/vdec_print.h"

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
static UINT32 gSTCTmrState = 0;
/*----------------------------------------------------------------------------------------
 *   Static Function Prototypes Declarations
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/

void STC_TIMER_Init(void)
{
#define STC_TIMER_ISR_IDLE_MASK	(LQC0_ALMOST_EMPTY+STC_TIMER_ID_IDLE)
#define STC_TIMER_ISR_FEED_MASK	(LQC0_ALMOST_EMPTY+STC_TIMER_ID_FEED)

#ifdef USE_MCU_FOR_VDEC_VDC
	TOP_HAL_EnableInterIntr(BUFFER_STATUS);
#else
	TOP_HAL_EnableExtIntr(BUFFER_STATUS);
#endif

	STC_TIMER_Stop();

	// Set IDLE Timer
	LQ_HAL_SetAEmptyLevel(STC_TIMER_ID_IDLE, 0);
	LQ_HAL_SetEmptyTimer(STC_TIMER_ID_IDLE, FEED_TIMER_IDLE_msPERIOD);
	LQ_HAL_EnableEmptyTimer(STC_TIMER_ID_IDLE);

	// Set Feed Timer
	LQ_HAL_SetAEmptyLevel(STC_TIMER_ID_FEED, 0);
	LQ_HAL_SetEmptyTimer(STC_TIMER_ID_FEED, FEED_TIMER_Feed_msPERIOD);
	LQ_HAL_EnableEmptyTimer(STC_TIMER_ID_FEED);
}

void STC_TIMER_Stop(void)
{
	if(TOP_HAL_IsBufIntrEnable(STC_TIMER_ISR_IDLE_MASK)==1)
	{
		TOP_HAL_DisableBufIntr(STC_TIMER_ISR_IDLE_MASK);
		TOP_HAL_ClearBufIntr(STC_TIMER_ISR_IDLE_MASK);
	}

	if(TOP_HAL_IsBufIntrEnable(STC_TIMER_ISR_FEED_MASK)==1)
	{
		TOP_HAL_DisableBufIntr(STC_TIMER_ISR_FEED_MASK);
		TOP_HAL_ClearBufIntr(STC_TIMER_ISR_FEED_MASK);
	}

	gSTCTmrState = 0;
}

void STC_TIMER_Start(UINT32 u32TimerID)
{
	switch(u32TimerID)
	{
	case STC_TIMER_ID_IDLE:
		TOP_HAL_DisableBufIntr(STC_TIMER_ISR_FEED_MASK);
		TOP_HAL_ClearBufIntr(STC_TIMER_ISR_FEED_MASK);
		if(TOP_HAL_IsBufIntrEnable(STC_TIMER_ISR_IDLE_MASK)==0)
		{
			TOP_HAL_EnableBufIntr(STC_TIMER_ISR_IDLE_MASK);
		}
		gSTCTmrState = STC_TIMER_ID_IDLE;
		break;

	case STC_TIMER_ID_FEED:
		TOP_HAL_DisableBufIntr(STC_TIMER_ISR_IDLE_MASK);
		TOP_HAL_ClearBufIntr(STC_TIMER_ISR_IDLE_MASK);
		if(TOP_HAL_IsBufIntrEnable(STC_TIMER_ISR_FEED_MASK)==0)
		{
			TOP_HAL_EnableBufIntr(STC_TIMER_ISR_FEED_MASK);
		}
		gSTCTmrState = STC_TIMER_ID_FEED;
		break;

	default:
		VDEC_KDRV_Message(ERROR, "[STCTMR][Err] Invalid Timer ID(%d), %s", u32TimerID, __FUNCTION__ );
	}
}

UINT32 STC_TIMER_GetStatus(void)
{
	return gSTCTmrState;
}
