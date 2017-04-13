/****************************************************************************************
 *   DTV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   COPYRIGHT(c) 1998-2010 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work covered by this copyright hereon
 *   may be reproduced, stored in a retrieval system, in any form
 *   or by any means, electronic, mechanical, photocopying, recording
 *   or otherwise, without the prior written  permission of LG Electronics.
 ***************************************************************************************/

/** @file
 *
 *  #MOD# register details. ( used only within kdriver )
 *
 *  author     user name (user_name@lge.com)
 *  version    1.0
 *  date       2010.xx.xx
 *
 */

#ifndef _SYNC_REG_H_
#define _SYNC_REG_H_

/*----------------------------------------------------------------------------------------
    Control Constants
----------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------------------------*/


#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
	0x0004 sync_conf ''
------------------------------------------------------------------------------*/
typedef struct {
	UINT32
	reg_vsync0_field_sel	: 1,	//	 0
	reg_vsync1_field_sel	: 1,	//	 1
	reserved				: 26,	//
	reg_vsync_srst			:1;	//	28
} SYNC_CONF;

/*-----------------------------------------------------------------------------
	0x0010 vsync0_field0_num ''
------------------------------------------------------------------------------*/
typedef struct {
	UINT32
	reg_vsync0_field0_num           ;   	// 31: 0
} VSYNC0_FIELD0_NUM;

/*-----------------------------------------------------------------------------
	0x0014 vsync0_field1_num ''
------------------------------------------------------------------------------*/
typedef struct {
	UINT32
	reg_vsync0_field1_num           ;   	// 31: 0
} VSYNC0_FIELD1_NUM;

/*-----------------------------------------------------------------------------
	0x0018 vsync1_field0_num ''
------------------------------------------------------------------------------*/
typedef struct {
	UINT32
	reg_vsync1_field0_num           ;   	// 31: 0
} VSYNC1_FIELD0_NUM;

/*-----------------------------------------------------------------------------
	0x001c vsync1_field1_num ''
------------------------------------------------------------------------------*/
typedef struct {
	UINT32
	reg_vsync1_field1_num           ;   	// 31: 0
} VSYNC1_FIELD1_NUM;

typedef struct {
	UINT32	reg_field0_num           ;   	// 31: 0
	UINT32	reg_field1_num           ;   	// 31: 0
} VSYNC_FIELD_NUM;

typedef struct {
	UINT32                         	version                         ;	// 0x0000 : ''
	SYNC_CONF                        sync_conf                       ;	// 0x0004 : ''
	UINT32                               __rsvd_00[   2];	// 0x0008 ~ 0x000c
	VSYNC_FIELD_NUM              vsync_field_num[2];             ;	// 0x0010 ~ 0x001c
} Sync_REG_T;
/* 6 regs, 6 types */

/*
 * @{
 * Naming for register pointer.
 * gpRealRegSync : real register of Sync.
 * gpRegSync     : shadow register.
 *
 * @def Sync_RdFL: Read  FLushing : Shadow <- Real.
 * @def Sync_WrFL: Write FLushing : Shadow -> Real.
 * @def Sync_Rd  : Read  whole register(UINT32) from Shadow register.
 * @def Sync_Wr  : Write whole register(UINT32) from Shadow register.
 * @def Sync_Rd01 ~ Sync_Rdnn: Read  given '01~nn' fields from Shadow register.
 * @def Sync_Wr01 ~ Sync_Wrnn: Write given '01~nn' fields to   Shadow register.
 * */
#define Sync_RdFL(_r)			((gpRegSync->_r)=(gpRealRegSync->_r))
#define Sync_WrFL(_r)			((gpRealRegSync->_r)=(gpRegSync->_r))

#define Sync_Rd(_r)			*((UINT32*)(&(gpRegSync->_r)))
#define Sync_Wr(_r,_v)			((Sync_Rd(_r))=((UINT32)(_v)))

#define Sync_Rd01(_r,_f01,_v01)													\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
								} while(0)

