# 진행 상황 / 다음 세션 인수인계

최종 갱신: 2026-09-06 · 커밋 `b8cc273`

프로젝트 지침과 설계 결정은 [CLAUDE.md](../CLAUDE.md) 가 정본이다.
이 문서는 **"지금 어디까지 됐고 다음에 뭘 하면 되는지"** 만 짧게 적는다.

---

## 1. 지금 동작하는 것 (M1 거의 완료 · M3 BLE peripheral 동작)

실기 보드 **NU54-DK / nRF54L05** 에서 확인:

| 항목 | 상태 |
|---|---|
| FreeRTOS (ARM_CM33_NTZ) + GRTC 틱 | ✅ 1000 tick/s |
| `setup()`/`loop()` + `Scheduler.startLoop()` 두 번째 태스크 | ✅ |
| GPIO (LED 4 / 버튼 4, 내부 풀업) | ✅ |
| `millis()` / `micros()` / `delay()` | ✅ 델타 정확 |
| `Serial` (UARTE30 → CP2102N) | ✅ 흐름제어 없음 |
| **tickless idle** | ✅ 틱 vs SYSCOUNTER 0 ppm, 5분 소크 이상 0건 |
| LFXO 클럭 정확도 | ✅ 호스트 대비 +25~38 ppm |
| arduino-cli / Arduino IDE 컴파일·업로드 | ✅ probe-rs, CMSIS-DAP |
| 보드 3종 | ✅ NU54-DK / NU54V-DK / XIAO nRF54L15 |

**XIAO nRF54L15 실기 확인 (2026-09-06)** — `docs/HIL/M1-xiao.md`:
온보드 CMSIS-DAP 업로드 1.9초, LED 점멸(active LOW) 및 `Serial`(UARTE20) 정상,
틱 vs SYSCOUNTER **0.0 ppm**, 호스트 대비 **-14 ppm**, 180초 90샘플 이상 0건.
LFXO 내부 로드 캡을 잡기 전에는 **+805 ppm** 이었다 (§ 아래 4-7).

```
FQBN  baram-nrf54:nrf54l:nu54dk          NU54-DK    (nRF54L05, 500KB/96KB)
      baram-nrf54:nrf54l:nu54vdk         NU54V-DK   (nRF54L15, 1.5MB/256KB)
      baram-nrf54:nrf54l:xiao_nrf54l15   XIAO       (nRF54L15, 온보드 CMSIS-DAP)
```

빌드 크기(blink + Serial + 2태스크): Flash 38004 B, RAM 3856 B.

### BLE (M3) — peripheral 역할 실기 확인

XIAO nRF54L15 + Mac(bleak) / 폰(nRF Connect) 로 확인한 것:

| 항목 | 상태 |
|---|---|
| SoftDevice S145 활성화 + 이벤트 펌프 | ✅ |
| advertising / GATT 서버 / 커스텀 서비스 | ✅ |
| `BLEUart`(NUS) · `BLEDis` · `BLEBas` | ✅ |
| ATT MTU 협상 | ✅ 247 |
| **동시 연결** | ✅ L15 4개 / L05 2개, 폰+Mac 2링크 양방향 실증 |
| `BLEBeacon`(iBeacon) · `EddyStoneUrl` | ✅ 광고 바이트 단위 검증 |
| `getPeerName()` (GATT 클라이언트) | ✅ `Connected to Mac` |
| tickless idle 과 BLE 동시 동작 | ✅ 틱 vs SYSCOUNTER 0.0 ppm |

**없는 것:** central 역할(스캔·연결), 본딩/페어링, HID, 실제 DFU.
예제 호환 현황은 `docs/EXAMPLE-COMPAT.md` (71개 중 14개 통과).

---

## 2. 바로 다음에 할 일

### (a) 전류 측정 — M1 을 닫으려면 이게 남았다

tickless 를 켠 목적이 전력인데 아직 재지 못했다. **SWD 프로브를 물리적으로
분리하고** 재야 한다 (§7 F8 — 붙어 있으면 수치가 안 나온다).

측정 4종 (§4.6):
1. `delay(1000)` 루프 + `Serial` 켠 상태
2. 같은 조건에서 `Serial.end()` 후 (UARTE 가 바닥 전류를 올리는지)
3. `systemOff()` 후 — **`systemOff()` 는 아직 구현 안 됨**
4. (M3) advertising 중

기준선은 Nordic `ble_pwr_profiling` 샘플을 같은 보드에 구워서 잡는다.
이 비교 없이는 "이 정도면 괜찮은가"를 판단할 근거가 없다.

같이 판단할 것: `port_grtc.c` 의 `nrfx_grtc_active_request_set(true)` 를 빼도
되는지. `MODE.AUTOEN` 이 이미 0 이라 중복일 가능성이 높고, 뺐을 때 전력이
내려가면 빼면 된다. 기능적으로는 없어도 틱이 정확하다 (확인됨).

### (b) 앱 레벨 저전력 API (§4.5)

`waitForEvent()`, `systemOff(pin, wake_logic)`, `readResetReason()`.
`readResetReason()` 은 `NRF_RESET` 이다 (`NRF_POWER->RESETREAS` 아님, `NRF_RESETINFO` 도 아님).

### (c) 릴리스 파이프라인 — ✅ **0.1.0 배포 완료 (2026-09-06)**

깨끗한 환경에서 Board Manager 로 설치 → 컴파일 → **실기 업로드·동작까지 확인했다**
(M5 DoD 중 macOS/arm64 부분). Linux / Windows 는 자산은 올라갔지만 미검증이다.

재배포할 때는 아래 순서다. **저장소의 인덱스에는 업로드 안 한 것을 넣지 않는다** —
아카이브 바이트가 매번 달라 체크섬과 파일이 같은 실행에서 나와야 한다.

```sh
extras/make_tools.sh              # probe-rs 재포장 + 업로드 (버전 바뀔 때만)
extras/make_release.sh 0.1.0      # 플랫폼 아카이브 + 업로드
git add package_baram_nrf54_index.json nrf54l/platform.txt && git commit && git push
```

