#ifndef OS_CONFIG_H
#define OS_CONFIG_H

/* ========== OS Kernel Configuration ========== */

/* Maximum number of tasks */
#define OS_CONFIG_MAX_TASKS             16

/* System tick rate (Hz) */
#define OS_CONFIG_TICK_RATE_HZ         1000

/* CPU core clock frequency (Hz). STM32F103 default: 72 MHz */
#define OS_CONFIG_CPU_CLOCK_HZ          72000000

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
#define OS_CONFIG_HEAP_SIZE             (10 * 1024)

/* Minimum block size for heap alignment */
#define OS_CONFIG_HEAP_ALIGNMENT        8

/* ========== NVIC Priority Configuration ========== */

/* Number of priority bits implemented in the MCU (STM32F1: 4 bits) */
#define OS_CONFIG_NVIC_PRIO_BITS        4

/*
 * NVIC priority group value (PRIGROUP field in AIRCR).
 * Controls the split between preemption priority and sub-priority.
 *
 * Group 7: 0 bits preempt / 4 bits sub  (no preemption)
 * Group 6: 1 bit  preempt / 3 bits sub
 * Group 5: 2 bits preempt / 2 bits sub
 * Group 4: 3 bits preempt / 1 bit  sub
 * Group 3: 4 bits preempt / 0 bits sub  (full preemption, default)
 */
#define OS_CONFIG_NVIC_PRIGROUP         3

/* PendSV interrupt priority (0xF = lowest, must be lowest for context switch) */
#define OS_CONFIG_PENDSV_PRIORITY       0xF

/* SysTick interrupt priority */
#define OS_CONFIG_SYSTICK_PRIORITY      0xF

/* Enable interrupt nesting (BASEPRI-based critical sections).
 * 0 = use PRIMASK (global interrupt disable, legacy behavior)
 * 1 = use BASEPRI (only mask interrupts at or below threshold) */
#define OS_CONFIG_USE_INTERRUPT_NESTING         0

/* Maximum interrupt priority that may call RTOS _from_isr APIs.
 * Interrupts with logical priority 0 .. (threshold-1) are never masked.
 * Interrupts with logical priority threshold .. 15 are masked in critical sections.
 * Must be > 0, < (1 << OS_CONFIG_NVIC_PRIO_BITS), and >= PendSV/SysTick priority. */
#define OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Compile-time validation */
#if OS_CONFIG_USE_INTERRUPT_NESTING
    _Static_assert(OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY > 0,
        "MAX_SYSCALL_INTERRUPT_PRIORITY must be > 0");
    _Static_assert(OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY < (1 << OS_CONFIG_NVIC_PRIO_BITS),
        "MAX_SYSCALL_INTERRUPT_PRIORITY must be < (1 << NVIC_PRIO_BITS)");
    _Static_assert(OS_CONFIG_PENDSV_PRIORITY >= OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY,
        "PENDSV_PRIORITY must be >= MAX_SYSCALL_INTERRUPT_PRIORITY");
    _Static_assert(OS_CONFIG_SYSTICK_PRIORITY >= OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY,
        "SYSTICK_PRIORITY must be >= MAX_SYSCALL_INTERRUPT_PRIORITY");
#endif

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

/* Memory Pool */
#define OS_CONFIG_USE_MEMPOOL           1

/* Mailbox (single-element queue) */
#define OS_CONFIG_USE_MAILBOX           1

/* Tickless Idle (low-power mode, stops SysTick during idle) */
#define OS_CONFIG_USE_TICKLESS_IDLE     0

/* ========== Task Notification ========== */

#define OS_CONFIG_USE_TASK_NOTIFY       1

/* ========== Software Watchdog ========== */

#define OS_CONFIG_USE_WATCHDOG          1

/* ========== CPU Usage Statistics ========== */

#define OS_CONFIG_USE_STATS             1

/* ========== Idle Hooks ========== */

#define OS_CONFIG_MAX_IDLE_HOOKS        4

/* ========== Trace Logging ========== */

#define OS_CONFIG_USE_TRACE             1
#define OS_CONFIG_TRACE_DEPTH           32

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
