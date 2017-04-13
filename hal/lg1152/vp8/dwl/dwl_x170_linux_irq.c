/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : System Wrapper Layer for Linux using IRQs
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl_x170_linux_irq.c,v $
--  $Revision: 1.15 $
--  $Date: 2010/03/24 06:21:33 $
--
------------------------------------------------------------------------------*/

#include "../../lg1152_vdec_base.h"
#include "../../../../mcu/vdec_type_defs.h"
#include "../../top_hal_api.h"

#ifdef __XTENSA__
#include <stdio.h>
#include "stdarg.h"
#else
#include <linux/kernel.h>
#include <asm/string.h>	// memset
#include <linux/delay.h>
#endif

#include "../../../../mcu/vdec_shm.h"
#include "../../../../mcu/os_adap.h"

#include "../inc/basetype.h"
#include "../inc/dwl.h"
#include "dwl_linux.h"
//#include "hx170dec.h"
//#include "memalloc.h"
//#include "dwl_linux_lock.h"

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <sys/mman.h>
//#include <sys/ioctl.h>
#ifdef _DWL_HW_PERFORMANCE
#include <sys/time.h>
#endif

//#include <signal.h>
//#include <fcntl.h>
//#include <unistd.h>

//#include <pthread.h>

//#include <errno.h>

//#include <assert.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#ifdef USE_EFENCE
#include "efence.h"
#endif

#ifdef _DWL_HW_PERFORMANCE
#define HW_PERFORMANCE_LOG "hw_performance.log"
#define HW_PERFORMANCE_RESOLUTION 0.00001   /* 10^-5 */
static FILE *hw_performance_log = NULL;
static u32 dec_pic_counter = 0;
static u32 pp_pic_counter = 0;
u32 dec_time;                /* decode time in milliseconds */
struct timeval dec_start_time;
struct timeval pp_start_time;
u32 pp_time /* pp time in milliseconds */ ;
#endif

