/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

#ifndef __APPLICFG_ZEPHYR__
#define __APPLICFG_ZEPHYR__

#include <stdint.h>
#include <string.h>
#include <zephyr/sys/printk.h>

/*  Define the architecture : little_endian or big_endian
 -----------------------------------------------------
 Endianness is detected at build time (see zephyr/CMakeLists.txt) and
 reflected through CANOPEN_BIG_ENDIAN in the generated config.h.
 */

/* Integers */
#define INTEGER8 int8_t
#define INTEGER16 int16_t
#define INTEGER24 int32_t
#define INTEGER32 int32_t
#define INTEGER40 int64_t
#define INTEGER48 int64_t
#define INTEGER56 int64_t
#define INTEGER64 int64_t

/* Unsigned integers */
#define UNS8   uint8_t
#define UNS16  uint16_t
#define UNS32  uint32_t
#define UNS24  uint32_t
#define UNS40  uint64_t
#define UNS48  uint64_t
#define UNS56  uint64_t
#define UNS64  uint64_t

/* Reals */
#define REAL32	float
#define REAL64 double

/* Definition of error and warning macros */
/* -------------------------------------- */
#define MSG(...) printk(__VA_ARGS__)

/* Definition of MSG_ERR */
/* --------------------- */
#ifdef DEBUG_ERR_CONSOLE_ON
#    define MSG_ERR(num, str, val)            \
          MSG("%s,%d : 0X%x %s 0X%x \n",__FILE__, __LINE__,num, str, val);
#else
#    define MSG_ERR(num, str, val)
#endif

/* Definition of MSG_WAR */
/* --------------------- */
#ifdef DEBUG_WAR_CONSOLE_ON
#    define MSG_WAR(num, str, val)          \
          MSG("%s,%d : 0X%x %s 0X%x \n",__FILE__, __LINE__,num, str, val);
#else
#    define MSG_WAR(num, str, val)
#endif

typedef void* CAN_HANDLE;

typedef void* CAN_PORT;

#endif
