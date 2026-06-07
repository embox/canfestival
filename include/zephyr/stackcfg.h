/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

#ifndef __stackcfg_h__
#define __stackcfg_h__

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

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

/* CanFestival messages go to the Zephyr "canfestival" log module, registered
 * once in drivers/zephyr/zephyr.c. Every other translation unit references it
 * through this declaration. The registering TU defines
 * CANFESTIVAL_LOG_MODULE_REGISTER before including this header to suppress the
 * declaration (LOG_MODULE_REGISTER and LOG_MODULE_DECLARE cannot coexist). */
#ifndef CONFIG_CANFESTIVAL_LOG_LEVEL
#define CONFIG_CANFESTIVAL_LOG_LEVEL 0 /* LOG_LEVEL_NONE when not configured */
#endif
#ifndef CANFESTIVAL_LOG_MODULE_REGISTER
LOG_MODULE_DECLARE(canfestival, CONFIG_CANFESTIVAL_LOG_LEVEL);
#endif

/* Definition of error and warning macros */
/* -------------------------------------- */
#define MSG(...) LOG_INF(__VA_ARGS__)

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

#endif /* __stackcfg_h__ */
