#ifdef __XTENSA__
#include <stdio.h>
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>	// udelay
#endif

#include "../../mcu/vdec_type_defs.h"
#include "../../mcu/os_adap.h"

#include "msvc_hal_api.h"

#include "../../mcu/vdec_print.h"
#include "msvc_fw.h"

typedef struct
{
	UINT8 ProgSeq;
	UINT8 ProgFrame;
	UINT8 TopFieldFirst;
	UINT8 RepeatFirstField;
	UINT8 PictureStructure;
	UINT8 NumClockTS;
	UINT8 DeltaTfiDivisor;
} PIC_STRUCT_T;

static PIC_STRUCT_T stAVCPicStruct[9] =
{
	{1,1,0,0,3,1,2},	// 0: 1 Frame Display
	{0,0,0,0,1,1,1},	// 1: B->T Display (Field Pic)
	{0,0,1,0,2,1,1},	// 2: T->B Display (Field Pic)
	{0,0,1,0,3,2,2},	// 3: T->B Display
	{0,0,0,0,3,2,2},	// 4: B->T Display
	{0,1,1,1,3,3,3},	// 5: T->B->T Display
	{0,1,0,1,3,3,3},	// 6: B->T->B Display
	{1,1,0,1,3,2,4},	// 7: 2 Frame Display
	{1,1,1,1,3,3,8}	// 8: 3 Frame Display
};

static t_CoreInfo gstrCoreInfo[MSVC_NUM_OF_CORE];

static UINT32 *gpVdecBaseVirAddr[2] = {NULL, NULL};

static UINT32 dbgFrameCnt=0;
volatile UINT32 MSVC_BASEReg;

/****************************************************************************
 *         Internal Function
 ****************************************************************************/
static BOOLEAN _MSVC_HAL_BusyWait(UINT32 u32CoreNum)
{
	UINT32 u32BusyCnt=0;
	//UINT64 u64PreTick, u64CurTick;
	//UINT32 u32ElapTick;

	//u64PreTick = get_jiffies_64();	// measure real delay
	while( MSVC_HAL_IsBusy(u32CoreNum) && (u32BusyCnt < BODA_BUSY_THRSHD) )
	{
#ifndef __XTENSA__
		udelay(50);
#endif
		u32BusyCnt++;
	}
	//u64CurTick = get_jiffies_64();
	//u32ElapTick = u64CurTick - u64PreTick;

	if( u32BusyCnt == BODA_BUSY_THRSHD )
	{
		//VDEC_KDRV_Message(ERROR, "[MSVC%d] Busy cancelling fail! %u  %u ms",
		//		u32CoreNum,  u32BusyCnt, u32ElapTick*1000/HZ );
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d] Busy cancelling fail! %u", u32CoreNum, u32BusyCnt );

		return FALSE;
	}

	return TRUE;
}

void MSVC_Reg_Init(void)
{
	MSVC_BASEReg = (volatile UINT32)VDEC_TranselateVirualAddr((LG1152_VDEC_BASE + 0x1000), 0x400);
}

void InitCoreInfo(t_CoreInfo *pstrCoreInfo, UINT32 u32VdecBufBase, UINT32 u32VdecBufSize)
{
	pstrCoreInfo->u32VirCoreBaseAddr	= (UINT32)VDEC_TranselateVirualAddr(u32VdecBufBase, u32VdecBufSize);

	pstrCoreInfo->u32CoreBaseAddr		= u32VdecBufBase;
	pstrCoreInfo->u32CodeBufStartAddr	= u32VdecBufBase;
	pstrCoreInfo->u32ParaBufStartAddr	= u32VdecBufBase + CODE_BUF_SIZE;
	pstrCoreInfo->u32PsBufStartAddr		= u32VdecBufBase + CODE_BUF_SIZE + PARA_BUF_SIZE;
	pstrCoreInfo->u32SliceBufStartAddr	= u32VdecBufBase + CODE_BUF_SIZE + PARA_BUF_SIZE
													+ PS_BUF_SIZE;
	pstrCoreInfo->u32WorkBufStartAddr	= u32VdecBufBase + CODE_BUF_SIZE + PARA_BUF_SIZE
													+ PS_BUF_SIZE + SLICE_BUF_SIZE;
	pstrCoreInfo->u32UserBufStartAddr	= u32VdecBufBase + CODE_BUF_SIZE + PARA_BUF_SIZE
													+ PS_BUF_SIZE + SLICE_BUF_SIZE + WORK_BUF_SIZE;
	pstrCoreInfo->u32UserBufCurAddr		= u32VdecBufBase + CODE_BUF_SIZE + PARA_BUF_SIZE
													+ PS_BUF_SIZE + SLICE_BUF_SIZE + WORK_BUF_SIZE;
	pstrCoreInfo->u32CodeBufSize			= CODE_BUF_SIZE;
	pstrCoreInfo->u32ParaBufSize			= PARA_BUF_SIZE;
	pstrCoreInfo->u32PsBufSize			= PS_BUF_SIZE;
	pstrCoreInfo->u32SliceBufSize		= SLICE_BUF_SIZE;
	pstrCoreInfo->u32WorkBufSize			= WORK_BUF_SIZE;
	pstrCoreInfo->u32UserBufSize			= USER_BUF_SIZE;
	pstrCoreInfo->u32UserBufOneSize		= USER_BUF_ONE_SIZE;

	if( pstrCoreInfo->u32UserBufStartAddr + pstrCoreInfo->u32UserBufSize > u32VdecBufBase + u32VdecBufSize )
		VDEC_KDRV_Message(ERROR, "[MSVC C%u] Error Buff Setting %u<%u",
				pstrCoreInfo->u32CoreNum, u32VdecBufSize,
				pstrCoreInfo->u32UserBufStartAddr + pstrCoreInfo->u32UserBufSize - u32VdecBufBase);

	VDEC_KDRV_Message(DBG_VDC, "[MSVC C%d] Base 0x%08X(%X) VirBase 0x%08X",
			pstrCoreInfo->u32CoreNum, u32VdecBufBase, u32VdecBufSize, pstrCoreInfo->u32VirCoreBaseAddr);
}

SINT32 MSVC_InitInstance(t_CoreInfo *pstrCoreInfo, t_InstanceInfo *pstInstInfo, UINT32 u32InstNum)
{
	DEC_PARAM_T *pstDecParam;

	pstDecParam = &pstInstInfo->stDecParam;

	pstInstInfo->pstCoreInfo = pstrCoreInfo;
	pstInstInfo->u32InstNum = u32InstNum;

	//pstDecParam->stUserDataParam.i32UserDataEn = 0;
	//pstDecParam->stUserDataParam.ui32UserDataBufAddr=USER_DATA_BASE_ADDR;
	//pstDecParam->stUserDataParam.i32UserDataBufSize=USER_DATA_BUF_SIZE;

	//pstDecParam->i32FrameSearchEnable = 0;
	//pstDecParam->i32PrescanEnable = 0;
	//pstDecParam->i32PrescanMode = 0;

	//pstDecParam->i32SkipframeMode = 0;
	//pstDecParam->i32SkipframeNum = 0;

	memset( &pstInstInfo->stRotInfo, 0, sizeof(PIC_ROTATION_INFO_T));

	return 0;
}

SINT32 MSVC_InitCore(t_CoreInfo *pstrCoreInfo, UINT32 u32CoreNum)
{
	SINT32 i;
	t_InstanceInfo *pstInstInfo;

	pstrCoreInfo->u32CoreNum = u32CoreNum;

	for(i=0; i<MSVC_NUM_OF_CHANNEL; i++)
	{
		pstInstInfo = &pstrCoreInfo->stInstInfo[i];
		MSVC_InitInstance(pstrCoreInfo, pstInstInfo, i);
	}

	return 0;
}

BOOLEAN MSVC_InitReg(t_CoreInfo *pstrCoreInfo)
{
	UINT32 u32CoreNum;

	u32CoreNum = pstrCoreInfo->u32CoreNum;

	//InitCoreInfo(u32CoreNum);
	//pstrCoreInfo = &gstrCoreInfo[u32CoreNum];

	BIT_BUSY_FLAG(u32CoreNum) = 0;

	BIT_CODE_BUF_ADDR(u32CoreNum) = pstrCoreInfo->u32CodeBufStartAddr;
	BIT_PARA_BUF_ADDR(u32CoreNum) = pstrCoreInfo->u32ParaBufStartAddr;
	BIT_WORK_BUF_ADDR(u32CoreNum) = pstrCoreInfo->u32WorkBufStartAddr;

	BIT_BIT_STREAM_CTRL(u32CoreNum) = (BIT_STREAM_BUFF_LITTLE_ENDIAN |
										BIT_STREAM_BUFF_64BIT_ENDIAN |
										BIT_STREAM_FULL_EMPTY_CHECK_ENABLE);
	BIT_FRAME_MEM_CTRL(u32CoreNum) = (BIT_FRAME_CBCR_INTERLEAVE |
										BIT_FRAME_MEM_LITTLE_ENDIAN |
										BIT_FRAME_BUFF_64BIT_ENDIAN |
										BIT_FRAME_CBCR_INTERLEAVE1 |
										BIT_FRAME_CBCR_INTERLEAVE2 |
										BIT_FRAME_CBCR_INTERLEAVE3);

	BIT_BIT_STREAM_PARAM(u32CoreNum) = 0;
#ifdef SECOND_AXI
	BIT_AXI_SRAM_USE(u32CoreNum) = 0x3F3F; //0; // for secondary setting
#else
	BIT_AXI_SRAM_USE(u32CoreNum) = 0;
#endif
	BIT_INT_ENABLE(u32CoreNum) =
		//MSVC_RESERVED0 |
		MSVC_SEQ_INIT_CMD_DONE |
		MSVC_SEQ_END_CMD_DONE |
		MSVC_PIC_RUN_CMD_DONE |
		//MSVC_SET_FRAME_BUF_CMD_DONE |
		//MSVC_RESERVED1 |
		//MSVC_RESERVED2 |
		MSVC_DEC_PARA_SET_CMD_DONE |
		//MSVC_DEC_BUF_FLUSH_CMD_DONE |
		//MSVC_USRDATA_OVERFLOW |
		MSVC_FIELD_PIC_DEC_DONE |
		//MSVC_RESERVED4 |
		//MSVC_RESERVED5 |
		//MSVC_RESERVED6 |
		MSVC_CPB_EMPTY_STATE ;
		//MSVC_RESERVED7 ;

	BIT_FRM_DIS_FLG_0(u32CoreNum) = 0;
	BIT_FRM_DIS_FLG_1(u32CoreNum) = 0;
	BIT_FRM_DIS_FLG_2(u32CoreNum) = 0;
	BIT_FRM_DIS_FLG_3(u32CoreNum) = 0;

//	BIT_CODE_RUN(u32CoreNum) = 0x1;

	return TRUE;
}

