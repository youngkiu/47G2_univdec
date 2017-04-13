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
#define		IPC_REQ_BUF_SIZE				0x4000

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "ipc_req.h"
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
#define		IPC_REQ_MAGIC_CODE		0x19C08E90
#define		IPC_REQ_MAGIC_CLEAR		0x09E80C91
#define		IPC_REQ_PADDING_BITS	0x8E9F9ADE

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT32 		*pui32VirBasePtr;	// constant
	UINT32 		ui32BufSize;			// constant

	// for ARM ISR --> ARM BottomHalf
	UINT32		ui32WrOffset;	// byte size
	UINT32		ui32RdOffset;	// byte size
} S_IPC_REQ_BUF_T;

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
static void _IPC_REQ_Receive_workfunc(struct work_struct *data);

DECLARE_WORK( _IPC_REQ_work, _IPC_REQ_Receive_workfunc );
#endif

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static UINT32		gIpcReqMem_Arm[IPC_REQ_BUF_SIZE/4 * 2];
static S_IPC_REQ_BUF_T		gsIpcReq_Arm;	// ARM ISR --> ARM BottomHalf
static S_IPC_REQ_BUF_T		gsIpcReq_Mcu;	// MCU ISR --> ARM BottomHalf
static IPC_REQ_RECEIVER_CALLBACK_T 		fpIpcReq_Receiver[IPC_REQ_ID_MAX];

