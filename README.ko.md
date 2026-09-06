# BARAM nRF54L Arduino Core

**nRF54L 시리즈용 Arduino 코어 — 기존 Adafruit Bluefruit(nRF52) 스케치가 그대로
동작하는 것을 목표로 만든다.**

*[English](README.md) · [한국어](README.ko.md)*

[![License: MIT](https://img.shields.io/badge/core-MIT-blue.svg)](LICENSE)
[![SoftDevice](https://img.shields.io/badge/SoftDevice-S145%20v10.0.1-orange.svg)](docs/LICENSE-INVENTORY.md)
[![Status](https://img.shields.io/badge/status-M1%20(pre--release)-yellow.svg)](docs/STATUS.md)

> ### ⚠ 초기 릴리스 — v0.1.0
> blink / `Serial` / 멀티태스킹 / tickless idle 까지 **보드 3종에서 실기 동작**하고
> Board Manager 로 설치된다. **BLE 는 아직 구현되지 않았다**(M3).
> `analogRead` / `Wire` / `SPI` 는 M2 다.
> 진행 상황: [docs/STATUS.md](docs/STATUS.md)

---

## 목차

- [왜 만드는가](#왜-만드는가)
- [특징](#특징)
- [지원 보드](#지원-보드)
- [설치](#설치)
- [첫 스케치](#첫-스케치)
- [지원 범위](#지원-범위)
- [구조](#구조)
- [문제 해결](#문제-해결)
- [기여](#기여)
- [라이선스](#라이선스--혼재-라이선스이며-오픈소스가-아니다)

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
| [probe-rs](https://github.com/probe-rs/probe-rs/releases/tag/v0.32.0) | **0.32.0** | macOS 바이너리는 `nrf54l/tools/` 에 동봉돼 있다 |

경로는 예시 파일을 복사해서 적는다:

```sh
cp nrf54l/platform.local.txt.example nrf54l/platform.local.txt
```

빠뜨리면 어떻게 되는지까지 포함한 전체 절차:
[docs/STATUS.md § 다른 PC에서 이어서 작업하기](docs/STATUS.md).

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

## 지원 범위

**지원** — GPIO, `millis()` / `micros()` / `delay()`, `Serial`, `SchedulerRTOS`,
tickless idle 을 켠 FreeRTOS.

**예정** — `analogRead` / `analogWrite`, `Wire`, `SPI`, `attachInterrupt` (M2),
BLE (M3), 부트로더와 UART / BLE OTA DFU (M4).

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

## 라이선스 — **혼재 라이선스이며 "오픈소스"가 아니다**

번들된 SoftDevice 가 OSI 정의를 충족하지 않으므로, 프로젝트 전체를 오픈소스라고
말할 수 없다.

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
| [docs/HIL/](docs/HIL/) | 실기 검증 기록 |
| [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md) | 구성 요소별 라이선스 |

## 참고한 것들

- [Adafruit nRF52 Arduino core](https://github.com/adafruit/Adafruit_nRF52_Arduino) — 호환 대상 API 이자 구조 참조
- [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm) — 베어메탈 SDK 와 SoftDevice
- [probe-rs](https://github.com/probe-rs/probe-rs) — 플래시와 디버깅
- [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel), [nrfx](https://github.com/NordicSemiconductor/nrfx)