/****************************************************************************
 *         HAL Function
 ****************************************************************************/
t_CoreInfo* MSVC_HAL_GetCoreInfo(UINT32 u32CoreNum)
{
	if( u32CoreNum >= MSVC_NUM_OF_CORE )
		return 0;

	return &gstrCoreInfo[u32CoreNum];
}

SINT32 MSVC_HAL_Init(UINT32 u32CoreNum, UINT32 u32VdecBufBase, UINT32 u32VdecBufSize)
{
	SINT32 i;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstCoreInfo->u32CoreNum = u32CoreNum;

	InitCoreInfo(pstCoreInfo, u32VdecBufBase, u32VdecBufSize);

	for(i=0; i<MSVC_NUM_OF_CHANNEL; i++)
	{
		pstInstInfo = &pstCoreInfo->stInstInfo[i];
		MSVC_InitInstance(pstCoreInfo, pstInstInfo, i);
	}

	MSVC_InitReg(pstCoreInfo);

	return 0;
}

UINT32 MSVC_HAL_IsBusy(UINT32 u32CoreNum)
{
	return BIT_BUSY_FLAG(u32CoreNum);
}

SINT32 MSVC_HAL_SetStreamFormat(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32BitStreamFormat)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	pstInstInfo->u32BitStreamFormat = u32BitStreamFormat;

	return OK;
}

UINT32 MSVC_HAL_GetStreamFormat(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	return pstInstInfo->u32BitStreamFormat;
}

BOOLEAN MSVC_HAL_SeqInit(UINT32 u32CoreNum, UINT32 u32InstNum, SEQ_PARAM_T *pstSeqParam)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;
	UINT32		u32SeqOption;

	dbgFrameCnt=0;
	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	pstInstInfo->u32PrePicStruct = 0xFF;
	pstInstInfo->i16PreFramePackArrange = -1;
	pstInstInfo->u32FieldPicFlag = FALSE;
	pstInstInfo->u32BitStreamFormat = pstSeqParam->u32BitStreamFormat;
	pstInstInfo->u32RunAuxStd = pstSeqParam->u32AuxStd;
	pstInstInfo->u32SeqAspClass = pstSeqParam->u32SeqAspClass;

	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
		return FALSE;
	}

	BIT_BIT_STREAM_PARAM(u32CoreNum) &= ~(0x1 << (2 + u32InstNum));

	BIT_DEC_FUN_CTRL(u32CoreNum) |= 1;
	BIT_DEC_FUN_CTRL(u32CoreNum) &= ~(1<<(2+u32InstNum));
	CMD_DEC_SEQ_BB_START(u32CoreNum) = pstSeqParam->u32BitStreamBuffer;
	CMD_DEC_SEQ_BB_SIZE(u32CoreNum) = pstSeqParam->u32BitStreamBufferSize >> 10;

	// Ring Buffer : 0010
	// Line Buffer : 1110
	u32SeqOption = 0;
	if( pstSeqParam->sSeqOption.bDeblockEnable == TRUE )	// MP2, MP4
		u32SeqOption |= (1 << 0);
	if( pstSeqParam->sSeqOption.bReorderEnable == TRUE )
		u32SeqOption |= (1 << 1);
	if( pstSeqParam->sSeqOption.bFilePlayEnable == TRUE )
		u32SeqOption |= (1 << 2);
	if( pstSeqParam->sSeqOption.bDynamicBufEnable == TRUE )
		u32SeqOption |= (1 << 3);

	pstInstInfo->u32SeqOption = u32SeqOption;

	CMD_DEC_SEQ_OPTION(u32CoreNum) = u32SeqOption;
	CMD_DEC_SEQ_SRC_SIZE(u32CoreNum) = (pstSeqParam->u32PicWidth<<16) | pstSeqParam->u32PicHeight;
	CMD_DEC_SEQ_START_BYTE(u32CoreNum) = 0;

	if (pstInstInfo->u32BitStreamFormat == AVC_DEC) {
		CMD_DEC_SEQ_PS_BB_START(u32CoreNum) = pstCoreInfo->u32PsBufStartAddr;
		CMD_DEC_SEQ_PS_BB_SIZE(u32CoreNum) = pstCoreInfo->u32PsBufSize >> 10;
	}

	CMD_DEC_SEQ_ASP_CLASS(u32CoreNum) = pstSeqParam->u32SeqAspClass;

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum) = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_SEQ_INIT;

	return TRUE;
}

SINT32 MSVC_HAL_SeqEnd(UINT32 u32CoreNum, UINT32 u32InstNum )
{
	SINT32 s32RetVal;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum) = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_SEQ_END;
	s32RetVal = OK;

	return s32RetVal;
}

SINT32 MSVC_HAL_SetFrameBuffer(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32nFrame, UINT32 u32FrameDelay, UINT32 u32Stride)
{
	SINT32 s32RetVal = OK;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	CMD_SET_FRAME_BUF_NUM(u32CoreNum) = u32nFrame;
	CMD_SET_FRAME_BUF_STRIDE(u32CoreNum) = u32Stride;

	CMD_SET_FRAME_SLICE_BB_START(u32CoreNum) = pstCoreInfo->u32SliceBufStartAddr;
	CMD_SET_FRAME_SLICE_BB_SIZE(u32CoreNum) = pstCoreInfo->u32SliceBufSize >> 10;
	CMD_SET_FRAME_FRAME_DELAY(u32CoreNum) = u32FrameDelay;		// default: -1

#ifdef SECOND_AXI
	CMD_AXI_SRAM_BIT_ADDR(u32CoreNum) = 0xAA00000;
	CMD_AXI_SRAM_IPACDC_ADDR(u32CoreNum) = 0xAA04000;
	CMD_AXI_SRAM_DBKY_ADDR(u32CoreNum) = 0xAA08000;
	CMD_AXI_SRAM_DBKC_ADDR(u32CoreNum) = 0xAA10000;
	CMD_AXI_SRAM_OVL_ADDR(u32CoreNum) = 0xAA180000;
#endif

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum)   = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_SET_FRAME;

	if( _MSVC_HAL_BusyWait(u32CoreNum) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
	}

	return s32RetVal;
}

/** u32FlushType
 *	0: for default flush   1: for in-place flush
 **/
BOOLEAN MSVC_HAL_Flush(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32FlushType, UINT32 u32ReadPtr)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	//VDEC_KDRV_Message(ERROR,"FLush Rd %08X", u32ReadPtr);
	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy - FlushType:%d, %s", u32CoreNum, u32InstNum, u32FlushType, __FUNCTION__ );
		return FALSE;
	}

	CMD_DEC_BUF_FLUSH_TYPE(u32CoreNum) = u32FlushType;

	// twkim
	if( u32FlushType == 1 )
		CMD_DEC_BUF_FLUSH_RDPTR(u32CoreNum) = u32ReadPtr;
	//	CMD_DEC_BUF_FLUSH_RDPTR(u32CoreNum) = pstInstInfo->u32CPB_Rptr;

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum)   = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_DEC_FLUSH;

	if( u32FlushType == 0 )
	{
		// twkim
		//pstInstInfo->u32AUI_Rptr     = pstInstInfo->u32AUI_StartAddr;
		//pstInstInfo->u32AUI_Wptr     = pstInstInfo->u32AUI_StartAddr;
	}

	//while ((BIT_INT_REASON(u32CoreNum) & MSVC_DEC_BUF_FLUSH_CMD_DONE) == 0);
	if( _MSVC_HAL_BusyWait(u32CoreNum) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] BusyWait - FlushType:%d, %s", u32CoreNum, u32InstNum, u32FlushType, __FUNCTION__ );
		return FALSE;
	}

	return TRUE;
}

SINT32 MSVC_HAL_Sleep(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	SINT32 s32RetVal;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum)   = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_SLEEP;

	s32RetVal = OK;

	return s32RetVal;
}

SINT32 MSVC_HAL_Wake(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	SINT32 s32RetVal;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
	BIT_RUN_INDEX(u32CoreNum)   = u32InstNum;
	BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_WAKE_UP;

	s32RetVal = OK;

	return s32RetVal;
}

static UINT16 _msvc_get_GCD(UINT32 u, UINT32 v)
{
	UINT32 t;

	while(u)
	{
		if(u<v)
		{
			t=u; u=v; v=t;
		}

		u = u-v;
	}

	return v;
}

static UINT16 _msvc_GetAspectRatio(UINT16 frameW, UINT16 frameH, UINT32 u32SarVal,
		UINT8 *pu8Idc, UINT16 *pu16SarW, UINT16 *pu16SarH)
{
	UINT32 idc;
	UINT16 sarW=1, sarH=1;
	UINT32 dispW, dispH;
	UINT32 arMp2;
	UINT32 dispAR100;
	UINT8 aDar[5][2] = {
		{ 0, 0}, { 1, 1}, {4,3}, {16,9}, {221,100}
	};
	UINT16 gcd;

	idc = u32SarVal;

	if( idc >=2 && idc <= 4 )
	{
		if( frameH == 1088 )
			frameH = 1080;

		sarW = aDar[idc][0] * frameH;
		sarH = aDar[idc][1] * frameW;
		gcd = _msvc_get_GCD(sarW, sarH);

		if( gcd > 1 )
		{
			sarW = sarW/gcd;
			sarH = sarH/gcd;
		}
		else
		{
			sarW = 100;
			sarH = 100 * aDar[idc][1] * frameW / frameH / aDar[idc][0];
		}

		arMp2 = idc;

//		printk(" DAR %d SAR %d %d GCD %d F %d/%d \n", arMp2, sarW, sarH, gcd, frameW, frameH);
	}
	else
	{
		sarW = 1;
		sarH = 1;

		dispW = frameW * sarW;
		dispH = frameH * sarH;

		if( dispH == 0 )
			dispH = 1;

		dispAR100 = (UINT32)dispW*100/dispH;

		if( dispAR100 >= 210)
			arMp2 = 4;
		else if( dispAR100 >= 160)
			arMp2 = 3;
		else
			arMp2 = 2;
	}

	*pu8Idc = idc;
	*pu16SarW = sarW;
	*pu16SarH = sarH;

	return arMp2;
}

