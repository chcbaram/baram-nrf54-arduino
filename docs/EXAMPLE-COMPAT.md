# Adafruit 예제 호환 현황

`adafruit/Adafruit_nRF52_Arduino` 의 `Bluefruit52Lib/examples` **71개 전부**를 이
코어로 컴파일해 본 결과다. **무엇을 먼저 만들지 감으로 정하지 않으려고** 재는 것이다.

⚠ **분모를 미리 깎지 않는다.** 71 개를 다 세어 두고, 하나씩 실제로 확인한 뒤에만
"세는 대상에서 뺀다". 한 번 일괄로 16개를 뺐다가 되돌린 적이 있는데, 그때
`Bluefruit.configUuid` · `getAddr` · `configAttrTableSize` 세 API 가 **제외된 예제에만
있어서 조용히 시야에서 사라졌다.** 제외 기준은 아래 §제외 규칙에 적어 둔다.

⚠ **외부 라이브러리가 필요하면 설치해서 실제로 재라.** "라이브러리가 없어서 못 잰다"
와 "우리 API 가 없다" 는 완전히 다른데, 안 재면 구분이 안 된다. 실제로 설치해 보니
`blemidi` 는 라이브러리 문제가 아니라 **우리에게 `BLEMidi` 가 없는 것**이었고,
`neopixel` 은 **우리 코어에 `interrupts()`/`noInterrupts()` 가 없던 것**이었다.

최종 측정: 2026-09-07 · XIAO nRF54L15 기준

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

## 제외 규칙 — 하나씩 확인한 뒤에만 뺀다

예제를 세는 대상에서 빼려면 **둘 다** 만족해야 한다.

1. 외부 기기나 보드 고유 하드웨어에 묶여 있어, 빌드 여부가 우리 BLE 지원을
   말해 주지 않는다
2. **그 예제가 쓰는 BLE API 가 남는 예제에도 있다** — 즉 빼도 시야에서 사라지는
   API 가 없다

2번이 핵심이다. 일괄로 뺐을 때 이걸 안 봐서 `configUuid` · `getAddr` ·
`configAttrTableSize` 가 통째로 사라졌다. 뺄 때는 그 예제의 BLE 클래스·호출을
뽑아 남는 예제와 대조하라:

```sh
grep -ohE '\b(BLE[A-Z][A-Za-z]*|Bluefruit\.[A-Za-z]+)' <예제>.ino | sort -u
```

⚠ 다음 셋은 우리 힘으로 안 되는 것이 확실하다. 다만 **분모에는 남겨** 둔다 —
빼면 "왜 없지?" 를 다시 조사하게 된다.
- `neopixel` `neomatrix` `gpstest_swuart` — bit-banging. SoftDevice 가 최상위
  인터럽트를 점유해 **동작이** 구조적으로 깨진다 (CLAUDE.md §7 F6).
  단 **컴파일은 된다** — `neopixel` 은 실제로 통과한다

⚠ **`nfc_to_gpio` 는 "구조적 불가" 가 아니라 그냥 미구현이다.**
한때 "nRF54L 에 NFC 하드웨어가 없다" 고 적었는데 **틀렸다.** L15 와 L05 **양쪽 다
NFCT 가 있다** — `NRF_NFCT_NS_BASE = 0x400D6000`, `NFCT_IRQn = 214`, 핀은
NFC1/NFC2 = P1.02/P1.03 (`docs/PERIPHERAL-PINMAP.md`). nrfx 에 NFCT 드라이버도 있다.
막고 있는 것은 **우리가 NFC 드라이버를 안 만든 것**뿐이다.

> ⚠ 이 오류는 **우리 저장소 안에 이미 반증이 있는데도** 확인하지 않고 단정해서
> 생겼다 (`nrf54l_domains.h` 가 NFCT 를 P1 도메인에 적어 두고 있다).
> "칩에 없다" 는 강한 주장이다. 하기 전에 MDK 헤더를 열어라.

⚠ USB 는 다르다. **nRF54L15 에는 USB 하드웨어가 정말 없다** — 이건 맞다
(nRF54LM20A 에는 있고 M6 에서 본다).

⚠ 외부 기기가 필요해도 **BLE 자체를 검증하는** 예제는 당연히 남는다:
`central_*` · `dual_bleuart` · `rssi_proximity_*` (보드 2대), `pairing_*` ·
`blehid_*` · `ancs` · `client_cts` (폰만 있으면 된다), `throughput` (처리량 실측).

⚠ `Hardware/` 의 `adc` · `hwpwm` · `Fading` 등은 BLE 와 무관하지만
**우리 M2(Arduino API) 목표에는 해당**한다. 아래 표에서 따로 센다.

## 외부 라이브러리를 설치하고 재 본 결과

