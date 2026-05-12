#ifndef OS_PORT_H
#define OS_PORT_H

#include "os_types.h"

/*
 * Port layer for ARM Cortex-M3 (STM32F1/F4 etc.)
 * This is the hardware abstraction layer.
 */

/* ========== Stack Initialization ========== */

/*
 * Initialize a task's stack frame so it looks like
 * the task was already running and interrupted.
 * Returns the new top-of-stack pointer.
 */
os_stack_t* os_port_stack_init(os_task_func_t func,
                                void *param,
                                os_stack_t *stack_base,
                                uint32_t stack_size);

/* ========== Context Switch ========== */

/*
 * Perform the actual context switch.
 * Saves current task context, restores next task context.
 * Implemented in assembly (port_asm.s or inline asm).
 */
void os_port_context_switch(void);

/*
 * Start the first task (special: restores context from the
 * highest-priority ready task and jumps to it).
 */
void os_port_start_first_task(void);

/* ========== Critical Sections ========== */

/*
 * Enter critical section: disable interrupts up to configurable priority.
 */
uint32_t os_port_enter_critical(void);

/*
 * Exit critical section: restore interrupt mask.
 */
void os_port_exit_critical(uint32_t mask);

/* ========== PendSV & SysTick ========== */

/*
 * Trigger PendSV for context switch.
 */
void os_port_yield(void);

/*
 * Set up the SysTick timer for the given frequency.
 */
void os_port_systick_init(uint32_t freq_hz);

/* ========== Debug Output ========== */

/*
 * Low-level debug print (UART or semihosting).
 */
void os_port_debug_print(const char *str);

#endif /* OS_PORT_H */
