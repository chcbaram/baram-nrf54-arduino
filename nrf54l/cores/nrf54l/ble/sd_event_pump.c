/*
 * SoftDevice 활성화 + 이벤트 펌프 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * sdk-nrf-bm v2.0.1 의 subsys/softdevice_handler (nrf_sdh.c / nrf_sdh_ble.c /
 * irq_connect.c, LicenseRef-Nordic-5-Clause, Copyright (c) 2024 Nordic
 * Semiconductor ASA) 가 하는 일을 Zephyr 없이 다시 구현한 것이다.
 * Kconfig / SYS_INIT / iterable section 은 걷어내고 배열 하나로 대체했다.
 */

#include "sd_event_pump.h"

#include "Arduino.h"
#include "variant.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <nrf.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <nrf_error.h>
#include <nrfx_cracen.h>
#include <string.h>

/* sd_irq_forward.S / sd_support.c */
extern void CallSoftDeviceResetHandler(void);
extern uint32_t softdevice_vector_forward_address;
extern uint32_t irq_forwarding_enabled_magic_number_holder;

/* 링커 스크립트 (nrf54l15_s145_v10.ld 등) */
extern uint32_t __softdevice_start__;
extern uint32_t __app_ram_start__;

/* irq_connect.h : IRQ_FORWARDING_ENABLED_MAGIC_NUMBER */
#define IRQ_FWD_MAGIC                 0x47F34BC1UL

/*
 * SoftDevice 예약 인터럽트 우선순위 (CLAUDE.md §7 F2, irq_connect.c).
 *   0 : zero-latency. RADIO_0 / TIMER10 / GRTC_3
 *   4 : non-time-critical. AAR00_CCM00 / CLOCK_POWER / ECB00 / SWI00 / SVCall
 * sd_softdevice_enable() 이 필요한 것은 자기가 다시 덮어쓴다.
 */
#define SD_PRIO_ZERO_LATENCY          0
#define SD_PRIO_LOW                   4

/*
 * SD_EVT_IRQn(=SWI01_IRQn) 의 우선순위는 **우리가 정한다.**
 *
 * 이 인터럽트는 SoftDevice 가 "이벤트가 있다" 고 앱을 깨우는 용도이고
 * 핸들러는 앱 소유다. 여기서 xSemaphoreGiveFromISR() 을 부르므로
 * configMAX_SYSCALL_INTERRUPT_PRIORITY(=5) 보다 **낮은 긴급도**여야 한다
 * (숫자가 크면 긴급도가 낮다). 5~7 만 허용된다 — §7 F2.
 * SoftDevice 가 이 값을 4 로 잡아 두면 FreeRTOS 규칙 위반이 되므로
 * sd_softdevice_enable() **이후에** 우리가 다시 설정한다.
 */
#define SD_EVT_IRQ_PRIORITY           6

#ifndef SD_EVT_TASK_STACK_WORDS
#define SD_EVT_TASK_STACK_WORDS       (512)
#endif

/*
 * 이벤트 태스크 우선순위. rtos.h 의 TASK_PRIO_HIGH(=3) 와 같은 값이다.
 * rtos.h 는 C++ 헤더라 여기서 include 할 수 없어 숫자로 적는다.
 * rtos.h 의 주석이 이 자리를 "Bluefruit task (M3)" 로 이미 예약해 두었다.
 * loop() 태스크(TASK_PRIO_LOW=1)보다 높아야 이벤트가 밀리지 않는다.
 */
#define SD_EVT_TASK_PRIORITY          (3)

/*
 * 이벤트 버퍼. sd_ble_evt_get() 이 여기에 이벤트를 복사한다.
 * 부족하면 NRF_ERROR_DATA_SIZE 가 나오는데, 조용히 넘기지 않고 기록한다.
 * MTU 를 키우거나 긴 GATT 쓰기를 받으면 늘려야 한다.
 */
/*
 * ⚠ 이벤트 버퍼는 **MTU 에 따라 커져야 한다.** ble.h 의 BLE_EVT_LEN_MAX:
 *   "The highest value used for ble_gatt_conn_cfg_t::att_mtu in any connection
 *    configuration shall be used as a parameter."
 *   모자라면 sd_ble_evt_get() 이 NRF_ERROR_DATA_SIZE 를 내고 이벤트가 스택에
 *   남아 진행이 멈춘다.
 */
