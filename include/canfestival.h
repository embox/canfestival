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

#ifndef CANFESTIVAL_H_
#define CANFESTIVAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "canfestival_datatypes.h"

/** @defgroup userapi User API */

/** @defgroup can CAN management
 *  @ingroup userapi
 */

/**
 * @brief Send a CAN message
 * @param port CanFestival file descriptor
 * @param *m The CAN message to send
 * @return 0 if succes
 */
UNS8 canSend(CAN_PORT port, Message *m);

/**
 * @ingroup can
 * @brief Query the local CAN controller state and bus error counters
 * @param *d Pointer to the CAN object data structure
 * @param *state Receives the controller state (driver-specific encoding)
 * @param *txerr Receives the transmit error counter
 * @param *rxerr Receives the receive error counter
 * @return 0 on success; non-zero if unsupported or unavailable (outputs untouched)
 * @note Only implemented by the Zephyr driver; other targets do not provide it.
 */
UNS8 canGetState(CO_Data *d, UNS8 *state, UNS8 *txerr, UNS8 *rxerr);

/**
 * @ingroup can
 * @brief Open a CANOpen device
 * @param *board Pointer to the board structure that contains busname and baudrate
 * @param *d Pointer to the CAN object data structure
 * @return
 *       - CanFestival file descriptor is returned upon success.
 *       - NULL is returned if the CANOpen board can't be opened.
 */
CAN_PORT canOpen(s_BOARD *board, CO_Data * d);

/**
 * @ingroup can
 * @brief Close a CANOpen device
 * @param *d Pointer to the CAN object data structure
 * @return
 *       - 0 is returned upon success.
 *       - errorcode if error. (if implemented)
 */
int canClose(CO_Data * d);

/**
 * @ingroup can
 * @brief Change the CANOpen device baudrate
 * @param port CanFestival file descriptor
 * @param *baud The new baudrate to assign
 * @return
 *       - 0 is returned upon success or if not supported by the CAN driver.
 *       - errorcode from the CAN driver is returned if an error occurs. (if implemented in the CAN driver)
 */
UNS8 canChangeBaudRate(CAN_PORT port, char* baud);

#ifdef USE_DYNAMIC_CAN_DRIVER_LOADING

/**
 * @ingroup can
 * @brief Load CAN driver interface.
 * @param *driver_name The location of the library to load
 * @return
 *       - handle of the CAN driver interface is returned upon success.
 *       - NULL is returned if the CAN driver interface can't be loaded.
 */
LIB_HANDLE LoadCanDriver(const char* driver_name);

/**
 * @ingroup can
 * @brief Unload CAN driver interface
 * @param handle The library handle
 * @return
 *       -  0 is returned upon success.
 *       - -1 is returned if the CAN driver interface can't be unloaded.
 */
UNS8 UnLoadCanDriver(LIB_HANDLE handle);

#endif /* USE_DYNAMIC_CAN_DRIVER_LOADING */

#ifdef __cplusplus
};
#endif

#endif /* CANFESTIVAL_H_ */