static UINT16 _msvc_GetAspectRatioForMP4(UINT16 u16StreamFormat,
		UINT16 frameW, UINT16 frameH, UINT32 u32SarVal,
		UINT8 *pu8Idc, UINT16 *pu16SarW, UINT16 *pu16SarH)
{
	static UINT32 u32DbgCnt=0;
	UINT32 idc;
	UINT16 sarW=1, sarH=1;
	UINT32 dispW, dispH;
	UINT32 arMp2;
	UINT32 dispAR100;
	UINT8 aSar[17][2] = {
		{ 0, 0}, { 1, 1}, {12,11}, {10,11},
		{16,11}, {40,33}, {24,11}, {20,11},
		{32,11}, {80,33}, {18,11}, {15,11},
		{64,33}, {160,99}, {4, 3}, { 3, 2},
		{ 2, 1}
	};

	if( u16StreamFormat == AVC_DEC )
	{
		// AVC
		if( u32SarVal >> 16 )
			idc = 0xFF;
		else
			idc = u32SarVal & 0xFF;

		if( idc > 0 && idc <= 16 )
		{
			sarW = aSar[idc][0];
			sarH = aSar[idc][1];
		}
		else if( idc == 0xFF )
		{
			// extended SAR
			sarW = u32SarVal >> 16;
			sarH = u32SarVal & 0xFFFF;
		}
		else
		{
			// != Unspecified
			if( idc != 0 && !((u32DbgCnt++)%90) )
				VDEC_KDRV_Message(ERROR,"wrong SAR idc %d", idc);
		}
	}
	else	// MP4 DivX
	{
		idc = u32SarVal & 0x0F;

		if( idc > 0 && idc <= 5 )
		{
			sarW = aSar[idc][0];
			sarH = aSar[idc][1];
		}
		else if( idc == 0x0F )
		{
			// extended SAR
			sarW = (u32SarVal >> 4) & 0xFF;
			sarH = (u32SarVal >>12) & 0xFF;
		}
		else // Unspecified
			if( !((u32DbgCnt++)%90))
				VDEC_KDRV_Message(ERROR,"wrong SAR idc %d", idc);
	}

	dispW = frameW * sarW;
	dispH = frameH * sarH;

	if( dispH == 0 )
		dispH = 1;

	dispAR100 = (UINT32)dispW*100/dispH;

	if( dispAR100 >= 210)
		arMp2 = 4;
	else if( dispAR100 >= 160)
		arMp2 = 3;
	else
		arMp2 = 2;

	*pu8Idc = idc;
	*pu16SarW = sarW;
	*pu16SarH = sarH;

	return arMp2;
}

void _msvc_set_min_frame_num(UINT32 u32StreamFormat, UINT32 u32Level,
			UINT32 u32PicWidth, UINT32 u32PicHeight,
			UINT32 *pu32MinFrameBufferCount, UINT32 *pu32FrameBufDelay)
{
	UINT32 u32Temp;
	UINT32 u32BufDelay;

	if( u32StreamFormat == AVC_DEC && u32Level > 41 )
	{
		if( pu32FrameBufDelay == NULL )
			u32BufDelay = *pu32MinFrameBufferCount-2;
		else
			u32BufDelay = *pu32FrameBufDelay;

		u32Temp = 32768*16*16/u32PicWidth/u32PicHeight;
		if( u32Temp < u32BufDelay )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC] AVC Over spec L%d(>AVC L4.1) nMinFrm %d BufDelay %d",
					u32Level,*pu32MinFrameBufferCount, u32BufDelay);

			u32BufDelay = u32Temp;
			if( pu32FrameBufDelay != NULL )
				*pu32FrameBufDelay = u32BufDelay;
			*pu32MinFrameBufferCount = u32Temp+2;

			VDEC_KDRV_Message(ERROR, "[MSVC]   -> nMinFrm %d BufDelay %d",
					*pu32MinFrameBufferCount,
					u32BufDelay);
		}
	}
}

BOOLEAN _MSVC_HAL_CheckDpbLevel(UINT32 ui32PicWidth, UINT32 ui32PicHeight, UINT32 ui32DpbSize)
{
	UINT32	ui32NumMb_Width;
	UINT32	ui32NumMb_Height;
	UINT32	ui32NumMb;

	ui32NumMb_Width = ui32PicWidth >> 4; // / 16;
	if( ui32PicWidth & 0xF )
		ui32NumMb_Width++;
	ui32NumMb_Height = ui32PicHeight >> 4; // / 16;
	if( ui32PicHeight & 0xF )
		ui32NumMb_Height++;

	ui32NumMb = ui32NumMb_Width * ui32NumMb_Height;

	// Level 4.1 from ISO/IEC 14496-10:2010(E) Table A-1 - Level limits
	if( (ui32NumMb * ui32DpbSize) <= 32768 )
		return TRUE;

	VDEC_KDRV_Message(ERROR, "[MSVC_HAL][Err] MB Num(%d * %d) * DpbSize(%d) = %d, %s", ui32NumMb_Width, ui32NumMb_Height, ui32DpbSize, ui32NumMb * ui32DpbSize, __FUNCTION__ );

	return FALSE;
}

