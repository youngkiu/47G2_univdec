#ifndef __MSVC_HAL_H__
#define __MSVC_HAL_H__

#include "lg1152_vdec_base.h"
#include "msvc_reg.h"

#define BODA_MAX_FRAME_BUFF_NUM		32
#define BODA_BUSY_THRSHD		1000		// BODA busy threshold ms

#define EXTINT0         0
#define EXTINT1         1
#define EXTINT2         2
#define EXTINT0_MASK    0x1
#define EXTINT1_MASK    0x2
#define EXTINT2_MASK    0x4
// ===> need to move to OS HAL


//for MSVC command
#define MSVC_INIT									0x0
#define MSVC_SEQ_INIT								0x1
#define MSVC_SEQ_END								0x2
#define MSVC_PIC_RUN								0x3
#define MSVC_SET_FRAME								0x4
#define MSVC_DEC_PARA_SET							0x7
#define MSVC_DEC_FLUSH								0x8
#define MSVC_WAKE_UP								0xB
#define MSVC_SLEEP                                  0xA
#define MSVC_GET_FW_VER								0xF
#define MSVC_FLUSH_AND_SEQ_INIT						0x11
#define MSVC_FLUSH_AND_SEQ_END						0x12
#define MSVC_FLUSH_AND_PIC_RUN						0x13

#define CODE_BUF_SIZE								0x22000
#define PARA_BUF_SIZE								0x3000
#define PS_BUF_SIZE									0xA000
#define SLICE_BUF_SIZE								0x4000
#define WORK_BUF_SIZE								0x180000
#define USER_BUF_SIZE								0x30000
#define USER_BUF_ONE_SIZE							0x2000


// Following are the decoding formats supported by the Video decoder
enum {
	AVC_DEC 		= 0,							// Decoding format type AVC/H.264
	VC1_DEC 		= 1,							// Decoding format type VC1
	MP2_DEC 		= 2,							// Decoding format type MPEG2
	MP4_DIVX3_DEC 	= 3,							// Decoding format type MPEG4, DV3
	RV_DEC 			= 4,							// Decoding format type RV
	AVS_DEC   		= 5								// Decoding format type AVS
	//MJPG_DEC  		= 8								// Decoding format type MJPG
};

typedef enum
{
	VDEC_BROADCAST_MODE,
	VDEC_FILEPLAY_MODE
} VDEC_PLAY_MODE_E;

typedef enum
{
	MSVC_3D_UNSPEC_FRM = 0,
	MSVC_3D_RIGHT_FRM,
	MSVC_3D_LEFT_FRM
} MSVC_3D_FRM_SIDE;

typedef struct
{
	UINT32			u32FrameNum;
	struct
	{
		UINT32			u32AddrY;						// Frame buffer base address for Y
		UINT32			u32AddrCb;						// Frame buffer base address for Cb
		UINT32			u32AddrCr;						// Frame buffer base address for Cr(Not Using)
		UINT32			u32MvColBuf;					// Frame buffer base address for MV(In case of H.264/AVC)
 	} astFrameBuf[BODA_MAX_FRAME_BUFF_NUM];
} FRAME_BUF_INFO_T;

typedef struct ParaBuf{
	UINT32			u32FrameAddrC0;					// Frame0 C buffer base address
	UINT32			u32FrameAddrY0;					// Frame0 Y buffer base address
	UINT32			u32FrameAddrY1;					// Frame1 Y buffer base address
	UINT32			u32Unused;						// unused (as Cb Cr interleved)
	UINT32			u32Unused1;						// unused (as Cb Cr interleved)
	UINT32			u32FrameAddrC1;					// Frame1 C buffer base address
} t_ParaBuf;

#if 0
typedef struct
{
	UINT32	_rsvd;
	UINT16	bytes;
	UINT16	type;
} MSVC_USRD_SEG_HEAD_T;

typedef struct
{
	UINT32	_rsvd;

	UINT16	bytesTotal;
	UINT16	nSegment;

	MSVC_USRD_SEG_HEAD_T	seg[16];
} MSVC_USRD_HEAD_T;

#define MSVC_GET_16BIT(_u16)	(_u16)

#else
typedef struct
{
	UINT16	type;
	UINT16	bytes;

	UINT32	_rsvd;
} MSVC_USRD_SEG_HEAD_T;

