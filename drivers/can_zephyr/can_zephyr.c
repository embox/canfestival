/*
This file is part of CanFestival.
Written by Edouard TISSERANT
See COPYING file for copyrights details.
*/

/* CanFestival CAN driver binding the Zephyr CAN subsystem.
 *
 * The CAN controllers usable by CanFestival are listed in a devicetree node
 * with compatible "canfestival,interfaces" (declared in a board overlay, see
 * dts/bindings/canfestival,interfaces.yaml). canOpen_driver() selects one of
 * them from the s_BOARD busname, mirroring can_socket.c: an all-digit busname
 * picks the Nth interface, otherwise the busname is matched against each
 * controller's devicetree node label.
 *
 * Received frames are delivered to a per-handle message queue by the CAN
 * subsystem (the filter callback runs in interrupt context, where k_msgq_put
 * is allowed); canReceive_driver() simply blocks on that queue. Only classic
 * CAN with 11-bit identifiers is used, as required by CANopen / CanFestival.
 */

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

#include "applicfg.h"
#include "can_driver.h"
#include "canfestival_config.h"

/* Devicetree node listing the CAN controllers usable by CanFestival. */
#define CF_IFACES_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(canfestival_interfaces)
#if DT_NODE_EXISTS(CF_IFACES_NODE)
#define CF_NUM_IFACES DT_PROP_LEN(CF_IFACES_NODE, interfaces)
#else
#define CF_NUM_IFACES 1
#endif

#ifndef CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH
#define CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH 16
#endif

#ifndef CONFIG_CANFESTIVAL_TX_TIMEOUT_MS
#define CONFIG_CANFESTIVAL_TX_TIMEOUT_MS 100
#endif

/* Internal handle: what CAN_HANDLE actually points to. One static slot per
 * declared CAN interface. */
struct cf_can_dev {
	char used;
	const struct device *dev;
	struct k_msgq rxq;
	char rxq_buf[CONFIG_CANFESTIVAL_RX_MSGQ_DEPTH * sizeof(struct can_frame)];
	int filter_id;
	atomic_t closing;
};

static struct cf_can_dev cf_can_devs[CF_NUM_IFACES];

/* Return true if s is a non-empty string of decimal digits only. */
static bool cf_all_digits(const char *s)
{
	if (s == NULL || *s == '\0') {
		return false;
	}
	for (; *s; ++s) {
		if (*s < '0' || *s > '9') {
			return false;
		}
	}
	return true;
}

/* Select a CAN controller from the "canfestival,interfaces" list, the same way
 * can_socket.c interprets busname: an all-digit busname picks the Nth declared
 * interface, otherwise busname is matched against each controller's node label.
 * When no such list node exists, fall back to the single controller from the
 * "zephyr,canbus" chosen node (busname is then ignored). */
static const struct device *cf_select_can(const char *busname)
{
#if DT_NODE_EXISTS(CF_IFACES_NODE)
	if (busname == NULL) {
		return NULL;
	}
	if (cf_all_digits(busname)) {
		switch (atoi(busname)) {
#define CF_IDX_CASE(node, prop, idx) \
		case idx: return DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx));
		DT_FOREACH_PROP_ELEM(CF_IFACES_NODE, interfaces, CF_IDX_CASE)
#undef CF_IDX_CASE
		default: return NULL;
		}
	}
#define CF_NAME_CASE(node, prop, idx)                                          \
	{                                                                      \
		static const char *const _lbl[] =                              \
			DT_NODELABEL_STRING_ARRAY(DT_PHANDLE_BY_IDX(node, prop, idx)); \
		for (int _i = 0; _i < (int)ARRAY_SIZE(_lbl); _i++) {           \
			if (strcmp(busname, _lbl[_i]) == 0) {                  \
				return DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx)); \
			}                                                      \
		}                                                              \
	}
	DT_FOREACH_PROP_ELEM(CF_IFACES_NODE, interfaces, CF_NAME_CASE)
#undef CF_NAME_CASE
	return NULL;
#elif DT_HAS_CHOSEN(zephyr_canbus)
	/* No interface list: use the single "zephyr,canbus" chosen controller. */
	ARG_UNUSED(busname);
	return DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
#else
	ARG_UNUSED(busname);
	return NULL;
#endif
}

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

#if defined DEBUG_MSG_CONSOLE_ON
	MSG("in : ");
	print_message(m);
#endif
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

#if defined DEBUG_MSG_CONSOLE_ON
	MSG("out : ");
	print_message(m);
#endif
	if (can_send(h->dev, &frame, K_MSEC(CONFIG_CANFESTIVAL_TX_TIMEOUT_MS),
		     NULL, NULL) != 0) {
		return 1;
	}

	return 0;
}

CAN_HANDLE canOpen_driver(s_BOARD *board)
{
	const struct device *dev = cf_select_can(board->busname);
	struct cf_can_dev *h = NULL;
	struct can_filter filter;
	UNS32 bitrate;
	int i;

	if (dev == NULL) {
		MSG("canOpen_driver: no CAN interface matching busname '%s'\n",
		    board->busname ? board->busname : "(null)");
		return NULL;
	}

	if (!device_is_ready(dev)) {
		MSG("canOpen_driver: CAN device not ready\n");
		return NULL;
	}

	for (i = 0; i < CF_NUM_IFACES; i++) {
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