BOOLEAN MSVC_HAL_GetSeqInfo(UINT32 u32CoreNum, UINT32 u32InstNum, SEQ_INFO_T *pstSeqInfo)
{
	BOOLEAN bRetVal = TRUE;
	UINT32 u32SrcSize;
	UINT32 u32RateInfo;
	UINT32 u32StreamFormat;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;
	UINT32 ui32DpbSize;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];
	u32StreamFormat = pstInstInfo->u32BitStreamFormat;

	pstSeqInfo->bOverSpec = FALSE;

	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
		return FALSE;
	}

	pstSeqInfo->u32IsSuccess = RET_DEC_SEQ_SUCCESS(u32CoreNum);
	if( (pstSeqInfo->u32IsSuccess&0x01) == 0x0)
		return FALSE;

	u32SrcSize = RET_DEC_SEQ_SRC_SIZE(u32CoreNum);
	pstSeqInfo->u32PicWidth= ( (u32SrcSize >> 16) & 0xffff );
	pstSeqInfo->u32PicHeight = ( u32SrcSize & 0xffff );
	pstSeqInfo->u32MinFrameBufferCount = RET_DEC_SEQ_FRAME_NEED(u32CoreNum) & 0x1F;
	pstSeqInfo->u32FrameBufDelay = RET_DEC_SEQ_FRAME_DELAY(u32CoreNum) & 0x1F;
	pstSeqInfo->u32AspectRatio = RET_DEC_SEQ_ASPECT(u32CoreNum);
	pstSeqInfo->u32DecSeqInfo = RET_DEC_SEQ_INFO(u32CoreNum);
	pstSeqInfo->u32NextDecodedIdxNum = RET_DEC_SEQ_NEXT_FRAME_NUM(u32CoreNum);
	pstSeqInfo->u32SeqHeadInfo = RET_DEC_SEQ_HEADER_INFO(u32CoreNum);
	pstSeqInfo->u32Profile = RET_DEC_SEQ_HEADER_INFO(u32CoreNum) & 0xFF;
	pstSeqInfo->u32Level = (RET_DEC_SEQ_HEADER_INFO(u32CoreNum)>>8) & 0xFF;
	pstSeqInfo->u8FixedFrameRate = 1;

	if( (u32StreamFormat == AVC_DEC) && (pstInstInfo->u32RunAuxStd == 1) ) // MVC
		ui32DpbSize = pstSeqInfo->u32MinFrameBufferCount / 2;
	else if( pstSeqInfo->u32MinFrameBufferCount > 2 )
		ui32DpbSize = pstSeqInfo->u32MinFrameBufferCount - 2;
	else
		ui32DpbSize = pstSeqInfo->u32MinFrameBufferCount;

	if( _MSVC_HAL_CheckDpbLevel(pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight, ui32DpbSize) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Codec:%d, Profile:%d, Level:%d, Resolution: %d * %d, MinBufCnt:%d, %s", u32CoreNum, u32InstNum, u32StreamFormat, pstSeqInfo->u32Profile, pstSeqInfo->u32Level, pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight, pstSeqInfo->u32MinFrameBufferCount, __FUNCTION__ );
		pstSeqInfo->u32IsSuccess &= ~0x1;
		pstSeqInfo->bOverSpec = TRUE;
		bRetVal = FALSE;
	}

	_msvc_set_min_frame_num(u32StreamFormat,pstSeqInfo->u32Level,
			pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight,
			&pstSeqInfo->u32MinFrameBufferCount,
			&pstSeqInfo->u32FrameBufDelay);
	pstInstInfo->u32MinFrameBufferCount = pstSeqInfo->u32MinFrameBufferCount;

	// Progressive sequence ??
	if( u32StreamFormat == RV_DEC )
	{
		pstSeqInfo->u32ProgSeq = 0x1;
	}
	else
	{
		if( (RET_DEC_SEQ_HEADER_INFO(u32CoreNum) >> 16) & 0x01 )
			pstSeqInfo->u32ProgSeq = 0x0;
		else
			pstSeqInfo->u32ProgSeq = 0x1;
	}

	if( pstSeqInfo->u32ProgSeq == 0x1 )
		pstInstInfo->u32FrameMbsOnlyFlag = 0x0;
	else
		pstInstInfo->u32FrameMbsOnlyFlag = 0x1;

	switch( u32StreamFormat )
	{
		case MP2_DEC :
		case AVS_DEC :
		case VC1_DEC :
		case MP4_DIVX3_DEC :
			u32RateInfo = RET_DEC_SEQ_SRC_F_RATE(u32CoreNum);

			if( (SINT32)u32RateInfo == -1 )
			{
				pstSeqInfo->u32FrameRateRes = 0;
				pstSeqInfo->u32FrameRateDiv = 0;
			}
			else
			{
				pstSeqInfo->u32FrameRateRes = (u32RateInfo & 0xFFFF);
				pstSeqInfo->u32FrameRateDiv = ((u32RateInfo >> 16) & 0xFFFF) + 1;

				if( pstSeqInfo->u32FrameRateDiv == 0 )
					pstSeqInfo->u32FrameRateRes = 0;
			}
/*
			if( u32StreamFormat == VC1_DEC )
			{
				if(	pstInstInfo->u32SeqAspClass == 2 )	// VC1 AP
				{
					if( (pstSeqInfo->u32FrameRateDiv == 1000 || pstSeqInfo->u32FrameRateDiv == 1001)
							&& pstSeqInfo->u32FrameRateRes <= 72 )
						pstSeqInfo->u32FrameRateRes *= 1000;
				}
				else // VC1 SP/MP
				{
					pstSeqInfo->u32FrameRateRes = 0;
					pstSeqInfo->u32FrameRateDiv = 0;
				}
			}
*/
			if( u32StreamFormat == MP4_DIVX3_DEC )
			{
				if( pstInstInfo->u32SeqAspClass == 0 && pstInstInfo->u32RunAuxStd == 1 )
				{
					// for DivX3
					pstSeqInfo->u8FixedFrameRate = 1;
					pstSeqInfo->u32FrameRateRes = 0;
					pstSeqInfo->u32FrameRateDiv = 0;
				}
				else
				{
					// for DivX4/5  Xvid, MPEG4_ASP, H.263
					pstSeqInfo->u8FixedFrameRate = (pstSeqInfo->u32DecSeqInfo >> 4 ) & 1;

					if( pstSeqInfo->u8FixedFrameRate == 0 )
					{
						pstSeqInfo->u32FrameRateRes = 0;
						pstSeqInfo->u32FrameRateDiv = 0;
					}
				}
			}

			pstSeqInfo->u32InterlacedSeq = (pstSeqInfo->u32SeqHeadInfo >> 16 ) & 1;
			break;

		case AVC_DEC :
			pstSeqInfo->u8FixedFrameRate = (pstSeqInfo->u32SeqHeadInfo >> 23 ) & 1;
			pstSeqInfo->u32FrameRateRes = RET_DEC_SEQ_TIME_SCALE(u32CoreNum);
			pstSeqInfo->u32FrameRateDiv = RET_DEC_SEQ_NUM_UNITS_IN_TICK(u32CoreNum) << 1;

			// Work around for old AVC standard
			if( pstSeqInfo->u8FixedFrameRate == TRUE &&
					pstSeqInfo->u32FrameRateRes == 25 && pstSeqInfo->u32FrameRateDiv == 2 )
				pstSeqInfo->u32FrameRateRes = 50;

			// Work around for AVC variable frame rate
			if( pstSeqInfo->u8FixedFrameRate == FALSE &&
					pstSeqInfo->u32FrameRateRes == 30000 && pstSeqInfo->u32FrameRateDiv == 2002 )
				pstSeqInfo->u32FrameRateRes = 60000;

			// Wrok around for variable framerate
			if( pstSeqInfo->u8FixedFrameRate == FALSE )
			{
				if( pstSeqInfo->u32FrameRateDiv == 0 ||
						(pstSeqInfo->u32FrameRateRes/pstSeqInfo->u32FrameRateDiv) > 60 )
				{
					pstSeqInfo->u32FrameRateRes = 0;
					pstSeqInfo->u32FrameRateDiv = 0;
				}
			}

			pstSeqInfo->u32InterlacedSeq = (pstSeqInfo->u32SeqHeadInfo >> 16 ) & 1;
			break;
		case RV_DEC :
			pstSeqInfo->u32FrameRateRes = 0;
			pstSeqInfo->u32FrameRateDiv = 0;
			break;
		default :
			pstSeqInfo->u32InterlacedSeq = 0;
			pstSeqInfo->u32FrameRateRes = 0;
			pstSeqInfo->u32FrameRateDiv = 0;

			VDEC_KDRV_Message(ERROR,"%s Invalid Stream Format %d",__FUNCTION__, u32StreamFormat);
			bRetVal = FALSE;
			break;
	}

	// DE constraint(HD 60p is not available)
	if( (pstSeqInfo->u32PicWidth >= 1920 && pstSeqInfo->u32PicHeight >= 1080) &&
			pstSeqInfo->u32ProgSeq &&
			(pstSeqInfo->u32FrameRateDiv != 0 &&
			(pstSeqInfo->u32FrameRateRes/pstSeqInfo->u32FrameRateDiv) > 30)
	  )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] Too high freq. for HD %d/%d",
				u32CoreNum, u32InstNum,
				pstSeqInfo->u32FrameRateRes, pstSeqInfo->u32FrameRateDiv);

		pstSeqInfo->u32FrameRateRes = pstSeqInfo->u32FrameRateDiv*30;
	}

	if( u32StreamFormat == AVC_DEC || u32StreamFormat == MP4_DIVX3_DEC )
	{
		// get aspect ratio from aspect ratio idc
		pstSeqInfo->u32AspectRatio = _msvc_GetAspectRatioForMP4(
				(UINT16)u32StreamFormat,
				pstSeqInfo->u32PicWidth,
				pstSeqInfo->u32PicHeight,
				pstSeqInfo->u32AspectRatio,
				&pstSeqInfo->u8AspectRatioIdc,
				&pstSeqInfo->u16SarW,
				&pstSeqInfo->u16SarH
				);
	}
	else
	{
		pstSeqInfo->u32AspectRatio = _msvc_GetAspectRatio(
				pstSeqInfo->u32PicWidth,
				pstSeqInfo->u32PicHeight,
				pstSeqInfo->u32AspectRatio,
				&pstSeqInfo->u8AspectRatioIdc,
				&pstSeqInfo->u16SarW,
				&pstSeqInfo->u16SarH
				);
	}

	pstInstInfo->u8FixedFrameRate = pstSeqInfo->u8FixedFrameRate;
	VDEC_KDRV_Message(DBG_VDC,"Seq Info Frame Rate %d/%d fixed? %d\n",
			pstSeqInfo->u32FrameRateRes, pstSeqInfo->u32FrameRateDiv, pstSeqInfo->u8FixedFrameRate);

	VDEC_KDRV_Message(DBG_VDC, "[MSVC C%d] Seq Info :", u32CoreNum);
	if( pstSeqInfo->u32IsSuccess )
	{
		VDEC_KDRV_Message(DBG_VDC, " Format		- %u", u32StreamFormat);
		VDEC_KDRV_Message(DBG_VDC, " Profile(Lv)	- %u(%u)", pstSeqInfo->u32Profile, pstSeqInfo->u32Level);
		VDEC_KDRV_Message(DBG_VDC, " Width/Height	- %u/%u", pstSeqInfo->u32PicWidth, pstSeqInfo->u32PicHeight);
		VDEC_KDRV_Message(DBG_VDC, " MinFrameNum	- %u", pstSeqInfo->u32MinFrameBufferCount);
		VDEC_KDRV_Message(DBG_VDC, " Frame Delay	- %u", pstSeqInfo->u32FrameBufDelay);
		VDEC_KDRV_Message(DBG_VDC, " Aspect Ratio	- %u", pstSeqInfo->u32AspectRatio);
		VDEC_KDRV_Message(DBG_VDC, " Seq Info		- 0x%X", pstSeqInfo->u32DecSeqInfo);
		VDEC_KDRV_Message(DBG_VDC, " Seq Header Info - 0x%X", pstSeqInfo->u32SeqHeadInfo);
		VDEC_KDRV_Message(DBG_VDC, " Prog Seq	- %X", pstSeqInfo->u32ProgSeq);
		VDEC_KDRV_Message(DBG_VDC, " Timing Info		- %X", (pstSeqInfo->u32SeqHeadInfo >> 24 ) & 1);
		VDEC_KDRV_Message(DBG_VDC, " Fixed Frm Rate	- %X(%d/%d)",
				pstSeqInfo->u8FixedFrameRate,
				pstSeqInfo->u32FrameRateRes,
				pstSeqInfo->u32FrameRateDiv);
	}

	return bRetVal;
}

static SINT32 _Calculate_Framerate(t_InstanceInfo *pstInstInfo, PIC_INFO_T *pstPicInfo)
{
	SINT32 i32RetVal = OK;
	UINT32 u32CoreNum;
	UINT32 u32RateInfo;
	UINT32 u32FrameRateRes, u32FrameRateDiv;
	UINT32 u32StreamFormat;

	u32CoreNum = pstInstInfo->pstCoreInfo->u32CoreNum;
	u32StreamFormat = pstInstInfo->u32BitStreamFormat;

	if( u32StreamFormat == MP2_DEC || u32StreamFormat == AVS_DEC ||
			u32StreamFormat == VC1_DEC || u32StreamFormat == MP4_DIVX3_DEC )
	{
		u32RateInfo = pstPicInfo->u32FrameRateInfo;

		if( (SINT32)pstPicInfo->u32FrameRateInfo == -1 )
		{
			u32FrameRateRes			= 0;
			u32FrameRateDiv			= 0;
		}
		else
		{
			u32FrameRateRes			= (u32RateInfo & 0xFFFF) ;
			u32FrameRateDiv			= ((u32RateInfo >> 16 ) & 0xFFFF) + 1 ;

			if( u32FrameRateDiv == 0 )
				u32FrameRateRes = 0;
		}
/*
		if( u32StreamFormat == VC1_DEC )
		{
			if( pstInstInfo->u32SeqAspClass == 2 )	// VC1 AP
			{
				if( (u32FrameRateDiv == 1000 || u32FrameRateDiv == 1001)
						&& u32FrameRateRes <= 72)
					u32FrameRateRes *= 1000;
			}
			else	// VC1 SP/MP
			{
				u32FrameRateRes = 0;
				u32FrameRateDiv = 0;
			}
		}
*/
		if( u32StreamFormat == MP4_DIVX3_DEC )
		{
			if( pstInstInfo->u32SeqAspClass == 0 && pstInstInfo->u32RunAuxStd == 1 )
			{
				// for DivX3
				u32FrameRateRes = 0;
				u32FrameRateDiv = 0;
			}
			else
			{
				// for DivX4/5/Xvid/H.263
				if( pstInstInfo->u8FixedFrameRate == 0 )
					u32FrameRateDiv = 0;
			}

		}
	}
	else if(u32StreamFormat == AVC_DEC)
	{
		u32FrameRateRes		= RET_DEC_PIC_TIME_SCALE(u32CoreNum);
		u32FrameRateDiv		= (RET_DEC_PIC_NUM_UNITS_IN_TICK(u32CoreNum)) << 1;

		// Work around for old AVC standard
		if( pstInstInfo->u8FixedFrameRate == TRUE &&
				u32FrameRateRes == 25 && u32FrameRateDiv == 2)
			u32FrameRateRes = 50;

		// Work around for AVC variable frame rate
		if( pstInstInfo->u8FixedFrameRate == FALSE &&
				u32FrameRateRes == 30000 && u32FrameRateDiv == 2002 )
			u32FrameRateRes = 60000;

		// Wrok around for variable framerate
		if( pstInstInfo->u8FixedFrameRate == FALSE )
		{
			if( u32FrameRateDiv == 0 || (u32FrameRateRes/u32FrameRateDiv) > 60 )
			{
				u32FrameRateRes = 0;
				u32FrameRateDiv = 0;
			}
		}

		if( pstInstInfo->u32RunAuxStd == 1 )	// MVC
		{
			u32FrameRateRes *= 2;
		}
	}
	else if( u32StreamFormat == RV_DEC )
	{
		u32FrameRateRes = 0;
		u32FrameRateDiv = 0;
	}
	else
	{
		VDEC_KDRV_Message(ERROR, " Not supported stream format %d", u32StreamFormat);
		u32FrameRateRes=1;
		u32FrameRateDiv=1;
		i32RetVal = NOT_OK;
	}

	// DE constraint(HD 60p is not available)
	if( (pstPicInfo->u32PicWidth >= 1920 && pstPicInfo->u32PicHeight >= 1080) &&
			pstPicInfo->u32ProgSeq &&
			(u32FrameRateDiv != 0 && (u32FrameRateRes/u32FrameRateDiv) > 30)   )
	{
		if( !((dbgFrameCnt-1) % 149) )
			VDEC_KDRV_Message(ERROR, "[MSVC C%d] Too high freq. for HD %d/%d",
					u32CoreNum, u32FrameRateRes, u32FrameRateDiv);

		u32FrameRateRes = u32FrameRateDiv*30;
	}

	if(u32FrameRateRes == 0){
		VDEC_KDRV_Message(DBG_VDC, " Zero residual %u", u32FrameRateRes);
	}
	if(u32FrameRateDiv == 0){
		VDEC_KDRV_Message(DBG_VDC, " Zero divider %u", u32FrameRateDiv);
	}

	pstPicInfo->u32FrameRateDiv = u32FrameRateDiv;
	pstPicInfo->u32FrameRateRes = u32FrameRateRes;

	VDEC_KDRV_Message(DBG_VDC, "[MSVC] Frame Rate %d/%d fixed? %d form%d %d %d %d %d\n",
		u32FrameRateRes, u32FrameRateDiv,
		pstInstInfo->u8FixedFrameRate, u32StreamFormat, pstInstInfo->u32SeqAspClass, pstInstInfo->u32RunAuxStd,
		RET_DEC_PIC_VOP_TIME_INCR(u32CoreNum), RET_DEC_PIC_MODULO_TIME_BASE(u32CoreNum)
		);

	return i32RetVal;
}

