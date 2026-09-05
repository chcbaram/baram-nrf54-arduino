/*
 * FreeRTOS 틱 소스 — nRF54L GRTC
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 구조 참조:
 *   - Zephyr drivers/timer/nrf_grtc_timer.c (Apache-2.0) — CC 재장전 방식과
 *     safe_setting 판정
 *   - Adafruit_nRF52_Arduino cores/nRF5/freertos/portable/CMSIS/nrf52/
 *     port_cmsis_systick.c (MIT) — 틱 보정 로직의 골격
 *
 * ── 왜 GRTC 인가 (CLAUDE.md §7 F3) ────────────────────────────────────
 * SysTick 은 저전력 모드에서 멈추므로 tickless 에 못 쓴다.
 * nRF52 는 RTC1 을 썼지만 nRF54L 은 GRTC 를 쓴다.
 *
 * GRTC 가 RTC1 보다 유리한 점:
 *   - SYSCOUNTER 가 64비트다. nRF52 의 24비트 래핑 보정 코드가 통째로 없다
 *   - 1 MHz 라 1000 Hz 틱이 정확히 1000 카운트로 떨어진다
 *   - micros() 를 카운터에서 직접 뽑을 수 있다
 *
 * ── 채널 배정 ─────────────────────────────────────────────────────────
 * SoftDevice 가 CC7~11 과 GRTC_3_IRQn 을 점유한다 (nrf_sd_def.h:
 *   SD_GRTC_CC_CHANNELS_USED 0x00000F80, SD_GRTC_IRQn_USED GRTC_3_IRQn).
 * 그래서 nordic/nrfx_config.h 에서 허용 채널을 0x7F(CC0~6)로 못박았고,
 * 여기서는 nrfx 할당기에서 하나를 받아 쓴다.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <errno.h>

#include <nrfx.h>
#include <nrfx_grtc.h>
#include <hal/nrf_grtc.h>

/* 틱 하나가 몇 SYSCOUNTER 카운트인가.
 * configSYSTICK_CLOCK_HZ = 1000000 (GRTC), configTICK_RATE_HZ = 1000
 * → 정확히 1000. 나누어떨어지지 않으면 millis() 에 오차가 쌓인다. */
#define GRTC_CYCLES_PER_TICK ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ )

#if ( configSYSTICK_CLOCK_HZ % configTICK_RATE_HZ ) != 0
    #error "configTICK_RATE_HZ 가 GRTC 주파수를 정확히 나누지 못한다. millis() 에 오차가 쌓인다."
#endif

/* 틱 채널. nrfx 할당기가 CC0~6 중에서 준다. */
static nrfx_grtc_channel_t m_tick_channel = {
    .handler   = NULL,
    .p_context = NULL,
    .channel   = (uint8_t)-1,
};

/* 마지막으로 장전한 CC 절대값. tickless 에서 safe_setting 판정에 쓴다. */
static uint64_t m_last_cc;

/* 부팅 시점의 SYSCOUNTER 값. micros() 를 0 부터 세기 위해 뺀다. */
static uint64_t m_syscounter_base;

/* nrfx 가 SYSCOUNTER 용으로 잡는 "메인" 채널. 우리는 직접 쓰지 않지만
 * NULL 을 넘기지 않도록 받아 둔다 (GRTC_MAIN_CC_CHANNEL = 0). */
static uint8_t m_main_channel;

/* ─────────────────────────────────────────────────────────────────────
 * 틱 핸들러
 * ───────────────────────────────────────────────────────────────────── */
static void grtc_tick_handler(int32_t id, uint64_t cc_value, void * p_context)
{
    (void)id;
    (void)cc_value;
    (void)p_context;

    /* 다음 틱을 직전 CC 기준 상대값으로 재장전한다.
     * SYSCOUNTER 기준이 아니라 CC 기준(RELATIVE_COMPARE)이라 핸들러 진입
     * 지연이 누적되지 않는다. Zephyr nrf_grtc_timer.c 와 같은 방식이다. */
    m_last_cc += GRTC_CYCLES_PER_TICK;
    nrfx_grtc_syscounter_cc_rel_set(m_tick_channel.channel,
                                    GRTC_CYCLES_PER_TICK,
                                    NRFX_GRTC_CC_RELATIVE_COMPARE);

    if (xTaskIncrementTick() != pdFALSE)
    {
        /* 컨텍스트 스위치가 필요하다. PendSV 를 건다. */
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
    }
}

/* ─────────────────────────────────────────────────────────────────────
 * 틱 설정 — 포트의 weak 정의(port.c:847, SysTick 기반)를 대체한다.
 * 패치 불필요. freertos/PATCHES.md §2 참조.
 * ───────────────────────────────────────────────────────────────────── */