#define Sync_Rd02(_r,_f01,_v01,_f02,_v02)										\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
								} while(0)

#define Sync_Rd03(_r,_f01,_v01,_f02,_v02,_f03,_v03)								\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
								} while(0)

#define Sync_Rd04(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04)					\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
								} while(0)

#define Sync_Rd05(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05)													\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
								} while(0)

#define Sync_Rd06(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06)										\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
								} while(0)

#define Sync_Rd07(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07)								\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
								} while(0)

#define Sync_Rd08(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08)					\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
								} while(0)

#define Sync_Rd09(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09)													\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
								} while(0)

#define Sync_Rd10(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10)										\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
								} while(0)

#define Sync_Rd11(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11)								\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
								} while(0)

#define Sync_Rd12(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12)					\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
									(_v12) = (gpRegSync->_r._f12);				\
								} while(0)

#define Sync_Rd13(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13)													\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
									(_v12) = (gpRegSync->_r._f12);				\
									(_v13) = (gpRegSync->_r._f13);				\
								} while(0)

#define Sync_Rd14(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14)										\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
									(_v12) = (gpRegSync->_r._f12);				\
									(_v13) = (gpRegSync->_r._f13);				\
									(_v14) = (gpRegSync->_r._f14);				\
								} while(0)

#define Sync_Rd15(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14,_f15,_v15)								\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
									(_v12) = (gpRegSync->_r._f12);				\
									(_v13) = (gpRegSync->_r._f13);				\
									(_v14) = (gpRegSync->_r._f14);				\
									(_v15) = (gpRegSync->_r._f15);				\
								} while(0)

#define Sync_Rd16(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14,_f15,_v15,_f16,_v16)					\
								do { 											\
									(_v01) = (gpRegSync->_r._f01);				\
									(_v02) = (gpRegSync->_r._f02);				\
									(_v03) = (gpRegSync->_r._f03);				\
									(_v04) = (gpRegSync->_r._f04);				\
									(_v05) = (gpRegSync->_r._f05);				\
									(_v06) = (gpRegSync->_r._f06);				\
									(_v07) = (gpRegSync->_r._f07);				\
									(_v08) = (gpRegSync->_r._f08);				\
									(_v09) = (gpRegSync->_r._f09);				\
									(_v10) = (gpRegSync->_r._f10);				\
									(_v11) = (gpRegSync->_r._f11);				\
									(_v12) = (gpRegSync->_r._f12);				\
									(_v13) = (gpRegSync->_r._f13);				\
									(_v14) = (gpRegSync->_r._f14);				\
									(_v15) = (gpRegSync->_r._f15);				\
									(_v16) = (gpRegSync->_r._f16);				\
								} while(0)


#define Sync_Wr01(_r,_f01,_v01)													\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
								} while(0)

#define Sync_Wr02(_r,_f01,_v01,_f02,_v02)										\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
								} while(0)

#define Sync_Wr03(_r,_f01,_v01,_f02,_v02,_f03,_v03)								\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
								} while(0)

#define Sync_Wr04(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04)					\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
								} while(0)

#define Sync_Wr05(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05)													\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
								} while(0)

#define Sync_Wr06(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06)										\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
								} while(0)

#define Sync_Wr07(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07)								\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
								} while(0)

#define Sync_Wr08(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08)					\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
								} while(0)

#define Sync_Wr09(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09)													\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
								} while(0)

#define Sync_Wr10(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10)										\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
								} while(0)

#define Sync_Wr11(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11)								\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
								} while(0)

#define Sync_Wr12(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12)					\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
									(gpRegSync->_r._f12) = (_v12);				\
								} while(0)

#define Sync_Wr13(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13)													\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
									(gpRegSync->_r._f12) = (_v12);				\
									(gpRegSync->_r._f13) = (_v13);				\
								} while(0)

