/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN
Copyright (C) Win32 Port Leonid Tochinski
Modified by: Jaroslav Fojtik

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

/*
 CAN driver interface.
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/timeb.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "canfestival.h"
#include "timers_driver.h"

#ifdef __cplusplus
};
#endif

// GetProcAddress doesn't have an UNICODE version for NT
#ifdef UNDER_CE
  #define myTEXT(str) TEXT(str)
#else
  #define myTEXT(str) str
#endif

#define MAX_NB_CAN_PORTS 16

typedef UNS8 (CALLBACK* CANRECEIVE_DRIVER_PROC)(void* inst, Message *m);
typedef UNS8 (CALLBACK* CANSEND_DRIVER_PROC)(void* inst, const Message *m);
typedef void* (CALLBACK* CANOPEN_DRIVER_PROC)(s_BOARD *board);
typedef int (CALLBACK* CANCLOSE_DRIVER_PROC)(void* inst);
typedef UNS8 (CALLBACK* CANCHANGEBAUDRATE_DRIVER_PROC)(void* fd, char* baud);

CANRECEIVE_DRIVER_PROC m_canReceive;
CANSEND_DRIVER_PROC m_canSend;
CANOPEN_DRIVER_PROC m_canOpen;
CANCLOSE_DRIVER_PROC m_canClose;
CANCHANGEBAUDRATE_DRIVER_PROC m_canChangeBaudRate;

/* CAN port structure */
typedef struct
{
  char used;  /**< flag indicating CAN port usage, will be used to abort Receiver task*/
  CAN_HANDLE fd; /**< CAN port file descriptor*/
  TASK_HANDLE receiveTask; /**< CAN Receiver task*/
  CO_Data* d; /**< CAN object data*/
}CANPort;

CANPort canports[MAX_NB_CAN_PORTS] = {{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,},{0,}};


/***************************************************************************/
UNS8 UnLoadCanDriver(LIB_HANDLE handle)
{
#ifndef NOT_USE_DYNAMIC_LOADING
	if(handle != NULL)
	{
		FreeLibrary(handle);
		handle=NULL;
		return 0;
	}
	return -1;
#else
  handle = NULL;
  return 0;
#endif //NOT_USE_DYNAMIC_LOADING
}

/***************************************************************************/
/**
 * Loads the dll and get funcs ptr
 *
 * @param driver_name String containing driver's dynamic library name
 * @return Library handle
 */
LIB_HANDLE LoadCanDriver(LPCSTR driver_name)
{
    // driver module handle
    LIB_HANDLE handle = NULL;

#ifndef NOT_USE_DYNAMIC_LOADING
    handle = LoadLibrary(driver_name);

    if (!handle)
    {
        fprintf (stderr, "LoadLibrary error : %d\n", GetLastError());
        return NULL;
    }

    m_canReceive = (CANRECEIVE_DRIVER_PROC)GetProcAddress(handle, myTEXT("canReceive_driver"));
    m_canSend = (CANSEND_DRIVER_PROC)GetProcAddress(handle, myTEXT("canSend_driver"));
    m_canOpen = (CANOPEN_DRIVER_PROC)GetProcAddress(handle, myTEXT("canOpen_driver"));
    m_canClose = (CANCLOSE_DRIVER_PROC)GetProcAddress(handle, myTEXT("canClose_driver"));
    m_canChangeBaudRate = (CANCHANGEBAUDRATE_DRIVER_PROC)GetProcAddress(handle, myTEXT("canChangeBaudRate_driver"));

    if(m_canReceive==NULL || m_canSend==NULL || m_canOpen==NULL || m_canClose==NULL || m_canChangeBaudRate==NULL)
    {
        m_canReceive = NULL;
        m_canSend = NULL;
        m_canOpen = NULL;
        m_canClose = NULL;
        m_canChangeBaudRate = NULL;
        FreeLibrary(handle);
        fprintf (stderr, "GetProc error : %d\n", GetLastError());
        return NULL;
    }
#else
    //compiled in...
    handle = 1; //TODO: remove this hack

    m_canReceive = canReceive_driver;
    m_canSend = canSend_driver;
    m_canOpen = canOpen_driver;
    m_canClose = canClose_driver;
    m_canChangeBaudRate = canChangeBaudRate_driver;
#endif


    return handle;
}

/***************************************************************************/
UNS8 canSend(CAN_PORT port, Message *m)
{
	if (port && (m_canSend != NULL))
	{
		return m_canSend(((CANPort*)port)->fd, m);
	}
	return 1; /* NOT OK */	
}

/***************************************************************************/
DWORD canReceiveLoop(CAN_PORT port)
{
	Message m;
	while(((CANPort*)port)->used)
	{
		if(m_canReceive(((CANPort*)port)->fd, &m) != 0) 
            break;
		EnterMutex();
		canDispatch(((CANPort*)port)->d, &m);
		LeaveMutex();
	}
	return 0;
}