| 항목 | 값 |
|---|---|
| packager (FQBN 앞부분) | **`baram-nrf54`** |
| Board Manager URL | `https://raw.githubusercontent.com/chcbaram/baram-nrf54-arduino/main/package_baram_nrf54_index.json` |
| 플랫폼 아카이브 | 1.7 MB (`tools/` 제외) |
| 릴리스 태그 | `0.1.0` (플랫폼) / `probe-rs-0.32.0` (툴) |
| 툴 — GCC | xPack `14.2.1-1.1`. **업스트림 URL 을 그대로 가리킨다** (재호스팅 안 함) |
| 툴 — probe-rs | `0.32.0`. 업스트림 레이아웃이 `platform.txt` 의 `{...}/bin` 과 안 맞아 **재포장**해서 이 저장소 릴리스에 올린다 |

주의할 것 두 가지:

- **`boards` 목록은 `boards.txt` 에서 자동으로 읽는다.** 보드를 추가해도 스크립트를
  고칠 필요가 없다
- **Linux probe-rs 는 스트립이 필요하다.** 업스트림이 비스트립본(136 MB, 그중 94 MB 가
  DWARF)을 배포한다. `make_tools.sh` 는 ELF 를 다룰 수 있는 strip 이 있을 때만
  스트립하고 없으면 경고한다 → **macOS 에서 돌리지 말고 Linux 나 `brew install llvm`
  환경에서 돌려라.** 스트립하면 136 MB → 42 MB 다

### (d) M3 A단계 — ✅ **끝났다. F9 가 풀렸다 (2026-09-06)**

SoftDevice S145 가 뜨고 advertising 이 공중에서 잡히며 연결까지 성립한다.
**그 상태에서 틱 vs SYSCOUNTER 0.0 ppm** — §7 F9(BASEPRI/PRIMASK 분리)가
라디오와 공존한다는 것이 실측으로 확인됐다. 이 프로젝트 최대의 미지수였다.
기록: `docs/HIL/M3-softdevice.md`.

이 과정에서 코어가 두 군데 바뀌었다. **둘 다 M1 회귀 없음을 확인했다.**

| 바뀐 것 | 왜 |
|---|---|
| `NRFX_GRTC_CONFIG_AUTOEN` 0 → **1** | SoftDevice 요구사항. 안 켜면 `0x1003` 으로 거부당한다 |
| GRTC `CLKSEL` LFXO → **SystemLFCLK** | SoftDevice 가 LFCLK 를 관리한다는 전제와 맞춘다 |

### B 단계 — Bluefruit API 계층 (진행 중)

**목표는 M3 DoD**: Adafruit `Bluefruit52Lib/examples/Peripheral/bleuart` 원본이
**무수정으로** 컴파일·동작하는 것.

규모를 먼저 알아 둘 것: Bluefruit52Lib 본체만 **약 250 KB / 30여 파일**이고
`services/`(BLEUart·BLEDis·BLEBas·BLEDfu)와 파일시스템 2종이 더 붙는다.
한 번에 끝나지 않으므로 아래처럼 쪼갠다. **각 단계마다 bleak 으로 실증한다.**

| 단계 | 내용 | 검증 |
|---|---|---|
| ~~**B1**~~ ✅ | `BLEUuid` / `BLEService` / `BLECharacteristic` + 최소 `Bluefruit` 싱글턴 | **완료.** 탐색·읽기·알림·쓰기 전부 실증 |
| ~~**B2**~~ ✅ | `BLEUart` (NUS) | **완료.** 18바이트 에코 왕복 일치 |
| ~~**B3**~~ ✅ | MTU 협상(247), `BLEConnection`, `BLEDis`, `BLEBas`, `autoConnLed` 등 | **완료.** Adafruit `bleuart` 예제가 API 호출 그대로 동작 |
| ~~**B4**~~ ✅ | `BLEDfu` 스텁, 파일시스템 안내 헤더, `bluefruit.h` 가 서비스 포함 | **완료. Adafruit 원본 `bleuart.ino` 가 include 2줄 삭제만으로 동작 = M3 DoD** |
| ~~**B5**~~ ✅ | **다중 연결** (nRF54L15 4개) + notify 큐 설정 | **완료.** 폰 + Mac 동시 2링크 실증 |
| ~~**B6**~~ ✅ | **Beacon** — `BLEBeacon`(iBeacon) + `EddyStoneUrl` | **완료.** 광고 페이로드 실측 검증 |
| ~~**B7**~~ ✅ | **GATT 클라이언트** + 콜백 지연 실행 -> `getPeerName()` | **완료.** `Connected to Mac` 실증 |
| B8 (진행 중) | **central 역할** — RAM 배분 확정, 스캔/연결 구현 전 | |
| B9 (남음) | 본딩/`BLESecurity`, HID | |

지금 위치: **M3 DoD 달성.** Adafruit 원본 `bleuart.ino` 가
`#include <Adafruit_LittleFS.h>` / `<InternalFileSystem.h>` **두 줄 삭제만으로**
컴파일·동작한다 (`~/Documents/Arduino/bleuart_orig/`).
서비스 4종·DIS·배터리·UART·MTU 247 전부 확인.
DoD 문구를 그렇게 바꾼 근거(예제 71개 전수 조사)는 CLAUDE.md §8.1.

**아직 없는 것:**

- **본딩 / 페어링** (`BLESecurity`). pairing 계열 예제는 아직 안 된다.
  키는 `peer_manager` 4 KB 파티션에 고정 레코드로 넣을 계획이다 (§8.1)
- **central 역할** — peripheral 전용 구성이다. 스캔·연결과 `BLEClient*` 계열이
  없다. GATT **클라이언트**는 B7 에서 생겼으므로(읽기 경로) 그 위에 얹으면 된다
- **HID 서비스** — 본딩에 의존한다. HID over GATT 는 보통 암호화된 링크를
  요구해서, 본딩 없이 만들면 폰이 붙어도 동작하지 않는다
- **실제 DFU** — `BLEDfu` 는 서비스만 등록하고 명확히 거절한다. M4 에서 연결

