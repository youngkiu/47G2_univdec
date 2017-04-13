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
 * date       2011.03.09
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
#include "ves_cpb.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#include <asm/uaccess.h> // copy_from_user
#include <asm/cacheflush.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#endif

#include "../hal/lg1152/lg1152_vdec_base.h"
#include "../hal/lg1152/pdec_hal_api.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/ipc_reg_api.h"
#include "../mcu/vdec_shm.h"
#include "../mcu/os_adap.h"
#include "../mcu/ipc_cmd.h"
#include "ves_drv.h"

#include "../mcu/vdec_print.h"


/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		VES_NUM_OF_CPB_POOL			VES_NUM_OF_CHANNEL

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define		VES_CPB_ALIGNED_OFFSET( _offset, _cpbsize )	\
					((VES_CPB_CEILING_512BYTES(_offset) >= (_cpbsize)) ? 0 : VES_CPB_CEILING_512BYTES(_offset))
#define		VES_CPB_ALIGNED_ADDR( _addr, _cpbbase, _cpbsize )	\
					((VES_CPB_CEILING_512BYTES(_addr) >= ((_cpbbase) + (_cpbsize))) ? (_cpbbase) : VES_CPB_CEILING_512BYTES(_addr))

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
#ifdef CPB_USE_IPC_CMD
typedef struct
{
	UINT8	ui8VesCh;
	UINT32	ui32CpbBufAddr;
	UINT32	ui32CpbBufSize;
	BOOLEAN	bRingBufferMode;
	BOOLEAN	b512bytesAligned;
	BOOLEAN	bIsHwPath;
	BOOLEAN	bFromTVP;
} S_IPC_CPB_INFO_T;
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
static void _VES_CPB_SetDb(UINT8 ui8VesCh, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned, BOOLEAN bIsHwPath, BOOLEAN bFromTVP);

/*----------------------------------------------------------------------------------------
 *   Static Variables
 *---------------------------------------------------------------------------------------*/
static struct
{
	UINT32 		ui32PhyBaseAddr;	// constant
	UINT8 		*pui8VirBasePtr;		// constant
	UINT32 		ui32BufSize;			// constant
	BOOLEAN		bRingBufferMode;
	BOOLEAN		b512bytesAligned;
	BOOLEAN		bIsHwPath;
	BOOLEAN		bFromTVP;

	// for Debug
	UINT32 		ui32WrOffset;		// variable
	UINT32 		ui32RdOffset;		// variable
} gsVesCpb[VES_NUM_OF_CHANNEL];

static void (*_gfpVES_CpbStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus);


