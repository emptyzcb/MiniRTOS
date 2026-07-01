/*
 * kernel.c - OS Kernel Core
 *
 * Initializes all kernel subsystems and manages the system tick.
 */

#include "kernel.h"
#include "heap4.h"
#include "task.h"
#include "scheduler.h"
#include "port.h"
#include "os_config.h"
#include <string.h>

#if OS_CONFIG_USE_SOFTWARE_TIMERS
#include "timer.h"
#endif

#if OS_CONFIG_USE_TICKLESS_IDLE
#include "tickless.h"
#endif

#if OS_CONFIG_USE_WATCHDOG
#include "watchdog.h"
#endif

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

#define OS_VERSION "MiniOS v0.9.0"

/* ========== Internal Data ========== */

static os_tick_t system_tick = 0;
static bool kernel_initialized = false;

#define OS_CONFIG_MAX_TICK_HOOKS 4

static os_tick_hook_t tick_hooks[OS_CONFIG_MAX_TICK_HOOKS];
static uint32_t tick_hook_count = 0;

/* ========== Public API ========== */

void os_kernel_init(void)
{
    if (kernel_initialized) {
        return;
    }

    /* Initialize NVIC priority group and system handler priorities */
    os_port_nvic_set_priority_init();

    /* Initialize heap */
    os_heap_init();

    /* Initialize task subsystem */
    os_task_init_ready_list();

    /* Initialize scheduler */
    os_sched_init();

#if OS_CONFIG_USE_SOFTWARE_TIMERS
    /* Initialize timer subsystem */
    os_timer_init();
#endif

#if OS_CONFIG_USE_TICKLESS_IDLE
    /* Initialize tickless idle subsystem */
    os_tickless_init();
#endif

#if OS_CONFIG_USE_TRACE
    /* Initialize trace system */
    os_trace_init();
#endif

#if OS_CONFIG_USE_WATCHDOG
    /* Initialize watchdog subsystem */
    os_wdt_init();
#endif

    kernel_initialized = true;
}

void os_kernel_start(void)
{
    if (!kernel_initialized) {
        return;
    }

    /* Set up SysTick timer */
    os_port_systick_init(OS_CONFIG_TICK_RATE_HZ);

    /* Start the scheduler (never returns) */
    os_sched_start();
}

os_tick_t os_kernel_get_tick(void)
{
    return system_tick;
}

void os_kernel_tick_increment(void)
{
    system_tick++;

    /* Call registered tick hooks */
    for (uint32_t i = 0; i < tick_hook_count; i++) {
        if (tick_hooks[i] != NULL) {
            tick_hooks[i]();
        }
    }

    /* Process task delays */
    os_task_tick();

#if OS_CONFIG_USE_SOFTWARE_TIMERS
    /* Process software timers */
    os_timer_tick();
#endif

#if OS_CONFIG_USE_WATCHDOG
    /* Process watchdog countdowns */
    os_wdt_tick();
#endif

#if !OS_CONFIG_USE_INTERRUPT_NESTING
    /* Without interrupt nesting, select next task here (legacy behavior).
     * With nesting enabled, PendSV handles this after all ISRs exit. */
    os_sched_select_next();
#endif
}

const char* os_kernel_get_version(void)
{
    return OS_VERSION;
}

void os_assert_failed(const char *file, uint32_t line)
{
    (void)file;
    (void)line;

    /* Disable all interrupts */
    os_sched_enter_critical();

    /* Debug output */
    os_port_debug_print("ASSERT FAILED: ");
    os_port_debug_print(file);
    os_port_debug_print("\r\n");

    /* Halt */
    while (1) {
#ifndef TEST_HOST_BUILD
        __asm volatile("bkpt #0");
#endif
    }
}

/* ========== Tick Hook ========== */

os_status_t os_kernel_register_tick_hook(os_tick_hook_t hook)
{
    if (hook == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    if (tick_hook_count >= OS_CONFIG_MAX_TICK_HOOKS) {
        os_sched_exit_critical();
        return OS_ERR_FULL;
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < tick_hook_count; i++) {
        if (tick_hooks[i] == hook) {
            os_sched_exit_critical();
            return OS_ERR_STATE;
        }
    }

    tick_hooks[tick_hook_count++] = hook;
    os_sched_exit_critical();
    return OS_OK;
}

os_status_t os_kernel_unregister_tick_hook(os_tick_hook_t hook)
{
    if (hook == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    for (uint32_t i = 0; i < tick_hook_count; i++) {
        if (tick_hooks[i] == hook) {
            for (uint32_t j = i; j < tick_hook_count - 1; j++) {
                tick_hooks[j] = tick_hooks[j + 1];
            }
            tick_hook_count--;
            tick_hooks[tick_hook_count] = NULL;
            os_sched_exit_critical();
            return OS_OK;
        }
    }

    os_sched_exit_critical();
    return OS_ERR_PARAM;
}