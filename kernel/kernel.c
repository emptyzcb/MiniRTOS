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

#if OS_CONFIG_USE_WATCHDOG
#include "watchdog.h"
#endif

#if OS_CONFIG_USE_TRACE
#include "trace.h"
#endif

#define OS_VERSION "MiniOS v0.5.0"

/* ========== Internal Data ========== */

static os_tick_t system_tick = 0;
static bool kernel_initialized = false;

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

    /* Check if a context switch is needed */
    os_sched_select_next();
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
        __asm volatile("bkpt #0");
    }
}
