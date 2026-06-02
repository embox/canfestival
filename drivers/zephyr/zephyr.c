/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

/* CanFestival platform wrapper for Zephyr.
 *
 * Mirrors drivers/unix/unix.c but without dynamic loading: the module is
 * always built with NOT_USE_DYNAMIC_LOADING, so DLL_CALL(f) resolves directly
 * to the statically linked f##_driver symbol of the can_zephyr driver. */

#include "data.h"
#include "canfestival.h"
#include "timers_driver.h"
#include "config.h"

/* One CAN port per CAN bus; each port statically owns its receive task control
 * block and stack through the embedded TASK_HANDLE. */
#define MAX_NB_CAN_PORTS MAX_CAN_BUS_ID

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
		res = DLL_CALL(canSend)(((CANPort*)port)->fd, m);
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

	if(i == MAX_NB_CAN_PORTS){
		MSG("CanOpen : no free CAN port\n");
		return NULL;
	}

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
		res = DLL_CALL(canChangeBaudRate)(((CANPort*)port)->fd, baud);
		return res; // OK
	}
	return 1; // NOT OK
}
