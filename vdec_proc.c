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
 *  Linux proc interface for vdec device.
 *	vdec device will teach you how to make device driver with new platform.
 *
 *  author		seokjoo.lee (seokjoo.lee@lge.com)
 *  version		1.0
 *  date		2009.12.30
 *  note		Additional information.
 *
 *  @addtogroup lg1152_vdec
 *	@{
 */

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	File Inclusions
----------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include "vdec_drv.h"
#include "vdec_io.h"
#include "vdec_dbg.h"
#include "proc_util.h"
#include "debug_util.h"

#include "mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Type Definitions
----------------------------------------------------------------------------------------*/
enum {
	PROC_ID_LOG=0,
	PROC_ID_ISR_DELAY,
	PROC_ID_FPQ,
	PROC_ID_FRAME,
	PROC_ID_STATUS,
	PROC_ID_CHANNEL,
	PROC_ID_CPB,
	PROC_ID_MSVC,
	PROC_ID_MAX,
};

/*----------------------------------------------------------------------------------------
	External Function Prototype Declarations
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	External Variables
----------------------------------------------------------------------------------------*/
extern const struct file_operations vdec_fpq_proc_fops;
extern const struct file_operations vdec_frame_proc_fops;

/*----------------------------------------------------------------------------------------
	global Variables
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Static Function Prototypes Declarations
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Static Variables
----------------------------------------------------------------------------------------*/
static OS_PROC_DESC_TABLE_T	_g_vdec_device_proc_table[] =
{
	{ "log",		PROC_ID_LOG     , OS_PROC_FLAG_WRITE },
	{ "delay",		PROC_ID_ISR_DELAY,OS_PROC_FLAG_READ },
	{ "fpq",		PROC_ID_FPQ,      OS_PROC_FLAG_READ|OS_PROC_FLAG_FOP, (void*)&vdec_fpq_proc_fops },
	{ "frame",		PROC_ID_FRAME,    OS_PROC_FLAG_READ|OS_PROC_FLAG_FOP, (void*)&vdec_frame_proc_fops },
	{ "status",		PROC_ID_STATUS,   OS_PROC_FLAG_READ },
	{ "channel",	PROC_ID_CHANNEL,  OS_PROC_FLAG_READ },
#ifdef _PROC_CPB_DUMP
	{ "cpb",		PROC_ID_CPB,	  OS_PROC_FLAG_READ },
#endif
	{ "msvc",		PROC_ID_MSVC,	  OS_PROC_FLAG_READ|OS_PROC_FLAG_WRITE },
	{ NULL, 		PROC_ID_MAX		, 0 }
};

/*========================================================================================
	Implementation Group
========================================================================================*/

/*
 * read_proc implementation of vdec device
 *
*/
static int	_VDEC_PROC_Reader( char* buffer, char** buffer_location, off_t offset, int buffer_length, int* eof, void* data )
{
	int		ret=0;
	UINT8	ch		= (((UINT32)data) >> 8 ) & 0xff;	// HACK!.
	UINT8	procId	= ((UINT32)data) & 0xff;

	//if ( offset > 0 ) return 0;

	switch( procId )
	{
		case PROC_ID_ISR_DELAY:
		{
			ret = VDEC_DBG_PrintISRDelay(buffer);
			*eof = 1;
		}
		break;
		case PROC_ID_STATUS:
		{
//			int enabled = 0;

			//ret  = VDEC_DBG_PrintStatus(ch, buffer, buffer_length, &enabled);
			ret += VDEC_DBG_EVCNT_Print(ch, buffer+ret, buffer_length-ret);
			*eof = 1;
		}
		break;

		case PROC_ID_CHANNEL:
			ret += VDEC_DBG_CHANNEL_Print(ch, buffer+ret, buffer_length-ret);
			*eof = 1;
		break;

		case PROC_ID_CPB:
		{
#ifdef _PROC_CPB_DUMP
			ret = VDEC_DBG_DumpCPB(ch, buffer, buffer_location, offset, buffer_length, eof, NULL);
#endif
		}
		break;

		case PROC_ID_MSVC:
			ret = VDEC_DBG_MSVC_Info(ch, buffer, buffer_length);
			*eof = 1;
		break;

		default:
		{
			ret = snprintf( buffer, buffer_length, "%s(%d)\n", "unimplemented read proc", procId );
			*eof = 1;
		}
	}

	return ret;
}

static int	_VDEC_PROC_Writer( struct file* file, const char* command, unsigned long count, void* data )
{
	UINT8	ch		= (((UINT32)data) >> 8 ) & 0xff;	// HACK!.
	UINT8	procId	= ((UINT32)data) & 0xff;

	/* TODO: add your proc_write implementation */

	if(command == NULL)
		return 0;

	switch( procId )
	{
		case PROC_ID_LOG:
		{
			int idx;
			UINT32 flag = 0;
			if ( command ) flag = simple_strtoul(command, NULL, 0);

			if(g_vdec_debug_fd>=0)
			{
				for ( idx = 0; idx < LX_MAX_MODULE_DEBUG_NUM; idx++)
				{
					if ( flag & (1<<idx) )	OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, idx, DBG_COLOR_NONE );
					else					OS_DEBUG_DisableModuleByIndex( g_vdec_debug_fd, idx);
				}
			}
		}
		break;

		case PROC_ID_MSVC:
		{
			char cmd_line_buf[41];

			if( count > 40 )
				count = 40;

			if( copy_from_user(cmd_line_buf, command, count) )
				return -EFAULT;
			cmd_line_buf[count] = '\0';

			VDEC_DBG_MSVC_Cmd(ch, cmd_line_buf);
		}
		break;

		default:
		break;
	}

	return strlen(command);
}

/**
 * initialize proc utility for vdec device for given channel.
 *
 * @param data	[IN] SHOULD be &g_vdec_device[ch] for given vdec channel 'ch'.
 *
 * @see VDEC_Init
*/
void	VDEC_PROC_Init (UINT8 ch, void* data)
{
	char proc_name[8] = { 'v','d','e','c', 0, 0};
	OS_PROC_DESC_TABLE_T	*pProcDesc = &_g_vdec_device_proc_table[0];

	proc_name[4] = '0' + ch;
	// HACK!
	// 1. for OS_PROC_FLAG_FOP case :
	// just temporily pass a pointer value via OS_PROC_DESC_TABLE_T.data and reuse all other things.
	// I'll pass &g_vdec_device_t ( per-channel module data ) to proc entry.
	// 2. otherwise, make own proc_id and channel pair. Why did you choose 16 bit, T.T

	while (pProcDesc->name)
	{
		pProcDesc->data = data;
		pProcDesc->id = ( (((UINT32)ch)<<8) | ( (pProcDesc->id) & 0xff));
		pProcDesc++;
	}
	OS_PROC_CreateEntry ( proc_name, _g_vdec_device_proc_table,
										_VDEC_PROC_Reader,
										_VDEC_PROC_Writer );
}

/**
 * cleanup proc utility for vdec device
 *
 * @see VDEC_Cleanup
*/
void	VDEC_PROC_Cleanup (UINT8 ch)
{
	char proc_name[8] = { 'v','d','e','c', 0, 0};
	proc_name[4] = '0' + ch;
	OS_PROC_RemoveEntry( proc_name );
}

/** @} */

