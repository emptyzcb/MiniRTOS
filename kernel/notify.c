/*
 * notify.c - Task Notification Implementation
 *
 * Lightweight single-value notification per task.
 * Faster than queues for simple signaling patterns.
 */

#include "notify.h"
#include "task.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_TASK_NOTIFY

/* ========== Public API ========== */

os_status_t os_task_notify(os_task_handle_t handle, uint32_t value)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    os_tcb_t *current;

    if (tcb == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    tcb->notify_value = value;
    tcb->notify_pending = 1;

    /* If target task is blocked waiting for notification, wake it */
    if (tcb->state == OS_TASK_BLOCKED && tcb->blocked_reason == OS_BLOCKED_ON_NOTIFY) {
        os_task_remove_from_blocked(tcb);
        tcb->blocked_reason = OS_BLOCKED_NONE;
        tcb->delay_ticks = 0;
        tcb->timed_out = 0;
        os_task_add_to_ready(tcb);

        current = (os_tcb_t*)os_task_get_current();
        os_sched_exit_critical();

        if (tcb->priority < current->priority) {
            os_sched_yield();
        }
        return OS_OK;
    }

    os_sched_exit_critical();
    return OS_OK;
}

os_status_t os_task_notify_from_isr(os_task_handle_t handle, uint32_t value)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    uint32_t mask;

    if (tcb == NULL) return OS_ERR_PARAM;

    mask = os_port_enter_critical();

    tcb->notify_value = value;
    tcb->notify_pending = 1;

    if (tcb->state == OS_TASK_BLOCKED && tcb->blocked_reason == OS_BLOCKED_ON_NOTIFY) {
        os_task_remove_from_blocked(tcb);
        tcb->blocked_reason = OS_BLOCKED_NONE;
        tcb->delay_ticks = 0;
        tcb->timed_out = 0;
        os_task_add_to_ready(tcb);
        os_sched_request_switch_from_isr();
    }

    os_port_exit_critical(mask);
    return OS_OK;
}

os_status_t os_task_notify_wait(uint32_t *value_out, os_tick_t timeout)
{
    os_tcb_t *tcb = (os_tcb_t*)os_task_get_current();

    if (tcb == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (tcb->notify_pending) {
        /* Notification already pending: return immediately */
        if (value_out != NULL) {
            *value_out = tcb->notify_value;
        }
        tcb->notify_pending = 0;
        os_sched_exit_critical();
        return OS_OK;
    }

    /* No notification pending */
    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return OS_ERR_TIMEOUT;
    }

    /* Block waiting for notification */
    tcb->blocked_reason = OS_BLOCKED_ON_NOTIFY;
    tcb->timed_out = 0;

    /* Add to blocked list (for timeout management by tick handler) */
    if (timeout == OS_WAIT_FOREVER) {
        os_task_add_to_blocked(tcb, 0);
    } else {
        os_task_add_to_blocked(tcb, timeout);
    }

    os_sched_exit_critical();
    os_sched_yield();

    /* Woken up: check result */
    tcb = (os_tcb_t*)os_task_get_current();

    if (tcb->notify_pending) {
        if (value_out != NULL) {
            *value_out = tcb->notify_value;
        }
        tcb->notify_pending = 0;
        return OS_OK;
    }

    return OS_ERR_TIMEOUT;
}

#endif /* OS_CONFIG_USE_TASK_NOTIFY */
