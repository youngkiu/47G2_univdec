/*
	SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
	Copyright(c) 2010 by LG Electronics Inc.

	This program is free software; you can redistribute it and/or 
	modify it under the terms of the GNU General Public License
	version 2 as published by the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
	GNU General Public License for more details.
*/ 

/** @file
 *
 *  main driver implementation for vdec device.
 *	vdec device will teach you how to make device driver with new platform.
 *
 *  author		seokjoo.lee (seokjoo.lee@lge.com)
 *  version		1.0
 *  date		2009.12.30
 *  note		Additional information.
 *
 *  @addtogroup lg1152_vdec
 *	@{
 */

/*----------------------------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------------------------*/
#undef	SUPPORT_VDEC_DEVICE_READ_WRITE_FOPS
#define	SUPPORT_VDEC_DEVICE_READ_WRITE_FOPS

#define _VDEC_DRV_C_ 1

/*----------------------------------------------------------------------------------------
	File Inclusions
----------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/interrupt.h>    /**< For isr */
#include <linux/irqreturn.h>
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <asm/irq.h>                    /**< For isr */
#ifdef KDRV_CONFIG_PM	// added by SC Jung for quick booting
#include <linux/platform_device.h>
#endif
#include <linux/version.h>
#include <linux/smp.h>

//#include "vdec_mem.h"
#include "os_util.h"
#include "base_dev_cfg.h"
#include "vdec_drv.h"
#include "vdec_io.h"
//#include "vdec_isr.h"
#include "vdec_kapi.h"
#include "vdec_ktop.h"
#include "vdec_dbg.h"

#include "mcu/mcu_config.h"

//#include "../sys/ctop_regs.h"
#include "hal/lg1152/lg1152_vdec_base.h"

#include "mcu/vdec_isr.h"
#include "hal/lg1152/mcu_hal_api.h"

#include "mcu/vdec_print.h"
#include "mcu/ram_log.h"

#ifdef USE_MCU
#include "mcu/mcu_fw.h"
#include "mcu/ipc_req.h"
#include "hal/lg1152/ipc_reg_api.h"
#endif

#ifdef USE_QEMU_SYSTEM
#ifndef IRQ_VDEC
#define IRQ_VDEC        66
#endif
#endif
/*----------------------------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------------------------*/
#define	VDEC_MODULE		"vdec"

/*----------------------------------------------------------------------------------------
	Macro Definitions
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	Type Definitions
----------------------------------------------------------------------------------------*/
#ifdef KDRV_CONFIG_PM
typedef struct
{
	// add here extra parameter
	bool	is_suspended;
}VDEC_DRVDATA_T;
#endif

typedef struct
{
	// BEGIN of common device
	dev_t					devno;			///< device number
	struct cdev				cdev;			///< char device structure
	unsigned int			opened_ch;
	// END of command device
}
VDEC_DEVICE_T;

/*----------------------------------------------------------------------------------------
	External Function Prototype Declarations
----------------------------------------------------------------------------------------*/
extern	void	VDEC_PROC_Init(UINT8 ch, void* data);
extern	void	VDEC_PROC_Cleanup(UINT8 ch);

/*----------------------------------------------------------------------------------------
	External Variables
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
	global Functions
----------------------------------------------------------------------------------------*/
int		VDEC_Init(void);
void	VDEC_Cleanup(void);

/*----------------------------------------------------------------------------------------
	global Variables
----------------------------------------------------------------------------------------*/
int		g_vdec_debug_fd;
int		g_vdec_major = VDEC_MAJOR;
int		g_vdec_minor = VDEC_MINOR;