시험 스케치는 `~/Documents/Arduino/nrf54_ble_gatt/` (GATT),
`~/Documents/Arduino/nrf54_bleuart/` (NUS).

⚠ MTU 를 키울 때 걸린 것 네 가지는 `docs/HIL/M3-softdevice.md` §3.7 에 있다.
특히 `ble_gap_cfg_role_count_t.adv_set_count` 는 구조체 첫 필드라
`memset` 뒤에 빠뜨리기 쉽고, 증상이 "BLE 를 못 켠다" 로만 보인다.

⚠ B1 에서 잡은 것: **`DATA_LENGTH_UPDATE_REQUEST` 에 답하지 않으면 연결은
유지되는데 ATT 가 전혀 흐르지 않는다.** 자세한 건 `docs/HIL/M3-softdevice.md` §3.5.

**파일시스템은 B4 전까지 필요 없다.** 예제의 `#include <Adafruit_LittleFS.h>` 는
본딩 저장 때문이고, RRAM 에는 erase 가 없어 그대로 못 올린다.
배경과 권장 경로는 CLAUDE.md §8.1.

### B5 — 다중 연결 ✅ (2026-09-06 완료)

**실기 결과** (XIAO nRF54L15, 폰(nRF Connect) + Mac(bleak) 동시 연결):

```
Connected, handle 4, total 2        <- 폰 handle 1, Mac handle 4
Keep advertising                     <- 2/4 라 광고 계속
[1] test2                            <- 폰 -> 보드
[1] test3
Disconnected, handle 4, reason 0x13, left 1   <- 폰 링크는 살아 있다
```
```
1.5s  mac link up, mtu 247
5.5s  *** mac RX <- b'test2'         <- 보드 -> Mac (링크 간 전달)
```

확인된 것: 동시 2링크, 링크별 핸들 태깅, **양방향 링크 간 전달**,
**연결 중 광고 유지**(연결을 쥔 채 다시 스캔해서 잡히는 것으로 확인),
한쪽만 끊어도 나머지 생존, MTU 247.

#### ⚠ 연결 핸들은 슬롯 번호가 아니다 — 여기서 한 번 물렸다

**링크 수를 4 로 두고도 SoftDevice 가 핸들 4 를 줬다.** 핸들은 슬롯 인덱스가 아니라
연결마다 새로 매기는 번호에 가깝고, 설정한 링크 수보다 커질 수 있다.

처음엔 Adafruit 처럼 `_connection[conn_hdl]` 로 핸들을 배열 인덱스로 썼다.
Adafruit 이 이걸로 안 터지는 이유는 `BLE_MAX_CONNECTION` 을 설정된 링크 수가 아니라
**20**(SoftDevice 최대)으로 고정해 배열을 넉넉히 두기 때문이다. 그 상수를
"동시 연결 수" 로 읽은 게 잘못이었다.

증상: Mac 이 붙자마자 `reason 0x16`(LOCAL_HOST_TERMINATED)으로 튕겼다.
범위 밖 핸들을 **무시하지 않고 끊도록** 짜 뒀기 때문에 즉시 드러났다.
무시했으면 "가끔 연결은 되는데 데이터가 안 온다" 로 나타났을 것이다.
링크를 5 -> 4 로 줄이면서 드러났고, 5 였으면 핸들 4 가 우연히 통과해 더 오래 숨었다.

**고친 방식:** 핸들 -> 슬롯 매핑(`_slotOf()`, 선형 탐색). 슬롯은 링크 수만큼만 둔다.
공개 API 는 그대로 핸들을 받으므로 스케치 쪽은 차이가 없다.

⚠ 그래서 **연결을 훑을 때 핸들 값으로 반복하면 안 된다.** `connHandleAt(i)` 로
슬롯을 돈다. Adafruit `bleuart_multi` 의 `for (conn_hdl = 0; conn_hdl < MAX; ...)` 는
같은 결함이 있다 — 핸들이 낮게 유지되는 동안만 우연히 동작한다.

**최종 구성 (nRF54L15): 링크 4개 + notify 큐 3, SD 예약 28 KB.**
링크 5개도 RAM 은 되지만 그러면 큐를 1 에서 못 올리고, 큐 1 은 연결 이벤트당
notify 1건이라 처리량이 크게 깎인다. 연결 수보다 처리량을 골랐다 —
Adafruit `BANDWIDTH_MAX` 와 같은 조합이다. 실측표는 `docs/MEMORY-MAP.md`.

**nRF54L05 도 실측했다 (2026-09-06, NU54-DK 실기): 링크 2개 + 큐 3.**
필요량 `0x20004AC8`, 예약을 `0x4780` → `0x4B80` 으로 올려 여유 184 B.
앱 RAM 은 80,000 → 78,976 B (−1,024 B). 링크 3개는 `0x5668` 이라 96 KB 로는 안 된다.

⚠ **L05 와 L15 의 SoftDevice RAM 요구량이 완전히 같았다.** SoC 별로 재배치된
별도 SD 빌드인데도 그렇다 — 요구량은 설정(링크 수 · MTU · 큐 깊이)만 따른다.
그래서 한쪽에서 잰 값을 다른 쪽에 그대로 쓸 수 있다. 다만 그걸 **모르는 상태에서
추정으로 넘어가지는 않았고**, 여유가 168 B 뿐이라 실측 전에는 올리지 않았다.

**남은 것:** 없다. 링크 3개 이상 동시 연결은 호스트가 모자라 못 해 봤지만,
슬롯 관리가 개수와 무관하므로 2개에서 검증된 경로와 같다.

---

#### (아래는 착수 전에 정리한 설계 메모 — 결과와 함께 남겨 둔다)

**끝난 것: RAM.** nRF54L15 의 SoftDevice 예약을 26 KB → **30 KB(`0x20007800`)** 로
넓혔다 (커밋 `7e4e2d5`). 실기에서 `SD_BLE_PERIPH_LINK_COUNT=5` + MTU 247 로
`sd_ble_enable()` 이 성공하고 요구치가 `0x20007590` (여유 624 B) 인 것을 확인했다.
연결당 실측 ~3980 B. 측정 표는 `docs/MEMORY-MAP.md`.