#ifndef SD_BLE_EVT_BUF_SIZE
#define SD_BLE_EVT_BUF_SIZE           BLE_EVT_LEN_MAX(SD_BLE_ATT_MTU)
#endif

#ifndef SD_BLE_MAX_OBSERVERS
#define SD_BLE_MAX_OBSERVERS          (8)
#endif

/*
 * ATT MTU. 기본값 23 이면 한 번에 20바이트(23 − ATT 헤더 3)밖에 못 싣고,
 * 그보다 큰 쓰기는 ATT long write 경로가 필요해 연결이 끊긴다.
 * ⚠ 키우면 SoftDevice RAM 요구량이 늘 수 있다. sdCfgResults() 로 확인하라.
 */
#ifndef SD_BLE_ATT_MTU
#define SD_BLE_ATT_MTU                (247)
#endif

/*
 * 연결 구성 태그.
 *
 * ⚠ **0(BLE_CONN_CFG_TAG_DEFAULT)을 쓰면 안 된다.** ble.h 원문:
 *   "Must be different for all connection configurations added and
 *    not BLE_CONN_CFG_TAG_DEFAULT"
 *   0 이면 sd_ble_cfg_set() 이 성공을 돌려주는데도 설정이 적용되지 않는다.
 * ⚠ sd_ble_gap_adv_start() 에 **같은 태그**를 넘겨야 그 구성이 쓰인다.
 */
#ifndef SD_BLE_CONN_CFG_TAG
#define SD_BLE_CONN_CFG_TAG           (1)
#endif

/* 연결 이벤트 길이 (1.25 ms 단위). */
#ifndef SD_BLE_EVENT_LENGTH
#define SD_BLE_EVENT_LENGTH           (6)
#endif

/*
 * LFCLK 설정. 소스는 variant 가 정한 크리스털 유무를 따른다.
 *
 * ⚠ accuracy 는 **선언**이지 측정치가 아니다. 실제보다 타이트하게 선언하면
 *   스택이 스케줄링 마진을 좁게 잡아 연결이 불안정해진다. Nordic 기본값인
 *   250 ppm 을 쓴다. 우리 보드는 실측 ±40 ppm 안이지만(docs/HIL/),
 *   기본값으로 문제가 없으면 굳이 좁힐 이유가 없다.
 *   좁히려면 variant 에서 SD_CLOCK_LF_ACCURACY 를 정의한다.
 */
#ifndef SD_CLOCK_LF_ACCURACY
#define SD_CLOCK_LF_ACCURACY          NRF_CLOCK_LF_ACCURACY_250_PPM
#endif

/*
 * HFXO 램프업 시간 (us). 스택이 라디오를 켜기 전에 얼마나 일찍 HFXO 를
 * 기동할지 정하는 값이다. 너무 작으면 클럭이 안정되기 전에 송신한다.
 * 실기에서 확인할 것 — sd_softdevice_enable() 이 범위를 벗어난 값을 거부한다.
 */
#ifndef SD_CLOCK_HFCLK_LATENCY_US
#define SD_CLOCK_HFCLK_LATENCY_US     (1500)
#endif

/*
 * HFINT 보정 주기 (초).
 *
 * ⚠ **0 을 넣으면 안 된다.** nrf_sdm.h 가 범위를 1~255 로 규정하고,
 *   벗어나면 sd_softdevice_enable() 이 NRF_ERROR_INVALID_PARAM(0x07) 을
 *   돌려준다. 실제로 0 으로 두었다가 여기서 막혔다.
 *   "LFCLK 이 크리스털이니 HFINT 보정은 무관하다" 고 생각하기 쉬운데
 *   이 필드는 소스와 무관하게 유효한 값이어야 한다.
 *   sdk-nrf-bm 의 기본값이 60 이다.
 */
#ifndef SD_CLOCK_HFINT_CTIV_SEC
#define SD_CLOCK_HFINT_CTIV_SEC       (60)
#endif

