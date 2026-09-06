/*
 * SoftDevice 지원 상태 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nRF54L 은 애플리케이션이 벡터 테이블을 소유하고 SoftDevice 로 예외를
 * 포워딩한다 (CLAUDE.md §7 F1). 그 포워딩에 필요한 상태를 담는다.
 *
 * SoftDevice 를 켜고 끄는 것은 ble/sd_event_pump.c 가 한다.
 * 이 파일은 **포워딩에 필요한 상태와, SoftDevice 가 꺼져 있을 때의
 * 기본 핸들러**만 담는다 (sd_irq_forward.S 가 참조한다).
 */

#include <stdint.h>

/*
 * SoftDevice 벡터 테이블의 베이스 주소.
 *
 * SoftDevice 가 활성화되면 sd_softdevice_enable() 전에
 * 이 값을 SoftDevice 파티션 시작 주소(0x0015A800, docs/MEMORY-MAP.md)로
 * 채워야 한다. sd_svc_dispatch.S 가 여기서 읽어
 * [base + NRF_SD_ISR_OFFSET_SVC] 로 점프한다.
 *
 * 0 이면 SoftDevice 가 없다는 뜻이다. 이 상태에서 SVC >= 0x10 이
 * 발생하면 널 포인터로 점프해 HardFault 가 난다 — SoftDevice 를 켜지
 * 않고 sd_* API 를 부른 것이므로 그게 맞는 동작이다.
 */
uint32_t softdevice_vector_forward_address = 0;

/*
 * IRQ 포워딩 활성 플래그. sdk-nrf-bm 의
 * irq_forwarding_enabled_magic_number_holder 와 같은 역할이다.
 * M3 에서 ConsumeOrForwardIRQ 계열을 이식할 때 쓴다.
 */
uint32_t irq_forwarding_enabled_magic_number_holder = 0;


/* ─────────────────────────────────────────────────────────────────────
 * SoftDevice 가 꺼져 있을 때의 기본 핸들러
 * ─────────────────────────────────────────────────────────────────────
 * sd_irq_forward.S 의 각 벡터가 "SD 꺼짐" 경로에서 여기로 온다.
 *
 * 이 인터럽트들은 전부 SoftDevice 전용 자원이다 (nrf_sd_def.h). SoftDevice 가
 * 꺼져 있는데 떴다는 것은 앱이 그 페리페럴을 건드렸다는 뜻이므로 버그다.
 *
 * 원본(sdk-nrf-bm)은 여기서 `SVC 255` 를 실행해 폴트를 낸다. 우리는 어느
 * 인터럽트였는지 남기고 멈춘다 — SWD 로 g_sd_unexpected_irq 를 읽으면 바로
 * 알 수 있다 (docs/HIL/M1-nu54dk.md §5 의 계측 방식과 같다).
 *
 * weak 이므로 앱이나 코어가 필요하면 덮어쓸 수 있다.
 */

/** 예상치 못한 SoftDevice IRQ 가 떴을 때 그 식별자가 남는다. 0 이면 없었다. */
volatile uint32_t g_sd_unexpected_irq = 0;

static void sd_unexpected_irq(uint32_t which)
{
    g_sd_unexpected_irq = which;
    for (;;) {
        /* 멈춘다. 조용히 돌아가면 원인을 못 찾는다. */
    }
}

#define SD_DEFAULT_HANDLER(name, id)                    \
    __attribute__((weak)) void name(void)               \
    {                                                   \
        sd_unexpected_irq(id);                          \
    }

SD_DEFAULT_HANDLER(C_CLOCK_POWER_Handler,  1)
SD_DEFAULT_HANDLER(C_RADIO_0_Handler,      2)
SD_DEFAULT_HANDLER(C_TIMER10_Handler,      3)
SD_DEFAULT_HANDLER(C_GRTC_3_Handler,       4)
SD_DEFAULT_HANDLER(C_ECB00_Handler,        5)
SD_DEFAULT_HANDLER(C_AAR00_CCM00_Handler,  6)
SD_DEFAULT_HANDLER(C_SWI00_Handler,        7)
