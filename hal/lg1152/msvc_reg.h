/******************************************************************
	File	: VDECReg.h
			: register define for VDEC
	Author 	: tichung (tichung@lge.com 4699)
	Update 	: 03 Jul 2001 - register tag for HD2 1st set for test
*******************************************************************/

#ifndef __MSVC_REG_H__
#define __MSVC_REG_H__

extern volatile UINT32 MSVC_BASEReg;
//------------------------------------------------------------------------------
// REGISTER BASE
//------------------------------------------------------------------------------
#define BIT_BASE(i)			(MSVC_BASEReg + (i*0x200))

/////////////////////////////////////////////////////////////////////////////////////////
// MSVC0 Register
// BIT_BASE(0)			: 0x000
// MSVC0				: 0x000 - 0x1FC
// MSVC1				: 0x200 - 0x3FC
/////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
// HARDWARE REGISTER
//------------------------------------------------------------------------------
#define BIT_CODE_RUN(i)            			(*(volatile UINT32 *)(BIT_BASE(i) + 0x000))
#define BIT_CODE_DOWN(i)        			(*(volatile UINT32 *)(BIT_BASE(i) + 0x004))
#define BIT_INT_REQ(i)              		(*(volatile UINT32 *)(BIT_BASE(i) + 0x008))
#define BIT_INT_CLEAR(i)            		(*(volatile UINT32 *)(BIT_BASE(i) + 0x00C))
#define BIT_CODE_RESET(i)					(*(volatile UINT32 *)(BIT_BASE(i) + 0x014))
#define BIT_CUR_PC(i)                		(*(volatile UINT32 *)(BIT_BASE(i) + 0x018))

//------------------------------------------------------------------------------
// GLOBAL REGISTER
//------------------------------------------------------------------------------
#define BIT_CODE_BUF_ADDR(i)        	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x100))
#define BIT_WORK_BUF_ADDR(i)    		 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x104))
#define BIT_PARA_BUF_ADDR(i)         	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x108))
#define BIT_BIT_STREAM_CTRL(i)      	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x10C))
#define BIT_FRAME_MEM_CTRL(i)        	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x110))
#define CMD_DEC_DISPLAY_REORDER(i)   		(*(volatile UINT32 *)(BIT_BASE(i) + 0x114))
#define BIT_BIT_STREAM_PARAM(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x114))
#define BIT_DEC_FUN_CTRL(i)					(*(volatile UINT32 *)(BIT_BASE(i) + 0x114))

#define BIT_RD_PTR_0(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x120))
#define BIT_WR_PTR_0(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x124))
#define BIT_RD_PTR_1(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x128))
#define BIT_WR_PTR_1(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x12C))
#define BIT_RD_PTR_2(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x130))
#define BIT_WR_PTR_2(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x134))
#define BIT_RD_PTR_3(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x138))
#define BIT_WR_PTR_3(i)          			(*(volatile UINT32 *)(BIT_BASE(i) + 0x13C))
#define BIT_AXI_SRAM_USE(i)  				(*(volatile UINT32 *)(BIT_BASE(i) + 0x140))

#define BIT_SEARCH_RAM_BASE_ADDR(i) 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x144))
#define BIT_SEARCH_RAM_SIZE(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x148))

#define	BIT_FRM_DIS_FLG(i)					(*(volatile UINT32 *)(BIT_BASE(i) + 0x150))
#define	BIT_FRM_DIS_FLG_0(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x150))
#define	BIT_FRM_DIS_FLG_1(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x154))
#define	BIT_FRM_DIS_FLG_2(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x158))
#define	BIT_FRM_DIS_FLG_3(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x15C))

#define BIT_BUSY_FLAG(i)           			(*(volatile UINT32 *)(BIT_BASE(i) + 0x160))
#define BIT_RUN_COMMAND(i)      			(*(volatile UINT32 *)(BIT_BASE(i) + 0x164))
#define BIT_RUN_INDEX(i)            		(*(volatile UINT32 *)(BIT_BASE(i) + 0x168))
#define BIT_RUN_COD_STD(i)      			(*(volatile UINT32 *)(BIT_BASE(i) + 0x16C))
#define BIT_INT_ENABLE(i)            		(*(volatile UINT32 *)(BIT_BASE(i) + 0x170))
#define BIT_INT_REASON(i)           		(*(volatile UINT32 *)(BIT_BASE(i) + 0x174))
#define BIT_RUN_AUX_STD(i)         			(*(volatile UINT32 *)(BIT_BASE(i) + 0x178))

#define BIT_CMD_0(i)                 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E0))
#define BIT_CMD_1(i)                 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))

