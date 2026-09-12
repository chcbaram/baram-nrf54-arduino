# M2 실기 검증 — `analogRead` (SAADC)

날짜: 2026-09-12
보드: NU54-DK (nRF54L05, variant `nu54dk`)

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| A0~A7 이 전부 AIN 을 갖는다 | ✅ `oooooooo` |
| AIN 없는 핀 거부 (P0.00) | ✅ |
| 0 V / 3.3 V 측정 | ✅ 0 mV / **3304 mV** |
| 해상도 8·10·12·14비트 | ✅ 전부 ~3300 mV (아래) |
| `analogReference()` 반영 | ✅ 풀스케일이 바뀐다 |
| 안 쓰는 스케치의 크기 증가 | ✅ **0 B** (23,784 B 그대로) |

---

## 2. 배선 없이 알려진 전압을 만드는 법

NU54-DK 는 운 좋게 **A7 = P1.14 = LED4**, **A6 = P1.13 = SW2** 다.
LED 는 N-MOSFET 게이트 구동(고임피던스)이라 출력으로 몰면 핀이 곧 0 V / VDD 다.

```c
pinMode(PIN_LED4, OUTPUT);
digitalWrite(PIN_LED4, LOW);   analogReadMillivolts(PIN_A7);  /* 0 mV */
digitalWrite(PIN_LED4, HIGH);  analogReadMillivolts(PIN_A7);  /* 3304 mV */
```

**외부 계측기 없이 절대 정확도를 잴 수 있다.** 보드 3V3 레귤레이터(AZ1117CR)의
오차를 감안하면 3304 mV 는 정상 범위다.

## 3. 해상도를 바꿔도 전압은 같아야 한다

raw 만 스케일이 바뀌고 mV 는 유지돼야 한다. 이게 어긋나면 변환식이 틀린 것이다.

| 해상도 | raw / 최대 | mV |
|---|---|---|
| 8 | 235 / 255 | 3317 |
| 10 | 939 / 1023 | 3300 |
| 12 | 3760 / 4095 | 3301 |
| 14 | 15008 / 16383 | 3297 |

8비트가 17 mV 높은 것은 양자화 때문이다 (1 LSB = 14 mV).

## 4. 기준전압

| 설정 | 풀스케일 | 3.3 V 입력의 raw(10비트) | mV |
|---|---|---|---|
| `AR_INTERNAL_3_6` | 3600 mV | 940 | 3300 |
| `AR_INTERNAL_1_8` | 1800 mV | **1023 (포화)** | 1800 |
| `AR_INTERNAL_0_9` | 900 mV | **1023 (포화)** | 900 |

풀스케일보다 높은 입력이 최대값에 붙는 것이 맞는 동작이다.

---

## 5. ⚠ nRF52 와 전압 구성이 다르다 — 호환 별칭이 정확하지 않다

| | nRF52 | **nRF54L** |
|---|---|---|
| 내부 기준전압 | 600 mV | **900 mV** |
| 게인 | 1/6 부터 | **1/4 부터** (1/6 없음) |
| VDD/4 기준 | 있음 | **없음** |

근거: `bsp/soc/nrfx_soc_defines.h` 의 `ANALOG_REF_INTERNAL_VAL`
(`NRF54L_SERIES` → 900), MDK `nrf54l15_types.h` 의 `SAADC_CH_CONFIG_GAIN_*`
와 `REFSEL_*` 목록.

나올 수 있는 풀스케일은 이 여덟 개뿐이다:

| 게인 | 풀스케일 |
|---|---|
| 1/4 (2/8) | **3.6 V** |
| 2/7 | 3.15 V |
| 1/3 (2/6) | 2.7 V |
| 2/5 | 2.25 V |
| 1/2 (2/4) | **1.8 V** |
| 2/3 | 1.35 V |
| 1 | 0.9 V |
| 2 | 0.45 V |

Adafruit 이름을 그대로 쓸 수 있게 별칭을 두었지만 **전압이 같지 않은 것이 있다**:

| Adafruit 이름 | 의도 | 우리에게 실제 |
|---|---|---|
| `AR_DEFAULT` / `AR_INTERNAL` | 3.6 V | **3.6 V** ✅ |
| `AR_INTERNAL_1_8` | 1.8 V | **1.8 V** ✅ |
| `AR_INTERNAL_3_0` | 3.0 V | ⚠ **3.15 V** |
| `AR_INTERNAL_2_4` | 2.4 V | ⚠ **2.25 V** |
| `AR_INTERNAL_1_2` | 1.2 V | ⚠ **1.35 V** |
| `AR_VDD4` | VDD | ⚠ **3.6 V** (VDD/4 기준이 없다) |

**대응**: `analogReadMillivolts()` 를 제공한다. 어떤 기준을 골랐든 올바른 mV 를
돌려주므로, 그것만 쓰면 이 차이에 걸리지 않는다.
`analogReferenceMillivolts()` 로 지금 풀스케일을 확인할 수도 있다.

README 의 "부분 지원" 항목에 이 표를 그대로 넣을 것.

---

## 6. AIN 매핑을 손으로 적지 않았다

`nrf54l_pinmap.h` 가 Pin Planner 에서 생성될 때 `NRF54L_SIG_SAADC_AIN0()` 같은
매크로가 이미 들어간다. `ain_of()` 는 그걸 순서대로 물어볼 뿐이다.

```c
if (NRF54L_SIG_SAADC_AIN0(abs_pin)) return 0;
```

**LM20A 는 AIN 배정이 전혀 다르다** (AIN0=P1.00, AIN1=P1.31 …). 생성기를
다시 돌리면 코어 코드를 안 고쳐도 맞는다.

---

## 7. 벡터 — 이번엔 **직접 이으면 안 된다**

GPIOTE/PWM 과 반대다. SAADC 는 **단일 인스턴스**라
`bsp/soc/irqs/nrfx_irqs_nrf54l15_application.h` 가

```c
#define nrfx_saadc_irq_handler          SAADC_IRQHandler
```

로 이름을 바꿔 둔다. nrfx 의 핸들러가 곧 벡터이므로 우리가 또 정의하면
**무한 재귀**다 (§7 F10 ③).

⚠ **부작용**: 그래서 `nrfx_saadc.c` 는 **모든 스케치에 링크된다.** 벡터 테이블이
KEEP 되는데 그 엔트리가 곧 드라이버 함수라 트램폴린으로 끊을 방법이 없다.
단 이건 `analogRead` 를 넣기 **전부터** 그랬다 — 그래서 이번 추가의 크기 증가가
0 B 다.

### 같은 이유로 새고 있는 것이 더 있다

blink 에 링크된 벡터를 전수 조사하니, **쓰지도 않는 단일 인스턴스 드라이버**가
여럿 들어 있었다: `COMP_LPCOMP`, `NFCT`, `TEMP`, `QDEC`, `I2S`, `PDM`, `WDT`.

측정: 그 7개의 `.c` 를 치우면 blink 가 **23,784 → 22,284 B (−1,500 B)**.

되돌려 두었다. **지우면 나중에 쓸 때 다시 가져와야 하므로 판단이 필요하다** —
`nrfx_pdm.c` 는 XIAO 마이크에, `nrfx_i2s.c`/`nrfx_qdec.c` 는 언젠가 쓸 수 있다.
`cores/nrf54l/nordic/nrfx/VENDORING.md` 가 이미 21개를 지운 전례를 적어 두었다.
**M5(패키징) 전에 결정할 것.**
