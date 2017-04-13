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
 * date       2013.01.28
 * note       Additional information.
 *
 */

#ifndef _VDEC_CPB_H_
#define _VDEC_CPB_H_


#include "mcu/vdec_type_defs.h"
#include "mcu/mcu_config.h"



#ifdef __cplusplus
extern "C" {
#endif



void VDEC_CPB_Init(UINT32 ui32PoolAddr, UINT32 ui32PoolSize);
BOOLEAN VDEC_CPB_Alloc( UINT32 *pui32CpbBufAddr, UINT32 *pui32CpbBufSize, BOOLEAN bSecured );
BOOLEAN VDEC_CPB_Free(UINT32 ui32BufAddr);



#ifdef __cplusplus
}
#endif


#endif /* _VDEC_CPB_H_ */

