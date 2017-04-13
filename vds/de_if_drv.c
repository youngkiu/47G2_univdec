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
 * author     youngki.lyu@lge.com
 * version    1.0
 * date       2011.04.09
 * note       Additional information.
 *
 * @addtogroup lg115x_vdec
 * @{
 */


/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "de_if_drv.h"
#include "vsync_drv.h"
#include "disp_q.h"

#include <linux/kernel.h>
#include <asm/string.h>	// memset

#include "../mcu/os_adap.h"

#include "../hal/lg1152/ipc_reg_api.h"
#include "../mcu/vdec_shm.h"

#include "../hal/lg1152/de_ipc_hal_api.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/lq_hal_api.h"

#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define	DE_IF_NUM_OF_CMD_Q					0x10

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	S_DE_IPC_T		sDeIpc;

	E_DISPQ_DISPLAY_INFO_T eDisplayInfo;
	UINT32			ui32DisplayPeriod;
	UINT32			ui32FieldCount;

	UINT32			ui32FrameDuration90K;

	UINT32			ui32InputGSTC;
	UINT32			ui32OutputGSTC;

	// for Debug
	UINT32			ui32LogTick;
	UINT32			ui32UpdateFrameCount;
} S_DE_IF_DB_T;


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
volatile S_DE_IF_DB_T *gsDeIf = NULL;


/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
static void _DE_IF_Reset(UINT8 ui8DeIfCh)
{
	S_DE_IPC_T		*psDeIpc = NULL;

	psDeIpc = (S_DE_IPC_T *)&gsDeIf[ui8DeIfCh].sDeIpc;
	memset((void *)psDeIpc, 0x0, sizeof(S_DE_IPC_T));
	psDeIpc->ui32Y_FrameBaseAddr = 0x0;
	psDeIpc->ui32C_FrameBaseAddr = 0x0;
	psDeIpc->ui32FrameIdx = 0xFF;
	psDeIpc->eDisplayMode = DE_IPC_DISPLAY_MODE_FRAME_PICTURE;
	psDeIpc->eLR_Order = DE_IPC_3D_FRAME_NONE;

	gsDeIf[ui8DeIfCh].eDisplayInfo = DISPQ_DISPLAY_INFO_NULL;
	gsDeIf[ui8DeIfCh].ui32DisplayPeriod = 0x0;
	gsDeIf[ui8DeIfCh].ui32FieldCount = 0xFFFFFFFF;

	gsDeIf[ui8DeIfCh].ui32FrameDuration90K = 1500;
	psDeIpc->ui16FrameRateRes = 60;
	psDeIpc->ui16FrameRateDiv = 1;

	gsDeIf[ui8DeIfCh].ui32InputGSTC = 0x80000000;
	gsDeIf[ui8DeIfCh].ui32OutputGSTC = 0x80000000;

	gsDeIf[ui8DeIfCh].ui32LogTick = 0xFFFFFFF0;
	gsDeIf[ui8DeIfCh].ui32UpdateFrameCount = 0;

	if( ui8DeIfCh < DE_IPC_NUM_OF_CHANNEL )
	{
		DE_IPC_HAL_Update( ui8DeIfCh, psDeIpc );
	}
}

/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
void DE_IF_Init(void)
{
	UINT32	gsDeIfPhy, gsDeIfSize;
	UINT8	ui8DeIfCh;

	gsDeIfSize = sizeof(S_DE_IF_DB_T) * DE_IF_NUM_OF_CHANNEL;
#if (!defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__))
	gsDeIfPhy = IPC_REG_DE_IF_GetDb();
#else
	gsDeIfPhy = VDEC_SHM_Malloc(gsDeIfSize);
	IPC_REG_DE_IF_SetDb(gsDeIfPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[DE_IF][DBG] gsDeIfPhy: 0x%X, %s", gsDeIfPhy, __FUNCTION__ );

	gsDeIf = (S_DE_IF_DB_T *)VDEC_TranselateVirualAddr(gsDeIfPhy, gsDeIfSize);

	for( ui8DeIfCh = 0; ui8DeIfCh < DE_IF_NUM_OF_CHANNEL; ui8DeIfCh++ )
	{
		_DE_IF_Reset(ui8DeIfCh);
	}
}

/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
BOOLEAN DE_IF_Open(E_DE_IF_DST_T eDeIfDstCh)
{
	UINT8		ui8DeIfCh;

	switch( eDeIfDstCh )
	{
	case DE_IF_DST_DE_IF0 :
	case DE_IF_DST_DE_IF1 :
		ui8DeIfCh = (UINT8)eDeIfDstCh;

		VDEC_KDRV_Message(_TRACE, "[DE_IF%d][DBG] %s ", ui8DeIfCh, __FUNCTION__ );

		_DE_IF_Reset(ui8DeIfCh);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[DE_IF%d][Err] %s", eDeIfDstCh, __FUNCTION__ );
	}

	return TRUE;
}

/**
********************************************************************************
* @brief
*
* @remarks
*  DETAIL INFORMATION
* @par requirements:
*
* @param
*
* @return
*
********************************************************************************
*/
void DE_IF_Close(E_DE_IF_DST_T eDeIfDstCh)
{
	UINT8		ui8DeIfCh;

	switch( eDeIfDstCh )
	{
	case DE_IF_DST_DE_IF0 :
	case DE_IF_DST_DE_IF1 :
		ui8DeIfCh = (UINT8)eDeIfDstCh;

		VDEC_KDRV_Message(_TRACE, "[DE_IF%d][DBG] %s ", ui8DeIfCh, __FUNCTION__ );

		_DE_IF_Reset(ui8DeIfCh);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[DE_IF%d][Err] %s", eDeIfDstCh, __FUNCTION__ );
	}
}

