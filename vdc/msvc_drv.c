
#include "../mcu/vdec_type_defs.h"

#ifdef __XTENSA__
#include <stdio.h>
#else
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#include <linux/spinlock.h>
#endif

#include "../mcu/os_adap.h"

#include "../hal/lg1152/msvc_hal_api.h"
#include "msvc_drv.h"
#include "../hal/lg1152/top_hal_api.h"
#include "../hal/lg1152/mcu_hal_api.h"
#include "../hal/lg1152/lq_hal_api.h"
#include "../ves/ves_cpb.h"

#include "vdc_drv.h"
#include "msvc_adp.h"

#include "../mcu/vdec_print.h"
#include "../mcu/ram_log.h"
#include "../vds/de_if_drv.h"

#if !defined(__XTENSA__)
#include "../vdec_cfg.h"
#endif

#define MSVC_RESET_TEST

//static UINT8 msvc_dpb_overflow_cnt[MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL];

//static UINT8 msvc_rst_ready[MSVC_NUM_OF_CORE];
static MSVC_CH_T gstMsvcCh[MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL];
static UINT8 msvc_dbgPackedMode[MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL];
static UINT8 msvc_dbgFieldPic[MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL];
static UINT32 msvc_dbgCmdTs[MSVC_NUM_OF_CORE][2];
static UINT32 msvc_dbgPoss[MSVC_NUM_OF_CORE][2];
//static UINT8 u8DebugModeOn = FALSE;
UINT32 dbgPrtLvDeQEnt=DBG_VDC;
UINT32 dbgPrtLvPicDone=DBG_VDC;
//static UINT32 dbgPrtLvCqEnt=LRP_FREQ;

//static UINT32 gui32MsvcMvColBufPool = 0xFFFFFFFF;	// 0x40000 * 32

#if 0
static void _msvc_cmd_ram_log(UINT32 u32CoreNum, UINT32 u32InstNum, CQ_ENTRY_T *pstCqEntry, UINT32 u32ClearMark)
{
	gMSVCCmdLogMem	sRamLog = {0, };

	sRamLog.ui64TimeStamp = 0x0;
	sRamLog.ui32GSTC = TOP_HAL_GetGSTCC();
	sRamLog.ui32ClearIdx = u32ClearMark;
	sRamLog.wDispMark = MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum);
	sRamLog.ui16AuType = pstCqEntry->auType;
	sRamLog.ui16resv = 0;
	sRamLog.ui32AuAddr = pstCqEntry->auAddr;
	sRamLog.ui32MSVCRdPtr = MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum);

	RAM_LOG_Write(RAM_LOG_MOD_MSVC_CMD, (void *)&sRamLog);
}

static void _msvc_rsp_ram_log(UINT32 u32CoreNum, UINT32 u32InstNum, PIC_INFO_T *pstPicInfo)
{
	gMSVCLogMem	sRamLog = {0, };

	//sRamLog.ui64TimeStamp = pstPicInfo->u64TimeStamp;
	sRamLog.ui32GSTC = TOP_HAL_GetGSTCC();
	sRamLog.ui32Size = pstPicInfo->u32AuSize;
	sRamLog.ui16DispIdx = pstPicInfo->u32IndexFrameDisplay;
	sRamLog.ui16DecodedIdx = pstPicInfo->u32IndexFrameDecoded;
	sRamLog.wDispMark = MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum);
	sRamLog.ui32RdPtr = MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum);
	sRamLog.ui32WrPtr = MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum);

	RAM_LOG_Write(RAM_LOG_MOD_MSVC_RSP, (void *)&sRamLog);
}
#endif
SINT32 _msvc_ChInit(MSVC_CH_T *pstMsvcCh, UINT32 u32Ch)
{
	memset(pstMsvcCh, 0, sizeof(MSVC_CH_T));

	pstMsvcCh->u32Ch = u32Ch;
	pstMsvcCh->u32CoreNum = u32Ch >> 2;
	pstMsvcCh->u32InstNum = u32Ch & 0x03;
//	pstMsvcCh->u8DqCh = 0xFF;
//	pstMsvcCh->u8PdecCh = 0xFF;
//	pstMsvcCh->bSeqInit = 0;
//	pstMsvcCh->bFieldDone = 0;
//	pstMsvcCh->sVdecFb.ui32NumOfFb = 0;
//	pstMsvcCh->i16UnlockIndex = -3;

	return 0;
}
#if 0
void MSVC_PrintStatus(UINT32 u32CoreNum)
{
	UINT32 u32InstNum=0;
	MSVC_CH_T *pstMsvcCh;
//	CQ_ENTRY_T *pstCmd;

	if( MSVC_HAL_IsBusy(u32CoreNum) )
		u32InstNum = MSVC_HAL_GetCurInstance(u32CoreNum);

	pstMsvcCh = MSVC_GetHandler(u32CoreNum*MSVC_NUM_OF_CHANNEL + u32InstNum);
	if( pstMsvcCh == NULL )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] Getting handler fail", u32CoreNum, u32InstNum);
		return;
	}

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;
//	pstCmd = &pstMsvcCh->stLastCmd;

	VDEC_KDRV_Message(ERROR, "[MSVC%d %d] BZ %d Instance %d Instruction %d RD %08X WR %08X",
			pstMsvcCh->u32Ch, TOP_HAL_GetGSTCC(),
			MSVC_HAL_IsBusy(u32CoreNum),
			MSVC_HAL_GetCurInstance(u32CoreNum),
			MSVC_HAL_GetCurCommand(u32CoreNum),
			MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum),
			MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum) );

//	VDEC_KDRV_Message(ERROR, "   Cmd Type %d AuAddr %08X AuSize %d AuEnd %08X PreRD %08X",
//			pstCmd->auType, pstCmd->auAddr, pstCmd->size, pstCmd->auEnd, pstMsvcCh->u32PreRdPtr);

	return;
}
#endif
SINT32 MSVC_Init(void)
{
	SINT32 i;
	MSVC_CH_T *pstMsvcCh;

	VDEC_KDRV_Message(_TRACE, "[MSVC][DBG] %s", __FUNCTION__ );

	for(i=0; i<MSVC_NUM_OF_CORE*MSVC_NUM_OF_CHANNEL; i++)
	{
		pstMsvcCh = MSVC_GetHandler(i);
		_msvc_ChInit(pstMsvcCh, i);
		pstMsvcCh->bOpened = FALSE;
	}

	MSVC_Reg_Init();

	for(i=0 ; i<MSVC_NUM_OF_CORE; i++)
	{
		// MSVC_Init
		MSVC_HAL_CodeDown(i, VDEC_MSVC_CORE0_BASE + i*VDEC_MSVC_SIZE, VDEC_MSVC_SIZE);

		// Stop running BIT processor
		MSVC_HAL_SetRun(i, 0);

		// 3. Download BIT assembly code to system
		MSVC_BootCodeDown(i);

		VDEC_KDRV_Message(_TRACE, "[MSVC] Core(%d) is Loaded", i);

		// Core Init
		MSVC_HAL_Init(i, VDEC_MSVC_CORE0_BASE + i*VDEC_MSVC_SIZE, VDEC_MSVC_SIZE);

		MSVC_HAL_SetRun(i, 1);

		VDEC_KDRV_Message(_TRACE, "[MSVC] Core(%d) Base Buffer Address 0x%08X, Size 0x%08X", i, VDEC_MSVC_CORE0_BASE + i*VDEC_MSVC_SIZE, VDEC_MSVC_SIZE);

//		msvc_rst_ready[i] = 0;
	}

	return 0;
}
#if 0
void MSVC_SetRstReady(UINT32 u32CoreNum)
{
	if( u32CoreNum >= MSVC_NUM_OF_CORE )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC] Wrong Core Number : %d- %s", u32CoreNum, __FUNCTION__);
		return;
	}

	msvc_rst_ready[u32CoreNum] = 1;
}

void MSVC_ClearRstReady(UINT32 u32CoreNum)
{
	if( u32CoreNum >= MSVC_NUM_OF_CORE )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC] Wrong Core Number : %d- %s", u32CoreNum, __FUNCTION__);
		return;
	}

	msvc_rst_ready[u32CoreNum] = 0;
}

UINT8 MSVC_GetRstReady(UINT32 u32CoreNum)
{
	if( u32CoreNum >= MSVC_NUM_OF_CORE )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC] Wrong Core Number : %d- %s", u32CoreNum, __FUNCTION__);
		return 0;
	}

	return msvc_rst_ready[u32CoreNum];
}
#endif
MSVC_CH_T* MSVC_GetHandler(UINT32 u32Ch)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;

	u32CoreNum = u32Ch >> 2;
	u32InstNum = u32Ch & 0x03;

	if( u32CoreNum >= MSVC_NUM_OF_CORE )
	{
		VDEC_KDRV_Message(ERROR,"Wrong Ch Num %u %u/%u", u32Ch, u32CoreNum, u32InstNum);
		return NULL;
	}

	if( u32InstNum >= MSVC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR,"Wrong Ch Num %u %u/%u", u32Ch, u32CoreNum, u32InstNum);
		return NULL;
	}

	return (MSVC_CH_T *)&gstMsvcCh[u32Ch];
}

UINT8 MSVC_Open(UINT8 u8MsvcCh, BOOLEAN b1FrameDec, UINT32 u32CodecType)
{
	MSVC_CH_T* pstMsvcCh;
	UINT32 u32CoreNum;
	UINT32 u32InstNum;

	if( u8MsvcCh >= NUM_OF_MSVC_CHANNEL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC] open error : not enough channel in core(%d)", u8MsvcCh);
		return 0xFF;
	}

	pstMsvcCh = MSVC_GetHandler(u8MsvcCh);

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

//	if(pstMsvcCh->u8DqCh != 0xFF)
//	{
//		VDEC_KDRV_Message(ERROR, "[MSVC%d][Wrn] please check MSVC Alloc Num  %s", u8MsvcCh, __FUNCTION__ );
//	}

//	if( pstMsvcCh->u32CPB_VirAddr != 0 )
//	{
//		VDEC_ClearVirualAddr((void *)pstMsvcCh->u32CPB_VirAddr );
//		pstMsvcCh->u32CPB_VirAddr = 0;
//	}

	_msvc_ChInit(pstMsvcCh, u8MsvcCh);
	pstMsvcCh->bOpened = TRUE;

	pstMsvcCh->b1FrameDec = b1FrameDec;
//	pstMsvcCh->bDual3D = pstOpenMode->bDual3D;
//	pstMsvcCh->u8SrcMode = pstOpenMode->srcMode;	// 0:live 1:file
//	pstMsvcCh->u8NoAdaptiveStream = pstOpenMode->noAdpStr;	// 0:could be adaptive 1:no adaptive
	pstMsvcCh->u32CodecType = u32CodecType;
//	pstMsvcCh->u32CPB_StartAddr = u32CPBAddr;
//	pstMsvcCh->u32CPB_Size = u32CPBSize;
//	pstMsvcCh->u32CPB_VirAddr    = 0;	//(UINT32)VDEC_TranselateVirualAddr(u32CPBAddr, u32CPBSize);
//	pstMsvcCh->u32ChBusy = FALSE;

//	MSVC_SetErrorMBFrameFilter(u8MsvcCh, 0);
//	MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, u32InstNum, 0);
//	MSVC_HAL_InitDecFunCtrl(u32CoreNum);
//	MSVC_HAL_SetBitstreamWrPtr(u32CoreNum, u32InstNum, pstMsvcCh->u32CPB_StartAddr);
//	MSVC_HAL_SetBitstreamRdPtr(u32CoreNum, u32InstNum, pstMsvcCh->u32CPB_StartAddr);

	MSVC_Flush(u8MsvcCh);

//	pstMsvcCh->u32PreRdPtr = pstMsvcCh->u32CPB_StartAddr;
//	pstMsvcCh->u32PreAuAddr = pstMsvcCh->u32CPB_StartAddr-0x200;
//	pstMsvcCh->u32ExactRdPtr = pstMsvcCh->u32CPB_StartAddr;
	pstMsvcCh->i32PreTempRefer = -1;
	pstMsvcCh->u32CurFrameRateRes = 0;
	pstMsvcCh->u32CurFrameRateDiv = 0;
	pstMsvcCh->s32PreFrameRateDiv = -1;
//	pstMsvcCh->u32LockedFrameIdx = 0xFF;

//	VDEC_KDRV_Message(_TRACE, "[MSVC%d][DBG] Picture Size - Width:%d, Height: %d, %s", u8MsvcCh, ui32Width, ui32Height, __FUNCTION__ );

//	pstMsvcCh->u32PicWidth = ui32Width;
//	pstMsvcCh->u32PicHeight = ui32Height;

#if 0
#ifdef USE_MCU_FOR_VDEC_VDC
	TOP_HAL_EnableInterIntr(MACH0+u32CoreNum);
#else
	TOP_HAL_EnableExtIntr(MACH0+u32CoreNum);
#endif
#endif

//	pstMsvcCh->ui8Vch = ui8Vch;
//	pstMsvcCh->u8PdecCh = ui8PdecCh;
//	pstMsvcCh->u8DqCh = ui8DqCh;
//	pstMsvcCh->u8DeCh = ui8Dst;

	VDEC_KDRV_Message(_TRACE, "[MSVC%d] Codec %d", u8MsvcCh, u32CodecType);
	VDEC_KDRV_Message(_TRACE, "[MSVC%u] Start RdPtr %08X  WrPtr %08X", pstMsvcCh->u32Ch,
			MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum),
			MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum));

	return u8MsvcCh;
}

BOOLEAN MSVC_Flush(UINT8 u8MsvcCh)
{
//	UINT8 i;
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(u8MsvcCh);

	if( pstMsvcCh == NULL )
		return FALSE;

//	pstMsvcCh->u8SkipModeOn = 2;
//	pstMsvcCh->u16SkipPBCnt = 0;
//	pstMsvcCh->u32PicOption |= 1<<12;
//	pstMsvcCh->i16UnlockIndex = -2;

//	for(i=0; i<MSVC_MAX_DPB_FRAME; i++)
//		pstMsvcCh->au8DpbAge[i] = 0;

//	pstMsvcCh->u8CPBFlushReq = 1;
//	pstMsvcCh->u8OnPicDrop = 1;
//	pstMsvcCh->u8DpbLockReq = 1;
	pstMsvcCh->i32PreTempRefer = -1;

//	pstMsvcCh->u32DqDpbMark = 0;

	VDEC_KDRV_Message(_TRACE,"[MSVC%d] Flush!", u8MsvcCh);

	return TRUE;
}
#if 0
void MSVC_Reset(UINT8 ui32VdecCh)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);
	UINT32 u32CoreNum, ui32WaintCount, u32IntReason;

	u32CoreNum = pstMsvcCh->u32CoreNum;

	if(pstMsvcCh->u32Ch!=ui32VdecCh)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Ch missmatch, %s", ui32VdecCh, __FUNCTION__);
		u32CoreNum = (ui32VdecCh>>2);
	}

	MSVC_PrintStatus(u32CoreNum);

//	pstMsvcCh->bFieldDone = 0;
//	pstMsvcCh->u32ChBusy = 0;

#ifdef MSVC_RESET_TEST
	MSVC_Flush(ui32VdecCh);
#else
	pstMsvcCh->bSeqInit = 0;
