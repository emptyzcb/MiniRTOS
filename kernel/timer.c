/*
 * timer.c - Software Timer Implementation
 *
 * Callback-based timers driven by the system tick.
 * Uses a dedicated timer service task so callbacks run in
 * task context (not ISR context) and can call blocking APIs.
 *
 * Architecture:
 *   os_timer_tick() [called from SysTick ISR]
 *     -> decrements active timer list
 *     -> moves expired timers to expired_list
 *     -> gives timer_sem to wake service task
 *
 *   timer_service_task [runs at OS_PRIO_LOWEST - 1]
 *     -> waits on timer_sem
 *     -> processes expired_list, calls callbacks
 */

#include "timer.h"
#include "semaphore.h"
#include "task.h"
#include "scheduler.h"
#include "heap4.h"
#include "port.h"
#include <string.h>

#if OS_CONFIG_USE_SOFTWARE_TIMERS

/* ========== Internal Data ========== */

static os_timer_t   *active_timer_list = NULL;  /* Sorted by remaining ticks */
static os_timer_t   *expired_list = NULL;       /* Expired timers to process */

static os_sem_t     timer_sem;                   /* Signals the service task */
static os_task_handle_t timer_task_handle = NULL;
static os_stack_t   timer_task_stack[OS_CONFIG_TIMER_SERVICE_STACK / sizeof(os_stack_t)];

static bool         timer_initialized = false;

/* ========== Internal Functions ========== */

/*
 * Insert a timer into the active list sorted by remaining ticks (ascending).
 * Timers with the same remaining are inserted after existing ones (FIFO).
 */
static void prv_timer_insert_sorted(os_timer_t *timer)
{
    os_timer_t *iter;
    os_timer_t *prev = NULL;

    timer->next = NULL;

    for (iter = active_timer_list; iter != NULL; iter = iter->next) {
        if (timer->remaining < iter->remaining) {
            break;
        }
        prev = iter;
    }

    /* Adjust remaining relative to preceding timers */
    if (prev != NULL) {
        timer->remaining -= prev->remaining;
    }

    /* Adjust remaining of following timers */
    if (iter != NULL) {
        iter->remaining -= timer->remaining;
    }

    timer->next = iter;
    if (prev != NULL) {
        prev->next = timer;
    } else {
        active_timer_list = timer;
    }
}

/*
 * Remove a timer from the active list.
 */
static void prv_timer_remove_from_active(os_timer_t *timer)
{
    os_timer_t *iter;
    os_timer_t *prev = NULL;

    for (iter = active_timer_list; iter != NULL; iter = iter->next) {
        if (iter == timer) {
            if (prev != NULL) {
                prev->next = timer->next;
            } else {
                active_timer_list = timer->next;
            }

            /* Transfer remaining to next timer */
            if (timer->next != NULL) {
                timer->next->remaining += timer->remaining;
            }
            break;
        }
        prev = iter;
    }
}

/*
 * Timer service task: waits for expired timers and calls their callbacks.
 */
static void timer_service_task(void *param)
{
    os_timer_t *timer;
    os_timer_t *list;

    (void)param;

    while (1) {
        /* Wait for a timer to expire */
        os_sem_take(&timer_sem, OS_WAIT_FOREVER);

        /* Grab all expired timers */
        os_sched_enter_critical();
        list = expired_list;
        expired_list = NULL;
        os_sched_exit_critical();

        /* Process each expired timer */
        while (list != NULL) {
            timer = list;
            list = timer->next;
            timer->next = NULL;

            /* Call the callback */
            if (timer->callback != NULL) {
                timer->callback(timer);
            }
        }
    }
}

/* ========== Public API ========== */

