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
 * date       2013.01.09
 * note       Additional information.
 *
 */

#ifndef _MUTEX_ARM_MCU_H_
#define _MUTEX_ARM_MCU_H_


#include "../mcu/vdec_type_defs.h"
#include "../mcu/mcu_config.h"



#ifdef __cplusplus
extern "C" {
#endif



void Mutex_ARM_Lock(void);
void Mutex_ARM_Unlock(void);
void Mutex_MCU_Lock(void);
void Mutex_MCU_Unlock(void);



#ifdef __cplusplus
}
#endif

#endif /* _MUTEX_ARM_MCU_H_ */