SINT32 MSVC_HAL_GetPicInfo(UINT32 u32CoreNum, UINT32 u32InstNum, PIC_INFO_T *pstPicInfo)
{
	SINT32 s32RetVal = 0;
	UINT32 u32StreamFormat, u32PicTypeInfo, u32PicStruct;
	UINT32 u32CurCodecLevel=0;
	UINT32 u32PicStructPresentFlag;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];
	u32StreamFormat = pstInstInfo->u32BitStreamFormat;

#if 0
	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
		return FALSE;
	}
#endif

	pstPicInfo->u32DecodingSuccess = RET_DEC_PIC_SUCCESS(u32CoreNum) & 0x01;
	pstPicInfo->u32DecResult = RET_DEC_PIC_SUCCESS(u32CoreNum);
	// ref. list clear noti bit
	pstPicInfo->u32DecResult |= RET_DEC_PIC_HRD_INFO(u32CoreNum)&(1<<31);
	pstPicInfo->u32NumOfErrMBs = RET_DEC_PIC_ERR_MB(u32CoreNum);
	pstPicInfo->u32IndexFrameDecoded= RET_DEC_PIC_CUR_IDX(u32CoreNum);
	pstPicInfo->u32IndexFrameDisplay= RET_DEC_PIC_FRAME_IDX(u32CoreNum);
	pstPicInfo->u32PicStruct = RET_DEC_PIC_PICSTRUCT(u32CoreNum);
	pstPicInfo->u32AspectRatio = RET_DEC_PIC_ASPECT_RATIO(u32CoreNum);
	pstPicInfo->u32PicWidth = RET_DEC_PIC_SRC_SIZE(u32CoreNum) >> 16;
	pstPicInfo->u32PicHeight = RET_DEC_PIC_SRC_SIZE(u32CoreNum) & 0xFFFF;