#endif

	TOP_HAL_EnableExtIntr(MACH0_SRST_DONE+u32CoreNum);
	TOP_HAL_ResetMach(u32CoreNum);

	ui32WaintCount = 0;
	while(MSVC_GetRstReady(u32CoreNum)==0)
	{
#ifdef __XTENSA__
		if(TOP_HAL_IsInterIntrEnable(MACH0_SRST_DONE+u32CoreNum))
#else // !__XTENSA__
		if(TOP_HAL_IsExtIntrEnable(MACH0_SRST_DONE+u32CoreNum))
#endif // ~__XTENSA__
		{
#ifdef __XTENSA__
			u32IntReason = TOP_HAL_GetInterIntrStatus();
#else // !__XTENSA__
			u32IntReason = TOP_HAL_GetExtIntrStatus();
#endif // ~__XTENSA__

			if (u32IntReason&(1<<(MACH0_SRST_DONE+u32CoreNum)))
			{
#ifdef __XTENSA__
				TOP_HAL_ClearInterIntrMsk(1<<(MACH0_SRST_DONE+u32CoreNum));
				TOP_HAL_DisableExtIntr(MACH0_SRST_DONE+u32CoreNum);
#else //!__XTENSA__
				TOP_HAL_ClearExtIntrMsk(1<<(MACH0_SRST_DONE+u32CoreNum));
				TOP_HAL_DisableExtIntr(MACH0_SRST_DONE+u32CoreNum);
#endif //~__XTENSA__
				MSVC_SetRstReady(u32CoreNum);
				break;
			}

			if( ui32WaintCount > 50 )
			{
#ifdef __XTENSA__
				TOP_HAL_ClearInterIntrMsk(1<<(MACH0_SRST_DONE+u32CoreNum));
				TOP_HAL_DisableExtIntr(MACH0_SRST_DONE+u32CoreNum);
#else //!__XTENSA__
				TOP_HAL_ClearExtIntrMsk(1<<(MACH0_SRST_DONE+u32CoreNum));
				TOP_HAL_DisableExtIntr(MACH0_SRST_DONE+u32CoreNum);
#endif //~__XTENSA__
				VDEC_KDRV_Message(ERROR, "[MSVC%d] Missing RST Isr, %s", ui32VdecCh, __FUNCTION__);
				break;
			}
#ifndef __XTENSA__
			mdelay(1);
#endif
			ui32WaintCount++;

		}
	}

	MSVC_HAL_SetRun(u32CoreNum, 0);
	MSVC_BootCodeDown(u32CoreNum);
	MSVC_HAL_SetRun(u32CoreNum, 1);
	MSVC_ClearRstReady(u32CoreNum);

	MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, pstMsvcCh->u32InstNum, 0);
}
#endif
SINT32 MSVC_Close(UINT32 ui32VdecCh)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);
//	UINT32 uEndAddr;
	UINT32 u32CoreNum, u32InstNum;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	if(pstMsvcCh->u32Ch != ui32VdecCh)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Missmatch Channel Number : %d - %s", pstMsvcCh->u32Ch, ui32VdecCh, __FUNCTION__);
	}
#if 0
#ifdef USE_MCU_FOR_VDEC_VDC
	TOP_HAL_ClearInterIntr(MACH0+(pstMsvcCh->u32Ch>>2));
	TOP_HAL_DisableInterIntr(MACH0+(pstMsvcCh->u32Ch>>2));
#else
	TOP_HAL_ClearExtIntr(MACH0+(pstMsvcCh->u32Ch>>2));
	TOP_HAL_DisableExtIntr(MACH0+(pstMsvcCh->u32Ch>>2));
#endif

	if( MSVC_HAL_IsBusy(u32CoreNum) && MSVC_HAL_GetCurInstance(u32CoreNum) == u32InstNum )
	{
		uEndAddr = (VES_CPB_GetPhyWrPtr(pstMsvcCh->u8PdecCh)+0x1FF) & (~0x1FF);
		MSVC_UpdateWrPtr(pstMsvcCh->u32Ch, uEndAddr);

//		if( MSVC_HAL_GetCurCommand(u32CoreNum) == MSVC_SEQ_INIT )
//			MSVC_HAL_SeqInitEscape(pstMsvcCh->u32CoreNum);
	}

	if( pstMsvcCh->u32CPB_VirAddr != 0 )
	{
		VDEC_ClearVirualAddr((void *)pstMsvcCh->u32CPB_VirAddr );
		pstMsvcCh->u32CPB_VirAddr = 0;
	}
#endif
	VDEC_KDRV_Message(_TRACE, "[MSVC%d] Close Ch",	pstMsvcCh->u32Ch);
#if 0
	if( pstMsvcCh->sVdecFb.ui32NumOfFb )
	{
//		gui32MsvcMvColBufPool |= pstMsvcCh->ui32MsvcMvColBufUsed;
//		VDEC_DPB_FreeAll( &(pstMsvcCh->sVdecFb) );
		VDEC_KDRV_Message(_TRACE, "MSVC CLOSE DPB Deinit Ch %d :: %d", 	pstMsvcCh->u32Ch, pstMsvcCh->sVdecFb.ui32NumOfFb);
	}
#endif
	_msvc_ChInit(pstMsvcCh, pstMsvcCh->u32Ch);
	pstMsvcCh->bOpened = FALSE;
#if 0
	// Drop picture after close/flush
	pstMsvcCh->u8OnPicDrop = 1;

	if( MSVC_HAL_IsBusy(u32CoreNum) && MSVC_HAL_GetCurInstance(u32CoreNum) == u32InstNum )
	{
		VDEC_KDRV_Message(ERROR, "MSVC Close Fail  Ch %u :: Busy Status", pstMsvcCh->u32Ch);
		MSVC_Reset(pstMsvcCh->u32Ch);
	}

	if(MSVC_HAL_GetIntReason(u32CoreNum) && MSVC_HAL_GetCurInstance(u32CoreNum) == u32InstNum)
	{
		VDEC_KDRV_Message(ERROR, "MSVC Close Clear INTR Ch : %u", pstMsvcCh->u32Ch);
		MSVC_HAL_SetIntReason(u32CoreNum, 0x0);
		MSVC_HAL_ClearInt(u32CoreNum, 0x1);
	}
#endif

	return 0;
}
#if 0
int MSVC_GetUsedDPB(UINT8 ui32VdecCh)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);
	UINT32 u32FrameFlag, i, n;

	if(pstMsvcCh==NULL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC %d] %s - pstMsvcCh is NULL", ui32VdecCh, __FUNCTION__);
		return -1;
	}

	if(pstMsvcCh->u32Ch != ui32VdecCh)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Missmatch Channel Number : %d - %s", pstMsvcCh->u32Ch, ui32VdecCh, __FUNCTION__);
		return -1;
	}

	u32FrameFlag = MSVC_HAL_GetFrameDisplayFlag(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum);
	n=0;

	for(i=0; i<pstMsvcCh->stFrameBufInfo.u32FrameNum; i++)
	{
		if(u32FrameFlag & (1<<i)) n++;
	}

	return n;
}

UINT32 MSVC_GetDecCount(MSVC_CH_T* pstMsvcCh)
{
	return pstMsvcCh->u32DecCount;
}
#endif
SINT32 MSVC_SetUserDataEn(UINT32 u32MsvcCh, UINT32 u32Enable)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(u32MsvcCh);

	if( u32Enable )
	{
		pstMsvcCh->u8UserDataEn = TRUE;
//		pstMsvcCh->u32PicOption |= 0x20;
	}
	else
	{
		pstMsvcCh->u8UserDataEn = FALSE;
//		pstMsvcCh->u32PicOption &= ~0x20;
	}

	return 0;
}
#if 0
BOOLEAN MSVC_SetPicScanMode(UINT32 ui32VdecCh, UINT8 ui8ScanMode)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);

	if(pstMsvcCh==NULL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC %d] %s - pstMsvcCh is NULL", ui32VdecCh, __FUNCTION__);
		return FALSE;
	}

	pstMsvcCh->u32SkipNum = 0;
#if 0
	pstMsvcCh->u32PicOption &= ~0x1C;
	if(ui8ScanMode==1)
		pstMsvcCh->u32PicOption |= 0x4;
	else if(ui8ScanMode>1)
	{
		pstMsvcCh->u32PicOption |= ((ui8ScanMode&0x3)<<3);
		pstMsvcCh->u32SkipNum = 1;
	}
#else
	pstMsvcCh->u32PicOption &= ~0x18;

	if(ui8ScanMode>0)
	{
		pstMsvcCh->u32PicOption |= ((ui8ScanMode&0x3)<<3);
		pstMsvcCh->u32SkipNum = 1;
	}
#endif

	VDEC_KDRV_Message(_TRACE, "[MSVC %d] %s - ui32ScanMode %d:%d ", ui32VdecCh,
			__FUNCTION__, ui8ScanMode, pstMsvcCh->u32PicOption);
	return TRUE;
}

BOOLEAN MSVC_SetSrcScanType(UINT32 ui32VdecCh, UINT8 ui32ScanType)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);

	if(pstMsvcCh==NULL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC %d] %s - pstMsvcCh is NULL", ui32VdecCh, __FUNCTION__);
		return FALSE;
	}

	if( ui32ScanType == MSVC_B_SKIP )
		pstMsvcCh->u32PicOption |= (1<<11);		// Enable Trick Ref. List Ctrl. Mode
	else
		pstMsvcCh->u32PicOption &= ~(1<<11);

	VDEC_KDRV_Message(_TRACE, "[MSVC %d] %s - SrcType %d:%d ", ui32VdecCh,
			__FUNCTION__, ui32ScanType, pstMsvcCh->u32PicOption);

	return TRUE;
}
#endif
BOOLEAN MSVC_GetUserDataInfo(UINT32 ui32VdecCh, UINT32 *pui32Address, UINT32 *pui32Size)
{
	t_CoreInfo *pstCoreInfo;
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);

	if(pstMsvcCh==NULL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC %d] %s - pstMsvcCh is NULL", ui32VdecCh, __FUNCTION__);
		return FALSE;
	}

	pstCoreInfo = MSVC_HAL_GetCoreInfo(pstMsvcCh->u32CoreNum);

	*pui32Address = pstCoreInfo->u32UserBufStartAddr;
	*pui32Size = pstCoreInfo->u32UserBufSize;

	return TRUE;
}

t_InstanceInfo* MSVC_GetInstanceInfo(t_CoreInfo* pstrCoreInfo, UINT32 u32InstNum)
{
	if( u32InstNum >= MSVC_NUM_OF_CHANNEL )
		return 0;

	return &pstrCoreInfo->stInstInfo[u32InstNum];
}
#if 0
/***
 *  return val :
 *				0  : No mismatch
 *				1  : MSVC Rd Pointer Exceed
 *				-1 : AU Start Address Exceed
 */
SINT32 _msvc_CheckRdPtrMismatch(MSVC_CH_T *pstMsvcCh, CQ_ENTRY_T *pstCqEntry)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32CurMSVC_Rptr, u32PreMSVC_Rptr;
	UINT32 u32CurAU_StartAddr, u32PreAU_StartAddr;
	UINT32 u32CPB_EndAddr;
	SINT32 i32Delta=0;
	UINT32 u32RetVal = 0;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	u32CurMSVC_Rptr = pstMsvcCh->u32ExactRdPtr;

	if( pstCqEntry->auEnd == 0)
	{
		pstCqEntry->auEnd = MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum) + 0x1FF;
		pstCqEntry->auEnd &= ~0x1FF;
	}

	u32CurAU_StartAddr = pstCqEntry->auAddr;

	if( pstCqEntry->auType == SEQUENCE_END )
		return 0;

	u32CPB_EndAddr = pstMsvcCh->u32CPB_StartAddr + pstMsvcCh->u32CPB_Size;
	u32PreAU_StartAddr = pstMsvcCh->u32PreAuAddr;

	// MSVC Exceed Check
	// We want PreMSVC_Rd<CurMSVC_Rd<=AU_Start
	u32PreMSVC_Rptr = pstMsvcCh->u32PreRdPtr;
	u32CurMSVC_Rptr += pstMsvcCh->u32BodaRdEx;
	if( u32CurMSVC_Rptr >= u32CPB_EndAddr)
		u32CurMSVC_Rptr -= pstMsvcCh->u32CPB_Size;
	pstMsvcCh->u32BodaRdEx = 0;

	// Normal case
	if (u32PreMSVC_Rptr <= u32CurMSVC_Rptr)
	{
		if (u32CurMSVC_Rptr > u32CurAU_StartAddr &&
				u32PreMSVC_Rptr <= u32CurAU_StartAddr)
			u32RetVal = 1;
	}
	// Wrap around case
	else
	{
		if(u32PreMSVC_Rptr > (u32CurAU_StartAddr))
		{
			if(u32CurMSVC_Rptr > u32CurAU_StartAddr)
				u32RetVal = 1;
		}
		else
		{
			if(u32CurAU_StartAddr > u32PreMSVC_Rptr)
				u32RetVal = 1;
		}
	}

	if ( u32RetVal != 0)
		goto mismatch_found;

	//// AU Start Address Exceed Check	////
	// shift to make previous AU be CPB start address for calculation simplicity
	i32Delta = pstMsvcCh->u32CPB_StartAddr - u32PreAU_StartAddr;
	u32CurAU_StartAddr += i32Delta;
	u32PreAU_StartAddr += i32Delta;
	u32CurMSVC_Rptr += i32Delta;

	if(u32CurAU_StartAddr < pstMsvcCh->u32CPB_StartAddr)
		u32CurAU_StartAddr += pstMsvcCh->u32CPB_Size;

	if(u32CurMSVC_Rptr < pstMsvcCh->u32CPB_StartAddr)
		u32CurMSVC_Rptr += pstMsvcCh->u32CPB_Size;

	if( u32CurMSVC_Rptr == u32PreAU_StartAddr || u32CurMSVC_Rptr > u32CurAU_StartAddr+0x10000 )
		u32RetVal = -1;


mismatch_found:
	if ( u32RetVal )
	{
		VDEC_KDRV_Message(DBG_VDC, "Ch%d Mismch %d preM %08X curM %08X(Ex%08X) PreAU %08X AU %08X dif %d",
				pstMsvcCh->u32Ch,
				u32RetVal, u32PreMSVC_Rptr-i32Delta,
				u32CurMSVC_Rptr-i32Delta, pstMsvcCh->u32ExactRdPtr,
				pstMsvcCh->u32PreAuAddr, pstCqEntry->auAddr,
				pstCqEntry->auAddr - u32CurMSVC_Rptr );

		VDEC_KDRV_Message(DBG_VDC, " ##  Mismch %d d %d preAu %08X curAu %08X preM %08X curM %08X",
				u32RetVal, i32Delta, u32PreAU_StartAddr, u32CurAU_StartAddr,
				u32PreMSVC_Rptr, u32CurMSVC_Rptr);
	}

	return u32RetVal;

}

SINT32 _msvc_CheckRdPtrMismatch2(MSVC_CH_T *pstMsvcCh, CQ_ENTRY_T *pstCqEntry)
{
	UINT32 u32CurMSVC_Rptr;
	UINT32 u32PreMSVC_Rptr;
	UINT32 u32CurAU_StartAddr;
	UINT32 u32RetVal = 0;
	SINT32 i32Delta = 0;

	u32PreMSVC_Rptr = pstMsvcCh->u32PreRdPtr;
	u32CurMSVC_Rptr = pstMsvcCh->u32ExactRdPtr;
	u32CurAU_StartAddr = pstCqEntry->auAddr;

	//// AU Start Address Exceed over MSVC RdPtr Check	////
	// shift to make previous MSVC ReadPtr be CPB start address for calculation simplicity
	i32Delta = pstMsvcCh->u32CPB_StartAddr - u32PreMSVC_Rptr;
	u32CurAU_StartAddr += i32Delta;
	u32CurMSVC_Rptr += i32Delta;

	if(u32CurAU_StartAddr < pstMsvcCh->u32CPB_StartAddr)
		u32CurAU_StartAddr += pstMsvcCh->u32CPB_Size;

	if(u32CurMSVC_Rptr < pstMsvcCh->u32CPB_StartAddr)
		u32CurMSVC_Rptr += pstMsvcCh->u32CPB_Size;

	if(u32CurMSVC_Rptr <= u32CurAU_StartAddr &&
			u32CurAU_StartAddr < pstMsvcCh->u32CPB_StartAddr + (pstMsvcCh->u32CPB_Size>>1))
		u32RetVal = -1;

	return u32RetVal;
}

