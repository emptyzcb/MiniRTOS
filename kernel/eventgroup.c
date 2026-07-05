/*
 * eventgroup.c - Event Group Implementation
 *
 * 32-bit event flags with wait-any/wait-all/clear-on-exit support.
 * Tasks can wait for arbitrary combinations of bits.
 */

#include "eventgroup.h"
#include "task.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_EVENTGROUP

/* ========== Internal Functions ========== */

/*
 * Insert a task into the wait list sorted by priority (highest first).
 */
static void prv_block_task(os_eventgroup_t *eg, os_tcb_t *tcb,
                           uint32_t bits, uint32_t options,
                           os_tick_t timeout)
{
    os_tcb_t *iter;
    os_tcb_t *prev = NULL;

    tcb->blocked_on = eg;
    tcb->blocked_reason = OS_BLOCKED_ON_EVENT_WAIT;
    tcb->timed_out = 0;
    tcb->delay_ticks = timeout;
    tcb->event_wait_bits = bits;
    tcb->event_wait_options = options;
    tcb->event_return_bits = 0;

    os_task_remove_from_ready(tcb);
    tcb->state = OS_TASK_BLOCKED;

    for (iter = eg->wait_list; iter != NULL; iter = iter->next) {
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
        eg->wait_list = tcb;
    }
}

/*
 * Remove a specific task from the wait list.
 */
static void prv_unblock_task(os_eventgroup_t *eg, os_tcb_t *tcb)
{
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        eg->wait_list = tcb->next;
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
 * Check if a task's wait condition is met.
 */
static bool prv_check_condition(os_eventgroup_t *eg, os_tcb_t *tcb)
{
    uint32_t bits = tcb->event_wait_bits;
    uint32_t options = tcb->event_wait_options;

    if (options & OS_EVENT_WAIT_ALL) {
        return (eg->bits & bits) == bits;
    }
    /* OS_EVENT_WAIT_ANY */
    return (eg->bits & bits) != 0;
}

/* ========== Public API ========== */

os_status_t os_eventgroup_create(os_eventgroup_t *eg)
{
    if (eg == NULL) return OS_ERR_PARAM;

    eg->bits = 0;
    eg->wait_list = NULL;

    return OS_OK;
}

os_status_t os_eventgroup_delete(os_eventgroup_t *eg)
{
    os_tcb_t *tcb;

    if (eg == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    /* Wake all waiting tasks */
    while (eg->wait_list != NULL) {
        tcb = eg->wait_list;
        prv_unblock_task(eg, tcb);
        tcb->delay_ticks = 0;
        tcb->timed_out = 1;
        tcb->event_return_bits = 0;
        os_task_add_to_ready(tcb);
    }

    eg->bits = 0;

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_eventgroup_set_bits(os_eventgroup_t *eg, uint32_t bits)
{
    os_tcb_t *tcb;
    os_tcb_t *next;
    os_tcb_t *current;
    bool need_yield = false;

    if (eg == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    eg->bits |= bits;

    /* Check each waiting task */
    tcb = eg->wait_list;
    while (tcb != NULL) {
        next = tcb->next;

        if (prv_check_condition(eg, tcb)) {
            /* Condition met: unblock this task */
            tcb->event_return_bits = eg->bits;

            if (tcb->event_wait_options & OS_EVENT_CLEAR_ON_EXIT) {
                eg->bits &= ~tcb->event_wait_bits;
            }

            prv_unblock_task(eg, tcb);
            tcb->delay_ticks = 0;
            tcb->timed_out = 0;
            os_task_add_to_ready(tcb);

            current = (os_tcb_t*)os_task_get_current();
            if (tcb->priority < current->priority) {
                need_yield = true;
            }
        }

        tcb = next;
    }

    os_sched_exit_critical();

    if (need_yield) {
        os_sched_yield();
    }

    return OS_OK;
}

os_status_t os_eventgroup_set_bits_from_isr(os_eventgroup_t *eg, uint32_t bits)
{
    os_tcb_t *tcb;
    os_tcb_t *next;
    bool need_switch = false;

    if (eg == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    eg->bits |= bits;

    tcb = eg->wait_list;
    while (tcb != NULL) {
        next = tcb->next;

        if (prv_check_condition(eg, tcb)) {
            tcb->event_return_bits = eg->bits;

            if (tcb->event_wait_options & OS_EVENT_CLEAR_ON_EXIT) {
                eg->bits &= ~tcb->event_wait_bits;
            }

            prv_unblock_task(eg, tcb);
            tcb->delay_ticks = 0;
            tcb->timed_out = 0;
            os_task_add_to_ready(tcb);
            need_switch = true;
        }

        tcb = next;
    }

    os_sched_exit_critical();

    if (need_switch) {
        os_sched_request_switch_from_isr();
    }

    return OS_OK;
}

os_status_t os_eventgroup_clear_bits(os_eventgroup_t *eg, uint32_t bits)
{
    if (eg == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();
    eg->bits &= ~bits;
    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_eventgroup_clear_bits_from_isr(os_eventgroup_t *eg, uint32_t bits)
{
    if (eg == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();
    eg->bits &= ~bits;
    os_sched_exit_critical();

    return OS_OK;
}

uint32_t os_eventgroup_wait_bits(os_eventgroup_t *eg, uint32_t bits_to_wait,
                                 uint32_t options, os_tick_t timeout)
{
    os_tcb_t *current;
    uint32_t result;

    if (eg == NULL) return 0;

    os_sched_enter_critical();

    /* Check if condition already met */
    if (options & OS_EVENT_WAIT_ALL) {
        if ((eg->bits & bits_to_wait) == bits_to_wait) {
            result = eg->bits;
            if (options & OS_EVENT_CLEAR_ON_EXIT) {
                eg->bits &= ~bits_to_wait;
            }
            os_sched_exit_critical();
            return result;
        }
    } else {
        if ((eg->bits & bits_to_wait) != 0) {
            result = eg->bits;
            if (options & OS_EVENT_CLEAR_ON_EXIT) {
                eg->bits &= ~bits_to_wait;
            }
            os_sched_exit_critical();
            return result;
        }
    }

    /* Condition not met */
    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return 0;
    }

    /* Block on wait list */
    current = (os_tcb_t*)os_task_get_current();
    prv_block_task(eg, current, bits_to_wait, options, timeout);

    os_sched_exit_critical();
    os_sched_yield();

    /* Resumed: return the bits that were set when unblocked */
    return current->event_return_bits;
}

uint32_t os_eventgroup_get_bits(os_eventgroup_t *eg)
{
    uint32_t bits;

    if (eg == NULL) return 0;

    os_sched_enter_critical();
    bits = eg->bits;
    os_sched_exit_critical();

    return bits;
}

uint32_t os_eventgroup_get_bits_from_isr(os_eventgroup_t *eg)
{
    uint32_t bits;

    if (eg == NULL) return 0;

    os_sched_enter_critical();
    bits = eg->bits;
    os_sched_exit_critical();

    return bits;
}

void os_eventgroup_remove_task(os_eventgroup_t *eg, os_tcb_t *tcb)
{
    if (eg == NULL || tcb == NULL) return;

    os_sched_enter_critical();
    prv_unblock_task(eg, tcb);
    os_sched_exit_critical();
}

#endif /* OS_CONFIG_USE_EVENTGROUP */
