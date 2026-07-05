#ifndef OS_EVENTGROUP_H
#define OS_EVENTGROUP_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_EVENTGROUP

/* Forward declaration */
struct os_tcb;

/* ========== Event Group Control Block ========== */

typedef struct os_eventgroup {
    uint32_t    bits;               /* Current event bits */
    struct os_tcb *wait_list;       /* Tasks waiting for bits */
} os_eventgroup_t;

/* ========== Wait Options ========== */

#define OS_EVENT_WAIT_ANY       0x00000000U  /* Wake on any requested bit */
#define OS_EVENT_WAIT_ALL       0x80000000U  /* Wake on all requested bits */
#define OS_EVENT_CLEAR_ON_EXIT  0x40000000U  /* Clear matched bits on exit */

/* ========== Event Group API ========== */

os_status_t os_eventgroup_create(os_eventgroup_t *eg);
os_status_t os_eventgroup_delete(os_eventgroup_t *eg);

os_status_t os_eventgroup_set_bits(os_eventgroup_t *eg, uint32_t bits);
os_status_t os_eventgroup_set_bits_from_isr(os_eventgroup_t *eg, uint32_t bits);
os_status_t os_eventgroup_clear_bits(os_eventgroup_t *eg, uint32_t bits);
os_status_t os_eventgroup_clear_bits_from_isr(os_eventgroup_t *eg, uint32_t bits);

uint32_t os_eventgroup_wait_bits(os_eventgroup_t *eg, uint32_t bits_to_wait,
                                 uint32_t options, os_tick_t timeout);
uint32_t os_eventgroup_get_bits(os_eventgroup_t *eg);
uint32_t os_eventgroup_get_bits_from_isr(os_eventgroup_t *eg);

/* Internal: remove a task from event group blocked list */
void os_eventgroup_remove_task(struct os_eventgroup *eg, struct os_tcb *tcb);

#endif /* OS_CONFIG_USE_EVENTGROUP */

#endif /* OS_EVENTGROUP_H */
