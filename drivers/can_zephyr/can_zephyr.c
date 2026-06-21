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

#ifndef CONFIG_CANFESTIVAL_TX_MSGQ_DEPTH
#define CONFIG_CANFESTIVAL_TX_MSGQ_DEPTH 16
#endif

#ifndef CONFIG_CANFESTIVAL_TX_THREAD_PRIO
#define CONFIG_CANFESTIVAL_TX_THREAD_PRIO 5
#endif

#ifndef CONFIG_CANFESTIVAL_TX_STACK_SIZE
#define CONFIG_CANFESTIVAL_TX_STACK_SIZE 1024
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

	/* Non-blocking transmit path. canSend_driver() never touches the CAN
	 * device directly: it inserts the frame into txq (a software priority
	 * queue kept sorted by ascending CAN id, i.e. highest priority at the
	 * head) and wakes tx_thread. The thread hands one frame at a time to
	 * can_send() with a completion callback; the next frame is dequeued only
	 * once cf_tx_done() fires. Single-in-flight keeps same-COB-id segmented
	 * transfers (SDO) in FIFO order, as required by the Zephyr CAN API, and
	 * lets a newly-arrived higher-priority frame overtake between sends. */
	struct can_frame txq[CONFIG_CANFESTIVAL_TX_MSGQ_DEPTH];
	int txq_count;
	struct k_spinlock txq_lock;
	bool tx_busy;
	struct k_sem tx_wake;
	struct k_thread tx_thread;
	K_KERNEL_STACK_MEMBER(tx_stack, CONFIG_CANFESTIVAL_TX_STACK_SIZE);
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

/* Frame ordering on the CAN bus: the lower the id, the higher the priority; a
 * data frame outranks a remote frame with the same id. Returns <0 if a sorts
 * before b (a is higher priority), >0 if after, 0 if equal priority. */
static int cf_frame_cmp(const struct can_frame *a, const struct can_frame *b)
{
	if (a->id != b->id) {
		return (int)a->id - (int)b->id;
	}
	/* Data (RTR clear) before remote (RTR set) at equal id. */
	return (a->flags & CAN_FRAME_RTR) - (b->flags & CAN_FRAME_RTR);
}

/* Insert frame into the priority queue, keeping it sorted by ascending
 * priority (head = highest priority). Stable: among equal-priority frames the
 * new one goes last, preserving FIFO order for segmented same-COB-id transfers.
 * When the queue is full, the lowest-priority frame is evicted so priority
 * prevails. Returns true if the incoming frame was stored, false if it was the
 * one dropped. Caller must hold txq_lock. */
static bool cf_txq_insert(struct cf_can_dev *h, const struct can_frame *frame)
{
	int pos;

	if (h->txq_count == CONFIG_CANFESTIVAL_TX_MSGQ_DEPTH) {
		/* Full: drop the incoming frame unless it outranks the current
		 * lowest-priority one (the tail), in which case evict the tail. */
		if (cf_frame_cmp(frame, &h->txq[h->txq_count - 1]) >= 0) {
			return false;
		}
		h->txq_count--;
	}

	/* Stable insertion sort: place after all frames of equal-or-higher
	 * priority. */
	for (pos = h->txq_count; pos > 0; pos--) {
		if (cf_frame_cmp(&h->txq[pos - 1], frame) <= 0) {
			break;
		}
		h->txq[pos] = h->txq[pos - 1];
	}
	h->txq[pos] = *frame;
	h->txq_count++;
	return true;
}

/* Pop the highest-priority frame (head) into *out. Returns false if empty.
 * Caller must hold txq_lock. */
static bool cf_txq_pop(struct cf_can_dev *h, struct can_frame *out)
{
	if (h->txq_count == 0) {
		return false;
	}
	*out = h->txq[0];
	h->txq_count--;
	memmove(&h->txq[0], &h->txq[1], h->txq_count * sizeof(h->txq[0]));
	return true;
}

