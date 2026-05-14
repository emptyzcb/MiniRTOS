/*
 * task.c - Task Management
 *
 * Implements task creation, deletion, and the ready list.
 * Uses a per-priority linked list for O(1) ready-list operations.
 */

#include "task.h"
#include "heap4.h"
#include "scheduler.h"
#include "kernel.h"
#include "port.h"
#include "os_config.h"
#include <string.h>

/* ========== Internal Data ========== */

/* Ready list: one linked list per priority level */
typedef struct {
    os_tcb_t *head;
    os_tcb_t *tail;
    uint32_t count;
} os_ready_list_t;

static os_ready_list_t ready_list[OS_CONFIG_NUM_PRIORITIES];

/* Blocked list (tasks waiting on delay) */
static os_tcb_t *blocked_list = NULL;

/* Deferred delete list (tasks pending resource cleanup) */
static os_tcb_t *deferred_delete_list = NULL;

/* Task table for quick lookup */
static os_tcb_t *task_table[OS_CONFIG_MAX_TASKS];
static uint32_t task_count = 0;

/* Currently running task - defined in port.c for assembly access */
extern os_tcb_t *current_task_ptr;

/* Idle task */
static os_stack_t idle_task_stack[OS_CONFIG_IDLE_STACK_SIZE / sizeof(os_stack_t)];

/* ========== Internal Functions ========== */

static os_stack_t* prv_allocate_stack(uint32_t size)
{
    return (os_stack_t*)os_heap_alloc(size);
}

/*
 * Fill the stack with a known pattern for high-water mark detection.
 * Also writes a canary at the stack bottom for overflow detection.
 */
static void prv_fill_stack(os_stack_t *stack, uint32_t size)
{
    uint32_t words = size / sizeof(os_stack_t);

    /* Fill with high-water mark pattern */
    for (uint32_t i = 0; i < words; i++) {
        stack[i] = 0xA5A5A5A5;
    }

#if OS_CONFIG_STACK_OVERFLOW_CHECK
    /* Write canary at the bottom of the stack (lowest address) */
    stack[0] = OS_STACK_CANARY_VALUE;
#endif
}

/*
 * Calculate stack high-water mark (how much was actually used).
 */
static uint32_t prv_calc_high_water(os_stack_t *stack_base, uint32_t stack_size)
{
    uint32_t words = stack_size / sizeof(os_stack_t);
    uint32_t unused = 0;

    for (uint32_t i = 0; i < words; i++) {
        if (stack_base[i] == 0xA5A5A5A5) {
            unused++;
        } else {
            break;
        }
    }

    return stack_size - (unused * sizeof(os_stack_t));
}

/* ========== Idle Task ========== */

static void idle_task_func(void *param)
{
    (void)param;
    while (1) {
        __asm volatile("wfi");
    }
}

void os_task_create_idle(void)
{
    os_task_create(idle_task_func, "IDLE", NULL,
                   OS_PRIO_LOWEST, idle_task_stack,
                   sizeof(idle_task_stack), NULL);
}

/* ========== Ready List Operations ========== */

void os_task_init_ready_list(void)
{
    for (int i = 0; i < OS_CONFIG_NUM_PRIORITIES; i++) {
        ready_list[i].head = NULL;
        ready_list[i].tail = NULL;
        ready_list[i].count = 0;
    }
    blocked_list = NULL;
    deferred_delete_list = NULL;
    task_count = 0;
    current_task_ptr = NULL;
}

void os_task_add_to_ready(os_tcb_t *tcb)
{
    os_prio_t prio = tcb->priority;
    OS_ASSERT(prio < OS_CONFIG_NUM_PRIORITIES);

    tcb->state = OS_TASK_READY;
    tcb->next = NULL;
    tcb->prev = ready_list[prio].tail;

    if (ready_list[prio].tail != NULL) {
        ready_list[prio].tail->next = tcb;
    } else {
        ready_list[prio].head = tcb;
    }
    ready_list[prio].tail = tcb;
    ready_list[prio].count++;
}

void os_task_remove_from_ready(os_tcb_t *tcb)
{
    os_prio_t prio = tcb->priority;
    OS_ASSERT(prio < OS_CONFIG_NUM_PRIORITIES);

    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        ready_list[prio].head = tcb->next;
    }

    if (tcb->next != NULL) {
        tcb->next->prev = tcb->prev;
    } else {
        ready_list[prio].tail = tcb->prev;
    }

    tcb->next = NULL;
    tcb->prev = NULL;
    if (ready_list[prio].count > 0) {
        ready_list[prio].count--;
    }
}

os_tcb_t* os_task_find_highest_ready(void)
{
    /* Scan from highest priority (0) to lowest */
    for (int i = 0; i < OS_CONFIG_NUM_PRIORITIES; i++) {
        if (ready_list[i].head != NULL) {
            return ready_list[i].head;
        }
    }
    return NULL; /* Should never happen if idle task exists */
}

