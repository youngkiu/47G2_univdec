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
 * date       2013.01.28
 * note       Additional information.
 *
 * @addtogroup lg115x_ves
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "vdec_cpb.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#include <linux/version.h>
#endif

#include "hal/lg1152/lg1152_vdec_base.h"
#include "mcu/vdec_shm.h"
#include "mcu/os_adap.h"
#include "ves/ves_drv.h"
#include "ves/ves_cpb.h"

#include "mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		VES_NUM_OF_CPB_POOL			VES_NUM_OF_CHANNEL

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
	UINT32		ui32PoolAddr;
	UINT32		ui32PoolSize;
	UINT32		ui32AllocedOffset;

	struct
	{
		BOOLEAN		bUsed;
		BOOLEAN		bAlloced;
		UINT32		ui32BufAddr;
		UINT32		ui32BufSize;
	} Segments[VES_NUM_OF_CPB_POOL];

	BOOLEAN		bUsingSecuredPool;
} gsCpbMem;


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
void VDEC_CPB_Init(UINT32 ui32PoolAddr, UINT32 ui32PoolSize)
{
	UINT32	i;

	VDEC_KDRV_Message(_TRACE, "[CPB][DBG] Addr:0x%08X, Size:0x%08X, %s", ui32PoolAddr, ui32PoolSize, __FUNCTION__ );

	gsCpbMem.ui32PoolAddr = ui32PoolAddr;
	gsCpbMem.ui32PoolSize = ui32PoolSize;
	gsCpbMem.ui32AllocedOffset = 0;

	for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
	{
		gsCpbMem.Segments[i].bUsed = FALSE;
		gsCpbMem.Segments[i].bAlloced = FALSE;
	}

	gsCpbMem.bUsingSecuredPool = FALSE;
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
static UINT32 _VDEC_CPB_Alloc(UINT32 ui32BufSize)
{
	UINT32		i;

	ui32BufSize = VES_CPB_CEILING_512BYTES( ui32BufSize );

	for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
	{
		if( (gsCpbMem.Segments[i].bUsed == FALSE) &&
			(gsCpbMem.Segments[i].bAlloced == TRUE) &&
			(gsCpbMem.Segments[i].ui32BufSize == ui32BufSize) )
		{
			gsCpbMem.Segments[i].bUsed = TRUE;

			VDEC_KDRV_Message(_TRACE, "[CPB][DBG][CPB] Addr: 0x%X, Size: 0x%X", gsCpbMem.Segments[i].ui32BufAddr, ui32BufSize);

			return gsCpbMem.Segments[i].ui32BufAddr;
		}
	}

	for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
	{
		if( gsCpbMem.Segments[i].bAlloced == FALSE )
			break;
	}
	if( i == VES_NUM_OF_CPB_POOL )
	{
		VDEC_KDRV_Message(ERROR, "[CPB][Err] Num of CPB Pool %s", __FUNCTION__ );
		for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
			VDEC_KDRV_Message(ERROR, "[CPB][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsCpbMem.Segments[i].bUsed, gsCpbMem.Segments[i].bAlloced, gsCpbMem.Segments[i].ui32BufAddr, gsCpbMem.Segments[i].ui32BufSize );
		return 0x0;
	}
	if( (gsCpbMem.ui32PoolSize - gsCpbMem.ui32AllocedOffset) < ui32BufSize )
	{
		VDEC_KDRV_Message(ERROR, "[CPB][Err] CPB Pool Size, 0x%X/0x%X,  %s", gsCpbMem.ui32AllocedOffset, gsCpbMem.ui32PoolSize, __FUNCTION__ );
		for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
			VDEC_KDRV_Message(ERROR, "[CPB][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsCpbMem.Segments[i].bUsed, gsCpbMem.Segments[i].bAlloced, gsCpbMem.Segments[i].ui32BufAddr, gsCpbMem.Segments[i].ui32BufSize );
		return 0x0;
	}

	gsCpbMem.Segments[i].bUsed = TRUE;
	gsCpbMem.Segments[i].bAlloced = TRUE;
	gsCpbMem.Segments[i].ui32BufAddr = VES_CPB_BOTTOM_512BYTES( gsCpbMem.ui32PoolAddr + gsCpbMem.ui32AllocedOffset );
	gsCpbMem.Segments[i].ui32BufSize = ui32BufSize;

	gsCpbMem.ui32AllocedOffset += ui32BufSize;

	VDEC_KDRV_Message(_TRACE, "[CPB][DBG][CPB] Addr: 0x%X, Size: 0x%X", gsCpbMem.Segments[i].ui32BufAddr, ui32BufSize);

	return gsCpbMem.Segments[i].ui32BufAddr;
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
static BOOLEAN _VDEC_CPB_Free(UINT32 ui32BufAddr)
{
	UINT32		i;

	for( i = 0; i < VES_NUM_OF_CPB_POOL; i++ )
	{
		if( (gsCpbMem.Segments[i].bUsed == TRUE) &&
			(gsCpbMem.Segments[i].bAlloced == TRUE) &&
			(gsCpbMem.Segments[i].ui32BufAddr == ui32BufAddr) )
		{
			gsCpbMem.Segments[i].bUsed = FALSE;

			VDEC_KDRV_Message(_TRACE, "[CPB][DBG][CPB] Addr: 0x%X, Size: 0x%X", ui32BufAddr, gsCpbMem.Segments[i].ui32BufSize);

			return TRUE;
		}
	}

	VDEC_KDRV_Message(ERROR, "[CPB][Err] Not Matched CPB Pool - 0x%X %s", ui32BufAddr, __FUNCTION__ );

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
BOOLEAN VDEC_CPB_Alloc( UINT32 *pui32CpbBufAddr, UINT32 *pui32CpbBufSize, BOOLEAN bSecured )
{
	if( bSecured == TRUE )
	{
		if( gsCpbMem.bUsingSecuredPool == TRUE )
		{
			VDEC_KDRV_Message(ERROR, "[CPB][Err] Not Enough Secured CPB Pool - %s", __FUNCTION__ );
			return FALSE;
		}

		*pui32CpbBufAddr = VDEC_SECURE_CPB_BASE;
		*pui32CpbBufSize = VDEC_SECURE_CPB_SIZE;

		gsCpbMem.bUsingSecuredPool = TRUE;
	}
	else
	{
		*pui32CpbBufSize = VDEC_CPB_SIZE / 2;
		*pui32CpbBufAddr = _VDEC_CPB_Alloc(*pui32CpbBufSize);
		if( *pui32CpbBufAddr == 0x0 )
		{
			VDEC_KDRV_Message(ERROR, "[CPB][Err] Not Enough CPB Pool - %s", __FUNCTION__ );
			return FALSE;
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
BOOLEAN VDEC_CPB_Free(UINT32 ui32BufAddr)
{
	if( ui32BufAddr == VDEC_SECURE_CPB_BASE )
	{
		gsCpbMem.bUsingSecuredPool = FALSE;
		return TRUE;
	}

	return _VDEC_CPB_Free(ui32BufAddr);
}

