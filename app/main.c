/*
 * main.c - MiniOS Demo Application (v0.5.0)
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
 *   - Task notification (lightweight signaling)
 *   - Mailbox (single-element message passing)
 *   - Software watchdog (task liveness monitoring)
 *   - CPU usage statistics
 *   - Idle hook
 *   - System info query
 *   - Trace logging
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

/* Task handles for notification demo */
static os_task_handle_t sensor_task_handle;
static os_task_handle_t monitor_task_handle;
static os_task_handle_t producer_task_handle;
static os_task_handle_t consumer_task_handle;

/* Mailbox: single-element signaling */
static os_mailbox_t alarm_mailbox;

/* ========== Timer Callback ========== */

static void heartbeat_callback(os_timer_t *timer)
{
    (void)timer;
    heartbeat_count++;
}

/* ========== Idle Hook Demo ========== */

static volatile uint32_t idle_tick_count = 0;

static void my_idle_hook(void)
{
    idle_tick_count++;
    __asm volatile("wfi");
}

/* ========== Task 1: Queue Producer ========== */

static os_stack_t task1_stack[64];
static void producer_task(void *param)
{
    (void)param;
    uint32_t value = 0;

    while (1) {
        value++;
        os_queue_send(&demo_queue, &value, OS_WAIT_FOREVER);

        /* Signal data ready */
        os_sem_give(&data_ready_sem);

        /* Notify sensor task via task notification */
        os_task_notify(sensor_task_handle, value);

        /* Send alarm via mailbox every 10 iterations */
        if ((value % 10) == 0) {
            uint32_t alarm_code = 0xA000 | value;
            os_mailbox_send(&alarm_mailbox, &alarm_code, OS_WAIT_NONE);
        }

        /* Feed watchdog */
        os_wdt_feed();

        OS_DELAY_MS(200);
    }
}

/* ========== Task 2: Queue Consumer + Mutex ========== */

static os_stack_t task2_stack[64];
static void consumer_task(void *param)
{
    (void)param;
    uint32_t received;
    uint32_t alarm;

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

            /* Trace the event */
            os_trace_record(OS_TRACE_USER + 1, received, shared_counter);
        }

        /* Non-blocking check for alarm from mailbox */
        if (os_mailbox_receive(&alarm_mailbox, &alarm, OS_WAIT_NONE) == OS_OK) {
            os_trace_record(OS_TRACE_USER + 4, alarm, 0);
        }

        /* Feed watchdog */
        os_wdt_feed();
    }
}

/* ========== Task 3: Event Group Waiter ========== */

static os_stack_t task3_stack[64];
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

/* ========== Task 4: Sensor (Notification Receiver) ========== */

static os_stack_t task4_stack[64];
static void sensor_task(void *param)
{
    (void)param;
    uint32_t notify_val;

    while (1) {
        /* Wait for notification from producer */
        if (os_task_notify_wait(&notify_val, OS_WAIT_FOREVER) == OS_OK) {
            /* Process the notification value */
            os_trace_record(OS_TRACE_USER + 2, notify_val, 0);
        }
    }
}

/* ========== Task 5: System Monitor ========== */

static os_stack_t task5_stack[64];
static void monitor_task(void *param)
{
    (void)param;
    os_sysinfo_t sysinfo;

    while (1) {
        /* Query system info */
        os_sysinfo_get(&sysinfo);

        /* Get CPU usage for each task */
        uint32_t cpu_sensor = os_stats_get_cpu_usage(sensor_task_handle);

        /* Record monitor snapshot as trace */
        os_trace_record(OS_TRACE_USER + 3, sysinfo.heap_free, cpu_sensor);

        (void)sysinfo;
        (void)cpu_sensor;
        (void)heartbeat_count;
        (void)shared_counter;
        (void)idle_tick_count;

        OS_DELAY_SEC(3);
    }
}

/* ========== Main ========== */

int main(void)
{
    /* Initialize the OS kernel */
    os_kernel_init();

    /* Register an idle hook */
    os_task_register_idle_hook(my_idle_hook);

    /* Create synchronization objects */
    os_queue_create(&demo_queue, sizeof(uint32_t), 10);
    os_sem_create_binary(&data_ready_sem);
    os_mutex_create(&counter_mutex);
    os_eventgroup_create(&demo_events);
    os_mailbox_create(&alarm_mailbox, sizeof(uint32_t));

    /* Create software timer (1 second auto-reload heartbeat) */
    os_timer_create(&heartbeat_timer, "HB", OS_CONFIG_TICK_RATE_HZ,
                    OS_TIMER_AUTO_RELOAD, heartbeat_callback);
    os_timer_start(&heartbeat_timer, OS_WAIT_NONE);

    /* Create application tasks */
    os_task_create(producer_task,    "PROD",  NULL, 2,
                   task1_stack, sizeof(task1_stack), &producer_task_handle);

    os_task_create(consumer_task,    "CONS",  NULL, 3,
                   task2_stack, sizeof(task2_stack), &consumer_task_handle);

    os_task_create(event_waiter_task,"EVT",   NULL, 4,
                   task3_stack, sizeof(task3_stack), NULL);

    os_task_create(sensor_task,      "SENS",  NULL, 3,
                   task4_stack, sizeof(task4_stack), &sensor_task_handle);

    os_task_create(monitor_task,     "MON",   NULL, 6,
                   task5_stack, sizeof(task5_stack), &monitor_task_handle);

    /* Register watchdog for critical tasks (deadline: 5000 ticks = 5s) */
    os_wdt_register(producer_task_handle, 5000, NULL, NULL);
    os_wdt_register(consumer_task_handle, 5000, NULL, NULL);

    /* Start the OS (never returns) */
    os_kernel_start();

    /* Should never reach here */
    while (1) {}
    return 0;
}
