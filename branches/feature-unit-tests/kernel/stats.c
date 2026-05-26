/*
 * stats.c - CPU Usage Statistics
 *
 * Tracks per-task run time by accumulating ticks on each context switch.
 * Usage percentage = (task_run_time / total_uptime) * 100.
 */

#include "stats.h"
#include "task.h"
#include "kernel.h"

#if OS_CONFIG_USE_STATS

uint32_t os_stats_get_cpu_usage(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return 0;

    os_tick_t uptime = os_kernel_get_tick();
    if (uptime == 0) return 0;

    return (uint32_t)(((uint64_t)tcb->run_time_ticks * 100) / uptime);
}

os_tick_t os_stats_get_run_time(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return 0;
    return tcb->run_time_ticks;
}

void os_stats_reset(void)
{
    os_tcb_t *tcb;

    /* Zero run_time_ticks for every task in the task table */
    for (uint32_t i = 0; ; i++) {
        tcb = (os_tcb_t*)os_task_get_by_index(i);
        if (tcb == NULL) break;
        tcb->run_time_ticks = 0;
    }
}

#endif /* OS_CONFIG_USE_STATS */