static SINT32 MSVC_ProcessCQEntry(MSVC_CH_T *pstMsvcCh, CQ_ENTRY_T *pstCqEntry)
{
	SINT32 s32Result = -1;
	UINT32 u32RdPtrFw = 0;

#ifdef USE_MCU_FOR_VDEC_VDC
	if(TOP_HAL_IsExtIntrEnable(MACH0+pstMsvcCh->u32CoreNum)==0)
		TOP_HAL_EnableInterIntr(MACH0+pstMsvcCh->u32CoreNum);
#else
	if(TOP_HAL_IsInterIntrEnable(MACH0+pstMsvcCh->u32CoreNum)==0)
		TOP_HAL_EnableExtIntr(MACH0+pstMsvcCh->u32CoreNum);
#endif

	memcpy(&pstMsvcCh->stLastCmd, pstCqEntry, sizeof(CQ_ENTRY_T));

	// SEQ_HDR is a kind of anchor for lipsync!!
	if( pstMsvcCh->u8CPBFlushReq
			&& pstMsvcCh->bSeqInit)
		pstCqEntry->flush = 1;


	if( pstMsvcCh->u8CPBFlushReq )
	{
		pstMsvcCh->u8CPBFlushReq = 0;

		MSVC_HAL_SetFrameDisplayFlag(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum, 0);
		pstMsvcCh->u16NewDpbId = (++pstMsvcCh->u16DpbIdCnt);
	}

	if (pstCqEntry->flush == 1 && pstCqEntry->bRingBufferMode == TRUE)
	{
		if(pstMsvcCh->bSeqInit == 0)	// Is invalid state?
		{

		}
		else if( pstCqEntry->auAddr < pstMsvcCh->u32CPB_StartAddr ||	// Is in invalid range?
				pstCqEntry->auAddr >= pstMsvcCh->u32CPB_StartAddr + pstMsvcCh->u32CPB_Size)
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC%d], Flush range error. Req 0x%08X is not in 0x%08X~0x%08X)",
					pstMsvcCh->u32Ch, pstCqEntry->auAddr, pstMsvcCh->u32CPB_StartAddr,
					pstMsvcCh->u32CPB_StartAddr + pstMsvcCh->u32CPB_Size-1);

		}
		else	// OK for flush
		{
			UINT32 u32PreRd = MSVC_HAL_GetBitstreamRdPtr( pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum);
			pstMsvcCh->u32BodaRdEx = 0;
			pstMsvcCh->u32ExactRdPtr = pstCqEntry->auAddr;
			MSVC_HAL_Flush( pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum, 1, pstCqEntry->auAddr);
			VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] Flush! Req %08X->0x%08X ~== RdPtr 0x%08X ??",
					pstMsvcCh->u32Ch, u32PreRd, pstCqEntry->auAddr,
					MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum));
		}
	}

	VDEC_KDRV_Message(dbgPrtLvCqEnt, "[MSVC%d] Typ %d curM %08X  AUS %08X dif %d  AUE %08X Pre %08X",
			pstMsvcCh->u32Ch, pstCqEntry->auType,
			MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
			pstCqEntry->auAddr, pstCqEntry->auAddr-
			MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
			MSVC_HAL_GetBitstreamWrPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
			pstMsvcCh->u32PreRdPtr
			);

	switch( pstCqEntry->auType )
	{
		case NOSKIP_B_PICTURE:
		case SKIP_B_PICTURE:
		case P_PICTURE:
		case I_PICTURE:
			MSVC_PicRun(pstMsvcCh, pstCqEntry);
			s32Result = 0;
			break;

		case SEQUENCE_HEADER:
			if(pstMsvcCh->bSeqInit==0)
			{
				MSVC_SeqInit(pstMsvcCh, pstCqEntry);
				s32Result = 0;
			}
			else
			{
				UINT32 u32Thold;

				u32RdPtrFw = 1;

				if( pstMsvcCh->u8SkipModeOn )
				{
					if( pstMsvcCh->u8SrcMode == 1 )
						u32Thold = MSVC_PB_SKIP_THD_FILE;
					else
						u32Thold = MSVC_PB_SKIP_THD;

					pstMsvcCh->u16SkipPBCnt = (u32Thold*10)-30;
					pstMsvcCh->u8CancelRA = TRUE;
					VDEC_KDRV_Message(ERROR,"[MSVC%d] SEQ HDR during I pic search", pstMsvcCh->u32Ch);
				}
			}

			break;
		case SEQUENCE_END:
			u32RdPtrFw = 1;
			break;

		default:
			break;
	}

	if( u32RdPtrFw )
	{
		UINT32 u32PreMSVC_Rptr;

		u32PreMSVC_Rptr = pstMsvcCh->u32PreRdPtr;
		u32PreMSVC_Rptr += 0x200;
		if( u32PreMSVC_Rptr >= pstMsvcCh->u32CPB_StartAddr + pstMsvcCh->u32CPB_Size)
			u32PreMSVC_Rptr -= pstMsvcCh->u32CPB_Size;

		pstMsvcCh->u32PreRdPtr = u32PreMSVC_Rptr;
		pstMsvcCh->u32BodaRdEx += 0x200;
	}

	return s32Result;
}

/**********************************************************************************
					Export Functions
 **********************************************************************************/
static SINT32 _msvc_au_start_code_check(UINT32 u32StreamFormat, volatile UINT32* pu32AuVirAddr)
{
	UINT32 u32StartCode;
	SINT32 i32Ret = NOT_OK;

	u32StartCode = *pu32AuVirAddr;

	switch(u32StreamFormat)
	{
		case MP2_DEC:
		case AVC_DEC:
			if( (u32StartCode&0xFFFFFF) == 0x010000 || (u32StartCode&0xFFFFFFFF) == 0x01000000)
				i32Ret = OK;
			break;
		default:
			i32Ret = OK;
			break;
	}

	if(i32Ret != OK)
		VDEC_KDRV_Message(ERROR, "	Wrong Start Code 0x%08X (StmFormat %u)", u32StartCode, u32StreamFormat);

	return i32Ret;
}

SINT32 MSVC_SendCmd(UINT8 ui8VdecCh, CQ_ENTRY_T *pstCqEntry)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	MSVC_CH_T *pstMsvcCh;
	static UINT32 u32DbgCmdNum=0;
	UINT32 u32Skip = FALSE;
	SINT32 s32Result = -1;
	UINT32 ui32WtrAddr;
	UINT32 u32FieldPic=FALSE;

	if( ! (u32DbgCmdNum % (30*30)) )
		VDEC_KDRV_Message(MONITOR, "[MSVC%d] #%u Cq 0x%08X",
				ui8VdecCh, u32DbgCmdNum, (UINT32)pstCqEntry);
	u32DbgCmdNum++;

	pstMsvcCh = MSVC_GetHandler(ui8VdecCh);

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	ui32WtrAddr = VES_CPB_GetPhyWrPtr(pstMsvcCh->u8PdecCh) & (~0x1FF);
	if( u8DebugModeOn == FALSE )
		MSVC_UpdateWrPtr(pstMsvcCh->u32Ch, ui32WtrAddr);

	if( MSVC_HAL_IsBusy(u32CoreNum) && MSVC_HAL_GetCurInstance(u32CoreNum) == u32InstNum)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d][Err] State inconsistant", pstMsvcCh->u32Ch);
		goto error;
	}

	if(pstMsvcCh->bFieldDone==TRUE)
	{
		VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] skip -> Field Picture", pstMsvcCh->u32Ch);
		pstMsvcCh->bFieldDone = FALSE;
		u32FieldPic = TRUE;
		//u32Skip = 1;
	}

	pstCqEntry->flush = FALSE;
	pstCqEntry->bDecAgain = FALSE;
	if( !pstCqEntry->flush && pstMsvcCh->u32DynamicBufEn == FALSE && u32Skip == 0 )
	{
		s32Result = _msvc_CheckRdPtrMismatch(pstMsvcCh, pstCqEntry);
		if( s32Result == 1)	// need to skip
		{
			if( pstCqEntry->auType != SEQUENCE_HEADER )
			{
				u32Skip = TRUE;

				if( u32FieldPic == FALSE )
				VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] skip -> Typ %d curM %08X  AUS %08X dif %d",
						pstMsvcCh->u32Ch, pstCqEntry->auType,
						MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
						pstCqEntry->auAddr, pstCqEntry->auAddr-
						MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum)
						);
			}
		}
		else if( s32Result == -1)
		{
			if( pstCqEntry->auType == SEQUENCE_HEADER )
			{
				pstCqEntry->flush = TRUE;

				VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] Enfoced Rd set -> Typ %d curM %08X  AUS %08X dif %d",
						pstMsvcCh->u32Ch, pstCqEntry->auType,
						MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
						pstCqEntry->auAddr, pstCqEntry->auAddr-
						MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum)
						);
			}
			else
			{
				if( pstMsvcCh->u32CodecType == VDC_CODEC_MPEG2_HP )
					pstCqEntry->bDecAgain = TRUE;
			}
		}
	}

	if(u32Skip==TRUE)
	{
		s32Result = -1;
//		VDEC_KDRV_Message(DBG_VDC, "[MSVC%d][DBG] Check Skip Condition : %d", pstMsvcCh->u32Ch, pstMsvcCh->bFieldDone);
		goto error;
	}

	// Start Code Check
	if( 0 /*pstMsvcCh->u32CPB_VirAddr != 0*/ )
	{
		UINT32 *pu32VirAuStart;
		SINT32 i32Ret;

		pu32VirAuStart = (UINT32*)(pstMsvcCh->u32CPB_VirAddr +
				(pstCqEntry->auAddr - pstMsvcCh->u32CPB_StartAddr));

		i32Ret = _msvc_au_start_code_check(
				MSVC_HAL_GetStreamFormat(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum),
				pu32VirAuStart
				);

		if( i32Ret == NOT_OK )
			VDEC_KDRV_Message(ERROR, "[MSVC%d] AU Start Code Error AuAddr %08X",
					pstMsvcCh->u32Ch, pstCqEntry->auAddr);
	}

	s32Result = MSVC_ProcessCQEntry(pstMsvcCh, pstCqEntry);

error:
	return s32Result;
}

SINT32 MSVC_UpdateWrPtr(UINT32 ui32VdecCh, UINT32 u32WrPtr)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32PrevWrPtr;
	SINT32 iResult = -1;
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);

	// In Dynamic Buff(ie. File play)
	// It's useless
	if( pstMsvcCh->u32DynamicBufEn == TRUE )
		return iResult;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	u32PrevWrPtr = MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum);
	if(u32PrevWrPtr != u32WrPtr)
	{
		MSVC_HAL_SetBitstreamWrPtr(u32CoreNum, u32InstNum, u32WrPtr);
		iResult = 0;
	}


	return iResult;
}

void MSVC_SetWrPtr(UINT32 ui32VdecCh)
{
	UINT32 u32WrPtr;

	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);

	u32WrPtr = VES_CPB_GetPhyWrPtr(pstMsvcCh->u8PdecCh) & (~0x1FF);

	if(u32WrPtr != 0)
		MSVC_UpdateWrPtr(ui32VdecCh, u32WrPtr);
	else
		VDEC_KDRV_Message(ERROR, "[MSVC %d][Err] Get Pdec WritePtr %s", ui32VdecCh, __FUNCTION__ );
}

UINT32 MSVC_GetExactRdPtr(UINT32 u32ChNum)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32ExactRdPtr;

	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(u32ChNum);

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	u32ExactRdPtr = RET_DEC_PIC_EXACT_RDPTR(u32CoreNum) + 0x1FF;
	u32ExactRdPtr &= ~(0x1FF);

	if( u32ExactRdPtr < pstMsvcCh->u32CPB_StartAddr )
	{
		// BODA F/W Bug
		// Workaround
		VDEC_KDRV_Message(DBG_VDC, "[MSVC] wrong boda read point %08X", u32ExactRdPtr);
		u32ExactRdPtr += pstMsvcCh->u32CPB_Size;
	}

	return u32ExactRdPtr;
}

SINT32 MSVC_CancelSeqInit(UINT32 ui32VdecCh)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(ui32VdecCh);
//	UINT32 uEndAddr;

	VDEC_KDRV_Message(_TRACE, "MSVC_CancelSeqInit Ch %d", pstMsvcCh->u32Ch);

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	if(MSVC_HAL_IsBusy(u32CoreNum))
	{
		MSVC_HAL_SeqInitEscape(u32CoreNum);
	}
	return 0;
}

void MSVC_SeqInit(MSVC_CH_T *pstMsvcCh, CQ_ENTRY_T *pstCqEntry)
{
	SEQ_PARAM_T stSeqParam;
	SEQ_OPTION_T *pstSeqOp;
	UINT32 u32CodecType;
	UINT32 u32CoreNum;
	UINT32 u32InstNum;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	u32CodecType = pstMsvcCh->u32CodecType;

	if( pstMsvcCh->u32ChBusy )
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Channel Busy at SeqInit", pstMsvcCh->u32ChBusy);
	pstMsvcCh->u32ChBusy = TRUE;

	// Always enable userdata for AFD info reporting
	pstMsvcCh->u32PicOption |= 0x20;

	// Set Sequence Init Parameters
	stSeqParam.u32BitStreamFormat = stMSVCStdInfo[u32CodecType].bitStreamFormat;
	stSeqParam.u32AuxStd = stMSVCStdInfo[u32CodecType].u32AuxStd;
	stSeqParam.u32BitStreamBuffer = pstMsvcCh->u32CPB_StartAddr;
	stSeqParam.u32BitStreamBufferSize = pstMsvcCh->u32CPB_Size;
	stSeqParam.u32PicWidth = pstMsvcCh->u32PicWidth;
	stSeqParam.u32PicHeight = pstMsvcCh->u32PicHeight;
	stSeqParam.u32SeqOption = (UINT32)stMSVCStdInfo[u32CodecType].u32SeqOption;

//	if( u32CodecType == MSVC_COD_VC1_AP || u32CodecType == MSVC_COD_H264)
//		pstCqEntry->bRingBufferMode = TRUE;

	// Use PDEC(Broadcasting Mode)
	if( pstCqEntry->bRingBufferMode == TRUE )
	{
		pstSeqOp = (SEQ_OPTION_T*)&stSeqParam.u32SeqOption;
		pstSeqOp->FilePlayEn = 0;
		pstSeqOp->DecDynBufAllocEn = 0;
		pstMsvcCh->u32DynamicBufEn = FALSE;
	}
	else
	{
		pstMsvcCh->u32DynamicBufEn = TRUE;
	}

	stSeqParam.u32SeqAspClass = stMSVCStdInfo[u32CodecType].seqAspClass;
	// ~Set Sequence Init Parameters

	//u32Wptr = CPB_HAL_GetWritePtr();

//	MSVC_HAL_SetBitstreamRdPtr(u32CoreNum, u32InstNum, pstCqEntry->auAddr);
	VDEC_KDRV_Message(_TRACE, "[MSVC%d] Seq Init Req, Rd %08X Wr %08X Au %08X FilePlay %d",
			pstMsvcCh->u32Ch, MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum),
			MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum),
			pstCqEntry->auAddr,	pstMsvcCh->u32DynamicBufEn
			);

	MSVC_HAL_SeqInit(u32CoreNum, u32InstNum, &stSeqParam);
}

SINT32 MSVC_SetErrorMBFrameFilter(UINT8 u8MsvcCh, UINT32 u32nErrorMb)
{
	MSVC_CH_T* pstMsvcCh = MSVC_GetHandler(u8MsvcCh);

	if(pstMsvcCh==NULL)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Error %s", u8MsvcCh, __FUNCTION__);
		return -1;
	}

	pstMsvcCh->u32ErrorMBFilter = u32nErrorMb;

	return OK;
}

