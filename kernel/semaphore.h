#ifndef OS_SEMAPHORE_H
#define OS_SEMAPHORE_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_SEMAPHORE

/* Forward declaration */
struct os_tcb;

/* ========== Semaphore Control Block ========== */

typedef struct os_sem {
    uint32_t    count;              /* Current semaphore count */
    uint32_t    max_count;          /* Maximum count (1 for binary) */
    struct os_tcb *wait_list;       /* Tasks waiting to take (priority-ordered) */
} os_sem_t;

/* ========== Semaphore API ========== */

os_status_t os_sem_create_binary(os_sem_t *sem);
os_status_t os_sem_create_counting(os_sem_t *sem, uint32_t max_count,
                                   uint32_t initial_count);
os_status_t os_sem_delete(os_sem_t *sem);

os_status_t os_sem_take(os_sem_t *sem, os_tick_t timeout);
os_status_t os_sem_take_from_isr(os_sem_t *sem);
os_status_t os_sem_give(os_sem_t *sem);
os_status_t os_sem_give_from_isr(os_sem_t *sem);

uint32_t os_sem_get_count(os_sem_t *sem);
uint32_t os_sem_get_count_from_isr(os_sem_t *sem);

/* Internal: remove a task from semaphore blocked list */
void os_sem_remove_task(struct os_sem *sem, struct os_tcb *tcb);

#endif /* OS_CONFIG_USE_SEMAPHORE */

#endif /* OS_SEMAPHORE_H */
