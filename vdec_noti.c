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
 *  Notification module of Video Decoder.
 *  Notifies event to user layer from kernel driver.
 *
 * author     youngki.lyu@lge.com
 * version    1.0
 * date       2011.02.21
 *
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/
#define _VDEC_NOTI_C_	1

/*
 * debug point. and mask.
 *
 * 0x0001 VDEC_NOTI_Alloc(UINT8 ch, UINT32 length);
 * 0x0002 VDEC_NOTI_Free(UINT8 ch );
 * 0x0004 VDEC_NOTI_GarbageCollect(UINT8 ch);
 * 0x0008 VDEC_NOTI_SetMask(UINT8 ch, UINT32 mskNotify);
 * 0x0010 VDEC_NOTI_GetMask(UINT8 ch);
 * 0x0020 VDEC_NOTI_CopyToUser(UINT8 ch, char __user *buffer, size_t buffer_size);
 */
#define VDEC_NOTI_DEBUG 0

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include "os_util.h"
#include "vdec_drv.h"
#include "vdec_kapi.h"
#include "vdec_io.h"
//#include "vdec_fb.h"
#include "vdec_noti.h"

#include "mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define check_one_bit(data, loc)			((0x1) & (data >> (loc)))

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#ifdef VDEC_NOTI_DEBUG
#define VDEC_NOTI_PRINT		VDEC_PRINT
#else
#define VDEC_NOTI_PRINT
#endif

/**
 * Duplication check method.
 *
 * @def _NOTI_A_QUEUE_FOR_A_PROCESS
 * allocate a queue for a process, and notification should be done in one thread for given process.
 */
#define _NOTI_A_QUEUE_FOR_A_PROCESS(_pNoti, _pTsk)	(_pNoti->tgid ==  task_tgid_nr(_pTsk))

/**
 * Duplication check method.
 *
 */
#define IS_SAME_CHECK(_pNoti, _pTsk)				_NOTI_A_QUEUE_FOR_A_PROCESS(_pNoti, _pTsk)

#define CROP_CHECK(_Codecsize, _Cropsize0, _Cropsize1) (((_Cropsize0 == 0x0) && (_Cropsize1 == 0x0)) ? (_Codecsize) : ((_Codecsize) - (_Cropsize0 + _Cropsize1)))

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
/**
 *  per process, per channel, queue.
 *
 *  stored from interrupt context,( ! USE_NOTIFY_WORKQ )
 *           or task context( USE_NOTIFY_WORKQ )
 *
 *  retrieved from task context.
 *
 */
struct VDEC_KDRV_NOTI_QUEUE
{
	struct list_head
			noti_list;		///< list member.
	spinlock_t	lock;		///< lock for read & write rofs/wofs


	UINT32		mskNotify;	///< notify Mask. see VDEC_KDRV_NOTIFY_MSK_T
	pid_t		tgid;		///< task group id for a process.

	#define	VDEC_KDRV_NOTI_QUEUE_FLAG_ENABLED		0x01
	#define VDEC_KDRV_NOTI_QUEUE_FLAG_OVERFLOW	0x02

	UINT32		length;		///< length of buffer in number of unit.
	UINT32		rofs;		///< read  offset.

							// above : mainly written from task layer.
							// below : mainly written from interrupt layer.

	UINT32		wofs;		///< write offset. (updated interrupt context.)
	UINT32		flag;		///< flag enabled or not is enabled via VDEC_KDRV_IO_SET_NOTIFY

	char		*buffer;	///< notification buffer.
};

typedef struct VDEC_KDRV_NOTI_QUEUE VDEC_KDRV_NOTI_QUEUE_T;

/**
 * Notification header structure.
 * @var lock lock for list manipulation.(list add entry/delete entry/traverse)
 */
typedef struct
{
	struct list_head	noti_list;
	spinlock_t	 		lock;
	wait_queue_head_t	wq;

	UINT32		mskNotifyToCheck;	///< bit-wise OR'ed notify mask for every client process.
} VDEC_KDRV_NOTI_HEAD_T;

typedef struct VDEC_KDRV_NOTI_NODE
{
	VDEC_KDRV_NOTI_INFO_T	info;

	struct VDEC_KDRV_NOTI_NODE *next;
} VDEC_KDRV_NOTI_NODE_T;

/**
 * initialize VDEC_KDRV_NOTI_HEAD_T.
 * with SPIN_LOCK_UNLOCKED, noti_list to be Empty.
 */
static inline void INIT_NOTI_HEAD(VDEC_KDRV_NOTI_HEAD_T *noti_head)
{
	noti_head->lock = __SPIN_LOCK_UNLOCKED(noti_head->lock);
	INIT_LIST_HEAD(&noti_head->noti_list);
	init_waitqueue_head(&noti_head->wq);
	noti_head->mskNotifyToCheck = 0;
}

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
VDEC_KDRV_OUTPUT_T			stVdecOutput[MAX_NUM_INSTANCE];

static struct
{
	struct
	{
		wait_queue_head_t		waitqueue;
		spinlock_t				spinlock;
	} sCtrl;
	struct
	{
		BOOLEAN			bRegInputCb;
		BOOLEAN			bRegFeedCb;
		BOOLEAN			bRegDecCb;
		BOOLEAN			bRegDispCb;
	} sConfig;
	struct
	{
		VDEC_KDRV_NOTI_NODE_T		*pHead;
		UINT32						ui32Count;
	} sList;
	struct
	{
		BOOLEAN			bUpdateInput;
		VDEC_KDRV_NOTIFY_PARAM_ES_T	input;
		BOOLEAN			bUpdateFeed;
		VDEC_KDRV_NOTIFY_PARAM_ES_T	feed;

		struct
		{
			BOOLEAN			bUpdate;
			UINT32			ui32uID;
			UINT64			ui64TimeStamp;
//			UINT32			ui32DTS;
		} sDec;

		BOOLEAN			bUpdateSeqh;
		VDEC_KDRV_NOTIFY_PARAM_SEQH_T	seqh;
		BOOLEAN			bUpdatePicd;
		VDEC_KDRV_NOTIFY_PARAM_PICD_T	picd;

		struct
		{
			BOOLEAN			bUpdate;
			UINT32			ui32uID;
			UINT64			ui64TimeStamp;
			UINT32			ui32PTS;
		} sDisp;

		BOOLEAN			bUpdateDisp;
		VDEC_KDRV_NOTIFY_PARAM_DISP_T	disp;
	} sLatest;
} stVdecNoti[VDEC_KDRV_CH_MAXN];


/**
 * Notify List Header for each channel.
 */
static VDEC_KDRV_NOTI_HEAD_T	sNotifyHead[VDEC_KDRV_CH_MAXN];

/*========================================================================================
	Implementation Group
========================================================================================*/

static void _VDEC_NOTI_SleepOn(UINT8 ui8Ch)
{
//	VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] Sleep Callback Thread, %s", ui8Ch, __FUNCTION__);
	interruptible_sleep_on(&stVdecNoti[ui8Ch].sCtrl.waitqueue);
}

static void _VDEC_NOTI_WakeUp(UINT8 ui8Ch)
{
	wake_up_interruptible(&stVdecNoti[ui8Ch].sCtrl.waitqueue);
//	VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] Wake up Callback Thread, %s", ui8Ch, __FUNCTION__);
}

void VDEC_NOTI_Init(void)
{
	UINT8	ui8Ch;

	for ( ui8Ch = 0; ui8Ch < VDEC_KDRV_CH_MAXN; ui8Ch++)
	{
		init_waitqueue_head(&stVdecNoti[ui8Ch].sCtrl.waitqueue);
		spin_lock_init(&stVdecNoti[ui8Ch].sCtrl.spinlock);
		stVdecNoti[ui8Ch].sConfig.bRegInputCb = FALSE;
		stVdecNoti[ui8Ch].sConfig.bRegFeedCb = FALSE;
		stVdecNoti[ui8Ch].sConfig.bRegDecCb = FALSE;
		stVdecNoti[ui8Ch].sConfig.bRegDispCb = FALSE;
		stVdecNoti[ui8Ch].sList.pHead = NULL;
		stVdecNoti[ui8Ch].sList.ui32Count = 0;

		stVdecNoti[ui8Ch].sLatest.sDec.bUpdate = FALSE;
		stVdecNoti[ui8Ch].sLatest.bUpdateSeqh = FALSE;
		stVdecNoti[ui8Ch].sLatest.bUpdatePicd = FALSE;
		stVdecNoti[ui8Ch].sLatest.sDisp.bUpdate = FALSE;
		stVdecNoti[ui8Ch].sLatest.bUpdateDisp = FALSE;
	}

	for ( ui8Ch = 0; ui8Ch < VDEC_KDRV_CH_MAXN; ui8Ch++)
	{
		INIT_NOTI_HEAD(&sNotifyHead[ui8Ch]);
		memset((void*)&stVdecOutput[ui8Ch], 0x0, sizeof(VDEC_KDRV_OUTPUT_T));
	}
}

