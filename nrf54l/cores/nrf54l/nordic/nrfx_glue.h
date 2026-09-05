/*
 * nrfx 글루 레이어 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nrfx/templates/nrfx_glue.h (BSD-3-Clause, Nordic Semiconductor ASA)의
 * 인터페이스를 구현한 것. 템플릿 자체는 주석뿐인 스텁이다.
 */
#ifndef NRFX_GLUE_H__
#define NRFX_GLUE_H__

#include <soc/nrfx_irqs.h>
#include <lib/nrfx_coredep.h>
#include <lib/nrfx_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── assert ─────────────────────────────────────────────────────────── */
void nrf54l_assert_failed(const char *file, int line);

#ifdef NDEBUG
#define NRFX_ASSERT(expression)  ((void)(expression))
#else
#define NRFX_ASSERT(expression)                                  \
    do {                                                         \
        if (!(expression)) { nrf54l_assert_failed(__FILE__, __LINE__); } \
    } while (0)
#endif

#define NRFX_STATIC_ASSERT(expression) _Static_assert(expression, "assert")

/* ── IRQ ────────────────────────────────────────────────────────────── */
#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) \
    NVIC_SetPriority((IRQn_Type)(irq_number), (uint32_t)(priority))
#define NRFX_IRQ_ENABLE(irq_number)        NVIC_EnableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_ENABLED(irq_number)    (0 != NVIC_GetEnableIRQ((IRQn_Type)(irq_number)))
#define NRFX_IRQ_DISABLE(irq_number)       NVIC_DisableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_SET(irq_number)   NVIC_SetPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_CLEAR(irq_number) NVIC_ClearPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_PENDING(irq_number)    (0 != NVIC_GetPendingIRQ((IRQn_Type)(irq_number)))

/*
 * ── 크리티컬 섹션 ────────────────────────────────────────────────────
 *
 * PRIMASK(__disable_irq)를 쓰지 않는다. CLAUDE.md §7 F9와 같은 이유다:
 * SoftDevice의 zero-latency IRQ(RADIO_0 / TIMER10 / GRTC_3)가 우선순위 0으로
 * 동작하는데, PRIMASK로 막으면 라디오 타이밍이 깨진다.
 *
 * BASEPRI를 FreeRTOS의 configMAX_SYSCALL_INTERRUPT_PRIORITY(=5)와 같은
 * 값으로 올린다. 우선순위 0~4(SoftDevice 전용)는 계속 서비스되고,
 * 5~7(애플리케이션·nrfx 드라이버)만 마스크된다.
 *
 * 근거: sdk-nrf-bm subsys/softdevice_handler/irq_connect.c
 *       (zero-latency = PRIO 0, SD non-time-critical = PRIO 4)
 *       __NVIC_PRIO_BITS = 3  (mdk .../nrf54l15_application.h)
 *
 * 스케줄러 시작 전에도 불리므로 FreeRTOS API에 의존하지 않는다.
 */
#define NRFX_GLUE_BASEPRI_MASK_SYSCALL  (5U << (8U - 3U))   /* 5 << 5 = 0xA0 */

extern uint32_t nrfx_glue_cs_nesting;
extern uint32_t nrfx_glue_cs_saved_basepri;

#define NRFX_CRITICAL_SECTION_ENTER()                                    \
    do {                                                                 \
        uint32_t _prev = __get_BASEPRI();                                \
        __set_BASEPRI_MAX(NRFX_GLUE_BASEPRI_MASK_SYSCALL);               \
        __DSB(); __ISB();                                                \
        if (nrfx_glue_cs_nesting++ == 0U) {                              \
            nrfx_glue_cs_saved_basepri = _prev;                          \
        }                                                                \
    } while (0)

#define NRFX_CRITICAL_SECTION_EXIT()                                     \
    do {                                                                 \
        if (--nrfx_glue_cs_nesting == 0U) {                              \
            __set_BASEPRI(nrfx_glue_cs_saved_basepri);                   \
        }                                                                \
    } while (0)

/* ── 지연 ───────────────────────────────────────────────────────────── */
#define NRFX_COREDEP_DELAY_DWT_BASED 0
#define NRFX_DELAY_US(us_time)       nrfx_coredep_delay_us(us_time)

/* ── 원자 연산 ──────────────────────────────────────────────────────── */
/** 32비트 원자 타입. nrfx가 glue에서 정의해 주기를 기대한다. */
#define nrfx_atomic_t nrfx_atomic_u32_t

#define NRFX_ATOMIC_FETCH_STORE(p_data, value) nrfx_atomic_u32_fetch_store(p_data, value)
#define NRFX_ATOMIC_FETCH_OR(p_data, value)    nrfx_atomic_u32_fetch_or(p_data, value)
#define NRFX_ATOMIC_FETCH_AND(p_data, value)   nrfx_atomic_u32_fetch_and(p_data, value)
#define NRFX_ATOMIC_FETCH_XOR(p_data, value)   nrfx_atomic_u32_fetch_xor(p_data, value)
#define NRFX_ATOMIC_FETCH_ADD(p_data, value)   nrfx_atomic_u32_fetch_add(p_data, value)
#define NRFX_ATOMIC_FETCH_SUB(p_data, value)   nrfx_atomic_u32_fetch_sub(p_data, value)
#define NRFX_ATOMIC_CAS(p_data, old_value, new_value) \
    nrfx_atomic_u32_cmp_exch(p_data, &(old_value), new_value)

#define NRFX_CLZ(value) __CLZ(value)
#define NRFX_CTZ(value) __CLZ(__RBIT(value))

#define NRFX_EVENT_READBACK_ENABLED 1

/*
 * ── SoftDevice 예약 리소스 (nrf_sd_def.h) ────────────────────────────
 * SD가 쓰는 것을 nrfx 할당기에서 빼둔다. 겹치면 라디오가 깨진다.
 */
#define NRFX_DPPI00_CHANNELS_USED  0x0000000AUL   /* SD_DPPIC00_CHANNELS_USED */
#define NRFX_DPPI10_CHANNELS_USED  0x00000FFFUL   /* SD_DPPIC10_CHANNELS_USED */
#define NRFX_DPPI20_CHANNELS_USED  0x00000001UL   /* SD_DPPIC20_CHANNELS_USED */
#define NRFX_DPPI_CHANNELS_USED    0x00000000UL
#define NRFX_DPPI_GROUPS_USED      0x00000000UL
#define NRFX_PPI_CHANNELS_USED     0x00000000UL
#define NRFX_PPI_GROUPS_USED       0x00000000UL
#define NRFX_GPIOTE_CHANNELS_USED  0x00000000UL
#define NRFX_EGUS_USED             0x00000000UL
/* SD_TIMER1X/2X_INSTANCES_USED = TIMER10, TIMER20 */
#define NRFX_TIMERS_USED           0x00000000UL

#ifdef __cplusplus
}
#endif

#endif /* NRFX_GLUE_H__ */
