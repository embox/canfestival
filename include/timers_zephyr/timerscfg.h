/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

#ifndef __TIMERSCFG_H__
#define __TIMERSCFG_H__

#include <zephyr/kernel.h>

/* Time unit : us */
/* Time resolution : 64bit (~584942 years) */
#define TIMEVAL unsigned long long
#define TIMEVAL_MAX ~(TIMEVAL)0
#define MS_TO_TIMEVAL(ms) ms*1000L
#define US_TO_TIMEVAL(us) us

#if defined(CONFIG_CANFESTIVAL_RX_STACK_SIZE)
#define CANFESTIVAL_RX_STACK_SIZE CONFIG_CANFESTIVAL_RX_STACK_SIZE
#elif !defined(CANFESTIVAL_RX_STACK_SIZE)
#define CANFESTIVAL_RX_STACK_SIZE 2048
#endif

/* The receive task handle bundles the Zephyr thread control block together
 * with its stack, so that whoever embeds a TASK_HANDLE (the canports[] array
 * in the platform driver) statically owns the memory - no dynamic allocation. */
struct canfestival_task {
	struct k_thread thread;
	K_KERNEL_STACK_MEMBER(stack, CANFESTIVAL_RX_STACK_SIZE);
	k_tid_t tid;
};

#define TASK_HANDLE struct canfestival_task

#endif