static hX170dwl_t gDecDWL[NUM_OF_VP8_CHANNEL];
/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : void * param - not in use, application passes NULL
------------------------------------------------------------------------------*/
const void *DWLInit(u8 ui8ch, DWLInitParam_t * param)
{
	hX170dwl_t *dec_dwl;

	if(ui8ch >=NUM_OF_VP8_CHANNEL)
	{
		// ERROR_PRINT
		return NULL;
	}

	dec_dwl = (hX170dwl_t *)&gDecDWL[ui8ch];

	DWL_DEBUG("DWL: INITIALIZE\n");
	if(dec_dwl == NULL)
	{
		DWL_DEBUG("DWL: failed to alloc hX170dwl_t struct\n");
		return NULL;
	}

#ifdef INTERNAL_TEST
	dec_dwl->regDump = fopen(REG_DUMP_FILE, "a+");
	if(NULL == dec_dwl->regDump)
	{
		DWL_DEBUG("DWL: failed to open: %s\n", REG_DUMP_FILE);
		goto err;
	}
#endif

	dec_dwl->clientType = param->clientType;

#ifdef _DWL_HW_PERFORMANCE
	if(NULL == hw_performance_log)
	{
		hw_performance_log = fopen(HW_PERFORMANCE_LOG, "w");
		if(NULL == hw_performance_log)
		{
			DWL_DEBUG("DWL: failed to open: %s\n", HW_PERFORMANCE_LOG);
			goto err;
		}
	}
	switch (dec_dwl->clientType)
	{
	case DWL_CLIENT_TYPE_H264_DEC:
	case DWL_CLIENT_TYPE_MPEG4_DEC:
	case DWL_CLIENT_TYPE_JPEG_DEC:
	case DWL_CLIENT_TYPE_VC1_DEC:
	case DWL_CLIENT_TYPE_MPEG2_DEC:
	case DWL_CLIENT_TYPE_VP6_DEC:
	case DWL_CLIENT_TYPE_VP8_DEC:
	case DWL_CLIENT_TYPE_RV_DEC:
	case DWL_CLIENT_TYPE_AVS_DEC:
		dec_pic_counter = 0;
		dec_time = 0;
		dec_start_time.tv_sec = 0;
		dec_start_time.tv_usec = 0;
		break;
	case DWL_CLIENT_TYPE_PP:
		pp_pic_counter = 0;
		pp_time = 0;
		pp_start_time.tv_sec = 0;
		pp_start_time.tv_usec = 0;
		break;
	}
#endif

	dec_dwl->pRegBase = NULL;

	switch (dec_dwl->clientType)
	{
	case DWL_CLIENT_TYPE_VP8_DEC:
		/* asynchronus notification needed. This will be done in first DWLEnableHW,
		* so that we picup the decoding thread and not the init */
		dec_dwl->sigio_needed = 1;
		break;
	case DWL_CLIENT_TYPE_H264_DEC:
	case DWL_CLIENT_TYPE_MPEG4_DEC:
	case DWL_CLIENT_TYPE_JPEG_DEC:
	case DWL_CLIENT_TYPE_VC1_DEC:
	case DWL_CLIENT_TYPE_MPEG2_DEC:
	case DWL_CLIENT_TYPE_VP6_DEC:
	case DWL_CLIENT_TYPE_RV_DEC:
	case DWL_CLIENT_TYPE_AVS_DEC:
	case DWL_CLIENT_TYPE_PP:
		DWL_DEBUG("DWL: not support client type no. %d\n", dec_dwl->clientType);
		goto err;
	default:
		DWL_DEBUG("DWL: Unknown client type no. %d\n", dec_dwl->clientType);
		goto err;
	}

	/* map the hw registers to user space */
	dec_dwl->pRegBase = DWLMapRegisters();

	if(dec_dwl->pRegBase == NULL)
	{
		DWL_DEBUG("DWL: Failed to mmap regs\n");
		goto err;
	}

	DWL_DEBUG("DWL: regs size %d bytes, virt %08x\n", dec_dwl->regSize, (u32) dec_dwl->pRegBase);

	{
#if 0
	    key_t key = X170_SEM_KEY;
	    int semid;

	    if((semid = binary_semaphore_allocation(key, O_RDWR)) != -1)
	    {
	        DWL_DEBUG("DWL: HW lock sem %x aquired\n", key);
	    }
	    else if(errno == ENOENT)
	    {
	        semid = binary_semaphore_allocation(key, IPC_CREAT | O_RDWR);

	        binary_semaphore_initialize(semid);

	        DWL_DEBUG("DWL: HW lock sem %x created\n", key);
	    }
	    else
	    {
	        DWL_DEBUG("DWL: FAILED to get HW lock sem %x\n", key);
	        goto err;
	    }

	    dec_dwl->semid = semid;
#endif
	}

	return dec_dwl;

err:
	DWLmemset(dec_dwl, 0, sizeof(hX170dwl_t));
	return NULL;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
i32 DWLRelease(const void *instance)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	if(dec_dwl == NULL)
	{
//		 VDEC_KDRV_Message(ERROR, "%s param is NULL", __FUNCTION__);
		return DWL_ERROR;
	}

	if(dec_dwl->pRegBase != NULL)
	{
		DWLUnmapRegisters((void *) dec_dwl->pRegBase);
		dec_dwl->pRegBase = NULL;
	}

#ifdef _DWL_HW_PERFORMANCE
	switch (dec_dwl->clientType)
	{
	case DWL_CLIENT_TYPE_H264_DEC:
	case DWL_CLIENT_TYPE_MPEG4_DEC:
	case DWL_CLIENT_TYPE_JPEG_DEC:
	case DWL_CLIENT_TYPE_VC1_DEC:
	case DWL_CLIENT_TYPE_MPEG2_DEC:
	case DWL_CLIENT_TYPE_VP6_DEC:
	case DWL_CLIENT_TYPE_VP8_DEC:
	case DWL_CLIENT_TYPE_RV_DEC:
	case DWL_CLIENT_TYPE_AVS_DEC:
		fprintf(hw_performance_log, "dec;");
		fprintf(hw_performance_log, "%d;", dec_pic_counter);
		fprintf(hw_performance_log, "%d\n", dec_time);
		break;
	case DWL_CLIENT_TYPE_PP:
		fprintf(hw_performance_log, "pp;");
		fprintf(hw_performance_log, "%d;", pp_pic_counter);
		fprintf(hw_performance_log, "%d\n", pp_time);
		break;
	}
#endif

#ifdef INTERNAL_TEST
	fclose(dec_dwl->regDump);
	dec_dwl->regDump = NULL;
#endif
	DWLmemset(dec_dwl, 0, sizeof(hX170dwl_t));
	return (DWL_OK);
}