/*----------------------------------------------------------------------------------------
	Static Function Prototypes Declarations
----------------------------------------------------------------------------------------*/
static int VDEC_Open(struct inode *, struct file *);
static int VDEC_Close(struct inode *, struct file *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int VDEC_Ioctl (struct inode *, struct file *, unsigned int, unsigned long );
#else
static long VDEC_Ioctl (struct file *filp, unsigned int cmd, unsigned long arg );
#endif
static int 		VDEC_Mmap (struct file *filep, struct vm_area_struct *vma );
#ifdef SUPPORT_VDEC_DEVICE_READ_WRITE_FOPS
static ssize_t  VDEC_Read (
			struct file *i_pstrFilp, 				/* file pointer representing driver instance */
			char __user *o_pcBufferToLoad,     	/* buffer from user space */
			size_t      i_uiSizeToRead,    		/* size of buffer in bytes*/
			loff_t 		*i_FileOffset  );   		/* offset in the file */
//static ssize_t  VDEC_Write(struct file *, const char *, size_t, loff_t *);
#endif

/*----------------------------------------------------------------------------------------
	Static Variables
----------------------------------------------------------------------------------------*/

/**
 * main control block for vdec device
*/
static VDEC_DEVICE_T		*g_vdec_device = NULL;

/**
 * file I/O description for vdec device
 *
*/
static struct file_operations g_vdec_fops =
{
	.open 	= VDEC_Open,
	.release= VDEC_Close,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.ioctl= VDEC_Ioctl,
#else
	.unlocked_ioctl= VDEC_Ioctl,
#endif
#ifdef SUPPORT_VDEC_DEVICE_READ_WRITE_FOPS
	.read 	= VDEC_Read,
	.mmap	= VDEC_Mmap,
//	.write 	= VDEC_Write,
#else
//	.write	= NULL,
#endif
};

static irqreturn_t VDEC_irq_handler(int irq, void *dev_id)
{
	VDEC_ISR_Handler();
	return IRQ_HANDLED;
}

static irqreturn_t VDEC_irq2_handler(int irq, void *dev_id)
{
	MCU_ISR_Handler();
	return IRQ_HANDLED;
}

#ifdef USE_MCU
struct timer_list mcu_watchdog;
BOOLEAN		bWatchdogMcuReset = FALSE;
UINT32		ui32McuResetCnt = 0;

static void _VDEC_MCU_Start_Watchdog(unsigned long msec);

void VDEC_MCU_Reset(void)
{
	VDEC_KDRV_Message(ERROR, "[VDEC_DRV][Err] Expired Watchdog Timer to Check MCU Alive, %s", __FUNCTION__ );

	bWatchdogMcuReset = TRUE;
	MCU_HAL_Reset();
	ui32McuResetCnt++;

	IPC_REG_CMD_SetWrOffset(0);
	IPC_REG_CMD_SetRdOffset(0);
}

static void _VDEC_MCU_Expired_Watchdog(unsigned long arg)
{
	if( (unsigned long)IPC_REG_Get_WatchdogCnt() == arg )
	{
		VDEC_MCU_Reset();
		_VDEC_MCU_Start_Watchdog(1000);
	}
	else
	{
		static UINT32	ui32WatchdogAliveCount = 0;

		_VDEC_MCU_Start_Watchdog(200);

		ui32WatchdogAliveCount++;
		if( (ui32WatchdogAliveCount % 0x40) == 0 )
			VDEC_KDRV_Message(MONITOR, "[VDEC_DRV][DBG] MCU Alive Timer Count: %d, MCU Alive Time: 0x%X, ResetCnt: %d, smp:%d", ui32WatchdogAliveCount, IPC_REG_Get_WatchdogCnt(), ui32McuResetCnt, smp_processor_id());
	}
}

static void _VDEC_MCU_Start_Watchdog(unsigned long msec)
{
	init_timer(&mcu_watchdog);
	mcu_watchdog.expires = get_jiffies_64() + (msec*HZ/1000);
	mcu_watchdog.function = _VDEC_MCU_Expired_Watchdog;
	mcu_watchdog.data = (unsigned long)IPC_REG_Get_WatchdogCnt();
	add_timer(&mcu_watchdog);
}

static void _VDEC_MCU_Receive_McuReady(void *arg)
{
	if( bWatchdogMcuReset == TRUE )
	{
		bWatchdogMcuReset = FALSE;
		VDEC_IO_Restart();
	}

	VDEC_KDRV_Message(_TRACE, "[VDEC_DRV][DBG] Recieved MCU_READY Request");
}

static void _VDEC_LoadMcuFw(void)
{
	if(MCU_HAL_IsExtIntrEnable(MCU_ISR_IPC)==0)
		MCU_HAL_EnableExtIntr(MCU_ISR_IPC);

	IPC_REQ_Register_ReceiverCallback(IPC_REQ_ID_MCU_READY, _VDEC_MCU_Receive_McuReady);

	MCU_HAL_InitSystemMemory(VDEC_MCU_SROM_BASE, VDEC_MCU_SRAM0_BASE, VDEC_MCU_SRAM1_BASE, VDEC_MCU_SRAM2_BASE);
	MCU_HAL_CodeDown(VDEC_MCU_SROM_BASE, VDEC_MCU_SROM_SIZE, aui8VdecCodec, sizeof(aui8VdecCodec));

	if(MCU_HAL_IsInterIntrEnable(MCU_ISR_IPC)==0)
		MCU_HAL_EnableInterIntr(MCU_ISR_IPC);

	_VDEC_MCU_Start_Watchdog(1000);
}
#endif


/*========================================================================================
	Implementation Group
========================================================================================*/
#ifdef KDRV_CONFIG_PM	// added by SC Jung for quick booting
/**
 *
 * suspending module.
 *
 * @param	struct platform_device *pdev pm_message_t state
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int VDEC_suspend(struct platform_device *pdev, pm_message_t state)
{
	VDEC_DRVDATA_T *drv_data;

	drv_data = platform_get_drvdata(pdev);

	// add here the suspend code
	VDEC_KTOP_CloseAll();

	if(MCU_HAL_IsExtIntrEnable(MCU_ISR_IPC)==1)
		MCU_HAL_DisableExtIntr(MCU_ISR_IPC);

//	gbIsLoadedVdecFw = FALSE;
	MCU_HAL_SegRunStall(1);

	drv_data->is_suspended = 1;
	VDEC_KDRV_Message(ERROR, "[VDEC] %s", __FUNCTION__);

	return 0;
}


/**
 *
 * resuming module.
 *
 * @param	struct platform_device *
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int VDEC_resume(struct platform_device *pdev)
{
	VDEC_DRVDATA_T *drv_data;

	drv_data = platform_get_drvdata(pdev);

	if(drv_data->is_suspended == 0) return -1;

	// add here the resume code
	Vdec_IO_Init();
	MSVC_ADP_Reset(0);
	MSVC_ADP_Reset(MSVC_NUM_OF_CHANNEL);
//	_VDEC_LoadMcuFw();
	MCU_HAL_Reset();

	drv_data->is_suspended = 0;
	VDEC_KDRV_Message(ERROR, "[VDEC] %s", __FUNCTION__);
	return 0;
}
/**
 *
 * probing module.
 *
 * @param	struct platform_device *pdev
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int __init VDEC_probe(struct platform_device *pdev)
{

	VDEC_DRVDATA_T *drv_data;

	drv_data = (VDEC_DRVDATA_T *)kmalloc(sizeof(VDEC_DRVDATA_T) , GFP_KERNEL);


	// add here driver registering code & allocating resource code


	VDEC_KDRV_Message(ERROR, "[VDEC] %s", __FUNCTION__);
	drv_data->is_suspended = 0;
	platform_set_drvdata(pdev, drv_data);

	return 0;
}


/**
 *
 * module remove function. this function will be called in rmmod fbdev module.
 *
 * @param	struct platform_device
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int  VDEC_remove(struct platform_device *pdev)
{
	VDEC_DRVDATA_T *drv_data;

	// add here driver unregistering code & deallocating resource code



	drv_data = platform_get_drvdata(pdev);
	kfree(drv_data);

	VDEC_KDRV_Message(ERROR, "[VDEC] %s", __FUNCTION__);

	return 0;
}

/**
 *
 * module release function. this function will be called in rmmod module.
 *
 * @param	struct device *dev
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static void  VDEC_release(struct device *dev)
{
	VDEC_KDRV_Message(ERROR, "[VDEC] %s", __FUNCTION__);
}

/*
 *	module platform driver structure
 */
static struct platform_driver vdec_driver =
{
	.probe          = VDEC_probe,
	.suspend        = VDEC_suspend,
	.remove         = VDEC_remove,
	.resume         = VDEC_resume,
	.driver         =
	{
		.name   = VDEC_MODULE,
	},
};

static struct platform_device vdec_device = {
	.name = VDEC_MODULE,
	.id = 0,
	.id = -1,
	.dev = {
		.release = VDEC_release,
	},
};
#endif

int VDEC_Init(void)
{
	int			i;
	int			err;
	dev_t		dev;
	unsigned long	dev_sz;

	/* Get the handle of debug output for vdec device.
	 *
	 * Most module should open debug handle before the real initialization of module.
	 * As you know, debug_util offers 4 independent debug outputs for your device driver.
	 * So if you want to use all the debug outputs, you should initialize each debug output
	 * using OS_DEBUG_EnableModuleByIndex() function.
	 */
	g_vdec_debug_fd = DBG_OPEN( VDEC_MODULE );
	if(g_vdec_debug_fd>=0)
	{
		OS_DEBUG_EnableModule ( g_vdec_debug_fd );
		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, ERROR, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, WARNING, DBG_COLOR_NONE );
		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_SYS, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, _TRACE, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, MONITOR, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_IPC, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_PDEC, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_FEED, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_VDC, DBG_COLOR_NONE );
//		OS_DEBUG_EnableModuleByIndex ( g_vdec_debug_fd, DBG_DEIF, DBG_COLOR_NONE );
	}
	else
	{
        printk("[vdec] can't get debug handle\n");
        g_vdec_debug_fd = g_global_debug_fd;
	}