/* ───────────────────────────────────────────────────────────────────── */

typedef struct {
    sd_ble_observer_t handler;
    void             *ctx;
} sd_observer_t;

static sd_observer_t     m_observers[SD_BLE_MAX_OBSERVERS];
static uint8_t           m_observer_count = 0;

static bool              m_enabled     = false;
static uint32_t          m_last_error  = 0;
static uint32_t          m_ram_used    = 0;
static uint32_t          m_ram_required = 0;

/* cfg_set 각 항목의 반환값. sdCfgResults() 로 읽는다 (진단용). */
static uint32_t          m_cfg_role    = 0xFFFFFFFFu;
static uint32_t          m_cfg_gap     = 0xFFFFFFFFu;
static uint32_t          m_cfg_gatt    = 0xFFFFFFFFu;

static SemaphoreHandle_t m_evt_sem     = NULL;
static TaskHandle_t      m_evt_task    = NULL;

/** sd_ble_evt_get() 이 버퍼 부족을 알렸을 때 남는다. 0 이 아니면 버퍼를 키워라. */
volatile uint32_t        g_sd_evt_buf_too_small = 0;

/**
 * sdEnable() 이 어디까지 갔는지 남긴다. 실기에서 멈췄을 때 SWD 로 읽는다.
 * probe-rs 는 halt 를 유지하지 못하므로 이렇게 흔적을 남기는 편이 확실하다
 * (docs/HIL/M1-nu54dk.md §5).
 */
volatile uint32_t        g_sd_stage = 0;

/** SoftDevice 가 치명적 오류를 보고했을 때 남는다 (id / pc / info). */
volatile uint32_t        g_sd_fault_id  = 0;
volatile uint32_t        g_sd_fault_pc  = 0;
volatile uint32_t        g_sd_fault_info = 0;

/* 4바이트 정렬. sd_ble_evt_get() 이 워드 정렬을 요구한다. */
static uint32_t          m_evt_buf[(SD_BLE_EVT_BUF_SIZE + 3) / 4];

/* ───────────────────────────────────────────────────────────────────── */

/*
 * SoftDevice 치명적 오류. 여기서 돌아가면 안 되는 상태다.
 * SWD 로 g_sd_fault_* 를 읽으면 원인을 알 수 있다
 * (docs/HIL/M1-nu54dk.md §5 의 계측 방식과 같다).
 */
static void sd_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    g_sd_fault_id   = id;
    g_sd_fault_pc   = pc;
    g_sd_fault_info = info;

    for (;;) {
        /* 멈춘다 */
    }
}

/*
 * SoftDevice RNG 시딩.
 *
 * ⚠ 이걸 안 하면 sd_ble_enable() 이 **NRF_ERROR_INVALID_STATE(0x08)** 로 실패한다.
 *   ble.h 의 설명이 "The BLE stack had already been initialized ... **or the random
 *   number generator has not been seeded**" 라서, 메시지만 보면 "이미 초기화됨"
 *   쪽으로 오해하기 쉽다. 실제로 그렇게 한참 헤맸다.
 *
 * SoftDevice 는 켜진 직후 NRF_EVT_RAND_SEED_REQUEST 를 올린다. 엔트로피는
 * nRF54L 의 CRACEN TRNG 에서 얻는다 (nRF52 의 RNG 페리페럴이 아니다 —
 * nrfx/VENDORING.md 에서 nrfx_rng.c 를 뺀 이유가 이것이다).
 */
static uint32_t sd_seed_rng(void)
{
    uint8_t  seed[SD_RAND_SEED_SIZE];
    uint32_t err;
    int      ret;

    ret = nrfx_cracen_init();
    if (ret != 0 && ret != -EALREADY) {
        return NRF_ERROR_INTERNAL;
    }

    ret = nrfx_cracen_entropy_get(seed, sizeof(seed));
    if (ret != 0) {
        return NRF_ERROR_INTERNAL;
    }

    err = sd_rand_seed_set(seed);

    /* 시드는 즉시 지운다 (원본 rand_seed.c 와 같다). */
    memset(seed, 0, sizeof(seed));

    return err;
}

