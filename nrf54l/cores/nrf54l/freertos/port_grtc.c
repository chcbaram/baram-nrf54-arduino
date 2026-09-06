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
#include <hal/nrf_clock.h>
#include <hal/nrf_oscillators.h>

#include "variant.h"   /* USE_LFXO / USE_LFRC */

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
/*
 * ─────────────────────────────────────────────────────────────────────
 * LFCLK 소스 설정
 * ─────────────────────────────────────────────────────────────────────
 *
 * ⚠ 이걸 빠뜨리면 조용히 부정확해진다. 실측으로 잡은 문제다.
 *
 * 리셋 직후 GRTC 의 CLKCFG.CLKSEL 은 SystemLFCLK(=1) 이고, 시스템 LFCLK 는
 * 아무 설정도 하지 않으면 내부 RC 로 돈다. 10분 연속 시험에서 타깃 시계가
 * 호스트보다 **약 0.9% (9000 ppm) 빨랐다.** 크래시도 없고 millis/micros 가
 * 서로 완벽히 일치해서 로그만 봐서는 정상으로 보인다.
 *
 * BLE 는 더 심각하다. 연결 유지에 보통 ±250 ppm 이하가 요구되므로
 * RC 로는 M3 에서 연결이 끊긴다.
 *
 * 소스를 LFXO 로 바꾸는 것만으로는 부족하다. **로드 커패시터도 맞춰야 한다.**
 * 크리스털에 외부 캡이 달린 보드가 있고 칩 내부 캡에 의존하는 보드가 있는데,
 * 이건 칩이 아니라 보드의 성질이라 variant 가 정한다 (LFXO_INTCAP_VAL).
 * 틀리면 소스는 LFXO 인데도 수백 ppm 이 어긋난다 — 아래 참조.
 */
/*
 * LFXO 내부 로드 커패시터 값 계산.
 *
 * variant 가 LFXO_LOAD_CAP_FF (femtofarad) 를 정의하면 내부 캡을 쓰고,
 * 정의하지 않으면 외부 캡이 실장된 보드로 보고 INTCAP=0 을 쓴다.
 *
 * ⚠ 상수를 박아 두면 안 된다. 계산에 FICR.XOSC32KTRIM 이 들어가는데 이 트림은
 *   **칩 개체마다 다르다.** 같은 보드라도 다른 개체에서 값이 달라진다.
 */
#if defined(USE_LFXO) && defined(LFXO_LOAD_CAP_FF)

/* nRF54L15 PS 가 규정하는 범위 (4~18 pF, 0.5 pF 단위). */
#if (LFXO_LOAD_CAP_FF < 3000) || (LFXO_LOAD_CAP_FF > 18000)
  #error "LFXO_LOAD_CAP_FF 는 3000~18000 (fF) 이어야 한다"
#endif

static uint32_t lfxo_intcap_calc(void)
{
    uint32_t trim   = NRF_FICR->XOSC32KTRIM;
    uint32_t sfield = (trim & FICR_XOSC32KTRIM_SLOPE_Msk) >> FICR_XOSC32KTRIM_SLOPE_Pos;
    uint32_t smask  = FICR_XOSC32KTRIM_SLOPE_Msk >> FICR_XOSC32KTRIM_SLOPE_Pos;
    uint32_t ssign  = smask - (smask >> 1);
    /* SLOPE 는 2의 보수라 부호 확장이 필요하다. */
    int32_t  slope  = (int32_t)(sfield ^ ssign) - (int32_t)ssign;
    uint32_t offset = (trim & FICR_XOSC32KTRIM_OFFSET_Msk) >> FICR_XOSC32KTRIM_OFFSET_Pos;

    /*
     * nRF54L15 PS:
     *   CAPVALUE = round( (2*C_pF - 12) * (SLOPE + 0.765625 * 2^9) / 2^9
     *                     + OFFSET / 2^6 )
     * 부동소수를 피하려고 fF 단위로 받아 2^9 배 스케일에서 정수로 계산한다
     * (0.765625 * 2^9 = 392). Zephyr soc/nordic/nrf54l/soc.c 와 같은 식이다.
     */
    uint32_t mid = (2UL * (uint32_t)LFXO_LOAD_CAP_FF - 12000UL)
                 * (uint32_t)(slope + 392L)
                 + (offset << 3UL) * 1000UL;

    uint32_t cap = mid / 512000UL;
    if ((mid % 512000UL) >= 256000UL) {
        cap++;                          /* 소수부 반올림 */
    }
    return cap;
}
#endif /* USE_LFXO && LFXO_LOAD_CAP_FF */