//	VDEC_INIT_ProbeConfig();

	/* allocate main device handler, register current device.
	 *
	 * If devie major is predefined then register device using that number.
	 * otherwise, major number of device is automatically assigned by Linux kernel.
	 *
	 */
#ifdef KDRV_CONFIG_PM
	// added by SC Jung for quick booting
	if(platform_driver_register(&vdec_driver) < 0)
	{
		VDEC_KDRV_Message(ERROR, "[VDEC] platform driver register failed, %s", __FUNCTION__);
	}
	else
	{
		if(platform_device_register(&vdec_device))
		{
			platform_driver_unregister(&vdec_driver);
			VDEC_KDRV_Message(ERROR, "[VDEC] platform device register failed, %s", __FUNCTION__);
		}
		else
		{
			VDEC_KDRV_Message(ERROR, "[VDEC] platform register done, %s", __FUNCTION__);
		}
	}
#endif

	dev_sz = sizeof(VDEC_DEVICE_T)*VDEC_NUM_OF_CHANNEL;

#define GUARD_ON_G_VDEC

	#ifdef GUARD_ON_G_VDEC
	dev_sz += PAGE_SIZE * 2;
	#endif

	g_vdec_device = (VDEC_DEVICE_T*)OS_KMalloc( dev_sz);

	#ifdef GUARD_ON_G_VDEC
	g_vdec_device = (VDEC_DEVICE_T*)( ((UINT32)g_vdec_device) + PAGE_SIZE );
	#endif


	if ( NULL == g_vdec_device )
	{
		DBG_PRINT_ERROR("out of memory. can't allocate %d bytes\n", sizeof(VDEC_DEVICE_T)*VDEC_NUM_OF_CHANNEL);
		return -ENOMEM;
	}

	memset( g_vdec_device, 0x0, sizeof(VDEC_DEVICE_T)*VDEC_NUM_OF_CHANNEL);

	if (g_vdec_major)
	{
		dev = MKDEV( g_vdec_major, g_vdec_minor );
		err = register_chrdev_region(dev, VDEC_NUM_OF_CHANNEL, VDEC_MODULE );
	}
	else
	{
		err = alloc_chrdev_region(&dev, g_vdec_minor, VDEC_NUM_OF_CHANNEL, VDEC_MODULE);
		g_vdec_major = MAJOR(dev);
	}

	VDEC_KDRV_Message(ERROR, "[VDEC] %s:%d:, %p, sz, %08x", __FUNCTION__, __LINE__, g_vdec_device, sizeof(VDEC_DEVICE_T));

	if ( err < 0 )
	{
		DBG_PRINT_ERROR("can't register vdec device\n" );
		return -EIO;
	}

	/* TODO : initialize your module not specific minor device */
	VDEC_DBG_EVCNT_Init(VDEC_NUM_OF_CHANNEL);

	for ( i=0; i<VDEC_NUM_OF_CHANNEL; i++ )
	{
		/* initialize cdev structure with predefined variable */
		dev = MKDEV( g_vdec_major, g_vdec_minor+i );

		cdev_init( &(g_vdec_device[i].cdev), &g_vdec_fops );

		g_vdec_device[i].devno			= dev;
		g_vdec_device[i].cdev.owner		= THIS_MODULE;
		g_vdec_device[i].cdev.ops		= &g_vdec_fops;

		err = cdev_add (&(g_vdec_device[i].cdev), dev, 1 );

		if (err)
		{
			DBG_PRINT_ERROR("error (%d) while adding vdec device (%d.%d)\n", err, MAJOR(dev), MINOR(dev) );
			return -EIO;
		}

#ifndef DEFINE_EVALUATION
		OS_CreateDeviceClass(g_vdec_device[i].devno, "%s%d", VDEC_MODULE, i);
#endif

		/* initialize proc system */
		VDEC_PROC_Init (i, &g_vdec_device[i] );
	}

	VDEC_IO_Init();
