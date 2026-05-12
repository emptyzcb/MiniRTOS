#ifndef OS_SCHEDULER_H
#define OS_SCHEDULER_H

#include "os_types.h"

/* ========== Scheduler API ========== */

/*
 * Initialize the scheduler.
 */
void os_sched_init(void);

/*
 * Start the scheduler. Does not return.
 */
void os_sched_start(void);

/*
 * Check if the scheduler is running.
 */
bool os_sched_is_running(void);

/*
 * Yield the current task, triggering a context switch.
 */
void os_sched_yield(void);

/*
 * Enter a critical section (disable interrupts).
 */
void os_sched_enter_critical(void);

/*
 * Exit a critical section (re-enable interrupts).
 */
void os_sched_exit_critical(void);

/*
 * Called from ISR to request a context switch on ISR exit.
 */
void os_sched_request_switch_from_isr(void);

/*
 * Select the next task to run (finds highest priority ready task).
 * Called internally by the port layer.
 */
void os_sched_select_next(void);

/*
 * Get the current critical section nesting depth.
 */
uint32_t os_sched_get_critical_nesting(void);

#endif /* OS_SCHEDULER_H */
