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
#ifdef __XTENSA__
#include "mcu_hal_api.h"
#else
#include <linux/kernel.h>
#endif

#include "ipc_reg_api.h"

#include "lg1152_vdec_base.h"
#include "../../vdc/vdc_auq.h"
#include "../../vds/disp_q.h"
#include "../../mcu/os_adap.h"
#include "../../mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define VDEC_IPC_REG_BASE				(LG1152_VDEC_BASE + 0x800)

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct {
	UINT32	wr_offset;	// byte offset
	UINT32	rd_offset;	// byte offset
	UINT32	base;
	UINT32	size;
} buf_base_index_t;

typedef struct {
	UINT32	wr_offset;	// byte offset
	UINT32	rd_offset;	// byte offset
} buf_index_t;

typedef struct {
   buf_base_index_t	dbgb;             		// 00
   buf_base_index_t	cmdb;            		// 10
   buf_base_index_t	reqb;             		// 20

   buf_index_t			dispq[DISP_Q_NUM_OF_CHANNEL];			// 30
   buf_index_t			dispclearq[DISP_Q_NUM_OF_CHANNEL];		// 48
   buf_index_t			auq[AU_Q_NUM_OF_CHANNEL];				// 60
   UINT32				cpb_wr[AU_Q_NUM_OF_CHANNEL];			// 78
   buf_index_t			cpb[CPB_NUM_OF_CHANNEL];				// 84

   UINT32				ves_db;				// 9C
   UINT32				vdc_db;				// A0
   UINT32				dpb_db;				// A4
   UINT32				lipsync_db;			// A8
   UINT32				rate_db;			// AC
   UINT32				pts_db;				// B0
   UINT32				de_if_db;			// B4
   UINT32				de_sync_db;			// B8
   UINT32				ramlog_db;			// BC
   UINT32				dispq_db;			// C0

   UINT32				reserved[8];			// C4, C8, CC, D0, D4, D8, DC, E0

   UINT32				mcu_debug_mask;		// E4
   UINT32				mcu_sw_ver;			// E8
   UINT32				watchdog_cnt;		// EC

   UINT32				lipsync_base[4];		// F0 ~ FC, for LipSync
} s_reg_ipc_t;

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
volatile static s_reg_ipc_t *stpIPC_Reg;


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
void IPC_REG_Init(void)
{
	stpIPC_Reg = (s_reg_ipc_t *)VDEC_TranselateVirualAddr(VDEC_IPC_REG_BASE, 0xF0);
}


UINT32 IPC_REG_AUQ_GetWrOffset(UINT8 ch)
{
	return stpIPC_Reg->auq[ch].wr_offset;
}

void IPC_REG_AUQ_SetWrOffset(UINT8 ch, UINT32 ui32WrOffset)
{
	stpIPC_Reg->auq[ch].wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_AUQ_GetRdOffset(UINT8 ch)
{
	return stpIPC_Reg->auq[ch].rd_offset;
}

void IPC_REG_AUQ_SetRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	stpIPC_Reg->auq[ch].rd_offset = ui32RdOffset;
}

UINT32 IPC_REG_CPB_GetWrAddr(UINT8 ch)
{
	return stpIPC_Reg->cpb_wr[ch];
}

void IPC_REG_CPB_SetWrAddr(UINT8 ch, UINT32 ui32WrAddr)
{
	stpIPC_Reg->cpb_wr[ch] = ui32WrAddr;
}


UINT32 IPC_REG_CPB_GetWrOffset(UINT8 ch)
{
	return stpIPC_Reg->cpb[ch].wr_offset;
}

void IPC_REG_CPB_SetWrOffset(UINT8 ch, UINT32 ui32WrOffset)
{
	stpIPC_Reg->cpb[ch].wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_CPB_GetRdOffset(UINT8 ch)
{
	return stpIPC_Reg->cpb[ch].rd_offset;
}

void IPC_REG_CPB_SetRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	stpIPC_Reg->cpb[ch].rd_offset = ui32RdOffset;
}


UINT32 IPC_REG_DISPQ_GetWrOffset(UINT8 ch)
{
	return stpIPC_Reg->dispq[ch].wr_offset;
}

