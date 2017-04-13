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
 *  AV LipSync HAL.
 *  (Register Access Hardware Abstraction Layer )
 *
 *  author	youngki.lyu@lge.com
 *  version	1.0
 *  date		2011-10-19
 * @addtogroup lg1152_vdec
 * @{
 */

/*----------------------------------------------------------------------------------------
 *   Control Constants
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   File Inclusions
 *---------------------------------------------------------------------------------------*/
#include "lg1152_vdec_base.h"
#include "../../mcu/os_adap.h"
#include "../../mcu/vdec_print.h"

#include "av_lipsync_hal_api.h"

#ifndef __XTENSA__
#include <linux/kernel.h>
#endif

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/
#define 		VDEC_AV_LIPSYNC_BASE(ch)				(LG1152_VDEC_BASE + 0x8F0 + (ch * 0x8))	// write
#ifndef __XTENSA__
#define 		ADEC_AV_LIPSYNC_BASE(ch)				(0xC00084E8 + (ch * 0x8))					// read
#endif

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------
 *   Type Definitions
 *---------------------------------------------------------------------------------------*/
typedef struct {
	UINT32	ui32Gstc;	// 0:27
	UINT32	ui32Pts;		// 0:27
} AV_LIPSYNC_BASE_REG_T;


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
static volatile AV_LIPSYNC_BASE_REG_T		*stpVdec_LipSync_Base_Reg[SRCBUF_NUM_OF_CHANNEL];
#ifndef __XTENSA__
static volatile AV_LIPSYNC_BASE_REG_T		*stpAdec_LipSync_Base_Reg[SRCBUF_NUM_OF_CHANNEL];
#endif

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
void AV_LipSync_HAL_Init(void)
{
	UINT8	ui8SrcBufCh;

	VDEC_KDRV_Message(_TRACE, "[AV_LipSync][DBG] %s", __FUNCTION__ );

	for( ui8SrcBufCh = 0; ui8SrcBufCh < SRCBUF_NUM_OF_CHANNEL; ui8SrcBufCh++ )
	{
		stpVdec_LipSync_Base_Reg[ui8SrcBufCh] = (volatile AV_LIPSYNC_BASE_REG_T *)VDEC_TranselateVirualAddr(VDEC_AV_LIPSYNC_BASE(ui8SrcBufCh), 0x8);
#ifndef __XTENSA__
		stpAdec_LipSync_Base_Reg[ui8SrcBufCh] = (volatile AV_LIPSYNC_BASE_REG_T *)VDEC_TranselateVirualAddr(ADEC_AV_LIPSYNC_BASE(ui8SrcBufCh), 0x8);
#endif
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
void AV_LipSync_HAL_Vdec_SetBase(UINT8 ui8SrcBufCh, UINT32 ui32BaseTime, UINT32 ui32BasePTS)
{
	VDEC_KDRV_Message(_TRACE, "[AV_LipSync%d][DBG] Set VDEC Base Time:0x%08X, PTS:0x%08X, %s", ui8SrcBufCh, ui32BaseTime, ui32BasePTS, __FUNCTION__ );

	if( ui8SrcBufCh >= SRCBUF_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AV_LIPSYNC][Err] Channel Number(%d), %s", ui8SrcBufCh, __FUNCTION__ );
		return;
	}

	stpVdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Gstc = ui32BaseTime;
	stpVdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Pts = ui32BasePTS;
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
void AV_LipSync_HAL_Vdec_GetBase(UINT8 ui8SrcBufCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS)
{
	if( ui8SrcBufCh >= SRCBUF_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AV_LIPSYNC][Err] Channel Number(%d), %s", ui8SrcBufCh, __FUNCTION__ );
		return;
	}

	*pui32BaseTime = stpVdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Gstc;
	*pui32BasePTS  = stpVdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Pts;

	VDEC_KDRV_Message(_TRACE, "[AV_LipSync%d][DBG] Get VDEC Base Time:0x%08X, PTS:0x%08X, %s", ui8SrcBufCh, *pui32BaseTime, *pui32BasePTS, __FUNCTION__ );
}

#ifndef __XTENSA__
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
void AV_LipSync_HAL_Adec_GetBase(UINT8 ui8SrcBufCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS)
{
	if( ui8SrcBufCh >= SRCBUF_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[AV_LIPSYNC][Err] Channel Number(%d), %s", ui8SrcBufCh, __FUNCTION__ );
		return;
	}

	*pui32BaseTime = stpAdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Gstc;
	*pui32BasePTS  = stpAdec_LipSync_Base_Reg[ui8SrcBufCh]->ui32Pts;

	VDEC_KDRV_Message(_TRACE, "[AV_LipSync%d][DBG] Get ADEC Base Time:0x%08X, PTS:0x%08X, %s", ui8SrcBufCh, *pui32BaseTime, *pui32BasePTS, __FUNCTION__ );
}
#endif

/** @} */