#ifndef __XTENSA__
struct workqueue_struct *_IPC_REQ_workqueue;
static spinlock_t	stIpcReqTxSpinlock;
static spinlock_t	stIpcReqRxSpinlock;
#endif

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
void IPC_REQ_Init(void)
{
	UINT32	ui32MemPtr;
	UINT32	ui32MemSize;
#ifndef __XTENSA__
	UINT32	i;
#endif

#ifdef __XTENSA__
	ui32MemPtr = IPC_REG_REQ_GetBaseAddr();
	ui32MemSize = IPC_REG_REQ_GetBufSize();
#else
	ui32MemPtr = VDEC_SHM_Malloc(IPC_REQ_BUF_SIZE);
	ui32MemSize = IPC_REQ_BUF_SIZE;

	IPC_REG_REQ_SetBaseAddr(ui32MemPtr);
	IPC_REG_REQ_SetBufSize(ui32MemSize);
	IPC_REG_REQ_SetWrOffset(0);
	IPC_REG_REQ_SetRdOffset(0);

	VDEC_KDRV_Message(_TRACE, "[IPC][REQ] Base:0x%X, Size: 0x%X, %s", ui32MemPtr, ui32MemSize, __FUNCTION__ );


	_IPC_REQ_workqueue = create_workqueue("VDEC_IPC_REQ");
	spin_lock_init(&stIpcReqTxSpinlock);
	spin_lock_init(&stIpcReqRxSpinlock);

	gsIpcReq_Arm.pui32VirBasePtr = gIpcReqMem_Arm;
	gsIpcReq_Arm.ui32BufSize = IPC_REQ_BUF_SIZE * 2;
	gsIpcReq_Arm.ui32WrOffset = 0;
	gsIpcReq_Arm.ui32RdOffset = 0;

	for( i = 0; i < IPC_REQ_ID_MAX; i++ )
	{
		fpIpcReq_Receiver[i] = NULL;
	}
#endif
	gsIpcReq_Mcu.ui32PhyBaseAddr = ui32MemPtr;
	gsIpcReq_Mcu.ui32BufSize = ui32MemSize;
	gsIpcReq_Mcu.pui32VirBasePtr = VDEC_TranselateVirualAddr(ui32MemPtr, ui32MemSize);

#ifdef __XTENSA__
	memset(gsIpcReq_Mcu.pui32VirBasePtr, 0x0, ui32MemSize);
#endif
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
static BOOLEAN _IPC_REQ_IsWithinRange(UINT32 ui32StartAddr, UINT32 ui32EndAddr, UINT32 ui32TargetAddr)
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
static UINT32 _IPC_REQ_CheckOverflow(UINT32 ui32ReqSize, S_IPC_REQ_BUF_T *pIpcReq)
{
	UINT32		ui32WrOffset, ui32WrOffset_Org;
	UINT32		ui32RdOffset;
	static UINT32	ui32LogTick = 0xFFFFFFF0;

	if( ui32ReqSize > pIpcReq->ui32BufSize )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err]REQ] Overflow - Too Big REQ Message Size: %d, Buf Size:%d", ui32ReqSize, pIpcReq->ui32BufSize );
		return IPC_REG_INVALID_OFFSET;
	}

	ui32WrOffset_Org = pIpcReq->ui32WrOffset;
	ui32WrOffset = ui32WrOffset_Org;
	ui32RdOffset = pIpcReq->ui32RdOffset;
	if( (ui32WrOffset + ui32ReqSize) >= pIpcReq->ui32BufSize )
	{
		ui32WrOffset = 0;
	}

	if( _IPC_REQ_IsWithinRange(ui32WrOffset_Org, ui32WrOffset + ui32ReqSize, ui32RdOffset) == TRUE )
	{
		ui32LogTick++;
		if( ui32LogTick >= 0x80 )
		{
			VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Overflow - Write:0x%X, Size:0x%X, Read:0x%X, Buf Size:%d", ui32WrOffset, ui32ReqSize, ui32RdOffset, pIpcReq->ui32BufSize );
			ui32LogTick = 0;
		}
		return IPC_REG_INVALID_OFFSET;
	}
	else
	{
		ui32LogTick = 0xFFFFFFF0;
	}

	if( ui32WrOffset != ui32WrOffset_Org )
	{
		UINT32	ui32WrIndex;

		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][REQ] Padding - Wr:%d, Buf Size:%d", ui32WrOffset, pIpcReq->ui32BufSize );

		for( ui32WrIndex = ui32WrOffset_Org; ui32WrIndex < pIpcReq->ui32BufSize; ui32WrIndex += 4 )
			pIpcReq->pui32VirBasePtr[ui32WrIndex>>2] = IPC_REQ_PADDING_BITS;
	}

	return ui32WrOffset;
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
UINT32 _IPC_REQ_Write(E_IPC_REQ_ID_T eIpcReqId, UINT32 ui32BodySize, void *pIpcBody, S_IPC_REQ_BUF_T *pIpcReq)
{
	volatile UINT32	*pui32WrVirPtr;
	const UINT32	ui32ReqHeaderSize = 12;	// 4 * 3 : Header Size(MAGIC CODE + Header Type + Body Length)
	UINT32		ui32WrOffset, ui32WrOffset_Org;
	volatile UINT32	ui32WriteConfirm;
	UINT32		ui32WriteFailRetryCount = 0;

	// 1. Check Buffer Overflow
	ui32WrOffset = _IPC_REQ_CheckOverflow(ui32ReqHeaderSize + ui32BodySize, pIpcReq);
	if( ui32WrOffset == IPC_REG_INVALID_OFFSET )
		return IPC_REG_INVALID_OFFSET;

	pui32WrVirPtr = pIpcReq->pui32VirBasePtr;

	ui32WrOffset_Org = ui32WrOffset;

IPC_REQ_Write_Retry :
	// 2. Write Header
	pui32WrVirPtr[ui32WrOffset>>2] = IPC_REQ_MAGIC_CODE;
	ui32WrOffset += 4;
	pui32WrVirPtr[ui32WrOffset>>2] = (UINT32)eIpcReqId;
	ui32WrOffset += 4;

	// 3. Write Length
	pui32WrVirPtr[ui32WrOffset>>2] = ui32BodySize;
	ui32WrOffset += 4;

	// 4. Write Body
	memcpy((void *)&pui32WrVirPtr[ui32WrOffset>>2], (void *)pIpcBody, ui32BodySize);
	ui32WrOffset += ui32BodySize;

	ui32WrOffset = IPC_REG_CEILING_4BYTES(ui32WrOffset);

	if( ui32WrOffset >= pIpcReq->ui32BufSize )
		ui32WrOffset = 0;

	// 6. Confirm Writing
	ui32WriteConfirm = pui32WrVirPtr[ui32WrOffset_Org>>2];
	if( ui32WriteConfirm != IPC_REQ_MAGIC_CODE )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Fail to Write - Wr: 0x%X(0x%X), Rd: 0x%X, Retry Count: %d", pIpcReq->ui32WrOffset, ui32WrOffset, pIpcReq->ui32RdOffset, ui32WriteFailRetryCount );

		ui32WriteFailRetryCount++;
		if( ui32WriteFailRetryCount < 3 )
		{
			ui32WrOffset = ui32WrOffset_Org;
			goto IPC_REQ_Write_Retry;
		}
	}

	return ui32WrOffset;
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
BOOLEAN IPC_REQ_Send(E_IPC_REQ_ID_T eIpcReqId, UINT32 ui32BodySize, void *pIpcBody)
{
	UINT32		ui32WrOffset;

	if( !gsIpcReq_Mcu.pui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Not Initialised" );
		return FALSE;
	}

	gsIpcReq_Mcu.ui32WrOffset = IPC_REG_REQ_GetWrOffset();
	gsIpcReq_Mcu.ui32RdOffset = IPC_REG_REQ_GetRdOffset();
	ui32WrOffset = _IPC_REQ_Write(eIpcReqId, ui32BodySize, pIpcBody, &gsIpcReq_Mcu);
	if( ui32WrOffset == IPC_REG_INVALID_OFFSET )
		return FALSE;

	// 5. Update Write IPC Register
	IPC_REG_REQ_SetWrOffset(ui32WrOffset);

	MCU_HAL_SetExtIntrStatus(MCU_ISR_IPC, 1);	// external interrupt : arm receive
	MCU_HAL_SetIntrEvent();

	return TRUE;
}
#else // #ifdef __XTENSA__
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
BOOLEAN IPC_REQ_Send(E_IPC_REQ_ID_T eIpcReqId, UINT32 ui32BodySize, void *pIpcBody)
{
	UINT32		ui32WrOffset;
	unsigned long		irq_flag;

	spin_lock_irqsave(&stIpcReqTxSpinlock, irq_flag);

	if( !gsIpcReq_Arm.pui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Not Initialised" );
		spin_unlock_irqrestore(&stIpcReqTxSpinlock, irq_flag);
		return FALSE;
	}

	ui32WrOffset = _IPC_REQ_Write(eIpcReqId, ui32BodySize, pIpcBody, &gsIpcReq_Arm);
	if( ui32WrOffset == IPC_REG_INVALID_OFFSET )
	{
		spin_unlock_irqrestore(&stIpcReqTxSpinlock, irq_flag);
		return FALSE;
	}

	// 5. Update Write Register
	gsIpcReq_Arm.ui32WrOffset = ui32WrOffset;

	spin_unlock_irqrestore(&stIpcReqTxSpinlock, irq_flag);

	IPC_REQ_Receive();

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
BOOLEAN IPC_REQ_Register_ReceiverCallback(E_IPC_REQ_ID_T eIpcReqId, IPC_REQ_RECEIVER_CALLBACK_T fpCallback)
{
	if( eIpcReqId >= IPC_REQ_ID_MAX )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] IPC REQ ID: %d", eIpcReqId );
		return FALSE;
	}

	if( fpIpcReq_Receiver[eIpcReqId] != NULL )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Callback of IPC REQ ID(%d) not Empth", eIpcReqId );
	}

	fpIpcReq_Receiver[eIpcReqId] = fpCallback;

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
static UINT32 _IPC_REQ_VerifyMagicCode(UINT32 *pui32RdVirPtr, UINT32 ui32RdOffset, UINT32 ui32BufSize)
{
	UINT32			ui32RdOffset_Org;
	UINT32			ui32ReadFailRetryCount = 0;

	ui32RdOffset_Org = ui32RdOffset;

_IPC_REQ_VerifyMagicCode_Retry :
	if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_REQ_MAGIC_CODE )
	{
		pui32RdVirPtr[ui32RdOffset>>2] = IPC_REQ_MAGIC_CLEAR;
		return ui32RdOffset;
	}

