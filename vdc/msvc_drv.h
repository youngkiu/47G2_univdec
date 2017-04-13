#ifndef _MSVC_DRV_H_
#define _MSVC_DRV_H_


#include "../hal/lg1152/msvc_hal_api.h"
//#include "vdec_dpb.h"

//#define DPB_ID_USE

#define MSVC_PB_SKIP_THD		32
#define MSVC_PB_SKIP_THD_FILE	460
#define MSVC_MAX_DPB_FRAME	32

typedef enum
{
	MSVC_CH_NULL = 0x01,	// Before Init
	MSVC_CH_CLOSED,	// After Init & Before Open
	MSVC_CH_OPENED,	// After Open & Before Start
	MSVC_CH_SEQ_READY,	// After Start & Waiting SEQ HEADER
	MSVC_CH_SEQ_RUNNING,
	MSVC_CH_PIC_READY,	// After SEQ Init
	MSVC_CH_PIC_RUNNING,
	MSVC_CH_FIELD_DONE,
	MSVC_CH_FIELD_WAIT,	// wait second field
}MSVC_CH_STATE_N;

typedef enum
{
	MSVC_NO_SKIP = 0,
	MSVC_BP_SKIP,
	MSVC_B_SKIP,
	MSVC_ONEFRAME_SKIP
} MSVC_PICSCAN_MODE_T;

#define	SEQUENCE_HEADER 							1
#define	SEQUENCE_END 								2
#define	I_PICTURE	 								3       //TODO: check
#define	P_PICTURE 									4       //TODO: check
#define	NOSKIP_B_PICTURE							5       //TODO: check
#define	SKIP_B_PICTURE								6       //TODO: check

#if 0
typedef struct
{
	UINT8	b1FrameDec;
	UINT8	bDual3D;
	UINT8	srcMode;	// 0:live 1:file
	UINT8	noAdpStr;	// 0:could be adaptive 1:no adaptive
} MSVC_OPEN_MODE_T;
#endif

typedef struct
{
	UINT32 u32nSeg;
	UINT32 u32BuffAddr;
	UINT32 u32TotalLen;
	UINT32 u32Len;
} MSVC_USRD_INFO_T;

typedef enum
{
	TOP_FIELD_PIC = 0x1,
	BOTTOM_FIELD_PIC,
	FRAME_PIC
} PIC_STRUCT_N;

/*
typedef struct {
	UINT32
		auType			: 4,    //  0: 3
		resv			: 4,	//	4: 7
		flush			: 1,	//	8
		auAddr			: 23;	//	9:31
} CQ_ADDR_FIELD;

typedef union {
	UINT32
		dts				: 28;   //  0: 27
} CQ_DTS_FIELD;

typedef union {
	UINT32 size;
} CQ_SIZE_FIELD;

typedef struct {
	UINT32
		pts				: 28;   //  0: 27
} CQ_PTS_FIELD;

typedef struct {
	UINT32
		resv			: 9,	//	0: 8
		bufEndAddr		: 23;	//	9: 31
} CQ_END_FIELD;
*/
#if 0
typedef struct CQ_ENTRY_T_
{
	UINT32		auType;
	BOOLEAN		flush;
	UINT32		auAddr;
	UINT32		size;
	UINT32		auEnd;

	BOOLEAN		bRingBufferMode;
	BOOLEAN		bDecAgain;
} CQ_ENTRY_T;
#endif