typedef struct
{
	UINT16	nSegment;
	UINT16	bytesTotal;

	UINT32	_rsvd;

	MSVC_USRD_SEG_HEAD_T	seg[16];
} MSVC_USRD_HEAD_T;

#define MSVC_GET_16BIT(_u16)	( (((_u16) & 0x00FF) << 8) | (((_u16) & 0xFF00)>>8))

#endif


typedef struct {
	UINT32
		Mp4DbkEn			: 1, // Enables out-loop de-blocking of MP2/4
		ReorderEn			: 1, // Enable display buffer reordering in H.264 decode case
		FilePlayEn			: 1, // Enable file-play mode in decoder operation with frame based streaming
		DecDynBufAllocEn	: 1, // Enable dynamic picture stream buffer allocation in file play mode
		reserved			: 2,
		ArbitraryRdPtr		: 1; // SEQ_INIT from arbitrary position (Only MPEG2)
} SEQ_OPTION_T;

typedef struct {
	UINT32	u32IsSuccess;
	BOOLEAN	bOverSpec;
	UINT32	u32PicWidth;
	UINT32	u32PicHeight;
	UINT32	u32FrmCnt;
	UINT32	u32MinFrameBufferCount;
	UINT32	u32FrameBufDelay;
	UINT32	u32AspectRatio;
	UINT32	u32DecSeqInfo;
	UINT32	u32NextDecodedIdxNum;
	UINT32	u32SeqHeadInfo;
	UINT32	u32InterlacedSeq;

	UINT32	u32Profile;
	UINT32	u32Level;

	UINT32	u32FrameRateRes;
	UINT32	u32FrameRateDiv;

	UINT64	u64TimeStamp;
	UINT32	u32uID;

	// Sample aspect ratio (only for AVC)
	UINT8 u8AspectRatioIdc;
	UINT16 u16SarW;
	UINT16 u16SarH;

	UINT8 u8FixedFrameRate;

	UINT32 u32ProgSeq;
} SEQ_INFO_T;

typedef struct {
	UINT32 u32DecodingSuccess;
	UINT32 u32DecResult;
	UINT32 u32NumOfErrMBs;
	UINT32 u32IndexFrameDecoded;
	UINT32 u32IndexFrameDisplay;
	UINT32 u32PicType;
	UINT32 u32PicStruct;
	UINT32 u32AspectRatio;
	UINT32 u32PicWidth;
	UINT32 u32PicHeight;
	UINT32 u32CropLeftOffset;
	UINT32 u32CropRightOffset;
	UINT32 u32CropTopOffset;
	UINT32 u32CropBottomOffset;
	UINT32 u32ActiveFMT;
	UINT32 u32DecodedFrameNum;
	UINT32 u32FrameRateInfo;
	UINT32 u32TimeScale;
	UINT32 u32NumUnitTick;
	UINT32 u32MinFrameBufferCount;

	UINT32 u32ProgSeq;
	UINT32 u32ProgFrame;
	UINT32 u32TopFieldFirst;
	UINT32 u32RepeatFirstField;
	UINT32 u32PictureStructure;
	UINT32 u32NumClockTS;
	UINT32 u32DeltaTfiDivisor;
	SINT32 i32FramePackArrange;
	UINT8 u8LR3DInfo;		// only for frame seq 3D stream

	UINT32 u32FrameRateRes;
	UINT32 u32FrameRateDiv;
	UINT32 u32TimeIncr;
	UINT32 u32ModTimeBase;

//	UINT32 u32AuSize;

	// Sample aspect ratio (only for AVC)
	UINT8 u8AspectRatioIdc;
	UINT16 u16SarW;
	UINT16 u16SarH;
//	UINT8 u8DispSkipFlag;

	UINT8 u8TopFieldType;
	UINT8 u8BotFieldType;
	UINT8 u8LowDelay;
} PIC_INFO_T;


typedef struct {
	UINT32
		versionId               :16,	//  0: 15
		productId               :16;	//  16: 31
} MSVC_FW_VERSION_T;

typedef struct {
	SINT32 i32UserDataEn;
	SINT32 i32UserDataBufSize;
	UINT32 ui32UserDataBufAddr;
	SINT32 i32NumUsrData;
	SINT32 i32szUsrData;

	SINT32 i32UserDataFrameSize;
	UINT32 u32UserDataFrameFlag;
} UsrDataParam;