//	VDEC_KDRV_Message(DBG_IPC, "[IPC][Dbg][REQ] Not Found MAGIC CODE - Read Offset: 0x%X", ui32RdOffset );

	if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_REQ_PADDING_BITS )
	{
		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][REQ] Padding - Rd:%d, Buf Size:%d", ui32RdOffset, ui32BufSize );

		ui32RdOffset = 0;
		if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_REQ_MAGIC_CODE )
		{
			VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][REQ] WrapAround - Rd:%d, Buf Size:%d", ui32RdOffset, ui32BufSize );
			pui32RdVirPtr[ui32RdOffset>>2] = IPC_REQ_MAGIC_CLEAR;
			return ui32RdOffset;
		}
	}

	ui32ReadFailRetryCount++;
	if( ui32ReadFailRetryCount < 0x10 )
	{
		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][REQ] Retry to Verify Magic Code: %d", ui32ReadFailRetryCount );
		ui32RdOffset = ui32RdOffset_Org;
		mdelay(1);
		goto _IPC_REQ_VerifyMagicCode_Retry;
	}

	VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] No MAGIC CODE: 0x%08X - Rd: 0x%X, BufSize: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, ui32BufSize );

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
static UINT32 _IPC_REQ_FindNextMagicCode(UINT32 *pui32RdVirPtr, UINT32 ui32RdOffset, UINT32 ui32WrOffset, UINT32 ui32BufSize)
{
	while( ui32RdOffset != ui32WrOffset )
	{
		if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_REQ_MAGIC_CODE )
		{
			pui32RdVirPtr[ui32RdOffset>>2] = IPC_REQ_MAGIC_CLEAR;
			return ui32RdOffset;
		}
		else if( pui32RdVirPtr[ui32RdOffset>>2] == IPC_REQ_PADDING_BITS )
		{
			UINT32	ui32RdOffset_i = ui32RdOffset;

			while( ui32RdOffset_i < ui32BufSize )
			{
				if( pui32RdVirPtr[ui32RdOffset_i>>2] != IPC_REQ_PADDING_BITS )
				{
					VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] No Next PADDING CODE: 0x%X - Rd: 0x%X(0x%X), BufSize: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, ui32RdOffset_i, ui32BufSize );
				}

//				VDEC_KDRV_Message(REQ_IPC, "[IPC][DBG][REQ] PADDING BITS - Rd: 0x%X", ui32RdOffset );

				ui32RdOffset_i += 4;
				if( ui32RdOffset_i == ui32WrOffset )
				{
					VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] PADDING CODE Area: 0x%X - Rd: 0x%X(0x%X), Wr: 0x%X", pui32RdVirPtr[ui32RdOffset>>2], ui32RdOffset, ui32RdOffset_i, ui32WrOffset );
					break;
				}
			}
			ui32RdOffset = ui32RdOffset_i;

			if( ui32RdOffset == ui32BufSize )
				ui32RdOffset = 0;
		}
		else
		{
			ui32RdOffset += 4;
			if( (ui32RdOffset + 4) >= ui32BufSize )
				ui32RdOffset = 0;
		}
	}

	VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Rd: 0x%X, Wr: 0x%X - Data: 0x%X", ui32RdOffset, ui32WrOffset, pui32RdVirPtr[ui32RdOffset>>2] );

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
static UINT32 _IPC_REQ_Read(S_IPC_REQ_BUF_T *pIpcReq)
{
	UINT32		ui32WrOffset = pIpcReq->ui32WrOffset;
	UINT32		ui32RdOffset = pIpcReq->ui32RdOffset;
	E_IPC_REQ_ID_T eIpcReqId;
	UINT32		ui32BodySize;
	volatile UINT32	*pui32RdVirPtr;


	if( ui32RdOffset == ui32WrOffset )
		return IPC_REG_INVALID_OFFSET;

	pui32RdVirPtr = pIpcReq->pui32VirBasePtr;

	// 1. Check Magic Code
	ui32RdOffset = _IPC_REQ_VerifyMagicCode((UINT32 *)pui32RdVirPtr, ui32RdOffset, pIpcReq->ui32BufSize);
	if( ui32RdOffset == IPC_REG_INVALID_OFFSET )
	{
		ui32RdOffset = pIpcReq->ui32RdOffset;

		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Try to Find Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );

		ui32RdOffset = _IPC_REQ_FindNextMagicCode((UINT32 *)pui32RdVirPtr, ui32RdOffset, ui32WrOffset, pIpcReq->ui32BufSize);
		if( ui32RdOffset == IPC_REG_INVALID_OFFSET )
		{
			ui32RdOffset = pIpcReq->ui32RdOffset;
			VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Not Found Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );
			return IPC_REG_INVALID_OFFSET;
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Found Next MAGIC Code - Rd:0x%X, Wr:0x%X", ui32RdOffset, ui32WrOffset );
		}
	}
	ui32RdOffset += 4;

	// 2. Read Header
	eIpcReqId = (E_IPC_REQ_ID_T)pui32RdVirPtr[ui32RdOffset>>2];
	ui32RdOffset += 4;

	if( eIpcReqId >= IPC_REQ_ID_MAX )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] IPC REQ ID: %u - Rd:0x%X, Wr:0x%X", eIpcReqId, ui32RdOffset, ui32WrOffset  );
		return IPC_REG_INVALID_OFFSET;
	}

	// 3. Read Length
	ui32BodySize = pui32RdVirPtr[ui32RdOffset>>2];
	ui32RdOffset += 4;

	// 4. Call Registered Callback Function
	if( fpIpcReq_Receiver[eIpcReqId] == NULL )
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Callback of IPC REQ ID(%d) not Empth", eIpcReqId );
	else
		fpIpcReq_Receiver[eIpcReqId]( (void *)&pui32RdVirPtr[ui32RdOffset>>2] );

	ui32RdOffset += ui32BodySize;

	ui32RdOffset = IPC_REG_CEILING_4BYTES(ui32RdOffset);
	if( ui32RdOffset >= pIpcReq->ui32BufSize )
		ui32RdOffset = 0;

	return ui32RdOffset;
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
static UINT32 _IPC_REQ_Receive(void)
{
	UINT32		ui32RdOffset;
	UINT32		ui32RdCnt = 0;

	if( !gsIpcReq_Mcu.pui32VirBasePtr || !gsIpcReq_Arm.pui32VirBasePtr )
	{
		VDEC_KDRV_Message(ERROR, "[IPC][Err][REQ] Not Initialised" );
		return 0;
	}


	//{ for between MCU ISR and ARM bottomhalf
	gsIpcReq_Mcu.ui32WrOffset = IPC_REG_REQ_GetWrOffset();
	gsIpcReq_Mcu.ui32RdOffset = IPC_REG_REQ_GetRdOffset();
	ui32RdOffset = _IPC_REQ_Read(&gsIpcReq_Mcu);
	if( ui32RdOffset != IPC_REG_INVALID_OFFSET )
	{
		// 5. Update Read IPC Register
		IPC_REG_REQ_SetRdOffset(ui32RdOffset);

		VDEC_KDRV_Message(DBG_IPC, "[IPC][DBG][REQ] Wr:%d, Rd - Get/Set:%d/%d", gsIpcReq_Mcu.ui32WrOffset, gsIpcReq_Mcu.ui32RdOffset, ui32RdOffset );

		ui32RdCnt++;
	}
	//} for between MCU ISR and ARM bottomhalf


	//{ for between ARM ISR and ARM bottomhalf
	ui32RdOffset = _IPC_REQ_Read(&gsIpcReq_Arm);
	if( ui32RdOffset != IPC_REG_INVALID_OFFSET )
	{
		// 5. Update Write Register
		gsIpcReq_Arm.ui32RdOffset = ui32RdOffset;

		ui32RdCnt++;
	}
	//} for between ARM ISR and ARM bottomhalf


	return ui32RdCnt;
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
static void _IPC_REQ_Receive_workfunc(struct work_struct *data)
{
	spin_lock(&stIpcReqRxSpinlock);

	while( _IPC_REQ_Receive() );

	spin_unlock(&stIpcReqRxSpinlock);
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
void IPC_REQ_Receive(void)
{
	queue_work(_IPC_REQ_workqueue,  &_IPC_REQ_work);
}

#endif // #ifdef __XTENSA__

/** @} */

