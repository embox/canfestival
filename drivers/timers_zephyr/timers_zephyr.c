/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

/* CanFestival timer driver for Zephyr.
 *
 * setTimer() arms a one-shot k_timer; the core re-arms it from TimeDispatch().
 * Because a k_timer expiry callback runs in interrupt context - where the
 * CanFestival mutex cannot be taken and TimeDispatch() (which calls user
 * callbacks) must not run - the expiry callback only gives a semaphore. A
 * dedicated dispatch thread waits on that semaphore and runs TimeDispatch()
 * in thread context under the mutex. The receive task(s) are plain Zephyr
 * threads whose control block and stack live in the caller-owned TASK_HANDLE. */

#include <zephyr/kernel.h>

#include "applicfg.h"
#include "timers.h"
#include "timers_driver.h"

#ifndef CONFIG_CANFESTIVAL_TIMER_THREAD_PRIO
#define CONFIG_CANFESTIVAL_TIMER_THREAD_PRIO 4
#endif

#ifndef CONFIG_CANFESTIVAL_TIMER_STACK_SIZE
#define CONFIG_CANFESTIVAL_TIMER_STACK_SIZE 1024
#endif

#ifndef CONFIG_CANFESTIVAL_RX_THREAD_PRIO
#define CONFIG_CANFESTIVAL_RX_THREAD_PRIO 5
#endif

static K_MUTEX_DEFINE(canfestival_mutex);
static K_SEM_DEFINE(dispatch_sem, 0, 1);

static void timer_expiry(struct k_timer *timer);
static K_TIMER_DEFINE(canfestival_timer, timer_expiry, NULL);

/* microsecond timestamp of the latest timer expiry, as seen by getElapsedTime */
static uint64_t last_sig_us;

static uint64_t now_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

void EnterMutex(void)
{
	k_mutex_lock(&canfestival_mutex, K_FOREVER);
}

void LeaveMutex(void)
{
	k_mutex_unlock(&canfestival_mutex);
}

/* Runs in interrupt context: only signal the dispatch thread. */
static void timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	k_sem_give(&dispatch_sem);
}

static void dispatch_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (;;) {
		k_sem_take(&dispatch_sem, K_FOREVER);
		EnterMutex();
		last_sig_us = now_us();
		TimeDispatch();
		LeaveMutex();
	}
}

static K_THREAD_DEFINE(canfestival_dispatch_tid, CONFIG_CANFESTIVAL_TIMER_STACK_SIZE,
		       dispatch_thread, NULL, NULL, NULL,
		       CONFIG_CANFESTIVAL_TIMER_THREAD_PRIO, 0, 0);

void TimerInit(void)
{
	/* Take first absolute time reference (the dispatch thread is started at
	 * kernel init and is idle until the first setTimer/expiry). */
	last_sig_us = now_us();
}

void TimerCleanup(void)
{
	k_timer_stop(&canfestival_timer);
}

void StartTimerLoop(TimerCallback_t init_callback)
{
	EnterMutex();
	/* At first, TimeDispatch will call init_callback. */
	SetAlarm(NULL, 0, init_callback, 0, 0);
	LeaveMutex();
}

void StopTimerLoop(TimerCallback_t exitfunction)
{
	EnterMutex();
	k_timer_stop(&canfestival_timer);
	exitfunction(NULL, 0);
	LeaveMutex();
}

void setTimer(TIMEVAL value)
{
	/* value is in microseconds. TIMEVAL_MAX means "no alarm pending". */
	if (value == TIMEVAL_MAX) {
		k_timer_stop(&canfestival_timer);
		return;
	}
	k_timer_start(&canfestival_timer, K_USEC(value ? value : 1), K_NO_WAIT);
}

TIMEVAL getElapsedTime(void)
{
	return (TIMEVAL)(now_us() - last_sig_us);
}

/* We assume that ReceiveLoop_task_proc is always the same (canReceiveLoop). */
static void (*zephyrtimer_ReceiveLoop_task_proc)(CAN_PORT) = NULL;

static void zephyrtimer_canReceiveLoop(void *port, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	zephyrtimer_ReceiveLoop_task_proc((CAN_PORT)port);
}

void CreateReceiveTask(CAN_PORT port, TASK_HANDLE *handle, void *ReceiveLoopPtr)
{
	zephyrtimer_ReceiveLoop_task_proc = ReceiveLoopPtr;
	handle->tid = k_thread_create(&handle->thread, handle->stack,
				      K_KERNEL_STACK_SIZEOF(handle->stack),
				      zephyrtimer_canReceiveLoop,
				      (void *)port, NULL, NULL,
				      CONFIG_CANFESTIVAL_RX_THREAD_PRIO, 0, K_NO_WAIT);
}

void WaitReceiveTaskEnd(TASK_HANDLE *handle)
{
	/* The receive loop exits once the port is marked unused and the close
	 * sentinel unblocks canReceive_driver(); just wait for it to return. */
	k_thread_join(&handle->thread, K_FOREVER);
}