void MSVC_PicRun(MSVC_CH_T* pstMsvcCh, CQ_ENTRY_T *pstCqEntry)
{
	DEC_PARAM_T stDecParam;
	UINT32 u32CoreNum, u32InstNum;
	UINT32 u32Gstc;
	UINT32 u32ChunkSize;
	UINT32 u32DisplayMark, u32ClearMark=0;
	S_DISPCLEARQ_BUF_T stClearQ;
	//UINT32 u32STC;
	//static UINT32 u32PreSTC=0;
	static UINT32 u32PrevMark=0;
	UINT32 u32FrameIdx=0;
	UINT32 u32DpbId=0;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	if( pstMsvcCh->u32ChBusy )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Can't do picrun : busy(%d %d)!!!",
				pstMsvcCh->u32Ch, pstMsvcCh->u32ChBusy, MSVC_HAL_IsBusy(u32CoreNum));
		//if( !MSVC_HAL_IsBusy(u32CoreNum) )
		//	pstMsvcCh->u32ChBusy = FALSE;
	}
	pstMsvcCh->u32ChBusy = TRUE;

	if( pstMsvcCh->u8DpbLockReq && pstMsvcCh->u8DeCh != 0xFF )
	{
		UINT32 u32LockFrameIdx;

		pstMsvcCh->u8DpbLockReq = 0;
		u32LockFrameIdx = DE_IF_GetRunningFrameIndex(pstMsvcCh->u8DeCh);
		u32LockFrameIdx &= 0xFFFF;

		if(u32LockFrameIdx>=0 && u32LockFrameIdx<pstMsvcCh->stFrameBufInfo.u32FrameNum)
		{
			pstMsvcCh->u32LockedFrameIdx = u32LockFrameIdx;
			MSVC_HAL_LockDpbFrame(u32CoreNum, u32InstNum, u32LockFrameIdx);
			VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] Lock DPB Frame #%d", pstMsvcCh->u32Ch, u32LockFrameIdx);
		}
		else
			VDEC_KDRV_Message(DBG_VDC,"invalid lock frame %d\n", u32LockFrameIdx);
	}

	if( pstCqEntry->auEnd > pstCqEntry->auAddr )
	{
		u32ChunkSize = (pstCqEntry->auEnd - pstCqEntry->auAddr);
	}
	else
	{
		u32ChunkSize = (pstMsvcCh->u32CPB_StartAddr + pstMsvcCh->u32CPB_Size - pstCqEntry->auAddr)
			+ ((pstCqEntry->auEnd) - pstMsvcCh->u32CPB_StartAddr);
	}


	stDecParam.u32AuAddr = pstCqEntry->auAddr;
	stDecParam.u32ChunkSize = u32ChunkSize;
	stDecParam.u32PicOption = pstMsvcCh->u32PicOption;
	stDecParam.u32SkipNum = pstMsvcCh->u32SkipNum;

	//MSVC_PicRunConfig(pstrChInfo, &stDecParam);

	// Save MSVC Rd Ptr for checking error resilience
	pstMsvcCh->u32PreRdPtr = pstMsvcCh->u32ExactRdPtr;
	pstMsvcCh->u32PreAuAddr = pstCqEntry->auAddr;

	u32DisplayMark = MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum);
	while( DISP_CLEAR_Q_Pop( pstMsvcCh->u8DqCh, &stClearQ) )
	{
		u32FrameIdx = stClearQ.ui32FrameIdx & 0xFFFF;
		u32DpbId = stClearQ.ui32FrameIdx >> 16;

		if( pstMsvcCh->i16UnlockIndex == u32FrameIdx )
			pstMsvcCh->i16UnlockIndex = -3;

		if( pstMsvcCh->i16UnlockIndex >= 0 )
		{
			VDEC_KDRV_Message(DBG_VDC,"[MSVC%d] Try to clear old index %d(wait idx %d)",
					pstMsvcCh->u32Ch, u32FrameIdx, pstMsvcCh->i16UnlockIndex);

			pstMsvcCh->i16UnlockIndex = -3;
			//continue;
		}

		if( u32FrameIdx >= pstMsvcCh->stFrameBufInfo.u32FrameNum)
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC%d] Too Big Clear Idx %u >= %u",
					pstMsvcCh->u32Ch, u32FrameIdx, pstMsvcCh->stFrameBufInfo.u32FrameNum );
		}
		else
		{
#ifdef DPB_ID_USE
			// bit wrap around(carry)
			if( u32DpbId < pstMsvcCh->u16NewDpbId )
				u32DpbId += 0xFFFF;

			// Locked DPB frame clear
			if( u32FrameIdx == pstMsvcCh->u32LockedFrameIdx )
			{
				u32DpbId = pstMsvcCh->u16NewDpbId;
				pstMsvcCh->u32LockedFrameIdx = 0xFF;
			}

			if( u32DpbId - pstMsvcCh->u16NewDpbId < 0xFF )
#else
			if(1)
#endif
			{
				pstMsvcCh->u32ClearCount++;
				u32ClearMark |= 1<<u32FrameIdx;
				pstMsvcCh->u32DqDpbMark &= ~(1<<u32FrameIdx);
			}
			else
			{
				VDEC_KDRV_Message(DBG_VDC,"[MSVC%d] old index ID %d(%d)  idx %d",
						pstMsvcCh->u32Ch, stClearQ.ui32FrameIdx>>16,
						pstMsvcCh->u16NewDpbId, u32FrameIdx);
			}
		}

		if( !(u32DisplayMark & (1<<u32FrameIdx)) )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC%d] DispMark: 0x%X(prev %x), ClrMark: 0x%X, FrmIdx: %d",
					pstMsvcCh->u32Ch, u32DisplayMark, u32PrevMark, u32ClearMark, u32FrameIdx);
			VDEC_KDRV_Message(ERROR, "	DQ%d Display Q: %d, Display Clear Q: %d",
					pstMsvcCh->u8DqCh, DISP_Q_NumActive(pstMsvcCh->u8DqCh), DISP_CLEAR_Q_NumActive(pstMsvcCh->u8DqCh));
		}
	}

	u32DisplayMark = u32DisplayMark & (~u32ClearMark);
	u32PrevMark = u32DisplayMark;

	MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, u32InstNum, u32DisplayMark);

	_msvc_cmd_ram_log(u32CoreNum, u32InstNum, pstCqEntry, u32ClearMark);

	//VDEC_KDRV_Message(DBG_MSVC2, "DispFlg 0x%08X", MSVC_HAL_GetFrameDisplayFlag( u32CoreNum, u32InstNum));
/*
	u32STC = LQ_HAL_GetSTC(pstMsvcCh->u32LQch);
	VDEC_KDRV_Message(DBG_VDC, "STC %X DTS %X", LQ_HAL_GetSTC(pstMsvcCh->u32LQch), pstCqEntry->dts);

	if( u32STC - u32PreSTC < 500 )
	{
		VDEC_KDRV_Message(ERROR, "Frame %d - STC %08X  PreSTC %08X  Diff %04X  DTS %08X",
			MSVC_GetDecCount(pstMsvcCh), u32STC, u32PreSTC, u32STC - u32PreSTC, pstCqEntry->dts);
	}
	u32PreSTC = u32STC;
*/

	pstMsvcCh->u32AuSize = pstCqEntry->size;

	// prevent to drop pic after new dec
	pstMsvcCh->u8OnPicDrop = 0;

	u32Gstc = TOP_HAL_GetGSTCC();
	msvc_dbgPoss[u32CoreNum][0] += msvc_dbgCmdTs[u32CoreNum][1]-msvc_dbgCmdTs[u32CoreNum][0]; // busy
	msvc_dbgPoss[u32CoreNum][1] += u32Gstc-msvc_dbgCmdTs[u32CoreNum][1]; // idle

	msvc_dbgCmdTs[u32CoreNum][0] = u32Gstc;

	MSVC_HAL_PicRun(u32CoreNum, u32InstNum, &stDecParam);
}
#endif
/******************************************************************
  Internal Functions
 *******************************************************************/
static SINT32 msvc_ReportSeqInfo(MSVC_CH_T *pstMsvcCh, SEQ_INFO_T *pstSeqInfo)
{
	S_MSVC_DONE_INFO_T msvc_seqinfo_param;

	msvc_seqinfo_param.eMsvcDoneHdr = MSVC_DONE_HDR_SEQINFO;
	msvc_seqinfo_param.u.seqInfo.u32IsSuccess = pstSeqInfo->u32IsSuccess;
	msvc_seqinfo_param.u.seqInfo.bOverSpec = pstSeqInfo->bOverSpec;
	msvc_seqinfo_param.u.seqInfo.u32PicWidth = pstSeqInfo->u32PicWidth;
	msvc_seqinfo_param.u.seqInfo.u32PicHeight = pstSeqInfo->u32PicHeight;
	msvc_seqinfo_param.u.seqInfo.u32FrameRateRes = pstSeqInfo->u32FrameRateRes;
	msvc_seqinfo_param.u.seqInfo.u32FrameRateDiv = pstSeqInfo->u32FrameRateDiv;
//	msvc_seqinfo_param.u.seqInfo.u32FrmCnt = pstSeqInfo->u32FrmCnt;
//	msvc_seqinfo_param.u.seqInfo.u32MinFrameBufferCount = pstSeqInfo->u32MinFrameBufferCount;
	msvc_seqinfo_param.u.seqInfo.ui32FrameBufDelay = pstSeqInfo->u32FrameBufDelay;
	msvc_seqinfo_param.u.seqInfo.u32AspectRatio = pstSeqInfo->u32AspectRatio;
//	msvc_seqinfo_param.u.seqInfo.u32DecSeqInfo = pstSeqInfo->u32DecSeqInfo;
//	msvc_seqinfo_param.u.seqInfo.u32NextDecodedIdxNum = pstSeqInfo->u32NextDecodedIdxNum;
	msvc_seqinfo_param.u.seqInfo.u32Profile = pstSeqInfo->u32Profile;
	msvc_seqinfo_param.u.seqInfo.u32Level = pstSeqInfo->u32Level;
	msvc_seqinfo_param.u.seqInfo.u32ProgSeq = pstSeqInfo->u32ProgSeq;
//	msvc_seqinfo_param.u.seqInfo.u32VideoStandard = MSVC_HAL_GetStreamFormat(pstMsvcCh->u32CoreNum,
//			pstMsvcCh->u32InstNum);
//	msvc_seqinfo_param.u.seqInfo.u32CurRdPtr = u32CurRdPtr;
	msvc_seqinfo_param.u.seqInfo.ui32RefFrameCount = pstSeqInfo->u32MinFrameBufferCount;

	VDEC_KDRV_Message(_TRACE, "[MSVC%d][SEQ] Width: %d, Height: %d, MinFrameBufferCount: %d, AspectRatio: %d",
			pstMsvcCh->u32Ch,
			pstSeqInfo->u32PicWidth,
			pstSeqInfo->u32PicHeight,
			pstSeqInfo->u32MinFrameBufferCount,
			pstSeqInfo->u32AspectRatio);

	MSVC_ADP_ISR_RunDone((UINT8)pstMsvcCh->u32Ch, &msvc_seqinfo_param);

	return OK;
}

static void msvc_CheckHeaderInfo(UINT32 u32StreamFormat, SEQ_INFO_T *pstSeqInfo)
{
	UINT32 u32Temp = 0;
	SINT32 s32Profile = 0;
	SINT32 s32Level = 0;
	SINT32 s32InterlacedSeq = 0;
	SINT32 s32Direct8x8Flag = 0;

	if(u32StreamFormat == AVC_DEC)
	{
		u32Temp = pstSeqInfo->u32SeqHeadInfo;
		s32Profile = (u32Temp >> 0)  & 0xFF;
		s32Level =  (u32Temp >> 8)  & 0xFF;
		s32InterlacedSeq = (u32Temp >> 16) & 0x01;
		s32Direct8x8Flag= (u32Temp >> 17) & 0x01;

		if(s32Profile == 66)
		{
			_printf( "Profile            :- BASELINE (%d)\n", s32Profile);
#if 0
			if(s32Level == 12)
				//pPdecReg->frame_interval = 6000; // 15 Frames per second
				//else
				///pPdecReg->frame_interval = 3000; // 30 Frames per second
#endif
		}
		else if(s32Profile == 77)
		{
			_printf( "Profile            :- MAIN (%d)\n", s32Profile);
			//pPdecReg->frame_interval = ceil((double)(90000/(double)(i32TimeScale/2)));
		}
		else if(s32Profile == 100)
		{
			_printf( "Profile            :- HIGH (%d)\n", s32Profile);
			//pPdecReg->frame_interval = ceil((double)(90000/(double)(i32TimeScale/2)));
		}
		else
			_printf( "Profile            :- **** (%d)\n", s32Profile);

		_printf( "Level              :- %d (Multiplied by 10)\n", s32Level);
		//		_printf( "Interlace Seq      :- 0x%x\n", i32InterlaceSeq);
		_printf( "Direct 8x8 Flag    :- 0x%x\n", s32Direct8x8Flag);

	}
	else if(u32StreamFormat == MP2_DEC)
	{
		//pPdecReg->frame_interval = (UINT32)(((float)90000/flFrameRate + 0.5));
		u32Temp = pstSeqInfo->u32SeqHeadInfo;
		s32Profile = (u32Temp >> 0)  & 0xFF;
		s32Level =  (u32Temp >> 8)  & 0xFF;
		s32InterlacedSeq = (u32Temp >> 16) & 0x01;

		_printf( "Profile            :- ");
		switch(s32Profile)
		{
			case 1: _printf("HIGH");				break;
			case 2: _printf("SPATIAL SCALABLE");	break;
			case 3: _printf("SNR SCALABLE");		break;
			case 4: _printf("MAIN");		break;
			case 5: _printf("SIMPLE");		break;
			default : _printf("****");		break;
		}
		_printf(" (%d)\n", s32Profile);

		_printf( "Level            :- ");
		switch(s32Profile)
		{
			case 4: _printf("HIGH");		break;
			case 6: _printf("HIGH 1440");	break;
			case 8: _printf("MAIN");		break;
			case 10: _printf("LOW");		break;
			default : _printf("****");		break;
		}
		_printf(" (%d)\n", s32Level);
		//_printf( "Interlace Seq      :- 0x%x\n", i32InterlaceSeq);

	}
	else if(u32StreamFormat == MP4_DIVX3_DEC)
	{
		u32Temp = pstSeqInfo->u32DecSeqInfo;
		_printf( "mp4 short Video Header = %d\n", (u32Temp >> 2) & 1);
		_printf( "mp4 data Partition Enable = %d\n", (u32Temp & 1));
		_printf( "mp4 reversible Vlc Enable = %d\n",
				((u32Temp & 1) ?
				 ((u32Temp >> 1) & 1) : 0));
		_printf( "h263 annex JEnable = %d\n", (u32Temp >> 3) & 1);
	}
}
#if 0
static BOOLEAN MSVC_ConfigFrameBuffer(MSVC_CH_T *pstMsvcCh,
		UINT32 u32FrameNum, UINT32 u32BufStride, UINT32 u32BufHeight)
{
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	FRAME_BUF_INFO_T *pstFrameBufInfo;
	S_VDEC_DPB_INFO_T *psVdecDpb;
	UINT32 u32Ctr;
	UINT32 ui32FrameSize;
	UINT32 ui32BaseAddrMvCol;
	UINT32 ui32MsvcMvColBuf = 0x0;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	pstFrameBufInfo = &pstMsvcCh->stFrameBufInfo;
	psVdecDpb = &pstMsvcCh->sVdecFb;

	// Frame Buffer Init
	// Broadcast or normal FilePlay
	pstFrameBufInfo->u32FrameNum = u32FrameNum;
	pstFrameBufInfo->u32Stride = u32BufStride;

	// Alloc MV_Col Buffer
	for( u32Ctr = 0; u32Ctr < u32FrameNum; u32Ctr++ )
	{
		ui32MsvcMvColBuf >>= 1;
		ui32MsvcMvColBuf |= 0x80000000;
	}
	for( u32Ctr = 0; u32Ctr <= (32 - u32FrameNum); u32Ctr++ )
	{
		if( (gui32MsvcMvColBufPool & ui32MsvcMvColBuf) == ui32MsvcMvColBuf )
			break;

		ui32MsvcMvColBuf >>= 1;
	}
	if( u32Ctr > (32 - u32FrameNum) )
	{
		VDEC_KDRV_Message(ERROR,"[MSVC%d][Err] Not Enough MV_Col Buffer, Pool:0x%08X, Alloc:0%08X(%d), %s", pstMsvcCh->u32Ch,
									gui32MsvcMvColBufPool, ui32MsvcMvColBuf, u32FrameNum, __FUNCTION__);
		return FALSE;
	}
	else
	{
		gui32MsvcMvColBufPool &= ~ui32MsvcMvColBuf;
		pstMsvcCh->ui32MsvcMvColBufUsed = ui32MsvcMvColBuf;
		ui32BaseAddrMvCol = VDEC_MSVC_MVCOL_BUF_BASE + (VDEC_MSVC_MVCOL_SIZE * u32Ctr);
	}

	ui32FrameSize = psVdecDpb->ui32FrameSizeY + psVdecDpb->ui32FrameSizeCb + psVdecDpb->ui32FrameSizeCr;
	for (u32Ctr=0; u32Ctr < u32FrameNum; u32Ctr++)
	{
		pstFrameBufInfo->astFrameBuf[u32Ctr].u32AddrY = psVdecDpb->ui32BaseAddr + (ui32FrameSize * u32Ctr);
		pstFrameBufInfo->astFrameBuf[u32Ctr].u32AddrCb = psVdecDpb->ui32BaseAddr + (ui32FrameSize * u32Ctr) + psVdecDpb->ui32FrameSizeY;
		pstFrameBufInfo->astFrameBuf[u32Ctr].u32AddrCr = psVdecDpb->ui32BaseAddr + (ui32FrameSize * u32Ctr) + psVdecDpb->ui32FrameSizeY + psVdecDpb->ui32FrameSizeCb;
		pstFrameBufInfo->astFrameBuf[u32Ctr].u32MvColBuf = ui32BaseAddrMvCol + (VDEC_MSVC_MVCOL_SIZE * u32Ctr);
	}

	MSVC_HAL_RegisterFrameBuffer(u32CoreNum, u32InstNum, pstFrameBufInfo);

	MSVC_HAL_SetFrameBuffer(u32CoreNum, u32InstNum, u32FrameNum,
			pstMsvcCh->stSeqInfo.u32FrameBufDelay, u32BufStride);

	VDEC_KDRV_Message(_TRACE,"[MSVC%d] %s : nFrm %d  Stride %d  Delay %d", pstMsvcCh->u32Ch,
			__FUNCTION__, u32FrameNum, u32BufStride, pstMsvcCh->stSeqInfo.u32FrameBufDelay);

	return TRUE;
}
#endif
static void _print_seqinit_fail_reason(UINT32 u32StreamFormat, UINT32 u32FailWord)
{
	switch( u32StreamFormat )
	{
		case MP2_DEC:
			if( ((u32FailWord>>16) && 0x01))
				VDEC_KDRV_Message(ERROR, "Syntax Error in seq header");
			if( ((u32FailWord>>17) && 0x01))
				VDEC_KDRV_Message(ERROR, "No Sequence Header");
			if( ((u32FailWord>>18) && 0x01))
				VDEC_KDRV_Message(ERROR, "Seq Extenstion Header Error");
			break;
		case MP4_DIVX3_DEC:
			if( ((u32FailWord>>16) && 0x01))
				VDEC_KDRV_Message(ERROR, "Syntax Error in VOL header");
			if( ((u32FailWord>>17) && 0x01))
				VDEC_KDRV_Message(ERROR, "No VOL Header");
			if( ((u32FailWord>>18) && 0x01))
				VDEC_KDRV_Message(ERROR, "GMC used");
			break;
		case AVC_DEC:
			if( ((u32FailWord>>16) && 0x01))
				VDEC_KDRV_Message(ERROR, "Syntax Error in the SPS");
			if( ((u32FailWord>>17) && 0x01))
				VDEC_KDRV_Message(ERROR, "No SPS");
			break;
		default :
			VDEC_KDRV_Message(ERROR, "FailWord:0x%X", u32FailWord);
	}

	return;
}
#if 0
void _msvc_decide_dpb_param(MSVC_CH_T *pstMsvcCh, UINT32 u32StreamFormat,
		UINT32 u32PicWidth, UINT32 u32PicHeight, UINT32 *pu32FrameCnt,
		UINT32 *pu32BufStride, UINT32 *pu32BufHeight,
		UINT32 *pu32MinFrameBufferCount)
{
	UINT32 u32BufH, u32BufW, u32NFrm;

	if( pstMsvcCh->u8NoAdaptiveStream )
	{
		u32BufH = (u32PicHeight + 0x1F) & (~0x1F);
		u32BufW = (u32PicWidth + 0xF) & (~0xF);
	}
	else if(*pu32MinFrameBufferCount<15)
	{
		u32BufW = 2048;
		u32BufH = 1088;
		//VDEC_KDRV_Message(ERROR,"[MSVC%d][Dbg] forced alloc with HD Size %d",
		//		pstMsvcCh->u32Ch, *pu32MinFrameBufferCount);
	}
	else
	{
		// if use H/W source path or not AVC codec
		if( pstMsvcCh->u8SrcMode == 0 || u32StreamFormat != AVC_DEC )
		{
			u32BufH = (u32PicHeight + 0x1F) & (~0x1F);

			if( u32BufH>720 )
				u32BufW = (u32PicWidth + 0xF) & (~0xF);
			else
				u32BufW = 2048;
		}
		else
		{
			u32BufW = 2048;
			u32BufH = 1088;
		}

		//VDEC_KDRV_Message(ERROR,"[MSVC%d][Warn] Too many needed frame %d",
		//      pstMsvcCh->u32Ch, pstSeqInfo->u32MinFrameBufferCount);
	}

	if( pstMsvcCh->b1FrameDec )
	{
		u32BufW = 2048;
		u32BufH = 1088;

		u32NFrm = 1;
	}
	else if( pstMsvcCh->u32CodecType == VDC_CODEC_H264_MVC )
	{
		u32NFrm = 22;
	}
	else
	{
		u32NFrm = *pu32MinFrameBufferCount+5;
	}

	if( pu32FrameCnt != NULL )
		*pu32FrameCnt = u32NFrm;
	if( pu32BufStride != NULL )
		*pu32BufStride = u32BufW;
	if( pu32BufHeight != NULL )
		*pu32BufHeight = u32BufH;
}
#endif
SINT32 MSVC_SeqInitDone(MSVC_CH_T *pstMsvcCh, t_CoreInfo *pstCoreInfo)
{
	SEQ_INFO_T *pstSeqInfo = NULL;
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32StreamFormat;
//	UINT32 u32CurRdPtr = 0;
//	UINT32 u32FrmCnt = 0;
//	UINT32 u32BufStride, u32BufHeight;

	if( pstMsvcCh->bOpened == FALSE )
	{
		return NOT_OK;
	}

//	if(pstMsvcCh->u8PdecCh==0xFF)
//	{
//		VDEC_KDRV_Message(ERROR, "[MSVC%d][Wrn] Already Closed %s", pstMsvcCh->u32Ch, __FUNCTION__ );
//		return NOT_OK;
//	}

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;
	pstMsvcCh->u32FrameCnt = 0;
	u32StreamFormat = MSVC_HAL_GetStreamFormat(u32CoreNum, u32InstNum);
//	u32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum);

	pstSeqInfo = &pstMsvcCh->stSeqInfo;
	MSVC_HAL_GetSeqInfo(u32CoreNum, u32InstNum, pstSeqInfo);
	pstSeqInfo->u32FrmCnt = 0;

	VDEC_KDRV_Message(_TRACE, "[MSVC%d] SEQ_INIT_DONE RET(0x%08X) RD %08X WR %08X ",
			pstMsvcCh->u32Ch, pstSeqInfo->u32IsSuccess,
			MSVC_HAL_GetBitstreamRdPtr(u32CoreNum,u32InstNum),
			MSVC_HAL_GetBitstreamWrPtr(u32CoreNum,u32InstNum));

	if( (pstSeqInfo->u32IsSuccess&0x01) == 0x0){
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Sequence init fail!!", pstMsvcCh->u32Ch);
		_print_seqinit_fail_reason(u32StreamFormat, pstSeqInfo->u32IsSuccess);

		goto error;
	}

//	pstMsvcCh->bSeqInit = 1;
	msvc_CheckHeaderInfo(u32StreamFormat, pstSeqInfo);

	if( pstSeqInfo->u8FixedFrameRate )
	{
		pstMsvcCh->u32CurFrameRateRes = pstSeqInfo->u32FrameRateRes;
		pstMsvcCh->u32CurFrameRateDiv = pstSeqInfo->u32FrameRateDiv;
	}
	else
	{
		pstMsvcCh->u32CurFrameRateRes = 0;
		pstMsvcCh->u32CurFrameRateDiv = 0;
	}

#if 0
_MSVC_Retry_DPB_Alloc :
	_msvc_decide_dpb_param(pstMsvcCh, u32StreamFormat,
			pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight,
			&u32FrmCnt, &u32BufStride, &u32BufHeight,
			&pstSeqInfo->u32MinFrameBufferCount);

	pstSeqInfo->u32FrmCnt = u32FrmCnt;
	if( (u32BufStride == 2048) && (u32BufHeight == 1088) )
		pstSeqInfo->bMaxSizeDpbAlloc = TRUE;
	else
		pstSeqInfo->bMaxSizeDpbAlloc = FALSE;

	if( pstMsvcCh->sVdecFb.ui32NumOfFb == 0 )		// for MSVC Restart
	{
		if( VDEC_DPB_AllocAll(u32BufStride, u32BufHeight, u32FrmCnt, &(pstMsvcCh->sVdecFb), pstMsvcCh->bDual3D) == FALSE )
		{
			VDEC_KDRV_Message(ERROR,"[MSVC%d] DPB Alloc Fail!!!!!", pstMsvcCh->u32Ch);

			if( pstMsvcCh->u8NoAdaptiveStream == FALSE )
			{
				pstMsvcCh->u8NoAdaptiveStream = TRUE;
				VDEC_KDRV_Message(ERROR, "[MSVC%d][Warning] Retry DPB Allocation - Width * Height * FrameNum = %d * %d * %d, %s", pstMsvcCh->u32Ch,
											pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight, u32FrmCnt, __FUNCTION__ );
				goto _MSVC_Retry_DPB_Alloc;
			}

			pstSeqInfo->u32IsSuccess = 0;
			goto error;
		}
	}

	if( MSVC_ConfigFrameBuffer(pstMsvcCh, u32FrmCnt, u32BufStride, u32BufHeight) == FALSE )
	{
		pstSeqInfo->u32IsSuccess = 0;
		goto error;
	}

	VDEC_KDRV_Message(_TRACE,"[MSVC] Frameinfo Base: %08X, Size: Y %08X Cb %08X Cr %08X",
			pstMsvcCh->sVdecFb.ui32BaseAddr, pstMsvcCh->sVdecFb.ui32FrameSizeY, pstMsvcCh->sVdecFb.ui32FrameSizeCb, pstMsvcCh->sVdecFb.ui32FrameSizeCr);
#endif
error:

//	pstMsvcCh->u32ChBusy = FALSE;
	msvc_ReportSeqInfo(pstMsvcCh, pstSeqInfo);

	return OK;
}