#define BIT_MSG_0(i)            			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1F0))
#define BIT_MSG_1(i)                 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1F4))
#define BIT_MSG_2(i)               			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1F8))
#define BIT_MSG_3(i)                		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1FC))


//-----------------------------------------------------------------------------
// [DEC SEQ INIT] COMMAND
//-----------------------------------------------------------------------------
#define CMD_DEC_SEQ_BB_START(i)       		(*(volatile UINT32 *)(BIT_BASE(i) + 0x180))
#define CMD_DEC_SEQ_BB_SIZE(i)        		(*(volatile UINT32 *)(BIT_BASE(i) + 0x184))
#define CMD_DEC_SEQ_OPTION(i)         		(*(volatile UINT32 *)(BIT_BASE(i) + 0x188))
#define CMD_DEC_SEQ_SRC_SIZE(i)       		(*(volatile UINT32 *)(BIT_BASE(i) + 0x18C))
#define CMD_DEC_SEQ_START_BYTE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x190))
#define CMD_DEC_SEQ_USER_DATA_OPTION(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x194))
#define CMD_DEC_SEQ_PS_BB_START(i)    		(*(volatile UINT32 *)(BIT_BASE(i) + 0x194))
#define CMD_DEC_SEQ_PS_BB_SIZE(i)     		(*(volatile UINT32 *)(BIT_BASE(i) + 0x198))
#define CMD_DEC_SEQ_JPG_THUMB_EN(i)    		(*(volatile UINT32 *)(BIT_BASE(i) + 0x19C))
#define CMD_DEC_SEQ_ASP_CLASS(i)	    	(*(volatile UINT32 *)(BIT_BASE(i) + 0x19C))

#define CMD_DEC_SEQ_INIT_ESCAPE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x114))

#define RET_DEC_SEQ_ASPECT(i)        		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B0))
#define RET_DEC_SEQ_SUCCESS(i)         		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C0))
#define RET_DEC_SEQ_SRC_FMT(i)         		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C4))
#define RET_DEC_SEQ_SRC_SIZE(i)        		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C4))
#define RET_DEC_SEQ_SRC_F_RATE(i)     		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C8))
#define RET_DEC_SEQ_FRAME_NEED(i)      		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1CC))
#define RET_DEC_SEQ_FRAME_DELAY(i)     		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D0))
#define RET_DEC_SEQ_INFO(i)            		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D4))
#define RET_DEC_SEQ_CROP_LEFT_RIGHT(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D8))
#define RET_DEC_SEQ_CROP_TOP_BOTTOM(i) 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1DC))
#define RET_DEC_SEQ_NEXT_FRAME_NUM(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E0))
#define RET_DEC_SEQ_JPG_PARA(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))
#define RET_DEC_SEQ_JPG_THUMB_IND(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E8))

#define RET_DEC_SEQ_NUM_UNITS_IN_TICK(i) 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))
#define RET_DEC_SEQ_TIME_SCALE(i)		  	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E8))
#define RET_DEC_SEQ_HEADER_INFO(i)	  	  	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1EC))


//-----------------------------------------------------------------------------
// [DEC PIC RUN] COMMAND
//-----------------------------------------------------------------------------
#define CMD_DEC_PIC_ROT_MODE(i)        		(*(volatile UINT32 *)(BIT_BASE(i) + 0x180))
#define CMD_DEC_PIC_ROT_INDEX(i)	       	(*(volatile UINT32 *)(BIT_BASE(i) + 0x184))
#define CMD_DEC_PIC_ROT_ADDR_Y(i)       	(*(volatile UINT32 *)(BIT_BASE(i) + 0x188))
#define CMD_DEC_PIC_ROT_ADDR_CB(i)     		(*(volatile UINT32 *)(BIT_BASE(i) + 0x18C))
#define CMD_DEC_PIC_ROT_ADDR_CR(i)     		(*(volatile UINT32 *)(BIT_BASE(i) + 0x190))

#define CMD_DEC_PIC_DBK_ADDR_Y(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x190))
#define CMD_DEC_PIC_DBK_ADDR_CB(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x194))
#define CMD_DEC_PIC_DBK_ADDR_CR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x198))
#define CMD_DEC_PIC_ROT_STRIDE(i)       	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B8))//--0x190
#define CMD_DEC_PIC_OPTION(i)			 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x194))
#define CMD_DEC_PIC_SKIP_NUM(i)		 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x198))
#define CMD_DEC_PIC_CHUNK_SIZE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x19C))
#define CMD_DEC_PIC_BB_START(i)		 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A0))
#define CMD_DEC_PIC_START_BYTE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A4))

