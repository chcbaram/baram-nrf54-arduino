# Adafruit 예제 호환 현황

`adafruit/Adafruit_nRF52_Arduino` 의 `Bluefruit52Lib/examples` **71개**를 이 코어로
컴파일해 본 결과다. **무엇을 먼저 만들지 감으로 정하지 않으려고** 재는 것이다.

최종 측정: 2026-09-06 · XIAO nRF54L15 기준

---

## 이식 규칙 — include 세 줄을 지운다

측정할 때 아래 세 줄만 제거하고 나머지는 손대지 않는다.

```cpp
#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
```

**셋 다 include 만 하고 API 는 쓰지 않는다.** 전수 확인했다 —
TinyUSB 를 include 하는 23개 중 API 를 쓰는 것은 **0개**, 파일시스템을
include 하는 6개 중 API 를 쓰는 것도 **0개**다.

Arduino 는 "스케치가 include 한 라이브러리만 링크" 하므로 Adafruit 은 `Serial`
(USB CDC)과 본딩 저장을 끌어오려고 그 줄들을 넣었다. 이 코어는 `Serial` 이 UART 이고
본딩을 RRAM 파티션에 넣으므로 둘 다 필요 없다. 배경: CLAUDE.md §8.1.

> ⚠ 처음에는 TinyUSB 를 include 하는 23개를 "USB 가 없으니 구조적으로 불가능"
> 으로 분류했다. **틀렸다.** `adc` · `hwpwm` · `rtos_scheduler` 처럼 BLE 와
> 무관한 기본 예제들이 거기 묶여 있었다. R10(USB 하드웨어 없음)은 사실이지만
> 그게 이 예제들을 막는 이유는 아니었다.

## 현황

| | 개수 |
|---|---|
| **컴파일 통과** | **16** |
| 우리 BLE API 부족 | 약 16 |
| 우리 M2(Arduino API) 부족 | 약 16 |
| 외부 라이브러리 미설치 | 11 |
| 계 | 71 |

**⚠ 컴파일 통과가 동작을 뜻하지 않는다.** 별표(*)가 실기까지 확인한 것이다.

### 통과 (16)

```
bleuart*  bleuart_multi*  beacon*  eddystone_url*
central_scan*  central_bleuart*
blinky  blinky_ota  rtos_scheduler  SerialEcho
temp_measure_blocking  temp_measure_non_blocking
adv_AdafruitColor  adv_advanced  rssi_callback  rssi_poll
```

실기 확인 내용:
- `bleuart` — `docs/HIL/M3-softdevice.md` §3.9
- `bleuart_multi` — 폰 + Mac 동시 2링크, 양방향 전달 (`docs/STATUS.md` B5)
- `central_scan` — 주변 광고 수신, 주소·RSSI·AD 파싱, 16비트 UUID 필터 실기 확인
- `central_bleuart` — 스캔·연결·GATT 탐색·알림·양방향 데이터. 보드 간, 그리고
  `extras/mac_peripheral.py` 를 상대로 확인. DIS/배터리 클라이언트는 컴파일만 확인
- `beacon` / `eddystone_url` — 광고 페이로드를 bleak 로 스캔해 바이트 단위로 검증.
  iBeacon 은 major/minor 가 **빅엔디안**으로, EddyStone 은 URL 압축 코드가
  규격대로 나가는 것까지 확인했다

### 무엇을 만들면 몇 개가 열리나

| 기능 | 막고 있는 예제 수 | 비고 |
|---|---|---|
| Client / Central 스택 | ~7 | 기본은 끝났다. 남은 것은 `BLEAncs` / `BLEClientCts` / `BLEClientHidAdafruit` 같은 개별 클라이언트 |
| M2 — SPI / Wire / PDM / PWM | ~16 | Arduino API. BLE 와 무관 |
| HID 서비스 | 5 | `BLEHidAdafruit`, `BLEHidGamepad` |
| 본딩 / 페어링 | 3 | `Bluefruit.Security`, `utility/bonding.h` |


### 외부 라이브러리 (우리 책임 아님, 11)

`Adafruit_NeoPixel`(4) · `Adafruit_Arcada`(2) · `Adafruit_GFX` · `MIDI` · `Servo` ·
`SoftwareSerial` · `BLEHomekit`.

⚠ NeoPixel 과 SoftwareSerial 은 **설치하면 컴파일은 되지만 동작하지 않는다.**
SoftDevice 가 최상위 인터럽트를 점유해 bit-banging 이 깨진다 (CLAUDE.md §7 F6).

---

## 다시 재는 법

```sh
# 1. 예제를 받아 include 세 줄을 지운 사본을 만든다
# 2. 각각 컴파일해 "저장 공간" 이 나오면 통과
for d in */; do
  arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 "$d" 2>&1 \
    | grep -q "저장 공간" && echo "PASS $d" || echo "FAIL $d"
done
```

첫 오류만 보면 그 뒤에 숨은 의존을 못 본다. **기능을 하나 추가할 때마다 다시
재는 것**이 정확하다 — 실제로 작은 API 묶음 하나로 7 → 12 가 됐다.
