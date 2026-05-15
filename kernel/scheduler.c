/*
 * scheduler.c - Scheduler Implementation
 *
 * Priority-based preemptive scheduler with round-robin
 * time slicing within the same priority level.
 */

#include "scheduler.h"
#include "task.h"
#include "kernel.h"
#include "port.h"
#include "os_config.h"

/* ========== Internal Data ========== */

static bool scheduler_running = false;
static uint32_t critical_nesting = 0;
static volatile uint32_t yield_pending = 0;

#if OS_CONFIG_USE_STATS
static os_tick_t last_switch_tick = 0;
extern os_tick_t os_kernel_get_tick(void);
#endif

/* Forward declarations */
extern os_tcb_t* os_task_find_highest_ready(void);

/* ========== Public API ========== */

void os_sched_init(void)
{
    scheduler_running = false;
    critical_nesting = 0;
    yield_pending = 0;
}

void os_sched_start(void)
{
    /* Create the idle task */
    os_task_create_idle();

    scheduler_running = true;

    /* Select the first task to run */
    os_sched_select_next();

    /* Start the first task (never returns) */
    os_port_start_first_task();
}

bool os_sched_is_running(void)
{
    return scheduler_running;
}

void os_sched_yield(void)
{
    if (scheduler_running) {
        yield_pending = 1;
        os_port_yield();
    }
}

void os_sched_enter_critical(void)
{
    uint32_t mask = os_port_enter_critical();
    (void)mask;
    critical_nesting++;
}

void os_sched_exit_critical(void)
{
    if (critical_nesting > 0) {
        critical_nesting--;
        if (critical_nesting == 0) {
            os_port_exit_critical(0);
        }
    }
}

void os_sched_request_switch_from_isr(void)
{
    yield_pending = 1;
}

void os_sched_select_next(void)
{
    os_tcb_t *next_task = os_task_find_highest_ready();
    os_tcb_t *current;

    if (next_task == NULL) {
        return; /* No tasks ready - should not happen */
    }

    current = os_task_get_current();

    /* If same task, check round-robin */
    if (next_task == current) {
#if OS_CONFIG_USE_TIME_SLICING
        /* Round-robin: move current task to tail of its priority list */
        if (current != NULL) {
            os_task_remove_from_ready(current);
            os_task_add_to_ready(current);
            next_task = os_task_find_highest_ready();
            if (next_task == NULL) return;
        }
#else
        return; /* No switch needed */
#endif
    }

#if OS_CONFIG_USE_STATS
    /* Accumulate run time for the outgoing task */
    {
        os_tick_t now = os_kernel_get_tick();
        if (current != NULL && current->state == OS_TASK_RUNNING) {
            current->run_time_ticks += (now - last_switch_tick);
        }
        last_switch_tick = now;
    }
#endif

    /* Update current task pointer */
    if (current != NULL) {
        if (current->state == OS_TASK_RUNNING) {
            current->state = OS_TASK_READY;
        }
    }

    next_task->state = OS_TASK_RUNNING;
    os_task_set_current(next_task);
}

uint32_t os_sched_get_critical_nesting(void)
{
    return critical_nesting;
}
