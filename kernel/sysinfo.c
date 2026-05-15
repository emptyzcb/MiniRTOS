/*
 * sysinfo.c - System Information Query API
 *
 * Provides a unified interface to query system state:
 * heap usage, uptime, version, task details.
 */

#include "sysinfo.h"
#include "task.h"
#include "kernel.h"
#include "heap4.h"
#include "os_config.h"
#include <string.h>

void os_sysinfo_get(os_sysinfo_t *info)
{
    if (info == NULL) return;

    info->task_count    = os_task_get_count();
    info->heap_free     = os_heap_get_free_size();
    info->heap_min_free = os_heap_get_min_free_size();
    info->heap_largest  = os_heap_get_largest_free_block();
    info->uptime        = os_kernel_get_tick();
    info->version       = os_kernel_get_version();
    info->tick_rate     = OS_CONFIG_TICK_RATE_HZ;
}

void os_task_get_info(os_task_handle_t handle, char *name_buf,
                      uint32_t name_buf_size, os_task_state_t *state,
                      os_prio_t *prio, uint32_t *stack_used,
                      uint32_t *stack_total)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return;

    if (name_buf != NULL && name_buf_size > 0) {
        strncpy(name_buf, tcb->name, name_buf_size - 1);
        name_buf[name_buf_size - 1] = '\0';
    }

    if (state != NULL) {
        *state = tcb->state;
    }

    if (prio != NULL) {
        *prio = tcb->priority;
    }

    if (stack_used != NULL) {
        *stack_used = os_task_get_stack_high_water(handle);
    }

    if (stack_total != NULL) {
        *stack_total = tcb->stack_size;
    }
}