#ifdef USE_MCU
	_VDEC_LoadMcuFw();
#endif

	err = request_irq(VDEC_MAIN_ISR_HANDLE, (irq_handler_t)VDEC_irq_handler,0,"DTVSoC2VDEC ", &g_vdec_device[0]);
	if (err)
	{
		VDEC_KDRV_Message(DBG_SYS, "request_irq in VDEC(%d) is failed:[err : %d]", VDEC_MAIN_ISR_HANDLE, err);
		return -EIO;
	}

	err = request_irq(VDEC_MCU_ISR_HANDLE, (irq_handler_t)VDEC_irq2_handler,0,"L9VDECISR", &g_vdec_device[0]);
	if (err)
	{
		VDEC_KDRV_Message(DBG_SYS, "request_irq in MCU(%d) is failed:[err : %d]", VDEC_MCU_ISR_HANDLE, err);
		return -EIO;
	}

	VDEC_KDRV_Message(ERROR, "[VDEC] vdec device initialized :: VDEC_IRQ(%d), MCU_IRQ(%d)", VDEC_MAIN_ISR_HANDLE, VDEC_MCU_ISR_HANDLE);

//	CTOP_CTRL_L9_Wr01(ctr29, jtag_sel, 	0x0);	// setting JTAG for VDEC
	return 0;
}

