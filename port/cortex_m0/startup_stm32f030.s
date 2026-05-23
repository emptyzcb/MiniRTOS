/**
 * startup_stm32f030.s
 *
 * Cortex-M0 startup file for STM32F030F4P6.
 * Sets up the initial stack, vector table, and calls main().
 */

    .syntax unified
    .cpu    cortex-m0
    .thumb

/* ========== Global symbols ========== */

.global g_pfnVectors
.global Default_Handler

/* ========== Linker symbols ========== */

    .word _sidata
    .word _sdata
    .word _edata
    .word _sbss
    .word _ebss

/* ========== Reset Handler ========== */

    .section .text.Reset_Handler
    .weak   Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    ldr sp, =_estack

    /* Copy .data from FLASH to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b   LoopCopyDataInit

CopyDataInit:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

LoopCopyDataInit:
    adds r4, r0, r3
    cmp r4, r1
    bcc CopyDataInit

    /* Zero fill .bss */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b   LoopFillZerobss

FillZerobss:
    str r3, [r2]
    adds r2, r2, #4

LoopFillZerobss:
    cmp r2, r4
    bcc FillZerobss

    /* Call main() */
    bl  main

    /* If main returns, loop forever */
    b   .

    .size Reset_Handler, .-Reset_Handler

/* ========== Default Handler ========== */

    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
    b   .
    .size Default_Handler, .-Default_Handler

/* ========== Vector Table ========== */

    .section .isr_vector, "a", %progbits
    .type  g_pfnVectors, %object
    .size  g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    /* Cortex-M0 core exceptions */
    .word _estack               /* 0: Initial Stack Pointer */
    .word Reset_Handler         /* 1: Reset */
    .word NMI_Handler           /* 2: NMI */
    .word HardFault_Handler     /* 3: Hard Fault */
    .word 0                     /* 4: Reserved (no MemManage on M0) */
    .word 0                     /* 5: Reserved (no BusFault on M0) */
    .word 0                     /* 6: Reserved (no UsageFault on M0) */
    .word 0                     /* 7: Reserved */
    .word 0                     /* 8: Reserved */
    .word 0                     /* 9: Reserved */
    .word 0                     /* 10: Reserved */
    .word SVC_Handler           /* 11: SVCall */
    .word 0                     /* 12: Reserved (no DebugMon on M0) */
    .word 0                     /* 13: Reserved */
    .word PendSV_Handler        /* 14: PendSV */
    .word SysTick_Handler       /* 15: SysTick */

    /* STM32F030 external interrupts */
    .word WWDG_IRQHandler               /* 16 */
    .word 0                             /* 17: Reserved */
    .word RTC_IRQHandler                /* 18 */
    .word FLASH_IRQHandler              /* 19 */
    .word RCC_IRQHandler                /* 20 */
    .word EXTI0_1_IRQHandler            /* 21 */
    .word EXTI2_3_IRQHandler            /* 22 */
    .word EXTI4_15_IRQHandler           /* 23 */
    .word 0                             /* 24: Reserved */
    .word DMA1_Channel1_IRQHandler      /* 25 */
    .word DMA1_Channel2_3_IRQHandler    /* 26 */
    .word DMA1_Channel4_5_IRQHandler    /* 27 */
    .word ADC1_IRQHandler               /* 28 */
    .word TIM1_BRK_UP_TRG_COM_IRQHandler/* 29 */
    .word TIM1_CC_IRQHandler            /* 30 */
    .word 0                             /* 31: Reserved */
    .word TIM3_IRQHandler               /* 32 */
    .word 0                             /* 33: Reserved */
    .word 0                             /* 34: Reserved */
    .word TIM14_IRQHandler              /* 35 */
    .word 0                             /* 36: Reserved */
    .word TIM16_IRQHandler              /* 37 */
    .word TIM17_IRQHandler              /* 38 */
    .word I2C1_IRQHandler               /* 39 */
    .word 0                             /* 40: Reserved */
    .word SPI1_IRQHandler               /* 41 */
    .word 0                             /* 42: Reserved */
    .word USART1_IRQHandler             /* 43 */

/* ========== Weak aliases ========== */

    .weak      NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak      HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak      SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak      PendSV_Handler
    /* PendSV_Handler is defined in port_m0.c */

    .weak      SysTick_Handler
    /* SysTick_Handler is defined in port_m0.c */

    /* External interrupt handlers */
    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_1_IRQHandler
    .thumb_set EXTI0_1_IRQHandler, Default_Handler
    .weak EXTI2_3_IRQHandler
    .thumb_set EXTI2_3_IRQHandler, Default_Handler
    .weak EXTI4_15_IRQHandler
    .thumb_set EXTI4_15_IRQHandler, Default_Handler
    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler, Default_Handler
    .weak DMA1_Channel2_3_IRQHandler
    .thumb_set DMA1_Channel2_3_IRQHandler, Default_Handler
    .weak DMA1_Channel4_5_IRQHandler
    .thumb_set DMA1_Channel4_5_IRQHandler, Default_Handler
    .weak ADC1_IRQHandler
    .thumb_set ADC1_IRQHandler, Default_Handler
    .weak TIM1_BRK_UP_TRG_COM_IRQHandler
    .thumb_set TIM1_BRK_UP_TRG_COM_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak TIM14_IRQHandler
    .thumb_set TIM14_IRQHandler, Default_Handler
    .weak TIM16_IRQHandler
    .thumb_set TIM16_IRQHandler, Default_Handler
    .weak TIM17_IRQHandler
    .thumb_set TIM17_IRQHandler, Default_Handler
    .weak I2C1_IRQHandler
    .thumb_set I2C1_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
