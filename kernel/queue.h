#ifndef OS_QUEUE_H
#define OS_QUEUE_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_QUEUE

/* Forward declaration */
struct os_tcb;

/* ========== Queue Control Block ========== */

typedef struct os_queue {
    uint8_t     *buffer;            /* Ring buffer storage (heap-allocated) */
    uint32_t    item_size;          /* Size of each item in bytes */
    uint32_t    max_items;          /* Maximum number of items */
    uint32_t    count;              /* Current number of items */
    uint32_t    head;               /* Byte index of next item to read */
    uint32_t    tail;               /* Byte index of next slot to write */
    uint32_t    buf_size;           /* Total buffer size in bytes */

    /* Blocked task lists (priority-ordered, using TCB next/prev) */
    struct os_tcb *send_wait_list;  /* Tasks waiting to send (queue full) */
    struct os_tcb *recv_wait_list;  /* Tasks waiting to receive (queue empty) */
} os_queue_t;

/* ========== Queue API ========== */

os_status_t os_queue_create(os_queue_t *queue, uint32_t item_size,
                            uint32_t max_items);
os_status_t os_queue_delete(os_queue_t *queue);
os_status_t os_queue_reset(os_queue_t *queue);

os_status_t os_queue_send(os_queue_t *queue, const void *item,
                          os_tick_t timeout);
os_status_t os_queue_send_from_isr(os_queue_t *queue, const void *item);
os_status_t os_queue_overwrite(os_queue_t *queue, const void *item);
os_status_t os_queue_overwrite_from_isr(os_queue_t *queue, const void *item);

os_status_t os_queue_receive(os_queue_t *queue, void *item,
                             os_tick_t timeout);
os_status_t os_queue_receive_from_isr(os_queue_t *queue, void *item);

os_status_t os_queue_peek(os_queue_t *queue, void *item);

uint32_t os_queue_get_count(os_queue_t *queue);
uint32_t os_queue_get_count_from_isr(os_queue_t *queue);
uint32_t os_queue_get_spaces(os_queue_t *queue);
bool os_queue_is_empty(os_queue_t *queue);
bool os_queue_is_full(os_queue_t *queue);

/* Internal: remove a task from queue blocked list (called by task tick handler) */
void os_queue_remove_task(struct os_queue *queue, struct os_tcb *tcb);

#endif /* OS_CONFIG_USE_QUEUE */

#endif /* OS_QUEUE_H */
