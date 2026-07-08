#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_MUTEX

/* Forward declaration */
struct os_tcb;

/* ========== Mutex Control Block ========== */

typedef struct os_mutex {
    struct os_tcb *owner;           /* Task that holds the mutex (NULL if free) */
    uint32_t    lock_count;         /* Lock depth (for recursive locking) */
    os_prio_t   original_prio;      /* Owner's original priority before inheritance */
    struct os_tcb *wait_list;       /* Tasks waiting to lock (priority-ordered) */
} os_mutex_t;

/* ========== Mutex API ========== */

os_status_t os_mutex_create(os_mutex_t *mutex);
os_status_t os_mutex_delete(os_mutex_t *mutex);
os_status_t os_mutex_lock(os_mutex_t *mutex, os_tick_t timeout);
os_status_t os_mutex_unlock(os_mutex_t *mutex);
struct os_tcb* os_mutex_get_owner(os_mutex_t *mutex);
bool os_mutex_is_locked(os_mutex_t *mutex);
uint32_t os_mutex_get_lock_count(os_mutex_t *mutex);

/* Internal: remove a task from mutex blocked list */
void os_mutex_remove_task(struct os_mutex *mutex, struct os_tcb *tcb);

#endif /* OS_CONFIG_USE_MUTEX */

#endif /* OS_MUTEX_H */
