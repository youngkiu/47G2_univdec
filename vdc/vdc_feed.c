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
 * date       2012.06.22
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
#include <linux/kernel.h>
#include <linux/version.h>
#include "vdc_feed.h"
#include "../mcu/vdec_print.h"
#include "../hal/lg1152/top_hal_api.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define		MAX_NUM_OF_FEED_GROUP		2
#define		VDEC_NUM_OF_FEED			3
#define		VDEC_MAX_DECODING_TIME		90000	// 1000ms

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct
{
	UINT8 ui8VdcCh[MAX_NUM_OF_FEED_GROUP];
	UINT32 ui32NumOfVdcCh;
} S_FEED_GROUP_T;

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
struct
{
	UINT32			ui32LogTick_Kick;

	BOOLEAN (*_fpFEED_GetDQStatus)(UINT8 ui8VdcCh, UINT32 *pui32NumOfActiveDQ, UINT32 *pui32NumOfFreeDQ);
	BOOLEAN (*_fpFEED_IsReady)(UINT8 ui8VdcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ);
	BOOLEAN (*_fpFEED_RequestReset)(UINT8 ui8VdcCh);
	BOOLEAN (*_fpFEED_SendCmd)(UINT8 ui8VdcCh, S_VDC_AU_T *pVdcAu, S_VDC_AU_T *pVdcAu_Next, BOOLEAN *pbNeedMore, UINT32 ui32GSTC, UINT32 ui32NumOfActiveAuQ, UINT32 ui32NumOfActiveDQ);

	struct
	{
		struct
		{
			BOOLEAN		bDual;
		} sConfig;
		struct
		{
			BOOLEAN		bOpened;
			UINT32		ui32SentTime;

			UINT32		ui32BusyTime_Checked;
			UINT32		ui32BusyTime_Sum;

