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
 * date       2012.06.09
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
#include "vdc_auq.h"

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


#if defined(USE_MCU_FOR_TOP) || defined(USE_MCU_FOR_VDEC_VES) || defined(USE_MCU_FOR_VDEC_VDC)
	#if !(defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VES) && defined(USE_MCU_FOR_VDEC_VDC))
		#define AU_Q_USE_IPC_CMD
	#endif
#endif


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define		VDEC_AUQ_NEXT_START_OFFSET( _offset, _nodesize, _queuesize )	\
				((((_offset) + (_nodesize)) >= (_queuesize)) ? 0 : (_offset) + (_nodesize))
#define		VDEC_AUQ_NEXT_START_ADDR( _nodeaddr, _nodesize, _queuebase, _queuesize )	\
				((((_nodeaddr) + (_nodesize)) >= ((_queuebase) + (_queuesize))) ? (_queuebase) : (_nodeaddr) + (_nodesize))


/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
#ifdef AU_Q_USE_IPC_CMD
typedef struct
{
	UINT32	ui32ch;
	UINT32	ui32AuqBase;
	UINT32	ui32AuqSize;
	UINT32	ui32CpbBufAddr;
	UINT32	ui32CpbBufSize;
} S_IPC_AUQ_INFO_T;
#endif


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
	BOOLEAN		bUsed;
	BOOLEAN		bAlloced;
	UINT32		ui32BufAddr;
	UINT32		ui32BufSize;
} gsAuQ_Buf[AU_Q_NUM_OF_CHANNEL];

static struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT32 		*pui32VirBasePtr;	// constant
	UINT32 		ui32QueueSize;		// constant
	UINT32		ui32NodeSize;

	UINT32		ui32CpbBufSize;

	// for Debug
	UINT32		ui32LogTick;
	UINT32 		ui32WrOffset;		// offset
	UINT32 		ui32RdOffset;		// offset

	UINT32		ui32AuStartAddr_Push_Prev;
	UINT32		ui32AuEndAddr_Push_Prev;
	UINT32		ui32AuStartAddr_Pop_Prev;
	UINT32		ui32AuEndAddr_Pop_Prev;
}gsVdecAuQ[AU_Q_NUM_OF_CHANNEL];


