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
 * @addtogroup lg115x_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "../mcu/mcu_config.h"
#include "vdec_rate.h"
#include "vsync_drv.h"
#include "pts_drv.h"

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#endif

#include "../mcu/os_adap.h"

#include "../hal/lg1152/ipc_reg_api.h"
#include "../mcu/vdec_shm.h"

#include "../mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define	VDEC_RATE_NUM_OF_CHANNEL				DE_IF_NUM_OF_CHANNEL
#define	VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF		0x80
#define	VDEC_RATE_MAX_DIFF_HISTORY_NUM			(0xFFFFFFFF / VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF)

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define	VDEC_DIFF(_a, _b)		(((_a) >= (_b)) ? (_a) - (_b) : (_b) - (_a))

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT32		ui32_Prev;
	UINT32		ui32Diff_History[VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF];
	UINT32		ui32Diff_History_Index;
	UINT32		ui32Diff_History_Sum;
	UINT32		ui32Diff_History_Count;
	UINT32		ui32Diff_Event_Count;
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;
} S_RATE_MOVING_AVERAGE_T;

typedef struct
{
	SINT32		i32Speed;		// 1x: 1000, 2x: 2000, ...
	BOOLEAN		bFieldPicture;
	BOOLEAN		bInterlaced;
	BOOLEAN		bVariableFrameRate;
	BOOLEAN		bChooseParserFrameRate;
	UINT8		ui8CodecType;
	struct
	{
		UINT32	ui32FrameRateRes_Org;
		UINT32	ui32FrameRateDiv_Org;
		UINT32	ui32FrameDuration90K_Org;
		UINT32	ui32FrameRateRes;
		UINT32	ui32FrameRateDiv;
		UINT32	ui32FrameDuration90K;
	} Parser;
	struct
	{
		UINT32	ui32FrameRateRes_Org;
		UINT32	ui32FrameRateDiv_Org;
		UINT32	ui32FrameDuration90K_Org;
		UINT32	ui32FrameRateRes;
		UINT32	ui32FrameRateDiv;
		UINT32	ui32FrameDuration90K;
	} Decoder;
	S_RATE_MOVING_AVERAGE_T Input;
	S_RATE_MOVING_AVERAGE_T Feed;
	S_RATE_MOVING_AVERAGE_T DecDone;
	S_RATE_MOVING_AVERAGE_T DTS;
	S_RATE_MOVING_AVERAGE_T PTS;
	S_RATE_MOVING_AVERAGE_T PTS_NoErr;
	S_RATE_MOVING_AVERAGE_T Dropped;
	S_RATE_MOVING_AVERAGE_T Displayed;
	struct
	{
		UINT32	ui32FrameRateRes;
		UINT32	ui32FrameRateDiv;
		UINT32	ui32FrameDuration90K;	// frame duration @ 90KHz clock tick. valid only after sequence header found.
	} Status;
	struct
	{
		UINT32		ui32LogTick_NoFrameRate;
		UINT32		ui32LogTick_InvalidTsRate;
	} Debug;
} S_VDEC_RATE_DB_T;


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
volatile S_VDEC_RATE_DB_T *gsVdecRate = NULL;


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
void VDEC_RATE_Init(void)
{
	UINT32	gsVdecRatePhy, gsVdecRateSize;

#if !defined(USE_MCU_FOR_TOP) && defined(USE_MCU_FOR_VDEC_VDS) && defined(__XTENSA__)
	gsVdecRateSize = sizeof(S_VDEC_RATE_DB_T) * VDEC_RATE_NUM_OF_CHANNEL;
	gsVdecRatePhy = IPC_REG_RATE_GetDb();

	VDEC_KDRV_Message(_TRACE, "[RATE][DBG] gsVdecRatePhy: 0x%X, %s", gsVdecRatePhy, __FUNCTION__ );
#else
	gsVdecRateSize = sizeof(S_VDEC_RATE_DB_T) * VDEC_RATE_NUM_OF_CHANNEL;
	gsVdecRatePhy = VDEC_SHM_Malloc(gsVdecRateSize);
	IPC_REG_RATE_SetDb(gsVdecRatePhy);

	VDEC_KDRV_Message(_TRACE, "[RATE][DBG] gsVdecRatePhy: 0x%X, %s", gsVdecRatePhy, __FUNCTION__ );
#endif

	gsVdecRate = (S_VDEC_RATE_DB_T *)VDEC_TranselateVirualAddr(gsVdecRatePhy, gsVdecRateSize);
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
static void _VDEC_RATE_Reset_MOVING_AVERAGE(S_RATE_MOVING_AVERAGE_T *pMovingAverage)
{
	pMovingAverage->ui32_Prev = 0x80000000;
	pMovingAverage->ui32Diff_History_Index = 0;
	pMovingAverage->ui32Diff_History_Sum = 0;
	pMovingAverage->ui32Diff_History_Count = 0xFFFFFFFF;
	pMovingAverage->ui32Diff_Event_Count = 0xFFFFFFFF;
	pMovingAverage->ui32FrameRateRes = 0;
	pMovingAverage->ui32FrameRateDiv = 0xFFFFFFFF;
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
void VDEC_RATE_Reset(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[RATE%d][DBG] %s", ui8SyncCh, __FUNCTION__ );

	gsVdecRate[ui8SyncCh].i32Speed = 1000;
	gsVdecRate[ui8SyncCh].bFieldPicture = TRUE;
	gsVdecRate[ui8SyncCh].bInterlaced = TRUE;
	gsVdecRate[ui8SyncCh].bVariableFrameRate = FALSE;
	gsVdecRate[ui8SyncCh].bChooseParserFrameRate = FALSE;
	gsVdecRate[ui8SyncCh].ui8CodecType = 0xFF;

	gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes_Org = 0x0;
	gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv_Org = 0xFFFFFFFF;
	gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K_Org = 0;
	gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes = 0x0;
	gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv = 0xFFFFFFFF;
	gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K = 0;

	gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes_Org = 0x0;
	gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv_Org = 0xFFFFFFFF;
	gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K_Org = 0;
	gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes = 0x0;
	gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv = 0xFFFFFFFF;
	gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K = 0;

	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Input);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Feed);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DecDone);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DTS);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS_NoErr);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Dropped);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Displayed);

	gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes = 0x0;
	gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv = 0xFFFFFFFF;
	gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K = 0;

	gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate = 0x0;
	gsVdecRate[ui8SyncCh].Debug.ui32LogTick_InvalidTsRate = 0x0;
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
void VDEC_RATE_Flush(UINT8 ui8SyncCh)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[RATE%d][DBG] %s", ui8SyncCh, __FUNCTION__ );

	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Input);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Feed);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DecDone);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DTS);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS_NoErr);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Dropped);
	_VDEC_RATE_Reset_MOVING_AVERAGE((S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Displayed);

	gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate = 0x0;
	gsVdecRate[ui8SyncCh].Debug.ui32LogTick_InvalidTsRate = 0x0;
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
BOOLEAN VDEC_RATE_Set_Speed( UINT8 ui8SyncCh, SINT32 i32Speed )
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[RATE%d][DBG] Speed: %d --> %d", ui8SyncCh, gsVdecRate[ui8SyncCh].i32Speed, i32Speed);

	if( i32Speed == gsVdecRate[ui8SyncCh].i32Speed )
	{
		return FALSE;
	}

	gsVdecRate[ui8SyncCh].i32Speed = i32Speed;

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
SINT32 VDEC_RATE_Get_Speed( UINT8 ui8SyncCh )
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return gsVdecRate[ui8SyncCh].i32Speed;
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
static UINT32 _VDEC_RATE_GetAverageFrameDuration90K(UINT8 ui8SyncCh, BOOLEAN bParser, BOOLEAN bDecoder, BOOLEAN bInput, BOOLEAN bTimeStamp)
{
	UINT32		ui32FrameRate_Parser = 0;
	UINT32		ui32FrameRate_Decoder = 0;
	UINT32		ui32FrameRate_Input = 0;
	UINT32		ui32FrameRate_TS = 0;
	UINT32		ui32FrameDuration90K_Average = 0;
	UINT32		ui32NumOfFrameDuration90K = 0;
	SINT32		i32Speed;

	i32Speed = gsVdecRate[ui8SyncCh].i32Speed;

	if( (bParser == TRUE) && gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes && gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv )
		ui32FrameRate_Parser = gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes / gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv;
	if( (bDecoder == TRUE) && gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes && gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv )
		ui32FrameRate_Decoder = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes / gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv;

	if( (ui32FrameRate_Parser >= VDEC_FRAMERATE_MIN) && (ui32FrameRate_Parser <= VDEC_FRAMERATE_MAX) )
	{
		ui32FrameDuration90K_Average += VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv);
		ui32NumOfFrameDuration90K++;
	}
	if( (ui32FrameRate_Decoder >= VDEC_FRAMERATE_MIN) && (ui32FrameRate_Decoder <= VDEC_FRAMERATE_MAX) )
	{
		ui32FrameDuration90K_Average += VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv);
		ui32NumOfFrameDuration90K++;
	}

	if( i32Speed == 1000 )
	{
		if( (bInput == TRUE) && gsVdecRate[ui8SyncCh].Input.ui32FrameRateRes && gsVdecRate[ui8SyncCh].Input.ui32FrameRateDiv )
			ui32FrameRate_Input = gsVdecRate[ui8SyncCh].Input.ui32FrameRateRes / gsVdecRate[ui8SyncCh].Input.ui32FrameRateDiv;

		if( (ui32FrameRate_Input >= VDEC_FRAMERATE_MIN) && (ui32FrameRate_Input <= VDEC_FRAMERATE_MAX) )
		{
			UINT32	ui32FieldRate;

			ui32FieldRate = (gsVdecRate[ui8SyncCh].bFieldPicture == TRUE) ? 2 : 1;
			ui32FrameDuration90K_Average += VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].Input.ui32FrameRateRes / ui32FieldRate, gsVdecRate[ui8SyncCh].Input.ui32FrameRateDiv);
			ui32NumOfFrameDuration90K++;
		}
	}

	if( bTimeStamp == TRUE )
	{
		UINT32		ui32FrameRateRes;
		UINT32		ui32FrameRateDiv;

		if( PTS_IsDTSMode(ui8SyncCh) == TRUE )
		{
			ui32FrameRateRes = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes;
			ui32FrameRateDiv = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv;
		}
		else
		{
			ui32FrameRateRes = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes;
			ui32FrameRateDiv = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv;
		}
		if( ui32FrameRateRes && ui32FrameRateDiv )
		{
			ui32FrameRate_TS = ui32FrameRateRes / ui32FrameRateDiv;
			ui32FrameRate_TS = ui32FrameRate_TS * i32Speed / 1000;
		}

		if( (ui32FrameRate_TS >= VDEC_FRAMERATE_MIN) && (ui32FrameRate_TS <= VDEC_FRAMERATE_MAX) )
		{
			ui32FrameDuration90K_Average += VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
			ui32FrameDuration90K_Average = ui32FrameDuration90K_Average * 1000 / i32Speed;
			ui32NumOfFrameDuration90K++;
		}
	}

	if( ui32NumOfFrameDuration90K )
	{
		ui32FrameDuration90K_Average /= ui32NumOfFrameDuration90K;
	}

	return ui32FrameDuration90K_Average;
}
#if 0
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
static BOOLEAN _VDEC_RATE_CorrectParserFrameRateError(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	BOOLEAN		bCorrect = FALSE;
	UINT32		ui32FrameDuration90K_Parser = 0;
	UINT32		ui32FrameDuration90K_TimeStamp = 0;
	UINT32		ui32FrameDuration90K_Decoder = 0;

	ui32FrameDuration90K_Parser = gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K_Org;
	ui32FrameDuration90K_Decoder = gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K;
	ui32FrameDuration90K_TimeStamp = _VDEC_RATE_GetAverageFrameDuration90K( ui8SyncCh, FALSE, FALSE, FALSE, TRUE );

	if( ui32FrameDuration90K_Parser != ui32FrameDuration90K_Decoder )
	{
		if( gsVdecRate[ui8SyncCh].i32Speed == 1000 )
		{
			// Parser == DTS/PTS
			if( ((ui32FrameDuration90K_Parser) < (ui32FrameDuration90K_TimeStamp * 12 / 10)) &&
				((ui32FrameDuration90K_Parser) >= (ui32FrameDuration90K_TimeStamp * 8 / 10)) )
			{
				bCorrect = TRUE;
			}
			// Parser / 2 == DTS/PTS
			else if( ((ui32FrameDuration90K_Parser * 2) < (ui32FrameDuration90K_TimeStamp * 12 / 10)) &&
				((ui32FrameDuration90K_Parser * 2) >= (ui32FrameDuration90K_TimeStamp * 8 / 10)) )
			{
				*pui32FrameRateDiv *= 2;
				VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Compensate FrameRate - TimeStamp : %d/%d, %s(%d)", ui8SyncCh, *pui32FrameRateRes, *pui32FrameRateDiv, __FUNCTION__, __LINE__);
				bCorrect =  TRUE;
			}
		}
		else
		{
			*pui32FrameRateRes = gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes;
			*pui32FrameRateDiv = gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv;
		}
	}

	return bCorrect;
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
void VDEC_RATE_UpdateFrameRate_Parser(UINT8 ui8SyncCh, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv)
{
	UINT32		ui32FrameRate;
	BOOLEAN		bCorrect = TRUE;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( (ui32FrameRateRes != gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes_Org) ||
		(ui32FrameRateDiv != gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv_Org) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][DBG] Parser FrameRate_Org: %d/%d --> %d/%d", ui8SyncCh,
									gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes_Org, gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv_Org,
									ui32FrameRateRes, ui32FrameRateDiv);

		gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes_Org = ui32FrameRateRes;
		gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv_Org = ui32FrameRateDiv;
		gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K_Org = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
	}

	if( (ui32FrameRateRes == 0) || (ui32FrameRateDiv == 0) )
	{
//		VDEC_KDRV_Message(WARNING, "[RATE%d][Err] ui32FrameRateRes:%d, ui32FrameRateDiv:%d, %s", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);
		return;
	}

	ui32FrameRate = ui32FrameRateRes / ui32FrameRateDiv;
	if( (ui32FrameRate < VDEC_FRAMERATE_MIN) || (ui32FrameRate > VDEC_FRAMERATE_MAX) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][Err] Parser FrameRate: %d/%d, %s", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);
		return;
	}

	// Frame Rate Error Correction
