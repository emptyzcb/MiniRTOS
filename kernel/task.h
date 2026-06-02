#ifndef OS_TASK_H
#define OS_TASK_H

#include "os_types.h"
#include "os_config.h"

/* Idle hook function type */
typedef void (*os_idle_hook_t)(void);

/* Stack overflow canary value (easy to spot in debugger) */
#define OS_STACK_CANARY_VALUE       0xCCCCCCCCU

/* ========== Task Control Block (TCB) ========== */

typedef struct os_tcb {
    os_stack_t          *stack_ptr;         /* Current stack pointer (must be first) */
    os_stack_t          *stack_base;        /* Stack base address */
    uint32_t            stack_size;         /* Stack size in bytes */
    os_prio_t           priority;           /* Task priority */
    os_task_state_t     state;              /* Task state */
    os_tick_t           delay_ticks;        /* Ticks remaining when blocked */
    char                name[OS_CONFIG_MAX_NAME_LEN];
    os_task_func_t      entry_func;         /* Task entry function */
    void                *param;             /* Task entry parameter */

    /* Linked list pointers for ready/blocked lists */
    struct os_tcb       *next;
    struct os_tcb       *prev;

    /* Deferred self-deletion flag */
    uint8_t             pending_delete;

    /* For tracking stack high-water mark */
    uint32_t            stack_high_water;

    /* Blocked-on-object tracking (for queue/sem/mutex/eventgroup with timeout) */
    void                *blocked_on;        /* Pointer to the object this task is blocked on */
    uint8_t             blocked_reason;     /* Which primitive type (os_blocked_reason_t) */
    uint8_t             timed_out;          /* Set to 1 by tick handler if timeout expired */

    /* Event group wait state */
    uint32_t            event_wait_bits;    /* Bits this task is waiting for */
    uint32_t            event_wait_options; /* WAIT_ANY / WAIT_ALL / CLEAR_ON_EXIT */
    uint32_t            event_return_bits;  /* Bits value when task was unblocked */

#if OS_CONFIG_USE_TASK_NOTIFY
    /* Task notification */
    volatile uint32_t   notify_value;       /* Notification value */
    volatile uint8_t    notify_pending;     /* 1 if notification pending */
#endif

#if OS_CONFIG_USE_STATS
    /* CPU usage statistics */
    os_tick_t           run_time_ticks;     /* Total ticks spent running */
#endif
} os_tcb_t;

/* ========== Task API ========== */

os_status_t os_task_create(os_task_func_t func,
                           const char *name,
                           void *param,
                           os_prio_t priority,
                           os_stack_t *stack_buf,
                           uint32_t stack_size,
                           os_task_handle_t *handle);

os_status_t os_task_create_suspended(os_task_func_t func,
                                     const char *name,
                                     void *param,
                                     os_prio_t priority,
                                     os_stack_t *stack_buf,
                                     uint32_t stack_size,
                                     os_task_handle_t *handle);

os_status_t os_task_delete(os_task_handle_t handle);
os_status_t os_task_suspend(os_task_handle_t handle);
os_status_t os_task_resume(os_task_handle_t handle);
os_status_t os_task_delay(os_tick_t ticks);
os_status_t os_task_delay_until(os_tick_t *previous_wake_tick,
                                os_tick_t period_ticks);
os_status_t os_task_set_priority(os_task_handle_t handle, os_prio_t new_prio);
const char* os_task_get_name(os_task_handle_t handle);

/* Current task management (implemented in port.c) */
os_task_handle_t os_task_get_current(void);
void os_task_set_current(struct os_tcb *tcb);

/* Internal functions */
void os_task_init_ready_list(void);
void os_task_add_to_ready(os_tcb_t *tcb);
void os_task_remove_from_ready(os_tcb_t *tcb);
os_tcb_t* os_task_find_highest_ready(void);
void os_task_tick(void);

/* Idle task */
void os_task_create_idle(void);

/* Stack utilities */
uint32_t os_task_get_stack_high_water(os_task_handle_t handle);

/* Query APIs */
os_task_state_t os_task_get_state(os_task_handle_t handle);
os_prio_t os_task_get_priority(os_task_handle_t handle);
uint32_t os_task_get_count(void);

/* Deferred delete processing (called from os_task_tick) */
void os_task_process_deferred_delete(void);

/* Blocked list management (used by notify and other primitives) */
void os_task_add_to_blocked(os_tcb_t *tcb, os_tick_t timeout);
void os_task_remove_from_blocked(os_tcb_t *tcb);

/* Stack overflow check (called from os_task_tick) */
void os_task_check_stack_overflow(void);

/* Idle hook management */
os_status_t os_task_register_idle_hook(os_idle_hook_t hook);
os_status_t os_task_unregister_idle_hook(os_idle_hook_t hook);

/* Task table iterator */
os_task_handle_t os_task_get_by_index(uint32_t index);

#endif /* OS_TASK_H */