#ifdef AU_Q_USE_IPC_CMD
#ifdef __XTENSA__
static UINT32 _AU_Q_SetDb(UINT8 ch, UINT32 ui32AuqBase, UINT32 ui32AuqSize, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize);
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
static void _IPC_CMD_Receive_AuQ_Info(void *pIpcBody)
{
	S_IPC_AUQ_INFO_T *pAuQ_Info;

	VDEC_KDRV_Message(DBG_VES, "[AU_Q][DBG] %s", __FUNCTION__ );

	pAuQ_Info = (S_IPC_AUQ_INFO_T *)pIpcBody;

	_AU_Q_SetDb(pAuQ_Info->ui32ch, pAuQ_Info->ui32AuqBase, pAuQ_Info->ui32AuqSize, pAuQ_Info->ui32CpbBufAddr, pAuQ_Info->ui32CpbBufSize);
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
static void _IPC_CMD_Send_AuQ_Info(UINT8 ch, UINT32 ui32AuqBase, UINT32 ui32AuqSize, UINT32 ui32CpbBufAddr)
{
	S_IPC_AUQ_INFO_T sAuQ_Info;

	VDEC_KDRV_Message(DBG_VES, "[AU_Q][DBG] %s", __FUNCTION__ );

	sAuQ_Info.ui32ch = ch;
	sAuQ_Info.ui32AuqBase = ui32AuqBase;
	sAuQ_Info.ui32AuqSize = ui32AuqSize;
	sAuQ_Info.ui32CpbBufAddr = ui32CpbBufAddr;
	sAuQ_Info.ui32CpbBufAddr = ui32CpbBufAddr;
	IPC_CMD_Send(IPC_CMD_ID_AU_Q_REGISTER, sizeof(S_IPC_AUQ_INFO_T) , (void *)&sAuQ_Info);
}
#endif // #ifdef __XTENSA__
#endif // #ifdef AU_Q_USE_IPC_CMD

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
void AU_Q_Init(void)
{
	UINT8	ch;

	VDEC_KDRV_Message(_TRACE, "[AU_Q][DBG] %s", __FUNCTION__ );

	for( ch = 0; ch < AU_Q_NUM_OF_CHANNEL; ch++ )
	{
		gsAuQ_Buf[ch].bUsed = FALSE;
#if ( !defined(AU_Q_USE_IPC_CMD) || (defined(AU_Q_USE_IPC_CMD) && !defined(__XTENSA__)) )
		gsAuQ_Buf[ch].bAlloced = FALSE;
#endif
	}

	for( ch = 0; ch < AU_Q_NUM_OF_CHANNEL; ch++ )
	{
		gsVdecAuQ[ch].ui32PhyBaseAddr = 0x0;
		gsVdecAuQ[ch].pui32VirBasePtr = 0x0;
		gsVdecAuQ[ch].ui32QueueSize = 0x0;
		gsVdecAuQ[ch].ui32NodeSize = 0x0;

		gsVdecAuQ[ch].ui32LogTick = 0;
		gsVdecAuQ[ch].ui32WrOffset = 0;
		gsVdecAuQ[ch].ui32RdOffset = 0;
#if ( !defined(AU_Q_USE_IPC_CMD) || (defined(AU_Q_USE_IPC_CMD) && !defined(__XTENSA__)) )
		IPC_REG_AUQ_SetWrOffset(ch, 0);
		IPC_REG_AUQ_SetRdOffset(ch, 0);
		IPC_REG_CPB_SetWrAddr(ch, 0);
#endif
	}

#if (defined(AU_Q_USE_IPC_CMD)&& defined(__XTENSA__))
	IPC_CMD_Register_ReceiverCallback(IPC_CMD_ID_AU_Q_REGISTER, (IPC_CMD_RECEIVER_CALLBACK_T)_IPC_CMD_Receive_AuQ_Info);
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
static UINT32 _AU_Q_Buf_Alloc(UINT32 ui32BufSize)
{
	UINT32		i;
	UINT32		ui32BufAddr;

	VDEC_KDRV_Message(_TRACE, "[AU_Q][DBG] %s", __FUNCTION__ );

	for( i = 0; i < AU_Q_NUM_OF_CHANNEL; i++ )
	{
		if( (gsAuQ_Buf[i].bUsed == FALSE) &&
			(gsAuQ_Buf[i].bAlloced == TRUE) &&
			(gsAuQ_Buf[i].ui32BufSize == ui32BufSize) )
		{
			gsAuQ_Buf[i].bUsed = TRUE;

			return gsAuQ_Buf[i].ui32BufAddr;
		}
	}

	for( i = 0; i < AU_Q_NUM_OF_CHANNEL; i++ )
	{
		if( gsAuQ_Buf[i].bAlloced == FALSE )
			break;
	}
	if( i == AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q][Err] Num of AuQ Buf Pool, Regest Buf Size: 0x%08X", ui32BufSize );
		for( i = 0; i < AU_Q_NUM_OF_CHANNEL; i++ )
			VDEC_KDRV_Message(ERROR, "[AU_Q][Err] Used:%d, Alloced:%d, BufAddr:0x%08X, BufSize:0x%08X", gsAuQ_Buf[i].bUsed, gsAuQ_Buf[i].bAlloced, gsAuQ_Buf[i].ui32BufAddr, gsAuQ_Buf[i].ui32BufSize );
		return 0x0;
	}

	ui32BufAddr = VDEC_SHM_Malloc( ui32BufSize);
	if( ui32BufAddr == 0x0 )
		return 0x0;

	gsAuQ_Buf[i].bUsed = TRUE;
	gsAuQ_Buf[i].bAlloced = TRUE;
	gsAuQ_Buf[i].ui32BufAddr = ui32BufAddr;
	gsAuQ_Buf[i].ui32BufSize = ui32BufSize;

	VDEC_KDRV_Message(DBG_VES, "[AU_Q][DBG] Addr: 0x%X, Size: 0x%X", ui32BufAddr, ui32BufSize);

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
static BOOLEAN _AU_Q_Buf_Free(UINT32 ui32BufAddr, UINT32 ui32BufSize)
{
	UINT32		i;

	VDEC_KDRV_Message(_TRACE, "[AU_Q][DBG] %s", __FUNCTION__ );

	for( i = 0; i < AU_Q_NUM_OF_CHANNEL; i++ )
	{
		if( (gsAuQ_Buf[i].bUsed == TRUE) &&
			(gsAuQ_Buf[i].bAlloced == TRUE) &&
			(gsAuQ_Buf[i].ui32BufAddr == ui32BufAddr) &&
			(gsAuQ_Buf[i].ui32BufSize == ui32BufSize) )
		{
			gsAuQ_Buf[i].bUsed = FALSE;
			return TRUE;
		}
	}

	VDEC_KDRV_Message(ERROR, "[AU_Q][Err] Not Matched AuQ Buf Pool - 0x%X/0x%X %s", ui32BufAddr, ui32BufSize, __FUNCTION__ );

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
void AU_Q_Reset(UINT8 ch, UINT32 ui32CpbBufAddr)
{
	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q][Err] Channel Number(%d), %s", ch, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[AU_Q%d][DBG] %s", ch, __FUNCTION__ );

	gsVdecAuQ[ch].ui32WrOffset = 0;
	gsVdecAuQ[ch].ui32RdOffset = 0;
	IPC_REG_AUQ_SetWrOffset(ch, 0);
	IPC_REG_AUQ_SetRdOffset(ch, 0);
	IPC_REG_CPB_SetWrAddr(ch, ui32CpbBufAddr);

	gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev = 0;
	gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev = 0;
	gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev = 0;
	gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev = 0;
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
static UINT32 _AU_Q_SetDb(UINT8 ch, UINT32 ui32AuqBase, UINT32 ui32AuqSize, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize)
{
	UINT32	ui32NumOfNode = 0;

	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] %s", ch, __FUNCTION__ );
		return 0;
	}

	VDEC_KDRV_Message(_TRACE, "[AU_Q%d][DBG] Base: 0x%X, Size: 0x%X, %s(%d)", ch, ui32AuqBase, ui32AuqSize, __FUNCTION__, __LINE__ );

	gsVdecAuQ[ch].ui32PhyBaseAddr = ui32AuqBase;
	gsVdecAuQ[ch].ui32QueueSize = ui32AuqSize;

	if( ui32AuqBase && ui32AuqSize )
	{
		UINT32	ui32AuQNodeSize;

		ui32AuQNodeSize = sizeof(S_VDC_AU_T);

		ui32NumOfNode = ui32AuqSize / ui32AuQNodeSize;

		gsVdecAuQ[ch].ui32NodeSize = ui32AuQNodeSize;
		gsVdecAuQ[ch].pui32VirBasePtr = VDEC_TranselateVirualAddr(ui32AuqBase, ui32AuqSize);
		VDEC_KDRV_Message(_TRACE, "[AU_Q%d][DBG] Node - Num:0x%X, Size:0x%X ", ch, ui32NumOfNode, ui32AuQNodeSize );
	}
	else
	{
		gsVdecAuQ[ch].ui32NodeSize = 0;
		VDEC_ClearVirualAddr((void *)gsVdecAuQ[ch].pui32VirBasePtr );
		gsVdecAuQ[ch].pui32VirBasePtr = 0x0;
	}

#if defined(AU_Q_USE_IPC_CMD)
#if !defined(__XTENSA__)
	if( ui32AuqBase != 0 ) // open
	{
		IPC_REG_AUQ_SetWrOffset(ch, 0);
		IPC_REG_AUQ_SetRdOffset(ch, 0);
		IPC_REG_CPB_SetWrAddr(ch, ui32CpbBufAddr);
	}
#endif
#else
	IPC_REG_AUQ_SetWrOffset(ch, 0);
	IPC_REG_AUQ_SetRdOffset(ch, 0);
	IPC_REG_CPB_SetWrAddr(ch, ui32CpbBufAddr);
#endif
	gsVdecAuQ[ch].ui32CpbBufSize = ui32CpbBufSize;

	gsVdecAuQ[ch].ui32WrOffset = 0;
	gsVdecAuQ[ch].ui32RdOffset = 0;
	gsVdecAuQ[ch].ui32LogTick = 0;

	gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev = 0;
	gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev = 0;
	gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev = 0;
	gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev = 0;

#if (defined(AU_Q_USE_IPC_CMD) && !defined(__XTENSA__))
	_IPC_CMD_Send_AuQ_Info(ch, ui32AuqBase, ui32AuqSize, ui32CpbBufAddr);
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
BOOLEAN AU_Q_Open(UINT8 ch, UINT32 ui32NumOfAuQ, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize)
{
	UINT32 	ui32AuQBufAddr;
	UINT32	ui32AuQBufSize;

	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] %s", ch, __FUNCTION__ );
		return 0;
	}

	ui32AuQBufSize = sizeof(S_VDC_AU_T) * ui32NumOfAuQ;
	ui32AuQBufAddr = _AU_Q_Buf_Alloc(ui32AuQBufSize);
	if( ui32AuQBufAddr == 0x0 )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Failed to Alloc AU Queue Memory, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[AU_Q%d][DBG] Base: 0x%X, Size: 0x%X, %s(%d)", ch, ui32AuQBufAddr, ui32AuQBufSize, __FUNCTION__, __LINE__ );

	_AU_Q_SetDb(ch, ui32AuQBufAddr, ui32AuQBufSize, ui32CpbBufAddr, ui32CpbBufSize);

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
void AU_Q_Close(UINT8 ch)
{
	VDEC_KDRV_Message(_TRACE, "[AU_Q%d][DBG] %s(%d)", ch, __FUNCTION__, __LINE__ );

	_AU_Q_Buf_Free(gsVdecAuQ[ch].ui32PhyBaseAddr, gsVdecAuQ[ch].ui32QueueSize);

	_AU_Q_SetDb(ch, 0x0, 0x0, 0x0, 0x0);
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
static void _AU_Q_UpdateWrOffset(UINT8 ch, UINT32 ui32WrOffset, UINT32 ui32WrAddr)
{
	IPC_REG_AUQ_SetWrOffset(ch, ui32WrOffset);
	IPC_REG_CPB_SetWrAddr(ch, ui32WrAddr);

	// for Debug
	gsVdecAuQ[ch].ui32LogTick = 0;
	gsVdecAuQ[ch].ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	gsVdecAuQ[ch].ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);
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
BOOLEAN AU_Q_Push(UINT8 ch, S_VDC_AU_T *psAuQNode)
{
	UINT32		ui32WrOffset, ui32WrOffset_Next;
	UINT32		ui32RdOffset;

	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( !gsVdecAuQ[ch].ui32PhyBaseAddr )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Warning] Not Initialised, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);

	ui32WrOffset_Next = VDEC_AUQ_NEXT_START_OFFSET(	ui32WrOffset,
														gsVdecAuQ[ch].ui32NodeSize,
														gsVdecAuQ[ch].ui32QueueSize);
	if( ui32WrOffset_Next == ui32RdOffset )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Overflow - WrOffset: %d, RdOffset: %d, %s", ch, ui32WrOffset, ui32RdOffset, __FUNCTION__ );
		return FALSE;
	}

	*(S_VDC_AU_T *)(gsVdecAuQ[ch].pui32VirBasePtr + (ui32WrOffset>>2)) = *psAuQNode;

	_AU_Q_UpdateWrOffset(ch, ui32WrOffset_Next, psAuQNode->ui32AuEndAddr);

	if( gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev )
	{
		if( gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev > psAuQNode->ui32AuStartAddr )
			if( (gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev - psAuQNode->ui32AuStartAddr) < (gsVdecAuQ[ch].ui32CpbBufSize * 8 / 10) )
				VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Start - Prev:0x%X, Curr:0x%X + 0x%X = 0x%X, %s(%d)", ch, gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev, psAuQNode->ui32AuStartAddr, psAuQNode->ui32AuSize, psAuQNode->ui32AuEndAddr, __FUNCTION__, __LINE__ );
	}
	if( gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev )
	{
		if( gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev > psAuQNode->ui32AuEndAddr )
			if( (gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev - psAuQNode->ui32AuEndAddr) < (gsVdecAuQ[ch].ui32CpbBufSize * 8 / 10) )
				VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] End - Prev:0x%X, Curr:0x%X + 0x%X = 0x%X, %s(%d)", ch, gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev, psAuQNode->ui32AuStartAddr, psAuQNode->ui32AuSize, psAuQNode->ui32AuEndAddr, __FUNCTION__, __LINE__ );
	}
	gsVdecAuQ[ch].ui32AuStartAddr_Push_Prev = psAuQNode->ui32AuStartAddr;
	gsVdecAuQ[ch].ui32AuEndAddr_Push_Prev = psAuQNode->ui32AuEndAddr;

	return TRUE;
}

