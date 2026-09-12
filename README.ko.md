# BARAM nRF54L Arduino Core

**nRF54L 시리즈용 Arduino 코어 — 기존 Adafruit Bluefruit(nRF52) 스케치가 그대로
동작하는 것을 목표로 만든다.**

*[English](README.md) · [한국어](README.ko.md)*

[![License: MIT](https://img.shields.io/badge/core-MIT-blue.svg)](LICENSE)
[![SoftDevice](https://img.shields.io/badge/SoftDevice-S145%20v10.0.1-orange.svg)](docs/LICENSE-INVENTORY.md)
[![Status](https://img.shields.io/badge/status-M4%20(DFU)%20next-yellow.svg)](docs/STATUS.md)

> ### ⚠ 초기 릴리스 — v0.3.0
> **Arduino 페리페럴 API 가 전부 들어왔다** — `Wire` · `SPI` · `attachInterrupt` ·
> `analogWrite` · `analogRead` 다섯 개를 모두 실기 검증했다. blink / `Serial` /
> 멀티태스킹 / tickless idle 도 **보드 3종에서 동작**하고 Board Manager 로 설치된다.
>
> **BLE 는 peripheral 과 central 이 모두 동작한다** — Adafruit `bleuart` 원본
> 예제가 `#include` 두 줄만 지우면 그대로 돌고, MTU 247 협상·다중 연결·
> iBeacon / EddyStone·스캔·연결·GATT 클라이언트까지 실기에서 확인했다.
> **페어링·본딩(LESC 포함)과 HID** — 키보드 / 마우스 / 미디어키 / 게임패드 — 에
> 더해 BLE-MIDI, iPhone 알림(ANCS), 페어링된 폰에서 시각 읽기도 동작한다.
>
> ⚠ **nRF54L 은 페리페럴이 GPIO 포트에 묶여 있다.** 아무 핀에나 붙던 nRF52 와
> 다르다. 잘못 배정하면 런타임에 조용히 실패하는 대신 **빌드가 멈추고**, 칩별·
> 보드별 전체 표는 동봉된 **PinMap** 예제에 있다.
>
> 아직 없는 것: **부트로더.** 업로드는 지금 SWD 로만 된다. UART / BLE OTA DFU 가
> 다음(M4)이다.
>
> 진행 상황과 호환 현황: [docs/STATUS.md](docs/STATUS.md) ·
> [docs/EXAMPLE-COMPAT.md](docs/EXAMPLE-COMPAT.md) ·
> [docs/LIBRARY-COMPAT.md](docs/LIBRARY-COMPAT.md)

---

## 목차

- [왜 만드는가](#왜-만드는가)
- [특징](#특징)
- [지원 보드](#지원-보드)
- [설치](#설치)
- [SoftDevice 를 먼저 굽는다](#softdevice-를-먼저-굽는다)
- [첫 스케치](#첫-스케치)
- [지원 범위](#지원-범위)
- [구조](#구조)
- [문제 해결](#문제-해결)
- [기여](#기여)
- [라이선스](#라이선스--오픈소스-다만-한-덩어리는-바이너리로-동봉된다)

---

## 왜 만드는가

nRF54L 은 nRF52 의 후속이다. 그리고 nRF52 에는 Arduino BLE 코드 자산이 가장 많이
쌓여 있다 — Adafruit 의 Bluefruit 생태계다. 그런데 그게 그냥은 넘어오지 않는다.

지금까지 나온 nRF54L Arduino 시도들은 **전부 제3의 자체 BLE API** 를 노출한다.
그것도 합리적인 선택이지만, 결과적으로 잘 돌던 nRF52 스케치를 처음부터 다시 써야 하고
`Bluefruit.begin()` 위에 쌓아 둔 라이브러리와 예제가 새 칩에서는 아무 가치가 없다.

이 프로젝트는 반대쪽에 선다.

> **이식성과 Adafruit 호환이 충돌하면 호환을 택한다.**

Bluefruit Feather 용으로 짠 `.ino` 가 nRF54L 보드에서 최소 수정으로 컴파일되고
동작하는 것이 목표다 — 같은 `Bluefruit` API, 같은 `Scheduler.startLoop()`,
FreeRTOS 위에서 같은 의미로 도는 `delay()`.

설계를 그만큼 좌우한 두 번째 목표는 **커스텀 보드** 다. 직접 만든 보드에 펌웨어를
넣고, 필드에서 UART 나 BLE 로 업데이트하는 데에 **모든 유닛마다 디버그 프로브가
붙어 있어야 할 이유는 없다.**

## 특징

| | |
|---|---|
| **기존 nRF52 스케치가 그대로 동작** | Bluefruit 호환 API, `SchedulerRTOS`, AVR 호환 셰임. 마이그레이션은 부수 효과가 아니라 이 프로젝트의 존재 이유다 |
| **인증받은 BLE 스택** | Nordic SoftDevice **S145** — peripheral **과** central 모두. 자체 구현이 아니다 |
| **추가 설치가 없다** | Board Manager 가 코어·컴파일러·플래시 툴을 한 번에 깐다. Python 도, SDK 도, `west` 도 필요 없다 |
| **가볍고 오프라인에서도 된다** | 플랫폼 아카이브가 **1.7 MB**. SDK 기반 툴체인은 첫 설치가 수 GB 다 |
| **크로스 플랫폼** | macOS / Linux / Windows, x86-64 와 arm64 |
| **진짜 FreeRTOS, tickless** | GRTC 틱이 하드웨어 카운터 대비 **0 ppm** 으로 실측됐고, 슬립 중에도 `millis()` 가 어긋나지 않는다 |
| **부트로더가 로드맵에 있다** | UART / BLE OTA DFU (M4). SWD 경로는 그 뒤에도 유지한다 |

**과장하지 않기 위해 적어 둔다.** BLE 성능이 Zephyr/NCS 기반 코어보다 낫지 않다.
그쪽은 Nordic 의 SoftDevice Controller 를 쓰는데, 여기서 쓰는 SoftDevice 와 같은
인증 컨트롤러 계열이다. 차이는 **어떤 API 로 코드를 쓰는가, 설치가 얼마나 무거운가,
프로젝트가 어디로 가는가** 이지 라디오 품질이 아니다.

## 지원 보드

| 보드 | MCU | Flash / RAM | 디버그 | 핀맵 |
|---|---|---|---|---|
| **Seeed XIAO nRF54L15** / Sense | nRF54L15 | 1.5 MB / 256 KB | **온보드 CMSIS-DAP** | [문서](docs/boards/XIAO-nRF54L15.md) |
| **NU54-DK** | nRF54L05 | 500 KB / 96 KB | 외부 프로브 | [문서](docs/boards/NU54-DK.md) |
| **NU54V-DK** | nRF54L15 | 1.5 MB / 256 KB | 외부 프로브 | [문서](docs/boards/NU54-DK.md) |

전부 128 MHz Cortex-M33 이다. **XIAO 는 USB-C 케이블 하나면 된다** — 온보드
디버거가 플래시와 시리얼을 모두 처리하므로 시작하기에 가장 편하다.

메모리 배치는 보드가 아니라 칩 단위다: [docs/MEMORY-MAP.md](docs/MEMORY-MAP.md).

> 보드를 하나 추가하는 데 필요한 건 `variants/` 디렉토리 하나와 `boards.txt` 항목
> 하나다. [docs/boards/XIAO-nRF54L15.md](docs/boards/XIAO-nRF54L15.md) 가 실제 사례다.

## 설치

### Board Manager (권장)

1. **Arduino IDE → 환경설정** 을 연다
2. **추가 보드 매니저 URL** 에 아래를 넣는다:
   ```
   https://raw.githubusercontent.com/chcbaram/baram-nrf54-arduino/main/package_baram_nrf54_index.json
   ```
3. **툴 → 보드 → 보드 매니저** 에서 `nRF54L` 로 검색해
   **BARAM nRF54L Boards** 를 설치한다
4. **툴 → 보드 → BARAM nRF54L Boards** 에서 보드를 고른다

Board Manager 가 Arm 툴체인과 `probe-rs` 까지 함께 설치하므로 따로 준비할 것이 없다.
macOS/arm64 에서 설치 → 컴파일 → 업로드 → 동작까지 확인했다.
Linux / Windows 는 배포는 됐지만 아직 실기에서 확인하지 못했다.

### 소스에서 설치 — **코어를 직접 고칠 때**

코어 자체를 개발하려면 저장소를 sketchbook 의 `hardware/` 밑에 둔다.
심볼릭 링크를 쓰면 git 작업은 원래 위치에서 그대로 한다.

```sh
git clone https://github.com/chcbaram/baram-nrf54-arduino
mkdir -p ~/Documents/Arduino/hardware
ln -s "$(pwd)/baram-nrf54-arduino" ~/Documents/Arduino/hardware/baram-nrf54
```

sketchbook 경로는 `arduino-cli config get directories.user` 로 확인한다
(IDE 에서는 **환경설정 → 스케치북 위치**).

> **링크 이름은 반드시 `baram-nrf54` 여야 한다.** 이 방식에서는 FQBN 의 앞부분이
> 디렉토리 이름으로 정해지고, Board Manager 로 설치하면 패키지 인덱스에서 정해진다.
> 다른 이름을 쓰면 나중에 릴리스본으로 옮겼을 때 FQBN 이 맞지 않는다.

그리고 툴 두 개를 직접 준비한다. `platform.txt` 는 Board Manager 가 이미 깔아 둔
것을 전제하기 때문이다.

| 툴 | 버전 | 비고 |
|---|---|---|
| [xPack arm-none-eabi-gcc](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v14.2.1-1.1) | **14.2.1-1.1** | 릴리스가 이 버전에 고정돼 있다. 다른 버전도 빌드는 되지만 크기가 달라진다 |
| [probe-rs](https://github.com/probe-rs/probe-rs/releases/tag/v0.32.0) | **0.32.0** | 저장소에 동봉하지 않는다. 릴리스 설치본은 Board Manager 가 받아 오고, 소스 설치는 업스트림이나 이 저장소의 [`probe-rs-0.32.0`](https://github.com/chcbaram/baram-nrf54-arduino/releases/tag/probe-rs-0.32.0) 릴리스에서 받는다 (Board Manager 가 쓰는 것과 같은 재포장본이다) |

경로는 예시 파일을 복사해서 적는다:

```sh
cp nrf54l/platform.local.txt.example nrf54l/platform.local.txt
```

빠뜨리면 어떻게 되는지까지 포함한 전체 절차:
[docs/STATUS.md § 다른 PC에서 이어서 작업하기](docs/STATUS.md).

## SoftDevice 를 먼저 굽는다

**새 보드마다 한 번, 첫 스케치를 올리기 전에 해야 한다.**

SoftDevice 는 Nordic 의 Bluetooth 스택이고, 스케치와는 **별개의 이미지**다.
스케치는 주소 0 에서 시작하고 SoftDevice 는 RRAM 상단의 자기 파티션에 들어간다.
스케치를 업로드하면 스케치만 쓰인다 — **SoftDevice 는 같이 올라가지 않는다.**
공장에서 갓 나온 보드나 mass erase 를 한 보드는 그 파티션이 비어 있다.

건너뛰면 BLE 가 아예 뜨지 않는다. 파티션에 무엇이 남아 있느냐에 따라
`Bluefruit.begin()` 이 `false` 를 돌려줄 수도 있고, 비어 있는 메모리로 SVC 를 포워딩해
돌아오지 않을 수도 있다. 어느 쪽이든 **시리얼에는 원인을 알려 주는 것이 아무것도
나오지 않는다.** 이 문서가 이 이야기를 문제 해결이 아니라 첫 스케치 앞에 두는 이유다.

두 이미지는 영역이 겹치지 않으므로 SoftDevice 를 구워도 이미 올라가 있는 스케치는
지워지지 않는다. 그래도 순서는 **SoftDevice 부터**로 두는 게 낫다 — 스케치를 올리는
순간 바로 쓸 수 있는 상태가 된다.

### Arduino IDE 에서

1. 디버그 프로브를 연결한다. XIAO nRF54L15 는 온보드라 USB-C 하나면 되고,
   NU54-DK / NU54V-DK 는 J3 헤더에 외부 CMSIS-DAP 프로브가 필요하다
2. **툴 → 보드** 에서 보드를 **먼저** 고른다. 어떤 hex 를 쓸지가 여기서 정해진다 —
   nRF54L05 와 nRF54L15 는 SoftDevice 빌드가 서로 다르다
3. **툴 → 프로그래머 → `Burn SoftDevice (probe-rs)`**
4. **툴 → 부트로더 굽기**

> 메뉴 이름과 달리 4번이 굽는 것은 **SoftDevice** 이고 부트로더가 아니다.
> 부트로더는 아직 없다 (M4 예정). Arduino IDE 에 "두 번째 이미지를 굽는" 메뉴가
> 따로 없어서 여기에 얹었다.

### arduino-cli 에서

```sh
arduino-cli burn-bootloader --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 --programmer sd_burn
```

FQBN 은 자기 보드로 바꾼다 — `nu54dk` / `nu54vdk` / `xiao_nrf54l15`.
그 다음은 평소대로 올리면 된다:

```sh
arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <스케치>
arduino-cli upload  --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <스케치>
```

프로브가 여러 개 꽂혀 있으면 `probe-rs` 가 어느 보드인지 고르지 못해 실패한다.
나머지를 빼거나, **툴 → Upload method → CMSIS-DAP + Probe UID** 로 두고
`probe-rs list` 가 알려 주는 UID 를 넣는다.

### 다시 구워야 하는 경우

- **툴 → 프로그래머 → `Mass erase / recover (probe-rs)`** 를 실행했을 때 —
  SoftDevice 를 포함해 RRAM 전체가 지워진다
- `probe-rs download` 나 `nrfjprog` 로 직접 구웠을 때 — 스케치와 함께
  SoftDevice 파티션까지 지워질 수 있다

### 안 구워진 보드는 이렇게 보인다

첫 BLE 호출 전까지는 멀쩡히 돈다 — LED 도 깜빡이고 `Serial` 도 나온다 — 그런데
**광고가 시작되지 않는다.** BLE 를 안 쓰는 스케치는 SoftDevice 를 건드리지 않으므로
없어도 잘 돈다. 즉 **blink 가 된다고 해서 BLE 준비가 됐다는 뜻은 아니다.**

## 첫 스케치

```cpp
void loop2()
{
  digitalToggle(LED_CONN);
  delay(500);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_CONN, OUTPUT);

  Scheduler.startLoop(loop2);      // 두 번째 태스크. Adafruit rtos.h 와 같은 API
}

void loop()
{
  digitalToggle(LED_BUILTIN);
  delay(1000);
  Serial.printf("millis=%lu micros=%lu\n", millis(), micros());
}
```

`loop()` 와 `loop2()` 는 각각 별도의 FreeRTOS 태스크이고, `delay()` 는 바쁜 대기가
아니라 양보한다. Adafruit nRF52 코어와 같은 동작이다.

IDE 의 **업로드** 버튼을 쓰거나:

```sh
arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <스케치>
arduino-cli upload  --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <스케치>
```

시리얼 모니터는 **115200 보** 로 연다.

### BLE 스케치

Adafruit Bluefruit 과 같은 API 다. 폰의 Bluefruit Connect 나 nRF Connect 로 붙어
UART 처럼 주고받는다.

```cpp
#include <bluefruit.h>

BLEUart bleuart;

void setup()
{
  Serial.begin(115200);

  Bluefruit.begin();
  Bluefruit.setName("BARAM nRF54L");
  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);          // 0 = 계속 광고
}

void loop()
{
  while (bleuart.available()) Serial.write(bleuart.read());
  while (Serial.available())  bleuart.write(Serial.read());
}
```

예제가 더 있다: **파일 → 예제 → Bluefruit54Lib** — `bleuart`, `bleuart_multi`,
`custom_service`, `beacon`, `eddystone_url`, `blehid_keyboard`, `blehid_gamepad`,
`blemidi`, `ancs`, `client_cts`, `central_hid` 등. **Wire** · **SPI** · **PinMap**
라이브러리도 같은 메뉴에 각자 예제를 둔다.

## 지원 범위

**지원** — GPIO, `millis()` / `micros()` / `delay()`, `Serial`, `SchedulerRTOS`,
tickless idle 을 켠 FreeRTOS.

**BLE (peripheral)** — advertising, GATT 서버(커스텀 서비스 포함), `BLEUart`(NUS),
`BLEDis`, `BLEBas`, ATT MTU 247 협상, `BLEBeacon`(iBeacon)과 `EddyStoneUrl`,
상대 이름 읽기(`getPeerName()`).

**BLE (central)** — 스캔과 필터(`BLEScanner`), 연결(`BLECentral`), 서비스·특성
탐색, `BLEClientUart` / `BLEClientBas` / `BLEClientDis`.

**동시 연결** — 역할 배분은 `Bluefruit.begin(peripheral, central)` 로 스케치가
정한다. nRF54L15 는 합쳐서 링크 5개(기본 peripheral 4 + central 1),
nRF54L05 는 3개(기본 peripheral 2 + central 1)까지 RAM 이 잡혀 있다.

**페어링 / 본딩** — Just Works, passkey, PIN 에 더해 LE Secure Connections
(P-256, micro-ecc). 키는 **앱 파티션 밖** RRAM 에 저장되므로 펌웨어를 갱신해도
남는다. 재연결 시 CCCD 를 복원한다.

**HID** — `BLEHidAdafruit`. 키보드 / 마우스 / 컨슈머(미디어)키를 하나의 복합
리포트 디스크립터로 제공한다. `BLEHidGamepad` 와, 다른 기기의 키보드·마우스를
읽는 `BLEClientHidAdafruit` 도 있다.

**그 밖의 BLE 서비스** — `BLEMidi`(BLE-MIDI 1.0), `BLEClientCts`(페어링된 폰에서
시각 읽기), `BLEAncs`(iPhone 알림 수신).

**페리페럴** — `Wire`(I2C), `SPI`, `attachInterrupt`, `analogWrite`(PWM),
`analogRead`(SAADC). 다섯 개 모두 실기 검증했다.

⚠ **nRF54L 은 페리페럴이 GPIO 포트에 묶여 있다.** 아무 핀에나 붙던 nRF52 와 다르다.
PWM 과 ADC 는 **P1 에만**, 핀 인터럽트는 P1·P0 에 되고 **P2 에는 안 된다**,
`SPI` 는 P2 에서 돌고 신호마다 가능한 핀이 **두 개씩**이다.
잘못 배정하면 **쓸 수 있는 핀을 알려주며 빌드가 멈춘다.** 칩별·보드별 전체 표는
동봉된 **PinMap** 예제에 있다 — **파일 → 예제 → PinMap**.

⚠ **`analogRead` 의 전압 구성이 nRF52 와 다르다.** 내부 기준전압이 600 mV 가 아니라
**900 mV** 이고, 1/6 게인과 VDD/4 기준이 없다. Adafruit 의 `AR_DEFAULT`(3.6 V)와
`AR_INTERNAL_1_8` 은 정확히 맞지만, `AR_INTERNAL_3_0` 은 실제 3.15 V,
`AR_INTERNAL_2_4` 는 2.25 V, `AR_INTERNAL_1_2` 는 1.35 V 이고 `AR_VDD4` 는 3.6 V 로
대체된다. **`analogReadMillivolts()` 를 쓰면 이 차이에 걸리지 않는다.**

**제3자 라이브러리** — `Adafruit_BusIO` 가 동작하므로 Adafruit 센서 계열이 컴파일된다:
`Adafruit_BME280`, `Adafruit_seesaw`, `Adafruit_GFX` 를 쓰는 `Adafruit_ST7735/ST7789`,
그리고 `SdFat`, Arduino `SD`, `MIDI_Library`, `ArduinoJson`,
`Seeed_Arduino_LSM6DS3`. **어디까지 컴파일만 됐고 어디까지 실기로 확인했는지**는
[docs/LIBRARY-COMPAT.md](docs/LIBRARY-COMPAT.md) 에 구분해 적어 두었다.

**부분 지원**

- **`Wire` 는 master 전용**이다. target(slave) 은 구현하지 않았다.
- **`Servo`** 는 빌드되지 않는다. 라이브러리 자신이 모르는 아키텍처를 `#error` 로
  막는다. 서보는 `analogWrite()` 로 직접 몰 수 있고, 50 Hz 프레임은
  `analogWriteResolution()` 으로 맞춘다.
- **센서 라이브러리가 엉뚱한 버스를 잡을 수 있다.** `Seeed_Arduino_LSM6DS3` 는
  특정 Seeed 보드 매크로에서만 `Wire1` 로 바꾸므로, 이 코어에서는 헤더 쪽 `Wire` 를
  보고 0 을 읽는다. `bme.begin(0x76, &Wire1)` 처럼 **버스를 인자로 받는** 라이브러리를
  쓰는 편이 안전하다.

**예정** — 부트로더와 UART / BLE OTA DFU (M4).

**미지원**

- **ArduinoBLE** — HCI 가 필요한데 SoftDevice 는 HCI 를 노출하지 않는다.
  노력의 문제가 아니라 구조적으로 불가능하다.
- **bit-banging 라이브러리** (NeoPixel, DHT, OneWire, SoftwareSerial 등) —
  SoftDevice 가 최상위 인터럽트 우선순위를 점유하고 라디오 이벤트 중 애플리케이션을
  블로킹한다. PWM + EasyDMA 기반 대안을 쓸 것.
- **USB** — nRF54L15 에 USB 하드웨어가 없어서, 현재 지원하는 칩에서는 USB CDC / UF2 /
  1200bps touch 리셋을 쓸 수 없다. 선택이 아니라 칩의 성질이다.
  **nRF54LM20A 에는 USB(high-speed USBHS)가 있고 M6 에서 지원할 계획**이므로,
  그 시점에 USB 지원을 다시 판단한다.
- **Matter / Thread / Zigbee / LE Audio / 802.15.4** — 범위 밖이다.
  이 코어는 BLE 애플리케이션을 대상으로 한다.

## 구조

```
사용자 스케치 (.ino)
├─ Bluefruit52Lib 호환 API      ← sd_ble_* 위에 재구현
├─ Arduino API                   ← nrfx 위에 구현
├─ SchedulerRTOS                 ← Adafruit rtos.h 와 동일 API
├─ FreeRTOS (tickless, GRTC 틱)
├─ SoftDevice S145 v10.0.1       ← BT 인증
└─ nrfx
```

베이스 SDK 는 [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm)
v2.0.1, 즉 nRF54L 시리즈의 **베어메탈** 옵션이다. Zephyr 는 쓰지 않는다 —
그렇게 정한 근거와, 어떤 조건이 되면 재검토하는지는 [CLAUDE.md § 2](CLAUDE.md) 에 있다.

업로드는 `probe-rs` 를 통한 **CMSIS-DAP + SWD** 다. XIAO 는 프로브가 보드에 있다.
UART / BLE OTA DFU 는 M4 에서 추가하며 SWD 경로는 그대로 남는다.

## 문제 해결

**툴 → 보드 메뉴에 보드가 안 보인다.**
디렉토리 이름이 정확히 `baram-nrf54` 인지, sketchbook 의 `hardware/` 바로 밑에 있는지
확인하라. `arduino-cli board listall | grep nrf54l` 에 보드 3종이 나와야 한다.

**업로드가 `cannot execute upload tool: fork/exec {runtime.tools....}` 로 실패한다.**
`platform.local.txt` 가 없거나 `probers.path` 가 틀린 것이다.
[소스에서 설치](#소스에서-설치--코어를-직접-고칠-때) 참조.

**업로드 직후 시리얼 포트가 사라진다.**
온보드 디버거가 있는 보드에서는 정상이다. 타깃을 리셋하면 USB 장치가 다시 열거되어
기존 포트 핸들이 무효가 된다. 포트를 다시 열면 된다.

**빌드는 되는데 문서에 적힌 크기와 다르다.**
컴파일러 버전이 다를 가능성이 높다. `toolchain.path` 를 반드시 명시하라.
명시하지 않으면 다른 Arduino 패키지가 설치해 둔 툴체인이 조용히 잡힌다.

**`probe-rs` 가 프로브를 못 찾는다.**
`probe-rs list` 에 디버거가 보여야 한다. XIAO 는
`Seeed Studio XIAO nrf54 CMSIS-DAP` 로 잡힌다. DK 계열은 외부 CMSIS-DAP 프로브를
SWD 헤더에 연결해야 한다.

## 기여

이슈와 PR 을 환영한다. 두 가지만 먼저 알아 두면 좋다.

- **[CLAUDE.md](CLAUDE.md) 가 설계 문서다.** 규칙, 이미 확정된 결정, 그리고 가장
  쓸모 있는 것 — **실제로 디버깅 시간을 태운 함정 목록**(`§ 7`)이 들어 있다.
  FreeRTOS·nrfx·빌드 레시피를 건드리기 전에 해당 항목을 읽어라.
- **하드웨어에 대한 주장에는 실측 근거가 필요하다.** 측정은 재현 가능한 수준으로
  [docs/HIL/](docs/HIL/) 에 남긴다. 함정 목록의 여러 항목이 "데이터시트나 주석에
  적힌 내용이 틀려서" 생긴 것이다.

## 라이선스 — 오픈소스, 다만 한 덩어리는 바이너리로 동봉된다

**코어 자체는 MIT** 다. 이 프로젝트를 위해 쓴 코드는 전부 열려 있고 읽고 고치고
재배포할 수 있다.

한 가지가 바이너리로 온다 — Nordic 의 **SoftDevice S145** hex 이미지다. 이건
개방성에 대한 선택이 아니라, **Bluetooth 인증이 바로 그 이미지에 붙어 있어서**
손대면 인증이 무효가 되기 때문이다. Nordic 라이선스는 이 바이너리의 **재배포를
명시적으로 허용**하고, 그 덕에 Board Manager 설치만으로 동작하는 BLE 스택이
따라온다. 조건은 Nordic IC 에서만 돌릴 것, 수정·리버스엔지니어링 금지다.

`adafruit/Adafruit_nRF52_Arduino` 도 수년째 같은 방식이고, Seeed · smartme.io ·
CAMI 포크들도 그렇다.

| 대상 | 라이선스 |
|---|---|
| 코어 자체 코드 | **MIT** ([LICENSE](LICENSE)) |
| 번들 SoftDevice (S145 hex) | **LicenseRef-Nordic-5-Clause** — **Nordic IC 에서만 사용 가능**, 수정·리버스엔지니어링 금지 |
| Arduino API 파일 (`Print`, `Stream`, `WString` 등) | LGPL-2.1 (다른 Arduino 코어들과 동일) |
| nrfx / MDK / CMSIS / FreeRTOS | 구성 요소별 BSD-3-Clause / Apache-2.0 / MIT |

전체 내역: [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md).

이 프로젝트는 Nordic Semiconductor 와 무관하며 후원받지 않았다.

## 문서

| | |
|---|---|
| [CLAUDE.md](CLAUDE.md) | 프로젝트 지침, 알려진 함정, 마일스톤 |
| [docs/STATUS.md](docs/STATUS.md) | 지금 어디까지 됐고 다음에 뭘 하는지 |
| [docs/boards/](docs/boards/) | 보드별 회로도 분석 (보드 하나당 문서 하나) |
| [docs/MEMORY-MAP.md](docs/MEMORY-MAP.md) | 칩별 RRAM / RAM 배치 |
| [docs/PERIPHERAL-PINMAP.md](docs/PERIPHERAL-PINMAP.md) | 페리페럴이 쓸 수 있는 GPIO |
| [docs/LIBRARY-COMPAT.md](docs/LIBRARY-COMPAT.md) | 제3자 라이브러리 — 실제로 빌드해 본 결과 |
| [docs/EXAMPLE-COMPAT.md](docs/EXAMPLE-COMPAT.md) | Adafruit 예제가 얼마나 그대로 빌드되나 |
| [docs/HIL/](docs/HIL/) | 실기 검증 기록 |
| [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md) | 구성 요소별 라이선스 |

## 참고한 것들

- [Adafruit nRF52 Arduino core](https://github.com/adafruit/Adafruit_nRF52_Arduino) — 호환 대상 API 이자 구조 참조
- [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm) — 베어메탈 SDK 와 SoftDevice
- [probe-rs](https://github.com/probe-rs/probe-rs) — 플래시와 디버깅
- [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel), [nrfx](https://github.com/NordicSemiconductor/nrfx)