/*
 * SoC 이벤트를 전부 꺼낸다. BLE 이벤트와 같은 인터럽트(SD_EVT_IRQn)로 알려오므로
 * 깨어날 때마다 둘 다 비워야 한다.
 */
static void sd_soc_evts_poll(void)
{
    uint32_t evt_id;

    while (sd_evt_get(&evt_id) == NRF_SUCCESS) {
        if (evt_id == NRF_EVT_RAND_SEED_REQUEST) {
            uint32_t err = sd_seed_rng();
            if (err != NRF_SUCCESS) {
                m_last_error = err;
            }
        }
        /* 그 밖의 SoC 이벤트는 M3 뒷단계에서 다룬다
         * (플래시 동작 완료, 전원 경고, 라디오 타임슬롯 등). */
    }
}

static void sd_dispatch(const ble_evt_t *evt)
{
    for (uint8_t i = 0; i < m_observer_count; i++) {
        m_observers[i].handler(evt, m_observers[i].ctx);
    }
}

/*
 * 이벤트 태스크. SD_EVT_IRQ 가 깨우면 스택이 빌 때까지 전부 꺼낸다.
 *
 * ⚠ 한 번 깨울 때 **가능한 이벤트를 모두** 꺼내야 한다. 하나만 꺼내고 자면
 *   나머지가 그대로 남고 다시 깨워 주지 않는다 (ble.h 의 SD_EVT_IRQn 설명).
 */
static void sd_evt_task(void *arg)
{
    (void) arg;

    for (;;) {
        xSemaphoreTake(m_evt_sem, portMAX_DELAY);

        sd_soc_evts_poll();

        for (;;) {
            uint16_t len = (uint16_t) sizeof(m_evt_buf);
            uint32_t err = sd_ble_evt_get((uint8_t *) m_evt_buf, &len);

            if (err == NRF_SUCCESS) {
                sd_dispatch((const ble_evt_t *) m_evt_buf);
                continue;
            }
            if (err == NRF_ERROR_NOT_FOUND) {
                break;                      /* 다 꺼냈다 */
            }
            if (err == NRF_ERROR_DATA_SIZE) {
                /* 버퍼가 작다. 이벤트가 스택에 남아 무한 루프가 되므로
                 * 기록하고 빠져나온다. SD_BLE_EVT_BUF_SIZE 를 키워야 한다. */
                g_sd_evt_buf_too_small = len;
                break;
            }
            m_last_error = err;
            break;
        }
    }
}

