/*
 * mutex.c - Mutex with Priority Inheritance
 *
 * Prevents priority inversion by temporarily boosting the priority
 * of the mutex holder when a higher-priority task waits for it.
 * Supports recursive locking by the same task.
 */

#include "mutex.h"
#include "task.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

#if OS_CONFIG_USE_MUTEX

/* ========== Internal Functions ========== */

/*
 * Insert a task into the wait list sorted by priority (highest first).
 */
static void prv_block_task(os_mutex_t *mutex, os_tcb_t *tcb, os_tick_t timeout)
{
    os_tcb_t *iter;
    os_tcb_t *prev = NULL;

    tcb->blocked_on = mutex;
    tcb->blocked_reason = OS_BLOCKED_ON_MUTEX_LOCK;
    tcb->timed_out = 0;
    tcb->delay_ticks = timeout;

    os_task_remove_from_ready(tcb);
    tcb->state = OS_TASK_BLOCKED;

    for (iter = mutex->wait_list; iter != NULL; iter = iter->next) {
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
        mutex->wait_list = tcb;
    }
}

/*
 * Remove a specific task from the wait list.
 */
static void prv_unblock_task(os_mutex_t *mutex, os_tcb_t *tcb)
{
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        mutex->wait_list = tcb->next;
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
 * Wake the highest-priority waiting task and transfer ownership.
 */
static os_tcb_t* prv_wake_and_transfer(os_mutex_t *mutex)
{
    os_tcb_t *tcb = mutex->wait_list;
    if (tcb == NULL) return NULL;

    prv_unblock_task(mutex, tcb);

    tcb->delay_ticks = 0;
    tcb->timed_out = 0;
    os_task_add_to_ready(tcb);

    /* Transfer ownership */
    mutex->owner = tcb;
    mutex->lock_count = 1;
    mutex->original_prio = tcb->priority;

    return tcb;
}

/*
 * Boost mutex owner's priority to the waiting task's priority.
 */
static void prv_boost_priority(os_mutex_t *mutex, os_prio_t new_prio)
{
    if (mutex->owner == NULL) return;

    /* Only boost if new priority is higher (lower number) */
    if (new_prio < mutex->owner->priority) {
        os_task_set_priority(mutex->owner, new_prio);
    }
}

/*
 * Restore mutex owner's priority to its original value.
 */
static void prv_restore_priority(os_mutex_t *mutex)
{
    if (mutex->owner == NULL) return;

    os_task_set_priority(mutex->owner, mutex->original_prio);
}

/* ========== Public API ========== */

os_status_t os_mutex_create(os_mutex_t *mutex)
{
    if (mutex == NULL) return OS_ERR_PARAM;

    mutex->owner = NULL;
    mutex->lock_count = 0;
    mutex->original_prio = 0;
    mutex->wait_list = NULL;

    return OS_OK;
}

os_status_t os_mutex_delete(os_mutex_t *mutex)
{
    os_tcb_t *tcb;

    if (mutex == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    /* Restore owner's priority if boosted */
    if (mutex->owner != NULL) {
        prv_restore_priority(mutex);
    }

    /* Wake all waiting tasks */
    while (mutex->wait_list != NULL) {
        tcb = mutex->wait_list;
        prv_unblock_task(mutex, tcb);
        tcb->delay_ticks = 0;
        tcb->timed_out = 1;
        os_task_add_to_ready(tcb);
    }

    mutex->owner = NULL;
    mutex->lock_count = 0;

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_mutex_lock(os_mutex_t *mutex, os_tick_t timeout)
{
    os_tcb_t *current;

    if (mutex == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    current = (os_tcb_t*)os_task_get_current();

    if (mutex->owner == NULL) {
        /* Mutex is free, acquire it */
        mutex->owner = current;
        mutex->lock_count = 1;
        mutex->original_prio = current->priority;
#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_MUTEX_LOCK, (uint32_t)mutex, (uint32_t)current->priority);
#endif
        os_sched_exit_critical();
        return OS_OK;
    }

    if (mutex->owner == current) {
        /* Recursive lock by the same task */
        mutex->lock_count++;
        os_sched_exit_critical();
        return OS_OK;
    }

    /* Mutex held by another task: apply priority inheritance */
    prv_boost_priority(mutex, current->priority);

    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return OS_ERR_WOULD_BLOCK;
    }

    /* Block on wait list */
    prv_block_task(mutex, current, timeout);

    os_sched_exit_critical();
    os_sched_yield();

    /* Resumed */
    if (current->timed_out) {
        return OS_ERR_TIMEOUT;
    }

#if OS_CONFIG_USE_TRACE
    os_trace_record(OS_TRACE_MUTEX_LOCK, (uint32_t)mutex, (uint32_t)current->priority);
#endif

    return OS_OK;
}

os_status_t os_mutex_unlock(os_mutex_t *mutex)
{
    os_tcb_t *current;
    os_tcb_t *woken_tcb;

    if (mutex == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    current = (os_tcb_t*)os_task_get_current();

    if (mutex->owner != current) {
        os_sched_exit_critical();
        return OS_ERR_STATE;
    }

    mutex->lock_count--;

    if (mutex->lock_count > 0) {
        /* Still locked (recursive) */
        os_sched_exit_critical();
        return OS_OK;
    }

    /* Fully released: restore original priority */
    prv_restore_priority(mutex);

    /* Clear ownership before transferring */
    mutex->owner = NULL;

    /* Transfer ownership to highest-priority waiter, if any */
    woken_tcb = prv_wake_and_transfer(mutex);

#if OS_CONFIG_USE_TRACE
    os_trace_record(OS_TRACE_MUTEX_UNLOCK, (uint32_t)mutex, 0);
#endif

    os_sched_exit_critical();

    if (woken_tcb != NULL && woken_tcb->priority < current->priority) {
        os_sched_yield();
    }

    return OS_OK;
}

os_tcb_t* os_mutex_get_owner(os_mutex_t *mutex)
{
    if (mutex == NULL) return NULL;
    return mutex->owner;
}

void os_mutex_remove_task(os_mutex_t *mutex, os_tcb_t *tcb)
{
    if (mutex == NULL || tcb == NULL) return;

    os_sched_enter_critical();
    prv_unblock_task(mutex, tcb);
    os_sched_exit_critical();
}

#endif /* OS_CONFIG_USE_MUTEX */