#define CMD_DEC_PIC_PARA_BASE_ADDR(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A8))
#define CMD_DEC_PIC_USER_DATA_BASE_ADDR(i)	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1AC))
#define CMD_DEC_PIC_USER_DATA_BUF_SIZE(i)	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B0))

#define RET_DEC_PIC_SRC_SIZE(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1BC))
#define RET_DEC_PIC_FRAME_NUM(i)        	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C0))
#define RET_DEC_PIC_FRAME_IDX(i)       		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C4))
#define RET_DEC_PIC_ERR_MB(i)          		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1C8))
#define RET_DEC_PIC_TYPE(i)             	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1CC))
#define RET_DEC_PIC_MVC_REPORT(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D0))
#define RET_DEC_PIC_OPTION(i)			 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D4))
#define RET_DEC_PIC_SUCCESS(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1D8))
#define RET_DEC_PIC_CUR_IDX(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1DC))
#define RET_DEC_PIC_MIN_FRAME(i)		 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A8))
//#define RET_DEC_PIC_ASPECT(i)		 		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1EC))

// For H.263
#define RET_DEC_PIC_TEMPORAL_REFERENCE(i)	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B8))

// For AVC
#define RET_DEC_PIC_NUM_UNITS_IN_TICK(i)	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B8))
#define RET_DEC_PIC_TIME_SCALE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1F0))

#define RET_DEC_PIC_HOFFSET(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E0))
#define RET_DEC_PIC_VOFFSET(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))
#define RET_DEC_PIC_HEADER_REPORT(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))
#define RET_DEC_PIC_ACTIVE_FMT(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E8))
#define RET_DEC_PIC_HRD_INFO(i)				(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E8))
#define RET_DEC_PIC_ASPECT_RATIO(i)      	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1EC))
#define RET_DEC_PIC_FRAME_RATE(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1F0))
#define RET_DEC_PIC_PICSTRUCT(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1FC))//1F8¿¡¼­ change

#define RET_DEC_PIC_MODULO_TIME_BASE(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E0))
#define RET_DEC_PIC_VOP_TIME_INCR(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1E4))

// ** 3D packing info **
// -2: No frame packing arrangement SEI
// -1: Frame packing arrangement cacel flag == 1
// 0~5 : Exist frame packing arrangement type
#define RET_DEC_PIC_FRAME_PACKING(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x1B4))

#define RET_DEC_PIC_EXACT_RDPTR(i)		(*(volatile UINT32 *)(BIT_BASE(i) + 0x144))

//-----------------------------------------------------------------------------
// [SET FRAME BUF] COMMAND
//-----------------------------------------------------------------------------
#define CMD_SET_FRAME_BUF_NUM(i)       	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x180))
#define CMD_SET_FRAME_BUF_STRIDE(i)    		(*(volatile UINT32 *)(BIT_BASE(i) + 0x184))
#define CMD_SET_FRAME_SLICE_BB_START(i)	 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x188))
#define CMD_SET_FRAME_SLICE_BB_SIZE(i) 	   	(*(volatile UINT32 *)(BIT_BASE(i) + 0x18C))
#define CMD_SET_FRAME_FRAME_DELAY(i)   	   	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A4))

#define CMD_AXI_SRAM_BIT_ADDR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x190))
#define CMD_AXI_SRAM_IPACDC_ADDR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x194))
#define CMD_AXI_SRAM_DBKY_ADDR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x198))
#define CMD_AXI_SRAM_DBKC_ADDR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x19C))
#define CMD_AXI_SRAM_OVL_ADDR(i)			(*(volatile UINT32 *)(BIT_BASE(i) + 0x1A0))

//-----------------------------------------------------------------------------
// [DEC_FLUSH] COMMAND
//-----------------------------------------------------------------------------
#define CMD_DEC_BUF_FLUSH_TYPE(i)  		   	(*(volatile UINT32 *)(BIT_BASE(i) + 0x180))
#define CMD_DEC_BUF_FLUSH_RDPTR(i) 	   	  	(*(volatile UINT32 *)(BIT_BASE(i) + 0x184))

//-----------------------------------------------------------------------------
// [DEC_PARA_SET] COMMAND
//-----------------------------------------------------------------------------
#define CMD_DEC_PARA_SET_TYPE(i)    	   	(*(volatile UINT32 *)(BIT_BASE(i) + 0x180))
#define CMD_DEC_PARA_SET_SIZE(i)      	  	(*(volatile UINT32 *)(BIT_BASE(i) + 0x184))


//-----------------------------------------------------------------------------
// [FIRMWARE VERSION] COMMAND
// [32:16] project number =>
// [16:0]  version => xxxx.xxxx.xxxxxxxx
//-----------------------------------------------------------------------------
#define RET_VER_NUM(i)					 	(*(volatile UINT32 *)(BIT_BASE(i) + 0x1c0))



