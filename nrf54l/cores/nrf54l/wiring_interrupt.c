/*
 * wiring_interrupt — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "Arduino.h"
#include "wiring_interrupt.h"
#include "nrf54l_domains.h"
#include "wiring_private.h"

#include "nrfx_gpiote.h"

/*
 * GPIOTE 는 둘뿐이고 **각자 자기 도메인 포트만 본다**
 * (docs/PERIPHERAL-PINMAP.md §4, Pin Planner 의 SoC 정의로 확인):
 *
 *   GPIOTE20  0x400DA000  P1 전용  채널 8
 *   GPIOTE30  0x4010C000  P0 전용  채널 4
 *
 * ⚠ **P2 를 담당하는 GPIOTE 가 없다.** P2 핀에는 핀 인터럽트를 걸 수 없다.
 *   (System OFF 기상용 SENSE 는 별개다 — wiring.c 의 systemOff() 참조.)
 */
static nrfx_gpiote_t m_gpiote20 = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
static nrfx_gpiote_t m_gpiote30 = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE30);

/*
 * 콜백 표. 핀 번호로 바로 찾지 않고 선형 탐색하는 이유는, 핀 번호가
 * `포트<<5 | 인덱스` 라 성기게 흩어져 있어 배열로 잡으면 낭비가 크기 때문이다.
 * 채널 수만큼만 있으면 된다 (P1 8 + P0 4).
 */
#define WIRING_IRQ_MAX   (12)

typedef struct {
  uint32_t    pin;
  voidFuncPtr cb;
  bool        used;
} irq_slot_t;

static irq_slot_t m_slots[WIRING_IRQ_MAX];
static bool       m_inited20, m_inited30;

static nrfx_gpiote_t *instance_of(uint32_t pin)
{
  switch (NRF54L_PORT_OF(pin)) {
    case 0:  return &m_gpiote30;
    case 1:  return &m_gpiote20;
    default: return NULL;      /* P2 — 담당 GPIOTE 가 없다 */
  }
}

static irq_slot_t *slot_of(uint32_t pin)
{
  for (uint8_t i = 0; i < WIRING_IRQ_MAX; i++) {
    if (m_slots[i].used && m_slots[i].pin == pin) return &m_slots[i];
  }
  return NULL;
}

static irq_slot_t *slot_free(void)
{
  for (uint8_t i = 0; i < WIRING_IRQ_MAX; i++) {
    if (!m_slots[i].used) return &m_slots[i];
  }
  return NULL;
}

/* nrfx 가 부른다 — 인터럽트 문맥이다. */
static void gpiote_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger, void *ctx)
{
  (void) trigger;
  (void) ctx;

  irq_slot_t *s = slot_of((uint32_t) pin);
  if (s && s->cb) s->cb();
}

static void dispatch20(void);
static void dispatch30(void);
static irq_vector_fn_t s_isr20, s_isr30;

