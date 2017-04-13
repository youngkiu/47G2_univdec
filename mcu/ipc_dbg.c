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
 * author     youngki.lyu (youngki.lyu@lge.com)
 * version    1.0
 * date       2010.05.08
 * note       Additional information.
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/
#define		IPC_DBG_BUF_SIZE				0x8000

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "ipc_dbg.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "vdec_shm.h"

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#endif

#include "os_adap.h"

#include "../hal/lg1152/mcu_hal_api.h"
#include "vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		IPC_DBG_MAGIC_CODE		0x19C0DB90
#define		IPC_DBG_MAGIC_CLEAR		0x09BD0C91
#define		IPC_DBG_PADDING_BITS	0xDB9F9ADE

#define		IPC_DBG_MAX_STRING_LENGTH	0x100

#define		IPC_DBG_ERROR_CODE_01	0xF0000001
#define		IPC_DBG_ERROR_CODE_02	0xF0000002
#define		IPC_DBG_ERROR_CODE_03	0xF0000003
#define		IPC_DBG_ERROR_CODE_04	0xF0000004
#define		IPC_DBG_DEBUG_CODE_01	0xF0000011

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
#ifndef __XTENSA__
struct workqueue_struct *_IPC_DBG_workqueue;

void _IPC_DBG_Receive_workfunc(struct work_struct *data);

DECLARE_WORK( _IPC_DBG_work, _IPC_DBG_Receive_workfunc );
#endif

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT32 		*pui32VirBasePtr;	// constant
	UINT32 		ui32BufSize;			// constant	// byte size
}gsIpcDbg;

#ifndef __XTENSA__
static spinlock_t	stIpcDbgSpinlock;
#endif

