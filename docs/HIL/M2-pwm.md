# M2 실기 검증 — `analogWrite` (PWM)

날짜: 2026-09-12
보드: NU54-DK (nRF54L05, variant `nu54dk`)
프로브: **NU-DAP** — CMSIS-DAP `0d28:0204`, J3 연결

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| PWM20 출력 (P1.10 = LED2) | ✅ |
| 듀티 정확도 | ✅ 0 / 24 / 50 / 74 / 100 % (요청 0 / 25 / 50 / 74 / 100) |
| **극성** | ✅ 반전 아님 |
| `analogWriteResolution(10)` | ✅ 256/1023 → 24 % |
| `digitalWrite()` 가 PWM 을 놓는다 | ✅ 0 % |
| P2 핀 거부 | ✅ `analogWriteOk(PIN_LED1)` = false |
| 안 쓰는 스케치의 크기 증가 | ✅ **+88 B** (GPIOTE 와 합쳐서) |

---

## 2. 듀티를 어떻게 쟀나 — `digitalRead` 로는 못 잰다

처음 시험 스케치는 `digitalRead()` 로 HIGH 비율을 세려 했다. **그러면 항상 0 % 가 나온다.**

`digitalRead()` 는 출력 방향인 핀에서 **OUT 레지스터**를 읽는다(`nrf_gpio_pin_read`
가 방향을 보고 고른다). 그런데 **PWM 은 OUT 을 거치지 않고 PSEL 로 패드를 직접 몬다.**
OUT 은 `pins_configure()` 가 0 으로 지워 둔 그대로다.

→ 입력 버퍼를 다시 붙이고 **IN 레지스터를 직접** 읽는다:

```c
nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_CONNECT,
             NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
...
(nrf_gpio_port_in_read(port) >> (pin & 31)) & 1u
```

출력 방향은 그대로 두므로 PWM 구동을 방해하지 않는다.
`analogWrite()` 가 핀을 다시 잡을 때마다 버퍼가 떨어지므로 **매번 다시 붙여야** 한다.

측정값 (50,000 샘플):

```
duty   0% -> 0%
duty  25% -> 24%
duty  50% -> 50%
duty  74% -> 74%
duty 100% -> 100%
```

**극성이 뒤집혔다면 100 / 76 / 50 / 26 / 0 이 나왔을 것이다.** 이 시험이
아니었으면 LED 밝기만으로는 눈치채지 못했을 값이다 (0% 와 100% 는 양쪽 다
"꺼짐/켜짐" 으로 보인다).

---

## 3. 극성 비트 — 직관과 반대다

듀티 값의 **최상위 비트(15)가 극성**이고, **정상(액티브 하이) 출력일 때 그 비트를
세운다.** 근거는 Zephyr `drivers/pwm/pwm_nrfx.c`:

```c
#define PWM_NRFX_CH_VALUE(compare_value, inverted) \
	(compare_value | (inverted ? 0 : PWM_NRFX_CH_POLARITY_MASK))
```

비워 두면 듀티가 통째로 뒤집힌다. 크래시도 로그도 없다.

100 % 는 비교값을 `top` 이 아니라 **`0x7FFF`** 로 준다. `top` 을 그대로 주면
한 틱 모자라 99.6 % 가 된다 (Zephyr 도 같은 처리를 한다 —
*"This value is always greater than or equal to COUNTERTOP"*).

---

## 4. 벡터를 반드시 이어야 하는 이유

핸들러를 `NULL` 로 등록하므로 PWM 인터럽트를 쓰지 않는다. 그래도 벡터를
비워 두면 안 된다 — **nrfx 가 NVIC 라인을 무조건 켠다.**

`nrfy_pwm.h` 의 `nrfy_pwm_int_init()`:

```c
NRFX_IRQ_PRIORITY_SET(nrfx_get_irq_number(p_reg), irq_priority);
NRFX_IRQ_ENABLE(nrfx_get_irq_number(p_reg));     /* <- enable 인자와 무관 */
if (enable) { nrf_pwm_int_enable(p_reg, mask); }
```

`enable = false` 로 불러도 NVIC 는 켜진다. 페리페럴 INTEN 만 꺼져 있을 뿐이다.
**"인터럽트를 안 쓰니까 벡터를 안 이어도 된다" 는 판단은 틀렸다.**

---

## 5. 크기 — 벡터에서 드라이버로 가는 길을 끊어라

`docs/HIL/M2-interrupt.md` §4 와 같은 내용이다. 요약:

| blink | flash |
|---|---|
| 진짜 기준선 | 23,696 B |
| 벡터 → 드라이버 직접 호출 (GPIOTE + PWM) | 25,840 B (**+2,144**) |
| 함수 포인터 트램폴린 | **23,784 B** (**+88**) |

벡터 테이블은 링커가 `KEEP` 하므로 GC 의 루트다. 벡터가 드라이버를 직접 부르면
안 쓰는 스케치도 드라이버를 문다.

---

## 6. 주파수

기준 클럭을 **1 MHz 고정**으로 두었다. 주파수 = 1 MHz / 2^bits.

| 해상도 | 주파수 |
|---|---|
| 8비트 (기본) | 3.9 kHz |
| 10비트 | 977 Hz |
| 12비트 | 244 Hz |

LED·모터에 무난한 범위다. 해상도를 올리면 주파수가 떨어진다는 점을 헤더에 적어 두었다.

---

## 7. 제약

- **P1 핀만** (LM20A 는 P1 과 P3). PWM20/21/22 가 전부 도메인 20 이다
- 동시에 **12핀** (3 인스턴스 × 4채널)
- ⚠ **NU54-DK 의 `LED_BUILTIN`(P2.09)과 `PIN_LED3`(P2.07)에는 안 된다.**
  LED2(P1.10) 나 LED4(P1.14) 를 써라
- ⚠ **XIAO nRF54L15 는 사용자 LED 가 P2.00 하나뿐이라 아예 안 된다.**
  헤더 핀(D0~D5 는 P1)에 LED 를 달아야 한다.
  XIAO nRF54LM20A 는 LED 셋이 전부 P1 이라 문제없다