static bool ensure_init(nrfx_gpiote_t *inst)
{
  bool *flag = (inst == &m_gpiote20) ? &m_inited20 : &m_inited30;
  if (*flag) return true;

  /*
   * 우선순위는 FreeRTOS 규칙을 따라 5~7 이어야 한다 (CLAUDE.md §7 F2).
   * 콜백에서 `...FromISR` 을 부를 수 있어야 하기 때문이다.
   * nrfx 4.x 는 0 이 성공, 음수가 오류다 (§7 F10 ①).
   */
  int err = nrfx_gpiote_init(inst, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
  if (err != 0 && err != -EALREADY) return false;

  /* 여기서 벡터를 실제 핸들러에 잇는다. 이 대입이 유일한 참조라서,
   * attachInterrupt 를 안 부르면 드라이버가 통째로 GC 된다. */
  if (inst == &m_gpiote20) s_isr20 = dispatch20;
  else                     s_isr30 = dispatch30;

  *flag = true;
  return true;
}

bool attachInterruptOk(uint32_t arduino_pin, voidFuncPtr callback, uint32_t mode)
{
  if (callback == NULL) return false;

  /* ⚠ 반드시 매핑을 거친다. 한동안 절대 번호를 그대로 썼는데, 그러면
   *   `NRF54L_PIN_NC` 로 막아 둔 핀 — 모듈이 안 뽑은 핀이나 LFXO 의
   *   XL1/XL2 같은 것 — 에도 GPIOTE 가 붙는다. */
  uint32_t pin;
  if (!nrf54lPinResolve(arduino_pin, &pin)) return false;

  nrfx_gpiote_t *inst = instance_of(pin);
  if (inst == NULL) return false;            /* P2 */
  if (!ensure_init(inst)) return false;

  nrfx_gpiote_trigger_t trig;
  switch (mode) {
    case RISING:    trig = NRFX_GPIOTE_TRIGGER_LOTOHI; break;
    case FALLING:   trig = NRFX_GPIOTE_TRIGGER_HITOLO; break;
    case CHANGE:    trig = NRFX_GPIOTE_TRIGGER_TOGGLE; break;
    case LOW_LEVEL: trig = NRFX_GPIOTE_TRIGGER_LOW;    break;
    default:        return false;
  }

  irq_slot_t *s = slot_of(pin);
  if (s == NULL) {
    s = slot_free();
    if (s == NULL) return false;             /* 표가 찼다 */
  }

  uint8_t channel;
  uint8_t *p_channel = NULL;

  /*
   * 엣지 트리거만 GPIOTE 채널을 쓴다. 레벨 트리거는 채널이 아니라 SENSE 로
   * 동작하므로 채널을 주면 안 된다 (nrfx 주석: "when channel is provided
   * only edge triggering can be used").
   */
  if (trig != NRFX_GPIOTE_TRIGGER_LOW) {
    if (nrfx_gpiote_channel_alloc(inst, &channel) != 0) return false;
    p_channel = &channel;
  }

  nrfx_gpiote_trigger_config_t tcfg = { .trigger = trig, .p_in_channel = p_channel };
  nrfx_gpiote_handler_config_t hcfg = { .handler = gpiote_handler, .p_context = NULL };
  nrfx_gpiote_input_pin_config_t cfg = {
    .p_pull_config    = NULL,                /* pinMode() 로 잡아 둔 풀을 건드리지 않는다 */
    .p_trigger_config = &tcfg,
    .p_handler_config = &hcfg,
  };

  if (nrfx_gpiote_input_configure(inst, (nrfx_gpiote_pin_t) pin, &cfg) != 0) {
    if (p_channel) nrfx_gpiote_channel_free(inst, channel);
    return false;
  }

  s->pin  = pin;
  s->cb   = callback;
  s->used = true;

  nrfx_gpiote_trigger_enable(inst, (nrfx_gpiote_pin_t) pin, true);
  return true;
}

void attachInterrupt(uint32_t pin, voidFuncPtr callback, uint32_t mode)
{
  (void) attachInterruptOk(pin, callback, mode);
}

void detachInterrupt(uint32_t arduino_pin)
{
  uint32_t pin;
  if (!nrf54lPinResolve(arduino_pin, &pin)) return;

  nrfx_gpiote_t *inst = instance_of(pin);
  if (inst == NULL) return;

  irq_slot_t *s = slot_of(pin);
  if (s == NULL) return;

  nrfx_gpiote_trigger_disable(inst, (nrfx_gpiote_pin_t) pin);

  s->used = false;
  s->cb   = NULL;
}

/*
 * ⚠ 벡터는 **반드시 정의해 두어야 한다** (CLAUDE.md §7 F10 ③). nrfx 가
 *   NVIC 라인을 무조건 켜므로, 안 이으면 MDK 의 weak Default_Handler 가
 *   남아 인터럽트가 뜨는 순간 무한루프다.
 *
 * ⚠ **인스턴스마다 벡터가 둘이다** (`_0` / `_1`). 하나만 이으면 그쪽으로
 *   오는 이벤트만 처리되고 나머지는 조용히 사라진다.
 *
 * ⭐ 그런데 벡터가 `nrfx_gpiote_irq_handler` 를 **직접** 부르면 안 된다.
 *   벡터 테이블은 링커가 KEEP 하므로 GPIOTE 드라이버가 모든 스케치에
 *   링크된다 — 실측 **+1,568 B**. 그래서 함수 포인터를 한 겹 둔다
 *   (wiring_private.h 의 설명 참조). attachInterrupt 를 안 부르는 스케치는
 *   아래 트램폴린 넷과 포인터 둘만 남는다.
 *
 *   링크 후 `nm <elf> | grep GPIOTE` 로 `T` 인지 확인하라.
 */
static void dispatch20(void) { nrfx_gpiote_irq_handler(&m_gpiote20); }
static void dispatch30(void) { nrfx_gpiote_irq_handler(&m_gpiote30); }

void GPIOTE20_0_IRQHandler(void) { if (s_isr20) s_isr20(); }
void GPIOTE20_1_IRQHandler(void) { if (s_isr20) s_isr20(); }
void GPIOTE30_0_IRQHandler(void) { if (s_isr30) s_isr30(); }
void GPIOTE30_1_IRQHandler(void) { if (s_isr30) s_isr30(); }
