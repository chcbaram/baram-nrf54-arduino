/*
 * wiring_analog.c — analogWrite (PWM)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "Arduino.h"
#include "wiring_analog.h"
#include "wiring_private.h"
#include "nrf54l_domains.h"

#include "nrfx_pwm.h"

/*
 * PWM 인스턴스 셋, 각 4채널 = 동시에 12핀.
 * 셋 다 도메인 20 이라 **P1 핀만** 몰 수 있다 (LM20A 는 P3 도).
 */
#define PWM_INST_COUNT   (3)
#define PWM_CH_COUNT     (NRF_PWM_CHANNEL_COUNT)   /* 4 */

/*
 * 기준 클럭. 1 MHz 로 고정한 이유는 주파수를 예측 가능하게 두기 위해서다 —
 * 주파수 = 1 MHz / (top + 1). 8비트면 3.9 kHz 로 LED·모터에 무난하고,
 * 사람이 깜빡임을 못 느끼는 범위다.
 */
#define PWM_BASE_CLOCK   NRF_PWM_CLK_1MHz

/*
 * 듀티 값의 최상위 비트가 극성이다. **정상(액티브 하이) 출력일 때 이 비트를
 * 세운다** — 직관과 반대라 틀리기 쉽다. 근거는 Zephyr `drivers/pwm/pwm_nrfx.c`:
 *
 *     #define PWM_NRFX_CH_VALUE(compare_value, inverted) \
 *         (compare_value | (inverted ? 0 : PWM_NRFX_CH_POLARITY_MASK))
 *
 * 비워 두면 듀티가 통째로 뒤집힌다. 크래시가 없어 증상만으로는 못 찾는다.
 */
#define PWM_POLARITY_NORMAL   (0x8000u)
#define PWM_COMPARE_MAX       (0x7FFFu)

typedef struct {
  nrfx_pwm_t                  drv;
  nrf_pwm_values_individual_t seq;      /* EasyDMA 가 읽는다 — 반드시 RAM */
  uint32_t                    pin[PWM_CH_COUNT];   /* arduino 핀 번호. 0xFFFFFFFF = 빈 칸 */
  bool                        running;
} pwm_slot_t;

#define PWM_PIN_FREE   (0xFFFFFFFFu)

static pwm_slot_t m_pwm[PWM_INST_COUNT] = {
  { .drv = NRFX_PWM_INSTANCE(NRF_PWM20) },
  { .drv = NRFX_PWM_INSTANCE(NRF_PWM21) },
  { .drv = NRFX_PWM_INSTANCE(NRF_PWM22) },
};

static void dispatch0(void);
static void dispatch1(void);
static void dispatch2(void);
static irq_vector_fn_t s_isr[PWM_INST_COUNT];

static uint8_t  m_bits = 8;
static bool     m_slots_inited;

static uint16_t top_value(void) { return (uint16_t)((1u << m_bits) - 1u); }

static void slots_init_once(void)
{
  if (m_slots_inited) return;
  for (int i = 0; i < PWM_INST_COUNT; i++) {
    for (int c = 0; c < PWM_CH_COUNT; c++) m_pwm[i].pin[c] = PWM_PIN_FREE;
  }
  m_slots_inited = true;
}

/** 듀티 → 시퀀스 값. 0 은 계속 LOW, 최대는 계속 HIGH. */
static uint16_t duty_value(uint32_t value)
{
  uint16_t top = top_value();
  uint16_t cmp;

  if (value == 0) {
    cmp = 0;                     /* 계속 비활성 */
  } else if (value >= top) {
    /*
     * COUNTERTOP 이상이면 비교가 영영 성립하지 않아 계속 활성이다.
     * `top` 을 그대로 주면 한 틱 모자라 99.6% 가 되므로 최대값을 쓴다.
     */
    cmp = PWM_COMPARE_MAX;
  } else {
    cmp = (uint16_t) value;
  }
  return cmp | PWM_POLARITY_NORMAL;
}

static uint16_t *seq_slot(pwm_slot_t *s, int ch)
{
  /* nrf_pwm_values_individual_t 는 channel_0~3 네 개짜리 구조체다.
   * 레이아웃이 uint16_t 넷과 같으므로 배열로 본다. */
  return &((uint16_t *) &s->seq)[ch];
}

