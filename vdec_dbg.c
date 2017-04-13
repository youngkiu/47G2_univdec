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
 * main driver implementation for de device.
 * de device will teach you how to make device driver with new platform.
 *
 * author     seokjoo.lee (seokjoo.lee@lge.com)
 * version    1.0
 * date		2011.03.11
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "vdec_dbg.h"
#include "hal/lg1152/lg1152_vdec_base.h"
#include "vdec_ktop.h"
#include "ves/ves_cpb.h"
#include "vdc/msvc_adp.h"
#include "vdc/vdc_drv.h"
#include "mcu/vdec_print.h"
/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define VDEC_MEASURE_ISR_DELAY	1

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
// TODO : duplicated inf vdec_io.h
#define DIFF_28BIT(_a,_b)	(((_a)>= (_b)) ? ((SINT32)(_a) - (SINT32)(_b)): (0x10000000 - ((SINT32)(_b) - (SINT32)(_a))))

/*----------------------------------------------------------------------------------------
	Static Function Prototypes Declarations
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Static Variables
----------------------------------------------------------------------------------------*/
#ifdef VDEC_MEASURE_ISR_DELAY
static unsigned long	isr_cnt = 0;		// number of isr invocation.
static unsigned long	isr_jit_sum = 0;	// hw signal -> sw isr delay.
static unsigned long	isr_jit_max = 0;	// max of delay
static unsigned long	isr_dur_cnt = 0;
static unsigned long	isr_dur_sum = 0;	// duration of isr
static unsigned long	isr_dur_max = 0;	// max of duration
static unsigned long	isr_gstc = 0, isr_gstc1= 0, isr_cgstc = 0, isr_jit = 0, isr_dur = 0;
#endif	/* VDEC_MEASURE_ISR_DELAY */

/**
 * [debug] VDEC Event Counter log.
 */
typedef struct
{
	UINT32	cnt[VDEC_DBG_EVCNT_MAX+1];
} VDEC_DBG_EVCNT_T;

/**
 * [debug] list of debug event name.
 */
static char *_strEVCNT_name_list[] =
{
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_GSTC_START ),

	LX_STRING_ENTRY( VDEC_DBG_EVCNT_PIC_DETECT ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_BUF_UPDATE ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_CMD_START ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_PIC_DECODED ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_DE_VSYNC ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_USRD ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_USRD_VALID ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_USRD_SKIP ),

	LX_STRING_ENTRY( VDEC_DBG_EVCNT_CPB_FLUSH ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_WATCHDOG ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_SDEC_WATCHDOG ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_LIPSYNC_LOST ),

	LX_STRING_ENTRY( VDEC_DBG_EVCNT_DPB_REPEAT ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_DPB_SKIP ),

	LX_STRING_ENTRY( VDEC_DBG_EVCNT_H264_PACKED ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_FIELD_PIC ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_OTHER_14 ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_OTHER_UFI ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_RERUN ),

	LX_STRING_ENTRY( VDEC_DBG_EVCNT_LQ_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_CQ_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_DQ_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_RQ_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_AU_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_ES_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_CPB_FULL ),
	LX_STRING_ENTRY( VDEC_DBG_EVCNT_CPB_EMPT ),

	NULL	// end marker. : DO NOT DELETE LX_ENUM_NAME depends on this.
};

#define _EVCNT_NAME(_id)		LX_ENUM_NAME(_id, _strEVCNT_name_list, VDEC_DBG_EVCNT_MAX-1, sizeof("VDEC_DBG_EVCNT_") -1 )

static VDEC_DBG_EVCNT_T *_sVdecEventCount = NULL;
static UINT8			 _gVdecEventChMaxN= VDEC_KDRV_CH_MAXN;

/*========================================================================================
	ISR Delay debugging.
========================================================================================*/

/**
 * Prints ISR Delay value.
 *
 * [NOTE] Only Call from sequence file interafce. Do Not call directly.
 *
 * @param buffer	[OUT] debug message for ISR Delay.
 *
 * returns bytes written.
 */