/*-------------------
 |	MAGIC CODE		|
 --------------------
 |	LENGTH			|
 --------------------
 |	DEBUG			|
 |	STRING			|
 |					|
 --------------------
 |	MAGIC CODE		|
 --------------------
 |	LENGTH			|
 --------------------
 |	DEBUG			|
 |	STRING			|
 |					|
 --------------------
 |	PADDING_BITS	|
 |	PADDING_BITS	|
 ------------------*/


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
void IPC_DBG_Init(void)
{
	UINT32	ui32MemPtr;
	UINT32	ui32MemSize;

#ifdef __XTENSA__
	ui32MemPtr = IPC_REG_DBG_GetBaseAddr();
	ui32MemSize = IPC_REG_DBG_GetBufSize();
#else
	ui32MemPtr = VDEC_SHM_Malloc(IPC_DBG_BUF_SIZE);
	ui32MemSize = IPC_DBG_BUF_SIZE;

	IPC_REG_DBG_SetBaseAddr(ui32MemPtr);
	IPC_REG_DBG_SetBufSize(ui32MemSize);

	VDEC_KDRV_Message(_TRACE, "[IPC][DBG] Base:0x%X, Size: 0x%X, %s", ui32MemPtr, ui32MemSize, __FUNCTION__ );

	_IPC_DBG_workqueue = create_workqueue("VDEC_IPC_DBG");
	spin_lock_init(&stIpcDbgSpinlock);

	IPC_REG_Set_McuDebugMask( (1 << ERROR) | (1 << DBG_SYS) );
//	IPC_REG_Set_McuDebugMask( (1 << ERROR) | (1 << WARNING) | (1 << DBG_SYS) | (1 << _TRACE) | (1 << MONITOR) );
#endif

	gsIpcDbg.ui32PhyBaseAddr = ui32MemPtr;
	gsIpcDbg.ui32BufSize = ui32MemSize;
	gsIpcDbg.pui32VirBasePtr = VDEC_TranselateVirualAddr(ui32MemPtr, ui32MemSize);
#ifdef __XTENSA__
	memset(gsIpcDbg.pui32VirBasePtr , 0x0, ui32MemSize);
	IPC_REG_DBG_SetWrOffset(0);
	IPC_REG_DBG_SetRdOffset(0);
#endif
}
#ifdef __XTENSA__
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
static BOOLEAN _IPC_DBG_IsWithinRange(UINT32 ui32StartAddr, UINT32 ui32EndAddr, UINT32 ui32TargetAddr)
{
	if( ui32StartAddr <= ui32EndAddr )
	{
		if( (ui32TargetAddr > ui32StartAddr) &&
			(ui32TargetAddr <= ui32EndAddr) )
			return TRUE;
	}
	else
	{
		if( (ui32TargetAddr > ui32StartAddr) ||
			(ui32TargetAddr <= ui32EndAddr) )
			return TRUE;
	}

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
static UINT32 _IPC_DBG_CheckOverflow(UINT32 ui32DbgSize, UINT32 ui32WrOffset, UINT32 ui32RdOffset)
{
	UINT32		ui32WrOffset_Start, ui32WrOffset_End;

	if( ui32DbgSize > gsIpcDbg.ui32BufSize )
	{
		IPC_REG_DBG_SetBaseAddr(IPC_DBG_ERROR_CODE_01);
		IPC_REG_DBG_SetBufSize(ui32DbgSize);
//		VDEC_KDRV_Message(ERROR, "[IPC][Err]DBG] Overflow - Too Big DBG Message Size: %d", ui32DbgSize );
		return IPC_REG_INVALID_OFFSET;
	}

	ui32WrOffset_Start = ui32WrOffset;
	if( (ui32WrOffset_Start + ui32DbgSize) >= gsIpcDbg.ui32BufSize )
	{
		ui32WrOffset_Start = 0;
	}
	ui32WrOffset_End = ui32WrOffset_Start + ui32DbgSize;

	if( _IPC_DBG_IsWithinRange(ui32WrOffset, ui32WrOffset_End, ui32RdOffset) == TRUE )
	{
		IPC_REG_DBG_SetBaseAddr(IPC_DBG_ERROR_CODE_02);
		IPC_REG_DBG_SetBufSize(ui32WrOffset);
//		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Overflow - Write:0x%X, Size:0x%X, Read:0x%X", ui32WrOffset, ui32DbgSize, ui32RdOffset );
		return IPC_REG_INVALID_OFFSET;
	}

	if( ui32WrOffset_Start != ui32WrOffset )
	{
		UINT32	ui32WrIndex;

		IPC_REG_DBG_SetBaseAddr(IPC_DBG_DEBUG_CODE_01);
		IPC_REG_DBG_SetBufSize(ui32WrOffset);
//		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][DBG] Padding - Wr:%d, Buf Size:%d", ui32WrOffset, gsIpcDbg.ui32BufSize );

		for( ui32WrIndex = ui32WrOffset; ui32WrIndex < gsIpcDbg.ui32BufSize; ui32WrIndex += 4 )
			gsIpcDbg.pui32VirBasePtr[ui32WrIndex>>2] = IPC_DBG_PADDING_BITS;
	}

	return ui32WrOffset_Start;
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
BOOLEAN IPC_DBG_Send(UINT32 mode, const char *fmt, ...)
{
	static char		ui8Buffer[IPC_DBG_MAX_STRING_LENGTH];
	UINT32			*pui32Buffer;
	__VALIST ap;
	UINT32			ui32BufLengByte;
	UINT32			ui32WrOffset, ui32WrOffset_Org;
	UINT32			ui32RdOffset;
	volatile UINT32	*pui32WrVirPtr;
	volatile UINT32	ui32WrOffset_Aligned, ui32WriteConfirm;
	UINT32			ui32WriteFailRetryCount = 0;

	if( !gsIpcDbg.pui32VirBasePtr )
	{
		IPC_REG_DBG_SetBaseAddr(IPC_DBG_ERROR_CODE_03);
//		VDEC_KDRV_Message(ERROR, "[IPC][Err][CMD] Not Initialised" );
		return FALSE;
	}

	switch( IPC_REG_DBG_GetBaseAddr() )
	{
	case IPC_DBG_ERROR_CODE_01 :
	case IPC_DBG_ERROR_CODE_02 :
	case IPC_DBG_ERROR_CODE_03 :
	case IPC_DBG_ERROR_CODE_04 :
		return FALSE;
		break;
	default :
		break;
	}

	if( !(IPC_REG_Get_McuDebugMask() & (1 << mode)) )
		return FALSE;

	// 1. Calculate Arguments Length
	va_start(ap, fmt);
	ui32BufLengByte = vsnprintf(ui8Buffer, 0xFF, fmt, ap);
	va_end(ap);

	ui8Buffer[ui32BufLengByte] = 0x0;
	ui32BufLengByte++;

	ui32WrOffset = IPC_REG_DBG_GetWrOffset();
	ui32RdOffset = IPC_REG_DBG_GetRdOffset();

	// 2. Check DBG Buffer Overflow
	ui32WrOffset = _IPC_DBG_CheckOverflow(12 + ui32BufLengByte, ui32WrOffset, ui32RdOffset);	// 12: MAGIC_CODE(4) + Length(4) + Mode(4)
	if( ui32WrOffset == IPC_REG_INVALID_OFFSET )
		return FALSE;

	// 5. Update Write IPC Register
	ui32WrOffset_Aligned = ui32WrOffset + 12 + IPC_REG_CEILING_4BYTES(ui32BufLengByte);
	if( ui32WrOffset_Aligned >= gsIpcDbg.ui32BufSize )
		ui32WrOffset_Aligned = 0;

	pui32WrVirPtr = gsIpcDbg.pui32VirBasePtr;

	ui32WrOffset_Org = ui32WrOffset;

IPC_DBG_Write_Retry :
	IPC_REG_DBG_SetWrOffset(ui32WrOffset_Aligned);

	// 3.1 Magic Code
	pui32WrVirPtr[ui32WrOffset>>2] = IPC_DBG_MAGIC_CODE;
	ui32WrOffset += 4;
	// 3.2 Write Length
	pui32WrVirPtr[ui32WrOffset>>2] = ui32BufLengByte;
	ui32WrOffset += 4;
	// 3.3 Write Mode
	pui32WrVirPtr[ui32WrOffset>>2] = mode;
	ui32WrOffset += 4;

	// 4. Write Debug String
	memcpy((char *)&pui32WrVirPtr[ui32WrOffset>>2], ui8Buffer, ui32BufLengByte);
//	ui32WrOffset += ui32BufLengByte;

	// 5. Update Write IPC Register
//	IPC_REG_DBG_SetWrOffset(ui32WrOffset);

	// 6. Confirm Writing
	ui32WriteConfirm = pui32WrVirPtr[ui32WrOffset_Org>>2];
	if( ui32WriteConfirm != IPC_DBG_MAGIC_CODE )
	{
		ui32WriteFailRetryCount++;
		if( ui32WriteFailRetryCount < 3 )
		{
			ui32WrOffset = ui32WrOffset_Org;
			goto IPC_DBG_Write_Retry;
		}
	}
	if( ui32WriteFailRetryCount > 1 )
	{
		IPC_REG_DBG_SetBaseAddr(IPC_DBG_ERROR_CODE_04);
		IPC_REG_DBG_SetBufSize(ui32WriteFailRetryCount);
	}

	MCU_HAL_SetExtIntrStatus(MCU_ISR_IPC, 1);
	MCU_HAL_SetIntrEvent();

	return TRUE;
}
#else
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
static UINT32 _IPC_DBG_VerifyMagicCode(UINT32 *pui32RdVirPtr, UINT32 ui32RdOffset)
{
	UINT32			ui32RdOffset_Org;
	UINT32			ui32ReadFailRetryCount = 0;

	ui32RdOffset_Org = ui32RdOffset;

_IPC_DBG_VerifyMagicCode_Retry :
	if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_DBG_MAGIC_CODE )
	{
		pui32RdVirPtr[ui32RdOffset>>2] = IPC_DBG_MAGIC_CLEAR;
		return ui32RdOffset;
	}

//	VDEC_KDRV_Message(DBG_IPC, "[IPC][Dbg][DBG] Not Found MAGIC CODE - Rd: 0x%X", ui32RdOffset );

	if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_DBG_PADDING_BITS )
	{
		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][DBG] Padding - Rd:%d, Buf Size:%d", ui32RdOffset, gsIpcDbg.ui32BufSize );

		ui32RdOffset = 0;
		if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_DBG_MAGIC_CODE )
		{
			VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][DBG] WrapAround - Rd:%d, Buf Size:%d", ui32RdOffset, gsIpcDbg.ui32BufSize );
			pui32RdVirPtr[ui32RdOffset>>2] = IPC_DBG_MAGIC_CLEAR;
			return ui32RdOffset;
		}
	}

	ui32ReadFailRetryCount++;
	if( ui32ReadFailRetryCount < 0x10 )
	{
		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][DBG] Retry to Verify Magic Code: %d", ui32ReadFailRetryCount );
		ui32RdOffset = ui32RdOffset_Org;
		mdelay(1);
		goto _IPC_DBG_VerifyMagicCode_Retry;
	}

	VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] No MAGIC CODE: 0x%08X - Rd: 0x%X, BufSize: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, gsIpcDbg.ui32BufSize );

	return IPC_REG_INVALID_OFFSET;
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
static UINT32 _IPC_DBG_FindNextMagicCode(UINT32 *pui32RdVirPtr, UINT32 ui32RdOffset, UINT32 ui32WrOffset)
{
	while( ui32RdOffset != ui32WrOffset )
	{
		if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_DBG_MAGIC_CODE )
		{
			pui32RdVirPtr[ui32RdOffset>>2] = IPC_DBG_MAGIC_CLEAR;
			return ui32RdOffset;
		}
		else if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_DBG_PADDING_BITS )
		{
			UINT32	ui32RdOffset_i = ui32RdOffset;

			while( ui32RdOffset_i < gsIpcDbg.ui32BufSize )
			{
				if( pui32RdVirPtr[ui32RdOffset_i>>2] != IPC_DBG_PADDING_BITS )
				{
					VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] No Next PADDING CODE: 0x%X - Rd: 0x%X(0x%X), BufSize: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, ui32RdOffset_i, gsIpcDbg.ui32BufSize );
				}