**아직 안 된 것: 라이브러리.** 그래서 `SD_BLE_PERIPH_LINK_COUNT` 는 **1 로 되돌려 뒀다.**
`AdafruitBluefruit` 이 연결 하나만 추적하므로 링크 수만 올리면 두 번째 연결이
SoftDevice 에서는 맺어지는데 라이브러리가 관리하지 못한다.
`begin()` 은 지금도 `prph_count != 1` 이면 **false 를 돌려준다** (조용히 깎지 않는다).

#### 상류(Adafruit) 실제 동작 — 추측하지 말 것

구현 전에 upstream 소스를 받아 확인한 결과다. 세 가지가 처음 예상과 달랐다:

1. **`BLEUart` 의 RX FIFO 는 연결별이 아니라 하나를 공유한다.**
   `available()` / `read()` 는 어느 연결에서 왔는지 구분하지 않는다.
   `bleuart_multi.ino` 도 받은 것을 모든 연결에 그대로 되뿌린다.
   → **우리도 공유 FIFO 로 간다.** 연결별로 나누면 무핸들 `available()` 이
   어느 쪽을 봐야 할지 정의할 수 없고, 상류 예제의 동작이 달라진다 (R12).
   섞이는 문제는 상류의 한계 그대로이므로 주석으로 남긴다.
2. **핸들 없는 `write()` / `notify()` 는 "모든 연결" 이 아니라 `connHandle()` 한 곳으로 간다.**
   `BLEUart::write(buf,len)` = `write(Bluefruit.connHandle(), buf, len)`.
   전체에 보내는 것은 **스케치가** `for` 로 돈다.
3. **연결 유지 중 advertising 재시작도 라이브러리가 아니라 스케치가 한다.**
   `connect_callback` 안에서 `if (count < MAX) Advertising.start(0)`.
   → 라이브러리에서 자동 재시작을 넣지 마라. 상류와 동작이 달라진다.

또 하나: 상류는 `_connection[conn_hdl]` 로 **핸들을 배열 인덱스로 그대로 쓴다.**
SoftDevice 가 핸들을 `[0, 링크수)` 로 준다는 전제다. 우리도 같이 가되
`conn_hdl >= BLE_MAX_CONNECTION` 이면 NULL 을 돌려주는 경계 검사는 넣는다.

받아 둔 상류 소스: `bluefruit.h`, `BLEPeriph.cpp`, `services/BLEUart.{h,cpp}`,
`BLECharacteristic.h`, `examples/Peripheral/bleuart_multi/bleuart_multi.ino`.
(스크래치패드에만 있다 — 저장소에는 넣지 않는다.)

#### 구현 순서

1. **`SD_BLE_PERIPH_LINK_COUNT` 를 보드별 설정으로 뺀다.**
   `sd_event_pump.c` 의 `#ifndef` 블록을 **`sd_event_pump.h` 로 옮겨** 라이브러리에서도
   보이게 하고, `boards.txt` 의 `build.extra_flags` 에 `-DSD_BLE_PERIPH_LINK_COUNT=N` 을 준다.
   `build.extra_flags` 는 core/라이브러리/스케치 recipe 에 모두 들어가므로 한 곳에서 통한다.
   값: `nu54vdk`/`xiao_nrf54l15` = **5**, `nu54dk`(L05) = **2**
   (L05 는 예약이 `0x4780`=18,304 B 라 실측상 2개까지다), 헤더 기본값 = 1.
2. **`bluefruit.h/.cpp`** — `BLE_MAX_CONNECTION` = `SD_BLE_PERIPH_LINK_COUNT`,
   `BLEConnection _connection[BLE_MAX_CONNECTION]` (**고정 배열 — `new` 금지**,
   상류는 `new` 를 쓰지만 전역 규칙 위반), `_prph_count` 보관,
   `uint8_t connected(void)`(개수) + `bool connected(uint16_t)`,
   `connHandle()` = 마지막 연결, `attMtu(conn_hdl)` / `maxPayload(conn_hdl)`.
   `_att_mtu` 는 **연결별**로 옮겨야 한다 (지금은 싱글턴 멤버 하나다).
3. **TX 세마포어를 연결별 배열로.** 지금 `_tx_sem` 이 하나라 A 링크의
   `HVN_TX_COMPLETE` 가 B 링크 대기자를 깨운다. `_waitTxComplete(conn_hdl, ms)` 로 바꾼다.
4. **`BLECharacteristic::notify(conn_hdl, ...)`** 추가, 무핸들판은 `connHandle()` 위임.
5. **`BLEUart`** — `write(conn_hdl,...)` 를 실제 연결별 notify 로, 청크 크기는
   그 연결의 MTU 를 쓴다. `notifyEnabled(conn_hdl)` 추가.
6. **`BLEPeriph::connected()` / `connected(conn_hdl)`** (상류 호환).
7. `Advertising::_restartIfNeeded()` 는 `_running` 이면 건너뛴다
   (연결 중에도 광고를 켜 두는 경우가 생기므로 이중 start 를 막는다).
8. 예제 `examples/Peripheral/bleuart_multi` 추가 → 링크 수를 5 로 올리고 실기 확인.

#### 검증

호스트 2대가 동시에 붙어야 한다. Mac(bleak) + 폰 조합을 쓴다.
확인할 것: 연결 2개 동시 유지, 각 링크의 MTU 가 따로 협상되는지,
한쪽만 끊었을 때 남은 링크가 살아 있는지, 광고가 다시 도는지.

⚠ 링크가 늘면 라디오 시간을 나눠 쓰므로 **연결당 처리량이 떨어진다.**
`SD_BLE_EVENT_LENGTH`(현재 6 = 7.5 ms) 를 같이 봐야 할 수 있다. 실측 대상이다.

### B7 — GATT 클라이언트 + 콜백 지연 실행 ✅ (2026-09-06)

`getPeerName()` 이 빈 문자열 대신 실제 이름을 준다. 실기에서 `Connected to Mac`.

#### 핵심은 GATT 가 아니라 **콜백을 어디서 도느냐** 였다