static BOOLEAN _VDEC_NOTI_UpdateLatest( UINT8 ui8Ch, VDEC_KDRV_NOTI_NODE_T *pNode )
{
	BOOLEAN		bUpdateFirst = FALSE;

	if( pNode == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[NOTI%d] Invalid Pointer, %s", ui8Ch, __FUNCTION__);
		return FALSE;
	}

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	switch( pNode->info.id )
	{
	case VDEC_KDRV_NOTI_ID_INPUT :
		stVdecNoti[ui8Ch].sLatest.bUpdateInput = TRUE;
		stVdecNoti[ui8Ch].sLatest.input = pNode->info.u.input;
		break;
	case VDEC_KDRV_NOTI_ID_FEED :
		stVdecNoti[ui8Ch].sLatest.bUpdateFeed = TRUE;
		stVdecNoti[ui8Ch].sLatest.feed = pNode->info.u.feed;
		break;
	case VDEC_KDRV_NOTI_ID_DEC :
		switch( pNode->info.u.dec.id )
		{
		case VDEC_KDRV_NOTI_DEC_ID_SEQH :
			stVdecNoti[ui8Ch].sLatest.sDec.bUpdate = TRUE;
			stVdecNoti[ui8Ch].sLatest.sDec.ui32uID = pNode->info.u.dec.u.seqh.ui32uID;

			stVdecNoti[ui8Ch].sLatest.seqh = pNode->info.u.dec.u.seqh;
			if( stVdecNoti[ui8Ch].sLatest.bUpdateSeqh == FALSE )
			{
				VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] SEQH First Update - %s", ui8Ch, (pNode->info.u.dec.u.seqh.bFailed) ? "Fail" : "Success");
				bUpdateFirst = TRUE;
			}
			stVdecNoti[ui8Ch].sLatest.bUpdateSeqh = TRUE;
			break;
		case VDEC_KDRV_NOTI_DEC_ID_PICD :
			stVdecNoti[ui8Ch].sLatest.sDec.bUpdate = TRUE;
			stVdecNoti[ui8Ch].sLatest.sDec.ui32uID = pNode->info.u.dec.u.picd.sPTS.ui32uID;
			stVdecNoti[ui8Ch].sLatest.sDec.ui64TimeStamp = pNode->info.u.dec.u.picd.sPTS.ui64TimeStamp;

			stVdecNoti[ui8Ch].sLatest.picd = pNode->info.u.dec.u.picd;
			if( stVdecNoti[ui8Ch].sLatest.bUpdatePicd == FALSE )
			{
				VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] PICD First Update - DPB:0x%X", ui8Ch, (UINT32)pNode->info.u.dec.u.picd.pY);
				bUpdateFirst = TRUE;
			}
			stVdecNoti[ui8Ch].sLatest.bUpdatePicd = TRUE;
			break;
		default :
			VDEC_KDRV_Message(ERROR, "[NOTI%d] Decode Node ID: %d, %s", ui8Ch, pNode->info.id, __FUNCTION__);
			break;
		}
		break;
	case VDEC_KDRV_NOTI_ID_DISP :
		stVdecNoti[ui8Ch].sLatest.sDisp.bUpdate = TRUE;
		stVdecNoti[ui8Ch].sLatest.sDisp.ui32uID = pNode->info.u.disp.ui32uID;
		stVdecNoti[ui8Ch].sLatest.sDisp.ui64TimeStamp = pNode->info.u.disp.ui64TimeStamp;
		stVdecNoti[ui8Ch].sLatest.sDisp.ui32PTS = pNode->info.u.disp.ui32PTS;

		stVdecNoti[ui8Ch].sLatest.disp = pNode->info.u.disp;
		if( stVdecNoti[ui8Ch].sLatest.bUpdateDisp == FALSE )
		{
			VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] DISP First Update - DQ Occupancy:%d", ui8Ch, pNode->info.u.disp.ui32DqOccupancy);
			bUpdateFirst = TRUE;
		}
		stVdecNoti[ui8Ch].sLatest.bUpdateDisp = TRUE;
		break;
	default :
		VDEC_KDRV_Message(ERROR, "[NOTI%d] Node ID: %d, %s", ui8Ch, pNode->info.id, __FUNCTION__);
		break;
	}

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return bUpdateFirst;
}

static BOOLEAN _VDEC_NOTI_PushNode( UINT8 ui8Ch, VDEC_KDRV_NOTI_NODE_T *pNode )
{
	BOOLEAN		bUpdateOne = FALSE;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	pNode->next = NULL;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	if( stVdecNoti[ui8Ch].sList.pHead == NULL )
	{
		stVdecNoti[ui8Ch].sList.pHead = pNode;
		bUpdateOne = TRUE;
	}
	else
	{
		VDEC_KDRV_NOTI_NODE_T	*pHead;

		pHead = stVdecNoti[ui8Ch].sList.pHead;
		while( pHead->next != NULL )
		{
			pHead = pHead->next;
		}
		pHead->next = pNode;
	}

	stVdecNoti[ui8Ch].sList.ui32Count++;
	if( (stVdecNoti[ui8Ch].sList.ui32Count % 20) == 0 )
		VDEC_KDRV_Message(ERROR, "[NOTI%d][DBG] List Count: %d, %s", ui8Ch, stVdecNoti[ui8Ch].sList.ui32Count, __FUNCTION__);

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return bUpdateOne;
}

static VDEC_KDRV_NOTI_NODE_T *_VDEC_NOTI_PopNode( UINT8 ui8Ch )
{
	VDEC_KDRV_NOTI_NODE_T	*pNode = NULL;
	VDEC_KDRV_NOTI_NODE_T	*pHead;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return NULL;
	}

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	pHead = stVdecNoti[ui8Ch].sList.pHead;

	pNode = pHead;

	if( pHead != NULL )
	{
		stVdecNoti[ui8Ch].sList.pHead = pHead->next;
	}

	if( pNode != NULL )
	{
		pNode->next = NULL;

		stVdecNoti[ui8Ch].sList.ui32Count--;
		if( (stVdecNoti[ui8Ch].sList.ui32Count % 40) == 19 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][DBG] List Count: %d, %s", ui8Ch, stVdecNoti[ui8Ch].sList.ui32Count, __FUNCTION__);
	}

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return pNode;
}

void VDEC_NOTI_EnableCallback( UINT8 ui8Ch, BOOLEAN bInput, BOOLEAN bFeed, BOOLEAN bDec, BOOLEAN bDisp )
{
	BOOLEAN		bOldNone, bNewNone;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return;
	}

	VDEC_KDRV_Message(_TRACE, "[NOTI%d][DBG] Enable - %d:%d, %s ", ui8Ch, bDec, bDisp, __FUNCTION__ );

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	if( (stVdecNoti[ui8Ch].sConfig.bRegInputCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegFeedCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegDecCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegDispCb == FALSE) )
	{
		bOldNone = TRUE;
	}
	else
	{
		bOldNone = FALSE;
	}

	if( (bDec == FALSE) &&
		(bDisp == FALSE) )
	{
		bNewNone = TRUE;
	}
	else
	{
		bNewNone = FALSE;
	}

	if( (bOldNone == TRUE) && (bNewNone == FALSE) )
	{ // Open
		VDEC_KDRV_Message(DBG_SYS, "[NOTI%d][DBG] Open(PID:%d/TGID:%d) - %d:%d:%d:%d --> %d:%d:%d:%d, %s ", ui8Ch, get_current()->pid, get_current()->tgid,
									stVdecNoti[ui8Ch].sConfig.bRegInputCb, stVdecNoti[ui8Ch].sConfig.bRegFeedCb, stVdecNoti[ui8Ch].sConfig.bRegDecCb, stVdecNoti[ui8Ch].sConfig.bRegDispCb,
									bInput, bFeed, bDec, bDisp, __FUNCTION__ );
	}
	if( bNewNone == TRUE )
	{ // Close
		VDEC_KDRV_Message(DBG_SYS, "[NOTI%d][DBG] Close(PID:%d/TGID:%d) - %d:%d:%d:%d --> %d:%d:%d:%d, %s ", ui8Ch, get_current()->pid, get_current()->tgid,
									stVdecNoti[ui8Ch].sConfig.bRegInputCb, stVdecNoti[ui8Ch].sConfig.bRegFeedCb, stVdecNoti[ui8Ch].sConfig.bRegDecCb, stVdecNoti[ui8Ch].sConfig.bRegDispCb,
									bInput, bFeed, bDec, bDisp, __FUNCTION__ );
	}

	stVdecNoti[ui8Ch].sConfig.bRegInputCb = bInput;
	stVdecNoti[ui8Ch].sConfig.bRegFeedCb = bFeed;
	stVdecNoti[ui8Ch].sConfig.bRegDecCb = bDec;
	stVdecNoti[ui8Ch].sConfig.bRegDispCb = bDisp;

	if( (bOldNone == TRUE) && (bNewNone == FALSE) )
	{ // Open
		stVdecNoti[ui8Ch].sLatest.bUpdateInput = FALSE;
		memset((void*)&stVdecNoti[ui8Ch].sLatest.input, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_ES_T));
		stVdecNoti[ui8Ch].sLatest.bUpdateFeed = FALSE;
		memset((void*)&stVdecNoti[ui8Ch].sLatest.feed, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_ES_T));
		stVdecNoti[ui8Ch].sLatest.bUpdateSeqh = FALSE;
		memset((void*)&stVdecNoti[ui8Ch].sLatest.seqh, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_SEQH_T));
		stVdecNoti[ui8Ch].sLatest.bUpdatePicd = FALSE;
		memset((void*)&stVdecNoti[ui8Ch].sLatest.picd, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_PICD_T));
		stVdecNoti[ui8Ch].sLatest.bUpdateDisp = FALSE;
		memset((void*)&stVdecNoti[ui8Ch].sLatest.disp, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_DISP_T));
	}

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	if( bNewNone == TRUE )
	{ // Close
		VDEC_KDRV_NOTI_NODE_T *pNode = NULL;

		while( (pNode = _VDEC_NOTI_PopNode(ui8Ch)) != NULL )
		{
			OS_KFree( pNode );
		}

		_VDEC_NOTI_WakeUp(ui8Ch);
	}

}

