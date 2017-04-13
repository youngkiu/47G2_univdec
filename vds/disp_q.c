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
 * date       2011.02.21
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
#include "disp_q.h"

#ifdef __XTENSA__
#include <stdio.h>
#include <string.h>
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#endif

#include "../mcu/os_adap.h"

#include "../hal/lg1152/ipc_reg_api.h"
#include "../mcu/ipc_cmd.h"

#include "../mcu/vdec_shm.h"

#include "../mcu/vdec_print.h"


#if defined(USE_MCU_FOR_TOP) || defined(USE_MCU_FOR_VDEC_VDC) || defined(USE_MCU_FOR_VDEC_VDS)
	#if !(defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDC) && defined(USE_MCU_FOR_VDEC_VDS))
		#define DISP_Q_USE_IPC_CMD
	#endif
#endif


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define		VDEC_DISPQ_NEXT_START_OFFSET( _offset, _nodesize, _queuesize )	\
				((((_offset) + (_nodesize)) >= (_queuesize)) ? 0 : (_offset) + (_nodesize))
#define		VDEC_DISPQ_NEXT_START_ADDR( _nodeaddr, _nodesize, _queuebase, _queuesize )	\
				((((_nodeaddr) + (_nodesize)) >= ((_queuebase) + (_queuesize))) ? (_queuebase) : (_nodeaddr) + (_nodesize))


/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
#ifdef DISP_Q_USE_IPC_CMD
typedef struct
{
	UINT32	ui32ch;
	UINT32	ui32DispqBase;
	UINT32	ui32DispqSize;
} S_IPC_DISPQ_INFO_T;
#endif

typedef struct
{
	BOOLEAN		bUsed;
	BOOLEAN		bAlloced;
	UINT32		ui32BufAddr;
	UINT32		ui32BufSize;
} S_DISPQ_MEM_T;


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
static S_DISPQ_MEM_T *gsDispQ_Buf = NULL;

static struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT32 		*pui32VirBasePtr;	// constant
	UINT32 		ui32QueueSize;		// constant
	UINT32		ui32NodeSize;

	// for Debug
	UINT32		ui32LogTick;
	UINT32 		ui32WrOffset;		// offset
	UINT32 		ui32RdOffset;		// offset
}gsVdecDispQ[DISP_Q_NUM_OF_CHANNEL];