#ifdef CPB_USE_IPC_CMD
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
static void _IPC_CMD_Receive_CPB_Info(void *pIpcBody)
{
	S_IPC_CPB_INFO_T *pCPB_Info;

	VDEC_KDRV_Message(DBG_VES, "[CPB][DBG] %s", __FUNCTION__ );

	pCPB_Info = (S_IPC_CPB_INFO_T *)pIpcBody;

	_VES_CPB_SetDb(pCPB_Info->ui8VesCh, pCPB_Info->ui32CpbBufAddr, pCPB_Info->ui32CpbBufSize, pCPB_Info->bRingBufferMode, pCPB_Info->b512bytesAligned, pCPB_Info->bIsHwPath, pCPB_Info->bFromTVP);
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
static void _IPC_CMD_Send_CPB_Info(UINT8 ui8VesCh, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned, BOOLEAN bIsHwPath, BOOLEAN bFromTVP)
{
	S_IPC_CPB_INFO_T sCPB_Info;

	VDEC_KDRV_Message(DBG_VES, "[CPB][DBG] %s", __FUNCTION__ );

	sCPB_Info.ui8VesCh = ui8VesCh;
	sCPB_Info.ui32CpbBufAddr = ui32CpbBufAddr;
	sCPB_Info.ui32CpbBufSize = ui32CpbBufSize;
	sCPB_Info.bRingBufferMode = bRingBufferMode;
	sCPB_Info.b512bytesAligned = b512bytesAligned;
	sCPB_Info.bIsHwPath = bIsHwPath;
	sCPB_Info.bFromTVP = bFromTVP;
	IPC_CMD_Send(IPC_CMD_ID_CPB_REGISTER, sizeof(S_IPC_CPB_INFO_T) , (void *)&sCPB_Info);
}
#endif // #ifdef __XTENSA__
#endif // #ifdef CPB_USE_IPC_CMD

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
void VES_CPB_Init( void (*_fpVES_CpbStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus) )
{
	UINT32		i;

	for( i = 0; i < VES_NUM_OF_CHANNEL; i++ )
	{
		gsVesCpb[i].ui32PhyBaseAddr = 0x0;
		gsVesCpb[i].pui8VirBasePtr = NULL;
		gsVesCpb[i].ui32BufSize = 0x0;
		gsVesCpb[i].bIsHwPath = TRUE;
		gsVesCpb[i].bFromTVP = FALSE;

		gsVesCpb[i].ui32WrOffset = 0x0;
		gsVesCpb[i].ui32RdOffset = 0x0;
	}

	_gfpVES_CpbStatus = _fpVES_CpbStatus;

#if (defined(CPB_USE_IPC_CMD) && defined(__XTENSA__))
	IPC_CMD_Register_ReceiverCallback(IPC_CMD_ID_CPB_REGISTER, (IPC_CMD_RECEIVER_CALLBACK_T)_IPC_CMD_Receive_CPB_Info);
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
static BOOLEAN _VES_CPB_IsWithinRange(UINT32 ui32StartAddr, UINT32 ui32EndAddr, UINT32 ui32TargetAddr)
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
BOOLEAN VES_CPB_UpdateWrPtr(UINT8 ui8VesCh, UINT32 ui32WrPhyAddr)
{
	UINT32		ui32RdOffset;
	UINT32		ui32WrOffset, ui32WrOffset_Next;
	BOOLEAN		bRet = TRUE;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}

	ui32WrOffset_Next = ui32WrPhyAddr - gsVesCpb[ui8VesCh].ui32PhyBaseAddr;

	if( ui32WrOffset_Next > gsVesCpb[ui8VesCh].ui32BufSize )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Invalid Write Addr:0x%X", ui8VesCh, ui32WrPhyAddr );
		return FALSE;
	}
	if( ui32WrOffset_Next == gsVesCpb[ui8VesCh].ui32BufSize )
	{
		ui32WrOffset_Next = 0;
	}

	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);
	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);

	if( _VES_CPB_IsWithinRange(ui32WrOffset, ui32WrOffset_Next, ui32RdOffset) == TRUE )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Overwrite - Write:0x%X, Write_Next:0x%X, Read:0x%X", ui8VesCh, ui32WrOffset, ui32WrOffset_Next, ui32RdOffset );
		ui32WrOffset_Next = ui32RdOffset;
		ui32WrPhyAddr = gsVesCpb[ui8VesCh].ui32PhyBaseAddr + ui32WrOffset_Next;
		bRet = FALSE;
	}

	IPC_REG_CPB_SetWrOffset(ui8VesCh, ui32WrOffset_Next);

	// for Debug
	gsVesCpb[ui8VesCh].ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	gsVesCpb[ui8VesCh].ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);

	return bRet;
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
BOOLEAN VES_CPB_UpdateRdPtr(UINT8 ui8VesCh, UINT32 ui32RdPhyAddr)
{
	UINT32		ui32RdOffset, ui32RdOffset_Next;
	UINT32		ui32WrOffset;
	BOOLEAN		bRet = TRUE;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}

	ui32RdOffset_Next = ui32RdPhyAddr - gsVesCpb[ui8VesCh].ui32PhyBaseAddr;

	if( ui32RdOffset_Next > gsVesCpb[ui8VesCh].ui32BufSize )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Invalid Read Addr:0x%X, Base Addr:0x%X", ui8VesCh, ui32RdPhyAddr, gsVesCpb[ui8VesCh].ui32PhyBaseAddr );
		return FALSE;
	}
	if( ui32RdOffset_Next == gsVesCpb[ui8VesCh].ui32BufSize )
	{
		ui32RdOffset_Next = 0;
	}

	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);
	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);

	if( _VES_CPB_IsWithinRange(ui32RdOffset, ui32RdOffset_Next, ui32WrOffset) == TRUE )
	{
		if( ui32RdOffset_Next != ui32WrOffset )
		{
			VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Overread - Read:0x%X, Read_Next:0x%X, Write:0x%X", ui8VesCh, ui32RdOffset, ui32RdOffset_Next, ui32WrOffset );
			ui32RdOffset_Next = ui32RdOffset;
			ui32RdPhyAddr = gsVesCpb[ui8VesCh].ui32PhyBaseAddr + ui32RdOffset_Next;
			bRet = FALSE;
		}
	}

	IPC_REG_CPB_SetRdOffset(ui8VesCh, ui32RdOffset_Next);

	// for Debug
	gsVesCpb[ui8VesCh].ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	gsVesCpb[ui8VesCh].ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);

	if( gsVesCpb[ui8VesCh].bIsHwPath == TRUE )
	{
		PDEC_HAL_CPB_SetRdPtr(ui8VesCh, ui32RdPhyAddr);
	}

	return bRet;
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
void VES_CPB_Reset(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

#if !defined(CPB_USE_IPC_INTERFACE) || !defined(__XTENSA__)
	IPC_REG_CPB_SetWrOffset(ui8VesCh, 0);
	IPC_REG_CPB_SetRdOffset(ui8VesCh, 0);
#endif

	// for Debug
	gsVesCpb[ui8VesCh].ui32WrOffset = 0;
	gsVesCpb[ui8VesCh].ui32RdOffset = 0;

	if( gsVesCpb[ui8VesCh].bIsHwPath == TRUE )
	{
		PDEC_HAL_CPB_Init(ui8VesCh, gsVesCpb[ui8VesCh].ui32PhyBaseAddr, gsVesCpb[ui8VesCh].ui32BufSize);
	}

	if( gsVesCpb[ui8VesCh].pui8VirBasePtr != NULL )
	{
		memset(gsVesCpb[ui8VesCh].pui8VirBasePtr, 0x00, gsVesCpb[ui8VesCh].ui32BufSize);
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
void VES_CPB_Flush(UINT8 ui8VesCh)
{
	UINT32		ui32WrOffset;
	UINT32		ui32RdOffset;
	UINT8		*pui8CpbRdVirPtr;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] WrOffset:%d, RdOffset:%d, BufSize:%d, %s", ui8VesCh, ui32WrOffset, ui32RdOffset, gsVesCpb[ui8VesCh].ui32BufSize, __FUNCTION__ );

	if( gsVesCpb[ui8VesCh].pui8VirBasePtr != NULL )
	{ // for Ring Buffer
		pui8CpbRdVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr + ui32RdOffset;
		if( ui32WrOffset >= ui32RdOffset )
		{
			memset(pui8CpbRdVirPtr, 0x00, (ui32WrOffset - ui32RdOffset));
		}
		else
		{
			memset(pui8CpbRdVirPtr, 0x00, (gsVesCpb[ui8VesCh].ui32BufSize - ui32RdOffset));
			if( ui32WrOffset )
			{
				pui8CpbRdVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr;
				memset(pui8CpbRdVirPtr, 0x00, ui32WrOffset);
			}
		}
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
static void _VES_CPB_SetDb(UINT8 ui8VesCh, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned, BOOLEAN bIsHwPath, BOOLEAN bFromTVP)
{
	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] CPB Base: 0x%X, Size: 0x%X, %s(%d)", ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize, __FUNCTION__, __LINE__ );

	if( ui32CpbBufAddr && ui32CpbBufSize )
	{
		if( bFromTVP == FALSE )
			gsVesCpb[ui8VesCh].pui8VirBasePtr = (UINT8 *)VDEC_TranselateVirualAddr(ui32CpbBufAddr, ui32CpbBufSize);
		else
			gsVesCpb[ui8VesCh].pui8VirBasePtr = NULL;
	}
	else
	{
		if( bFromTVP == FALSE )
			VDEC_ClearVirualAddr((void *)gsVesCpb[ui8VesCh].pui8VirBasePtr );

		gsVesCpb[ui8VesCh].pui8VirBasePtr = NULL;
	}

	gsVesCpb[ui8VesCh].ui32PhyBaseAddr = ui32CpbBufAddr;
	gsVesCpb[ui8VesCh].ui32BufSize = ui32CpbBufSize;

	gsVesCpb[ui8VesCh].bRingBufferMode = bRingBufferMode;
	gsVesCpb[ui8VesCh].b512bytesAligned = b512bytesAligned;

	gsVesCpb[ui8VesCh].bIsHwPath = bIsHwPath;
	gsVesCpb[ui8VesCh].bFromTVP = bFromTVP;

#if ( !defined(CPB_USE_IPC_CMD) || \
	(defined(CPB_USE_IPC_CMD) && !defined(__XTENSA__)) )
	gsVesCpb[ui8VesCh].ui32WrOffset = 0;
	gsVesCpb[ui8VesCh].ui32RdOffset = 0;
	IPC_REG_CPB_SetWrOffset(ui8VesCh, 0);
	IPC_REG_CPB_SetRdOffset(ui8VesCh, 0);
#else
	gsVesCpb[ui8VesCh].ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	gsVesCpb[ui8VesCh].ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);
#endif

	if( (bIsHwPath == TRUE) && (bFromTVP == FALSE) )
		PDEC_HAL_CPB_SetCpbVirBase(ui8VesCh, gsVesCpb[ui8VesCh].pui8VirBasePtr);

#if (defined(CPB_USE_IPC_CMD) && !defined(__XTENSA__))
	_IPC_CMD_Send_CPB_Info(ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize, bRingBufferMode, b512bytesAligned, bIsHwPath, bFromTVP);
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
BOOLEAN VES_CPB_Open(UINT8 ui8VesCh, UINT32 ui32CpbBufAddr, UINT32 ui32CpbBufSize, BOOLEAN bRingBufferMode, BOOLEAN b512bytesAligned, BOOLEAN bIsHwPath, BOOLEAN bFromTVP)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[VES%d][DBG] %s", ui8VesCh, __FUNCTION__ );

	VDEC_KDRV_Message(DBG_SYS, "[VES%d][DBG][CPB] Addr: 0x%X, Size: 0x%X", ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize );

	_VES_CPB_SetDb(ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize, bRingBufferMode, b512bytesAligned, bIsHwPath, bFromTVP);

	if( bIsHwPath == TRUE )
	{
		if( bFromTVP == TRUE )
		{
			PDEC_HAL_Disable(ui8VesCh);
			PDEC_HAL_CPB_Init(ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize);
		}
		else
		{
			PDEC_HAL_CPB_Init(ui8VesCh, ui32CpbBufAddr, ui32CpbBufSize);

			PDEC_HAL_CPB_SetBufALevel(ui8VesCh, 80, 5);

			switch( ui8VesCh )
			{
			case 0 :
				TOP_HAL_EnableBufIntr(PDEC0_CPB_ALMOST_FULL);
				TOP_HAL_EnableBufIntr(PDEC0_CPB_ALMOST_EMPTY);
				break;
			case 1 :
				TOP_HAL_EnableBufIntr(PDEC1_CPB_ALMOST_FULL);
				TOP_HAL_EnableBufIntr(PDEC1_CPB_ALMOST_EMPTY);
				break;
			case 2 :
				TOP_HAL_EnableBufIntr(PDEC2_CPB_ALMOST_FULL);
				TOP_HAL_EnableBufIntr(PDEC2_CPB_ALMOST_EMPTY);
				break;
			default :
				VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
			}
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
void VES_CPB_Close(UINT8 ui8VesCh)
{
	if( gsVesCpb[ui8VesCh].bIsHwPath == TRUE )
	{
		PDEC_HAL_CPB_Init(ui8VesCh, 0x0, 0x0);

		switch( ui8VesCh )
		{
		case 0 :
			TOP_HAL_ClearBufIntr(PDEC0_CPB_ALMOST_FULL);
			TOP_HAL_DisableBufIntr(PDEC0_CPB_ALMOST_FULL);
			TOP_HAL_ClearBufIntr(PDEC0_CPB_ALMOST_EMPTY);
			TOP_HAL_DisableBufIntr(PDEC0_CPB_ALMOST_EMPTY);
			break;
		case 1 :
			TOP_HAL_ClearBufIntr(PDEC1_CPB_ALMOST_FULL);
			TOP_HAL_DisableBufIntr(PDEC1_CPB_ALMOST_FULL);
			TOP_HAL_ClearBufIntr(PDEC1_CPB_ALMOST_EMPTY);
			TOP_HAL_DisableBufIntr(PDEC1_CPB_ALMOST_EMPTY);
			break;
		case 2 :
			TOP_HAL_ClearBufIntr(PDEC2_CPB_ALMOST_FULL);
			TOP_HAL_DisableBufIntr(PDEC2_CPB_ALMOST_FULL);
			TOP_HAL_ClearBufIntr(PDEC2_CPB_ALMOST_EMPTY);
			TOP_HAL_DisableBufIntr(PDEC2_CPB_ALMOST_EMPTY);
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
		}
	}

	_VES_CPB_SetDb(ui8VesCh, 0x0, 0x0, TRUE, TRUE, FALSE, FALSE);
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
UINT32 VES_CPB_GetPhyWrPtr(UINT8 ui8VesCh)
{
	UINT32		ui32WrPhyAddr;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0x0;
	}

	if( !gsVesCpb[ui8VesCh].ui32PhyBaseAddr )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Not Initialised, %s", ui8VesCh, __FUNCTION__ );
		return 0x0;
	}

	if( gsVesCpb[ui8VesCh].bIsHwPath == TRUE )
		ui32WrPhyAddr = PDEC_HAL_CPB_GetWrPtr(ui8VesCh);
	else
		ui32WrPhyAddr = gsVesCpb[ui8VesCh].ui32PhyBaseAddr + IPC_REG_CPB_GetWrOffset(ui8VesCh);

	return ui32WrPhyAddr;
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
UINT8 *VES_CPB_TranslatePhyWrPtr(UINT8 ui8VesCh, UINT32 ui32WrPhyAddr)
{
	UINT32		ui32WrOffset;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0x0;
	}

	if( (ui32WrPhyAddr < gsVesCpb[ui8VesCh].ui32PhyBaseAddr) ||
		(ui32WrPhyAddr >= (gsVesCpb[ui8VesCh].ui32PhyBaseAddr + gsVesCpb[ui8VesCh].ui32BufSize)) )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] 0x%X <= CPB Wr Addr(0x%X) < 0x%X, %s", ui8VesCh, gsVesCpb[ui8VesCh].ui32PhyBaseAddr, ui32WrPhyAddr, gsVesCpb[ui8VesCh].ui32PhyBaseAddr + gsVesCpb[ui8VesCh].ui32BufSize, __FUNCTION__ );
		return 0x0;
	}

	if( gsVesCpb[ui8VesCh].pui8VirBasePtr == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Not Initialised, %s", ui8VesCh, __FUNCTION__ );
		return 0x0;
	}

	ui32WrOffset = ui32WrPhyAddr - gsVesCpb[ui8VesCh].ui32PhyBaseAddr;

	return gsVesCpb[ui8VesCh].pui8VirBasePtr + ui32WrOffset;
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
static BOOLEAN _VES_CPB_CheckOverflow(UINT8 ui8VesCh, UINT32 ui32AuSize_Modified, BOOLEAN bRingBufferMode, UINT32 *pui32WrOffset_AuStart, UINT32 *pui32WrOffset_AlignedEnd, UINT32 ui32RdOffset)
{
	UINT32 ui32WrOffset_Org = *pui32WrOffset_AuStart ;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return TRUE;
	}

	if( (*pui32WrOffset_AuStart + ui32AuSize_Modified) >= gsVesCpb[ui8VesCh].ui32BufSize )
	{
		if( bRingBufferMode == TRUE )
		{
			*pui32WrOffset_AlignedEnd = (*pui32WrOffset_AuStart + ui32AuSize_Modified) - gsVesCpb[ui8VesCh].ui32BufSize;
		}
		else
		{
			if( (*pui32WrOffset_AuStart + ui32AuSize_Modified) >= gsVesCpb[ui8VesCh].ui32BufSize )
			{
				*pui32WrOffset_AuStart = 0;
			}
			*pui32WrOffset_AlignedEnd = *pui32WrOffset_AuStart + ui32AuSize_Modified;
		}
	}
	else
	{
		*pui32WrOffset_AlignedEnd = *pui32WrOffset_AuStart + ui32AuSize_Modified;
	}

	if( _VES_CPB_IsWithinRange(ui32WrOffset_Org, *pui32WrOffset_AlignedEnd, ui32RdOffset) == TRUE )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Overflow - Write:0x%X, Size:0x%X, Read:0x%X", ui8VesCh, ui32WrOffset_Org, ui32AuSize_Modified, ui32RdOffset );
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
UINT32 VES_CPB_Write(	UINT8 ui8VesCh,
							UINT32 ui32AuStartAddr,
							UINT32 ui32AuSize,
							fpCpbCopyfunc fpCopyfunc,
							UINT32 *pui32CpbWrPhyAddr_End)
{
/*
	ui32WrOffset,
				ui32WrOffset_AuStart + ui32AuSize = ui32WrOffset_AuEnd,
																ui32WrOffset_AlignedEnd
*/

	UINT32		ui32WrOffset, ui32WrOffset_AuStart, ui32WrOffset_AuEnd, ui32WrOffset_AlignedEnd;
	UINT32		ui32RdOffset;
	UINT32		ui32CpbWrPhyAddr = 0x0;
	UINT8		*pui8CpbWrVirPtr;
	char			*pui8AuStartAddr = (char*)ui32AuStartAddr;
	UINT32		ui32AuSize_Modified;


	if( ui32AuSize > gsVesCpb[ui8VesCh].ui32BufSize )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] AU Size - SRC:0x%X ++ 0x%X - %s(%d)", ui8VesCh, ui32AuStartAddr, ui32AuSize, __FUNCTION__, __LINE__);
		return 0x0;
	}

	if( gsVesCpb[ui8VesCh].b512bytesAligned == TRUE )
	{
		ui32AuSize_Modified = VES_CPB_CEILING_512BYTES(ui32AuSize);
		if( ui32AuSize_Modified == VES_CPB_CEILING_512BYTES(ui32AuSize) )
			ui32AuSize_Modified += 1024;
		else
			ui32AuSize_Modified += 512;
	}
	else
	{
		ui32AuSize_Modified = ui32AuSize;
	}

	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);
	ui32WrOffset_AuStart = ui32WrOffset;

	// 1. Check CPB Overflow
	if( _VES_CPB_CheckOverflow(ui8VesCh, ui32AuSize_Modified, gsVesCpb[ui8VesCh].bRingBufferMode, &ui32WrOffset_AuStart, &ui32WrOffset_AlignedEnd, ui32RdOffset) == TRUE )
		return 0x0;

	// pad buffer end
	if( ui32WrOffset > ui32WrOffset_AuStart )
	{
		UINT32		ui32PadSize;

		VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Pad CPB End, WrOffset:0x%08X/0x%08X/0x%08X, RdOffset:0x%08X", ui8VesCh, ui32WrOffset, ui32WrOffset_AuStart, ui32WrOffset_AlignedEnd, ui32RdOffset);

		if( ui32WrOffset_AuStart != 0 )
		{
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] WrOffset:0x%08X/0x%08X/0x%08X, RdOffset:0x%08X", ui8VesCh, ui32WrOffset, ui32WrOffset_AuStart, ui32WrOffset_AlignedEnd, ui32RdOffset);
			return 0x0;
		}

		pui8CpbWrVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr + ui32WrOffset;
		ui32PadSize = gsVesCpb[ui8VesCh].ui32BufSize - ui32WrOffset;

		memset(pui8CpbWrVirPtr, 0x00, ui32PadSize);
	}

	ui32CpbWrPhyAddr = gsVesCpb[ui8VesCh].ui32PhyBaseAddr + ui32WrOffset_AuStart;
	pui8CpbWrVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr + ui32WrOffset_AuStart;

	// 2. Write User Data to CPB
	if( (ui32WrOffset_AuStart + ui32AuSize) > gsVesCpb[ui8VesCh].ui32BufSize )
	{ // Wrap Around
		UINT32		ui32AuSize_Front;
		UINT32		ui32AuSize_Back;

		VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Cpb Wrap Around, WrOffset:0x%08X/0x%08X/0x%08X, RdOffset:0x%08X", ui8VesCh, ui32WrOffset, ui32WrOffset_AuStart, ui32WrOffset_AlignedEnd, ui32RdOffset);

		ui32AuSize_Front = gsVesCpb[ui8VesCh].ui32BufSize - ui32WrOffset_AuStart;

		if( fpCopyfunc(pui8CpbWrVirPtr, pui8AuStartAddr, ui32AuSize_Front) )
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] copy_from_user - SRC:0x%X ++ 0x%X --> DST:0x%X/0x%X - %s(%d)", ui8VesCh, (UINT32)pui8AuStartAddr, ui32AuSize_Front, ui32CpbWrPhyAddr, (UINT32)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

		VDEC_KDRV_Message(DBG_VES, "[VES%d] UV:0x%08X, KP:0x%08X, Size:0x%X, Data:0x%08X - %s(%d)", ui8VesCh, (UINT32)pui8AuStartAddr, ui32CpbWrPhyAddr, ui32AuSize, *(UINT32 *)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

//		if( *(UINT32 *)pui8CpbWrVirPtr != 0x01000000 )
//			VDEC_KDRV_Message(ERROR, "[VES%d] Addr:0x%X, Size:0x%X, Data:0x%08X - %s(%d)", ui8VesCh, ui32CpbWrPhyAddr, ui32AuSize, *(UINT32 *)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

		pui8CpbWrVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr;
		pui8AuStartAddr += ui32AuSize_Front;

		ui32AuSize_Back = (ui32WrOffset_AuStart + ui32AuSize) - gsVesCpb[ui8VesCh].ui32BufSize;

		if( fpCopyfunc(pui8CpbWrVirPtr, pui8AuStartAddr, ui32AuSize_Back) )
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] copy_from_user - SRC:0x%X ++ 0x%X --> DST:0x%X/0x%X - %s(%d)", ui8VesCh, (UINT32)pui8AuStartAddr, ui32AuSize_Back, ui32CpbWrPhyAddr, (UINT32)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

		pui8CpbWrVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr + ui32AuSize_Back;
		ui32WrOffset_AuEnd = ui32AuSize_Back;
	}
	else
	{
#if 0 // def __XTENSA__
		UINT32		ui32GSTC_Start, ui32GSTC_End;
		ui32GSTC_Start = TOP_HAL_GetGSTCC();
#endif
		if( fpCopyfunc(pui8CpbWrVirPtr, pui8AuStartAddr, ui32AuSize) )
			VDEC_KDRV_Message(ERROR, "[VES%d][Err] copy_from_user - SRC:0x%X ++ 0x%X --> DST:0x%X/0x%X - %s(%d)", ui8VesCh, (UINT32)pui8AuStartAddr, ui32AuSize, ui32CpbWrPhyAddr, (UINT32)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );
#if 0 // def __XTENSA__
		ui32GSTC_End = TOP_HAL_GetGSTCC();
		VDEC_KDRV_Message(ERROR, "[VES%d] Size:0x%08X, Start_T:0x%08X, Start_T:0x%08X", ui8VesCh, ui32AuSize, ui32GSTC_Start, ui32GSTC_End );
#endif

//		VDEC_KDRV_Message(DBG_VES, "[VES%d] UV:0x%08X, KP:0x%08X, Size:0x%X, Data:0x%08X - %s(%d)", ui8VesCh, (UINT32)pui8AuStartAddr, ui32CpbWrPhyAddr, ui32AuSize, *(UINT32 *)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

//		if( *(UINT32 *)pui8CpbWrVirPtr != 0x01000000 )
//			VDEC_KDRV_Message(ERROR, "[VES%d] Addr:0x%X, Size:0x%X, Data:0x%08X - %s(%d)", ui8VesCh, ui32CpbWrPhyAddr, ui32AuSize, *(UINT32 *)pui8CpbWrVirPtr, __FUNCTION__, __LINE__ );

		pui8CpbWrVirPtr += ui32AuSize;
		ui32WrOffset_AuEnd = ui32WrOffset_AuStart + ui32AuSize;
	}

	// pad au end
	if( ui32WrOffset_AuEnd > ui32WrOffset_AlignedEnd )
	{
		memset(pui8CpbWrVirPtr, 0x00, gsVesCpb[ui8VesCh].ui32BufSize - ui32WrOffset_AuEnd);

		if( ui32WrOffset_AlignedEnd > 0 )
		{
			pui8CpbWrVirPtr = gsVesCpb[ui8VesCh].pui8VirBasePtr;
			memset(pui8CpbWrVirPtr, 0x00, ui32WrOffset_AlignedEnd);
		}
	}
	else if( ui32WrOffset_AuEnd < ui32WrOffset_AlignedEnd )
	{
		memset(pui8CpbWrVirPtr, 0x00, ui32WrOffset_AlignedEnd - ui32WrOffset_AuEnd);
	}

	*pui32CpbWrPhyAddr_End = gsVesCpb[ui8VesCh].ui32PhyBaseAddr + ui32WrOffset_AlignedEnd;

	return ui32CpbWrPhyAddr;
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
UINT32 VES_CPB_GetUsedBuffer(UINT8 ui8VesCh)
{
	UINT32		ui32UseBytes;
	UINT32		ui32WrOffset;
	UINT32		ui32RdOffset;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);

	if( ui32WrOffset >= ui32RdOffset )
		ui32UseBytes = ui32WrOffset - ui32RdOffset;
	else
		ui32UseBytes = ui32WrOffset + gsVesCpb[ui8VesCh].ui32BufSize - ui32RdOffset;