void VDEC_Cleanup(void)
{
	int i;
	dev_t dev = MKDEV( g_vdec_major, g_vdec_minor );

#ifdef KDRV_CONFIG_PM
	// added by SC Jung for quick booting
	platform_driver_unregister(&vdec_driver);
	platform_device_unregister(&vdec_device);
#endif


	/* remove all minor devicies and unregister current device */
	for ( i=0; i<VDEC_NUM_OF_CHANNEL;i++)
	{
		VDEC_PROC_Cleanup( i );
		cdev_del( &(g_vdec_device[i].cdev) );
	}

	MCU_HAL_DisableExtIntrAll();
	MCU_HAL_DisableInterIntrAll();

	free_irq(VDEC_MAIN_ISR_HANDLE, &g_vdec_device[0]);
	free_irq(VDEC_MCU_ISR_HANDLE, &g_vdec_device[0]);

	/* TODO : cleanup your module not specific minor device */

	unregister_chrdev_region(dev, VDEC_NUM_OF_CHANNEL);

	#ifdef GUARD_ON_G_VDEC
	g_vdec_device = (VDEC_DEVICE_T*)( ((UINT32)g_vdec_device) - PAGE_SIZE );
	#endif
	OS_Free( g_vdec_device );

	VDEC_KDRV_Message(ERROR, "[VDEC] device closed (%d:%d)", g_vdec_major, g_vdec_minor );
}


///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * open handler for vdec device
 *
 */
static int VDEC_Open(struct inode *inode, struct file *filp)
{
	int					major,minor;
	struct cdev*    	cdev;
	VDEC_DEVICE_T*		my_dev;
	VDEC_DEVICE_T*		pVdec;

	major = imajor(inode);
	minor = iminor(inode);

	VDEC_KDRV_Message(ERROR, "[VDEC] device opened (%d:%d) by client pid/tgid %d %d", major, minor, task_pid_nr(current), task_tgid_nr(current) );

	cdev	= inode->i_cdev;
	my_dev	= container_of ( cdev, VDEC_DEVICE_T, cdev);

	/* TODO : add your device specific code */

//	_VDEC_LoadMcuFw();

	/* END */

	pVdec = (VDEC_DEVICE_T*)OS_KMalloc( sizeof(VDEC_DEVICE_T) );

	memcpy(pVdec, my_dev, sizeof(VDEC_DEVICE_T));
	pVdec->opened_ch = 0x0;

	filp->private_data = pVdec;

	// TODO per channel initialization.

	return 0;
}

/**
 * release handler for vdec device
 *
 */
static int VDEC_Close(struct inode *inode, struct file *filp)
{
	int					major,minor;
	VDEC_DEVICE_T*		my_dev;
	VDEC_DEVICE_T*		pVdec;
	struct cdev*		cdev;
	int					ch;

	major = imajor(inode);
	minor = iminor(inode);

	VDEC_KDRV_Message(ERROR, "[VDEC] device closed (%d:%d) by client pid/tgid %d %d", major, minor, task_pid_nr(current), task_tgid_nr(current) );

	cdev	= inode->i_cdev;
	my_dev	= container_of(cdev, VDEC_DEVICE_T, cdev);

	pVdec = (VDEC_DEVICE_T*)filp->private_data;

	if( pVdec->opened_ch )
		VDEC_KDRV_Message(ERROR, "[VDEC] Opened Channels:0x%x", pVdec->opened_ch );

	for( ch = 0; ch < VDEC_NUM_OF_CHANNEL; ch++ )
	{
		if( pVdec->opened_ch & (1 << ch) )
		{
			VDEC_NOTI_EnableCallback(ch, FALSE, FALSE, FALSE, FALSE);
			VDEC_KTOP_Close(ch);
		}
	}

	filp->private_data = NULL;

	OS_Free( pVdec );

	/* TODO : add your device specific code */

	/* END */

	return 0;
}