/** 설정을 적용하고 재생을 (다시) 시작한다. */
static bool slot_apply(pwm_slot_t *s)
{
  nrfx_pwm_config_t cfg = NRFX_PWM_DEFAULT_CONFIG(
      NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED,
      NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED);

  for (int c = 0; c < PWM_CH_COUNT; c++) {
    uint32_t abs;
    cfg.output_pins[c] = (s->pin[c] != PWM_PIN_FREE &&
                          nrf54lPinResolve(s->pin[c], &abs))
                         ? abs : NRF_PWM_PIN_NOT_CONNECTED;
  }
  cfg.base_clock = PWM_BASE_CLOCK;
  cfg.count_mode = NRF_PWM_MODE_UP;
  cfg.top_value  = top_value();
  cfg.load_mode  = NRF_PWM_LOAD_INDIVIDUAL;   /* 채널마다 듀티가 다르다 */
  cfg.step_mode  = NRF_PWM_STEP_AUTO;
  cfg.irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY;

  if (s->running) {
    /*
     * 핀이 늘거나 해상도가 바뀌면 재설정이 필요하다. 돌고 있는 상태에서
     * 핀을 바꿀 수 없으므로 세웠다가 다시 올린다.
     */
    nrfx_pwm_stop(&s->drv, true);
    if (nrfx_pwm_reconfigure(&s->drv, &cfg) != 0) return false;
  } else {
    /* nrfx 4.x 는 0 이 성공, 음수가 오류다 (CLAUDE.md §7 F10 ①). */
    int err = nrfx_pwm_init(&s->drv, &cfg, NULL, NULL);
    if (err != 0 && err != -EALREADY) return false;

    /* 벡터를 실제 핸들러에 잇는다. 이 대입이 유일한 참조라서, analogWrite 를
     * 안 부르는 스케치에서는 드라이버가 통째로 GC 된다. */
    static const irq_vector_fn_t fns[PWM_INST_COUNT] = { dispatch0, dispatch1, dispatch2 };
    s_isr[s - m_pwm] = fns[s - m_pwm];
  }

  nrf_pwm_sequence_t const seq = {
    .values     = { .p_individual = &s->seq },
    .length     = NRF_PWM_VALUES_LENGTH(s->seq),
    .repeats    = 0,
    .end_delay  = 0,
  };

  /*
   * ⚠ LOOP 로 돌려 둔다. 그래야 듀티를 바꿀 때 재생을 다시 시작하지 않고
   *   **버퍼만 고쳐 쓰면** 다음 주기부터 반영된다 (EasyDMA 가 매 주기 읽는다).
   *   한 번만 재생하면 analogWrite 마다 stop/start 가 필요해 출력에 글리치가 난다.
   */
  nrfx_pwm_simple_playback(&s->drv, &seq, 1, NRFX_PWM_FLAG_LOOP);
  s->running = true;
  return true;
}

/** 이 핀이 이미 잡혀 있나. 없으면 빈 칸을 준다. */
static bool find_slot(uint32_t arduino_pin, pwm_slot_t **out_s, int *out_ch, bool *is_new)
{
  pwm_slot_t *free_s = NULL;
  int         free_c = -1;

  for (int i = 0; i < PWM_INST_COUNT; i++) {
    for (int c = 0; c < PWM_CH_COUNT; c++) {
      if (m_pwm[i].pin[c] == arduino_pin) {
        *out_s = &m_pwm[i]; *out_ch = c; *is_new = false;
        return true;
      }
      if (free_s == NULL && m_pwm[i].pin[c] == PWM_PIN_FREE) {
        free_s = &m_pwm[i]; free_c = c;
      }
    }
  }
  if (free_s == NULL) return false;      /* 12개를 다 썼다 */

  *out_s = free_s; *out_ch = free_c; *is_new = true;
  return true;
}

bool analogWriteOk(uint32_t arduino_pin, uint32_t value)
{
  uint32_t abs;
  if (!nrf54lPinResolve(arduino_pin, &abs)) return false;

  /* PWM20/21/22 는 전부 도메인 20 이다. P2·P0 를 담당하는 PWM 은 없다. */
  if (NRF54L_PORT_OF(abs) != NRF54L_DOMAIN20_PORT) return false;

  slots_init_once();

  pwm_slot_t *s; int ch; bool is_new;
  if (!find_slot(arduino_pin, &s, &ch, &is_new)) return false;

  *seq_slot(s, ch) = duty_value(value);

  if (is_new) {
    s->pin[ch] = arduino_pin;
    /* 훅을 여기서 단다. wiring_digital.c 가 PWM 코드를 직접 부르지 않게 하려는
     * 것이다 — 이유는 wiring_private.h 참조. */
    g_pwmRelease = analogWriteStop;

    if (!slot_apply(s)) {
      s->pin[ch] = PWM_PIN_FREE;
      return false;
    }
  }
  /* 기존 핀이면 버퍼만 고쳤다. LOOP 재생이라 다음 주기에 반영된다. */
  return true;
}

void analogWrite(uint32_t pin, uint32_t value)
{
  (void) analogWriteOk(pin, value);
}

void analogWriteResolution(uint8_t bits)
{
  if (bits < 1 || bits > 15) return;      /* 비교값이 15비트다 */
  if (bits == m_bits) return;
  m_bits = bits;

  /* 돌고 있는 인스턴스는 top 이 바뀌었으므로 다시 설정한다. */
  for (int i = 0; i < PWM_INST_COUNT; i++) {
    if (m_pwm[i].running) slot_apply(&m_pwm[i]);
  }
}

