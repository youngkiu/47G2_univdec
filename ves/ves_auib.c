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
 * date       2011.04.19
 * note       Additional information.
 *
 * @addtogroup lg1150_ves
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "ves_auib.h"
#include "ves_cpb.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#endif

#include "../mcu/os_adap.h"

#include "../mcu/vdec_shm.h"
#include "../hal/lg1152/pdec_hal_api.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../mcu/ipc_cmd.h"
#include "ves_drv.h"

#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define	VES_AUIB_NUM_OF_MEM			(VES_NUM_OF_CHANNEL)

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
static struct
{
	UINT32 		ui32PhyBaseAddr;
	UINT32 		*pui32VirBasePtr;
	UINT32 		ui32BufSize;
} gsVesAuib[VES_NUM_OF_CHANNEL];

static struct
{
	BOOLEAN		bUsed;
	BOOLEAN		bAlloced;
	UINT32		ui32BufAddr;
	UINT32		ui32BufSize;
} gsAuibMem[VES_NUM_OF_CHANNEL];

static void (*_gfpVES_AuibStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus);


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
void VES_AUIB_Init( void (*_fpVES_CpbStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus) )
{
	UINT32	i;

	VDEC_KDRV_Message(_TRACE, "[VES][DBG] %s", __FUNCTION__ );

	for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
	{
		gsAuibMem[i].bUsed = FALSE;
		gsAuibMem[i].bAlloced = FALSE;
	}

	for( i = 0; i < VES_NUM_OF_CHANNEL; i++ )
	{
		gsVesAuib[i].ui32PhyBaseAddr = 0x0;
		gsVesAuib[i].pui32VirBasePtr = NULL;
		gsVesAuib[i].ui32BufSize = 0x0;
	}

	_gfpVES_AuibStatus = _fpVES_CpbStatus;
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
static UINT32 _VES_AUIB_Alloc(UINT32 ui32BufSize)
{
	UINT32		i;
	UINT32		ui32BufAddr;

	for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
	{
		if( (gsAuibMem[i].bUsed == FALSE) &&
			(gsAuibMem[i].bAlloced == TRUE) &&
			(gsAuibMem[i].ui32BufSize == ui32BufSize) )
		{
			gsAuibMem[i].bUsed = TRUE;

			VDEC_KDRV_Message(_TRACE, "[VES][DBG][AUIB] Addr: 0x%X, Size: 0x%X", gsAuibMem[i].ui32BufAddr, ui32BufSize);

			return gsAuibMem[i].ui32BufAddr;
		}
	}

	for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
	{
		if( gsAuibMem[i].bAlloced == FALSE )
			break;
	}
	if( i == VES_AUIB_NUM_OF_MEM )
	{
		VDEC_KDRV_Message(ERROR, "[VES][Err] Num of AUIB Pool, Request Buf Size: 0x%08X", ui32BufSize );
		for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
			VDEC_KDRV_Message(ERROR, "[VES][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsAuibMem[i].bUsed, gsAuibMem[i].bAlloced, gsAuibMem[i].ui32BufAddr, gsAuibMem[i].ui32BufSize );
		return 0x0;
	}

	ui32BufAddr = VDEC_SHM_Malloc( ui32BufSize + PDEC_AUI_SIZE );
	if( ui32BufAddr == 0x0 )
	{
		VDEC_KDRV_Message(ERROR, "[VES][Err] Failed to VDEC_SHM_Malloc, Request Buf Size: 0x%08X", ui32BufSize );
		for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
			VDEC_KDRV_Message(ERROR, "[VES][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsAuibMem[i].bUsed, gsAuibMem[i].bAlloced, gsAuibMem[i].ui32BufAddr, gsAuibMem[i].ui32BufSize );
		return 0x0;
	}
	ui32BufAddr = PDEC_AUIB_CEILING_16BYTES( ui32BufAddr );

	gsAuibMem[i].bUsed = TRUE;
	gsAuibMem[i].bAlloced = TRUE;
	gsAuibMem[i].ui32BufAddr = ui32BufAddr;
	gsAuibMem[i].ui32BufSize = ui32BufSize;

	VDEC_KDRV_Message(_TRACE, "[VES][DBG][AUIB] Addr: 0x%X, Size: 0x%X", ui32BufAddr, ui32BufSize);

	return ui32BufAddr;

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
static BOOLEAN _VES_AUIB_Free(UINT32 ui32BufAddr, UINT32 ui32BufSize)
{
	UINT32		i;

	for( i = 0; i < VES_AUIB_NUM_OF_MEM; i++ )
	{
		if( (gsAuibMem[i].bUsed == TRUE) &&
			(gsAuibMem[i].bAlloced == TRUE) &&
			(gsAuibMem[i].ui32BufAddr == ui32BufAddr) &&
			(gsAuibMem[i].ui32BufSize == ui32BufSize) )
		{
			gsAuibMem[i].bUsed = FALSE;

			VDEC_KDRV_Message(_TRACE, "[VES][DBG][AUIB] Addr: 0x%X, Size: 0x%X", ui32BufAddr, ui32BufSize);

			return TRUE;
		}
	}

	VDEC_KDRV_Message(ERROR, "[VES][Err] Not Matched AUIB Pool - 0x%X/0x%X %s", ui32BufAddr, ui32BufSize, __FUNCTION__ );

	return FALSE;
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
void VES_AUIB_Reset(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

	PDEC_HAL_AUIB_Init(ui8VesCh, gsVesAuib[ui8VesCh].ui32PhyBaseAddr, gsVesAuib[ui8VesCh].ui32BufSize);
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
static void _VES_AUIB_SetDb(UINT8 ui8VesCh, UINT32 ui32AuibBufAddr, UINT32 ui32AuibBufSize)
{
	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] AUIB Hw Base: 0x%X, Size: 0x%X, %s(%d)", ui8VesCh, ui32AuibBufAddr, ui32AuibBufSize, __FUNCTION__, __LINE__ );

	if( ui32AuibBufAddr && ui32AuibBufSize )
	{
		gsVesAuib[ui8VesCh].pui32VirBasePtr = VDEC_TranselateVirualAddr(ui32AuibBufAddr, ui32AuibBufSize);
	}
	else
	{
		VDEC_ClearVirualAddr((void *)gsVesAuib[ui8VesCh].pui32VirBasePtr);
		gsVesAuib[ui8VesCh].pui32VirBasePtr = NULL;
	}

	gsVesAuib[ui8VesCh].ui32PhyBaseAddr = ui32AuibBufAddr;
	gsVesAuib[ui8VesCh].ui32BufSize = ui32AuibBufSize;

	PDEC_HAL_AUIB_SetAuibVirBase(ui8VesCh, gsVesAuib[ui8VesCh].pui32VirBasePtr);
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
BOOLEAN VES_AUIB_Open(UINT8 ui8VesCh, BOOLEAN bFromTVP)
{
	UINT32 		ui32AuibBufAddr;
	UINT32 		ui32AuibBufSize = VES_AUIB_NUM_OF_NODE * PDEC_AUI_SIZE;

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}

	ui32AuibBufAddr = _VES_AUIB_Alloc( ui32AuibBufSize );
	if( ui32AuibBufAddr == 0x0 )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}
	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG][AUIB] # of Node: %d", ui8VesCh, ui32AuibBufSize/PDEC_AUI_SIZE );

	_VES_AUIB_SetDb(ui8VesCh, ui32AuibBufAddr, ui32AuibBufSize);

	if( bFromTVP == TRUE )
	{
		PDEC_HAL_Disable(ui8VesCh);
		PDEC_HAL_AUIB_Init(ui8VesCh, ui32AuibBufAddr, ui32AuibBufSize);
	}
	else
	{
		PDEC_HAL_AUIB_Init(ui8VesCh, ui32AuibBufAddr, ui32AuibBufSize);
		PDEC_HAL_AUIB_SetBufALevel(ui8VesCh, 80, 2);

		switch( ui8VesCh )
		{
		case 0 :
			TOP_HAL_EnableBufIntr(PDEC0_AUB_ALMOST_FULL);
			TOP_HAL_EnableBufIntr(PDEC0_AUB_ALMOST_EMPTY);
			break;
		case 1 :
			TOP_HAL_EnableBufIntr(PDEC1_AUB_ALMOST_FULL);
			TOP_HAL_EnableBufIntr(PDEC1_AUB_ALMOST_EMPTY);
			break;
		case 2 :
			TOP_HAL_EnableBufIntr(PDEC2_AUB_ALMOST_FULL);
			TOP_HAL_EnableBufIntr(PDEC2_AUB_ALMOST_EMPTY);
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
		}
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
void VES_AUIB_Close(UINT8 ui8VesCh)
{
	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

	PDEC_HAL_AUIB_Init(ui8VesCh, 0x0, 0x0);

	switch( ui8VesCh )
	{
	case 0 :
		TOP_HAL_ClearBufIntr(PDEC0_AUB_ALMOST_FULL);
		TOP_HAL_DisableBufIntr(PDEC0_AUB_ALMOST_FULL);
		TOP_HAL_ClearBufIntr(PDEC0_AUB_ALMOST_EMPTY);
		TOP_HAL_DisableBufIntr(PDEC0_AUB_ALMOST_EMPTY);
		break;
	case 1 :
		TOP_HAL_ClearBufIntr(PDEC1_AUB_ALMOST_FULL);
		TOP_HAL_DisableBufIntr(PDEC1_AUB_ALMOST_FULL);
		TOP_HAL_ClearBufIntr(PDEC1_AUB_ALMOST_EMPTY);
		TOP_HAL_DisableBufIntr(PDEC1_AUB_ALMOST_EMPTY);
		break;
	case 2 :
		TOP_HAL_ClearBufIntr(PDEC2_AUB_ALMOST_FULL);
		TOP_HAL_DisableBufIntr(PDEC2_AUB_ALMOST_FULL);
		TOP_HAL_ClearBufIntr(PDEC2_AUB_ALMOST_EMPTY);
		TOP_HAL_DisableBufIntr(PDEC2_AUB_ALMOST_EMPTY);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
	}

	_VES_AUIB_Free(gsVesAuib[ui8VesCh].ui32PhyBaseAddr, gsVesAuib[ui8VesCh].ui32BufSize);

	_VES_AUIB_SetDb(ui8VesCh, 0x0, 0x0);
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
void VDEC_ISR_AUIB_AlmostFull(UINT8 ui8VesCh)
{
	S_VES_CALLBACK_BODY_CPBSTATUS_T sReportCpbStatus;

	if( ui8VesCh >= PDEC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	switch( ui8VesCh )
	{
	case 0 :
		TOP_HAL_ClearBufIntr(PDEC0_AUB_ALMOST_FULL);
		break;
	case 1 :
		TOP_HAL_ClearBufIntr(PDEC1_AUB_ALMOST_FULL);
		break;
	case 2 :
		TOP_HAL_ClearBufIntr(PDEC2_AUB_ALMOST_FULL);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
	}

	sReportCpbStatus.ui8Vch = VES_GetVdecCh(ui8VesCh);
	sReportCpbStatus.ui8VesCh = ui8VesCh;
	sReportCpbStatus.eBufStatus = CPB_STATUS_ALMOST_FULL;
	_gfpVES_AuibStatus( &sReportCpbStatus );

	VDEC_KDRV_Message(ERROR, "[VES%d][Err][AUIB] Almost Full - Buf Status: %d ", ui8VesCh, PDEC_HAL_AUIB_GetStatus(ui8VesCh) );
	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Buffer Status: 0x%X ", ui8VesCh, PDEC_HAL_GetBufferStatus(ui8VesCh) );

//	PDEC_HAL_Ignore_Stall(ui8VesCh);
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
void VDEC_ISR_AUIB_AlmostEmpty(UINT8 ui8VesCh)
{
	if( ui8VesCh >= PDEC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	switch( ui8VesCh )
	{
	case 0 :
		TOP_HAL_ClearBufIntr(PDEC0_AUB_ALMOST_EMPTY);
		break;
	case 1 :
		TOP_HAL_ClearBufIntr(PDEC1_AUB_ALMOST_EMPTY);
		break;
	case 2 :
		TOP_HAL_ClearBufIntr(PDEC2_AUB_ALMOST_EMPTY);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
	}

	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG][AUIB] Almost Empty - Buf Status: %d ", ui8VesCh, PDEC_HAL_AUIB_GetStatus(ui8VesCh) );
	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Buffer Status: 0x%X ", ui8VesCh, PDEC_HAL_GetBufferStatus(ui8VesCh) );
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
UINT32 VES_AUIB_NumActive(UINT8 ui8VesCh)
{
	UINT32	ui32UsedBuf;
	UINT32	ui32WrAddr;
	UINT32	ui32RdAddr;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

	ui32WrAddr = PDEC_HAL_AUIB_GetWrPtr(ui8VesCh);
	ui32RdAddr = PDEC_HAL_AUIB_GetRdPtr(ui8VesCh);

	if( ui32WrAddr == ui32RdAddr )
		return 0;

	ui32WrAddr = PDEC_AUIB_PREV_START_ADDR(ui32WrAddr, gsVesAuib[ui8VesCh].ui32PhyBaseAddr, gsVesAuib[ui8VesCh].ui32BufSize);

	if( ui32WrAddr >= ui32RdAddr )
		ui32UsedBuf = ui32WrAddr - ui32RdAddr;
	else
		ui32UsedBuf = ui32WrAddr + gsVesAuib[ui8VesCh].ui32BufSize - ui32RdAddr;

	return (ui32UsedBuf / PDEC_AUI_SIZE);
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
UINT32 VES_AUIB_NumFree(UINT8 ui8VesCh)
{
	UINT32	ui32FreeBuf;
	UINT32	ui32WrAddr;
	UINT32	ui32RdAddr;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

	ui32WrAddr = PDEC_HAL_AUIB_GetWrPtr(ui8VesCh);
	ui32RdAddr = PDEC_HAL_AUIB_GetRdPtr(ui8VesCh);

	if( ui32WrAddr == ui32RdAddr )
		return 0;

	ui32WrAddr = PDEC_AUIB_PREV_START_ADDR(ui32WrAddr, gsVesAuib[ui8VesCh].ui32PhyBaseAddr, gsVesAuib[ui8VesCh].ui32BufSize);

	if( ui32WrAddr < ui32RdAddr )
		ui32FreeBuf = ui32RdAddr - ui32WrAddr;
	else
		ui32FreeBuf = ui32RdAddr + gsVesAuib[ui8VesCh].ui32BufSize - ui32WrAddr;

	return (ui32FreeBuf / PDEC_AUI_SIZE);
}


