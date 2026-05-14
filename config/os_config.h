#ifndef OS_CONFIG_H
#define OS_CONFIG_H

/* ========== OS Kernel Configuration ========== */

/* Maximum number of tasks */
#define OS_CONFIG_MAX_TASKS             16

/* System tick rate (Hz) */
#define OS_CONFIG_TICK_RATE_HZ         1000

/* Default task stack size (in bytes) */
#define OS_CONFIG_DEFAULT_STACK_SIZE    512

/* Idle task stack size (in bytes) */
#define OS_CONFIG_IDLE_STACK_SIZE       256

/* Maximum task name length */
#define OS_CONFIG_MAX_NAME_LEN          16

/* Number of priority levels (0 = highest) */
#define OS_CONFIG_NUM_PRIORITIES        8

/* Enable round-robin within same priority */
#define OS_CONFIG_USE_TIME_SLICING      1

/* Time slice length in ticks (only used with time slicing) */
#define OS_CONFIG_TIME_SLICE_TICKS      5

/* ========== Heap-4 Configuration ========== */

/* Total heap size in bytes */
#define OS_CONFIG_HEAP_SIZE             (14 * 1024)

/* Minimum block size for heap alignment */
#define OS_CONFIG_HEAP_ALIGNMENT        8

/* ========== Synchronization Primitives ========== */

/* Queue */
#define OS_CONFIG_USE_QUEUE             1

/* Semaphore */
#define OS_CONFIG_USE_SEMAPHORE         1

/* Mutex */
#define OS_CONFIG_USE_MUTEX             1

/* Software Timer */
#define OS_CONFIG_USE_SOFTWARE_TIMERS   1
#define OS_CONFIG_TIMER_SERVICE_STACK   256

/* Event Group */
#define OS_CONFIG_USE_EVENTGROUP        1

/* ========== Debug & Assert ========== */

#define OS_CONFIG_ASSERT_ENABLE         1
#define OS_CONFIG_DEBUG_LOG_ENABLE      1
#define OS_CONFIG_STACK_OVERFLOW_CHECK  1

#if OS_CONFIG_ASSERT_ENABLE
    #define OS_ASSERT(expr)  do { if (!(expr)) os_assert_failed(__FILE__, __LINE__); } while(0)
#else
    #define OS_ASSERT(expr)  ((void)0)
#endif

#endif /* OS_CONFIG_H */