static void lfclk_start(void)
{
#if defined(USE_LFXO)
    /*
     * LFXO 내부 로드 커패시터. **보드마다 다르므로 variant 가 정한다.**
     *
     *   LFXO_LOAD_CAP_FF 미정의  외부 캡이 실장된 보드 → INTCAP = 0
     *                              (NU54-DK: C13/C14 13 pF)
     *   LFXO_LOAD_CAP_FF 정의      내부 캡을 쓰는 보드. 그 용량(fF)에서
     *                              FICR 트림으로 INTCAP 을 계산한다
     *                              (XIAO nRF54L15: 16000 fF)
     *
     * 여기서 EXTERNAL(0)을 하드코딩하고 있었는데, 그건 NU54-DK 의 보드 사실이지
     * 칩의 성질이 아니다. 외부 캡이 없는 보드(XIAO nRF54L15)에서 0 을 써 넣으면
     * 부하용량이 모자라 발진이 빨라진다 — **실측 +805 ppm.** BLE 요구치
     * ±250 ppm 을 넘으므로 M3 에서 연결이 끊긴다 (§7 F12 와 같은 계열의 함정).
     *
     * ⚠ nrfx 의 NRF_OSCILLATORS_LFXO_CAP_CALCULATE 는 쓰지 않는다.
     *   `((SLOPE + 392) >> 9) * (cap*2-12)` 라서 SLOPE 가 작으면 앞항이 0 으로
     *   잘리고, cap_val 을 무엇으로 주든 같은 값이 나온다 (실측: SLOPE=21,
     *   OFFSET=317 인 칩에서 6/7/9/11 pF 전부 4). 값은 실측으로 정한다.
     */
#if defined(LFXO_LOAD_CAP_FF)
    nrf_oscillators_lfxo_cap_set(NRF_OSCILLATORS,
                                 (nrf_oscillators_lfxo_cap_t) lfxo_intcap_calc());
#else
    nrf_oscillators_lfxo_cap_set(NRF_OSCILLATORS, (nrf_oscillators_lfxo_cap_t) 0);
#endif
    nrf_clock_lf_src_set(NRF_CLOCK, NRF_CLOCK_LFCLK_XTAL);
#elif defined(USE_LFRC)
    nrf_clock_lf_src_set(NRF_CLOCK, NRF_CLOCK_LFCLK_RC);
#endif

    nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_LFCLKSTART);

    /* 안정될 때까지 기다린다. 부팅 경로라 블로킹해도 된다.
     * LFXO 는 기동에 보통 수백 ms 가 걸린다. */
    while (!nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_LFCLK, NULL))
    {
        /* wait */
    }
}