BOOLEAN VDEC_NOTI_GetCbInfo( UINT8 ui8Ch, VDEC_KDRV_NOTI_INFO_T *pInfo )
{
	VDEC_KDRV_NOTI_NODE_T *pNode = NULL;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( (stVdecNoti[ui8Ch].sConfig.bRegInputCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegFeedCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegDecCb == FALSE) &&
		(stVdecNoti[ui8Ch].sConfig.bRegDispCb == FALSE) )
	{
		VDEC_KDRV_Message(ERROR, "[NOTI%d][Warning] No Registered Callback Function, %s ", ui8Ch, __FUNCTION__ );
		return FALSE;
	}

	pNode = _VDEC_NOTI_PopNode(ui8Ch);
	if( pNode == NULL )
	{
//		VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] Sleep Callback Thread(PID:%d/TGID:%d), %s", ui8Ch, get_current()->pid, get_current()->tgid, __FUNCTION__);
		_VDEC_NOTI_SleepOn(ui8Ch);
//		VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] Wake up Callback Thread(PID:%d/TGID:%d), %s", ui8Ch, get_current()->pid, get_current()->tgid, __FUNCTION__);

		pNode = _VDEC_NOTI_PopNode(ui8Ch);
	}

	if( pNode == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] pNode == NULL(PID:%d/TGID:%d), %s ", ui8Ch, get_current()->pid, get_current()->tgid, __FUNCTION__ );
		return FALSE;
	}

	*pInfo = pNode->info;

	OS_KFree( pNode );

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetInput( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_ES_T *pInput )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.bUpdateFeed == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.bUpdateFeed, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pInput = stVdecNoti[ui8Ch].sLatest.input;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetFeed( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_ES_T *pFeed )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.bUpdateFeed == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.bUpdateFeed, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pFeed = stVdecNoti[ui8Ch].sLatest.feed;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetSeqh( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_SEQH_T *pSeqh )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.bUpdateSeqh == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.bUpdateSeqh, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pSeqh = stVdecNoti[ui8Ch].sLatest.seqh;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetPicd( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_PICD_T *pPicd )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.bUpdatePicd == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.bUpdatePicd, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pPicd = stVdecNoti[ui8Ch].sLatest.picd;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetDisp( UINT8 ui8Ch, VDEC_KDRV_NOTIFY_PARAM_DISP_T *pDisp )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.bUpdateDisp == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.bUpdateDisp, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pDisp = stVdecNoti[ui8Ch].sLatest.disp;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetLatestDecUID( UINT8 ui8Ch, UINT32 *pui32UID )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.sDec.bUpdate == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.sDec.bUpdate, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pui32UID = stVdecNoti[ui8Ch].sLatest.sDec.ui32uID;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

BOOLEAN VDEC_NOTI_GetLatestDispUID( UINT8 ui8Ch, UINT32 *pui32UID )
{
	static UINT32		ui32LogTick = 0x0;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return FALSE;
	}

	if( stVdecNoti[ui8Ch].sLatest.sDisp.bUpdate == FALSE )
	{
		if( (ui32LogTick % 10) == 0 )
			VDEC_KDRV_Message(ERROR, "[NOTI%d][ERR] Update: %d, %s ", ui8Ch, stVdecNoti[ui8Ch].sLatest.sDisp.bUpdate, __FUNCTION__ );
		ui32LogTick++;

		return FALSE;
	}
	ui32LogTick = 0;

	spin_lock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	*pui32UID = stVdecNoti[ui8Ch].sLatest.sDisp.ui32uID;

	spin_unlock(&stVdecNoti[ui8Ch].sCtrl.spinlock);

	return TRUE;
}

/**
 * get decoded information.
 *
 * @param ch [IN]	from which channel to retrive decoded information.
 *
 * @return pointer to saved VDEC_KDRV_OUTPUT_T structure.
 *
 * @see VDEC_KDRV_OUTPUT_T
 */
VDEC_KDRV_OUTPUT_T *VDEC_NOTI_GetOutput( UINT32 ch )
{
	return &stVdecOutput[ch];
}


void VDEC_NOTI_UpdateInputInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_ESINFO_T *pInputInfo)
{
	static UINT32		ui32LogTick = 0x40;
	VDEC_KDRV_NOTI_NODE_T	*pNode = NULL;
	VDEC_KDRV_NOTIFY_PARAM_ES_T	*pVdecInput;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return;
	}

	pNode = OS_KMalloc( sizeof ( VDEC_KDRV_NOTI_NODE_T ) );

	pNode->info.id = VDEC_KDRV_NOTI_ID_INPUT;
	pVdecInput = &pNode->info.u.input;

 	pVdecInput->bRet 			= pInputInfo->bRet;
	pVdecInput->ui32AuType	= pInputInfo->ui32AuType;
	pVdecInput->ui32uID		= pInputInfo->ui32uID;
	pVdecInput->ui64TimeStamp	= pInputInfo->ui64TimeStamp;
	pVdecInput->ui32DTS 		= pInputInfo->ui32DTS;
	pVdecInput->ui32PTS		= pInputInfo->ui32PTS;
	pVdecInput->ui32AuqOccupancy	= pInputInfo->ui32AuqOccupancy;
	pVdecInput->ui32CpbOccupancy	= pInputInfo->ui32CpbOccupancy;
	pVdecInput->ui32CpbSize	= pInputInfo->ui32CpbSize;

	_VDEC_NOTI_UpdateLatest(ui8Ch, pNode);

	if( stVdecNoti[ui8Ch].sConfig.bRegDispCb == TRUE )
	{
		if( _VDEC_NOTI_PushNode(ui8Ch, pNode) == TRUE );
			_VDEC_NOTI_WakeUp(ui8Ch);
	}
	else
	{
		OS_KFree( pNode );
	}

	if( ui32LogTick == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[NOTI%d] Feed - Time Stamp: %llu, UID: %d", ui8Ch, pInputInfo->ui64TimeStamp,  pInputInfo->ui32uID);
		ui32LogTick = 0x0;
	}
	ui32LogTick++;
}

