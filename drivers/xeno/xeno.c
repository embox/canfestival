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

/* CanFestival platform wrapper for Xenomai.
 *
 * The CAN platform wrapper (dynamic driver loading + canOpen/canClose
 * dispatch) is identical to drivers/unix/unix.c; only the timer driver below
 * differs (nanosecond resolution, timerfd-based loop, RT-scheduled threads). */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#ifndef NOT_USE_DYNAMIC_LOADING
#define DLL_CALL(funcname) (* funcname##_driver)
#define FCT_PTR_INIT =NULL

#define DLSYM(name)\
	*(void **) (&name##_driver) = dlsym(handle, #name"_driver");\
	if ((error = dlerror()) != NULL)  {\
		fprintf (stderr, "%s\n", error);\
		UnLoadCanDriver(handle);\
		return NULL;\
	}

#else /*NOT_USE_DYNAMIC_LOADING*/

/*Function call is direct*/
#define DLL_CALL(funcname) funcname##_driver

#endif /*NOT_USE_DYNAMIC_LOADING*/

#include "data.h"
#include "canfestival.h"
#include "timers_driver.h"

#define MAX_NB_CAN_PORTS 16

/** CAN port structure */
typedef struct {
  char used;  /**< flag indicating CAN port usage, will be used to abort Receiver task*/
  CAN_HANDLE fd; /**< CAN port file descriptor*/
  TASK_HANDLE receiveTask; /**< CAN Receiver task*/
  CO_Data* d; /**< CAN object data*/
} CANPort;

#include "can_driver.h"

CANPort canports[MAX_NB_CAN_PORTS] = {{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,}};

#ifndef NOT_USE_DYNAMIC_LOADING

/*UnLoads the dll*/
UNS8 UnLoadCanDriver(LIB_HANDLE handle)
{
	if(handle!=NULL)
	{
		dlclose(handle);

		handle=NULL;
		return 0;
	}
	return -1;
}

/**
 * Loads the dll and get funcs ptr
 *
 * @param driver_name String containing driver's dynamic library name
 * @return Library handle
 */
LIB_HANDLE LoadCanDriver(const char* driver_name)
{
	LIB_HANDLE handle = NULL;
	char *error;


	if(handle==NULL)
	{
		handle = dlopen(driver_name, RTLD_LAZY);
	}

	if (!handle) {
		fprintf (stderr, "%s\n", dlerror());
        	return NULL;
	}

	/*Get function ptr*/
	DLSYM(canReceive)
	DLSYM(canSend)
	DLSYM(canOpen)
	DLSYM(canChangeBaudRate)
	DLSYM(canClose)

	return handle;
}

#endif

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
	        //LeaveMutex();
		res = DLL_CALL(canSend)(((CANPort*)port)->fd, m);
		//EnterMutex();
		return res; // OK
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
               if (DLL_CALL(canReceive)(((CANPort*)port)->fd, &m) != 0)
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

#ifndef NOT_USE_DYNAMIC_LOADING
	if (&DLL_CALL(canOpen)==NULL) {
        	fprintf(stderr,"CanOpen : Can Driver dll not loaded\n");
        	return NULL;
	}
#endif
	CAN_HANDLE fd0 = DLL_CALL(canOpen)(board);
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

        res = DLL_CALL(canClose)(port->fd);

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
	    //LeaveMutex();
		res = DLL_CALL(canChangeBaudRate)(((CANPort*)port)->fd, baud);
		//EnterMutex();
		return res; // OK
	}
	return 1; // NOT OK
}

/* ------------------------------------------------------------------------- *
 *  Timer driver (formerly drivers/timers_xeno/timers_xeno.c)
 * ------------------------------------------------------------------------- */

static pthread_mutex_t CanFestival_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
int tfd;
static int timer_created = 0;
struct timespec last_occured_alarm;
struct timespec next_alarm;
struct timespec time_ref;

static pthread_t timer_thread;
int stop_timer = 0;

/* Subtract the struct timespec values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */
int
timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < y->tv_nsec) {
    int seconds = (y->tv_nsec - x->tv_nsec) / 1000000000L + 1;
    y->tv_nsec -= 1000000000L * seconds;
    y->tv_sec += seconds;
  }
  if (x->tv_nsec - y->tv_nsec > 1000000000L) {
    int seconds = (x->tv_nsec - y->tv_nsec) / 1000000000L;
    y->tv_nsec += 1000000000L * seconds;
    y->tv_sec -= seconds;
  }

  /* Compute the time remaining to wait.
     tv_nsec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

void timespec_add (struct timespec *result, struct timespec *x, struct timespec *y)
{
  result->tv_sec = x->tv_sec + y->tv_sec;
  result->tv_nsec = x->tv_nsec + y->tv_nsec;

  while (result->tv_nsec > 1000000000L) {
      result->tv_sec ++;
      result->tv_nsec -= 1000000000L;
  }
}

void TimerCleanup(void)
{
}

void EnterTimerMutex(void)
{
	if(pthread_mutex_lock(&timer_mutex)) {
		perror("pthread_mutex_lock(timer_mutex) failed\n");
	}
}

void LeaveTimerMutex(void)
{
	if(pthread_mutex_unlock(&timer_mutex)) {
		perror("pthread_mutex_unlock(timer_mutex) failed\n");
	}
}

void EnterMutex(void)
{
	if(pthread_mutex_lock(&CanFestival_mutex)) {
		perror("pthread_mutex_lock(CanFestival_mutex) failed\n");
	}
}

void LeaveMutex(void)
{
	if(pthread_mutex_unlock(&CanFestival_mutex)) {
		perror("pthread_mutex_unlock(CanFestival_mutex) failed\n");
	}
}

void TimerInit(void)
{
    /* Initialize absolute time references */
    if(clock_gettime(CLOCK_MONOTONIC, &time_ref)){
        perror("clock_gettime(time_ref)");
    }
    next_alarm = last_occured_alarm = time_ref;
}