// While opening the decoder instance, user has to provide following parameters
// to the decoder open API
typedef struct tagDecOpenParam{
	UINT32 	u32BitStreamFormat;			// Bitstream/Encoded picture format (MPEG2/H.264 etc.)
	UINT32 	u32BitStreamBuffer;				// Encoded picture buffer start address
	UINT32 	u32BitStreamBufferSize;			// Encoded picture buffer size
	//UINT32 	u32Mp4DeblkEnable;			// Deblock enable/disable
	//UINT32 	u32ReorderEnable;				// Picture reorder enable/disable
	UINT32 	u32PicWidth;					// Encoded picture width (Optional)
	UINT32 	u32PicHeight;					// Encoded picture height (Optional)
	UINT32 	u32AuxStd;						// In case it is Divx
	UINT32	u32SeqAspClass;
	struct
	{
		BOOLEAN	bDeblockEnable;
		BOOLEAN	bReorderEnable;
		BOOLEAN	bFilePlayEnable;
		BOOLEAN	bDynamicBufEnable;
	} sSeqOption;

} SEQ_PARAM_T;

typedef enum
{
	DEC_PARAM_SKIP_MODE_NO = 0,
	DEC_PARAM_SKIP_MODE_PB = 1,
	DEC_PARAM_SKIP_MODE_B = 2,
	DEC_PARAM_SKIP_MODE_ANY = 3,
	DEC_PARAM_SKIP_MODE_32bits = 0x20120725,
} E_DEC_PARAM_SKIP_FRAME_MODE_T;

typedef struct {
	UINT32	u32ChunkSize;					// Ring Buffer : 0
	BOOLEAN	bPackedMode;
	SINT32	i32PicStartByteOffset;			// for not-aligned

	struct
	{
		BOOLEAN	bIframeSearchEn;
		E_DEC_PARAM_SKIP_FRAME_MODE_T	eSkipFrameMode;
		BOOLEAN	bPicUserDataEn;
		BOOLEAN	bPicUserMode;
		UsrDataParam stUserDataParam;
		BOOLEAN	bSrcSkipB;
	} sPicOption;

	UINT32 u32AuAddr;
	BOOLEAN	bEOF;
} DEC_PARAM_T;

typedef struct {
	UINT32 u32RotMode;
	UINT32 u32RotIndex;
	UINT32 u32RotAddrY;
	UINT32 u32RotAddrCB;
	UINT32 u32RotAddrCR;
	UINT32 u32RotStride;
} PIC_ROTATION_INFO_T;

typedef struct t_CoreInfo t_CoreInfo;

typedef struct t_InstaceInfo_ {
	t_CoreInfo *pstCoreInfo;
	VDEC_PLAY_MODE_E u32PlayMode;
	DEC_PARAM_T stDecParam;
	PIC_ROTATION_INFO_T stRotInfo;

	UINT32 u32SeqInitDone;
	UINT32 u32InstNum;
//	UINT32 u32CPB_StartAddr;
//	UINT32 u32CPB_Size;
	UINT32 u32SeqOption;
	UINT32 u32BitStreamFormat;
	UINT32 u32SeqAspClass;
	UINT32 u32RunAuxStd;
	UINT32 u32PicOption;
	UINT32 u32SkipNum;
	UINT32 u32ChunkSize;
	UINT32 u32CPB_Rptr;

	UINT32 u32Stride;
	UINT32 u32PrePicStruct;
	UINT32 u32FieldPicFlag;

	SINT16 i16PreFramePackArrange;
	UINT8 u8FixedFrameRate;

	UINT32 u32FrameMbsOnlyFlag;

	FRAME_BUF_INFO_T stFrameBufInfo;
	UINT32 u32MinFrameBufferCount;
} t_InstanceInfo;

struct t_CoreInfo {
	UINT32 u32CoreNum;
	UINT32 u32Ch;
	UINT32 u32CoreBaseAddr;
	UINT32 u32VirCoreBaseAddr;
	UINT32 u32CodeBufStartAddr;
	UINT32 u32CodeBufSize;
	UINT32 u32ParaBufStartAddr;
	UINT32 u32ParaBufSize;
	UINT32 u32PsBufStartAddr;
	UINT32 u32PsBufSize;
	UINT32 u32SliceBufStartAddr;
	UINT32 u32SliceBufSize;
	UINT32 u32WorkBufStartAddr;
	UINT32 u32WorkBufSize;
	UINT32 u32UserBufStartAddr;
	UINT32 u32UserBufSize;
	UINT32 u32UserBufCurAddr;
	UINT32 u32UserBufOneSize;

