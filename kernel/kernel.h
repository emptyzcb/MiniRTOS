#ifndef OS_KERNEL_H
#define OS_KERNEL_H

#include "os_types.h"

/* ========== Kernel API ========== */

/*
 * Initialize the entire OS kernel.
 * Must be called before any other OS function.
 */
void os_kernel_init(void);

/*
 * Start the OS (starts scheduler, never returns).
 */
void os_kernel_start(void);

/*
 * Get the current system tick count.
 */
os_tick_t os_kernel_get_tick(void);

/*
 * Increment the system tick. Called from the SysTick ISR.
 */
void os_kernel_tick_increment(void);

/*
 * Get the OS version string.
 */
const char* os_kernel_get_version(void);

/*
 * Assert failure handler (halts the system).
 */
void os_assert_failed(const char *file, uint32_t line);


/* ========== Tick Hook ========== */

/*
 * Tick hook function type. Called on every system tick from ISR context.
 * Keep tick hooks short and non-blocking.
 */
typedef void (*os_tick_hook_t)(void);

/*
 * Register a tick hook. Multiple hooks can be registered.
 * Returns OS_OK on success, OS_ERR_FULL if no more slots.
 */
os_status_t os_kernel_register_tick_hook(os_tick_hook_t hook);
os_status_t os_kernel_unregister_tick_hook(os_tick_hook_t hook);

#endif /* OS_KERNEL_H */