#ifdef DISP_Q_USE_IPC_CMD
#ifdef __XTENSA__
static UINT32 _DISP_Q_SetDb(UINT8 ch, UINT32 ui32DispqBase, UINT32 ui32DispqSize);
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
static void _IPC_CMD_Receive_DisplayQ_Info(void *pIpcBody)
{
	S_IPC_DISPQ_INFO_T *pDispQ_Info;

	VDEC_KDRV_Message(DBG_DEIF, "[DISP_Q][DBG] %s", __FUNCTION__ );

	pDispQ_Info = (S_IPC_DISPQ_INFO_T *)pIpcBody;

	_DISP_Q_SetDb(pDispQ_Info->ui32ch, pDispQ_Info->ui32DispqBase, pDispQ_Info->ui32DispqSize);
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
static void _IPC_CMD_Send_DisplayQ_Info(UINT8 ch, UINT32 ui32DispqBase, UINT32 ui32DispqSize)
{
	S_IPC_DISPQ_INFO_T sDispQ_Info;

	VDEC_KDRV_Message(DBG_DEIF, "[DISP_Q][DBG] %s", __FUNCTION__ );

	sDispQ_Info.ui32ch = ch;
	sDispQ_Info.ui32DispqBase = ui32DispqBase;
	sDispQ_Info.ui32DispqSize = ui32DispqSize;
	IPC_CMD_Send(IPC_CMD_ID_DISP_Q_REGISTER, sizeof(S_IPC_DISPQ_INFO_T) , (void *)&sDispQ_Info);
}
#endif // #ifdef __XTENSA__
#endif // #ifdef DISP_Q_USE_IPC_CMD

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
void DISP_Q_Init(void)
{
	UINT8	ch;
	UINT32	gsDqPhy, gsDqSize;

	VDEC_KDRV_Message(_TRACE, "[DISP_Q][DBG] %s", __FUNCTION__ );

	gsDqSize = sizeof(S_DISPQ_MEM_T) * DISP_Q_NUM_OF_CHANNEL;
#if ( defined(DISP_Q_USE_IPC_CMD) && defined(__XTENSA__) )
	gsDqPhy = IPC_REG_DISPQ_GetDb();
#else
	gsDqPhy = VDEC_SHM_Malloc(gsDqSize);
	IPC_REG_DISPQ_SetDb(gsDqPhy);
#endif
	VDEC_KDRV_Message(_TRACE, "[DISP_Q][DBG] gsDqPhy: 0x%X, %s", gsDqPhy, __FUNCTION__ );

	gsDispQ_Buf = (S_DISPQ_MEM_T *)VDEC_TranselateVirualAddr(gsDqPhy, gsDqSize);

	for( ch = 0; ch < DISP_Q_NUM_OF_CHANNEL; ch++ )
	{
		gsDispQ_Buf[ch].bUsed = FALSE;
#if ( !defined(DISP_Q_USE_IPC_CMD) || (defined(DISP_Q_USE_IPC_CMD) && !defined(__XTENSA__)) )
		gsDispQ_Buf[ch].bAlloced = FALSE;
#endif
	}

	for( ch = 0; ch < DISP_Q_NUM_OF_CHANNEL; ch++ )
	{
		gsVdecDispQ[ch].ui32PhyBaseAddr = 0x0;
		gsVdecDispQ[ch].pui32VirBasePtr = 0x0;
		gsVdecDispQ[ch].ui32QueueSize = 0x0;
		gsVdecDispQ[ch].ui32NodeSize = 0x0;

		gsVdecDispQ[ch].ui32LogTick = 0;
		gsVdecDispQ[ch].ui32WrOffset = 0;
		gsVdecDispQ[ch].ui32RdOffset = 0;
#if ( !defined(DISP_Q_USE_IPC_CMD) || (defined(DISP_Q_USE_IPC_CMD) && !defined(__XTENSA__)) )
		IPC_REG_DISPQ_SetWrOffset(ch, 0);
		IPC_REG_DISPQ_SetRdOffset(ch, 0);
#endif
	}

#if (defined(DISP_Q_USE_IPC_CMD)&& defined(__XTENSA__))
	IPC_CMD_Register_ReceiverCallback(IPC_CMD_ID_DISP_Q_REGISTER, (IPC_CMD_RECEIVER_CALLBACK_T)_IPC_CMD_Receive_DisplayQ_Info);
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
static UINT32 _DISP_Q_Buf_Alloc(UINT32 ui32BufSize)
{
	UINT32		i;
	UINT32		ui32BufAddr;

	VDEC_KDRV_Message(_TRACE, "[DISP_Q][DBG] %s", __FUNCTION__ );

	for( i = 0; i < DISP_Q_NUM_OF_CHANNEL; i++ )
	{
		if( (gsDispQ_Buf[i].bUsed == FALSE) &&
			(gsDispQ_Buf[i].bAlloced == TRUE) &&
			(gsDispQ_Buf[i].ui32BufSize == ui32BufSize) )
		{
			gsDispQ_Buf[i].bUsed = TRUE;

			return gsDispQ_Buf[i].ui32BufAddr;
		}
	}

	for( i = 0; i < DISP_Q_NUM_OF_CHANNEL; i++ )
	{
		if( gsDispQ_Buf[i].bAlloced == FALSE )
			break;
	}
	if( i == DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q][Err] Num of DispQ Buf Pool, Regest Buf Size: 0x%08X", ui32BufSize );
		for( i = 0; i < DISP_Q_NUM_OF_CHANNEL; i++ )
			VDEC_KDRV_Message(ERROR, "[DISP_Q][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsDispQ_Buf[i].bUsed, gsDispQ_Buf[i].bAlloced, gsDispQ_Buf[i].ui32BufAddr, gsDispQ_Buf[i].ui32BufSize );
		return 0x0;
	}

	ui32BufAddr = VDEC_SHM_Malloc( ui32BufSize);
	if( ui32BufAddr == 0x0 )
		return 0x0;

	gsDispQ_Buf[i].bUsed = TRUE;
	gsDispQ_Buf[i].bAlloced = TRUE;
	gsDispQ_Buf[i].ui32BufAddr = ui32BufAddr;
	gsDispQ_Buf[i].ui32BufSize = ui32BufSize;

	VDEC_KDRV_Message(DBG_DEIF, "[DISP_Q][DBG] Addr: 0x%X, Size: 0x%X", ui32BufAddr, ui32BufSize);

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
static BOOLEAN _DISP_Q_Buf_Free(UINT32 ui32BufAddr, UINT32 ui32BufSize)
{
	UINT32		i;

	VDEC_KDRV_Message(_TRACE, "[DISP_Q][DBG] %s", __FUNCTION__ );

	for( i = 0; i < DISP_Q_NUM_OF_CHANNEL; i++ )
	{
		if( (gsDispQ_Buf[i].bUsed == TRUE) &&
			(gsDispQ_Buf[i].bAlloced == TRUE) &&
			(gsDispQ_Buf[i].ui32BufAddr == ui32BufAddr) &&
			(gsDispQ_Buf[i].ui32BufSize == ui32BufSize) )
		{
			gsDispQ_Buf[i].bUsed = FALSE;
			return TRUE;
		}
	}

	VDEC_KDRV_Message(ERROR, "[DISP_Q][Err] Not Matched DispQ Buf Pool - 0x%X/0x%X %s", ui32BufAddr, ui32BufSize, __FUNCTION__ );

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
void DISP_Q_Reset(UINT8 ch)
{
	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q][Err] Channel Number(%d), %s", ch, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[DISP_Q%d][DBG] %s", ch, __FUNCTION__ );

	gsVdecDispQ[ch].ui32WrOffset = 0;
	gsVdecDispQ[ch].ui32RdOffset = 0;
	IPC_REG_DISPQ_SetWrOffset(ch, 0);
	IPC_REG_DISPQ_SetRdOffset(ch, 0);
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
static UINT32 _DISP_Q_SetDb(UINT8 ch, UINT32 ui32DispqBase, UINT32 ui32DispqSize)
{
	UINT32	ui32NumOfNode = 0;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] %s", ch, __FUNCTION__ );
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[DISP_Q%d][DBG] Base: 0x%X, Size: 0x%X, %s(%d)", ch, ui32DispqBase, ui32DispqSize, __FUNCTION__, __LINE__ );

	gsVdecDispQ[ch].ui32PhyBaseAddr = ui32DispqBase;
	gsVdecDispQ[ch].ui32QueueSize = ui32DispqSize;

	if( ui32DispqBase && ui32DispqSize )
	{
		UINT32	ui32DispQNodeSize;

		ui32DispQNodeSize = sizeof(S_DISPQ_BUF_T);

		ui32NumOfNode = ui32DispqSize / ui32DispQNodeSize;

		gsVdecDispQ[ch].ui32NodeSize = ui32DispQNodeSize;
		gsVdecDispQ[ch].pui32VirBasePtr = VDEC_TranselateVirualAddr(ui32DispqBase, ui32DispqSize);
		VDEC_KDRV_Message(_TRACE, "[DISP_Q%d][DBG] Node - Num:0x%X, Size:0x%X ", ch, ui32NumOfNode, ui32DispQNodeSize );
	}
	else
	{
		gsVdecDispQ[ch].ui32NodeSize = 0;
		VDEC_ClearVirualAddr((void *)gsVdecDispQ[ch].pui32VirBasePtr );
		gsVdecDispQ[ch].pui32VirBasePtr = 0x0;
	}

#if defined(DISP_Q_USE_IPC_CMD)
#if !defined(__XTENSA__)
	if( ui32DispqBase != 0 ) // open
	{
		IPC_REG_DISPQ_SetWrOffset(ch, 0);
		IPC_REG_DISPQ_SetRdOffset(ch, 0);
	}
#endif
#else
	IPC_REG_DISPQ_SetWrOffset(ch, 0);
	IPC_REG_DISPQ_SetRdOffset(ch, 0);
#endif
	gsVdecDispQ[ch].ui32WrOffset = 0;
	gsVdecDispQ[ch].ui32RdOffset = 0;
	gsVdecDispQ[ch].ui32LogTick = 0;

#if (defined(DISP_Q_USE_IPC_CMD) && !defined(__XTENSA__))
	_IPC_CMD_Send_DisplayQ_Info(ch, ui32DispqBase, ui32DispqSize);
#endif

	return ui32NumOfNode;
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
BOOLEAN DISP_Q_Open(UINT8 ch, UINT32 ui32NumOfDq)
{
	UINT32 	ui32DispQBufAddr;
	UINT32	ui32DispQBufSize;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] %s", ch, __FUNCTION__ );
		return 0;
	}

	ui32DispQBufSize = sizeof(S_DISPQ_BUF_T) * ui32NumOfDq;
	ui32DispQBufAddr = _DISP_Q_Buf_Alloc(ui32DispQBufSize);
	if( ui32DispQBufAddr == 0x0 )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Failed to Alloc Display Queue Memory, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[DISP_Q%d][DBG] Base: 0x%X, Size: 0x%X, %s(%d)", ch, ui32DispQBufAddr, ui32DispQBufSize, __FUNCTION__, __LINE__ );

	_DISP_Q_SetDb(ch, ui32DispQBufAddr, ui32DispQBufSize);

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
void DISP_Q_Close(UINT8 ch)
{
	VDEC_KDRV_Message(_TRACE, "[DISP_Q%d][DBG] %s(%d)", ch, __FUNCTION__, __LINE__ );

	_DISP_Q_Buf_Free(gsVdecDispQ[ch].ui32PhyBaseAddr, gsVdecDispQ[ch].ui32QueueSize);

	_DISP_Q_SetDb(ch, 0x0, 0x0);
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
static void _DISP_Q_UpdateWrOffset(UINT8 ch, UINT32 ui32WrOffset)
{
	IPC_REG_DISPQ_SetWrOffset(ch, ui32WrOffset);

	// for Debug
	gsVdecDispQ[ch].ui32LogTick = 0;
	gsVdecDispQ[ch].ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	gsVdecDispQ[ch].ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);
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
BOOLEAN DISP_Q_Push(UINT8 ch, S_DISPQ_BUF_T *psDisBuf)
{
	UINT32		ui32WrOffset, ui32WrOffset_Next;
	UINT32		ui32RdOffset;
	UINT32 		*pui32WrVirPtr;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( !gsVdecDispQ[ch].ui32PhyBaseAddr )
	{
		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Warning] Not Initialised, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	ui32WrOffset_Next = VDEC_DISPQ_NEXT_START_OFFSET(	ui32WrOffset,
														gsVdecDispQ[ch].ui32NodeSize,
														gsVdecDispQ[ch].ui32QueueSize);
	if( ui32WrOffset_Next == ui32RdOffset )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Overflow - WrOffset: %d, RdOffset: %d, %s", ch, ui32WrOffset, ui32RdOffset, __FUNCTION__ );
		return FALSE;
	}

	pui32WrVirPtr = gsVdecDispQ[ch].pui32VirBasePtr + (ui32WrOffset>>2);
	memcpy(pui32WrVirPtr, (void *)psDisBuf, sizeof(S_DISPQ_BUF_T));
//	*(S_DISPQ_BUF_T *)pui32WrVirPtr = *psDisBuf;

	_DISP_Q_UpdateWrOffset(ch, ui32WrOffset_Next);

	DISP_Q_ValidateFrameAddr(ch);	// to validate

	return TRUE;
}


#if ( (defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDS) && !defined(__XTENSA__)) )
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
static void _DISP_Q_UpdateRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	IPC_REG_DISPQ_SetRdOffset(ch, ui32RdOffset);

	// for Debug
	gsVdecDispQ[ch].ui32LogTick = 0;
	gsVdecDispQ[ch].ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	gsVdecDispQ[ch].ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);
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
BOOLEAN DISP_Q_PeepLast(UINT8 ch, S_DISPQ_BUF_T *psDisBuf)
{
	UINT32 		ui32WrOffset;
	UINT32 		ui32RdOffset, ui32RdOffset_Next;
	UINT32 		*pui32RdVirPtr;


	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( DISP_Q_NumActive( ch ) ==  0 )
		return FALSE;

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	ui32RdOffset_Next = VDEC_DISPQ_NEXT_START_OFFSET(	ui32RdOffset,
														gsVdecDispQ[ch].ui32NodeSize,
														gsVdecDispQ[ch].ui32QueueSize);

//	if( ui32RdOffset_Next == ui32WrOffset )
//	{
//		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Warning] Underrun", ch );
//	}

	pui32RdVirPtr = gsVdecDispQ[ch].pui32VirBasePtr + (ui32RdOffset>>2);
	memcpy((void *)psDisBuf, pui32RdVirPtr, sizeof(S_DISPQ_BUF_T));
//	*psDisBuf = *(S_DISPQ_BUF_T *)pui32RdVirPtr;
//	VDEC_KDRV_Message(ERROR, "[DISP_Q%d] PTS: %d", ch, psDisBuf->ui32PTS );

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
BOOLEAN DISP_Q_Pop(UINT8 ch, S_DISPQ_BUF_T *psDisBuf)
{
	UINT32 		ui32WrOffset;
	UINT32 		ui32RdOffset, ui32RdOffset_Next;
	UINT32 		*pui32RdVirPtr;


	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( DISP_Q_NumActive( ch ) ==  0 )
		return FALSE;

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	ui32RdOffset_Next = VDEC_DISPQ_NEXT_START_OFFSET(	ui32RdOffset,
														gsVdecDispQ[ch].ui32NodeSize,
														gsVdecDispQ[ch].ui32QueueSize);

//	if( ui32RdOffset_Next == ui32WrOffset )
//	{
//		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Warning] Underrun", ch );
//	}

	pui32RdVirPtr = gsVdecDispQ[ch].pui32VirBasePtr + (ui32RdOffset>>2);
	memcpy((void *)psDisBuf, pui32RdVirPtr, sizeof(S_DISPQ_BUF_T));
//	*psDisBuf = *(S_DISPQ_BUF_T *)pui32RdVirPtr;
//	VDEC_KDRV_Message(ERROR, "[DISP_Q%d] PTS: %d", ch, psDisBuf->ui32PTS );

	_DISP_Q_UpdateRdOffset(ch, ui32RdOffset_Next);

	DISP_Q_ValidateFrameAddr(ch);	// to validate

	return TRUE;
}
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
UINT32 DISP_Q_NumActive(UINT8 ch)
{
	UINT32	ui32UsedBuf;
	UINT32	ui32WrOffset;
	UINT32	ui32RdOffset;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return 0;
	}

	if( !gsVdecDispQ[ch].ui32PhyBaseAddr )
	{
		if( (gsVdecDispQ[ch].ui32LogTick % 10) == 0 )
		{
			VDEC_KDRV_Message(WARNING, "[DISPQ%d][Wrn] Not Initialised, %s", ch, __FUNCTION__ );
		}
		gsVdecDispQ[ch].ui32LogTick++;
		return 0;
	}

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	if( ui32WrOffset >= ui32RdOffset )
		ui32UsedBuf = ui32WrOffset - ui32RdOffset;
	else
		ui32UsedBuf = ui32WrOffset + gsVdecDispQ[ch].ui32QueueSize - ui32RdOffset;

	return (ui32UsedBuf / gsVdecDispQ[ch].ui32NodeSize);
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
UINT32 DISP_Q_NumFree(UINT8 ch)
{
	UINT32	ui32FreeBuf;
	UINT32	ui32WrOffset;
	UINT32	ui32RdOffset;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return 0;
	}

	if( !gsVdecDispQ[ch].ui32PhyBaseAddr )
	{
		VDEC_KDRV_Message(WARNING, "[DISPQ%d][Wrn] Not Initialised, %s", ch, __FUNCTION__ );
		return 0;
	}

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	if( ui32WrOffset < ui32RdOffset )
		ui32FreeBuf = ui32RdOffset - ui32WrOffset;
	else
		ui32FreeBuf = ui32RdOffset + gsVdecDispQ[ch].ui32QueueSize - ui32WrOffset;

	return ((ui32FreeBuf / gsVdecDispQ[ch].ui32NodeSize) - 1);
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
UINT32 DISP_Q_ValidateFrameAddr(UINT8 ch)
{
	UINT32 		ui32WrOffset;
	UINT32 		ui32RdOffset;
	UINT32 		*pui32RdVirPtr;
	UINT32		ui32UsedBuf;
	UINT32		ui32Occupancy;

	S_DISPQ_BUF_T *psDisBuf;
	UINT32		ui32FrameAddr[0x80];
	UINT32		ui32FrameAddrCount = 0;
	UINT32		ui32FrameAddrIndex;
	BOOLEAN		bRet = TRUE;

	if( ch >= DISP_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return 0;
	}

	if( !gsVdecDispQ[ch].ui32PhyBaseAddr )
	{
//		VDEC_KDRV_Message(ERROR, "[DISPQ%d][Warning] Not Initialised, %s", ch, __FUNCTION__ );
		return 0;
	}

	ui32WrOffset = IPC_REG_DISPQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_DISPQ_GetRdOffset(ch);

	if( ui32WrOffset >= ui32RdOffset )
		ui32UsedBuf = ui32WrOffset - ui32RdOffset;
	else
		ui32UsedBuf = ui32WrOffset + gsVdecDispQ[ch].ui32QueueSize - ui32RdOffset;

	ui32Occupancy = (ui32UsedBuf / gsVdecDispQ[ch].ui32NodeSize);

	while( ui32WrOffset != ui32RdOffset )
	{
		pui32RdVirPtr = gsVdecDispQ[ch].pui32VirBasePtr + (ui32RdOffset>>2);
		psDisBuf = (S_DISPQ_BUF_T *)pui32RdVirPtr;

		for( ui32FrameAddrIndex = 0; ui32FrameAddrIndex < ui32FrameAddrCount; ui32FrameAddrIndex++ )
		{
			if( psDisBuf->ui32YFrameAddr == ui32FrameAddr[ui32FrameAddrIndex] )
			{
				VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Frame Addr: 0x%X, Occupancy: %d, %s", ch, psDisBuf->ui32YFrameAddr, ui32Occupancy, __FUNCTION__ );
				bRet = FALSE;
			}
		}
		if( ui32FrameAddrIndex == ui32FrameAddrCount )
			ui32FrameAddr[ui32FrameAddrCount] = psDisBuf->ui32YFrameAddr;

		if( ui32FrameAddrCount >= (0x80 - 1) )
			VDEC_KDRV_Message(ERROR, "[DISP_Q%d][Err] Frame Count: %d, Occupancy: %d, %s", ch, ui32FrameAddrCount, ui32Occupancy, __FUNCTION__ );
		else
			ui32FrameAddrCount++;

		ui32RdOffset = VDEC_DISPQ_NEXT_START_OFFSET(	ui32RdOffset,
														gsVdecDispQ[ch].ui32NodeSize,
														gsVdecDispQ[ch].ui32QueueSize);
	}

	return bRet;
}


