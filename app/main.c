/*
 * main.c - MiniOS Demo Application
 *
 * Demonstrates basic OS functionality:
 *   - Task creation with different priorities
 *   - Task delay and scheduling
 *   - Heap memory allocation
 */

#include "os.h"
#include <string.h>

/* ========== Task Definitions ========== */

/* Task 1: High priority - LED blink simulation */
static os_stack_t task1_stack[128];  /* 512 bytes */
static void task1_func(void *param)
{
    (void)param;
    while (1) {
        /* Simulate LED toggle */
        OS_DELAY_MS(500);
    }
}

/* Task 2: Medium priority - data processing */
static os_stack_t task2_stack[128];
static void task2_func(void *param)
{
    (void)param;
    uint32_t counter = 0;
    while (1) {
        counter++;
        OS_DELAY_MS(100);
    }
}

/* Task 3: Low priority - system monitor */
static os_stack_t task3_stack[128];
static void task3_func(void *param)
{
    (void)param;
    while (1) {
        /* Monitor heap usage */
        uint32_t free_heap = os_heap_get_free_size();
        uint32_t min_heap  = os_heap_get_min_free_size();
        (void)free_heap;
        (void)min_heap;
        OS_DELAY_SEC(2);
    }
}

/* ========== Main ========== */

int main(void)
{
    /* Initialize the OS kernel */
    os_kernel_init();

    /* Create application tasks */
    os_task_create(task1_func, "LED",   NULL, 2,
                   task1_stack, sizeof(task1_stack), NULL);

    os_task_create(task2_func, "PROC",  NULL, 4,
                   task2_stack, sizeof(task2_stack), NULL);

    os_task_create(task3_func, "MON",   NULL, 6,
                   task3_stack, sizeof(task3_stack), NULL);

    /* Start the OS (never returns) */
    os_kernel_start();

    /* Should never reach here */
    while (1) {}
    return 0;
}
