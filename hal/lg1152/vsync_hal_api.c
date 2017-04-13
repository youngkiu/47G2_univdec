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
 * version    0.1
 * date       2010.03.11
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
#include "../../mcu/vdec_type_defs.h"
#include "lg1152_vdec_base.h"
#include "../../mcu/os_adap.h"
#include "vsync_reg.h"
#include "vsync_hal_api.h"
#include "../../mcu/vdec_print.h"

/*----------------------------------------------------------------------------------------
 *   Constant Definitions
 *---------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
 *   Macro Definitions
 *---------------------------------------------------------------------------------------*/
#define VDEC_SYNC_BASE					(LG1152_VDEC_BASE + 0x1700)

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
static volatile Sync_REG_T			*stpSync_Reg = NULL;


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
void VSync_HAL_Init(void)
{
	stpSync_Reg		= (volatile Sync_REG_T *)VDEC_TranselateVirualAddr(VDEC_SYNC_BASE, 0x20);
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
UINT32 VSync_HAL_GetVersion(void)
{
	return stpSync_Reg->version;
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
void VSync_HAL_Reset(void)
{
	SYNC_CONF		sync_conf;

	sync_conf = stpSync_Reg->sync_conf;

	sync_conf.reg_vsync_srst = 1;

	stpSync_Reg->sync_conf = sync_conf;
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
UINT32 VSync_HAL_UTIL_GCD(UINT32 a, UINT32 b)
{
	if(a%b == 0)
	{
		return b;
	}

	return VSync_HAL_UTIL_GCD(b,a%b);
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
BOOLEAN VSync_HAL_SetVsyncField(UINT8 ui8VSyncCh, UINT32 ui32FrameRateRes, UINT32 ui32FrameRateDiv, BOOLEAN bInterlaced)
{
	UINT32	ui32FieldNum;

	UINT32	ui32FrameRateRes_Scaled;
	UINT32	ui32_27MHz_Scaled = 27000000;
	UINT32	ui32Gcd1, ui32Gcd2;
	UINT32	ui320xFFFFFFFF_Boundary;
	UINT32	ui32FrameRateDiv_Scaled;

	const UINT32	ui32ScaleUnit = 0x2;	// must > 1
	UINT32	ui32FieldRate;

	if( ui8VSyncCh >= VSYNC_NUM_OF_CHANNEL )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][Err] Channel Number, %s", ui8VSyncCh, __FUNCTION__ );
		return FALSE;
	}

	if( (ui32FrameRateRes == 0) || (ui32FrameRateDiv == 0) )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][ERR] Frame Rate Residual(%d)/Divider(%d)", ui8VSyncCh, ui32FrameRateRes, ui32FrameRateDiv );
		return FALSE;
	}

	if( (ui32FrameRateRes * bInterlaced / ui32FrameRateDiv) > VSYNC_FIELDRATE_MAX )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][ERR] Frame Rate Residual(%d)/Divider(%d)", ui8VSyncCh, ui32FrameRateRes, ui32FrameRateDiv );
		return FALSE;
	}

	ui32Gcd1 = VSync_HAL_UTIL_GCD( ui32FrameRateDiv, ui32FrameRateRes );
	ui32FrameRateDiv /= ui32Gcd1;
	ui32FrameRateRes /= ui32Gcd1;

	ui32Gcd2 = VSync_HAL_UTIL_GCD( ui32_27MHz_Scaled, ui32FrameRateRes );
	ui32_27MHz_Scaled /= ui32Gcd2;
	ui32FrameRateRes_Scaled = ui32FrameRateRes / ui32Gcd2;

	ui32FrameRateDiv_Scaled = ui32FrameRateDiv;
	while( ui32_27MHz_Scaled )
	{
		ui320xFFFFFFFF_Boundary = 0xFFFFFFFF / ui32_27MHz_Scaled;

		if( ui32FrameRateDiv_Scaled <= ui320xFFFFFFFF_Boundary )
			break;

		ui32_27MHz_Scaled /= ui32ScaleUnit;
		ui32FrameRateDiv_Scaled /= ui32ScaleUnit;
		ui32FrameRateRes_Scaled /= (ui32ScaleUnit * 2);
	}
	VDEC_KDRV_Message(_TRACE, "[VSync%d][DBG] GCD:%d:%d, Scaled - Frame Rate Residual(%d)/Divider(%d) 27MHz:%d", ui8VSyncCh, ui32Gcd1, ui32Gcd2, ui32FrameRateRes_Scaled, ui32FrameRateDiv_Scaled, ui32_27MHz_Scaled );

	ui32FieldRate = (bInterlaced == TRUE) ? 2 : 1;
	ui32FieldNum = ui32_27MHz_Scaled * ui32FrameRateDiv_Scaled / (ui32FrameRateRes_Scaled * ui32FieldRate); // = 27MHz / FieldRate

	stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field1_num = ui32FieldNum;

	VDEC_KDRV_Message(DBG_SYS, "[VSync%d][DBG] Field Rate Residual(%d)/Divider(%d), FieldNum:%d", ui8VSyncCh, ui32FrameRateRes * ui32FieldRate, ui32FrameRateDiv, ui32FieldNum );

	return TRUE;
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
void VSync_HAL_RestartAllVsync(void)
{
	UINT8		ui8VSyncCh;
	UINT32		ui32FieldNum[VSYNC_NUM_OF_CHANNEL];

	VDEC_KDRV_Message(_TRACE, "[VSync][DBG] %s", __FUNCTION__ );

	for( ui8VSyncCh = 0; ui8VSyncCh < VSYNC_NUM_OF_CHANNEL; ui8VSyncCh++ )
	{
		ui32FieldNum[ui8VSyncCh] = stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field1_num;
		stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field1_num = 0x0006DF92;
	}
	VSync_HAL_Reset();
	for( ui8VSyncCh = 0; ui8VSyncCh < VSYNC_NUM_OF_CHANNEL; ui8VSyncCh++ )
	{
		stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field1_num = ui32FieldNum[ui8VSyncCh];
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
void VSync_HAL_PrintReg(void)
{
	UINT8		ui8VSyncCh;

	for( ui8VSyncCh = 0; ui8VSyncCh < VSYNC_NUM_OF_CHANNEL; ui8VSyncCh++ )
	{
		VDEC_KDRV_Message(ERROR, "[VSync%d][DBG] Conf:0x%X, FieldNum:0x%X 0x%X", ui8VSyncCh, *(UINT32 *)&stpSync_Reg->sync_conf, stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field0_num, stpSync_Reg->vsync_field_num[ui8VSyncCh].reg_field1_num );
	}
}

/** @} */