//	bCorrect = _VDEC_RATE_CorrectParserFrameRateError(ui8SyncCh, &ui32FrameRateRes, &ui32FrameRateDiv);

	if( (gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes == 0x0) || (bCorrect == TRUE) )
	{
		if( (ui32FrameRateRes != gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes) ||
			(ui32FrameRateDiv != gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv) )
		{
			VDEC_KDRV_Message(ERROR, "[RATE%d][DBG] Parser FrameRate: %d/%d --> %d/%d (DTS:%d/%d, PTS:%d/%d)", ui8SyncCh,
										gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv,
										ui32FrameRateRes, ui32FrameRateDiv,
										gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv,
										gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv);

			gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes = ui32FrameRateRes;
			gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv = ui32FrameRateDiv;
			gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
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
UINT32 VDEC_RATE_GetDTSRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;
	SINT32		i32Speed;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	i32Speed = gsVdecRate[ui8SyncCh].i32Speed;

	if( i32Speed == 0 )
		return 0;

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv;

	if( i32Speed != 1000 )
	{
		ui32FrameRateRes /= 1000;
		ui32FrameRateDiv /= i32Speed;
	}

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
UINT32 VDEC_RATE_GetPTSRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;
	SINT32		i32Speed;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	i32Speed = gsVdecRate[ui8SyncCh].i32Speed;

	if( i32Speed == 0 )
		return 0;

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv;

	if( i32Speed != 1000 )
	{
		ui32FrameRateRes /= 1000;
		ui32FrameRateDiv /= i32Speed;
	}

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
UINT32 VDEC_RATE_GetInputRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].Input.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Input.ui32FrameRateDiv;

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
UINT32 VDEC_RATE_GetDecodingRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].DecDone.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].DecDone.ui32FrameRateDiv;

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
UINT32 VDEC_RATE_GetDisplayRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].Displayed.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Displayed.ui32FrameRateDiv;

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
UINT32 VDEC_RATE_GetDropRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	ui32FrameRateRes = gsVdecRate[ui8SyncCh].Dropped.ui32FrameRateRes;
	ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Dropped.ui32FrameRateDiv;

	if( pui32FrameRateRes != NULL && pui32FrameRateDiv != NULL )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	if( ui32FrameRateDiv == 0 )
		return 0;

	return ui32FrameRateRes / ui32FrameRateDiv;
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
static BOOLEAN _VDEC_RATE_CorrectDecoderFrameRateError(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv, BOOLEAN bIsDTSMode)
{
	UINT32		ui32FrameRate = 0;
	SINT32		i32UpDown = 0;
	BOOLEAN		bCorrect = FALSE;

	UINT32		ui32FrameDuration90K_Decoder_Prev = 0;
	UINT32		ui32FrameDuration90K_Decoder_Curr = 0;
	UINT32		ui32FrameDuration90K_TS;
	UINT32		ui32FrameDuration90K_Decoder_Diff = 0;

	UINT32		ui32FrameRateRes;
	UINT32		ui32FrameRateDiv;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	ui32FrameRateRes = *pui32FrameRateRes;
	ui32FrameRateDiv = *pui32FrameRateDiv;
	ui32FrameRate = ui32FrameRateRes / ui32FrameRateDiv;

	if( (ui32FrameRate < VDEC_FRAMERATE_MIN) || (ui32FrameRate > VDEC_FRAMERATE_MAX) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] FrameRateRes:%d, FrameRateDiv:%d, %s", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);

		while( (ui32FrameRate < VDEC_FRAMERATE_MIN) || (ui32FrameRate > VDEC_FRAMERATE_MAX) )
		{
			if( ui32FrameRate < VDEC_FRAMERATE_MIN )
			{
				ui32FrameRateRes *= 10;

				if( i32UpDown < 0 )
				{
					i32UpDown = 0;
					break;
				}
				i32UpDown++;
			}
			if( ui32FrameRate > VDEC_FRAMERATE_MAX )
			{
				ui32FrameRateDiv *= 10;

				if( i32UpDown > 0 )
				{
					i32UpDown = 0;
					break;
				}
				i32UpDown--;
			}

			ui32FrameRate = ui32FrameRateRes / ui32FrameRateDiv;
		}

		if( i32UpDown )
			VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Compensate FrameRateRes:%d, FrameRateDiv:%d, %s(%d)", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__, __LINE__);

		bCorrect = (i32UpDown == 0) ? FALSE : TRUE;
	}

	ui32FrameDuration90K_Decoder_Prev = VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv);
	ui32FrameDuration90K_Decoder_Curr = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
	ui32FrameDuration90K_Decoder_Diff = VDEC_DIFF(ui32FrameDuration90K_Decoder_Prev, ui32FrameDuration90K_Decoder_Curr);

	if( ui32FrameDuration90K_Decoder_Prev != ui32FrameDuration90K_Decoder_Curr )
	{
		if( (ui32FrameDuration90K_Decoder_Diff >= 1) && (ui32FrameDuration90K_Decoder_Diff <= 3) )
		{ // Micro
			VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Compensate FrameRate:%d/%d --> %d/%d, %s(%d)", ui8SyncCh,
											ui32FrameRateRes, ui32FrameRateDiv,
											gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv,
											__FUNCTION__, __LINE__);

			ui32FrameRateRes = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes;
			ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv;
			bCorrect = TRUE;
		}
		else
		{
			UINT32		ui32FrameRateRes_TS;
			UINT32		ui32FrameRateDiv_TS;
			UINT32		ui32FrameRate_TS;

			ui32FrameRate_TS = (bIsDTSMode == TRUE) ? VDEC_RATE_GetDTSRate( ui8SyncCh, &ui32FrameRateRes_TS, &ui32FrameRateDiv_TS ) : VDEC_RATE_GetPTSRate( ui8SyncCh, &ui32FrameRateRes_TS, &ui32FrameRateDiv_TS );

			if( (ui32FrameRate_TS >= VDEC_FRAMERATE_MIN) && (ui32FrameRate_TS <= VDEC_FRAMERATE_MAX) )
			{ // Macro
				ui32FrameDuration90K_TS = VSync_CalculateFrameDuration(ui32FrameRateRes_TS, ui32FrameRateDiv_TS);

				if( ((ui32FrameDuration90K_Decoder_Curr / 2) < (ui32FrameDuration90K_TS * 13 / 10)) &&
					((ui32FrameDuration90K_Decoder_Curr / 2) >= (ui32FrameDuration90K_TS * 7 / 10)) )
				{
					ui32FrameRateRes *= 2;
					bCorrect = TRUE;
					VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Compensate FrameRate TS Res:%d, Div:%d, %s(%d)", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__, __LINE__);
				}
				else if( ui32FrameDuration90K_Decoder_Curr != ui32FrameDuration90K_Decoder_Prev )
				{
					VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Compensate FrameRate:%d/%d --> %d/%d, %s(%d)", ui8SyncCh,
													ui32FrameRateRes, ui32FrameRateDiv,
													gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv,
													__FUNCTION__, __LINE__);

					gsVdecRate[ui8SyncCh].bVariableFrameRate = TRUE;

					ui32FrameRateRes = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes;
					ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv;
					bCorrect = TRUE;
				}
			}
			else if( gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K != 0 )
			{
				ui32FrameRateRes = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes;
				ui32FrameRateDiv = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv;
				bCorrect = TRUE;
			}
		}
	}

	if( bCorrect == TRUE )
	{
		*pui32FrameRateRes = ui32FrameRateRes;
		*pui32FrameRateDiv = ui32FrameRateDiv;
	}

	return bCorrect;
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
void VDEC_RATE_UpdateFrameRate_Decoder(UINT8 ui8SyncCh, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bFieldPicture, BOOLEAN bInterlaced, BOOLEAN bVariableFrameRate, BOOLEAN bIsDTSMode)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( (ui32FrameRateRes != gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes_Org) ||
		(ui32FrameRateDiv != gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv_Org) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][DBG] Decoder FrameRate_Org: %d/%d --> %d/%d", ui8SyncCh,
									gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes_Org, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv_Org,
									ui32FrameRateRes, ui32FrameRateDiv);

		gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes_Org = ui32FrameRateRes;
		gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv_Org = ui32FrameRateDiv;
		gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K_Org = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
	}

	if( gsVdecRate[ui8SyncCh].bFieldPicture != bFieldPicture )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][DBG] Field Picture: %d", ui8SyncCh, bFieldPicture );
		gsVdecRate[ui8SyncCh].bFieldPicture = bFieldPicture;
	}

	if( gsVdecRate[ui8SyncCh].bInterlaced != bInterlaced )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][DBG] %s", ui8SyncCh, (bInterlaced == TRUE) ? "Interlaced" : "Progressive" );
		gsVdecRate[ui8SyncCh].bInterlaced = bInterlaced;
	}

	if( gsVdecRate[ui8SyncCh].bVariableFrameRate != bVariableFrameRate )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][DBG] Variable Frame Rate: %s", ui8SyncCh, (bVariableFrameRate == TRUE) ? "On" : "Off" );
		gsVdecRate[ui8SyncCh].bVariableFrameRate = bVariableFrameRate;
	}

	if( (ui32FrameRateRes == 0) || (ui32FrameRateDiv == 0) )
	{
		if( (gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate % 0x80) == 0x0 )
			VDEC_KDRV_Message(ERROR, "[RATE%d][Err] ui32FrameRateRes:%d, ui32FrameRateDiv:%d, %s", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);
		gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate++;
		return;
	}
	if( (ui32FrameRateRes > 0x3FFFFFFF) || (ui32FrameRateDiv > 0x3FFFFFFF) )
	{
		if( (gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate % 0x80) == 0x0 )
			VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Decoder FrameRate: %d/%d, %s", ui8SyncCh, ui32FrameRateRes, ui32FrameRateDiv, __FUNCTION__);
		gsVdecRate[ui8SyncCh].Debug.ui32LogTick_NoFrameRate++;
		return;
	}

	// Frame Rate Error Correction
	_VDEC_RATE_CorrectDecoderFrameRateError(ui8SyncCh, &ui32FrameRateRes, &ui32FrameRateDiv, bIsDTSMode);

	if( (ui32FrameRateRes != gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes) ||
		(ui32FrameRateDiv != gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Decoder FrameRate: %d/%d --> %d/%d", ui8SyncCh,
									gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv,
									ui32FrameRateRes, ui32FrameRateDiv);

		gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes = ui32FrameRateRes;
		gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv = ui32FrameRateDiv;
		gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K = VSync_CalculateFrameDuration(ui32FrameRateRes, ui32FrameRateDiv);
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
void VDEC_RATE_SetCodecType( UINT8 ui8SyncCh, UINT8 ui8CodecType )
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return;
	}

	if( ui8CodecType != gsVdecRate[ui8SyncCh].ui8CodecType )
		VDEC_KDRV_Message(_TRACE, "[RATE%d][DBG] Parser CodecType: %d --> %d", ui8SyncCh, gsVdecRate[ui8SyncCh].ui8CodecType, ui8CodecType);

	gsVdecRate[ui8SyncCh].ui8CodecType = ui8CodecType;
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
UINT8 VDEC_RATE_GetCodecType( UINT8 ui8SyncCh )
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0xFF;
	}

	return gsVdecRate[ui8SyncCh].ui8CodecType;
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
static UINT32 _VDEC_RATE_UpdateFrameRate_byXXX(BOOLEAN bEvent, BOOLEAN bErrFilter, UINT32 ui32TS_90kTick, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv, S_RATE_MOVING_AVERAGE_T *pMovingAverage)
{
	UINT32		ui32TS_90kTickDiff = 0;
	UINT32		ui32TS_90kTickDiff_Ratio = 0;

	ui32TS_90kTick &= 0x0FFFFFFF;

	if( pMovingAverage->ui32_Prev == 0x80000000 )
	{
		pMovingAverage->ui32Diff_History_Sum = 0;
		pMovingAverage->ui32Diff_History_Count = 0;
		pMovingAverage->ui32Diff_Event_Count = 0;

		pMovingAverage->ui32_Prev = ui32TS_90kTick;

		*pui32FrameRateRes = 0;
		*pui32FrameRateDiv = 1;
	}
	else
	{
		if( ui32TS_90kTick >= pMovingAverage->ui32_Prev )
			ui32TS_90kTickDiff = ui32TS_90kTick - pMovingAverage->ui32_Prev;
		else
			ui32TS_90kTickDiff = 0x10000000 - pMovingAverage->ui32_Prev + ui32TS_90kTick;

		if( ui32TS_90kTickDiff > VDEC_RATE_MAX_DIFF_HISTORY_NUM )
		{
			if( bErrFilter == TRUE )
			{
				ui32TS_90kTickDiff = 0;

				if( pMovingAverage->ui32Diff_History_Sum == 0 )
				{
					VDEC_KDRV_Message(ERROR, "[RATE][Err] Replace Previous 90k Tick Value: 0x%X --> 0x%X, %s", pMovingAverage->ui32_Prev, ui32TS_90kTick, __FUNCTION__ );
					pMovingAverage->ui32_Prev = ui32TS_90kTick;
					return 0;
				}
			}
			else
			{
				ui32TS_90kTickDiff = VDEC_RATE_MAX_DIFF_HISTORY_NUM;
				pMovingAverage->ui32_Prev = ui32TS_90kTick;
			}
		}
		else
		{
			pMovingAverage->ui32_Prev = ui32TS_90kTick;
		}

		if( pMovingAverage->ui32Diff_History_Count >= VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF )
		{
			// Remove Value
			pMovingAverage->ui32Diff_History_Sum -= (pMovingAverage->ui32Diff_History[pMovingAverage->ui32Diff_History_Index] & ~0x80000000);

			// Decrease Count
			if( pMovingAverage->ui32Diff_History[pMovingAverage->ui32Diff_History_Index] & 0x80000000 )
				pMovingAverage->ui32Diff_Event_Count--;

			pMovingAverage->ui32Diff_History_Count--;
		}

		// Add Value
		pMovingAverage->ui32Diff_History[pMovingAverage->ui32Diff_History_Index] = ui32TS_90kTickDiff;
		if( bEvent == TRUE )
			pMovingAverage->ui32Diff_History[pMovingAverage->ui32Diff_History_Index] |= 0x80000000;

		pMovingAverage->ui32Diff_History_Index++;
		if( pMovingAverage->ui32Diff_History_Index >= VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF )
			pMovingAverage->ui32Diff_History_Index = 0;

		pMovingAverage->ui32Diff_History_Sum += ui32TS_90kTickDiff;

		// Increase Count
		if( bEvent == TRUE )
			pMovingAverage->ui32Diff_Event_Count++;

		pMovingAverage->ui32Diff_History_Count++;

		// Calculate Frame Rate
		if( pMovingAverage->ui32Diff_History_Count >= 4 )
		{
			*pui32FrameRateRes = 90000 * pMovingAverage->ui32Diff_Event_Count;
			*pui32FrameRateDiv = pMovingAverage->ui32Diff_History_Sum;

			if( *pui32FrameRateDiv == 0 )
			{ // All Same PTS
				*pui32FrameRateRes = 0;
				*pui32FrameRateDiv = 1;
			}

			pMovingAverage->ui32FrameRateRes = *pui32FrameRateRes;
			pMovingAverage->ui32FrameRateDiv = *pui32FrameRateDiv;
		}
		else
		{
			*pui32FrameRateRes = 0;
			*pui32FrameRateDiv = 1;
		}

		if( pMovingAverage->ui32Diff_Event_Count >= VDEC_RATE_NUM_OF_DIFF_HISTORY_BUFF )
		{
			if( ui32TS_90kTickDiff == 0 )
				ui32TS_90kTickDiff = 1;
			ui32TS_90kTickDiff_Ratio = pMovingAverage->ui32Diff_History_Sum / (pMovingAverage->ui32Diff_Event_Count * ui32TS_90kTickDiff);
		}
	}

//	VDEC_KDRV_Message(ERROR, "[RATE][DBG] STC:0x%X, Diff:0x%X, Count:%u, Sum:%u, Res:0x%X, Div:0x%X", ui32TS_90kTick, ui32TS_90kTickDiff, pMovingAverage->ui32Diff_History_Count, pMovingAverage->ui32Diff_History_Sum, *pui32FrameRateRes, *pui32FrameRateDiv);

	return ui32TS_90kTickDiff_Ratio;
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
UINT32 VDEC_RATE_UpdateFrameRate_byDTS(UINT8 ui8SyncCh, UINT32 ui32DTS, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32TS_90kTickDiff_Ratio = 0;
#if 0
	UINT32		ui32FrameRateRes_Prev = 0x0;
	UINT32		ui32FrameRateDiv_Prev = 0xFFFFFFFF;
	UINT32		ui32FrameRate = 0;
#endif

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}
#if 0
	ui32FrameRateRes_Prev = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes;
	ui32FrameRateDiv_Prev = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv;
#endif

	if( ui32DTS == VDEC_UNKNOWN_PTS )
		ui32DTS = gsVdecRate[ui8SyncCh].DTS.ui32_Prev;
	ui32TS_90kTickDiff_Ratio = _VDEC_RATE_UpdateFrameRate_byXXX(TRUE, FALSE, ui32DTS, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DTS);
#if 0
	if( gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv )
		ui32FrameRate = gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes / gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv;

	if( ui32FrameRate )
	{
		UINT32		ui32FrameDuration90K_Prev = 0xFFFFFFFF;
		UINT32		ui32FrameDuration90K_Curr = 0xFFFFFFFF;

		if( ui32FrameRateRes_Prev && ui32FrameRateDiv_Prev )
			ui32FrameDuration90K_Prev = VSync_CalculateFrameDuration(ui32FrameRateRes_Prev, ui32FrameRateDiv_Prev);

		if( gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes && gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv )
			ui32FrameDuration90K_Curr = VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv);

		if( VDEC_DIFF(ui32FrameDuration90K_Prev, ui32FrameDuration90K_Curr) > 3 )
			VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] FrameRate DTS: %d/%d(=%d) --> %d/%d(=%d), Status: %d/%d(=%d), Ratio:%d", ui8SyncCh,
										ui32FrameRateRes_Prev, ui32FrameRateDiv_Prev, ui32FrameDuration90K_Prev,
										gsVdecRate[ui8SyncCh].DTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].DTS.ui32FrameRateDiv, ui32FrameDuration90K_Curr,
										gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv, gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K,
										ui32TS_90kTickDiff_Ratio);
	}