#if ( (defined(USE_MCU_FOR_VDEC_VDC) && defined(__XTENSA__)) || \
	(!defined(USE_MCU_FOR_VDEC_VDC) && !defined(__XTENSA__)) )
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
static void _AU_Q_UpdateRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	IPC_REG_AUQ_SetRdOffset(ch, ui32RdOffset);

	// for Debug
	gsVdecAuQ[ch].ui32LogTick = 0;
	gsVdecAuQ[ch].ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	gsVdecAuQ[ch].ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);
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
BOOLEAN AU_Q_Pop(UINT8 ch, S_VDC_AU_T *psAuQNode)
{
	UINT32 		ui32WrOffset;
	UINT32 		ui32RdOffset, ui32RdOffset_Next;
	UINT32 		*pui32RdVirPtr;


	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( AU_Q_NumActive( ch ) ==  0 )
		return FALSE;

	ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);

	ui32RdOffset_Next = VDEC_AUQ_NEXT_START_OFFSET(	ui32RdOffset,
														gsVdecAuQ[ch].ui32NodeSize,
														gsVdecAuQ[ch].ui32QueueSize);

//	if( ui32RdOffset_Next == ui32WrOffset )
//	{
//		VDEC_KDRV_Message(ERROR, "[AUQ%d][Warning] Underrun", ch );
//	}

	pui32RdVirPtr = gsVdecAuQ[ch].pui32VirBasePtr + (ui32RdOffset>>2);
