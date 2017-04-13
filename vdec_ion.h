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
 */

#ifndef _VDEC_ION_H_
#define _VDEC_ION_H_


#include "mcu/vdec_type_defs.h"


#ifdef __cplusplus
extern "C" {
#endif

extern struct ion_client *lg115x_ion_client_create(unsigned int heap_mask,
              const char *name);

struct s_ion_handle;

struct s_ion_handle *ION_Init(void);
void ION_Deinit(struct s_ion_handle *ionHandle);
UINT32 ION_Alloc(struct s_ion_handle *ionHandle, UINT32 size, BOOLEAN bPrefer2ndBuf);
BOOLEAN ION_GetPhy(struct s_ion_handle *ionHandle, SINT32 si32FreeFrameFd, UINT32 *pui32FrameAddr, UINT32 *pui32FrameSize);

/* Deprecated
 void ION_Free(UINT32 addr);
*/



#ifdef __cplusplus
}
#endif

#endif /* _VDEC_ION_H_ */