#endif
	return ui32TS_90kTickDiff_Ratio;
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
UINT32 VDEC_RATE_UpdateFrameRate_byPTS(UINT8 ui8SyncCh, UINT32 ui32PTS, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32TS_90kTickDiff_Ratio = 0;
	UINT32		ui32PTS_NoErr;
#if 0
	UINT32		ui32FrameRateRes_Prev = 0x0;
	UINT32		ui32FrameRateDiv_Prev = 0xFFFFFFFF;
	UINT32		ui32FrameRate = 0;
#endif

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}
#if 0
	ui32FrameRateRes_Prev = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes;
	ui32FrameRateDiv_Prev = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv;
#endif
	if( ui32PTS == VDEC_UNKNOWN_PTS )
	{
		ui32PTS_NoErr = gsVdecRate[ui8SyncCh].PTS_NoErr.ui32_Prev;
		ui32PTS = gsVdecRate[ui8SyncCh].PTS.ui32_Prev;
	}
	else
	{
		ui32PTS_NoErr = ui32PTS;
	}
	_VDEC_RATE_UpdateFrameRate_byXXX(TRUE, TRUE, ui32PTS, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS_NoErr);
	ui32TS_90kTickDiff_Ratio = _VDEC_RATE_UpdateFrameRate_byXXX(TRUE, FALSE, ui32PTS, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].PTS);