void analogWriteStop(uint32_t arduino_pin)
{
  slots_init_once();

  for (int i = 0; i < PWM_INST_COUNT; i++) {
    for (int c = 0; c < PWM_CH_COUNT; c++) {
      if (m_pwm[i].pin[c] != arduino_pin) continue;

      m_pwm[i].pin[c] = PWM_PIN_FREE;
      *seq_slot(&m_pwm[i], c) = duty_value(0);

      /* 남은 핀이 없으면 페리페럴을 내린다 (저전력). */
      bool any = false;
      for (int k = 0; k < PWM_CH_COUNT; k++) {
        if (m_pwm[i].pin[k] != PWM_PIN_FREE) any = true;
      }
      if (any) {
        slot_apply(&m_pwm[i]);
      } else if (m_pwm[i].running) {
        nrfx_pwm_stop(&m_pwm[i].drv, true);
        nrfx_pwm_uninit(&m_pwm[i].drv);
        m_pwm[i].running = false;
      }
      return;
    }
  }
}

/*
 * ⚠ 벡터는 **반드시 정의해 두어야 한다** (CLAUDE.md §7 F10 ③).
 *   지금은 이벤트 핸들러를 NULL 로 등록해 인터럽트를 쓰지 않지만, 그래도
 *   비워 두면 안 된다 — nrfx 가 **NVIC 라인은 무조건 켜기** 때문이다
 *   (`nrfy_pwm_int_init` 안의 `NRFX_IRQ_ENABLE` 은 `enable` 인자와 무관하다).
 *
 * ⭐ 벡터가 `nrfx_pwm_irq_handler` 를 직접 부르면 PWM 드라이버가 모든
 *   스케치에 링크된다 — 실측 **+576 B**. 함수 포인터를 한 겹 둔다
 *   (wiring_private.h 참조). analogWrite 를 안 부르면 트램폴린 셋만 남는다.
 */
static void dispatch0(void) { nrfx_pwm_irq_handler(&m_pwm[0].drv); }
static void dispatch1(void) { nrfx_pwm_irq_handler(&m_pwm[1].drv); }
static void dispatch2(void) { nrfx_pwm_irq_handler(&m_pwm[2].drv); }

void PWM20_IRQHandler(void) { if (s_isr[0]) s_isr[0](); }
void PWM21_IRQHandler(void) { if (s_isr[1]) s_isr[1](); }
void PWM22_IRQHandler(void) { if (s_isr[2]) s_isr[2](); }

/*═══════════════════════════════════════════════════════════════════════
 * analogRead (SAADC)
 *═══════════════════════════════════════════════════════════════════════
 *
 * ⚠ SAADC 는 **단일 인스턴스**라 벡터를 직접 이으면 안 된다 (§7 F10 ③).
 *   `bsp/soc/irqs/nrfx_irqs_nrf54l15_application.h` 가
 *   `#define nrfx_saadc_irq_handler SAADC_IRQHandler` 로 이름을 바꿔 두어
 *   nrfx 의 핸들러가 곧 벡터다. 우리가 또 정의하면 무한 재귀다.
 *
 *   부작용이 하나 있다 — 그래서 **`nrfx_saadc.c` 는 모든 스케치에 링크된다.**
 *   벡터 테이블이 KEEP 되는데 그 엔트리가 곧 드라이버 함수이기 때문이다.
 *   PWM/GPIOTE 처럼 트램폴린으로 끊을 수가 없다. analogRead 를 넣기 전부터
 *   이미 그랬다 (실측으로 확인).
 */
#include "nrfx_saadc.h"
#include "nrf54l_pinmap.h"

#define SAADC_CH   (0)     /* 한 번에 한 핀만 읽으므로 채널 하나면 된다 */

static uint8_t           m_adc_bits = 10;
static eAnalogReference  m_adc_ref  = AR_INTERNAL_3_6;
static bool              m_adc_inited;

/** 기준전압(900 mV)에 게인을 적용한 풀스케일. 헤더의 이름과 맞아야 한다. */
static uint32_t ref_millivolts(eAnalogReference r)
{
  switch (r) {
    case AR_INTERNAL_3_15: return 3150;
    case AR_INTERNAL_2_7:  return 2700;
    case AR_INTERNAL_2_25: return 2250;
    case AR_INTERNAL_1_8:  return 1800;
    case AR_INTERNAL_1_35: return 1350;
    case AR_INTERNAL_0_9:  return  900;
    case AR_INTERNAL_0_45: return  450;
    default:               return 3600;
  }
}

