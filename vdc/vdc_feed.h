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
 * date       2012.07.11
 * note       Additional information.
 *
 */

#ifndef _VDEC_FEED_H_
#define _VDEC_FEED_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "vdc_auq.h"



#ifdef __cplusplus
extern "C" {
#endif




void FEED_Init(	BOOLEAN (*_fpFEED_GetDQStatus)(UINT8 ui8VdcCh, UINT32 *pui32NumOfActiveDQ, UINT32 *pui32NumOfFreeDQ),
				BOOLEAN (*_fpFEED_IsReady)(UINT8 ui8VdcCh, UINT32 ui32GSTC, UINT32 ui32NumOfFreeDQ),
				BOOLEAN (*_fpFEED_RequestReset)(UINT8 ui8VdcCh),
				BOOLEAN (*_fpFEED_SendCmd)(UINT8 ui8VdcCh, S_VDC_AU_T *pVesAu, S_VDC_AU_T *pVesAu_Next, BOOLEAN *pbNeedMore, UINT32 ui32GSTC, UINT32 ui32NumOfActiveAuQ, UINT32 ui32NumOfActiveDQ) );
BOOLEAN FEED_Open( UINT8 ui8VdcCh, BOOLEAN bDual );
BOOLEAN FEED_Close( UINT8 ui8VdcCh );
UINT8 FEED_Kick( void );




#ifdef __cplusplus
}
#endif

#endif /* _VDEC_FEED_H_ */