설치: `Adafruit NeoPixel` · `Adafruit GFX` · `Adafruit SSD1306` · `Adafruit BusIO` ·
`MIDI Library` · `Servo`.

| 예제 | 결과 | 진짜 원인 |
|---|---|---|
| `neopixel` | ✅ 컴파일 통과 | **우리 코어에 `interrupts()`/`noInterrupts()` 가 없었다.** 추가함. (동작은 여전히 bit-banging 이라 안 된다) |
| `neomatrix` | ❌ `Wire.h` 없음 | 우리 M2 (`Wire` 미구현) |
| `ancs_oled` | ❌ `Wire.h` 없음 | 우리 M2 |
| `client_cts_oled` | ❌ `Wire.h` 없음 | 우리 M2 |
| `blemidi` | ❌ `BLEMidi` 없음 | **우리 BLE API 부족.** MIDI 라이브러리는 잘 설치됐다 |

**둘이 재분류됐다.** `blemidi` 는 "외부 라이브러리 미설치" 가 아니라 우리 BLE API
부족이었고, `neopixel` 은 우리 코어 API 부족이었다. 안 재봤으면 둘 다 남의 탓으로
남아 있었을 것이다.

## 현황

| | 개수 |
|---|---|
| **컴파일 통과** | **25** |
| 우리 BLE API 부족 | 약 12 |
| 우리 M2(Arduino API) 부족 | 약 20 |
| 외부 라이브러리·외부 기기 | 약 14 |
| 계 | 71 |

⚠ 아래 세 항목은 **아직 전수로 다시 재지 않은 어림값**이다. 통과 개수만 실측이다.
기능을 추가할 때마다 전수로 다시 재는 것이 정확하다 (맨 아래 절).

**⚠ 컴파일 통과가 동작을 뜻하지 않는다.** 별표(*)가 실기까지 확인한 것이다.

### 통과 (25)

```
bleuart*  bleuart_multi*  beacon*  eddystone_url*
central_scan*  central_bleuart*  neopixel(컴파일만 — 동작 불가)
pairing_pin  pairing_passkey  clearbonds  central_pairing
blehid_keyboard  blehid_mouse  blehid_camerashutter  blehid_keyscan
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
⚠ HID 계열 4개도 **컴파일만 확인했다.** 그 이유가 특별하다 —
**Apple 이 HID 서비스(0x1812)를 앱에 안 보여준다.** 시스템이 직접 처리하는
프로파일이라 CoreBluetooth 가 걸러내므로, bleak 으로는 GATT 계층조차 못 본다.
확인하려면 호스트의 Bluetooth 설정에서 직접 페어링하고 키를 눌러 보는 수밖에 없다
(그래서 `examples/Peripheral/blehid_button` 을 만들었다 — 버튼 하나에 키 하나).

⚠ 페어링 계열 4개(`pairing_pin` · `pairing_passkey` · `clearbonds` ·
`central_pairing`)도 **컴파일만 확인했다.** 페어링·본딩 동작 자체는 별도 시험
스케치로 Mac 을 상대로 실증했지만(`docs/STATUS.md` B10·B11), 그건 이 예제들을
구워서 돌린 것이 아니다. 별표는 예제를 실제로 돌린 것에만 붙인다.
- `beacon` / `eddystone_url` — 광고 페이로드를 bleak 로 스캔해 바이트 단위로 검증.
  iBeacon 은 major/minor 가 **빅엔디안**으로, EddyStone 은 URL 압축 코드가
  규격대로 나가는 것까지 확인했다

### 무엇을 만들면 몇 개가 열리나

| 기능 | 막고 있는 예제 수 | 비고 |
|---|---|---|
| Client / Central 스택 | ~7 | 기본은 끝났다. 남은 것은 `BLEAncs` / `BLEClientCts` / `BLEClientHidAdafruit` 같은 개별 클라이언트 |
| M2 — SPI / Wire / PDM / PWM | ~16 | Arduino API. BLE 와 무관 |
| HID — 게임패드 / 클라이언트 | 2 | `BLEHidGamepad`(+`hid_gamepad_report_t`), `BLEClientHidAdafruit` |



### 아직 설치해 보지 않은 외부 라이브러리

`Adafruit_Arcada` (의존 트리가 크다) · `SdFat` · `PDM` ·
`Adafruit_CircuitPlayground` · `APDS9960` · `Firmata` · `SoftwareSerial` ·
`BLEHomekit` (레지스트리에 없음) · TFLite.

⚠ 이것들도 **설치해서 재 봐야** 원인이 우리 쪽인지 갈린다. 위에서 `blemidi` 와
`neopixel` 이 그렇게 뒤집혔다.

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

⚠ **분모는 71 이다.** 미리 깎지 않는다. 빼려면 §제외 규칙의 두 조건을 모두
확인하고, 뺀 이유를 그때 이 문서에 적는다.
