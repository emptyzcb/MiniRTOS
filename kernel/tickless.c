/*
 * tickless.c - Tickless Idle Mode Implementation
 *
 * Stops the SysTick timer during idle periods to save power.
 * Uses SysTick itself as a one-shot wake-up timer.
 *
 * Algorithm:
 *   Enter:
 *     1. Read remaining SysTick cycles in current tick period
 *     2. Stop periodic SysTick
 *     3. Calculate total sleep cycles = remaining + (expected_ticks - 1) * reload
 *     4. Clamp to SysTick 24-bit maximum
 *     5. Configure SysTick as one-shot (no interrupt, poll COUNTFLAG)
 *     6. Execute WFI
 *
 *   Exit:
 *     1. Read how many SysTick cycles actually elapsed
 *     2. Convert to system ticks
 *     3. Restart periodic SysTick
 *     4. Return elapsed ticks for the caller to advance system_tick
 */

#include "tickless.h"
#include "port.h"
#include "kernel.h"

#if OS_CONFIG_USE_TICKLESS_IDLE

/* SysTick register access (same base as port.c) */
#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CTRL        (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD        (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL         (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)
#define SYSTICK_LOAD_MAX        0x00FFFFFFUL

/* ========== Internal Data ========== */

static bool tickless_active = false;
static uint32_t tickless_reload = 0;    /* SysTick reload value per system tick */
static uint32_t tickless_saved_load = 0;

/* ========== Internal Helpers ========== */

/*
 * Configure SysTick as a one-shot timer (no interrupt).
 * The timer counts down from 'cycles' to 0. Poll COUNTFLAG to detect expiry.
 */
static void prv_start_one_shot(uint32_t cycles)
{
    SYSTICK_CTRL = 0;                          /* Stop timer */
    SYSTICK_LOAD = cycles - 1;                 /* Set reload */
    SYSTICK_VAL  = 0;                          /* Clear current value */
    SYSTICK_CTRL = SYSTICK_CTRL_ENABLE         /* Start (no TICKINT) */
                 | SYSTICK_CTRL_CLKSOURCE;
}

/*
 * Stop the one-shot timer and return remaining cycles.
 */
static uint32_t prv_stop_one_shot(void)
{
    uint32_t val = SYSTICK_VAL;
    SYSTICK_CTRL = 0;
    return val;
}

/* ========== Public API ========== */

void os_tickless_init(void)
{
    tickless_active = false;

    /* Calculate SysTick reload value from the configured tick rate.
     * This must match what os_port_systick_init() programs. */
    tickless_reload = 72000000UL / OS_CONFIG_TICK_RATE_HZ;
    if (tickless_reload > SYSTICK_LOAD_MAX) {
        tickless_reload = SYSTICK_LOAD_MAX;
    }
}

void os_tickless_idle_enter(os_tick_t expected_idle_ticks)
{
    uint32_t remaining_in_period;
    uint32_t total_cycles;
    uint32_t max_cycles;

    if (expected_idle_ticks < OS_TICKLESS_MIN_IDLE_TICKS) {
        return; /* Not worth entering tickless mode */
    }

    /* Save current periodic SysTick state */
    tickless_saved_load = SYSTICK_LOAD;
    remaining_in_period = SYSTICK_VAL;

    /* Stop periodic SysTick */
    SYSTICK_CTRL = 0;

    /* Calculate total sleep cycles:
     *   remaining cycles in current tick + (expected - 1) full tick periods
     * The -1 because the current tick is partially elapsed. */
    if (expected_idle_ticks > 1) {
        total_cycles = remaining_in_period +
                       (expected_idle_ticks - 1) * tickless_reload;
    } else {
        total_cycles = remaining_in_period;
    }

    /* Clamp to 24-bit maximum */
    max_cycles = SYSTICK_LOAD_MAX;
    if (total_cycles > max_cycles) {
        total_cycles = max_cycles;
    }

    tickless_active = true;

    /* Start one-shot timer and sleep */
    prv_start_one_shot(total_cycles);
    __asm volatile("wfi");
}

os_tick_t os_tickless_idle_exit(void)
{
    uint32_t elapsed_cycles;
    os_tick_t elapsed_ticks;

    if (!tickless_active) {
        return 0;
    }

    /* Read remaining cycles in the one-shot timer */
    uint32_t remaining = prv_stop_one_shot();

    /* Calculate elapsed SysTick cycles.
     * If COUNTFLAG is set, the timer expired (we slept for the full duration).
     * Otherwise, we were woken early by an interrupt. */
    uint32_t total_cycles;
    if (SYSTICK_CTRL & SYSTICK_CTRL_COUNTFLAG) {
        /* Timer expired: we slept for the full programmed duration */
        total_cycles = SYSTICK_LOAD + 1;
    } else {
        /* Woken early: calculate from remaining */
        total_cycles = SYSTICK_LOAD + 1;
    }

    /* Elapsed = programmed - remaining */
    elapsed_cycles = total_cycles - remaining;

    /* Convert to system ticks */
    elapsed_ticks = elapsed_cycles / tickless_reload;

    /* Clamp to at least 1 */
    if (elapsed_ticks == 0) {
        elapsed_ticks = 1;
    }

    /* Restart periodic SysTick */
    SYSTICK_LOAD = tickless_saved_load;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = SYSTICK_CTRL_ENABLE
                 | SYSTICK_CTRL_TICKINT
                 | SYSTICK_CTRL_CLKSOURCE;

    tickless_active = false;

    return elapsed_ticks;
}

bool os_tickless_is_active(void)
{
    return tickless_active;
}

#endif /* OS_CONFIG_USE_TICKLESS_IDLE */
