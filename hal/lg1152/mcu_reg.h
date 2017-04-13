/* Written by anissa <anissa@lge.com> */

#ifndef __REG_MCU_H__
#define __REG_MCU_H__

#include "../../mcu/vdec_type_defs.h"
#include "lg1152_vdec_base.h"

#define VDEC_MCU_REG_BASE				(LG1152_VDEC_BASE + 0x700)


typedef union {
   struct {
      UINT32 runstall        :  1;
      UINT32 vector_sel      :  1;
      UINT32 pdbg_en         :  1;
      UINT32                 : 25;
      UINT32 sw_reset        :  1;
      UINT32                 :  3;
      UINT32 prid            : 16;
      UINT32 pdbg_st         :  8;
      UINT32                 :  8;
      UINT32 pdbg_data       : 32;
      UINT32 pdbg_pc         : 32;
      UINT32                 : 12;
      UINT32 srom_offset     : 20;
      UINT32                 : 12;
      UINT32 sram_offset_0   : 20;
      UINT32                 : 12;
      UINT32 sram_offset_1   : 20;
      UINT32                 : 12;
      UINT32 sram_offset_2   : 20;
      UINT32 awuser          : 12;
      UINT32                 :  4;
      UINT32 aruser          : 12;
      UINT32                 :  4;
   } f;
   UINT32 w[9];
} sys_ctrl_t;
typedef union {
   struct {
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32 dma             :  1;
      UINT32 ipc             :  1;
      UINT32                 : 21;
   } f;
   UINT32 w;
} intr_en_t;
typedef union {
   struct {
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32 dma             :  1;
      UINT32 ipc             :  1;
      UINT32                 : 21;
   } f;
   UINT32 w;
} intr_st_t;
typedef union {
   struct {
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32                 :  1;
      UINT32 dma             :  1;
      UINT32 ipc             :  1;
      UINT32                 : 21;
   } f;
   UINT32 w;
} intr_cl_t;
typedef union {
   struct {
      UINT32 ipc		:  1;
      UINT32			: 31;
   } f;
   UINT32 w;
} intr_ev_t;
typedef union {
   struct {
      UINT32 req             :  1;
      UINT32 cmd             :  1;
      UINT32 src             :  1;
      UINT32 icpu_au         :  1;
      UINT32 ecpu_be         :  1;
      UINT32 icpu_be         :  1;
      UINT32                 : 26;
      UINT32 da              : 13;
      UINT32                 :  3;
      UINT32 len             : 13;
      UINT32                 :  3;
      UINT32 sa              : 32;
      UINT32 tunit           : 13;
      UINT32                 : 19;
   } f;
   UINT32 w[4];
} dma_ctrl_t;
typedef union {
   struct {
      UINT32 mcu_apb_pri     :  4;
      UINT32 host_pri        :  4;
      UINT32 mcu_xlmi_pri    :  4;
      UINT32                 : 20;
   } f;
   UINT32 w;
} apbbr_pri_t;
typedef union {
   struct {
      UINT32 dd              :  8;
      UINT32 mm              :  8;
      UINT32 yy              : 16;
   } f;
   UINT32 w;
} mcu_ver_t;
typedef union {
   struct {
      UINT32 dd              :  8;
      UINT32 mm              :  8;
      UINT32 yy              : 16;
   } f;
   UINT32 w;
} ipc_ver_t;


typedef struct {
   sys_ctrl_t          sys_ctrl;                   // 000-020
   UINT32             addr_sw_de_sav;		// 0x24
   UINT32             addr_sw_cpu_gpu;		// 0x28
   UINT32             addr_sw_cpu_shadow;		// 0x2C
   intr_en_t           intr_e_en;                  // 030
   intr_st_t           intr_e_st;                  // 034
   intr_cl_t           intr_e_cl;                  // 038
   intr_ev_t           intr_e_ev;                  // 03C
   intr_en_t           intr_en;                    // 040
   intr_st_t           intr_st;                    // 044
   intr_cl_t           intr_cl;                    // 048
   intr_ev_t           intr_ev;                    // 04C
   dma_ctrl_t          dma_ctrl;                   // 050-05C
   apbbr_pri_t         apbbr_ctrl;                 // 060
   UINT32              reserved_02[3];             // 064-06C
   mcu_ver_t           mcu_ver;                    // 070
   UINT32              reserved_03[35];            // 074-0FC

#if 0
   // IPC Registers
   UINT32              vdec_cmd_st;                // 100
   UINT32              vdec_cmd_param_num;         // 104
   UINT32              reserved_04[2];             // 108-10C
   UINT32              mcu_req_en;                 // 110
   UINT32              mcu_req_st;                 // 114
   UINT32              mcu_req_param_num;          // 118
   UINT32              reserved_05;                // 11C
   UINT32              vdec_cmd_param[4];          // 120-12C
   UINT32              mcu_req_param[8];           // 130-14C
   UINT32              vdec_cmd_info_base_addr;    // 150
   UINT32              vdec_cmd_info_end_addr;     // 154
   UINT32              vdec_cmd_info_wptr;         // 158
   UINT32              reserved_06;                // 15C
   UINT32              mcu_req_info_base_addr;     // 160
   UINT32              mcu_req_info_end_addr;      // 164
   UINT32              mcu_req_info_wptr;          // 168
   UINT32              reserved_07;                // 16C
   UINT32              vdec_buf_base[2];           // 170-174
   UINT32              pic_cnt;                    // 178
   UINT32              vsync_duration;             // 17C
#if 0
   UINT32              au_wptr;             		// 180
   UINT32              au_rptr;             		// 184
   UINT32              aui_wptr;             		// 188
   UINT32              aui_rptr;             		// 18C
   UINT32              au_base_addr;             	// 190
   UINT32              au_size;             		// 194
   UINT32              aui_base_addr;             	// 198
   UINT32              aui_size;             		// 19C
#else
   cpb_ptr_t           cpb_ptr[1];
   auib_ptr_t          auib_ptr[1];
   dispq_ptr_t         dispq_ptr[1];
   UINT32              ipc_semaphore;
   UINT32              clear_disp_mark[1];
#endif
   UINT32              LQ_duration;                // 1A0
   UINT32              pic_done_duration;          // 1A4
   UINT32              pic_done_max;               // 1A8
   UINT32              pic_done_min;               // 1AC
   UINT32              pic_run_cnt;                // 1B0
   UINT32              reserved_08[3];             // 1B4-1CC
   cmdb_ptr_t        cmdb_ptr;
   reqb_ptr_t          reqb_ptr;
   UINT32              decoding_time[8];           // 1D0-1EC
   UINT32              reserved_09[3];             // 1F0-1FC
   ipc_ver_t           ipc_ver;                    // 1FC
#endif
} reg_mcu_top_t;

#endif /* __REG_MCU_H__ */

