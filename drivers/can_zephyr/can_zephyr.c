/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

/* CanFestival CAN driver binding the Zephyr CAN subsystem.
 *
 * The CAN controller is selected through the devicetree "zephyr,canbus"
 * chosen node (same convention as the canopennode module). Received frames
 * are delivered to a per-handle message queue by the CAN subsystem (the
 * filter callback runs in interrupt context, where k_msgq_put is allowed);
 * canReceive_driver() simply blocks on that queue. Only classic CAN with
 * 11-bit identifiers is used, as required by CANopen / CanFestival. 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

#include "applicfg.h"
#include "can_driver.h"
#include "config.h"

#ifndef CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH
#define CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH 16
#endif

#ifndef CONFIG_CANFESTIVAL_TX_TIMEOUT_MS
#define CONFIG_CANFESTIVAL_TX_TIMEOUT_MS 100
#endif

/* Internal handle: what CAN_HANDLE actually points to. One static slot per
 * CAN bus (MAX_CAN_BUS_ID comes from the generated config.h). */
struct cf_can_dev {
	char used;
	const struct device *dev;
	struct k_msgq rxq;
	char rxq_buf[CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH * sizeof(struct can_frame)];
	int filter_id;
	atomic_t closing;
};

static struct cf_can_dev cf_can_devs[MAX_CAN_BUS_ID];

/* Translate a CanFestival baudrate string into a bitrate in bit/s. */
static UNS32 cf_parse_baudrate(const char *baud)
{
	if (baud == NULL) {
		return 0;
	}

	struct { const char *name; UNS32 bps; } table[] = {
		{ "1M",   1000000 },
		{ "800K",  800000 },
		{ "500K",  500000 },
		{ "250K",  250000 },
		{ "125K",  125000 },
		{ "100K",  100000 },
		{ "50K",    50000 },
		{ "20K",    20000 },
		{ "10K",    10000 },
	};

	for (int i = 0; i < (int)(sizeof(table) / sizeof(table[0])); i++) {
		if (strcmp(baud, table[i].name) == 0) {
			return table[i].bps;
		}
	}

	return 0;
}

UNS8 canReceive_driver(CAN_HANDLE fd0, Message *m)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	struct can_frame frame;

	if (h == NULL) {
		return 1;
	}

	if (k_msgq_get(&h->rxq, &frame, K_FOREVER) != 0) {
		return 1;
	}

	/* Woken up for shutdown by canClose_driver()'s sentinel frame. */
	if (atomic_get(&h->closing)) {
		return 1;
	}

	m->cob_id = (UNS16)(frame.id & 0x7FF);
	m->len = frame.dlc;
	m->rtr = (frame.flags & CAN_FRAME_RTR) ? 1 : 0;
	if (m->rtr) {
		memset(m->data, 0, sizeof(m->data));
	} else {
		memcpy(m->data, frame.data, frame.dlc);
	}

	return 0;
}

UNS8 canSend_driver(CAN_HANDLE fd0, Message const *m)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	struct can_frame frame;

	if (h == NULL || h->dev == NULL) {
		return 1;
	}

	memset(&frame, 0, sizeof(frame));
	frame.id = m->cob_id & 0x7FF; /* standard 11-bit identifier */
	frame.dlc = m->len;
	if (m->rtr) {
		frame.flags |= CAN_FRAME_RTR;
	} else {
		memcpy(frame.data, m->data, m->len);
	}

	if (can_send(h->dev, &frame, K_MSEC(CONFIG_CANFESTIVAL_TX_TIMEOUT_MS),
		     NULL, NULL) != 0) {
		return 1;
	}

	return 0;
}

CAN_HANDLE canOpen_driver(s_BOARD *board)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	struct cf_can_dev *h = NULL;
	struct can_filter filter;
	UNS32 bitrate;
	int i;

	if (!device_is_ready(dev)) {
		MSG("canOpen_driver: CAN device not ready\n");
		return NULL;
	}

	for (i = 0; i < MAX_CAN_BUS_ID; i++) {
		if (!cf_can_devs[i].used) {
			h = &cf_can_devs[i];
			break;
		}
	}
	if (h == NULL) {
		MSG("canOpen_driver: no free CAN slot\n");
		return NULL;
	}

	bitrate = cf_parse_baudrate(board->baudrate);
	if (bitrate == 0) {
		MSG("canOpen_driver: invalid baudrate '%s'\n", board->baudrate);
		return NULL;
	}

	if (can_stop(dev) != 0) {
		/* may already be stopped, ignore */
	}

	if (can_set_bitrate(dev, bitrate) != 0) {
		MSG("canOpen_driver: failed to set bitrate %u\n", bitrate);
		return NULL;
	}

	if (can_set_mode(dev, CAN_MODE_NORMAL) != 0) {
		MSG("canOpen_driver: failed to set normal mode\n");
		return NULL;
	}

	memset(h, 0, sizeof(*h));
	h->dev = dev;
	atomic_set(&h->closing, 0);
	k_msgq_init(&h->rxq, h->rxq_buf, sizeof(struct can_frame),
		    CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH);

	/* Accept every standard frame; filtering is done by the stack. */
	filter.id = 0;
	filter.mask = 0;
	filter.flags = 0;
	h->filter_id = can_add_rx_filter_msgq(dev, &h->rxq, &filter);
	if (h->filter_id < 0) {
		MSG("canOpen_driver: failed to add rx filter (%d)\n", h->filter_id);
		return NULL;
	}

	if (can_start(dev) != 0) {
		MSG("canOpen_driver: failed to start CAN device\n");
		can_remove_rx_filter(dev, h->filter_id);
		return NULL;
	}

	h->used = 1;
	return (CAN_HANDLE)h;
}

int canClose_driver(CAN_HANDLE fd0)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	struct can_frame sentinel;

	if (h == NULL) {
		return 0;
	}

	atomic_set(&h->closing, 1);

	can_remove_rx_filter(h->dev, h->filter_id);
	can_stop(h->dev);

	/* Unblock canReceive_driver(), which is waiting in k_msgq_get(). */
	memset(&sentinel, 0, sizeof(sentinel));
	k_msgq_put(&h->rxq, &sentinel, K_NO_WAIT);

	h->used = 0;
	return 0;
}

UNS8 canChangeBaudRate_driver(CAN_HANDLE fd0, char *baud)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	UNS32 bitrate;

	if (h == NULL || h->dev == NULL) {
		return 1;
	}

	bitrate = cf_parse_baudrate(baud);
	if (bitrate == 0) {
		return 1;
	}

	if (can_stop(h->dev) != 0) {
		return 1;
	}
	if (can_set_bitrate(h->dev, bitrate) != 0) {
		return 1;
	}
	if (can_start(h->dev) != 0) {
		return 1;
	}

	return 0;
}
