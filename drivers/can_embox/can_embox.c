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

#include <errno.h>
#include <fcntl.h>
#include <linux/can.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <drivers/char_dev.h>

#include "can_driver.h"

CAN_HANDLE canOpen_driver(s_BOARD *board) {
	int fd;

	fd = char_dev_open(board->busname, O_RDWR);
	if (fd < 0) {
		return NULL;
	}

	return (CAN_HANDLE)(intptr_t)fd;
}

int canClose_driver(CAN_HANDLE fd0) {
	int fd;

	fd = (intptr_t)fd0;

	close(fd);

	return 0;
}

UNS8 canReceive_driver(CAN_HANDLE fd0, Message *m) {
	struct can_frame frame;
	int res;
	int fd;

	fd = (intptr_t)fd0;

	while (1) {
		res = read(fd, &frame, sizeof(frame));
		if (res == sizeof(frame)) {
			break;
		}
		if (res < 0) {
			if (errno == EINTR) {
				continue;
			}
			else if (errno == EAGAIN) {
				fprintf(stderr, "Recv failed: %s\n", strerror(errno));
				usleep(20000); /* wait 20ms */
				continue;
			}
		}
		return 1; // NOT OK
	}

	m->cob_id = frame.can_id & CAN_EFF_MASK;
	m->len = frame.len;
	m->rtr = (frame.can_id & CAN_RTR_FLAG) ? 1 : 0;
	memcpy(m->data, frame.data, 8);

#if defined DEBUG_MSG_CONSOLE_ON
	MSG("in : ");
	print_message(m);
#endif

	return 0; // OK
}

UNS8 canSend_driver(CAN_HANDLE fd0, Message const *m) {
	struct can_frame frame;
	int res;
	int fd;

	fd = (intptr_t)fd0;

	frame.len = m->len;

	frame.can_id = m->cob_id;
	if (frame.can_id >= 0x800) {
		frame.can_id |= CAN_EFF_FLAG;
	}

	if (m->rtr) {
		frame.can_id |= CAN_RTR_FLAG;
	}
	else {
		memcpy(frame.data, m->data, 8);
	}

#if defined DEBUG_MSG_CONSOLE_ON
	MSG("out : ");
	print_message(m);
#endif

	res = write(fd, &frame, sizeof(frame));
	if (res <= 0) {
		fprintf(stderr, "Send failed: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

UNS8 canChangeBaudRate_driver(CAN_HANDLE fd, char *baud) {
	return 0;
}
