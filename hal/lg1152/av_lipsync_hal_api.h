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
 * DE IPC HAL.
 * (Register Access Hardware Abstraction Layer)
 *
 * author     youngki.lyu (youngki.lyu@lge.com)
 * version    0.1
 * date       2011.10.19
 * note       Additional information.
 *
 */

#ifndef _VDEC_AV_LIPSYNC_HAL_API_H_
#define _VDEC_AV_LIPSYNC_HAL_API_H_

#include "../../mcu/vdec_type_defs.h"



#ifdef __cplusplus
extern "C" {
#endif



#define	SRCBUF_NUM_OF_CHANNEL			2



void AV_LipSync_HAL_Init(void);
void AV_LipSync_HAL_Vdec_SetBase(UINT8 ui8SrcBufCh, UINT32 ui32BaseTime, UINT32 ui32BasePTS);
void AV_LipSync_HAL_Vdec_GetBase(UINT8 ui8SrcBufCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS);
#ifndef __XTENSA__
void AV_LipSync_HAL_Adec_GetBase(UINT8 ui8SrcBufCh, UINT32 *pui32BaseTime, UINT32 *pui32BasePTS);
#endif



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_AV_LIPSYNC_HAL_API_H_ */