// BIT stream control bits
#define BIT_STREAM_BUFF_BIG_ENDIAN					0x00000001
#define BIT_STREAM_BUFF_LITTLE_ENDIAN				0x00000000
#define BIT_STREAM_BUFF_64BIT_ENDIAN				0x00000000
#define BIT_STREAM_BUFF_32BIT_ENDIAN				0x00000002
#define BIT_STREAM_FULL_EMPTY_CHECK_DISABLE  		0x00000004
#define BIT_STREAM_FULL_EMPTY_CHECK_ENABLE  		0x00000000

// BIT frame control bits
#define BIT_FRAME_MEM_LITTLE_ENDIAN					0x00000000
#define BIT_FRAME_MEM_BIG_ENDIAN					0x00000001
#define BIT_FRAME_BUFF_64BIT_ENDIAN					0x00000000
#define BIT_FRAME_BUFF_32BIT_ENDIAN					0x00000002
#define BIT_FRAME_CBCR_SEPARATE_MEM					0x00000000
#define BIT_FRAME_CBCR_INTERLEAVE					0x00000004
#define BIT_FRAME_CBCR_SEPARATE_MEM1				0x00000000
#define BIT_FRAME_CBCR_INTERLEAVE1					0x00000008
#define BIT_FRAME_CBCR_SEPARATE_MEM2				0x00000000
#define BIT_FRAME_CBCR_INTERLEAVE2					0x00000010
#define BIT_FRAME_CBCR_SEPARATE_MEM3				0x00000000
#define BIT_FRAME_CBCR_INTERLEAVE3					0x00000020

// BIT SRAM control bits
#define BIT_SRAM_OVL_ENABLE							0x00000008
#define BIT_SRAM_OVL_DISABLE						0x00000000
#define BIT_SRAM_DBK_ENABLE							0x00000004
#define BIT_SRAM_DBK_DISABLE						0x00000000
#define BIT_SRAM_IP_ENABLE							0x00000002
#define BIT_SRAM_IP_DISABLE							0x00000000
#define BIT_SRAM_ENABLE								0x00000001
#define BIT_SRAM_DISABLE							0x00000000

// BIT Commands
#define SEQ_INIT									0x00000001
#define SEQ_END										0x00000002
#define PICURE_RUN									0x00000003
#define SET_FRAME_BUF								0x00000004
#define DEC_PARA_SET								0x00000007
#define DEC_BUF_FLUSH								0x00000008
#define GET_FW_VERSION								0x0000000F

// MSVC Interrupt Status
#define MSVC_RESERVED0  							0x00000001
#define MSVC_SEQ_INIT_CMD_DONE  					0x00000002
#define MSVC_SEQ_END_CMD_DONE  						0x00000004
#define MSVC_PIC_RUN_CMD_DONE  						0x00000008
#define MSVC_SET_FRAME_BUF_CMD_DONE  				0x00000010
#define MSVC_RESERVED1  							0x00000020
#define MSVC_RESERVED2  							0x00000040
#define MSVC_DEC_PARA_SET_CMD_DONE  				0x00000080
#define MSVC_DEC_BUF_FLUSH_CMD_DONE  				0x00000100
#define MSVC_USERDATA_OVERFLOW						0x00000200
#define MSVC_FIELD_PIC_DEC_DONE						0x00000400
#define MSVC_RESERVED4  							0x00000800
#define MSVC_RESERVED5  							0x00001000
#define MSVC_RESERVED6  							0x00002000
#define MSVC_CPB_EMPTY_STATE  						0x00004000
#define MSVC_RESERVED7  							0x00008000
#define MSVC_RESERVED8  							0x00010000
#define MSVC_RESERVED9  							0x00020000
#define MSVC_RESERVED10  							0x00040000
#define MSVC_RESERVED11  							0x00080000
#define MSVC_RESERVED12  							0x00100000
#define MSVC_RESERVED13  							0x00200000
#define MSVC_RESERVED14  							0x00400000
#define MSVC_RESERVED15  							0x00800000
#define MSVC_RESERVED16  							0x01000000
#define MSVC_RESERVED17  							0x02000000
#define MSVC_RESERVED18  							0x04000000
#define MSVC_RESERVED19  							0x08000000
#define MSVC_RESERVED20  							0x10000000
#define MSVC_RESERVED21  							0x20000000
#define MSVC_RESERVED22  							0x40000000
#define MSVC_RESERVED23  							0x80000000

#endif /* __MSVC_REG_H__ */