/**
 * ioctl handler for vdec device.
 *
 *
 * note: if you have some critial data, you should protect them using semaphore or spin lock.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int VDEC_Ioctl ( struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg )
#else
static long VDEC_Ioctl (struct file *filp, unsigned int cmd, unsigned long arg )
#endif
{
	int err = 0, ret = 0;

	VDEC_DEVICE_T*		my_dev;
	VDEC_DEVICE_T*		pVdec;
	struct cdev*		cdev;
	UINT8	ch;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	struct inode *inode = filp->f_path.dentry->d_inode;
#endif

	/*
	 * get current vdec device object
	 */
	cdev	= inode->i_cdev;
	my_dev	= container_of ( cdev, VDEC_DEVICE_T, cdev);

	/*
	 * check if IOCTL command is valid or not.
	 * - if magic value doesn't match, return error (-ENOTTY)
	 * - if command is out of range, return error (-ENOTTY)
	 *
	 * note) -ENOTTY means "Inappropriate ioctl for device.
	 */
	if (_IOC_TYPE(cmd) != VDEC_KDRV_IOC_MAGIC)
	{
		DBG_PRINT_WARNING("invalid magic. magic=0x%02X\n", _IOC_TYPE(cmd) );
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > VDEC_KDRV_IOC_MAXNR)
	{
		DBG_PRINT_WARNING("out of ioctl command. cmd_idx=%d\n", _IOC_NR(cmd) );
		return -ENOTTY;
	}

	/* TODO : add some check routine for your device */

	/*
	 * check if user memory is valid or not.
	 * if memory can't be accessed from kernel, return error (-EFAULT)
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
	{
		DBG_PRINT_WARNING("memory access error. cmd_idx=%d, rw=%c%c, memptr=%p\n",
													_IOC_NR(cmd),
													(_IOC_DIR(cmd) & _IOC_READ)? 'r':'-',
													(_IOC_DIR(cmd) & _IOC_WRITE)? 'w':'-',
													(void*)arg );
		return -EFAULT;
	}

	if (!my_dev)				{ return INVALID_PARAMS;}

	ch = MINOR(my_dev->devno);

//	VDEC_KDRV_Message(DBG_SYS, "cmd = %08X (cmd_idx=%d)\n", cmd, _IOC_NR(cmd) );

	switch(cmd)
	{
		case VDEC_KDRV_IO_OPEN_PARAM:
		{
			if( (ret = VDEC_IO_OpenChannel(arg)) < 0 )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_OPEN_PARAM failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
			{
				pVdec = (VDEC_DEVICE_T*)filp->private_data;
				pVdec->opened_ch |= (1 << ret);
				VDEC_KDRV_Message(ERROR, "[VDEC] Opened Channels:0x%x by client pid/tgid %d %d, %s(%d)", pVdec->opened_ch, task_pid_nr(current), task_tgid_nr(current), __FUNCTION__, __LINE__ );
				ret = 0;
			}
		}
		break;

		case VDEC_KDRV_IO_CLOSE_PARAM:
		{
			if( (ret = VDEC_IO_CloseChannel(arg)) < 0 )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_CLOSE_PARAM failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
			{
				pVdec = (VDEC_DEVICE_T*)filp->private_data;
				pVdec->opened_ch &= ~(1 << ret);
				VDEC_KDRV_Message(ERROR, "[VDEC] Opened Channels:0x%x by client pid/tgid %d %d, %s(%d)", pVdec->opened_ch, task_pid_nr(current), task_tgid_nr(current), __FUNCTION__, __LINE__ );
				ret = 0;
			}
		}
		break;

		case VDEC_KDRV_IO_PLAY_PARAM:
		{
			if( (ret = VDEC_IO_PlayChannel(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_PLAY_PARAM failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_FLUSH_BUFFER:
		{
			if( (ret = VDEC_IO_FlushChannel(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_FLUSH_BUFFER failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_ENABLE_CALLBACK:
		{
			if( (ret = VDEC_IO_EnableCallback(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_ENABLE_CALLBACK failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_CALLBACK_NOTI:
		{
			if( (ret = VDEC_IO_CallbackNoti(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_CALLBACK_NOTI failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_DEC_SEQH:
		{
			if( (ret = VDEC_IO_GetSeqh(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_DEC_SEQH failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_DEC_PICD:
		{
			if( (ret = VDEC_IO_GetPicd(arg)) )
			{
				VDEC_KDRV_Message(_TRACE, "VDEC_KDRV_IO_GET_DEC_PICD failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_DISPLAY:
		{
			if( (ret = VDEC_IO_GetDisp(arg)) )
			{
				VDEC_KDRV_Message(_TRACE, "VDEC_KDRV_IO_GET_DISPLAY failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_RUNNING_FB:
		{
			if( (ret = VDEC_IO_GetRunningFb(arg)) )
			{
				VDEC_KDRV_Message(_TRACE, "VDEC_KDRV_IO_GET_RUNNING_FB failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_LATEST_DEC_UID:
		{
			if( (ret = VDEC_IO_GetLatestDecUID(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_LATEST_DEC_UID failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_LATEST_DISP_UID:
		{
			if( (ret = VDEC_IO_GetLatestDispUID(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_LATEST_DISP_UID failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_DISPLAY_FPS:
		{
			if( (ret = VDEC_IO_GetDisplayFps(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_DISPLAY_FPS failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_DROP_FPS:
		{
			if( (ret = VDEC_IO_GetDropFps(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_DROP_FPS failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_SET_BASETIME:
		{
			if( (ret = VDEC_IO_SetBaseTime(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_SET_BASETIME failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_BASETIME:
		{
			if( (ret = VDEC_IO_GetBaseTime(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_BASETIME failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_CTRL_USRD:
		{
			if( (ret = VDEC_IO_CtrlUsrd(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_CTRL_USRD failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_SET_FREE_DPB:
		{
			if( (ret = VDEC_IO_SetFreeDPB(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_SET_FREE_DPB failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_RECONFIG_DPB:
		{
			if( (ret = VDEC_IO_ReconfigDPB(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_RECONFIG_DPB failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_OPEN_CH:
		{
			if( (ret = VDEC_InitChannel(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_OPEN_CH failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
			{
				pVdec = (VDEC_DEVICE_T*)filp->private_data;
				pVdec->opened_ch |= (1 << ch);
				VDEC_KDRV_Message(DBG_SYS, "[VDEC] Opened Channels:0x%x by client pid/tgid %d %d, %s(%d)", pVdec->opened_ch, task_pid_nr(current), task_tgid_nr(current), __FUNCTION__, __LINE__ );
				ret = 0;
			}
		}
		break;

		case VDEC_KDRV_IO_CLOSE_CH:
		{
			if( (ret = VDEC_CloseChannel(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_CLOSE_CH failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
			{
				pVdec = (VDEC_DEVICE_T*)filp->private_data;
				pVdec->opened_ch &= ~(1 << ch);
				VDEC_KDRV_Message(DBG_SYS, "[VDEC] Opened Channels:0x%x by client pid/tgid %d %d, %s(%d)", pVdec->opened_ch, task_pid_nr(current), task_tgid_nr(current), __FUNCTION__, __LINE__ );
				ret = 0;
			}
		}
		break;

		case VDEC_KDRV_IO_PLAY:
		{
			if( (ret = VDEC_StartChannel(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_PLAY failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_PLAY_SET:
		{
			if( (ret = VDEC_IO_SetPLAYOption(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_PLAY_SET failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_PLAY_GET:
		{
			if( (ret = VDEC_IO_GetPLAYOption(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_PLAY_GET failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_FLUSH:
		{
			if( (ret = VDEC_FlushChannel(ch, 0)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_FLUSH failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_SET_BUFFER_LVL:
		{
			if( (ret = VDEC_IO_SetBufferLVL(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_SetBufferLVL failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_BUFFER_STATUS:
		{
			if( (ret = VDEC_IO_GetBufferStatus(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_KDRV_IO_GET_BUFFER_STATUS failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_UPDATE_BUFFER:
		{
			if( (ret = VDEC_IO_UpdateBuffer(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_UpdateBuffer failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_UPDATE_GRAPBUF:
		{
			if( (ret = VDEC_IO_UpdateGraphicBuffer(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_UpdateGraphicBuffer failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_OUTPUT:
		{
			if( (ret = VDEC_IO_GetOutput(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_GetOutput failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_VERSION:
		{
			if( (ret = VDEC_IO_GetFiemwareVersion(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_GetFiemwareVersion failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_SET_REG:
		{
			if( (ret = VDEC_IO_SetRegister(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_SetRegister failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_REG:
		{
			if( (ret = VDEC_IO_GetRegister(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_GetRegister failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_ENABLELOG:
		{
			if( (ret = VDEC_IO_EnableLog(arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_EnableLog failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_DBG_CMD:
		{
			if( (ret = VDEC_IO_DbgCmd(ch, arg)) )
			{
				VDEC_KDRV_Message(ERROR, "VDEC_IO_DbgCmd failed:[%d]", ret);

				ret = -ENOTTY;
			}
			else
				ret = 0;
		}
		break;

		case VDEC_KDRV_IO_GET_MEM_LOG:
		{
			VDEC_KDRV_GET_MEM_LOG_T	mem_log;

			if (copy_from_user(&mem_log, (void*)arg, sizeof(VDEC_KDRV_GET_MEM_LOG_T))) return -EFAULT;

			switch(mem_log.mem_type)
			{
				case VDEC_KDRV_MEM_BUFF_PDEC:
					{
						mem_log.size = RAM_LOG_Read(RAM_LOG_MOD_VES, (UINT32*)mem_log.buff_ptr, mem_log.size);
					}
					break;
				case VDEC_KDRV_MEM_BUFF_LQ:
					{
						mem_log.size = RAM_LOG_Read(RAM_LOG_MOD_VDC, (UINT32*)mem_log.buff_ptr, mem_log.size);
					}
					break;
				case VDEC_KDRV_MEM_BUFF_MSVC_CMD:
					{
						mem_log.size = RAM_LOG_Read(RAM_LOG_MOD_MSVC_CMD, (UINT32*)mem_log.buff_ptr, mem_log.size);
					}
					break;
				case VDEC_KDRV_MEM_BUFF_MSVC_RSP:
					{
						mem_log.size = RAM_LOG_Read(RAM_LOG_MOD_MSVC_RSP, (UINT32*)mem_log.buff_ptr, mem_log.size);
					}
					break;
				case VDEC_KDRV_MEM_BUFF_DQ:
					{
						mem_log.size = RAM_LOG_Read(RAM_LOG_MOD_DQ, (UINT32*)mem_log.buff_ptr, mem_log.size);
					}
					break;
				default:
					ret = -ENOTTY;
					break;
			}

			if (ret == 0)
			{
				if (copy_to_user((void*)arg, &mem_log, sizeof(VDEC_KDRV_GET_MEM_LOG_T)))
				{
					ret = -EFAULT;
				}
			}
		}
		break;
		default:
		{
			/* redundant check but it seems more readable */
			ret = -ENOTTY;
		}
		break;
	}

	return ret;
}