void VDEC_NOTI_UpdateFeedInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_ESINFO_T *pFeedInfo)
{
	static UINT32		ui32LogTick = 0x40;
	VDEC_KDRV_NOTI_NODE_T	*pNode = NULL;
	VDEC_KDRV_NOTIFY_PARAM_ES_T	*pVdecFeed;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return;
	}

	pNode = OS_KMalloc( sizeof ( VDEC_KDRV_NOTI_NODE_T ) );

	pNode->info.id = VDEC_KDRV_NOTI_ID_FEED;
	pVdecFeed = &pNode->info.u.feed;

 	pVdecFeed->bRet 			= pFeedInfo->bRet;
	pVdecFeed->ui32AuType	= pFeedInfo->ui32AuType;
	pVdecFeed->ui32uID		= pFeedInfo->ui32uID;
	pVdecFeed->ui64TimeStamp	= pFeedInfo->ui64TimeStamp;
	pVdecFeed->ui32DTS 		= pFeedInfo->ui32DTS;
	pVdecFeed->ui32PTS		= pFeedInfo->ui32PTS;
	pVdecFeed->ui32AuqOccupancy	= pFeedInfo->ui32AuqOccupancy;
	pVdecFeed->ui32CpbOccupancy	= pFeedInfo->ui32CpbOccupancy;
	pVdecFeed->ui32CpbSize	= pFeedInfo->ui32CpbSize;

	_VDEC_NOTI_UpdateLatest(ui8Ch, pNode);

	if( stVdecNoti[ui8Ch].sConfig.bRegDispCb == TRUE )
	{
		if( _VDEC_NOTI_PushNode(ui8Ch, pNode) == TRUE );
			_VDEC_NOTI_WakeUp(ui8Ch);
	}
	else
	{
		OS_KFree( pNode );
	}

	if( ui32LogTick == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[NOTI%d] Feed - Time Stamp: %llu, UID: %d", ui8Ch, pFeedInfo->ui64TimeStamp,  pFeedInfo->ui32uID);
		ui32LogTick = 0x0;
	}
	ui32LogTick++;
}

/**
 * Save decoded information to static notification buffer.
 *
 * actually collect information and build VDEC_KDRV_NOTIFY_PARAM_PICD_T/VDEC_KDRV_NOTIFY_PARAM_USRD_T/VDEC_KDRV_NOTIFY_PARAM_SEQH_T.
 *
 * @param ch [IN]	to which channel to save decoded information.
 */
void VDEC_NOTI_UpdateDecodeInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_DECINFO_T *pDecInfo)
{
	static UINT32		ui32LogTick = 0x0;
	VDEC_KDRV_NOTI_NODE_T	*pNode = NULL;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return;
	}

	pNode = OS_KMalloc( sizeof ( VDEC_KDRV_NOTI_NODE_T ) );
	pNode->info.id = VDEC_KDRV_NOTI_ID_DEC;

	if(pDecInfo->eCmdHdr == KDRV_NOTI_DECINFO_UPDATE_HDR_SEQINFO)
	{
		VDEC_KDRV_NOTIFY_PARAM_SEQH_T	*pVdecSeqh;

		pNode->info.u.dec.id = VDEC_KDRV_NOTI_DEC_ID_SEQH;
		pVdecSeqh = &pNode->info.u.dec.u.seqh;

		pVdecSeqh->bGMC = 0;
		if(check_one_bit(pDecInfo->u.seqInfo.u32IsSuccess, 0) == 0x1)
		{
			pVdecSeqh->bFailed = 0;
			pVdecSeqh->ui32Profile = pDecInfo->u.seqInfo.u32Profile;
			pVdecSeqh->ui32Level = pDecInfo->u.seqInfo.u32Level;
			pVdecSeqh->ui32PicWidth = pDecInfo->u.seqInfo.u32PicWidth;
			pVdecSeqh->ui32PicHeight = pDecInfo->u.seqInfo.u32PicHeight;
			pVdecSeqh->ui32RefFrameCount = pDecInfo->u.seqInfo.ui32RefFrameCount;
//			pVdecSeqh->ui32ProgSeq = pDecInfo->u.seqInfo.ui32ProgSeq;
//			pVdecSeqh->ui32FrameRateRes = pDecInfo->u.seqInfo.ui32FrameRateRes;
//			pVdecSeqh->ui32FrameRateDiv = pDecInfo->u.seqInfo.ui32FrameRateDiv;
		}
		else
		{
			pVdecSeqh->bFailed = 1;
			pVdecSeqh->ui32Profile = 0;
			pVdecSeqh->ui32Level = 0;
			pVdecSeqh->ui32PicWidth = 0;
			pVdecSeqh->ui32PicHeight = 0;
			pVdecSeqh->ui32RefFrameCount = 0;
			if( (VDEC_GetVdecStdType(pDecInfo->u32CodecType_Config) == 3/*MP4_DIVX3_DEC*/)
				&& (check_one_bit(pDecInfo->u.seqInfo.u32IsSuccess, 18) == 0x1))
			{
				pVdecSeqh->bGMC = 1;
			}
		}
		pVdecSeqh->ui32uID = pDecInfo->u.seqInfo.ui32uID;

		VDEC_NOTI_SaveSEQH(ui8Ch, pVdecSeqh);

		VDEC_KDRV_Message(DBG_SYS, "[NOTI%d] SEQ Docode - Failed:%d, Profile:%d, Level:%d", ui8Ch, pVdecSeqh->bFailed,  pVdecSeqh->ui32Profile,  pVdecSeqh->ui32Level);
	}
	else if(pDecInfo->eCmdHdr == KDRV_NOTI_DECINFO_UPDATE_HDR_PICINFO)
	{
		UINT8 ui8FrameNum;
		UINT32	ui32PicWidth;
		UINT32	ui32PicHeight;

		VDEC_KDRV_NOTIFY_PARAM_PICD_T	*pVdecPicd;

		pNode->info.u.dec.id = VDEC_KDRV_NOTI_DEC_ID_PICD;
		pVdecPicd = &pNode->info.u.dec.u.picd;

		if(pDecInfo->u.picInfo.u32DecodingSuccess)
			pVdecPicd->bFail = 0;
		else
			pVdecPicd->bFail = 1;

		//stored decoded frame info
		pVdecPicd->bGotAnchor = 1;		//	 check seq init??

		// if frame index is strange, then map to 0 (first dpb
		// TODO : Boda has special frame index value of 255 & 254, how to report them. T.T
		ui8FrameNum = 8;

		pVdecPicd->ui32BufStride		= pDecInfo->u.picInfo.u32BufStride;
		pVdecPicd->ui32PicWidth			= pDecInfo->u.picInfo.u32PicWidth;
		pVdecPicd->ui32PicHeight		= pDecInfo->u.picInfo.u32PicHeight;

		pVdecPicd->ui32CropLeftOffset		= pDecInfo->u.picInfo.u32CropLeftOffset;
		pVdecPicd->ui32CropRightOffset		= pDecInfo->u.picInfo.u32CropRightOffset;
		pVdecPicd->ui32CropTopOffset		= pDecInfo->u.picInfo.u32CropTopOffset;
		pVdecPicd->ui32CropBottomOffset		= pDecInfo->u.picInfo.u32CropBottomOffset;

		pVdecPicd->si32FrameFd	= pDecInfo->u.picInfo.si32FrameFd;
		pVdecPicd->pY			= (UINT32 *)pDecInfo->u.picInfo.ui32YFrameAddr;
		pVdecPicd->pCb			= (UINT32 *)pDecInfo->u.picInfo.ui32CFrameAddr;
		pVdecPicd->pCr			= NULL;

		ui32PicWidth = CROP_CHECK(pDecInfo->u.picInfo.u32PicWidth, pVdecPicd->ui32CropLeftOffset, pVdecPicd->ui32CropRightOffset);
		ui32PicHeight = CROP_CHECK(pDecInfo->u.picInfo.u32PicHeight, pVdecPicd->ui32CropTopOffset, pVdecPicd->ui32CropBottomOffset);

		if(ui32PicWidth && ui32PicHeight)
		{
			pVdecPicd->picErr		= (pDecInfo->u.picInfo.u32NumOfErrMBs*256*100)/(ui32PicWidth*ui32PicHeight );
		}

		pVdecPicd->m.picType		= pDecInfo->u.picInfo.u32PicType;
		pVdecPicd->m.interlSeq	= pDecInfo->u.picInfo.u32ProgSeq ? 0 : 1;
		pVdecPicd->m.interlFrm	= pDecInfo->u.picInfo.u32ProgFrame ? 0: 1;

		if(pDecInfo->u.picInfo.si32FrmPackArrSei >= 0)
		{
			pVdecPicd->m.avc3DFpaValid	= 1;
			pVdecPicd->m.avc3DFpaType	= pDecInfo->u.picInfo.si32FrmPackArrSei;
		}
		else if(pDecInfo->u.picInfo.si32FrmPackArrSei == -1)
		{
			pVdecPicd->m.avc3DFpaValid	= 0;
		}

		pVdecPicd->m.frameRateRes	= pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Residual;
		pVdecPicd->m.frameRateDiv	= pDecInfo->u.picInfo.sFrameRate_Decoder.ui32Divider;
		pVdecPicd->m.sar			= pDecInfo->u.picInfo.u32Aspectratio;

		pVdecPicd->bLowDelay		= pDecInfo->u.picInfo.u32LowDelay;
		pVdecPicd->sDTS.ui64TimeStamp	= pDecInfo->u.picInfo.sDTS.ui64TimeStamp;
		pVdecPicd->sDTS.ui32DTS		= pDecInfo->u.picInfo.sDTS.ui32DTS;
		pVdecPicd->sDTS.ui32uID		= pDecInfo->u.picInfo.sDTS.ui32uID;
		pVdecPicd->sPTS.ui64TimeStamp	= pDecInfo->u.picInfo.sPTS.ui64TimeStamp;
		pVdecPicd->sPTS.ui32PTS		= pDecInfo->u.picInfo.sPTS.ui32PTS;
		pVdecPicd->sPTS.ui32uID		= pDecInfo->u.picInfo.sPTS.ui32uID;
		pVdecPicd->ui32DqOccupancy		= pDecInfo->u.picInfo.ui32DqOccupancy;

		if( pDecInfo->u.picInfo.sUsrData.ui32Size )
		{
			pVdecPicd->sUsrData.ui32PicType 	= pDecInfo->u.picInfo.sUsrData.ui32PicType;
			pVdecPicd->sUsrData.ui32Rpt_ff 		= pDecInfo->u.picInfo.sUsrData.ui32Rpt_ff;
			pVdecPicd->sUsrData.ui32Top_ff 		= pDecInfo->u.picInfo.sUsrData.ui32Top_ff;
			pVdecPicd->sUsrData.ui32BuffAddr 	= pDecInfo->u.picInfo.sUsrData.ui32BuffAddr;
			pVdecPicd->sUsrData.ui32Size 		= pDecInfo->u.picInfo.sUsrData.ui32Size;

			if( VDEC_KDRV_NOTIFY_MSK_USRD & VDEC_NOTI_GetMaskToCheck(ui8Ch) )
			{
				VDEC_KDRV_NOTIFY_PARAM_USRD_T notiUserData = {0, };

				notiUserData.picType 	= pDecInfo->u.picInfo.sUsrData.ui32PicType;
				notiUserData.offset		= pDecInfo->u.picInfo.sUsrData.ui32BuffAddr;
				notiUserData.size		= pDecInfo->u.picInfo.sUsrData.ui32Size;
				notiUserData.top_ff		= pDecInfo->u.picInfo.sUsrData.ui32Top_ff;
				notiUserData.rpt_ff		= pDecInfo->u.picInfo.sUsrData.ui32Rpt_ff;

				VDEC_NOTI_SaveUSRD(ui8Ch, &notiUserData);
			}
		}
		else
		{
			pVdecPicd->sUsrData.ui32PicType 	= 0;
			pVdecPicd->sUsrData.ui32Rpt_ff 		= 0;
			pVdecPicd->sUsrData.ui32Top_ff 		= 0;
			pVdecPicd->sUsrData.ui32BuffAddr 	= 0;
			pVdecPicd->sUsrData.ui32Size 		= 0;
		}

		pVdecPicd->m.afd = stVdecOutput[ui8Ch].decode.m.afd;
		if(		(VDEC_GetVdecStdType(pDecInfo->u32CodecType_Config) == 0/*AVC_DEC*/)
			||	(VDEC_GetVdecStdType(pDecInfo->u32CodecType_Config) == 2/*MP2_DEC*/))
		{
			/* Save AFD */
			if( pDecInfo->u.picInfo.u32ActiveFMT)
				pVdecPicd->m.afd = pDecInfo->u.picInfo.u32ActiveFMT;
		}

		VDEC_NOTI_SavePICD(ui8Ch, (VDEC_KDRV_NOTIFY_PARAM_PICD_T *)pVdecPicd);

		stVdecOutput[ui8Ch].decode = *pVdecPicd;

		if( ui32LogTick == 0x80 )
			VDEC_KDRV_Message(MONITOR, "[NOTI%d] PIC Docode - Width:%d * Heigth:%d", ui8Ch, pVdecPicd->ui32PicWidth,  pVdecPicd->ui32PicHeight);
	}
	else
	{
		VDEC_KDRV_Message(ERROR, "%s: invalid decoding info (eCmdHdr : 0x%x) \n", __FUNCTION__, pDecInfo->eCmdHdr);
	}

	_VDEC_NOTI_UpdateLatest(ui8Ch, pNode);

	if( stVdecNoti[ui8Ch].sConfig.bRegDecCb == TRUE )
	{
		if( _VDEC_NOTI_PushNode(ui8Ch, pNode) == TRUE );
			_VDEC_NOTI_WakeUp(ui8Ch);
	}
	else
	{
		OS_KFree( pNode );
	}

	if( ui32LogTick == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[NOTI%d] Docode - Time Stamp: %llu, UID: %d", ui8Ch, pDecInfo->u.picInfo.sPTS.ui64TimeStamp,  pDecInfo->u.picInfo.sPTS.ui32uID);
		ui32LogTick = 0x0;
	}
	ui32LogTick++;

	VDEC_NOTI_Wakeup(ui8Ch);
}

