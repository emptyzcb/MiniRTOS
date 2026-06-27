/*
 * port_m0.c - Port Layer for ARM Cortex-M0 (STM32F030)
 *
 * Hardware abstraction for Cortex-M0.
 * Key differences from M3 port:
 *   - NO BASEPRI register: only PRIMASK-based critical sections
 *   - NO FPU
 *   - Thumb instruction set (not Thumb-2)
 *   - Only 2 priority bits (4 levels)
 *   - SystemCoreClock = 48 MHz
 */

#include "port.h"
#include "os_config.h"
#include "task.h"

/* ========== ARM Cortex-M0 Register Definitions ========== */

/* System Control Block */
#define SCB_BASE            0xE000ED00UL
#define SCB                 ((scb_t*)SCB_BASE)
#define SCB_ICSR            (*(volatile uint32_t*)(SCB_BASE + 0x04))
#define SCB_VTOR            (*(volatile uint32_t*)(SCB_BASE + 0x08))

/* SysTick */
#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK             ((systick_t*)SYSTICK_BASE)

/* NVIC */
#define NVIC_BASE           0xE000E100UL
#define NVIC                ((nvic_t*)NVIC_BASE)

/* ========== Register Structures ========== */

typedef struct {
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR;
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint32_t SHPR[2];      /* M0 only has SHPR1-SHPR2 (8 bytes) */
} scb_t;

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} systick_t;

typedef struct {
    volatile uint32_t ISER[1];      /* M0: only 1 ISER register */
    uint32_t RESERVED0[31];
    volatile uint32_t ICER[1];      /* M0: only 1 ICER register */
    uint32_t RESERVED1[31];
    volatile uint32_t ISPR[1];      /* M0: only 1 ISPR register */
    uint32_t RESERVED2[31];
    volatile uint32_t ICPR[1];      /* M0: only 1 ICPR register */
    uint32_t RESERVED3[31];
    uint32_t RESERVED4[64];
    volatile uint32_t IP[8];        /* M0: only 8 priority registers (32 IRQs) */
} nvic_t;

/* ========== Constants ========== */

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_LOAD_MAX        0x00FFFFFFUL

#define SCB_ICSR_PENDSVSET      (1UL << 28)

/* Cortex-M0: 2 priority bits */
#define CORTEX_M0_PRIO_BITS     2

/* AIRCR register constants */
#define SCB_AIRCR_VECTKEY       0x05FA0000UL
#define SCB_AIRCR_PRIGROUP_MASK 0x00000700UL

/* Initial xPSR value (Thumb mode bit set) */
#define INITIAL_XPSR            0x01000000UL

/* ========== Stack Initialization ========== */

os_stack_t* os_port_stack_init(os_task_func_t func,
                                void *param,
                                os_stack_t *stack_base,
                                uint32_t stack_size)
{
    os_stack_t *sp;

    /* Align stack to 8-byte boundary (ARM AAPCS requirement) */
    sp = (os_stack_t*)((((uint32_t)stack_base + stack_size) + 7) & ~7UL);

    /* Hardware-saved frame (8 words, pushed from top) */
    *(--sp) = INITIAL_XPSR;                 /* xPSR */
    *(--sp) = (uint32_t)func;               /* PC (task entry point) */
    *(--sp) = 0xFFFFFFFEUL;                 /* LR (return to Thread mode) */
    *(--sp) = 0x12121212UL;                 /* R12 */
    *(--sp) = 0x03030303UL;                 /* R3 */
    *(--sp) = 0x02020202UL;                 /* R2 */
    *(--sp) = 0x01010101UL;                 /* R1 */
    *(--sp) = (uint32_t)param;              /* R0 (task parameter) */

    /* Software-saved frame (8 words) */
    *(--sp) = 0x11111111UL;                 /* R11 */
    *(--sp) = 0x10101010UL;                 /* R10 */
    *(--sp) = 0x09090909UL;                 /* R9 */
    *(--sp) = 0x08080808UL;                 /* R8 */
    *(--sp) = 0x07070707UL;                 /* R7 */
    *(--sp) = 0x06060606UL;                 /* R6 */
    *(--sp) = 0x05050505UL;                 /* R5 */
    *(--sp) = 0x04040404UL;                 /* R4 */

    return sp;
}