//	pstPicInfo->u8DispSkipFlag = FALSE;
	pstPicInfo->i32FramePackArrange = -2;
	pstPicInfo->u16SarW = 1;
	pstPicInfo->u16SarH = 1;
	pstPicInfo->u8TopFieldType = 0xFF;
	pstPicInfo->u8BotFieldType = 0xFF;
	pstPicInfo->u8LR3DInfo = 0;
	if( (u32StreamFormat == AVC_DEC) && (pstInstInfo->u32RunAuxStd == 0) )
	{
		pstPicInfo->u32MinFrameBufferCount = RET_DEC_PIC_MIN_FRAME(u32CoreNum) & 0xFFFF;
		u32CurCodecLevel = RET_DEC_PIC_MIN_FRAME(u32CoreNum) >> 16;
	}
	else
	{
		pstPicInfo->u32MinFrameBufferCount = pstInstInfo->u32MinFrameBufferCount;
		u32CurCodecLevel = 0;
	}

	_msvc_set_min_frame_num(u32StreamFormat, u32CurCodecLevel,
			pstPicInfo->u32PicWidth, pstPicInfo->u32PicHeight,
			&pstPicInfo->u32MinFrameBufferCount, NULL);

	if( u32StreamFormat == MP2_DEC )
		pstPicInfo->u8LowDelay = ((RET_DEC_PIC_HRD_INFO(u32CoreNum))>>11)&1;
	else
		pstPicInfo->u8LowDelay = 0;

	// 3D Info & Crop Info for AVC
	if( u32StreamFormat == AVC_DEC )
	{
		UINT32 u32FrmPackType;

		u32FrmPackType = RET_DEC_PIC_FRAME_PACKING(u32CoreNum);
		pstPicInfo->i32FramePackArrange = u32FrmPackType;

		if(pstPicInfo->i32FramePackArrange != -1 && pstPicInfo->i32FramePackArrange != -2 )
			pstPicInfo->i32FramePackArrange &= 0x7;

		if(pstPicInfo->i32FramePackArrange < -2 || pstPicInfo->i32FramePackArrange > 5 )
		{
			VDEC_KDRV_Message(DBG_VDC, "[MSVC] Packing arrangement value error %d", pstPicInfo->i32FramePackArrange);
			pstPicInfo->i32FramePackArrange = -1;
		}

		if(pstPicInfo->i32FramePackArrange == -2)
			pstPicInfo->i32FramePackArrange = pstInstInfo->i16PreFramePackArrange;
		else
			pstInstInfo->i16PreFramePackArrange = pstPicInfo->i32FramePackArrange;

		if(pstPicInfo->i32FramePackArrange >= 0)
		{
			// For 3D stream
			pstPicInfo->u32CropLeftOffset = 0;
			pstPicInfo->u32CropRightOffset = 0;
			pstPicInfo->u32CropTopOffset = 0;
			pstPicInfo->u32CropBottomOffset = 0;

			if( pstPicInfo->i32FramePackArrange == 5 ) // Frame Sequential
			{
				if( ((u32FrmPackType>>3) & 0x3) == 0 )
					pstPicInfo->u8LR3DInfo = MSVC_3D_UNSPEC_FRM; //Unspecified
				else if( ((u32FrmPackType>>5)+1) == ((u32FrmPackType >> 3) & 0x3) )
					pstPicInfo->u8LR3DInfo = MSVC_3D_RIGHT_FRM; //RightFrame
				else
					pstPicInfo->u8LR3DInfo = MSVC_3D_LEFT_FRM; //LeftFrame
			}
		}
		else
		{
			// For 2D stream
			pstPicInfo->u32CropLeftOffset = RET_DEC_PIC_HOFFSET(u32CoreNum) >> 16;
			pstPicInfo->u32CropRightOffset = RET_DEC_PIC_HOFFSET(u32CoreNum) & 0xFFFF;
			pstPicInfo->u32CropTopOffset = RET_DEC_PIC_VOFFSET(u32CoreNum) >> 16;
			pstPicInfo->u32CropBottomOffset = RET_DEC_PIC_VOFFSET(u32CoreNum) & 0xFFFF;
		}

		if( pstInstInfo->u32RunAuxStd == 1 ) // MVC
		{
			UINT32 u32MvcReport;

			pstPicInfo->i32FramePackArrange = 5; // Frame Sequential

			u32MvcReport = RET_DEC_PIC_MVC_REPORT(u32CoreNum);	// ViewIdxOfDecIDx[1], ViewIdxOfDispIdx[0]
			if( (u32MvcReport != 0x0) && (u32MvcReport != 0x3) )
			{
				VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Wrn] MVC Report:0x%x, %s", u32CoreNum, u32InstNum, u32MvcReport, __FUNCTION__ );
			}

			u32MvcReport &= 0x1;	// Check ViewIdxOfDispIdx[0]
			if( u32MvcReport == 0x0 )
			{
				pstPicInfo->u8LR3DInfo = MSVC_3D_LEFT_FRM; //Base View
			}
			else if( u32MvcReport == 0x1 )
			{
				pstPicInfo->u8LR3DInfo = MSVC_3D_RIGHT_FRM; //Second View
			}
		}
	}
	else
	{
		pstPicInfo->u32CropLeftOffset = 0;
		pstPicInfo->u32CropRightOffset = 0;
		pstPicInfo->u32CropTopOffset = 0;
		pstPicInfo->u32CropBottomOffset = 0;
	}

	pstPicInfo->u32ActiveFMT = RET_DEC_PIC_ACTIVE_FMT(u32CoreNum) & 0xF;
	pstPicInfo->u32DecodedFrameNum = RET_DEC_PIC_FRAME_NUM(u32CoreNum);
	pstPicInfo->u32FrameRateInfo = RET_DEC_PIC_FRAME_RATE(u32CoreNum);
	pstPicInfo->u32TimeScale = RET_DEC_PIC_TIME_SCALE(u32CoreNum);
	pstPicInfo->u32NumUnitTick = RET_DEC_PIC_NUM_UNITS_IN_TICK(u32CoreNum);

	// Frame, Top(234) : IPB
	// Bottom(567) : IPB
	u32PicTypeInfo = RET_DEC_PIC_TYPE(u32CoreNum);
	pstPicInfo->u32PicType = u32PicTypeInfo & 0x03;

	pstInstInfo->u32FrameMbsOnlyFlag = (u32PicTypeInfo>>30) & 0x1;

	if( u32StreamFormat == MP2_DEC || u32StreamFormat == AVS_DEC || u32StreamFormat == RV_DEC ||
			u32StreamFormat == VC1_DEC || u32StreamFormat == MP4_DIVX3_DEC)
	{

		if( u32StreamFormat == MP2_DEC || u32StreamFormat == AVS_DEC )
		{
			pstPicInfo->u32ProgSeq           	= (u32PicTypeInfo>>29) & 0x1 ;//in seq_header
			pstPicInfo->u32ProgFrame         	= (u32PicTypeInfo>>23) & 0x3 ;
			pstPicInfo->u32TopFieldFirst      	= (u32PicTypeInfo>>21) & 0x1 ;
			pstPicInfo->u32RepeatFirstField   	= (u32PicTypeInfo>>22) & 0x1 ;
			pstPicInfo->u32PictureStructure   	= (u32PicTypeInfo>>19) & 0x3 ;
		}
		else if( u32StreamFormat == MP4_DIVX3_DEC )
		{
			pstPicInfo->u32TopFieldFirst		= (u32PicTypeInfo>>21) & 0x1;

			if( (u32PicTypeInfo>>30) & 0x1 )
			{
				pstPicInfo->u32ProgSeq		= 0x0;
				pstPicInfo->u32ProgFrame	= 0x1;

				if(pstPicInfo->u32TopFieldFirst)
					pstPicInfo->u32PictureStructure	= 0x2;
				else
					pstPicInfo->u32PictureStructure = 0x1;
			}
			else
			{
				pstPicInfo->u32ProgSeq		= 0x1;
				pstPicInfo->u32ProgFrame	= 0x1;
				pstPicInfo->u32PictureStructure	= 0x3;
			}

			pstPicInfo->u32RepeatFirstField	= 0x0;
			pstPicInfo->u32TopFieldFirst	= (u32PicTypeInfo>>21) & 0x1;

			if( pstInstInfo->u8FixedFrameRate == 0 )
			{
				pstPicInfo->u32TimeIncr = RET_DEC_PIC_VOP_TIME_INCR(u32CoreNum);
				pstPicInfo->u32ModTimeBase = RET_DEC_PIC_MODULO_TIME_BASE(u32CoreNum);
			}

		}
		else if( u32StreamFormat == RV_DEC )
		{
			pstPicInfo->u32ProgSeq           	= 0x1;
			pstPicInfo->u32ProgFrame         	= 0x1;
			pstPicInfo->u32TopFieldFirst      	= 0x0;
			pstPicInfo->u32RepeatFirstField   	= 0x1;
			pstPicInfo->u32PictureStructure   	= 0x3;
		}
		else if( u32StreamFormat == VC1_DEC )
		{
			pstPicInfo->u32ProgFrame         	= (u32PicTypeInfo>>23) & 0x3;

			if( pstPicInfo->u32ProgFrame )
				pstPicInfo->u32ProgSeq           	= 0x1;
			else
				pstPicInfo->u32ProgSeq           	= 0x0;

			pstPicInfo->u32RepeatFirstField   	= (u32PicTypeInfo>>22) & 0x1;
			pstPicInfo->u32TopFieldFirst      	= (u32PicTypeInfo>>21) & 0x1;
			pstPicInfo->u32PictureStructure   	= (u32PicTypeInfo>>19) & 0x3;

			if( pstPicInfo->u32PictureStructure == 0x0 )
				pstPicInfo->u32PictureStructure = 0x3;
		}
	}
	else if(u32StreamFormat == AVC_DEC)
	{
		u32PicStruct = pstPicInfo->u32PicStruct & 0xF;  // 0x31FC
		u32PicStructPresentFlag = (pstPicInfo->u32PicStruct>>16) & 0x1;

		if ( u32PicStruct >= 9 && pstInstInfo->u32PrePicStruct < 9 )
			u32PicStruct = pstInstInfo->u32PrePicStruct;

		if( u32PicStruct >= 9 )
		{
			// No Picture Structure
			pstPicInfo->u32ProgSeq           	= (u32PicTypeInfo>>29) & 0x1 ;//in seq_header
			pstPicInfo->u32ProgFrame         	= ((u32PicTypeInfo>>18) & 0x1) == 0x1 ? 0:1 ;
			if( pstPicInfo->u32ProgSeq )
			{
				pstPicInfo->u32TopFieldFirst      	= 0;
				pstPicInfo->u32RepeatFirstField   	= 0;
				pstPicInfo->u32PictureStructure   	= 0x3;	// Frame Pic
				pstPicInfo->u32NumClockTS    	   	= 1;
				pstPicInfo->u32DeltaTfiDivisor     	= 2;
			}
			else
			{
				if( pstPicInfo->u32ProgFrame )
					pstPicInfo->u32TopFieldFirst 	= 1;
				else
					pstPicInfo->u32TopFieldFirst	= ((u32PicTypeInfo>>21) & 0x1);

				pstPicInfo->u32RepeatFirstField   	= 0;
				if(pstPicInfo->u32TopFieldFirst)
					pstPicInfo->u32PictureStructure = 0x2;	// Bottom Field Last
				else
					pstPicInfo->u32PictureStructure = 0x1;	// Top Field Last

				pstPicInfo->u32NumClockTS    	   	= 2;
				pstPicInfo->u32DeltaTfiDivisor     	= 2;
			}
			s32RetVal = NOT_OK;
		}
		else
		{
			if( (u32PicStructPresentFlag == 0) &&
				(pstInstInfo->u32FrameMbsOnlyFlag == 0x0) &&
				(u32PicStruct == 0) )
			{
				VDEC_KDRV_Message(WARNING, "[MSVC_HAL%d-%d][Wrn] Change Scan Type, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
				u32PicStruct = 3;
			}

			pstPicInfo->u32ProgSeq           	= stAVCPicStruct[u32PicStruct].ProgSeq;
			pstPicInfo->u32ProgFrame         	= stAVCPicStruct[u32PicStruct].ProgFrame;
			pstPicInfo->u32TopFieldFirst      	= stAVCPicStruct[u32PicStruct].TopFieldFirst;
			pstPicInfo->u32RepeatFirstField   	= stAVCPicStruct[u32PicStruct].RepeatFirstField;
			pstPicInfo->u32PictureStructure   	= stAVCPicStruct[u32PicStruct].PictureStructure;
			pstPicInfo->u32NumClockTS    	   	= stAVCPicStruct[u32PicStruct].NumClockTS;
			pstPicInfo->u32DeltaTfiDivisor     	= stAVCPicStruct[u32PicStruct].DeltaTfiDivisor;
			pstInstInfo->u32PrePicStruct = u32PicStruct;
		}
	}

	// get aspect ratio from aspect ratio idc
	if( u32StreamFormat == AVC_DEC || u32StreamFormat == MP4_DIVX3_DEC )
	{
		pstPicInfo->u32AspectRatio = _msvc_GetAspectRatioForMP4(
				(UINT16)u32StreamFormat,
				pstPicInfo->u32PicWidth,
				pstPicInfo->u32PicHeight,
				pstPicInfo->u32AspectRatio,
				&pstPicInfo->u8AspectRatioIdc,
				&pstPicInfo->u16SarW,
				&pstPicInfo->u16SarH
				);
	}
	else if( u32StreamFormat == MP2_DEC )
	{
		pstPicInfo->u32AspectRatio = _msvc_GetAspectRatio(
				pstPicInfo->u32PicWidth,
				pstPicInfo->u32PicHeight,
				pstPicInfo->u32AspectRatio,
				&pstPicInfo->u8AspectRatioIdc,
				&pstPicInfo->u16SarW,
				&pstPicInfo->u16SarH
				);
	}

	// Field pic : pic type handling
	if( (u32StreamFormat == AVC_DEC || u32StreamFormat == MP2_DEC) &&
			( pstInstInfo->u32FieldPicFlag == TRUE) )
	{
		if( (u32PicTypeInfo >> 2)&1 )
			pstPicInfo->u8TopFieldType = 0;
		else if( (u32PicTypeInfo >> 3)&1 )
			pstPicInfo->u8TopFieldType = 1;
		else if( (u32PicTypeInfo >> 4)&1 )
			pstPicInfo->u8TopFieldType = 2;

		if( (u32PicTypeInfo >> 5)&1 )
			pstPicInfo->u8BotFieldType = 0;
		else if( (u32PicTypeInfo >> 6)&1 )
			pstPicInfo->u8BotFieldType = 1;
		else if( (u32PicTypeInfo >> 7)&1 )
			pstPicInfo->u8BotFieldType = 2;

		if( pstPicInfo->u8TopFieldType == 0 ||
				pstPicInfo->u8BotFieldType == 0 )
		{
			pstPicInfo->u32PicType = 0;	//mark as I-pic
		}
	}

	if( pstPicInfo->u32PicHeight == 720 && pstPicInfo->u32ProgSeq == 0 )
	{
		VDEC_KDRV_Message(DBG_VDC, "[MSVC C%dI%d] 720i -> 720p",u32CoreNum, u32InstNum);

		pstPicInfo->u32ProgSeq = 1;
		pstPicInfo->u32ProgFrame = 1;
	}

	if( pstPicInfo->u32ProgSeq == 1 && pstPicInfo->u32ProgFrame == 0 )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] prog seg %d & prg frame %d mismatch",
				u32CoreNum, u32InstNum, pstPicInfo->u32ProgSeq, pstPicInfo->u32ProgFrame);

		pstPicInfo->u32ProgFrame = 0x1;
	}

	if( pstPicInfo->u32ProgSeq == 1 && pstPicInfo->u32PictureStructure != 3 )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] prog frame %d & pic structure %d mismatch",
				u32CoreNum, u32InstNum, pstPicInfo->u32ProgSeq, pstPicInfo->u32PictureStructure);

		pstPicInfo->u32PictureStructure = 0x3;
	}
#if 0
	if(pstPicInfo->u32ProgFrame != 1)
	{
		// Check picture struct
		if( pstPicInfo->u32PictureStructure < 1 || pstPicInfo->u32PictureStructure > 3)
			pstPicInfo->u8DispSkipFlag = 1;
	}
#endif
	if( _Calculate_Framerate(pstInstInfo, pstPicInfo) != OK )
		s32RetVal = NOT_OK;

	return s32RetVal;
}

SINT32 MSVC_HAL_SetFrameDisplayFlag(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32DispFlag)
{
	SINT32 s32RetVal=0;
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

#if 0
	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
		return FALSE;
	}
#else
	if( _MSVC_HAL_BusyWait(u32CoreNum) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
	}