`sd_ble_gattc_char_value_by_uuid_read()` 한 번이면 탐색과 읽기가 같이 끝나서
GATT 쪽은 간단했다. 문제는 그게 **블로킹**이고, 상류 예제가 그걸
**연결 콜백 안에서** 부른다는 것이었다.

우리는 콜백을 BLE 이벤트 태스크에서 직접 불렀다. 거기서 블로킹하면
기다리는 응답 이벤트를 처리할 주체가 자기 자신이라 **영영 안 온다.**
Adafruit 이 `ada_callback()` 으로 콜백을 다른 태스크에 넘기는 이유가 그것이다.

그래서 콜백 태스크(`ble_cb`, 우선순위 2 — 이벤트 태스크 3보다 낮게)와 큐를 뒀다.
연결/해제 콜백이 거기서 돈다. 부수 효과로 **콜백이 오래 걸려도 이벤트 펌프가
막히지 않는다.**

⚠ 슬롯 반납(`_end()`)도 콜백 태스크로 옮겼다. 이벤트 핸들러에서 바로 반납하면
콜백이 도는 시점엔 이미 다른 연결이 그 슬롯에 들어와 있을 수 있다.
대신 링크가 꽉 찬 상태에서 끊고 곧바로 다시 붙으면 슬롯이 아직 안 비어
거절될 수 있다 — 콜백 태스크가 금방 돌므로 실제로는 좁은 창이다.
(Adafruit 은 반대로 즉시 free 해서, 그쪽은 해제 콜백에서 `Connection()` 이 NULL 이다.)

#### GATTC 는 RAM 을 더 안 먹었다

착수 전에 "`BLE_CONN_CFG_GATTC` 때문에 RAM 이 더 필요할 것" 이라고 봤는데 **틀렸다.**
그 설정은 write command 큐 깊이만 조정하는 것이고, GATT 클라이언트 절차 자체는
기본 구성으로 동작한다. cfg_set 을 추가하지 않았고 요구량은 `0x20006DD8` 그대로다.

#### 남은 것

`getPeerName()` 은 **상대가 이름을 공개할 때만** 값이 온다. iOS 는 본딩 전에
GAP Device Name 을 주지 않는 경우가 많아 0 이 정상일 수 있다.
반환형은 상류에 맞춰 `bool` -> `uint16_t`(길이) 로 바꿨다.

### B8 — central 역할 (2026-09-06: RAM 배분 확정)

**nRF54L15 는 `peripheral 4 + central 1`, 예약 32 KB 다** (`0x20007F48` 실측, 여유 184 B).
앱 RAM 은 233,472 -> 229,376 B 로 4 KB 준다 (256 KB 중 1.6%).

비용은 역할이 아니라 **링크 수**에 붙는다 — `3+1`(`0x20006DC0`) 이 `4+0`(`0x20006DD8`)
보다 24 B 싸다. 28 KB 를 유지하려면 `3+1` 로 가면 되지만, 다중 peripheral 을
4 개로 유지하려고 예약을 키웠다.

#### ⚠ 역할별로 다른 MTU 를 줄 수 없다

`sd_ble_cfg_set()` 으로 두 번째 `conn_cfg_tag` 를 만들려 하면
**`NRF_ERROR_NOT_SUPPORTED`(6)** 이 온다. S145 v10.0.1 은 **연결 구성을 하나만**
허용한다. Adafruit 은 nRF52 에서 `CONN_CFG_PERIPHERAL` / `CONN_CFG_CENTRAL` 을
나눠 역할마다 다른 MTU·큐를 주는데, **그 설계를 그대로 옮길 수 없다.**

결과: 두 역할이 MTU 247 · 이벤트 길이 · notify 큐 3 을 공유한다. central 에만
작은 MTU 를 줘서 RAM 을 아끼는 길은 없다. 실측표는 `docs/MEMORY-MAP.md`.

착수 전 추측은 "태그를 나누는 게 유리할 것" 이었는데 **애초에 선택지가 아니었다.**

#### 역할 배분을 런타임으로 옮겼다 (2026-09-07)

`begin(prph, central)` 인자가 실제로 `sd_ble_cfg_set()` 까지 간다.
그전에는 boards.txt 의 `-D` 로 **컴파일 타임에 고정**돼 있어서 `begin(0, 4)` 가
무조건 false 였다.

계기는 예제 전수 조사였다. 72개의 `Bluefruit.begin()` 호출을 훑어 보니
`central_bleuart_multi` 가 **`begin(0, 4)`**, `dual_bleuart` 와
`rssi_proximity_central` 이 `begin(1, 1)` 이다. 컴파일 타임 고정으로는
`(4,0)` 과 `(0,4)` 를 같이 지원하려면 링크 8개분 RAM 을 잡아야 하는데,
런타임이면 **어느 쪽이든 4개분만** 쓴다.

실측으로 확인 (예약 32 KB): `(4,0)`=`0x6DD8`, `(0,4)`=`0x6868`,
`(2,2)`=`0x6C00`, `(0,5)`=`0x7828` 전부 성공. `(8,0)`/`(6,2)` 는
`begin()`=false, `err=0x04`(NO_MEM), 필요량 `0x2000B3F8`/`0x2000B220` 을 보고한다.

같은 이유로 `configPrphConn(mtu, event_len, hvn_qsize, ...)` 과
`configPrphBandwidth()` 도 이제 **실제로 동작한다** (begin() 전에 부를 것).

⚠ MTU 는 `SD_BLE_ATT_MTU`(컴파일 타임) 를 넘길 수 없다. 이벤트 버퍼가 그 값
기준으로 잡혀 있어서, 넘기면 이벤트가 잘린다. 넘기면 `sdEnable()` 이 거절한다.

#### 스캐너 ✅ (2026-09-06)

`BLEScanner` 가 동작한다. 실기에서 주변 광고 수신, 주소 역순 출력, RSSI,
AD 타입 파싱(이름), active scan(스캔 응답), 16비트 UUID 필터까지 확인했다.
예제는 `examples/Central/central_scan`, 상류 `central_scan.ino` 도 그대로 컴파일된다.