/* SoftDevice 가 이벤트를 알릴 때 편다. SD_EVT_IRQn = SWI01_IRQn (nrf_soc.h). */
void SWI01_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;

    if (m_evt_sem != NULL) {
        xSemaphoreGiveFromISR(m_evt_sem, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

/* ───────────────────────────────────────────────────────────────────── */

bool sdIsEnabled(void)
{
    return m_enabled;
}

uint32_t sdLastError(void)
{
    return m_last_error;
}

uint32_t sdRamUsed(void)
{
    return m_ram_used;
}

uint16_t sdAttMtu(void)
{
    return (uint16_t) SD_BLE_ATT_MTU;
}

uint8_t sdConnCfgTag(void)
{
    return (uint8_t) SD_BLE_CONN_CFG_TAG;
}

void sdCfgResults(uint32_t *role, uint32_t *gap, uint32_t *gatt, uint32_t *ram_required)
{
    if (role)         *role         = m_cfg_role;
    if (gap)          *gap          = m_cfg_gap;
    if (gatt)         *gatt         = m_cfg_gatt;
    if (ram_required) *ram_required = m_ram_required;
}

bool sdBleObserverAdd(sd_ble_observer_t handler, void *ctx)
{
    if (handler == NULL || m_observer_count >= SD_BLE_MAX_OBSERVERS) {
        return false;
    }
    m_observers[m_observer_count].handler = handler;
    m_observers[m_observer_count].ctx     = ctx;
    m_observer_count++;
    return true;
}

/*
 * BLE 스택 구성. **sd_ble_enable() 전에** 불러야 한다.
 * 실패해도 그 항목만 기본값으로 남으므로 기록만 하고 계속 간다.
 */
static void sd_ble_cfg_apply(uint32_t ram_base)
{
    ble_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    /*
     * ⚠ adv_set_count 를 빠뜨리지 마라. 구조체 **첫 필드**라 memset 뒤에
     *   periph_role_count 만 채우기 쉬운데, ble_gap.h 가 이 조합을 명시적으로
     *   거부한다: "NRF_ERROR_INVALID_PARAM — adv_set_count is 0 and
     *   periph_role_count is non-zero."
     *   그러면 역할 수가 기본값(peripheral 1 / central 3)으로 남고, 이어지는
     *   sd_ble_enable() 이 conn_count 와 안 맞아 NRF_ERROR_NOT_SUPPORTED(0x06)
     *   로 실패한다. 증상은 "BLE 를 못 켠다" 로만 보인다.
     */
    cfg.gap_cfg.role_count_cfg.adv_set_count      = BLE_GAP_ADV_SET_COUNT_DEFAULT;
    cfg.gap_cfg.role_count_cfg.periph_role_count  = SD_BLE_PERIPH_LINK_COUNT;
    cfg.gap_cfg.role_count_cfg.central_role_count = 0;
    cfg.gap_cfg.role_count_cfg.central_sec_count  = 0;
    m_cfg_role = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &cfg, ram_base);

    memset(&cfg, 0, sizeof(cfg));
    cfg.conn_cfg.conn_cfg_tag                     = SD_BLE_CONN_CFG_TAG;
    cfg.conn_cfg.params.gap_conn_cfg.conn_count   = SD_BLE_PERIPH_LINK_COUNT;
    cfg.conn_cfg.params.gap_conn_cfg.event_length = SD_BLE_EVENT_LENGTH;
    m_cfg_gap = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &cfg, ram_base);

    memset(&cfg, 0, sizeof(cfg));
    cfg.conn_cfg.conn_cfg_tag                 = SD_BLE_CONN_CFG_TAG;
    cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = SD_BLE_ATT_MTU;
    m_cfg_gatt = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &cfg, ram_base);
}