void vPortSetupTimerInterrupt(void)
{
    int err;

    /* GRTC 를 만지기 전에 LFCLK 부터 세운다. */
    lfclk_start();

    /*
     * GRTC 가 쓸 클럭을 명시한다.
     * CLKSEL 기본값은 SystemLFCLK 이고 위에서 그걸 LFXO 로 맞췄지만,
     * LFXO 를 직접 지정해 두면 의도가 코드에 남고 시스템 LFCLK 설정이
     * 나중에 바뀌어도 틱은 영향받지 않는다.
     */
#if defined(USE_LFXO)
    nrf_grtc_clksel_set(NRF_GRTC, NRF_GRTC_CLKSEL_LFXO);
#else
    nrf_grtc_clksel_set(NRF_GRTC, NRF_GRTC_CLKSEL_LFCLK);
#endif

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

    /*
     * SYSCOUNTER 가 슬립 중에도 돌게 요청해 둔다.
     *
     * nrfx_grtc_init() 은 NRFX_GRTC_CONFIG_AUTOEN 이 꺼져 있으면
     * MODE.AUTOEN = 0 으로 두므로(실측 MODE = 0x02) 어차피 자동 슬립은
     * 하지 않는다. 이 호출은 그 위에 도메인별 SYSCOUNTER[n].ACTIVE 요청을
     * 얹는 것이라 지금 구성에서는 사실상 중복이다.
     *
     * ⚠ 예전 주석에 "이걸 빼면 틱이 절반 속도로 돌았다"고 적혀 있었는데
     *   그 측정은 §7 F9 (BASEPRI 로 WFI 가 안 깨던 버그) 때문에 오염된
     *   값이었다. F9 를 고친 뒤로는 이 호출 없이도 틱이 정확하다.
     *   다만 전력 측정을 아직 안 했으므로 지금은 남겨 둔다.
     *   전류 측정 단계(§4.6)에서 빼 보고 차이를 확인한 뒤 결정한다.
     *
     * 실측 확인: SYSCOUNTER 는 슬립 중에도 실시간을 따라간다
     * (SWD 로 6 초 간격 두 번 읽어 100.3%, 오차는 호스트 지연).
     */
#if ( configUSE_TICKLESS_IDLE == 1 )
    nrfx_grtc_active_request_set(true);
#endif

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
 * ⚠⚠ WFI 를 BASEPRI 로 마스킹한 채 실행하면 안 된다. 실기에서 잡은 문제다.
 *
 * ARM 사양의 WFI 기상 조건은 PRIMASK 는 무시하지만 **BASEPRI 와 인터럽트
 * 인에이블은 무시하지 않는다** (ARMv7-M ARM B1.5.19 / ARMv8-M 동일):
 *
 *   "the assertion of an asynchronous exception that has sufficient priority
 *    to cause exception entry when the value of PRIMASK is 0. This means the
 *    value of PRIMASK does not affect whether an asynchronous exception is a
 *    WFI wake-up event, but the values of FAULTMASK, BASEPRI, and the
 *    exception enables do affect this."
 *
 * 즉 BASEPRI 로 가린 인터럽트는 **CPU 를 깨우지 못한다.** 틱 CC 는 커널
 * 우선순위 7 이라 BASEPRI=0x20 에 가려지고, 그러면 슬립에서 영영 안 깬다.
 * FreeRTOS 의 표준 ARM_CM 포트가 이 함수에서만 `cpsid i`(PRIMASK)를 쓰는
 * 이유가 바로 이것이다.
 *
 *   실측 증상: 틱이 25~250 tick/s 로 떨어지고 Serial 이 죽는다. 폴트도
 *   assert 도 없다. 계측해 보면 WFI 한 번이 수 초씩 지속되고
 *   (max_elapsed 8.1 s, xExpectedIdleTime 은 500 ms), CC 기상 횟수가 거의 0 이다.
 *   실제로 깨우는 것은 디버거 attach 뿐이다.
 *   격리 시험으로 함수 본문을 __WFI() 하나로 줄이면 정확히 1000 tick/s 로 돈다.
 *
 * ── 그래서 두 마스크를 나눠 쓴다 ──────────────────────────────────────
 *
 * BASEPRI(우선순위 1)  : eTaskConfirmSleepModeStatus() ~ CC 장전 구간과
 *                        기상 후 틱 보정 구간. 애플리케이션 IRQ 만 막고
 *                        SoftDevice 의 우선순위 0 zero-latency IRQ
 *                        (RADIO_0/TIMER10/GRTC_3)는 계속 서비스된다
 *                        → 라디오 타이밍 안전 (CLAUDE.md §7 F2/F9)
 * PRIMASK             : WFI 전후 몇 명령어 구간만. 이 구간에서는 BASEPRI 를
 *                        0 으로 내려야 CC 가 CPU 를 깨울 수 있고, PRIMASK 가
 *                        대신 레이스를 막아 준다. PRIMASK 는 기상을 막지 않는다.
 *
 * SD 우선순위 0 IRQ 가 지연되는 구간은 WFI 기상 직후 PRIMASK 를 푸는 데
 * 걸리는 몇 명령어뿐이다. 이보다 짧게 만들 방법은 없다.
 *
 * (Adafruit 는 sd_nvic_critical_region_enter() 로 감싸지만 그 API 는 S145 에
 *  없다. nRF52 의 SoftDevice 는 NVIC 를 가상화해서 앱 인터럽트만 가릴 수
 *  있었고, nRF54L 은 앱이 NVIC 를 직접 소유하므로 그 방법 자체가 없다.)
 */
#define SLEEP_BASEPRI ( 1U << ( 8U - configPRIO_BITS ) )

/* CC 를 뒤로 미룰 때 직전 CC 가 이보다 가까우면 safe 절차를 쓴다.
 * 안 그러면 가짜 COMPARE 이벤트가 떠서 슬립이 즉시 깨진다.
 * Zephyr sys_clock_set_timeout() 의 LATENCY_THR_TICKS 와 같은 취지다. */
#define CC_SAFE_SET_THRESHOLD_CYCLES ( GRTC_CYCLES_PER_TICK * 2 )

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    uint64_t   grid_next, target, exit_cnt;
    uint32_t   prev_basepri;
    TickType_t completed_ticks;

    /* 애플리케이션 IRQ 만 막는다. 여기서는 아직 PRIMASK 를 쓰지 않는다. */
    prev_basepri = __get_BASEPRI();
    __set_BASEPRI_MAX(SLEEP_BASEPRI);
    __DSB();
    __ISB();

    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
    {
        __set_BASEPRI(prev_basepri);
        return;
    }

    /*
     * ── 틱 그리드 ─────────────────────────────────────────────────────
     * m_last_cc 는 "다음 틱의 절대 시각"이고 항상 1 ms 그리드 위에 있다.
     * 슬립 후에도 이 그리드를 유지해야 한다.
     *
     * ⚠ 기상 시각(exit_cnt) 기준으로 다음 CC 를 잡으면 매 슬립마다
     *   1 틱 미만의 나머지가 버려져 millis() 가 조금씩 느려진다.
     *   실측: +253 ppm. LFXO 로 잡아 둔 +25 ppm 을 통째로 날린다.
     */
    grid_next = m_last_cc;

    /* xExpectedIdleTime 틱만큼 재우려면 (n-1) 틱 뒤 그리드에서 깨면 된다.
     * 마지막 틱은 여기 아래에서 vTaskStepTick() 으로 우리가 센다. */
    target = grid_next + (uint64_t)(xExpectedIdleTime - 1U) * GRTC_CYCLES_PER_TICK;

    /*
     * ── 슬립 구간 동안 틱 인터럽트를 끈다 ──────────────────────────────
     * 끄지 않으면 CC 만료로 GRTC ISR 이 pending 이 되고, 마스크를 푸는
     * 순간 실행되어 xTaskIncrementTick() 과 CC 재장전을 또 한다.
     * 우리가 하는 보정과 이중으로 겹친다.
     */
    (void) nrfx_grtc_syscounter_cc_int_disable(m_tick_channel.channel);

    /* CC 를 뒤로 미루는 경우라 safe 절차를 쓴다 (가짜 COMPARE 방지). */
    nrfx_grtc_syscounter_cc_abs_set(m_tick_channel.channel, target, true);
    (void) nrfx_grtc_syscounter_cc_int_enable(m_tick_channel.channel);

    /*
     * ── 슬립 ──────────────────────────────────────────────────────────
     * PRIMASK 를 걸고 BASEPRI 를 0 으로 내린다. 순서가 중요하다:
     * BASEPRI 가 남아 있으면 틱 CC 가 WFI 를 깨우지 못한다 (위 설명 참조).
     * PRIMASK 가 걸려 있으므로 그 사이 인터럽트가 실행되지는 않는다.
     *
     * NVIC->ISPR 폴링은 하지 않는다. WFI 가 알아서 깨고,
     * nRF54L 은 IRQ 가 269번까지 있어 Adafruit 의 ISPR[0]|ISPR[1]
     * 관용구는 애초에 쓸 수 없다.
     */
    __disable_irq();
    __set_BASEPRI(0U);
    __DSB();
    __ISB();

    __WFI();

    /* 기상. 틱 보정 동안 애플리케이션 IRQ 를 다시 막고 PRIMASK 를 푼다.
     * 이 순서라야 SoftDevice 의 우선순위 0 핸들러가 곧바로 실행된다. */
    __set_BASEPRI_MAX(SLEEP_BASEPRI);
    __DSB();
    __ISB();
    __enable_irq();

    exit_cnt = nrfx_grtc_syscounter_get();

    /* 다시 끄고 밀린 인터럽트를 버린다. 틱은 아래에서 우리가 센다. */
    (void) nrfx_grtc_syscounter_cc_int_disable(m_tick_channel.channel);
    NVIC_ClearPendingIRQ(GRTC_IRQn);

    /* 그리드를 몇 칸 지났는가. grid_next 를 지났으면 최소 1 틱이다. */
    if (exit_cnt >= grid_next)
    {
        completed_ticks =
            (TickType_t)(1U + (exit_cnt - grid_next) / GRTC_CYCLES_PER_TICK);
        if (completed_ticks > xExpectedIdleTime)
        {
            completed_ticks = xExpectedIdleTime;
        }
    }
    else
    {
        /* CC 가 아니라 다른 인터럽트가 일찍 깨웠다. 아직 한 틱도 안 지났다. */
        completed_ticks = 0U;
    }

    /* 다음 틱을 그리드 위에서 재장전한다. 나머지가 버려지지 않는다. */
    m_last_cc = grid_next + (uint64_t)completed_ticks * GRTC_CYCLES_PER_TICK;
    if (m_last_cc <= exit_cnt)
    {
        /* 예상보다 오래 잤다(보정이 상한에 걸렸다). 그리드를 다시 잡는다. */
        m_last_cc = exit_cnt + GRTC_CYCLES_PER_TICK;
    }
    nrfx_grtc_syscounter_cc_abs_set(m_tick_channel.channel, m_last_cc, true);
    (void) nrfx_grtc_syscounter_cc_int_enable(m_tick_channel.channel);

    /*
     * 건너뛴 틱을 한 번에 보정한다.
     *
     * ⚠ vTaskStepTick() 만 쓴다. xTaskIncrementTick() 을 같이 부르면 안 된다.
     *   FreeRTOS 표준 tickless 패턴이 이렇다 — 아이들 태스크가 이 함수에서
     *   돌아온 뒤 xTaskResumeAll() 을 부르고, 거기서 스케줄링이 처리된다.
     *   (Adafruit 의 nRF52 포트는 둘 다 부르는데, 그쪽은 RTC1 이 계속 도는
     *    전제라서 가능한 것이고 여기 그대로 옮기면 이중 계산이 된다.)
     */
    if (completed_ticks > 0U)
    {
        vTaskStepTick(completed_ticks);
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