//				VDEC_KDRV_Message(DBG_IPC, "[IPC][Dbg][DBG] PADDING BITS - Rd: 0x%X", ui32RdOffset );

				ui32RdOffset_i += 4;
				if( ui32RdOffset_i == ui32WrOffset )
				{
					VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] PADDING CODE Area: 0x%X - Rd: 0x%X(0x%X), Wr: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, ui32RdOffset_i, ui32WrOffset );
					break;
				}
			}
			ui32RdOffset = ui32RdOffset_i;

			if( ui32RdOffset == gsIpcDbg.ui32BufSize )
				ui32RdOffset = 0;
		}
		else
		{
			ui32RdOffset += 4;
			if( (ui32RdOffset + 4) >= gsIpcDbg.ui32BufSize )
				ui32RdOffset = 0;
		}
	}

	VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Rd: 0x%X, Wr: 0x%X - Data: 0x%X", ui32RdOffset, ui32WrOffset, pui32RdVirPtr[ui32RdOffset>>2] );

	return IPC_REG_INVALID_OFFSET;
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
static UINT32 _IPC_DBG_Receive(void)
{
	UINT32			ui32WrOffset;
	UINT32			ui32RdOffset, ui32RdOffset_Org;
	volatile UINT32	*pui32RdVirPtr;
	UINT32			ui32DbgBodySize;
	UINT32			ui32PrintMode;
	volatile UINT32	ui32RdOffset_Aligned;
	UINT32			ui32ReadFailRetryCount = 0;

	ui32WrOffset = IPC_REG_DBG_GetWrOffset();
	ui32RdOffset = IPC_REG_DBG_GetRdOffset();

	switch( IPC_REG_DBG_GetBaseAddr() )
	{
	case IPC_DBG_ERROR_CODE_01 :
		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Too Big DBG Message Size: %d", IPC_REG_DBG_GetBufSize() );
		goto IPC_DBG_ERROR_Recovery;
		break;
	case IPC_DBG_ERROR_CODE_02 :
		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Overflow - Rd:0x%X, Wr:0x%X", ui32RdOffset, IPC_REG_DBG_GetBufSize() );
		goto IPC_DBG_ERROR_Recovery;
		break;
	case IPC_DBG_ERROR_CODE_03 :
		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Not Initialised" );
		goto IPC_DBG_ERROR_Recovery;
		break;
	case IPC_DBG_ERROR_CODE_04 :
		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Fail to Write - Retry Count: %d", IPC_REG_DBG_GetBufSize() );
		goto IPC_DBG_ERROR_Recovery;
		break;
	case IPC_DBG_DEBUG_CODE_01 :
		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][DBG] Padding - Wr:%d, Buf Size:%d", IPC_REG_DBG_GetBufSize(), gsIpcDbg.ui32BufSize );
		goto IPC_DBG_ERROR_Recovery;
		break;
IPC_DBG_ERROR_Recovery :
		IPC_REG_DBG_SetBaseAddr(gsIpcDbg.ui32PhyBaseAddr);
		IPC_REG_DBG_SetBufSize(gsIpcDbg.ui32BufSize);
	default :
		break;
	}

	if( !gsIpcDbg.pui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][CMD] Not Initialised" );
		return 0;
	}

	if( ui32RdOffset == ui32WrOffset )
		return 0;

	pui32RdVirPtr = gsIpcDbg.pui32VirBasePtr;

	ui32RdOffset_Org = ui32RdOffset;