#if 0
	if( gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv )
		ui32FrameRate = gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes / gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv;

	if( ui32FrameRate )
	{
		UINT32		ui32FrameDuration90K_Prev = 0xFFFFFFFF;
		UINT32		ui32FrameDuration90K_Curr = 0xFFFFFFFF;

		if( ui32FrameRateRes_Prev && ui32FrameRateDiv_Prev )
			ui32FrameDuration90K_Prev = VSync_CalculateFrameDuration(ui32FrameRateRes_Prev, ui32FrameRateDiv_Prev);

		if( gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes && gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv )
			ui32FrameDuration90K_Curr = VSync_CalculateFrameDuration(gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv);

		if( VDEC_DIFF(ui32FrameDuration90K_Prev, ui32FrameDuration90K_Curr) > 3 )
			VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] FrameRate PTS: %d/%d(=%d) --> %d/%d(=%d), Status: %d/%d(=%d), Ratio:%d", ui8SyncCh,
										ui32FrameRateRes_Prev, ui32FrameRateDiv_Prev, ui32FrameDuration90K_Prev,
										gsVdecRate[ui8SyncCh].PTS.ui32FrameRateRes, gsVdecRate[ui8SyncCh].PTS.ui32FrameRateDiv, ui32FrameDuration90K_Curr,
										gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv, gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K,
										ui32TS_90kTickDiff_Ratio);
	}