static UINT8 bReceive;
void DWLReceiveIntr(void)
{
	bReceive = 1;
}

i32 DWLWaitDecHwReady(const void *instance, u32 timeout)
{
	while(!bReceive)
	{
#ifdef USE_MCU_FOR_VDEC_VDC
		if(TOP_HAL_GetInterIntrStatus()&(1<<G1DEC))
		{
			TOP_HAL_ClearInterIntrMsk(1<<G1DEC);
			TOP_HAL_DisableInterIntr(G1DEC);
			break;
		}
#else
		if(TOP_HAL_GetExtIntrStatus()&(1<<G1DEC))
		{
			TOP_HAL_ClearExtIntrMsk(1<<G1DEC);
			TOP_HAL_DisableExtIntr(G1DEC);
			break;
		}
#endif
	}

	return DWL_HW_WAIT_OK;
}

#if 0
/*------------------------------------------------------------------------------
    Function name   : DWLWaitDecHwReady
    Description     : Wait until hardware has stopped running
                        and Decoder interrupt comes.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.

    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
i32 DWLWaitDecHwReady(const void *instance, u32 timeout)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

#ifdef _DWL_HW_PERFORMANCE
	u32 offset = 0;
	u32 val = 0;
	struct timeval end_time;
#endif

	DWL_DEBUG("DWLWaitDecHwReady: Start\n");

	/* Check invalid parameters */
	if(dec_dwl == NULL)
	{
		return DWL_HW_WAIT_ERROR;
	}
#if 0
    {
        sigset_t set, oldset;

        sigemptyset(&set);
        sigaddset(&set, SIGIO);

        sigprocmask(SIG_BLOCK, &set, &oldset);

        /* keep looping for the int until it is a decoder int */
        while(!DWL_DECODER_INT)
        {
#if 0
            while(!x170_sig_dec_delivered)
            {
                DWL_DEBUG("DWLWaitDecHwReady: suspend for SIGIO\n");
                sigsuspend(&oldset);
            }

            x170_sig_dec_delivered = 0;
#endif
            if(!DWL_DECODER_INT)
            {
                DWL_DEBUG("DWLWaitDecHwReady: PP IRQ received\n");
            }
        }

//        x170_sig_dec_delivered = 0;
//        sigprocmask(SIG_UNBLOCK, &set, NULL);
    }
#endif

#ifdef _DWL_HW_PERFORMANCE
    /*if (-1 == gettimeofday(&end_time, NULL))
     * assert(0); */

    /* decoder mode is enabled */
    if(dec_start_time.tv_sec)
    {
        offset = HX170DEC_REG_START / 4;
        val = *(dec_dwl->pRegBase + offset);
        if(val & DWL_HW_PIC_RDY_BIT)
        {
            u32 tmp = 0;

            DWL_DEBUG("DWLWaitDecHwReady: decoder pic rdy\n");
            dec_pic_counter++;
            ioctl(dec_dwl->fd, HX170DEC_HW_PERFORMANCE, &end_time);
            tmp =
                (end_time.tv_sec -
                 dec_start_time.tv_sec) * (1 / HW_PERFORMANCE_RESOLUTION);
            tmp +=
                (end_time.tv_usec -
                 dec_start_time.tv_usec) / (1000000 / (1 /
                                                       HW_PERFORMANCE_RESOLUTION));
            dec_time += tmp;
        }
    }
#endif

    DWL_DEBUG("DWLWaitDecHwReady: OK\n");
    return DWL_HW_WAIT_OK;

}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitPpHwReady
    Description     : Wait until hardware has stopped running
                      and pp interrupt comes.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.

    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
