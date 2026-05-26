/*
 * watchdog.h - Software Watchdog Timer
 *
 * Monitors task liveness: each registered task must periodically
 * "feed" the watchdog within its deadline. If a task misses its
 * deadline, a registered timeout callback is invoked.
 */

#ifndef OS_WATCHDOG_H
#define OS_WATCHDOG_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_WATCHDOG

/* Forward declaration */
struct os_tcb;

/* ========== Watchdog Configuration ========== */

#define OS_WDT_MAX_ENTRIES      OS_CONFIG_MAX_TASKS

/* ========== Watchdog Callback ========== */

/*
 * Timeout callback signature.
 *   tcb    - the task that missed its deadline
 *   param  - user-registered context pointer
 *
 * Called from the tick ISR context — keep it short, do NOT block.
 */
typedef void (*os_wdt_timeout_cb_t)(os_task_handle_t handle, void *param);

/* ========== Watchdog Entry ========== */

typedef struct {
    struct os_tcb   *tcb;           /* Monitored task (NULL = unused slot) */
    os_tick_t       deadline;       /* Max ticks allowed between feeds */
    os_tick_t       remaining;      /* Ticks remaining before timeout */
    os_wdt_timeout_cb_t callback;   /* Timeout callback (NULL = use default) */
    void            *cb_param;      /* User parameter for callback */
    bool            paused;         /* Temporarily paused */
} os_wdt_entry_t;

/* ========== Watchdog API ========== */

/*
 * Initialize the watchdog subsystem.
 * Called automatically by os_kernel_init().
 */
void os_wdt_init(void);

/*
 * Register a task with the watchdog.
 *
 * handle   - task handle
 * deadline - max ticks between feeds (must be > 0)
 * cb       - timeout callback (NULL to use the default handler)
 * param    - user context passed to callback
 *
 * Returns OS_OK on success, OS_ERR_FULL if the table is full,
 * OS_ERR_PARAM for bad arguments, OS_ERR_STATE if already registered.
 */
os_status_t os_wdt_register(os_task_handle_t handle, os_tick_t deadline,
                            os_wdt_timeout_cb_t cb, void *param);

/*
 * Unregister a task from the watchdog.
 */
os_status_t os_wdt_unregister(os_task_handle_t handle);

/*
 * Feed (kick) the watchdog for the current task.
 * Resets the countdown for this task. Must be called within
 * the deadline specified at registration time.
 */
os_status_t os_wdt_feed(void);

/*
 * Feed the watchdog for a specific task (e.g. from an ISR
 * or a monitor task).
 */
os_status_t os_wdt_feed_task(os_task_handle_t handle);

/*
 * Pause monitoring for a task (e.g. during a long blocking operation).
 * The task will not time out while paused.
 */
os_status_t os_wdt_pause(os_task_handle_t handle);

/*
 * Resume monitoring for a paused task.
 * The countdown restarts from the full deadline.
 */
os_status_t os_wdt_resume(os_task_handle_t handle);

/*
 * Change the deadline for a registered task.
 */
os_status_t os_wdt_set_deadline(os_task_handle_t handle, os_tick_t new_deadline);

/*
 * Get remaining ticks before a task times out.
 * Returns 0 if the task is not registered.
 */
os_tick_t os_wdt_get_remaining(os_task_handle_t handle);

/*
 * Check if a task is registered with the watchdog.
 */
bool os_wdt_is_registered(os_task_handle_t handle);

/*
 * Register a default timeout handler used when a task's individual
 * callback is NULL. If no default is set, os_assert_failed() is called.
 */
os_status_t os_wdt_set_default_callback(os_wdt_timeout_cb_t cb, void *param);

/*
 * Watchdog tick processing. Called from os_kernel_tick_increment().
 * Decrements all active entries and fires callbacks on expiry.
 */
void os_wdt_tick(void);

#endif /* OS_CONFIG_USE_WATCHDOG */

#endif /* OS_WATCHDOG_H */