int VDEC_DBG_PrintISRDelay(char * buffer)
{
	int		written = 0;
#ifdef VDEC_MEASURE_ISR_DELAY
	unsigned long	jit_int =0, jit_frac = 0;
	unsigned long	dur_int =0, dur_frac = 0;
	unsigned long	jit_max_int =0, jit_max_frac = 0;
	unsigned long	dur_max_int =0, dur_max_frac = 0;

	static	unsigned long prv_isr_cnt = 0;
	unsigned long diff_printed;

#define VDEC_GSTC_KHZ	90

	if ( isr_cnt )
	{
		// calculate milli seconds.
		jit_int		=  (isr_jit_sum / isr_cnt ) / VDEC_GSTC_KHZ;
		jit_frac	= ((isr_jit_sum * 1000UL / isr_cnt ) / VDEC_GSTC_KHZ) % 1000UL;

		jit_max_int =  (isr_jit_max ) / VDEC_GSTC_KHZ;
		jit_max_frac= ((isr_jit_max * 1000UL) / VDEC_GSTC_KHZ) % 1000UL;

		dur_int		=  (isr_dur_sum / isr_dur_cnt ) / VDEC_GSTC_KHZ;
		dur_frac 	= ((isr_dur_sum * 1000UL / isr_dur_cnt ) / VDEC_GSTC_KHZ) % 1000UL;

		dur_max_int =  (isr_dur_max ) / VDEC_GSTC_KHZ;
		dur_max_frac= ((isr_dur_max * 1000UL) / VDEC_GSTC_KHZ) % 1000UL;
	}
	else
	{
		jit_int = isr_jit_sum;
		dur_int = isr_dur_sum;
	}

	written += sprintf(buffer+written, "VDEC ISR Delay   : average / max : %3lu.%03lu ms/ %3lu.%03lu ms\n",
							  			jit_int, jit_frac, jit_max_int, jit_max_frac);
	written += sprintf(buffer+written, "VDEC ISR Duration: average / max : %3lu.%03lu ms/ %3lu.%03lu ms\n",
							  			dur_int, dur_frac, dur_max_int, dur_max_frac);
	/*
	written += sprintf(buffer+written, "VDEC ISR Duration: current / sum (@90KHz): %ld, %ld, %ld, %ld, sum, %ld, %ld, max, %ld, %ld,cnt %ld\n",
											isr_cgstc, isr_gstc, isr_jit, isr_dur,
											isr_jit_sum, isr_dur_sum,
											isr_jit_max, isr_dur_max,
											isr_cnt);
											*/

	diff_printed = isr_cnt - prv_isr_cnt;
	if ( diff_printed < 120 )
	{
		isr_jit_sum = 0;
		isr_dur_sum = 0;
		written += sprintf(buffer+written, "VDEC ISR ");
		if ( diff_printed < 60 )
		{
			isr_dur_max = 0;
			isr_jit_max = 0;
			written += sprintf(buffer+written, "Max & ");
		}
		written += sprintf(buffer+written, "Sum of Delay/Duration are reseted\n");
		isr_cnt = 0;
		isr_dur_cnt = 0;
		prv_isr_cnt = 0;
	}
	prv_isr_cnt = isr_cnt;

#endif	/* VDEC_MEASURE_ISR_DELAY */
	return written;
}

/**
 * capture ISR Delay infomation.
 *
 * call this function with isr_position = 0, at the entry point of ISR,
 * call this function with isr_position = 1, at the last stage of ISR.
 *
 * @param isr_position	[IN] 0 at the entry point of ISR, 1 at the last stage of ISR
 * @param gstc			[IN] current GSTC value.
 * @param captured_gstc	[IN] captured GSTC value.(only used when if isr_position==0, otherwise set 0)
 * @param isVsyncIsr	[IN] is Vsync Interrupt or not.
 */
void VDEC_DBG_ISRDelayCapture(int isr_position, UINT32 gstc, UINT32 captured_gstc, UINT32 isVsyncIsr)
{
#ifdef VDEC_MEASURE_ISR_DELAY
	if ( isr_position == 0 )
	{
		isr_gstc  = gstc;
		if ( isVsyncIsr )
		{
			isr_cgstc = captured_gstc;

			isr_cnt ++;
			isr_jit = DIFF_28BIT( isr_gstc, isr_cgstc);
			isr_jit_sum+= isr_jit;
			isr_jit_max = max( isr_jit_max, isr_jit);
		}
	}
	else
	{
		isr_gstc1  = gstc;
		isr_dur_cnt++;
		isr_dur = DIFF_28BIT(isr_gstc1, isr_gstc);
		isr_dur_sum+= isr_dur;
		isr_dur_max = max( isr_dur_max, isr_dur);
	}
#endif	/* VDEC_MEASURE_ISR_DELAY */
}