typedef struct{
	UINT32 ui32FlushAge;
	UINT32 ui32FrameIdx;
	UINT32 ui32AspectRatio;
	UINT32 ui32PicWidth;
	UINT32 ui32PicHeight;
//	UINT32 ui32H_Offset;
//	UINT32 ui32V_Offset;
	UINT32 ui32FrameRateRes;
	UINT32 ui32FrameRateDiv;
	BOOLEAN	bVariableFrameRate;
	UINT32 ui32DisplayInfo;
	UINT32 ui32DisplayPeriod;

	SINT32 i32FramePackArrange;		// 3D SEI
	UINT32 ui32LR_Order;
	UINT16 ui16ParW;
	UINT16 ui16ParH;
}S_DISPQ_PICRUN_T;

static SINT32 msvc_ReportPicInfo(MSVC_CH_T *pstMsvcCh, PIC_INFO_T *pstPicInfo, MSVC_USRD_INFO_T *pstUDInfo, UINT8 bPackedMode, S_DISPQ_PICRUN_T *pstDispQEntry )
{
	S_MSVC_DONE_INFO_T msvc_picinfo_param;

	msvc_picinfo_param.eMsvcDoneHdr = MSVC_DONE_HDR_PICINFO;
	msvc_picinfo_param.u.picInfo.u32DecodingSuccess = pstPicInfo->u32DecodingSuccess;
	msvc_picinfo_param.u.picInfo.u32NumOfErrMBs = pstPicInfo->u32NumOfErrMBs;
	msvc_picinfo_param.u.picInfo.i32IndexFrameDecoded = (SINT32)pstPicInfo->u32IndexFrameDecoded;
	msvc_picinfo_param.u.picInfo.i32IndexFrameDisplay = (SINT32)pstPicInfo->u32IndexFrameDisplay;
	msvc_picinfo_param.u.picInfo.u32PicType = pstPicInfo->u32PicType;
	msvc_picinfo_param.u.picInfo.u32PictureStructure = pstPicInfo->u32PictureStructure;
	msvc_picinfo_param.u.picInfo.u32Aspectratio = pstPicInfo->u32AspectRatio;
	msvc_picinfo_param.u.picInfo.u32PicWidth = pstPicInfo->u32PicWidth;
	msvc_picinfo_param.u.picInfo.u32PicHeight = pstPicInfo->u32PicHeight;
	msvc_picinfo_param.u.picInfo.u32CropLeftOffset = pstPicInfo->u32CropLeftOffset;
	msvc_picinfo_param.u.picInfo.u32CropRightOffset = pstPicInfo->u32CropRightOffset;
	msvc_picinfo_param.u.picInfo.u32CropTopOffset = pstPicInfo->u32CropTopOffset;
	msvc_picinfo_param.u.picInfo.u32CropBottomOffset = pstPicInfo->u32CropBottomOffset;
	msvc_picinfo_param.u.picInfo.u32ActiveFMT = pstPicInfo->u32ActiveFMT;
//	msvc_picinfo_param.u.picInfo.u32DecodedFrameNum = pstPicInfo->u32DecodedFrameNum;
//	msvc_picinfo_param.u.picInfo.u32CurRdPtr = MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum);
	msvc_picinfo_param.u.picInfo.u32ProgSeq = pstPicInfo->u32ProgSeq;
	msvc_picinfo_param.u.picInfo.u32ProgFrame = pstPicInfo->u32ProgFrame;
	msvc_picinfo_param.u.picInfo.si32FrmPackArrSei = pstPicInfo->i32FramePackArrange;
	msvc_picinfo_param.u.picInfo.u32LowDelay = pstPicInfo->u8LowDelay;
//	msvc_picinfo_param.u.picInfo.u32MinFrameBufferCount = pstPicInfo->u32MinFrameBufferCount;

	if(bPackedMode)
		msvc_picinfo_param.u.picInfo.u32bPackedMode = 1;
	else
		msvc_picinfo_param.u.picInfo.u32bPackedMode = 0;

	if((SINT32)pstPicInfo->u32IndexFrameDecoded == -1 )
	{
		msvc_picinfo_param.u.picInfo.u32bRetryDec = 1;
		VDEC_KDRV_Message(DBG_VDC,"[MSVC] DPB insufficient -> dec retry");
	}
	else
		msvc_picinfo_param.u.picInfo.u32bRetryDec = 0;

	if( pstMsvcCh->u32NReDec > 0 )
	{
		msvc_picinfo_param.u.picInfo.u32bRetryDec = 1;
		pstMsvcCh->u32NReDec--;
	}

	msvc_picinfo_param.u.picInfo.ui32FrameRateRes = pstPicInfo->u32FrameRateRes;
	msvc_picinfo_param.u.picInfo.ui32FrameRateDiv = pstPicInfo->u32FrameRateDiv;

	// User data proc
	if(pstUDInfo)
	{
		msvc_picinfo_param.u.picInfo.sUsrData.ui32PicType = pstPicInfo->u32PicType;
		msvc_picinfo_param.u.picInfo.sUsrData.ui32Rpt_ff = pstPicInfo->u32RepeatFirstField;
		msvc_picinfo_param.u.picInfo.sUsrData.ui32Top_ff = pstPicInfo->u32TopFieldFirst;
		msvc_picinfo_param.u.picInfo.sUsrData.ui32BuffAddr = pstUDInfo->u32BuffAddr;					// physical address
		msvc_picinfo_param.u.picInfo.sUsrData.ui32Size = pstUDInfo->u32Len;							// scanned length
	}
	else
	{
		msvc_picinfo_param.u.picInfo.sUsrData.ui32Size = 0;
	}

//	msvc_picinfo_param.u.picInfo.u16DpbIdCnt = (pstDispQEntry->ui32FrameIdx >> 16);
	msvc_picinfo_param.u.picInfo.bVariableFrameRate = pstDispQEntry->bVariableFrameRate;
	msvc_picinfo_param.u.picInfo.ui16ParW = pstDispQEntry->ui16ParW;
	msvc_picinfo_param.u.picInfo.ui16ParH = pstDispQEntry->ui16ParH;
//	msvc_picinfo_param.u.picInfo.ui32H_Offset = pstDispQEntry->ui32H_Offset;
//	msvc_picinfo_param.u.picInfo.ui32V_Offset = pstDispQEntry->ui32V_Offset;
	msvc_picinfo_param.u.picInfo.ui32DisplayPeriod = pstDispQEntry->ui32DisplayPeriod;
	msvc_picinfo_param.u.picInfo.i32FramePackArrange = pstDispQEntry->i32FramePackArrange;
	msvc_picinfo_param.u.picInfo.ui32DisplayInfo = pstDispQEntry->ui32DisplayInfo;
	msvc_picinfo_param.u.picInfo.ui32LR_Order = pstDispQEntry->ui32LR_Order;

	MSVC_ADP_ISR_RunDone((UINT8)pstMsvcCh->u32Ch, &msvc_picinfo_param);
	return OK;
}
#if 0
static SINT32 msvc_DpbOverflowCheck(UINT32 u32CoreNum, UINT32 u32InstNum, SINT32 i32IndexFrameDisplay, SINT32 i32IndexFrameDecoded)
{
	if( i32IndexFrameDisplay < 0 )
	{
		if( i32IndexFrameDecoded != -2 )
		{
			//i32IndexFrameDecoded
			// -1 (0xFFFF) : insufficient frame buffer
			// -2 (0xFFFE) : no decoded frame(because of  skip option or picture header error)
			//MCU_printf("No frame to display!!\n");
			//MCU_printf("Frame Buf Idx:%d     Disp buf idx:%d      frame flag:%x\n",
			//	i32IndexFrameDecoded, i32IndexFrameDisplay, BIT_FRM_DIS_FLG_0(u32CoreNum));
		}
		if( i32IndexFrameDisplay == -3 && i32IndexFrameDecoded == -1)
		{
			//FCVDP_FlushMsg();
			VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] FrmBuf Idx:%d  DispBuf idx:%d  frame flag:%x",
					u32CoreNum, u32InstNum,
					i32IndexFrameDecoded, i32IndexFrameDisplay,
					MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum));

			//VDEC_KDRV_Message(ERROR, "Frame buffer full->flush all!!");
			return DN_BUF_OVERFLOW;
		}
		//vdec_ClrDispFlag(hDec, 1<<i32IndexFrameDisplay);
		//return;
	}

	return OK;
}

