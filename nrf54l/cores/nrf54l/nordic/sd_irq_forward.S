/*
 * SoftDevice IRQ 포워딩 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * sdk-nrf-bm v2.0.1 subsys/softdevice_handler/irq_forward.s
 * (LicenseRef-Nordic-5-Clause, Copyright (c) 2024 Nordic Semiconductor ASA)
 * 의 구조를 Arduino 코어에 맞게 옮긴 것이다.
 *
 * ── 무엇을 하는가 ─────────────────────────────────────────────────────
 * nRF54L 은 애플리케이션이 벡터 테이블을 소유한다 (CLAUDE.md §7 F1).
 * SoftDevice 가 쓰는 인터럽트도 일단 앱 벡터로 들어오므로, SoftDevice 가
 * 켜져 있으면 SoftDevice 벡터 테이블로 넘겨줘야 한다.
 *
 *   SD 켜짐 (매직 일치) -> softdevice_vector_forward_address + 오프셋 으로 점프
 *   SD 꺼짐             -> 앱의 C_xxx_Handler 로 (기본 구현은 sd_support.c)
 *
 * ── 원본과 다른 점 ───────────────────────────────────────────────────
 * 1. **SVC_Handler 가 여기 없다.** 번호로 갈라야 하므로 sd_svc_dispatch.S 에 있다.
 *    원본은 SVC 를 전부 SoftDevice 로 넘기는데, 그러면 FreeRTOS 가 죽는다 (§7 F1).
 *
 * 2. **HardFault 를 포워딩하지 않는다.** 원본은 SD 로 넘기지만, 우리는
 *    fault_handler.c 의 기록기(g_fault)를 살린다. 개발 중에는 폴트가 났을 때
 *    스택 프레임과 CFSR 을 남기는 쪽이 훨씬 쓸모 있기 때문이다 (§7 참조,
 *    docs/HIL/M1-nu54dk.md §5). SoftDevice 내부 폴트 처리가 필요해지면
 *    이 결정을 되돌려야 한다 — 그때는 여기에 HardFault_Handler 를 추가하고
 *    fault_handler.c 의 FAULT_ENTRY(HardFault_Handler) 를 C_HardFault_Handler
 *    로 바꾸면 된다.
 *
 * ── 주의 ─────────────────────────────────────────────────────────────
 * 벡터 이름은 MDK 스타트업(gcc_startup_nrf54l15_application.S)의 weak 심볼과
 * **정확히 같아야** 벡터에 실린다. 링크 후 확인:
 *     arm-none-eabi-nm <elf> | grep RADIO_0_IRQHandler   ->  T 여야 한다
 * core.a 아카이브 경계 때문에 --whole-archive 가 필요하다 (§7 F13 ①).
 */

    .syntax unified
    .arch armv8-m.main
    .thumb

    .extern softdevice_vector_forward_address
    .extern irq_forwarding_enabled_magic_number_holder

/* irq_connect.h : IRQ_FORWARDING_ENABLED_MAGIC_NUMBER */
    .equ IRQ_FWD_MAGIC,                 0x47F34BC1

/* nrf_sd_isr.h */
    .equ NRF_SD_ISR_OFFSET_RESET,       0x0000
    .equ NRF_SD_ISR_OFFSET_SWI00,       0x000c
    .equ NRF_SD_ISR_OFFSET_AAR00_CCM00, 0x0010
    .equ NRF_SD_ISR_OFFSET_ECB00,       0x0014
    .equ NRF_SD_ISR_OFFSET_TIMER10,     0x0018
    .equ NRF_SD_ISR_OFFSET_RADIO_0,     0x001c
    .equ NRF_SD_ISR_OFFSET_GRTC_3,      0x0020
    .equ NRF_SD_ISR_OFFSET_CLOCK_POWER, 0x0024

    .section .text.SD_IRQ_FORWARD, "ax"

/*
 * SoftDevice 가 켜져 있으면 넘기고, 아니면 앱 핸들러로 간다.
 *   R0 = SoftDevice 벡터 테이블 오프셋
 *   R3 = SoftDevice 가 꺼져 있을 때 점프할 앱 핸들러 주소
 */
    .thumb_func
    .align 2
    .globl  ConsumeOrForwardIRQ
    .type   ConsumeOrForwardIRQ, %function
ConsumeOrForwardIRQ:
    ldr     r2, =irq_forwarding_enabled_magic_number_holder
    ldr     r2, [r2]
    ldr     r1, =IRQ_FWD_MAGIC
    cmp     r2, r1
    beq     ForwardIRQ_ToSoftDevice
    bx      r3                      /* SD 꺼짐: 앱이 처리 */

    .thumb_func
    .align 2
    .globl  ForwardIRQ_ToSoftDevice
    .type   ForwardIRQ_ToSoftDevice, %function
ForwardIRQ_ToSoftDevice:
    ldr     r1, =softdevice_vector_forward_address
    ldr     r1, [r1]
    ldr     r1, [r1, r0]            /* 벡터 값 (bit0 = Thumb 마커 포함) */
    bx      r1

/*
 * SoftDevice 의 Reset Handler 를 부른다.
 * 포워딩 매직을 세우기 **전에** 한 번 불러야 한다 (원본 sd_enable_irq_forwarding).
 */
    .thumb_func
    .align 2
    .globl  CallSoftDeviceResetHandler
    .type   CallSoftDeviceResetHandler, %function
CallSoftDeviceResetHandler:
    ldr     r0, =NRF_SD_ISR_OFFSET_RESET
    ldr     r1, =softdevice_vector_forward_address
    ldr     r1, [r1]
    ldr     r1, [r1, r0]
    push    {lr}
    blx     r1
    pop     {lr}
    bx      lr

/* 벡터 하나를 만드는 매크로. 이름은 MDK 벡터 테이블과 같아야 한다. */
    .macro SD_FWD_VECTOR vector, cfunc, offset
    .thumb_func
    .align 2
    .globl  \vector
    .type   \vector, %function
\vector:
    ldr     r3, =\cfunc
    ldr     r0, =\offset
    ldr     r1, =ConsumeOrForwardIRQ
    bx      r1
    .size   \vector, . - \vector
    .endm

SD_FWD_VECTOR CLOCK_POWER_IRQHandler, C_CLOCK_POWER_Handler,  NRF_SD_ISR_OFFSET_CLOCK_POWER
SD_FWD_VECTOR RADIO_0_IRQHandler,     C_RADIO_0_Handler,      NRF_SD_ISR_OFFSET_RADIO_0
SD_FWD_VECTOR TIMER10_IRQHandler,     C_TIMER10_Handler,      NRF_SD_ISR_OFFSET_TIMER10
SD_FWD_VECTOR GRTC_3_IRQHandler,      C_GRTC_3_Handler,       NRF_SD_ISR_OFFSET_GRTC_3
SD_FWD_VECTOR ECB00_IRQHandler,       C_ECB00_Handler,        NRF_SD_ISR_OFFSET_ECB00
SD_FWD_VECTOR AAR00_CCM00_IRQHandler, C_AAR00_CCM00_Handler,  NRF_SD_ISR_OFFSET_AAR00_CCM00
SD_FWD_VECTOR SWI00_IRQHandler,       C_SWI00_Handler,        NRF_SD_ISR_OFFSET_SWI00

    .end
