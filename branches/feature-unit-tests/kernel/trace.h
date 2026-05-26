#ifndef OS_TRACE_H
#define OS_TRACE_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_TRACE

/* ========== Trace Events ========== */

typedef enum {
    OS_TRACE_TASK_CREATE    = 0x00,
    OS_TRACE_TASK_DELETE    = 0x01,
    OS_TRACE_TASK_SWITCH    = 0x02,
    OS_TRACE_TASK_DELAY     = 0x03,
    OS_TRACE_TASK_SUSPEND   = 0x04,
    OS_TRACE_TASK_RESUME    = 0x05,
    OS_TRACE_ISR_ENTER      = 0x10,
    OS_TRACE_ISR_EXIT       = 0x11,
    OS_TRACE_QUEUE_SEND     = 0x20,
    OS_TRACE_QUEUE_RECV     = 0x21,
    OS_TRACE_SEM_GIVE       = 0x30,
    OS_TRACE_SEM_TAKE       = 0x31,
    OS_TRACE_MUTEX_LOCK     = 0x40,
    OS_TRACE_MUTEX_UNLOCK   = 0x41,
    OS_TRACE_USER           = 0x80,  /* User-defined events start here */
} os_trace_event_t;

/* ========== Trace Entry ========== */

typedef struct {
    os_tick_t           timestamp;
    os_trace_event_t    event;
    uint32_t            param1;
    uint32_t            param2;
} os_trace_entry_t;

/* ========== Trace API ========== */

/*
 * Initialize the trace system (called from os_kernel_init).
 */
void os_trace_init(void);

/*
 * Record a trace event. Safe to call from ISR context.
 */
void os_trace_record(os_trace_event_t event, uint32_t param1, uint32_t param2);

/*
 * Get the number of entries in the trace buffer.
 */
uint32_t os_trace_get_count(void);

/*
 * Read a trace entry by index (0 = oldest).
 * Returns OS_OK on success, OS_ERR_PARAM if index invalid.
 */
os_status_t os_trace_read(uint32_t index, os_trace_entry_t *entry);

/*
 * Clear all trace entries.
 */
void os_trace_clear(void);

/*
 * Iterate all entries via callback. Callback receives entry pointer and user param.
 * Returns number of entries iterated.
 */
typedef void (*os_trace_visitor_t)(const os_trace_entry_t *entry, void *param);
uint32_t os_trace_iterate(os_trace_visitor_t visitor, void *param);

#endif /* OS_CONFIG_USE_TRACE */

#endif /* OS_TRACE_H */