os_status_t os_timer_init(void)
{
    if (timer_initialized) return OS_OK;

    /* Create binary semaphore for signaling the service task */
    os_sem_create_binary(&timer_sem);

    /* Create the timer service task at low priority */
    os_status_t ret = os_task_create(timer_service_task, "TMR",
                                     NULL,
                                     OS_PRIO_LOWEST > 0 ? OS_PRIO_LOWEST - 1 : 0,
                                     timer_task_stack,
                                     sizeof(timer_task_stack),
                                     &timer_task_handle);
    if (ret != OS_OK) return ret;

    timer_initialized = true;
    return OS_OK;
}

os_status_t os_timer_create(os_timer_t *timer, const char *name,
                            os_tick_t period, os_timer_type_t type,
                            os_timer_callback_t callback)
{
    if (timer == NULL || callback == NULL || period == 0) {
        return OS_ERR_PARAM;
    }

    (void)name; /* Name not stored in this implementation */

    timer->callback = callback;
    timer->period = period;
    timer->remaining = 0;
    timer->type = type;
    timer->active = false;
    timer->next = NULL;

    return OS_OK;
}

os_status_t os_timer_delete(os_timer_t *timer, os_tick_t timeout)
{
    if (timer == NULL) return OS_ERR_PARAM;

    (void)timeout;

    os_sched_enter_critical();

    if (timer->active) {
        prv_timer_remove_from_active(timer);
        timer->active = false;
    }

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_timer_start(os_timer_t *timer, os_tick_t timeout)
{
    if (timer == NULL) return OS_ERR_PARAM;

    (void)timeout;

    os_sched_enter_critical();

    if (timer->active) {
        /* Already running, remove and re-insert */
        prv_timer_remove_from_active(timer);
    }

    timer->remaining = timer->period;
    timer->active = true;
    prv_timer_insert_sorted(timer);

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_timer_stop(os_timer_t *timer, os_tick_t timeout)
{
    if (timer == NULL) return OS_ERR_PARAM;

    (void)timeout;

    os_sched_enter_critical();

    if (timer->active) {
        prv_timer_remove_from_active(timer);
        timer->active = false;
    }

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_timer_reset(os_timer_t *timer, os_tick_t timeout)
{
    if (timer == NULL) return OS_ERR_PARAM;

    (void)timeout;

    os_sched_enter_critical();

    if (timer->active) {
        prv_timer_remove_from_active(timer);
        timer->remaining = timer->period;
        prv_timer_insert_sorted(timer);
    }

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_timer_change_period(os_timer_t *timer, os_tick_t new_period,
                                   os_tick_t timeout)
{
    if (timer == NULL || new_period == 0) return OS_ERR_PARAM;

    (void)timeout;

    os_sched_enter_critical();

    timer->period = new_period;

    if (timer->active) {
        prv_timer_remove_from_active(timer);
        timer->remaining = new_period;
        prv_timer_insert_sorted(timer);
    }

    os_sched_exit_critical();

    return OS_OK;
}

bool os_timer_is_active(os_timer_t *timer)
{
    if (timer == NULL) return false;
    return timer->active;
}

void os_timer_tick(void)
{
    os_timer_t *timer;
    bool need_signal = false;

    if (active_timer_list == NULL) return;

    /* Decrement the first timer's remaining ticks */
    active_timer_list->remaining--;

    /* Move all expired timers to the expired list */
    while (active_timer_list != NULL && active_timer_list->remaining == 0) {
        timer = active_timer_list;
        active_timer_list = timer->next;

        if (timer->type == OS_TIMER_AUTO_RELOAD) {
            /* Re-insert with full period */
            timer->remaining = timer->period;
            timer->next = NULL;
            prv_timer_insert_sorted(timer);
        } else {
            /* One-shot: mark as inactive */
            timer->active = false;
        }

        /* Add to expired list for service task */
        timer->next = expired_list;
        expired_list = timer;
        need_signal = true;
    }

    /* Wake the timer service task if any timers expired */
    if (need_signal) {
        os_sem_give(&timer_sem);
    }
}

#endif /* OS_CONFIG_USE_SOFTWARE_TIMERS */
