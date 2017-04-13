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
 * author     youngki.lyu (youngki.lyu@lge.com)
 * version    1.0
 * date       2010.04.30
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
#include "vdec_shm.h"

#ifdef __XTENSA__
#include <stdio.h>
#include <xtensa/xtruntime.h>
#include "../hal/lg1152/mcu_hal_api.h"
#else
#include <linux/kernel.h>
#endif

#include "vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		VDEC_SHM_NUM_OF_DIVIDE				128

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
	UINT32	ui32MemPtr;
	UINT32	ui32MemSize;

	UINT32	ui32_1FragmentSize;
	UINT32	ui32UsedFragment[VDEC_SHM_NUM_OF_DIVIDE/32];	// Bit Flag

	UINT32	ui32_AllocedFragmentSize[VDEC_SHM_NUM_OF_DIVIDE];
} gsVdecShm;


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
void VDEC_SHM_Init(UINT32 ui32MemPtr, UINT32 ui32MemSize)
{
	UINT32	i;

	VDEC_KDRV_Message(_TRACE, "[SHM][DBG] Addr:0x%08X, Size:0x%08X, %s", ui32MemPtr, ui32MemSize, __FUNCTION__ );

	gsVdecShm.ui32MemPtr = ui32MemPtr;
	gsVdecShm.ui32MemSize = ui32MemSize;

	for( i = 0; i < (VDEC_SHM_NUM_OF_DIVIDE/32); i++ )
		gsVdecShm.ui32UsedFragment[i] = 0;
	gsVdecShm.ui32_1FragmentSize = ui32MemSize/VDEC_SHM_NUM_OF_DIVIDE;

	for( i = 0; i < VDEC_SHM_NUM_OF_DIVIDE; i++ )
		gsVdecShm.ui32_AllocedFragmentSize[i] = 0;
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
UINT32 VDEC_SHM_Malloc(UINT32 ui32MemSize)
{
	UINT32	ui32NeedFragments;
	UINT32	ui32FragmentsStart, ui32FragmentsCount;
	UINT32	i;
	UINT32	ui32MemPtr;

	VDEC_KDRV_Message(_TRACE, "[SHM][DBG] Mem Size: 0x%X, %s", ui32MemSize,__FUNCTION__ );

	if( gsVdecShm.ui32MemPtr == 0 )	// Not Initialised
		return 0x0;

	ui32NeedFragments = (ui32MemSize + (gsVdecShm.ui32_1FragmentSize - 1)) / gsVdecShm.ui32_1FragmentSize;

	ui32FragmentsStart = VDEC_SHM_NUM_OF_DIVIDE;	// invalid value
	ui32FragmentsCount = 0;
	for( i = 0; i < VDEC_SHM_NUM_OF_DIVIDE; i++ )
	{
		if( gsVdecShm.ui32UsedFragment[i / 32] & (0x1 << (i % 32)) )
		{
			ui32FragmentsStart = VDEC_SHM_NUM_OF_DIVIDE;	// invalid value
			ui32FragmentsCount = 0;
		}
		else
		{
			if( ui32FragmentsStart == VDEC_SHM_NUM_OF_DIVIDE )
			{
				ui32FragmentsStart = i;
				ui32FragmentsCount = 1;
			}
			else
			{
				ui32FragmentsCount++;
			}
		}

		if( ui32FragmentsCount >= ui32NeedFragments )
			break;
	}
	if( i == VDEC_SHM_NUM_OF_DIVIDE )
	{
		VDEC_KDRV_Message(ERROR,  "[SHM][Err] Failed to Alloc Shared Memory - Request Size: %d, 1 Fragment Size: %d, %s", ui32MemSize, gsVdecShm.ui32_1FragmentSize, __FUNCTION__ );
		for( i = 0; i < (VDEC_SHM_NUM_OF_DIVIDE/32); i++ )
			VDEC_KDRV_Message(ERROR,  "[SHM][Err] Used Fragment[%d]: 0x%08X", i, gsVdecShm.ui32UsedFragment[i] );

		return 0x0;
	}

	for( i = ui32FragmentsStart; i < (ui32FragmentsStart + ui32FragmentsCount); i++ )
	{
		gsVdecShm.ui32UsedFragment[i / 32] |= (0x1 << (i % 32));
	}
	gsVdecShm.ui32_AllocedFragmentSize[ui32FragmentsStart] = ui32FragmentsCount;

	ui32MemPtr = gsVdecShm.ui32MemPtr + (gsVdecShm.ui32_1FragmentSize * ui32FragmentsStart);
	VDEC_KDRV_Message(_TRACE, "[SHM][DBG] Alloc Addr: 0x%X, Size: 0x%X, %d/%d", ui32MemPtr, ui32MemSize, (ui32FragmentsStart + ui32FragmentsCount), VDEC_SHM_NUM_OF_DIVIDE);

	return ui32MemPtr;
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
void VDEC_SHM_Free(UINT32 ui32MemPtr)
{
	UINT32	ui32MemOffset;
	UINT32	ui32FragmentsStart, ui32FragmentsCount;
	UINT32	i;

	VDEC_KDRV_Message(_TRACE, "[SHM][DBG] Free Addr: 0x%X, %s", ui32MemPtr, __FUNCTION__ );

	if( gsVdecShm.ui32MemPtr == 0 )	// Not Initialised
		return;

	ui32MemOffset = gsVdecShm.ui32MemPtr - ui32MemPtr;

	ui32FragmentsStart = ui32MemOffset / gsVdecShm.ui32_1FragmentSize;

	ui32FragmentsCount = gsVdecShm.ui32_AllocedFragmentSize[ui32FragmentsStart];
	if( ui32FragmentsCount == 0 )
	{
		VDEC_KDRV_Message(ERROR,  "[SHM][Err] Failed to Free Shared Memory: 0x%X, %s", ui32MemPtr, __FUNCTION__ );
		return;
	}

	for( i = ui32FragmentsStart; i < (ui32FragmentsStart + ui32FragmentsCount); i++ )
	{
		gsVdecShm.ui32UsedFragment[i / 32] &= ~(0x1 << (i % 32));
	}
	gsVdecShm.ui32_AllocedFragmentSize[ui32FragmentsStart] = 0;
}

/** @} */