void StopTimerLoop(TimerCallback_t exitfunction)
{

	stop_timer = 1;

    pthread_cancel(timer_thread);
    pthread_join(timer_thread, NULL);

	EnterMutex();
	exitfunction(NULL,0);
	LeaveMutex();
}

void* xenomai_timerLoop(void* unused)
{
    struct sched_param param = { .sched_priority = 80 };
    int ret;

    if (ret = pthread_setname_np(pthread_self(), "canfestival_timer")) {
        fprintf(stderr, "pthread_setname_np(): %s\n",
                strerror(-ret));
        goto exit_timerLoop;
    }

    if (ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param)) {
        fprintf(stderr, "pthread_setschedparam(): %s\n",
                strerror(-ret));
        goto exit_timerLoop;
    }

    EnterTimerMutex();

    tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if(tfd == -1) {
		perror("timer_create() failed\n");
        goto exit_timerLoop_timermutex;
    }
    timer_created = 1;

    while (!stop_timer)
    {
        uint64_t ticks;

        LeaveTimerMutex();

        EnterMutex();
        TimeDispatch();
        LeaveMutex();

        /*  wait next timer occurence */

        ret = read(tfd, &ticks, sizeof(ticks));
        if (ret < 0) {
            perror("timerfd read()\n");
            break;
        }

        EnterTimerMutex();

        last_occured_alarm = next_alarm;
    }

    close(tfd);

    timer_created = 0;

exit_timerLoop_timermutex:
    LeaveTimerMutex();

exit_timerLoop:
    return NULL;
}


void StartTimerLoop(TimerCallback_t init_callback)
{
    int ret;

	stop_timer = 0;

	EnterMutex();
	// At first, TimeDispatch will call init_callback.
	SetAlarm(NULL, 0, init_callback, 0, 0);
	LeaveMutex();

    ret = pthread_create(&timer_thread, NULL, &xenomai_timerLoop, NULL);
    if (ret) {
        fprintf(stderr, "StartTimerLoop pthread_create(): %s\n",
                strerror(-ret));
    }
}

static void (*unixtimer_ReceiveLoop_task_proc)(CAN_PORT) = NULL;

void* xenomai_canReceiveLoop(void* port)
{
    int ret;
    struct sched_param param = { .sched_priority = 82 };
    pthread_setname_np(pthread_self(), "canReceiveLoop");
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    unixtimer_ReceiveLoop_task_proc((CAN_PORT)port);

    return NULL;
}

void CreateReceiveTask(CAN_PORT port, TASK_HANDLE* Thread, void* ReceiveLoopPtr)
{
    unixtimer_ReceiveLoop_task_proc = ReceiveLoopPtr;

	if(pthread_create(Thread, NULL, xenomai_canReceiveLoop, (void*)port)) {
		perror("CreateReceiveTask pthread_create()");
	}
}

void WaitReceiveTaskEnd(TASK_HANDLE *Thread)
{
	if(pthread_cancel(*Thread)) {
		perror("pthread_cancel()");
	}
	if(pthread_join(*Thread, NULL)) {
		perror("pthread_join()");
	}
}

#define maxval(a,b) ((a>b)?a:b)
void setTimer(TIMEVAL value)
{
    struct itimerspec timerValues;

    EnterTimerMutex();

    if(timer_created){
        long tv_nsec = (maxval(value,1)%1000000000LL);
        time_t tv_sec = value/1000000000LL;
        timerValues.it_value.tv_sec = tv_sec;
        timerValues.it_value.tv_nsec = tv_nsec;
        timerValues.it_interval.tv_sec = 0;
        timerValues.it_interval.tv_nsec = 0;

        /* keep track of when should alarm occur*/
        timespec_add(&next_alarm, &time_ref, &timerValues.it_value);
        timerfd_settime (tfd, 0, &timerValues, NULL);
    }

    LeaveTimerMutex();
}


TIMEVAL getElapsedTime(void)
{
    struct itimerspec outTimerValues;
    struct timespec ts;
    struct timespec last_occured_alarm_copy;

    TIMEVAL res = 0;

    EnterTimerMutex();

    if(timer_created){
        if(clock_gettime(CLOCK_MONOTONIC, &time_ref)){
            perror("clock_gettime(ts)");

        }
        /* timespec_substract modifies second operand */
        last_occured_alarm_copy = last_occured_alarm;

        timespec_subtract(&ts, &time_ref, &last_occured_alarm_copy);

        /* TIMEVAL is nano seconds */
        res = (ts.tv_sec * 1000000000L) + ts.tv_nsec;
    }

    LeaveTimerMutex();

	return res;
}