/*========================================================================================
	Internal Event Counter for VDEC Device Driver
========================================================================================*/
/**
 * [debug] Initialize Event Counter.
 *
 * allocates event counter variables for every channel.
 *
 * [NOTE] SHOULD BE CALLED BEFORE any of following condition.
 * # calling VDEC_PROC_Init()
 * # enable Interrupt.
 * # any VDEC_Ioctl();
 *
 * @param max_ch	[IN] number of channels for monitoring.
 *
 * @return	0 for success, negative for error.
 */
int	VDEC_DBG_EVCNT_Init(UINT8	max_ch)
{
	_sVdecEventCount = (VDEC_DBG_EVCNT_T*) kmalloc( max_ch * sizeof(VDEC_DBG_EVCNT_T) , GFP_KERNEL);
	_gVdecEventChMaxN= max_ch;

	if ( NULL == _sVdecEventCount ) return -ENOMEM;

	memset(_sVdecEventCount, 0, max_ch*sizeof(VDEC_DBG_EVCNT_T));

	return 0;
}

/**
 * un initialize event counter.
 */
void VDEC_DBG_EVCNT_UnInit(void)
{
	if ( _sVdecEventCount )
		kfree(_sVdecEventCount);

	return;
}

/**
 * Clears event counters for given VDEC channel.
 *
 * Only clears those related to current source of decoding. In order to do that, this function should be called
 *
 * VDEC_IO_ChInitialize. (e.g. decoder source is changed TE0 -> Buffer play, and etc. )
 *
 * [NOTE] only clears GSTC_START, PIC_DETECT, BUF_UPDATE, CMD_START, PIC_DECODED, DE_VSYNC, CPB_FLUSH.
 *
 * other value shall be preserved before power off.
 *
 * @param ch	[IN]	event counter for channel 'ch' shall be reset.
 * @param gstc	[IN]	current gstc value when this function called.
 *
 * @see VDEC_IO_ChInitialize
 *
 */
void VDEC_DBG_EVCNT_Clear(UINT8 ch, UINT32 gstc)
{
	if ( !_sVdecEventCount )		return;
	if ( ch >= _gVdecEventChMaxN)	return;

	//memset(&_sVdecEventCount[ch], 0, sizeof(VDEC_DBG_EVCNT_T));
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_PIC_DETECT]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_BUF_UPDATE]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_CMD_START]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_PIC_DECODED]= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_DE_VSYNC]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_CPB_FLUSH]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_USRD]		= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_USRD_VALID]	= 0;
	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_USRD_SKIP]	= 0;

	_sVdecEventCount[ch].cnt[VDEC_DBG_EVCNT_GSTC_START]	= gstc;
}

/**
 * Increases event.
 *
 * @param ch	[IN]	channel to be logged.
 * @param ev_id	[IN]	Event ID @see VDEC_DBG_EVCNT_PIC_DETECT.
 *
 */
void VDEC_DBG_EVCNT_Increase(UINT8	ch, UINT8 ev_id)
{
	if ( !_sVdecEventCount )			return;
	if ( ch >= _gVdecEventChMaxN)		return;
	if ( ev_id >= VDEC_DBG_EVCNT_MAX )	return;

	_sVdecEventCount[ch].cnt[ev_id] ++;
}

/**
 * Prints Event Counter.
 *
 * @param	ch		[IN]	which channel to print.
 * @param	buffer	[IN]	buffer pointer where message saved.
 * @param	length	[IN]	size of buffer in btyes.
 *
 * @return	bytes written on success, zero on error.
 */