⚠ **보고 하나마다 스캔이 멈춘다** (SoftDevice v6 이후). 콜백 안에서 `resume()` 을
불러야 이어진다. 안 부르면 "처음 하나만 잡히고 조용하다" 가 된다.
필터에 걸러진 보고도 라이브러리가 대신 `resume()` 해 준다 — 안 그러면 필터에
처음 걸린 순간 스캔이 멈춘다.

⚠ 스캔 콜백은 **연결 콜백과 달리 BLE 이벤트 태스크에서 직접** 불린다.
`report` 가 이벤트 버퍼를 가리켜서 미루면 덮이기 때문이다.
그래서 **스캔 콜백 안에서 블로킹 호출을 하면 안 된다** (`getPeerName()` 등).
출력·필터·연결 시작은 괜찮다.

#### central 연결 + 클라이언트 UART ✅ (2026-09-07)

두 보드로 실증했다. XIAO(central) ↔ NU54-DK(peripheral), MTU 247, 양방향:

```
XIAO:     Found E8:FA:C5:C4:FD:A5  rssi -64, connecting
          Connected, discovering UART service ... ready, MTU 247
          [peer] tick-0 ... tick-7
NU54-DK:  [from-central] hello-0 ... hello-6
```

경로 전체가 동작한다: UUID 필터 스캔 -> 연결 -> MTU 교환 -> 서비스/characteristic/
CCCD 탐색 -> 알림 켜기 -> 양방향 데이터.

⚠ **central 은 MTU 교환을 우리가 먼저 걸어야 한다.** peripheral 일 때는 상대가
걸어 주지만 central 일 때는 걸어 주는 쪽이 없다. 넣기 전에는 MTU 가 23 이었다.
콜백 태스크에서 **스케치 콜백보다 먼저** 끝낸다 — 콜백 안의 `discover()` 도
GATT 절차라 겹치면 `NRF_ERROR_BUSY` 가 난다.

⚠ **characteristic 은 UUID 로 가려야 한다.** 탐색 응답의 순서로 고르면 안 된다 —
규격이 순서를 정해 두지 않아 구현마다 다르다.

⚠ **characteristic 탐색은 한 번에 안 끝난다.** 마지막 핸들 다음부터 다시 물어
범위가 끝날 때까지 반복해야 한다. 한 번만 부르면 뒤쪽이 조용히 빠진다.

#### 다음

`BLEClientService` / `BLEClientCharacteristic` 를 일반화하면 `BLEClientBas` /
`BLEClientDis` 가 생기고, 상류 `central_bleuart` 가 그걸 쓴다 (지금은 그 둘 때문에
컴파일이 안 된다). 우리 `Central/central_bleuart` 예제는 동작한다.

nRF54L05 도 실측해서 **peripheral 2 + central 1** 로 올렸다 (`0x20005C38`).
둘 다 가지려고 예약을 `0x4B80` -> `0x5D00` 으로 키웠고, 앱 RAM 은
78,976 -> 74,496 B 다.

⚠ **프로브가 두 개 붙어 있으면 arduino-cli 업로드가 실패한다.**
어느 프로브인지 못 고른다. `probe-rs ... --probe <VID:PID:serial>` 로 직접 굽되,
**그 과정에서 SoftDevice 영역이 지워질 수 있다** — 실제로 겪었고 증상은
`begin=0 err=0 need=0` (cfg_set 이 아예 안 불린 상태) 이었다.
SoftDevice hex 를 다시 구우면 복구된다.

### 이어서 작업할 때 알아 둘 것

- **SoftDevice hex 를 먼저 구워야 한다.** 앱만 구우면 `sdEnable()` 이 SVC 를
  널 포인터로 포워딩한다. `docs/HIL/M3-softdevice.md` §5 에 명령이 있다
- **`g_sd_stage` 로 어디까지 갔는지 읽는다.** SoftDevice API 는 실패해도 원인이
  안 보이므로 단계 마커 + `m_last_error` 를 SWD 로 읽는 게 가장 빠르다
- **오류 코드는 헤더 retval 주석을 끝까지 읽어라.** A 단계에서 막힌 네 건이
  전부 거기 답이 있었다. 특히 `sd_ble_enable()` 의 `INVALID_STATE` 는
  "이미 초기화됨" 이 아니라 **RNG 미시딩**이었다
- **진단 출력을 부팅 때 한 번만 찍지 마라.** USB CDC 는 포트를 여는 사이에
  놓친다. 실패해도 주기적으로 찍게 해라
- 시험 스케치: `~/Documents/Arduino/nrf54_ble_adv/` (advertising 스파이크)

### (e) M2 — Arduino API

`analogRead`/`analogWrite`/`Wire`/`SPI`/`attachInterrupt`.
**착수 전에 CLAUDE.md §7 F10(nrfx 4.x 규칙)의 체크리스트를 반드시 읽어라.**
M1 에서 UARTE/GRTC 로 태운 함정이 그대로 반복된다.

---

## 2.5 `Serial` 수신이 32바이트 단위였다 — ✅ 고침 (2026-09-07)

**증상:** 짧은 문자열을 보내면 스케치가 못 읽었다. 32바이트가 찰 때까지 DMA 버퍼에
갇혀 있다가 나중 데이터가 청크를 채우면 **한꺼번에** 올라왔다.

**실기 재현 (NU54-DK):** 2바이트·5바이트 → 0개. 이어서 31바이트 → 32개가 쏟아짐.

**원인:** `RX_CHUNK = 32` 짜리 EasyDMA 버퍼를 걸어 두고, **버퍼가 다 차야** 오는
완료 이벤트로만 링버퍼에 옮겼다. 유휴 감지가 없었다.

**고친 방법:** `RX_CHUNK = 1`, 그리고 `NRFX_UARTE_RX_ENABLE_CONT` **해제**.
1·2·5·64·1024 바이트 모두 손실 없이 수신 확인.

#### 왜 이렇게 골랐나 — 다른 길들

