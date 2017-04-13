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
 * date       2013.02.06
 * note       Additional information.
 *
 * @addtogroup lg115x_dpb
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "vdec_ion.h"

#include "mcu/os_adap.h"
#include "mcu/vdec_print.h"
#include "hal/lg1152/lg1152_vdec_base.h"

#include <linux/ion.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/list.h>

#ifdef INCLUDE_KDRV_DUALDPB
#include <linux/lg115x_ion.h>
#endif

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define VDEC_ION_HANDLE_ADDR_TABLE_NUM		4

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/

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

struct s_ion_handle {
	struct ion_client *m_ion_client;
	struct hlist_head ion_hlist_head;
};

struct ion_struct {
	struct hlist_node hlist;
	struct ion_handle *ion_handle;
	SINT32 share_fd;
	UINT32 frame_address;
	UINT32 frame_size;
};


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
struct s_ion_handle *ION_Init(void)
{
	struct s_ion_handle *ionHandle = (struct s_ion_handle*)kmalloc(sizeof(struct s_ion_handle), GFP_KERNEL);

	if (ionHandle == NULL)
		return NULL;

	ionHandle->m_ion_client = lg115x_ion_client_create(0,"VDEC");

	if (IS_ERR_OR_NULL(ionHandle->m_ion_client))
	{
		VDEC_KDRV_Message(ERROR,"%s ion create fail",__func__);
		kfree(ionHandle);
		return NULL;
	}
	INIT_HLIST_HEAD(&ionHandle->ion_hlist_head);

	return ionHandle;
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
void ION_Deinit(struct s_ion_handle *ionHandle)
{
	struct ion_struct *is;
	struct hlist_node *pos, *tmp;

	if (ionHandle) {
		hlist_for_each_entry_safe(is, pos, tmp, &ionHandle->ion_hlist_head, hlist) {
			hlist_del(&is->hlist);
			//ion_free(ionHandle->m_ion_client, is->ion_handle);
			kfree(is);
		}

		ion_client_destroy(ionHandle->m_ion_client);
		kfree(ionHandle);
		ionHandle = NULL;
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
UINT32 ION_Alloc(struct s_ion_handle *ionHandle, UINT32 size, BOOLEAN bPrefer2ndBuf)
{
	unsigned int heap_mask = 0;
	unsigned int flags = 0;
	size_t align = 0;
	struct ion_struct *ion_struct = (struct ion_struct*)kmalloc(sizeof(struct ion_struct*), GFP_KERNEL);

	if (ion_struct == NULL) {
		VDEC_KDRV_Message(ERROR,"%s:%d kmalloc fail",__func__,__LINE__);
		return 0;
	}

#ifdef INCLUDE_KDRV_DUALDPB
	heap_mask = (bPrefer2ndBuf == TRUE) ? ION_HEAP_ID_LG115X_VDEC_BOTTOM_MASK : ION_HEAP_ID_LG115X_VDEC_TOP_MASK;
#else
	heap_mask = ION_HEAP_SYSTEM_CONTIG_MASK | ION_HEAP_CARVEOUT_MASK;
#endif
	ion_struct->ion_handle = ion_alloc(ionHandle->m_ion_client, size, align, heap_mask, flags);

	if (IS_ERR_OR_NULL(ion_struct->ion_handle))
	{
#ifdef INCLUDE_KDRV_DUALDPB
		VDEC_KDRV_Message(ERROR,"%s:%d ion_alloc retry",__func__,__LINE__);

		// swap heap mask
		heap_mask = (bPrefer2ndBuf == TRUE) ? ION_HEAP_SYSTEM_CONTIG_MASK | ION_HEAP_ID_LG115X_VDEC_TOP_MASK : ION_HEAP_SYSTEM_CONTIG_MASK | ION_HEAP_ID_LG115X_VDEC_BOTTOM_MASK;
		ion_struct->ion_handle = ion_alloc(ionHandle->m_ion_client, size, align, heap_mask, flags);

		if (IS_ERR_OR_NULL(ion_struct->ion_handle))
#endif
		{
			VDEC_KDRV_Message(ERROR,"%s:%d ion_alloc fail",__func__,__LINE__);
			kfree(ion_struct);
			return 0;
		}
	}

	ion_phys(ionHandle->m_ion_client, ion_struct->ion_handle, (ion_phys_addr_t *)&ion_struct->frame_address, &ion_struct->frame_size);
	hlist_add_head(&ion_struct->hlist, &ionHandle->ion_hlist_head);

	return ion_struct->frame_address;
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
BOOLEAN ION_GetPhy(struct s_ion_handle *ionHandle, SINT32 si32FreeFrameFd, UINT32 *pui32FrameAddr, UINT32 *pui32FrameSize)
{
	struct ion_struct *is;
	struct hlist_node *pos;
	UINT32 ui32FrameAddr= 0;
	UINT32 ui32FrameSize = 0;

	hlist_for_each_entry(is, pos, &ionHandle->ion_hlist_head, hlist) {
		if (is->share_fd == si32FreeFrameFd) {
			ui32FrameAddr = is->frame_address;
			ui32FrameSize = is->frame_size;
			break;
		}
	}

	if (ui32FrameAddr == 0) {
		is = (struct ion_struct *)kmalloc(sizeof(struct ion_struct), GFP_KERNEL);
		if (is == NULL)
			return FALSE;

		is->share_fd = si32FreeFrameFd;
		is->ion_handle = ion_import_dma_buf(ionHandle->m_ion_client, si32FreeFrameFd);
		ion_phys(ionHandle->m_ion_client, is->ion_handle, (ion_phys_addr_t *)&is->frame_address, &is->frame_size);
		ui32FrameAddr = is->frame_address;
		ui32FrameSize = is->frame_size;
		hlist_add_head(&is->hlist, &ionHandle->ion_hlist_head);
	}

	*pui32FrameAddr = ui32FrameAddr;
	*pui32FrameSize = ui32FrameSize;

	return TRUE;
}