static int 	VDEC_Mmap (struct file *filep, struct vm_area_struct *vma )
{
	ULONG phy_start,phy_end;
	ULONG offset= vma->vm_pgoff << PAGE_SHIFT;
	ULONG size	= vma->vm_end - vma->vm_start;
	ULONG end   = PAGE_ALIGN(offset + size);

	if (size & (PAGE_SIZE-1)) return -EINVAL;

	VDEC_KDRV_Message(DBG_SYS, "mmap: vma : %08lx %08lx %08lx", vma->vm_start, vma->vm_pgoff<<PAGE_SHIFT, vma->vm_end);

	// check offset is allowed range or not.
	phy_start = VDEC_CPB_BASE & PAGE_MASK;
	phy_end   = PAGE_ALIGN( phy_start + VDEC_CPB_SIZE);
	if ( phy_start <= offset && end <= phy_end ) goto allowed;

	phy_start = VDEC_DPB_BASE & PAGE_MASK;
	phy_end   = PAGE_ALIGN( phy_start + VDEC_DPB_SIZE);
	if ( phy_start <= offset && end <= phy_end ) goto allowed;

	phy_start = VDEC_MSVC_CORE0_BASE & PAGE_MASK;
	phy_end   = PAGE_ALIGN( phy_start + VDEC_MSVC_SIZE*2);
	if ( phy_start <= offset && end <= phy_end ) goto allowed;

	return -EINVAL;

allowed:
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static ssize_t VDEC_Read(
			struct file *i_pstrFilp, 				/* file pointer representing driver instance */
			char __user *o_pcBufferToLoad,     	/* buffer from user space */
			size_t      i_uiSizeToRead,    		/* size of buffer in bytes*/
			loff_t 		*i_FileOffset  )  		/* offset in the file */
{
	VDEC_DEVICE_T*	pVdec;
	UINT8 ui8Ch;
	signed long	timeout = HZ /10;

	pVdec = (VDEC_DEVICE_T*)i_pstrFilp->private_data;

	if (!pVdec) return -EIO;

	ui8Ch = MINOR(pVdec->devno);

	VDEC_DBG_CheckCallerProcess(ui8Ch);

	VDEC_NOTI_GarbageCollect(ui8Ch);

	timeout  = VDEC_NOTI_WaitTimeout(ui8Ch, timeout);

	if ( !timeout )
	{
		VDEC_KDRV_NOTIFY_PARAM_RTIMEOUT_T	pParam[1];
		VDEC_NOTI_SaveRTIMEOUT(ui8Ch, pParam);
	}

	return VDEC_NOTI_CopyToUser(ui8Ch, o_pcBufferToLoad, i_uiSizeToRead);
}


/* { proc entry for lg/vdec/fpq */
static int vdec_fpq_proc_show(struct seq_file *m, void *v)
{
	VDEC_IO_DBG_DumpFPQTS(m);
	return 0;
}

static int vdec_fpq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, vdec_fpq_proc_show, NULL);
}

