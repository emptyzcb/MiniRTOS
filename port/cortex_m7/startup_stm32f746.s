/**
 * startup_stm32f746.s
 *
 * Cortex-M7 startup file for STM32F746NG.
 * Sets up the initial stack, vector table, and calls main().
 */

    .syntax unified
    .cpu    cortex-m7
    .fpu    fpv5-d16
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
    /* Cortex-M7 core exceptions */
    .word _estack                   /* 0:  Initial Stack Pointer */
    .word Reset_Handler             /* 1:  Reset */
    .word NMI_Handler               /* 2:  NMI */
    .word HardFault_Handler         /* 3:  Hard Fault */
    .word MemManage_Handler         /* 4:  Memory Management */
    .word BusFault_Handler          /* 5:  Bus Fault */
    .word UsageFault_Handler        /* 6:  Usage Fault */
    .word 0                         /* 7:  Reserved */
    .word 0                         /* 8:  Reserved */
    .word 0                         /* 9:  Reserved */
    .word 0                         /* 10: Reserved */
    .word SVC_Handler               /* 11: SVCall */
    .word DebugMon_Handler          /* 12: Debug Monitor */
    .word 0                         /* 13: Reserved */
    .word PendSV_Handler            /* 14: PendSV */
    .word SysTick_Handler           /* 15: SysTick */

    /* STM32F746 external interrupts */
    .word WWDG_IRQHandler                   /* 16 */
    .word PVD_IRQHandler                    /* 17 */
    .word TAMP_STAMP_IRQHandler             /* 18 */
    .word RTC_WKUP_IRQHandler               /* 19 */
    .word FLASH_IRQHandler                  /* 20 */
    .word RCC_IRQHandler                    /* 21 */
    .word EXTI0_IRQHandler                  /* 22 */
    .word EXTI1_IRQHandler                  /* 23 */
    .word EXTI2_IRQHandler                  /* 24 */
    .word EXTI3_IRQHandler                  /* 25 */
    .word EXTI4_IRQHandler                  /* 26 */
    .word DMA1_Stream0_IRQHandler           /* 27 */
    .word DMA1_Stream1_IRQHandler           /* 28 */
    .word DMA1_Stream2_IRQHandler           /* 29 */
    .word DMA1_Stream3_IRQHandler           /* 30 */
    .word DMA1_Stream4_IRQHandler           /* 31 */
    .word DMA1_Stream5_IRQHandler           /* 32 */
    .word DMA1_Stream6_IRQHandler           /* 33 */
    .word ADC_IRQHandler                    /* 34 */
    .word CAN1_TX_IRQHandler                /* 35 */
    .word CAN1_RX0_IRQHandler               /* 36 */
    .word CAN1_RX1_IRQHandler               /* 37 */
    .word CAN1_SCE_IRQHandler               /* 38 */
    .word EXTI9_5_IRQHandler                /* 39 */
    .word TIM1_BRK_TIM9_IRQHandler          /* 40 */
    .word TIM1_UP_TIM10_IRQHandler          /* 41 */
    .word TIM1_TRG_COM_TIM11_IRQHandler     /* 42 */
    .word TIM1_CC_IRQHandler                /* 43 */
    .word TIM2_IRQHandler                   /* 44 */
    .word TIM3_IRQHandler                   /* 45 */
    .word TIM4_IRQHandler                   /* 46 */
    .word I2C1_EV_IRQHandler                /* 47 */
    .word I2C1_ER_IRQHandler                /* 48 */
    .word I2C2_EV_IRQHandler                /* 49 */
    .word I2C2_ER_IRQHandler                /* 50 */
    .word SPI1_IRQHandler                   /* 51 */
    .word SPI2_IRQHandler                   /* 52 */
    .word USART1_IRQHandler                 /* 53 */
    .word USART2_IRQHandler                 /* 54 */
    .word USART3_IRQHandler                 /* 55 */
    .word EXTI15_10_IRQHandler              /* 56 */
    .word RTC_Alarm_IRQHandler              /* 57 */
    .word OTG_FS_WKUP_IRQHandler            /* 58 */
    .word TIM8_BRK_TIM12_IRQHandler         /* 59 */
    .word TIM8_UP_TIM13_IRQHandler          /* 60 */
    .word TIM8_TRG_COM_TIM14_IRQHandler     /* 61 */
    .word TIM8_CC_IRQHandler                /* 62 */
    .word DMA1_Stream7_IRQHandler           /* 63 */
    .word FMC_IRQHandler                    /* 64 */
    .word SDMMC1_IRQHandler                 /* 65 */
    .word TIM5_IRQHandler                   /* 66 */
    .word SPI3_IRQHandler                   /* 67 */
    .word UART4_IRQHandler                  /* 68 */
    .word UART5_IRQHandler                  /* 69 */
    .word TIM6_DAC_IRQHandler               /* 70 */
    .word TIM7_IRQHandler                   /* 71 */
    .word DMA2_Stream0_IRQHandler           /* 72 */
    .word DMA2_Stream1_IRQHandler           /* 73 */
    .word DMA2_Stream2_IRQHandler           /* 74 */
    .word DMA2_Stream3_IRQHandler           /* 75 */
    .word DMA2_Stream4_IRQHandler           /* 76 */
    .word ETH_IRQHandler                    /* 77 */
    .word ETH_WKUP_IRQHandler              /* 78 */
    .word CAN2_TX_IRQHandler                /* 79 */
    .word CAN2_RX0_IRQHandler               /* 80 */
    .word CAN2_RX1_IRQHandler               /* 81 */
    .word CAN2_SCE_IRQHandler               /* 82 */
    .word OTG_FS_IRQHandler                 /* 83 */
    .word DMA2_Stream5_IRQHandler           /* 84 */
    .word DMA2_Stream6_IRQHandler           /* 85 */
    .word DMA2_Stream7_IRQHandler           /* 86 */
    .word USART6_IRQHandler                 /* 87 */
    .word I2C3_EV_IRQHandler                /* 88 */
    .word I2C3_ER_IRQHandler                /* 89 */
    .word OTG_HS_EP1_OUT_IRQHandler         /* 90 */
    .word OTG_HS_EP1_IN_IRQHandler          /* 91 */
    .word OTG_HS_WKUP_IRQHandler            /* 92 */
    .word OTG_HS_IRQHandler                 /* 93 */
    .word DCMI_IRQHandler                   /* 94 */
    .word 0                                 /* 95: Reserved */
    .word RNG_IRQHandler                    /* 96 */
    .word FPU_IRQHandler                    /* 97 */
    .word UART7_IRQHandler                  /* 98 */
    .word UART8_IRQHandler                  /* 99 */
    .word SPI4_IRQHandler                   /* 100 */
    .word SPI5_IRQHandler                   /* 101 */
    .word SPI6_IRQHandler                   /* 102 */
    .word SAI1_IRQHandler                   /* 103 */
    .word LTDC_IRQHandler                   /* 104 */
    .word LTDC_ER_IRQHandler                /* 105 */
    .word DMA2D_IRQHandler                  /* 106 */
    .word SAI2_IRQHandler                   /* 107 */
    .word QUADSPI_IRQHandler                /* 108 */
    .word LPTIM1_IRQHandler                 /* 109 */
    .word CEC_IRQHandler                    /* 110 */
    .word I2C4_EV_IRQHandler                /* 111 */
    .word I2C4_ER_IRQHandler                /* 112 */
    .word SPDIF_RX_IRQHandler              /* 113 */
    .word OTG_FS_EP1_OUT_IRQHandler         /* 114 */
    .word OTG_FS_EP1_IN_IRQHandler          /* 115 */
    .word DMAMUX1_OVR_IRQHandler            /* 116 */
    .word DMAMUX2_OVR_IRQHandler            /* 117 */
    .word DFSDM1_FLT0_IRQHandler            /* 118 */
    .word DFSDM1_FLT1_IRQHandler            /* 119 */
    .word DFSDM1_FLT2_IRQHandler            /* 120 */
    .word DFSDM1_FLT3_IRQHandler            /* 121 */
    .word SDMMC2_IRQHandler                 /* 122 */

