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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/time.h>
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
 *  Timer driver (formerly drivers/timers_unix/timers_unix.c)
 * ------------------------------------------------------------------------- */

static pthread_mutex_t CanFestival_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct timeval last_sig;

static timer_t timer;

void TimerCleanup(void)
{
	/* only used in realtime apps */
}

void EnterMutex(void)
{
	if(pthread_mutex_lock(&CanFestival_mutex)) {
		fprintf(stderr, "pthread_mutex_lock() failed\n");
	}
}

void LeaveMutex(void)
{
	if(pthread_mutex_unlock(&CanFestival_mutex)) {
		fprintf(stderr, "pthread_mutex_unlock() failed\n");
	}
}

void timer_notify(sigval_t val)
{
	if(gettimeofday(&last_sig,NULL)) {
		perror("gettimeofday()");
	}
	EnterMutex();
	TimeDispatch();
	LeaveMutex();
//	printf("getCurrentTime() return=%u\n", p.tv_usec);
}

void TimerInit(void)
{
	struct sigevent sigev;

	// Take first absolute time ref.
	if(gettimeofday(&last_sig,NULL)){
		perror("gettimeofday()");
	}

#if defined(__UCLIBC__)
	int ret;
	ret = timer_create(CLOCK_PROCESS_CPUTIME_ID, NULL, &timer);
	signal(SIGALRM, timer_notify);
#else
	memset (&sigev, 0, sizeof (struct sigevent));
	sigev.sigev_value.sival_int = 0;
	sigev.sigev_notify = SIGEV_THREAD;
	sigev.sigev_notify_attributes = NULL;
	sigev.sigev_notify_function = timer_notify;

	if(timer_create (CLOCK_REALTIME, &sigev, &timer)) {
		perror("timer_create()");
	}
#endif
}

void StopTimerLoop(TimerCallback_t exitfunction)
{
	EnterMutex();
	if(timer_delete (timer)) {
		perror("timer_delete()");
	}
	exitfunction(NULL,0);
	LeaveMutex();
}

void StartTimerLoop(TimerCallback_t init_callback)
{
	EnterMutex();
	// At first, TimeDispatch will call init_callback.
	SetAlarm(NULL, 0, init_callback, 0, 0);
	LeaveMutex();
}

void canReceiveLoop_signal(int sig)
{
}
/* We assume that ReceiveLoop_task_proc is always the same */
static void (*unixtimer_ReceiveLoop_task_proc)(CAN_PORT) = NULL;

/**
 * Enter in realtime and start the CAN receiver loop
 * @param port
 */
void* unixtimer_canReceiveLoop(void* port)
{
    /*get signal*/
  	//  if(signal(SIGTERM, canReceiveLoop_signal) == SIG_ERR) {
	//		perror("signal()");
	//}

    // Set the cancelation state for immediatly cancel the task
    int ret;

    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    if (ret != 0)
        perror("Can't enable the cancelation of the receiving task");

    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    if (ret != 0)
        perror("Can't set the asynchronous cancel typ");

    unixtimer_ReceiveLoop_task_proc((CAN_PORT)port);

    return NULL;
}

void CreateReceiveTask(CAN_PORT port, TASK_HANDLE* Thread, void* ReceiveLoopPtr)
{
    unixtimer_ReceiveLoop_task_proc = ReceiveLoopPtr;
	if(pthread_create(Thread, NULL, unixtimer_canReceiveLoop, (void*)port)) {
		perror("pthread_create()");
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
//	printf("setTimer(TIMEVAL value=%d)\n", value);
	// TIMEVAL is us whereas setitimer wants ns...
	long tv_nsec = 1000 * (maxval(value,1)%1000000);
	time_t tv_sec = value/1000000;
	struct itimerspec timerValues;
	timerValues.it_value.tv_sec = tv_sec;
	timerValues.it_value.tv_nsec = tv_nsec;
	timerValues.it_interval.tv_sec = 0;
	timerValues.it_interval.tv_nsec = 0;

 	timer_settime (timer, 0, &timerValues, NULL);
}

TIMEVAL getElapsedTime(void)
{
	struct timeval p;
	if(gettimeofday(&p,NULL)) {
		perror("gettimeofday()");
	}
//	printf("getCurrentTime() return=%u\n", p.tv_usec);
	return (p.tv_sec - last_sig.tv_sec)* 1000000 + p.tv_usec - last_sig.tv_usec;
}