//	if( gsVesCpb[ui8VesCh].bIsHwPath == TRUE )
//		VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG][CPB] Used Size SW:0x%X, HW:0x%X", ui8VesCh, ui32UseBytes, PDEC_HAL_CPB_GetBufferLevel(ui8VesCh) );
//	else
//		VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG][CPB] Used Size SW:0x%X", ui8VesCh, ui32UseBytes );

	return ui32UseBytes;
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
UINT32 VES_CPB_GetBufferBaseAddr(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

//	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] buf base addr : 0x%X %s", ui8VesCh, gsVesCpb[ui8VesCh].ui32PhyBaseAddr, __FUNCTION__ );

	return gsVesCpb[ui8VesCh].ui32PhyBaseAddr;
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
UINT32 VES_CPB_GetBufferSize(UINT8 ui8VesCh)
{
	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return 0;
	}

//	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] buf size : 0x%X %s", ui8VesCh, gsVesCpb[ui8VesCh].ui32BufSize, __FUNCTION__ );

	return gsVesCpb[ui8VesCh].ui32BufSize;
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
void VES_CPB_PrintUsedBuffer(UINT8 ui8VesCh)
{
	UINT32		ui32UseBytes;
	UINT32		ui32WrOffset;
	UINT32		ui32RdOffset;

	if( ui8VesCh >= VES_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	ui32WrOffset = IPC_REG_CPB_GetWrOffset(ui8VesCh);
	ui32RdOffset = IPC_REG_CPB_GetRdOffset(ui8VesCh);

	if( ui32WrOffset >= ui32RdOffset )
		ui32UseBytes = ui32WrOffset - ui32RdOffset;
	else
		ui32UseBytes = ui32WrOffset + gsVesCpb[ui8VesCh].ui32BufSize - ui32RdOffset;

	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG][CPB] Use: 0x%08X, Free: 0x%08X ", ui8VesCh, ui32UseBytes, gsVesCpb[ui8VesCh].ui32BufSize - ui32UseBytes );
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
void VDEC_ISR_CPB_AlmostFull(UINT8 ui8VesCh)
{
	S_VES_CALLBACK_BODY_CPBSTATUS_T sReportCpbStatus;

	if( ui8VesCh >= PDEC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	switch( ui8VesCh)
	{
	case 0 :
		TOP_HAL_ClearBufIntr(PDEC0_CPB_ALMOST_FULL);
		break;
	case 1 :
		TOP_HAL_ClearBufIntr(PDEC1_CPB_ALMOST_FULL);
		break;
	case 2 :
		TOP_HAL_ClearBufIntr(PDEC2_CPB_ALMOST_FULL);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
	}

	sReportCpbStatus.ui8Vch = VES_GetVdecCh(ui8VesCh);
	sReportCpbStatus.ui8VesCh = ui8VesCh;
	sReportCpbStatus.eBufStatus = CPB_STATUS_ALMOST_FULL;
	_gfpVES_CpbStatus( &sReportCpbStatus );

	VDEC_KDRV_Message(ERROR, "[VES%d][Err][CPB] Almost Full - Buffer Level: %d ", ui8VesCh, PDEC_HAL_CPB_GetBufferLevel(ui8VesCh) );
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
void VDEC_ISR_CPB_AlmostEmpty(UINT8 ui8VesCh)
{
	if( ui8VesCh >= PDEC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] Channel Number, %s", ui8VesCh, __FUNCTION__ );
		return;
	}

	switch( ui8VesCh )
	{
	case 0 :
		TOP_HAL_ClearBufIntr(PDEC0_CPB_ALMOST_EMPTY);
		break;
	case 1 :
		TOP_HAL_ClearBufIntr(PDEC1_CPB_ALMOST_EMPTY);
		break;
	case 2 :
		TOP_HAL_ClearBufIntr(PDEC2_CPB_ALMOST_EMPTY);
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[VES%d][Err] %s(%d)", ui8VesCh, __FUNCTION__, __LINE__ );
	}

	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG][CPB] Almost Empty - Buffer Level: %d ", ui8VesCh, PDEC_HAL_CPB_GetBufferLevel(ui8VesCh) );
	VDEC_KDRV_Message(DBG_VES, "[VES%d][DBG] Buffer Status: 0x%X ", ui8VesCh, PDEC_HAL_GetBufferStatus(ui8VesCh) );
}
#endif