| 방법 | 판단 |
|---|---|
| UARTE **FRAMETIMEOUT**(유휴 감지)로 부분 버퍼 끊기 | ❌ **nRF54L 에서 그 이벤트가 수신 도중 걸리면 뒤따르는 데이터의 비트가 깨진다**고 Nordic 이 확인 |
| RXDRDY → DPPI → **TIMER 카운터**로 바이트 세기 | 정석이고 Nordic 이 NCS 3.1 에서 택한 길(`UARTE_NRFX_UARTE_COUNT_BYTES_WITH_TIMER`). 다만 TIMER 인스턴스와 DPPI 채널을 선점한다 |
| **1바이트 DMA** (채택) | Adafruit nRF52 코어와 같은 구조(`RXD.MAXCNT = 1`). 자원을 안 먹고, 115200 에서 초당 약 11.5k 인터럽트로 감당된다 |

아주 높은 보드레이트가 필요해지면 TIMER + DPPI 로 옮긴다.

⚠ `NRFX_UARTE_RX_ENABLE_CONT` 는 ENDRX -> STARTRX 하드웨어 short 인데,
nrfx 문서가 **짧은 버퍼와 함께 쓰지 말라**고 못박는다. 끄면 드라이버가 ENDRX
인터럽트에서 다음 전송을 걸고, 그 틈의 바이트는 UARTE 하드웨어 FIFO 가 받는다.

#### ⚠ XIAO 의 VCOM 은 큰 입력 버스트에 멎는다 (보드 문제, 코어 아님)

XIAO 도 USB 를 다시 꽂으면 1·2·5·64 바이트가 정확히 수신된다. 그런데
**한 번에 1024 바이트를 밀어 넣으면 그 뒤로 수신이 완전히 죽는다** — 5바이트도
안 들어온다. 송신(보드 -> PC)은 그동안에도 멀쩡하다.

**우리 코어 문제가 아니라는 근거:**
- 같은 펌웨어로 NU54-DK(CP2102N)는 1024 바이트를 손실 없이 받는다
- **타깃만 리셋해도 안 살아난다.** 리셋하면 `Uart::begin()` 이 다시 도니
  우리 쪽 상태였다면 복구돼야 한다
- USB 를 뽑았다 꽂아야만 살아난다

즉 XIAO 온보드 CMSIS-DAP 의 **VCOM 이 호스트 -> 타깃 방향에서 멎는다.**
nrfx 드라이버도 오류 이벤트에서 수신을 멈추지 않는다(보고만 한다)는 것을
소스로 확인했다.

**사용자 안내:** XIAO 에서 시리얼 입력이 갑자기 안 되면 USB 를 다시 꽂는다.
큰 덩어리를 한 번에 붙여 넣지 말고 나눠 보낸다.

---

## 3. 아직 검증 못 한 가정

| 가정 | 언제 검증되나 |
|---|---|
| ~~BASEPRI/PRIMASK 분리가 BLE 라디오 타이밍을 지키는지 (§7 F9)~~ | ✅ **풀렸다.** advertising 과 연결을 유지한 채 tickless idle 에서 틱 vs SYSCOUNTER 0.0 ppm |
| 링크 3개 이상 동시 연결 | 호스트가 2대뿐이라 못 해 봤다. 슬롯 관리는 개수와 무관하므로 2개에서 검증된 경로와 같다 |
| BLE 실효 처리량 (특히 iOS) | 아직 안 쟀다. notify 큐를 3 으로 올려 뒀지만 수치가 없다. Adafruit `throughput` 예제로 잴 수 있다 |
| `USE_LFRC` 경로 (크리스털 없는 보드) | 해당 보드가 생길 때 |
| probe-rs 가 아닌 J-Link 업로드 | 메뉴에 없음. 필요해지면 추가 |
| 10분 이상 장시간(수 시간) 안정성 | 5분까지만 확인 |

---

## 4. 이 프로젝트에서 두 번 이상 물린 것

새 세션에서 시간을 아끼려면 이것만이라도 보고 시작해라.

1. **CLAUDE.md 의 함정 목록(§7 F1~F13)이 이 프로젝트의 핵심 자산이다.**
   F1(SVC), F9(WFI/BASEPRI), F10(nrfx 4.x), F13(Arduino 빌드)은 전부
   "문서에 적혀 있던 내용이 틀려서" 오래 걸린 항목이다. 의심되면 실측해라.
2. **Arduino 는 `cores/` 를 `core.a` 로 묶는다.** weak 심볼 오버라이드가
   아카이브 경계를 넘지 못해 조용히 실패한다 → `-Wl,--whole-archive` 필수 (§7 F13).
   링크 후 `nm | grep <심볼>` 로 `T` 인지 확인하는 습관을 들여라.
3. **zsh 는 변수를 단어 분리하지 않는다.** `$ARGS` 로 넘기면 통째로 한 인자가 된다.
   반드시 배열 `VAR=(a b c)` + `"${VAR[@]}"`.
4. **probe-rs 는 detach 하면 코어를 재개한다.** halt 상태를 유지하며 여러 번
   읽는 디버깅은 안 된다. 대신 RAM 에 계측 변수를 심고 `probe-rs read` 로 읽는다.
   `g_fault`(magic `0xFA0175ED`) / `g_assert_file` / `g_assert_line` 이 이미 있다.
   심볼 주소는 `arm-none-eabi-nm <elf>` 로 뽑는다.
5. **디버거 attach 자체가 증상을 지울 수 있다.** tickless 버그가 그랬다.
   "SWD 로 보면 정상"이 곧 "문제 없음"은 아니다 (docs/HIL/M1-tickless.md).
6. **LFXO 는 소스만 고르면 끝이 아니다. 로드 커패시터도 보드마다 맞춰야 한다.**
   외부 캡이 없는 보드에 외부 캡 설정을 쓰면 발진이 빨라진다 (XIAO 실측 +805 ppm).
   F12 와 증상이 똑같다 — 타깃 안에서는 완벽히 정상으로 보이고 **호스트 시계와
   비교해야만 드러난다.** 값은 벤더 보드 정의(Zephyr DTS)에서 가져오는 게 빠르다.