//	memcpy((void *)psAuQNode, pui32RdVirPtr, gsVdecAuQ[ch].ui32NodeSize);
	*psAuQNode = *(S_VDC_AU_T *)pui32RdVirPtr;
//	VDEC_KDRV_Message(ERROR, "[AU_Q%d] PTS: %d", ch, psAuQNode->ui32PTS );

	_AU_Q_UpdateRdOffset(ch, ui32RdOffset_Next);

	if( gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev )
	{
		if( gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev > psAuQNode->ui32AuStartAddr )
			if( (gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev - psAuQNode->ui32AuStartAddr) < (gsVdecAuQ[ch].ui32CpbBufSize * 8 / 10) )
				VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Start - Prev:0x%X, Curr:0x%X + 0x%X = 0x%X, %s(%d)", ch, gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev, psAuQNode->ui32AuStartAddr, psAuQNode->ui32AuSize, psAuQNode->ui32AuEndAddr, __FUNCTION__, __LINE__ );
	}
	if( gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev )
	{
		if( gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev > psAuQNode->ui32AuEndAddr )
			if( (gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev - psAuQNode->ui32AuEndAddr) < (gsVdecAuQ[ch].ui32CpbBufSize * 8 / 10) )
				VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] End - Prev:0x%X, Curr:0x%X + 0x%X = 0x%X, %s(%d)", ch, gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev, psAuQNode->ui32AuStartAddr, psAuQNode->ui32AuSize, psAuQNode->ui32AuEndAddr, __FUNCTION__, __LINE__ );
	}
	gsVdecAuQ[ch].ui32AuStartAddr_Pop_Prev = psAuQNode->ui32AuStartAddr;
	gsVdecAuQ[ch].ui32AuEndAddr_Pop_Prev = psAuQNode->ui32AuEndAddr;

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
BOOLEAN AU_Q_Peep(UINT8 ch, S_VDC_AU_T *psAuQNode)
{
	UINT32 		ui32WrOffset;
	UINT32 		ui32RdOffset, ui32RdOffset_Next;
	UINT32 		*pui32RdVirPtr;


	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	if( AU_Q_NumActive( ch ) ==  0 )
		return FALSE;

	ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);

	ui32RdOffset_Next = VDEC_AUQ_NEXT_START_OFFSET(	ui32RdOffset,
														gsVdecAuQ[ch].ui32NodeSize,
														gsVdecAuQ[ch].ui32QueueSize);