void vPortSetupTimerInterrupt(void)
{
    int err;

    /* nrfx_config.h 의 NRFX_GRTC_CONFIG_IRQ_PRIORITY(=6)를 쓰지 않고
     * 커널 우선순위(7, 최저)를 명시한다. 틱 핸들러가 xTaskIncrementTick()
     * 을 부르므로 configMAX_SYSCALL_INTERRUPT_PRIORITY(5) 보다 낮은
     * 긴급도여야 한다 (CLAUDE.md §7 F2). */
    /* nrfx 4.x 는 POSIX errno 규약이다. 0 이 성공, 음수가 오류.
     * (nrfx 3.x 의 NRFX_SUCCESS / nrfx_err_t 가 아니다.) */
    err = nrfx_grtc_init(configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    configASSERT(err == 0 || err == -EALREADY);

    /* SYSCOUNTER 를 먼저 기동한다. 이 함수가 메인 CC 채널을 할당하므로
     * 틱 채널보다 앞서야 채널 번호가 꼬이지 않는다.
     * busy_wait=true 로 준비될 때까지 기다린다. 부팅 경로라 블로킹해도
     * 문제없고, 준비 전에 CC 를 걸면 빗나간다. */
    err = nrfx_grtc_syscounter_start(true, &m_main_channel);
    configASSERT(err == 0 || err == -EALREADY);

    err = nrfx_grtc_channel_alloc(&m_tick_channel.channel);
    configASSERT(err == 0);

    nrfx_grtc_channel_callback_set(m_tick_channel.channel, grtc_tick_handler, NULL);

    /* micros() 의 기준점. 여기부터 0 으로 센다. */
    m_syscounter_base = nrfx_grtc_syscounter_get();

    /* 첫 틱을 현재 SYSCOUNTER 기준으로 건다. */
    m_last_cc = m_syscounter_base + GRTC_CYCLES_PER_TICK;
    nrfx_grtc_syscounter_cc_abs_set(m_tick_channel.channel, m_last_cc, true);
    (void)nrfx_grtc_syscounter_cc_int_enable(m_tick_channel.channel);
}

/* ─────────────────────────────────────────────────────────────────────
 * micros() 용 — 틱이 아니라 SYSCOUNTER 를 직접 읽는다
 * ───────────────────────────────────────────────────────────────────── */
uint64_t nrf54l_syscounter_us(void)
{
    /*
     * SYSCOUNTER 는 64비트 1 MHz 이므로 값이 곧 마이크로초다.
     *
     * 다만 이 카운터는 리셋으로 0 이 되지 않는다. 이전 펌웨어가 돌던
     * 시간까지 누적된 값이 그대로 남아 있어서, 빼주지 않으면 부팅 직후
     * micros() 가 수십억으로 나온다 (실측: 부팅 직후 1.47e9).
     * millis() 는 xTickCount 라 0 에서 시작하므로 둘이 어긋나고,
     * 32비트로 잘리는 micros() 의 랩어라운드 시점도 예측할 수 없게 된다.
     *
     * 틱 초기화 시점을 기준으로 잡아 Arduino 의 기대(부팅 시 0)에 맞춘다.
     */
    return nrfx_grtc_syscounter_get() - m_syscounter_base;
}

/* ─────────────────────────────────────────────────────────────────────
 * tickless idle — 단계 4에서 켠다 (CLAUDE.md §6.1)
 * 포트의 weak vPortSuppressTicksAndSleep(port.c:628)을 대체한다.
 * ───────────────────────────────────────────────────────────────────── */
#if ( configUSE_TICKLESS_IDLE == 1 )

/*
 * ⚠ 크리티컬 섹션에 PRIMASK 를 쓰지 마라 (CLAUDE.md §7 F9).
 *
 * Adafruit 는 sd_nvic_critical_region_enter() 로 감싸는데 그 API 는
 * S145 에 없고, __disable_irq()(PRIMASK)로 대체하면 SoftDevice 의
 * 우선순위 0 zero-latency IRQ(RADIO_0/TIMER10/GRTC_3)까지 막혀
 * 라디오 타이밍이 깨진다.
 *
 * BASEPRI 를 우선순위 1 로 올린다:
 *   - 우선순위 0(SD zero-latency)은 계속 서비스됨 → 라디오 안전
 *   - 1~7 은 마스크됨 → eTaskConfirmSleepModeStatus() 와 WFI 사이의 레이스 차단
 *   - SD 의 우선순위 4 IRQ 는 pending 이 되어 CPU 를 깨우고, 복구 후 처리됨
 */
#define SLEEP_BASEPRI ( 1U << ( 8U - configPRIO_BITS ) )

/* CC 를 뒤로 미룰 때 직전 CC 가 이보다 가까우면 safe 절차를 쓴다.
 * 안 그러면 가짜 COMPARE 이벤트가 떠서 슬립이 즉시 깨진다.
 * Zephyr sys_clock_set_timeout() 의 LATENCY_THR_TICKS 와 같은 취지다. */
#define CC_SAFE_SET_THRESHOLD_CYCLES ( GRTC_CYCLES_PER_TICK * 2 )

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    uint64_t enter_cnt;
    uint64_t wake_cc;
    uint32_t prev_basepri;
    TickType_t completed_ticks;

    /* BASEPRI 로 애플리케이션 인터럽트만 막는다. PRIMASK 아님. */
    prev_basepri = __get_BASEPRI();
    __set_BASEPRI_MAX(SLEEP_BASEPRI);
    __DSB();
    __ISB();

    enter_cnt = nrfx_grtc_syscounter_get();

    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
    {
        __set_BASEPRI(prev_basepri);
        return;
    }

    /* 기상 시점을 절대값으로 장전한다. 64비트라 래핑 걱정이 없다. */
    wake_cc = enter_cnt + (uint64_t)xExpectedIdleTime * GRTC_CYCLES_PER_TICK;

    {
        /* CC 를 미래로 미루는 경우 직전 CC 가 가까우면 safe 절차가 필요하다. */
        bool safe = (m_last_cc > enter_cnt) &&
                    ((m_last_cc - enter_cnt) < CC_SAFE_SET_THRESHOLD_CYCLES);
        nrfx_grtc_syscounter_cc_abs_set(m_tick_channel.channel, wake_cc, safe);
        m_last_cc = wake_cc;
    }

    __DSB();

    /* BASEPRI 로 마스크된 인터럽트도 pending 이 되면 WFI 를 깨운다.
     * Adafruit 처럼 NVIC->ISPR 를 폴링할 필요가 없고, nRF54L15 는 IRQ 가
     * 269번까지 있어 ISPR[0]|ISPR[1] 관용구가 애초에 틀린다. */
    __WFI();

    /* 실제로 얼마나 잤는지 SYSCOUNTER 차로 계산한다. */
    {
        uint64_t exit_cnt = nrfx_grtc_syscounter_get();
        uint64_t elapsed  = exit_cnt - enter_cnt;

        completed_ticks = (TickType_t)(elapsed / GRTC_CYCLES_PER_TICK);
        if (completed_ticks > xExpectedIdleTime)
        {
            completed_ticks = xExpectedIdleTime;
        }
    }

    /* 다음 틱을 다시 정상 주기로 건다. */
    m_last_cc = nrfx_grtc_syscounter_get() + GRTC_CYCLES_PER_TICK;
    nrfx_grtc_syscounter_cc_abs_set(m_tick_channel.channel, m_last_cc, true);

    /* 틱 카운트 보정.
     * 마지막 1틱은 xTaskIncrementTick() 으로 처리해 스케줄링이 걸리게 한다.
     * Nordic 이 vTaskDelay 1ms 스핀 낭비를 고치려 넣은 패턴이다
     * (DevZone 63828). */
    if (completed_ticks > 1)
    {
        vTaskStepTick(completed_ticks - 1);
        if (xTaskIncrementTick() != pdFALSE)
        {
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }
    else if (completed_ticks == 1)
    {
        if (xTaskIncrementTick() != pdFALSE)
        {
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }

    __set_BASEPRI(prev_basepri);
}

#endif /* configUSE_TICKLESS_IDLE == 1 */

/* ─────────────────────────────────────────────────────────────────────
 * GRTC IRQ 벡터
 * ─────────────────────────────────────────────────────────────────────
 * 여기서 포워딩 함수를 만들면 안 된다. nrfx 가 이미 매크로로 연결한다:
 *
 *   nrfx_irqs_nrf54l15_application.h : nrfx_grtc_irq_handler -> GRTC_IRQHandler
 *   nrfx_mdk_fixups.h (NRF_APPLICATION && !NRF_TRUSTZONE_NONSECURE)
 *                                    : GRTC_IRQHandler       -> GRTC_2_IRQHandler
 *
 * 즉 nrfx_grtc.c 가 정의하는 함수의 실제 심볼 이름이 곧 GRTC_2_IRQHandler 라
 * 벡터 테이블에 자동으로 들어간다. 직접 GRTC_2_IRQHandler 를 정의하면
 * 무한 재귀가 된다 (-Winfinite-recursion 으로 잡힌다).
 *
 * 그룹 배정 확인 (nrf54l15_interim.h / nrfx_mdk_fixups.h):
 *   secure-only 애플리케이션 코어 -> GRTC_IRQ_GROUP = 2, GRTC_2_IRQn
 *   SoftDevice 는 GRTC_3_IRQn 사용 (nrf_sd_def.h) -> 겹치지 않는다
 */
