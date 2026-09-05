# baram-nrf54-arduino

nRF54L 시리즈용 Arduino 코어. Nordic **SoftDevice + FreeRTOS** 기반이며
**Adafruit Bluefruit(nRF52) API 호환**을 목표로 한다.

> **상태: 개발 착수 단계 (M0 완료, M1 진행 전).**
> 아직 설치 가능한 릴리스가 없다. 설계와 조사 결과는 [CLAUDE.md](CLAUDE.md)에 정리돼 있다.

## 지원 보드

| 보드 | MCU | 문서 |
|---|---|---|
| **NU54-DK** | nRF54L15 (128MHz Cortex-M33, 1.5MB RRAM, 256KB RAM) | [docs/NU54-DK.md](docs/NU54-DK.md) |

## 구성

```
사용자 스케치 (.ino)
├─ Bluefruit52Lib 호환 API      ← sd_ble_* 위에 재구현
├─ Arduino API                   ← nrfx 위에 구현
├─ SchedulerRTOS                 ← Adafruit rtos.h와 동일 API
├─ FreeRTOS (tickless, GRTC 틱)
├─ SoftDevice S145 v10.0.1       ← BT 인증
└─ nrfx
```

베이스 SDK는 [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm) v2.0.1
(nRF Connect SDK **Bare Metal**)이다. Zephyr RTOS는 쓰지 않는다.

## 업로드

**CMSIS-DAP 프로브 + SWD**. probe-rs 바이너리를 코어에 동봉하므로 **추가 설치가 필요 없다.**
NU54-DK에는 온보드 프로브가 없으므로 외부 CMSIS-DAP 장비를 J3(ARM 10핀 1.27mm)에 연결한다.
UART DFU 부트로더는 이후 마일스톤에서 추가하며, 그때도 SWD 경로는 유지된다.

## 지원 범위

정해지는 대로 `지원` / `부분 지원` / `미지원` 3단계로 여기에 기록한다.

**미지원이 확정된 것:**

- **USB 관련 기능 전부** — nRF54L15에 USB 하드웨어가 없다. USB CDC·UF2·1200bps touch 불가
- **ArduinoBLE** — HCI가 필요한데 SoftDevice는 HCI를 노출하지 않는다. 구조적으로 불가능
- **bit-banging 라이브러리** (NeoPixel, DHT, OneWire, SoftwareSerial 등) — SoftDevice가 최상위
  인터럽트 우선순위를 점유하고 라디오 이벤트 중 애플리케이션을 블로킹한다. PWM+EasyDMA 기반 대안을 쓸 것

## 라이선스 — **혼재 라이선스**

**"오픈소스"가 아니다.** 번들된 SoftDevice가 OSI 정의를 충족하지 않는다.

| 대상 | 라이선스 |
|---|---|
| 코어 자체 코드 | **MIT** ([LICENSE](LICENSE)) |
| 번들 SoftDevice (S145 hex) | **LicenseRef-Nordic-5-Clause** — **Nordic IC에서만 사용 가능**, 바이너리 수정·리버스엔지니어링 금지 |
| nrfx / MDK / CMSIS / FreeRTOS 등 | BSD-3-Clause / Apache-2.0 / MIT (구성 요소별 상이) |

전체 내역: [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md)

이 프로젝트는 Nordic Semiconductor와 무관하며 후원받지 않았다.

## 문서

- [CLAUDE.md](CLAUDE.md) — 프로젝트 지침. 절대 규칙, 알려진 함정, 마일스톤
- [docs/NU54-DK.md](docs/NU54-DK.md) — 보드 핀맵·회로 분석
- [docs/MEMORY-MAP.md](docs/MEMORY-MAP.md) — RRAM / RAM 배치
- [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md) — 라이선스 내역