//	if( ui32RdOffset_Next == ui32WrOffset )
//	{
//		VDEC_KDRV_Message(ERROR, "[AUQ%d][Warning] Underrun", ch );
//	}

	pui32RdVirPtr = gsVdecAuQ[ch].pui32VirBasePtr + (ui32RdOffset>>2);
//	memcpy((void *)psAuQNode, pui32RdVirPtr, gsVdecAuQ[ch].ui32NodeSize);
	*psAuQNode = *(S_VDC_AU_T *)pui32RdVirPtr;
//	VDEC_KDRV_Message(ERROR, "[AU_Q%d] PTS: %d", ch, psAuQNode->ui32PTS );

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
UINT32 AU_Q_GetWrAddr(UINT8 ch)
{
	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return FALSE;
	}

	return IPC_REG_CPB_GetWrAddr(ch);
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
UINT32 AU_Q_NumActive(UINT8 ch)
{
	UINT32	ui32UsedBuf;
	UINT32	ui32WrOffset;
	UINT32	ui32RdOffset;

	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return 0;
	}

	if( !gsVdecAuQ[ch].ui32PhyBaseAddr )
	{
		if( (gsVdecAuQ[ch].ui32LogTick % 10) == 0 )
		{
			VDEC_KDRV_Message(ERROR, "[AU_Q%d][Warning] Not Initialised, %s", ch, __FUNCTION__ );
		}
		gsVdecAuQ[ch].ui32LogTick++;
		return 0;
	}

	ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);

	if( ui32WrOffset >= ui32RdOffset )
		ui32UsedBuf = ui32WrOffset - ui32RdOffset;
	else
		ui32UsedBuf = ui32WrOffset + gsVdecAuQ[ch].ui32QueueSize - ui32RdOffset;

	return (ui32UsedBuf / gsVdecAuQ[ch].ui32NodeSize);
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
UINT32 AU_Q_NumFree(UINT8 ch)
{
	UINT32	ui32FreeBuf;
	UINT32	ui32WrOffset;
	UINT32	ui32RdOffset;

	if( ch >= AU_Q_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Err] Channel Number, %s", ch, __FUNCTION__ );
		return 0;
	}

	if( !gsVdecAuQ[ch].ui32PhyBaseAddr )
	{
		VDEC_KDRV_Message(ERROR, "[AU_Q%d][Warning] Not Initialised, %s", ch, __FUNCTION__ );
		return 0;
	}

	ui32WrOffset = IPC_REG_AUQ_GetWrOffset(ch);
	ui32RdOffset = IPC_REG_AUQ_GetRdOffset(ch);

	if( ui32WrOffset < ui32RdOffset )
		ui32FreeBuf = ui32RdOffset - ui32WrOffset;
	else
		ui32FreeBuf = ui32RdOffset + gsVdecAuQ[ch].ui32QueueSize - ui32WrOffset;

	return ((ui32FreeBuf / gsVdecAuQ[ch].ui32NodeSize) - 1);
}


