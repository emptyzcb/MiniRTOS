/*
 * tickless.h - Tickless Idle Mode
 *
 * Reduces power consumption by stopping the SysTick timer during
 * idle periods and using a longer sleep (WFI). On wake-up, the
 * elapsed time is calculated and the system tick is advanced by
 * the correct amount.
 *
 * Usage:
 *   1. Enable OS_CONFIG_USE_TICKLESS_IDLE in os_config.h
 *   2. The idle task automatically calls os_tickless_idle_enter()
 *      when no tasks are ready for a long idle period.
 *   3. On wake-up, os_tickless_idle_exit() compensates lost ticks.
 *
 * Hardware:
 *   Uses SysTick in one-shot mode for the wake-up timer.
 *   Alternatively can use a low-power timer (LPTIM) if available.
 */

#ifndef OS_TICKLESS_H
#define OS_TICKLESS_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_TICKLESS_IDLE

/* Minimum idle ticks before entering tickless mode */
#ifndef OS_TICKLESS_MIN_IDLE_TICKS
#define OS_TICKLESS_MIN_IDLE_TICKS  5
#endif

/* Maximum ticks that can be represented in the wake timer */
#ifndef OS_TICKLESS_MAX_TICKS
#define OS_TICKLESS_MAX_TICKS       0x00FFFFFFUL  /* SysTick 24-bit counter */
#endif

/* ========== Tickless API ========== */

/*
 * Initialize the tickless idle subsystem.
 * Called from os_kernel_init().
 */
void os_tickless_init(void);

/*
 * Enter tickless idle mode.
 *
 * expected_idle_ticks: the number of ticks until the next task
 * needs to run (determined by the idle task from the blocked list).
 *
 * This function:
 *   1. Stops the periodic SysTick
 *   2. Configures a one-shot wake-up timer
 *   3. Executes WFI (Wait For Interrupt)
 *   4. Returns after wake-up
 *
 * Must be called from the idle task with interrupts disabled.
 */
void os_tickless_idle_enter(os_tick_t expected_idle_ticks);

/*
 * Exit tickless idle mode.
 *
 * Called after wake-up to:
 *   1. Calculate elapsed ticks
 *   2. Restart the periodic SysTick
 *   3. Advance the system tick by the elapsed amount
 *
 * Returns the number of ticks that elapsed during sleep.
 */
os_tick_t os_tickless_idle_exit(void);

/*
 * Query whether tickless idle is currently active.
 */
bool os_tickless_is_active(void);

#endif /* OS_CONFIG_USE_TICKLESS_IDLE */

#endif /* OS_TICKLESS_H */