#endif
	switch(u32InstNum)
	{
		case 0 : BIT_FRM_DIS_FLG_0(u32CoreNum) = u32DispFlag;	break;
		case 1 : BIT_FRM_DIS_FLG_1(u32CoreNum) = u32DispFlag;	break;
		case 2 : BIT_FRM_DIS_FLG_2(u32CoreNum) = u32DispFlag;	break;
		case 3 : BIT_FRM_DIS_FLG_3(u32CoreNum) = u32DispFlag;	break;
		default :
			VDEC_KDRV_Message(ERROR,"%s Invalid core num %d",__FUNCTION__,  u32CoreNum);
			break;
	}
	//BIT_FRM_DIS_FLG_0(1);

	return s32RetVal;
}

UINT32 MSVC_HAL_GetFrameDisplayFlag(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	UINT32 u32Flag=0;

	switch(u32InstNum)
	{
		case 0 : u32Flag = BIT_FRM_DIS_FLG_0(u32CoreNum);	break;
		case 1 : u32Flag = BIT_FRM_DIS_FLG_1(u32CoreNum);	break;
		case 2 : u32Flag = BIT_FRM_DIS_FLG_2(u32CoreNum);	break;
		case 3 : u32Flag = BIT_FRM_DIS_FLG_3(u32CoreNum);	break;
		default :
			VDEC_KDRV_Message(ERROR,"%s Invalid core num %d",__FUNCTION__,  u32CoreNum);
			break;
	}

	return u32Flag;
}

UINT32 MSVC_HAL_GetFWVersion(UINT32 u32CoreNum, MSVC_FW_VERSION_T *pstVer)
{
	UINT32 u32Save;

	if( pstVer != NULL )
	{
		u32Save = RET_VER_NUM(u32CoreNum);
		BIT_RUN_COMMAND(u32CoreNum) = MSVC_GET_FW_VER;

		if( _MSVC_HAL_BusyWait(u32CoreNum) == FALSE )
		{
			VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d][Err] BusyWait, %s", u32CoreNum, __FUNCTION__ );
		}

		*((UINT32*)pstVer) = RET_VER_NUM(u32CoreNum);
		RET_VER_NUM(u32CoreNum) = u32Save;
	}

	return (BODA_FW_VERSION);
}

SINT32 MSVC_HAL_SetBitstreamWrPtr(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32WrPtr)
{
	switch (u32InstNum)
	{
		case 0: BIT_WR_PTR_0(u32CoreNum) = u32WrPtr; 	break;
		case 1: BIT_WR_PTR_1(u32CoreNum) = u32WrPtr;	break;
		case 2: BIT_WR_PTR_2(u32CoreNum) = u32WrPtr;	break;
		case 3: BIT_WR_PTR_3(u32CoreNum) = u32WrPtr;	break;
		default:
			break;
	}

	return OK;
}

UINT32 MSVC_HAL_GetBitstreamWrPtr(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	UINT32 u32WrPtr;

	switch (u32InstNum)
	{
		case 0: u32WrPtr = BIT_WR_PTR_0(u32CoreNum);	break;
		case 1: u32WrPtr = BIT_WR_PTR_1(u32CoreNum);	break;
		case 2: u32WrPtr = BIT_WR_PTR_2(u32CoreNum);	break;
		case 3: u32WrPtr = BIT_WR_PTR_3(u32CoreNum);	break;
		default:
			u32WrPtr = 0x0;
			break;
	}

	return u32WrPtr;
}

SINT32 MSVC_HAL_SetBitstreamRdPtr(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32RdPtr)
{
	switch (u32InstNum)
	{
		case 0: BIT_RD_PTR_0(u32CoreNum) = u32RdPtr; 	break;
		case 1: BIT_RD_PTR_1(u32CoreNum) = u32RdPtr;	break;
		case 2: BIT_RD_PTR_2(u32CoreNum) = u32RdPtr;	break;
		case 3: BIT_RD_PTR_3(u32CoreNum) = u32RdPtr;	break;
		default:
			break;
	}

	return OK;
}

UINT32 MSVC_HAL_GetBitstreamRdPtr(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	UINT32 u32RdPtr;

	switch (u32InstNum)
	{
		case 0: u32RdPtr = BIT_RD_PTR_0(u32CoreNum);	break;
		case 1: u32RdPtr = BIT_RD_PTR_1(u32CoreNum);	break;
		case 2: u32RdPtr = BIT_RD_PTR_2(u32CoreNum);	break;
		case 3: u32RdPtr = BIT_RD_PTR_3(u32CoreNum);	break;
		default:
			u32RdPtr = 0;
			break;
	}

	return u32RdPtr;
}

static UINT32 _msvc_hal_GetUserdataBuf(UINT32 u32CoreNum)
{
	UINT32 u32RetVal;
	UINT32 u32NextAddr;
	t_CoreInfo *pstCoreInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	u32RetVal = pstCoreInfo->u32UserBufCurAddr;
	u32NextAddr = pstCoreInfo->u32UserBufCurAddr;
	u32NextAddr += pstCoreInfo->u32UserBufOneSize;
	if( u32NextAddr >= pstCoreInfo->u32UserBufStartAddr + pstCoreInfo->u32UserBufSize )
		u32NextAddr = pstCoreInfo->u32UserBufStartAddr;

	pstCoreInfo->u32UserBufCurAddr  = u32NextAddr;

	VDEC_KDRV_Message(DBG_VDC, "[MSVC] Core%d New USRD Addr 0x%08X", u32CoreNum, u32RetVal);

	return u32RetVal;
}

UINT32 MSVC_HAL_GetUserdataBufInfo(UINT32 u32CoreNum, UINT32 *u32BuffAddr, UINT32 *u32BuffSize)
{
	*u32BuffAddr = CMD_DEC_PIC_USER_DATA_BASE_ADDR(u32CoreNum);
	*u32BuffSize = CMD_DEC_PIC_USER_DATA_BUF_SIZE(u32CoreNum);

	return 0;
}

BOOLEAN MSVC_HAL_PicRun(UINT32 u32CoreNum, UINT32 u32InstNum, DEC_PARAM_T *pstDecParam)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;
	PIC_ROTATION_INFO_T *pstRotInfo;
	SEQ_OPTION_T *pstSeqOption;
	UINT32	u32PicOption;

	dbgFrameCnt++;
	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	pstRotInfo = &pstInstInfo->stRotInfo;
	pstSeqOption = (SEQ_OPTION_T*)&pstInstInfo->u32SeqOption;

#if 0
	if( MSVC_HAL_IsBusy(u32CoreNum) )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
		return FALSE;
	}
#else
	if( _MSVC_HAL_BusyWait(u32CoreNum) == FALSE )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC_HAL%d-%d][Err] Busy, %s", u32CoreNum, u32InstNum, __FUNCTION__ );
	}
#endif

	if( pstDecParam->bPackedMode == FALSE )
	{
		if( pstDecParam->bEOF == TRUE )
			BIT_DEC_FUN_CTRL(u32CoreNum) |= 1<<(2+u32InstNum);
		else
			BIT_DEC_FUN_CTRL(u32CoreNum) &= ~(1<<(2+u32InstNum));
		CMD_DEC_PIC_ROT_MODE(u32CoreNum) = 0;//pstRotInfo->u32RotMode;
		CMD_DEC_PIC_ROT_INDEX(u32CoreNum) = 0;//pstRotInfo->u32RotIndex;
		CMD_DEC_PIC_ROT_ADDR_Y(u32CoreNum) = 0;//pstRotInfo->u32RotAddrY;
		CMD_DEC_PIC_ROT_ADDR_CB(u32CoreNum) = 0;//pstRotInfo->u32RotAddrCB;
		CMD_DEC_PIC_ROT_ADDR_CR(u32CoreNum) = 0;//pstRotInfo->u32RotAddrCR;
		CMD_DEC_PIC_ROT_STRIDE(u32CoreNum) = 0;//pstRotInfo->u32RotStride;

		/*
		typedef struct {
			UINT32
				DecPreScanEn	: 1,	// Check if whole one frame in buffer
				DecPreScanMode	: 1,	// 0: pre scan & dec    1: pre-scan & no dec
				IframeSearchEn	: 1,	// If enalbed, DecPreScanEn/DecPreScanMode/SkipFrameMode ignored
				SkipFrameMode	: 2,	// 0: no skip  1: B skip  2: unconditionally skips one picture
				picUserDataEn	: 1,
				mvReportEnable	: 1,	// Enables motion vdector reporting
				mbParamEnable	: 1,	// Enables slice info reporting
				frmBufStaEnable	: 1,	// Enables reporting on frame buffer status info
				DbkOffsetEnable : 1,	// Set MP4 deblock filter offsets
				UserDataReportMode	: 1,	// 0: interrupt mode  1:interrupt diable mode
				_reserved		: 5,
				RvDbkMode		: 2;	// Sets a de-blocking filter mode for RV streams
		} PIC_OPTION_T;
		*/
		u32PicOption = 0;
		if( pstDecParam->sPicOption.bIframeSearchEn == TRUE )
			u32PicOption |= (1 << 2);
		u32PicOption |= ((pstDecParam->sPicOption.eSkipFrameMode & 0x3) << 3);
		if( pstDecParam->sPicOption.bPicUserDataEn == TRUE )
			u32PicOption |= (1 << 5);
		if( pstDecParam->sPicOption.bPicUserMode == TRUE )
			u32PicOption |= (1 << 10);
		if( pstDecParam->sPicOption.bSrcSkipB == TRUE )
			u32PicOption |= (1 << 11);

		CMD_DEC_PIC_OPTION(u32CoreNum) = u32PicOption;
		CMD_DEC_PIC_SKIP_NUM(u32CoreNum) = 1;

		// For dynamic buffer
		if( pstDecParam->u32ChunkSize ) // Ring Buffer : 0
		{
			if( pstDecParam->u32ChunkSize < 1024 )
				CMD_DEC_PIC_CHUNK_SIZE(u32CoreNum) = 1024;
			else
				CMD_DEC_PIC_CHUNK_SIZE(u32CoreNum) = pstDecParam->u32ChunkSize;

			CMD_DEC_PIC_BB_START(u32CoreNum) = pstDecParam->u32AuAddr & (~0x7);
			CMD_DEC_PIC_START_BYTE(u32CoreNum) = pstDecParam->u32AuAddr & 0x7;
		}

		// If user data is enabled
//		if( pstDecParam->sPicOption.bPicUserDataEn == TRUE )
		{
			CMD_DEC_PIC_USER_DATA_BASE_ADDR(u32CoreNum) = _msvc_hal_GetUserdataBuf(u32CoreNum);
			CMD_DEC_PIC_USER_DATA_BUF_SIZE(u32CoreNum) = pstCoreInfo->u32UserBufOneSize;
		}

		BIT_RUN_AUX_STD(u32CoreNum) = pstInstInfo->u32RunAuxStd;
		BIT_RUN_INDEX(u32CoreNum) = u32InstNum;
		BIT_RUN_COD_STD(u32CoreNum) = pstInstInfo->u32BitStreamFormat;
	}

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_PIC_RUN;

	return TRUE;
}

