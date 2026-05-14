/*
 * port.c - Port Layer for ARM Cortex-M3
 *
 * Hardware abstraction for STM32 (Cortex-M3/M4).
 * Implements stack initialization, context switch via PendSV,
 * SysTick setup, and critical sections.
 */

#include "port.h"
#include "os_config.h"
#include "task.h"

/* ========== ARM Cortex-M3 Register Definitions ========== */

/* System Control Block */
#define SCB_BASE            0xE000ED00UL
#define SCB                 ((scb_t*)SCB_BASE)
#define SCB_ICSR            (*(volatile uint32_t*)(SCB_BASE + 0x04))
#define SCB_VTOR            (*(volatile uint32_t*)(SCB_BASE + 0x08))
#define SCB_SHPR3           (*(volatile uint32_t*)(SCB_BASE + 0x20))

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
    volatile uint32_t SHPR[3];
    volatile uint32_t SHCSR;
} scb_t;

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} systick_t;

typedef struct {
    volatile uint32_t ISER[8];
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];
} nvic_t;

/* ========== Constants ========== */

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_LOAD_MAX        0x00FFFFFFUL

#define SCB_ICSR_PENDSVSET      (1UL << 28)

#define CORTEX_M3_PRIO_BITS     4   /* STM32F1 uses 4 priority bits */

/* Initial xPSR value (Thumb mode bit set) */
#define INITIAL_XPSR            0x01000000UL

/* EXC_RETURN value for returning to Thread mode using PSP */
#define EXC_RETURN_PSP          0xFFFFFFFDUL

/* ========== Stack Frame Layout ========== */

/*
 * Cortex-M3 exception stack frame (pushed by hardware):
 *   R0, R1, R2, R3, R12, LR, PC, xPSR   (8 words)
 *
 * Software-saved context (pushed by PendSV handler):
 *   R4, R5, R6, R7, R8, R9, R10, R11     (8 words)
 *
 * Total: 16 words = 64 bytes minimum stack frame
 */

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

/* ========== PendSV & SysTick ========== */

void os_port_yield(void)
{
    /* Set PendSV bit to trigger context switch */
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

void os_port_systick_init(uint32_t freq_hz)
{
    /* Calculate reload value */
    /* Assuming SystemCoreClock = 72MHz for STM32F103 */
    uint32_t reload = 72000000UL / freq_hz;
    if (reload > SYSTICK_LOAD_MAX) {
        reload = SYSTICK_LOAD_MAX;
    }

    SYSTICK->LOAD = reload - 1;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = SYSTICK_CTRL_ENABLE |
                    SYSTICK_CTRL_TICKINT |
                    SYSTICK_CTRL_CLKSOURCE;

    /* Set PendSV to lowest priority (for context switch) */
    /* SHPR3[23:16] = PendSV priority */
    SCB_SHPR3 |= (0xFFUL << 16);

    /* Set SysTick priority just above PendSV */
    /* SHPR3[31:24] = SysTick priority */
    SCB_SHPR3 |= (0xFFUL << 24);
}

/* ========== Debug Output ========== */

void os_port_debug_print(const char *str)
{
    /* Stub: in real implementation, use UART or semihosting */
    (void)str;
}

/* ========== Context Switch (Assembly) ========== */

/*
 * PendSV_Handler - performs the actual context switch.
 *
 * This is implemented in assembly for portability and correctness.
 * The handler:
 *   1. Disables interrupts
 *   2. Saves R4-R11 to current task's stack
 *   3. Updates current TCB's stack pointer
 *   4. Loads next task's stack pointer from its TCB
 *   5. Restores R4-R11 from new stack
 *   6. Re-enables interrupts
 *   7. Returns to new task
 *
 * Note: In a real STM32 project, this would be in port_asm.s
 * with proper .weak declarations. Here we use GCC inline asm.
 */

void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n"                     /* Disable interrupts */

        "mrs r0, psp\n"                 /* Get PSP (Process Stack Pointer) */

        "stmdb r0!, {r4-r11}\n"         /* Save R4-R11 */

        /* Save current stack pointer to current TCB */
        "ldr r1, =current_task_ptr\n"
        "ldr r2, [r1]\n"
        "str r0, [r2]\n"                /* tcb->stack_ptr = sp */

        /* Call scheduler to select next task */
        "push {r1, r14}\n"
        "bl os_sched_select_next\n"
        "pop {r1, r14}\n"

        /* Load new task's stack pointer */
        "ldr r2, [r1]\n"
        "ldr r0, [r2]\n"                /* sp = new_tcb->stack_ptr */

        /* Restore context */
        "ldmia r0!, {r4-r11}\n"         /* Restore R4-R11 */

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
    /* Set PSP to the first task's stack pointer */
    os_tcb_t *tcb = os_task_get_current();
    if (tcb == NULL) return;

    /* PSP = tcb->stack_ptr */
    __asm volatile(
        "msr psp, %0\n"                 /* Set PSP to stack pointer */
        "mov r0, #2\n"                  /* Switch to PSP (CONTROL.SPSEL=1) */
        "msr control, r0\n"
        "isb\n"
        "pop {r4-r11}\n"                /* Restore initial context */
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
    /* Increment OS tick */
    extern void os_kernel_tick_increment(void);
    os_kernel_tick_increment();

    /* Request context switch if needed */
    os_port_yield();
}
