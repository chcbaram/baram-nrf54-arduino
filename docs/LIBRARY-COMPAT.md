# 제3자 라이브러리 호환 현황

CLAUDE.md §11 의 원칙 — **추측하지 말고 컴파일해 보라.**
이 문서는 실제로 빌드해 본 결과이고, README 의 "지원 범위" 표기의 근거다.

측정: 2026-09-12 · XIAO nRF54L15 (`baram-nrf54:nrf54l:xiao_nrf54l15`)

---

## 1. 결과

| 라이브러리 | 버스 | 컴파일 | 실기 |
|---|---|---|---|
| `Adafruit_BME280` (+ `Adafruit_BusIO`, `Adafruit_Unified_Sensor`) | I2C | ✅ | — |
| `Adafruit_seesaw` | I2C | ✅ | — |
| `Seeed_Arduino_LSM6DS3` | I2C | ✅ | ⚠ 아래 §3 |
| `Adafruit_ST7735/ST7789` (+ `Adafruit_GFX`) | SPI | ✅ | — |
| `SdFat` | SPI | ✅ | ✅ NU54-DK + SD 소켓 |
| `SD` (Arduino) | SPI | ✅ | ✅ NU54-DK + SD 소켓 |
| `MIDI_Library` | UART | ✅ | — |
| `ArduinoJson` | — | ✅ | — |
| **`Servo`** | — | ❌ | **구조적 불가.** §4 |

**"컴파일 ✅ / 실기 —"** 는 부품이 없어 못 재 본 것이지 실패한 것이 아니다.
구분해서 읽어라.

---

## 2. 여기까지 오는 데 필요했던 것 — 셰임 세 벌

처음에는 **I2C 만 쓰는 스케치조차** 컴파일되지 않았다. 원인은 전부 우리 코어의
결손이었고, 라이브러리 쪽 문제가 아니었다.

| 없던 것 | 무엇이 깨졌나 | 넣은 곳 |
|---|---|---|
| `digitalPinToPort` / `digitalPinToBitMask` / `portOutputRegister` / `portInputRegister` | `Adafruit_BusIO` — **거의 모든 Adafruit 센서가 쓴다** | `cores/nrf54l/Arduino.h` |
| `constrain` / `round` / `sq` / `radians` / `bitRead` … | `Adafruit_seesaw` | 〃 |
| `pins_arduino.h` | `Adafruit_ST77xx` | `cores/nrf54l/pins_arduino.h` |

**⚠ `Adafruit_BusIO` 가 관문이다.** I2C 만 쓰는 스케치여도 BusIO 의 **SPI 쪽
소스가 함께 컴파일**되므로, `SPI.h` 와 AVR 포트 매크로가 없으면 무조건 깨진다.
그래서 `SPI` 를 먼저 만들어야 했고, 포트 셰임도 있어야 했다.

⚠ Adafruit 의 2포트 판(`abs < 32 ? NRF_P0 : NRF_P1`)을 그대로 쓸 수 없다.
nRF54L 은 포트가 **셋**, LM20A 는 **넷**이다.

덤: `digitalPinHasPWM()` 은 Adafruit 처럼 대충(`P > 1`) 두지 않고 실제로 답한다.
PWM20/21/22 가 도메인 20 이라 **P1 에만** 붙는다 — 생성된 `nrf54l_pinmap.h` 를 쓴다.

---

## 3. `Seeed_Arduino_LSM6DS3` — 컴파일은 되지만 값이 0 이다

**우리 문제가 아니다.** 그 라이브러리는 `Wire` → `Wire1` 치환을
**특정 Seeed 보드 매크로**(`TARGET_SEEED_XIAO_NRF52840_SENSE` 등)에만 걸어 둔다.
우리 보드에서는 헤더 쪽 `Wire`(TWIM22, D4/D5)를 쓰는데 거기엔 아무것도 없다.
XIAO 의 온보드 IMU 는 `Wire1`(TWIM30, P0.03/P0.04)에 있다.

→ 버스를 인자로 받는 라이브러리를 쓰거나, 센서를 헤더 쪽 `Wire` 에 붙여라.
`Adafruit_BME280` 처럼 `begin(addr, &Wire1)` 를 받는 API 면 문제없다.

**우리 `Wire1` 자체는 동작한다** — `i2c_scanner` 예제가 XIAO 온보드 IMU 를
`0x6A` 로 찾는다 (`docs/STATUS.md` §2.11).

---

## 4. `Servo` — 구조적으로 안 된다

라이브러리 자신이 막는다:

```
Servo.h:79:2: error: #error "This library only supports boards with an AVR, SAM, SAMD, NRF52 or STM32F4 processor."
```

아키텍처 매크로를 검사하는 `#error` 라 셰임으로는 통과시킬 수 없다.
`ARDUINO_ARCH_NRF52` 를 정의하면 통과하겠지만 **그건 별개의 결정이고
위험하다** — nRF52 레지스터 접근 경로가 함께 열린다 (CLAUDE.md §11).
지금은 **미지원**으로 둔다.

PWM 서보가 필요하면 `analogWrite()` 로 직접 몰 수 있다. 단 서보는 보통
50 Hz 를 요구하므로 `analogWriteResolution()` 으로 주파수를 맞춰야 한다
(기준 클럭 1 MHz 고정 → 주파수 = 1 MHz / 2^bits).

---

## 5. 여전히 안 되는 부류 (CLAUDE.md §7 F6)

**bit-banging 라이브러리** — NeoPixel, DHT, OneWire, SoftwareSerial 등.
SoftDevice 가 최상위 우선순위를 점유하고 라디오 이벤트 중 애플리케이션을
블로킹하므로 타이밍이 깨진다. **고치는 것이 아니라 문서화 대상이다.**

---

## 6. 재현

```sh
arduino-cli compile -b baram-nrf54:nrf54l:xiao_nrf54l15 <스케치>
```

라이브러리는 Library Manager 로 설치한다. `architectures=` 불일치 경고는
무시해도 된다 — arduino-cli 는 경고만 내고 컴파일을 진행한다 (§11).