int VDEC_DBG_EVCNT_Print(UINT8 ch, char *buffer, int length)
{
	UINT32	*pCnt;
	int		written = 0;
	int		ev_id;
	int		ev_id_max = VDEC_DBG_EVCNT_MAX;
	int		cols = 3;

	if ( !_sVdecEventCount )		return written;
	if ( ch >= _gVdecEventChMaxN)	return written;

	pCnt = &_sVdecEventCount[ch].cnt[0];

	for ( ev_id = 0; ev_id < ev_id_max; ev_id++)
	{
		char delim = ' ';

		if ( written >= length ) break;

		if ( (!((ev_id+1)%cols)) || ev_id == ev_id_max-1 ) delim = '\n';

		written += snprintf(buffer+written, length-written, "%14.14s,%8u,%c", _EVCNT_NAME(ev_id), pCnt[ev_id], delim);
	}

	return written;
}

/**
 * Prints Channel Info
 *
 * @param	ch		[IN]	which channel to print.
 * @param	buffer	[IN]	buffer pointer where message saved.
 * @param	length	[IN]	size of buffer in btyes.
 *
 * @return	bytes written on success, zero on error.
 */
int VDEC_DBG_CHANNEL_Print(UINT8 ch, char *buffer, int length)
{
	int		written = 0;
	int		state;

	if ( ch >= _gVdecEventChMaxN)	return written;

	state = VDEC_KTOP_GetState(ch);
	written += snprintf(buffer+written, length-written, "Channel %u info(state:%d)\n", ch, state);

	if( state != VDEC_KTOP_NULL )
	{
		written += snprintf(buffer+written, length-written, "- VES Channel: %u\n", VDEC_KTOP_GetChannel(ch, 1));
		written += snprintf(buffer+written, length-written, "- VDC Channel: %u\n", VDEC_KTOP_GetChannel(ch, 2));
		written += snprintf(buffer+written, length-written, "- DQ Channel: %u\n", VDEC_KTOP_GetChannel(ch, 3));
		written += snprintf(buffer+written, length-written, "- DE Channel: %u\n", VDEC_KTOP_GetChannel(ch, 4));
	}
	else
	{
		written += snprintf(buffer+written, length-written, "- Not opened state\n");
	}

	return written;
}

void VES_Stop(UINT8 ui8VesCh);
void VES_Start(UINT8 ui8VesCh);

#ifdef _PROC_CPB_DUMP
int VDEC_DBG_DumpCPB( char ch,  char* buffer, char** buffer_location, off_t offset, int buffer_length, int* eof, void* data )
{
	static char ves_ch;
	static unsigned int cpb_addr=VDEC_CPB_BASE, cpb_size=VDEC_CPB_SIZE / 2, cpb_wroffset=0;
	unsigned int rd_size=0;
	unsigned char *cpb_rd_ptr, *cpb_start_ptr;

	if( offset == 0 )
	{
		ves_ch = VDEC_KTOP_GetChannel(ch, 1);

		if( ves_ch != 0xFF )
		{
			cpb_addr = VES_CPB_GetBufferBaseAddr(ves_ch);
			cpb_size = VES_CPB_GetBufferSize(ves_ch);
			cpb_wroffset =  VES_CPB_GetPhyWrPtr(ves_ch) - cpb_addr;
		}
		else
		{
			cpb_addr = VDEC_CPB_BASE;
			cpb_size = VDEC_CPB_SIZE / 2;
			cpb_wroffset = 0;
		}

		VES_Stop(ves_ch);

		printk("\n0x%X => 0x0 shifted\n", cpb_wroffset);
	}
	//cpb_addr = (unsigned int)unitz;
	//cpb_size = sizeof(unitz);

	cpb_start_ptr = (unsigned char*)ioremap(cpb_addr, cpb_size);
	//cpb_start_ptr = unitz;

	if( cpb_start_ptr == NULL )
		goto error_exit;

	cpb_rd_ptr = cpb_start_ptr + cpb_wroffset + (unsigned int)offset;
	if(cpb_rd_ptr >= cpb_start_ptr+cpb_size)
		cpb_rd_ptr -= cpb_size;

	if( cpb_size <= (unsigned int)offset )
		rd_size = 0;
	else
	{
		rd_size = cpb_size - (unsigned int)offset;

		if( rd_size > buffer_length )
			rd_size = buffer_length;
	}

	if( cpb_rd_ptr+rd_size <= cpb_start_ptr+cpb_size )
		memcpy(buffer, cpb_rd_ptr, rd_size);
	else
	{
		unsigned int part_size;
		part_size = cpb_start_ptr + cpb_size - cpb_rd_ptr;
		memcpy(buffer, cpb_rd_ptr, part_size);
		buffer += part_size;
		part_size = rd_size - part_size;
		memcpy(buffer, cpb_start_ptr, part_size);
	}

	*buffer_location = (char*)rd_size;

	if( rd_size == 0 )
	{
		*eof = 1;
		VES_Start(ves_ch);
	}
	else
		*eof = 0;

	iounmap(cpb_start_ptr);

error_exit:

	return rd_size;
}
#endif