7. **nRF54L05 는 L15 다이의 비닝이다.** 사양 밖 메모리가 물리적으로 있어서
   잘못된 링커 스크립트로도 동작해 버린다. 칩은 FICR 로 확인해라
   (`INFO.PART` @ `0x00FFC31C`).

---

## 5. 다른 PC에서 이어서 작업하기

**클론만으로는 안 된다. 두 가지가 더 필요하다.**

### 1) 위치 — sketchbook 의 `hardware/` 밑에 **심링크**

저장소를 옮기지 마라. 심링크만 걸면 git 작업은 원래 위치에서 그대로 한다.

```sh
mkdir -p ~/Documents/Arduino/hardware
ln -sfn <저장소 경로> ~/Documents/Arduino/hardware/baram-nrf54
```
(sketchbook 경로는 `arduino-cli config get directories.user` 로 확인)

⚠ **링크 이름은 반드시 `baram-nrf54` 여야 한다.** 이 방식에서는 FQBN 의 packager 가
디렉토리 이름으로 정해지는데, Board Manager 로 설치하면 인덱스의
`packages[0].name` 으로 정해진다. 둘을 맞춰 놓지 않으면 개발 중 쓰던 FQBN 이
릴리스 설치본에서 안 먹는다.

`arduino-cli board listall | grep NU54` 로 확인.

**개발은 이 방식으로만 한다.** Board Manager 설치 경로는 고칠 때마다 아카이브 →
업로드 → 인덱스 갱신 → 재설치를 돌아야 해서 개발 루프로 못 쓴다.
그쪽은 M5 DoD("깨끗한 환경에서 Board Manager URL 로 설치 → blink 업로드")를
검증할 때만 쓴다.

### 2) `nrf54l/platform.local.txt` — **gitignore 되므로 직접 만들어야 한다**

```sh
cp nrf54l/platform.local.txt.example nrf54l/platform.local.txt
# 그 안의 toolchain.path 를 자기 PC 경로로 고친다
```

없으면 이렇게 된다 (실제로 재현해 본 결과):

- **업로드**: 확실히 실패한다
  `cannot execute upload tool: fork/exec {runtime.tools.probe-rs-0.32.0.path}/bin/probe-rs`
- **컴파일**: ⚠ **조용히 잘못될 수 있다.** 다른 Arduino 패키지(STM32 등)가
  설치해 둔 xpack GCC 가 잡혀서 빌드는 성공하는데 컴파일러 버전이 다르다.
  (실측: 같은 스케치가 38004 B → 38504 B 로 나왔다)

### 3) 준비물

| | |
|---|---|
| Arm GNU Toolchain | **xPack 14.2.1-1.1**. 릴리스가 이 버전에 고정돼 있다 (`platform.txt` 의 `runtime.tools.xpack-arm-none-eabi-gcc-14.2.1-1.1`). 다른 버전으로도 빌드는 되지만 **크기·측정값이 달라져 기존 HIL 기록과 비교할 수 없다** |
| arduino-cli | 1.0.3 / 1.2.2 에서 확인 |
| probe-rs | `0.32.0`. 개발용으로는 저장소 동봉본(`nrf54l/tools/probe-rs/macosx/bin`, **macOS 만**)을 쓰고, 릴리스 설치본은 Board Manager 가 툴로 내려받는다 |
| 하드웨어 | NU54-DK 계열은 외부 CMSIS-DAP 프로브 필요. **XIAO 는 온보드라 USB-C 하나면 된다** |

xPack GCC 는 이렇게 받는다 (Board Manager 가 쓰는 것과 같은 아카이브다):

```sh
curl -L -o /tmp/gcc.tar.gz \
  https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-darwin-arm64.tar.gz
mkdir -p ~/opt && tar xzf /tmp/gcc.tar.gz -C ~/opt
```

### 4) 예제 — 이제 저장소 안에 있다

`nrf54l/libraries/Bluefruit54Lib/examples/` 에 넣었다. IDE 의
**파일 → 예제 → Bluefruit54Lib** 에서 열린다. 전부 3보드 컴파일 확인됐다.

| 예제 | 내용 |
|---|---|
| `Peripheral/bleuart` | **Adafruit 원본 이식.** include 2줄만 지웠다 (원본 고지 유지) |
| `Peripheral/custom_service` | 128비트 커스텀 GATT 서비스 (읽기/알림/쓰기) |
| `Hardware/rtos_scheduler` | `Scheduler.startLoop()` 두 번째 태스크 + millis/micros |
| `Hardware/board_test` | 새 보드 붙였을 때 첫 확인 (LED 극성 / 버튼 / Serial) |

**예제 주석은 영어로 쓴다.** 사용자 대상이라 README 와 같은 기준이다.

### 5) 저장소에 없는 것

시험 스케치는 저장소 밖에 있다. 새 PC에서는 직접 만들어야 한다:
`~/Documents/Arduino/nu54dk_blink/nu54dk_blink.ino`
(LED1 1초 점멸 + `Scheduler.startLoop` 로 LED2 500ms 점멸
 + `millis` / `micros` / 버튼 상태를 1초마다 `Serial` 출력)

### 6) 동작 확인 순서

```sh
arduino-cli board listall | grep NU54          # 보드 2종이 보이는지
arduino-cli compile --fqbn baram-nrf54:nrf54l:nu54dk  <스케치>
arduino-cli upload  --fqbn baram-nrf54:nrf54l:nu54dk  <스케치>
```
시리얼 `/dev/cu.usbserial-*` (CP2102N) 115200 에서
`millis=... micros=... btn1=1` 이 2초 간격으로 나오고,
**Δmillis = 2000 이면서 Δmicros = 2000000** 이면 정상이다.
(둘 중 하나만 봐서는 tickless 드리프트를 못 잡는다 — §7 F9b)

시험 스케치: `~/Documents/Arduino/nu54dk_blink/nu54dk_blink.ino`
(LED1 1초 점멸 + `Scheduler.startLoop` 로 LED2 500ms + millis/micros/버튼 출력)

시리얼: `/dev/cu.usbserial-*` (CP2102N), 115200.
