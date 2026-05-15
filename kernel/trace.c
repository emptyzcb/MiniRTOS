/*
 * trace.c - Trace Logging System
 *
 * Ring buffer of timestamped events for debugging and profiling.
 * All operations are ISR-safe (single-writer, uses critical section).
 */

#include "trace.h"
#include "kernel.h"
#include "port.h"

#if OS_CONFIG_USE_TRACE

/* ========== Internal Data ========== */

static os_trace_entry_t trace_buffer[OS_CONFIG_TRACE_DEPTH];
static uint32_t trace_head = 0;     /* Next write position */
static uint32_t trace_count = 0;    /* Number of valid entries */
static bool trace_initialized = false;

/* ========== Public API ========== */

void os_trace_init(void)
{
    trace_head = 0;
    trace_count = 0;
    trace_initialized = true;

    /* Record a boot event */
    os_trace_record(OS_TRACE_USER, 0x424F4F54, 0); /* "BOOT" in hex */
}

void os_trace_record(os_trace_event_t event, uint32_t param1, uint32_t param2)
{
    if (!trace_initialized) return;

    uint32_t mask = os_port_enter_critical();

    trace_buffer[trace_head].timestamp = os_kernel_get_tick();
    trace_buffer[trace_head].event = event;
    trace_buffer[trace_head].param1 = param1;
    trace_buffer[trace_head].param2 = param2;

    trace_head = (trace_head + 1) % OS_CONFIG_TRACE_DEPTH;

    if (trace_count < OS_CONFIG_TRACE_DEPTH) {
        trace_count++;
    }

    os_port_exit_critical(mask);
}

uint32_t os_trace_get_count(void)
{
    return trace_count;
}

os_status_t os_trace_read(uint32_t index, os_trace_entry_t *entry)
{
    if (entry == NULL || index >= trace_count) {
        return OS_ERR_PARAM;
    }

    /* Calculate the actual position: oldest entry first */
    uint32_t pos;
    if (trace_count < OS_CONFIG_TRACE_DEPTH) {
        pos = index;
    } else {
        pos = (trace_head + index) % OS_CONFIG_TRACE_DEPTH;
    }

    *entry = trace_buffer[pos];
    return OS_OK;
}

void os_trace_clear(void)
{
    uint32_t mask = os_port_enter_critical();
    trace_head = 0;
    trace_count = 0;
    os_port_exit_critical(mask);
}

uint32_t os_trace_iterate(os_trace_visitor_t visitor, void *param)
{
    if (visitor == NULL) return 0;

    os_trace_entry_t entry;
    uint32_t count = trace_count;

    for (uint32_t i = 0; i < count; i++) {
        if (os_trace_read(i, &entry) == OS_OK) {
            visitor(&entry, param);
        }
    }

    return count;
}

#endif /* OS_CONFIG_USE_TRACE */
