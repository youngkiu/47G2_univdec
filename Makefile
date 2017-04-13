
# SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
# Copyright(c) 2010 by LG Electronics Inc.

# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# GNU General Public License for more details.
 

#   ---------------------------------------------------------------------------#
#                                                                              #
#	FILE NAME	:	makefile                                                   #
#	VERSION		:	1.0                                                        #
#	AUTHOR		:	seokjoo.lee (seokjoo.lee@lge.com)
#	DATE        :	2009.12.30
#	DESCRIPTION	:	Makefile for building vdec module                      #
#******************************************************************************#
ifeq ($(KDRV_GLOBAL_LINK), YES)
include $(KDRV_TOP_DIR)/modules.mk

vdec_MODULE			:= $(UNIVDEC)
else
# for L9 Evaluation environment.
MODULE_NAME = kdrv_vdec
obj-m := $(MODULE_NAME).o
endif

#-------------------------------------------------------------------------------
# TODO: define your driver source file
#-------------------------------------------------------------------------------
kdrv_vdec-objs		:= vdec_cfg.o
kdrv_vdec-objs		+= vdec_drv.o
kdrv_vdec-objs		+= vdec_proc.o
kdrv_vdec-objs		+= vdec_noti.o
kdrv_vdec-objs		+= vdec_dbg.o
kdrv_vdec-objs		+= vdec_io.o
kdrv_vdec-objs		+= vdec_ktop.o
kdrv_vdec-objs		+= vdec_cpb.o
kdrv_vdec-objs		+= vdec_dpb.o
kdrv_vdec-objs		+= vdec_ion.o

kdrv_vdec-objs		+= hal/lg1152/ipc_reg_api.o
kdrv_vdec-objs		+= hal/lg1152/pdec_hal_api.o
kdrv_vdec-objs		+= hal/lg1152/de_ipc_hal_api.o

kdrv_vdec-objs		+= hal/lg1152/top_hal_api.o
kdrv_vdec-objs		+= hal/lg1152/vsync_hal_api.o
kdrv_vdec-objs		+= hal/lg1152/lq_hal_api.o
kdrv_vdec-objs		+= hal/lg1152/msvc_hal_api.o
kdrv_vdec-objs		+= hal/lg1152/mcu_hal_api.o

kdrv_vdec-objs		+= hal/lg1152/av_lipsync_hal_api.o

kdrv_vdec-objs		+= mcu/ram_log.o

kdrv_vdec-objs		+= ves/ves_auib.o
kdrv_vdec-objs		+= ves/ves_cpb.o
kdrv_vdec-objs		+= ves/ves_drv.o

kdrv_vdec-objs		+= mcu/vdec_isr.o

kdrv_vdec-objs		+= vds/disp_q.o
kdrv_vdec-objs		+= vds/sync_drv.o
kdrv_vdec-objs		+= vds/vdec_rate.o
kdrv_vdec-objs		+= vds/pts_drv.o
kdrv_vdec-objs		+= vds/vsync_drv.o
kdrv_vdec-objs		+= vds/de_if_drv.o

kdrv_vdec-objs		+= mcu/vdec_shm.o

kdrv_vdec-objs		+= mcu/ipc_cmd.o
kdrv_vdec-objs		+= mcu/ipc_req.o
kdrv_vdec-objs		+= mcu/ipc_dbg.o
kdrv_vdec-objs		+= mcu/mutex_ipc.o

kdrv_vdec-objs		+= vdc/vdc_auq.o
kdrv_vdec-objs		+= vdc/vdc_feed.o
kdrv_vdec-objs		+= vdc/vdec_stc_timer.o
kdrv_vdec-objs		+= vdc/msvc_adp.o
kdrv_vdec-objs		+= vdc/msvc_drv.o
kdrv_vdec-objs		+= vdc/vp8_drv.o
kdrv_vdec-objs		+= vdc/vdc_drv.o

kdrv_vdec-objs		+= hal/lg1152/vp8/common/bqueue.o
kdrv_vdec-objs		+= hal/lg1152/vp8/common/refbuffer.o
kdrv_vdec-objs		+= hal/lg1152/vp8/common/regdrv.o
kdrv_vdec-objs		+= hal/lg1152/vp8/common/tiledref.o
kdrv_vdec-objs		+= hal/lg1152/vp8/common/workaround.o

kdrv_vdec-objs		+= hal/lg1152/vp8/dwl/dwl_linux.o
kdrv_vdec-objs		+= hal/lg1152/vp8/dwl/dwl_x170_linux_irq.o

kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8decapi.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_asic.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_bool.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_decoder.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_headers.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_pp_pipeline.o
kdrv_vdec-objs		+= hal/lg1152/vp8/vp8/vp8hwd_probs.o

ifeq ($(KDRV_GLOBAL_LINK), YES)
# KDRV_MODULE_OBJS is used for KDRV_GLOBAL_LINK mode
KDRV_MODULE_OBJS	+= $(addprefix $(vdec_MODULE)/,$(kdrv_vdec-objs))
else
# for L9 Evaluation environment.
$(MODULE_NAME)-objs := $(kdrv_vdec-objs)
obj-m				:= $(MODULE_NAME).o
endif
#-------------------------------------------------------------------------------
# TODO: define your driver specific CFLAGS
#-------------------------------------------------------------------------------
kdrv_vdec-CFLAGS	+=
kdrv_vdec-CFLAGS	+=
kdrv_vdec-CFLAGS	+=

#-------------------------------------------------------------------------------
# DO NOT change the below part
#-------------------------------------------------------------------------------
EXTRA_CFLAGS		+= $(kdrv_vdec-CFLAGS)

