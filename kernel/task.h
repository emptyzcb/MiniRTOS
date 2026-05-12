#ifndef OS_TASK_H
#define OS_TASK_H

#include "os_types.h"
#include "os_config.h"

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

    /* For tracking stack high-water mark */
    uint32_t            stack_high_water;
} os_tcb_t;

/* ========== Task API ========== */

os_status_t os_task_create(os_task_func_t func,
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

#endif /* OS_TASK_H */