IPC_DBG_Read_Retry :
	// 1. Check Magic Code
	ui32RdOffset = _IPC_DBG_VerifyMagicCode((UINT32 *)pui32RdVirPtr, ui32RdOffset);
	if( ui32RdOffset == IPC_REG_INVALID_OFFSET )
	{
		ui32RdOffset = ui32RdOffset_Org;

		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Try to Find Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );

		ui32RdOffset = _IPC_DBG_FindNextMagicCode((UINT32 *)pui32RdVirPtr, ui32RdOffset, ui32WrOffset);
		if( ui32RdOffset == IPC_REG_INVALID_OFFSET )
		{
			ui32RdOffset = ui32RdOffset_Org;
			VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Not Found Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );
			return 0;
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] Found Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );
		}
	}
	ui32RdOffset += 4;

	// 2. Read Length Field
	ui32DbgBodySize = pui32RdVirPtr[ui32RdOffset>>2];
	if( ui32DbgBodySize > IPC_DBG_MAX_STRING_LENGTH )
	{
		ui32ReadFailRetryCount++;
		if( ui32ReadFailRetryCount < 3 )
		{
			ui32RdOffset = ui32RdOffset_Org;
			goto IPC_DBG_Read_Retry;
		}
	}
	if( ui32ReadFailRetryCount > 1 )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] String Length:0x%08X, Retry Count:%d - Rd:0x%X, Wr:0x%X", ui32DbgBodySize, ui32ReadFailRetryCount, ui32RdOffset_Org, ui32WrOffset );
	}
	ui32RdOffset += 4;

	// 5. Update Read IPC Register
	ui32RdOffset_Aligned = ui32RdOffset + 4 + IPC_REG_CEILING_4BYTES(ui32DbgBodySize);
	if( ui32RdOffset_Aligned >= gsIpcDbg.ui32BufSize )
	{
		if( ui32RdOffset_Aligned > gsIpcDbg.ui32BufSize )
			VDEC_KDRV_Message(ERROR, "[IPC][Err][DBG] String Length:0x%08X - Rd:0x%X, Wr:0x%X", ui32DbgBodySize, ui32RdOffset_Org, ui32WrOffset );

		ui32RdOffset_Aligned = 0;
	}
	IPC_REG_DBG_SetRdOffset(ui32RdOffset_Aligned);

	// 3. Read Mode Field
	ui32PrintMode = pui32RdVirPtr[ui32RdOffset>>2];
	ui32RdOffset += 4;

	// 4. Print to host debug port
	VDEC_KDRV_Message(ui32PrintMode, "%s", (char *)&pui32RdVirPtr[ui32RdOffset>>2] );
//	ui32RdOffset += ui32DbgBodySize;

	// 5. Update Read IPC Register
//	IPC_REG_DBG_SetRdOffset(ui32RdOffset);

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
void _IPC_DBG_Receive_workfunc(struct work_struct *data)
{
	spin_lock(&stIpcDbgSpinlock);

	while( _IPC_DBG_Receive() );

	spin_unlock(&stIpcDbgSpinlock);
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
void IPC_DBG_Receive(void)
{
	queue_work(_IPC_DBG_workqueue, &_IPC_DBG_work);
}
#endif
/** @} */

