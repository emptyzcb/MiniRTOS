#ifndef OS_SYSINFO_H
#define OS_SYSINFO_H

#include "os_types.h"

/* ========== System Information ========== */

typedef struct {
    uint32_t    task_count;     /* Current number of tasks */
    uint32_t    heap_free;      /* Free heap size in bytes */
    uint32_t    heap_min_free;  /* Minimum free heap ever (high-water mark) */
    uint32_t    heap_largest;   /* Largest contiguous free block */
    os_tick_t   uptime;         /* System uptime in ticks */
    const char  *version;       /* OS version string */
    uint32_t    tick_rate;      /* System tick rate in Hz */
} os_sysinfo_t;

/*
 * Fill in a system information structure with current values.
 */
void os_sysinfo_get(os_sysinfo_t *info);

/*
 * Get detailed info about a specific task.
 * Any output parameter may be NULL if not needed.
 */
void os_task_get_info(os_task_handle_t handle, char *name_buf,
                      uint32_t name_buf_size, os_task_state_t *state,
                      os_prio_t *prio, uint32_t *stack_used,
                      uint32_t *stack_total);

#endif /* OS_SYSINFO_H */