/***************************************************************************/
CAN_HANDLE canOpen(s_BOARD *board, CO_Data * d)
{
	int i;
	CAN_HANDLE fd0;


	  /* Fix of multiple opening one data set, added by J.Fojtik. */
	if(d->canHandle)
	{
	  canClose(d);
	}

	for(i=0; i < MAX_NB_CAN_PORTS; i++)
	{
		if(!canports[i].used)
		break;
	}

	#ifndef NOT_USE_DYNAMIC_LOADING
	if (m_canOpen == NULL)
	{
	   	fprintf(stderr,"CanOpen : Can Driver dll not loaded\n");
	   	return NULL;
	}
	#endif

	fd0 = m_canOpen(board);
	if(fd0)
	{
		canports[i].used = 1;
		canports[i].fd = fd0;
		canports[i].d = d;
		d->canHandle = (CAN_PORT)&canports[i];
		CreateReceiveTask(&(canports[i]), &canports[i].receiveTask, (void *)canReceiveLoop);
		return (CAN_PORT)&canports[i];
	}
	else
	{
		MSG("CanOpen : Cannot open board {busname='%S',baudrate='%S'}\n",board->busname, board->baudrate);
		return NULL;
	}
}

/***************************************************************************/
int canClose(CO_Data * d)
{
    int res = 0;

    CANPort* port = (CANPort*)d->canHandle;
    if(port){
        ((CANPort*)d->canHandle)->used = 0;

        res = m_canClose(port->fd);

        WaitReceiveTaskEnd(&port->receiveTask);

        d->canHandle = NULL;
    }

    return res;
}

UNS8 canChangeBaudRate(CAN_PORT port, char* baud)
{
   if(port){
		UNS8 res;
	    //LeaveMutex();
		res = m_canChangeBaudRate(((CANPort*)port)->fd, baud);
		//EnterMutex();
		return res; // OK
	}
	return 1; // NOT OK
}

/* ------------------------------------------------------------------------- *
 *  Timer driver (formerly drivers/timers_windows/timers_windows.c)
 * ------------------------------------------------------------------------- */

DWORD timebuffer;

/* Synchronization Object Implementation */
CRITICAL_SECTION CanFestival_mutex;
HANDLE timer_thread = NULL;
HANDLE timer = NULL;

volatile int stop_timer=0;

static TimerCallback_t init_callback;


void EnterMutex(void)
{
	EnterCriticalSection(&CanFestival_mutex);
}

void LeaveMutex(void)
{
	LeaveCriticalSection(&CanFestival_mutex);
}

// --------------- CAN Receive Thread Implementation ---------------

void CreateReceiveTask(CAN_HANDLE fd0, TASK_HANDLE* Thread, void* ReceiveLoopPtr)
{
	unsigned long thread_id = 0;
	*Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveLoopPtr, fd0, 0, &thread_id);
}

void WaitReceiveTaskEnd(TASK_HANDLE *Thread)
{
	if(WaitForSingleObject(*Thread, 1000) == WAIT_TIMEOUT)
	{
		TerminateThread(*Thread, -1);
	}
	CloseHandle(*Thread);
}

#if !defined(WIN32) || defined(__CYGWIN__)
int TimerThreadLoop(void)
#else
DWORD TimerThreadLoop(LPVOID arg)
#endif
{


	while(!stop_timer)
	{
		WaitForSingleObject(timer, INFINITE);
		if(stop_timer)
			break;
		EnterMutex();
		timebuffer = GetTickCount();
		TimeDispatch();
		LeaveMutex();
	}
	return 0;
}

void TimerInit(void)
{
	LARGE_INTEGER liDueTime;
	liDueTime.QuadPart = 0;

	InitializeCriticalSection(&CanFestival_mutex);

	timer = CreateWaitableTimer(NULL, FALSE, NULL);
	if(NULL == timer)
    {
        printf("CreateWaitableTimer failed (%d)\n", GetLastError());
    }

	// Take first absolute time ref in milliseconds.
	timebuffer = GetTickCount();
}

void TimerCleanup(void)
{
	DeleteCriticalSection(&CanFestival_mutex);
}

void StopTimerLoop(TimerCallback_t exitfunction)
{
	EnterMutex();
	exitfunction(NULL,0);
	LeaveMutex();

	stop_timer = 1;
	setTimer(0);
	if(WaitForSingleObject(timer_thread,1000) == WAIT_TIMEOUT)
	{
		TerminateThread(timer_thread, -1);
	}
	CloseHandle(timer);
	CloseHandle(timer_thread);
}

void StartTimerLoop(TimerCallback_t _init_callback)
{
	unsigned long timer_thread_id;
	stop_timer = 0;
	init_callback = _init_callback;
	EnterMutex();
		// At first, TimeDispatch will call init_callback.
	SetAlarm(NULL, 0, init_callback, 0, 0);
	LeaveMutex();
	timer_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TimerThreadLoop, NULL, 0, &timer_thread_id);
}

/* Set the next alarm */
void setTimer(TIMEVAL value)
{
	if(value == TIMEVAL_MAX)
		CancelWaitableTimer(timer);
	else
	{
		LARGE_INTEGER liDueTime;

		/* arg 2 of SetWaitableTimer take 100 ns interval */
		liDueTime.QuadPart = ((long long) (-1) * value * 10000);
		//printf("SetTimer(%llu)\n", value);

		if (!SetWaitableTimer(timer, &liDueTime, 0, NULL, NULL, FALSE))
		{
			printf("SetWaitableTimer failed (%d)\n", GetLastError());
		}
	}
}

/* Get the elapsed time since the last occured alarm */
TIMEVAL getElapsedTime(void)
{
  DWORD timetmp = GetTickCount();
  return (timetmp - timebuffer);
}