i32 DWLWaitPpHwReady(const void *instance, u32 timeout)
{
    hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

#ifdef _DWL_HW_PERFORMANCE
    u32 offset = 0;
    u32 val = 0;
    struct timeval end_time;
#endif
    DWL_DEBUG("DWLWaitPpHwReady: Start\n");

    /* Check invalid parameters */
    if(dec_dwl == NULL)
    {
//        assert(0);
        return DWL_HW_WAIT_ERROR;
    }
#if 0
    {
        sigset_t set, oldset;

        sigemptyset(&set);
        sigaddset(&set, SIGIO);

        sigprocmask(SIG_BLOCK, &set, &oldset);

        /* keep looping for the int until it is a PP int */
        while(!DWL_PP_INT)
        {

            while(!x170_sig_pp_delivered)
            {
                DWL_DEBUG("DWLWaitPpHwReady: suspend for SIGIO\n");
                sigsuspend(&oldset);
            }

            x170_sig_pp_delivered = 0;

            if(!DWL_PP_INT)
            {
                DWL_DEBUG("DWLWaitPpHwReady: DEC IRQ received\n");
            }
        }

        x170_sig_pp_delivered = 0;

        sigprocmask(SIG_UNBLOCK, &set, NULL);
    }
#endif

#ifdef _DWL_HW_PERFORMANCE
    /*if (-1 == gettimeofday(&end_time, NULL))
     * assert(0); */

    /* pp external mode is enabled */
    if(pp_start_time.tv_sec)
    {
        offset = HX170PP_REG_START / 4;
        val = *(dec_dwl->pRegBase + offset);
        if(val & DWL_HW_PIC_RDY_BIT)
        {
            u32 tmp = 0;

            DWL_DEBUG("DWLWaitPpHwReady: pp pic rdy\n");
            pp_pic_counter++;
            ioctl(dec_dwl->fd, HX170DEC_HW_PERFORMANCE, &end_time);
            tmp =
                (end_time.tv_sec -
                 pp_start_time.tv_sec) * (1 / HW_PERFORMANCE_RESOLUTION);
            tmp +=
                (end_time.tv_usec -
                 pp_start_time.tv_usec) / (1000000 / (1 /
                                                      HW_PERFORMANCE_RESOLUTION));
            pp_time += tmp;
        }
    }
#endif

    DWL_DEBUG("DWLWaitPpHwReady: OK\n");
    return DWL_HW_WAIT_OK;

}

/* HW locking */
#endif
/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
------------------------------------------------------------------------------*/
i32 DWLReserveHw(const void *instance)
{
//	i32 ret = 0;
//	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	// check Hardware is Decodable
#if 0
	do
	{
		/* select which semaphore to use */
		if(dec_dwl->clientType == DWL_CLIENT_TYPE_PP)
		{
			DWL_DEBUG("DWL: PP locked by PID %d\n", getpid());
			ret = binary_semaphore_wait(dec_dwl->semid, 1);
		}
		else
		{
			DWL_DEBUG("DWL: Dec locked by PID %d\n", getpid());
			ret = binary_semaphore_wait(dec_dwl->semid, 0);
		}
	}   /* if error is "error, interrupt", try again */
	while(ret != 0);// && errno == EINTR);
#endif
	DWL_DEBUG("DWL: success\n");

//	if(ret)
//		return DWL_ERROR;
//	else

	{
		// Enable Interrupt
#ifdef USE_MCU_FOR_VDEC_VDC
	if(TOP_HAL_IsInterIntrEnable(G1DEC)==0)
		TOP_HAL_EnableInterIntr(G1DEC);
#else
	if(TOP_HAL_IsExtIntrEnable(G1DEC)==0)
		TOP_HAL_EnableExtIntr(G1DEC);
#endif
	bReceive = 0;

#if 0
		sigset_t set;

		sigemptyset(&set);
		sigaddset(&set, SIGIO);

		sigprocmask(SIG_BLOCK, &set, NULL);
		if(dec_dwl->clientType == DWL_CLIENT_TYPE_PP)
		{
		x170_sig_pp_delivered = 0;
		}
		else
		{
		x170_sig_dec_delivered = 0;
		}

		sigprocmask(SIG_UNBLOCK, &set, NULL);
#endif
		return DWL_OK;
	}
}

#ifdef _DWL_HW_PERFORMANCE
void DwlDecoderEnable(void)
{
	DWL_DEBUG("DWL: decoder enabled\n");

	if(-1 == gettimeofday(&dec_start_time, NULL))
		assert(0);
}

void DwlPpEnable(void)
{
	DWL_DEBUG("DWL: pp enabled\n");

	if(-1 == gettimeofday(&pp_start_time, NULL))
		assert(0);
}
#endif
