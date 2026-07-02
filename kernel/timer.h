#ifndef OS_TIMER_H
#define OS_TIMER_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_SOFTWARE_TIMERS

/* Forward declaration */
struct os_timer;

/* ========== Timer Types ========== */

typedef void (*os_timer_callback_t)(struct os_timer *timer);

typedef enum {
    OS_TIMER_ONE_SHOT    = 0,
    OS_TIMER_AUTO_RELOAD = 1,
} os_timer_type_t;

/* ========== Timer Control Block ========== */

typedef struct os_timer {
    os_timer_callback_t callback;       /* Function to call on expiry */
    os_tick_t           period;         /* Period in ticks */
    os_tick_t           remaining;      /* Ticks remaining until expiry */
    os_timer_type_t     type;           /* One-shot or auto-reload */
    bool                active;         /* Is the timer running? */
    char                name[OS_CONFIG_MAX_NAME_LEN];  /* Timer name for debugging */
    struct os_timer     *next;          /* Linked list pointer */
} os_timer_t;

/* ========== Timer API ========== */

os_status_t os_timer_init(void);

os_status_t os_timer_create(os_timer_t *timer, const char *name,
                            os_tick_t period, os_timer_type_t type,
                            os_timer_callback_t callback);
os_status_t os_timer_delete(os_timer_t *timer, os_tick_t timeout);
os_status_t os_timer_start(os_timer_t *timer, os_tick_t timeout);
os_status_t os_timer_stop(os_timer_t *timer, os_tick_t timeout);
os_status_t os_timer_reset(os_timer_t *timer, os_tick_t timeout);
os_status_t os_timer_change_period(os_timer_t *timer, os_tick_t new_period,
                                   os_tick_t timeout);


os_status_t os_timer_start_from_isr(os_timer_t *timer);
os_status_t os_timer_stop_from_isr(os_timer_t *timer);
os_status_t os_timer_reset_from_isr(os_timer_t *timer);
os_status_t os_timer_change_period_from_isr(os_timer_t *timer, os_tick_t new_period);

/* Called from os_kernel_tick_increment() */
void os_timer_tick(void);

/* Get timer name (for debugging) */
const char* os_timer_get_name(os_timer_t *timer);

#endif /* OS_CONFIG_USE_SOFTWARE_TIMERS */

#endif /* OS_TIMER_H */
