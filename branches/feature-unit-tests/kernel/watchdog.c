/*
 * watchdog.c - Software Watchdog Timer Implementation
 *
 * Monitors task liveness by requiring periodic "feed" calls.
 * Each registered task has an independent countdown that is
 * decremented every tick. If the countdown reaches zero before
 * the task feeds, the timeout callback fires.
 *
 * Architecture:
 *   os_wdt_register()  — add a task to the watch table
 *   os_wdt_feed()      — task calls this to reset its countdown
 *   os_wdt_tick()      — called from SysTick ISR, decrements counters
 */

#include "watchdog.h"
#include "task.h"
#include "kernel.h"
#include "scheduler.h"
#include "port.h"

#if OS_CONFIG_USE_WATCHDOG

/* ========== Internal Data ========== */

static os_wdt_entry_t wdt_table[OS_WDT_MAX_ENTRIES];
static uint32_t       wdt_count = 0;

static os_wdt_timeout_cb_t wdt_default_cb = NULL;
static void               *wdt_default_param = NULL;

/* ========== Internal Helpers ========== */

static os_wdt_entry_t* prv_find_entry(os_task_handle_t handle)
{
    for (uint32_t i = 0; i < wdt_count; i++) {
        if ((os_task_handle_t)wdt_table[i].tcb == handle) {
            return &wdt_table[i];
        }
    }
    return NULL;
}

static void prv_default_timeout_handler(os_task_handle_t handle, void *param)
{
    (void)handle;
    (void)param;

    /*
     * Halt the system — a task missed its watchdog deadline.
     * In a production system this might trigger a hardware reset
     * or log diagnostics before resetting.
     */
    os_port_debug_print("WDT TIMEOUT\r\n");
    os_assert_failed(__FILE__, __LINE__);
}

/* ========== Public API ========== */

void os_wdt_init(void)
{
    for (uint32_t i = 0; i < OS_WDT_MAX_ENTRIES; i++) {
        wdt_table[i].tcb = NULL;
        wdt_table[i].deadline = 0;
        wdt_table[i].remaining = 0;
        wdt_table[i].callback = NULL;
        wdt_table[i].cb_param = NULL;
        wdt_table[i].paused = false;
    }
    wdt_count = 0;
    wdt_default_cb = NULL;
    wdt_default_param = NULL;
}

os_status_t os_wdt_register(os_task_handle_t handle, os_tick_t deadline,
                            os_wdt_timeout_cb_t cb, void *param)
{
    os_tcb_t *tcb = (os_tcb_t*)handle;

    if (handle == NULL || deadline == 0) {
        return OS_ERR_PARAM;
    }

    os_sched_enter_critical();

    /* Already registered? */
    if (prv_find_entry(handle) != NULL) {
        os_sched_exit_critical();
        return OS_ERR_STATE;
    }

    /* Table full? */
    if (wdt_count >= OS_WDT_MAX_ENTRIES) {
        os_sched_exit_critical();
        return OS_ERR_FULL;
    }

    /* Add to table */
    wdt_table[wdt_count].tcb = tcb;
    wdt_table[wdt_count].deadline = deadline;
    wdt_table[wdt_count].remaining = deadline;
    wdt_table[wdt_count].callback = cb;
    wdt_table[wdt_count].cb_param = param;
    wdt_table[wdt_count].paused = false;
    wdt_count++;

    os_sched_exit_critical();

    return OS_OK;
}

os_status_t os_wdt_unregister(os_task_handle_t handle)
{
    if (handle == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();

    for (uint32_t i = 0; i < wdt_count; i++) {
        if ((os_task_handle_t)wdt_table[i].tcb == handle) {
            /* Compact the table */
            wdt_table[i] = wdt_table[wdt_count - 1];
            wdt_table[wdt_count - 1].tcb = NULL;
            wdt_count--;
            os_sched_exit_critical();
            return OS_OK;
        }
    }

    os_sched_exit_critical();
    return OS_ERR_PARAM;
}

os_status_t os_wdt_feed(void)
{
    return os_wdt_feed_task(os_task_get_current());
}

os_status_t os_wdt_feed_task(os_task_handle_t handle)
{
    if (handle == NULL) return OS_ERR_PARAM;

    /* No critical section needed: single-word write is atomic on Cortex-M */
    os_wdt_entry_t *entry = prv_find_entry(handle);
    if (entry == NULL) return OS_ERR_PARAM;

    entry->remaining = entry->deadline;
    return OS_OK;
}

os_status_t os_wdt_pause(os_task_handle_t handle)
{
    if (handle == NULL) return OS_ERR_PARAM;

    os_wdt_entry_t *entry = prv_find_entry(handle);
    if (entry == NULL) return OS_ERR_PARAM;

    entry->paused = true;
    return OS_OK;
}

os_status_t os_wdt_resume(os_task_handle_t handle)
{
    if (handle == NULL) return OS_ERR_PARAM;

    os_wdt_entry_t *entry = prv_find_entry(handle);
    if (entry == NULL) return OS_ERR_PARAM;

    entry->paused = false;
    entry->remaining = entry->deadline; /* Restart countdown */
    return OS_OK;
}

os_status_t os_wdt_set_deadline(os_task_handle_t handle, os_tick_t new_deadline)
{
    if (handle == NULL || new_deadline == 0) return OS_ERR_PARAM;

    os_wdt_entry_t *entry = prv_find_entry(handle);
    if (entry == NULL) return OS_ERR_PARAM;

    os_sched_enter_critical();
    entry->deadline = new_deadline;
    entry->remaining = new_deadline;
    os_sched_exit_critical();

    return OS_OK;
}

os_tick_t os_wdt_get_remaining(os_task_handle_t handle)
{
    if (handle == NULL) return 0;

    os_wdt_entry_t *entry = prv_find_entry(handle);
    if (entry == NULL) return 0;

    return entry->remaining;
}

bool os_wdt_is_registered(os_task_handle_t handle)
{
    if (handle == NULL) return false;
    return prv_find_entry(handle) != NULL;
}

os_status_t os_wdt_set_default_callback(os_wdt_timeout_cb_t cb, void *param)
{
    wdt_default_cb = cb;
    wdt_default_param = param;
    return OS_OK;
}

void os_wdt_tick(void)
{
    os_wdt_timeout_cb_t cb;
    void *param;
    os_task_handle_t handle;

    for (uint32_t i = 0; i < wdt_count; i++) {
        if (wdt_table[i].paused || wdt_table[i].tcb == NULL) {
            continue;
        }

        if (wdt_table[i].remaining > 0) {
            wdt_table[i].remaining--;
        }

        if (wdt_table[i].remaining == 0) {
            handle = (os_task_handle_t)wdt_table[i].tcb;
            cb = wdt_table[i].callback;
            param = wdt_table[i].cb_param;

            /* Use default handler if task has no custom callback */
            if (cb == NULL) {
                cb = wdt_default_cb ? wdt_default_cb : prv_default_timeout_handler;
                param = wdt_default_cb ? wdt_default_param : NULL;
            }

            /*
             * Fire the callback. Note: this runs in ISR context.
             * The callback must be short and non-blocking.
             */
            cb(handle, param);

            /* Reset the countdown so it doesn't fire every tick */
            wdt_table[i].remaining = wdt_table[i].deadline;
        }
    }
}

#endif /* OS_CONFIG_USE_WATCHDOG */
