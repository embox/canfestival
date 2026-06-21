/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

/* CanFestival platform wrapper for Zephyr.
 *
 * Mirrors drivers/unix/unix.c but without dynamic loading.
 *
 * This TU registers the "canfestival" Zephyr log module used by the MSG macros;
 * CANFESTIVAL_LOG_MODULE_REGISTER tells applicfg.h not to LOG_MODULE_DECLARE it
 * here (a TU cannot both register and declare the same module). */
#define CANFESTIVAL_LOG_MODULE_REGISTER

#include <zephyr/devicetree.h>

#include "canfestival.h"
#include "canfestival_config.h"
#include "timers_driver.h"

LOG_MODULE_REGISTER(canfestival, CONFIG_CANFESTIVAL_LOG_LEVEL);

/* One CAN port per declared CAN interface; each port statically owns its receive
 * task control block and stack through the embedded TASK_HANDLE.
 *
 * The count comes from the "canfestival,interfaces" devicetree node - the same
 * source can_zephyr.c sizes its device pool from. It is computed in C with
 * DT_PROP_LEN rather than at CMake configure time, because Zephyr's dt_prop()
 * cannot enumerate a phandle-array (it would yield 1, leaving no free port for
 * the second node). One node per interface is assumed. */
#define CF_IFACES_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(canfestival_interfaces)
#if DT_NODE_EXISTS(CF_IFACES_NODE)
#define MAX_NB_CAN_PORTS DT_PROP_LEN(CF_IFACES_NODE, interfaces)
#else
#define MAX_NB_CAN_PORTS 1
#endif

/** CAN port structure */
typedef struct {
  char used;  /**< flag indicating CAN port usage, will be used to abort Receiver task*/
  CAN_HANDLE fd; /**< CAN port file descriptor*/
  TASK_HANDLE receiveTask; /**< CAN Receiver task*/
  CO_Data* d; /**< CAN object data*/
} CANPort;

#include "can_driver.h"

CANPort canports[MAX_NB_CAN_PORTS];

/**
 * CAN send routine
 * @param port CAN port
 * @param m CAN message
 * @return success or error
 */
UNS8 canSend(CAN_PORT port, Message *m)
{
	if(port){
		UNS8 res;
		res = canSend_driver(((CANPort*)port)->fd, m);
		return res; // OK
	}
	return 1; // NOT OK
}

/**
 * Query the local CAN controller state and bus error counters
 * @param d CAN object data
 * @param state receives the controller state
 * @param txerr receives the transmit error counter
 * @param rxerr receives the receive error counter
 * @return 0 on success, non-zero otherwise
 */
UNS8 canGetState(CO_Data *d, UNS8 *state, UNS8 *txerr, UNS8 *rxerr)
{
	CANPort *port = (CANPort *)d->canHandle;
	if(port){
		return canGetState_driver(port->fd, state, txerr, rxerr);
	}
	return 1; // NOT OK
}

/**
 * CAN Receiver Task
 * @param port CAN port
 */
void canReceiveLoop(CAN_PORT port)
{
       Message m;

       while (((CANPort*)port)->used) {
               if (canReceive_driver(((CANPort*)port)->fd, &m) != 0)
                       break;

               EnterMutex();
               canDispatch(((CANPort*)port)->d, &m);
               LeaveMutex();
       }
}

/**
 * CAN open routine
 * @param board device name and baudrate
 * @param d CAN object data
 * @return valid CAN_PORT pointer or NULL
 */
CAN_PORT canOpen(s_BOARD *board, CO_Data * d)
{
	int i;
	for(i=0; i < MAX_NB_CAN_PORTS; i++)
	{
		if(!canports[i].used)
		break;
	}

	if(i == MAX_NB_CAN_PORTS){
		MSG("CanOpen : no free CAN port\n");
		return NULL;
	}

	CAN_HANDLE fd0 = canOpen_driver(board);
	if(fd0){
		canports[i].used = 1;
		canports[i].fd = fd0;
		canports[i].d = d;
		d->canHandle = (CAN_PORT)&canports[i];
		CreateReceiveTask(&(canports[i]), &canports[i].receiveTask, &canReceiveLoop);
		return (CAN_PORT)&canports[i];
	}else{
        	MSG("CanOpen : Cannot open board {busname='%s',baudrate='%s'}\n",board->busname, board->baudrate);
		return NULL;
	}
}

/**
 * CAN close routine
 * @param d CAN object data
 * @return success or error
 */
int canClose(CO_Data * d)
{
	int res = 0;

	CANPort* port = (CANPort*)d->canHandle;
    if(port){
        ((CANPort*)d->canHandle)->used = 0;

        res = canClose_driver(port->fd);

        WaitReceiveTaskEnd(&port->receiveTask);

        d->canHandle = NULL;
    }

	return res;
}


/**
 * CAN change baudrate routine
 * @param port CAN port
 * @param baud baudrate
 * @return success or error
 */
UNS8 canChangeBaudRate(CAN_PORT port, char* baud)
{
   if(port){
		UNS8 res;
		res = canChangeBaudRate_driver(((CANPort*)port)->fd, baud);
		return res; // OK
	}
	return 1; // NOT OK
}

/* ------------------------------------------------------------------------- *
 *  Timer driver (formerly drivers/timers_zephyr/timers_zephyr.c)
 *
 *  setTimer() arms a one-shot k_timer; the core re-arms it from TimeDispatch().
 *  Because a k_timer expiry callback runs in interrupt context - where the
 *  CanFestival mutex cannot be taken and TimeDispatch() (which calls user
 *  callbacks) must not run - the expiry callback only gives a semaphore. A
 *  dedicated dispatch thread waits on that semaphore and runs TimeDispatch()
 *  in thread context under the mutex. The receive task(s) are plain Zephyr
 *  threads whose control block and stack live in the caller-owned TASK_HANDLE.
 * ------------------------------------------------------------------------- */

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