#endif
	return ui32TS_90kTickDiff_Ratio;
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
UINT32 VDEC_RATE_UpdateFrameRate_byInput(UINT8 ui8SyncCh, UINT32 ui32GSTC, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return _VDEC_RATE_UpdateFrameRate_byXXX(TRUE, TRUE, ui32GSTC, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Input);
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
UINT32 VDEC_RATE_UpdateFrameRate_byFeed(UINT8 ui8SyncCh, UINT32 ui32GSTC, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return _VDEC_RATE_UpdateFrameRate_byXXX(TRUE, TRUE, ui32GSTC, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].Feed);
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
UINT32 VDEC_RATE_UpdateFrameRate_byDecDone(UINT8 ui8SyncCh, UINT32 ui32GSTC, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	return _VDEC_RATE_UpdateFrameRate_byXXX(TRUE, TRUE, ui32GSTC, pui32FrameRateRes, pui32FrameRateDiv, (S_RATE_MOVING_AVERAGE_T *)&gsVdecRate[ui8SyncCh].DecDone);
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
static BOOLEAN _VDEC_RATE_DecideParserDecoderFrameRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv, UINT32 *pui32FrameDuration90K)
{
	UINT32		ui32FrameDuration90K_Parser = 0;
	UINT32		ui32FrameDuration90K_Decoder = 0;
	BOOLEAN		bChanged = FALSE;
	BOOLEAN		bChooseParserFrameRate = gsVdecRate[ui8SyncCh].bChooseParserFrameRate;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	if( (gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes) && (gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv) )
		ui32FrameDuration90K_Parser = gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K;
	if( (gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes) && (gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv) )
		ui32FrameDuration90K_Decoder = gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K;

	if( ui32FrameDuration90K_Parser && ui32FrameDuration90K_Decoder )
	{
		UINT32		ui32FrameDuration90K_Bigger;
		UINT32		ui32FrameDuration90K_Smaller;
		UINT32		ui32FrameDuration90K_30;
		UINT32		ui32FrameDuration90K_70;
		UINT32		ui32Diff_Parser;
		UINT32		ui32Diff_Decoder;
		UINT32		ui32FrameDuration90K_Status;

		if( ui32FrameDuration90K_Parser > ui32FrameDuration90K_Decoder )
		{
			ui32FrameDuration90K_Bigger = ui32FrameDuration90K_Parser;
			ui32FrameDuration90K_Smaller = ui32FrameDuration90K_Decoder;
		}
		else
		{
			ui32FrameDuration90K_Bigger = ui32FrameDuration90K_Decoder;
			ui32FrameDuration90K_Smaller = ui32FrameDuration90K_Parser;
		}

		// Macro
		if( ui32FrameDuration90K_Bigger > (ui32FrameDuration90K_Smaller * 12 / 10) )
		{
			UINT32		ui32FrameDuration90K_Average;

			ui32FrameDuration90K_30 = ui32FrameDuration90K_Smaller + (ui32FrameDuration90K_Bigger - ui32FrameDuration90K_Smaller) * 3 / 10;
			ui32FrameDuration90K_70 = ui32FrameDuration90K_Smaller + (ui32FrameDuration90K_Bigger - ui32FrameDuration90K_Smaller) * 7 / 10;

			ui32FrameDuration90K_Average = _VDEC_RATE_GetAverageFrameDuration90K( ui8SyncCh, FALSE, FALSE, FALSE, TRUE );

			if( (ui32FrameDuration90K_Average > (ui32FrameDuration90K_Smaller * 7 / 10)) &&
				(ui32FrameDuration90K_Average < ui32FrameDuration90K_30) )
			{
				bChanged = TRUE;
			}
			else if( (ui32FrameDuration90K_Average < (ui32FrameDuration90K_Bigger * 13 / 10)) &&
				(ui32FrameDuration90K_Average > ui32FrameDuration90K_70) )
			{
				bChanged = TRUE;
			}

			if( bChanged == TRUE )
			{
				VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Frame Duration Compensation Parser:%d, Decoder:%d, Average:%d", ui8SyncCh, ui32FrameDuration90K_Parser, ui32FrameDuration90K_Decoder, ui32FrameDuration90K_Average);

				ui32Diff_Parser = VDEC_DIFF(ui32FrameDuration90K_Parser, ui32FrameDuration90K_Average);
				ui32Diff_Decoder = VDEC_DIFF(ui32FrameDuration90K_Decoder, ui32FrameDuration90K_Average);

				if( ui32Diff_Parser < ui32Diff_Decoder )
				{
					bChooseParserFrameRate = TRUE;
				}
				else
				{
					bChooseParserFrameRate = FALSE;
				}
			}
		}

		ui32FrameDuration90K_Status = gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K;

		// Micro
		if( (bChanged == FALSE) && (ui32FrameDuration90K_Status) )
		{
			ui32Diff_Parser = VDEC_DIFF(ui32FrameDuration90K_Parser, ui32FrameDuration90K_Status);
			ui32Diff_Decoder = VDEC_DIFF(ui32FrameDuration90K_Decoder, ui32FrameDuration90K_Status);

			if( ui32Diff_Parser == ui32Diff_Decoder )
			{
				if( (gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes == gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes) &&
					(gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv == gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv) )
				{
					bChooseParserFrameRate = TRUE;
				}
				else
				{
					bChooseParserFrameRate = FALSE;
				}
			}
			else if( ui32Diff_Parser < ui32Diff_Decoder )
			{
				bChooseParserFrameRate = TRUE;
			}
			else
			{
				bChooseParserFrameRate = FALSE;
			}
		}

		gsVdecRate[ui8SyncCh].bChooseParserFrameRate = bChooseParserFrameRate;
	}
	else if( ui32FrameDuration90K_Parser )
	{
		bChooseParserFrameRate = TRUE;
		gsVdecRate[ui8SyncCh].bChooseParserFrameRate = bChooseParserFrameRate;
	}
	else if( ui32FrameDuration90K_Decoder )
	{
		bChooseParserFrameRate = FALSE;
		gsVdecRate[ui8SyncCh].bChooseParserFrameRate = bChooseParserFrameRate;
	}
	else
	{
//		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] No Frame Rate, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	if( bChooseParserFrameRate == TRUE )
	{
//		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Frame Duration Compensation PARSER:%d, Decoder:%d", ui8SyncCh, ui32FrameDuration90K_Parser, ui32FrameDuration90K_Decoder);

		*pui32FrameRateRes = gsVdecRate[ui8SyncCh].Parser.ui32FrameRateRes;
		*pui32FrameRateDiv = gsVdecRate[ui8SyncCh].Parser.ui32FrameRateDiv;
		*pui32FrameDuration90K = gsVdecRate[ui8SyncCh].Parser.ui32FrameDuration90K;
	}
	else
	{
//		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] Frame Duration Compensation Parser:%d, DECODER:%d", ui8SyncCh, ui32FrameDuration90K_Parser, ui32FrameDuration90K_Decoder);

		*pui32FrameRateRes = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateRes;
		*pui32FrameRateDiv = gsVdecRate[ui8SyncCh].Decoder.ui32FrameRateDiv;
		*pui32FrameDuration90K = gsVdecRate[ui8SyncCh].Decoder.ui32FrameDuration90K;
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
static BOOLEAN _VDEC_RATE_GetUpdateFrameRate(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv, UINT32 *pui32FrameDuration90K)
{
	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return FALSE;
	}

	if( _VDEC_RATE_DecideParserDecoderFrameRate( ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv, pui32FrameDuration90K ) == FALSE )
	{
		return FALSE;
	}

	if( (gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes != *pui32FrameRateRes) ||
		(gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv != *pui32FrameRateDiv) ||
		(gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K != *pui32FrameDuration90K) )
	{
		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] FrameRate: %d/%d -> %d/%d", 	ui8SyncCh,
									gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes, gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv,
									*pui32FrameRateRes, *pui32FrameRateDiv);
		VDEC_KDRV_Message(WARNING, "[RATE%d][Wrn] FrameDuration90k: %d --> %d", ui8SyncCh,
									gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K, *pui32FrameDuration90K);

		gsVdecRate[ui8SyncCh].Status.ui32FrameRateRes = *pui32FrameRateRes;
		gsVdecRate[ui8SyncCh].Status.ui32FrameRateDiv = *pui32FrameRateDiv;
		gsVdecRate[ui8SyncCh].Status.ui32FrameDuration90K = *pui32FrameDuration90K;
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
UINT32 VDEC_RATE_GetFrameRateDuration(UINT8 ui8SyncCh)
{
	UINT32		ui32FrameRateRes = 0;
	UINT32		ui32FrameRateDiv = 1;
	UINT32		ui32FrameDuration90K = 0;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	if( _VDEC_RATE_GetUpdateFrameRate( ui8SyncCh, &ui32FrameRateRes, &ui32FrameRateDiv, &ui32FrameDuration90K ) == FALSE )
		return 0;

	return ui32FrameDuration90K;
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
UINT32 VDEC_RATE_GetFrameRateResDiv(UINT8 ui8SyncCh, UINT32 *pui32FrameRateRes, UINT32 *pui32FrameRateDiv)
{
	UINT32		ui32FrameDuration90K = 0;

	if( ui8SyncCh >= VDEC_RATE_NUM_OF_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[RATE%d][Err] Channel Number, %s", ui8SyncCh, __FUNCTION__ );
		return 0;
	}

	if( _VDEC_RATE_GetUpdateFrameRate( ui8SyncCh, pui32FrameRateRes, pui32FrameRateDiv, &ui32FrameDuration90K ) == FALSE )
	{
		*pui32FrameRateRes = 0;
		*pui32FrameRateDiv = 1;
		return 0;
	}

	return *pui32FrameRateRes / *pui32FrameRateDiv;
}


/** @} */
