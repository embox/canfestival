/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <kernel/sched.h>
#include <kernel/thread.h>
#include <kernel/thread/sync/mutex.h>
#include <kernel/thread/thread_sched_wait.h>
#include <kernel/time/sys_timer.h>
#include <kernel/time/time.h>
#include <util/err.h>

#include "can_driver.h"
#include "canfestival.h"
#include "timers_driver.h"

#ifdef CF_MAX_NB_CAN_PORTS
#define MAX_NB_CAN_PORTS CF_MAX_NB_CAN_PORTS
#else
#define MAX_NB_CAN_PORTS 1
#endif

/** CAN port structure */
typedef struct {
	CAN_HANDLE fd;               /* CAN port file descriptor */
	CO_Data *d;                  /* CAN object data*/
	TASK_HANDLE receiveTask;     /* CAN Receiver task */
	void (*receiveLoop)(void *); /* CAN Receiver loop routine */
	int used;                    /* Flag indicating CAN port usage */
} CANPort;

static CANPort canports[MAX_NB_CAN_PORTS];

/**
 * CAN send routine
 * @param port CAN port
 * @param m CAN message
 * @return success or error
 */
UNS8 canSend(CAN_PORT port, Message *m) {
	CANPort *canport;

	canport = (CANPort *)port;
	if (!canport || !canport->used) {
		return 1;
	}

	return canSend_driver(canport->fd, m);
}

/**
 * CAN Receiver Task
 * @param port CAN port
 */
void canReceiveLoop(CAN_PORT port) {
	CANPort *canport;
	Message m;

	canport = (CANPort *)port;

	while (canport->used) {
		if (0 != canReceive_driver(canport->fd, &m)) {
			if (canport->used) {
				MSG("canReceiveLoop : CAN RX error\n");
				sleep(2);
			}
			continue;
		}

		EnterMutex();
		{
			/* canDispatch will call canSend() */
			canDispatch(canport->d, &m);
		}
		LeaveMutex();
	}
}

/**
 * CAN open routine
 * @param board device name and baudrate
 * @param d CAN object data
 * @return valid CAN_PORT pointer or NULL
 */
CAN_PORT canOpen(s_BOARD *board, CO_Data *d) {
	CAN_HANDLE fd0;
	int i;

	for (i = 0; i < MAX_NB_CAN_PORTS; i++) {
		if (!canports[i].used) {
			break;
		}
	}

	if (i == MAX_NB_CAN_PORTS) {
		MSG("CanOpen : no free CAN port\n");
		return NULL;
	}

	fd0 = canOpen_driver(board);
	if (!fd0) {
		MSG("CanOpen : Cannot open '%s'\n", board->busname);
		return NULL;
	}

	d->canHandle = (CAN_PORT)&canports[i];

	canports[i].fd = fd0;
	canports[i].d = d;
	canports[i].used = 1;

	CreateReceiveTask(&(canports[i]), NULL, canReceiveLoop);

	return (CAN_PORT)&canports[i];
}

/**
 * CAN close routine
 * @param d CAN object data
 * @return success or error
 */
int canClose(CO_Data *d) {
	CANPort *canport;
	int res;

	canport = (CANPort *)d->canHandle;
	if (!canport || !canport->used) {
		return 1;
	}

	canport->used = 0;

	res = canClose_driver(canport->fd);

	WaitReceiveTaskEnd(&canport->receiveTask);

	d->canHandle = NULL;

	canport->receiveTask = NULL;
	canport->receiveLoop = NULL;
	canport->fd = (CAN_HANDLE)(intptr_t)-1;

	return res;
}

/**
 * CAN change baudrate routine
 * @param port CAN port
 * @param baud baudrate
 * @return success or error
 */
UNS8 canChangeBaudRate(CAN_PORT port, char *baud) {
	CANPort *canport;

	canport = (CANPort *)port;
	if (!canport || !canport->used) {
		return 1;
	}

	return canChangeBaudRate_driver(canport->fd, baud);
}

