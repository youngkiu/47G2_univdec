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
 * author     sooya.joo (sooya.joo@lge.com)
 * version    1.0
 * date       2011.08.03
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/
#include "../hal/lg1152/lg1152_vdec_base.h"

#define	RAM_LOG_UNIT_SIZE			0x20

#define	RAM_LOG_BUF_BASE			VDEC_RAM_LOG_BUF_BASE		//0x75A7A000
#define	RAM_LOG_BUF_SIZE			VDEC_RAM_LOG_BUF_SIZE		//32KByte

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "ram_log.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "vdec_shm.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#endif

#include "os_adap.h"

#include "vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		RAM_LOG_HEADER_INVALID	0xFFFF1234

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
typedef struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT32 		ui32BufSize;			// constant
	UINT32 		ui32ModSize;		// constant

	struct
	{
		UINT32	ui32ModID;
		UINT32	ui32RdIdx;
		UINT32	ui32WrIdx;
	} sModule[RAM_LOG_MOD_NUM];
}VDEC_RAM_LOG_DB;

volatile VDEC_RAM_LOG_DB *gsRamLog = NULL;
static UINT32 *psRamLog_ui32VirBasePtr=NULL;	// constant

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
static BOOLEAN RAM_LOG_Register(UINT32 ui32ModID)
{
	UINT32	i;

	for( i = 0; i < RAM_LOG_MOD_NUM; i++ )
	{
		if( gsRamLog->sModule[i].ui32ModID == RAM_LOG_HEADER_INVALID)
			break;

		if( gsRamLog->sModule[i].ui32ModID == ui32ModID)
		{
			VDEC_KDRV_Message(ERROR, "[RAMLog][Err] %d Module is already Registered", ui32ModID);
			break;
		}
	}

	if( i == RAM_LOG_MOD_NUM )
	{
		VDEC_KDRV_Message(ERROR, "[RAMLog][Err] Not Enough Module Space %d", RAM_LOG_MOD_NUM );
		return 0;
	}

	gsRamLog->sModule[i].ui32ModID = ui32ModID;
	gsRamLog->sModule[i].ui32RdIdx = 0;
	gsRamLog->sModule[i].ui32WrIdx = 0;

	return 1;
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
void RAM_LOG_Init(void)
{
	UINT32 ui32DBSize;
	UINT32 ui32DBPhy;
	UINT32 i;

	VDEC_KDRV_Message(_TRACE, "[RAMLOG][DBG] %s", __FUNCTION__ );
#if (!defined (__XTENSA__))
	ui32DBSize = sizeof(VDEC_RAM_LOG_DB);
	ui32DBPhy = VDEC_SHM_Malloc(ui32DBSize);
	IPC_REG_RAMLOG_SetDb(ui32DBPhy);
	VDEC_KDRV_Message(_TRACE, "[RAMLOG][DBG] ui32DBPhy: 0x%X, %s", ui32DBPhy, __FUNCTION__ );
#else
	VDEC_KDRV_Message(_TRACE, "[RAMLOG][DBG] %s", __FUNCTION__ );
	ui32DBSize = sizeof(VDEC_RAM_LOG_DB);
	ui32DBPhy = IPC_REG_RAMLOG_GetDb();
#endif

	gsRamLog = (VDEC_RAM_LOG_DB *)VDEC_TranselateVirualAddr(ui32DBPhy, ui32DBSize);

#if (!defined (__XTENSA__))
	gsRamLog->ui32PhyBaseAddr = RAM_LOG_BUF_BASE;
	gsRamLog->ui32BufSize = RAM_LOG_BUF_SIZE;
	gsRamLog->ui32ModSize = gsRamLog->ui32BufSize/RAM_LOG_MOD_NUM;
#endif

	VDEC_KDRV_Message(DBG_IPC, "[RAMLOG] Base:0x%X, Size: 0x%X", gsRamLog->ui32PhyBaseAddr, gsRamLog->ui32BufSize );

	psRamLog_ui32VirBasePtr = VDEC_TranselateVirualAddr(gsRamLog->ui32PhyBaseAddr, gsRamLog->ui32BufSize);
	memset(psRamLog_ui32VirBasePtr , 0x0, gsRamLog->ui32BufSize);

	for( i = 0; i < RAM_LOG_MOD_NUM; i++ )
	{
		gsRamLog->sModule[i].ui32ModID = RAM_LOG_HEADER_INVALID;
		RAM_LOG_Register(i);
	}

	VDEC_KDRV_Message(_TRACE, "[RAMLOG  : %s] %x, %x, %x, %x"
		, __FUNCTION__
		, gsRamLog->ui32PhyBaseAddr
		, (UINT32)psRamLog_ui32VirBasePtr
		, gsRamLog->ui32BufSize
		, gsRamLog->ui32ModSize);

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
static UINT32 _RAM_LOG_CheckOverflow(UINT32 ui32ModID)
{
	UINT32		ui32TotalCnt;

	if(ui32ModID>=RAM_LOG_MOD_NUM)
	{
		VDEC_KDRV_Message(ERROR, "[RAMLOG][Err] Invalid Module ID :%d", ui32ModID);
		return 0;
	}

	ui32TotalCnt = gsRamLog->ui32ModSize/RAM_LOG_UNIT_SIZE;

	if(gsRamLog->sModule[ui32ModID].ui32RdIdx==gsRamLog->sModule[ui32ModID].ui32WrIdx)
	{
//		VDEC_KDRV_Message(DBG_SYS, "[RAMLOG %d][Wrn] Full Log Buffer  RdIdx:%d, WrIdx:%d, Total Cnt: %d", ui32ModID, gsRamLog->sModule[ui32ModID].ui32RdIdx, gsRamLog->sModule[ui32ModID].ui32WrIdx, ui32TotalCnt);
	}

	return 1;
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
BOOLEAN RAM_LOG_Write(UINT32 ui32ModID, void *pLogBody)
{
	UINT32		*pui32WrVirPtr;
	UINT32	ui32WrOffset;
	UINT32	ui32WrIdx, ui32TotalCnt;

	if( !psRamLog_ui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[RAMLOG][Err] Not Initialised" );
		return 0;
	}

	if(ui32ModID>=RAM_LOG_MOD_NUM)
	{
		VDEC_KDRV_Message(ERROR, "[RAMLOG][Err] Invalid Module ID :%d", ui32ModID);
		return 0;
	}

	ui32WrIdx = gsRamLog->sModule[ui32ModID].ui32WrIdx;
	ui32WrOffset = ui32ModID*gsRamLog->ui32ModSize + ui32WrIdx*RAM_LOG_UNIT_SIZE;

	pui32WrVirPtr = psRamLog_ui32VirBasePtr+ (ui32WrOffset>>2);

	// 2. Write Body
	memcpy((void *)pui32WrVirPtr, (void *)pLogBody, RAM_LOG_UNIT_SIZE);

	ui32TotalCnt = gsRamLog->ui32ModSize/RAM_LOG_UNIT_SIZE;
	gsRamLog->sModule[ui32ModID].ui32WrIdx = (gsRamLog->sModule[ui32ModID].ui32WrIdx+1)%ui32TotalCnt;

	// 1. Check Buffer Overflow
	if(_RAM_LOG_CheckOverflow(ui32ModID) == FALSE)
		return 0;
	return 1;
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
UINT32 RAM_LOG_Read(UINT32 ui32ModID, UINT32 *pui32Body, UINT32 ui32DstSize)
{
	UINT32	ui32RdIdx;
	UINT32	ui32RdOffset;
	UINT32	*pui32RdVirPtr;
	UINT32	ui32CopySize, ui32TmpCopySize;
	UINT32	ui32TotalCnt;
	UINT8 *pui8Target;

	if( !psRamLog_ui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[RAMLog][Err] Not Initialised" );
		return 0;
	}

	ui32TotalCnt = gsRamLog->ui32ModSize/RAM_LOG_UNIT_SIZE;

	if(ui32ModID>=RAM_LOG_MOD_NUM)
	{
		VDEC_KDRV_Message(ERROR, "[RAMLOG][Err] Invalid Module ID :%d", ui32ModID);
		return 0;
	}

	if(gsRamLog->sModule[ui32ModID].ui32RdIdx == gsRamLog->sModule[ui32ModID].ui32WrIdx)
	{
		VDEC_KDRV_Message(DBG_SYS, "[RAMLOG][Dbg] No More Data :%d", ui32ModID);
		return 0;
	}

	// 1. Source & Destination Size check
	if(gsRamLog->sModule[ui32ModID].ui32RdIdx > gsRamLog->sModule[ui32ModID].ui32WrIdx)
	{
		ui32CopySize = ui32TotalCnt - gsRamLog->sModule[ui32ModID].ui32RdIdx + gsRamLog->sModule[ui32ModID].ui32WrIdx;
	}
	else
	{
		ui32CopySize =gsRamLog->sModule[ui32ModID].ui32WrIdx - gsRamLog->sModule[ui32ModID].ui32RdIdx;
	}

	if(ui32CopySize>ui32DstSize)
	{
		VDEC_KDRV_Message(ERROR, "[RAMLOG][Err] ui32CopySize : %d,  ui32DstSize :%d", ui32CopySize, ui32DstSize);
		return 0xFFFFFFFF;
	}

	pui8Target = (UINT8 *)pui32Body;
	ui32TmpCopySize = 0;
	ui32CopySize = 0;

	// 2. check wrap around
	if(gsRamLog->sModule[ui32ModID].ui32RdIdx > gsRamLog->sModule[ui32ModID].ui32WrIdx)
	{
		ui32CopySize =ui32TotalCnt - gsRamLog->sModule[ui32ModID].ui32RdIdx;

		ui32RdIdx = gsRamLog->sModule[ui32ModID].ui32RdIdx;
		ui32RdOffset = ui32ModID*gsRamLog->ui32ModSize + ui32RdIdx*RAM_LOG_UNIT_SIZE;

		pui32RdVirPtr = psRamLog_ui32VirBasePtr+(ui32RdOffset>>2);

		ui32CopySize = ui32CopySize *RAM_LOG_UNIT_SIZE;
		memcpy((void *)pui8Target, (void *)pui32RdVirPtr, ui32CopySize);
		pui8Target+=ui32CopySize;
		ui32TmpCopySize = ui32CopySize;

		gsRamLog->sModule[ui32ModID].ui32RdIdx = 0;
	}

	ui32CopySize =gsRamLog->sModule[ui32ModID].ui32WrIdx - gsRamLog->sModule[ui32ModID].ui32RdIdx;

	if(ui32CopySize>0)
	{
		ui32RdIdx = gsRamLog->sModule[ui32ModID].ui32RdIdx;
		ui32RdOffset = ui32ModID*gsRamLog->ui32ModSize + ui32RdIdx*RAM_LOG_UNIT_SIZE;

		pui32RdVirPtr = psRamLog_ui32VirBasePtr+(ui32RdOffset>>2);

		ui32CopySize = ui32CopySize *RAM_LOG_UNIT_SIZE;

		memcpy((void *)pui8Target, (void *)pui32RdVirPtr, ui32CopySize);
		ui32TmpCopySize += ui32CopySize;
	}

	ui32CopySize = ui32TmpCopySize;
	gsRamLog->sModule[ui32ModID].ui32RdIdx = gsRamLog->sModule[ui32ModID].ui32WrIdx;

	return ui32CopySize;
}

/** @} */