/**
 * Save displayed information to static notification buffer.
 *
 * actually collect information and build VDEC_KDRV_NOTIFY_PARAM_DISP_INFO_T.
 *
 * @param ch [IN]	to which channel to save displayed information.
 */
void VDEC_NOTI_UpdateDisplayInfo(UINT8 ui8Ch, S_VDEC_NOTI_CALLBACK_DISPINFO_T *pDispInfo)
{
	static UINT32		ui32LogTick = 0x40;
	VDEC_KDRV_NOTI_NODE_T	*pNode = NULL;
	VDEC_KDRV_NOTIFY_PARAM_DISP_T	*pVdecDisp;

	if (ui8Ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ui8Ch);
		return;
	}

	pNode = OS_KMalloc( sizeof ( VDEC_KDRV_NOTI_NODE_T ) );

	pNode->info.id = VDEC_KDRV_NOTI_ID_DISP;
	pVdecDisp = &pNode->info.u.disp;

	pVdecDisp->ui32uID		= pDispInfo->ui32uID;
	pVdecDisp->ui32PTS 		= pDispInfo->ui32DisplayedPTS;
	pVdecDisp->bPtsMatched	= pDispInfo->bPtsMatched;
	pVdecDisp->bDropped		= pDispInfo->bDropped;
	pVdecDisp->ui32NumDisplayed = pDispInfo->ui32NumDisplayed;
	pVdecDisp->ui32DqOccupancy	= pDispInfo->ui32DqOccupancy;
	pVdecDisp->ui32PicWidth	= pDispInfo->ui32PicWidth;
	pVdecDisp->ui32PicHeight	= pDispInfo->ui32PicHeight;
	pVdecDisp->ui64TimeStamp	= pDispInfo->ui64TimeStamp;
	pVdecDisp->si32FrameFd	= pDispInfo->si32FrameFd;

	if( pDispInfo->sUsrData.ui32Size )
	{
		pVdecDisp->sUsrData.ui32PicType 	= pDispInfo->sUsrData.ui32PicType;
		pVdecDisp->sUsrData.ui32Rpt_ff 		= pDispInfo->sUsrData.ui32Rpt_ff;
		pVdecDisp->sUsrData.ui32Top_ff 		= pDispInfo->sUsrData.ui32Top_ff;
		pVdecDisp->sUsrData.ui32BuffAddr 	= pDispInfo->sUsrData.ui32BuffAddr;
		pVdecDisp->sUsrData.ui32Size 		= pDispInfo->sUsrData.ui32Size;

		if( VDEC_KDRV_NOTIFY_MSK_USRD & VDEC_NOTI_GetMaskToCheck(ui8Ch) )
		{
			VDEC_KDRV_NOTIFY_PARAM_USRD_T notiUserData = {0, };

			notiUserData.picType 	= pDispInfo->sUsrData.ui32PicType;
			notiUserData.offset		= pDispInfo->sUsrData.ui32BuffAddr;
			notiUserData.size		= pDispInfo->sUsrData.ui32Size;
			notiUserData.top_ff		= pDispInfo->sUsrData.ui32Top_ff;
			notiUserData.rpt_ff		= pDispInfo->sUsrData.ui32Rpt_ff;

			VDEC_NOTI_SaveUSRD(ui8Ch, &notiUserData);
		}
	}
	else
	{
		pVdecDisp->sUsrData.ui32PicType 	= 0;
		pVdecDisp->sUsrData.ui32Rpt_ff 		= 0;
		pVdecDisp->sUsrData.ui32Top_ff 		= 0;
		pVdecDisp->sUsrData.ui32BuffAddr 	= 0;
		pVdecDisp->sUsrData.ui32Size 		= 0;
	}

	stVdecOutput[ui8Ch].display = *pVdecDisp;

	_VDEC_NOTI_UpdateLatest(ui8Ch, pNode);

	if( stVdecNoti[ui8Ch].sConfig.bRegDispCb == TRUE )
	{
		if( _VDEC_NOTI_PushNode(ui8Ch, pNode) == TRUE );
			_VDEC_NOTI_WakeUp(ui8Ch);
	}
	else
	{
		OS_KFree( pNode );
	}

	if( ui32LogTick == 0x100 )
	{
		VDEC_KDRV_Message(MONITOR, "[NOTI%d] Display - Time Stamp: %llu, UID: %d", ui8Ch, pDispInfo->ui64TimeStamp,  pDispInfo->ui32uID);
		ui32LogTick = 0x0;
	}
	ui32LogTick++;

	VDEC_NOTI_SaveDISP(ui8Ch, (VDEC_KDRV_NOTIFY_PARAM_DISP_T *)pVdecDisp);
	VDEC_NOTI_Wakeup(ui8Ch);
}

