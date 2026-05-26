/*
 * queue.c - Message Queue Implementation
 *
 * Ring buffer based inter-task communication with blocking
 * send/receive and ISR-safe variants.
 */

#include "queue.h"
#include "task.h"
#include "scheduler.h"
#include "heap4.h"
#include "port.h"
#include <string.h>

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

#if OS_CONFIG_USE_QUEUE

/* ========== Internal Functions ========== */

/*
 * Insert a task into a blocked list sorted by priority (highest first).
 */
static void prv_block_task(os_tcb_t **list, os_tcb_t *tcb,
                           os_tick_t timeout, uint8_t reason,
                           void *owner)
{
    os_tcb_t *iter;
    os_tcb_t *prev = NULL;

    /* Set up blocking state in TCB */
    tcb->blocked_on = owner;
    tcb->blocked_reason = reason;
    tcb->timed_out = 0;
    tcb->delay_ticks = timeout;

    /* Remove from ready list */
    os_task_remove_from_ready(tcb);
    tcb->state = OS_TASK_BLOCKED;

    /* Insert sorted by priority (lower number = higher priority) */
    for (iter = *list; iter != NULL; iter = iter->next) {
        if (tcb->priority < iter->priority) {
            break;
        }
        prev = iter;
    }

    tcb->next = iter;
    tcb->prev = prev;

    if (iter != NULL) {
        iter->prev = tcb;
    }
    if (prev != NULL) {
        prev->next = tcb;
    } else {
        *list = tcb;
    }
}

/*
 * Remove a specific task from a blocked list.
 */
static void prv_unblock_task(os_tcb_t **list, os_tcb_t *tcb)
{
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        *list = tcb->next;
    }

    if (tcb->next != NULL) {
        tcb->next->prev = tcb->prev;
    }

    tcb->next = NULL;
    tcb->prev = NULL;
    tcb->blocked_on = NULL;
    tcb->blocked_reason = OS_BLOCKED_NONE;
}

/*
 * Wake the highest-priority task from a blocked list.
 * Returns the woken task, or NULL if list is empty.
 */
static os_tcb_t* prv_wake_task(os_tcb_t **list)
{
    os_tcb_t *tcb = *list;
    if (tcb == NULL) return NULL;

    prv_unblock_task(list, tcb);

    tcb->delay_ticks = 0;
    tcb->timed_out = 0;
    os_task_add_to_ready(tcb);

    return tcb;
}

/* ========== Public API ========== */

os_status_t os_queue_create(os_queue_t *queue, uint32_t item_size,
                            uint32_t max_items)
{
    if (queue == NULL || item_size == 0 || max_items == 0) {
        return OS_ERR_PARAM;
    }

    /* Allocate ring buffer from heap */
    queue->buf_size = item_size * max_items;
    queue->buffer = (uint8_t*)os_heap_alloc(queue->buf_size);
    if (queue->buffer == NULL) {
        return OS_ERR_NOMEM;
    }

    queue->item_size = item_size;
    queue->max_items = max_items;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->send_wait_list = NULL;
    queue->recv_wait_list = NULL;

    return OS_OK;
}