			UINT32		ui32NumOfActiveAuQ;
			UINT32		ui32NumOfActiveDQ;
			UINT32		ui32NumOfFreeDQ;
			BOOLEAN		bReadyState;
			BOOLEAN		bPrevEmpty;
		} sStatus;
		struct
		{
			UINT32		ui32LogTick_ResetNoAuib;
			UINT32		ui32SendCmdCnt;
			UINT32		ui32SendCmdCnt_Log;
			UINT32		ui32BusyTime_Log;
			S_VDC_AU_T	sVdcAu_Sent;
		} sDebug;
	} sCh[VDEC_NUM_OF_FEED];

} gsFeedDb;



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
extern void FEED_Init(	BOOLEAN (*_fpFEED_GetDQStatus)(UINT8 ui8VdcCh, UINT32 *pui32NumOfActiveDQ, UINT32 *pui32NumOfFreeDQ),
						BOOLEAN (*_fpFEED_IsReady)(UINT8 ui8VdcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ),
						BOOLEAN (*_fpFEED_RequestReset)(UINT8 ui8VdcCh),
						BOOLEAN (*_fpFEED_SendCmd)(UINT8 ui8VdcCh, S_VDC_AU_T *pVdcAu, S_VDC_AU_T *pVdcAu_Next, BOOLEAN *pbNeedMore, UINT32 ui32GSTC, UINT32 ui32NumOfActiveAuQ, UINT32 ui32NumOfActiveDQ) )
{
	UINT8	ui8VdcCh;

	VDEC_KDRV_Message(_TRACE, "[FEED][DBG] %s", __FUNCTION__ );

	gsFeedDb.ui32LogTick_Kick = 0;
	gsFeedDb._fpFEED_GetDQStatus = _fpFEED_GetDQStatus;
	gsFeedDb._fpFEED_IsReady = _fpFEED_IsReady;
	gsFeedDb._fpFEED_RequestReset = _fpFEED_RequestReset;
	gsFeedDb._fpFEED_SendCmd = _fpFEED_SendCmd;

	for( ui8VdcCh = 0; ui8VdcCh < VDEC_NUM_OF_FEED; ui8VdcCh++ )
	{
		gsFeedDb.sCh[ui8VdcCh].sConfig.bDual = FALSE;
		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime = 0x80000000;
		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked = 0x80000000;
		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum = 0x0;
		gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened = FALSE;
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
extern BOOLEAN FEED_Open( UINT8 ui8VdcCh, BOOLEAN bDual )
{
	UINT8	ui8Ch;

	if( ui8VdcCh > VDEC_NUM_OF_FEED )
	{
		VDEC_KDRV_Message(ERROR, "[FEED%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if( gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened == TRUE )
	{
		VDEC_KDRV_Message(ERROR, "[FEED%d][Err] Not Closed Channel, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[FEED%d][DBG] Dual:%d, %s", ui8VdcCh, bDual, __FUNCTION__ );

	for( ui8Ch = 0; ui8Ch < VDEC_NUM_OF_FEED; ui8Ch++ )
	{
		if( gsFeedDb.sCh[ui8Ch].sStatus.bOpened == TRUE )
		{
			VDEC_KDRV_Message(DBG_SYS, "[FEED%d][DBG] AuQ:%d, DQ:A%dF%d, SendCmdCnt:%d, %s(%d)", ui8Ch,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfActiveAuQ,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfActiveDQ,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfFreeDQ,
										gsFeedDb.sCh[ui8Ch].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );
		}
	}

	gsFeedDb.sCh[ui8VdcCh].sConfig.bDual = bDual;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime = 0x80000000;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked = 0x80000000;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum = 0x0;
	gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened = TRUE;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ = 0;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveDQ = 0;
	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfFreeDQ = 0;
	gsFeedDb.sCh[ui8VdcCh].sStatus.bReadyState = TRUE;
	gsFeedDb.sCh[ui8VdcCh].sStatus.bPrevEmpty = TRUE;
	gsFeedDb.sCh[ui8VdcCh].sDebug.ui32LogTick_ResetNoAuib = 0;
	gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt = 0;
	gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt_Log = 0xFFFFFFFF;
	gsFeedDb.sCh[ui8VdcCh].sDebug.ui32BusyTime_Log = 0x0;
	gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuStartAddr = 0x0;
	gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuEndAddr = 0x0;
	gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuSize = 0x0;

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
extern BOOLEAN FEED_Close( UINT8 ui8VdcCh )
{
	UINT8	ui8Ch;

	if( ui8VdcCh > VDEC_NUM_OF_FEED )
	{
		VDEC_KDRV_Message(ERROR, "[FEED%d][Err] Channel Number, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	if( gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[FEED%d][Err] Already Closed Channel, %s", ui8VdcCh, __FUNCTION__ );
		return FALSE;
	}

	VDEC_KDRV_Message(_TRACE, "[FEED%d][DBG] %s", ui8VdcCh, __FUNCTION__ );

	for( ui8Ch = 0; ui8Ch < VDEC_NUM_OF_FEED; ui8Ch++ )
	{
		if( gsFeedDb.sCh[ui8Ch].sStatus.bOpened == TRUE )
		{
			VDEC_KDRV_Message(DBG_SYS, "[FEED%d][DBG] AuQ:%d, DQ:A%dF%d, SendCmdCnt:%d, %s(%d)", ui8Ch,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfActiveAuQ,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfActiveDQ,
										gsFeedDb.sCh[ui8Ch].sStatus.ui32NumOfFreeDQ,
										gsFeedDb.sCh[ui8Ch].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );
		}
	}

	gsFeedDb.sCh[ui8VdcCh].sConfig.bDual = FALSE;
	gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened = FALSE;

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
static UINT32 _FEED_GroupChannel( S_FEED_GROUP_T sFeedGroup[] )
{
	UINT8 				ui8VdcCh;
	UINT32 				ui32Group;
	UINT32 				ui32Group_Dual = VDEC_NUM_OF_FEED;
	UINT32				ui32NumOfGroup = 0;

	for( ui8VdcCh = 0; ui8VdcCh < VDEC_NUM_OF_FEED; ui8VdcCh++ )
	{
		if( gsFeedDb.sCh[ui8VdcCh].sStatus.bOpened == TRUE )
		{
			if( gsFeedDb.sCh[ui8VdcCh].sConfig.bDual == TRUE )
			{
				if( ui32Group_Dual == VDEC_NUM_OF_FEED )
				{
					ui32Group_Dual = ui32NumOfGroup;
					sFeedGroup[ui32Group_Dual].ui32NumOfVdcCh = 0;
					ui32NumOfGroup++;
				}

				ui32Group = ui32Group_Dual;
			}
			else
			{
				ui32Group = ui32NumOfGroup;
				sFeedGroup[ui32Group].ui32NumOfVdcCh = 0;
				ui32NumOfGroup++;
			}

			sFeedGroup[ui32Group].ui8VdcCh[sFeedGroup[ui32Group].ui32NumOfVdcCh] = ui8VdcCh;
			sFeedGroup[ui32Group].ui32NumOfVdcCh++;
		}
	}

	return ui32NumOfGroup;
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
static void _FEED_GetQStatus(UINT8 ui8VdcCh, BOOLEAN bRefresh, UINT32 *pui32NumOfActiveAuQ, UINT32 *pui32NumOfActiveDQ, UINT32 *pui32NumOfFreeDQ, BOOLEAN *pbReadyState, UINT32 ui32GSTC)
{
	if( bRefresh == TRUE )
	{
		*pui32NumOfActiveAuQ = AU_Q_NumActive( ui8VdcCh );
		gsFeedDb._fpFEED_GetDQStatus( ui8VdcCh, pui32NumOfActiveDQ, pui32NumOfFreeDQ );
		*pbReadyState = gsFeedDb._fpFEED_IsReady( ui8VdcCh, ui32GSTC, *pui32NumOfFreeDQ );

		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ = *pui32NumOfActiveAuQ;
		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveDQ = *pui32NumOfActiveDQ;
		gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfFreeDQ = *pui32NumOfFreeDQ;
		gsFeedDb.sCh[ui8VdcCh].sStatus.bReadyState = *pbReadyState;
	}
	else
	{
		*pui32NumOfActiveAuQ = gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ;
		*pui32NumOfActiveDQ = gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveDQ;
		*pui32NumOfFreeDQ = gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfFreeDQ;
		*pbReadyState = gsFeedDb.sCh[ui8VdcCh].sStatus.bReadyState;
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
static void _FEED_UpdateRunningTime( UINT8 ui8VdcCh, BOOLEAN bRunning, UINT32 ui32GSTC )
{
	UINT32		ui32ElapseTime;

	if( gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked != 0x80000000 )
	{
		ui32ElapseTime = (ui32GSTC >= gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked) ? (ui32GSTC - gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked) : (ui32GSTC + 0x80000000 - gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked);

		if( ui32ElapseTime > 0x7F000000 )
		{
			VDEC_KDRV_Message(ERROR, "[FEED%d][Err:0x%08X] ElapseTime:0x%08X(~0x%08X-0x%08X), %s(%d)", ui8VdcCh, ui32GSTC,
										ui32ElapseTime, ui32GSTC, gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked,
										__FUNCTION__, __LINE__ );
		}
		else
		{
			gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum += ui32ElapseTime;
		}
	}

	gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked = (bRunning == TRUE) ? ui32GSTC : 0x80000000;
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
static UINT8 _FEED_SelectChannel( S_FEED_GROUP_T *psFeedGroup, UINT32 ui32GSTC )
{
	UINT8 		ui8VdcCh_Candidate = 0xFF;		// Invalid Channel Number
	UINT32 		ui32NumOfFreeDQ_Most = 0;
	UINT32 		ui32NumOfActiveDQ_Most = 32;

	UINT8		ui8Ch;
	UINT8		ui8VdcCh;

	UINT32		ui32NumOfActiveAuQ;
	UINT32		ui32NumOfActiveDQ;
	UINT32		ui32NumOfFreeDQ;
	BOOLEAN 	bReadyState[VDEC_NUM_OF_FEED];
	BOOLEAN		bRunning;

	for( ui8Ch = 0; ui8Ch < psFeedGroup->ui32NumOfVdcCh; ui8Ch++ )
	{
		ui8VdcCh = psFeedGroup->ui8VdcCh[ui8Ch];

		_FEED_GetQStatus( ui8VdcCh, TRUE, &ui32NumOfActiveAuQ, &ui32NumOfActiveDQ, &ui32NumOfFreeDQ, &bReadyState[ui8VdcCh], ui32GSTC );

		if( (ui32NumOfActiveAuQ > 0) && (ui32NumOfFreeDQ > 0) )
		{
			if( ui32NumOfFreeDQ > ui32NumOfFreeDQ_Most )
			{
				ui8VdcCh_Candidate = ui8VdcCh;
				ui32NumOfFreeDQ_Most = ui32NumOfFreeDQ;
				ui32NumOfActiveDQ_Most = ui32NumOfActiveDQ;
			}
			else if( ui32NumOfFreeDQ == ui32NumOfFreeDQ_Most )
			{
				if( bReadyState[ui8VdcCh] == TRUE )
				{
					if( bReadyState[ui8VdcCh_Candidate] == FALSE )
					{
						ui8VdcCh_Candidate = ui8VdcCh;
						ui32NumOfActiveDQ_Most = ui32NumOfActiveDQ;
					}
					else if( ui32NumOfActiveDQ > ui32NumOfActiveDQ_Most )
					{
						ui8VdcCh_Candidate = ui8VdcCh;
						ui32NumOfActiveDQ_Most = ui32NumOfActiveDQ;
					}
				}
			}

			bRunning = (bReadyState[ui8VdcCh] == FALSE) ? TRUE : FALSE;
		}
		else
		{
			bRunning = FALSE;
		}

		_FEED_UpdateRunningTime(ui8VdcCh, bRunning, ui32GSTC);

		if( (ui32NumOfActiveAuQ > 0) && (gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime == 0x80000000) )
			gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime = ui32GSTC;

		VDEC_KDRV_Message(DBG_FEED, "[FEED%d][DBG:0x%08X] AuQ(%d):%d, DQ:%d/%d, %s, Candidate:%d, BusyTime:0x%08X, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
									ui8VdcCh, ui32NumOfActiveAuQ,
									ui32NumOfActiveDQ, ui32NumOfFreeDQ,
									(bReadyState[ui8VdcCh] == TRUE) ? "Ready" : "Busy",
									ui8VdcCh_Candidate,
									gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum,
									gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
									__FUNCTION__, __LINE__ );
	}

	if( ui8VdcCh_Candidate != 0xFF )
	{
		VDEC_KDRV_Message(DBG_FEED, "[FEED%d][DBG:0x%08X] AuQ(%d):%d, DQ:%d/%d, %s, BusyTime:0x%08X, SendCmdCnt:%d, %s(%d)", ui8VdcCh_Candidate, ui32GSTC,
									ui8VdcCh_Candidate, gsFeedDb.sCh[ui8VdcCh_Candidate].sStatus.ui32NumOfActiveAuQ,
									gsFeedDb.sCh[ui8VdcCh_Candidate].sStatus.ui32NumOfActiveDQ, gsFeedDb.sCh[ui8VdcCh_Candidate].sStatus.ui32NumOfFreeDQ,
									(bReadyState[ui8VdcCh_Candidate] == TRUE) ? "Ready" : "Busy",
									gsFeedDb.sCh[ui8VdcCh_Candidate].sStatus.ui32BusyTime_Sum,
									gsFeedDb.sCh[ui8VdcCh_Candidate].sDebug.ui32SendCmdCnt,
									__FUNCTION__, __LINE__ );

		if( bReadyState[ui8VdcCh_Candidate] == FALSE )
			ui8VdcCh_Candidate = 0xFF;
	}

	return ui8VdcCh_Candidate;
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
static BOOLEAN _FEED_CheckReset( UINT8 ui8VdcCh, UINT32 ui32BusyTime, UINT32 ui32NumOfActiveAuQ, UINT32 ui32NumOfFreeDQ, UINT32 bReadyState, UINT32 ui32GSTC )
{
	if( ui32BusyTime > 0x7F000000 )
	{
		VDEC_KDRV_Message(ERROR, "[FEED%d][Err:0x%08X] BusyTime:0x%08X(~0x%08X-0x%08X), %s(%d)", ui8VdcCh, ui32GSTC,
									ui32BusyTime, ui32GSTC, gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime,
									__FUNCTION__, __LINE__ );
		return FALSE;
	}

	if( ui32BusyTime > VDEC_MAX_DECODING_TIME )
	{
		if( (ui32NumOfActiveAuQ > 0) &&
			(ui32NumOfFreeDQ > 0) &&
			(bReadyState == FALSE) )
		{
			gsFeedDb._fpFEED_RequestReset(ui8VdcCh);

			VDEC_KDRV_Message(ERROR, "[FEED%d][Err:0x%08X] BusyTime:0x%08X(~0x%08X-0x%08X), AuQ(%d):%d, DQ:F%d, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
										ui32BusyTime, ui32GSTC, gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime,
										ui8VdcCh, ui32NumOfActiveAuQ,
										ui32NumOfFreeDQ,
										gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );

			gsFeedDb.sCh[ui8VdcCh].sDebug.ui32BusyTime_Log = 0x0;
			gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuStartAddr = 0x0;
			gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuEndAddr = 0x0;
			gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent.ui32AuSize = 0x0;

			return TRUE;
		}

		if( (ui32BusyTime - gsFeedDb.sCh[ui8VdcCh].sDebug.ui32BusyTime_Log) > (90000 * 2) )
		{
			VDEC_KDRV_Message(ERROR, "[FEED%d][Wrn:0x%08X] BusyTime:0x%08X(~0x%08X-0x%08X), AuQ(%d):%d, DQ:F%d, %s, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
										ui32BusyTime, ui32GSTC, gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime,
										ui8VdcCh, ui32NumOfActiveAuQ,
										ui32NumOfFreeDQ,
										(bReadyState == TRUE) ? "Ready" : "Busy",
										gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );

			gsFeedDb.sCh[ui8VdcCh].sDebug.ui32BusyTime_Log = ui32BusyTime;
		}
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
static UINT32 _FEED_CheckStarvation( S_FEED_GROUP_T *psFeedGroup, UINT32 ui32GSTC )
{
	UINT8		ui8Ch;
	UINT32		ui32NumOfActiveAuQ;
	UINT32		ui32NumOfActiveDQ;
	UINT32		ui32NumOfFreeDQ;
	BOOLEAN 	bReadyState;
	UINT32		ui32BusyTime_Max = 0;

	for( ui8Ch = 0; ui8Ch < psFeedGroup->ui32NumOfVdcCh; ui8Ch++ )
	{
		UINT8		ui8VdcCh;

		ui8VdcCh = psFeedGroup->ui8VdcCh[ui8Ch];

		_FEED_GetQStatus( ui8VdcCh, FALSE, &ui32NumOfActiveAuQ, &ui32NumOfActiveDQ, &ui32NumOfFreeDQ, &bReadyState, ui32GSTC );

		if( gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime != 0x80000000 )
		{
			if( ui32BusyTime_Max < gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum )
				ui32BusyTime_Max = gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum;

			if( _FEED_CheckReset(ui8VdcCh, gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum, ui32NumOfActiveAuQ, ui32NumOfFreeDQ, bReadyState, ui32GSTC) == TRUE )
			{
				gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime = 0x80000000;
				gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum = 0x0;
				gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked = 0x80000000;
			}
		}

#if 0
		if( (gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ == 0) || (ui32NumOfActiveAuQ == 0) )
		{
			if( (gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ != ui32NumOfActiveAuQ) &&
				(gsFeedDb.sCh[ui8VdcCh].sConfig.bDual == TRUE) &&
				(gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt > 100) )
				VDEC_KDRV_Message(ERROR, "[FEED%d][Err:0x%08X] ElapseTime:0x%08X, AuQ(%d):%d, DQ:%d, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
											gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum,
											ui8VdcCh, ui32NumOfActiveAuQ,
											ui32NumOfActiveDQ,
											gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
											__FUNCTION__, __LINE__ );
		}
#else
		if( (gsFeedDb.sCh[ui8VdcCh].sStatus.bPrevEmpty == FALSE) &&
			(((ui32NumOfActiveAuQ == 0) && (ui32NumOfActiveDQ < 3)) || (ui32NumOfActiveDQ == 0)) &&
			(gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt_Log != gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt) &&
			(gsFeedDb.sCh[ui8VdcCh].sConfig.bDual == FALSE) )
		{
			VDEC_KDRV_Message(WARNING, "[FEED%d][Wrn:0x%08X] ElapseTime:0x%08X, AuQ(%d):%d, DQ:%d, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
										gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum,
										ui8VdcCh, ui32NumOfActiveAuQ,
										ui32NumOfActiveDQ,
										gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );
			gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt_Log = gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt;
		}
#endif
		if( (gsFeedDb.ui32LogTick_Kick % 0x200) == 0 )
		{
			VDEC_KDRV_Message(MONITOR, "[FEED%d][DBG:0x%08X] ElapseTime:0x%08X, AuQ(%d):%d, DQ:%d, SendCmdCnt:%d, %s(%d)", ui8VdcCh, ui32GSTC,
										gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum,
										ui8VdcCh, ui32NumOfActiveAuQ,
										ui32NumOfActiveDQ,
										gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt,
										__FUNCTION__, __LINE__ );
		}

		if( (ui32NumOfActiveAuQ > 0) && (ui32NumOfActiveDQ > 0) )
			gsFeedDb.sCh[ui8VdcCh].sStatus.bPrevEmpty = FALSE;
		else if( (ui32NumOfActiveAuQ == 0) && (ui32NumOfActiveDQ == 0) )
			gsFeedDb.sCh[ui8VdcCh].sStatus.bPrevEmpty = TRUE;
	}

	gsFeedDb.ui32LogTick_Kick++;

	return ui32BusyTime_Max;
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
extern UINT8 FEED_Kick( void )
{
	UINT32			ui32NumOfGroup;
	S_FEED_GROUP_T	sFeedGroup[VDEC_NUM_OF_FEED];
	UINT32 			ui32Group;
	UINT8			ui8KicCnt = 0;
	UINT32			ui32GSTC;

	ui32GSTC = TOP_HAL_GetGSTCC() & 0x7FFFFFFF;

	ui32NumOfGroup = _FEED_GroupChannel(sFeedGroup);

	for( ui32Group = 0; ui32Group < ui32NumOfGroup; ui32Group++ )
	{
		UINT8		ui8VdcCh;

		while( (ui8VdcCh = _FEED_SelectChannel(&sFeedGroup[ui32Group], ui32GSTC)) != 0xFF )
		{
			S_VDC_AU_T			sVdcAu, sVdcAu_Next;
			BOOLEAN				bNeedMore;

FEED_Kick_More :
			bNeedMore = FALSE;

			if( AU_Q_Pop( ui8VdcCh, &sVdcAu ) == TRUE )
			{
				S_VDC_AU_T			*pVdcAu_Next;

				gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ--;
				gsFeedDb.sCh[ui8VdcCh].sDebug.sVdcAu_Sent = sVdcAu;

				if( AU_Q_Peep( ui8VdcCh, &sVdcAu_Next ) == TRUE )
					pVdcAu_Next = &sVdcAu_Next;
				else
					pVdcAu_Next = NULL;

				if( gsFeedDb._fpFEED_SendCmd(ui8VdcCh, &sVdcAu, pVdcAu_Next, &bNeedMore, ui32GSTC,
								gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveAuQ,
								gsFeedDb.sCh[ui8VdcCh].sStatus.ui32NumOfActiveDQ) == TRUE )
				{
					gsFeedDb.sCh[ui8VdcCh].sStatus.ui32SentTime = ui32GSTC;
					gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Sum = 0x0;
					gsFeedDb.sCh[ui8VdcCh].sStatus.ui32BusyTime_Checked = 0x80000000;
					gsFeedDb.sCh[ui8VdcCh].sDebug.ui32SendCmdCnt++;
					ui8KicCnt++;

					if( bNeedMore == TRUE )
						goto FEED_Kick_More;
				}
				else
				{
					VDEC_KDRV_Message(ERROR, "[FEED%d][Err] SendCmd Failed - AU Type:%d, %s", ui8VdcCh, sVdcAu.eAuType, __FUNCTION__ );
				}
			}
			else
			{
				VDEC_KDRV_Message(WARNING, "[FEED%d][Err] AUIB(%d) Pop(%d), %s", ui8VdcCh,
										ui8VdcCh, AU_Q_NumActive( ui8VdcCh ),
										__FUNCTION__ );
			}
		}
	}

	for( ui32Group = 0; ui32Group < ui32NumOfGroup; ui32Group++ )
	{
		_FEED_CheckStarvation(&sFeedGroup[ui32Group], ui32GSTC);
	}

	return ui8KicCnt;
}


