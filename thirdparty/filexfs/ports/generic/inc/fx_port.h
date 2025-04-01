/***************************************************************************
 * Copyright (c) 2024 Microsoft Corporation 
 * 
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 * 
 * SPDX-License-Identifier: MIT
 **************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** FileX Component                                                       */
/**                                                                       */
/**   Port Specific                                                       */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/


/**************************************************************************/ 
/*                                                                        */ 
/*  PORT SPECIFIC C INFORMATION                            RELEASE        */ 
/*                                                                        */ 
/*    fx_port.h                                            Generic        */ 
/*                                                           6.3.0        */
/*                                                                        */
/*  AUTHOR                                                                */
/*                                                                        */
/*    William E. Lamie, Microsoft Corporation                             */
/*                                                                        */
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This file contains data type definitions that make the FileX FAT    */ 
/*    compatible file system function identically on a variety of         */ 
/*    different processor architectures.  For example, the byte offset of */ 
/*    various entries in the boot record, and directory entries are       */ 
/*    defined in this file.                                               */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  11-09-2020     William E. Lamie         Initial Version 6.1.2         */
/*  03-02-2021     William E. Lamie         Modified comment(s), and      */
/*                                            added standalone support,   */
/*                                            resulting in version 6.1.5  */
/*  10-31-2023     Xiuwen Cai               Modified comment(s),          */
/*                                            added basic types guards,   */
/*                                            resulting in version 6.3.0  */
/*                                                                        */
/**************************************************************************/

#ifndef FX_PORT_H
#define FX_PORT_H


/* Determine if the optional FileX user define file should be used.  */

#ifdef FX_INCLUDE_USER_DEFINE_FILE


/* Yes, include the user defines in fx_user.h. The defines in this file may 
   alternately be defined on the command line.  */

#include "fx_user.h"
#endif


/* Include the ThreadX api file.  */
#include <zephyr.h>

#define FX_LEGACY_INTERRUPT_PROTECTION


/* Define compiler library include files.  */

#ifndef VOID
#define VOID                                    void
typedef char                                    CHAR;
typedef char                                    BOOL;
typedef unsigned char                           UCHAR;
typedef int                                     INT;
typedef unsigned int                            UINT;
typedef long                                    LONG;
typedef unsigned long                           ULONG;
typedef short                                   SHORT;
typedef unsigned short                          USHORT;
#endif

#ifndef ULONG64_DEFINED
#define ULONG64_DEFINED
typedef unsigned long long                      ULONG64;
#endif

/* Define basic alignment type used in block and byte pool operations. This data type must
   be at least 32-bits in size and also be large enough to hold a pointer type.  */

#ifndef ALIGN_TYPE_DEFINED
#define ALIGN_TYPE_DEFINED
#define ALIGN_TYPE                              ULONG
#endif


/* Define FileX internal protection macros.  If FX_SINGLE_THREAD is defined,
   these protection macros are effectively disabled.  However, for multi-thread
   uses, the macros are setup to utilize a ThreadX mutex for multiple thread 
   access control into an open media.  */
#define FX_MUTEX_DEFINE(_name)          struct k_mutex _name;
#define FX_MUTEX_INIT(_name, ...)       k_mutex_init(_name);
#define FX_MUTEX_UINIT(_name)
#define FX_PROTECT                      k_mutex_lock(&media_ptr->fx_media_protect, K_FOREVER);
#define FX_UNPROTECT                    k_mutex_unlock(&media_ptr->fx_media_protect);


#define FX_PREEMPT_DISABLE              k_sched_lock();
#define FX_PREEMPT_RESTORE              k_sched_unlock();

#define FX_LOCAL_PATH_SETUP
#define _tx_thread_current_ptr          k_current_get()

#ifndef FX_NO_TIMER
#define FX_TIMER                        struct k_timer
#define FX_TIMER_CREATE  \
   do { \
      k_timeout_t __ticks = {FX_UPDATE_RATE_IN_TICKS}; \
      k_timer_init(&_fx_system_timer, (k_timer_expiry_t)_fx_system_timer_entry, NULL); \
      k_timer_start(&_fx_system_timer, __ticks, __ticks); \
   } while (0);
#endif /* FX_NO_TIMER */

/* Define interrupt lockout constructs to protect the system date/time from being updated
   while they are being read.  */
#define FX_INT_SAVE_AREA                unsigned int __key;
#define FX_DISABLE_INTS                 __key = irq_lock();
#define FX_RESTORE_INTS                 irq_unlock(__key);

/* Define the error checking logic to determine if there is a caller error in the FileX API.  
   The default definitions assume ThreadX is being used.  This code can be completely turned 
   off by just defining these macros to white space.  */

#ifndef FX_STANDALONE_ENABLE
#define FX_CALLER_CHECKING_EXTERNS
#define FX_CALLER_CHECKING_CODE
#endif


/* Define the update rate of the system timer.  These values may also be defined at the command
   line when compiling the fx_system_initialize.c module in the FileX library build.  Alternatively, they can
   be modified in this file or fx_user.h. Note: the update rate must be an even number of seconds greater
   than or equal to 2, which is the minimal update rate for FAT time. */

/* Define the number of seconds the timer parameters are updated in FileX.  The default
   value is 10 seconds.  This value can be overwritten externally. */

#ifndef FX_UPDATE_RATE_IN_SECONDS
#define FX_UPDATE_RATE_IN_SECONDS 10
#endif


/* Defines the number of ThreadX timer ticks required to achieve the update rate specified by 
   FX_UPDATE_RATE_IN_SECONDS defined previously. By default, the ThreadX timer tick is 10ms, 
   so the default value for this constant is 1000.  If TX_TIMER_TICKS_PER_SECOND is defined,
   this value is derived from TX_TIMER_TICKS_PER_SECOND.  */
 
#define FX_UPDATE_RATE_IN_TICKS        (CONFIG_SYS_CLOCK_TICKS_PER_SEC * FX_UPDATE_RATE_IN_SECONDS)



/* Define the version ID of FileX.  This may be utilized by the application.  */

#ifdef FX_SYSTEM_INIT
CHAR                            _fx_version_id[] = "Copyright (c) 2024 FileX";
#else
extern  CHAR                    _fx_version_id[];
#endif

/* User extension for FileX */
typedef struct FX_LOGDEVICE {
    const CHAR *name;
    ULONG part_offset;
    ULONG part_size;
    UINT  number_of_fats;
    UINT  directory_entries;
    void *media_buffer;
    UINT  media_buffer_size;
} FX_LOGDEVICE;

#define FX_MEDIA_MODULE_EXTENSION \
    VOID *fx_media_device_; \
    ULONG fx_media_sector_base_; \
    ULONG fx_media_sector_num_;

#endif /* FX_PORT_H */