typedef struct
{
	BOOLEAN bOpened;
	UINT32 u32Ch;
//	UINT32 u32ChBusy;
	UINT32 u32CoreNum;
	UINT32 u32InstNum;
	UINT32 u32CodecType;
	UINT32 u32DynamicBufEn;
	UINT8   b1FrameDec;
	UINT8   bDual3D;
//	UINT32 u32VCS_En;
//	UINT32 bSeqInit;
//	UINT32 bFieldDone;
//	FRAME_BUF_INFO_T stFrameBufInfo;
	SEQ_INFO_T stSeqInfo;
//	CQ_ENTRY_T stLastCmd;
	UINT32 u32FrameCnt;

//	UINT32 u32PicWidth;		// for DivX3
//	UINT32 u32PicHeight;	// for DivX3

//	UINT32 u32AuSize;

//	UINT32 u32CPB_StartAddr;
//	UINT32 u32CPB_VirAddr;
//	UINT32 u32CPB_Size;

	UINT32 u32PicOption;
	UINT32 u32SkipNum;

//	UINT32 u32PreRdPtr;
//	UINT32 u32PreAuAddr;
//	UINT8 ui8Vch;
//	UINT8 u8DqCh;
//	UINT8 u8PdecCh;
//	UINT8 u8DeCh;

//	UINT32 u32DecCount;
//	UINT32 u32ValidDecCount;
//	UINT32 u32ClearCount;

//	S_VDEC_DPB_INFO_T sVdecDpb;
//	UINT32 ui32MsvcMvColBufUsed;
//	UINT32 u32BodaRdEx;
//	UINT32 u32ErrorMBFilter;
//	UINT8 u8SkipModeOn;
//	UINT16 u16SkipPBCnt;

	UINT8 u8UserDataEn;
//	UINT8 u8CPBFlushReq;
//	UINT8 u8OnPicDrop;
//	UINT8 u8DpbLockReq;

//	SINT16 i16UnlockIndex;

//	UINT32 u32ExactRdPtr;
//	UINT8 au8DpbAge[MSVC_MAX_DPB_FRAME];
	UINT8 u8SrcMode;	// 0:live(freq I pic)  1:file(sparse I pic)

//	UINT8 u8CancelRA;

	// For variable frame rate(DivX, Xvid)
	SINT32 i32PreTempRefer;
	UINT32 u32CurFrameRateRes;
	UINT32 u32CurFrameRateDiv;
	SINT32 s32PreFrameRateDiv;

//	UINT32 u32PrePts;

//	UINT16 u16DpbIdCnt;
//	UINT16 u16NewDpbId;
//	UINT32 u32LockedFrameIdx;
	UINT8 u8NoAdaptiveStream;

//	UINT32 u32DqDpbMark;

	UINT32 u32NReDec;
} MSVC_CH_T;

SINT32 MSVC_Init(void);
//void MSVC_PrintStatus(UINT32 u32CoreNum);
UINT8 MSVC_Open(UINT8 u8MsvcCh, BOOLEAN b1FrameDec, UINT32 u32CodecType);
BOOLEAN MSVC_Flush(UINT8 u8MsvcCh);
//SINT32 MSVC_UpdateWrPtr(UINT32 ui32VdecCh, UINT32 u32WrPtr);
//void MSVC_SetWrPtr(UINT32 ui32VdecCh);
//void MSVC_Reset(UINT8 ui32VdecCh);
SINT32 MSVC_Close(UINT32 ui32VdecCh);
//SINT32 MSVC_SendCmd(UINT8 ui8VdecCh, CQ_ENTRY_T *pstCqEntry);

//SINT32 MSVC_SetErrorMBFrameFilter(UINT8 u8MsvcCh, UINT32 u32nErrorMb);
//SINT32 MSVC_CancelSeqInit(UINT32 ui32VdecCh);
//void MSVC_SeqInit(MSVC_CH_T *pstMsvcCh, CQ_ENTRY_T *pstCqEntry);
//void MSVC_PicRun(MSVC_CH_T* pstMsvcCh, CQ_ENTRY_T *pstCqEntry);

SINT32 MSVC_SetUserDataEn(UINT32 u32MsvcCh, UINT32 u32Enable);
#if 0
BOOLEAN MSVC_SetPicScanMode(UINT32 ui32VdecCh, UINT8 ui32ScanMode);
BOOLEAN MSVC_SetSrcScanType(UINT32 ui32VdecCh, UINT8 ui32ScanType);
#endif
BOOLEAN MSVC_GetUserDataInfo(UINT32 ui32VdecCh, UINT32 *pui32Address, UINT32 *pui32Size);

//int MSVC_GetUsedDPB(UINT8 ui32VdecCh);
//UINT32 MSVC_GetDecCount(MSVC_CH_T* pstMsvcCh);
UINT32 MSVC_GetFWVersion(void);

//void MSVC_SetRstReady(UINT32 u32CoreNum);
//void MSVC_ClearRstReady(UINT32 u32CoreNum);
//UINT8 MSVC_GetRstReady(UINT32 u32CoreNum);

MSVC_CH_T* MSVC_GetHandler(UINT32 u32Ch);
//CQ_ENTRY_T* MSVC_GetLastCmd(MSVC_CH_T *pstMsvcCh);

void MSVC_InterruptHandler(UINT32 u32CoreNum);
UINT32 MSVC_DECODER_ISR(UINT32 ui32ISRMask);

#endif //~_MSVC_DRV_H_

