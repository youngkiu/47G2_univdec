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
 * author     sooya.joo@lge.com
 * version    1.0
 * date       2012.01.19
 * note
 *
 */

#ifndef _VDEC_STC_TIMER_DRV_H_
#define _VDEC_STC_TIMER_DRV_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	STC_TIMER_ID_BASE = (LQC_NUM_OF_CHANNEL-2),
	STC_TIMER_ID_IDLE = STC_TIMER_ID_BASE,
	STC_TIMER_ID_FEED = STC_TIMER_ID_BASE+1,
	STC_TIMER_ID_MAX = 0xFF
} STC_TIMER_ID;

#define STC_TIMER_ISR_IDLE_MASK	(LQC0_ALMOST_EMPTY+STC_TIMER_ID_IDLE)
#define STC_TIMER_ISR_FEED_MASK	(LQC0_ALMOST_EMPTY+STC_TIMER_ID_FEED)


#define FEED_TIMER_IDLE_msPERIOD	60	// ms
#define FEED_TIMER_Feed_msPERIOD	30	// ms

void STC_TIMER_Init(void);
void STC_TIMER_Stop(void);
void STC_TIMER_Start(UINT32 u32TimerID);
UINT32 STC_TIMER_GetStatus(void);

#ifdef __cplusplus
}
#endif
#endif /* _VDEC_STC_TIMER_DRV_H_*/
