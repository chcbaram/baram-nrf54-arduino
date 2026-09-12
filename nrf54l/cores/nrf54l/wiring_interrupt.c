/*
 * wiring_interrupt — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "Arduino.h"
#include "wiring_interrupt.h"
#include "nrf54l_domains.h"

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

  *flag = true;
  return true;
}

bool attachInterruptOk(uint32_t pin, voidFuncPtr callback, uint32_t mode)
{
  if (callback == NULL) return false;

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

void detachInterrupt(uint32_t pin)
{
  nrfx_gpiote_t *inst = instance_of(pin);
  if (inst == NULL) return;

  irq_slot_t *s = slot_of(pin);
  if (s == NULL) return;

  nrfx_gpiote_trigger_disable(inst, (nrfx_gpiote_pin_t) pin);

  s->used = false;
  s->cb   = NULL;
}

/*
 * ⚠ 벡터를 직접 잇는다 (CLAUDE.md §7 F10 ③). nrfx 가 주는 것은 인스턴스를 받는
 *   `nrfx_gpiote_irq_handler()` 하나뿐이라 어느 벡터가 어느 인스턴스인지 알려 줘야
 *   한다. 안 이으면 MDK 의 weak Default_Handler 가 남아 인터럽트가 뜨는 순간
 *   무한루프다. 링크 후 `nm <elf> | grep GPIOTE` 로 `T` 인지 확인하라.
 *
 * ⚠ **인스턴스마다 벡터가 둘이다** (`_0` / `_1`). 둘 다 같은 핸들러로 보낸다 —
 *   하나만 이으면 그쪽으로 오는 이벤트만 처리되고 나머지는 조용히 사라진다.
 */
void GPIOTE20_0_IRQHandler(void) { nrfx_gpiote_irq_handler(&m_gpiote20); }
void GPIOTE20_1_IRQHandler(void) { nrfx_gpiote_irq_handler(&m_gpiote20); }
void GPIOTE30_0_IRQHandler(void) { nrfx_gpiote_irq_handler(&m_gpiote30); }
void GPIOTE30_1_IRQHandler(void) { nrfx_gpiote_irq_handler(&m_gpiote30); }
