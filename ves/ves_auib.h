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
 * date       2011.04.19
 * note       Additional information.
 *
 */

#ifndef _VES_AUIB_H_
#define _VES_AUIB_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"
#include "ves_drv.h"
#include "ves_cpb.h"



#define	VES_AUIB_NUM_OF_NODE				0x800



#ifdef __cplusplus
extern "C" {
#endif



void VES_AUIB_Init( void (*_fpVES_CpbStatus)(S_VES_CALLBACK_BODY_CPBSTATUS_T *pCpbStatus) );
void VES_AUIB_Reset(UINT8 ui8VesCh);
BOOLEAN VES_AUIB_Open(UINT8 ui8VesCh, BOOLEAN bFromTVP);
void VES_AUIB_Close(UINT8 ui8VesCh);
void VDEC_ISR_AUIB_AlmostFull(UINT8 ui8VesCh);
void VDEC_ISR_AUIB_AlmostEmpty(UINT8 ui8VesCh);
UINT32 VES_AUIB_NumActive(UINT8 ui8VesCh);
UINT32 VES_AUIB_NumFree(UINT8 ui8VesCh);


#ifdef __cplusplus
}
#endif

#endif /* _VES_AUIB_H_ */