static nrf_saadc_gain_t ref_gain(eAnalogReference r)
{
  switch (r) {
    case AR_INTERNAL_3_15: return NRF_SAADC_GAIN2_7;
    case AR_INTERNAL_2_7:  return NRF_SAADC_GAIN1_3;
    case AR_INTERNAL_2_25: return NRF_SAADC_GAIN2_5;
    case AR_INTERNAL_1_8:  return NRF_SAADC_GAIN1_2;
    case AR_INTERNAL_1_35: return NRF_SAADC_GAIN2_3;
    case AR_INTERNAL_0_9:  return NRF_SAADC_GAIN1;
    case AR_INTERNAL_0_45: return NRF_SAADC_GAIN2;
    default:               return NRF_SAADC_GAIN1_4;
  }
}

static nrf_saadc_resolution_t res_enum(void)
{
  switch (m_adc_bits) {
    case 8:  return NRF_SAADC_RESOLUTION_8BIT;
    case 12: return NRF_SAADC_RESOLUTION_12BIT;
    case 14: return NRF_SAADC_RESOLUTION_14BIT;
    default: return NRF_SAADC_RESOLUTION_10BIT;
  }
}

/**
 * 절대 GPIO 번호 → AIN 번호. 없으면 -1.
 *
 * 표를 손으로 적지 않는다 — `nrf54l_pinmap.h` 가 Pin Planner 에서 생성된
 * 것이고, 거기 `NRF54L_SIG_SAADC_AINn()` 이 이미 들어 있다. SoC 가 바뀌면
 * 생성기만 다시 돌리면 된다 (LM20A 는 AIN 배정이 전혀 다르다).
 */
static int ain_of(uint32_t abs_pin)
{
  if (NRF54L_SIG_SAADC_AIN0(abs_pin)) return 0;
  if (NRF54L_SIG_SAADC_AIN1(abs_pin)) return 1;
  if (NRF54L_SIG_SAADC_AIN2(abs_pin)) return 2;
  if (NRF54L_SIG_SAADC_AIN3(abs_pin)) return 3;
  if (NRF54L_SIG_SAADC_AIN4(abs_pin)) return 4;
  if (NRF54L_SIG_SAADC_AIN5(abs_pin)) return 5;
  if (NRF54L_SIG_SAADC_AIN6(abs_pin)) return 6;
  if (NRF54L_SIG_SAADC_AIN7(abs_pin)) return 7;
  return -1;
}

bool analogReadOk(uint32_t arduino_pin, int *out_value)
{
  uint32_t abs;
  if (!nrf54lPinResolve(arduino_pin, &abs)) return false;

  int ain = ain_of(abs);
  if (ain < 0) return false;

  if (!m_adc_inited) {
    /* 핸들러 없이 블로킹으로 쓴다. 한 번 읽는 데 수십 µs 라 태스크를
     * 재우는 것이 오히려 손해다 (Wire/SPI 와 판단이 다른 이유). */
    int err = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
    if (err != 0 && err != -EALREADY) return false;
    m_adc_inited = true;
  }

  nrfx_saadc_channel_t ch = NRFX_SAADC_DEFAULT_CHANNEL_SE(
      (nrfx_analog_input_t)(NRFX_ANALOG_EXTERNAL_AIN0 + ain), SAADC_CH);
  ch.channel_config.gain      = ref_gain(m_adc_ref);
  ch.channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;

  if (nrfx_saadc_channel_config(&ch) != 0) return false;
  if (nrfx_saadc_simple_mode_set(1u << SAADC_CH, res_enum(),
                                 NRF_SAADC_OVERSAMPLE_DISABLED, NULL) != 0) return false;

  nrf_saadc_value_t sample = 0;
  if (nrfx_saadc_buffer_set(&sample, 1) != 0) return false;
  if (nrfx_saadc_mode_trigger() != 0) return false;

  /* 단극(single-ended)이라 음수는 나오지 않아야 하지만, 오프셋 때문에
   * 0 근처에서 −1 같은 값이 나온다. Arduino 규약대로 0 으로 자른다. */
  *out_value = (sample < 0) ? 0 : (int) sample;
  return true;
}

int analogRead(uint32_t pin)
{
  int v = 0;
  (void) analogReadOk(pin, &v);
  return v;
}

int analogReadMillivolts(uint32_t pin)
{
  int raw = 0;
  if (!analogReadOk(pin, &raw)) return 0;

  uint32_t full = (1u << m_adc_bits) - 1u;
  return (int) (((uint32_t) raw * ref_millivolts(m_adc_ref)) / full);
}

void analogReadResolution(uint8_t bits)
{
  if (bits == 8 || bits == 10 || bits == 12 || bits == 14) m_adc_bits = bits;
}

void analogReference(eAnalogReference ref)
{
  m_adc_ref = ref;
}

uint32_t analogReferenceMillivolts(void)
{
  return ref_millivolts(m_adc_ref);
}