SINT32 MSVC_HAL_PicRunPack(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	if( BIT_RUN_INDEX(u32CoreNum) != u32InstNum )
	{
		VDEC_KDRV_Message(ERROR, "[MSVC C%dI%d] Pack run error CurInst %d", u32CoreNum, u32InstNum,
				BIT_RUN_INDEX(u32CoreNum) );
		return NOT_OK;
	}

	BIT_RUN_COMMAND(u32CoreNum) = MSVC_PIC_RUN;

	return OK;
}

SINT32 MSVC_HAL_SetEoS(UINT32 u32CoreNum, UINT32 u32InstNum)
{
	VDEC_KDRV_Message(DBG_SYS, "[MSVC C%dI%d] %s", u32CoreNum, u32InstNum, __FUNCTION__ );

	BIT_BIT_STREAM_PARAM(u32CoreNum) |= (0x1 << (2 + u32InstNum));

	return OK;
}

SINT32 MSVC_HAL_SetRun(UINT32 u32CoreNum, UINT32 u32Run)
{
	BIT_CODE_RUN(u32CoreNum) = u32Run?0x1:0x0;

	return OK;
}

UINT32 MSVC_HAL_GetCurInstance(UINT32 u32CoreNum)
{
	return BIT_RUN_INDEX(u32CoreNum);
}

UINT32 MSVC_HAL_GetCurCommand(UINT32 u32CoreNum)
{
	return BIT_RUN_COMMAND(u32CoreNum);
}

UINT32 MSVC_HAL_GetRun(UINT32 u32CoreNum)
{
	return BIT_CODE_RUN(u32CoreNum);
}

void MSVC_HAL_SetFieldPicFlag(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 val)
{
	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	pstInstInfo->u32FieldPicFlag = val;
}

UINT32 MSVC_HAL_SetIntReason(UINT32 u32CoreNum, UINT32 u32Val)
{
	BIT_INT_REASON(u32CoreNum) = u32Val;

	return OK;
}

UINT32 MSVC_HAL_GetIntReason(UINT32 u32CoreNum)
{
	return BIT_INT_REASON(u32CoreNum);
}

void MSVC_HAL_ClearInt(UINT32 u32CoreNum, UINT32 ui32val)
{
	BIT_INT_CLEAR(u32CoreNum) = ui32val;
}

SINT32 MSVC_HAL_RegisterFrameBuffer(UINT32 u32CoreNum,	UINT32 u32InstNum, FRAME_BUF_INFO_T *pstFrameBufInfo)
{
	SINT32 s32RetVal = OK;
	UINT32 u32Ctr;
	UINT32 u32FrameNo = 0;
	t_ParaBuf *pstrParaBuff = NULL;
	UINT32 *pu32BaseAddr = NULL;

	UINT32 u32ParaBufAddr, u32BaseBufAddr;
#if 0
	UINT32 u32ParaBufSize;
#endif
	UINT32 u32ParaBufVirtualAddr;

	t_CoreInfo *pstCoreInfo;
	t_InstanceInfo *pstInstInfo;
	//PIC_ROTATION_INFO_T *pstRotInfo;

	_printf4("<--VDEC_RegisterFrameBuffer\n");

	pstCoreInfo = MSVC_HAL_GetCoreInfo(u32CoreNum);
	pstInstInfo = &pstCoreInfo->stInstInfo[u32InstNum];

	u32FrameNo = pstFrameBufInfo->u32FrameNum;

	//u32ParaBufVirtualAddr = (UINT32 )ParaBufVirAddr;
	u32ParaBufAddr = BIT_PARA_BUF_ADDR(u32CoreNum);
#ifdef __XTENSA__
	// get PARA Buf address for MCU side & PARA Buf cache bypass
	u32ParaBufVirtualAddr = (UINT32)VDEC_TranselateVirualAddr(u32ParaBufAddr, PARA_BUF_SIZE);
#else	// kernel panic when use ioremap ???
	//u32ParaBufVirtualAddr = (UINT32)ioremap(u32ParaBufAddr, PARA_BUF_SIZE);
	u32BaseBufAddr = BIT_CODE_BUF_ADDR(u32CoreNum);
	u32ParaBufVirtualAddr = (UINT32)gpVdecBaseVirAddr[u32CoreNum]+(u32ParaBufAddr - u32BaseBufAddr);
#endif
	// Make frame buffer count multiple of 2 so that we wont loose any parameter
	u32FrameNo = (u32FrameNo+1)/2 <<1;

	pstrParaBuff = (t_ParaBuf *)(u32ParaBufVirtualAddr + 512*u32InstNum);

	// Let the decoder know the addresses of the frame buffers.
	for (u32Ctr = 0; u32Ctr < u32FrameNo/2; u32Ctr++) {
		pstrParaBuff->u32FrameAddrC0 = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)].u32AddrCb;
		_printf4("C%1x         0x%8x            0x%8x\n",(u32Ctr<<1), &pstrParaBuff->u32FrameAddrC0, pstrParaBuff->u32FrameAddrC0);
		pstrParaBuff->u32FrameAddrY0 = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)].u32AddrY;
		_printf4("Y%1x         0x%8x            0x%8x\n",(u32Ctr<<1), &pstrParaBuff->u32FrameAddrY0, pstrParaBuff->u32FrameAddrY0);

		pstrParaBuff->u32FrameAddrY1 = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)+1].u32AddrY;
		_printf4("Y%1x         0x%8x            0x%8x\n",(u32Ctr<<1+1), &pstrParaBuff->u32FrameAddrY1, pstrParaBuff->u32FrameAddrY1);
		pstrParaBuff->u32FrameAddrC1 = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)+1].u32AddrCb;
		_printf4("C%1x         0x%8x            0x%8x\n",(u32Ctr<<1+1), &pstrParaBuff->u32FrameAddrC1, pstrParaBuff->u32FrameAddrC1);

		pstrParaBuff++;
	}

	// Motion vectors are required in case of H.264. In parameter buffer motion vectors are starting at address (PARA_BUFFER_BASE + 0x180)
	// please refer vdec_drv.h for parameter buffer memory map

	// Get the base address of the parameter buffer, which further used for calculating the motion vector offset
	pu32BaseAddr =(UINT32 *) (u32ParaBufVirtualAddr+ 512*u32InstNum);
	for (u32Ctr = 0; u32Ctr < u32FrameNo/2; u32Ctr++){
		*(UINT32 *)(pu32BaseAddr + 96) = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)+1].u32MvColBuf;
		_printf4("Mov%1x       0x%8x            0x%8x\n",(u32Ctr<<1)+1, (pu32BaseAddr + 96), pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)+1].u32MvColBuf);
		pu32BaseAddr++;
		*(UINT32 *)(pu32BaseAddr + 96) = pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)].u32MvColBuf;;
		_printf4("Mov%1x       0x%8x            0x%8x\n",(u32Ctr<<1), (pu32BaseAddr + 96), pstFrameBufInfo->astFrameBuf[(u32Ctr<<1)].u32MvColBuf);
		pu32BaseAddr++;
	}
	_printf4("-->VDEC_RegisterFrameBuffer\n");
	return s32RetVal;
}

#ifndef	__XTENSA__
void MSVC_BootCodeDown(UINT8 nBodaIP)
{
	UINT32 ui32Count = 0, ui32AsmCnt;
	UINT16 ui16AsmData = 0;
	UINT32 *pVpuBaseVirAddr;

	UINT16 msvc_code[4];

	pVpuBaseVirAddr = gpVdecBaseVirAddr[nBodaIP];

	for (ui32Count = 0; ui32Count < 0x100; ui32Count+=2)
	{
		msvc_code[3] = pVpuBaseVirAddr[ui32Count]&0xFFFF;
		msvc_code[2] = (pVpuBaseVirAddr[ui32Count]&0xFFFF0000)>>16;
		msvc_code[1] = pVpuBaseVirAddr[ui32Count+1]&0xFFFF;
		msvc_code[0] = (pVpuBaseVirAddr[ui32Count+1]&0xFFFF0000)>>16;

		for( ui32AsmCnt = ui32Count*2; ui32AsmCnt < ui32Count*2+4; ui32AsmCnt++)
		{
			// Get BIT code data from the table
			ui16AsmData = msvc_code[ui32AsmCnt-ui32Count*2];
			// 28:16 Addr      		          15:0 Code Data
			BIT_CODE_DOWN(nBodaIP) =  (ui32AsmCnt  << 16) |ui16AsmData;
		}
	}

	return;
}

void MSVC_HAL_CodeDown(UINT8 nBodaIP, UINT32 ui32MSVCAddr, UINT32 ui32MSVCSize)
{
	UINT32 ui32Count;
	UINT32 *pVpuBaseVirAddr;

	VDEC_KDRV_Message(_TRACE, "[%d] MSVCAddr[0x%08x], Size 0x%x", nBodaIP, ui32MSVCAddr, ui32MSVCSize);
	gpVdecBaseVirAddr[nBodaIP] = (UINT32 *)VDEC_TranselateVirualAddr(ui32MSVCAddr, ui32MSVCSize);
	pVpuBaseVirAddr = gpVdecBaseVirAddr[nBodaIP];
	VDEC_KDRV_Message(_TRACE, "[%d] VDECFirmwareVirtualAddr[0x%08x] __ %d", nBodaIP, (int)pVpuBaseVirAddr, sizeof(bit_code));

	// 2. Copy BIT firmware to SDRAM
	for (ui32Count = 0; ui32Count < sizeof(bit_code)/sizeof(bit_code[0]); ui32Count += 4)
	{
		*pVpuBaseVirAddr = (bit_code[ui32Count + 2] << 16) | bit_code[ui32Count + 3];
		pVpuBaseVirAddr++;

		*pVpuBaseVirAddr = (bit_code[ui32Count + 0] << 16) | bit_code[ui32Count + 1];
		pVpuBaseVirAddr++;
	}

//#ifndef __XTENSA__
//	VDEC_ClearVirualAddr((void *)gpVdecBaseVirAddr[nBodaIP]);
//#endif
}
#endif