/* can_send() completion callback - runs in interrupt context, so it must not
 * call can_send() itself (the Zephyr CAN send path takes a mutex). Release the
 * single-in-flight gate and wake the TX thread to send the next queued frame. */
static void cf_tx_done(const struct device *dev, int error, void *user_data)
{
	struct cf_can_dev *h = user_data;
	k_spinlock_key_t key;

	ARG_UNUSED(dev);
	ARG_UNUSED(error);

	key = k_spin_lock(&h->txq_lock);
	h->tx_busy = false;
	k_spin_unlock(&h->txq_lock, key);
	k_sem_give(&h->tx_wake);
}

/* TX worker: drains the priority queue into the CAN driver, one frame in flight
 * at a time. */
static void cf_tx_thread(void *p1, void *p2, void *p3)
{
	struct cf_can_dev *h = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (!atomic_get(&h->closing)) {
		struct can_frame frame;
		k_spinlock_key_t key;
		int err;

		k_sem_take(&h->tx_wake, K_FOREVER);

		key = k_spin_lock(&h->txq_lock);
		if (h->tx_busy || !cf_txq_pop(h, &frame)) {
			k_spin_unlock(&h->txq_lock, key);
			continue;
		}
		h->tx_busy = true;
		k_spin_unlock(&h->txq_lock, key);

		err = can_send(h->dev, &frame, K_NO_WAIT, cf_tx_done, h);
		if (err != 0) {
			key = k_spin_lock(&h->txq_lock);
			h->tx_busy = false;
			if (err == -EAGAIN) {
				(void)cf_txq_insert(h, &frame);
			}
			k_spin_unlock(&h->txq_lock, key);
			if (err == -EAGAIN) {
				/* No mailbox free: nothing is in flight, so no
				 * cf_tx_done() will re-wake us. Back off briefly
				 * and re-arm to retry the requeued frame. */
				k_sleep(K_MSEC(1));
				k_sem_give(&h->tx_wake);
			} else {
				MSG("canSend: can_send failed (%d), frame dropped\n",
				    err);
			}
		}
	}
}

UNS8 canSend_driver(CAN_HANDLE fd0, Message const *m)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	struct can_frame frame;
	k_spinlock_key_t key;
	bool stored;

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

	/* Non-blocking: queue the frame (priority-ordered, lowest priority
	 * dropped on overflow) and let the TX thread transmit it. */
	key = k_spin_lock(&h->txq_lock);
	stored = cf_txq_insert(h, &frame);
	k_spin_unlock(&h->txq_lock, key);
	k_sem_give(&h->tx_wake);

	/* Report failure only when this very frame was dropped on overflow. */
	return stored ? 0 : 1;
}

/* Report the local CAN controller state and bus error counters. All three
 * outputs fit in a byte (enum can_state is small; the error counters are
 * uint8_t). On failure the outputs are left untouched so the caller keeps the
 * last known value. */
UNS8 canGetState_driver(CAN_HANDLE fd0, UNS8 *state, UNS8 *txerr, UNS8 *rxerr)
{
	struct cf_can_dev *h = (struct cf_can_dev *)fd0;
	enum can_state st;
	struct can_bus_err_cnt cnt;

	if (h == NULL || h->dev == NULL) {
		return 1;
	}

	if (can_get_state(h->dev, &st, &cnt) != 0) {
		return 1;
	}

	*state = (UNS8)st;
	*txerr = cnt.tx_err_cnt;
	*rxerr = cnt.rx_err_cnt;
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
	h->tx_busy = false;
	h->txq_count = 0;
	k_sem_init(&h->tx_wake, 0, 1);
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

	k_thread_create(&h->tx_thread, h->tx_stack,
			K_KERNEL_STACK_SIZEOF(h->tx_stack),
			cf_tx_thread, h, NULL, NULL,
			CONFIG_CANFESTIVAL_TX_THREAD_PRIO, 0, K_NO_WAIT);

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

	/* Wake the TX thread so it observes the closing flag and exits. */
	k_sem_give(&h->tx_wake);
	k_thread_join(&h->tx_thread, K_FOREVER);

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