#define Sync_Wr14(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14)										\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
									(gpRegSync->_r._f12) = (_v12);				\
									(gpRegSync->_r._f13) = (_v13);				\
									(gpRegSync->_r._f14) = (_v14);				\
								} while(0)

#define Sync_Wr15(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14,_f15,_v15)								\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
									(gpRegSync->_r._f12) = (_v12);				\
									(gpRegSync->_r._f13) = (_v13);				\
									(gpRegSync->_r._f14) = (_v14);				\
									(gpRegSync->_r._f15) = (_v15);				\
								} while(0)

#define Sync_Wr16(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,					\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,					\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,					\
					_f13,_v13,_f14,_v14,_f15,_v15,_f16,_v16)					\
								do { 											\
									(gpRegSync->_r._f01) = (_v01);				\
									(gpRegSync->_r._f02) = (_v02);				\
									(gpRegSync->_r._f03) = (_v03);				\
									(gpRegSync->_r._f04) = (_v04);				\
									(gpRegSync->_r._f05) = (_v05);				\
									(gpRegSync->_r._f06) = (_v06);				\
									(gpRegSync->_r._f07) = (_v07);				\
									(gpRegSync->_r._f08) = (_v08);				\
									(gpRegSync->_r._f09) = (_v09);				\
									(gpRegSync->_r._f10) = (_v10);				\
									(gpRegSync->_r._f11) = (_v11);				\
									(gpRegSync->_r._f12) = (_v12);				\
									(gpRegSync->_r._f13) = (_v13);				\
									(gpRegSync->_r._f14) = (_v14);				\
									(gpRegSync->_r._f15) = (_v15);				\
									(gpRegSync->_r._f16) = (_v16);				\
								} while(0)

/* Indexed Register Access.
 *
 * There is in-direct field specified by 'index' field within a register.
 * Normally a register has only one meaning for a 'field_name', but indexed register
 * can hold several data for a 'field_name' specifed by 'index' field of indexed register.
 * When writing an 3rd data for given 'field_name' register, you need to set 'rw' = 0, 'index' = 2,
 * and 'load' = 0.
 *
 * ASSUMPTION
 * For Writing indexed register load bit
 *
 * parameter list
 * _r     : name of register
 * _lname : name of load  bit field	: shall be 0 after macro executed.
 * _rwname: name of rw    bit field : shall be 0 after Sync_Wind(), 1 for Sync_Rind()
 * _iname : name of index bit field
 * _ival  : index value
 * _fname : field name
 * _fval  : field variable that field value shall be stored.
 *
 * Sync_Rind : General indexed register Read.(
 * Sync_Wind : General indexed register Read.
 *
 * Sync_Ridx : For 'index', 'rw', 'load' field name
 * Sync_Widx : For 'index', 'rw', 'load' field name and NO_LOAD.
 */
#define Sync_Rind(_r, _lname, _rwname, _iname, _ival, _fname, _fval)				\
							do {												\
								Sync_Wr03(_r,_lname,0,_rwname,1,_iname,_ival);	\
								Sync_WrFL(_r);									\
								Sync_RdFL(_r);									\
								Sync_Rd01(_r,_fname,_fval);						\
							} while (0)

#define Sync_Wind(_r, _lname, _rwname, _iname, _ival, _fname, _fval)				\
				Sync_Wr04(_r, _lname,0, _rwname,0, _iname,_ival, _fname,_fval)


#define Sync_Ridx(_r, _ival, _fname, _fval)	Sync_Rind(_r,load,rw,index,_ival,_fname,_fval)

#define Sync_Widx(_r, _ival, _fname, _fval)	Sync_Wind(_r,load,rw,index,_ival,_fname,_fval)

/** @} *//* end of macro documentation */

#define gpRealRegSync		stpSync_Reg
#define gpRegSync			stpSync_RegShadow

#ifdef __cplusplus
}
#endif

#endif	/* _#MOD#_REG_H_ */

/* from 'VDEC_SYNC.csv' 20110322 14:00:25   대한민국 표준시 by getregs v2.7 */
