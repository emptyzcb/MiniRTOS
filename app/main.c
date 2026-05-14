/*
 * main.c - MiniOS Demo Application
 *
 * Demonstrates all OS features:
 *   - Task creation with different priorities
 *   - Task delay and scheduling
 *   - Heap memory allocation
 *   - Message queue (producer-consumer)
 *   - Binary semaphore (task synchronization)
 *   - Mutex (shared resource protection)
 *   - Software timer (periodic callback)
 *   - Event group (multi-condition synchronization)
 */

#include "os.h"
#include <string.h>

/* ========== Shared Resources ========== */

static uint32_t shared_counter = 0;

/* Queue: 10 items of uint32_t */
static os_queue_t demo_queue;

/* Binary semaphore: signals data ready */
static os_sem_t data_ready_sem;

/* Mutex: protects shared_counter */
static os_mutex_t counter_mutex;

/* Timer: periodic heartbeat */
static os_timer_t heartbeat_timer;
static volatile uint32_t heartbeat_count = 0;

/* Event group: multi-condition sync */
static os_eventgroup_t demo_events;
#define EVT_SENSOR_READY    (1 << 0)
#define EVT_DATA_PROCESSED  (1 << 1)
#define EVT_ALL_DONE        (1 << 2)

/* ========== Timer Callback ========== */

static void heartbeat_callback(os_timer_t *timer)
{
    (void)timer;
    heartbeat_count++;
}

/* ========== Task 1: Queue Producer ========== */

static os_stack_t task1_stack[128];
static void producer_task(void *param)
{
    (void)param;
    uint32_t value = 0;

    while (1) {
        value++;
        os_queue_send(&demo_queue, &value, OS_WAIT_FOREVER);

        /* Signal data ready */
        os_sem_give(&data_ready_sem);

        OS_DELAY_MS(200);
    }
}

/* ========== Task 2: Queue Consumer + Mutex ========== */

static os_stack_t task2_stack[128];
static void consumer_task(void *param)
{
    (void)param;
    uint32_t received;

    while (1) {
        /* Wait for data */
        os_sem_take(&data_ready_sem, OS_WAIT_FOREVER);

        /* Receive from queue */
        if (os_queue_receive(&demo_queue, &received, OS_WAIT_FOREVER) == OS_OK) {
            /* Update shared counter under mutex protection */
            os_mutex_lock(&counter_mutex, OS_WAIT_FOREVER);
            shared_counter += received;
            os_mutex_unlock(&counter_mutex);

            /* Signal sensor data processed */
            os_eventgroup_set_bits(&demo_events, EVT_DATA_PROCESSED);
        }
    }
}

/* ========== Task 3: Event Group Waiter ========== */

static os_stack_t task3_stack[128];
static void event_waiter_task(void *param)
{
    (void)param;

    while (1) {
        /* Set sensor ready event */
        os_eventgroup_set_bits(&demo_events, EVT_SENSOR_READY);

        /* Wait for data to be processed */
        os_eventgroup_wait_bits(&demo_events, EVT_DATA_PROCESSED,
                                OS_EVENT_WAIT_ANY | OS_EVENT_CLEAR_ON_EXIT,
                                OS_WAIT_FOREVER);

        /* Signal all done */
        os_eventgroup_set_bits(&demo_events, EVT_ALL_DONE);

        OS_DELAY_SEC(1);
    }
}

/* ========== Task 4: System Monitor ========== */

static os_stack_t task4_stack[128];
static void monitor_task(void *param)
{
    (void)param;

    while (1) {
        /* Monitor system state */
        uint32_t free_heap = os_heap_get_free_size();
        uint32_t min_heap  = os_heap_get_min_free_size();
        uint32_t queue_count = os_queue_get_count(&demo_queue);
        uint32_t sem_count = os_sem_get_count(&data_ready_sem);
        uint32_t events = os_eventgroup_get_bits(&demo_events);

        (void)free_heap;
        (void)min_heap;
        (void)queue_count;
        (void)sem_count;
        (void)events;
        (void)heartbeat_count;
        (void)shared_counter;

        OS_DELAY_SEC(3);
    }
}

/* ========== Main ========== */

int main(void)
{
    /* Initialize the OS kernel */
    os_kernel_init();

    /* Create synchronization objects */
    os_queue_create(&demo_queue, sizeof(uint32_t), 10);
    os_sem_create_binary(&data_ready_sem);
    os_mutex_create(&counter_mutex);
    os_eventgroup_create(&demo_events);

    /* Create software timer (1 second auto-reload heartbeat) */
    os_timer_create(&heartbeat_timer, "HB", OS_CONFIG_TICK_RATE_HZ,
                    OS_TIMER_AUTO_RELOAD, heartbeat_callback);
    os_timer_start(&heartbeat_timer, OS_WAIT_NONE);

    /* Create application tasks */
    os_task_create(producer_task,    "PROD",  NULL, 2,
                   task1_stack, sizeof(task1_stack), NULL);

    os_task_create(consumer_task,    "CONS",  NULL, 3,
                   task2_stack, sizeof(task2_stack), NULL);

    os_task_create(event_waiter_task,"EVT",   NULL, 4,
                   task3_stack, sizeof(task3_stack), NULL);

    os_task_create(monitor_task,     "MON",   NULL, 6,
                   task4_stack, sizeof(task4_stack), NULL);

    /* Start the OS (never returns) */
    os_kernel_start();

    /* Should never reach here */
    while (1) {}
    return 0;
}