bool sdEnable(void)
{
    uint32_t err;

    if (m_enabled) {
        return true;
    }

    /* ── 1. 이벤트 태스크를 먼저 만든다 ────────────────────────────────
     * SoftDevice 는 켜지자마자 rand seed 를 요청하는 이벤트를 올린다.
     * 펌프가 준비되기 전에 켜면 그 이벤트를 놓친다. */
    g_sd_stage = 1;
    if (m_evt_sem == NULL) {
        m_evt_sem = xSemaphoreCreateBinary();
        if (m_evt_sem == NULL) {
            m_last_error = NRF_ERROR_NO_MEM;
            return false;
        }
    }
    if (m_evt_task == NULL) {
        if (xTaskCreate(sd_evt_task, "sd_evt", SD_EVT_TASK_STACK_WORDS,
                        NULL, SD_EVT_TASK_PRIORITY, &m_evt_task) != pdPASS) {
            m_last_error = NRF_ERROR_NO_MEM;
            return false;
        }
    }

    /* ── 2. IRQ 포워딩을 켠다 ─────────────────────────────────────────
     * 순서가 중요하다: 주소를 넣고 → SoftDevice Reset Handler 를 부르고
     * → 마지막에 매직을 세운다. 매직을 먼저 세우면 아직 초기화되지 않은
     * SoftDevice 로 인터럽트가 넘어간다 (irq_connect.c 와 같은 순서). */
    g_sd_stage = 2;
    softdevice_vector_forward_address = (uint32_t) &__softdevice_start__;

    NVIC_SetPriority(RADIO_0_IRQn,     SD_PRIO_ZERO_LATENCY);
    NVIC_SetPriority(TIMER10_IRQn,     SD_PRIO_ZERO_LATENCY);
    NVIC_SetPriority(GRTC_3_IRQn,      SD_PRIO_ZERO_LATENCY);
    NVIC_SetPriority(AAR00_CCM00_IRQn, SD_PRIO_LOW);
    NVIC_SetPriority(CLOCK_POWER_IRQn, SD_PRIO_LOW);
    NVIC_SetPriority(ECB00_IRQn,       SD_PRIO_LOW);
    NVIC_SetPriority(SWI00_IRQn,       SD_PRIO_LOW);
    NVIC_SetPriority(SVCall_IRQn,      SD_PRIO_LOW);

    g_sd_stage = 3;
    CallSoftDeviceResetHandler();
    g_sd_stage = 4;
    irq_forwarding_enabled_magic_number_holder = IRQ_FWD_MAGIC;
    g_sd_stage = 5;

    /* ── 3. SoftDevice 활성화 ─────────────────────────────────────────
     * ⚠ 여기서부터 SVC >= 0x10 이 SoftDevice 로 흘러간다.
     *   그전에 부르면 널 포인터로 점프한다 (sd_svc_dispatch.S 주석). */
    {
        const nrf_clock_lf_cfg_t clock_cfg = {
#if defined(USE_LFXO)
            .source        = NRF_CLOCK_LF_SRC_XTAL,
            .rc_ctiv       = 0,
            .rc_temp_ctiv  = 0,
#else
            /* 크리스털이 없는 보드. Nordic 권장 보정 주기다.
             * ⚠ RC 는 BLE 요구치를 못 맞춘다 (§7 F12). 연결이 끊긴다. */
            .source        = NRF_CLOCK_LF_SRC_RC,
            .rc_ctiv       = 16,
            .rc_temp_ctiv  = 2,
#endif
            .accuracy      = SD_CLOCK_LF_ACCURACY,
            .hfclk_latency = SD_CLOCK_HFCLK_LATENCY_US,
            .hfint_ctiv    = SD_CLOCK_HFINT_CTIV_SEC,
        };

        g_sd_stage = 6;
        err = sd_softdevice_enable(&clock_cfg, sd_fault_handler);
        g_sd_stage = 7;
        if (err != NRF_SUCCESS) {
            m_last_error = err;
            irq_forwarding_enabled_magic_number_holder = 0;
            return false;
        }
    }

    /* ── 4. 이벤트 인터럽트 ───────────────────────────────────────────
     * SoftDevice 가 잡아 둔 우선순위를 우리 값으로 되돌린다 (위 주석 참조). */
    g_sd_stage = 8;
    NVIC_SetPriority((IRQn_Type) SD_EVT_IRQn, SD_EVT_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ((IRQn_Type) SD_EVT_IRQn);
    NVIC_EnableIRQ((IRQn_Type) SD_EVT_IRQn);

    /* ── 4b. RNG 시드 ────────────────────────────────────────────────
     * SoftDevice 가 켜지자마자 올린 NRF_EVT_RAND_SEED_REQUEST 를 **여기서
     * 동기적으로** 처리한다. 펌프 태스크가 처리하기를 기다리면 그 사이에
     * sd_ble_enable() 이 먼저 불려 INVALID_STATE 로 실패한다.
     * 원본 nrf_sdh.c 도 같은 이유로 sd_ble_enable() 전에 직접 폴링한다. */
    sd_soc_evts_poll();

    /* ── 5. BLE 스택 활성화 ───────────────────────────────────────────
     * sd_ble_enable() 은 앱 RAM 시작 주소를 받고, SoftDevice 가 필요로 하는
     * 최소 주소를 같은 변수에 돌려준다. 돌려준 값이 우리 시작 주소보다 크면
     * 링커 스크립트의 RAM ORIGIN 을 올려야 한다는 뜻이다. */
    {
        uint32_t app_ram_base = (uint32_t) &__app_ram_start__;
        const uint32_t requested = app_ram_base;

        sd_ble_cfg_apply(app_ram_base);

        g_sd_stage = 9;
        err = sd_ble_enable(&app_ram_base);
        g_sd_stage = 10;

        /* app_ram_base 에 SoftDevice 가 요구하는 **최소 시작 주소**가 돌아온다. */
        m_ram_required = app_ram_base;
        (void) requested;

        if (err != NRF_SUCCESS) {
            m_last_error = err;
            return false;
        }

        m_ram_used = app_ram_base - (uint32_t) 0x20000000UL;
        (void) app_ram_base;
    }

    g_sd_stage = 11;
    m_enabled = true;
    return true;
}
