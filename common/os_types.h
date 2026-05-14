#ifndef OS_TYPES_H
#define OS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========== Type Definitions ========== */

typedef uint32_t    os_tick_t;
typedef uint32_t    os_prio_t;
typedef uint32_t    os_stack_t;
typedef void*       os_task_handle_t;
typedef void        (*os_task_func_t)(void *param);

/* Handle types for synchronization primitives */
typedef void*       os_queue_handle_t;
typedef void*       os_sem_handle_t;
typedef void*       os_mutex_handle_t;
typedef void*       os_timer_handle_t;
typedef void*       os_eventgroup_handle_t;

/* ========== Return Codes ========== */

typedef enum {
    OS_OK           = 0,
    OS_ERR_NOMEM    = -1,
    OS_ERR_PARAM    = -2,
    OS_ERR_STATE    = -3,
    OS_ERR_FULL     = -4,
    OS_ERR_EMPTY    = -5,
    OS_ERR_TIMEOUT      = -6,
    OS_ERR_WOULD_BLOCK  = -7,
} os_status_t;

/* ========== Task States ========== */

typedef enum {
    OS_TASK_READY      = 0,
    OS_TASK_RUNNING    = 1,
    OS_TASK_SUSPENDED  = 2,
    OS_TASK_BLOCKED    = 3,
    OS_TASK_DELETED    = 4,
} os_task_state_t;

/* ========== Blocked Reason ========== */

typedef enum {
    OS_BLOCKED_NONE             = 0,
    OS_BLOCKED_ON_QUEUE_SEND    = 1,
    OS_BLOCKED_ON_QUEUE_RECV    = 2,
    OS_BLOCKED_ON_SEM_TAKE      = 3,
    OS_BLOCKED_ON_MUTEX_LOCK    = 4,
    OS_BLOCKED_ON_EVENT_WAIT    = 5,
} os_blocked_reason_t;

/* ========== Constants ========== */

#define OS_PRIO_HIGHEST     0
#define OS_PRIO_LOWEST      (OS_CONFIG_NUM_PRIORITIES - 1)
#define OS_WAIT_FOREVER     0xFFFFFFFFU
#define OS_WAIT_NONE        0U

#endif /* OS_TYPES_H */