void VDEC_NOTI_InitOutputInfo(UINT8 ch)
{
	spin_lock(&stVdecNoti[ch].sCtrl.spinlock);

	memset((void*)&stVdecOutput[ch], 0x0, sizeof(VDEC_KDRV_OUTPUT_T));

	stVdecNoti[ch].sLatest.bUpdateInput = FALSE;
	memset((void*)&stVdecNoti[ch].sLatest.input, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_ES_T));
	stVdecNoti[ch].sLatest.bUpdateFeed = FALSE;
	memset((void*)&stVdecNoti[ch].sLatest.feed, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_ES_T));
	stVdecNoti[ch].sLatest.bUpdateSeqh = FALSE;
	memset((void*)&stVdecNoti[ch].sLatest.seqh, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_SEQH_T));
	stVdecNoti[ch].sLatest.bUpdatePicd = FALSE;
	memset((void*)&stVdecNoti[ch].sLatest.picd, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_PICD_T));
	stVdecNoti[ch].sLatest.bUpdateDisp = FALSE;
	memset((void*)&stVdecNoti[ch].sLatest.disp, 0x0, sizeof(VDEC_KDRV_NOTIFY_PARAM_DISP_T));

	spin_unlock(&stVdecNoti[ch].sCtrl.spinlock);
}

/**
 * find previously allocated queue for target pid(process).
 *
 * @param ch			[IN] channel for search.
 * @param target_tsk	[IN] task struct for search.
 * @return previously allocated notify queue for given process, or NULL if not allocated yet.
 */
static VDEC_KDRV_NOTI_QUEUE_T	*vdec_NOTI_FindQueue(UINT8 ch, struct task_struct *pTsk)
{
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	VDEC_KDRV_NOTI_HEAD_T 	*noti_head;
	unsigned long			irq_flag;

	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return NULL;
	}

	noti_head = &sNotifyHead[ch];

	spin_lock_irqsave(&noti_head->lock, irq_flag);		//{
	list_for_each_entry(pNoti, &noti_head->noti_list, noti_list)
	{
		if ( IS_SAME_CHECK(pNoti, pTsk) )
		{
			spin_unlock_irqrestore(&noti_head->lock, irq_flag);
			return pNoti;
		}
	}
	spin_unlock_irqrestore(&noti_head->lock, irq_flag);	//}

	return NULL;
}

/**
 * allocates notify event Queue.(internal)
 *
 * @param length	[IN]	length of queue in number of notify parameter.
 * @param size		[IN]	byte size of notify parameter
 * @return allocated notify queue, NULL on error.
 */
static VDEC_KDRV_NOTI_QUEUE_T *vdec_NOTI_AllocQueue( UINT16 length )
{
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	UINT16					unit_size;
	unsigned long			irq_flag;

	unit_size = sizeof(VDEC_KDRV_NOTIFY_PARAM2_T);

	pNoti = OS_KMalloc( sizeof ( VDEC_KDRV_NOTI_QUEUE_T ) + (length * unit_size) );

	if ( NULL == pNoti ) return pNoti;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	pNoti->lock		= SPIN_LOCK_UNLOCKED;
#else
	spin_lock_init(&pNoti->lock);
#endif

	spin_lock_irqsave(&pNoti->lock, irq_flag);		//{

	INIT_LIST_HEAD(&pNoti->noti_list);
	pNoti->rofs		= 0;
	pNoti->wofs		= 0;
	pNoti->length	= length;
	pNoti->buffer	= (char*)&pNoti[1];
	pNoti->flag		= 0;
	pNoti->tgid		= task_tgid_nr(current);
	pNoti->mskNotify= 0;

	spin_unlock_irqrestore(&pNoti->lock, irq_flag);//}

	#if ( VDEC_NOTI_DEBUG & 0x0001 )
	VDEC_NOTI_PRINT("%s: length = %d, %p : for %4d,%4d\n", __FUNCTION__, pNoti->length, pNoti->buffer);
	#endif

	return pNoti;
}

/**
 * Allocates Notification Queue.
 *
 * After VDEC_Open() before Enable notification via ioctl ( VDEC_KDRV_IO_SET_NOTIFY )
 *
 * @param ch		[IN] channel for search.
 * @param length	[IN] queue length in number of events.
 *
 * @return 1 really allocated, zero for previously allocated. negative when error
 */
int VDEC_NOTI_Alloc(UINT8 ch, size_t length)
{
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	VDEC_KDRV_NOTI_HEAD_T 	*noti_head;
	unsigned long			irq_flag;
	int						rc = 0;

	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return rc;
	}

	noti_head = &sNotifyHead[ch];

	pNoti = vdec_NOTI_FindQueue(ch, current);

	if ( !pNoti )
	{
		pNoti = vdec_NOTI_AllocQueue( length );

		if ( NULL == pNoti ) { rc = -ENOMEM; goto VDEC_NOTI_Alloc_exit; }

		spin_lock_irqsave(&noti_head->lock, irq_flag);		//{
		list_add_tail(&pNoti->noti_list, &noti_head->noti_list);
		spin_unlock_irqrestore(&noti_head->lock, irq_flag);	//}

		rc = 1;
	}
	else
	{
		rc = 0;
	}

VDEC_NOTI_Alloc_exit:

	if ( rc != 0)
		VDEC_NOTI_PRINT("vdec_%d ] pid %4d,%4d,%s:%d: ptr: %p len %d rc = %d\n", ch, task_pid_nr(current), task_tgid_nr(current), __FUNCTION__, __LINE__, pNoti, length, rc);

	return rc;
}

/**
 * Frees a Notification queue.
 * [NOTE]
 * This should be
 * called AFTER detached from notification list.
 * called unlocked for notification list.
 *
 * @param pNoti	[IN] pointer to a notification queue to be freed.
 */
// [task context]
void VDEC_NOTI_Free( UINT8 ch )
{
	VDEC_KDRV_NOTI_QUEUE_T	*to_delete;
	unsigned long			irq_flag;

	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return;
	}

	to_delete = vdec_NOTI_FindQueue(ch, current);

	if (to_delete)
	{
		VDEC_KDRV_NOTI_HEAD_T 	*noti_head;
		noti_head = &sNotifyHead[ch];

		spin_lock_irqsave(&noti_head->lock, irq_flag);		//{
		list_del(&to_delete->noti_list);
		spin_unlock_irqrestore(&noti_head->lock, irq_flag);	//}
		OS_KFree( to_delete );

		#if (VDEC_NOTI_DEBUG & 0x0002  )
		VDEC_NOTI_PRINT("vdec_%d ] %s:%d:ch %d \n", ch, __FUNCTION__, __LINE__, ch);
		#endif
	}
}

/**
 * Garbage Collect for registered queue.
 *
 * to automatically free registered queue when client process was killed without explicit call of close("/dev/.../vdec#").
 *
 * @param ch	[IN]	whcich channel to check for GC.
 *
 */
