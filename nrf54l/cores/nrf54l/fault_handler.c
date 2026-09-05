/*
 * 하드폴트 캡처 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * MDK 스타트업의 기본 폴트 핸들러는 `b .` 무한루프뿐이라 원인을 알 수 없다.
 * 여기서 예외 프레임과 폴트 상태 레지스터를 전역에 저장한 뒤 멈춘다.
 *
 * 디버거 없이 SWD 로 g_fault 만 읽으면 된다:
 *   probe-rs read --chip nRF54L15 b32 <&g_fault> 14
 *
 * CLAUDE.md §7 F2 가 경고하는 "원인 추적이 매우 어려운 랜덤 크래시" 대비용이다.
 * 인터럽트 우선순위를 잘못 잡으면 이 경로로 떨어진다.
 */

#include <stdint.h>
#include "nrf.h"

typedef struct {
    /* 예외 진입 시 하드웨어가 쌓은 프레임 */
    uint32_t r0, r1, r2, r3, r12, lr, pc, psr;
    /* 폴트 상태 */
    uint32_t cfsr;        /* Configurable Fault Status */
    uint32_t hfsr;        /* HardFault Status */
    uint32_t bfar;        /* BusFault Address */
    uint32_t mmfar;       /* MemManage Fault Address */
    uint32_t exc_return;  /* EXC_RETURN (어느 스택/모드에서 왔는지) */
    uint32_t magic;       /* 0xFA0175ED = 캡처됨 */
} fault_info_t;

volatile fault_info_t g_fault;

#define FAULT_MAGIC 0xFA0175EDul

__attribute__((used))
void nrf54l_fault_capture(uint32_t *frame, uint32_t exc_return)
{
    g_fault.r0  = frame[0];
    g_fault.r1  = frame[1];
    g_fault.r2  = frame[2];
    g_fault.r3  = frame[3];
    g_fault.r12 = frame[4];
    g_fault.lr  = frame[5];
    g_fault.pc  = frame[6];
    g_fault.psr = frame[7];

    g_fault.cfsr       = SCB->CFSR;
    g_fault.hfsr       = SCB->HFSR;
    g_fault.bfar       = SCB->BFAR;
    g_fault.mmfar      = SCB->MMFAR;
    g_fault.exc_return = exc_return;
    g_fault.magic      = FAULT_MAGIC;

    __DSB();
    while (1) { __asm__ volatile("nop"); }
}

/* EXC_RETURN bit2 로 MSP/PSP 를 고른 뒤 C 쪽으로 넘긴다. */
#define FAULT_ENTRY(name)                          \
    __attribute__((naked)) void name(void)         \
    {                                              \
        __asm__ volatile (                         \
            "tst   lr, #4                    \n"   \
            "ite   eq                        \n"   \
            "mrseq r0, msp                   \n"   \
            "mrsne r0, psp                   \n"   \
            "mov   r1, lr                    \n"   \
            "b     nrf54l_fault_capture      \n"   \
        );                                         \
    }

FAULT_ENTRY(HardFault_Handler)
FAULT_ENTRY(MemoryManagement_Handler)
FAULT_ENTRY(BusFault_Handler)
FAULT_ENTRY(UsageFault_Handler)
