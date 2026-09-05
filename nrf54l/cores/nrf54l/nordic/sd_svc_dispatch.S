/*
 * SVC 디스패처 — SoftDevice 와 FreeRTOS 공존
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * sdk-nrf-bm v2.0.1 subsys/softdevice_handler/irq_forward.s
 * (LicenseRef-Nordic-5-Clause, Copyright (c) 2024 Nordic Semiconductor ASA)
 * 의 포워딩 방식을 참고했으나, SVC 번호 분기를 추가한 별도 구현이다.
 *
 * ── 왜 필요한가 (CLAUDE.md §7 F1) ─────────────────────────────────────
 *
 * nRF54L 은 nRF52 와 구조가 반대다. 애플리케이션이 벡터 테이블을 소유하고,
 * SoftDevice 가 필요한 예외를 애플리케이션이 "포워딩"해 준다.
 *
 * 그런데 sdk-nrf-bm 의 SVC_Handler 는 SVC 번호를 보지 않고 전부
 * SoftDevice 로 넘긴다. 그걸 그대로 쓰면 FreeRTOS 의
 * vPortSVCHandler(SVC #0, 첫 태스크 기동)가 SoftDevice 로 흘러가 죽는다.
 *
 * 번호 규칙 (s145_API/include/nrf_svc.h 원문):
 *   "The SVCs with SVC numbers 0x00-0x0F are forwarded to the application.
 *    All other SVCs are handled by the SoftDevice."
 *   SDM_SVC_BASE = 0x10 (nrf_sdm.h), SOC_SVC_BASE = 0x20 (nrf_soc.h)
 *
 * FreeRTOS ARM_CM33_NTZ 포트는 portSVC_START_SCHEDULER = 0 만 쓴다.
 * 따라서 번호 충돌은 없고, 여기서 갈라주기만 하면 된다.
 *
 *   SVC # < 0x10  -> vPortSVCHandler_C      (FreeRTOS / 애플리케이션)
 *   SVC # >= 0x10 -> SoftDevice 로 포워딩
 *
 * FreeRTOS 포트(portasm.c)도 SVC_Handler 를 정의하므로 심볼이 충돌한다.
 * FreeRTOSConfig.h 에서 configOVERRIDE_SVC_HANDLER 를 정의해 그쪽을 끈다.
 * 포트의 SVC_Handler 가 하던 일(r0 = 스택 프레임 포인터를 넘기고
 * vPortSVCHandler_C 로 분기)을 여기서 그대로 한다.
 *
 * ── SVC 번호를 읽는 법 ────────────────────────────────────────────────
 * SVC 명령은 즉치값을 명령어 안에 담는다. 예외 진입 시 스택된 PC 는
 * SVC 명령 "다음" 주소를 가리키므로, PC-2 위치의 하프워드 하위 바이트가
 * SVC 번호다. 어느 스택에 쌓였는지는 EXC_RETURN(LR) 의 bit2 로 판별한다.
 * (0 = MSP, 1 = PSP)
 */

    .syntax unified
    .arch armv8-m.main
    .thumb

    .extern vPortSVCHandler_C
    .extern softdevice_vector_forward_address

/* nrf_sd_isr.h : NRF_SD_ISR_OFFSET_SVC */
    .equ NRF_SD_ISR_OFFSET_SVC, 0x0008
/* nrf_sdm.h : SDM_SVC_BASE */
    .equ SD_SVC_BASE,           0x10

    .section .text.SVC_Handler, "ax"
    .thumb_func
    .align 2
    .globl  SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    /* 예외 진입 시 스택된 프레임의 위치를 고른다. EXC_RETURN bit2: 0=MSP, 1=PSP */
    tst     lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp

    /* 스택 프레임 오프셋 24 = 스택된 PC. 그 직전 하프워드가 SVC 명령. */
    ldr     r1, [r0, #24]
    ldrb    r1, [r1, #-2]           /* SVC 즉치값 */

    cmp     r1, #SD_SVC_BASE
    bhs     .Lforward_to_softdevice

    /* 0x00~0x0F : 애플리케이션 몫. FreeRTOS 로 넘긴다.
     * vPortSVCHandler_C(uint32_t *pulCallerStackAddress) 규약대로
     * r0 에 스택 프레임 포인터가 이미 들어 있다. */
    ldr     r2, =vPortSVCHandler_C
    bx      r2

.Lforward_to_softdevice:
    /* SoftDevice 벡터 테이블은 SD 베이스 주소에 있고,
     * softdevice_vector_forward_address 가 그 베이스를 담고 있다.
     * 베이스 + NRF_SD_ISR_OFFSET_SVC 에 SVC 핸들러 주소가 들어 있다. */
    ldr     r2, =softdevice_vector_forward_address
    ldr     r2, [r2]
    ldr     r2, [r2, #NRF_SD_ISR_OFFSET_SVC]
    bx      r2

    .size   SVC_Handler, . - SVC_Handler
    .end