	t_InstanceInfo stInstInfo[MSVC_NUM_OF_CHANNEL];
};

void MSVC_Reg_Init(void);
SINT32 MSVC_HAL_Init(UINT32 u32CoreNum, UINT32 u32VdecBufBase, UINT32 u32VdecBufSize);

UINT32 MSVC_HAL_IsBusy(UINT32 u32CoreNum);
SINT32 MSVC_HAL_PicRunPack(UINT32 u32CoreNum, UINT32 u32InstNum);
UINT32 MSVC_HAL_SeqInit(UINT32 u32CoreNum, UINT32 u32InstNum, SEQ_PARAM_T *pstSeqParam);
BOOLEAN MSVC_HAL_PicRun(UINT32 u32CoreNum, UINT32 u32InstNum, DEC_PARAM_T *pstDecParam);
SINT32 MSVC_HAL_SetEoS(UINT32 u32CoreNum, UINT32 u32InstNum);
SINT32 MSVC_HAL_SetFrameDisplayFlag(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32DispFlag);
UINT32 MSVC_HAL_GetFrameDisplayFlag(UINT32 u32CoreNum, UINT32 u32InstNum);

UINT32 MSVC_HAL_GetFWVersion(UINT32 u32CoreNum, MSVC_FW_VERSION_T *pstVer);
SINT32 MSVC_HAL_SetBitstreamWrPtr(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32WrPtr);
UINT32 MSVC_HAL_GetBitstreamWrPtr(UINT32 u32CoreNum, UINT32 u32InstNum);
SINT32 MSVC_HAL_SetBitstreamRdPtr(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32RdPtr);
UINT32 MSVC_HAL_GetBitstreamRdPtr(UINT32 u32CoreNum, UINT32 u32InstNum);

SINT32 MSVC_HAL_RegisterFrameBuffer(UINT32 u32CoreNum, UINT32 u32InstNum,	FRAME_BUF_INFO_T *pstFrameBufInfo);
SINT32 MSVC_HAL_SetFrameBuffer(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32nFrame, UINT32 u32FrameDelay, UINT32 u32Stride);
BOOLEAN MSVC_HAL_Flush(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32FlushType, UINT32 u32ReadPtr);

UINT32 MSVC_HAL_GetUserdataBufInfo(UINT32 u32CoreNum, UINT32 *u32BuffAddr, UINT32 *u32BuffSize);
UINT32 MSVC_HAL_GetCurInstance(UINT32 u32CoreNum);
UINT32 MSVC_HAL_GetCurCommand(UINT32 u32CoreNum);
BOOLEAN MSVC_HAL_GetSeqInfo(UINT32 u32CoreNum, UINT32 u32InstNum, SEQ_INFO_T *pstSeqInfo);
SINT32 MSVC_HAL_GetPicInfo(UINT32 u32CoreNum, UINT32 u32InstNum, PIC_INFO_T *pstPicInfo);
SINT32 MSVC_HAL_SetStreamFormat(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 u32BitStreamFormat);
UINT32 MSVC_HAL_GetStreamFormat(UINT32 u32CoreNum, UINT32 u32InstNum);

UINT32 MSVC_HAL_GetRun(UINT32 u32CoreNum);
UINT32 MSVC_HAL_SetIntReason(UINT32 u32CoreNum, UINT32 u32Val);
UINT32 MSVC_HAL_GetIntReason(UINT32 u32CoreNum);

t_CoreInfo* MSVC_HAL_GetCoreInfo(UINT32 u32CoreNum);
void MSVC_HAL_ClearInt(UINT32 u32CoreNum, UINT32 ui32val);

void MSVC_HAL_SetFieldPicFlag(UINT32 u32CoreNum, UINT32 u32InstNum, UINT32 val);
SINT32 MSVC_HAL_SetRun(UINT32 u32CoreNum, UINT32 u32Run);
void MSVC_BootCodeDown(UINT8 nBodaIP);
void MSVC_HAL_CodeDown(UINT8 nBodaIP, UINT32 ui32MSVCAddr, UINT32 ui32MSVCSize);
#endif	// __MSVC_HAL_H__
