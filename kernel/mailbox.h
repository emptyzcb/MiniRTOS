/*
 * mailbox.h - Mailbox (Single-Element Message Queue)
 *
 * A mailbox is a queue with capacity 1, optimized for single-value
 * signaling between tasks. Sending to a full mailbox overwrites the
 * previous value (or blocks until space is available, depending on
 * the API variant).
 *
 * Semantics:
 *   - os_mailbox_send(): if full, block until space or timeout
 *   - os_mailbox_overwrite(): if full, replace the pending value
 *   - os_mailbox_receive(): if empty, block until data or timeout
 *   - ISR variants are non-blocking
 */

#ifndef OS_MAILBOX_H
#define OS_MAILBOX_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_MAILBOX

#include "queue.h"

/* ========== Mailbox Control Block ========== */

typedef struct os_mailbox {
    os_queue_t  queue;              /* Underlying queue (capacity 1) */
} os_mailbox_t;

/* ========== Mailbox API ========== */

/*
 * Create a mailbox for items of the given size.
 * Internally creates a queue with capacity 1.
 */
os_status_t os_mailbox_create(os_mailbox_t *mb, uint32_t item_size);

/*
 * Delete the mailbox. Wakes all blocked tasks with timeout.
 */
os_status_t os_mailbox_delete(os_mailbox_t *mb);

/*
 * Send an item to the mailbox. Blocks if full.
 *   timeout: OS_WAIT_FOREVER, OS_WAIT_NONE, or tick count.
 */
os_status_t os_mailbox_send(os_mailbox_t *mb, const void *item,
                            os_tick_t timeout);

/*
 * Write an item to the mailbox, replacing the pending item if full.
 * Never blocks. If the mailbox is empty, behaves like a non-blocking send.
 */
os_status_t os_mailbox_overwrite(os_mailbox_t *mb, const void *item);

/*
 * Receive an item from the mailbox. Blocks if empty.
 *   timeout: OS_WAIT_FOREVER, OS_WAIT_NONE, or tick count.
 */
os_status_t os_mailbox_receive(os_mailbox_t *mb, void *item,
                               os_tick_t timeout);

/*
 * ISR-safe send (non-blocking). Returns OS_ERR_FULL if mailbox is full.
 */
os_status_t os_mailbox_send_from_isr(os_mailbox_t *mb, const void *item);

/*
 * ISR-safe overwrite. Never blocks and replaces the pending item if full.
 */
os_status_t os_mailbox_overwrite_from_isr(os_mailbox_t *mb, const void *item);

/*
 * ISR-safe receive (non-blocking). Returns OS_ERR_EMPTY if mailbox is empty.
 */
os_status_t os_mailbox_receive_from_isr(os_mailbox_t *mb, void *item);

/*
 * Check if the mailbox is empty.
 */
bool os_mailbox_is_empty(os_mailbox_t *mb);

/*
 * Check if the mailbox is full.
 */
bool os_mailbox_is_full(os_mailbox_t *mb);

#endif /* OS_CONFIG_USE_MAILBOX */

#endif /* OS_MAILBOX_H */