/* ========== Public Task API ========== */

os_status_t os_task_create(os_task_func_t func,
                           const char *name,
                           void *param,
                           os_prio_t priority,
                           os_stack_t *stack_buf,
                           uint32_t stack_size,
                           os_task_handle_t *handle)
{
    os_tcb_t *tcb;
    os_stack_t *stack;

    /* Validate parameters */
    if (func == NULL || priority >= OS_CONFIG_NUM_PRIORITIES) {
        return OS_ERR_PARAM;
    }
    if (task_count >= OS_CONFIG_MAX_TASKS) {
        return OS_ERR_FULL;
    }
    if (stack_size < 128) {
        return OS_ERR_PARAM; /* Stack too small */
    }

    /* Allocate stack if not provided */
    if (stack_buf == NULL) {
        stack = prv_allocate_stack(stack_size);
        if (stack == NULL) {
            return OS_ERR_NOMEM;
        }
    } else {
        stack = stack_buf;
    }

    /* Allocate TCB from heap */
    tcb = (os_tcb_t*)os_heap_alloc(sizeof(os_tcb_t));
    if (tcb == NULL) {
        if (stack_buf == NULL) {
            os_heap_free(stack);
        }
        return OS_ERR_NOMEM;
    }

    /* Fill stack with pattern for high-water tracking */
    prv_fill_stack(stack, stack_size);

    /* Initialize TCB fields */
    tcb->stack_base = stack;
    tcb->stack_size = stack_size;
    tcb->priority   = priority;
    tcb->state      = OS_TASK_READY;
    tcb->delay_ticks = 0;
    tcb->entry_func  = func;
    tcb->param       = param;
    tcb->next        = NULL;
    tcb->prev        = NULL;
    tcb->pending_delete = 0;
    tcb->stack_high_water = 0;
    tcb->blocked_on = NULL;
    tcb->blocked_reason = OS_BLOCKED_NONE;
    tcb->timed_out = 0;
    tcb->event_wait_bits = 0;
    tcb->event_wait_options = 0;
    tcb->event_return_bits = 0;

    /* Copy name */
    if (name != NULL) {
        strncpy(tcb->name, name, OS_CONFIG_MAX_NAME_LEN - 1);
        tcb->name[OS_CONFIG_MAX_NAME_LEN - 1] = '\0';
    } else {
        strcpy(tcb->name, "unnamed");
    }

    /* Initialize stack frame for port layer */
    tcb->stack_ptr = os_port_stack_init(func, param, stack, stack_size);

    /* Add to task table */
    task_table[task_count++] = tcb;

    /* Add to ready list */
    os_task_add_to_ready(tcb);

    /* Return handle */
    if (handle != NULL) {
        *handle = (os_task_handle_t)tcb;
    }

    return OS_OK;
}

os_status_t os_task_create_suspended(os_task_func_t func,
                                     const char *name,
                                     void *param,
                                     os_prio_t priority,
                                     os_stack_t *stack_buf,
                                     uint32_t stack_size,
                                     os_task_handle_t *handle)
{
    os_status_t ret;

    /* Create the task normally (goes to READY list) */
    ret = os_task_create(func, name, param, priority, stack_buf, stack_size, handle);
    if (ret != OS_OK) return ret;

    /* Immediately suspend it */
    if (handle != NULL) {
        os_task_suspend(*handle);
    }

    return OS_OK;
}