inline void _msvc_dpb_age_check(UINT8 u8CoreNum, UINT8 u8InstNum, UINT8 *au8Ages,
		SINT8 i8DispIdx, SINT8 i8DecIdx,
		UINT8 u8nFrame, UINT8 u8MaxAge)
{
	UINT32 u32DispMark;
	int i;

	u32DispMark = MSVC_HAL_GetFrameDisplayFlag(u8CoreNum, u8InstNum);

	for(i=0; i<u8nFrame;i++)
	{
		if( i==i8DispIdx )
		{
			// Die
			//printk(" die idx %d age %d\n", i, au8Ages[i8DispIdx]);
			au8Ages[i8DispIdx] = 0;
		}

		if( i==i8DecIdx )
		{
			//new born
			if (au8Ages[i8DecIdx] != 0)
			{
				// inconsistancy
				VDEC_KDRV_Message(DBG_VDC,"[MSVC C%dI%d] DPB overwrite dectected %d(ag%d)",
						u8CoreNum, u8InstNum, i8DecIdx, au8Ages[i8DecIdx]);
			}

			au8Ages[i8DecIdx] = 0;
		}

		if( u32DispMark & (1<<i) )
			au8Ages[i]++;
		else
			au8Ages[i]=0;

		if(au8Ages[i] > u8MaxAge)
		{
			// kill!!
			u32DispMark &= ~(1<<i);
			MSVC_HAL_SetFrameDisplayFlag(u8CoreNum, u8InstNum, u32DispMark);

			VDEC_KDRV_Message(ERROR,"[MSVC C%dI%d] Too old DPB frame %d(age%d>%d)",
					u8CoreNum, u8InstNum,i, au8Ages[i], u8MaxAge);
			au8Ages[i] = 0;
		}
	}
}
#endif
static UINT32 _GetDisplayPeriod(UINT32 u32ProgSeq, UINT32 u32ProgFrame, UINT32 u32TopFieldFirst, UINT32 u32RepeatFirstField, UINT32 u32PictureStructure)
{
/*
PROG	PROG	TFF		RFF		Picture										Display	1st		2nd		3rd
SEQ		FRAME					Structure										Period	T		T		T

1		1		0		0		3		1번 Decoding 1번 Display				1T		F		X		X
1		1		0		1		3		1번 Decoding 2번 Display				2T		F		F		X
1		1		1		1		3		1번 Decoding 3번 Display				3T		F		F		F
0		0		0		0		3		B -> T (frame picture)					2T		B		T		X
0		0		0		0		2		B (field picture) (Last Decoded Picture)		2T 		T		B		X
0		0		1		0		3		T -> B (frame picture)					2T		T		B		X
0		0		1		0		1		T (field picture) (Last Decoded Picture)		2T		B		T		X
0		1		0		0		3		B -> T								2T		B		T		X
0		1		1		0		3		T -> B								2T		T		B		X
0		1		0		1		3		B -> T -> B							3T		B		T		B
0		1		1		1		3		T -> B -> T							3T		T		B		T
*/

	UINT32 u32DisplayPeriod;

	if ( u32ProgSeq == 0x1 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x0 ) {	// 1 Frame Display
		u32DisplayPeriod = 1;
	} else if ( u32ProgSeq == 0x1 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x1 ) {	// 2 Frame Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x1 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x1 && u32RepeatFirstField == 0x1 ) {	// 3 Frame Display
		u32DisplayPeriod = 3;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x0 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x0 && u32PictureStructure == 0x3 ) {	// B -> T Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x0 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x0 && u32PictureStructure == 0x2 ) {	// T -> B Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x0 && u32TopFieldFirst == 0x1 && u32RepeatFirstField == 0x0 && u32PictureStructure == 0x3 ) {	// T -> B Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x0 && u32TopFieldFirst == 0x1 && u32RepeatFirstField == 0x0 && u32PictureStructure == 0x1 ) {	// B -> T Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x0 ) {	// B -> T Display
		u32DisplayPeriod = 2;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x1 && u32RepeatFirstField == 0x0 ) {	// T -> B Display
		u32DisplayPeriod = 2;
	}  else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x0 && u32RepeatFirstField == 0x1 ) {	// B -> T -> B Display
		u32DisplayPeriod = 3;
	} else if ( u32ProgSeq == 0x0 && u32ProgFrame == 0x1 && u32TopFieldFirst == 0x1 && u32RepeatFirstField == 0x1 ) {	// T -> B -> T Display
		u32DisplayPeriod = 3;
	}
	else
	{
		// Non existing syntax
		// Apply General Rule
		if( u32ProgSeq == 0x1 && u32ProgFrame == 0x1) {
			u32DisplayPeriod =	1;
		}else if( u32ProgSeq == 0x1) {
			u32DisplayPeriod =	u32TopFieldFirst + u32RepeatFirstField + 1;
		} else if (u32ProgFrame == 0x0) {
			u32DisplayPeriod =	2;
		} else if (u32RepeatFirstField == 0x0) {
			u32DisplayPeriod = 2;
		} else {
			u32DisplayPeriod = 3;
		}

		// Check Range
		if (u32DisplayPeriod < 1 || u32DisplayPeriod > 3) {
			u32DisplayPeriod = 2;
		}

//		VDEC_KDRV_Message(DBG_VDC, "%s: param error Ret:%u ->ProgSeq:%u ProgFrm:%u TopFst:%u Rpt:%u PicSt:%u", __FUNCTION__, u32DisplayPeriod,
//				u32ProgSeq, u32ProgFrame, u32TopFieldFirst, u32RepeatFirstField, u32PictureStructure);
	}

	return u32DisplayPeriod;
}

static void _SetDispQEntry(S_DISPQ_PICRUN_T *pstDispQEntry, PIC_INFO_T *pstPicInfo, UINT32 u32nDisplayRepeat, MSVC_CH_T *pstMsvcCh)
{
	UINT32 u32CurIdx;
//	static UINT32 u32PreDTS, u32PrePTS;

	u32CurIdx = pstPicInfo->u32IndexFrameDecoded;

	pstDispQEntry->ui32FrameIdx = u32CurIdx;
#ifdef DPB_ID_USE
//	pstDispQEntry->ui32FrameIdx |= (pstMsvcCh->u16DpbIdCnt<<16);
#endif
	pstDispQEntry->ui32FrameRateRes = pstPicInfo->u32FrameRateRes;
	pstDispQEntry->ui32FrameRateDiv = pstPicInfo->u32FrameRateDiv;
	if( pstMsvcCh->u32CodecType == VDC_CODEC_RVX )
		pstDispQEntry->bVariableFrameRate = TRUE;
	else
		pstDispQEntry->bVariableFrameRate = FALSE;

	pstDispQEntry->ui32AspectRatio = pstPicInfo->u32AspectRatio;
	pstDispQEntry->ui16ParW = pstPicInfo->u16SarW;
	pstDispQEntry->ui16ParH = pstPicInfo->u16SarH;

	pstDispQEntry->ui32PicWidth = pstPicInfo->u32PicWidth;
	pstDispQEntry->ui32PicHeight = pstPicInfo->u32PicHeight;

//	pstDispQEntry->ui32H_Offset = (pstPicInfo->u32CropLeftOffset<<16) | pstPicInfo->u32CropRightOffset;
//	pstDispQEntry->ui32V_Offset = (pstPicInfo->u32CropTopOffset<<16) | pstPicInfo->u32CropBottomOffset;

	pstDispQEntry->ui32DisplayPeriod = u32nDisplayRepeat;
	pstDispQEntry->i32FramePackArrange = pstPicInfo->i32FramePackArrange;
	//pstDispQEntry->ui32ErrorMb = pstPicInfo->u32NumOfErrMBs;

	//pstDispQEntry->ui32DisplayInfo = pstPicInfo->u32TopFieldFirst;
	// Set fist field structure
	if( pstPicInfo->u32ProgSeq == 0x1)
	{
		pstDispQEntry->ui32DisplayInfo = FRAME_PIC;
	}
	else
	{
		switch ( pstPicInfo->u32PictureStructure )
		{
			case FRAME_PIC:
				if( pstPicInfo->u32TopFieldFirst == 0x1 )
					pstDispQEntry->ui32DisplayInfo = TOP_FIELD_PIC;
				else
					pstDispQEntry->ui32DisplayInfo = BOTTOM_FIELD_PIC;
				break;

			case TOP_FIELD_PIC:
				pstDispQEntry->ui32DisplayInfo = BOTTOM_FIELD_PIC;
				break;

			case BOTTOM_FIELD_PIC:
				pstDispQEntry->ui32DisplayInfo = TOP_FIELD_PIC;
				break;

			default:
				pstDispQEntry->ui32DisplayInfo = 0xFFFFFFFF;
				VDEC_KDRV_Message(ERROR, "PIC STRUCT ERROR %d", pstPicInfo->u32PictureStructure);
				break;
		}
	}

	switch( pstPicInfo->u8LR3DInfo )
	{
		case MSVC_3D_RIGHT_FRM:
			pstDispQEntry->ui32LR_Order = DISPQ_3D_FRAME_RIGHT;
			break;
		case MSVC_3D_LEFT_FRM:
			pstDispQEntry->ui32LR_Order = DISPQ_3D_FRAME_LEFT;
			break;
		default:
			pstDispQEntry->ui32LR_Order = DISPQ_3D_FRAME_NONE;
			break;
	}

	// Set PTS
//	pstDispQEntry->sPTS.ui32PTS = pstPicInfo->u32Pts;
//	pstDispQEntry->sPTS.ui64TimeStamp = pstPicInfo->u64TimeStamp;
//	pstDispQEntry->sPTS.ui32uID = pstPicInfo->u32uID;

	VDEC_KDRV_Message(dbgPrtLvDeQEnt,"[%d]DQ %2d PrS%u PrF%u PStr%u DpInf%u DiPrd %u %u:%u Fr %d/%d %d",
			pstMsvcCh->u32FrameCnt, pstDispQEntry->ui32FrameIdx,
			pstPicInfo->u32ProgSeq, pstPicInfo->u32ProgFrame, pstPicInfo->u32PictureStructure,
			pstDispQEntry->ui32DisplayInfo, u32nDisplayRepeat,
			pstDispQEntry->ui32PicWidth, pstDispQEntry->ui32PicHeight,
			pstDispQEntry->ui32FrameRateRes, pstDispQEntry->ui32FrameRateDiv,
			pstDispQEntry->bVariableFrameRate);

//	u32PreDTS = pstPicInfo->u32Dts;
//	u32PrePTS = pstDispQEntry->sPTS.ui32PTS;

	/*
	// memory dump
	dbgTS[dbgTSidx][0]=pstDispQEntry->ui32PTS;
	dbgTS[dbgTSidx][1]=pstPicInfo->u32Dts;
	dbgTS[dbgTSidx][2]=pstMsvcCh->u32CurPicPts;
	dbgTS[dbgTSidx][3]=pstMsvcCh->u32CurPicDts;
	if( dbgTSidx <200-2 )
		dbgTSidx ++;
	*/

	return;
}

SINT32 _CalFramePeriod(UINT32 u32ProgSeq, UINT32 u32FrameRateRes, UINT32 u32FrameRateDiv, UINT32 *pu32FramePeriod, UINT32 *pu32SyncPeriod)
{
	SINT32 i32RetVal;
	UINT32 u32FramePeriod = 1500, u32SyncPeriod = 1500;
	// Frame Sync 및 Field Sync Period 계산
	// JGKim@1212: Error Check
	if (u32FrameRateRes == 0)
	{	// Error
		_printf("u32FrameRateRes equals zero :[0x%08x]\n",  u32FrameRateRes);
		u32SyncPeriod			= 1500;	// Default 60Hz
		u32FramePeriod			= 1500; // Default 60Hz
		i32RetVal = NOT_OK;

		//u32FrameRate = 0x10032;
	} else {
		if (u32FrameRateDiv != 0) {
			if ( u32ProgSeq == 0x1 ) {
				u32SyncPeriod			= 90000 * u32FrameRateDiv / u32FrameRateRes ;
			} else {
				u32SyncPeriod			= 90000 * u32FrameRateDiv / (u32FrameRateRes<<1);//case of field 2*frame rate
			}

			u32FramePeriod				= 90000 * u32FrameRateDiv / u32FrameRateRes ;
		}
	}

	*pu32FramePeriod = u32FramePeriod;
	*pu32SyncPeriod = u32SyncPeriod;

	return OK;
}


#if 0
static void _msvc_print_status(MSVC_CH_T *pstMsvcCh)
{
	static UINT32 u32PreGstc[8];
	static UINT32 u32PreDecCount[8];
	static UINT32 u32PreValidDecCount[8];
	static UINT32 u32PreClearCount[8];
	UINT32 ui32Gstc;
	UINT32 u32Ch;
	UINT32 u32Elapsed;
	UINT32 u32FrameFlag;
	UINT32 i,n;

	if( !(pstMsvcCh->u32DecCount % (30*5)) )
	{
		u32Ch = pstMsvcCh->u32Ch;
		ui32Gstc = TOP_HAL_GetGSTCC();
		u32Elapsed = ui32Gstc - u32PreGstc[u32Ch];

		u32FrameFlag = MSVC_HAL_GetFrameDisplayFlag( pstMsvcCh->u32CoreNum,
				pstMsvcCh->u32InstNum );

		n=0;
		for(i=0; i<pstMsvcCh->stFrameBufInfo.u32FrameNum; i++)
			if( u32FrameFlag & (1<<i)) n++;

		VDEC_KDRV_Message(MONITOR,
				"[MSVC%d] Dec100 %d  ValDec100 %d  Clear100 %d  CurDPBMark 0x%08X(%d) %d%%", u32Ch ,
				(pstMsvcCh->u32DecCount - u32PreDecCount[u32Ch])*90000 /(u32Elapsed/100) ,
				(pstMsvcCh->u32ValidDecCount - u32PreValidDecCount[u32Ch])*90000 /(u32Elapsed/100) ,
				(pstMsvcCh->u32ClearCount - u32PreClearCount[u32Ch])*90000 /(u32Elapsed/100),
				u32FrameFlag,n, msvc_dbgPoss[pstMsvcCh->u32CoreNum][0]*100 /
				(msvc_dbgPoss[pstMsvcCh->u32CoreNum][0]+msvc_dbgPoss[pstMsvcCh->u32CoreNum][1]));

//		VDEC_KDRV_Message(ERROR, "[MSVC%d] %d%%", u32Ch
//					, msvc_dbgPoss[pstMsvcCh->u32CoreNum][0]*100 /
//					(msvc_dbgPoss[pstMsvcCh->u32CoreNum][0]+msvc_dbgPoss[pstMsvcCh->u32CoreNum][1]));

		msvc_dbgPoss[pstMsvcCh->u32CoreNum][0] = 0;
		msvc_dbgPoss[pstMsvcCh->u32CoreNum][1] = 0;
		u32PreGstc[u32Ch] = ui32Gstc;
		u32PreDecCount[u32Ch] = pstMsvcCh->u32DecCount;
		u32PreValidDecCount[u32Ch] = pstMsvcCh->u32ValidDecCount;
		u32PreClearCount[u32Ch] = pstMsvcCh->u32ClearCount;
	}
}
#endif

void _msvc_decpic_fail_proc(MSVC_CH_T *pstMsvcCh, UINT32 u32StreamFormat, PIC_INFO_T *pstPicInfo)
{
	UINT32 u32DecRes = pstPicInfo->u32DecResult;

	if( (u32DecRes>>2) & 0x01 )
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Slice buffer is not sufficient", pstMsvcCh->u32Ch);
	if( (u32DecRes>>3) & 0x01 )
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Ps is not sufficient", pstMsvcCh->u32Ch);

	switch (u32StreamFormat)
	{
		case AVC_DEC:
			if( (u32DecRes>>16) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] Syntax error in the PPS", pstMsvcCh->u32Ch);

			if( (u32DecRes>>17) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] No PPS", pstMsvcCh->u32Ch);
			break;
		case MP4_DIVX3_DEC:
			if( (u32DecRes>>16) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] Syntax error in VOP header", pstMsvcCh->u32Ch);

			if( (u32DecRes>>17) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] No VOP header", pstMsvcCh->u32Ch);
			break;
		case MP2_DEC:
			if( (u32DecRes>>16) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] Syntax error in picture header", pstMsvcCh->u32Ch);

			if( (u32DecRes>>17) & 0x01 )
				VDEC_KDRV_Message(ERROR,"[MSVC%d] No picture header", pstMsvcCh->u32Ch);
			break;
	}
}

