/*
 * os.h - MiniOS Public API
 *
 * Single header to include for application code.
 */

#ifndef OS_H
#define OS_H

#include "os_types.h"
#include "os_config.h"
#include "kernel.h"
#include "task.h"
#include "heap4.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_QUEUE
#include "queue.h"
#endif

#if OS_CONFIG_USE_SEMAPHORE
#include "semaphore.h"
#endif

#if OS_CONFIG_USE_MUTEX
#include "mutex.h"
#endif

#if OS_CONFIG_USE_SOFTWARE_TIMERS
#include "timer.h"
#endif

#if OS_CONFIG_USE_EVENTGROUP
#include "eventgroup.h"
#endif

#if OS_CONFIG_USE_MEMPOOL
#include "mempool.h"
#endif

#if OS_CONFIG_USE_MAILBOX
#include "mailbox.h"
#endif

#if OS_CONFIG_USE_TICKLESS_IDLE
#include "tickless.h"
#endif

#if OS_CONFIG_USE_TASK_NOTIFY
#include "notify.h"
#endif

#if OS_CONFIG_USE_WATCHDOG
#include "watchdog.h"
#endif

#if OS_CONFIG_USE_STATS
#include "stats.h"
#endif

#include "sysinfo.h"

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

/* ========== Convenience Macros ========== */

/* Delay the current task */
#define OS_DELAY(ticks)         os_task_delay(ticks)

/* Delay until the next absolute period boundary */
#define OS_DELAY_UNTIL(previous_wake_tick, period_ticks) \
    os_task_delay_until(previous_wake_tick, period_ticks)

/* Delay in milliseconds */
#define OS_DELAY_MS(ms)         os_task_delay((ms) * OS_CONFIG_TICK_RATE_HZ / 1000)

/* Delay in seconds */
#define OS_DELAY_SEC(sec)       os_task_delay((sec) * OS_CONFIG_TICK_RATE_HZ)

/* Get current tick count */
#define OS_GET_TICK()           os_kernel_get_tick()

/* Enter/Exit critical section */
#define OS_ENTER_CRITICAL()     os_sched_enter_critical()
#define OS_EXIT_CRITICAL()      os_sched_exit_critical()

/* ISR entry/exit for interrupt nesting */
#if OS_CONFIG_USE_INTERRUPT_NESTING
#define OS_ISR_ENTER()          os_port_isr_enter()
#define OS_ISR_EXIT()           os_port_isr_exit()
#else
#define OS_ISR_ENTER()          ((void)0)
#define OS_ISR_EXIT()           ((void)0)
#endif

/* Yield current task */
#define OS_YIELD()              os_sched_yield()

/* Delete current task (self-deletion) */
#define OS_TASK_DELETE_SELF()    os_task_delete(NULL)

/* Query task info */
#define OS_TASK_GET_STATE(h)    os_task_get_state(h)
#define OS_TASK_GET_PRIO(h)     os_task_get_priority(h)
#define OS_TASK_GET_COUNT()     os_task_get_count()

/* Queue convenience macros */
#if OS_CONFIG_USE_QUEUE
#define OS_QUEUE_SEND(q, item, t)       os_queue_send(q, item, t)
#define OS_QUEUE_RECEIVE(q, item, t)    os_queue_receive(q, item, t)
#define OS_QUEUE_RESET(q)               os_queue_reset(q)
#define OS_QUEUE_OVERWRITE(q, item)     os_queue_overwrite(q, item)
#define OS_QUEUE_OVERWRITE_FROM_ISR(q, item) os_queue_overwrite_from_isr(q, item)
#define OS_QUEUE_COUNT_FROM_ISR(q)      os_queue_get_count_from_isr(q)
#endif

/* Semaphore convenience macros */
#if OS_CONFIG_USE_SEMAPHORE
#define OS_SEM_GIVE(s)                  os_sem_give(s)
#define OS_SEM_GIVE_FROM_ISR(s)         os_sem_give_from_isr(s)
#define OS_SEM_TAKE(s, t)              os_sem_take(s, t)
#define OS_SEM_TAKE_FROM_ISR(s)         os_sem_take_from_isr(s)
#define OS_SEM_COUNT(s)                 os_sem_get_count(s)
#define OS_SEM_COUNT_FROM_ISR(s)        os_sem_get_count_from_isr(s)
#endif

/* Mutex convenience macros */
#if OS_CONFIG_USE_MUTEX
#define OS_MUTEX_LOCK(m, t)            os_mutex_lock(m, t)
#define OS_MUTEX_UNLOCK(m)             os_mutex_unlock(m)
#endif

/* Event group convenience macros */
#if OS_CONFIG_USE_EVENTGROUP
#define OS_EVENT_SET(e, bits)          os_eventgroup_set_bits(e, bits)
#define OS_EVENT_SET_FROM_ISR(e, bits) os_eventgroup_set_bits_from_isr(e, bits)
#define OS_EVENT_CLEAR(e, bits)        os_eventgroup_clear_bits(e, bits)
#define OS_EVENT_CLEAR_FROM_ISR(e, bits) os_eventgroup_clear_bits_from_isr(e, bits)
#define OS_EVENT_GET(e)                os_eventgroup_get_bits(e)
#define OS_EVENT_GET_FROM_ISR(e)       os_eventgroup_get_bits_from_isr(e)
#define OS_EVENT_WAIT(e, bits, opts, t) os_eventgroup_wait_bits(e, bits, opts, t)
#endif

/* Mailbox convenience macros */
#if OS_CONFIG_USE_MAILBOX
#define OS_MAILBOX_SEND(m, item, t)    os_mailbox_send(m, item, t)
#define OS_MAILBOX_OVERWRITE(m, item)  os_mailbox_overwrite(m, item)
#define OS_MAILBOX_RECEIVE(m, item, t) os_mailbox_receive(m, item, t)
#endif

/* Timer ISR convenience macros */
#if OS_CONFIG_USE_SOFTWARE_TIMERS
#define OS_TIMER_START_FROM_ISR(t)       os_timer_start_from_isr(t)
#define OS_TIMER_STOP_FROM_ISR(t)        os_timer_stop_from_isr(t)
#define OS_TIMER_RESET_FROM_ISR(t)       os_timer_reset_from_isr(t)
#define OS_TIMER_CHANGE_PERIOD_FROM_ISR(t, p) os_timer_change_period_from_isr(t, p)
#endif

#endif /* OS_H */