os_status_t os_task_delete(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;

    if (handle == NULL) {
        /* Self-deletion: cannot free own stack while running on it */
        tcb = current_task_ptr;
        if (tcb == NULL) return OS_ERR_PARAM;

        os_sched_enter_critical();

        /* Remove from ready list */
        os_task_remove_from_ready(tcb);
        tcb->state = OS_TASK_DELETED;
        tcb->pending_delete = 1;

        /* Add to deferred delete list for cleanup after context switch */
        tcb->next = deferred_delete_list;
        deferred_delete_list = tcb;

        os_sched_exit_critical();

        /* Yield to trigger context switch; cleanup happens later */
        os_sched_yield();
        return OS_OK;
    }

    if (tcb == current_task_ptr) {
        /* Same as NULL case: self-deletion */
        return os_task_delete(NULL);
    }

    /* Deleting another task */
    os_sched_enter_critical();

    /* Remove from ready list if ready */
    if (tcb->state == OS_TASK_READY) {
        os_task_remove_from_ready(tcb);
    }

    tcb->state = OS_TASK_DELETED;

    /* Free resources */
    if (tcb->stack_base != idle_task_stack) {
        os_heap_free(tcb->stack_base);
    }
    os_heap_free(tcb);

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_task_suspend(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (tcb->state == OS_TASK_READY || tcb->state == OS_TASK_RUNNING) {
        os_task_remove_from_ready(tcb);
        tcb->state = OS_TASK_SUSPENDED;
    }

    os_sched_exit_critical();

    if (tcb == current_task_ptr) {
        os_sched_yield();
    }

    return OS_OK;
}

os_status_t os_task_resume(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (tcb->state == OS_TASK_SUSPENDED) {
        os_task_add_to_ready(tcb);
    }

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_task_delay(os_tick_t ticks)
{
    if (ticks == 0) {
        os_sched_yield();
        return OS_OK;
    }

    os_sched_enter_critical();

    /* Remove from ready list */
    os_task_remove_from_ready(current_task_ptr);

    /* Set delay and move to blocked list */
    current_task_ptr->delay_ticks = ticks;
    current_task_ptr->state = OS_TASK_BLOCKED;
    current_task_ptr->next = blocked_list;
    if (blocked_list != NULL) {
        blocked_list->prev = current_task_ptr;
    }
    blocked_list = current_task_ptr;

    os_sched_exit_critical();

    /* Trigger context switch */
    os_sched_yield();

    return OS_OK;
}

os_status_t os_task_set_priority(os_task_handle_t handle, os_prio_t new_prio)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL || new_prio >= OS_CONFIG_NUM_PRIORITIES) {
        return OS_ERR_PARAM;
    }

    os_sched_enter_critical();

    if (tcb->state == OS_TASK_READY) {
        os_task_remove_from_ready(tcb);
        tcb->priority = new_prio;
        os_task_add_to_ready(tcb);
    } else {
        tcb->priority = new_prio;
    }

    os_sched_exit_critical();

    return OS_OK;
}

const char* os_task_get_name(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return "?";
    return tcb->name;
}

/* ========== Tick Handler ========== */

void os_task_tick(void)
{
    os_tcb_t *tcb = blocked_list;
    os_tcb_t *next;

    /* Walk blocked list, decrement delays */
    while (tcb != NULL) {
        next = tcb->next;

        if (tcb->delay_ticks > 0) {
            tcb->delay_ticks--;
            if (tcb->delay_ticks == 0) {
                /* Remove from blocked list */
                if (tcb->prev != NULL) {
                    tcb->prev->next = tcb->next;
                } else {
                    blocked_list = tcb->next;
                }
                if (tcb->next != NULL) {
                    tcb->next->prev = tcb->prev;
                }
                tcb->next = NULL;
                tcb->prev = NULL;

                /* If blocked on a sync object, mark timed out */
                if (tcb->blocked_on != NULL) {
                    tcb->timed_out = 1;
                    tcb->blocked_on = NULL;
                    tcb->blocked_reason = OS_BLOCKED_NONE;
                }

                /* Add back to ready list */
                os_task_add_to_ready(tcb);
            }
        }
        tcb = next;
    }

    /* Update stack high-water mark for current task */
    if (current_task_ptr != NULL) {
        uint32_t used = prv_calc_high_water(current_task_ptr->stack_base,
                                             current_task_ptr->stack_size);
        if (used > current_task_ptr->stack_high_water) {
            current_task_ptr->stack_high_water = used;
        }
    }

#if OS_CONFIG_STACK_OVERFLOW_CHECK
    /* Check for stack overflow */
    os_task_check_stack_overflow();
#endif

    /* Process deferred task deletions */
    os_task_process_deferred_delete();
}

uint32_t os_task_get_stack_high_water(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return 0;
    return tcb->stack_high_water;
}

/* ========== Deferred Delete Processing ========== */

void os_task_process_deferred_delete(void)
{
    os_tcb_t *tcb;
    os_tcb_t *next;

    os_sched_enter_critical();

    tcb = deferred_delete_list;
    deferred_delete_list = NULL;

    os_sched_exit_critical();

    /* Free all deferred tasks outside of critical section */
    while (tcb != NULL) {
        next = tcb->next;

        /* Free resources */
        if (tcb->stack_base != idle_task_stack) {
            os_heap_free(tcb->stack_base);
        }
        os_heap_free(tcb);

        tcb = next;
    }
}

/* ========== Stack Overflow Detection ========== */

#if OS_CONFIG_STACK_OVERFLOW_CHECK
void os_task_check_stack_overflow(void)
{
    if (current_task_ptr == NULL) return;

    if (current_task_ptr->stack_base[0] != OS_STACK_CANARY_VALUE) {
        /* Stack has overflowed into the canary area */
        os_assert_failed(__FILE__, __LINE__);
    }
}
#endif

/* ========== Query APIs ========== */

os_task_state_t os_task_get_state(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return OS_TASK_DELETED;
    return tcb->state;
}

os_prio_t os_task_get_priority(os_task_handle_t handle)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;
    if (tcb == NULL) return OS_PRIO_LOWEST;
    return tcb->priority;
}

uint32_t os_task_get_count(void)
{
    return task_count;
}