void VDEC_NOTI_GarbageCollect( UINT8 ch )
{
	return;
#if 0
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	VDEC_KDRV_NOTI_HEAD_T		*noti_head;
	unsigned long 			irq_flag;

// 20110419 seokjoo.lee GarbageCollectiong bug.
// when a process spawns a thread and calls VDEC_KDRV_IO_SET_NOTIFY and thread exits, then pNoti->tsk->exit_state = EXIT_DEAD and so free notify queue.
// and then another thread monitoring notification queue becomes nothing to be read.
// TODO : how can I find out parent process is alive? how many back tracking its parent?

	noti_head = &sNotifyHead[ch];

	spin_lock_irqsave(&noti_head->lock, irq_flag);		//{

	list_for_each_entry(pNoti, &noti_head->noti_list, noti_list)
	{
		struct task_struct *tsk = pNoti->tsk;

		if ( tsk->exit_state == EXIT_DEAD || tsk->exit_state == EXIT_ZOMBIE )
		{
			VDEC_KDRV_NOTI_QUEUE_T	*to_delete;

			to_delete	= pNoti;
			pNoti		= list_entry(pNoti->noti_list.prev, typeof(*pNoti), noti_list);

			list_del(&to_delete->noti_list);
			spin_unlock_irqrestore(&noti_head->lock, irq_flag);

			OS_KFree(to_delete);

			#if (VDEC_NOTI_DEBUG & 0x0004)
			VDEC_NOTI_PRINT("vdec_%d ] NOTI_GC: %s :pid %4d,%4d, %d, %d\n",
				ch,
				task_tgid_nr(tsk) == task_tgid_nr(current) ? "current":"other  ",
				task_pid_nr(tsk), task_tgid_nr(tsk),
				tsk->exit_state, tsk->exit_code);
			#endif

			spin_lock_irqsave(&noti_head->lock, irq_flag);
		}
	}
	spin_unlock_irqrestore(&noti_head->lock, irq_flag);	//}
#endif
}

/**
 * Enable/Disable notification.
 * Implicitly, only enable/disable for given process.
 * @param ch	[IN] channel for search.
 * @param arg	[IN] notification ID see @ref VDEC_KDRV_NOTIFY_ID_SEQH
 */
// [task context]
void VDEC_NOTI_SetMask(UINT8 ch, UINT32 mskNotify)
{
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	VDEC_KDRV_NOTI_HEAD_T		*noti_head;
	int						bMaskChanged = 0;
	unsigned long			irq_flag;
	UINT32					collected_mask = 0;

	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return;
	}

	pNoti = vdec_NOTI_FindQueue(ch, current);

	if ( pNoti )
	{
		spin_lock_irqsave(&pNoti->lock, irq_flag);		//{
		if ( pNoti->mskNotify != mskNotify)
		{
			pNoti->mskNotify = mskNotify;
			bMaskChanged = 1;
		}

		pNoti->rofs = pNoti->wofs;
		spin_unlock_irqrestore(&pNoti->lock, irq_flag);	//}
	}

	if ( bMaskChanged )
	{
		noti_head = &sNotifyHead[ch];

		spin_lock_irqsave(&noti_head->lock, irq_flag);		//{
		list_for_each_entry(pNoti, &noti_head->noti_list, noti_list)
		{
			collected_mask |= pNoti->mskNotify;
		}
		noti_head->mskNotifyToCheck = collected_mask;
		spin_unlock_irqrestore(&noti_head->lock, irq_flag);	//}

#if 0//def L9_NOTIFICATION
		if((collected_mask&VDEC_KDRV_NOTIFY_MSK_SEQH)==VDEC_KDRV_NOTIFY_MSK_SEQH)
			VDEC_REG_MCU_SetNotify(MCU_REQ_SEQ_HEADER);
		else
			VDEC_REG_MCU_ClearNotify(MCU_REQ_SEQ_HEADER);

		if((collected_mask&VDEC_KDRV_NOTIFY_MSK_PICH)==VDEC_KDRV_NOTIFY_MSK_PICH)
			VDEC_REG_MCU_SetNotify(MCU_REQ_PIC_HEADER);
		else
			VDEC_REG_MCU_ClearNotify(MCU_REQ_PIC_HEADER);

		if((collected_mask&VDEC_KDRV_NOTIFY_MSK_PICD)==VDEC_KDRV_NOTIFY_MSK_PICD)
			VDEC_REG_MCU_SetNotify(MCU_REQ_PIC_DECODED);
		else
			VDEC_REG_MCU_ClearNotify(MCU_REQ_PIC_DECODED);
#endif

	}

	#if ( VDEC_NOTI_DEBUG & 0x0008 )
	VDEC_NOTI_PRINT("vdec_%d ] %s:%d: mskNotify %x %x pid %d,%d\n", ch, __FUNCTION__, __LINE__,
					mskNotify, collected_mask, task_pid_nr(current), task_tgid_nr(current));
	#endif
}

/**
 * Enable/Disable notification.
 * Implicitly, only enable/disable for given process.
 * @param ch	[IN] channel for search.
 * @param arg	[IN] notification ID see @ref VDEC_KDRV_NOTIFY_ID_SEQH
 */
// [task context]
UINT32 VDEC_NOTI_GetMask(UINT8 ch)
{
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	unsigned long			irq_flag;
	UINT32					mskNotify = 0;

	pNoti = vdec_NOTI_FindQueue(ch, current);

	if ( pNoti )
	{
		spin_lock_irqsave(&pNoti->lock, irq_flag);		//{
		mskNotify = pNoti->mskNotify;
		spin_unlock_irqrestore(&pNoti->lock, irq_flag);	//}
	}

	return mskNotify;
}

UINT32 VDEC_NOTI_GetMaskToCheck(UINT8 ch)
{
	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return 0;
	}

	return sNotifyHead[ch].mskNotifyToCheck;
}

#define __noti_ofs( _noti, _ofs )	( (_ofs) % _noti->length )
#define __noti_rofs_next( _noti )	__noti_ofs( _noti, _noti->rofs + 1 )
#define __noti_wofs_next( _noti )	__noti_ofs( _noti, _noti->wofs + 1 )
#define __noti_rofs_sync( _noti )	_noti->rofs = _noti
#define __noti_ofs_diff1( _len, _wofs, _rofs)	((_wofs >= _rofs) ? (wofs - rofs) : _len - rofs)
#define __noti_ofs_diff2( _len, _wofs, _rofs)	((_wofs >= _rofs) ? 0             : _wofs)

/**
 * save notify data to given queue for per-task(process) queue.
 *
 * @pre
 * [interrupt context]
 *
 * [ OBSOLETE ] 20110413 seokjoo.lee overflow check modified. T.T
 *
 * Normal condition.
 * ----------------
 * 0.0. initial condition.
 * +-------------------+
 * | | | | | | | | | | |
 * +-------------------+
 *       R=W			ISR
 *       r=w			USR
 *
 * 0.1.Save notify param to queue: 	ISR Happened and read process does not read it yet.
 * +-------------------+
 * | | | |1| | | | | | |
 * +-------------------+
 *        R>W			ISR		(locked)capture R,W to local variable. (unlocked)
 *       r=w			USR		copy_to_user( user_buffer, r, (w - r) )
 *
 * 0.2. user process read notifcation.
 * +-------------------+
 * | | | | | | | | | | |
 * +-------------------+
 *         R=W					(locked)re-read W, if W is not overflowed, then synchronize local R = W (unlocked)
 *         r=w					because overflow margin can prevent "rofs" race condition.
 *
 * 0.3.Lazy read cae
 * +-------------------+
 * | | | |1| | | | | | |
 * +-------------------+
 *        R>W
 *        r w			USR		process read r=R, w=W,
 *        				USR		copy_to_user(user_buffer, &r, (w-r))
 *
 * 0.3.1 ISR Happened during copy_to_user
 * +-------------------+
 * | | | | |1|1| | | | |
 * +-------------------+
 *        R   ->W
 *        r w			USR 	after copying is done.
 *       				USR		re-read r2=R, r2=W, and if r2 (read pointer after copied safely) is preserved before copying.
 *          R   W      	USR		update and R=w, (advance queue read pointer by pop'ed size)
 *         r=w
 *
 * overflow check.
 * --------------
 *
 * 1.1. before copy, 8 ISR with out readiing. and currently 9 th ISR happened.
 * +-------------------+
 * |1|1| | |1|1|1|1|1|1|
 * +-------------------+
 *      W   R					above condition, reader task holds R=W in local varable.
 *          O
 *
 * 1.2. after save parameter, and increasing write offset
 * +-------------------+
 * |1|1|1| |1|1|1|1|1|1|
 * +-------------------+
 *      ->W R
 *          O
 *
 * 1.3. Check Overflow in ISR
 * +-------------------+
 * |1|1|1| |1|1|1|1|1|1|
 * +-------------------+
 *        W R				if ( W + 1 == R ) R ++; // overflow warning.
 *          O
 *
 * 1.4. if (R == W+1) R++ : causes USR vs ISR rofs/wofs mismatch.
 * +-------------------+
 * |1|1|1| |1|1|1|1|1|1|
 * +-------------------+
 *        W ->R
 *          O
 *
 * 1.5. repeated 2 ISR 1.1 ~ 1.4 ( total queue length + 1 ISR happened ) -> Actual Overflow!!!
 * +-------------------+
 * |1|1|1|1|2|1|1|1|1|1|
 * +-------------------+
 *            W ->R
 *          O				-> causes copied parameter can not guarentee integrity at [w(USR) ~ W-1]
 *
 * 1.6. repeated queue length ISR happened (1.1 ~ 1.4) (ISR happened in queue length * 2 +1 times )
 * +-------------------+
 * |2|2|2|2|3|2|2|2|2|2|
 * +-------------------+
 *            W ->R
 *          O				-> what happened??
 *
 * 2.0 Extreme case.
 * How can we discriminate 1.5 between 1.6 ?
 *  -> use of notify sequence number generated by ISR.
 *
 * @endpre
 *
 * @return if success, returns zero. if error, then returns negative.
 */
