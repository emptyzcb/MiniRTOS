/*
 * mailbox.c - Mailbox Implementation
 *
 * Thin wrapper around os_queue_t with capacity 1.
 * Provides a simplified API for single-element message passing.
 */

#include "mailbox.h"
#include "queue.h"
#include "scheduler.h"
#include <string.h>

#if OS_CONFIG_USE_MAILBOX

/* ========== Public API ========== */

os_status_t os_mailbox_create(os_mailbox_t *mb, uint32_t item_size)
{
    if (mb == NULL || item_size == 0) {
        return OS_ERR_PARAM;
    }

    return os_queue_create(&mb->queue, item_size, 1);
}

os_status_t os_mailbox_delete(os_mailbox_t *mb)
{
    if (mb == NULL) return OS_ERR_PARAM;

    return os_queue_delete(&mb->queue);
}

os_status_t os_mailbox_send(os_mailbox_t *mb, const void *item,
                            os_tick_t timeout)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    return os_queue_send(&mb->queue, item, timeout);
}

os_status_t os_mailbox_overwrite(os_mailbox_t *mb, const void *item)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();
    if (mb->queue.count == 0) {
        os_sched_exit_critical();
        return os_queue_send(&mb->queue, item, OS_WAIT_NONE);
    }

    memcpy(&mb->queue.buffer[mb->queue.head], item, mb->queue.item_size);
    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_mailbox_receive(os_mailbox_t *mb, void *item,
                               os_tick_t timeout)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    return os_queue_receive(&mb->queue, item, timeout);
}

os_status_t os_mailbox_send_from_isr(os_mailbox_t *mb, const void *item)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    return os_queue_send_from_isr(&mb->queue, item);
}

os_status_t os_mailbox_overwrite_from_isr(os_mailbox_t *mb, const void *item)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();
    if (mb->queue.count == 0) {
        os_sched_exit_critical();
        return os_queue_send_from_isr(&mb->queue, item);
    }

    memcpy(&mb->queue.buffer[mb->queue.head], item, mb->queue.item_size);
    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_mailbox_receive_from_isr(os_mailbox_t *mb, void *item)
{
    if (mb == NULL || item == NULL) return OS_ERR_PARAM;

    return os_queue_receive_from_isr(&mb->queue, item);
}

bool os_mailbox_is_empty(os_mailbox_t *mb)
{
    if (mb == NULL) return true;

    return os_queue_is_empty(&mb->queue);
}

bool os_mailbox_is_full(os_mailbox_t *mb)
{
    if (mb == NULL) return false;

    return os_queue_is_full(&mb->queue);
}

#endif /* OS_CONFIG_USE_MAILBOX */