os_status_t os_queue_delete(os_queue_t *queue)
{
    os_tcb_t *tcb;

    if (queue == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    /* Wake all blocked senders */
    while (queue->send_wait_list != NULL) {
        tcb = prv_wake_task(&queue->send_wait_list);
        if (tcb != NULL) {
            tcb->timed_out = 1;
        }
    }

    /* Wake all blocked receivers */
    while (queue->recv_wait_list != NULL) {
        tcb = prv_wake_task(&queue->recv_wait_list);
        if (tcb != NULL) {
            tcb->timed_out = 1;
        }
    }

    /* Free buffer */
    if (queue->buffer != NULL) {
        os_heap_free(queue->buffer);
        queue->buffer = NULL;
    }

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_queue_send(os_queue_t *queue, const void *item,
                          os_tick_t timeout)
{
    os_tcb_t *woken_tcb = NULL;

    if (queue == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (queue->count < queue->max_items) {
        /* Space available: copy item to ring buffer */
        memcpy(&queue->buffer[queue->tail], item, queue->item_size);
        queue->tail = (queue->tail + queue->item_size) % queue->buf_size;
        queue->count++;

        /* Wake highest-priority receiver if any */
        woken_tcb = prv_wake_task(&queue->recv_wait_list);

#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_QUEUE_SEND, (uint32_t)queue, queue->count);
#endif

        os_sched_exit_critical();

        /* Preempt if woken task has higher priority */
        if (woken_tcb != NULL) {
            os_tcb_t *cur = (os_tcb_t*)os_task_get_current();
            if (woken_tcb->priority < cur->priority &&
                cur->state == OS_TASK_RUNNING) {
                os_sched_yield();
            }
        }
        return OS_OK;
    }

    /* Queue full */
    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return OS_ERR_FULL;
    }

    /* Block current task on send wait list */
    prv_block_task(&queue->send_wait_list,
                   (os_tcb_t*)os_task_get_current(),
                   timeout, OS_BLOCKED_ON_QUEUE_SEND, queue);

    os_sched_exit_critical();
    os_sched_yield();

    /* Resumed: check if we timed out */
    if (((os_tcb_t*)os_task_get_current())->timed_out) {
        return OS_ERR_TIMEOUT;
    }

#if OS_CONFIG_USE_TRACE
    os_trace_record(OS_TRACE_QUEUE_SEND, (uint32_t)queue, queue->count);
#endif

    return OS_OK;
}

os_status_t os_queue_send_from_isr(os_queue_t *queue, const void *item)
{
    os_tcb_t *woken_tcb = NULL;

    if (queue == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (queue->count < queue->max_items) {
        memcpy(&queue->buffer[queue->tail], item, queue->item_size);
        queue->tail = (queue->tail + queue->item_size) % queue->buf_size;
        queue->count++;

        woken_tcb = prv_wake_task(&queue->recv_wait_list);

        os_sched_exit_critical();

        if (woken_tcb != NULL) {
            os_sched_request_switch_from_isr();
        }
        return OS_OK;
    }

    os_sched_exit_critical();
    return OS_ERR_FULL;
}

os_status_t os_queue_receive(os_queue_t *queue, void *item,
                             os_tick_t timeout)
{
    os_tcb_t *woken_tcb = NULL;

    if (queue == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (queue->count > 0) {
        /* Data available: copy from ring buffer */
        memcpy(item, &queue->buffer[queue->head], queue->item_size);
        queue->head = (queue->head + queue->item_size) % queue->buf_size;
        queue->count--;

        /* Wake highest-priority sender if any */
        woken_tcb = prv_wake_task(&queue->send_wait_list);

#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_QUEUE_RECV, (uint32_t)queue, queue->count);
#endif

        os_sched_exit_critical();

        if (woken_tcb != NULL) {
            os_tcb_t *cur = (os_tcb_t*)os_task_get_current();
            if (woken_tcb->priority < cur->priority &&
                cur->state == OS_TASK_RUNNING) {
                os_sched_yield();
            }
        }
        return OS_OK;
    }

    /* Queue empty */
    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return OS_ERR_EMPTY;
    }

    /* Block current task on receive wait list */
    prv_block_task(&queue->recv_wait_list,
                   (os_tcb_t*)os_task_get_current(),
                   timeout, OS_BLOCKED_ON_QUEUE_RECV, queue);

    os_sched_exit_critical();
    os_sched_yield();

    if (((os_tcb_t*)os_task_get_current())->timed_out) {
        return OS_ERR_TIMEOUT;
    }

#if OS_CONFIG_USE_TRACE
    os_trace_record(OS_TRACE_QUEUE_RECV, (uint32_t)queue, queue->count);
#endif

    return OS_OK;
}

os_status_t os_queue_receive_from_isr(os_queue_t *queue, void *item)
{
    os_tcb_t *woken_tcb = NULL;

    if (queue == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (queue->count > 0) {
        memcpy(item, &queue->buffer[queue->head], queue->item_size);
        queue->head = (queue->head + queue->item_size) % queue->buf_size;
        queue->count--;

        woken_tcb = prv_wake_task(&queue->send_wait_list);

        os_sched_exit_critical();

        if (woken_tcb != NULL) {
            os_sched_request_switch_from_isr();
        }
        return OS_OK;
    }

    os_sched_exit_critical();
    return OS_ERR_EMPTY;
}

os_status_t os_queue_peek(os_queue_t *queue, void *item)
{
    if (queue == NULL || item == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (queue->count == 0) {
        os_sched_exit_critical();
        return OS_ERR_EMPTY;
    }

    memcpy(item, &queue->buffer[queue->head], queue->item_size);

    os_sched_exit_critical();
    return OS_OK;
}

uint32_t os_queue_get_count(os_queue_t *queue)
{
    if (queue == NULL) return 0;
    return queue->count;
}

uint32_t os_queue_get_spaces(os_queue_t *queue)
{
    if (queue == NULL) return 0;
    return queue->max_items - queue->count;
}

bool os_queue_is_empty(os_queue_t *queue)
{
    if (queue == NULL) return true;
    return queue->count == 0;
}

bool os_queue_is_full(os_queue_t *queue)
{
    if (queue == NULL) return false;
    return queue->count >= queue->max_items;
}

void os_queue_remove_task(os_queue_t *queue, os_tcb_t *tcb)
{
    if (queue == NULL || tcb == NULL) return;

    os_sched_enter_critical();

    /* Try send wait list */
    if (tcb->blocked_reason == OS_BLOCKED_ON_QUEUE_SEND) {
        prv_unblock_task(&queue->send_wait_list, tcb);
    }
    /* Try recv wait list */
    else if (tcb->blocked_reason == OS_BLOCKED_ON_QUEUE_RECV) {
        prv_unblock_task(&queue->recv_wait_list, tcb);
    }

    os_sched_exit_critical();
}

#endif /* OS_CONFIG_USE_QUEUE */