void IPC_REG_DISPQ_SetWrOffset(UINT8 ch, UINT32 ui32WrOffset)
{
	stpIPC_Reg->dispq[ch].wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_DISPQ_GetRdOffset(UINT8 ch)
{
	return stpIPC_Reg->dispq[ch].rd_offset;
}

void IPC_REG_DISPQ_SetRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	stpIPC_Reg->dispq[ch].rd_offset = ui32RdOffset;
}


UINT32 IPC_REG_DISPCLEARQ_GetWrOffset(UINT8 ch)
{
	return stpIPC_Reg->dispclearq[ch].wr_offset;
}

void IPC_REG_DISPCLEARQ_SetWrOffset(UINT8 ch, UINT32 ui32WrOffset)
{
	stpIPC_Reg->dispclearq[ch].wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_DISPCLEARQ_GetRdOffset(UINT8 ch)
{
	return stpIPC_Reg->dispclearq[ch].rd_offset;
}

void IPC_REG_DISPCLEARQ_SetRdOffset(UINT8 ch, UINT32 ui32RdOffset)
{
	stpIPC_Reg->dispclearq[ch].rd_offset = ui32RdOffset;
}


UINT32 IPC_REG_CMD_GetWrOffset(void)
{
	return stpIPC_Reg->cmdb.wr_offset;
}

void IPC_REG_CMD_SetWrOffset(UINT32 ui32WrOffset)
{
	stpIPC_Reg->cmdb.wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_CMD_GetRdOffset(void)
{
	return stpIPC_Reg->cmdb.rd_offset;
}

void IPC_REG_CMD_SetRdOffset(UINT32 ui32RdOffset)
{
	stpIPC_Reg->cmdb.rd_offset = ui32RdOffset;
}

UINT32 IPC_REG_CMD_GetBaseAddr(void)
{
	return stpIPC_Reg->cmdb.base;
}

void IPC_REG_CMD_SetBaseAddr(UINT32 ui32Cmdptr)
{
	stpIPC_Reg->cmdb.base = ui32Cmdptr;
}

UINT32 IPC_REG_CMD_GetBufSize(void)
{
	return stpIPC_Reg->cmdb.size;
}

void IPC_REG_CMD_SetBufSize(UINT32 ui32CmdSize)
{
	stpIPC_Reg->cmdb.size = ui32CmdSize;
}

UINT32 IPC_REG_REQ_GetWrOffset(void)
{
	return stpIPC_Reg->reqb.wr_offset;
}

void IPC_REG_REQ_SetWrOffset(UINT32 ui32WrOffset)
{
	stpIPC_Reg->reqb.wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_REQ_GetRdOffset(void)
{
	return stpIPC_Reg->reqb.rd_offset;
}

void IPC_REG_REQ_SetRdOffset(UINT32 ui32RdOffset)
{
	stpIPC_Reg->reqb.rd_offset = ui32RdOffset;
}

UINT32 IPC_REG_REQ_GetBaseAddr(void)
{
	return stpIPC_Reg->reqb.base;
}

void IPC_REG_REQ_SetBaseAddr(UINT32 ui32Reqptr)
{
	stpIPC_Reg->reqb.base = ui32Reqptr;
}

UINT32 IPC_REG_REQ_GetBufSize(void)
{
	return stpIPC_Reg->reqb.size;
}

void IPC_REG_REQ_SetBufSize(UINT32 ui32ReqSize)
{
	stpIPC_Reg->reqb.size = ui32ReqSize;
}

UINT32 IPC_REG_DBG_GetWrOffset(void)
{
	return stpIPC_Reg->dbgb.wr_offset;
}

void IPC_REG_DBG_SetWrOffset(UINT32 ui32WrOffset)
{
	stpIPC_Reg->dbgb.wr_offset = ui32WrOffset;
}

UINT32 IPC_REG_DBG_GetRdOffset(void)
{
	return stpIPC_Reg->dbgb.rd_offset;
}

void IPC_REG_DBG_SetRdOffset(UINT32 ui32RdOffset)
{
	stpIPC_Reg->dbgb.rd_offset = ui32RdOffset;
}

UINT32 IPC_REG_DBG_GetBaseAddr(void)
{
	return stpIPC_Reg->dbgb.base;
}

void IPC_REG_DBG_SetBaseAddr(UINT32 ui32DBGptr)
{
	stpIPC_Reg->dbgb.base = ui32DBGptr;
}

UINT32 IPC_REG_DBG_GetBufSize(void)
{
	return stpIPC_Reg->dbgb.size;
}

void IPC_REG_DBG_SetBufSize(UINT32 ui32DBGSize)
{
	stpIPC_Reg->dbgb.size = ui32DBGSize;
}


UINT32 IPC_REG_VES_GetDb(void)
{
	return stpIPC_Reg->ves_db;
}

void IPC_REG_VES_SetDb(UINT32 ves_db)
{
	stpIPC_Reg->ves_db = ves_db;
}

UINT32 IPC_REG_VDC_GetDb(void)
{
	return stpIPC_Reg->vdc_db;
}

void IPC_REG_VDC_SetDb(UINT32 vdc_db)
{
	stpIPC_Reg->vdc_db = vdc_db;
}

UINT32 IPC_REG_DPB_GetDb(void)
{
	return stpIPC_Reg->dpb_db;
}

void IPC_REG_DPB_SetDb(UINT32 dpb_db)
{
	stpIPC_Reg->dpb_db = dpb_db;
}

UINT32 IPC_REG_LIPSYNC_GetDb(void)
{
	return stpIPC_Reg->lipsync_db;
}

void IPC_REG_LIPSYNC_SetDb(UINT32 lipsync_db)
{
	stpIPC_Reg->lipsync_db = lipsync_db;
}

UINT32 IPC_REG_RATE_GetDb(void)
{
	return stpIPC_Reg->rate_db;
}

void IPC_REG_RATE_SetDb(UINT32 rate_db)
{
	stpIPC_Reg->rate_db = rate_db;
}

UINT32 IPC_REG_PTS_GetDb(void)
{
	return stpIPC_Reg->pts_db;
}

void IPC_REG_PTS_SetDb(UINT32 pts_db)
{
	stpIPC_Reg->pts_db = pts_db;
}

UINT32 IPC_REG_DE_IF_GetDb(void)
{
	return stpIPC_Reg->de_if_db;
}

void IPC_REG_DE_IF_SetDb(UINT32 de_if_db)
{
	stpIPC_Reg->de_if_db = de_if_db;
}

UINT32 IPC_REG_DE_SYNC_GetDb(void)
{
	return stpIPC_Reg->de_sync_db;
}

void IPC_REG_DE_SYNC_SetDb(UINT32 de_sync_db)
{
	stpIPC_Reg->de_sync_db = de_sync_db;
}

UINT32 IPC_REG_RAMLOG_GetDb(void)
{
	return stpIPC_Reg->ramlog_db;
}

void IPC_REG_RAMLOG_SetDb(UINT32 ramlog_db)
{
	stpIPC_Reg->ramlog_db = ramlog_db;
}

UINT32 IPC_REG_DISPQ_GetDb(void)
{
	return stpIPC_Reg->dispq_db;
}

void IPC_REG_DISPQ_SetDb(UINT32 dispq_db)
{
	stpIPC_Reg->dispq_db = dispq_db;
}


UINT32 IPC_REG_Get_McuDebugMask(void)
{
	return stpIPC_Reg->mcu_debug_mask;
}

void IPC_REG_Set_McuDebugMask(UINT32 mcu_debug_mask)
{
	stpIPC_Reg->mcu_debug_mask = mcu_debug_mask;
}

UINT32 IPC_REG_Get_McuSwVer(void)
{
	return stpIPC_Reg->mcu_sw_ver;
}

void IPC_REG_Set_McuSwVer(UINT32 mcu_sw_ver)
{
	stpIPC_Reg->mcu_sw_ver = mcu_sw_ver;
}

UINT32 IPC_REG_Get_WatchdogCnt(void)
{
	return stpIPC_Reg->watchdog_cnt;
}

void IPC_REG_Set_WatchdogCnt(UINT32 watchdog_cnt)
{
	stpIPC_Reg->watchdog_cnt = watchdog_cnt;
}


/** @} */

