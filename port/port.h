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

/* ========== NVIC Priority Group Management ========== */

/*
 * Configure NVIC priority group (AIRCR.PRIGROUP).
 * Controls the split between preemption priority and sub-priority.
 * Common values: 3 = 4-bit preempt / 0-bit sub (full preemption)
 */
void os_port_nvic_set_priority_group(uint32_t group);

/*
 * Set the priority for an interrupt or system exception.
 * irqn >= 0: peripheral interrupt (0, 1, 2, ...)
 * irqn < 0:  system exception (-1=SysTick, -4=MemManage, -5=BusFault,
 *             -6=UsageFault, -11=SVCall, -14=PendSV)
 * priority: 0 = highest, (1 << OS_CONFIG_NVIC_PRIO_BITS) - 1 = lowest
 */
void os_port_nvic_set_priority(int32_t irqn, uint32_t priority);

/*
 * Enable a peripheral interrupt in NVIC.
 */
void os_port_nvic_enable_irq(int32_t irqn);

/*
 * Disable a peripheral interrupt in NVIC.
 */
void os_port_nvic_disable_irq(int32_t irqn);

/*
 * Initialize NVIC priority configuration.
 * Called from os_kernel_init(). Sets priority group and
 * configures PendSV/SysTick priorities.
 */
void os_port_nvic_set_priority_init(void);

/* ========== Debug Output ========== */

/*
 * Low-level debug print (UART or semihosting).
 */
void os_port_debug_print(const char *str);

#endif /* OS_PORT_H */
