#ifndef OS_STATS_H
#define OS_STATS_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_STATS

/*
 * Get CPU usage percentage for a specific task (0-100).
 * Returns the percentage of total system ticks spent running this task.
 */
uint32_t os_stats_get_cpu_usage(os_task_handle_t handle);

/*
 * Get the raw run time in ticks for a specific task.
 */
os_tick_t os_stats_get_run_time(os_task_handle_t handle);

/*
 * Reset all CPU usage statistics to zero.
 */
void os_stats_reset(void);

#endif /* OS_CONFIG_USE_STATS */

#endif /* OS_STATS_H */