static SINT32 _msvc_userdata_scan(MSVC_USRD_INFO_T *pstUDInfo,
		t_CoreInfo *pstCoreInfo)
{
	SINT32 s32RetVal = OK;
	MSVC_USRD_HEAD_T *pstUDHeader;
	UINT32 u32UDBuffAddr, u32VirUDBuffAddr;
	UINT32 u32UDBuffSize;
	UINT32 u32nSeg;
	UINT32 u32TotLen;
	UINT32 u32CoreNum = pstCoreInfo->u32CoreNum;
	MSVC_USRD_SEG_HEAD_T *pSeg;
	UINT8 *pSrc;
	UINT32 u32Scanned=0;
	UINT32 u32Remained;
	UINT32 i=0;

	MSVC_HAL_GetUserdataBufInfo(u32CoreNum, &u32UDBuffAddr, &u32UDBuffSize);

	// Need to convert to virtual address for ARM mode
	u32VirUDBuffAddr = pstCoreInfo->u32VirCoreBaseAddr +
		(u32UDBuffAddr - pstCoreInfo->u32CoreBaseAddr);

	pstUDHeader = (MSVC_USRD_HEAD_T*)u32VirUDBuffAddr;

	u32nSeg = MSVC_GET_16BIT(pstUDHeader->nSegment);
	u32TotLen = MSVC_GET_16BIT(pstUDHeader->bytesTotal);

	u32nSeg &= 0x0F;

	pSeg = &pstUDHeader->seg[0];
	pSrc = (UINT8*)pstUDHeader + sizeof(MSVC_USRD_HEAD_T);
	u32Remained = u32UDBuffSize;

	for( i=0; i<u32nSeg; i++,pSeg++)
	{
		UINT32 u32Bytes = MSVC_GET_16BIT(pSeg->bytes);

		//VDEC_KDRV_Message(DBG_VDC,"[MSVC] Userdata %u/%u Seg: SegLen %u Total %u Buf %u",
		//			i+1, u32nSeg, u32Bytes, u32TotLen, u32UDBuffSize );

		if(u32Bytes && (u32Remained>u32Bytes) )
		{
			pSrc += u32Bytes;
			pSrc = (UINT8*)(((UINT32)pSrc + 7) & (~0x7));

			u32Remained		-= u32Bytes;
			u32Scanned		+= u32Bytes;
		}
		else
		{
			VDEC_KDRV_Message(ERROR,"[MSVC] Insufficient Userdata Buffer: %u > %u Total %u Buf %u",
					u32Bytes, u32Remained, u32TotLen, u32UDBuffSize );
		}
	}

	pstUDInfo->u32nSeg = u32nSeg;
	pstUDInfo->u32BuffAddr = u32UDBuffAddr;
	pstUDInfo->u32TotalLen = u32TotLen;
	pstUDInfo->u32Len = u32Scanned;

	//VDEC_KDRV_Message(ERROR,"--> UD %d seg TotalLen %u Scanned %u", u32nSeg, u32TotLen, u32Scanned);

	return s32RetVal;
}
#if 0
CQ_ENTRY_T* MSVC_GetLastCmd(MSVC_CH_T *pstMsvcCh)
{
	return &pstMsvcCh->stLastCmd;
}

static void msvc_field_loss_check(MSVC_CH_T *pstMsvcCh, PIC_INFO_T *pstPicInfo )
{
	if( ((pstPicInfo->u8TopFieldType == 0xFF && pstPicInfo->u8BotFieldType != 0xFF) // Top Field Loss
			|| (pstPicInfo->u8TopFieldType != 0xFF && pstPicInfo->u8BotFieldType == 0xFF )) // Bottom Field Loss
			&& pstPicInfo->u32NumOfErrMBs > 10 )
	{
		if( pstPicInfo->u32DecResult>> 31 )
		{
			pstMsvcCh->u8SkipModeOn = 2;
			VDEC_KDRV_Message(ERROR,"[MSVC%d] Field loss T%d B%d\n", pstMsvcCh->u32Ch, pstPicInfo->u8TopFieldType, pstPicInfo->u8BotFieldType);
		}
	}

	return;
}

static void msvc_check_display_skip(MSVC_CH_T *pstMsvcCh, PIC_INFO_T *pstPicInfo )
{
	UINT32 u32Thold;	// threshold
	UINT8 u8SkipTest = FALSE;

	if( pstMsvcCh->u8SrcMode == 1 )
		u32Thold = MSVC_PB_SKIP_THD_FILE;
	else
		u32Thold = MSVC_PB_SKIP_THD;

	u32Thold *= 10;

	// Check Error MB
	if( pstPicInfo->u32NumOfErrMBs != 0 )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] Error MBs %d", pstMsvcCh->u32Ch, pstPicInfo->u32NumOfErrMBs);

		if( pstMsvcCh->u32ErrorMBFilter != 0 &&
				pstMsvcCh->u32ErrorMBFilter < pstPicInfo->u32NumOfErrMBs )
		{
			pstMsvcCh->u8SkipModeOn = 2;	// skip all
			pstMsvcCh->u16SkipPBCnt = 0;
			u8SkipTest = TRUE;
			pstPicInfo->u8DispSkipFlag = TRUE;
		}
	}

	if( pstMsvcCh->u8SkipModeOn && u8SkipTest == FALSE )
	{
		if( pstMsvcCh->u8SkipModeOn == 2 )		// Wait I-pic
		{
			if( pstPicInfo->u32PicType == 0 )
			{
				VDEC_KDRV_Message(DBG_VDC,"[MSVC%d] I pic found %d", pstMsvcCh->u32Ch, pstMsvcCh->u16SkipPBCnt);
				pstMsvcCh->u8SkipModeOn = 1;	// skip B-pic
				pstMsvcCh->u16SkipPBCnt = 0;
			}
			else
			{
				pstPicInfo->u8DispSkipFlag = TRUE;

				if( pstPicInfo->u32PicType == 1)	// P-pic
					pstMsvcCh->u16SkipPBCnt+=10;
				else
					pstMsvcCh->u16SkipPBCnt+=1;		// B-pic

				if( pstMsvcCh->u16SkipPBCnt >= u32Thold )
				{
					VDEC_KDRV_Message(ERROR,"[MSVC%d] Give up I-pic search", pstMsvcCh->u32Ch);
					pstMsvcCh->u8SkipModeOn = 1;
					pstMsvcCh->u16SkipPBCnt = 0;
					pstMsvcCh->u32PicOption &= ~(1<<12);
				}
			}
		}
		else if( pstMsvcCh->u8SkipModeOn == 1 )
		{
			if( pstPicInfo->u32PicType == 2 )
			{
				pstPicInfo->u8DispSkipFlag = TRUE;
				pstMsvcCh->u16SkipPBCnt+=1;

				if( pstMsvcCh->u16SkipPBCnt >= (u32Thold>>1) )
				{
					VDEC_KDRV_Message(ERROR,"[MSVC%d] Give up I/P-pic search", pstMsvcCh->u32Ch);
					pstMsvcCh->u8SkipModeOn = 0;
				}
			}
			else
			{
				VDEC_KDRV_Message(DBG_VDC,"[MSVC%d] Disp En %d", pstMsvcCh->u32Ch, pstMsvcCh->u16SkipPBCnt);
				pstMsvcCh->u8SkipModeOn = 0;	// no skip
				pstMsvcCh->u16SkipPBCnt = 0;
			}
		}
	}

	return;
}
#endif
void msvc_cal_divx_vari_framerate(MSVC_CH_T *pstMsvcCh, PIC_INFO_T *pstPicInfo)
{
	SINT32 pre_tr, pre_div, new_div;
	UINT32 tr;

	pre_tr = pstMsvcCh->i32PreTempRefer;
	tr = pstPicInfo->u32TimeIncr;
	//tr += pstPicInfo->u32FrameRateRes * pstPicInfo->u32ModTimeBase;

#if 0
	if( pre_tr != -1
			&& !(pstMsvcCh->u32PicOption & (0x3<<3))	// No trick mode
			&& !(pstMsvcCh->u32PicOption & (0x1<<11)) ) // No trick mode
#else
	if( pre_tr != -1 )
#endif
	{
		while( tr < pre_tr )
		{
			VDEC_KDRV_Message(DBG_VDC, "tf < pre_tr !! %d %d\n", tr, pre_tr);
			tr += pstPicInfo->u32FrameRateRes;
		}

		//pstPicInfo->u32FrameRateDiv = tr - pre_tr;
		pstMsvcCh->u32CurFrameRateRes = pstPicInfo->u32FrameRateRes;
		pre_div = pstMsvcCh->s32PreFrameRateDiv;
		new_div = tr - pre_tr;
		if( pre_div == -1 ||
				(pre_div+1 >= new_div && pre_div-1 <= new_div) )
			pstMsvcCh->u32CurFrameRateDiv = new_div;

		pstMsvcCh->s32PreFrameRateDiv = new_div;
	}

	pstPicInfo->u32FrameRateRes = pstMsvcCh->u32CurFrameRateRes;	// default
	pstPicInfo->u32FrameRateDiv = pstMsvcCh->u32CurFrameRateDiv;	// default

	VDEC_KDRV_Message(DBG_VDC, "PreTr %d  TR %d  ModBS %d Res %d  Div %d %X\n",
			pstMsvcCh->i32PreTempRefer, pstPicInfo->u32TimeIncr, pstPicInfo->u32ModTimeBase,
			pstPicInfo->u32FrameRateRes, pstPicInfo->u32FrameRateDiv,
			MSVC_HAL_GetBitstreamRdPtr(0,0) );

	pstMsvcCh->i32PreTempRefer = pstPicInfo->u32TimeIncr;

	return;
}

SINT32 MSVC_PicRunDone(MSVC_CH_T *pstMsvcCh, t_CoreInfo *pstCoreInfo)
{
	PIC_INFO_T stPicInfo, *pstPicInfo = &stPicInfo;
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32StreamFormat;
	SINT32 i32IndexFrameDisplay, i32IndexFrameDecoded;
	S_DISPQ_PICRUN_T stDispQEntry = {0, };
	UINT32 u32nDisplayRepeat;
//	CQ_ENTRY_T *pstCqEntry;
//	UINT32 u32MismatchType=0;
	MSVC_USRD_INFO_T stUDInfo;
	MSVC_USRD_INFO_T *pstUDInfo=NULL;
//	UINT32 u32FrameNum;
	UINT32 u32Ch;
	UINT32 u32PackedDetected = FALSE;
//	UINT32 u32TempFlag;

	u32CoreNum = pstMsvcCh->u32CoreNum;
	u32InstNum = pstMsvcCh->u32InstNum;

	u32Ch = u32CoreNum * MSVC_NUM_OF_CHANNEL;
	u32Ch += u32InstNum;

	msvc_dbgCmdTs[u32CoreNum][1] = TOP_HAL_GetGSTCC();
	u32StreamFormat = MSVC_HAL_GetStreamFormat(u32CoreNum, u32InstNum);

//	pstCqEntry = MSVC_GetLastCmd(pstMsvcCh);
	MSVC_HAL_GetPicInfo(u32CoreNum, u32InstNum, pstPicInfo);
//	_msvc_decide_dpb_param(pstMsvcCh, u32StreamFormat,
//			pstPicInfo->u32PicWidth, pstPicInfo->u32PicHeight,
//			NULL, NULL, NULL,
//			&pstPicInfo->u32MinFrameBufferCount);

//	pstPicInfo->u32AuSize = pstMsvcCh->u32AuSize;

	i32IndexFrameDisplay = (SINT32)pstPicInfo->u32IndexFrameDisplay;
	i32IndexFrameDecoded = (SINT32)pstPicInfo->u32IndexFrameDecoded;
//	u32FrameNum = pstMsvcCh->stFrameBufInfo.u32FrameNum;
//	pstMsvcCh->u32ExactRdPtr = MSVC_GetExactRdPtr(u32Ch);
//	if( pstMsvcCh->u32CodecType == VDC_CODEC_MPEG2_HP )
//		u32MismatchType = _msvc_CheckRdPtrMismatch2(pstMsvcCh, pstCqEntry);

//	_msvc_rsp_ram_log(u32CoreNum, u32InstNum, pstPicInfo);

	if( pstPicInfo->u32DecodingSuccess == 0)
	{
		VDEC_KDRV_Message(ERROR, "[MSVC%d] PicRun Fail!(0x%08X) Disp %d  Dec %d",
				pstMsvcCh->u32Ch,
				pstPicInfo->u32DecodingSuccess,
				i32IndexFrameDisplay, i32IndexFrameDecoded);

		_msvc_decpic_fail_proc(pstMsvcCh, u32StreamFormat, pstPicInfo);
	}
	else
	{
//		pstMsvcCh->u32DecCount++;
//		if( i32IndexFrameDecoded >= 0 && i32IndexFrameDecoded < u32FrameNum )
//			pstMsvcCh->u32ValidDecCount++;

		if( u32StreamFormat == MP4_DIVX3_DEC && pstPicInfo->u32DecResult & 0x10000 )	// packed mode
		{
			u32PackedDetected = TRUE;
			if( msvc_dbgPackedMode[pstMsvcCh->u32Ch] == 0 )
			{
				VDEC_KDRV_Message(_TRACE, "[MSVC%d] Packed Mode", pstMsvcCh->u32Ch);
				msvc_dbgPackedMode[pstMsvcCh->u32Ch] = 1;
			}
		}
	}

//	_msvc_print_status(pstMsvcCh);

	VDEC_KDRV_Message(dbgPrtLvPicDone, "[MSVC%d] #%d Dec %2d Dis %2d Typ %d,%08X  DpFlg %06X Op %06X Rd %08X(%08X) Wr %08X Success %X",
			pstMsvcCh->u32Ch, pstMsvcCh->u32FrameCnt,	i32IndexFrameDecoded, i32IndexFrameDisplay,
			pstPicInfo->u32PicType, RET_DEC_PIC_TYPE(u32CoreNum),
			MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum),CMD_DEC_PIC_OPTION(u32CoreNum),
			MSVC_HAL_GetBitstreamRdPtr(u32CoreNum, u32InstNum),
			RET_DEC_PIC_EXACT_RDPTR(u32CoreNum),
			MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum),
			pstPicInfo->u32DecResult);

	if( (i32IndexFrameDecoded == -3) && (i32IndexFrameDisplay == -3) )
	{
		pstMsvcCh->u32NReDec = 3;
		VDEC_KDRV_Message(ERROR, "[MSVC%d][Wrn] Profile change, %s", pstMsvcCh->u32Ch, __FUNCTION__ );
	}

//	if( pstCqEntry->bDecAgain || u32MismatchType || u32PackedDetected )
//	{
//		//printk("No PTS %d %d %d\n", pstCqEntry->bDecAgain, u32MismatchType, u32PackedDetected);
//	}

	// notify Pic decoded state to ARM
//	if( (pstMsvcCh->b1FrameDec==FALSE) &&
//			pstMsvcCh->u8UserDataEn && (pstMsvcCh->u32PicOption & 0x20) )
	if( (pstMsvcCh->b1FrameDec==FALSE) && pstMsvcCh->u8UserDataEn )
	{
		// User data proc
		_msvc_userdata_scan(&stUDInfo, pstCoreInfo );
		pstUDInfo = &stUDInfo;
	}

	//_msvc_dpb_age_check(u32CoreNum, u32InstNum, pstMsvcCh->au8DpbAge, i32IndexFrameDisplay,
	//		i32IndexFrameDecoded, u32FrameNum, pstMsvcCh->stSeqInfo.u32FrameBufDelay+32);
#if 0
	// DPB flush when overflow occured!
	if( msvc_DpbOverflowCheck(u32CoreNum, u32InstNum, i32IndexFrameDisplay, i32IndexFrameDecoded) != OK )
	{
		msvc_dpb_overflow_cnt[pstMsvcCh->u32Ch]+=2;

		if( msvc_dpb_overflow_cnt[pstMsvcCh->u32Ch] > 10 )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC%d] DPB overflow detected!!", pstMsvcCh->u32Ch);
//			MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, u32InstNum, 0);
//			stDispQEntry.sPTS.ui32PTS = -1;
			//MSVC_Reset(pstMsvcCh->u32Ch);
			goto error;
		}
	}
	else if( msvc_dpb_overflow_cnt[pstMsvcCh->u32Ch] > 0 )
		msvc_dpb_overflow_cnt[pstMsvcCh->u32Ch]--;

	if( pstPicInfo->u32ProgSeq == 0 && pstPicInfo->u32ProgFrame ==0 )
		msvc_field_loss_check(pstMsvcCh, pstPicInfo);

	msvc_check_display_skip( pstMsvcCh, pstPicInfo );

	if( pstPicInfo->u32PicType == 0 )
	{
		pstMsvcCh->u32PicOption &= ~(1<<12);
	}
	else if( (pstMsvcCh->u32PicOption & (1<<12))
			&& i32IndexFrameDisplay == -3 && i32IndexFrameDecoded >= 0 )
	{
		u32TempFlag = MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum);
		u32TempFlag &= ~(1<<i32IndexFrameDecoded);
		MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, u32InstNum, u32TempFlag);
		pstPicInfo->u32IndexFrameDecoded = BODA_MAX_FRAME_BUFF_NUM;
	}

	if( pstMsvcCh->u8CancelRA )
	{
		pstMsvcCh->u8CancelRA = FALSE;
		pstMsvcCh->u32PicOption &= ~(1<<12);
	}
