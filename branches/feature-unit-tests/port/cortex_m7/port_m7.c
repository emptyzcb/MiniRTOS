/*
 * port_m7.c - Port Layer for ARM Cortex-M7 (STM32F746)
 *
 * Hardware abstraction for Cortex-M7 with double-precision FPU.
 * Key differences from M4 port:
 *   - Double-precision FPU: save/restore D16-D31 (32 words vs 16 for S16-S31)
 *   - SystemCoreClock = 216 MHz
 *   - Supports instruction and data caches
 */

#include "port.h"
#include "os_config.h"
#include "task.h"

/* ========== ARM Cortex-M7 Register Definitions ========== */

/* System Control Block */
#define SCB_BASE            0xE000ED00UL
#define SCB                 ((scb_t*)SCB_BASE)
#define SCB_ICSR            (*(volatile uint32_t*)(SCB_BASE + 0x04))
#define SCB_VTOR            (*(volatile uint32_t*)(SCB_BASE + 0x08))
#define SCB_SHPR3           (*(volatile uint32_t*)(SCB_BASE + 0x20))
#define SCB_CPACR           (*(volatile uint32_t*)(SCB_BASE + 0x88))

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
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];
    uint32_t RESERVED4[56];
    volatile uint32_t IP[240];
} nvic_t;

/* ========== Constants ========== */

#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_TICKINT    (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_LOAD_MAX        0x00FFFFFFUL

#define SCB_ICSR_PENDSVSET      (1UL << 28)

/* Cortex-M7: 4 priority bits */
#define CORTEX_M7_PRIO_BITS     4

/* BASEPRI shift */
#define BASEPRI_SHIFT           (8 - OS_CONFIG_NVIC_PRIO_BITS)

/* AIRCR register constants */
#define SCB_AIRCR_VECTKEY       0x05FA0000UL
#define SCB_AIRCR_PRIGROUP_MASK 0x00000700UL

/* Initial xPSR value (Thumb mode bit set) */
#define INITIAL_XPSR            0x01000000UL

/* EXC_RETURN: return to Thread mode using PSP, no FPU state */
#define EXC_RETURN_PSP          0xFFFFFFFDUL

/* CP10/CP11 full access (enable FPU) */
#define SCB_CPACR_CP10_FULL     (3UL << 20)
#define SCB_CPACR_CP11_FULL     (3UL << 22)

/* ========== FPU Enable ========== */

static void prv_fpu_enable(void)
{
    SCB_CPACR |= (SCB_CPACR_CP10_FULL | SCB_CPACR_CP11_FULL);
    __asm volatile("dsb\n\tisb" ::: "memory");
}

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

#if OS_CONFIG_USE_INTERRUPT_NESTING

uint32_t os_port_enter_critical(void)
{
    uint32_t basepri;
    uint32_t threshold = (OS_CONFIG_MAX_SYSCALL_INTERRUPT_PRIORITY << BASEPRI_SHIFT) & 0xFFUL;
    __asm volatile("mrs %0, basepri" : "=r"(basepri));
    __asm volatile("msr basepri, %0" :: "r"(threshold) : "memory");
    __asm volatile("dsb\n\tisb" ::: "memory");
    return basepri;
}

void os_port_exit_critical(uint32_t mask)
{
    __asm volatile("msr basepri, %0" :: "r"(mask) : "memory");
    __asm volatile("dsb\n\tisb" ::: "memory");
}

#else

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

#endif /* OS_CONFIG_USE_INTERRUPT_NESTING */

/* ========== ISR Entry/Exit (for interrupt nesting) ========== */

#if OS_CONFIG_USE_INTERRUPT_NESTING

extern void os_sched_isr_enter(void);
extern void os_sched_isr_exit(void);

void os_port_isr_enter(void)
{
    os_sched_isr_enter();
}

void os_port_isr_exit(void)
{
    os_sched_isr_exit();
}

#endif /* OS_CONFIG_USE_INTERRUPT_NESTING */

/* ========== PendSV & SysTick ========== */

void os_port_yield(void)
{
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

void os_port_systick_init(uint32_t freq_hz)
{
    /* STM32F746: SystemCoreClock = 216 MHz */
    uint32_t reload = 216000000UL / freq_hz;
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
        NVIC->IP[(uint32_t)irqn] = (uint8_t)((priority << (8 - OS_CONFIG_NVIC_PRIO_BITS)) & 0xFFUL);
    } else {
        uint32_t idx = ((uint32_t)(irqn & 0xF) - 4UL) / 4UL;
        uint32_t shift = (((uint32_t)(irqn & 0xF) - 4UL) % 4UL) * 8UL;
        SCB->SHPR[idx] &= ~(0xFFUL << shift);
        SCB->SHPR[idx] |= ((priority << (8 - OS_CONFIG_NVIC_PRIO_BITS)) & 0xFFUL) << shift;
    }
}

void os_port_nvic_enable_irq(int32_t irqn)
{
    if (irqn >= 0) {
        NVIC->ISER[(uint32_t)irqn >> 5] = (1UL << ((uint32_t)irqn & 0x1FUL));
    }
}

void os_port_nvic_disable_irq(int32_t irqn)
{
    if (irqn >= 0) {
        NVIC->ICER[(uint32_t)irqn >> 5] = (1UL << ((uint32_t)irqn & 0x1FUL));
    }
}

void os_port_nvic_set_priority_init(void)
{
    os_port_nvic_set_priority_group(OS_CONFIG_NVIC_PRIGROUP);
    os_port_nvic_set_priority(-6, OS_CONFIG_PENDSV_PRIORITY);  /* PendSV */
    os_port_nvic_set_priority(-1, OS_CONFIG_SYSTICK_PRIORITY); /* SysTick */
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
 * PendSV_Handler for Cortex-M7 with double-precision FPU support.
 *
 * Key difference from M4: saves/restores D16-D31 (double-precision)
 * when the FPU has been used. D16-D31 are 16 x 64-bit = 32 x 32-bit words.
 *
 * EXC_RETURN bit 4:
 *   0 → extended frame (FPU registers stacked by hardware)
 *   1 → basic frame (no FPU)
 *
 * Stack frame with FPU active (double-precision):
 *   Hardware: R0-R3, R12, LR, PC, xPSR, S0-S15, FPSCR, reserved (26 words)
 *   Software: R4-R11, D16-D31 (8 + 32 = 40 words)
 *   Total: 66 words = 264 bytes
 *
 * Stack frame without FPU:
 *   Hardware: R0-R3, R12, LR, PC, xPSR (8 words)
 *   Software: R4-R11 (8 words)
 *   Total: 16 words = 64 bytes
 */

void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n"                     /* Disable interrupts */

        "mrs r0, psp\n"                 /* Get PSP */

        "tst r14, #0x10\n"              /* Check EXC_RETURN bit 4 */
        "it eq\n"
        "vstmdbeq r0!, {d16-d31}\n"     /* If FPU used, save D16-D31 (32 words) */

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

        /* Restore R4-R11 */
        "ldmia r0!, {r4-r11}\n"

        "tst r14, #0x10\n"              /* Check FPU state for new task */
        "it eq\n"
        "vldmiaeq r0!, {d16-d31}\n"     /* If FPU used, restore D16-D31 */

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
    extern void os_kernel_tick_increment(void);

#if OS_CONFIG_USE_INTERRUPT_NESTING
    os_port_isr_enter();
#endif

    os_kernel_tick_increment();

#if OS_CONFIG_USE_INTERRUPT_NESTING
    os_port_isr_exit();
#else
    os_port_yield();
#endif
}
