/*
 * semaphore.c - Binary and Counting Semaphore Implementation
 *
 * Standalone semaphore with priority-ordered blocked task list.
 * Uses direct-to-task transfer pattern on give.
 */

#include "semaphore.h"
#include "task.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

#if OS_CONFIG_USE_SEMAPHORE

/* ========== Internal Functions ========== */

/*
 * Insert a task into the wait list sorted by priority (highest first).
 */
static void prv_block_task(os_sem_t *sem, os_tcb_t *tcb, os_tick_t timeout)
{
    os_tcb_t *iter;
    os_tcb_t *prev = NULL;

    tcb->blocked_on = sem;
    tcb->blocked_reason = OS_BLOCKED_ON_SEM_TAKE;
    tcb->timed_out = 0;
    tcb->delay_ticks = timeout;

    os_task_remove_from_ready(tcb);
    tcb->state = OS_TASK_BLOCKED;

    for (iter = sem->wait_list; iter != NULL; iter = iter->next) {
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
        sem->wait_list = tcb;
    }
}

/*
 * Remove a specific task from the wait list.
 */
static void prv_unblock_task(os_sem_t *sem, os_tcb_t *tcb)
{
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        sem->wait_list = tcb->next;
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
 * Wake the highest-priority waiting task.
 */
static os_tcb_t* prv_wake_task(os_sem_t *sem)
{
    os_tcb_t *tcb = sem->wait_list;
    if (tcb == NULL) return NULL;

    prv_unblock_task(sem, tcb);

    tcb->delay_ticks = 0;
    tcb->timed_out = 0;
    os_task_add_to_ready(tcb);

    return tcb;
}

/* ========== Public API ========== */

os_status_t os_sem_create_binary(os_sem_t *sem)
{
    if (sem == NULL) return OS_ERR_PARAM;

    sem->count = 0;
    sem->max_count = 1;
    sem->wait_list = NULL;

    return OS_OK;
}

os_status_t os_sem_create_counting(os_sem_t *sem, uint32_t max_count,
                                   uint32_t initial_count)
{
    if (sem == NULL || max_count == 0) return OS_ERR_PARAM;
    if (initial_count > max_count) return OS_ERR_PARAM;

    sem->count = initial_count;
    sem->max_count = max_count;
    sem->wait_list = NULL;

    return OS_OK;
}

os_status_t os_sem_delete(os_sem_t *sem)
{
    os_tcb_t *tcb;

    if (sem == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    /* Wake all waiting tasks */
    while (sem->wait_list != NULL) {
        tcb = prv_wake_task(sem);
        if (tcb != NULL) {
            tcb->timed_out = 1;
        }
    }

    sem->count = 0;
    sem->max_count = 0;

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_sem_take(os_sem_t *sem, os_tick_t timeout)
{
    if (sem == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (sem->count > 0) {
        sem->count--;
#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_SEM_TAKE, (uint32_t)sem, sem->count);
#endif
        os_sched_exit_critical();
        return OS_OK;
    }

    if (timeout == OS_WAIT_NONE) {
        os_sched_exit_critical();
        return OS_ERR_EMPTY;
    }

    /* Block on wait list */
    prv_block_task(sem, (os_tcb_t*)os_task_get_current(), timeout);

    os_sched_exit_critical();
    os_sched_yield();

    /* Resumed */
    if (((os_tcb_t*)os_task_get_current())->timed_out) {
        return OS_ERR_TIMEOUT;
    }

#if OS_CONFIG_USE_TRACE
    os_trace_record(OS_TRACE_SEM_TAKE, (uint32_t)sem, sem->count);
#endif

    return OS_OK;
}

os_status_t os_sem_give(os_sem_t *sem)
{
    os_tcb_t *woken_tcb;
    os_tcb_t *current;

    if (sem == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (sem->wait_list != NULL) {
        /* Direct-to-task transfer: wake waiter without incrementing count */
        woken_tcb = prv_wake_task(sem);
        current = (os_tcb_t*)os_task_get_current();

#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_SEM_GIVE, (uint32_t)sem, sem->count);
#endif

        os_sched_exit_critical();

        if (woken_tcb != NULL && woken_tcb->priority < current->priority) {
            os_sched_yield();
        }
        return OS_OK;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
#if OS_CONFIG_USE_TRACE
        os_trace_record(OS_TRACE_SEM_GIVE, (uint32_t)sem, sem->count);
#endif
        os_sched_exit_critical();
        return OS_OK;
    }

    os_sched_exit_critical();
    return OS_ERR_FULL;
}

os_status_t os_sem_give_from_isr(os_sem_t *sem)
{
    os_tcb_t *woken_tcb;

    if (sem == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (sem->wait_list != NULL) {
        woken_tcb = prv_wake_task(sem);
        os_sched_exit_critical();

        if (woken_tcb != NULL) {
            os_sched_request_switch_from_isr();
        }
        return OS_OK;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
        os_sched_exit_critical();
        return OS_OK;
    }

    os_sched_exit_critical();
    return OS_ERR_FULL;
}

uint32_t os_sem_get_count(os_sem_t *sem)
{
    if (sem == NULL) return 0;
    return sem->count;
}

void os_sem_remove_task(os_sem_t *sem, os_tcb_t *tcb)
{
    if (sem == NULL || tcb == NULL) return;

    os_sched_enter_critical();
    prv_unblock_task(sem, tcb);
    os_sched_exit_critical();
}

#endif /* OS_CONFIG_USE_SEMAPHORE */