#endif
	// Broadcast or Normal FilePlay
	if (pstMsvcCh->b1FrameDec==FALSE)
	{
//		if (i32IndexFrameDecoded >=0 && i32IndexFrameDecoded < u32FrameNum )
//		{
//			// Flushed frame during decoding
//			if( pstMsvcCh->u8OnPicDrop == 1 )
//			{
//				pstMsvcCh->u8OnPicDrop = 0;
//				pstPicInfo->u8DispSkipFlag = TRUE;
//				VDEC_KDRV_Message(DBG_VDC,"[MSVC%d] Pic Drop for Flush %d!!!!",
//						pstMsvcCh->u32Ch, pstPicInfo->u32IndexFrameDecoded );
//			}
//			else
//			{
				if( pstMsvcCh->stSeqInfo.u8FixedFrameRate == 0 &&
						(pstMsvcCh->u32CodecType == VDC_CODEC_H263 ||
						 pstMsvcCh->u32CodecType == VDC_CODEC_XVID ||
						 pstMsvcCh->u32CodecType == VDC_CODEC_DIVX4 ||
						 pstMsvcCh->u32CodecType == VDC_CODEC_DIVX5 ) )
					msvc_cal_divx_vari_framerate(pstMsvcCh, pstPicInfo);
//			}
#if 0
			if( pstPicInfo->u8DispSkipFlag )
			{
				u32TempFlag = MSVC_HAL_GetFrameDisplayFlag(u32CoreNum, u32InstNum);
				u32TempFlag &= ~(1<<i32IndexFrameDecoded);
				MSVC_HAL_SetFrameDisplayFlag(u32CoreNum, u32InstNum, u32TempFlag);
				pstPicInfo->u32IndexFrameDecoded = BODA_MAX_FRAME_BUFF_NUM;

//				VDEC_KDRV_Message(ERROR,"[MSVC%d] Display skip #%d CurDisp %d Mark %08X", pstMsvcCh->u32Ch,
//						i32IndexFrameDecoded,
//						DE_IF_GetRunningFrameIndex(pstMsvcCh->u8DeCh),
//						MSVC_HAL_GetFrameDisplayFlag(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum));
//				stDispQEntry.sPTS.ui32PTS = -1;
			}
			else
#endif
			{
				u32nDisplayRepeat = _GetDisplayPeriod(pstPicInfo->u32ProgSeq,
						pstPicInfo->u32ProgFrame,
						pstPicInfo->u32TopFieldFirst, pstPicInfo->u32RepeatFirstField,
						pstPicInfo->u32PictureStructure);

				_SetDispQEntry(&stDispQEntry, pstPicInfo, u32nDisplayRepeat, pstMsvcCh);

//				if( pstMsvcCh->u8DqCh != 0xFF )
//				{
//					if( pstMsvcCh->i16UnlockIndex == -2 )
//						pstMsvcCh->i16UnlockIndex = (SINT16)i32IndexFrameDecoded;
//					pstMsvcCh->u32DqDpbMark |= 1<<i32IndexFrameDecoded;
//				}
			}
//		}
//		else
//		{
//			if( i32IndexFrameDecoded >=0 )
//				VDEC_KDRV_Message(ERROR,"[MSVC%u] Wrong Frame Index %d>=%u",
//						pstMsvcCh->u32Ch, i32IndexFrameDecoded, u32FrameNum);
//		}
	} // Broadcast
	else
	{ // CHB
		u32nDisplayRepeat = _GetDisplayPeriod(pstPicInfo->u32ProgSeq, pstPicInfo->u32ProgFrame,
				pstPicInfo->u32TopFieldFirst, pstPicInfo->u32RepeatFirstField,
				pstPicInfo->u32PictureStructure);

		_SetDispQEntry(&stDispQEntry, pstPicInfo, u32nDisplayRepeat, pstMsvcCh);
	} // CHB

	if( u32PackedDetected == TRUE )
		MSVC_HAL_PicRunPack(u32CoreNum, u32InstNum);
//	else if( pstCqEntry->bDecAgain && pstMsvcCh->u32CodecType == VDC_CODEC_MPEG2_HP )
//	{
//		u32PackedDetected = TRUE;
//		pstCqEntry->bDecAgain = FALSE;
//		VDEC_KDRV_Message(ERROR,"[MSVC%d] catch up miss match", pstMsvcCh->u32Ch);
//		MSVC_HAL_PicRunPack(u32CoreNum, u32InstNum);
//	}

//error:
//	pstMsvcCh->u32ChBusy = FALSE;
	msvc_ReportPicInfo(pstMsvcCh, pstPicInfo, pstUDInfo, u32PackedDetected, &stDispQEntry);
	pstMsvcCh->u32FrameCnt++;
	return OK;
}
#if 0
SINT32 MSVC_FieldDecDone(MSVC_CH_T *pstMsvcCh, t_CoreInfo *pstCoreInfo)
{
	//static UINT32 u32FieldCnt=0;
	//VDEC_KDRV_Message(ERROR,"Field picture done!!!!!!!!!!!!!!!");
//	pstMsvcCh->bFieldDone = TRUE;
	MSVC_HAL_SetFieldPicFlag(pstMsvcCh->u32CoreNum, pstMsvcCh->u32InstNum, TRUE);
	return 0;
}
#endif
UINT32 MSVC_GetFWVersion(void)
{
	return MSVC_HAL_GetFWVersion(0,NULL);
}
#if 0
int _msvc_parse_param(char *param_line, unsigned int *param_buf, unsigned short param_buf_len)
{
	int n=0;
	char* token;
	int i;

	printk(" Param line %s\n", param_line);

	while( param_line != NULL && n < param_buf_len )
	{
		token = strsep(&param_line, " ");
		param_buf[n] = simple_strtoul(token, NULL, 0);
		n++;
	}

	for(i=0; i<n; i++)
		printk("param %d : %u\n", i, param_buf[i]);

	return n;
}

void MSVC_DebugCmd(UINT32 u32MsvcCh, char* zCmdLine)
{
	MSVC_CH_T *pstMsvcCh;
	UINT32 u32CmdType;
	UINT32 u32Param[10];
	UINT32 u32nParam;

	pstMsvcCh = MSVC_GetHandler(u32MsvcCh);

	u32nParam = _msvc_parse_param(zCmdLine, u32Param, 10);

	u32CmdType = u32Param[0];

	switch(u32CmdType)
	{
		case 12:
			{
				CQ_ENTRY_T stCqEntry;
				UINT32 u32AuType=3;

				if( u32nParam == 0 )
				{
					VDEC_KDRV_Message(ERROR,"CMD <AU Type> <Flush>");
				}

				u8DebugModeOn = TRUE;
				VDEC_KDRV_Message(ERROR, "MSVC Driver Debugging Mode On!!!!");

				if( u32nParam > 1 )
					u32AuType = u32Param[1];

				if( u32nParam > 2 )
					stCqEntry.auAddr = u32Param[2];
				else
					stCqEntry.auAddr = MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum,
							pstMsvcCh->u32InstNum);

				if( u32nParam > 3 )
					stCqEntry.auEnd = u32Param[3];
				else
					stCqEntry.auEnd = stCqEntry.auAddr+0x200000;

				stCqEntry.auType = u32AuType;
				//MSVC_HAL_GetBitstreamWrPtr(u32CoreNum,0);
				stCqEntry.flush = 0;
				stCqEntry.bRingBufferMode = 1;

				MSVC_UpdateWrPtr(pstMsvcCh->u32Ch, stCqEntry.auEnd);
				VDEC_KDRV_Message(ERROR, "[MSVC][DBG %d] Rd : %x Wr : %x AuType:%d Flush:%d",
						pstMsvcCh->u32Ch,
						MSVC_HAL_GetBitstreamRdPtr(pstMsvcCh->u32CoreNum,pstMsvcCh->u32InstNum),
						MSVC_HAL_GetBitstreamWrPtr(pstMsvcCh->u32CoreNum,pstMsvcCh->u32InstNum),
						u32AuType, stCqEntry.flush);
				MSVC_SendCmd(pstMsvcCh->u32Ch, &stCqEntry);
			}
			break;

		case 15:
			// Flush
			MSVC_Flush(pstMsvcCh->u32Ch);
			break;

		case 21:
			dbgPrtLvDeQEnt = u32Param[1];
			break;
		case 22:
			dbgPrtLvPicDone = u32Param[1];
			break;
		case 23:
			dbgPrtLvCqEnt = u32Param[1];
			break;

		default:
			VDEC_KDRV_Message(ERROR, "Not exist debug command %d", u32CmdType);
			break;
	}

	return;
}
#endif
void MSVC_InterruptHandler(UINT32 u32CoreNum)
{
	MSVC_CH_T *pstMsvcCh;
	UINT32 u32MSVC_IntrStatus = 0;
	UINT32 u32Ch;
	t_CoreInfo *pstCoreInfo;
	UINT32 u32InstNum;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);

	// Interrupt Clear
	u32MSVC_IntrStatus = MSVC_HAL_GetIntReason(u32CoreNum);
	MSVC_HAL_SetIntReason(u32CoreNum, 0x0);
	MSVC_HAL_ClearInt(u32CoreNum, 0x1);

	u32InstNum = MSVC_HAL_GetCurInstance(u32CoreNum);
	if( u32InstNum >= MSVC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "Wrong Inst Num %u>=%u", u32InstNum, MSVC_NUM_OF_CHANNEL);
		return;
	}

	u32Ch = u32CoreNum * MSVC_NUM_OF_CHANNEL;
	u32Ch += u32InstNum;

	pstMsvcCh = MSVC_GetHandler(u32Ch);

	if((u32MSVC_IntrStatus & MSVC_SEQ_INIT_CMD_DONE) == MSVC_SEQ_INIT_CMD_DONE)
	{
		msvc_dbgPackedMode[u32Ch] = 0;
		msvc_dbgFieldPic[u32Ch] = 0;
//		msvc_dpb_overflow_cnt[u32Ch] = 0;
		msvc_dbgCmdTs[u32CoreNum][0] = TOP_HAL_GetGSTCC();
		msvc_dbgCmdTs[u32CoreNum][1] = msvc_dbgCmdTs[u32CoreNum][0];
		msvc_dbgPoss[u32CoreNum][0] = 0;
		msvc_dbgPoss[u32CoreNum][1] = 0;
		MSVC_SeqInitDone(pstMsvcCh, pstCoreInfo);
//		pstMsvcCh->u32ChBusy = FALSE;
	}

	if((u32MSVC_IntrStatus & MSVC_PIC_RUN_CMD_DONE) == MSVC_PIC_RUN_CMD_DONE)
	{
		MSVC_PicRunDone(pstMsvcCh, pstCoreInfo);

		MSVC_HAL_SetFieldPicFlag(u32CoreNum, u32InstNum, FALSE);
	}

	if((u32MSVC_IntrStatus & MSVC_SEQ_END_CMD_DONE) == MSVC_SEQ_END_CMD_DONE)
	{
		// Core0 : Ch0, Ch1, Ch2, Ch3  Core1 : Ch4, Ch5, Ch6, Ch7
		//pstrChInfo = &gstrChannelInfo[u32CoreNum][u32InstNum];
		//pstrChInfo->u32SeqInitDone = 0;
	} // MSVC_SEQ_END_CMD_DONE
	if((u32MSVC_IntrStatus & MSVC_SET_FRAME_BUF_CMD_DONE) == MSVC_SET_FRAME_BUF_CMD_DONE)
	{
		//gstrPicRunCmd[u32CoreNum][gu32PicRunCmdRidx[u32CoreNum]].u32MSVC_Ready = 1;
	} // MSVC_SET_FRAME_BUF_CMD_DONE
	if((u32MSVC_IntrStatus & MSVC_DEC_PARA_SET_CMD_DONE) == MSVC_DEC_PARA_SET_CMD_DONE)
	{

	} // MSVC_DEC_PARA_SET_CMD_DONE
	if((u32MSVC_IntrStatus & MSVC_DEC_BUF_FLUSH_CMD_DONE) == MSVC_DEC_BUF_FLUSH_CMD_DONE)
	{
		// fill
		//BIT_WR_PTR_0(u32CoreNum) = pstrChInfo->u32CPB_Wptr;
	} // MSVC_DEC_BUF_FLUSH_CMD_DONE
	if((u32MSVC_IntrStatus & MSVC_FIELD_PIC_DEC_DONE) == MSVC_FIELD_PIC_DEC_DONE)
	{
#if 0
		UINT32 ui32WtrAddr;
		ui32WtrAddr = VES_CPB_GetPhyWrPtr(pstMsvcCh->u8PdecCh) & (~0x1FF);
		MSVC_UpdateWrPtr(pstMsvcCh->u32Ch, ui32WtrAddr);

		if( msvc_dbgFieldPic[u32Ch] == 0 )
		{
			VDEC_KDRV_Message(_TRACE, "[MSVC%d] FILED PIC", u32Ch);
			msvc_dbgFieldPic[u32Ch] = 1;
		}
		MSVC_FieldDecDone(pstMsvcCh, pstCoreInfo);
#else
		MSVC_ADP_ISR_FieldDone(u32Ch);
#endif
	} // MSVC_FIELD_PIC_DEC_DONE
	if((u32MSVC_IntrStatus & MSVC_CPB_EMPTY_STATE) == MSVC_CPB_EMPTY_STATE)
	{
#if 0
		UINT32 ui32WtrAddr, preAddr;

		ui32WtrAddr = VES_CPB_GetPhyWrPtr(pstMsvcCh->u8PdecCh) & (~0x1FF);
		preAddr = MSVC_HAL_GetBitstreamWrPtr(u32CoreNum, u32InstNum);

		if( u8DebugModeOn == FALSE )
		{
			MSVC_UpdateWrPtr(pstMsvcCh->u32Ch, ui32WtrAddr);
			VDEC_KDRV_Message(DBG_MSVC2, "MSVC #%d CPB is empty & update %08X -> %08X\n", u32CoreNum, preAddr, ui32WtrAddr);
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "MSVC #%d CPB is empty: %08X, PDEC_CPB %08X\n", u32CoreNum, preAddr, ui32WtrAddr);
		}
#else
		MSVC_ADP_ISR_Empty(u32Ch);
#endif
	} // MSVC_CPB_EMPTY_STATE
	if((u32MSVC_IntrStatus & MSVC_USERDATA_OVERFLOW) == MSVC_USERDATA_OVERFLOW)
	{
		VDEC_KDRV_Message(ERROR, "MSVC #%d Userdata buffer overflow", u32Ch);
	}
}

UINT32 MSVC_DECODER_ISR(UINT32 ui32ISRMask)
{
	if (ui32ISRMask&(1<<MACH0)) {
#ifdef __XTENSA__
		TOP_HAL_ClearInterIntrMsk(1<<MACH0);
#else //!__XTENSA__
		TOP_HAL_ClearExtIntrMsk(1<<MACH0);
#endif //~__XTENSA__
		ui32ISRMask = ui32ISRMask&~(1<<MACH0);

		MSVC_InterruptHandler(0);
	}
	if (ui32ISRMask&(1<<MACH1)) {
#ifdef __XTENSA__
		TOP_HAL_ClearInterIntrMsk(1<<MACH1);
#else //!__XTENSA__
		TOP_HAL_ClearExtIntrMsk(1<<MACH1);
#endif //~__XTENSA__
		ui32ISRMask = ui32ISRMask&~(1<<MACH1);

		MSVC_InterruptHandler(1);
	}

#ifndef __XTENSA__
	if (ui32ISRMask&(1<<MACH0_SRST_DONE)) {
//		VDEC_KDRV_Message(_TRACE, "Intr MACH0_SRST_DONE\n");
//		MSVC_SetRstReady(MACH0_SRST_DONE-MACH0_SRST_DONE);
//		TOP_HAL_DisableExtIntr(MACH0_SRST_DONE);
		ui32ISRMask = ui32ISRMask&~(1<<MACH0_SRST_DONE);
		MSVC_ADP_ISR_ResetDone(0);
	}
	if (ui32ISRMask&(1<<MACH1_SRST_DONE)) {
//		VDEC_KDRV_Message(_TRACE, "Intr MACH1_SRST_DONE\n");
//		MSVC_SetRstReady(MACH1_SRST_DONE-MACH0_SRST_DONE);
//		TOP_HAL_DisableExtIntr(MACH1_SRST_DONE);
		ui32ISRMask = ui32ISRMask&~(1<<MACH1_SRST_DONE);
		MSVC_ADP_ISR_ResetDone(1);
	}
#endif

	return ui32ISRMask;
}