/* ========== Weak aliases ========== */

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
    /* PendSV_Handler is defined in port_m7.c */

    .weak      SysTick_Handler
    /* SysTick_Handler is defined in port_m7.c */

    /* External interrupt handlers */
    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak TAMP_STAMP_IRQHandler
    .thumb_set TAMP_STAMP_IRQHandler, Default_Handler
    .weak RTC_WKUP_IRQHandler
    .thumb_set RTC_WKUP_IRQHandler, Default_Handler
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
    .weak DMA1_Stream0_IRQHandler
    .thumb_set DMA1_Stream0_IRQHandler, Default_Handler
    .weak DMA1_Stream1_IRQHandler
    .thumb_set DMA1_Stream1_IRQHandler, Default_Handler
    .weak DMA1_Stream2_IRQHandler
    .thumb_set DMA1_Stream2_IRQHandler, Default_Handler
    .weak DMA1_Stream3_IRQHandler
    .thumb_set DMA1_Stream3_IRQHandler, Default_Handler
    .weak DMA1_Stream4_IRQHandler
    .thumb_set DMA1_Stream4_IRQHandler, Default_Handler
    .weak DMA1_Stream5_IRQHandler
    .thumb_set DMA1_Stream5_IRQHandler, Default_Handler
    .weak DMA1_Stream6_IRQHandler
    .thumb_set DMA1_Stream6_IRQHandler, Default_Handler
    .weak ADC_IRQHandler
    .thumb_set ADC_IRQHandler, Default_Handler
    .weak CAN1_TX_IRQHandler
    .thumb_set CAN1_TX_IRQHandler, Default_Handler
    .weak CAN1_RX0_IRQHandler
    .thumb_set CAN1_RX0_IRQHandler, Default_Handler
    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler
    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler
    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler
    .weak TIM1_BRK_TIM9_IRQHandler
    .thumb_set TIM1_BRK_TIM9_IRQHandler, Default_Handler
    .weak TIM1_UP_TIM10_IRQHandler
    .thumb_set TIM1_UP_TIM10_IRQHandler, Default_Handler
    .weak TIM1_TRG_COM_TIM11_IRQHandler
    .thumb_set TIM1_TRG_COM_TIM11_IRQHandler, Default_Handler
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
    .weak RTC_Alarm_IRQHandler
    .thumb_set RTC_Alarm_IRQHandler, Default_Handler
    .weak OTG_FS_WKUP_IRQHandler
    .thumb_set OTG_FS_WKUP_IRQHandler, Default_Handler
    .weak TIM8_BRK_TIM12_IRQHandler
    .thumb_set TIM8_BRK_TIM12_IRQHandler, Default_Handler
    .weak TIM8_UP_TIM13_IRQHandler
    .thumb_set TIM8_UP_TIM13_IRQHandler, Default_Handler
    .weak TIM8_TRG_COM_TIM14_IRQHandler
    .thumb_set TIM8_TRG_COM_TIM14_IRQHandler, Default_Handler
    .weak TIM8_CC_IRQHandler
    .thumb_set TIM8_CC_IRQHandler, Default_Handler
    .weak DMA1_Stream7_IRQHandler
    .thumb_set DMA1_Stream7_IRQHandler, Default_Handler
    .weak FMC_IRQHandler
    .thumb_set FMC_IRQHandler, Default_Handler
    .weak SDMMC1_IRQHandler
    .thumb_set SDMMC1_IRQHandler, Default_Handler
    .weak TIM5_IRQHandler
    .thumb_set TIM5_IRQHandler, Default_Handler
    .weak SPI3_IRQHandler
    .thumb_set SPI3_IRQHandler, Default_Handler
    .weak UART4_IRQHandler
    .thumb_set UART4_IRQHandler, Default_Handler
    .weak UART5_IRQHandler
    .thumb_set UART5_IRQHandler, Default_Handler
    .weak TIM6_DAC_IRQHandler
    .thumb_set TIM6_DAC_IRQHandler, Default_Handler
    .weak TIM7_IRQHandler
    .thumb_set TIM7_IRQHandler, Default_Handler
    .weak DMA2_Stream0_IRQHandler
    .thumb_set DMA2_Stream0_IRQHandler, Default_Handler
    .weak DMA2_Stream1_IRQHandler
    .thumb_set DMA2_Stream1_IRQHandler, Default_Handler
    .weak DMA2_Stream2_IRQHandler
    .thumb_set DMA2_Stream2_IRQHandler, Default_Handler
    .weak DMA2_Stream3_IRQHandler
    .thumb_set DMA2_Stream3_IRQHandler, Default_Handler
    .weak DMA2_Stream4_IRQHandler
    .thumb_set DMA2_Stream4_IRQHandler, Default_Handler
    .weak ETH_IRQHandler
    .thumb_set ETH_IRQHandler, Default_Handler
    .weak ETH_WKUP_IRQHandler
    .thumb_set ETH_WKUP_IRQHandler, Default_Handler
    .weak CAN2_TX_IRQHandler
    .thumb_set CAN2_TX_IRQHandler, Default_Handler
    .weak CAN2_RX0_IRQHandler
    .thumb_set CAN2_RX0_IRQHandler, Default_Handler
    .weak CAN2_RX1_IRQHandler
    .thumb_set CAN2_RX1_IRQHandler, Default_Handler
    .weak CAN2_SCE_IRQHandler
    .thumb_set CAN2_SCE_IRQHandler, Default_Handler
    .weak OTG_FS_IRQHandler
    .thumb_set OTG_FS_IRQHandler, Default_Handler
    .weak DMA2_Stream5_IRQHandler
    .thumb_set DMA2_Stream5_IRQHandler, Default_Handler
    .weak DMA2_Stream6_IRQHandler
    .thumb_set DMA2_Stream6_IRQHandler, Default_Handler
    .weak DMA2_Stream7_IRQHandler
    .thumb_set DMA2_Stream7_IRQHandler, Default_Handler
    .weak USART6_IRQHandler
    .thumb_set USART6_IRQHandler, Default_Handler
    .weak I2C3_EV_IRQHandler
    .thumb_set I2C3_EV_IRQHandler, Default_Handler
    .weak I2C3_ER_IRQHandler
    .thumb_set I2C3_ER_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_OUT_IRQHandler
    .thumb_set OTG_HS_EP1_OUT_IRQHandler, Default_Handler
    .weak OTG_HS_EP1_IN_IRQHandler
    .thumb_set OTG_HS_EP1_IN_IRQHandler, Default_Handler
    .weak OTG_HS_WKUP_IRQHandler
    .thumb_set OTG_HS_WKUP_IRQHandler, Default_Handler
    .weak OTG_HS_IRQHandler
    .thumb_set OTG_HS_IRQHandler, Default_Handler
    .weak DCMI_IRQHandler
    .thumb_set DCMI_IRQHandler, Default_Handler
    .weak RNG_IRQHandler
    .thumb_set RNG_IRQHandler, Default_Handler
    .weak FPU_IRQHandler
    .thumb_set FPU_IRQHandler, Default_Handler
    .weak UART7_IRQHandler
    .thumb_set UART7_IRQHandler, Default_Handler
    .weak UART8_IRQHandler
    .thumb_set UART8_IRQHandler, Default_Handler
    .weak SPI4_IRQHandler
    .thumb_set SPI4_IRQHandler, Default_Handler
    .weak SPI5_IRQHandler
    .thumb_set SPI5_IRQHandler, Default_Handler
    .weak SPI6_IRQHandler
    .thumb_set SPI6_IRQHandler, Default_Handler
    .weak SAI1_IRQHandler
    .thumb_set SAI1_IRQHandler, Default_Handler
    .weak LTDC_IRQHandler
    .thumb_set LTDC_IRQHandler, Default_Handler
    .weak LTDC_ER_IRQHandler
    .thumb_set LTDC_ER_IRQHandler, Default_Handler
    .weak DMA2D_IRQHandler
    .thumb_set DMA2D_IRQHandler, Default_Handler
    .weak SAI2_IRQHandler
    .thumb_set SAI2_IRQHandler, Default_Handler
    .weak QUADSPI_IRQHandler
    .thumb_set QUADSPI_IRQHandler, Default_Handler
    .weak LPTIM1_IRQHandler
    .thumb_set LPTIM1_IRQHandler, Default_Handler
    .weak CEC_IRQHandler
    .thumb_set CEC_IRQHandler, Default_Handler
    .weak I2C4_EV_IRQHandler
    .thumb_set I2C4_EV_IRQHandler, Default_Handler
    .weak I2C4_ER_IRQHandler
    .thumb_set I2C4_ER_IRQHandler, Default_Handler
    .weak SPDIF_RX_IRQHandler
    .thumb_set SPDIF_RX_IRQHandler, Default_Handler
    .weak OTG_FS_EP1_OUT_IRQHandler
    .thumb_set OTG_FS_EP1_OUT_IRQHandler, Default_Handler
    .weak OTG_FS_EP1_IN_IRQHandler
    .thumb_set OTG_FS_EP1_IN_IRQHandler, Default_Handler
    .weak DMAMUX1_OVR_IRQHandler
    .thumb_set DMAMUX1_OVR_IRQHandler, Default_Handler
    .weak DMAMUX2_OVR_IRQHandler
    .thumb_set DMAMUX2_OVR_IRQHandler, Default_Handler
    .weak DFSDM1_FLT0_IRQHandler
    .thumb_set DFSDM1_FLT0_IRQHandler, Default_Handler
    .weak DFSDM1_FLT1_IRQHandler
    .thumb_set DFSDM1_FLT1_IRQHandler, Default_Handler
    .weak DFSDM1_FLT2_IRQHandler
    .thumb_set DFSDM1_FLT2_IRQHandler, Default_Handler
    .weak DFSDM1_FLT3_IRQHandler
    .thumb_set DFSDM1_FLT3_IRQHandler, Default_Handler
    .weak SDMMC2_IRQHandler
    .thumb_set SDMMC2_IRQHandler, Default_Handler
