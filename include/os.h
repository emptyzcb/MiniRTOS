/*
 * os.h - MiniOS Public API
 *
 * Single header to include for application code.
 */

#ifndef OS_H
#define OS_H

#include "os_types.h"
#include "os_config.h"
#include "kernel.h"
#include "task.h"
#include "heap4.h"
#include "scheduler.h"
#include "port.h"

/* ========== Convenience Macros ========== */

/* Delay the current task */
#define OS_DELAY(ticks)         os_task_delay(ticks)

/* Delay in milliseconds */
#define OS_DELAY_MS(ms)         os_task_delay((ms) * OS_CONFIG_TICK_RATE_HZ / 1000)

/* Delay in seconds */
#define OS_DELAY_SEC(sec)       os_task_delay((sec) * OS_CONFIG_TICK_RATE_HZ)

/* Get current tick count */
#define OS_GET_TICK()           os_kernel_get_tick()

/* Enter/Exit critical section */
#define OS_ENTER_CRITICAL()     os_sched_enter_critical()
#define OS_EXIT_CRITICAL()      os_sched_exit_critical()

/* Yield current task */
#define OS_YIELD()              os_sched_yield()

#endif /* OS_H */