// [interrupt context]
static void vdec_NOTI_SaveOneQueue(VDEC_KDRV_NOTI_QUEUE_T *pNoti, VDEC_KDRV_NOTIFY_PARAM2_T* param)
{
	size_t	unit_size = sizeof(VDEC_KDRV_NOTIFY_PARAM2_T);
	unsigned long irq_flag;
	int		bOverflowed = 0;

	memcpy(pNoti->buffer+(pNoti->wofs*unit_size), param, unit_size);

	spin_lock_irqsave(&pNoti->lock, irq_flag);		//{
	pNoti->wofs = __noti_wofs_next( pNoti );

	// check overflow. : 20110413 seokjoo.lee OVERFLOW CHECK mismatched T.T
	if ( pNoti->rofs == __noti_wofs_next(pNoti) )
	{
		pNoti->rofs = __noti_rofs_next( pNoti );

		if ( pNoti->flag & VDEC_KDRV_NOTI_QUEUE_FLAG_OVERFLOW )
		{
			pNoti->flag |= VDEC_KDRV_NOTI_QUEUE_FLAG_OVERFLOW;	// FIXME : warn too early.
			bOverflowed = 1;
		}
	}
	spin_unlock_irqrestore(&pNoti->lock, irq_flag);	//}

	return;
}

/**
 * save notify paramter to all process registered when enabled.
 */
// [interrupt context]
static void vdec_NOTI_Save(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM2_T *param)
{
	VDEC_KDRV_NOTI_HEAD_T		*noti_head;
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	UINT32					notifyID;

	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return;
	}

	if ( !param )						return;

	notifyID	= param->id;

	noti_head	= &sNotifyHead[ch];
	list_for_each_entry(pNoti, &noti_head->noti_list, noti_list)
	{
		#if (VDEC_NOTI_DEBUG & 0x0010 )
		char *p = pNoti->buffer + pNoti->wofs* sizeof(VDEC_KDRV_NOTIFY_PARAM2_T);
		#endif
		if ( pNoti->mskNotify & (1 << notifyID) )
		{

			vdec_NOTI_SaveOneQueue(pNoti, param);

		}
		#if (VDEC_NOTI_DEBUG & 0x0010 )
		VDEC_NOTI_PRINT("vdec_%d ] save (tgid/stat, %4d),{%08x, %3u,%3u, %p, %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x}\n",
			ch,
			pNoti->tgid,
			pNoti->mskNotify, pNoti->rofs, pNoti->wofs, p,
			p[3], p[2], p[1], p[0], p[4], p[5], p[6], p[7],
			p[8+0], p[8+1], p[8+2], p[8+3], p[8+4], p[8+5], p[8+6], p[8+7]
			);
		#endif
	}

	return;
}

void VDEC_NOTI_SaveSEQH(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_SEQH_T	*pSeqh)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_SEQH;
	param.u.seqh = *pSeqh;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SavePICD(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_PICD_T *pOut)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_PICD;
	param.u.picd = *pOut;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveUSRD(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_USRD_T	*pUsrd)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_USRD;
	param.u.usrd = *pUsrd;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveDISP(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_DISP_T	*pDisp)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_DISP;
	param.u.disp = *pDisp;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveERR(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_ERR_T	*pErr)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_ERR;
	param.u.err = *pErr;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveAFULL(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_AFULL_T	*pAFull)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_AFULL;
	param.u.afull = *pAFull;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveAEMPTY(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_AEMPTY_T	*pAEmpty)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_AEMPTY;
	param.u.aempty = *pAEmpty;

	vdec_NOTI_Save(ch, &param);
}

void VDEC_NOTI_SaveRTIMEOUT(UINT8 ch, VDEC_KDRV_NOTIFY_PARAM_RTIMEOUT_T	*pTimeout)
{
	VDEC_KDRV_NOTIFY_PARAM2_T	param;

	param.magic = VDEC_KDRV_NOTIFY_MAGIC;
	param.id = VDEC_KDRV_NOTIFY_ID_RTIMEOUT;
	param.u.rtimeout = *pTimeout;

	vdec_NOTI_Save(ch, &param);
}


/**
 * Wait for notification.
 *
 * @param ch		[IN]	which channel is current process waiting for.
 * @param timeout	[IN] timeout value in jiffies.
 *
 * @return remaining timeout value. zero for timeout.
 */
signed long VDEC_NOTI_WaitTimeout(UINT8 ch, signed long timeout)
{
	return interruptible_sleep_on_timeout(&sNotifyHead[ch].wq, timeout);
}

/**
 * Wake up all process.
 *
 *
 * @param ch		[IN] channel
 */
void VDEC_NOTI_Wakeup(UINT8 ch)
{
	if (ch >= VDEC_KDRV_CH_MAXN)
	{
		VDEC_KDRV_Message(ERROR, "%s Invalid Channel Number : %d", __FUNCTION__, ch);
		return;
	}

	wake_up_interruptible_all(&sNotifyHead[ch].wq);
}

/**
 * copy to user buffer from saved notification queue.
 *
 * @param ch			[IN] channel for search.
 * @param notifyID		[IN] notification ID see @ref VDEC_KDRV_NOTIFY_ID_SEQH
 * @return non-negative number of bytes copied, negative when error.
 */
// [task context]
int VDEC_NOTI_CopyToUser(UINT8 ch, char __user *buffer, size_t buffer_size)
{
	UINT16	rofs = 0, wofs = 0;
	UINT16	rofs2, wofs2;
	UINT16	length;
	VDEC_KDRV_NOTI_QUEUE_T	*pNoti;
	size_t	count1=0, count2=0;
	size_t	unit_size = sizeof(VDEC_KDRV_NOTIFY_PARAM2_T);
	int		bytes_copied = 0;
	int		rc = 0;

	unsigned long irq_flag;

	pNoti = vdec_NOTI_FindQueue(ch, current);

	// if not found.
	if ( !pNoti ) goto VDEC_NOTI_CopyToUser_exit;

	spin_lock_irqsave(&pNoti->lock, irq_flag);		//{
	rofs	= pNoti->rofs;
	wofs	= pNoti->wofs;
	length	= pNoti->length;
	spin_unlock_irqrestore(&pNoti->lock, irq_flag);	//}

	count1		= __noti_ofs_diff1(length, wofs, rofs);
	count2		= __noti_ofs_diff2(length, wofs, rofs);

	// TODO
	// insufficient 일 경우 대책?
	// oldest notification을 넘겨 줄 것인가?
	// newest notification을 넘겨 줄 것인가?
	if ( buffer_size < (count1+count2) * unit_size )
	{
		VDEC_KDRV_Message(ERROR, "%s: insufficient buffer size %d < (%d * %d) *%d \n", __FUNCTION__, buffer_size, count1, count2, unit_size);
		bytes_copied = -EFAULT;
		goto VDEC_NOTI_CopyToUser_exit;
	}

	rc = copy_to_user(buffer, pNoti->buffer + (rofs * unit_size), count1*unit_size);
	if ( count2 )
		rc = copy_to_user(buffer+(count1*unit_size), pNoti->buffer, count2*unit_size);

	// overflow check only.
	spin_lock_irqsave(&pNoti->lock, irq_flag);		//{
	rofs2 = pNoti->rofs;
	wofs2 = pNoti->wofs;

	// if not overflowed(is not updated by writer == ISR), then update it.
	if ( rofs == rofs2 ) pNoti->rofs = wofs;
	else
	{
		// TODO :
	}
	spin_unlock_irqrestore(&pNoti->lock, irq_flag);	//}

	bytes_copied = ((count1 + count2 ) * unit_size);

VDEC_NOTI_CopyToUser_exit:

	#if (VDEC_NOTI_DEBUG & 0x0020 )
	if ( 0 == ch)
		VDEC_NOTI_PRINT("vdec_%d ] CopyToUser  %3d,%3d, sz %d + %d, \n", ch, rofs, wofs, count1, count2);
	#endif

	return bytes_copied;
}

/** @} */

