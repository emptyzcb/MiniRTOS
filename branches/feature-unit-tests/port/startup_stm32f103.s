/**
 * startup_stm32f103.s
 *
 * Cortex-M3 startup file for STM32F103.
 * Sets up the initial stack, vector table, and calls main().
 */

    .syntax unified
    .cpu    cortex-m3
    .thumb

/* ========== Global symbols ========== */

.global g_pfnVectors
.global Default_Handler

/* ========== Linker symbols ========== */

    .word _sidata      /* Start of .data initialization values in FLASH */
    .word _sdata       /* Start of .data in RAM */
    .word _edata       /* End of .data in RAM */
    .word _sbss        /* Start of .bss in RAM */
    .word _ebss        /* End of .bss in RAM */

/* ========== Reset Handler ========== */

    .section .text.Reset_Handler
    .weak   Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* Set stack pointer */
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
    /* Cortex-M3 core exceptions */
    .word _estack               /* 0: Initial Stack Pointer */
    .word Reset_Handler         /* 1: Reset */
    .word NMI_Handler           /* 2: NMI */
    .word HardFault_Handler     /* 3: Hard Fault */
    .word MemManage_Handler     /* 4: Memory Management */
    .word BusFault_Handler      /* 5: Bus Fault */
    .word UsageFault_Handler    /* 6: Usage Fault */
    .word 0                     /* 7: Reserved */
    .word 0                     /* 8: Reserved */
    .word 0                     /* 9: Reserved */
    .word 0                     /* 10: Reserved */
    .word SVC_Handler           /* 11: SVCall */
    .word DebugMon_Handler      /* 12: Debug Monitor */
    .word 0                     /* 13: Reserved */
    .word PendSV_Handler        /* 14: PendSV */
    .word SysTick_Handler       /* 15: SysTick */

    /* STM32F103 external interrupts */
    .word WWDG_IRQHandler           /* 16 */
    .word PVD_IRQHandler            /* 17 */
    .word TAMPER_IRQHandler         /* 18 */
    .word RTC_IRQHandler            /* 19 */
    .word FLASH_IRQHandler          /* 20 */
    .word RCC_IRQHandler            /* 21 */
    .word EXTI0_IRQHandler          /* 22 */
    .word EXTI1_IRQHandler          /* 23 */
    .word EXTI2_IRQHandler          /* 24 */
    .word EXTI3_IRQHandler          /* 25 */
    .word EXTI4_IRQHandler          /* 26 */
    .word DMA1_Channel1_IRQHandler  /* 27 */
    .word DMA1_Channel2_IRQHandler  /* 28 */
    .word DMA1_Channel3_IRQHandler  /* 29 */
    .word DMA1_Channel4_IRQHandler  /* 30 */
    .word DMA1_Channel5_IRQHandler  /* 31 */
    .word DMA1_Channel6_IRQHandler  /* 32 */
    .word DMA1_Channel7_IRQHandler  /* 33 */
    .word ADC1_2_IRQHandler         /* 34 */
    .word USB_HP_CAN1_TX_IRQHandler /* 35 */
    .word USB_LP_CAN1_RX0_IRQHandler/* 36 */
    .word CAN1_RX1_IRQHandler       /* 37 */
    .word CAN1_SCE_IRQHandler       /* 38 */
    .word EXTI9_5_IRQHandler        /* 39 */
    .word TIM1_BRK_IRQHandler       /* 40 */
    .word TIM1_UP_IRQHandler        /* 41 */
    .word TIM1_TRG_COM_IRQHandler   /* 42 */
    .word TIM1_CC_IRQHandler        /* 43 */
    .word TIM2_IRQHandler           /* 44 */
    .word TIM3_IRQHandler           /* 45 */
    .word TIM4_IRQHandler           /* 46 */
    .word I2C1_EV_IRQHandler        /* 47 */
    .word I2C1_ER_IRQHandler        /* 48 */
    .word I2C2_EV_IRQHandler        /* 49 */
    .word I2C2_ER_IRQHandler        /* 50 */
    .word SPI1_IRQHandler           /* 51 */
    .word SPI2_IRQHandler           /* 52 */
    .word USART1_IRQHandler         /* 53 */
    .word USART2_IRQHandler         /* 54 */
    .word USART3_IRQHandler         /* 55 */
    .word EXTI15_10_IRQHandler      /* 56 */
    .word RTCAlarm_IRQHandler       /* 57 */
    .word USBWakeUp_IRQHandler      /* 58 */

/* ========== Weak aliases for handlers ========== */

    .weak      NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak      HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak      MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler

    .weak      BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

    .weak      UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler

    .weak      SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak      DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    .weak      PendSV_Handler
    /* PendSV_Handler is defined in port.c */

    .weak      SysTick_Handler
    /* SysTick_Handler is defined in port.c */

    /* External interrupt handlers - all default to Default_Handler */
    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak TAMPER_IRQHandler
    .thumb_set TAMPER_IRQHandler, Default_Handler
    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler
    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler
    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler
    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler
    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler
    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler, Default_Handler
    .weak DMA1_Channel2_IRQHandler
    .thumb_set DMA1_Channel2_IRQHandler, Default_Handler
    .weak DMA1_Channel3_IRQHandler
    .thumb_set DMA1_Channel3_IRQHandler, Default_Handler
    .weak DMA1_Channel4_IRQHandler
    .thumb_set DMA1_Channel4_IRQHandler, Default_Handler
    .weak DMA1_Channel5_IRQHandler
    .thumb_set DMA1_Channel5_IRQHandler, Default_Handler
    .weak DMA1_Channel6_IRQHandler
    .thumb_set DMA1_Channel6_IRQHandler, Default_Handler
    .weak DMA1_Channel7_IRQHandler
    .thumb_set DMA1_Channel7_IRQHandler, Default_Handler
    .weak ADC1_2_IRQHandler
    .thumb_set ADC1_2_IRQHandler, Default_Handler
    .weak USB_HP_CAN1_TX_IRQHandler
    .thumb_set USB_HP_CAN1_TX_IRQHandler, Default_Handler
    .weak USB_LP_CAN1_RX0_IRQHandler
    .thumb_set USB_LP_CAN1_RX0_IRQHandler, Default_Handler
    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler
    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler
    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler
    .weak TIM1_BRK_IRQHandler
    .thumb_set TIM1_BRK_IRQHandler, Default_Handler
    .weak TIM1_UP_IRQHandler
    .thumb_set TIM1_UP_IRQHandler, Default_Handler
    .weak TIM1_TRG_COM_IRQHandler
    .thumb_set TIM1_TRG_COM_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak TIM4_IRQHandler
    .thumb_set TIM4_IRQHandler, Default_Handler
    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler
    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler
    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler
    .weak I2C2_ER_IRQHandler
    .thumb_set I2C2_ER_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler
    .weak EXTI15_10_IRQHandler
    .thumb_set EXTI15_10_IRQHandler, Default_Handler
    .weak RTCAlarm_IRQHandler
    .thumb_set RTCAlarm_IRQHandler, Default_Handler
    .weak USBWakeUp_IRQHandler
    .thumb_set USBWakeUp_IRQHandler, Default_Handler