//extern void MSVC_DebugCmd(UINT32 u32MsvcCh, char* zCmdLine);
int VDEC_DBG_MSVC_Cmd( char vdec_ch, char *zCmd_line)
{
	unsigned char vdc_ch; //, msvc_ch;

	vdc_ch = VDEC_KTOP_GetChannel(vdec_ch, 2);
	if(vdc_ch>=NUM_OF_MSVC_CHANNEL)
	{
		vdc_ch = 0;
		//return 0;
	}

//	MSVC_DebugCmd(vdc_ch, zCmd_line);

	return 0;
}

int VDEC_DBG_MSVC_Info( char ch,  char* buffer, int length)
{
	int		written = 0;
	int		state;
	int		i, size;
	unsigned char	msvc_ch;
	FRAME_BUF_INFO_T *pstFrameBufInfo;

	if ( ch >= _gVdecEventChMaxN)	return written;

	written += snprintf(buffer+written, length-written,
			"BODA F/W Ver : %X\n", MSVC_GetFWVersion());

	state = VDEC_KTOP_GetState(ch);

	if( state == VDEC_KTOP_NULL )
	{
		written += snprintf(buffer+written, length-written, "- Not opened vdec channel %d\n", state);
		return written;
	}

	msvc_ch = VDEC_KTOP_GetChannel(ch, 2);
	written += snprintf(buffer+written, length-written, "- MSVC Channel: %u\n", msvc_ch);

	pstFrameBufInfo = MSVC_ADP_DBG_GetDPB(msvc_ch);
	written += snprintf(buffer+written, length-written,
			"Num Of Frames : %d\n", pstFrameBufInfo->u32FrameNum);
//	written += snprintf(buffer+written, length-written,
//			"Stride : %d\n\n", pstFrameBufInfo->u32Stride);

	for(i=0; i<pstFrameBufInfo->u32FrameNum;i++)
	{
		written += snprintf(buffer+written, length-written, "#%2d Lumi %08X\n", i, pstFrameBufInfo->astFrameBuf[i].u32AddrY);
	}

	written += snprintf(buffer+written, length-written, "\n");
	for(i=0; i<pstFrameBufInfo->u32FrameNum;i++)
	{
		written += snprintf(buffer+written, length-written, "#%2d Chromi %08X\n", i, pstFrameBufInfo->astFrameBuf[i].u32AddrCb);
	}

	written += snprintf(buffer+written, length-written, "\n");
	size = pstFrameBufInfo->astFrameBuf[1].u32AddrY - pstFrameBufInfo->astFrameBuf[0].u32AddrY;
	for(i=0; i<pstFrameBufInfo->u32FrameNum;i++)
	{
		written += snprintf(buffer+written, length-written,
			"data.save.binary C:\\yuv\\lumi%02d.yuv a:0x%08X++(0x%X-1)\n", i, pstFrameBufInfo->astFrameBuf[i].u32AddrY, size);
	}

	written += snprintf(buffer+written, length-written, "\n");
	size = pstFrameBufInfo->astFrameBuf[1].u32AddrCb - pstFrameBufInfo->astFrameBuf[0].u32AddrCb;
	for(i=0; i<pstFrameBufInfo->u32FrameNum;i++)
	{
		written += snprintf(buffer+written, length-written,
			"data.save.binary C:\\yuv\\chromi%02d.yuv a:0x%08X++(0x%X-1)\n", i, pstFrameBufInfo->astFrameBuf[i].u32AddrCb, size);
	}

	return written;
}
/** @} */