/* ------------------------------------------------------------------------- *
 *  Timer driver
 * ------------------------------------------------------------------------- */

static struct sys_timer canfestival_timer;

static struct thread *timer_thread;

static struct timespec last_sig_ts;

static volatile int time_dispatch_pending;

static volatile int timer_used = 0;

static void *canReceiveTask_proc(void *port) {
	CANPort *canport;

	canport = (CANPort *)port;

	if (canport && canport->receiveLoop) {
		canport->receiveLoop(port);
	}

	return NULL;
}

static void *timerTask_proc(void *data) {
	timer_used = 1;

	while (1) {
		SCHED_WAIT_TIMEOUT(time_dispatch_pending || !timer_used, 2000);

		if (!timer_used) {
			break;
		}

		EnterMutex();
		{
			clock_gettime(CLOCK_REALTIME, &last_sig_ts);
			time_dispatch_pending = 0;
			TimeDispatch();
		}
		LeaveMutex();
	}

	return NULL;
}

static void timer_notify(sys_timer_t *timer, void *param) {
	time_dispatch_pending = 1;
	if (timer_used) {
		sched_wakeup(&timer_thread->schedee);
	}
}

void TimerInit(void) {
	struct thread *t;

	if (!timer_used) {
		clock_gettime(CLOCK_REALTIME, &last_sig_ts);
		time_dispatch_pending = 0;
		timer_thread = thread_create(0, timerTask_proc, NULL);
		if (ptr2err(timer_thread)) {
			MSG("TimerInit : Cannot create thread\n");
			return;
		}
	}
}

void TimerCleanup(void) {
	if (timer_used) {
		timer_used = 0;
		sched_wakeup(&timer_thread->schedee);
		thread_join(timer_thread, NULL);
	}
}

void StartTimerLoop(TimerCallback_t init_callback) {
	EnterMutex();
	{
		// At first, TimeDispatch will call init_callback.
		SetAlarm(NULL, 0, init_callback, 0, 0);
	}
	LeaveMutex();
}

void StopTimerLoop(TimerCallback_t exitfunction) {
	EnterMutex();
	{
		sys_timer_stop(&canfestival_timer);
		exitfunction(NULL, 0);
	}
	LeaveMutex();
}

void setTimer(TIMEVAL value) {
	unsigned long msec;
	int err;

	if (value == TIMEVAL_MAX) {
		sys_timer_stop(&canfestival_timer);
		return;
	}

	msec = value / USEC_PER_MSEC;

	sys_timer_init_start_msec(&canfestival_timer, SYS_TIMER_ONESHOT, msec,
	    timer_notify, NULL);
}

TIMEVAL getElapsedTime(void) {
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts = timespec_sub(ts, last_sig_ts);

	return (ts.tv_sec * USEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_USEC);
}

void CreateReceiveTask(CAN_PORT port, TASK_HANDLE *Thread,
    void *ReceiveLoopPtr) {
	struct thread *t;
	CANPort *canport;

	canport = (CANPort *)port;

	canport->receiveLoop = ReceiveLoopPtr;

	if (!canport->receiveTask) {
		t = thread_create(0, canReceiveTask_proc, canport);
		if (ptr2err(t)) {
			MSG("CreateReceiveTask : Cannot create thread\n");
			return;
		}

		canport->receiveTask = t;

		if (Thread) {
			*Thread = t;
		}
	}
}

void WaitReceiveTaskEnd(TASK_HANDLE *Thread) {
	if (Thread && *Thread) {
		thread_join(*Thread, NULL);
	}
}

static struct mutex canfestival_mutex = RMUTEX_INIT(canfestival_mutex);

void EnterMutex(void) {
	mutex_lock(&canfestival_mutex);
}

void LeaveMutex(void) {
	mutex_unlock(&canfestival_mutex);
}
