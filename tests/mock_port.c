/*
 * mock_port.c - Mock hardware port layer for Host-side unit testing
 *
 * Replaces the ARM Cortex-M3 port.c with no-op stubs so the RTOS
 * kernel can compile and run on x86/x64 without real hardware.
 */

#include "port.h"
#include "task.h"
#include "os_config.h"
#include "trace.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ========== Mock current task pointer ========== */

os_tcb_t *current_task_ptr = NULL;

void os_task_set_current(os_tcb_t *tcb)
{
    current_task_ptr = tcb;
}

os_task_handle_t os_task_get_current(void)
{
    return (os_task_handle_t)current_task_ptr;
}

/* ========== Mock stack initialization ========== */

os_stack_t* os_port_stack_init(os_task_func_t func,
                                void *param,
                                os_stack_t *stack_base,
                                uint32_t stack_size)
{
    /* In mock, just return a pointer near the top of the stack.
     * We don't need a real ARM stack frame. */
    (void)func;
    (void)param;
    os_stack_t *sp = (os_stack_t*)((uint8_t*)stack_base + stack_size);
    sp--; /* Make it non-NULL */
    return sp;
}

/* ========== Mock critical sections (no-op) ========== */

static uint32_t mock_critical_nesting = 0;

uint32_t os_port_enter_critical(void)
{
    mock_critical_nesting++;
    return 0;
}

void os_port_exit_critical(uint32_t mask)
{
    (void)mask;
    if (mock_critical_nesting > 0) {
        mock_critical_nesting--;
    }
}

/* ========== Mock PendSV/SysTick (no-op) ========== */

void os_port_yield(void)
{
    /* No-op: no real context switch in host tests */
}

void os_port_systick_init(uint32_t freq_hz)
{
    (void)freq_hz;
}

void os_port_start_first_task(void)
{
    /* No-op: tests don't actually start the scheduler */
}

/* ========== Mock NVIC (no-op) ========== */

void os_port_nvic_set_priority_group(uint32_t group)
{
    (void)group;
}

void os_port_nvic_set_priority(int32_t irqn, uint32_t priority)
{
    (void)irqn;
    (void)priority;
}

void os_port_nvic_enable_irq(int32_t irqn)
{
    (void)irqn;
}

void os_port_nvic_disable_irq(int32_t irqn)
{
    (void)irqn;
}

void os_port_nvic_set_priority_init(void)
{
    /* No-op */
}

/* ========== Mock debug output ========== */

void os_port_debug_print(const char *str)
{
    /* Silently swallow debug output in tests */
    (void)str;
}

/* ========== Mock ISR entry/exit ========== */

#if OS_CONFIG_USE_INTERRUPT_NESTING

void os_port_isr_enter(void)
{
    /* No-op */
}

void os_port_isr_exit(void)
{
    /* No-op */
}

#endif

/* ========== Stubs for trace module ========== */

#if OS_CONFIG_USE_TRACE

void os_trace_init(void) { }

void os_trace_record(os_trace_event_t event, uint32_t param1, uint32_t param2)
{
    (void)event;
    (void)param1;
    (void)param2;
}

uint32_t os_trace_get_count(void) { return 0; }

os_status_t os_trace_read(uint32_t index, os_trace_entry_t *entry)
{
    (void)index;
    (void)entry;
    return OS_ERR_PARAM;
}

void os_trace_clear(void) { }

uint32_t os_trace_iterate(os_trace_visitor_t visitor, void *param)
{
    (void)visitor;
    (void)param;
    return 0;
}

#endif /* OS_CONFIG_USE_TRACE */

/* ========== Stubs for stats module ========== */

#if OS_CONFIG_USE_STATS

uint32_t os_stats_get_cpu_usage(os_task_handle_t handle)
{
    (void)handle;
    return 0;
}

os_tick_t os_stats_get_run_time(os_task_handle_t handle)
{
    (void)handle;
    return 0;
}

void os_stats_reset(void) { }

#endif /* OS_CONFIG_USE_STATS */

/*
 * os_assert_failed is defined in kernel.c.
 * For host tests, it will halt in a while(1) loop (bkpt is guarded by TEST_HOST_BUILD).
 */
