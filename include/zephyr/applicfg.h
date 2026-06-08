/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

#ifndef __APPLICFG_ZEPHYR__
#define __APPLICFG_ZEPHYR__

/* Timer base type. Time unit : us, 64-bit resolution (~584942 years) */
#define TIMEVAL unsigned long long
#define TIMEVAL_MAX ~(TIMEVAL)0
#define MS_TO_TIMEVAL(ms) ms*1000L
#define US_TO_TIMEVAL(us) us

/* Zephyr links the CAN driver statically (NOT_USE_DYNAMIC_LOADING): no
 * LoadCanDriver/LIB_HANDLE, hence no USE_DYNAMIC_CAN_DRIVER_LOADING here. */

#endif /* __APPLICFG_ZEPHYR__ */