const struct file_operations vdec_fpq_proc_fops = {
	.open		= vdec_fpq_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
/* } end of proc entry for lg/vdec/fpq */

/* { proc entry for lg/vdec#/frame */

static int vdec_frame_proc_show(struct seq_file *sq, void *v)
{
	return 0;
}

static void* vdec_proc_seq_start(struct seq_file *p, loff_t *pos)	{ return NULL+(*pos==0); }
static void* vdec_proc_seq_next(struct seq_file *p, void* v, loff_t *pos)	{ ++*pos; return NULL; }
static void  vdec_proc_seq_stop(struct seq_file *p, void* v)		{}

const static struct seq_operations vdec_frame_proc_seq_ops =
{
	.start = vdec_proc_seq_start,
	.next  = vdec_proc_seq_next,
	.stop  = vdec_proc_seq_stop,
	.show  = vdec_frame_proc_show
};

static int vdec_frame_proc_open(struct inode *inode, struct file *file);

const struct file_operations vdec_frame_proc_fops =
{
	.open		= vdec_frame_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int vdec_frame_proc_open(struct inode *inode, struct file *file)
{
	struct seq_file *sq;
	int result ;
	result = seq_open(file, (const struct seq_operations*)&vdec_frame_proc_seq_ops);
	if ( result < 0 ) return result;
	sq = (struct seq_file *) file->private_data;
	sq->private = PROC_I(inode)->pde->data;
	return result;
}
/* } end of proc entry for lg/vdec/frame */


///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef KDRV_GLOBAL_LINK
#if defined(CONFIG_LG_BUILTIN_KDRIVER) && defined(CONFIG_LGSNAP)
user_initcall_grp("kdrv",VDEC_Init);
#else
module_init(VDEC_Init);
#endif
module_exit(VDEC_Cleanup);

//MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("vdec driver");
MODULE_LICENSE("GPL");
#endif

/** @} */

