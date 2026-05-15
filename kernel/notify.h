#ifndef OS_NOTIFY_H
#define OS_NOTIFY_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_TASK_NOTIFY

/* ========== Task Notification API ========== */

/*
 * Send a notification value to a task.
 * If the target task is waiting for notification, it is unblocked.
 */
os_status_t os_task_notify(os_task_handle_t handle, uint32_t value);

/*
 * ISR-safe version of os_task_notify.
 */
os_status_t os_task_notify_from_isr(os_task_handle_t handle, uint32_t value);

/*
 * Wait for a notification. If one is already pending, returns immediately.
 * Otherwise blocks until notification arrives or timeout expires.
 * value_out receives the notification value (may be NULL).
 */
os_status_t os_task_notify_wait(uint32_t *value_out, os_tick_t timeout);

#endif /* OS_CONFIG_USE_TASK_NOTIFY */

#endif /* OS_NOTIFY_H */