/* ========== Critical Sections ========== */

/*
 * Cortex-M0 has NO BASEPRI register.
 * Only PRIMASK-based (global interrupt disable) critical sections are supported.
 * OS_CONFIG_USE_INTERRUPT_NESTING must be set to 0 for M0 targets.
 */
#if OS_CONFIG_USE_INTERRUPT_NESTING
    #error "Cortex-M0 does not support BASEPRI. Set OS_CONFIG_USE_INTERRUPT_NESTING to 0."
#endif

uint32_t os_port_enter_critical(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask" : "=r"(primask));
    __asm volatile("cpsid i" ::: "memory");
    return primask;
}

void os_port_exit_critical(uint32_t mask)
{
    __asm volatile("msr primask, %0" :: "r"(mask) : "memory");
}

/* ========== ISR Entry/Exit ========== */

/* Not supported on M0 (no BASEPRI for interrupt nesting) */

/* ========== PendSV & SysTick ========== */

void os_port_yield(void)
{
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

void os_port_systick_init(uint32_t freq_hz)
{
    /* STM32F030: SystemCoreClock = 48 MHz */
    uint32_t reload = OS_CONFIG_CPU_CLOCK_HZ / freq_hz;
    if (reload > SYSTICK_LOAD_MAX) {
        reload = SYSTICK_LOAD_MAX;
    }

    SYSTICK->LOAD = reload - 1;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = SYSTICK_CTRL_ENABLE |
                    SYSTICK_CTRL_TICKINT |
                    SYSTICK_CTRL_CLKSOURCE;
}

/* ========== NVIC Priority Group Management ========== */

void os_port_nvic_set_priority_group(uint32_t group)
{
    uint32_t aircr;

    aircr = SCB->AIRCR;
    aircr &= ~(SCB_AIRCR_PRIGROUP_MASK | 0xFFFFUL);
    aircr |= SCB_AIRCR_VECTKEY | (group << 8);
    SCB->AIRCR = aircr;
}

void os_port_nvic_set_priority(int32_t irqn, uint32_t priority)
{
    if (irqn >= 0) {
        /* M0: IP registers are 8-bit, only upper 2 bits used */
        NVIC->IP[(uint32_t)irqn] = (uint8_t)((priority << (8 - OS_CONFIG_NVIC_PRIO_BITS)) & 0xFFUL);
    } else {
        /* M0 system exceptions: SHPR1-SHPR2
         * irqn mapping: -4=SHPR[0][7:0], -5=SHPR[0][15:8], -6=SHPR[0][23:16], -7=SHPR[0][31:24]
         *               -8=SHPR[1][7:0], -9=SHPR[1][15:8], -10=SHPR[1][23:16], -11=SHPR[1][31:24]
         *               -12=SHPR[1] (reserved), -13=SHPR[2][7:0], -14=SHPR[2][15:8] (PendSV)
         *
         * Note: M0 has SHPR1-SHPR2 (indices 0-1) for system exceptions -12 to -4
         * PendSV (-14) and SysTick (-1) are in SHPR2 (index 1)
         */
        uint32_t idx;
        uint32_t shift;

        /* Map irqn to SHPR register index and byte position */
        if (irqn >= -11 && irqn <= -4) {
            idx = 0;
            shift = ((uint32_t)(-irqn - 4)) * 8UL;
        } else if (irqn >= -14 && irqn <= -12) {
            idx = 1;
            shift = ((uint32_t)(-irqn - 12)) * 8UL;
        } else if (irqn == -1) {
            /* SysTick: SHPR2[31:24] */
            idx = 1;
            shift = 24UL;
        } else {
            return; /* Unsupported irqn */
        }

        SCB->SHPR[idx] &= ~(0xFFUL << shift);
        SCB->SHPR[idx] |= ((priority << (8 - OS_CONFIG_NVIC_PRIO_BITS)) & 0xFFUL) << shift;
    }
}

void os_port_nvic_enable_irq(int32_t irqn)
{
    if (irqn >= 0) {
        NVIC->ISER[0] = (1UL << (uint32_t)irqn);
    }
}

void os_port_nvic_disable_irq(int32_t irqn)
{
    if (irqn >= 0) {
        NVIC->ICER[0] = (1UL << (uint32_t)irqn);
    }
}

void os_port_nvic_set_priority_init(void)
{
    /* Set PendSV to lowest priority (3 on M0, 2-bit priority) */
    os_port_nvic_set_priority(-14, OS_CONFIG_PENDSV_PRIORITY);

    /* Set SysTick priority */
    os_port_nvic_set_priority(-1, OS_CONFIG_SYSTICK_PRIORITY);
}

/* ========== Debug Output ========== */

void os_port_debug_print(const char *str)
{
    (void)str;
}

/* ========== Newlib Syscall Stubs ========== */

void _exit(int status)
{
    (void)status;
    while (1) { __asm volatile("bkpt #0"); }
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

int _getpid(void)
{
    return 1;
}

static char *heap_end = 0;
char* _sbrk(int incr)
{
    extern char _ebss;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_ebss;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return prev_heap_end;
}

/* ========== Context Switch (Assembly) ========== */

/*
 * PendSV_Handler for Cortex-M0.
 * Same as M3: no FPU, basic R4-R11 save/restore.
 * Note: M0 uses Thumb instruction set only (no stmdb/ldmia with r0!)
 *       Use stm/ldm with explicit register lists.
 */

void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n"                     /* Disable interrupts */

        "mrs r0, psp\n"                 /* Get PSP */

        /* Save R4-R11 to task stack */
        /* M0 doesn't support stmdb, so use stm with decrement */
        "mov r3, r0\n"
        "sub r3, r3, #32\n"             /* 8 registers * 4 bytes */
        "stmia r3!, {r4-r7}\n"          /* Save R4-R7 */
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r3!, {r4-r7}\n"          /* Save R8-R11 */
        "sub r0, r0, #32\n"             /* Adjust PSP */

        /* Save current stack pointer to current TCB */
        "ldr r1, =current_task_ptr\n"
        "ldr r2, [r1]\n"
        "str r0, [r2]\n"

        /* Call scheduler to select next task */
        "push {r1, r14}\n"
        "bl os_sched_select_next\n"
        "pop {r1, r14}\n"

        /* Load new task's stack pointer */
        "ldr r2, [r1]\n"
        "ldr r0, [r2]\n"

        /* Restore R4-R11 */
        "ldmia r0!, {r4-r7}\n"          /* Restore R4-R7 */
        "mov r8, r4\n"
        "mov r9, r5\n"
        "mov r10, r6\n"
        "mov r11, r7\n"
        "ldmia r0!, {r4-r7}\n"          /* Restore R8-R11 from stack to R4-R7, then move */

        "msr psp, r0\n"                 /* Update PSP */
        "cpsie i\n"                     /* Re-enable interrupts */
        "bx r14\n"                      /* Return to new task */
    );
}

/* Global pointer to current task (for assembly access) */
os_tcb_t *current_task_ptr = NULL;

void os_task_set_current(os_tcb_t *tcb)
{
    current_task_ptr = tcb;
}

os_task_handle_t os_task_get_current(void)
{
    return (os_task_handle_t)current_task_ptr;
}

/* ========== First Task Start ========== */

void os_port_start_first_task(void)
{
    os_tcb_t *tcb = os_task_get_current();
    if (tcb == NULL) return;

    __asm volatile(
        "msr psp, %0\n"                 /* Set PSP */
        "mov r0, #2\n"                  /* Switch to PSP */
        "msr control, r0\n"
        "isb\n"
        "pop {r4-r7}\n"                 /* Restore R4-R7 */
        "mov r8, r4\n"
        "mov r9, r5\n"
        "mov r10, r6\n"
        "mov r11, r7\n"
        "pop {r4-r7}\n"                 /* Restore R8-R11 (stored as R4-R7) */
        "pop {r0-r3}\n"
        "pop {r12}\n"
        "pop {lr}\n"
        "pop {pc}\n"                    /* Jump to task entry */
        :
        : "r"(tcb->stack_ptr)
    );
}

/* ========== SysTick Handler ========== */

void SysTick_Handler(void)
{
    extern void os_kernel_tick_increment(void);

    os_kernel_tick_increment();

    os_port_yield();
}
